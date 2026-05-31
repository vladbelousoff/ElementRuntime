#include "ElementOakBridge.h"

#include "ElementComponents.h"

#include <elm/oak_bridge_core.hpp>

namespace {

// ============================================================
// Input native functions (Unreal-specific)
// ============================================================

// Recover the owning bridge for a native call from the per-VM context. The
// bridge stores itself in vm->user_data, so every callback targets the bridge
// that scheduled the running system — no process-global pointers, and multiple
// bridges can run concurrently without clobbering each other.
inline FElementOakBridge* OwnerFromCtx(oak_native_ctx_t* ctx)
{
    return static_cast<FElementOakBridge*>(oak_bridge::bridge_from_ctx(ctx));
}

enum oak_fn_call_result_t NativeIsKeyDown(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    const int KeyValue = oak_bridge::as_int(args[0]);
    const auto Key = static_cast<EElementInputKey>(KeyValue);
    if (Owner
        && Owner->GetInputContext().IsKeyDown
        && KeyValue >= 0
        && KeyValue < static_cast<int>(EElementInputKey::Count)) {
        *out = oak_value_bool(Owner->GetInputContext().IsKeyDown(Key) ? 1 : 0);
    } else {
        *out = oak_value_bool(0);
    }
    return OAK_FN_CALL_OK;
}

void BindInputKeyEnum(oak_compile_options_t* CompileOpts)
{
    auto* InputKey = oak_bind_enum(CompileOpts, "InputKey");
    oak_bind_enum_variant(InputKey, "W", static_cast<int>(EElementInputKey::W));
    oak_bind_enum_variant(InputKey, "S", static_cast<int>(EElementInputKey::S));
    oak_bind_enum_variant(InputKey, "A", static_cast<int>(EElementInputKey::A));
    oak_bind_enum_variant(InputKey, "D", static_cast<int>(EElementInputKey::D));
    oak_bind_enum_variant(InputKey, "E", static_cast<int>(EElementInputKey::E));
    oak_bind_enum_variant(InputKey, "Q", static_cast<int>(EElementInputKey::Q));
    oak_bind_enum_variant(InputKey, "Space", static_cast<int>(EElementInputKey::Space));
    oak_bind_enum_variant(InputKey, "LeftControl", static_cast<int>(EElementInputKey::LeftControl));
    oak_bind_enum_variant(InputKey, "Up", static_cast<int>(EElementInputKey::Up));
    oak_bind_enum_variant(InputKey, "Down", static_cast<int>(EElementInputKey::Down));
    oak_bind_enum_variant(InputKey, "Left", static_cast<int>(EElementInputKey::Left));
    oak_bind_enum_variant(InputKey, "Right", static_cast<int>(EElementInputKey::Right));
    oak_bind_enum_variant(InputKey, "P", static_cast<int>(EElementInputKey::P));
}


// damage_entity does NOT write Health components during parallel system execution.
// It appends to a mutex-protected PendingDamage queue; ApplyQueuedDamage() on the
// game thread flushes the queue after Scheduler::run() completes.  This means Health
// components are read-only during the parallel phase and no Health write conflict
// exists between concurrent Oak systems — the deferred flush is the only write.
enum oak_fn_call_result_t NativeDamageEntity(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    const elm::Entity Target = oak_bridge::entity_from_value(args[0]);
    if (Owner && Target.valid()) {
        Owner->QueueDamage(Target, oak_bridge::as_float(args[1]));
    }
    *out = oak_value_none();
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetCameraYaw(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().CameraYaw : 0.0f);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetWaveEnemyCount(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_i32(Owner ? Owner->GetInputContext().WaveEnemyCount : 750);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetFlameRange(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().FlameRange : 500.0f);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetFlameAngle(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().FlameAngle : 60.0f);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetFlameDamage(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().FlameDamage : 150.0f);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetWaveSpawnRadius(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().WaveSpawnRadius : 1500.0f);
    return OAK_FN_CALL_OK;
}

enum oak_fn_call_result_t NativeGetZombieSpeed(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)args; (void)argc;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    *out = oak_value_f32(Owner ? Owner->GetInputContext().ZombieSpeed : 120.0f);
    return OAK_FN_CALL_OK;
}

// apply_avoidance(entity, vel, range, strength)
// Takes the caller's mutable Velocity record so the write is visible at the Oak
// call site (caller must already have declared `mut vel: Velocity`) rather than
// going through the registry behind the scheduler's back.
enum oak_fn_call_result_t NativeApplyAvoidance(oak_native_ctx_t* ctx, const oak_value_t* args, int argc, oak_value_t* out)
{
    (void)out;
    FElementOakBridge* Owner = OwnerFromCtx(ctx);
    if (!Owner) { return OAK_FN_CALL_OK; }

