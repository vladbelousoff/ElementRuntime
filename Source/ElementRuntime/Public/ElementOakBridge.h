#pragma once

#include "ElementComponents.h"

#include <elm/oak_bridge_core.hpp>

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class EElementInputKey : int {
    W = 0,
    S,
    A,
    D,
    E,
    Q,
    Space,
    LeftControl,
    Up,
    Down,
    Left,
    Right,
    P,
    Count,
};

struct FElementDamageEvent {
    elm::Entity Target;
    float Amount = 0.0f;
};

struct FInputContext {
    std::function<bool(EElementInputKey)> IsKeyDown;
    float CameraYaw = 0.0f;
    int WaveEnemyCount = 750;
    float WaveSpawnRadius = 1500.0f;
    float FlameRange = 500.0f;
    float FlameAngle = 60.0f;
    float FlameDamage = 150.0f;
    float ZombieSpeed = 120.0f;
};

class FElementOakBridge : public oak_bridge::BridgeInterface {
public:
    explicit FElementOakBridge(elm::Registry& Registry);
    ~FElementOakBridge() override;

    FElementOakBridge(const FElementOakBridge&) = delete;
    FElementOakBridge& operator=(const FElementOakBridge&) = delete;

    bool LoadScript(const char* ScriptSource, std::size_t Length);
    bool LoadProgram(const char* EntryPath);
    bool BuildSystems(std::vector<elm::System>& OutSystems, float* DeltaSecondsPtr);

    void ExpireEntity(elm::Entity Entity);
    void ExpireEntityById(int Index, int Generation);

    void Activate();
    elm::Entity SpawnEntity();
    void RebuildViewCaches();

    static constexpr int MaxSlots = oak_bridge::max_component_slots;

    elm::Registry& GetRegistry() { return Registry; }
    oak_allocator_t* GetAllocator() { return Allocator; }
    oak_bind_type_t* GetEntityType() { return EntityType; }
    oak_bind_type_t* GetSlotOakType(int Index) const { return SlotOakTypes[Index]; }
    oak_bind_type_t* GetSlotReadOnlyOakType(int Index) const { return SlotReadOnlyOakTypes[Index]; }
    oak_bind_type_t* GetSlotViewType(int Index) const { return SlotViewTypes[Index]; }

    oak_bridge::ViewCache& GetViewCacheMut(int Index) { return ViewCaches[Index]; }
    oak_bridge::ComponentSlot& GetSlotMut(int Index) { return Slots_[Index]; }
    int AllocSlot() { return SlotCount_++; }

    // oak_bridge::BridgeInterface overrides. Native Oak callbacks reach the
    // bridge through this interface (resolved per-VM via vm->user_data), so the
    // bridge no longer needs a separate adapter or process-global pointers.
    elm::Registry& get_registry() override { return Registry; }
    oak_allocator_t* get_allocator() override { return Allocator; }
    oak_bind_type_t* get_entity_type() override { return EntityType; }
    oak_bind_type_t* get_slot_oak_type(int Index) override { return GetSlotOakType(Index); }
    oak_bind_type_t* get_slot_read_only_oak_type(int Index) override { return GetSlotReadOnlyOakType(Index); }
    oak_bridge::ViewCache& get_view_cache(int Index) override { return GetViewCacheMut(Index); }
    oak_bridge::ComponentSlot& get_slot(int Index) override { return GetSlotMut(Index); }
    int get_slot_count() override { return AllocSlot(); }
    void expire_entity(elm::Entity Entity) override { ExpireEntity(Entity); }
    elm::Entity spawn_entity() override { return SpawnEntity(); }
    bool try_stage_emplace(elm::Entity Entity, int SlotIndex, oak_value_t* Out) override { return TryStageEmplace(Entity, SlotIndex, Out); }

    FInputContext& GetInputContext() { return InputCtx; }
    const FInputContext& GetInputContext() const { return InputCtx; }

    void QueueDamage(elm::Entity Target, float Amount);
    void ApplyQueuedDamage();

    // Called before/after Scheduler::run() to gate staged spawning.
    void BeginSystemExecution();
    void EndSystemExecution();
    // Replays buffered entity emplaces on the game thread. Call after EndSystemExecution.
    void FlushStagedSpawns();

