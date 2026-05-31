#include "ElementZombiesActor.h"

#include "ElementComponents.h"
#include "ElementOakBridge.h"
#include "ElementRuntime.h"

#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

#include <vector>

namespace {

static const FKey GKeyMapping[] = {
    EKeys::W,
    EKeys::S,
    EKeys::A,
    EKeys::D,
    EKeys::E,
    EKeys::Q,
    EKeys::SpaceBar,
    EKeys::LeftControl,
    EKeys::Up,
    EKeys::Down,
    EKeys::Left,
    EKeys::Right,
    EKeys::P,
};
static_assert(UE_ARRAY_COUNT(GKeyMapping) == static_cast<int32>(EElementInputKey::Count));

FString ResolveOakScriptFileName(const AElementZombiesActor& Owner)
{
    return Owner.GameScriptFileName.IsEmpty() ? FString(TEXT("zombies.oak")) : Owner.GameScriptFileName;
}

FString ResolveOakScriptPath(const AElementZombiesActor& Owner)
{
    const FString ScriptName = ResolveOakScriptFileName(Owner);
    if (FPaths::IsRelative(ScriptName)) {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(),
            TEXT("ElementRuntime"),
            TEXT("Content"),
            TEXT("Scripts"),
            ScriptName));
    }
    return ScriptName;
}

// Sentinel placed far below the world so the GPU clips it immediately.
static const FTransform GHiddenTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -1e6f), FVector::ZeroVector);

// Grow-only pool: never remove instances mid-frame. Excess slots are moved to
// GHiddenTransform so the GPU discards them cheaply. This avoids per-frame
// render-state rebuilds that fire on every RemoveInstance call.
void UpdateISM(UInstancedStaticMeshComponent* ISM,
               TArray<FTransform>& Instances)
{
    if (!ISM) { return; }

    const int32 Live    = Instances.Num();
    const int32 Current = ISM->GetInstanceCount();

    if (Live > Current) {
        for (int32 I = Current; I < Live; ++I) {
            ISM->AddInstance(GHiddenTransform, /*bWorldSpace=*/true);
        }
    }

    // Pad the active list so BatchUpdate covers every allocated slot.
    while (Instances.Num() < Current) {
        Instances.Add(GHiddenTransform);
    }

    ISM->BatchUpdateInstancesTransforms(0, Instances, /*bWorldSpace=*/true,
                                        /*bMarkRenderStateDirty=*/true,
                                        /*bTeleport=*/true);
}

} // namespace

struct FElementBoidsWorld {
    explicit FElementBoidsWorld(AElementZombiesActor& InOwner)
        : Owner(InOwner)
        , Bridge(Registry)
    {
        SetupInputContext();
        LoadOakScript();
        BuildSystems();
    }

    ~FElementBoidsWorld() = default;

    void Tick(float InDeltaSeconds)
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::Tick);
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::SetDeltaSeconds);
            DeltaSeconds = InDeltaSeconds;
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::UpdateInputContextStep);
            UpdateInputContext();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::ActivateBridge);
            Bridge.Activate();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::RebuildViewCaches);
            Bridge.RebuildViewCaches();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::RunSystems);
            Bridge.BeginSystemExecution();
            Scheduler.run(Systems, Registry);
            Bridge.EndSystemExecution();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::FlushStagedSpawns);
            Bridge.FlushStagedSpawns();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::ApplyQueuedDamage);
            Bridge.ApplyQueuedDamage();
        }
        {
            // Expire zombies whose HP reached zero from damage_entity calls this tick.
            // Oak's zombie_death system only sees HP during the scheduler pass (before
            // damage is applied), so deaths from queued damage need a native post-damage sweep.
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::ExpireDeadEntities);
            Registry.view([this](elm::Entity Entity, const FElementHealth& HP, const FElementZombie&) {
                if (HP.HP <= 0.0f) {
                    Bridge.ExpireEntity(Entity);
                }
            });
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::DestroyExpiredEntitiesStep);
            DestroyExpiredEntities();
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::SyncVisualsStep);
            SyncVisuals();
        }
    }

    FVector GetPlayerPosition() const
    {
        FVector Result = FVector::ZeroVector;
        Registry.view([&](const FElementPlayer&, const FElementPosition& Pos) {
            Result = FVector(Pos.X, Pos.Y, Pos.Z);
        });
        return Result;
    }

    FElementGameState GetGameState() const
    {
        FElementGameState Result;
        Registry.view([&](const FElementGameState& GS) { Result = GS; });
        return Result;
    }