    const elm::Entity Entity = oak_bridge::entity_from_value(args[0]);
    auto* Vel    = static_cast<FElementVelocity*>(oak_native_instance(args[1]));
    if (!Entity.valid() || !Vel) { return OAK_FN_CALL_OK; }

    float Range    = oak_bridge::as_float(args[2]);
    float Strength = oak_bridge::as_float(args[3]);

    float Vx = 0.0f, Vy = 0.0f;
    Owner->QueryAvoidance(Entity, Range, Strength, Vx, Vy);

    Vel->X += Vx;
    Vel->Y += Vy;

    return OAK_FN_CALL_OK;
}

} // namespace

// ------------------------------------------------------------
// Per-thread segregated free-list for small, short-lived Oak
// allocations (the dominant case being the view record wrappers
// minted by the per-component view .of()/.at(), which are allocated and freed
// once per entity per tick). Recycling them on a thread-local
// free list turns those mallocs into a pointer pop/push and avoids
// cross-thread heap contention on the parallel system phase.
//
// Every allocation carries a 16-byte header storing its usable
// block size, so Free()/Realloc() can recover the size class
// without the allocator API passing one. The header keeps the
// returned payload 16-byte aligned (matching oak_value_t).
// ------------------------------------------------------------
namespace OakPool
{
    constexpr SIZE_T HeaderBytes = 16;   // also preserves 16-byte payload alignment
    constexpr SIZE_T Granule = 16;
    constexpr SIZE_T MaxBlock = 256;     // only pool usable sizes up to this
    constexpr int BucketCount = static_cast<int>(MaxBlock / Granule);
    constexpr int CapPerBucket = 256;    // bound retained blocks per bucket per thread

    struct FNode { FNode* Next; };

    struct FThreadCache
    {
        FNode* Heads[BucketCount] = {};
        int Counts[BucketCount] = {};

        ~FThreadCache()
        {
            for (int B = 0; B < BucketCount; ++B)
            {
                for (FNode* N = Heads[B]; N != nullptr; )
                {
                    FNode* Next = N->Next;
                    FMemory::Free(reinterpret_cast<uint8*>(N) - HeaderBytes);
                    N = Next;
                }
            }
        }
    };

    thread_local FThreadCache GCache;

    // Bucket for a usable (already rounded) block size, or -1 if not poolable.
    FORCEINLINE int BucketOf(SIZE_T UsableSize)
    {
        if (UsableSize == 0 || UsableSize > MaxBlock)
        {
            return -1;
        }
        return static_cast<int>((UsableSize + Granule - 1) / Granule) - 1;
    }

    FORCEINLINE SIZE_T HeaderSizeOf(void* Payload)
    {
        return *reinterpret_cast<SIZE_T*>(reinterpret_cast<uint8*>(Payload) - HeaderBytes);
    }
}

static void* UnrealOakAlloc(oak_allocator_t* self, usize size, const char* file, int line)
{
    (void)self; (void)file; (void)line;
    using namespace OakPool;

    const bool bPoolable = size <= MaxBlock;
    const SIZE_T UsableSize = bPoolable ? ((size + Granule - 1) / Granule) * Granule : size;
    const int Bucket = BucketOf(UsableSize);

    if (Bucket >= 0)
    {
        if (FNode* Node = GCache.Heads[Bucket])
        {
            GCache.Heads[Bucket] = Node->Next;
            --GCache.Counts[Bucket];
            // Header below the payload already records UsableSize from when
            // this block was first allocated (same size class).
            return Node;
        }
    }

    uint8* Raw = static_cast<uint8*>(FMemory::Malloc(HeaderBytes + UsableSize, HeaderBytes));
    *reinterpret_cast<SIZE_T*>(Raw) = UsableSize;
    return Raw + HeaderBytes;
}

