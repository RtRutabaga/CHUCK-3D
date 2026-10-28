#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuckFlames.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

// Every torch, brazier, fireplace, forge, lamp and candle in the game (Claude, 2026-10-27; docs/FIRE.md). Each fire
// placed with AddDockFlame (DockFire.h) is drawn here, as the temple's fire jets are (ChultTempleJetFire.cpp): on
// instanced quads facing the camera with Unity Labs' CC0 Houdini flipbooks (Tools/create_flame_materials.py):
//  - two or three flame sheets per fire, each on its own frame and speed of the simulated flame (Flame02), side by
//    side and of different heights, so the fire has body and never repeats as a loop;
//  - the flame breathes: its height wavers on two slow beats of its own;
//  - sparks leave it, rise on the heat with a wander, and cool from yellow-white to red before they go out;
//  - thin smoke leaves its tip, rising, spreading and thinning (WispySmoke01).
// Only the fires nearest the camera are simulated (the nearest Near get sparks and smoke; out to FarDistance a
// single sheet); a fire whose owner is hidden (the docks while in Chult, and so on) is not drawn.
namespace ChuckFlames
{
    // The nearest Near fires within NearDistance get every sheet, sparks and smoke; out to Far fires and FarDistance, one sheet.
    constexpr int32 Near = 40, Far = 80, Sheets = 3, SmokePer = 6, SparksPer = 8;
    constexpr float NearDistance = 4500.f, FarDistance = 16000.f;
}

UCLASS()
class CHUCK3D_API AChuckFlames : public AActor
{
    GENERATED_BODY()
public:
    AChuckFlames();
    static AChuckFlames* Get(UWorld* World);
    void Add(AActor* FireOwner, const FVector& Base, float Width, float Height);
    virtual void Tick(float DeltaSeconds) override;
    int32 GetFlameCount() const { return Flames.Num(); }
    int32 GetMissingMaterials() const { return MissingMaterials; }
    int32 GetDrawnLastFrame() const { return DrawnLast; }
    int32 GetSparksLastFrame() const { return SparksLast; }
private:
    struct FMote { FVector P, V; float Age = 0, Life = 1, Size0 = 1, Size1 = 1, Frame = 0, FrameRate = 0, Spin = 0, SpinRate = 0, Seed = 0; };
    struct FFlame
    {
        TWeakObjectPtr<AActor> Owner;
        FVector Base; float Width = 20, Height = 30;
        float Seed = 0, Frames[ChuckFlames::Sheets] = {}, Rates[ChuckFlames::Sheets] = {};
        float SparkCarry = 0, SmokeCarry = 0;
        TArray<FMote> Sparks, Smoke;
        FRandomStream Rand;
    };
    TArray<FFlame> Flames;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> FlameFx;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SmokeFx;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SparkFx;
    int32 MissingMaterials = 0, DrawnLast = 0, SparksLast = 0;
    UInstancedStaticMeshComponent* Pool(const TCHAR* Name, const TCHAR* Material, int32 Count);
    void Simulate(FFlame& F, float Dt, float Time);
};