private:
    void SetupInputContext()
    {
        auto& Ctx = Bridge.GetInputContext();
        Ctx.CameraYaw = Owner.CameraYaw;
        Ctx.WaveEnemyCount = Owner.WaveEnemyCount;
        Ctx.WaveSpawnRadius = Owner.WaveSpawnRadius;
        Ctx.FlameRange = Owner.FlameRange;
        Ctx.FlameAngle = Owner.FlameAngle;
        Ctx.FlameDamage = Owner.FlameDamage;
        Ctx.ZombieSpeed = Owner.ZombieSpeed;
    }

    void UpdateInputContext()
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::UpdateInputContext);
        auto& Ctx = Bridge.GetInputContext();
        Ctx.CameraYaw = Owner.CameraYaw;
        Ctx.WaveEnemyCount = Owner.WaveEnemyCount;
        Ctx.WaveSpawnRadius = Owner.WaveSpawnRadius;
        Ctx.FlameRange = Owner.FlameRange;
        Ctx.FlameAngle = Owner.FlameAngle;
        Ctx.FlameDamage = Owner.FlameDamage;
        Ctx.ZombieSpeed = Owner.ZombieSpeed;

        APlayerController* PC = Owner.GetWorld() ? Owner.GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC) {
            Ctx.IsKeyDown = [PC](EElementInputKey Key) -> bool {
                const int32 KeyIndex = static_cast<int32>(Key);
                if (KeyIndex < 0 || KeyIndex >= UE_ARRAY_COUNT(GKeyMapping)) {
                    return false;
                }
                return PC->IsInputKeyDown(GKeyMapping[KeyIndex]);
            };
        } else {
            Ctx.IsKeyDown = nullptr;
        }
    }

    void LoadOakScript()
    {
        const FString ScriptPath = ResolveOakScriptPath(Owner);
        auto Utf8Path = StringCast<ANSICHAR>(*ScriptPath);
        if (!Bridge.LoadProgram(Utf8Path.Get())) {
            UE_LOG(LogTemp, Error, TEXT("Element: Failed to load Oak program from '%s'."), *ScriptPath);
        }
    }

    void BuildSystems()
    {
        // Build into a scratch vector and only swap it into the live schedule
        // once validation succeeds. This keeps Systems empty (rather than
        // half-populated) if the bridge rejects an unsafe schedule, so the
        // "Refusing" log below is truthful and run() never executes a partially
        // built or invalid system set.
        std::vector<elm::System> BuiltSystems;
        if (!Bridge.BuildSystems(BuiltSystems, &DeltaSeconds)) {
            UE_LOG(LogTemp, Error, TEXT("Element: Refusing to build an unsafe Oak system schedule."));
            Systems.clear();
            Scheduler.prepare(Systems);
            return;
        }
        Systems = std::move(BuiltSystems);
        Scheduler.prepare(Systems);
    }

    void DestroyExpiredEntities()
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::DestroyExpiredEntities);
        auto Taken = Bridge.TakeExpired();
        int KillsThisFrame = 0;
        for (const auto Entity : Taken) {
            if (Registry.has<FElementZombie>(Entity)) {
                ++KillsThisFrame;
            }
            Registry.destroy(Entity);
        }
        if (KillsThisFrame > 0) {
            Registry.view([KillsThisFrame](FElementGameState& GS) {
                GS.Kills += KillsThisFrame;
            });
        }
    }

    void SyncVisuals()
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(ElementBoids::SyncVisuals);
        ZombieTransforms.Reset(FMath::Max(ZombieTransforms.Num(), Owner.WaveEnemyCount));
        PlayerInstances.Reset(1);

        const FTransform& ActorTransform = Owner.GetActorTransform();
        const float Scale = Owner.ZombieScale;

        FVector PlayerPos = FVector::ZeroVector;
        Registry.view([&](const FElementPlayer&, const FElementPosition& Pos) {
            PlayerPos = FVector(Pos.X, Pos.Y, Pos.Z);
        });

        Registry.view([&](const FElementZombie& Zombie, const FElementPosition& Pos, const FElementHealth& HP) {
            const FVector WorldPos = ActorTransform.TransformPosition(FVector(Pos.X, Pos.Y, Pos.Z));
            const FVector ToPlayer = PlayerPos - FVector(Pos.X, Pos.Y, Pos.Z);
            FRotator Rotation = FRotator::ZeroRotator;
            if (!ToPlayer.IsNearlyZero()) {
                // Cone mesh tip is along local +Y; subtract 90° so Y aligns with the player direction.
                Rotation = ToPlayer.GetSafeNormal2D().Rotation();
                Rotation.Yaw -= 90.0f;
            }
            const float HealthFrac = FMath::Clamp(HP.HP / HP.MaxHP, 0.2f, 1.0f);
            ZombieTransforms.Add(FTransform(Rotation, WorldPos, FVector(Scale * Zombie.Size * HealthFrac)));
        });

        Registry.view([&](const FElementPlayer&, const FElementPosition& Pos) {
            const FVector WorldPos = ActorTransform.TransformPosition(FVector(Pos.X, Pos.Y, Pos.Z));
            PlayerInstances.Add(FTransform(FRotator::ZeroRotator, WorldPos, FVector(1.5f)));
        });

        UpdateISM(Owner.ZombieISM, ZombieTransforms);
        UpdateISM(Owner.PlayerISM, PlayerInstances);
    }

    AElementZombiesActor& Owner;
    elm::Registry Registry;
    FElementOakBridge Bridge;
    FElementRuntimeScheduler Scheduler;
    std::vector<elm::System> Systems;
    TArray<FTransform> ZombieTransforms;
    TArray<FTransform> PlayerInstances;
    float DeltaSeconds = 0.0f;
};