static void UnrealOakFree(oak_allocator_t* self, void* ptr, const char* file, int line)
{
    (void)self; (void)file; (void)line;
    using namespace OakPool;

    if (ptr == nullptr)
    {
        return;
    }

    const SIZE_T UsableSize = HeaderSizeOf(ptr);
    const int Bucket = BucketOf(UsableSize);
    if (Bucket >= 0 && GCache.Counts[Bucket] < CapPerBucket)
    {
        FNode* Node = static_cast<FNode*>(ptr);
        Node->Next = GCache.Heads[Bucket];
        GCache.Heads[Bucket] = Node;
        ++GCache.Counts[Bucket];
        return;
    }

    FMemory::Free(reinterpret_cast<uint8*>(ptr) - HeaderBytes);
}

static void* UnrealOakRealloc(oak_allocator_t* self, void* ptr, usize new_size, const char* file, int line)
{
    using namespace OakPool;

    if (ptr == nullptr)
    {
        return UnrealOakAlloc(self, new_size, file, line);
    }
    if (new_size == 0)
    {
        UnrealOakFree(self, ptr, file, line);
        return nullptr;
    }

    // Route through alloc/free so the header and pool bookkeeping stay
    // consistent. Realloc is rare and never on the per-entity view path.
    const SIZE_T OldUsable = HeaderSizeOf(ptr);
    if (new_size <= OldUsable)
    {
        return ptr;
    }

    void* New = UnrealOakAlloc(self, new_size, file, line);
    FMemory::Memcpy(New, ptr, FMath::Min(OldUsable, static_cast<SIZE_T>(new_size)));
    UnrealOakFree(self, ptr, file, line);
    return New;
}

static oak_allocator_t GUnrealAllocator = {
    UnrealOakAlloc,
    UnrealOakRealloc,
    UnrealOakFree,
    nullptr,
    nullptr
};

FElementOakBridge::FElementOakBridge(elm::Registry& InRegistry)
    : Registry(InRegistry)
{
    SlotCount_ = 0;
    Allocator = &GUnrealAllocator;

    VM = static_cast<oak_vm_t*>(OAK_ALLOC(Allocator, sizeof(oak_vm_t)));
    oak_vm_init(VM, Allocator);
    // Native Oak callbacks recover their owning bridge from vm->user_data
    // (see OwnerFromCtx / oak_bridge::bridge_from_ctx), so no process globals
    // are needed and multiple bridges can coexist. Store the BridgeInterface*
    // subobject pointer so the symmetric cast in bridge_from_ctx is correct
    // regardless of object layout.
    VM->user_data = static_cast<oak_bridge::BridgeInterface*>(this);

    CompileOpts = static_cast<oak_compile_options_t*>(OAK_ALLOC(Allocator, sizeof(oak_compile_options_t)));
    oak_compile_options_init(CompileOpts, Allocator);
    CompileOpts->allow_bodyless_fns = 1;
    oak_stdlib_register(CompileOpts);

    RegisterComponentBindings();
    RegisterNativeFunctions();
    RegisterAttributes();
}

FElementOakBridge::~FElementOakBridge()
{
    if (VM) {
        oak_vm_free(VM);
        OAK_FREE(Allocator, VM);
    }
    if (ModuleRegistry) {
        oak_module_registry_free(ModuleRegistry);
        OAK_FREE(Allocator, ModuleRegistry);
    } else if (Chunk) {
        oak_chunk_free(Chunk);
    }
    if (CompileOpts) {
        oak_compile_options_free(CompileOpts);
        OAK_FREE(Allocator, CompileOpts);
    }
}

