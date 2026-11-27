#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyDeepCrossing.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
struct FMapSurf;

// The Deepwood's climbs (Claude, 2026-11-26; the region plan's "brachiation, slide-jumps over ground roots", and
// docs/regions/FEYWILD-JELLYFISH.md, decision 6: crossings the jellyfish cannot get round, more of them beyond).
//
// The Deepwood's hidden way to Highmeadow (its north-west corner) is closed off by two arcs round its mouth, each from the
// district's west wall to its north wall (the district's invisible bounds), so neither can be walked round:
//  - the outer arc (75 m from the mouth): a wall of great ground roots. Where the road meets it a corridor runs through it:
//    a waist-high root to vault at a run, a strip of thorns to clear with the sprint leap, a second root to vault (the
//    "slide-jumps over ground roots": his speed vault and his four-legged leap);
//  - the inner arc (45 m from the mouth): a thicket of brambles; where the road meets it, a bed of thorns under five ivy
//    hoops (the maze's: their crowns the grips) to swing across.
// Thorns prick and slow him as the maze's do. Both ways work in either direction. The jellyfish refuse them
// (AFeyJelly::Barriers) and two wait either side of each.
namespace FeyDeep
{
    enum EKind { Roots = 0, Thorns = 1 };
    struct FCrossing
    {
        int32 Kind = Roots;
        FVector2D Mouth = FVector2D::ZeroVector;    // the arc's centre: the mouth of the way to Highmeadow
        float Radius = 0.f;
        FVector2D Centre = FVector2D::ZeroVector;   // where the road meets the arc
        FVector2D Along = FVector2D(1, 0);         // the road's way there, outward (away from the mouth)
        float HalfLen = 0.f, HalfWidth = 170.f;     // the corridor through it
        TArray<FVector2D> Line;                     // the barrier, wall to wall (beyond both walls a little)
        FVector2D SideIn = FVector2D::ZeroVector, SideOut = FVector2D::ZeroVector;   // 15 m either side along the road
        FVector2D Perp() const { return FVector2D(-Along.Y, Along.X); }
        float S(const FVector2D& P) const { return FVector2D::DotProduct(P - Centre, Along); }
        float X(const FVector2D& P) const { return FVector2D::DotProduct(P - Centre, Perp()); }
        FVector2D At(float Sv, float Xv) const { return Centre + Along * Sv + Perp() * Xv; }
    };
    const TArray<FCrossing>& Crossings();
    constexpr float VaultAt = 420.f, VaultR = 23.f, StripHalf = 80.f, BedHalf = 240.f, WallTall = 430.f, WallHalf = 60.f;
    constexpr int32 Hoops = 5;
    /** Among thorns, on the ground (local feet). */
    bool InThorns(const FVector& FeetLocal);
    /** (FeyMapTest's route walk) Walking from At toward Toward across a crossing: where he comes down on its far side. */
    bool WalkerHop(const FVector2D& At, const FVector2D& Toward, FVector2D& OutLand);
}

UCLASS()
class CHUCK3D_API AFeyDeepCrossings : public AActor
{
    GENERATED_BODY()
public:
    AFeyDeepCrossings();
    static AFeyDeepCrossings* Spawn(UWorld* World);
    virtual void Tick(float DeltaSeconds) override;
    static int32 Pricks;
    static int32 Built;   // crossings built (2)
private:
    UPROPERTY() TArray<UProceduralMeshComponent*> Meshes;
    float NextPrick = 0.f, Clock = 0.f;
    void Build();
    void Emit(const FMapSurf& S, UMaterialInterface* M, bool bCollide, bool bVisible, bool bShadow);
};