AElementZombiesActor::AElementZombiesActor()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = CameraDistance;
    SpringArm->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
    SpringArm->bDoCollisionTest = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    ZombieISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ZombieISM"));
    ZombieISM->SetupAttachment(RootComponent);
    ZombieISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ZombieISM->SetMobility(EComponentMobility::Movable);
    ZombieISM->SetCastShadow(false);

    PlayerISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PlayerISM"));
    PlayerISM->SetupAttachment(RootComponent);
    PlayerISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlayerISM->SetMobility(EComponentMobility::Movable);
    PlayerISM->SetCastShadow(false);

    if (ConeMesh.Succeeded()) {
        ZombieISM->SetStaticMesh(ConeMesh.Object);
    }
    if (SphereMesh.Succeeded()) {
        PlayerISM->SetStaticMesh(SphereMesh.Object);
    }
}

AElementZombiesActor::~AElementZombiesActor() = default;

void AElementZombiesActor::BeginPlay()
{
    Super::BeginPlay();

    SpringArm->TargetArmLength = CameraDistance;
    SpringArm->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
    RestartSimulation();

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) {
        if (APawn* DefaultPawn = PC->GetPawn()) {
            DefaultPawn->SetActorHiddenInGame(true);
        }
        PC->SetViewTarget(this);
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AElementZombiesActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

#if WITH_EDITOR
    if (bReloadOakScriptOnSave) {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now >= NextOakScriptReloadCheckTime) {
            NextOakScriptReloadCheckTime = Now + 0.25;
            ReloadOakScriptIfChanged();
        }
    }
#endif

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PC) {
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        PC->GetInputMouseDelta(MouseX, MouseY);

        CameraYaw += MouseX * CameraOrbitSensitivity;
        CameraPitch = FMath::Clamp(CameraPitch - MouseY * CameraOrbitSensitivity, -89.0f, 89.0f);

        if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) {
            CameraDistance = FMath::Max(CameraDistance - CameraZoomSpeed, CameraMinDistance);
        }
        if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) {
            CameraDistance = FMath::Min(CameraDistance + CameraZoomSpeed, CameraMaxDistance);
        }
        if (PC->WasInputKeyJustPressed(EKeys::P)) {
            bSimulationPaused = !bSimulationPaused;
        }

        SpringArm->TargetArmLength = CameraDistance;
        SpringArm->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
    }

    if (Simulation) {
        if (!bSimulationPaused) {
            Simulation->Tick(DeltaSeconds);
        }
        SpringArm->SetWorldLocation(GetActorTransform().TransformPosition(Simulation->GetPlayerPosition()));

        const FElementGameState GS = Simulation->GetGameState();
        if (GEngine) {
            GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow,
                FString::Printf(TEXT("Wave: %d   Kills: %d"), GS.Wave, GS.Kills));
        }
    }
}

void AElementZombiesActor::RestartSimulation()
{
    Simulation.Reset();

    if (ZombieISM) {
        ZombieISM->ClearInstances();
        ZombieISM->SetVisibility(true);
    }
    if (PlayerISM) {
        PlayerISM->ClearInstances();
    }

    Simulation = MakeShared<FElementBoidsWorld>(*this);
    bSimulationPaused = false;
    UpdateLoadedOakScriptTimestamp();
    NextOakScriptReloadCheckTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.25 : 0.25;
}

bool AElementZombiesActor::ReloadOakScriptIfChanged()
{
    const FString ScriptPath = ResolveOakScriptPath(*this);
    const FDateTime WriteTime = IFileManager::Get().GetTimeStamp(*ScriptPath);
    if (WriteTime == FDateTime::MinValue()) {
        return false;
    }

    if (!LoadedOakScriptPath.Equals(ScriptPath, ESearchCase::IgnoreCase)) {
        LoadedOakScriptPath = ScriptPath;
        LoadedOakScriptWriteTime = WriteTime;
        return false;
    }

    if (LoadedOakScriptWriteTime != FDateTime::MinValue() && WriteTime > LoadedOakScriptWriteTime) {
        UE_LOG(LogTemp, Log, TEXT("Element: Reloading Oak script '%s'."), *ScriptPath);
        RestartSimulation();
        return true;
    }

    return false;
}

void AElementZombiesActor::UpdateLoadedOakScriptTimestamp()
{
    LoadedOakScriptPath = ResolveOakScriptPath(*this);
    LoadedOakScriptWriteTime = IFileManager::Get().GetTimeStamp(*LoadedOakScriptPath);
}