void FElementOakBridge::RegisterComponentBindings()
{
    EntityType = oak_bind_type(CompileOpts, OAK_BIND_TYPE_VALUE, "Entity");

    // View types are not generated here: they are created on demand when the
    // Oak compiler encounters an @ElementComponentView record (see
    // register_attributes). SlotViewTypes[slot] stays null for components left
    // as plain @ElementComponent.
    auto Register = [this](const char* oak_name, elm::TypeId cpp_type_id, oak_bind_type_t* oak_type, oak_bind_type_t* read_only_oak_type, int slot_index) {
        Components[oak_name] = { oak_name, cpp_type_id, oak_type, read_only_oak_type, slot_index };
        SlotOakTypes[slot_index] = oak_type;
        SlotReadOnlyOakTypes[slot_index] = read_only_oak_type;
    };

    oak_bridge::ComponentBuilder<FElementPosition>(*this, CompileOpts, EntityType, "Position")
        .field<&FElementPosition::X>("x")
        .field<&FElementPosition::Y>("y")
        .field<&FElementPosition::Z>("z")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementVelocity>(*this, CompileOpts, EntityType, "Velocity")
        .field<&FElementVelocity::X>("x")
        .field<&FElementVelocity::Y>("y")
        .field<&FElementVelocity::Z>("z")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementHealth>(*this, CompileOpts, EntityType, "Health")
        .field<&FElementHealth::HP>("hp")
        .field<&FElementHealth::MaxHP>("max_hp")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementZombie>(*this, CompileOpts, EntityType, "Zombie")
        .field<&FElementZombie::SpeedMul>("speed_mul")
        .field<&FElementZombie::Size>("size")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementPlayer>(*this, CompileOpts, EntityType, "Player")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementPlayerInput>(*this, CompileOpts, EntityType, "PlayerInput")
        .field<&FElementPlayerInput::Forward>("forward")
        .field<&FElementPlayerInput::Right>("right")
        .field<&FElementPlayerInput::Up>("up")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementCameraState>(*this, CompileOpts, EntityType, "CameraState")
        .field<&FElementCameraState::Yaw>("yaw")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementFlame>(*this, CompileOpts, EntityType, "Flame")
        .field<&FElementFlame::Active>("active")
        .field<&FElementFlame::Range>("range")
        .field<&FElementFlame::Angle>("angle")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementFlameFrustum>(*this, CompileOpts, EntityType, "FlameFrustum")
        .field<&FElementFlameFrustum::Active>("active")
        .field<&FElementFlameFrustum::OX>("ox")
        .field<&FElementFlameFrustum::OY>("oy")
        .field<&FElementFlameFrustum::DX>("dx")
        .field<&FElementFlameFrustum::DY>("dy")
        .field<&FElementFlameFrustum::CosHalf>("cos_half")
        .field<&FElementFlameFrustum::RangeSq>("range_sq")
        .register_component(Register);

    oak_bridge::ComponentBuilder<FElementGameState>(*this, CompileOpts, EntityType, "GameState")
        .field<&FElementGameState::Wave>("wave")
        .field<&FElementGameState::SpawnTimer>("spawn_timer")
        .field<&FElementGameState::Kills>("kills")
        .field<&FElementGameState::Initialized>("initialized")
        .field<&FElementGameState::SpawnsRemaining>("spawns_remaining")
        .register_component(Register);

    ViewCaches.resize(SlotCount_);

    for (int I = 0; I < SlotCount_; ++I) {
        auto& Slot = Slots_[I];
        Registry.ensure_descriptor_raw(Slot.cpp_type_id, Slot.column_desc);
    }
}

void FElementOakBridge::RegisterNativeFunctions()
{
    oak_bridge::register_common_native_functions(CompileOpts, EntityType);
    BindInputKeyEnum(CompileOpts);

    oak_bridge::bind_global_fn(CompileOpts, "is_key_down", NativeIsKeyDown, 1, OAK_TYPE_BOOL);
    oak_bridge::bind_global_fn(CompileOpts, "get_camera_yaw", NativeGetCameraYaw, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "apply_avoidance", NativeApplyAvoidance, 4);
    oak_bridge::bind_global_fn(CompileOpts, "damage_entity", NativeDamageEntity, 2);
    oak_bridge::bind_global_fn(CompileOpts, "get_wave_enemy_count", NativeGetWaveEnemyCount, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "get_wave_spawn_radius", NativeGetWaveSpawnRadius, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "get_flame_range", NativeGetFlameRange, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "get_flame_angle", NativeGetFlameAngle, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "get_flame_damage", NativeGetFlameDamage, 0, OAK_TYPE_NUMBER);
    oak_bridge::bind_global_fn(CompileOpts, "get_zombie_speed", NativeGetZombieSpeed, 0, OAK_TYPE_NUMBER);
}

