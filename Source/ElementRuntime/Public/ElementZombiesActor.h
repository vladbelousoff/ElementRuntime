#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/DateTime.h"
#include "Templates/SharedPointer.h"

#include "ElementZombiesActor.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
class USpringArmComponent;
struct FElementZombiesWorld;

UCLASS()
class ELEMENTRUNTIME_API AElementZombiesActor : public AActor {
    GENERATED_BODY()

public:
    AElementZombiesActor();
    ~AElementZombiesActor() override;

    void BeginPlay() override;
    void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Element")
    FString GameScriptFileName = TEXT("zombies.oak");

    UPROPERTY(EditAnywhere, Category = "Element")
    bool bReloadOakScriptOnSave = true;

    UPROPERTY(EditAnywhere, Category = "Element")
    int32 WaveEnemyCount = 10000;

    UPROPERTY(EditAnywhere, Category = "Element")
    float WaveSpawnRadius = 15000.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Flamethrower")
    float FlameRange = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Flamethrower")
    float FlameAngle = 60.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Flamethrower")
    float FlameDamage = 150.0f;

    UPROPERTY(EditAnywhere, Category = "Element")
    float ZombieScale = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Element")
    float ZombieSpeed = 120.0f;

    UPROPERTY(VisibleAnywhere, Category = "Element")
    UInstancedStaticMeshComponent* ZombieISM = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Element")
    UInstancedStaticMeshComponent* PlayerISM = nullptr;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraDistance = 3000.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraMinDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraMaxDistance = 15000.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraZoomSpeed = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraOrbitSensitivity = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraPitch = -45.0f;

    UPROPERTY(EditAnywhere, Category = "Element|Camera")
    float CameraYaw = 45.0f;

    UPROPERTY(VisibleAnywhere, Category = "Element")
    USpringArmComponent* SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Element")
    UCameraComponent* Camera = nullptr;

private:
    void RestartSimulation();
    bool ReloadOakScriptIfChanged();
    void UpdateLoadedOakScriptTimestamp();

    TSharedPtr<FElementZombiesWorld> Simulation;
    bool bSimulationPaused = false;
    FString LoadedOakScriptPath;
    FDateTime LoadedOakScriptWriteTime = FDateTime::MinValue();
    double NextOakScriptReloadCheckTime = 0.0;
};
