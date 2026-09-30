#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstralSummon.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 * The astral light Chuck vanishes into and is summoned out of (user
 * 2026-09-30: a fey summon who can't die; GAME-BIBLE.md: at zero Sanity he
 * quietly disappears and returns at an Astral Anchor, and the Astral Sea is
 * peaceful, ancient, quiet). Quiet rather than flashy: a faint rune circle
 * on the ground, motes of starlight spiralling up, a soft column that peaks
 * as he appears (or is gone), then everything fades. Tools/build_astral_fx.py.
 */
UCLASS()
class CHUCK3D_API AAstralSummon : public AActor
{
    GENERATED_BODY()
public:
    AAstralSummon();
    virtual void Tick(float DeltaSeconds) override;
    /** Summon: light gathers, peaks at PeakTime (he appears), fades. Vanish: rises from him, peaks (he's gone), fades faster. */
    static AAstralSummon* Start(UWorld* World, const FVector& Ground, bool bSummon);
    static constexpr float SummonPeak = 1.f;     // s: Chuck appears
    static constexpr float SummonLength = 2.6f;
    static constexpr float VanishPeak = .65f;    // s: Chuck is gone
    static constexpr float VanishLength = 1.5f;
    static int32 GetStarted() { return Started; }
private:
    UPROPERTY() UStaticMeshComponent* Circle;
    UPROPERTY() UStaticMeshComponent* Column;
    UPROPERTY() UInstancedStaticMeshComponent* Motes;
    UPROPERTY() UMaterialInstanceDynamic* CircleGlow;
    UPROPERTY() UMaterialInstanceDynamic* ColumnGlow;
    UPROPERTY() UMaterialInstanceDynamic* MoteGlow;
    UPROPERTY() USoundBase* SummonSound;
    UPROPERTY() USoundBase* VanishSound;
    bool bSummon = true;
    float Clock = 0;
    struct FMote { float Angle; float Radius; float Speed; float Rise; float Delay; };
    TArray<FMote> MoteSeeds;
    static int32 Started;
};