void FElementOakBridge::RegisterAttributes()
{
    oak_bridge::register_attributes(CompileOpts, Allocator, EntityType, Components, ViewSlotIndices, PendingSystems, SlotViewTypes, AttrState);
}

bool FElementOakBridge::LoadScript(const char* ScriptSource, std::size_t Length)
{
    auto* Lexer = oak_lexer_tokenize(ScriptSource, Length, Allocator);
    if (oak_lexer_error_count(Lexer) > 0) {
        UE_LOG(LogTemp, Error, TEXT("Oak lexer errors:"));
        oak_lexer_free(Lexer);
        return false;
    }

    oak_parser_result_t Parser = {};
    oak_parse(Lexer, OAK_NODE_PROGRAM, &Parser, Allocator);
    if (oak_parser_error_count(&Parser) > 0) {
        const int Count = oak_parser_error_count(&Parser);
        const oak_diagnostic_t* Errors = oak_parser_errors(&Parser);
        for (int Index = 0; Index < Count; ++Index) {
            UE_LOG(LogTemp, Error, TEXT("Oak parse error [%d:%d]: %s"),
                Errors[Index].line, Errors[Index].column,
                UTF8_TO_TCHAR(Errors[Index].message));
        }
        oak_parser_free(&Parser);
        oak_lexer_free(Lexer);
        return false;
    }

    oak_compile_result_t Compile = {};
    oak_compile_ex(oak_parser_root(&Parser), CompileOpts, &Compile);
    if (!Compile.chunk || Compile.error_count > 0) {
        for (int Index = 0; Index < Compile.error_count; ++Index) {
            UE_LOG(LogTemp, Error, TEXT("Oak compile error [%d:%d]: %s"),
                Compile.errors[Index].line, Compile.errors[Index].column,
                UTF8_TO_TCHAR(Compile.errors[Index].message));
        }
        oak_compile_result_free(&Compile);
        oak_parser_free(&Parser);
        oak_lexer_free(Lexer);
        return false;
    }

    if (Chunk) {
        oak_chunk_free(Chunk);
    }
    Chunk = Compile.chunk;
    oak_vm_run(VM, Chunk);

    oak_parser_free(&Parser);
    oak_lexer_free(Lexer);
    return true;
}

bool FElementOakBridge::LoadProgram(const char* EntryPath)
{
    // Clear stale system callbacks before loading so PendingSystems only contains
    // systems discovered during THIS load. @ElementSystem callbacks fire inside
    // oak_module_loader_load_program and append to PendingSystems.
    PendingSystems.clear();

    // Allocate and populate a fresh module registry before touching the current one.
    // On failure we free the candidate registry and leave existing Chunk intact so
    // BuildSystems cannot observe a half-initialised or dangling Chunk.
    auto* NewRegistry = static_cast<oak_module_registry_t*>(OAK_ALLOC(Allocator, sizeof(oak_module_registry_t)));
    oak_module_registry_init(NewRegistry, Allocator);

    oak_module_loader_result_t Result = {};
    const int Err = oak_module_loader_load_program(EntryPath, CompileOpts, NewRegistry, &Result);

    if (Err != 0 || !Result.entry) {
        for (int Index = 0; Index < Result.error_count; ++Index) {
            UE_LOG(LogTemp, Error, TEXT("Oak error [%d:%d]: %s"),
                Result.errors[Index].line, Result.errors[Index].column,
                UTF8_TO_TCHAR(Result.errors[Index].message));
        }
        oak_module_registry_free(NewRegistry);
        OAK_FREE(Allocator, NewRegistry);
        Chunk = nullptr; // prevent BuildSystems from consuming old bytecode
        return false;
    }

    // Success: swap in the new registry and chunk.
    if (ModuleRegistry) {
        oak_module_registry_free(ModuleRegistry);
        OAK_FREE(Allocator, ModuleRegistry);
    }
    ModuleRegistry = NewRegistry;
    Chunk = Result.entry->chunk;
    oak_vm_set_module_registry(VM, ModuleRegistry);
    oak_vm_run(VM, Chunk);
    return true;
}

