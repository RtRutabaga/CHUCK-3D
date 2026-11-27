// The Deepwood's climbs (Claude, 2026-11-26). See FeyDeepCrossing.h.
#include "FeyDeepCrossing.h"
#include "FeyMap.h"
#include "FeyMesh.h"
#include "FeyLife.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace
{
    constexpr float OuterR = 7500.f, InnerR = 4500.f;
    const FColor WoodCol = FLinearColor(.22f, 1.f, 0.f, 1.f).ToFColor(false), CaneCol = FLinearColor(0.f, 1.f, 0.f, 1.f).ToFColor(false);
    const FColor IvyCol = FLinearColor(.16f, .34f, .1f, 0.f).ToFColor(false), IvyDark = FLinearColor(.1f, .24f, .07f, 0.f).ToFColor(false);
    const FColor ThornCol = FLinearColor(.78f, .66f, .5f, 0.f).ToFColor(false), BerryCol = FLinearColor(.2f, .03f, .1f, 0.f).ToFColor(false), BLeaf = FLinearColor(.17f, .27f, .08f, 0.f).ToFColor(false);
    float Ground(const FVector2D& P) { return FeyMap::LandZ(FeyMap::Deepwood, P.X, P.Y); }
    FVector G3(const FVector2D& P, float Up = 0.f) { return FVector(P.X, P.Y, Ground(P) + Up); }
}

int32 AFeyDeepCrossings::Pricks = 0;
int32 AFeyDeepCrossings::Built = 0;

