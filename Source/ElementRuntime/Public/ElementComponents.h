#pragma once

#include "CoreMinimal.h"

struct FElementPosition {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

struct FElementVelocity {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

struct FElementHealth {
    float HP = 100.0f;
    float MaxHP = 100.0f;
};

struct FElementZombie {
    float SpeedMul = 1.0f;
    float Size = 1.0f;
};

struct FElementPlayer {};

struct FElementPlayerInput {
    float Forward = 0.0f;
    float Right = 0.0f;
    float Up = 0.0f;
};

struct FElementCameraState {
    float Yaw = 0.0f;
};

struct FElementFlame {
    bool Active = false;
    float Range = 500.0f;
    float Angle = 45.0f;
};

// Per-tick cache of the player's flame cone, written once by the
// update_flame_frustum system so the per-zombie flame_damage system can skip
// the trig and player-state lookups. Mirrors the FlameFrustum oak record.
struct FElementFlameFrustum {
    bool Active = false;
    float OX = 0.0f;
    float OY = 0.0f;
    float DX = 0.0f;
    float DY = 0.0f;
    float CosHalf = 0.0f;
    float RangeSq = 0.0f;
};

struct FElementGameState {
    int Wave = 0;
    float SpawnTimer = 0.0f;
    int Kills = 0;
    bool Initialized = false;
    int SpawnsRemaining = 0;
};