    // Returns true and fills *out if entity is staged (created during system execution).
    bool TryStageEmplace(elm::Entity Entity, int SlotIndex, oak_value_t* Out);

    std::vector<elm::Entity> TakeExpired();

    void RebuildSpatialGrid();
    void QueryAvoidance(elm::Entity Entity, float Range, float Strength, float& OutVx, float& OutVy);

    static constexpr int GridDim = 256;
    static constexpr float GridCellSize = 64.0f;

    struct FSpatialCell {
        std::vector<float> Xs;
        std::vector<float> Ys;
    };

    std::vector<FSpatialCell> SpatialGrid;
    std::vector<int> DirtyCells;

private:
    void RegisterComponentBindings();
    void RegisterAttributes();
    void RegisterNativeFunctions();

    elm::Registry& Registry;
    oak_allocator_t* Allocator = nullptr;
    oak_vm_t* VM = nullptr;
    oak_compile_options_t* CompileOpts = nullptr;
    oak_chunk_t* Chunk = nullptr;

    std::unordered_map<std::string, oak_bridge::ComponentBinding> Components;
    std::unordered_map<std::string, int> ViewSlotIndices; // Oak view type name -> slot index
    std::vector<oak_bridge::SystemBinding> PendingSystems;

    // Per-bridge backing store for the attribute-callback user_data, so two
    // coexisting bridges never share/overwrite each other's callback targets.
    // The bridge is non-copyable and held by stable address, so &AttrState
    // stays valid for as long as the compile options reference it.
    oak_bridge::AttributeCallbackState AttrState;
    std::vector<elm::Entity> Expired;
    mutable std::mutex ExpiredMutex;
    std::vector<FElementDamageEvent> PendingDamage;
    std::mutex PendingDamageMutex;

    // Staged component for a deferred emplace. Holds an aligned, owning allocation.
    // The stored object is default-constructed in TryStageEmplace and either
    // move-constructed into the registry in FlushStagedSpawns (clearing Data) or
    // destructed by ~FStagedEmplace if the flush is skipped.
    struct FStagedEmplace {
        int SlotIndex = -1;
        void* Data = nullptr;
        void (*Destruct)(void*) = nullptr;
        uint32 DataAlignment = 1;

        FStagedEmplace() = default;
        ~FStagedEmplace()
        {
            if (Data) {
                if (Destruct) { Destruct(Data); }
                FMemory::Free(Data);
            }
        }
        FStagedEmplace(FStagedEmplace&& Other) noexcept
            : SlotIndex(Other.SlotIndex), Data(Other.Data)
            , Destruct(Other.Destruct), DataAlignment(Other.DataAlignment)
        {
            Other.Data = nullptr;
        }
        FStagedEmplace(const FStagedEmplace&) = delete;
        FStagedEmplace& operator=(const FStagedEmplace&) = delete;
        FStagedEmplace& operator=(FStagedEmplace&&) = delete;
    };
    struct FStagedEntity {
        elm::Entity Entity;
        std::vector<FStagedEmplace> Emplaces;
        bool bCancelled = false;
    };

    // Token entity indices during parallel execution start here to avoid
    // colliding with real registry indices (which grow from 0).
    static constexpr std::uint32_t StagingBase = 0xC0000000u;

    bool SystemExecutionActive = false;
    std::mutex SpawnMutex; // serializes staging map/vector access from worker threads
    std::unordered_map<std::uint32_t, std::size_t> StagedEntityMap; // token.index -> StagedEntities index
    std::vector<FStagedEntity> StagedEntities;

    std::vector<oak_bridge::ViewCache> ViewCaches;
    std::uint64_t LastViewCacheMutationVersion = ~std::uint64_t(0);
    FInputContext InputCtx;

    oak_bind_type_t* EntityType = nullptr;
    oak_bind_type_t* SlotOakTypes[MaxSlots] = {};
    oak_bind_type_t* SlotReadOnlyOakTypes[MaxSlots] = {};
    oak_bind_type_t* SlotViewTypes[MaxSlots] = {};
    oak_bridge::ComponentSlot Slots_[MaxSlots];
    int SlotCount_ = 0;
    oak_module_registry_t* ModuleRegistry = nullptr;
};