const TArray<FeyDeep::FCrossing>& FeyDeep::Crossings()
{
    static const TArray<FCrossing> All = []()
    {
        TArray<FCrossing> Out;
        int32 Way = -1;
        for (int32 Th = 0; Th < FeyMap::Thresholds; ++Th) if (FeyMap::Threshold(Th).A == FeyMap::Deepwood && FeyMap::Threshold(Th).B == FeyMap::Highmeadow) Way = Th;
        if (Way < 0) return Out;
        const FVector2D Mouth = FeyMap::Threshold(Way).PA;
        const TArray<FVector2D>& Road = FeyMap::RoadOf(FeyMap::Deepwood, Way);
        const FeyMap::FDistrict& Ds = FeyMap::District(FeyMap::Deepwood);
        for (int32 K = 0; K < 2; ++K)
        {
            FCrossing C; C.Kind = K == 0 ? Roots : Thorns; C.Mouth = Mouth; C.Radius = K == 0 ? OuterR : InnerR;
            C.HalfLen = C.Kind == Roots ? 560.f : 380.f;
            // Where the road (from the mouth inward) first passes the arc.
            for (int32 I = 0; I + 1 < Road.Num(); ++I)
            {
                const float D0 = static_cast<float>(FVector2D::Distance(Road[I], Mouth)), D1 = static_cast<float>(FVector2D::Distance(Road[I + 1], Mouth));
                if (D0 <= C.Radius && D1 > C.Radius)
                {
                    float Lo = 0.f, Hi = 1.f;
                    for (int32 It = 0; It < 30; ++It) { const float M = (Lo + Hi) * .5f; (FVector2D::Distance(FMath::Lerp(Road[I], Road[I + 1], M), Mouth) < C.Radius ? Lo : Hi) = M; }
                    C.Centre = FMath::Lerp(Road[I], Road[I + 1], Lo); C.Along = (Road[I + 1] - Road[I]).GetSafeNormal();
                    break;
                }
            }
            // The arc, from beyond the west wall round to beyond the north wall (the district's invisible bounds).
            const float Aw = -FMath::Acos(FMath::Clamp((Ds.X0 - Mouth.X) / C.Radius, -1.f, 1.f)), Ae = FMath::Asin(FMath::Clamp((Ds.Y1 - Mouth.Y) / C.Radius, -1.f, 1.f));
            const float A0 = Aw - 400.f / C.Radius, A1 = Ae + 400.f / C.Radius;
            const int32 N = FMath::Max(8, FMath::CeilToInt((A1 - A0) * C.Radius / 150.f));
            for (int32 I = 0; I <= N; ++I) { const float A = FMath::Lerp(A0, A1, static_cast<float>(I) / N); C.Line.Add(Mouth + FVector2D(FMath::Cos(A), FMath::Sin(A)) * C.Radius); }
            C.SideOut = C.At(C.HalfLen + 1000.f, 0.f); C.SideIn = C.At(-C.HalfLen - 1000.f, 0.f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DEEP_CROSSING kind=%d centre=%.0f,%.0f along=%.2f,%.2f arc_m=%.0f"), C.Kind, C.Centre.X, C.Centre.Y, C.Along.X, C.Along.Y, (A1 - A0) * C.Radius / 100.f);
            Out.Add(C);
        }
        return Out;
    }();
    return All;
}

bool FeyDeep::InThorns(const FVector& Feet)
{
    for (const FCrossing& C : Crossings())
    {
        const FVector2D P(Feet);
        const float S = C.S(P), X = C.X(P), Half = C.Kind == Roots ? StripHalf : BedHalf;
        if (FMath::Abs(X) < C.HalfWidth && S > -Half && S < Half) return Feet.Z < Ground(P) + 35.f;
    }
    return false;
}

bool FeyDeep::WalkerHop(const FVector2D& At, const FVector2D& Toward, FVector2D& OutLand)
{
    for (const FCrossing& C : Crossings())
    {
        const float Sa = C.S(At), St = C.S(Toward);
        if (FMath::Abs(C.X(At)) > 500.f) continue;
        // At its near end, the way on beyond its far end: over to the far side.
        if (St > Sa && Sa > -C.HalfLen - 300.f && Sa < -C.HalfLen + 80.f && St > C.HalfLen) { OutLand = C.At(C.HalfLen + 120.f, 0.f); return true; }
        if (St < Sa && Sa < C.HalfLen + 300.f && Sa > C.HalfLen - 80.f && St < -C.HalfLen) { OutLand = C.At(-C.HalfLen - 120.f, 0.f); return true; }
    }
    return false;
}

AFeyDeepCrossings::AFeyDeepCrossings()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .05f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AFeyDeepCrossings* AFeyDeepCrossings::Spawn(UWorld* World)
{
    if (!World) return nullptr;
    FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFeyDeepCrossings* A = World->SpawnActor<AFeyDeepCrossings>(FeyWorld(FVector::ZeroVector), FRotator::ZeroRotator, Sp);
    if (A) A->Build();
    return A;
}

void AFeyDeepCrossings::Emit(const FMapSurf& S, UMaterialInterface* M, bool bCollide, bool bVisible, bool bShadow)
{
    if (S.V.Num() == 0) return;
    auto* Pm = NewObject<UProceduralMeshComponent>(this); Pm->SetupAttachment(GetRootComponent()); Pm->bUseAsyncCooking = false; Pm->RegisterComponent();
    Pm->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, bCollide);
    if (M) Pm->SetMaterial(0, M);
    Pm->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bCollide) Pm->SetCollisionProfileName(TEXT("BlockAll"));
    Pm->SetVisibility(bVisible); Pm->SetHiddenInGame(!bVisible);
    Pm->SetCastShadow(bShadow);
    Meshes.Add(Pm);
}