bool FElementOakBridge::BuildSystems(std::vector<elm::System>& OutSystems, float* DeltaSecondsPtr)
{
    return oak_bridge::build_systems(Chunk, Allocator, EntityType, ModuleRegistry,
                             Components, PendingSystems, SlotViewTypes,
                             Slots_, DeltaSecondsPtr, this, OutSystems);
}

void FElementOakBridge::QueueDamage(elm::Entity Target, float Amount)
{
    if (Amount <= 0.0f) {
        return;
    }
    // Token entities (index >= StagingBase) are not yet in the registry; they
    // have no Health component and cannot receive damage before materialisation.
    if (Target.index >= StagingBase) {
        return;
    }

    std::lock_guard<std::mutex> Lock(PendingDamageMutex);
    PendingDamage.push_back(FElementDamageEvent { Target, Amount });
}

void FElementOakBridge::ApplyQueuedDamage()
{
    std::vector<FElementDamageEvent> Damage;
    {
        std::lock_guard<std::mutex> Lock(PendingDamageMutex);
        Damage.swap(PendingDamage);
    }

    for (const auto& Event : Damage) {
        if (Registry.alive(Event.Target) && Registry.has<FElementHealth>(Event.Target)) {
            Registry.get<FElementHealth>(Event.Target).HP -= Event.Amount;
        }
    }
}

void FElementOakBridge::Activate()
{
    // Per-bridge context now travels with each VM via vm->user_data (set at
    // construction and on every per-chunk system VM), so activation no longer
    // installs any process-global pointers. Re-affirm the main VM's owner
    // defensively; this is idempotent and cheap.
    if (VM) {
        VM->user_data = static_cast<oak_bridge::BridgeInterface*>(this);
    }
}

elm::Entity FElementOakBridge::SpawnEntity()
{
    if (!SystemExecutionActive) {
        return Registry.create();
    }
    // During parallel execution, never touch the registry. Issue a token entity
    // whose index lives above StagingBase so it cannot collide with real indices.
    std::lock_guard<std::mutex> Lock(SpawnMutex);
    const std::uint32_t TokenIndex = StagingBase + static_cast<std::uint32_t>(StagedEntities.size());
    elm::Entity Token{ TokenIndex, 0 };
    StagedEntityMap[TokenIndex] = StagedEntities.size();
    StagedEntities.push_back({ Token, {} });
    return Token;
}

void FElementOakBridge::BeginSystemExecution()
{
    SystemExecutionActive = true;
}

void FElementOakBridge::EndSystemExecution()
{
    SystemExecutionActive = false;
}

void FElementOakBridge::FlushStagedSpawns()
{
    // Runs on the game thread after all parallel systems complete.
    // Token entities (index >= StagingBase) become real registry entities here.
    // Live entities (index < StagingBase) had their emplaces deferred to avoid
    // concurrent archetype column mutation during parallel execution.
    for (auto& Staged : StagedEntities) {
        if (Staged.bCancelled) {
            continue; // staged component data freed by ~FStagedEmplace
        }

        elm::Entity Entity;
        if (Staged.Entity.index >= StagingBase) {
            Entity = Registry.create();
        } else {
            if (!Registry.alive(Staged.Entity)) {
                continue; // destroyed between staging and flush — discard
            }
            Entity = Staged.Entity;
        }

        for (auto& Ep : Staged.Emplaces) {
            if (Ep.SlotIndex < 0 || Ep.SlotIndex >= SlotCount_ || !Ep.Data) {
                continue;
            }
            auto& Slot = Slots_[Ep.SlotIndex];
            const auto& Desc = Slot.column_desc;
            void* Real = Slot.emplace_default(Registry, Entity);
            if (Real) {
                // Replace the default-constructed registry value with the staged
                // one using type-correct move semantics, then release staging storage.
                Desc.destruct(Real);
                Desc.move_construct(Real, Ep.Data);
                Desc.destruct(Ep.Data);
                FMemory::Free(Ep.Data);
                Ep.Data = nullptr; // prevent ~FStagedEmplace from double-freeing
            }
        }
    }
    StagedEntities.clear();
    StagedEntityMap.clear();
}

