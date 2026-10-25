#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TempleSeal.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class ATempleSanctumBattle;

/**
 * The wizard's seal over the sea tear (user 2026-10-24: the tear "the wizard is attempting to seal"; 2026-10-25: "Do
 * the animation for the wizard attempting to seal the sea world rupture"; docs/CHULT-TEMPLE-HEROES.md#sealing-the-sea-tear).
 * While he works it (his Seal clip), threads of sea-green light run from his staff's gem and his open hand to the tear's
 * edge and a web of light knits inward from its rim (M_TempleSealWeb over a disc that follows the rim's ragged outline).
 * The tear fights back: every 6-10 s it surges, the sea bursting up through the web in spray, tearing it back, and he is
 * wrenched toward it (SealStrain). Left alone the web frays; the beholder's cone (antimagic) shatters it. It never
 * closes in the fight: the Fireball cutscene ends that.
 */
UCLASS()
class CHUCK3D_API ATempleSeal : public AActor
{
    GENERATED_BODY()
public:
    ATempleSeal();
    virtual void Tick(float DeltaSeconds) override;
    void Setup(ATempleSanctumBattle* InBattle, UMaterialInterface* Web, UMaterialInterface* Thread, UMaterialInterface* Spark, UStaticMesh* Plane);

    /** The wizard works it this frame: his gem and his left palm (world). */
    void Work(const FVector& Gem, const FVector& Palm);
    /** Torn apart (the antimagic cone, or the wizard thrown off it): it shatters from From. */
    void Break(const FVector& From);
    /** Tests: the tear surges now. */
    void ForceSurge() { NextSurge = 0.f; }
    /** The tear's middle on the floor (world). */
    FVector GetCentre() const { return Centre; }
    float GetCoverage() const { return Coverage; }
    float GetMaxCoverage() const { return MaxCoverage; }
    bool IsWorked() const { return T - LastWork < .2f; }
    int32 GetSurges() const { return Surges; }
    int32 GetBreaks() const { return Breaks; }
    static constexpr float MaxKnit = .82f;   // it never closes in the fight
private:
    UPROPERTY() UProceduralMeshComponent* WebMesh = nullptr;
    UPROPERTY() UProceduralMeshComponent* Threads = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* Motes = nullptr;
    UPROPERTY() UPointLightComponent* Glow = nullptr;
    TWeakObjectPtr<ATempleSanctumBattle> Battle;
    FVector Centre = FVector::ZeroVector, Gem = FVector::ZeroVector, Palm = FVector::ZeroVector, CamEye = FVector::ZeroVector;
    float T = 0, LastWork = -10, Coverage = 0, MaxCoverage = 0, NextSurge = 7.f, SurgeAt = -10, BreakAt = -10, Flash = 0;
    int32 Surges = 0, Breaks = 0;
    bool bReady = false;
    FRandomStream Random;
    struct FMote { FVector Pos, Vel; FLinearColor Col; float Size, Age, Life, Grav; };
    TArray<FMote> Parts;
    void Surge();
    void Mote(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Grav = 0.f);
    FVector Rim(float Angle, float Grow, float Z = 3.f) const;
    void DrawWeb();
    void DrawThreads();
    void DrawMotes();
};