void AFeyDeepCrossings::Build()
{
    UMaterialInterface* MRoot = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot"));
    UMaterialInterface* MPlant = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyPlant.M_FeyPlant"));
    UMaterialInterface* MHedge = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyHedge.M_FeyHedge"));
    FMapSurf Walls, Roots, Canes, Bits, Hedge, Lath, Ivy;
    FRandomStream Rp(1126);
    // (the maze's little makers: a leaf card, a thorn, an ivy stem)
    auto LeafCard = [&](FMapSurf& Sf, const FVector& P0, const FVector& Dir, const FVector& Hint, float Len, float Wd, const FColor& Col)
    {
        FVector Sd = FVector::CrossProduct(Hint, Dir).GetSafeNormal(); if (Sd.IsNearlyZero()) Sd = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
        const FVector Nn = FVector::CrossProduct(Sd, Dir).GetSafeNormal();
        for (const float F : {1.f, -1.f})
        {
            const int32 B = Sf.V.Num();
            for (int32 U = 0; U <= 2; ++U) for (int32 J = 0; J <= 2; ++J)
            { const float Uf = U / 2.f, W = Wd * FMath::Sin(PI * FMath::Max(Uf, .1f)) * (J - 1.f); Sf.Vert(P0 + Dir * (Uf * Len) + Sd * W + Nn * (F * .15f + .8f * Uf * Uf), Nn * F, FVector2D(Uf, J * .5f), Col); }
            for (int32 U = 0; U < 2; ++U) for (int32 J = 0; J < 2; ++J) { const int32 Q = B + U * 3 + J; if (F > 0.f) Sf.Quad(Q, Q + 1, Q + 4, Q + 3); else Sf.Quad(Q, Q + 3, Q + 4, Q + 1); }
        }
    };
    auto Thorn = [&](FMapSurf& Sf, const FVector& Bs, const FVector& Out, float Len, const FColor& Col)
    {
        FVector Ax = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal(); if (Ax.IsNearlyZero()) Ax = FVector::ForwardVector;
        const FVector Ay = FVector::CrossProduct(Out, Ax).GetSafeNormal(), Tip = Bs + Out * Len; FVector Q[3];
        for (int32 K = 0; K < 3; ++K) Q[K] = Bs + (Ax * FMath::Cos(2 * PI * K / 3) + Ay * FMath::Sin(2 * PI * K / 3)) * Len * .26f;
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector& A = Q[K]; const FVector& B = Q[(K + 1) % 3]; FVector Nn = FVector::CrossProduct(B - A, Tip - A).GetSafeNormal();
            if (FVector::DotProduct(Nn, (A + B + Tip) / 3.f - Bs) < 0.f) Nn = -Nn;
            Sf.TriFacing(Sf.Vert(A, Nn, FVector2D(0, 0), Col), Sf.Vert(B, Nn, FVector2D(1, 0), Col), Sf.Vert(Tip, Nn, FVector2D(.5f, 1), Col), Nn);
        }
    };
    auto IvyStem = [&](TArrayView<const FVector> Pts, const FVector& Hint, int32 Leaves)
    {
        Lath.Pipe(Pts, .9f, .6f, 4, 2, WoodCol, Rp.FRand() * 9.f);
        for (int32 K = 0; K < Leaves; ++K)
        {
            const float U = Rp.FRand() * (Pts.Num() - 1); const int32 I0 = FMath::Min(int32(U), Pts.Num() - 2);
            const FVector P = FMath::Lerp(Pts[I0], Pts[I0 + 1], U - I0);
            const FVector Dir = (Rp.VRand() * .7f + FVector(0, 0, -.6f) + (Pts[I0 + 1] - Pts[I0]).GetSafeNormal() * .3f).GetSafeNormal();
            LeafCard(Ivy, P, Dir, Hint, Rp.FRandRange(6.f, 10.f), Rp.FRandRange(3.f, 4.5f), Rp.FRand() < .4f ? IvyDark : IvyCol);
        }
    };
    // A bramble cane from B0 arching out along Dh, thorned, now and then a leaf or berries.
    auto Cane = [&](const FVector& B0, const FVector& Dh, float Reach, float Hgt, float R0)
    {
        const FVector Pts[] = {B0, B0 + Dh * Reach * .25f + FVector(0, 0, Hgt * .85f), B0 + Dh * Reach * .65f + FVector(0, 0, Hgt), B0 + Dh * Reach + FVector(0, 0, Hgt * .35f), B0 + Dh * Reach * 1.12f + FVector(0, 0, -4.f)};
        Canes.Pipe(Pts, R0, R0 * .4f, 5, 2, CaneCol, Rp.FRand() * 9.f);
        for (int32 T = 0; T < 5; ++T)
        {
            const float U = Rp.FRandRange(.2f, 3.6f); const int32 I0 = int32(U); const FVector Pp = FMath::Lerp(Pts[I0], Pts[I0 + 1], U - I0);
            FVector O = Rp.VRand(); O.Z = FMath::Abs(O.Z) * .6f; Thorn(Bits, Pp, O.GetSafeNormal(), Rp.FRandRange(4.f, 7.f), ThornCol);
        }
        if (Rp.FRand() < .5f) LeafCard(Bits, Pts[1 + Rp.RandRange(0, 2)], (Dh + FVector(0, 0, .5f) + Rp.VRand() * .4f).GetSafeNormal(), FVector::UpVector, Rp.FRandRange(7.f, 11.f), 3.5f, BLeaf);
        if (Rp.FRand() < .14f) { const FVector Bp = Pts[3] + FVector(0, 0, 3.f); for (const FVector& O : {FVector(1.2f, 0, 0), FVector(-.6f, 1.f, 0), FVector(-.6f, -1.f, 0), FVector(0, 0, 1.2f)}) Thorn(Bits, Bp, O.GetSafeNormal(), 2.2f, BerryCol); }
    };
    // A bed of thorns across the corridor between S0 and S1 (the maze's density).
    auto Bed = [&](const FeyDeep::FCrossing& C, float S0, float S1)
    {
        const int32 Nc = FMath::RoundToInt((S1 - S0) * (2.f * C.HalfWidth) / 650.f);
        for (int32 K = 0; K < Nc; ++K)
        {
            const float S = Rp.FRandRange(S0 + 8.f, S1 - 8.f), X = Rp.FRandRange(-C.HalfWidth + 6.f, C.HalfWidth - 6.f);
            const float Ang = Rp.FRandRange(0.f, 2 * PI);
            Cane(G3(C.At(S, X), -3.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), Rp.FRandRange(28.f, 62.f), Rp.FRandRange(16.f, 40.f), 2.4f);
        }
    };
    // A collision slab along a path piece (hidden), WallTall high.
    auto Slab = [&](const FVector2D& A, const FVector2D& B, float Half)
    {
        const FVector2D M = (A + B) * .5f; const float Len = static_cast<float>(FVector2D::Distance(A, B));
        const float Z = FMath::Min(Ground(A), Ground(B)) - 60.f;
        Walls.Box(FVector(M.X, M.Y, Z + (FeyDeep::WallTall + 60.f) * .5f), FVector(Len * .5f + 15.f, Half, (FeyDeep::WallTall + 60.f) * .5f), FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X)), FColor::White);
    };
    int32 Grips = 0;
    for (const FeyDeep::FCrossing& C : FeyDeep::Crossings())
    {
        // The wall's pieces: the arc either side of the corridor, and the corridor's two cheeks.
        TArray<TArray<FVector2D>> Pieces;
        {
            TArray<FVector2D> Cur;
            for (const FVector2D& P : C.Line)
            {
                const bool bGap = FMath::Abs(C.X(P)) < C.HalfWidth + 2.f * FeyDeep::WallHalf && FMath::Abs(C.S(P)) < C.HalfLen;
                if (bGap) { if (Cur.Num() > 1) Pieces.Add(Cur); Cur.Reset(); continue; }
                Cur.Add(P);
            }
            if (Cur.Num() > 1) Pieces.Add(Cur);
            for (const float Side : {-1.f, 1.f})
            {
                TArray<FVector2D> Ck;
                for (int32 K = 0; K <= 8; ++K) Ck.Add(C.At(FMath::Lerp(-C.HalfLen, C.HalfLen, K / 8.f), Side * (C.HalfWidth + FeyDeep::WallHalf)));
                Pieces.Add(Ck);
            }
        }
        for (const TArray<FVector2D>& Pc : Pieces) for (int32 I = 0; I + 1 < Pc.Num(); ++I) Slab(Pc[I], Pc[I + 1], FeyDeep::WallHalf);
        if (C.Kind == FeyDeep::Roots)
        {
            // Great ground roots heaped along the wall: four strands at their heights, winding, and every few metres a knee
            // arching up out of the ground and back.
            for (const TArray<FVector2D>& Pc : Pieces)
            {
                TArray<FVector2D> Rs; for (int32 I = 0; I + 1 < Pc.Num(); ++I) for (int32 K = 0; K < 2; ++K) Rs.Add(FMath::Lerp(Pc[I], Pc[I + 1], K / 2.f)); Rs.Add(Pc.Last());
                // (2026-11-26, second look) Six strands, each rising out of the ground and diving back in as it goes, at its own
                // height and wave, crossing the others: a tangle of roots rather than a pile of logs.
                const float Base[] = {30.f, 95.f, 160.f, 225.f, 290.f, 120.f}, Amp[] = {70.f, 110.f, 120.f, 100.f, 80.f, 150.f}, Wave[] = {260.f, 330.f, 290.f, 380.f, 240.f, 420.f};
                const float Rr[] = {60.f, 50.f, 44.f, 38.f, 28.f, 34.f}, Off[] = {-34.f, 28.f, -14.f, 22.f, -6.f, 10.f};
                for (int32 Sr = 0; Sr < 6; ++Sr)
                {
                    TArray<FVector> Pts; const float Ph = Rp.FRandRange(0.f, 6.f), Ph2 = Rp.FRandRange(0.f, 6.f);
                    for (int32 I = 0; I < Rs.Num(); ++I)
                    {
                        const FVector2D Tn = (Rs[FMath::Min(I + 1, Rs.Num() - 1)] - Rs[FMath::Max(I - 1, 0)]).GetSafeNormal(), Lt(-Tn.Y, Tn.X);
                        const float U = static_cast<float>(I) * 75.f;
                        const float H = Base[Sr] + Amp[Sr] * FMath::Sin(U / Wave[Sr] + Ph) + 25.f * FMath::Sin(U / 97.f + Ph2);
                        Pts.Add(G3(Rs[I] + Lt * (Off[Sr] + 22.f * FMath::Sin(U / 310.f + Ph2)), FMath::Max(H, -Rr[Sr] * .6f)));
                    }
                    Roots.Pipe(Pts, Rr[Sr], Rr[Sr] * .8f, 9, 2, WoodCol, Rp.FRand() * 9.f);
                }
                for (int32 I = 2; I + 4 < Rs.Num(); I += Rp.RandRange(4, 7))
                {
                    const FVector2D A = Rs[I], B = Rs[I + 3]; const FVector2D Lt = FVector2D(-(B - A).Y, (B - A).X).GetSafeNormal() * Rp.FRandRange(-40.f, 40.f);
                    const float Top = Rp.FRandRange(330.f, 420.f);
                    const FVector Pts[] = {G3(A + Lt, -20.f), G3(FMath::Lerp(A, B, .2f) + Lt, Top * .7f), G3(FMath::Lerp(A, B, .5f) + Lt * 1.3f, Top), G3(FMath::Lerp(A, B, .8f) + Lt, Top * .65f), G3(B + Lt, -20.f)};
                    Roots.Pipe(Pts, Rp.FRandRange(26.f, 38.f), 20.f, 8, 3, WoodCol, Rp.FRand() * 9.f);
                }
            }
            // The corridor: a root to vault at each end (waist high to him), the strip of thorns to leap between them.
            for (const float Sv : {-FeyDeep::VaultAt, FeyDeep::VaultAt})
            {
                TArray<FVector> Pts;
                for (int32 K = -1; K <= 7; ++K) { const float X = FMath::Lerp(-C.HalfWidth - 60.f, C.HalfWidth + 60.f, K / 6.f); Pts.Add(G3(C.At(Sv + 6.f * FMath::Sin(K * 1.1f), X), FeyDeep::VaultR + 2.f)); }
                Roots.Pipe(Pts, FeyDeep::VaultR + 1.f, FeyDeep::VaultR, 10, 2, WoodCol, Rp.FRand() * 9.f);
                // (its collision: a box as tall as the root, his vault's ledge)
                const FVector2D M = C.At(Sv, 0.f);
                Walls.Box(FVector(M.X, M.Y, Ground(M) + FeyDeep::VaultR), FVector(FeyDeep::VaultR, C.HalfWidth + 10.f, FeyDeep::VaultR + 1.f), FMath::RadiansToDegrees(FMath::Atan2(C.Along.Y, C.Along.X)), FColor::White);
            }
            Bed(C, -FeyDeep::StripHalf, FeyDeep::StripHalf);
        }
        else
        {
            // A thicket of brambles: overlapping mounds of differing height and width (an uneven, wild top), canes arching out
            // of their faces and over them thickly, thorned.
            for (const TArray<FVector2D>& Pc : Pieces)
                for (int32 I = 0; I + 1 < Pc.Num(); ++I)
                {
                    const FVector2D A = Pc[I], B = Pc[I + 1], Tn = (B - A).GetSafeNormal(), Lt(-Tn.Y, Tn.X);
                    const float Len = static_cast<float>(FVector2D::Distance(A, B));
                    for (int32 K = 0; K < 2; ++K)
                    {
                        const FVector2D M = FMath::Lerp(A, B, Rp.FRandRange(.2f, .8f)) + Lt * Rp.FRandRange(-25.f, 25.f);
                        const FVector Hh(Len * Rp.FRandRange(.45f, .7f), Rp.FRandRange(32.f, 46.f), Rp.FRandRange(55.f, 85.f));   // (a low dark core: the canes make the thicket)
                        Hedge.Hedge(FVector(M.X, M.Y, Ground(M) + Hh.Z - 18.f), Hh, FMath::RadiansToDegrees(FMath::Atan2(Tn.Y, Tn.X)) + Rp.FRandRange(-12.f, 12.f), Rp.FRand() * 10.f);
                    }
                    for (float U = 0.f; U < Len; U += 18.f)
                        for (const float Sd : {-1.f, 1.f})
                        {
                            // (canes rising from the ground at its foot and arching out and over, of every height up to about 3 m)
                            const FVector2D Bp = A + Tn * (U + Rp.FRandRange(0.f, 16.f)) + Lt * Sd * Rp.FRandRange(0.f, 45.f);
                            const FVector Dh = (FVector(Lt.X, Lt.Y, 0.f) * Sd * Rp.FRandRange(.3f, 1.f) + FVector(Tn.X, Tn.Y, 0.f) * Rp.FRandRange(-.8f, .8f)).GetSafeNormal();
                            const float Tall = FMath::Pow(Rp.FRand(), .7f) * 270.f + 30.f;
                            Cane(G3(Bp, -4.f), Dh, Rp.FRandRange(50.f, 70.f) + Tall * .35f, Tall, Rp.FRandRange(2.f, 3.6f));
                        }
                    for (float U = 0.f; U < Len; U += 60.f)   // (and over the top)
                    {
                        const FVector2D Bp = A + Tn * U + Lt * Rp.FRandRange(-30.f, 30.f); const float Ang = Rp.FRandRange(0.f, 2 * PI);
                        Cane(G3(Bp, Rp.FRandRange(200.f, 290.f)), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), Rp.FRandRange(50.f, 90.f), Rp.FRandRange(30.f, 70.f), 2.2f);
                    }
                }
            // The bed under the hoops, and the hoops across the corridor (the maze's: crowns at GripUp, HoopStep apart).
            Bed(C, -FeyDeep::BedHalf, FeyDeep::BedHalf);
            TArray<FVector> Crowns;
            const FVector Along3(C.Along.X, C.Along.Y, 0.f);
            for (int32 K = 0; K < FeyDeep::Hoops; ++K)
            {
                const float S = (K - (FeyDeep::Hoops - 1) * .5f) * FeyMap::HoopStep; TArray<FVector> Pts;
                const float Span = C.HalfWidth - 10.f;
                for (int32 J = 0; J <= 10; ++J) { const float U = J / 10.f * 2.f - 1.f; Pts.Add(G3(C.At(S, U * Span), -6.f + (FeyMap::GripUp + 6.f) * FMath::Sqrt(FMath::Max(0.f, 1.f - U * U)))); }
                Lath.Pipe(Pts, 3.4f, 3.4f, 7, 3, WoodCol, K * 3.1f);
                Crowns.Add(G3(C.At(S, 0.f), FeyMap::GripUp));
                IvyStem(Pts, Along3, 36);
                for (const float Xs : {-.68f, -.42f, .45f, .72f})
                {
                    const FVector Top = G3(C.At(S, Xs * Span), (FeyMap::GripUp + 6.f) * FMath::Sqrt(1.f - Xs * Xs) - 6.f);
                    const FVector Hs[] = {Top, Top + FVector(0, 0, -12.f) + Along3 * 3.f, Top + FVector(0, 0, -26.f) - Along3 * 2.f, Top + FVector(0, 0, -38.f)};
                    IvyStem(Hs, Along3, 8);
                }
                FChuckSwingGrip Gr; Gr.Grip = FeyWorld(Crowns.Last()); Gr.Along = Along3; AddChuckSwingGrip(Gr); ++Grips;
            }
            for (int32 K = 0; K + 1 < Crowns.Num(); ++K)
                for (const float Xs : {-34.f, 30.f})
                {
                    const float Sa = (K - (FeyDeep::Hoops - 1) * .5f) * FeyMap::HoopStep; const float Span = C.HalfWidth - 10.f; const float U = Xs / Span, Zc = (FeyMap::GripUp + 6.f) * FMath::Sqrt(1.f - U * U) - 6.f;
                    const FVector Gs[] = {G3(C.At(Sa, Xs), Zc), G3(C.At(Sa + FeyMap::HoopStep * .5f, Xs * 1.1f), Zc - 16.f), G3(C.At(Sa + FeyMap::HoopStep, Xs), Zc)};
                    IvyStem(Gs, FVector::UpVector, 14);
                }
        }
        ++Built;
    }
    // (the surfaces are made in local coordinates; this actor stands at the Feywild's origin)
    Emit(Walls, nullptr, true, false, false);
    Emit(Roots, MRoot, false, true, true);
    Emit(Hedge, MHedge, false, true, true);
    Emit(Canes, MRoot, false, true, false);
    Emit(Lath, MRoot, false, true, true);
    Emit(Bits, MPlant, false, true, false);
    Emit(Ivy, MPlant, false, true, false);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DEEP_CROSSINGS built=%d grips=%d wall_tris=%d root_verts=%d hedge_verts=%d cane_verts=%d"), Built, Grips, Walls.T.Num() / 3, Roots.V.Num(), Hedge.V.Num(), Canes.V.Num());
}

void AFeyDeepCrossings::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    // Thorns, as in the maze: on the ground among them each 1.6 s costs a Sanity point and a stumble back (the slowing is
    // FeyGroundSpeedScale's).
    AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || Clock < NextPrick || !Chuck->GetCharacterMovement()->IsMovingOnGround()) return;
    const FVector Feet = FeyLocal(Chuck->GetActorLocation()) - FVector(0, 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (!FeyDeep::InThorns(Feet)) return;
    const FVector V = Chuck->GetVelocity().GetSafeNormal2D();
    if (Chuck->TakeBite(Chuck->GetActorLocation() + (V.IsNearlyZero() ? Chuck->GetActorForwardVector() : V) * 60.f, 1, true, false))
    { ++Pricks; NextPrick = Clock + 1.6f; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DEEP_PRICK sanity=%d"), Chuck->GetSanity()); }
}