bool FElementOakBridge::TryStageEmplace(elm::Entity Entity, int SlotIndex, oak_value_t* Out)
{
    if (!SystemExecutionActive) {
        return false;
    }
    // For live (non-token) entities validate liveness before staging; a script
    // may retain a stale Oak Entity handle across ticks after destruction.
    if (Entity.index < StagingBase && !Registry.alive(Entity)) {
        *Out = oak_value_none();
        return true; // consumed — caller must not fall through to direct emplace
    }

    std::lock_guard<std::mutex> Lock(SpawnMutex);

    // Register the entity in the staging map if not already there.
    // This handles both token spawns (added by SpawnEntity) and live entities
    // whose component additions must be deferred off the hot registry path.
    auto It = StagedEntityMap.find(Entity.index);
    if (It == StagedEntityMap.end()) {
        StagedEntityMap[Entity.index] = StagedEntities.size();
        StagedEntities.push_back({ Entity, {} });
        It = StagedEntityMap.find(Entity.index);
    }

    auto& Staged = StagedEntities[It->second];
    auto& Ep = Staged.Emplaces.emplace_back();
    Ep.SlotIndex = SlotIndex;

    const auto& Desc = Slots_[SlotIndex].column_desc;
    Ep.DataAlignment = static_cast<uint32>(Desc.alignment);
    Ep.Destruct = Desc.destruct;
    Ep.Data = FMemory::Malloc(Desc.element_size, Desc.alignment);
    if (Desc.default_construct) {
        Desc.default_construct(Ep.Data);
    }

    *Out = oak_native_record_new(Allocator, GetSlotOakType(SlotIndex), Ep.Data);
    return true;
}

void FElementOakBridge::RebuildViewCaches()
{
    const std::uint64_t CurrentVersion = Registry.mutation_version();
    if (CurrentVersion != LastViewCacheMutationVersion) {
        oak_bridge::rebuild_view_caches(Registry, ViewCaches, Slots_, SlotCount_);
        LastViewCacheMutationVersion = CurrentVersion;
    }
    RebuildSpatialGrid();
}

void FElementOakBridge::ExpireEntity(elm::Entity Entity)
{
    if (Entity.index >= StagingBase) {
        // Token entity: cancel the staged spawn so FlushStagedSpawns skips it.
        std::lock_guard<std::mutex> Lock(SpawnMutex);
        auto It = StagedEntityMap.find(Entity.index);
        if (It != StagedEntityMap.end()) {
            StagedEntities[It->second].bCancelled = true;
        }
        return;
    }
    std::lock_guard<std::mutex> Lock(ExpiredMutex);
    Expired.push_back(Entity);
}

void FElementOakBridge::ExpireEntityById(int Index, int Generation)
{
    ExpireEntity(elm::Entity { static_cast<std::uint32_t>(Index), static_cast<std::uint32_t>(Generation) });
}

std::vector<elm::Entity> FElementOakBridge::TakeExpired()
{
    std::lock_guard<std::mutex> Lock(ExpiredMutex);
    std::vector<elm::Entity> Result;
    Result.swap(Expired);
    return Result;
}

void FElementOakBridge::RebuildSpatialGrid()
{
    if (SpatialGrid.empty()) {
        SpatialGrid.resize(GridDim * GridDim);
    }

    for (int CellIdx : DirtyCells) {
        SpatialGrid[CellIdx].Xs.clear();
        SpatialGrid[CellIdx].Ys.clear();
    }
    DirtyCells.clear();

    auto* PosStorage = Registry.find_storage_raw(elm::component_type<FElementPosition>());
    auto* ZombieStorage = Registry.find_storage_raw(elm::component_type<FElementZombie>());
    if (!PosStorage || !ZombieStorage) { return; }

    const float HalfDim = GridDim * 0.5f;

    Registry.for_each_entity_with(elm::component_type<FElementZombie>(), [&](elm::Entity E) {
        auto* Pos = static_cast<FElementPosition*>(PosStorage->get_raw(E));
        if (!Pos) { return; }

        int Cx = static_cast<int>(std::floor(Pos->X / GridCellSize) + HalfDim);
        int Cy = static_cast<int>(std::floor(Pos->Y / GridCellSize) + HalfDim);
        Cx = FMath::Clamp(Cx, 0, GridDim - 1);
        Cy = FMath::Clamp(Cy, 0, GridDim - 1);

        int CellIdx = Cy * GridDim + Cx;
        auto& Cell = SpatialGrid[CellIdx];
        if (Cell.Xs.empty()) {
            DirtyCells.push_back(CellIdx);
        }
        Cell.Xs.push_back(Pos->X);
        Cell.Ys.push_back(Pos->Y);
    });
}

void FElementOakBridge::QueryAvoidance(elm::Entity Entity, float Range, float Strength, float& OutVx, float& OutVy)
{
    OutVx = 0.0f;
    OutVy = 0.0f;

    if (SpatialGrid.empty() || !Registry.alive(Entity)) { return; }
    auto* PosStorage = Registry.find_storage_raw(elm::component_type<FElementPosition>());
    if (!PosStorage) { return; }
    auto* Pos = static_cast<FElementPosition*>(PosStorage->get_raw(Entity));
    if (!Pos) { return; }

    const float RangeSq = Range * Range;
    const float HalfDim = GridDim * 0.5f;
    int Cx = static_cast<int>(std::floor(Pos->X / GridCellSize) + HalfDim);
    int Cy = static_cast<int>(std::floor(Pos->Y / GridCellSize) + HalfDim);

    int CellRadius = static_cast<int>(std::ceil(Range / GridCellSize));
    int X0 = FMath::Max(0, Cx - CellRadius);
    int X1 = FMath::Min(GridDim - 1, Cx + CellRadius);
    int Y0 = FMath::Max(0, Cy - CellRadius);
    int Y1 = FMath::Min(GridDim - 1, Cy + CellRadius);

    float SepX = 0.0f, SepY = 0.0f;

    // Cap how many neighbours a single zombie examines. When ~10k zombies
    // converge on the player they pack into a handful of grid cells, and an
    // uncapped scan degenerates toward O(N^2) (the game-thread spike). The
    // separation force is dominated by the nearest few neighbours, so a bounded
    // sample budget keeps the behaviour visually equivalent while making the
    // per-zombie cost O(1) regardless of how tightly the herd clumps.
    constexpr int MaxNeighborSamples = 48;
    int Sampled = 0;

    auto ScanCell = [&](int Gx, int Gy) -> bool {
        const auto& Cell = SpatialGrid[Gy * GridDim + Gx];
        const int Count = static_cast<int>(Cell.Xs.size());
        for (int K = 0; K < Count; ++K) {
            const float Dx = Cell.Xs[K] - Pos->X;
            const float Dy = Cell.Ys[K] - Pos->Y;
            const float D2 = Dx * Dx + Dy * Dy;
            if (D2 < RangeSq && D2 > 0.01f) {
                const float InvDist = FMath::InvSqrt(D2);
                const float Dist = D2 * InvDist;
                const float Proximity = FMath::Clamp((Range - Dist) / Range, 0.0f, 1.0f);
                const float Weight = Proximity * Proximity;
                SepX -= Dx * InvDist * Weight;
                SepY -= Dy * InvDist * Weight;
            }
            // Count every entity inspected, not just in-range ones: that is what
            // bounds the work when a cell holds thousands of (clamped) members.
            if (++Sampled >= MaxNeighborSamples) {
                return true;
            }
        }
        return false;
    };

    // Visit the zombie's own cell first: in a clump it holds the nearest, most
    // relevant neighbours, so the budget is spent where it matters and the push
    // stays roughly unbiased rather than skewing toward a corner of the window.
    const int CxClamped = FMath::Clamp(Cx, 0, GridDim - 1);
    const int CyClamped = FMath::Clamp(Cy, 0, GridDim - 1);

    bool Done = ScanCell(CxClamped, CyClamped);
    for (int Gy = Y0; !Done && Gy <= Y1; ++Gy) {
        for (int Gx = X0; !Done && Gx <= X1; ++Gx) {
            if (Gx == CxClamped && Gy == CyClamped) {
                continue; // already scanned as the centre cell
            }
            Done = ScanCell(Gx, Gy);
        }
    }

    const float SepLenSq = SepX * SepX + SepY * SepY;
    if (SepLenSq > 0.0001f) {
        const float InvSepLen = FMath::InvSqrt(SepLenSq);
        const float SepLen = SepLenSq * InvSepLen;
        const float Push = FMath::Min(Strength, SepLen * Strength);
        OutVx = SepX * InvSepLen * Push;
        OutVy = SepY * InvSepLen * Push;
    }
}
