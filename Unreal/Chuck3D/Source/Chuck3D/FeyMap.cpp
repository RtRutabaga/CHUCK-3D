#include "FeyMap.h"
#include "Misc/ScopeExit.h"
#include "FeyMesh.h"
#include "FeyLife.h"
#include "FeyTrio.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ProceduralMeshComponent.h"
#include "Algo/Reverse.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include <atomic>
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTargetCube.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"

// The Feywild in rough form (Claude, 2026-11-19; FeyMap.h, docs/regions/FEYWILD.md stage 3).
using namespace FeyMap;

// ---------------------------------------------------------------- cloud (2026-11-21, the user: the tower's cloud and its stair
// read as blocks, with straight lines, perfect angles and gaps between the steps). Built from lumps: noise-billowed spheres
// for the tower's cloud, and for the stair one long overlapping lump per step, each sunk into the next so nothing shows
// between them, flat on top where he will one day walk.
namespace FeyCloud
{
    struct FIco { TArray<FVector> V; TArray<int32> T; };
    const FIco& Ico(int32 Sub)
    {
        static FIco Cache[5]; static FCriticalSection Lock; FScopeLock L(&Lock);
        FIco& S = Cache[FMath::Clamp(Sub, 0, 4)];
        if (S.V.Num()) return S;
        const float Tt = (1.f + FMath::Sqrt(5.f)) * .5f;
        S.V = {{-1, Tt, 0}, {1, Tt, 0}, {-1, -Tt, 0}, {1, -Tt, 0}, {0, -1, Tt}, {0, 1, Tt}, {0, -1, -Tt}, {0, 1, -Tt}, {Tt, 0, -1}, {Tt, 0, 1}, {-Tt, 0, -1}, {-Tt, 0, 1}};
        for (FVector& P : S.V) P.Normalize();
        S.T = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
        for (int32 K = 0; K < FMath::Clamp(Sub, 0, 4); ++K)
        {
            TMap<uint64, int32> Mid; TArray<int32> Nt;
            auto M = [&](int32 A, int32 B) { const uint64 Key = (uint64(FMath::Min(A, B)) << 32) | uint32(FMath::Max(A, B)); if (const int32* F = Mid.Find(Key)) return *F; const int32 I = S.V.Add(((S.V[A] + S.V[B]) * .5f).GetSafeNormal()); Mid.Add(Key, I); return I; };
            for (int32 I = 0; I < S.T.Num(); I += 3)
            {
                const int32 A = S.T[I], B = S.T[I + 1], C = S.T[I + 2], Ab = M(A, B), Bc = M(B, C), Ca = M(C, A);
                Nt.Append({A, Ab, Ca, B, Bc, Ab, C, Ca, Bc, Ab, Bc, Ca});
            }
            S.T = MoveTemp(Nt);
        }
        return S;
    }
    // Billowing: rounded lumps on lumps (the absolute of noise, in three octaves).
    float Billow(const FVector& Q, float Seed)
    {
        float B = 0, Amp = 1, Sum = 0;
        for (int32 O = 0; O < 3; ++O) { B += Amp * (1.f - FMath::Abs(FMath::PerlinNoise3D(Q * (1.f + O * 1.9f) + FVector(Seed * 7.1f + O * 13.f, Seed * 3.3f, -Seed * 5.7f)))); Sum += Amp; Amp *= .38f; }
        return B / Sum;   // (about .5 to 1)
    }
    // One puff: radii R, its underside flattened toward Floor (world Z), billows Lumpy of its size.
    void Puff(FMapSurf& S, const FVector& C, const FVector& R, int32 Sub, float Seed, float Floor, float Lumpy = .32f)
    {
        const FIco& I = Ico(Sub); TArray<FVector> P, Out; TArray<FColor> Col;
        for (const FVector& D : I.V)
        {
            const float B = Billow(D * 1.4f, Seed), Rr = 1.f + Lumpy * (B - .7f) * 2.f;
            FVector Q = C + D * R * Rr;
            if (Q.Z < Floor) Q.Z = Floor - (Floor - Q.Z) * .18f;
            P.Add(Q); Out.Add(D);
            const float Lit = FMath::Clamp(.55f + 1.1f * (Rr - 1.f) + .25f * D.Z, 0.f, 1.f);   // (hollows between the billows a little darker)
            Col.Add(FLinearColor(Lit, 1, 0, 1).ToFColor(false));
        }
        S.Closed(P, Out, I.T, Col);
    }
}

namespace
{
    float Noise(float X, float Y, float Scale, float Seed = 0) { return FMath::PerlinNoise2D(FVector2D(X / Scale + Seed * 3.71f + .31f, Y / Scale - Seed * 1.93f + .77f)); }
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    constexpr float Stand = 34.65f;
    float SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A; const float L2 = AB.SizeSquared();
        const float U = L2 > 0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
        return FVector2D::Distance(P, A + AB * U);
    }

    const FDistrict Table[Districts] = {
        {TEXT("Riverbank"), TEXT("FeyArrival"), TEXT("Feywild Riverbank"), -24000, -4200, -5600, 5600, 0, {-4200, 0}, 0},
        {TEXT("Orchard"), TEXT("FeyOrchard"), TEXT("Feywild Orchard"), -52500, -28000, -12500, 12500, 0, {-40000, 0}, 1500},
        {TEXT("Crossroads"), TEXT("FeyCrossroads"), TEXT("Feywild Crossroads"), -79000, -56500, -11000, 11000, 0, {-67750, 0}, 3100},
        {TEXT("LilyMere"), TEXT("FeyMere"), TEXT("Feywild Lily Mere"), -80500, -54000, 15000, 41500, 2, {-67000, 28000}, 1700},
        {TEXT("Deepwood"), TEXT("FeyDeepwood"), TEXT("Feywild Deepwood"), -90000, -60000, -45000, -15000, 1, {-75000, -30000}, 1800},
        {TEXT("Highmeadow"), TEXT("FeyHighmeadow"), TEXT("Feywild Highmeadow"), -113000, -83000, -10000, 20000, 2, {-101000, 3000}, 1600},
        {TEXT("Maze"), TEXT("FeyMaze"), TEXT("Feywild Maze"), -149000, -119000, -10000, 20000, 3, {-134000, 5000}, 0},
    };
    const FThreshold Ways[Thresholds] = {
        {Riverbank, Orchard, {-24000, 0}, {-28000, 0}, 0, 0, 900, RockCut},
        {Orchard, Crossroads, {-52500, -6000}, {-56500, -6000}, 0, 0, 900, RockCut},
        {Orchard, Crossroads, {-52500, 6000}, {-56500, 6000}, 0, 0, -900, RootTunnel},
        {Crossroads, LilyMere, {-61500, 11000}, {-61500, 15000}, 0, 0, 900, RockCut},
        {Crossroads, LilyMere, {-73500, 11000}, {-73500, 15000}, 0, 0, -900, RockCut},
        {Crossroads, Deepwood, {-62500, -11000}, {-62500, -15000}, 0, 0, 900, RootTunnel},
        {Crossroads, Deepwood, {-74500, -11000}, {-74500, -15000}, 0, 0, -900, RootTunnel},
        {Deepwood, Highmeadow, {-86500, -15000}, {-86500, -10000}, 0, 0, 1000, RootTunnel},
        {Crossroads, Highmeadow, {-79000, 2000}, {-83000, 2000}, 0, 0, 900, RockCut},
        {Highmeadow, Maze, {-113000, 5000}, {-119000, 5000}, 2000, 0, 1100, StoneStair},   // (in line with the maze's gate)
    };
    // From a district's mouth on a threshold in to its ring (bends only).
    struct FRoad { int32 D, T, N; FVector2D P[4]; };
    const FRoad Roads[] = {
        {Riverbank, 0, 4, {{-20000, 1000}, {-15000, -1500}, {-9000, 1800}, {-7000, -229}}},
        {Orchard, 0, 1, {{-33000, 1500}}}, {Orchard, 1, 1, {{-46000, -4000}}}, {Orchard, 2, 1, {{-46500, 5000}}},
        {Crossroads, 1, 1, {{-60000, -3500}}}, {Crossroads, 2, 1, {{-60000, 3500}}}, {Crossroads, 3, 1, {{-63000, 6500}}}, {Crossroads, 4, 1, {{-72000, 6500}}},
        {Crossroads, 5, 1, {{-63500, -6500}}}, {Crossroads, 6, 1, {{-72500, -6500}}}, {Crossroads, 8, 1, {{-74000, 1500}}},
        {LilyMere, 3, 1, {{-62500, 21000}}}, {LilyMere, 4, 1, {{-72000, 21000}}},
        {Deepwood, 5, 2, {{-64000, -22000}, {-70000, -26000}}}, {Deepwood, 6, 1, {{-74000, -22000}}}, {Deepwood, 7, 2, {{-87000, -21000}, {-82000, -27000}}},
        {Highmeadow, 8, 2, {{-89000, 3500}, {-95000, 2500}}}, {Highmeadow, 7, 3, {{-89500, -3000}, {-93500, 1500}, {-95000, 2500}}}, {Highmeadow, 9, 1, {{-107000, 5000}}},
        {Maze, 9, 0, {}},
    };
    // From a district's ring out to a landmark.
    struct FSpur { int32 D, N; FVector2D P[3]; };
    const FSpur Spurs[] = {
        {Deepwood, 2, {{-80000, -37000}, {-82000, -40000}}},     // Redcap Hollow
        {Highmeadow, 2, {{-106000, 9000}, {-111900, 12500}}},    // the Maze Overlook
        {LilyMere, 1, {{-66500, 35000}}},                         // the fen
    };
    const FVector Pools[] = {{-67000, 28000, 1050}, {-59500, 32500, 2300}, {-75500, 35500, 2700}, {-66000, 39300, 1500}, {-57500, 21500, 1500}, {-77000, 23000, 1400}};
    const FVector2D Overlook(-111900, 12500), Redcap(-82000, -40000), Wisteria(-110300, 14800);
    float RapidX(float Y) { return -95000.f + 600.f * FMath::Sin(Y / 3000.f); }
    float RapidWL(float Y) { return 20.f + 90.f * (Y + 10000.f) / 30000.f + 220.f * Smooth(13800.f, 14200.f, Y); }

    FVector2D Dir(int32 Th) { return (Ways[Th].PB - Ways[Th].PA).GetSafeNormal(); }
    struct FMouth { int32 Th; FVector2D P, In; float Z; };
    struct FData
    {
        TArray<FMouth> Mouths[Districts];
        TArray<TArray<FVector2D>> Lines[Districts];
        TArray<FVector2D> RoadOf[Districts][Thresholds];
        TArray<FVector2D> SpurOf[Districts];
    };
    FVector2D RingPoint(int32 D, const FVector2D& Toward)
    {
        const FDistrict& Ds = Table[D];
        return Ds.HubR > 0 ? Ds.Hub + (Toward - Ds.Hub).GetSafeNormal() * Ds.HubR : Ds.Hub;
    }
    FVector2D MazeAt(float R, float Rad) { return Table[Maze].Hub + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * R; }   // (radians)
    const FData& Data()
    {
        static FData S = []()
        {
            FData Out;
            for (int32 T = 0; T < Thresholds; ++T)
            {
                const FThreshold& W = Ways[T];
                Out.Mouths[W.A].Add({T, W.PA, -Dir(T), W.ZA});
                Out.Mouths[W.B].Add({T, W.PB, Dir(T), W.ZB});
            }
            for (const FRoad& R : Roads)
            {
                TArray<FVector2D> L; const FThreshold& W = Ways[R.T];
                L.Add(W.A == R.D ? W.PA : W.PB);
                L.Add(L[0] + (W.A == R.D ? -Dir(R.T) : Dir(R.T)) * 1500.f);   // (straight in through the rim's notch first)
                for (int32 I = 0; I < R.N; ++I) L.Add(R.P[I]);
                if (R.D == Riverbank) { for (float X = -6000.f; X <= -2150.f; X += 600.f) L.Add(FVector2D(X, Fey::PathY(X))); }
                else if (R.D == Maze) {}   // (straight on to the gate: the maze's own way begins there)
                else L.Add(RingPoint(R.D, L.Last()));
                Out.RoadOf[R.D][R.T] = L; Out.Lines[R.D].Add(L);
            }
            for (int32 D = 0; D < Districts; ++D)
                if (Table[D].HubR > 0)
                {
                    TArray<FVector2D> Ring;
                    for (int32 I = 0; I <= 48; ++I) { const float A = 2 * PI * I / 48; Ring.Add(Table[D].Hub + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Table[D].HubR); }
                    Out.Lines[D].Add(Ring);
                }
            for (const FSpur& Sp : Spurs)
            {
                TArray<FVector2D> L; L.Add(RingPoint(Sp.D, Sp.P[0]));
                for (int32 I = 0; I < Sp.N; ++I) L.Add(Sp.P[I]);
                Out.SpurOf[Sp.D] = L; Out.Lines[Sp.D].Add(L);
            }
            return Out;
        }();
        return S;
    }
    float PathDist(int32 D, float X, float Y)
    {
        const FVector2D P(X, Y); float Best = 1e9f;
        for (const TArray<FVector2D>& L : Data().Lines[D]) for (int32 I = 0; I + 1 < L.Num(); ++I) Best = FMath::Min(Best, SegDist(P, L[I], L[I + 1]));
        return Best;
    }
    float PoolE(float X, float Y, int32& Which)
    {
        float Best = 9; Which = -1;
        for (int32 I = 0; I < UE_ARRAY_COUNT(Pools); ++I)
        {
            const FVector& Pl = Pools[I]; const float A = FMath::Atan2(Y - Pl.Y, X - Pl.X);
            const float E = FVector2D(X - Pl.X, Y - Pl.Y).Size() / (Pl.Z * (1.f + .08f * FMath::Sin(3.f * A + I) + .05f * FMath::Sin(7.f * A + 2 * I)));
            if (E < Best) { Best = E; Which = I; }
        }
        return Best;
    }
    float MazeRadius(float X, float Y) { return FVector2D(X - Table[Maze].Hub.X, Y - Table[Maze].Hub.Y).Size(); }

    float MoatK(float X, float Y);
    // (2026-11-23) The hag's cottage: just off the trail from the Crossroads into the Deepwood, on its outer side (away from the
    // hub), its door facing the trail across a small fenced garden, so he passes close by it. X, Y its middle, Z its yaw.
    const FVector& HagSpot()
    {
        static const FVector Spot = []()
        {
            const FVector2D Want(-66500.f, -21000.f); float Best = 1e18f; FVector2D At = Want, Tn(1, 0);
            for (const TArray<FVector2D>& L : Data().Lines[Deepwood])
                for (int32 I = 0; I + 1 < L.Num(); ++I)
                {
                    const FVector2D Q = FMath::ClosestPointOnSegment2D(Want, L[I], L[I + 1]); const float Dd = FVector2D::DistSquared(Q, Want);
                    if (Dd < Best) { Best = Dd; At = Q; Tn = (L[I + 1] - L[I]).GetSafeNormal(); }
                }
            FVector2D Side(-Tn.Y, Tn.X); if (FVector2D::DotProduct(Side, Table[Deepwood].Hub - At) > 0.f) Side = -Side;
            const FVector2D C = At + Side * 900.f;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HAG_SPOT at=%.0f,%.0f trail=%.0f,%.0f"), C.X, C.Y, At.X, At.Y);
            return FVector(C.X, C.Y, FMath::RadiansToDegrees(FMath::Atan2(-Side.Y, -Side.X)));
        }();
        return Spot;
    }
    // The Deepwood's own lie of the land before the cottage's ground is levelled (Land's terms for it).
    float DeepRaw(float X, float Y) { const int32 D = Deepwood; return 40.f * Noise(X, Y, 2600, D) + 18.f * Noise(X, Y, 700, D + 7) + 6.f * Noise(X, Y, 160, D + 13) + 80.f * Noise(X, Y, 1100, 50) + 40.f * Noise(X, Y, 330, 51); }
    // A district's land: its own character, its paths pressed in, flattened to each threshold's mouth, a wooded rim.
    float Land(int32 D, float X, float Y, float* OutPath = nullptr, float* OutWet = nullptr)
    {
        const FDistrict& Ds = Table[D];
        float H = 40.f * Noise(X, Y, 2600, D) + 18.f * Noise(X, Y, 700, D + 7) + 6.f * Noise(X, Y, 160, D + 13), Wet = 0;
        const float Hd = FVector2D(X - Ds.Hub.X, Y - Ds.Hub.Y).Size();
        switch (D)
        {
        case Riverbank: H += 50.f * Noise(X, Y, 1500, 40); break;
        case Orchard: H *= .6f; break;
        case Crossroads: H = H * .5f + 160.f * Smooth(7000.f, 10500.f, Hd); break;
        case LilyMere:
        {
            H = 30.f + .5f * H; int32 I; const float E = PoolE(X, Y, I);
            H = FMath::Lerp(H, 8.f + 4.f * Noise(X, Y, 200, 44), Smooth(1.4f, 1.03f, E));
            H = FMath::Lerp(H, -14.f, Smooth(1.f, .84f, E));
            Wet = Smooth(1.5f, 1.f, E);
            break;
        }
        case Deepwood:
        {
            H += 80.f * Noise(X, Y, 1100, 50) + 40.f * Noise(X, Y, 330, 51);
            const FVector& Hs = HagSpot(); const float Dh = FVector2D::Distance(FVector2D(X, Y), FVector2D(Hs));   // (2026-11-23: the cottage's ground levelled)
            if (Dh < 1300.f) H = FMath::Lerp(H, DeepRaw(Hs.X, Hs.Y), Smooth(1300.f, 800.f, Dh));
            break;
        }
        case Highmeadow:
        {
            H += 110.f * Noise(X, Y, 4200, 60) + 2000.f * Smooth(-100500.f, -110500.f, X) + 220.f * Smooth(12500.f, 15500.f, Y);
            const float Dx = FMath::Abs(X - RapidX(Y)), WL = RapidWL(Y);
            H = FMath::Lerp(H, FMath::Max(H, WL + 30.f), Smooth(1000.f, 560.f, Dx));
            H = FMath::Lerp(H, WL - 14.f, Smooth(560.f, 330.f, Dx));
            Wet = Smooth(900.f, 350.f, Dx);
            break;
        }
        case Maze: H = 4.f * Noise(X, Y, 900, 70); break;
        default: break;
        }
        const float Pd = PathDist(D, X, Y);
        float Pk = Smooth(260.f, 120.f, Pd);
        if (D == Maze) Pk = FMath::Max(Pk, Smooth(13900.f, 13500.f, MazeRadius(X, Y)));
        H -= 7.f * Pk;
        // Flat to each threshold's floor at its mouth, and the rim notched for it.
        float Notch = 0;
        for (const FMouth& M : Data().Mouths[D])
        {
            const float Dm = FVector2D::Distance(FVector2D(X, Y), M.P);
            H = FMath::Lerp(H, M.Z, Smooth(1700.f, 500.f, Dm));
            Notch = FMath::Max(Notch, Smooth(1500.f, 700.f, SegDist(FVector2D(X, Y), M.P, M.P + M.In * 2600.f)));
        }
        float E = FMath::Min(FMath::Min(X - Ds.X0, Ds.X1 - X), FMath::Min(Y - Ds.Y0, Ds.Y1 - Y));
        if (D == Riverbank) E = FMath::Min(X - Ds.X0, FMath::Min(Y - Ds.Y0, Ds.Y1 - Y));
        float Open = 0;
        if (D == Highmeadow) Open = Smooth(6000.f, 7500.f, Y) * Smooth(18500.f, 17000.f, Y) * Smooth(Ds.X0 + 3500.f, Ds.X0 + 2000.f, X);
        H += (1250.f + 250.f * Noise(X, Y, 1300, D + 80)) * Smooth(1900.f, 0.f, E) * (1.f - Notch) * (1.f - Open);
        if (D == Highmeadow) H += Open * (260.f * Smooth(1000.f, 300.f, FVector2D::Distance(FVector2D(X, Y), Overlook)) - 2700.f * Smooth(Ds.X0 + 1000.f, Ds.X0 + 250.f, X));   // (a knoll at the brink, then the drop)   // (2026-11-20: the overlook's edge falls away, so the maze shows below)
        if (D == Riverbank) H = FMath::Lerp(H, Fey::Ground(X, Y), Smooth(-7500.f, -4300.f, X));   // (into the look slice's own land)
        if (D == LilyMere) { const float K = MoatK(X, Y); H = FMath::Lerp(H, -170.f, K); Wet = FMath::Max(Wet, K); }   // (the deep water, through the rim to the edge)
        if (OutPath) *OutPath = Pk;
        if (OutWet) *OutWet = Wet;
        return H;
    }

    // ---------------------------------------------------------------- the maze (2026-11-20: at a person's scale)
    // The user: the hedges to either side should feel, for a person, like the maze in the Goblet of Fire. So corridors
    // 2.6 m wide between hedges 8.4 m high and 1.2 m thick, in 29 rings of cells round a plaza at the heart (the Cloud
    // Staircase's foot). Grown from the gate by a randomised Prim's walk (always one way through), with some walls opened
    // again so there are loops; the way through is the shortest path from the gate to the heart.
    struct FHedge { FVector C, H; float Yaw; int32 Kind; int32 Sc = -1; };   // 0 hedge, 1 stone (arch, statue); Sc: a shortcut's (FeyLife builds it)
    constexpr float MazeR0 = 1800.f, MazePitch = 380.f, MazeRmax = 13000.f, HedgeHalf = 60.f, HedgeTall = 840.f, MazeGap = 300.f;
    struct FMazeData { TArray<FHedge> Hedges; TArray<FVector2D> Route; TArray<FVector2D> DeadEnds; int32 Cells = 0; float Gate = 0, HeartA = 0;
                       TArray<FeyMap::FMazeShortcut> Shortcuts; TArray<FVector> Lanes; TArray<FeyMap::FMazeParkour> Parkour; TArray<FeyMap::FMazeGap> Gaps;
                       TArray<FeyMap::FMazeMark> Marks, Squeezes; TArray<FVector4> Decor; TArray<FVector4> Frowns; };   // (Frowns: dead-end statues X, Y, facing X, Y)   // (2026-11-23: Decor X, Y, the angle toward the corridor's middle, its kind)   // (2026-11-22) Lanes: straight runs (X, Y of its start; Z its angle) for orchids
    const FMazeData& MazeData()
    {
        static FMazeData S = []()
        {
            FMazeData Out; FRandomStream R(1213);
            const int32 Rings = int32((MazeRmax - MazeR0) / MazePitch);
            TArray<int32> N, Base; int32 Total = 0;
            for (int32 I = 0; I < Rings; ++I) { const float Rm = MazeR0 + (I + .5f) * MazePitch; N.Add(FMath::Max(8, FMath::RoundToInt(2 * PI * Rm / 430.f))); Base.Add(Total); Total += N.Last(); }
            auto Id = [&](int32 I, int32 J) { return Base[I] + ((J % N[I]) + N[I]) % N[I]; };
            auto Ring = [&](int32 C) { int32 I = Rings - 1; while (Base[I] > C) --I; return I; };
            auto A0 = [&](int32 C) { const int32 I = Ring(C); return 2 * PI * (C - Base[I]) / N[I]; };
            auto A1 = [&](int32 C) { const int32 I = Ring(C); return 2 * PI * (C - Base[I] + 1) / N[I]; };
            TArray<TArray<int32>> Adj; Adj.SetNum(Total);
            for (int32 I = 0; I < Rings; ++I)
                for (int32 J = 0; J < N[I]; ++J)
                {
                    const int32 C = Id(I, J); Adj[C].AddUnique(Id(I, J + 1)); Adj[Id(I, J + 1)].AddUnique(C);
                    if (I + 1 < Rings)
                        for (int32 K = 0; K < N[I + 1]; ++K)
                        {
                            const int32 O = Id(I + 1, K); const float Lo = FMath::Max(A0(C), A0(O)), Hi = FMath::Min(A1(C), A1(O));
                            if ((Hi - Lo) * (MazeR0 + (I + 1) * MazePitch) > MazeGap + 40.f) { Adj[C].Add(O); Adj[O].Add(C); }
                        }
                }
            TSet<uint64> Links; auto Key = [](int32 A, int32 B) { return (uint64(FMath::Min(A, B)) << 32) | uint32(FMath::Max(A, B)); };
            const int32 Gate = Id(Rings - 1, 0); TArray<bool> In; In.Init(false, Total); In[Gate] = true;
            // (2026-11-23, second round; the user: hazards only on the way he must take) The way through is laid out first, in
            // straight runs along the rings between turns inward, so the parkour has room on it; the rest of the maze grows out
            // from it, with no loops, so it stays the one way through.
            TArray<int32> Laid; Laid.Add(Gate);
            {
                const int32 Lens[] = {3, 2, 4, 2, 3, 2, 4, 2, 3, 2, 4, 2, 3, 2};
                int32 C = Gate, Run = 0; float Dir = 1.f;
                for (int32 I = Rings - 1; ; )
                {
                    if (I < Rings - 1 && (Rings - 1 - I) % 2 == 1)   // a straight run along every other ring
                    {
                        const int32 Len = Lens[Run++ % UE_ARRAY_COUNT(Lens)];
                        for (int32 K = 0; K < Len; ++K) { C = Id(I, C - Base[I] + int32(Dir)); Laid.Add(C); In[C] = true; }
                        Dir = -Dir;
                    }
                    if (I == 0) break;
                    // Inward: from here if this cell opens inward, else along to the nearest one that does (either way, not over
                    // the way already laid); and of the cells inward, one that opens on inward again if there is one.
                    auto Inward = [&](int32 X) { int32 Nb = 0; for (const int32 B : Adj[X]) Nb += Ring(B) == Ring(X) - 1; return Nb > 0 || Ring(X) == 0; };
                    if (!Inward(C))
                    {
                        int32 BestK = 1 << 20, BestD = 1;
                        for (const int32 Dd : {1, -1})
                            for (int32 K = 1; K < N[I] && K < BestK; ++K) { const int32 X = Id(I, C - Base[I] + Dd * K); if (In[X]) break; if (Inward(X)) { BestK = K; BestD = Dd; break; } }
                        check(BestK < (1 << 20));
                        for (int32 K = 0; K < BestK; ++K) { C = Id(I, C - Base[I] + BestD); Laid.Add(C); In[C] = true; }
                    }
                    TArray<int32> Down, Good; for (const int32 B : Adj[C]) if (Ring(B) == I - 1) { Down.Add(B); if (Inward(B)) Good.Add(B); }
                    const TArray<int32>& Pool = Good.Num() ? Good : Down;
                    C = Pool[R.RandRange(0, Pool.Num() - 1)]; --I; Laid.Add(C); In[C] = true;
                }
            }
            TArray<TPair<int32, int32>> Front;
            for (int32 K = 0; K < Laid.Num(); ++K) { if (K > 0) Links.Add(Key(Laid[K - 1], Laid[K])); for (int32 B : Adj[Laid[K]]) if (!In[B]) Front.Add({Laid[K], B}); }
            while (Front.Num())
            {
                const int32 K = R.RandRange(0, Front.Num() - 1); const TPair<int32, int32> E = Front[K]; Front.RemoveAtSwap(K);
                if (In[E.Value]) continue;
                In[E.Value] = true; Links.Add(Key(E.Key, E.Value));
                for (int32 B : Adj[E.Value]) if (!In[B]) Front.Add({E.Value, B});
            }
            const int32 Heart = Laid.Last();
            // The shortest way from the gate to the heart.
            TArray<int32> From; From.Init(-1, Total); TArray<int32> Q; Q.Add(Gate); From[Gate] = Gate;
            for (int32 H = 0; H < Q.Num(); ++H) for (int32 B : Adj[Q[H]]) if (From[B] < 0 && Links.Contains(Key(Q[H], B))) { From[B] = Q[H]; Q.Add(B); }
            TArray<int32> Way; for (int32 C = Heart; ; C = From[C]) { Way.Insert(C, 0); if (C == Gate) break; }
            auto Mid = [&](int32 C) { const int32 I = Ring(C); return MazeAt(MazeR0 + (I + .5f) * MazePitch, (A0(C) + A1(C)) * .5f); };
            const TSet<int32> WaySet(Way); TSet<int32> ParkCells;
            Out.Gate = (A0(Gate) + A1(Gate)) * .5f;
            Out.Route.Add(MazeAt(MazeRmax + 700.f, Out.Gate));
            for (int32 W = 0; W < Way.Num(); ++W)
            {
                if (W > 0 && Ring(Way[W]) != Ring(Way[W - 1]))   // through the gap in the hedge between the two rings
                {
                    const int32 Hi = FMath::Max(Ring(Way[W]), Ring(Way[W - 1]));
                    const float Lo = FMath::Max(A0(Way[W]), A0(Way[W - 1])), Up = FMath::Min(A1(Way[W]), A1(Way[W - 1]));
                    Out.Route.Add(MazeAt(MazeR0 + Hi * MazePitch, (Lo + Up) * .5f));
                }
                Out.Route.Add(Mid(Way[W]));
            }
            const float HeartA = (A0(Heart) + A1(Heart)) * .5f; Out.HeartA = HeartA;
            // (2026-11-22, the user: the longer he is lost, the more hit plants appear, each opening a new way through a hedge)
            // Shortcuts: walls whose opening most shortens the way to the heart, spread over the maze; their hedges are built
            // by FeyLife (so they can sink away), each with the place its hit plant appears (on the far side from the heart).
            TArray<int32> Dist; Dist.Init(-1, Total); { TArray<int32> Qh; Qh.Add(Heart); Dist[Heart] = 0;
                for (int32 H = 0; H < Qh.Num(); ++H) for (int32 B : Adj[Qh[H]]) if (Dist[B] < 0 && Links.Contains(Key(Qh[H], B))) { Dist[B] = Dist[Qh[H]] + 1; Qh.Add(B); } }
            struct FCand { int32 C, B, Gain; };
            TArray<FCand> Cands;
            for (int32 C = 0; C < Total; ++C) for (int32 B : Adj[C]) if (B > C && !Links.Contains(Key(C, B)) && Dist[C] >= 0 && Dist[B] >= 0 && Ring(C) > 1 && Ring(B) > 1)
            { const int32 G = FMath::Abs(Dist[C] - Dist[B]) - 1; if (G >= 6) Cands.Add({Dist[C] > Dist[B] ? C : B, Dist[C] > Dist[B] ? B : C, G}); }
            Cands.Sort([](const FCand& X, const FCand& Y) { return X.Gain > Y.Gain; });
            TMap<uint64, int32> ScOf;
            for (const FCand& Cd : Cands)
            {
                if (Out.Shortcuts.Num() >= 12) break;
                if (!WaySet.Contains(Cd.B)) continue;   // (2026-11-23: through one he comes out on the way through)
                const FVector2D M = (Mid(Cd.C) + Mid(Cd.B)) * .5f; bool bNear = false;
                for (const FeyMap::FMazeShortcut& O : Out.Shortcuts) bNear |= FVector2D::Distance(O.Mid, M) < 2600.f;
                if (bNear) continue;
                FeyMap::FMazeShortcut Sc; Sc.Mid = M; Sc.Gain = Cd.Gain; Sc.Plant = FMath::Lerp(M, Mid(Cd.C), .45f); ScOf.Add(Key(Cd.C, Cd.B), Out.Shortcuts.Num()); Out.Shortcuts.Add(Sc);
            }
            TSet<int32> LaneCells;
            // (2026-11-23, the plan: wall runs along trellised sections, brachiation under ivy arches, climbs to raised walkways,
            // Chuck-sized gaps through the roots; thorn hazards. Second round, the user: hazards and unusual things only on the
            // way he must take, so that one way to solve the maze is to keep heading toward them.) Each run of the way along one
            // ring, from the opening it comes in by to the one it leaves by, gets the way over thorns that fits between them, the
            // kinds taken in turn (a walkway, ivy hoops, a trellis, three hoops, a short bed to jump). Not by the orchids' lanes or
            // the redcaps at the middle of the way.
            {
                const FVector2D Middle = Out.Route[FMath::Min((Out.Route.Num() + 2) / 2, Out.Route.Num() - 1)];
                // Each kind's reach along the run from its middle (thorns, frame, run-up, landing) and its bed; 4 is three hoops.
                const float Ext[5][2] = {{-330.f, 300.f}, {-320.f, 340.f}, {-480.f, 340.f}, {-100.f, 100.f}, {-180.f, 220.f}};
                const float Beds[5][2] = {{-90.f, 90.f}, {-225.f, 225.f}, {-360.f, 220.f}, {-60.f, 60.f}, {-90.f, 90.f}};
                const float Pref[5] = {0.f, 0.f, 0.f, 1.6f, .6f};
                constexpr float Margin = 160.f;   // (clear of each opening's middle: an opening is 300 wide)
                int32 Have[5] = {0, 0, 0, 0, 0}, Runs = 0, TooShort = 0; TArray<FVector2D> Placed;
                auto Opening = [&](int32 C, int32 B) { const float Lo = FMath::Max(A0(C), A0(B)), Hi = FMath::Min(A1(C), A1(B)); return (Lo + Hi) * .5f; };
                for (int32 W0 = 1; W0 < Way.Num(); )
                {
                    int32 W1 = W0; while (W1 + 1 < Way.Num() && Ring(Way[W1 + 1]) == Ring(Way[W0])) ++W1;
                    const int32 Wa = W0, I = Ring(Way[Wa]); W0 = W1 + 1;
                    if (I < 2 || I > Rings - 2 || W1 + 1 >= Way.Num()) continue;
                    ++Runs;
                    const float Rm = MazeR0 + (I + .5f) * MazePitch, Ain = Opening(Way[Wa], Way[Wa - 1]), Dl = FMath::FindDeltaAngleRadians(Ain, Opening(Way[W1], Way[W1 + 1]));
                    const float Jd = Dl >= 0.f ? 1.f : -1.f, HalfS = FMath::Abs(Dl) * Rm * .5f, Ac0 = Ain + Dl * .5f;
                    int32 Pick = -1; float Best = 1e9f, Shift = 0.f, Side = 1.f;
                    for (int32 Kt = 0; Kt < 5; ++Kt)
                    {
                        if (Ext[Kt][1] - Ext[Kt][0] > 2.f * (HalfS - Margin)) continue;
                        const float Sh = -(Ext[Kt][0] + Ext[Kt][1]) * .5f;
                        float Sd = 1.f;
                        if (Kt == 0)   // the trellis's hedge unbroken beside it (no opening or shortcut there)
                        {
                            Sd = 0.f;
                            for (const float Sx : {1.f, -1.f})
                            {
                                const int32 Io = Sx > 0.f ? I + 1 : I - 1; bool bOpen = false;
                                for (int32 W = Wa; W <= W1; ++W) for (const int32 B : Adj[Way[W]]) if (Ring(B) == Io && (Links.Contains(Key(Way[W], B)) || ScOf.Contains(Key(Way[W], B))))
                                { const float So = FMath::FindDeltaAngleRadians(Ac0, Opening(Way[W], B)) * Rm * Jd - Sh; bOpen |= FMath::Abs(So) < FeyMap::TrellisReach + 40.f + MazeGap * .5f; }
                                if (!bOpen) { Sd = Sx; break; }
                            }
                            if (Sd == 0.f) continue;
                        }
                        const float Score = Have[Kt] + Pref[Kt];
                        if (Score < Best) { Best = Score; Pick = Kt; Shift = Sh; Side = Sd; }
                    }
                    if (Pick < 0) { ++TooShort; continue; }
                    const float Ac = Ac0 + Jd * Shift / Rm; const FVector2D Cp = MazeAt(Rm, Ac);
                    if (FVector2D::Distance(Cp, Middle) < 900.f) continue;
                    bool bNear = false; for (const FVector2D& P : Placed) bNear |= FVector2D::Distance(P, Cp) < 700.f;
                    if (bNear) continue;
                    FeyMap::FMazeParkour Pk; Pk.Kind = Pick == 4 ? 1 : Pick; Pk.NHoop = Pick == 4 ? 3 : FeyMap::Hoops; Pk.R = Rm; Pk.Ac = Ac; Pk.Dir = Jd; Pk.Side = Pick == 0 ? Side : 1.f;
                    Pk.Bed0 = Beds[Pick][0]; Pk.Bed1 = Beds[Pick][1];
                    Out.Parkour.Add(Pk); Placed.Add(Cp); ++Have[Pick];
                    for (int32 W = Wa; W <= W1; ++W) ParkCells.Add(Way[W]);
                }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_PARKOUR_PLAN trellis=%d hoops=%d walkways=%d jumps=%d way_hoops=%d runs=%d too_short=%d"), Have[0], Have[1], Have[2], Have[3], Have[4], Runs, TooShort);
            }
            // (2026-11-23) The orchids' lanes after the parkour, on straight runs of the way it left free.
            // Straight runs along the way through (orchids' lanes): three linked cells on one ring, on the way.
            for (int32 W = 2; W < Way.Num() && Out.Lanes.Num() < 3; ++W)
                if (Ring(Way[W]) == Ring(Way[W - 1]) && Ring(Way[W]) == Ring(Way[W - 2]) && Ring(Way[W]) > 6 && !ParkCells.Contains(Way[W]) && !ParkCells.Contains(Way[W - 1]) && !ParkCells.Contains(Way[W - 2]))
                { if (Out.Lanes.Num() && FVector2D::Distance(FVector2D(Out.Lanes.Last()), Mid(Way[W])) < 3000.f) continue; const FVector2D A = Mid(Way[W - 2]), B = Mid(Way[W]); Out.Lanes.Add(FVector(A.X, A.Y, FMath::Atan2(B.Y - A.Y, B.X - A.X))); for (int32 K = W - 2; K <= W; ++K) LaneCells.Add(Way[K]); }
            // (2026-11-23) Root gaps: holes at the foot of a hedge, Chuck-sized (the redcaps cannot follow), between two cells that
            // are not linked, saving a few cells; never by the parkour, a shortcut or another gap.
            TMap<uint64, int32> GapOf;
            // (2026-11-23, second round) Three of the way's own steps inward are root gaps: he must crawl through them (and the
            // redcaps cannot follow).
            {
                TArray<int32> Steps;
                for (int32 W = 4; W + 4 < Way.Num(); ++W)
                    if (Ring(Way[W]) != Ring(Way[W - 1]) && Ring(Way[W]) > 2 && Ring(Way[W - 1]) > 2)
                    {
                        const int32 C = Way[W - 1], B = Way[W]; const float Lo = FMath::Max(A0(C), A0(B)), Hi = FMath::Min(A1(C), A1(B));
                        const FVector2D G = MazeAt(MazeR0 + FMath::Max(Ring(C), Ring(B)) * MazePitch, (Lo + Hi) * .5f); bool bNear = false;
                        for (const FeyMap::FMazeParkour& Pk : Out.Parkour) for (float Sb = -520.f; Sb <= 380.f; Sb += 60.f) bNear |= FVector2D::Distance(MazeAt(Pk.R, Pk.Ac + Pk.Dir * Sb / Pk.R), G) < 230.f;
                        if (!bNear) Steps.Add(W);
                    }
                const FVector2D Patrol = Mid(Way[Way.Num() / 2]);   // (where the redcaps keep: none in a gap's mouth)
                for (const float Fr : {.2f, .7f, .85f})
                {
                    if (!Steps.Num()) break;
                    const int32 S0 = FMath::Clamp(int32(Fr * Steps.Num()), 0, Steps.Num() - 1); int32 Si = -1;
                    for (int32 Dd = 0; Dd < Steps.Num() && Si < 0; ++Dd)
                        for (const int32 Sg : {1, -1})
                        {
                            const int32 K = S0 + Sg * Dd; if (Si >= 0 || !Steps.IsValidIndex(K)) continue;
                            if (!GapOf.Contains(Key(Way[Steps[K] - 1], Way[Steps[K]])) && FVector2D::Distance(Mid(Way[Steps[K]]), Patrol) > 2000.f) Si = K;
                        }
                    if (Si < 0) continue;
                    const int32 W = Steps[Si], C = Way[W - 1], B = Way[W];
                    const int32 Hr = FMath::Max(Ring(C), Ring(B)); const float Lo = FMath::Max(A0(C), A0(B)), Hi = FMath::Min(A1(C), A1(B));
                    const FVector2D G = MazeAt(MazeR0 + Hr * MazePitch, (Lo + Hi) * .5f);
                    GapOf.Add(Key(C, B), Out.Gaps.Num()); Out.Gaps.Add({G, FMath::RadiansToDegrees((Lo + Hi) * .5f) + 90.f});
                    // The way through lines up with the hole: a point straight before it and one straight after (from a cell's
                    // middle it met the hole aslant, too narrow for that).
                    for (int32 K = 1; K + 1 < Out.Route.Num(); ++K)
                        if (FVector2D::Distance(Out.Route[K], G) < 1.f)
                        {
                            const FVector2D Rd = (G - Table[Maze].Hub).GetSafeNormal(), Ino = FVector2D::DotProduct(Out.Route[K - 1] - G, Rd) > 0.f ? -Rd : Rd;
                            Out.Route[K] = G + Ino * 220.f; Out.Route.Insert(G - Ino * 150.f, K);   // (the hole's own point dropped: a walker counted it reached a metre short)
                            break;
                        }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_GAP way=1 at=%s"), *G.ToString());
                }
            }
            {
                struct FG { int32 C, B, Gain; };
                TArray<FG> Gs;
                for (int32 C = 0; C < Total; ++C) for (const int32 B : Adj[C]) if (B > C && !Links.Contains(Key(C, B)) && !ScOf.Contains(Key(C, B)) && WaySet.Contains(C) && WaySet.Contains(B) && Ring(C) > 2 && Ring(B) > 2)   // (2026-11-23: both on the way through)
                { const int32 G = FMath::Abs(Dist[C] - Dist[B]) - 1; if (G >= 4 && G < 14) Gs.Add({C, B, G}); }
                Gs.Sort([](const FG& X, const FG& Y) { return X.Gain > Y.Gain; });
                for (const FG& G : Gs)
                {
                    if (Out.Gaps.Num() >= 6) break;
                    FVector2D P; float Yaw;
                    if (Ring(G.C) == Ring(G.B))   // the wall between neighbours on one ring, running out from the hub
                    {
                        const int32 I = Ring(G.C), Lo = ((G.C - Base[I] + 1) % N[I]) == G.B - Base[I] ? G.C : G.B; const float A = A1(Lo);
                        P = MazeAt(MazeR0 + (I + .5f) * MazePitch, A); Yaw = FMath::RadiansToDegrees(A);
                    }
                    else   // a ring's hedge, running round
                    {
                        const int32 Hr = FMath::Max(Ring(G.C), Ring(G.B)); const float Lo = FMath::Max(A0(G.C), A0(G.B)), Hi = FMath::Min(A1(G.C), A1(G.B));
                        if ((Hi - Lo) * (MazeR0 + Hr * MazePitch) < 220.f) continue;
                        P = MazeAt(MazeR0 + Hr * MazePitch, (Lo + Hi) * .5f); Yaw = FMath::RadiansToDegrees((Lo + Hi) * .5f) + 90.f;
                    }
                    bool bNear = false;
                    for (const FeyMap::FMazeGap& O : Out.Gaps) bNear |= FVector2D::Distance(O.C, P) < 2600.f;
                    for (const FeyMap::FMazeShortcut& O : Out.Shortcuts) bNear |= FVector2D::Distance(O.Mid, P) < 1500.f;
                    for (const FeyMap::FMazeParkour& O : Out.Parkour) bNear |= FVector2D::Distance(MazeAt(O.R, O.Ac), P) < 1500.f;
                    if (bNear) continue;
                    GapOf.Add(Key(G.C, G.B), Out.Gaps.Num()); Out.Gaps.Add({P, Yaw});
                }
            }
            Out.Route.Add(MazeAt(MazeR0, HeartA)); Out.Route.Add(MazeAt(MazeR0 - 700.f, HeartA));
            // The hedges: each ring's boundary in arcs, open where two cells are linked; the walls between neighbours.
            auto Arc = [&](float Rb, float Lo, float Hi, int32 Sc = -1)
            {
                const int32 Segs = FMath::Max(1, FMath::CeilToInt((Hi - Lo) * Rb / 300.f));
                for (int32 K = 0; K < Segs; ++K)
                {
                    const float A = FMath::Lerp(Lo, Hi, (K + .5f) / Segs), Len = (Hi - Lo) * Rb / Segs;
                    const FVector2D P = MazeAt(Rb, A);
                    Out.Hedges.Add({FVector(P.X, P.Y, HedgeTall * .5f), FVector(Len * .5f + 6.f, HedgeHalf, HedgeTall * .5f), FMath::RadiansToDegrees(A) + 90.f, 0, Sc});
                }
            };
            auto Open = [&](float Rb, float Lo, float Hi)   // a hedge over [Lo, Hi] with a way through its middle
            {
                const float Mid = (Lo + Hi) * .5f, HalfA = MazeGap * .5f / Rb;
                Arc(Rb, Lo, Mid - HalfA); Arc(Rb, Mid + HalfA, Hi);
            };
            // (2026-11-23) A root gap's hedge over the hole (its foot at the gap's height, the hole roofed with root in FeyMap's dressing).
            auto Lintel = [&](const FVector2D& P, float Yaw, float Hl)
            { const float Hz = (HedgeTall - 6.f - FeyMap::GapTall) * .5f; Out.Hedges.Add({FVector(P.X, P.Y, FeyMap::GapTall + 6.f + Hz), FVector(Hl, HedgeHalf, Hz), Yaw, 0, -1}); };
            for (int32 I = 0; I <= Rings; ++I)
            {
                const float Rb = MazeR0 + I * MazePitch;
                if (I == 0 || I == Rings)
                {
                    const int32 Ri = I == 0 ? 0 : Rings - 1;
                    for (int32 J = 0; J < N[Ri]; ++J)
                    {
                        const int32 C = Id(Ri, J);
                        if ((I == 0 && C == Heart) || (I == Rings && C == Gate)) Open(Rb, A0(C), A1(C)); else Arc(Rb, A0(C), A1(C));
                    }
                    continue;
                }
                for (int32 J = 0; J < N[I]; ++J)
                {
                    const int32 O = Id(I, J);
                    for (int32 K = 0; K < N[I - 1]; ++K)
                    {
                        const int32 C = Id(I - 1, K); const float Lo = FMath::Max(A0(C), A0(O)), Hi = FMath::Min(A1(C), A1(O));
                        if (Hi <= Lo) continue;
                        if (Links.Contains(Key(C, O)) && !GapOf.Contains(Key(C, O))) Open(Rb, Lo, Hi);
                        else if (GapOf.Contains(Key(C, O))) { const float Md = (Lo + Hi) * .5f, Ha = FeyMap::GapHalf / Rb; Arc(Rb, Lo, Md - Ha); Arc(Rb, Md + Ha, Hi); Lintel(MazeAt(Rb, Md), FMath::RadiansToDegrees(Md) + 90.f, FeyMap::GapHalf + 6.f); }
                        else { const int32* Sc = ScOf.Find(Key(C, O)); Arc(Rb, Lo, Hi, Sc ? *Sc : -1); }
                    }
                }
            }
            for (int32 I = 0; I < Rings; ++I)
                for (int32 J = 0; J < N[I]; ++J)
                {
                    if (Links.Contains(Key(Id(I, J), Id(I, J + 1)))) continue;
                    const float A = A1(Id(I, J)); const FVector2D P = MazeAt(MazeR0 + (I + .5f) * MazePitch, A);
                    const int32* Sc = ScOf.Find(Key(Id(I, J), Id(I, J + 1)));
                    if (GapOf.Contains(Key(Id(I, J), Id(I, J + 1))))   // in two, the gap between, the hedge over it
                    {
                        const float Rm = MazeR0 + (I + .5f) * MazePitch, L = MazePitch * .5f + HedgeHalf;
                        for (const float Sd : {-1.f, 1.f}) { const FVector2D Qg = MazeAt(Rm + Sd * (FeyMap::GapHalf + L) * .5f, A); Out.Hedges.Add({FVector(Qg.X, Qg.Y, HedgeTall * .5f), FVector((L - FeyMap::GapHalf) * .5f, HedgeHalf, HedgeTall * .5f), FMath::RadiansToDegrees(A), 0, -1}); }
                        Lintel(P, FMath::RadiansToDegrees(A), FeyMap::GapHalf + 6.f);
                        continue;
                    }
                    Out.Hedges.Add({FVector(P.X, P.Y, HedgeTall * .5f), FVector(MazePitch * .5f + HedgeHalf, HedgeHalf, HedgeTall * .5f), FMath::RadiansToDegrees(A), 0, Sc ? *Sc : -1});
                }
            // Stone arches at the gate and the heart; dead ends (one way in) for the statues.
            for (const FVector2D& Arch : {FVector2D(MazeRmax, Out.Gate), FVector2D(MazeR0, HeartA)})
            {
                const float A = Arch.Y, D = FMath::RadiansToDegrees(A); const FVector2D C = MazeAt(Arch.X, A), Across(-FMath::Sin(A), FMath::Cos(A));
                for (const float Sd : {-1.f, 1.f}) { const FVector2D P = C + Across * Sd * 200.f; Out.Hedges.Add({FVector(P.X, P.Y, 470.f), FVector(70.f, 70.f, 470.f), D, 1}); }
                Out.Hedges.Add({FVector(C.X, C.Y, 990.f), FVector(70.f, 270.f, 60.f), D, 1});
            }
            for (int32 C = 0; C < Total; ++C) { int32 Exits = 0; for (int32 B : Adj[C]) Exits += Links.Contains(Key(C, B)); if (Exits == 1 && C != Gate && C != Heart) Out.DeadEnds.Add(Mid(C)); }
            // (2026-11-23, the user: frowning statues in dead ends, smiling ones on the way) About twenty dead ends, spread over
            // the maze, each with a frowning stone head set back from its one way in and facing it.
            {
                TArray<int32> Ends; for (int32 C = 0; C < Total; ++C) { int32 Exits = 0; for (int32 B : Adj[C]) Exits += Links.Contains(Key(C, B)); if (Exits == 1 && C != Gate && C != Heart && Ring(C) > 0) Ends.Add(C); }
                for (int32 K = 0, Step = FMath::Max(1, Ends.Num() / 20); K < Ends.Num() && Out.Frowns.Num() < 20; K += Step)
                {
                    const int32 C = Ends[K]; int32 B = -1; for (const int32 O : Adj[C]) if (Links.Contains(Key(C, O))) B = O;
                    const int32 I = Ring(C), Ib = Ring(B); FVector2D Wy;
                    if (Ib == I) Wy = MazeAt(MazeR0 + (I + .5f) * MazePitch, FMath::Abs(FMath::FindDeltaAngleRadians(A1(C), A0(B))) < .01f ? A1(C) : A0(C));
                    else { const float Lo = FMath::Max(A0(C), A0(B)), Hi = FMath::Min(A1(C), A1(B)); Wy = MazeAt(MazeR0 + FMath::Max(I, Ib) * MazePitch, (Lo + Hi) * .5f); }
                    const FVector2D M = Mid(C), Back = (M - Wy).GetSafeNormal(), P = M + Back * (Ib == I ? 120.f : 40.f), Face = (Wy - P).GetSafeNormal();
                    Out.Frowns.Add(FVector4(P.X, P.Y, Face.X, Face.Y));
                }
            }
            // (2026-11-23, second round) Along the way only: stone figures, the statues that turn, thorn mites and cigarettes,
            // each against a hedge with no opening on its side, clear of the parkour, the lanes and each other.
            auto SideClear = [&](int32 C, int32 Io, float A, float Rb, float Clear)
            {
                if (Io < 0) return C != Heart;
                if (Io >= Rings) return C != Gate;
                for (const int32 B : Adj[C]) if (Ring(B) == Io && (Links.Contains(Key(C, B)) || ScOf.Contains(Key(C, B)) || GapOf.Contains(Key(C, B))))
                { const float Lo = FMath::Max(A0(C), A0(B)), Hi = FMath::Min(A1(C), A1(B)); if (FMath::Abs(FMath::FindDeltaAngleRadians(A, (Lo + Hi) * .5f)) * Rb < MazeGap * .5f + Clear) return false; }
                return true;
            };
            {
                const int32 Seq[] = {0, 1, 3, 0, 2, 1, 0, 3, 1, 0, 2, 1, 3, 0, 1, 0, 3, 0};
                int32 Next = 0; TArray<FVector2D> Done;
                for (int32 W = 3; W + 3 < Way.Num() && Next < UE_ARRAY_COUNT(Seq); ++W)
                {
                    const int32 C = Way[W], I = Ring(C);
                    if (LaneCells.Contains(C)) continue;
                    const float Rm = MazeR0 + (I + .5f) * MazePitch; float A = 0.f, Sd = 0.f;
                    for (const float U : {.5f, .28f, .72f}) for (const float Sx : {1.f, -1.f})
                        if (Sd == 0.f && SideClear(C, I + (Sx > 0.f ? 1 : -1), A0(C) + (A1(C) - A0(C)) * U, MazeR0 + (I + (Sx > 0.f ? 1 : 0)) * MazePitch, 90.f)) { Sd = Sx; A = A0(C) + (A1(C) - A0(C)) * U; }
                    if (Sd == 0.f) continue;
                    const FVector2D P = MazeAt(Rm + Sd * 82.f, A);
                    bool bNear = false; for (const FVector2D& Qd : Done) bNear |= FVector2D::Distance(Qd, P) < 900.f;
                    for (const FeyMap::FMazeParkour& Pk : Out.Parkour) for (float Sb = -560.f; Sb <= 420.f; Sb += 70.f) bNear |= FVector2D::Distance(MazeAt(Pk.R, Pk.Ac + Pk.Dir * Sb / Pk.R), P) < 200.f;
                    if (bNear) continue;
                    FeyMap::FMazeMark Mk; Mk.P = P; Mk.In = (MazeAt(Rm, A) - P).GetSafeNormal(); Mk.Kind = Seq[Next++];
                    Out.Marks.Add(Mk); Done.Add(P);
                }
            }
            // (2026-11-23, the user: spots narrow enough to wall jump to the top, only to be bee-d back down) Four squeezes on the
            // way: both hedges bulge in to leave a slot a metre wide (the temple chimney's), where no opening is near.
            {
                TArray<FVector2D> Taken; for (const FeyMap::FMazeMark& Mk : Out.Marks) Taken.Add(Mk.P);
                for (const FeyMap::FMazeParkour& Pk : Out.Parkour) Taken.Add(MazeAt(Pk.R, Pk.Ac));
                for (const float Fr : {.2f, .4f, .6f, .8f})
                    for (int32 W = FMath::Max(3, int32(Fr * Way.Num())); W + 3 < Way.Num(); ++W)
                    {
                        const int32 C = Way[W], I = Ring(C);
                        if (ParkCells.Contains(C) || LaneCells.Contains(C) || I < 2) continue;
                        const float Rm = MazeR0 + (I + .5f) * MazePitch, A = (A0(C) + A1(C)) * .5f, Clear = FeyMap::SqueezeLen * .5f + 40.f;
                        if (!SideClear(C, I + 1, A, MazeR0 + (I + 1) * MazePitch, Clear) || !SideClear(C, I - 1, A, MazeR0 + I * MazePitch, Clear)) continue;
                        const FVector2D P = MazeAt(Rm, A); bool bNear = false; for (const FVector2D& Qt : Taken) bNear |= FVector2D::Distance(Qt, P) < 700.f;
                        if (bNear) continue;
                        const float Th = (130.f - FeyMap::SqueezeGap * .5f) * .5f + 4.f;   // (each bulge, from a little inside its hedge's face)
                        for (const float Sd : {-1.f, 1.f}) { const FVector2D Qb = MazeAt(Rm + Sd * (FeyMap::SqueezeGap * .5f + Th), A); Out.Hedges.Add({FVector(Qb.X, Qb.Y, HedgeTall * .5f), FVector(FeyMap::SqueezeLen * .5f, Th, HedgeTall * .5f), FMath::RadiansToDegrees(A) + 90.f, 0, -1}); }
                        FeyMap::FMazeMark Sq; Sq.P = P; Sq.In = (MazeAt(Rm + 10.f, A) - P).GetSafeNormal(); Out.Squeezes.Add(Sq); Taken.Add(P);
                        break;
                    }
            }
            // (2026-11-23, the user: blue lamps as in the Deepwood, not as bright; very low fog in parts; more decor) Spread evenly
            // over the whole maze, so none of it marks the way: lanterns (0), plants at the hedges' feet (1), stone urns (2),
            // benches (3), patches of low fog (4), fallen petals (5).
            {
                FRandomStream Rd(5150);
                TArray<FVector2D> Busy;
                for (const FeyMap::FMazeParkour& Pk : Out.Parkour) for (float Sb = -520.f; Sb <= 380.f; Sb += 90.f) Busy.Add(MazeAt(Pk.R, Pk.Ac + Pk.Dir * Sb / Pk.R));
                for (const FeyMap::FMazeMark& Mk : Out.Marks) Busy.Add(Mk.P);
                for (const FeyMap::FMazeGap& G : Out.Gaps) Busy.Add(G.C);
                for (const FeyMap::FMazeMark& Sq : Out.Squeezes) { Busy.Add(Sq.P); const FVector2D Tn(-Sq.In.Y, Sq.In.X); Busy.Add(Sq.P + Tn * 130.f); Busy.Add(Sq.P - Tn * 130.f); }
                auto Near = [&](const FVector2D& P, float Rr) { for (const FVector2D& B : Busy) if (FVector2D::DistSquared(B, P) < Rr * Rr) return true; return false; };
                for (int32 C = 0; C < Total; ++C)
                {
                    const int32 I = Ring(C); const float Rm = MazeR0 + (I + .5f) * MazePitch, Span = A1(C) - A0(C);
                    for (int32 K = 0; K < 3; ++K)
                    {
                        const float U = Rd.FRandRange(.22f, .78f), A = A0(C) + Span * U, Sd = Rd.FRand() < .5f ? 1.f : -1.f, Roll = Rd.FRand();
                        if (!SideClear(C, I + (Sd > 0.f ? 1 : -1), A, MazeR0 + (I + (Sd > 0.f ? 1 : 0)) * MazePitch, 60.f)) continue;
                        const FVector2D P = MazeAt(Rm + Sd * 112.f, A), Inw = (MazeAt(Rm, A) - P).GetSafeNormal();
                        if (Near(P, 200.f)) continue;
                        const int32 Kind = K == 0 && Roll < .13f ? 0 : K == 1 && Roll < .06f ? 2 : K == 2 && Roll < .035f ? 3 : Roll < .6f ? 1 : -1;
                        if (Kind < 0) continue;
                        Out.Decor.Add(FVector4(P.X, P.Y, FMath::Atan2(Inw.Y, Inw.X), Kind));
                    }
                    if (Rd.FRand() < .35f) { const FVector2D P = MazeAt(Rm + Rd.FRandRange(-60.f, 60.f), A0(C) + Span * Rd.FRandRange(.25f, .75f)); if (!Near(P, 150.f)) Out.Decor.Add(FVector4(P.X, P.Y, Rd.FRand() * 2.f * PI, 5)); }
                    if (Rd.FRand() < .012f && I > 1) { const FVector2D P = MazeAt(Rm, A0(C) + Span * .5f); Out.Decor.Add(FVector4(P.X, P.Y, 0.f, 4)); }
                }
                int32 Count[6] = {0, 0, 0, 0, 0, 0}; for (const FVector4& Dc : Out.Decor) ++Count[FMath::Clamp(int32(Dc.W), 0, 5)];
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_MARKS squeezes=%d marks=%d gaps=%d shortcuts=%d decor lanterns=%d plants=%d urns=%d benches=%d fog=%d petals=%d"), Out.Squeezes.Num(), Out.Marks.Num(), Out.Gaps.Num(), Out.Shortcuts.Num(), Count[0], Count[1], Count[2], Count[3], Count[4], Count[5]);
            }
            for (FHedge& Hd : Out.Hedges) if (Hd.Sc >= 0) Out.Shortcuts[Hd.Sc].Hedges.Add(FVector4(Hd.C.X, Hd.C.Y, Hd.H.X, Hd.Yaw));
            Out.Cells = Total;
            return Out;
        }();
        return S;
    }
    // The Cloud Staircase (2026-11-21): one step after another round the maze's heart and up to the tower's cloud. The first
    // eight (22 degrees and 34 cm each, 11 m out) are walked now; the rest climb ever steeper. Each step's angle, radius, top,
    // angular span, half width across and depth below its top (always more than the rise, so it sinks into the one below).
    struct FStairStep { float A, R, Z, Da, W, Dp; };
    // (2026-11-22, stage 7) The walked stair: 56 steps of 34 cm winding out from 11 to 22 m round the heart and up over its
    // hedges, then a broad landing of cloud 19 m up (the stable boundary until the tower is planned); beyond it the stair
    // climbs on to the tower's cloud, scenery for now.
    constexpr int32 StairWalked = 57, StairLanding = 56, StairSteps = 150; constexpr float TowerBase = 38000.f;
    const TArray<FStairStep>& Stair()
    {
        static TArray<FStairStep> S = []()
        {
            TArray<FStairStep> Out; float A = FMath::DegreesToRadians(200.f), Prev = 0;
            for (int32 I = 0; I < StairSteps; ++I)
            {
                FStairStep St;
                if (I < StairLanding) { St.R = FMath::Lerp(1100.f, 2200.f, I / float(StairLanding - 1)); St.Da = 430.f / St.R; St.Z = 34.f * (I + 1); St.W = 200.f; }
                else if (I == StairLanding) { St.R = 2400.f; St.Da = 1500.f / St.R; St.Z = 34.f * (I + 1); St.W = 430.f; }
                else
                {
                    const float U = float(I - StairLanding) / (StairSteps - StairLanding - 1), Z0 = 34.f * (StairLanding + 1);
                    St.R = FMath::Lerp(2400.f, 4300.f, FMath::Pow(U, .8f)); St.Da = 440.f / St.R;
                    St.Z = Z0 + (TowerBase - 900.f - Z0) * FMath::Pow(U, 1.45f); St.W = FMath::Lerp(200.f, 560.f, U);
                }
                if (I > 0) A += (Out.Last().Da + St.Da) * .5f;
                St.A = A; St.Dp = (St.Z - Prev) + 160.f + .3f * St.W; Prev = St.Z;
                Out.Add(St);
            }
            return Out;
        }();
        return S;
    }
    float MazeRouteLength() { float L = 0; const TArray<FVector2D>& P = MazeData().Route; for (int32 I = 1; I < P.Num(); ++I) L += FVector2D::Distance(P[I], P[I - 1]); return L; }

    // ---------------------------------------------------------------- the deep water (the user, 2026-11-20)
    // Deep water only crossed by hopping lily pads: a moat round the Lily Mere's second threshold (from the Crossroads), so
    // that way into the mere is over the pads. Pads lie along the road over it, a hop apart (his run's jump is about 1 m).
    const FVector2D MoatC(-73500.f, 15000.f);
    constexpr float MoatIn = 1500.f, MoatOut = 2650.f, MoatBed = -170.f;
    float MoatK(float X, float Y) { if (Y < MoatC.Y) return 0.f; const float R = FVector2D::Distance(FVector2D(X, Y), MoatC); return Smooth(MoatIn - 220.f, MoatIn, R) * Smooth(MoatOut + 220.f, MoatOut, R); }
    const TArray<FVector>& MoatPads()   // (X, Y, radius)
    {
        static TArray<FVector> S = []()
        {
            TArray<FVector> Out; const TArray<FVector2D>& Road = Data().RoadOf[LilyMere][4]; int32 N = 0;
            float Next = 0, Run = 0;
            for (int32 I = 0; I + 1 < Road.Num(); ++I)
            {
                const float Len = FVector2D::Distance(Road[I], Road[I + 1]); const FVector2D T = (Road[I + 1] - Road[I]).GetSafeNormal();
                for (; Next < Run + Len; Next += 118.f)
                {
                    const FVector2D P = Road[I] + T * (Next - Run);
                    if (MoatK(P.X, P.Y) < .2f) continue;
                    const float Side = (N % 2 ? 1.f : -1.f) * (N % 3 == 0 ? 10.f : 26.f);
                    Out.Add(FVector(P.X - T.Y * Side, P.Y + T.X * Side, N % 4 == 1 ? 62.f : 55.f)); ++N;
                }
                Run += Len;
            }
            return Out;
        }();
        return S;
    }

    // ---------------------------------------------------------------- the signs (the user, 2026-11-20)
    struct FSignPost { int32 D; FVector2D At; TArray<FVector2D> Arrows; };   // arrows: (yaw, pitch) in degrees
    float YawTo(const FVector2D& From, const FVector2D& To) { return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X)); }
    const TArray<FSignPost>& SignPosts()
    {
        static TArray<FSignPost> S = []()
        {
            TArray<FSignPost> Out; const FData& Dt = Data();
            // A post beside road (D, T) at its K-th point (2 m to its left), with arrows toward each target.
            auto Post = [&](int32 D, int32 T, int32 K, TArray<FVector2D> To, TArray<FVector2D> Odd = {})
            {
                const TArray<FVector2D>& R = Dt.RoadOf[D][T]; K = FMath::Clamp(K, 0, R.Num() - 2);
                const FVector2D Tn = (R[K + 1] - R[K]).GetSafeNormal(), At = R[K] + Tn * 300.f + FVector2D(-Tn.Y, Tn.X) * 210.f;
                FSignPost P{D, At, {}};
                for (const FVector2D& Q : To) P.Arrows.Add(FVector2D(YawTo(At, Q), 0.f));
                P.Arrows.Append(Odd);
                Out.Add(P);
            };
            auto Ring = [&](int32 D, int32 T) { return Dt.RoadOf[D][T].Last(); };
            auto Mouth = [&](int32 D, int32 T) { return Dt.RoadOf[D][T][0]; };
            // The riverbank: on toward the orchard (and back to the river).
            { const TArray<FVector2D>& R = Dt.RoadOf[Riverbank][0]; Post(Riverbank, 0, R.Num() - 3, {R[1], FVector2D(3000.f, 0.f)}); }
            // The orchard: to the Needle Garden's lanes, then on to the Crossroads by its far road.
            Post(Orchard, 0, 1, {FVector2D(-47000.f, -9300.f), Ring(Orchard, 1)});
            Post(Orchard, 1, 1, {Mouth(Orchard, 1)});
            // The Crossroads: to the Lily Mere first.
            Post(Crossroads, 1, 1, {Ring(Crossroads, 3), Ring(Crossroads, 2)});
            // The Lily Mere: the fen, then the pads.
            Post(LilyMere, 3, 1, {Dt.SpurOf[LilyMere].Last(), Ring(LilyMere, 4)});
            Post(LilyMere, 4, Dt.RoadOf[LilyMere][4].Num() - 2, {MoatC});
            // Back at the Crossroads: into the Deepwood.
            Post(Crossroads, 4, 1, {Ring(Crossroads, 5)});
            // The Deepwood: to Redcap Hollow, and from it the hidden way.
            Post(Deepwood, 5, 1, {Dt.SpurOf[Deepwood].Last()});
            Post(Deepwood, 7, Dt.RoadOf[Deepwood][7].Num() - 2, {Mouth(Deepwood, 7)}, {FVector2D(200.f, 35.f)});
            // Highmeadow: the ford, the overlook, and now the arrows start to lie (one points at the sky).
            Post(Highmeadow, 7, 1, {FVector2D(-95000.f, 2500.f), Overlook}, {FVector2D(40.f, 70.f)});
            { FSignPost P{Highmeadow, Overlook + FVector2D(250.f, -350.f), {}};
              P.Arrows = {FVector2D(YawTo(P.At, Mouth(Highmeadow, 9)), 0.f), FVector2D(185.f, 62.f), FVector2D(10.f, -55.f), FVector2D(95.f, 0.f), FVector2D(275.f, 18.f)}; Out.Add(P); }
            // The maze's gate: arrows every way, up and down among them; at the heart one points straight up.
            { const FVector2D G = MazeAt(MazeRmax + 1100.f, .2f); FSignPost P{Maze, G, {}};
              for (int32 I = 0; I < 8; ++I) P.Arrows.Add(FVector2D(I * 45.f + 10.f, I % 3 == 0 ? 0.f : I % 3 == 1 ? 50.f : -40.f)); Out.Add(P); }
            { FSignPost P{Maze, MazeAt(700.f, .4f), {FVector2D(30.f, 88.f), FVector2D(210.f, -88.f)}}; Out.Add(P); }
            return Out;
        }();
        return S;
    }
    // One arrow board: a pentagon 86 cm long and 16 cm high, 3.6 cm thick, glyphs cut into both faces.
    void SignBoard(FMapSurf& S, const FVector& At, float YawDeg, float PitchDeg, float Seed)
    {
        const float Y = FMath::DegreesToRadians(YawDeg), Pt = FMath::DegreesToRadians(PitchDeg);
        const FVector F(FMath::Cos(Pt) * FMath::Cos(Y), FMath::Cos(Pt) * FMath::Sin(Y), FMath::Sin(Pt));
        FVector U = FVector::UpVector - F * F.Z; if (!U.Normalize()) U = FVector(-FMath::Sin(Y), FMath::Cos(Y), 0.f);
        const FVector Sd = FVector::CrossProduct(F, U).GetSafeNormal();
        constexpr float L = 78.f, Hh = 8.f, T = 1.8f;
        const FVector2D Shape[5] = {{-8.f, -Hh}, {L - 12.f, -Hh}, {L, 0.f}, {L - 12.f, Hh}, {-8.f, Hh}};
        auto P = [&](int32 I, float W) { return At + F * Shape[I].X + U * Shape[I].Y + Sd * W; };
        const uint8 Sv = uint8(Seed * 255.f);
        for (const float Side : {1.f, -1.f})
        {
            int32 Ix[5];
            for (int32 I = 0; I < 5; ++I) Ix[I] = S.Vert(P(I, T * Side), Sd * Side, FVector2D((Shape[I].X + 8.f) / (L + 8.f), (Shape[I].Y + Hh) / (2 * Hh)), FColor(Sv, 255, 0, 255));
            S.TriFacing(Ix[0], Ix[1], Ix[3], Sd * Side); S.TriFacing(Ix[0], Ix[3], Ix[4], Sd * Side); S.TriFacing(Ix[1], Ix[2], Ix[3], Sd * Side);
        }
        const FVector Mid = At + F * (L * .45f);
        for (int32 I = 0; I < 5; ++I)
        {
            const int32 J = (I + 1) % 5; FVector Out = ((P(I, 0.f) + P(J, 0.f)) * .5f - Mid).GetSafeNormal();
            S.Face(P(I, T), P(J, T), P(J, -T), P(I, -T), Out, FColor(Sv, 0, 0, 255), 40.f);
        }
    }

    // ---------------------------------------------------------------- the kit
    struct FKitDef { const TCHAR* Path; float C0, C1; bool bShadow, bSolid, bInvisible; };
    const FKitDef KitDefs[] = {
        {TEXT("/Game/Art/Feywild/SM_FeyOak0.SM_FeyOak0"), 16000, 26000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOak1.SM_FeyOak1"), 16000, 26000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOak0Collision.SM_FeyOak0Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyOak1Collision.SM_FeyOak1Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyCapAmber.SM_FeyCapAmber"), 22000, 30000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyCapBlue.SM_FeyCapBlue"), 7000, 11000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyCapViolet.SM_FeyCapViolet"), 7000, 11000, false, false, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultFern0.SM_ChultFern0"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultFern1.SM_ChultFern1"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultFern2.SM_ChultFern2"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultFern3.SM_ChultFern3"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyBroadLeaf.SM_FeyBroadLeaf"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyCrimsonLeaf.SM_FeyCrimsonLeaf"), 6000, 9000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyFoxglove.SM_FeyFoxglove"), 7000, 11000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyCelandine0.SM_FeyCelandine0"), 3200, 5000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyCelandine1.SM_FeyCelandine1"), 3200, 5000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGrass0.SM_FeyGrass0"), 3600, 5400, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGrass1.SM_FeyGrass1"), 3600, 5400, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyPetals.SM_FeyPetals"), 3500, 5500, false, false, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock0.SM_ChultRock0"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock1.SM_ChultRock1"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock2.SM_ChultRock2"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock3.SM_ChultRock3"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock4.SM_ChultRock4"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/ChultNature/SM_ChultRock5.SM_ChultRock5"), 14000, 22000, true, true, false},
        {TEXT("/Game/Art/Feywild/SM_FeyLilyPad.SM_FeyLilyPad"), 12000, 18000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyLily.SM_FeyLily"), 9000, 14000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyBellLamp.SM_FeyBellLamp"), 9000, 14000, true, false, false},
        // The art pass (2026-11-20): the Deepwood's oaks (mossed, rooty, hung with moss), moss clumps, the big pads he hops.
        {TEXT("/Game/Art/Feywild/SM_FeyOakDeep0.SM_FeyOakDeep0"), 14000, 22000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOakDeep1.SM_FeyOakDeep1"), 14000, 22000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOakDeep0Collision.SM_FeyOakDeep0Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyOakDeep1Collision.SM_FeyOakDeep1Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyMoss.SM_FeyMoss"), 4000, 7000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyLilyPad.SM_FeyLilyPad"), 16000, 24000, false, false, false},
        // (2026-11-21) The maze's hedges' leaves: a shell of sprigs over each section, near only (the box's own leaves beyond).
        {TEXT("/Game/Art/Feywild/SM_FeyHedgeLeaves.SM_FeyHedgeLeaves"), 3800, 5600, false, false, false},
        // (2026-11-23) The maze's marble satyrs: SMK's public-domain scan of the Medici Faun (Tools/import_fey_satyr.py).
        {TEXT("/Game/Art/Feywild/Satyr/SM_FeySatyr.SM_FeySatyr"), 9000, 14000, true, false, false},
        {TEXT("/Game/Art/Feywild/Satyr/SM_FeySatyrFrown.SM_FeySatyrFrown"), 9000, 14000, true, false, false},   // (the same, scowling)
        // (2026-11-23, the wonder pass; Tools/build_feywild_kit.py) Each district's landmarks, archway ruins, the great wisteria,
        // oaks hung with purple moss. Seen from far (most stand out over the rest), shadowed; the arches and oaks with collision.
        {TEXT("/Game/Art/Feywild/SM_FeyGiantBell.SM_FeyGiantBell"), 30000, 40000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGiantToadstool.SM_FeyGiantToadstool"), 30000, 40000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGiantBlue.SM_FeyGiantBlue"), 30000, 40000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGiantLily.SM_FeyGiantLily"), 26000, 34000, false, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyGiantSpire.SM_FeyGiantSpire"), 30000, 40000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyArch0.SM_FeyArch0"), 24000, 32000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyArch1.SM_FeyArch1"), 24000, 32000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyArch0Collision.SM_FeyArch0Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyArch1Collision.SM_FeyArch1Collision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyOakWisteria.SM_FeyOakWisteria"), 40000, 52000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOakWisteriaCollision.SM_FeyOakWisteriaCollision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyOakPurpleMoss.SM_FeyOakPurpleMoss"), 16000, 26000, true, false, false},
        {TEXT("/Game/Art/Feywild/SM_FeyOakPurpleMossCollision.SM_FeyOakPurpleMossCollision"), 0, 0, false, true, true},
        {TEXT("/Game/Art/Feywild/SM_FeyHagCottage.SM_FeyHagCottage"), 30000, 40000, true, false, false},   // (2026-11-23) the hag's cottage
    };
    enum EKit { KOak0, KOak1, KCol0, KCol1, KAmber, KBlue, KViolet, KFern0, KBroad = 11, KCrimson, KFox, KCel0, KCel1, KGrass0, KGrass1, KPetals, KRock0, KPad = 25, KLily, KBell,
                KDeep0, KDeep1, KDCol0, KDCol1, KMoss, KPadBig, KHedge, KSatyr, KSatyrFrown,
                KGiantBell, KToad, KGiantBlue, KGiantLily, KSpire, KArch0, KArch1, KArchCol0, KArchCol1, KWist, KWistCol, KPurple, KPurpleCol, KHag, KitCount };
    static_assert(UE_ARRAY_COUNT(KitDefs) == KitCount, "a definition for every kit");
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround"), TEXT("/Game/Art/Feywild/M_FeyWater.M_FeyWater"), TEXT("/Game/Art/Feywild/M_FeyRiver.M_FeyRiver"),
                               TEXT("/Game/Art/Feywild/M_FeyBlock.M_FeyBlock"), TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot"), TEXT("/Game/Art/Feywild/M_FeyStone.M_FeyStone"),
                               TEXT("/Game/Art/Feywild/M_FeyHedge.M_FeyHedge"), TEXT("/Game/Art/Feywild/M_FeySign.M_FeySign"),
                               TEXT("/Game/Art/Feywild/M_FeyPlant.M_FeyPlant"), TEXT("/Game/Art/Feywild/M_FeyPlank.M_FeyPlank"), TEXT("/Game/Art/Feywild/Satyr/M_FeyMarble.M_FeyMarble")};
    enum EMat { MGround, MWater, MRiver, MBlock, MRoot, MStone, MHedge, MSign, MPlant, MPlank, MMarble, MatCount };
    // Stone's vertex colour: R a seed, B 1 for glowing runes.
    const FColor Runes(80, 0, 255, 255), Plain(140, 0, 0, 255);
    const FColor Stone(198, 192, 178), HedgeGreen(34, 64, 24), Cloud(236, 240, 248), RoofBlue(38, 62, 150), Grey(146, 150, 146);

    int32 MouthNear(int32 D, const FVector2D& P, float R)
    {
        for (const FMouth& M : Data().Mouths[D]) if (FVector2D::Distance(P, M.P) < R) return M.Th;
        return -1;
    }

    // ---------------------------------------------------------------- a threshold's ground (its floor and masses)
    // One heightfield over the gap and a little into each district, from the true distance to the way's middle. Kept (one
    // per threshold, made once on the game thread) for the mesh and for the see-through check (FeyMap::SeeThrough).
    // (2026-11-21, the user: cracks in the first canyon's walls you could see through.) Inside a district's rectangle it used
    // to leave out the floor and the masses' feet, counting on the district's own land to cover them; once that district was
    // dropped behind him (past the bend), the walls stood over holes. Now it is closed everywhere: inside a rectangle it lies
    // just under the district's land (hidden while that stands, a floor when it has gone), and the walls rise out of both.
    constexpr bool bClosedField = true;
    struct FWayField { FBox2D Box = FBox2D(ForceInit); int32 Nx = 0, Ny = 0; float Step = 60.f; TArray<float> H; TArray<uint8> Skip, IsFloor; };
    const TArray<FVector2D>& WayProfile(bool bTube)
    {
        static const TArray<FVector2D> Cut = {{Half, 0}, {Half + 140, 300}, {Half + 520, 1150}, {Half + 950, 1230}, {Half + 1450, 650}, {Half + 1950, -250}};
        static const TArray<FVector2D> Tube = {{Half, 0}, {Half + 40, 0}, {Half + 130, 590}, {Half + 600, 720}, {Half + 1150, 380}, {Half + 1650, -250}};
        return bTube ? Tube : Cut;
    }
    float WayRise(float T) { return FMath::Min(Smooth(-.24f, -.02f, T), Smooth(1.24f, 1.02f, T)); }   // (the masses sink to nothing a little past each mouth)
    // (2026-11-21) A root tunnel's banks end in a steep face at each mouth, where the tube ends too.
    float TubeRise(float T) { return FMath::Min(Smooth(-.12f, -.03f, T), Smooth(1.12f, 1.03f, T)); }
    constexpr float RidgeReach = Half + 2600.f;   // how far either side of a way its ridge runs
    // (the see-through check, before and after; read when first needed: a static read at load ran before the packaged game's
    // command line existed and stopped it starting)
    bool RidgeOn() { static const bool bOn = !FParse::Param(FCommandLine::Get(), TEXT("ChuckFeyNoRidge")); return bOn; }
    // (2026-11-21, the user: views out of the canyons beside their mouths.) A way's walls used to slope away behind their
    // crest to the gap between the districts, and near each mouth they sank into the district along with its rim (notched
    // for the road): beside the opening the eye went past them to the gap, where the ground simply ends, and to the backs of
    // the rims (one-sided, so they do not show). Now each way is cut through a ridge: past the crest the rock runs on as a
    // broad plateau out to RidgeReach, tapering only at its far side; inside a district it holds its height near the edge
    // (fading over the 16 m where the district's own rim stands) and so meets the rim; and just outside a district it is
    // never lower than the district's edge, so no rim is seen from behind. Kept clear of the Lily Mere's moat and of the
    // overlook's open brink.
    float OuterMask(int32 D, float X, float Y)
    {
        const FDistrict& Ds = Table[D];
        const float E = FMath::Min(FMath::Min(X - Ds.X0, Ds.X1 - X), FMath::Min(Y - Ds.Y0, Ds.Y1 - Y));
        float K = Smooth(1700.f, 250.f, E);
        if (D == LilyMere) K *= Smooth(MoatOut + 300.f, MoatOut + 900.f, FVector2D::Distance(FVector2D(X, Y), MoatC));
        if (D == Highmeadow) K *= 1.f - Smooth(6000.f, 7500.f, Y) * Smooth(18500.f, 17000.f, Y) * Smooth(Ds.X0 + 3500.f, Ds.X0 + 2000.f, X);
        return K;
    }
    const FWayField& WayField(int32 Th)
    {
        static FWayField Cache[Thresholds];
        FWayField& F = Cache[Th];
        if (F.Nx) return F;
        const bool bTube = Ways[Th].Style == RootTunnel;
        F.Step = 70.f;
        const TArray<FVector2D>& Prof = WayProfile(bTube);
        auto Mass2 = [&](float D) -> float { if (D <= Prof[0].X) return 0.f; for (int32 I = 0; I + 1 < Prof.Num(); ++I) if (D <= Prof[I + 1].X) return FMath::Lerp(Prof[I].Y, Prof[I + 1].Y, (D - Prof[I].X) / (Prof[I + 1].X - Prof[I].X)); return Prof.Last().Y; };
        TArray<FVector> Curve; TArray<float> CurveT;
        for (int32 I = 0; I <= 600; ++I) { const float T = FMath::Lerp(-.45f, 1.45f, I / 600.f); Curve.Add(ThresholdAtT(Th, T)); CurveT.Add(T); F.Box += FVector2D(Curve.Last()); }
        F.Box = F.Box.ExpandBy(RidgeReach + 100.f);
        auto InRect = [](float X, float Y, int32& Which) { for (int32 D = 0; D < Districts; ++D) { const FDistrict& Ds = Table[D]; if (X > Ds.X0 + 1.f && X < Ds.X1 - 1.f && Y > Ds.Y0 + 1.f && Y < Ds.Y1 - 1.f) { Which = D; return true; } } Which = -1; return false; };
        F.Nx = FMath::CeilToInt(F.Box.GetSize().X / F.Step); F.Ny = FMath::CeilToInt(F.Box.GetSize().Y / F.Step);
        F.H.SetNum((F.Nx + 1) * (F.Ny + 1)); F.Skip.SetNum(F.H.Num()); F.IsFloor.SetNum(F.H.Num());
        ParallelFor(F.Nx + 1, [&](int32 I)
        {
            for (int32 J = 0; J <= F.Ny; ++J)
            {
                const float X = F.Box.Min.X + I * F.Step, Y = F.Box.Min.Y + J * F.Step; float Best = 1e12f; int32 Bi = 0;
                for (int32 K = 0; K < Curve.Num(); K += 4) { const float Dd = FVector::DistSquared2D(Curve[K], FVector(X, Y, 0)); if (Dd < Best) { Best = Dd; Bi = K; } }
                for (int32 K = FMath::Max(0, Bi - 4); K <= FMath::Min(Curve.Num() - 1, Bi + 4); ++K) { const float Dd = FVector::DistSquared2D(Curve[K], FVector(X, Y, 0)); if (Dd < Best) { Best = Dd; Bi = K; } }
                const float D = FMath::Sqrt(Best), T = CurveT[Bi], Fz = Curve[Bi].Z - 4.f, M = Mass2(D);
                const int32 Ix = I * (F.Ny + 1) + J; int32 Which; const bool bIn = InRect(X, Y, Which);
                // The way's own walls, then the ridge they are cut through.
                const float Inner = M > 0.f ? M * (bTube ? TubeRise(T) : WayRise(T)) : 0.f;
                const float Plateau = (bTube ? 780.f + 140.f * Noise(X, Y, 1400, Th + 40) : 1320.f + 240.f * Noise(X, Y, 1600, Th + 40)) + 60.f * Noise(X, Y, 330, Th + 41);
                const float Ridge = Plateau * Smooth(bTube ? Half + 300.f : Half + 500.f, bTube ? Half + 650.f : Half + 950.f, D) * Smooth(RidgeReach, RidgeReach - 700.f, D) * (bIn ? OuterMask(Which, X, Y) : 1.f) * (RidgeOn() && Ways[Th].Style != StoneStair ? 1.f : 0.f);   // (no ridge on the stair down the brink: it would wall off the overlook's view)
                const float Up = FMath::Max(Inner, Ridge);
                F.IsFloor[Ix] = D <= Half + 20.f;
                if (!bIn)
                {
                    F.Skip[Ix] = 0; F.H[Ix] = Fz + Up;
                    // Never lower than a district's edge close by (its rim is one-sided: from out here it would not show).
                    for (int32 Dn = 0; RidgeOn() && Dn < Districts; ++Dn)
                    {
                        const FDistrict& Ds = Table[Dn];
                        const float Cx = FMath::Clamp(X, Ds.X0 + 5.f, Ds.X1 - 5.f), Cy = FMath::Clamp(Y, Ds.Y0 + 5.f, Ds.Y1 - 5.f);
                        if (FVector2D::Distance(FVector2D(X, Y), FVector2D(Cx, Cy)) < 500.f) F.H[Ix] = FMath::Max(F.H[Ix], Land(Dn, Cx, Cy) - 5.f);
                    }
                }
                else if (bClosedField) { F.Skip[Ix] = 0; F.H[Ix] = FMath::Max(Fz + FMath::Max(Up, 0.f), Land(Which, X, Y) - 30.f); if (Up < 2.f) F.H[Ix] = Land(Which, X, Y) - 30.f; }
                else { F.Skip[Ix] = F.IsFloor[Ix] || Up < 2.f; F.H[Ix] = FMath::Max(Fz + Up, Land(Which, X, Y) + 3.f); }
            }
        });
        return F;
    }
    // The field at (X, Y) (bilinear), false outside its box or (OutPresent) where the mesh leaves it out.
    bool FieldAt(const FWayField& F, float X, float Y, float& OutZ, bool& OutPresent)
    {
        const float U = (X - F.Box.Min.X) / F.Step, V = (Y - F.Box.Min.Y) / F.Step;
        if (U < 0 || V < 0 || U >= F.Nx || V >= F.Ny) return false;
        const int32 I = int32(U), J = int32(V); const float A = U - I, B = V - J;
        const int32 I00 = I * (F.Ny + 1) + J, I10 = I00 + F.Ny + 1;
        OutZ = FMath::Lerp(FMath::Lerp(F.H[I00], F.H[I10], A), FMath::Lerp(F.H[I00 + 1], F.H[I10 + 1], A), B);
        OutPresent = (F.Skip[I00] + F.Skip[I00 + 1] + F.Skip[I10] + F.Skip[I10 + 1]) <= 2;
        return true;
    }
}

// ------------------------------------------------------------------ the map's places
const FDistrict& FeyMap::District(int32 D) { return Table[FMath::Clamp(D, 0, Districts - 1)]; }
const FThreshold& FeyMap::Threshold(int32 T) { return Ways[FMath::Clamp(T, 0, Thresholds - 1)]; }

int32 FeyMap::DistrictAt(float X, float Y)
{
    if (X > -4200.f && X < 9000.f && FMath::Abs(Y) < 5600.f) return Riverbank;   // the look slice
    for (int32 D = 0; D < Districts; ++D) { const FDistrict& Ds = Table[D]; if (X >= Ds.X0 && X <= Ds.X1 && Y >= Ds.Y0 && Y <= Ds.Y1) return D; }
    return -1;
}

FVector FeyMap::ThresholdAtT(int32 Th, float T)
{
    const FThreshold& W = Ways[Th]; const FVector2D Dd = Dir(Th), Perp(-Dd.Y, Dd.X);
    const float Tc = FMath::Clamp(T, 0.f, 1.f);
    // An S that leaves and reaches each mouth straight (its sideways slope is nil at both ends), 0 at the middle.
    const FVector2D P = FMath::Lerp(W.PA, W.PB, T) + Perp * W.Swing * 1.52f * FMath::Sin(2 * PI * Tc) * FMath::Square(FMath::Sin(PI * Tc));
    return FVector(P.X, P.Y, FMath::Lerp(W.ZA, W.ZB, FMath::SmoothStep(0.f, 1.f, Tc)));
}

int32 FeyMap::ThresholdAt(const FVector& L, float& OutT)
{
    for (int32 Th = 0; Th < Thresholds; ++Th)
    {
        const FThreshold& W = Ways[Th];
        const FVector2D Lo(FMath::Min(W.PA.X, W.PB.X) - 1600.f, FMath::Min(W.PA.Y, W.PB.Y) - 1600.f), Hi(FMath::Max(W.PA.X, W.PB.X) + 1600.f, FMath::Max(W.PA.Y, W.PB.Y) + 1600.f);
        if (L.X < Lo.X || L.X > Hi.X || L.Y < Lo.Y || L.Y > Hi.Y) continue;
        float Best = 1e9f, BestT = 0;
        for (int32 I = 0; I <= 80; ++I) { const float T = I / 80.f; const float D = FVector::DistSquared2D(ThresholdAtT(Th, T), L); if (D < Best) { Best = D; BestT = T; } }
        const FVector At = ThresholdAtT(Th, BestT);
        if (FMath::Sqrt(Best) < Half + 260.f && L.Z > At.Z - 400.f && L.Z < At.Z + 1200.f) { OutT = BestT; return Th; }
    }
    return -1;
}

bool FeyMap::GroundAt(float X, float Y, float& OutZ)
{
    const int32 D = DistrictAt(X, Y);
    if (D >= 0) { if (D == Riverbank && X > -4200.f) return false; OutZ = Land(D, X, Y); return true; }
    float T; const int32 Th = ThresholdAt(FVector(X, Y, 0), T);
    if (Th >= 0) { OutZ = ThresholdAtT(Th, T).Z; return true; }
    return false;
}

FVector FeyMap::Checkpoint(int32 D)
{
    if (D == Riverbank) return Fey::ShoreSpot();
    if (D == Maze) { const FVector2D P = MazeAt(MazeRmax + 650.f, .26f); return FVector(P.X, P.Y, Land(D, P.X, P.Y)); }
    const int32 Entry[] = {0, 0, 1, 3, 5, 8, 9};
    const TArray<FVector2D>& Road = Data().RoadOf[D][Entry[D]];
    const FVector2D P = Road.Last() + (Road[Road.Num() - 2] - Road.Last()).GetSafeNormal() * 250.f;
    return FVector(P.X, P.Y, Land(D, P.X, P.Y));
}

float FeyMap::CheckpointYaw(int32 D)
{
    if (D == Riverbank) return Fey::ShoreYaw();
    if (D == Maze) return 180.f;
    const FVector C = Checkpoint(D);
    return FMath::RadiansToDegrees(FMath::Atan2(Table[D].Hub.Y - C.Y, Table[D].Hub.X - C.X));
}

TArray<FVector> FeyMap::Walk(int32 D, int32 Ta, int32 Tb)
{
    const FData& S = Data(); const FDistrict& Ds = Table[D];
    TArray<FVector> Out;
    if (D == Riverbank)   // (one path: from the look slice's west end to the threshold, or back)
    {
        TArray<FVector2D> L = S.RoadOf[D][0]; if (Tb == 0) Algo::Reverse(L);
        for (const FVector2D& Q : L) Out.Add(FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y)));
        return Out;
    }
    auto End = [&](int32 T) -> TArray<FVector2D>
    {
        if (T >= 0) return S.RoadOf[D][T];
        if (T == -2) { TArray<FVector2D> L = S.SpurOf[D]; Algo::Reverse(L); return L; }
        if (D == Maze) { TArray<FVector2D> L = MazeData().Route; Algo::Reverse(L); return L; }
        const FVector C = Checkpoint(D); TArray<FVector2D> L; L.Add(FVector2D(C)); L.Add(RingPoint(D, FVector2D(C))); return L;
    };
    TArray<FVector2D> A = End(Ta), B = End(Tb), P = A;
    if (Ds.HubR > 0)
    {
        float A0 = FMath::Atan2(A.Last().Y - Ds.Hub.Y, A.Last().X - Ds.Hub.X), A1 = FMath::Atan2(B.Last().Y - Ds.Hub.Y, B.Last().X - Ds.Hub.X);
        float Delta = FMath::FindDeltaAngleRadians(A0, A1); const int32 N = FMath::CeilToInt(FMath::Abs(Delta) / FMath::DegreesToRadians(10.f));
        for (int32 I = 1; I < N; ++I) { const float Ang = A0 + Delta * I / N; P.Add(Ds.Hub + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Ds.HubR); }
    }
    for (int32 I = B.Num() - 1; I >= 0; --I) P.Add(B[I]);
    if (D == LilyMere)   // over the deep water: from pad to pad, nearest first
    {
        const int32 First = P.IndexOfByPredicate([](const FVector2D& Q) { return MoatK(Q.X, Q.Y) > .2f; });
        if (First > 0)
        {
            P.RemoveAll([](const FVector2D& Q) { return MoatK(Q.X, Q.Y) > .2f; });
            TArray<FVector> Pads = MoatPads(); const FVector2D From = P[First - 1];
            Pads.Sort([&From](const FVector& A, const FVector& Bb) { return FVector2D::DistSquared(FVector2D(A), From) < FVector2D::DistSquared(FVector2D(Bb), From); });
            for (int32 I = 0; I < Pads.Num(); ++I) P.Insert(FVector2D(Pads[I]), First + I);
        }
    }
    for (const FVector2D& Q : P) Out.Add(FVector(Q.X, Q.Y, IsPad(Q.X, Q.Y) ? 2.f : Land(D, Q.X, Q.Y)));
    return Out;
}

// ------------------------------------------------------------------ spots within the districts (2026-11-21)
namespace
{
    const FSpot ExtraSpots[] = {
        {Riverbank, TEXT("FeyRiverbankWest"), TEXT("Feywild Riverbank West")},     // at the first canyon's mouth
        {Orchard, TEXT("FeyNeedleGarden"), TEXT("Feywild Needle Garden")},
        {LilyMere, TEXT("FeyMereFen"), TEXT("Feywild Lily Mere Fen")},
        {LilyMere, TEXT("FeyMerePads"), TEXT("Feywild Lily Pads")},                // the pads' landward end, before the deep water
        {Deepwood, TEXT("FeyRedcap"), TEXT("Feywild Redcap Hollow")},
        {Deepwood, TEXT("FeyDeepwoodNorth"), TEXT("Feywild Deepwood North")},      // on the way up to Highmeadow
        {Highmeadow, TEXT("FeyFord"), TEXT("Feywild Highmeadow Ford")},
        {Highmeadow, TEXT("FeyOverlook"), TEXT("Feywild Maze Overlook")},
        {Maze, TEXT("FeyMazeMiddle"), TEXT("Feywild Maze Middle")},                // halfway along the way through
        {Maze, TEXT("FeyMazeHeart"), TEXT("Feywild Maze Heart")},                  // the Cloud Staircase's foot
        {Maze, TEXT("FeyCloudLanding"), TEXT("Feywild Cloud Landing")},            // (2026-11-22) the stair's landing, 19 m up
    };
    constexpr int32 SpotTotal = Districts + UE_ARRAY_COUNT(ExtraSpots);
    bool Reached[SpotTotal] = {};
    float MazeHeartA() { const FVector2D P = MazeData().Route.Last() - Table[Maze].Hub; return FMath::Atan2(P.Y, P.X); }
    // A place (2D) and the place it faces, for each extra spot.
    void SpotPlace(int32 E, FVector2D& At, FVector2D& Look)
    {
        const FData& S = Data();
        switch (E)
        {
        case 0: { const TArray<FVector2D>& R = S.RoadOf[Riverbank][0]; At = FMath::Lerp(R[1], R[2], .25f); Look = R[0]; break; }
        case 1: { const TArray<FVector2D>& R = S.RoadOf[Orchard][1]; At = R[2]; Look = FVector2D(-47000.f, -9300.f); break; }
        case 2: { const TArray<FVector2D>& R = S.SpurOf[LilyMere]; At = FMath::Lerp(R[R.Num() - 2], R.Last(), .8f); Look = FVector2D(-66000.f, 39300.f); break; }
        case 3:
        {
            // Along the road in from the ring, the last ground before the moat's water (4 m short of it).
            const TArray<FVector2D>& R = S.RoadOf[LilyMere][4]; At = R.Last(); Look = MoatC;
            for (int32 I = R.Num() - 1; I > 0; --I)
                for (int32 K = 0; K <= 40; ++K)
                {
                    const FVector2D P = FMath::Lerp(R[I], R[I - 1], K / 40.f);
                    if (FVector2D::Distance(P, MoatC) < MoatOut + 600.f) { I = 0; break; }
                    At = P;
                }
            break;
        }
        case 4: { const TArray<FVector2D>& R = S.SpurOf[Deepwood]; At = Redcap + (R[R.Num() - 2] - Redcap).GetSafeNormal() * 800.f; Look = Redcap; break; }
        case 5: { const TArray<FVector2D>& R = S.RoadOf[Deepwood][7]; At = R[2]; Look = R[0]; break; }
        case 6: { const TArray<FVector2D>& R = S.RoadOf[Highmeadow][8]; At = FMath::Lerp(R[2], R[3], .6f); Look = R[3]; break; }
        case 7: { const TArray<FVector2D>& R = S.SpurOf[Highmeadow]; At = FMath::Lerp(R[R.Num() - 2], R.Last(), .82f); Look = Table[Maze].Hub; break; }
        case 8: { const TArray<FVector2D>& R = MazeData().Route; const int32 M = R.Num() / 2; At = R[M]; Look = R[M + 1]; break; }
        case 9: { const float A = MazeHeartA(); At = MazeAt(520.f, A); Look = MazeAt(1100.f, FMath::DegreesToRadians(200.f)); break; }
        default: { const FStairStep& L = Stair()[StairLanding]; At = MazeAt(L.R, L.A); Look = MazeAt(L.R + 900.f, L.A + .2f); break; }
        }
    }
}
int32 FeyMap::SpotCount() { return SpotTotal; }
const FSpot& FeyMap::Spot(int32 I)
{
    static FSpot Own[Districts];
    if (I < Districts) { const int32 D = FMath::Clamp(I, 0, Districts - 1); Own[D] = {D, Table[D].Point, Table[D].Label}; return Own[D]; }
    return ExtraSpots[FMath::Clamp(I - Districts, 0, int32(UE_ARRAY_COUNT(ExtraSpots)) - 1)];
}
FVector FeyMap::SpotAt(int32 I)
{
    if (I < Districts) return Checkpoint(I);
    FVector2D At, Look; SpotPlace(I - Districts, At, Look);
    if (I - Districts == 10) return FVector(At.X, At.Y, Stair()[StairLanding].Z);   // (the landing, up on the cloud)
    return FVector(At.X, At.Y, Land(ExtraSpots[I - Districts].D, At.X, At.Y));
}
float FeyMap::SpotYaw(int32 I)
{
    if (I < Districts) return CheckpointYaw(I);
    FVector2D At, Look; SpotPlace(I - Districts, At, Look);
    return FMath::RadiansToDegrees(FMath::Atan2(Look.Y - At.Y, Look.X - At.X));
}
const TArray<FVector2D>& FeyMap::RoadOf(int32 D, int32 T) { return Data().RoadOf[FMath::Clamp(D, 0, Districts - 1)][FMath::Clamp(T, 0, Thresholds - 1)]; }
const TArray<FVector2D>& FeyMap::SpurOf(int32 D) { return Data().SpurOf[FMath::Clamp(D, 0, Districts - 1)]; }
float FeyMap::LandZ(int32 D, float X, float Y) { return Land(D, X, Y); }
int32 FeyMap::SpotOf(const FString& Point) { for (int32 I = 0; I < SpotTotal; ++I) if (Point == Spot(I).Point) return I; return -1; }
FVector FeyHagCottage() { return HagSpot(); }
FVector FeyHagPoint(const FVector& Own)
{
    const FVector& Hs = HagSpot(); const float Ya = FMath::DegreesToRadians(Hs.Z);
    const FVector2D Fx(FMath::Cos(Ya), FMath::Sin(Ya)), Fy(-Fx.Y, Fx.X), P = FVector2D(Hs) + Fx * Own.X + Fy * Own.Y;
    return FVector(P.X, P.Y, DeepRaw(Hs.X, Hs.Y) + Own.Z);
}

FVector FeySpotWorld(int32 I) { return I < Districts ? FeyCheckpointWorld(I) : FeyWorld(SpotAt(I) + FVector(0, 0, Stand + 2.f)); }
void FeyReachSpots(const FVector& World)
{
    for (int32 I = 0; I < SpotTotal; ++I) if (!Reached[I] && FVector::Dist2D(World, FeySpotWorld(I)) < 400.f) { Reached[I] = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_SPOT_REACHED point=%s"), Spot(I).Point); }
}
void FeyForgetSpots() { for (bool& R : Reached) R = false; }

bool FeyMap::IsPad(float X, float Y) { for (const FVector& P : MoatPads()) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(P)) < FMath::Square(P.Z)) return true; return false; }
bool FeyMap::InDeepWater(float X, float Y) { return MoatK(X, Y) > .5f; }
int32 FeyMap::MazeCells() { return MazeData().Cells; }
const TArray<FeyMap::FMazeShortcut>& FeyMap::MazeShortcuts() { return MazeData().Shortcuts; }
const TArray<FVector2D>& FeyMap::MazeDeadEnds() { return MazeData().DeadEnds; }
const TArray<FVector>& FeyMap::MazeLanes() { return MazeData().Lanes; }
const TArray<FeyMap::FMazeParkour>& FeyMap::MazeParkour() { return MazeData().Parkour; }
const TArray<FeyMap::FMazeGap>& FeyMap::MazeGaps() { return MazeData().Gaps; }
const TArray<FeyMap::FMazeMark>& FeyMap::MazeMarks() { return MazeData().Marks; }
const TArray<FeyMap::FMazeMark>& FeyMap::MazeSqueezes() { return MazeData().Squeezes; }
const TArray<FVector4>& FeyMap::MazeFrowns() { return MazeData().Frowns; }
const TArray<FVector4>& FeyMap::MazeDecor() { return MazeData().Decor; }
FVector2D FeyMap::MazeParkourAt(const FMazeParkour& P, float S, float Across) { return MazeAt(P.R + Across, P.Ac + P.Dir * S / P.R); }
namespace
{
    // (2026-11-23) The maze's parkour for his moves, once a world (world places): the hoops' grips, the walkways' ladders, the
    // root gaps' crawl spaces.
    void RegisterMazeParkour(UWorld* World)
    {
        static TWeakObjectPtr<UWorld> Done; if (Done.Get() == World) return; Done = World;
        int32 Grips = 0, Ladders = 0;
        for (const FeyMap::FMazeParkour& P : FeyMap::MazeParkour())
        {
            auto At = [&](float S, float X, float Z) { const FVector2D Q = FeyMap::MazeParkourAt(P, S, X); return FeyWorld(FVector(Q.X, Q.Y, Land(Maze, Q.X, Q.Y) + Z)); };
            auto Along = [&](float S) { return (At(S + 5.f, 0.f, 0.f) - At(S - 5.f, 0.f, 0.f)).GetSafeNormal2D(); };
            if (P.Kind == 1)
                for (int32 K = 0; K < P.NHoop; ++K)
                { const float S = (K - (P.NHoop - 1) * .5f) * FeyMap::HoopStep; FChuckSwingGrip G; G.Grip = At(S, 0.f, FeyMap::GripUp); G.Along = Along(S); AddChuckSwingGrip(G); ++Grips; }
            if (P.Kind == 2)
            {
                const float Sl = FeyMap::WalkFrom - 8.f;
                FChuckClimbable L; L.Foot = At(FeyMap::LadderFoot, FeyMap::WalkAcross, 0.f); L.Lip = At(FeyMap::WalkFrom, FeyMap::WalkAcross, FeyMap::WalkUp); L.TopZ = float(L.Lip.Z);
                L.Out = -Along(Sl); L.HalfWidth = 22.f; AddChuckClimbable(L); ++Ladders;
            }
        }
        for (const FeyMap::FMazeGap& G : FeyMap::MazeGaps())
            AChuckCharacter::RegisterCrawlSpace(World, FTransform(FRotator(0, G.Yaw + 90.f, 0), FeyWorld(FVector(G.C.X, G.C.Y, Land(Maze, G.C.X, G.C.Y) + 25.f))), FVector(HedgeHalf + 70.f, FeyMap::GapHalf - 8.f, 25.f));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_PARKOUR parkour=%d grips=%d ladders=%d gaps=%d"), FeyMap::MazeParkour().Num(), Grips, Ladders, FeyMap::MazeGaps().Num());
    }
}
bool FeyMap::InMazeThorns(const FVector& Feet)
{
    const FVector2D Q(Feet.X - Table[Maze].Hub.X, Feet.Y - Table[Maze].Hub.Y); const float Rq = Q.Size();
    if (Rq > MazeRmax + 200.f || Rq < MazeR0) return false;
    for (const FMazeParkour& P : MazeData().Parkour)
    {
        if (FMath::Abs(Rq - P.R) > MazePitch * .5f - HedgeHalf + 5.f) continue;
        const float S = FMath::FindDeltaAngleRadians(P.Ac, FMath::Atan2(Q.Y, Q.X)) * P.R * P.Dir;
        if (S >= P.Bed0 && S <= P.Bed1) return Feet.Z < Land(Maze, Feet.X, Feet.Y) + 35.f;
    }
    return false;
}
const TArray<FVector2D>& FeyMap::MazeRoute() { return MazeData().Route; }
void FeyMap::MazeRings(FVector2D& Hub, float& R0, float& Pitch, float& Rmax, float& Tall) { Hub = Table[Maze].Hub; R0 = MazeR0; Pitch = MazePitch; Rmax = MazeRmax; Tall = HedgeTall; }
float FeyMap::MazeHeartAngle() { return MazeData().HeartA; }
float FeyMap::MazeWay() { return MazeRouteLength(); }

bool InFeyMap(const FVector& World)
{
    const FVector L = FeyLocal(World);
    return L.X > -152000.f && L.X < -4000.f && L.Y > -48000.f && L.Y < 44000.f && L.Z > -2000.f && L.Z < 12000.f;
}

FVector FeyCheckpointWorld(int32 D) { return D == Riverbank ? FeywildArrival() : FeyWorld(Checkpoint(D) + FVector(0, 0, Stand + 2.f)); }

FVector FeyRespawn(const FVector& World)
{
    const FVector L = FeyLocal(World);
    int32 D = DistrictAt(L.X, L.Y);
    if (D < 0) { float T; const int32 Th = ThresholdAt(L, T); if (Th >= 0) D = T < .5f ? Ways[Th].A : Ways[Th].B; }
    if (D < 0) D = Riverbank;
    // (2026-11-21) The nearest of the district's checkpoint and the spots in it he has reached.
    int32 Best = D; float BestD = FVector::Dist2D(World, FeyCheckpointWorld(D));
    for (int32 I = Districts; I < SpotTotal; ++I)
        if (Reached[I] && ExtraSpots[I - Districts].D == D) { const float Dd = FVector::Dist2D(World, FeySpotWorld(I)); if (Dd < BestD) { BestD = Dd; Best = I; } }
    return FeySpotWorld(Best);
}

bool IsFeyCheckpoint(const FVector& World, float& OutYaw)
{
    for (int32 I = 0; I < SpotTotal; ++I) if (FVector::Dist(World, FeySpotWorld(I)) < 1.f) { OutYaw = SpotYaw(I); return true; }
    return false;
}

int32 FeyMusicZone(const FVector& World)
{
    const FVector L = FeyLocal(World);
    const int32 D = DistrictAt(L.X, L.Y);
    if (D >= 0) return Table[D].Music;
    float T; if (ThresholdAt(L, T) >= 0) return -1;
    return 0;
}

// ------------------------------------------------------------------ a district's plan, made on a worker
struct AFeyDistrict::FPlan
{
    TArray<FMapSurf> Surfs;
    TArray<FTransform> Inst[KitCount];
    struct FWall { FVector C, H; float Yaw; };
    TArray<FWall> Walls;
    TArray<FVector4> Lights, Veils;   // (X, Y, Z, candelas) and (X, Y, Z, size)
    TArray<FLinearColor> LightCols;   // each light's colour, its radius in A
    std::atomic<bool> bDone{false};
    float Seconds = 0;
};

namespace
{
    void PlanDistrict(int32 D, AFeyDistrict::FPlan& Plan)
    {
        const double T0 = FPlatformTime::Seconds();
        const FDistrict& Ds = Table[D];
        // ------------------------------ the land, in tiles
        constexpr float Tile = 3200.f; constexpr int32 CellsBase = 26;
        const int32 Tx = FMath::CeilToInt((Ds.X1 - Ds.X0) / Tile), Ty = FMath::CeilToInt((Ds.Y1 - Ds.Y0) / Tile);
        const float Gloom = D == Deepwood ? .62f : 1.f;
        for (int32 I = 0; I < Tx; ++I)
            for (int32 J = 0; J < Ty; ++J)
            {
                FMapSurf& L = Plan.Surfs.AddDefaulted_GetRef(); L.Mat = MGround;
                const float X0 = Ds.X0 + I * Tile, Y0 = Ds.Y0 + J * Tile, X1 = FMath::Min(X0 + Tile, Ds.X1), Y1 = FMath::Min(Y0 + Tile, Ds.Y1);
                // (2026-11-23) The narrow cut behind the arrival runs on into the riverbank: its 6 m walls need cells as fine as the
                // slice's (41 cm, not 123), or they smear and the slice's grid does not meet them.
                const int32 Cells = D == Riverbank && X1 > -8100.f ? CellsBase * 3 : CellsBase;
                for (int32 A = 0; A <= Cells; ++A)
                    for (int32 B = 0; B <= Cells; ++B)
                    {
                        const float X = FMath::Lerp(X0, X1, float(A) / Cells), Y = FMath::Lerp(Y0, Y1, float(B) / Cells), E = 40.f;
                        float Pk, Wet; const float H = Land(D, X, Y, &Pk, &Wet);
                        const FVector Nn = FVector(Land(D, X - E, Y) - Land(D, X + E, Y), Land(D, X, Y - E) - Land(D, X, Y + E), 2 * E).GetSafeNormal();
                        const float Moss = FMath::Clamp(.75f - Pk * 1.3f + .5f * Noise(X, Y, 500, 21) + (D == Deepwood ? .25f : 0.f), 0.f, 1.f) * FMath::Lerp(.35f, 1.f, Nn.Z * Nn.Z);
                        L.Vert(FVector(X, Y, H), Nn, FVector2D(X, Y) / 300.f, FLinearColor(Pk * Smooth(.55f, .85f, Nn.Z), Moss, FMath::Max(Wet, Pk * .4f) * Smooth(.55f, .85f, Nn.Z), Gloom).ToFColor(false));   // (2026-11-23: no wet sheen or path on steep faces)
                    }
                for (int32 A = 0; A < Cells; ++A) for (int32 B = 0; B < Cells; ++B) { const int32 Q = A * (Cells + 1) + B, R = Q + Cells + 1; L.Quad(Q, Q + 1, R + 1, R); }
            }
        // ------------------------------ water: the mere's pools, the meadow's rapids
        if (D == LilyMere)
        {
            FMapSurf& W = Plan.Surfs.AddDefaulted_GetRef(); W.Mat = MWater; W.bSolid = false; W.bShadow = false;
            for (const FVector& Pl : Pools)
            {
                const float R = Pl.Z * 1.16f, Step = 80.f; const int32 N = FMath::CeilToInt(2 * R / Step), Base = W.V.Num();
                for (int32 A = 0; A <= N; ++A) for (int32 B = 0; B <= N; ++B)
                {
                    const float X = Pl.X - R + A * Step, Y = Pl.Y - R + B * Step; int32 Which; const float E = PoolE(X, Y, Which);
                    W.Vert(FVector(X, Y, 0.f), FVector::UpVector, FVector2D(X, Y) / 200.f, FLinearColor(Smooth(.68f, 1.f, E), 0, 0, 1).ToFColor(false));
                }
                for (int32 A = 0; A < N; ++A) for (int32 B = 0; B < N; ++B) { const int32 Q = Base + A * (N + 1) + B, Rr = Q + N + 1; W.Quad(Q, Q + 1, Rr + 1, Rr); }
            }
            // The deep moat: a half ring of water round the second threshold's mouth, dark (shore colour) where it is deep.
            const int32 Base = W.V.Num(); constexpr int32 Around = 64, Across = 8;
            for (int32 A = 0; A <= Around; ++A)
                for (int32 B = 0; B <= Across; ++B)
                {
                    const float Ang = PI * A / Around, Rr = FMath::Lerp(MoatIn - 300.f, MoatOut + 300.f, float(B) / Across);
                    const FVector2D P = MoatC + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rr;
                    W.Vert(FVector(P.X, P.Y, 0.f), FVector::UpVector, P / 200.f, FLinearColor(1.f - MoatK(P.X, P.Y) * .5f, 0, 0, 1).ToFColor(false));
                }
            for (int32 A = 0; A < Around; ++A) for (int32 B = 0; B < Across; ++B) { const int32 Q = Base + A * (Across + 1) + B, Rr = Q + Across + 1; W.Quad(Q, Rr, Rr + 1, Q + 1); }   // (angle runs the other way to X: wound the other way)
        }
        if (D == Highmeadow)
        {
            FMapSurf& W = Plan.Surfs.AddDefaulted_GetRef(); W.Mat = MRiver; W.bSolid = false; W.bShadow = false;
            constexpr int32 Across = 8; constexpr float Hw = 520.f;
            const int32 Rows = FMath::CeilToInt((Ds.Y1 - Ds.Y0) / 100.f);
            for (int32 I = 0; I <= Rows; ++I)
                for (int32 J = 0; J <= Across; ++J)
                {
                    const float Y = Ds.Y1 - I * 100.f, Dd = FMath::Lerp(-Hw, Hw, float(J) / Across), Churn = .25f + .6f * Smooth(1200.f, 0.f, FMath::Abs(Y - 14000.f)) + .2f * FMath::Abs(Noise(Y, 0, 800, 90));
                    W.Vert(FVector(RapidX(Y) + Dd, Y, RapidWL(Y)), FVector::UpVector, FVector2D((Ds.Y1 - Y) / 200.f, Dd / 200.f), FLinearColor(FMath::Abs(Dd) / (Hw - 160.f), FMath::Clamp(Churn, 0.f, 1.f), 1, 1).ToFColor(false));
                }
            for (int32 I = 0; I < Rows; ++I) for (int32 J = 0; J < Across; ++J) { const int32 A = I * (Across + 1) + J, B = A + Across + 1; W.Quad(A, B, B + 1, A + 1); }
        }
        // ------------------------------ dressing: stone (arches, statues), the maze's hedges, signs, the cloud stair's foot
        {
            Plan.Surfs.Reserve(Plan.Surfs.Num() + 160);   // (references into it are held below: the maze alone adds 49, and 2026-11-23 three, and two a thorn bed)
            FMapSurf& St = Plan.Surfs.AddDefaulted_GetRef(); St.Mat = MStone;
            FMapSurf& Sg = Plan.Surfs.AddDefaulted_GetRef(); Sg.Mat = MSign;
            FMapSurf& Cl = Plan.Surfs.AddDefaulted_GetRef(); Cl.Mat = MBlock; Cl.bHidden = true; Cl.bShadow = false;
            auto Arch = [&](const FVector2D& C, float Yaw, float Z, const FColor& Col)
            {
                const float A = FMath::DegreesToRadians(Yaw); const FVector2D Across(-FMath::Sin(A), FMath::Cos(A));
                for (const float S : {-1.f, 1.f}) { const FVector2D P = C + Across * S * 300.f; St.Box(FVector(P.X, P.Y, Z + 250.f), FVector(60.f, 60.f, 300.f), Yaw, Col); }
                St.Box(FVector(C.X, C.Y, Z + 590.f), FVector(60.f, 370.f, 50.f), Yaw, Col);
            };
            if (D == Crossroads)   // rune arches over each road's run in, their runes glowing
                for (const FMouth& M : Data().Mouths[D]) { const FVector2D P = M.P + M.In * 1000.f; Arch(P, FMath::RadiansToDegrees(FMath::Atan2(M.In.Y, M.In.X)), Land(D, P.X, P.Y), Runes); }
            if (D == Maze)
            {
                // The hedges in 48 parts, eight sectors by six bands of about equal area (each its own mesh and collision, put in
                // place one a frame; 2026-11-23: eight parts of some 43,000 triangles took about 45 ms each to cook, the worst stutter).
                const FMazeData& Mz = MazeData(); TArray<FMapSurf*> Parts;
                for (int32 K = 0; K < 48; ++K) { FMapSurf& H = Plan.Surfs.AddDefaulted_GetRef(); H.Mat = MHedge; Parts.Add(&H); }
                for (const FHedge& Hd : Mz.Hedges)
                {
                    const FVector C = Hd.C + FVector(0, 0, Land(D, Hd.C.X, Hd.C.Y) - 6.f);
                    if (Hd.Kind) { St.Box(C, Hd.H, Hd.Yaw, Runes); continue; }
                    if (Hd.Sc >= 0) continue;   // (2026-11-22: a shortcut's hedge, FeyLife's to build and sink)
                    const float A = FMath::Atan2(Hd.C.Y - Table[Maze].Hub.Y, Hd.C.X - Table[Maze].Hub.X) + PI;
                    const float Rf = FVector2D::Distance(FVector2D(Hd.C), Table[Maze].Hub) / 13400.f;
                    Parts[FMath::Clamp(int32(A / (2 * PI) * 8.f), 0, 7) * 6 + FMath::Clamp(int32(Rf * Rf * 6.f), 0, 5)]->Hedge(C, Hd.H, Hd.Yaw, float(&Hd - Mz.Hedges.GetData()) * .618f);
                    // Its leaves (2026-11-21): sections of the sprig shell along it, each stretched a little to fit.
                    const int32 N = FMath::Max(1, FMath::RoundToInt(2.f * Hd.H.X / 300.f)); const float Len = 2.f * Hd.H.X / N, Yr = FMath::DegreesToRadians(Hd.Yaw);
                    const FVector Along(FMath::Cos(Yr), FMath::Sin(Yr), 0.f);
                    for (int32 K = 0; K < N; ++K)
                        Plan.Inst[KHedge].Add(FTransform(FRotator(0, Hd.Yaw + (K % 2 ? 180.f : 0.f), 0), C + Along * ((K + .5f) * Len - Hd.H.X), FVector(Len / 300.f, Hd.H.Y / 60.f, Hd.H.Z / 420.f)));
                }
                // (2026-11-23, the user: two kinds of statue, frowning in the dead ends and smiling along the way, real satyrs and
                // not cartoon heads) Marble satyrs on plinths: SMK's public-domain scan of the Medici Faun, laughing as carved, along
                // the way; the same faun re-carved to a scowl (Tools/build_fey_satyr.py) in the dead ends.
                FMapSurf& Mb = Plan.Surfs.AddDefaulted_GetRef(); Mb.Mat = MMarble; Mb.bSolid = false;   // (each plinth and satyr a box of collision: cooking the lot as one mesh stalled a frame)
                // (2026-11-23, the user: some of them detailed marble satyrs) The statues on the way are marble satyrs
                // dancing on a plinth instead, and two flank the heart's arch. (The scan faces +Y in the engine.)
                auto Satyr = [&](const FVector2D& P, const FVector2D& Dir, bool bSmile)
                {
                    const FVector C(P.X, P.Y, Land(D, P.X, P.Y));
                    const FVector2D Plinth[] = {{.5f, 46.f}, {44.f, 46.f}, {48.f, 40.f}, {48.f, 6.f}, {52.f, 0.f}, {52.f, -10.f}};
                    Mb.Lathe(C, Plinth, 14, FColor::White);
                    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.X, Dir.Y));   // (it faces +Y once imported: the importer mirrors Blender's Y)
                    Plan.Inst[bSmile ? KSatyr : KSatyrFrown].Add(FTransform(FRotator(0, Yaw, 0), C + FVector(0, 0, 46.f), FVector(1.f)));
                    Plan.Walls.Add({C + FVector(0, 0, 23.f), FVector(48.f, 48.f, 33.f), 0.f});
                    Plan.Walls.Add({C + FVector(0, 0, 46.f + 72.f), FVector(40.f, 28.f, 72.f), Yaw});
                };
                // (2026-11-23) Each squeeze's two faces: flat collision a little proud of the leaves, the hedges' full height, so
                // his wall runs and kicks catch the same way all the way up.
                for (const FeyMap::FMazeMark& Sq : Mz.Squeezes)
                {
                    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Sq.In.Y, Sq.In.X)) + 90.f, Z = Land(D, Sq.P.X, Sq.P.Y);
                    for (const float Sx : {-1.f, 1.f}) { const FVector2D Q = Sq.P + Sq.In * (Sx * (FeyMap::SqueezeGap * .5f - 6.f)); Plan.Walls.Add({FVector(Q.X, Q.Y, Z + HedgeTall * .5f), FVector(FeyMap::SqueezeLen * .5f, 4.f, HedgeTall * .5f), Yaw}); }
                }
                for (const FeyMap::FMazeMark& Mk : Mz.Marks) if (Mk.Kind == 0) Satyr(Mk.P + Mk.In * 10.f, Mk.In, true);
                for (const FVector4& Fr : Mz.Frowns) Satyr(FVector2D(Fr.X, Fr.Y), FVector2D(Fr.Z, Fr.W), false);
                {
                    const float A = Mz.HeartA; const FVector2D Out(FMath::Cos(A), FMath::Sin(A)), Across(-Out.Y, Out.X);
                    for (const float Sx : {-1.f, 1.f}) Satyr(MazeAt(MazeR0 - 260.f, A) + Across * (Sx * 330.f), Out, true);
                }
                // (2026-11-23, the user: blue lamps from the Deepwood inside the maze, not as bright; very low fog in parts; more
                // decor) Spread evenly over the whole maze (MazeData's Decor), so none of it marks the way.
                {
                    // (none of it solid: it stands at the hedges' feet, and cooking collision for it all stalled the maze's build by
                    // over 100 ms)
                    FMapSurf& Posts = Plan.Surfs.AddDefaulted_GetRef(); Posts.Mat = MRoot; Posts.bSolid = false;
                    FMapSurf& Lamps = Plan.Surfs.AddDefaulted_GetRef(); Lamps.Mat = MPlant; Lamps.bSolid = false; Lamps.bShadow = false;
                    FMapSurf& Deco = Plan.Surfs.AddDefaulted_GetRef(); Deco.Mat = MStone; Deco.bSolid = false;   // urns, benches
                    const FColor Wood = FLinearColor(.4f, .75f, 0, 1).ToFColor(false), Iron(30, 30, 36, 0), Glass(70, 150, 255, 150);
                    const int32 Plants[] = {KFern0, KFern0 + 1, KFern0 + 2, KFern0 + 3, KBlue, KViolet, KFox, KCel0, KCel1, KBroad, KCrimson, KBlue};
                    const float PlantS[][2] = {{.8f, 1.4f}, {.8f, 1.4f}, {.8f, 1.4f}, {.8f, 1.4f}, {.6f, 1.6f}, {.6f, 1.5f}, {.8f, 1.3f}, {1.4f, 2.4f}, {1.4f, 2.4f}, {.7f, 1.3f}, {.7f, 1.3f}, {.5f, 1.1f}};
                    FRandomStream Rd(6262); int32 Lanterns = 0, Lit = 0;
                    for (const FVector4& Dc : Mz.Decor)
                    {
                        const FVector2D P(Dc.X, Dc.Y), In(FMath::Cos(Dc.Z), FMath::Sin(Dc.Z)), Tn(-In.Y, In.X); const float Z = Land(D, P.X, P.Y); const int32 Kind = int32(Dc.W);
                        const FVector Fi(In.X, In.Y, 0.f), Ft(Tn.X, Tn.Y, 0.f);
                        if (Kind == 0)   // a crooked post at the hedge's foot, its arm out over the corridor, a lantern of blue glass
                        {
                            const FVector B(P.X, P.Y, Z - 20.f); const float Lean = Rd.FRandRange(-1.f, 1.f), Hgt = Rd.FRandRange(215.f, 245.f);
                            const FVector Pts[] = {B, B + FVector(0, 0, Hgt * .38f) + Ft * 8.f * Lean - Fi * 3.f, B + FVector(0, 0, Hgt * .75f) - Ft * 6.f * Lean - Fi * 4.f,
                                                   B + FVector(0, 0, Hgt) + Fi * 6.f, B + FVector(0, 0, Hgt + 20.f) + Fi * 34.f, B + FVector(0, 0, Hgt + 12.f) + Fi * 62.f, B + FVector(0, 0, Hgt - 4.f) + Fi * 72.f};
                            Posts.Pipe(Pts, 7.5f, 3.f, 8, 5, Wood, float(Lanterns) * 2.7f);
                            const FVector Hook = B + FVector(0, 0, Hgt + 8.f) + Fi * 64.f, Ce = Hook - FVector(0, 0, 13.f);
                            const FVector Chain[] = {Hook + FVector(0, 0, 4.f), Ce};
                            Lamps.Pipe(Chain, 1.3f, 1.3f, 4, 1, Iron, 0.f);
                            const FVector2D Cap[] = {{.5f, 0.f}, {3.f, -1.f}, {9.f, -6.f}, {13.5f, -10.f}, {14.f, -12.f}, {10.5f, -13.f}};
                            const FVector2D Pane[] = {{10.5f, -13.f}, {11.5f, -16.f}, {11.5f, -36.f}, {9.5f, -40.f}};
                            const FVector2D Foot[] = {{9.5f, -40.f}, {12.f, -41.5f}, {11.f, -44.f}, {5.f, -48.f}, {.5f, -56.f}};
                            Lamps.Lathe(Ce, Cap, 6, Iron); Lamps.Lathe(Ce, Pane, 6, Glass); Lamps.Lathe(Ce, Foot, 6, Iron);
                            for (int32 K = 0; K < 6; ++K) { const float A = 2.f * PI * K / 6.f; Lamps.Box(Ce + FVector(11.8f * FMath::Cos(A), 11.8f * FMath::Sin(A), -26.f), FVector(1.1f, 1.1f, 12.5f), FMath::RadiansToDegrees(A), Iron); }
                            if (Lanterns++ % 4 == 0 && Lit < 64) { Plan.Lights.Add(FVector4(Ce.X, Ce.Y, Ce.Z - 26.f, 20.f)); Plan.LightCols.Add(FLinearColor(.1f, .3f, 1.f, 950.f)); ++Lit; }
                        }
                        else if (Kind == 1)   // plants at the hedge's foot: ferns, glowing caps, foxgloves, celandine, broad leaves
                        {
                            for (int32 N = Rd.RandRange(2, 4), K = 0; K < N; ++K)
                            {
                                const int32 Pk = Rd.RandRange(0, UE_ARRAY_COUNT(Plants) - 1); const FVector2D Q = P + Tn * Rd.FRandRange(-55.f, 55.f) + In * Rd.FRandRange(-4.f, 22.f);
                                Plan.Inst[Plants[Pk]].Add(FTransform(FRotator(Rd.FRandRange(-5.f, 5.f), Rd.FRandRange(0.f, 360.f), Rd.FRandRange(-5.f, 5.f)), FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y) - 2.f), FVector(Rd.FRandRange(PlantS[Pk][0], PlantS[Pk][1]))));
                            }
                        }
                        else if (Kind == 2)   // a stone urn, flowers in it
                        {
                            const FVector C(P.X, P.Y, Z); const FVector2D Urn[] = {{.5f, 0.f}, {16.f, 0.f}, {17.f, 6.f}, {11.f, 12.f}, {10.f, 22.f}, {20.f, 34.f}, {26.f, 50.f}, {25.f, 62.f}, {29.f, 66.f}, {29.f, 70.f}, {22.f, 70.f}, {21.f, 64.f}};
                            Deco.Lathe(C, Urn, 14, Plain);
                            for (int32 K = 0; K < 3; ++K) Plan.Inst[K == 0 ? KFox : KCel0 + (K & 1)].Add(FTransform(FRotator(0, Rd.FRandRange(0.f, 360.f), 0), C + FVector(Rd.FRandRange(-8.f, 8.f), Rd.FRandRange(-8.f, 8.f), 62.f), FVector(K == 0 ? .7f : 1.3f)));
                        }
                        else if (Kind == 3)   // a stone bench along the hedge
                        {
                            const FVector C(P.X, P.Y, Z), Back = -Fi * 8.f; const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tn.Y, Tn.X));
                            for (const float Sx : {-1.f, 1.f}) Deco.Box(C + Back + Ft * (Sx * 46.f) + FVector(0, 0, 17.f), FVector(8.f, 18.f, 17.f), Yaw, Plain);
                            Deco.Box(C + Back + FVector(0, 0, 38.f), FVector(64.f, 21.f, 5.f), Yaw, Plain);
                        }
                        else if (Kind == 4)   // a patch of low fog: a few veils lying close over the floor
                            for (int32 K = 0; K < 6; ++K) { const float X = P.X + Rd.FRandRange(-260.f, 260.f), Y = P.Y + Rd.FRandRange(-260.f, 260.f); Plan.Veils.Add(FVector4(X, Y, Land(D, X, Y) + Rd.FRandRange(18.f, 40.f), Rd.FRandRange(240.f, 380.f))); }
                        else   // petals fallen on the floor
                            Plan.Inst[KPetals].Add(FTransform(FRotator(0, FMath::RadiansToDegrees(Dc.Z), 0), FVector(P.X, P.Y, Z + 1.f), FVector(Rd.FRandRange(.8f, 1.5f))));
                    }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_DECOR lanterns=%d lit=%d items=%d veils=%d"), Lanterns, Lit, Mz.Decor.Num(), Plan.Veils.Num());
                }
                // (2026-11-23) The parkour over the thorns, the thorns, and the root gaps (FeyMap::MazeParkour, MazeGaps).
                {
                    FMapSurf& Wood = Plan.Surfs.AddDefaulted_GetRef(); Wood.Mat = MRoot;                                // walkway, ladder, steps, gap roofs
                    FMapSurf& Lath = Plan.Surfs.AddDefaulted_GetRef(); Lath.Mat = MRoot; Lath.bSolid = false;            // trellis, hoops, roots
                    FMapSurf& Ivy = Plan.Surfs.AddDefaulted_GetRef(); Ivy.Mat = MPlant; Ivy.bSolid = false; Ivy.bShadow = false;
                    FMapSurf& Plank = Plan.Surfs.AddDefaulted_GetRef(); Plank.Mat = MPlank;                              // walkway timber
                    const FColor PlankCol = FLinearColor(0.f, 1.f, 0.f, 1.f).ToFColor(false);
                    const FColor WoodCol = FLinearColor(.22f, 1.f, 0.f, 1.f).ToFColor(false), CaneCol = FLinearColor(0.f, 1.f, 0.f, 1.f).ToFColor(false);
                    const FColor IvyCol = FLinearColor(.16f, .34f, .1f, 0.f).ToFColor(false), IvyDark = FLinearColor(.1f, .24f, .07f, 0.f).ToFColor(false);
                    const FColor ThornCol = FLinearColor(.78f, .66f, .5f, 0.f).ToFColor(false), BerryCol = FLinearColor(.2f, .03f, .1f, 0.f).ToFColor(false), BLeaf = FLinearColor(.17f, .27f, .08f, 0.f).ToFColor(false);
                    FRandomStream Rp(4417);
                    auto At = [&](const FeyMap::FMazeParkour& P, float S, float X, float Z) { const FVector2D Q = FeyMap::MazeParkourAt(P, S, X); return FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y) + Z); };
                    auto YawAt = [&](const FeyMap::FMazeParkour& P, float S) { return FMath::RadiansToDegrees(P.Ac + P.Dir * S / P.R) + 90.f; };   // X along the way
                    // A leaf: a little blade, both faces, lying across Hint (its face turned along Hint).
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
                    // Ivy: a stem along points, leaves on it.
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
                    for (const FeyMap::FMazeParkour& P : FeyMap::MazeParkour())
                    {
                        // The thorns: arching bramble canes thick across the whole corridor, thorned, a few leaves and berries.
                        FMapSurf& Canes = Plan.Surfs.AddDefaulted_GetRef(); Canes.Mat = MRoot; Canes.bSolid = false;
                        FMapSurf& Bits = Plan.Surfs.AddDefaulted_GetRef(); Bits.Mat = MPlant; Bits.bSolid = false; Bits.bShadow = false;
                        const int32 Nc = FMath::RoundToInt((P.Bed1 - P.Bed0) * 260.f / 650.f);
                        for (int32 K = 0; K < Nc; ++K)
                        {
                            const float S = Rp.FRandRange(P.Bed0 + 8.f, P.Bed1 - 8.f), X = Rp.FRandRange(-126.f, 126.f);
                            const FVector B0 = At(P, S, X, -3.f); const float Ang = Rp.FRandRange(0.f, 2 * PI), Reach = Rp.FRandRange(28.f, 62.f), Hgt = Rp.FRandRange(16.f, 40.f);
                            const FVector Dh(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
                            const FVector Pts[] = {B0, B0 + Dh * Reach * .25f + FVector(0, 0, Hgt * .85f), B0 + Dh * Reach * .65f + FVector(0, 0, Hgt), B0 + Dh * Reach + FVector(0, 0, Hgt * .35f), B0 + Dh * Reach * 1.12f + FVector(0, 0, -4.f)};
                            Canes.Pipe(Pts, 2.4f, 1.f, 5, 2, CaneCol, K * .37f);
                            for (int32 T = 0; T < 7; ++T)
                            {
                                const float U = Rp.FRandRange(.2f, 3.6f); const int32 I0 = int32(U); const FVector Pp = FMath::Lerp(Pts[I0], Pts[I0 + 1], U - I0);
                                FVector O = Rp.VRand(); O.Z = FMath::Abs(O.Z) * .6f; Thorn(Bits, Pp, O.GetSafeNormal(), Rp.FRandRange(4.f, 7.f), ThornCol);
                            }
                            if (K % 2 == 0) LeafCard(Bits, Pts[1 + K % 3], (Dh + FVector(0, 0, .5f) + Rp.VRand() * .4f).GetSafeNormal(), FVector::UpVector, Rp.FRandRange(7.f, 11.f), 3.5f, BLeaf);
                            if (K % 7 == 0) { const FVector Bp = Pts[3] + FVector(0, 0, 3.f); for (const FVector& O : {FVector(1.2f, 0, 0), FVector(-.6f, 1.f, 0), FVector(-.6f, -1.f, 0), FVector(0, 0, 1.2f)}) Thorn(Bits, Bp, O.GetSafeNormal(), 2.2f, BerryCol); }
                        }
                        if (P.Kind == 0)   // a trellis along the hedge, ivy on it, from TrellisReach before the thorns to as far after
                        {
                            const float X = P.Side * FeyMap::TrellisFace, Re = FeyMap::TrellisReach;
                            const FVector Rad = (At(P, 0.f, X + P.Side * 10.f, 0.f) - At(P, 0.f, X, 0.f)).GetSafeNormal2D();   // into the hedge
                            for (int32 K = 0; K <= 4; ++K) { const float S = FMath::Lerp(-Re, Re, K / 4.f); Lath.Box(At(P, S, X + P.Side * 3.f, 150.f), FVector(3.5f, 3.5f, 156.f), YawAt(P, S), WoodCol); }
                            // its face for his run: a flat wall at the trellis (the hedge behind is lumpy)
                            for (int32 K = 0; K < 4; ++K) { const float S = FMath::Lerp(-Re, Re, (K + .5f) / 4.f); Plan.Walls.Add({At(P, S, X + P.Side * 4.f, 160.f), FVector(Re / 4.f + 3.f, 4.f, 160.f), YawAt(P, S)}); }
                            for (const float Z : {12.f, 150.f, 298.f})
                            { TArray<FVector> Pts; for (int32 K = 0; K <= 8; ++K) Pts.Add(At(P, FMath::Lerp(-Re, Re, K / 8.f), X + P.Side * 2.f, Z)); Lath.Pipe(Pts, 2.4f, 2.4f, 4, 2, WoodCol, Z); }
                            for (const float Dg : {1.f, -1.f})
                                for (float T = -Re - 300.f; T < Re; T += 32.f)
                                {
                                    const float U0 = FMath::Max(0.f, (-Re - T) / 300.f), U1 = FMath::Min(1.f, (Re - T) / 300.f);
                                    if (U1 - U0 < .04f) continue;
                                    TArray<FVector> Pts; for (int32 K = 0; K <= 4; ++K) { const float U = FMath::Lerp(U0, U1, K / 4.f); Pts.Add(At(P, T + 300.f * U, X, 8.f + 290.f * (Dg > 0.f ? U : 1.f - U))); }
                                    Lath.Pipe(Pts, 1.3f, 1.3f, 4, 1, WoodCol, T);
                                }
                            for (int32 K = 0; K < 9; ++K)   // ivy climbing it
                            {
                                const float S0 = FMath::Lerp(-Re + 20.f, Re - 20.f, (K + Rp.FRand() * .6f) / 9.f); TArray<FVector> Pts;
                                for (int32 J = 0; J <= 6; ++J) Pts.Add(At(P, S0 + 22.f * FMath::Sin(J * 1.3f + K), X - P.Side * 2.f, -2.f + J * Rp.FRandRange(30.f, 48.f)));
                                IvyStem(Pts, Rad, 26);
                            }
                        }
                        else if (P.Kind == 1)   // ivy hoops across the corridor, their crowns the grips, garlands between
                        {
                            TArray<FVector> Crowns;
                            for (int32 K = 0; K < P.NHoop; ++K)
                            {
                                const float S = (K - (P.NHoop - 1) * .5f) * FeyMap::HoopStep; TArray<FVector> Pts;
                                for (int32 J = 0; J <= 10; ++J) { const float U = J / 10.f * 2.f - 1.f; Pts.Add(At(P, S, U * 132.f, -6.f + (FeyMap::GripUp + 6.f) * FMath::Sqrt(FMath::Max(0.f, 1.f - U * U)))); }
                                Lath.Pipe(Pts, 3.4f, 3.4f, 7, 3, WoodCol, K * 3.1f);
                                Crowns.Add(At(P, S, 0.f, FeyMap::GripUp));
                                const FVector Along = (At(P, S + 5.f, 0.f, 0.f) - At(P, S - 5.f, 0.f, 0.f)).GetSafeNormal2D();
                                IvyStem(Pts, Along, 36);
                                for (const float Xs : {-90.f, -55.f, 60.f, 95.f})   // hanging strands, clear of his paws at the crown
                                {
                                    const float U = Xs / 132.f; const FVector Top = At(P, S, Xs, (FeyMap::GripUp + 6.f) * FMath::Sqrt(1.f - U * U) - 6.f);
                                    const FVector Hs[] = {Top, Top + FVector(0, 0, -12.f) + Along * 3.f, Top + FVector(0, 0, -26.f) - Along * 2.f, Top + FVector(0, 0, -38.f)};
                                    IvyStem(Hs, Along, 8);
                                }
                            }
                            for (int32 K = 0; K + 1 < Crowns.Num(); ++K)
                                for (const float Xs : {-34.f, 30.f})
                                {
                                    const float Sa = (K - (P.NHoop - 1) * .5f) * FeyMap::HoopStep; const float U = Xs / 132.f, Zc = (FeyMap::GripUp + 6.f) * FMath::Sqrt(1.f - U * U) - 6.f;
                                    const FVector Gs[] = {At(P, Sa, Xs, Zc), At(P, Sa + FeyMap::HoopStep * .5f, Xs * 1.1f, Zc - 16.f), At(P, Sa + FeyMap::HoopStep, Xs, Zc)};
                                    IvyStem(Gs, FVector::UpVector, 14);
                                }
                        }
                        else if (P.Kind == 2)   // a raised walkway on the outer side: a ladder up at its start, steps down at its end
                        {
                            const float Xm = FeyMap::WalkAcross, Up = FeyMap::WalkUp;
                            for (float S = FeyMap::WalkFrom + 8.f; S < FeyMap::WalkTo; S += 16.f) Plank.Box(At(P, S, Xm, Up - 3.5f + Rp.FRandRange(-.6f, .6f)), FVector(7.4f, 46.f + Rp.FRandRange(-2.f, 2.f), 3.5f), YawAt(P, S) + Rp.FRandRange(-1.5f, 1.5f), PlankCol);
                            for (float S = FeyMap::WalkFrom; S < FeyMap::WalkTo; S += 70.f)
                                for (const float Xe : {Xm - 40.f, Xm + 40.f}) Plank.Box(At(P, S + 35.f, Xe, Up - 13.f), FVector(37.f, 4.f, 6.f), YawAt(P, S + 35.f), PlankCol);
                            for (float S = FeyMap::WalkFrom + 20.f; S <= FeyMap::WalkTo; S += 105.f)
                                for (const float Xe : {Xm - 40.f, Xm + 40.f}) Plank.Box(At(P, S, Xe, (Up - 16.f) * .5f - 4.f), FVector(4.5f, 4.5f, (Up - 16.f) * .5f + 4.f), YawAt(P, S) + 8.f, PlankCol);
                            // The ladder against its start: two rails, rungs every 28 cm, ivy up one side.
                            const float Sl = FeyMap::WalkFrom - 8.f;
                            for (const float Xe : {Xm - 24.f, Xm + 24.f}) Plank.Box(At(P, Sl, Xe, (Up + 24.f) * .5f - 4.f), FVector(3.f, 3.f, (Up + 24.f) * .5f + 4.f), YawAt(P, Sl), PlankCol);
                            for (float Z = 22.f; Z < Up; Z += 28.f) Plank.Box(At(P, Sl, Xm, Z), FVector(2.5f, 22.f, 2.f), YawAt(P, Sl), PlankCol);
                            { TArray<FVector> Pts; for (int32 J = 0; J <= 6; ++J) Pts.Add(At(P, Sl - 3.f, Xm + 26.f + 4.f * FMath::Sin(J * 1.7f), -2.f + J * 30.f)); IvyStem(Pts, (At(P, 5.f, 0, 0) - At(P, -5.f, 0, 0)).GetSafeNormal2D(), 24); }
                            // Steps down: four of 32 cm, each 60 along: a tread and a riser of boards on two stringers (open
                            // beneath), each step's block kept as collision only.
                            for (int32 K = 0; K < 4; ++K)
                            {
                                const float S = FeyMap::WalkTo + 30.f + 60.f * K, Top = Up - 32.f * (K + 1);
                                Plan.Walls.Add({At(P, S, Xm, (Top - 6.f) * .5f), FVector(30.5f, 45.f, (Top + 6.f) * .5f), YawAt(P, S)});
                                for (const float Dt2 : {-15.f, 15.f}) Plank.Box(At(P, S + Dt2, Xm, Top - 3.f), FVector(14.6f, 45.f + Rp.FRandRange(-1.5f, 1.5f), 3.f), YawAt(P, S + Dt2) + Rp.FRandRange(-1.f, 1.f), PlankCol);
                                Plank.Box(At(P, S - 28.5f, Xm, Top + 16.f), FVector(2.f, 44.f, 16.f), YawAt(P, S - 28.5f), PlankCol);
                            }
                            for (const float Xe : {Xm - 41.f, Xm + 41.f})   // the stringers, from the walkway's end to the floor
                            {
                                const FVector A = At(P, FeyMap::WalkTo, Xe, Up - 8.f), B = At(P, FeyMap::WalkTo + 270.f, Xe, 4.f);
                                TArray<FVector> Pts = {A, B}; Plank.Pipe(Pts, 4.5f, 4.5f, 4, 1, PlankCol, Xe);
                            }
                            // Ivy trailing along its outer edge.
                            { TArray<FVector> Pts; for (int32 J = 0; J <= 10; ++J) Pts.Add(At(P, FMath::Lerp(FeyMap::WalkFrom, FeyMap::WalkTo, J / 10.f), Xm + 44.f, Up - 6.f - 10.f * FMath::Abs(FMath::Sin(J * .9f)))); IvyStem(Pts, (At(P, 0, 10.f, 0) - At(P, 0, 0, 0)).GetSafeNormal2D(), 50); }
                        }
                    }
                    // The root gaps: a slab of root for a roof, roots arching over both mouths and down the sides.
                    for (const FeyMap::FMazeGap& G : FeyMap::MazeGaps())
                    {
                        const float Yr = FMath::DegreesToRadians(G.Yaw); const FVector Al(FMath::Cos(Yr), FMath::Sin(Yr), 0.f), Th(-Al.Y, Al.X, 0.f);
                        const FVector C(G.C.X, G.C.Y, Land(D, G.C.X, G.C.Y));
                        Wood.Box(C + FVector(0, 0, FeyMap::GapTall + 9.f), FVector(FeyMap::GapHalf + 16.f, HedgeHalf + 6.f, 9.f), G.Yaw, WoodCol);
                        for (const float Sd : {-1.f, 1.f})
                            for (int32 K = 0; K < 3; ++K)
                            {
                                const float W = FeyMap::GapHalf + 4.f + K * 9.f, Out = Sd * (HedgeHalf + 3.f + K * 4.f), Hg = FeyMap::GapTall + 6.f + K * 10.f;
                                const FVector Pts[] = {C - Al * (W + 14.f) + Th * Out + FVector(0, 0, -6.f), C - Al * W + Th * Out + FVector(0, 0, Hg * .7f), C + Th * (Out * 1.04f) + FVector(0, 0, Hg),
                                                       C + Al * W + Th * Out + FVector(0, 0, Hg * .7f), C + Al * (W + 18.f) + Th * Out + FVector(0, 0, -6.f)};
                                Lath.Pipe(Pts, 6.f - K * 1.2f, 4.f - K * .8f, 7, 3, WoodCol, K * 2.3f + Sd);
                            }
                    }
                }
                // The Cloud Staircase's first steps out of the maze's heart (stage 7 builds the stair; here it stops). What shows
                // is the tower's cloud (BuildTower); here only their footing, hidden, each step's whole span so there is no gap.
                for (int32 I = 0; I < StairWalked; ++I)
                {
                    const FStairStep& Sk = Stair()[I]; const FVector2D P = MazeAt(Sk.R, Sk.A); const float Top = Sk.Z;
                    const float Slab = FMath::Min(Top * .5f + 10.f, 45.f);   // (2026-11-22: a slab under each tread, not a column through the maze below)
                    Cl.Box(FVector(P.X, P.Y, Top - Slab + 0.f), FVector(Sk.R * FMath::Sin(Sk.Da * .5f) + 14.f, Sk.W - 15.f, Slab), FMath::RadiansToDegrees(Sk.A) + 90.f, Cloud);
                }
                const FStairStep& Ls = Stair()[StairWalked - 1];
                const float Ae = Ls.A + Ls.Da * .5f - 25.f / Ls.R; const FVector2D End = MazeAt(Ls.R, Ae);   // (at the landing's very end: 1.5 m beyond it left a gap to fall through, 2026-11-23)
                Plan.Walls.Add({FVector(End.X, End.Y, Ls.Z + 600.f), FVector(40.f, Ls.W + 120.f, 700.f), FMath::RadiansToDegrees(Ae) + 90.f});   // (the boundary: the way on is not open yet)
                // Its rails: invisible, either side of the landing and the steps over the hedges, so a misstep high up is not a fall to the maze.
                for (int32 I = 30; I < StairWalked; ++I)
                {
                    const FStairStep& Sk = Stair()[I];
                    for (const float Sd : {-1.f, 1.f})
                    { const FVector2D Q = MazeAt(Sk.R + Sd * (Sk.W + 20.f), Sk.A); Plan.Walls.Add({FVector(Q.X, Q.Y, Sk.Z + 110.f), FVector(Sk.R * FMath::Sin(Sk.Da * .5f) + 30.f, 20.f, 110.f), FMath::RadiansToDegrees(Sk.A) + 90.f}); }
                }
            }
            if (D == Deepwood)   // Redcap Hollow: a ring of stones round a cold fire pit
                for (int32 I = 0; I < 9; ++I)
                {
                    const float A = 2 * PI * I / 9; const FVector2D P = Redcap + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 160.f;
                    St.Box(FVector(P.X, P.Y, Land(D, P.X, P.Y) + 8.f), FVector(22.f, 16.f, 12.f), FMath::RadiansToDegrees(A), Plain);
                }
            if (D == LilyMere)   // the pads he hops across the deep water: each a flat disc of collision under its pad
            {
                FMapSurf& Pc = Plan.Surfs.AddDefaulted_GetRef(); Pc.Mat = MBlock; Pc.bHidden = true; Pc.bShadow = false;
                for (const FVector& Pd : MoatPads())
                {
                    const FVector2D Disc[] = {{0.f, 2.f}, {Pd.Z, 2.f}, {Pd.Z, -40.f}};
                    Pc.Lathe(FVector(Pd.X, Pd.Y, 0.f), Disc, 20, FColor::White);
                    Plan.Inst[KPadBig].Add(FTransform(FRotator(0, float(int32(Pd.X * 7) % 360), 0), FVector(Pd.X, Pd.Y, .4f), FVector(Pd.Z / 50.f, Pd.Z / 50.f, .6f)));
                }
            }
            // Signs (the user, 2026-11-20): wooden arrows with nonsense glyphs cut into them, leading on through every part of
            // the map; toward the end they turn mischievous (up, down, every way at once).
            for (const FSignPost& Sp : SignPosts())
            {
                if (Sp.D != D) continue;
                const float Z = Land(D, Sp.At.X, Sp.At.Y);
                Sg.Box(FVector(Sp.At.X, Sp.At.Y, Z + 72.f), FVector(5.f, 5.f, 85.f), Sp.Arrows.Num() ? Sp.Arrows[0].X : 0.f, FColor(40, 0, 0, 255));
                for (int32 I = 0; I < Sp.Arrows.Num(); ++I)
                    SignBoard(Sg, FVector(Sp.At.X, Sp.At.Y, Z + 140.f - 19.f * (I % 4) - (I >= 4 ? 9.f : 0.f)), Sp.Arrows[I].X, Sp.Arrows[I].Y, float(((I + 1) * 7919 + int32(Sp.At.X)) % 997) / 997.f);
            }
            for (int32 I = Plan.Surfs.Num() - 1; I >= 0; --I) if (Plan.Surfs[I].V.IsEmpty()) Plan.Surfs.RemoveAt(I);
        }
        // ------------------------------ the Deepwood: its blue lights low among the roots, its mist
        if (D == Deepwood)
        {
            FRandomStream Rl(4040);
            for (const TArray<FVector2D>& L : Data().Lines[D])
                for (int32 I = 0; I + 1 < L.Num(); ++I)
                    for (float U = 0; U < FVector2D::Distance(L[I], L[I + 1]); U += 2400.f)
                    {
                        const FVector2D Tn = (L[I + 1] - L[I]).GetSafeNormal(), P = L[I] + Tn * U + FVector2D(-Tn.Y, Tn.X) * Rl.FRandRange(-700.f, 700.f);
                        if (Rl.FRand() < .3f) { Plan.Lights.Add(FVector4(P.X, P.Y, Land(D, P.X, P.Y) + Rl.FRandRange(60.f, 160.f), Rl.FRandRange(8.f, 14.f))); Plan.LightCols.Add(FLinearColor(.2f, .42f, 1.f, 900.f)); }   // (2026-11-21, again: these softer; 2026-11-22: fewer and fainter, the lanterns light the way)
                    }
            // (2026-11-22, the user: the reference's blue lamps, deepwood-lantern-path.png) Crooked wooden posts along the paths,
            // each with a hooked arm and a hanging iron lantern of blue glass that lights the way a deep blue.
            {
                FMapSurf& Posts = Plan.Surfs.AddDefaulted_GetRef(); Posts.Mat = MRoot;
                FMapSurf& Lamps = Plan.Surfs.AddDefaulted_GetRef(); Lamps.Mat = MPlant; Lamps.bSolid = false; Lamps.bShadow = false;
                const FColor Wood = FLinearColor(.4f, .75f, 0, 1).ToFColor(false), Iron(30, 30, 36, 0), Glass(70, 150, 255, 225);
                bool bLeft = false; int32 Count = 0;
                for (const TArray<FVector2D>& L : Data().Lines[D])
                    for (int32 I = 0; I + 1 < L.Num(); ++I)
                        for (float U = 700.f; U < FVector2D::Distance(L[I], L[I + 1]) && Count < 80; U += 2100.f)
                        {
                            const FVector2D Tn = (L[I + 1] - L[I]).GetSafeNormal(), Sd = FVector2D(-Tn.Y, Tn.X) * (bLeft ? -1.f : 1.f); bLeft = !bLeft;
                            const FVector2D P = L[I] + Tn * U + Sd * 270.f;
                            if (MouthNear(D, P, 1200.f) >= 0 || FVector2D::Distance(P, Redcap) < 2400.f || PathDist(D, P.X, P.Y) < 220.f) continue;
                            ++Count;
                            const FVector B(P.X, P.Y, Land(D, P.X, P.Y) - 20.f), Arm(-Sd.X, -Sd.Y, 0.f), Tw(Tn.X, Tn.Y, 0.f);
                            const float Lean = Rl.FRandRange(-1.f, 1.f), Hgt = Rl.FRandRange(235.f, 270.f);
                            const FVector Pts[] = {B, B + FVector(0, 0, Hgt * .38f) + Tw * 9.f * Lean - Arm * 4.f, B + FVector(0, 0, Hgt * .75f) - Tw * 7.f * Lean - Arm * 6.f,
                                                   B + FVector(0, 0, Hgt) + Arm * 8.f, B + FVector(0, 0, Hgt + 24.f) + Arm * 42.f, B + FVector(0, 0, Hgt + 14.f) + Arm * 78.f, B + FVector(0, 0, Hgt - 4.f) + Arm * 90.f};
                            Posts.Pipe(Pts, 9.f, 3.5f, 8, 5, Wood, float(Count) * 2.7f);
                            // the lantern: a short chain from the hook, a domed iron cap, six panes of glowing glass in an iron frame, a finial
                            const FVector Hook = B + FVector(0, 0, Hgt + 12.f) + Arm * 80.f, Ce = Hook - FVector(0, 0, 13.f);
                            const FVector Chain[] = {Hook + FVector(0, 0, 4.f), Ce};
                            Lamps.Pipe(Chain, 1.3f, 1.3f, 4, 1, Iron, 0.f);
                            const FVector2D Cap[] = {{.5f, 0.f}, {3.f, -1.f}, {9.f, -6.f}, {13.5f, -10.f}, {14.f, -12.f}, {10.5f, -13.f}};
                            const FVector2D Pane[] = {{10.5f, -13.f}, {11.5f, -16.f}, {11.5f, -36.f}, {9.5f, -40.f}};
                            const FVector2D Foot[] = {{9.5f, -40.f}, {12.f, -41.5f}, {11.f, -44.f}, {5.f, -48.f}, {.5f, -56.f}};
                            Lamps.Lathe(Ce, Cap, 6, Iron); Lamps.Lathe(Ce, Pane, 6, Glass); Lamps.Lathe(Ce, Foot, 6, Iron);
                            for (int32 K = 0; K < 6; ++K)
                            {
                                const float A = 2.f * PI * K / 6.f;
                                Lamps.Box(Ce + FVector(11.8f * FMath::Cos(A), 11.8f * FMath::Sin(A), -26.f), FVector(1.1f, 1.1f, 12.5f), FMath::RadiansToDegrees(A), Iron);
                            }
                            Plan.Lights.Add(FVector4(Ce.X, Ce.Y, Ce.Z - 26.f, 70.f)); Plan.LightCols.Add(FLinearColor(.1f, .3f, 1.f, 1500.f));
                        }
            }
            for (int32 I = 0; I < 150; ++I)   // (2026-11-21: more of them, more along the paths)
            {
                float X = Rl.FRandRange(Ds.X0 + 1500.f, Ds.X1 - 1500.f), Y = Rl.FRandRange(Ds.Y0 + 1500.f, Ds.Y1 - 1500.f);
                if (I % 3 == 0) { const TArray<FVector2D>& L = Data().Lines[D][Rl.RandRange(0, Data().Lines[D].Num() - 1)]; const int32 K = Rl.RandRange(0, L.Num() - 2); const FVector2D Q = FMath::Lerp(L[K], L[K + 1], Rl.FRand()); X = Q.X + Rl.FRandRange(-500.f, 500.f); Y = Q.Y + Rl.FRandRange(-500.f, 500.f); }
                Plan.Veils.Add(FVector4(X, Y, Land(D, X, Y) + Rl.FRandRange(60.f, 320.f), Rl.FRandRange(800.f, 1800.f)));
            }
        }

        // ------------------------------ planting
        FRandomStream R(1000 + D);
        const float Ha = (Ds.X1 - Ds.X0) * (Ds.Y1 - Ds.Y0) / 1e8f;
        TArray<FVector> Oaks;   // (X, Y, scale)
        TArray<FVector> Wonders; int32 WonderCount = 0;   // (2026-11-23, the wonder pass: X, Y, its clear radius)
        auto Put = [&](int32 K, float X, float Y, float S, float Yaw, float Sink, float Tilt)
        {
            const FRotator Rot(R.FRandRange(-Tilt, Tilt), Yaw, R.FRandRange(-Tilt, Tilt));
            // (2026-11-23) Nothing on ground steeper than 45 degrees (petals and grass hung on the narrow cut's walls).
            const float E = 25.f;   // (and none beside such ground: tall grass at a wall's foot leant up into the rock)
            for (const FVector2D& O : {FVector2D(0, 0), FVector2D(80, 0), FVector2D(-80, 0), FVector2D(0, 80), FVector2D(0, -80)})
                if (FVector2D(Land(D, X + O.X + E, Y + O.Y) - Land(D, X + O.X - E, Y + O.Y), Land(D, X + O.X, Y + O.Y + E) - Land(D, X + O.X, Y + O.Y - E)).Size() / (2 * E) > 1.f) return;
            Plan.Inst[K].Add(FTransform(Rot, FVector(X, Y, Land(D, X, Y) - Sink), FVector(S)));
        };
        auto Edge = [&](float X, float Y) { return D == Riverbank ? FMath::Min(X - Ds.X0, FMath::Min(Y - Ds.Y0, Ds.Y1 - Y)) : FMath::Min(FMath::Min(X - Ds.X0, Ds.X1 - X), FMath::Min(Y - Ds.Y0, Ds.Y1 - Y)); };
        auto Free = [&](float X, float Y, float Clear)
        {
            if (Edge(X, Y) < 120.f || PathDist(D, X, Y) < Clear || MouthNear(D, FVector2D(X, Y), 1600.f) >= 0) return false;
            float Wet; Land(D, X, Y, nullptr, &Wet); if (Wet > .3f) return false;
            if (D == Crossroads && FVector2D::Distance(FVector2D(X, Y), Ds.Hub) < 2600.f) return false;
            if (D == Maze && MazeRadius(X, Y) < (Clear > 300.f ? 15200.f : 13700.f)) return false;   // (trees clear of the outer hedge)
            if (D == Highmeadow && FVector2D::Distance(FVector2D(X, Y), Overlook) < 1200.f) return false;
            for (const FVector& Cl : FeyLifeClearings()) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(Cl)) < FMath::Square(Cl.Z)) return false;   // (FeyLife's nooks and beds)
            if (D == Highmeadow)   // (2026-11-21) nothing tall in the overlook's view down onto the maze
            {
                const FVector2D V = FVector2D(X, Y) - Overlook, To = (Table[Maze].Hub - Overlook).GetSafeNormal();
                if (V.Size() < 14000.f && FVector2D::DotProduct(V.GetSafeNormal(), To) > .7f && Clear > 300.f) return false;
            }
            if (D == Deepwood && FVector2D::Distance(FVector2D(X, Y), Redcap) < 2200.f && Clear > 300.f) return false;
            if (D == Deepwood && FVector2D::Distance(FVector2D(X, Y), FVector2D(HagSpot())) < (Clear > 300.f ? 1300.f + Clear * .5f : 680.f)) return false;   // (the hag's cottage and its garden)
            return true;
        };
        auto Oak = [&](float X, float Y, float S, float Gap)
        {
            for (const FVector& O : Oaks) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(O)) < FMath::Square(Gap * FMath::Max(S, O.Z) * .5f + Gap * .5f)) return false;
            if (D == Highmeadow)   // (2026-11-21) none in the overlook's view down onto the maze, whatever planted it
            {
                const FVector2D V = FVector2D(X, Y) - Overlook, To = (Table[Maze].Hub - Overlook).GetSafeNormal();
                if (V.Size() < 16000.f && FVector2D::DotProduct(V.GetSafeNormal(), To) > .6f) { UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_OVERLOOK_CLEARED oak=%.0f,%.0f"), X, Y); return false; }
            }
            const int32 Kind = R.RandRange(0, 1); const float Yaw = R.FRandRange(0.f, 360.f);
            const FTransform Xf(FRotator(0, Yaw, 0), FVector(X, Y, Land(D, X, Y) - 14.f), FVector(S));
            if (D == Deepwood) { Plan.Inst[Kind ? KDeep1 : KDeep0].Add(Xf); Plan.Inst[Kind ? KDCol1 : KDCol0].Add(Xf); }   // (the Deepwood's own: mossed, rooty, hung with moss)
            else { Plan.Inst[Kind ? KOak1 : KOak0].Add(Xf); Plan.Inst[Kind ? KCol1 : KCol0].Add(Xf); }
            Oaks.Add(FVector(X, Y, S));
            return true;
        };
        auto Scatter = [&](int32 K, float PerHa, float S0, float S1, float Sink, float Tilt, float Clear, TFunctionRef<float(float, float)> Chance)
        {
            const int32 N = FMath::RoundToInt(PerHa * Ha);
            for (int32 I = 0; I < N; ++I)
            {
                const float X = R.FRandRange(Ds.X0, D == Riverbank ? -2300.f : Ds.X1), Y = R.FRandRange(Ds.Y0, Ds.Y1);
                if (!Free(X, Y, Clear) || R.FRand() > Chance(X, Y)) continue;
                Put(K, X, Y, R.FRandRange(S0, S1), R.FRandRange(0.f, 360.f), Sink, Tilt);
            }
        };
        auto Always = [](float, float) { return 1.f; };
        auto Clumps = [](float Scale, float Seed) { return [Scale, Seed](float X, float Y) { return Smooth(-.05f, .3f, Noise(X, Y, Scale, Seed)); }; };
        auto Walked = [&](float X, float Y) { return D == Highmeadow ? .85f : Smooth(1500.f, 500.f, PathDist(D, X, Y)); };
        auto Rooted = [&](float X, float Y)
        {
            float Best = 0; for (const FVector& O : Oaks) { const float Dd = FVector2D::Distance(FVector2D(X, Y), FVector2D(O)); if (Dd < 1300.f * O.Z) Best = FMath::Max(Best, Smooth(1100.f * O.Z, 220.f, Dd)); }
            return FMath::Max(Best, .25f * Smooth(620.f, 260.f, PathDist(D, X, Y)));
        };
        // The wooded rim: oaks shoulder to shoulder round the edge, gaps only at the thresholds and the overlook.
        {
            const float S0 = D == Deepwood ? 1.3f : 1.15f, S1 = D == Deepwood ? 1.9f : 1.8f;
            const float W = Ds.X1 - Ds.X0, Hh = Ds.Y1 - Ds.Y0, Per = 2 * (W + Hh);
            for (float U = 0; U < Per; U += R.FRandRange(650.f, 1050.f))
            {
                FVector2D P, In;
                if (U < W) { P = FVector2D(Ds.X0 + U, Ds.Y0); In = FVector2D(0, 1); }
                else if (U < W + Hh) { P = FVector2D(Ds.X1, Ds.Y0 + U - W); In = FVector2D(-1, 0); }
                else if (U < 2 * W + Hh) { P = FVector2D(Ds.X1 - (U - W - Hh), Ds.Y1); In = FVector2D(0, -1); }
                else { P = FVector2D(Ds.X0, Ds.Y1 - (U - 2 * W - Hh)); In = FVector2D(1, 0); }
                if (D == Riverbank && In.X < 0) continue;
                if (D == Highmeadow && In.X > 0 && P.Y > 6000.f && P.Y < 18500.f) continue;
                if (D == Maze && In.X < 0) continue;   // (2026-11-21: the maze's side facing the overlook stays open, as its stand-in shows it)
                P += In * R.FRandRange(300.f, 1500.f);
                if (MouthNear(D, P, 2600.f) >= 0) continue;
                const float S = R.FRandRange(S0, S1);
                if (PathDist(D, P.X, P.Y) < (D == Deepwood ? 250.f + 950.f * S : 350.f + 420.f * S)) continue;   // (roots clear of the paths, as inside)
                Oak(P.X, P.Y, S, 700.f);
            }
        }
        const float OaksPerHa = D == Deepwood ? 75.f : D == Riverbank ? 12.f : D == Highmeadow ? 4.f : D == Maze ? 14.f : D == Orchard ? 4.f : 8.f;
        for (int32 I = 0, N = FMath::RoundToInt(OaksPerHa * Ha * 1.6f); I < N; ++I)
        {
            const float X = R.FRandRange(Ds.X0, Ds.X1), Y = R.FRandRange(Ds.Y0, Ds.Y1);
            const float S = D == Deepwood ? R.FRandRange(1.05f, 1.6f) : R.FRandRange(1.f, 1.6f);
            if (!Free(X, Y, D == Deepwood ? 250.f + 950.f * S : 350.f + 420.f * S) || Edge(X, Y) < 1800.f) continue;   // (the Deepwood's roots reach 12 m)   // (its roots and low limbs clear of the path)
            Oak(X, Y, S, D == Deepwood ? 650.f : 1300.f);
        }
        switch (D)
        {
        case Riverbank:
        {
            // Bell-flower lamps along the path (glowing by their material only), and petals drifted on it.
            const TArray<FVector2D>& Road = Data().RoadOf[D][0]; bool bLeft = false;
            for (int32 I = 0; I + 1 < Road.Num(); ++I)
                for (float U = 0; U < FVector2D::Distance(Road[I], Road[I + 1]); U += 2600.f)
                {
                    const FVector2D T = (Road[I + 1] - Road[I]).GetSafeNormal(), Side(-T.Y, T.X), P = Road[I] + T * U + Side * (bLeft ? -215.f : 215.f);
                    Put(KBell, P.X, P.Y, 1.35f, FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X)) + (bLeft ? 90.f : -90.f), 2.f, 0.f); bLeft = !bLeft;
                }
            for (int32 I = 0; I < 260; ++I) { const int32 S = R.RandRange(0, Road.Num() - 2); const FVector2D P = FMath::Lerp(Road[S], Road[S + 1], R.FRand()) + FVector2D(R.FRandRange(-150.f, 150.f), R.FRandRange(-150.f, 150.f)); Put(KPetals, P.X, P.Y, R.FRandRange(.8f, 1.6f), R.FRandRange(0.f, 360.f), -1.f, 0.f); }
            const FVector Giants[] = {{-11000, -2600, 1.3f}, {-17500, 2900, 2.2f}, {-21000, -3800, 3.f}};
            for (const FVector& G : Giants) Put(KAmber, G.X, G.Y, G.Z, R.FRandRange(0.f, 360.f), 6.f, 2.f);
            break;
        }
        case Orchard:
        {
            // The orchard: rows of small blossoming trees (stand-ins until the art pass) east of the hub.
            for (float X = -41500.f; X < -29800.f; X += 900.f)
                for (float Y = Ds.Y0 + 1900.f; Y < Ds.Y1 - 1900.f; Y += 1100.f)
                {
                    const float Px = X + R.FRandRange(-150.f, 150.f), Py = Y + R.FRandRange(-150.f, 150.f);
                    if (!Free(Px, Py, 350.f)) continue;
                    const float S = R.FRandRange(.48f, .62f); const FTransform Xf(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(Px, Py, Land(D, Px, Py) - 8.f), FVector(S));
                    Plan.Inst[KOak0].Add(Xf); Plan.Inst[KCol0].Add(Xf); Oaks.Add(FVector(Px, Py, S));   // (the wisteria-hung oak: its blossom)
                }
            // The Needle Garden: lanes between low walls of stone, tall flower spires between them (orchids later).
            for (const float Y : {-10500.f, -8200.f, -2400.f, 2400.f, 8200.f, 10500.f})
                for (float X = -51800.f; X < -42500.f; X += R.FRandRange(150.f, 210.f))
                {
                    if (!Free(X, Y, 300.f) || Noise(X, Y, 900, 91) < -.35f) continue;
                    Put(KRock0 + R.RandRange(0, 5), X, Y, R.FRandRange(.9f, 1.4f), R.FRandRange(0.f, 360.f), 14.f, 8.f);
                }
            Scatter(KFox, 900.f, 1.6f, 2.6f, 2.f, 4.f, 260.f, [](float X, float) { return Smooth(-41000.f, -43000.f, X); });
            break;
        }
        case Crossroads:
        {
            // The vast old tree, and standing stones round the ring, clear of the roads.
            const FTransform Xf(FRotator(0, 30.f, 0), FVector(Ds.Hub.X, Ds.Hub.Y, Land(D, Ds.Hub.X, Ds.Hub.Y) - 60.f), FVector(5.2f));
            Plan.Inst[KOak0].Add(Xf); Plan.Inst[KCol0].Add(Xf);
            for (int32 I = 0; I < 16; ++I)
            {
                const float A = 2 * PI * (I + .5f) / 16; const FVector2D P = Ds.Hub + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 4300.f;
                if (PathDist(D, P.X, P.Y) < 500.f) continue;
                Put(KRock0 + R.RandRange(0, 5), P.X, P.Y, R.FRandRange(1.8f, 2.6f), R.FRandRange(0.f, 360.f), 20.f, 4.f);
            }
            const FVector Giants[] = {{-60500, -8000, 2.4f}, {-76000, 8200, 2.8f}, {-76500, -8200, 2.f}, {-59500, 8600, 1.8f}};
            for (const FVector& G : Giants) if (Free(G.X, G.Y, 500.f)) Put(KAmber, G.X, G.Y, G.Z, R.FRandRange(0.f, 360.f), 6.f, 2.f);
            break;
        }
        case LilyMere:
        {
            for (int32 P = 0; P < UE_ARRAY_COUNT(Pools); ++P)
            {
                const FVector& Pl = Pools[P]; const int32 N = FMath::RoundToInt(PI * Pl.Z * Pl.Z / 16000.f);
                for (int32 I = 0; I < N; ++I)
                {
                    const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand()) * Pl.Z * .95f, X = Pl.X + Rr * FMath::Cos(A), Y = Pl.Y + Rr * FMath::Sin(A);
                    int32 Which; if (PoolE(X, Y, Which) > .93f || Noise(X, Y, 520, 17 + P) < -.1f) continue;
                    const float S = R.FRandRange(.55f, 1.5f);
                    Plan.Inst[KPad].Add(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, .6f), FVector(S, S, 1)));
                    if (R.FRand() < .17f) Plan.Inst[KLily].Add(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X + R.FRandRange(-16.f, 16.f), Y + R.FRandRange(-16.f, 16.f), 3.f), FVector(R.FRandRange(.9f, 1.5f))));
                }
            }
            // Great rocks where the moat runs into the rim, hiding its ends.
            for (const float Ang : {.1f, PI - .1f})
                for (const float Rr : {MoatIn - 150.f, (MoatIn + MoatOut) * .5f, MoatOut + 150.f})
                {
                    const FVector2D P = MoatC + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rr;
                    Put(KRock0 + R.RandRange(0, 5), P.X, P.Y, R.FRandRange(2.6f, 3.6f), R.FRandRange(0.f, 360.f), 40.f, 8.f);
                }
            // Reeds along the shores.
            Scatter(KGrass1, 9000.f, 2.4f, 4.f, 1.f, 8.f, 140.f, [](float X, float Y) { int32 W; const float E = PoolE(X, Y, W); return E > 1.03f ? Smooth(1.5f, 1.08f, E) : 0.f; });
            break;
        }
        case Deepwood:
        {
            // The hollow's ring of stones, the camp's sitting stones.
            for (int32 I = 0; I < 11; ++I)
            {
                const float A = 2 * PI * I / 11 + R.FRandRange(-.1f, .1f); const FVector2D P = Redcap + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(1700.f, 2100.f);
                if (PathDist(D, P.X, P.Y) < 400.f) continue;
                Put(KRock0 + R.RandRange(0, 5), P.X, P.Y, R.FRandRange(1.6f, 2.8f), R.FRandRange(0.f, 360.f), 18.f, 8.f);
            }
            break;
        }
        case Highmeadow:
        {
            // (2026-11-23, the wonder pass) The great wisteria: an oak hung all over with wisteria (was the plain oak, a stand-in).
            const FTransform Xf(FRotator(0, 110.f, 0), FVector(Wisteria.X, Wisteria.Y, Land(D, Wisteria.X, Wisteria.Y) - 30.f), FVector(2.7f));
            Plan.Inst[KWist].Add(Xf); Plan.Inst[KWistCol].Add(Xf); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WONDER district=Highmeadow kit=%d at=%.0f,%.0f,%.0f"), int32(KWist), Wisteria.X, Wisteria.Y, Xf.GetLocation().Z);
            const FVector Giants[] = {{-90000, 12000, 2.6f}, {-104000, -6000, 3.4f}, {-97500, 16500, 2.2f}, {-108000, -2000, 4.f}, {-88500, -7000, 1.8f}};
            for (const FVector& G : Giants) if (Free(G.X, G.Y, 500.f)) Put(KAmber, G.X, G.Y, G.Z, R.FRandRange(0.f, 360.f), 6.f, 2.f);
            // Rocks along the rapids' banks.
            for (float Y = Ds.Y0 + 600.f; Y < Ds.Y1 - 600.f; Y += R.FRandRange(300.f, 700.f))
            {
                if (FMath::Abs(Y - 2500.f) < 900.f) continue;   // (the ford stays open)
                const float S = R.RandRange(0, 1) ? 1.f : -1.f, X = RapidX(Y) + S * R.FRandRange(560.f, 760.f);
                Put(KRock0 + R.RandRange(0, 5), X, Y, R.FRandRange(.8f, 1.8f), R.FRandRange(0.f, 360.f), 16.f, 10.f);
            }
            break;
        }
        case Maze:
        {
            // Glowing fungi and ferns at the hedges' feet.
            FRandomStream Rf(77);
            for (const FHedge& Hd : MazeData().Hedges)   // (one in four hedge sections, a few at its foot)
            {
                if (Hd.Kind || Rf.FRand() > .25f) continue;
                const float A = FMath::DegreesToRadians(Hd.Yaw); const FVector2D Along(FMath::Cos(A), FMath::Sin(A)), Off(-Along.Y, Along.X);
                const FVector2D P = FVector2D(Hd.C) + Along * Rf.FRandRange(-Hd.H.X, Hd.H.X) + Off * (Rf.RandRange(0, 1) ? 1.f : -1.f) * Rf.FRandRange(75.f, 105.f);
                Put(Rf.FRand() < .45f ? KBlue : Rf.FRand() < .5f ? KMoss : KFern0 + Rf.RandRange(0, 3), P.X, P.Y, Rf.FRandRange(.6f, 1.3f), Rf.FRandRange(0.f, 360.f), 1.f, 5.f);
            }
            break;
        }
        default: break;
        }
        // ------------------------------ the wonder pass (2026-11-23; docs/regions/FEYWILD.md#the-wonder-pass)
        // A landmark or two in each district (great bell flowers on the riverbank and in the meadow, flower spires in the
        // orchard, fly agarics at the Crossroads' edges and by the redcaps' camp, a stand of tall blue caps in the Deepwood,
        // giant lilies on the mere), rounded archway ruins in the open districts, and oaks hung with purple moss by the mere's
        // fen and the Crossroads' edges. Their own random stream, so the rest of the planting is as it was.
        {
            FRandomStream Rw(5000 + D);
            auto Clear = [&](float X, float Y, float R0)
            {
                if (!Free(X, Y, R0)) return false;
                for (const FVector& O : Oaks) if (FVector2D::Distance(FVector2D(X, Y), FVector2D(O)) < 650.f * O.Z + R0 * .6f) return false;
                for (const FVector& W : Wonders) if (FVector2D::Distance(FVector2D(X, Y), FVector2D(W)) < W.Z + R0) return false;
                return true;
            };
            auto Seek = [&](FVector2D Around, float Spread, float Near, float Far, float R0, FVector2D& Out)
            {
                for (int32 I = 0; I < 600; ++I)
                {
                    const FVector2D Q = Around + FVector2D(Rw.FRandRange(-Spread, Spread), Rw.FRandRange(-Spread, Spread));
                    const float Pd = PathDist(D, Q.X, Q.Y);
                    if (Pd < Near || Pd > Far || !Clear(Q.X, Q.Y, R0)) continue;
                    Out = Q; Wonders.Add(FVector(Q.X, Q.Y, R0)); return true;
                }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WONDER_MISSED district=%s near=%s"), Ds.Name, *Around.ToString());
                return false;
            };
            auto Raise = [&](int32 K, int32 Col, const FVector2D& Q, float S, float Yaw, float Sink)
            {
                const FTransform Xf(FRotator(0, Yaw, 0), FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y) - Sink), FVector(S));
                Plan.Inst[K].Add(Xf); if (Col >= 0) Plan.Inst[Col].Add(Xf); ++WonderCount; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WONDER district=%s kit=%d at=%.0f,%.0f,%.0f"), Ds.Name, K, Q.X, Q.Y, Xf.GetLocation().Z);
            };
            auto Stem = [&](const FVector2D& Q, float R0, float H) { Plan.Walls.Add({FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y) + H * .5f), FVector(R0, R0, H * .5f), 45.f}); };
            FVector2D P;
            auto Bells = [&](FVector2D At, float S) { if (Seek(At, 1600.f, 380.f, 1500.f, 450.f, P)) Raise(KGiantBell, -1, P, S * Rw.FRandRange(.85f, 1.1f), Rw.FRandRange(0.f, 360.f), 6.f); };
            auto Arch = [&](FVector2D At, int32 Kind, float Near, float Far)
            {
                if (Seek(At, 3200.f, Near, Far, 650.f, P)) Raise(Kind ? KArch1 : KArch0, Kind ? KArchCol1 : KArchCol0, P, Rw.FRandRange(.95f, 1.15f), Rw.FRandRange(0.f, 360.f), 18.f);
            };
            auto Mossed = [&](FVector2D At, float Spread) { if (Seek(At, Spread, 700.f, 5000.f, 1100.f, P)) { const float S = Rw.FRandRange(1.1f, 1.45f); Raise(KPurple, KPurpleCol, P, S, Rw.FRandRange(0.f, 360.f), 14.f); Oaks.Add(FVector(P.X, P.Y, S)); } };
            switch (D)
            {
            case Riverbank:
            {
                const TArray<FVector2D>& Road = Data().RoadOf[D][0];
                for (const float F : {.2f, .45f, .7f}) { const float Fi = F * (Road.Num() - 1); const int32 I = FMath::Min(int32(Fi), Road.Num() - 2); FVector2D At = FMath::Lerp(Road[I], Road[I + 1], Fi - I); At.X = FMath::Min(At.X, -6500.f); Bells(At, 1.5f); }   // (inside the district: its road runs on into the slice)
                break;
            }
            case Orchard:
            {
                for (const FVector2D At : {FVector2D(-47000, -5300), FVector2D(-47000, 5300), FVector2D(-50000, -6000)})
                    if (Seek(At, 1600.f, 350.f, 3000.f, 380.f, P)) Raise(KSpire, -1, P, Rw.FRandRange(.9f, 1.1f), Rw.FRandRange(0.f, 360.f), 4.f);
                // Two whole arches over the road through the orchard's rows, walked under on purpose (its edges are all oaks).
                for (const float Xa : {-31200.f, -36400.f})
                {
                    bool bDone = false;
                    for (const TArray<FVector2D>& L : Data().Lines[D])
                        for (int32 I = 0; I + 1 < L.Num() && !bDone; ++I)
                            if ((L[I].X - Xa) * (L[I + 1].X - Xa) <= 0.f && L[I].X != L[I + 1].X && FMath::Abs(L[I].Y) < 5000.f)
                            {
                                const FVector2D Tn = (L[I + 1] - L[I]).GetSafeNormal(), Q = FMath::Lerp(L[I], L[I + 1], (Xa - L[I].X) / (L[I + 1].X - L[I].X));
                                Raise(KArch0, KArchCol0, Q, 1.05f, FMath::RadiansToDegrees(FMath::Atan2(Tn.Y, Tn.X)) - 90.f, 18.f); Wonders.Add(FVector(Q.X, Q.Y, 500.f)); bDone = true;
                            }
                }
                break;
            }
            case Crossroads:
            {
                for (const FVector2D At : {FVector2D(-74500, -8500), FVector2D(-61000, 8800)})
                    if (Seek(At, 1600.f, 500.f, 3500.f, 650.f, P)) { Raise(KToad, -1, P, Rw.FRandRange(.9f, 1.15f), Rw.FRandRange(0.f, 360.f), 6.f); Stem(P, 34.f, 300.f); }
                Mossed(FVector2D(-58800, -2500), 1500.f); Mossed(FVector2D(-77500, -3500), 1500.f); Mossed(FVector2D(-77200, 5200), 1500.f);
                break;
            }
            case LilyMere:
            {
                for (const int32 Pi : {1, 2})   // a giant lily on each of the two great pools, on a great pad
                {
                    const FVector& Pl = Pools[Pi];
                    for (int32 I = 0; I < 60; ++I)
                    {
                        const float A = Rw.FRandRange(0.f, 2 * PI), Rr = Pl.Z * Rw.FRandRange(.2f, .5f); const FVector2D Q(Pl.X + Rr * FMath::Cos(A), Pl.Y + Rr * FMath::Sin(A));
                        int32 W; if (PoolE(Q.X, Q.Y, W) > .7f || FVector2D::Distance(Q, MoatC) < MoatOut + 900.f) continue;
                        const float Yaw = Rw.FRandRange(0.f, 360.f);
                        Plan.Inst[KPad].Add(FTransform(FRotator(0, Yaw, 0), FVector(Q.X, Q.Y, .8f), FVector(6.2f, 6.2f, 1.6f)));
                        Plan.Inst[KGiantLily].Add(FTransform(FRotator(0, Yaw + 20.f, 0), FVector(Q.X, Q.Y, -2.f), FVector(1.f))); ++WonderCount; break;
                    }
                }
                Mossed(FVector2D(-62500, 24500), 2000.f); Mossed(FVector2D(-71500, 29000), 2000.f); Mossed(FVector2D(-63500, 37500), 2000.f);
                Arch(FVector2D(-56500, 18500), 1, 600.f, 5000.f); Arch(FVector2D(-70500, 20000), 0, 600.f, 5000.f);
                break;
            }
            case Deepwood:
            {
                for (const FVector2D At : {FVector2D(-69500, -24000), FVector2D(-79500, -33500), FVector2D(-84500, -24500)})
                    if (Seek(At, 3200.f, 450.f, 2600.f, 500.f, P)) { Raise(KGiantBlue, -1, P, Rw.FRandRange(.9f, 1.15f), Rw.FRandRange(0.f, 360.f), 6.f); Stem(P, 26.f, 420.f); }
                if (Seek(Redcap + FVector2D(0, 2600), 1800.f, 400.f, 3000.f, 300.f, P)) { Raise(KToad, -1, P, .85f, Rw.FRandRange(0.f, 360.f), 6.f); Stem(P, 30.f, 260.f); }
                break;
            }
            case Highmeadow:
            {
                Bells(FVector2D(-98000, 0), 1.7f); Bells(FVector2D(-86500, 2500), 1.5f);
                Arch(FVector2D(-92000, -4500), 0, 600.f, 7000.f); Arch(FVector2D(-88000, 13000), 1, 600.f, 7000.f);
                Arch(FVector2D(-104000, 1500), 0, 600.f, 7000.f); Arch(FVector2D(-96000, -8000), 1, 600.f, 7000.f);
                break;
            }
            default: break;
            }
            if (WonderCount) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WONDERS district=%s pieces=%d"), Ds.Name, WonderCount);
        }
        if (D == Deepwood)
        {
            // (2026-11-23) The hag's cottage (Tools/build_feywild_kit.py hag_cottage): the house, its collision (the walls, the
            // chimney, the cauldron, the fence's posts as a few boxes), the warm light of its windows and lantern. Its fire and
            // smoke are FeyLife's.
            const FVector& Hs = HagSpot(); const float Z = DeepRaw(Hs.X, Hs.Y), Yaw = Hs.Z, Ya = FMath::DegreesToRadians(Yaw);
            const FVector2D Fx(FMath::Cos(Ya), FMath::Sin(Ya)), Fy(-Fx.Y, Fx.X);
            auto At = [&](float Ox, float Oy, float Oz) { const FVector2D P = FVector2D(Hs) + Fx * Ox + Fy * Oy; return FVector(P.X, P.Y, Z + Oz); };
            Plan.Inst[KHag].Add(FTransform(FRotator(0, Yaw, 0), FVector(Hs.X, Hs.Y, Z - 4.f)));
            Plan.Walls.Add({At(0, 0, 230), FVector(222.f, 272.f, 230.f), Yaw});
            Plan.Walls.Add({At(-14, 308, 300), FVector(40.f, 36.f, 300.f), Yaw});
            Plan.Walls.Add({At(440, -220, 60), FVector(52.f, 52.f, 60.f), Yaw});
            for (const FVector& L : {FVector(250, 105, 135), FVector(28, -300, 135), FVector(244, 9, 172)})   // (in front of each window, and the lantern by the door)
            { Plan.Lights.Add(FVector4(At(L.X, L.Y, L.Z), 12.f)); Plan.LightCols.Add(FLinearColor(1.f, .55f, .22f, 750.f)); }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HAG_BUILT at=%.0f,%.0f,%.0f yaw=%.0f"), Hs.X, Hs.Y, Z, Yaw);
        }
        if (D == Deepwood) { Scatter(KMoss, 1600.f, .8f, 1.8f, 2.f, 6.f, 160.f, Clumps(400, 5)); Scatter(KBlue, 900.f, .6f, 1.6f, 1.f, 5.f, 140.f, Clumps(300, 6)); }
        for (int32 I = 0; I < 4; ++I) Scatter(KFern0 + I, D == Deepwood ? 1100.f : 360.f, 1.f, 1.9f, 2.f, 8.f, 170.f, Clumps(620, I + 3 + D));
        Scatter(KBroad, D == Deepwood ? 420.f : 260.f, .8f, 1.7f, 2.f, 6.f, 180.f, Clumps(720, 8 + D));
        Scatter(KCrimson, 200.f, .8f, 1.6f, 2.f, 6.f, 180.f, Clumps(560, 9 + D));
        if (D != Orchard) Scatter(KFox, 200.f, .9f, 1.5f, 2.f, 4.f, 210.f, Clumps(460, 10 + D));
        const float Flowers = D == Riverbank || D == Highmeadow || D == Orchard ? 1500.f : D == Deepwood ? 300.f : 800.f;
        Scatter(KCel0, Flowers, 1.6f, 2.8f, 1.f, 4.f, 120.f, Clumps(380, 11 + D));
        Scatter(KCel1, Flowers, 1.6f, 2.8f, 1.f, 4.f, 120.f, Clumps(380, 12 + D));
        Scatter(KBlue, D == Deepwood ? 1200.f : 600.f, .7f, 2.1f, 1.f, 5.f, 150.f, Rooted);
        Scatter(KViolet, D == Deepwood ? 700.f : 350.f, .8f, 2.f, 1.f, 5.f, 190.f, Rooted);
        if (D != Deepwood) { Scatter(KGrass0, 7000.f, 2.6f, 4.4f, 1.f, 7.f, 125.f, Walked); Scatter(KGrass1, 4500.f, 2.f, 3.6f, 1.f, 7.f, 150.f, Walked); }
        else Scatter(KGrass1, 1200.f, 2.f, 3.2f, 1.f, 7.f, 150.f, Walked);
        for (int32 I = 0, N = FMath::RoundToInt(30.f * Ha); I < N; ++I)
        {
            const float X = R.FRandRange(Ds.X0, Ds.X1), Y = R.FRandRange(Ds.Y0, Ds.Y1);
            if (Free(X, Y, 950.f)) Put(KRock0 + R.RandRange(0, 5), X, Y, R.FRandRange(.7f, 2.2f), R.FRandRange(0.f, 360.f), 18.f, 10.f);
        }
        // ------------------------------ the Deepwood's light from its glowing things (2026-11-21, the user: less from the
        // floor's lights, more from the mushrooms and the trees' glowing bits). Where its glowing caps gather, a soft light of
        // their colour, stronger the more of them there are; its fungi and the veins in its oaks glow brighter (AFeyDistrict).
        if (D == Deepwood)
        {
            constexpr float Cell = 1600.f; TMap<FIntPoint, FVector4> Sum;   // (X, Y, count blue, count violet)
            for (const int32 K : {KBlue, KViolet, KAmber})
                for (const FTransform& Xf : Plan.Inst[K])
                {
                    const FVector L = Xf.GetLocation(); FVector4& C = Sum.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / Cell), FMath::FloorToInt(L.Y / Cell)));
                    const float Wt = K == KAmber ? 3.f : 1.f; C.X += L.X * Wt; C.Y += L.Y * Wt; (K == KBlue ? C.Z : C.W) += Wt;
                }
            TArray<FVector4> Clusters; for (const TPair<FIntPoint, FVector4>& Pr : Sum) Clusters.Add(Pr.Value);
            Clusters.Sort([](const FVector4& A, const FVector4& B) { return A.Z + A.W > B.Z + B.W; });   // (the 140 richest clusters)
            for (int32 Ci = 0; Ci < FMath::Min(140, Clusters.Num()); ++Ci)
            {
                const struct { FVector4 Value; } Pr{Clusters[Ci]};
                const float N = Pr.Value.Z + Pr.Value.W; if (N < 4.f) continue;
                const float X = Pr.Value.X / N, Y = Pr.Value.Y / N;   // (amber caps weigh more, roughly)
                const float Cx = FMath::Clamp(X, Ds.X0 + 200.f, Ds.X1 - 200.f), Cy = FMath::Clamp(Y, Ds.Y0 + 200.f, Ds.Y1 - 200.f), V = Pr.Value.W / N;
                Plan.Lights.Add(FVector4(Cx, Cy, Land(D, Cx, Cy) + 70.f, FMath::Min(18.f + 2.8f * N, 75.f)));   // (2026-11-22: deeper blue, a little stronger)
                Plan.LightCols.Add(FLinearColor(FMath::Lerp(.12f, .5f, V), FMath::Lerp(.36f, .22f, V), 1.f, FMath::Min(750.f + 25.f * N, 1350.f)));
            }
        }
        // ------------------------------ invisible bounds at the rectangle, open at each threshold's mouth
        auto Edges = [&](const FVector2D& A, const FVector2D& B)
        {
            TArray<float> Cuts; const FVector2D T = (B - A).GetSafeNormal(); const float Len = FVector2D::Distance(A, B);
            for (const FMouth& M : Data().Mouths[D]) { const float U = FVector2D::DotProduct(M.P - A, T); if (U > 0 && U < Len && SegDist(M.P, A, B) < 10.f) Cuts.Add(U); }
            Cuts.Sort(); float From = 0;
            auto Seg = [&](float U0, float U1) { if (U1 - U0 < 10.f) return; const FVector2D C = A + T * ((U0 + U1) * .5f); Plan.Walls.Add({FVector(C.X, C.Y, 2500.f), FVector((U1 - U0) * .5f, 50.f, 4000.f), float(FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X)))}); };
            for (const float U : Cuts) { Seg(From, U - 420.f); From = U + 420.f; }
            Seg(From, Len);
        };
        if (D == Highmeadow) Plan.Walls.Add({FVector(Ds.X0 + 980.f, 12250.f, 4500.f), FVector(50.f, 5800.f, 4000.f), 0.f});   // (the overlook's brink)
        Edges(FVector2D(Ds.X0, Ds.Y0), FVector2D(Ds.X0, Ds.Y1));
        Edges(FVector2D(D == Riverbank ? -2200.f : Ds.X1, Ds.Y0), FVector2D(Ds.X0, Ds.Y0));
        Edges(FVector2D(Ds.X0, Ds.Y1), FVector2D(D == Riverbank ? -2200.f : Ds.X1, Ds.Y1));
        if (D == Riverbank) { Edges(FVector2D(Ds.X1, Ds.Y0), FVector2D(Ds.X1, -4200.f)); Edges(FVector2D(Ds.X1, 4200.f), FVector2D(Ds.X1, Ds.Y1)); }
        else Edges(FVector2D(Ds.X1, Ds.Y0), FVector2D(Ds.X1, Ds.Y1));
        Plan.Seconds = float(FPlatformTime::Seconds() - T0);
    }
}

// ------------------------------------------------------------------ AFeyDistrict
AFeyDistrict::AFeyDistrict()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AFeyDistrict::Begin(int32 InDistrict)
{
    District = InDistrict; Started = FPlatformTime::Seconds();
    Plan = MakeShared<FPlan, ESPMode::ThreadSafe>();
    TSharedPtr<FPlan, ESPMode::ThreadSafe> P = Plan; const int32 D = District;
    Async(EAsyncExecution::ThreadPool, [P, D]() { PlanDistrict(D, *P); P->bDone = true; });
}

bool AFeyDistrict::Step(double BudgetMs)
{
    const double Until = FPlatformTime::Seconds() + BudgetMs / 1000.;
    if (!bPlanned)
    {
        if (!Plan.IsValid() || !Plan->bDone) return false;
        bPlanned = true; PlanSeconds = Plan->Seconds;
        for (const TCHAR* Path : MatPaths) { UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path); Missing += !M; Mats.Add(M); }
        if (Mats[MPlant]) { auto* Still = UMaterialInstanceDynamic::Create(Mats[MPlant], this); Still->SetScalarParameterValue(TEXT("Sway"), 0.f); Mats[MPlant] = Still; }   // (2026-11-23: the kit's plants sway; the map's own surfaces in it stay put)
        for (const FKitDef& K : KitDefs)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, K.Path); Missing += !Mesh;
            auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); H->SetupAttachment(RootComponent);
            H->SetMobility(EComponentMobility::Movable); H->SetStaticMesh(Mesh); H->SetCastShadow(K.bShadow);
            H->InstanceStartCullDistance = int32(K.C0); H->InstanceEndCullDistance = int32(K.C1); H->WorldPositionOffsetDisableDistance = 4500;
            H->SetCollisionEnabled(K.bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            if (K.bSolid) H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetHiddenInGame(K.bInvisible); H->bAutoRebuildTreeOnInstanceChanges = false;

            // (2026-11-21) The Deepwood's glowing caps, shelves and bark veins brighter than elsewhere (their own instances).
            const int32 Ki = Kits.Num();
            if (District == Deepwood && Mesh && (Ki == KBlue || Ki == KViolet || Ki == KAmber || Ki == KDeep0 || Ki == KDeep1))
                for (int32 Sl = 0; Sl < Mesh->GetStaticMaterials().Num(); ++Sl)
                {
                    UMaterialInterface* M = Mesh->GetMaterial(Sl); if (!M) continue;
                    const FString N = M->GetName();
                    if (N.Contains(TEXT("Fungus")) || N.Contains(TEXT("Plant"))) H->CreateDynamicMaterialInstance(Sl, M)->SetScalarParameterValue(TEXT("GlowGain"), 2.2f * 2.6f);
                    else if (N.Contains(TEXT("Bark"))) H->CreateDynamicMaterialInstance(Sl, M)->SetScalarParameterValue(TEXT("VeinGain"), 3.5f);
                }
            H->RegisterComponent(); Kits.Add(H);
        }
        if (FPlatformTime::Seconds() > Until) return false;
    }
    while (FPlatformTime::Seconds() < Until)
    {
        const double Ti = FPlatformTime::Seconds(); const int32 Si = Stage, Ii = Item;   // (2026-11-23: any one part slower than 8 ms is logged)
        ON_SCOPE_EXIT { const double Ms = (FPlatformTime::Seconds() - Ti) * 1000.; if (Ms > 8.) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_SLOW_PART district=%s stage=%d item=%d ms=%.1f"), Table[District].Name, Si, Ii, Ms); };
        if (Stage == 0)   // the surfaces, one each time round
        {
            if (Item >= Plan->Surfs.Num()) { Stage = 1; Item = 0; continue; }
            FMapSurf& S = Plan->Surfs[Item++];
            auto* Mesh = NewObject<UProceduralMeshComponent>(this); Mesh->SetupAttachment(RootComponent);
            Mesh->bUseComplexAsSimpleCollision = true; Mesh->bUseAsyncCooking = false; Mesh->RegisterComponent();
            Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, S.bSolid);
            if (S.T.Num() > 60000) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_BIG_SURF district=%s item=%d tris=%d solid=%d mat=%d"), Table[District].Name, Item - 1, S.T.Num() / 3, S.bSolid, S.Mat);
            Mesh->SetMaterial(0, Mats.IsValidIndex(S.Mat) ? Mats[S.Mat] : nullptr);
            Mesh->SetCollisionEnabled(S.bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            if (S.bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            Mesh->SetCastShadow(S.bShadow); Mesh->SetHiddenInGame(S.bHidden);
            Triangles += S.T.Num() / 3;
            if (District == LilyMere && S.Mat == MWater && !Mirror && Mats.IsValidIndex(MWater) && Mats[MWater])
            {
                // (2026-11-23, the wonder pass: mirror water at the lily-pad crossing) One capture taken from the great pool's
                // middle once the mere stands (and again when its textures have streamed in), the water reflecting it carried
                // out to about the trees' distance (M_FeyWater's Probe and Reach). Never drawn again per frame.
                const FVector At(Pools[1].X, Pools[1].Y, 70.f);
                MirrorTarget = NewObject<UTextureRenderTargetCube>(this); MirrorTarget->Init(512, PF_FloatRGBA); MirrorTarget->ClearColor = FLinearColor::Black; MirrorTarget->UpdateResourceImmediate(true);
                Mirror = NewObject<USceneCaptureComponentCube>(this); Mirror->SetupAttachment(RootComponent); Mirror->SetRelativeLocation(At);
                Mirror->TextureTarget = MirrorTarget; Mirror->CaptureSource = SCS_SceneColorHDR; Mirror->bCaptureEveryFrame = false; Mirror->bCaptureOnMovement = false;
                Mirror->HideComponent(Mesh); if (AActor* Chuck = UGameplayStatics::GetPlayerPawn(this, 0)) Mirror->HideActorComponents(Chuck);
                Mirror->RegisterComponent();
                auto* Mid = UMaterialInstanceDynamic::Create(Mats[MWater], this);
                Mid->SetTextureParameterValue(TEXT("Reflection"), MirrorTarget); Mid->SetVectorParameterValue(TEXT("Probe"), FLinearColor(At.X, At.Y, At.Z, 0)); Mid->SetVectorParameterValue(TEXT("Reach"), FLinearColor(7000.f, 7000.f, 2600.f, 0));
                Mesh->SetMaterial(0, Mid);
                for (const float When : {4.f, 12.f})
                {
                    FTimerHandle Th; GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Mirror) { Mirror->CaptureScene(); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MERE_MIRROR_CAPTURED")); } }), When, false);
                }
            }
            S = FMapSurf();   // (its arrays are not needed again)
        }
        else if (Stage == 1)   // the planting, a few thousand at a time
        {
            if (Item >= KitCount) { Stage = 2; continue; }
            TArray<FTransform>& All = Plan->Inst[Item];
            const int32 N = FMath::Min(4000, All.Num() - Offset);
            if (N > 0) { TArray<FTransform> Part(All.GetData() + Offset, N); Kits[Item]->AddInstances(Part, false, false); Planted += N; Offset += N; }
            if (Offset >= All.Num()) { if (All.Num()) Kits[Item]->BuildTreeIfOutdated(true, false); ++Item; Offset = 0; }
        }
        else if (Stage == 2)
        {
            for (const FPlan::FWall& W : Plan->Walls)
            {
                auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(RootComponent); B->SetBoxExtent(W.H);
                B->SetRelativeLocationAndRotation(W.C, FRotator(0, W.Yaw, 0)); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true); B->RegisterComponent();
            }
            // Small real lights (the Deepwood's blue glow among the roots), no shadows.
            for (const FVector4& Lt : Plan->Lights)
            {
                auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(RootComponent); L->SetRelativeLocation(FVector(Lt.X, Lt.Y, Lt.Z));
                L->SetIntensityUnits(ELightUnits::Candelas); const FLinearColor Lc = Plan->LightCols.IsValidIndex(int32(&Lt - Plan->Lights.GetData())) ? Plan->LightCols[int32(&Lt - Plan->Lights.GetData())] : FLinearColor(.28f, .55f, 1.f, 1000.f);
                L->SetIntensity(float(Lt.W)); L->SetLightColor(FLinearColor(Lc.R, Lc.G, Lc.B)); L->SetAttenuationRadius(Lc.A);
                L->SetCastShadows(false); L->SetVolumetricScatteringIntensity(2.f); L->RegisterComponent();
            }
            Veils = Plan->Veils;
            if (District == Maze) RegisterMazeParkour(GetWorld());
            Stage = 3;
        }
        else
        {
            for (UHierarchicalInstancedStaticMeshComponent* H : Kits) if (H->GetInstanceCount() && !H->IsTreeFullyBuilt()) return false;
            bReady = true; BuildSeconds = float(FPlatformTime::Seconds() - Started); Plan.Reset();
            { FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; Life = GetWorld()->SpawnActor<AFeyLife>(FeywildOrigin(), FRotator::ZeroRotator, Sp); if (Life) Life->Build(District); }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DISTRICT_READY name=%s seconds=%.2f plan=%.2f worst_step_ms=%.1f triangles=%d planted=%d missing=%d mem_mb=%.0f"),
                Table[District].Name, BuildSeconds, PlanSeconds, WorstStepMs, Triangles, Planted, Missing, FPlatformMemory::GetStats().UsedPhysical / 1048576.);
            return true;
        }
    }
    return false;
}

void AFeyDistrict::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Life) { Life->Destroy(); Life = nullptr; }
    Super::EndPlay(Reason);
}

void AFeyDistrict::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bReady) return;
    const double T = FPlatformTime::Seconds();
    Step(3.0);
    if (bPlanned) WorstStepMs = FMath::Max(WorstStepMs, float((FPlatformTime::Seconds() - T) * 1000.));
}

void AFeyDistrict::Finish()
{
    while (Plan.IsValid() && !Plan->bDone) FPlatformProcess::Sleep(.002f);
    for (int32 Guard = 0; !bReady && Guard < 100000; ++Guard)
    {
        Step(50.0);
        if (Stage == 3) for (UHierarchicalInstancedStaticMeshComponent* H : Kits) if (H->GetInstanceCount() && !H->IsTreeFullyBuilt()) H->BuildTreeIfOutdated(false, true);
    }
}

// ------------------------------------------------------------------ AFeyMap
AFeyMap::AFeyMap()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AFeyMap* AFeyMap::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AFeyMap> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AFeyMap* AFeyMap::Ensure(UWorld* World)
{
    if (!World) return nullptr;
    if (AFeyMap* Existing = Get(World)) return Existing;
    Data();
    const double T0 = FPlatformTime::Seconds();
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Map = World->SpawnActor<AFeyMap>(FeywildOrigin(), FRotator::ZeroRotator, P);
    Map->Districts.SetNum(FeyMap::Districts); FeyForgetSpots();
    Map->BuildThresholds(); Map->BuildDistance(); Map->BuildTower(); Map->BuildStandIn(); Map->BuildBrink(); Map->BuildWonders();
    Map->Missing += AFeyTrio::CountMissing();   // (2026-11-24: the package had been cooked without them)
    { float Jt; if (FParse::Value(FCommandLine::Get(), TEXT("-ChuckFeyJellyAt="), Jt)) JellyTime = Jt; }   // (stills: the jellyfish held where it is at that time)
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_BUILT seconds=%.2f triangles=%d missing=%d maze_cells=%d maze_way_m=%.0f maze_hedges=%d pads=%d signs=%d"), FPlatformTime::Seconds() - T0, Map->Triangles, Map->Missing,
        MazeCells(), MazeWay() / 100.f, MazeData().Hedges.Num(), MoatPads().Num(), SignPosts().Num());
    return Map;
}

UProceduralMeshComponent* AFeyMap::Emit(const TCHAR* Name, FMapSurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow)
{
    auto* Mesh = NewObject<UProceduralMeshComponent>(this, FName(Name)); Mesh->SetupAttachment(RootComponent);
    Mesh->bUseComplexAsSimpleCollision = true; Mesh->bUseAsyncCooking = false; Mesh->RegisterComponent();
    Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, bSolid);
    Missing += !Material; Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCastShadow(bShadow);
    Triangles += S.T.Num() / 3;
    return Mesh;
}

// The thresholds: a rock cutting, a root tunnel or a stone stair, each bent so neither end shows from its middle.
// (2026-11-20, the user: their ends were thin sheets.) Each is now solid ground: a cutting's sides are masses of rock with
// broad tops and outer slopes, a root tunnel runs under a mound, and both sink into each district's land past its mouth,
// so the way in reads as a gully or a tunnel mouth in a bank, never an edge.
void AFeyMap::BuildThresholds()
{
    UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, MatPaths[MGround]), *Root = LoadObject<UMaterialInterface>(nullptr, MatPaths[MRoot]);
    for (int32 Th = 0; Th < Thresholds; ++Th)
    {
        const FThreshold& W = Ways[Th];
        FMapSurf Mass, Tube; constexpr int32 Steps = 72;
        const bool bTube = W.Style == RootTunnel;
        auto Frame = [&](float T, FVector& C, FVector& Side)
        {
            C = ThresholdAtT(Th, T) - FVector(0, 0, 4.f);
            const FVector Tn = (ThresholdAtT(Th, T + .01f) - ThresholdAtT(Th, T - .01f)).GetSafeNormal2D(); Side = FVector(-Tn.Y, Tn.X, 0);
        };
        // How much of the masses stands: all of them in the gap, sinking to nothing a little way past each mouth.
        auto Rise = [](float T) { return WayRise(T); };
        auto Strip = [&](FMapSurf& S, const TArray<FVector2D>& Prof, float Sign, float T0, float T1, bool bRise, bool bBoth, TFunctionRef<FColor(float, int32)> Col)
        {
            for (int32 K = 0; K < Steps; ++K)
            {
                const float U0 = FMath::Lerp(T0, T1, float(K) / Steps), U1 = FMath::Lerp(T0, T1, float(K + 1) / Steps);
                FVector C0, S0, C1, S1; Frame(U0, C0, S0); Frame(U1, C1, S1);
                const float R0 = bRise ? Rise(U0) : 1.f, R1 = bRise ? Rise(U1) : 1.f;
                auto At = [&](const FVector& C, const FVector& Sd, const FVector2D& Q, float R, float U)
                {
                    const float Rough = FMath::Abs(Q.X) > Half + 1.f ? 140.f * Noise(U * 4000.f, Q.X, 700, Th + 30) : 0.f;
                    const float Up = Q.Y > 0.f ? Q.Y * R + (Q.Y > 400.f ? Rough * R : 0.f) : Q.Y;
                    return C + Sd * (Q.X * Sign + Rough * .3f * Sign) + FVector(0, 0, Up);
                };
                for (int32 M = 0; M + 1 < Prof.Num(); ++M)
                {
                    const FVector A = At(C0, S0, Prof[M], R0, U0), B = At(C0, S0, Prof[M + 1], R0, U0), Cc = At(C1, S1, Prof[M + 1], R1, U1), D = At(C1, S1, Prof[M], R1, U1);
                    // Facing: the profile's own left (its inner wall to the way, its top up, its outer slope away).
                    const FVector2D Tg = (Prof[M + 1] - Prof[M]).GetSafeNormal(); const FVector2D N2(-Tg.Y, Tg.X);
                    FVector Nn = (S0 * N2.X * Sign + FVector(0, 0, N2.Y)).GetSafeNormal(); if (Nn.IsNearlyZero()) Nn = FVector::UpVector;
                    S.Face(A, B, Cc, D, Nn, Col((U0 + U1) * .5f, M), 300.f);
                    if (bBoth) S.Face(A, B, Cc, D, -Nn, Col((U0 + U1) * .5f, M), 300.f);
                }
            }
        };
        const FColor Rock = FLinearColor(0, .55f, .12f, 1).ToFColor(false);
        auto Mossy = [&](float, int32) { return Rock; };
        // The floor and the masses either side as one heightfield over the gap and a little into each district, from the true
        // distance to the way's middle (offsetting cross-sections folded over on the inside of the S's bends and blocked it).
        {
            const FWayField& Fd = WayField(Th);
            const FBox2D& Box = Fd.Box; const float Step = Fd.Step; const int32 Nx = Fd.Nx, Ny = Fd.Ny;
            const TArray<float>& H = Fd.H; const TArray<uint8>& Skip = Fd.Skip; const TArray<uint8>& IsFloor = Fd.IsFloor;
            const int32 Base = Mass.V.Num();
            for (int32 I = 0; I <= Nx; ++I)
                for (int32 J = 0; J <= Ny; ++J)
                {
                    const int32 Ix = I * (Ny + 1) + J; auto At = [&](int32 A, int32 B) { return H[FMath::Clamp(A, 0, Nx) * (Ny + 1) + FMath::Clamp(B, 0, Ny)]; };
                    const FVector Nn = FVector(At(I - 1, J) - At(I + 1, J), At(I, J - 1) - At(I, J + 1), 2 * Step).GetSafeNormal();
                    const FVector P(Box.Min.X + I * Step, Box.Min.Y + J * Step, H[Ix]);
                    Mass.Vert(P, Nn, FVector2D(P) / 300.f, IsFloor[Ix] ? FLinearColor(1.f, .2f, .3f, 1.f).ToFColor(false) : Rock);
                }
            for (int32 I = 0; I < Nx; ++I)
                for (int32 J = 0; J < Ny; ++J)
                {
                    const int32 A = I * (Ny + 1) + J, B = A + Ny + 1;
                    if (Skip[A] && Skip[A + 1] && Skip[B] && Skip[B + 1]) continue;
                    if (Skip[A] || Skip[A + 1] || Skip[B] || Skip[B + 1]) { if ((Skip[A] + Skip[A + 1] + Skip[B] + Skip[B + 1]) > 2) continue; }
                    Mass.Quad(Base + A, Base + A + 1, Base + B + 1, Base + B);
                }
        }
        if (bTube)
        {
            // Over the tube a roof of earth only as long as the tube, so the way in at each end is the tube's own mouth.
            // (2026-11-21, the user: outside, the tunnels looked like impossibly thin tubes.) The tube no longer runs on out past
            // its banks: it ends in each bank's steep face (TubeRise), the face closed round it, a thick lip of root about
            // the opening, and great roots arching over it and down into the ground either side, as if under a vast tree.
            constexpr float E0 = -.03f, E1 = 1.03f;
            Strip(Mass, {{-Half - 140, 600}, {0, 650}, {Half + 140, 600}}, 1.f, E0, E1, false, false, Mossy);
            const TArray<FVector2D> Ring = {{Half, 0}, {Half + 70, 170}, {Half - 10, 360}, {130, 500}, {0, 530}, {-130, 500}, {-Half + 10, 360}, {-Half - 70, 170}, {-Half, 0}};
            Strip(Tube, Ring, 1.f, E0, E1, false, true, [](float U, int32) { return FLinearColor(.4f, FMath::Lerp(.45f, 1.f, FMath::Max(Smooth(.25f, -.12f, U), Smooth(.75f, 1.12f, U))), 0, 1).ToFColor(false); });
            const TArray<FVector2D> Bank = {{Half + 520, -40}, {Half + 600, 250}, {Half + 580, 520}, {Half + 380, 720}, {0, 830}, {-Half - 380, 720}, {-Half - 580, 520}, {-Half - 600, 250}, {-Half - 520, -40}};
            const FColor Bark = FLinearColor(.4f, 1.f, 0, 1).ToFColor(false);
            FRandomStream Rr(900 + Th);
            for (const float Te : {E0, E1})
            {
                FVector C, Sd; Frame(Te, C, Sd);
                const FVector Tn = (ThresholdAtT(Th, Te + .01f) - ThresholdAtT(Th, Te - .01f)).GetSafeNormal2D(), Od = Te < .5f ? -Tn : Tn;
                auto At = [&](const FVector2D& Q, float Back) { return C + Sd * Q.X + FVector(0, 0, Q.Y) - Od * Back; };
                for (int32 I = 0; I + 1 < Ring.Num(); ++I)   // the bank's face round the opening
                    Mass.Face(At(Ring[I], 0.f), At(Ring[I + 1], 0.f), At(Bank[I + 1], 40.f), At(Bank[I], 40.f), Od, Rock, 300.f);
                TArray<FVector> Lip; for (const FVector2D& Q : Ring) Lip.Add(At(Q, -12.f)); Lip[0].Z -= 40.f; Lip.Last().Z -= 40.f;
                Tube.Pipe(Lip, 46.f, 46.f, 12, 4, Bark, Th * 3.1f + Te);
                for (int32 K = 0; K < 6; ++K)   // roots over the mouth, down into the ground at either side
                {
                    const float S = K % 2 ? 1.f : -1.f, G = float(K / 2);
                    const FVector Pts[] = {At(FVector2D(-S * (Half + 200.f + 120.f * G), 760.f - 30.f * G), 260.f), At(FVector2D(-S * (60.f + 50.f * G), 600.f - 40.f * G + Rr.FRandRange(-20.f, 20.f)), -40.f),
                                           At(FVector2D(S * (Half + 60.f + 70.f * G), 380.f - 40.f * G), -90.f - 30.f * G), At(FVector2D(S * (Half + 200.f + 110.f * G), -60.f), -170.f - 60.f * G)};
                    Tube.Pipe(Pts, FMath::Lerp(62.f, 40.f, G / 2.f), 16.f, 10, 7, Bark, Th * 7.3f + K + Te);
                }
            }
        }
        Emit(*FString::Printf(TEXT("FeyThresholdMass%d"), Th), Mass, Ground, true, true);
        if (bTube) Emit(*FString::Printf(TEXT("FeyThresholdRoots%d"), Th), Tube, Root, true, true);
        // Each end shut, only while the district beyond it is not ready (it should never be needed).
        for (const float T : {.04f, .96f})
        {
            const FVector C = ThresholdAtT(Th, T), Tn = (ThresholdAtT(Th, T + .01f) - ThresholdAtT(Th, T - .01f)).GetSafeNormal2D();
            auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(RootComponent); B->SetBoxExtent(FVector(30.f, Half + 200.f, 700.f));
            B->SetRelativeLocationAndRotation(C + FVector(0, 0, 600.f), Tn.Rotation()); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true); B->RegisterComponent();
            B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            LateWalls.Add(B);
        }
    }
}


// The painted distance: low land under and between the districts, ridges closing the horizon, forest on all of it.
void AFeyMap::BuildDistance()
{
    UMaterialInterface* FarMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyFar.M_FeyFar")); Missing += !FarMat;
    if (FarMat)
    {
        FarMid = UMaterialInstanceDynamic::Create(FarMat, this);
        const FVector ToSun = -FRotator(-10.f, 168.f, 0.f).Vector();
        FarMid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0)); FarMid->SetScalarParameterValue(TEXT("OriginX"), FeywildOrigin().X);
    }
    constexpr float BX0 = -149000, BX1 = -4200, BY0 = -45000, BY1 = 41500;
    auto InRect = [](float X, float Y, float Inset) { for (const FDistrict& Ds : Table) if (X > Ds.X0 + Inset && X < Ds.X1 - Inset && Y > Ds.Y0 + Inset && Y < Ds.Y1 - Inset) return true; return false; };
    auto Height = [&](float X, float Y)
    {
        if (X > -26500.f && X < -4500.f && Y > -35000.f && Y < -25000.f) return -3000.f;   // (the river's set lies here, below)
        if (InRect(X, Y, 300.f)) return -800.f;
        const float O = FMath::Max(FMath::Max(BX0 - X, X - BX1), FMath::Max(BY0 - Y, Y - BY1));
        return -1000.f + 300.f * Noise(X, Y, 9000, 3) + Smooth(1000.f, 26000.f, O) * (5200.f + 3800.f * Noise(X, Y, 14000, 4) + 1500.f * Noise(X, Y, 4000, 5));
    };
    FMapSurf L; constexpr float X0 = -185000, X1 = -4400, Y0 = -82000, Y1 = 78000, Step = 1000.f;
    const int32 Nx = FMath::CeilToInt((X1 - X0) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step);
    for (int32 I = 0; I <= Nx; ++I)
        for (int32 J = 0; J <= Ny; ++J)
        {
            const float X = X0 + I * Step, Y = Y0 + J * Step, E = 500.f, H = Height(X, Y);
            const FVector Nn = FVector(Height(X - E, Y) - Height(X + E, Y), Height(X, Y - E) - Height(X, Y + E), 2 * E).GetSafeNormal();
            L.Vert(FVector(X, Y, H), Nn, FVector2D::ZeroVector, FColor(150, 0, 0, 255));
        }
    for (int32 I = 0; I < Nx; ++I) for (int32 J = 0; J < Ny; ++J) { const int32 A = I * (Ny + 1) + J, B = A + Ny + 1; L.Quad(A, A + 1, B + 1, B); }
    auto* Land = Emit(TEXT("FeyDistance"), L, FarMid, false, false); Land->bNeverDistanceCull = true;
    // Its forest: one set of trees for the ground between and beyond, and one for each district's ground, hidden once it stands.
    UStaticMesh* Tree = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Feywild/SM_FeyFarTree.SM_FeyFarTree")); Missing += !Tree;
    auto Set = [&]()
    {
        auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); H->SetupAttachment(RootComponent); H->SetMobility(EComponentMobility::Movable);
        H->SetStaticMesh(Tree); if (FarMid) H->SetMaterial(0, FarMid); H->SetCastShadow(false); H->SetCollisionEnabled(ECollisionEnabled::NoCollision); H->RegisterComponent();
        return H;
    };
    BackTrees.SetNum(FeyMap::Districts + 1);
    for (int32 I = 0; I <= FeyMap::Districts; ++I) BackTrees[I] = Set();
    FRandomStream R(404); int32 Planted = 0;
    for (int32 I = 0; I < 9000; ++I)
    {
        const float X = R.FRandRange(X0 + 4000.f, X1), Y = R.FRandRange(Y0 + 4000.f, Y1 - 4000.f), H = Height(X, Y);
        if (H < -2500.f || Noise(X, Y, 5000, 9) < -.3f) continue;
        // (2026-11-20) None near a threshold, where one would stand up through its rock or show over it.
        bool bNear = false; for (const FThreshold& Wy : Ways) bNear |= SegDist(FVector2D(X, Y), Wy.PA, Wy.PB) < 4800.f;
        if (bNear) continue;
        if (X > Table[Maze].X1 - 200.f && X < Table[Highmeadow].X0 + 200.f && Y > -10200.f && Y < 20200.f) continue;   // (the brink's valley stands there, BuildBrink)
        int32 Holder = FeyMap::Districts;
        for (int32 D = 0; D < FeyMap::Districts; ++D) { const FDistrict& Ds = Table[D]; if (X > Ds.X0 && X < Ds.X1 && Y > Ds.Y0 && Y < Ds.Y1) Holder = D; }
        if (Holder == Riverbank) continue;   // (it is never out of sight of its own trees)
        const float S = R.FRandRange(.8f, 1.5f);
        BackTrees[Holder]->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, H - 40.f), FVector(S, S, S * R.FRandRange(.85f, 1.2f)))); ++Planted;
    }
    for (auto* H : BackTrees) H->BuildTreeIfOutdated(false, true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_DISTANCE trees=%d"), Planted);
}

// Zephyros' tower on its cloud over the maze, and the cloud stair winding up to it: one far model, always there.
void AFeyMap::BuildTower()
{
    UMaterialInterface* Block = LoadObject<UMaterialInterface>(nullptr, MatPaths[MBlock]);
    UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyCloud.M_FeyCloud"));
    FMapSurf S, Cs, Ct; const FVector Hub(Table[Maze].Hub.X, Table[Maze].Hub.Y, 0.f);
    constexpr float Base = TowerBase;
    FRandomStream R(52);
    // The tower's cloud (2026-11-21): a heap of billowing lumps, its outline broken all round, its underside nearly flat with
    // a few hanging below, heaped higher toward the middle where the tower stands.
    {
        const float Floor = Base - 950.f;
        for (int32 I = 0; I < 38; ++I)   // the body: broad, flat, overlapping
        {
            const float A = R.FRandRange(0.f, 2 * PI), Rr = 5000.f * FMath::Sqrt(R.FRand()), K = 1.f - Rr / 5000.f;
            const float Sz = R.FRandRange(1700.f, 2700.f);
            FeyCloud::Puff(Cs, Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A) * .82f, Base - 250.f + 550.f * K + R.FRandRange(-150.f, 200.f)), FVector(Sz, Sz * R.FRandRange(.85f, 1.15f), Sz * R.FRandRange(.42f, .55f)), 4, I * 1.37f, Floor + R.FRandRange(-100.f, 100.f), .22f);
        }
        for (int32 I = 0; I < 30; ++I)   // the rim, for a broken outline
        {
            const float A = 2 * PI * (I + R.FRandRange(-.3f, .3f)) / 30.f, Rr = R.FRandRange(4900.f, 6200.f);
            const float Sz = R.FRandRange(1000.f, 1800.f);
            FeyCloud::Puff(Cs, Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A) * .82f, Base - 330.f + R.FRandRange(-200.f, 300.f)), FVector(Sz, Sz, Sz * R.FRandRange(.5f, .7f)), 3, 40.f + I * 2.11f, Floor + R.FRandRange(-60.f, 160.f), .26f);
        }
        for (int32 I = 0; I < 11; ++I)   // heaps on top
        {
            const float A = R.FRandRange(0.f, 2 * PI), Rr = R.FRandRange(1500.f, 4600.f), Sz = R.FRandRange(1100.f, 1800.f);
            FeyCloud::Puff(Cs, Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A) * .82f, Base + R.FRandRange(450.f, 900.f)), FVector(Sz, Sz, Sz * R.FRandRange(.65f, .85f)), 3, 90.f + I * 3.7f, Base - 300.f, .26f);
        }
        for (int32 I = 0; I < 6; ++I)    // a few soft lobes hanging below
        {
            const float A = R.FRandRange(0.f, 2 * PI), Rr = R.FRandRange(1500.f, 4800.f), Sz = R.FRandRange(1200.f, 2000.f);
            FeyCloud::Puff(Cs, Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A) * .82f, Floor + Sz * .05f), FVector(Sz * 1.2f, Sz, Sz * .3f), 3, 130.f + I * 1.9f, -1e6f, .2f);
        }
    }
    auto Spire = [&](const FVector& C, float Rad, float Height)
    {
        const FVector2D Wall[] = {{Rad, Height}, {Rad, 0.f}};
        S.Lathe(C, Wall, 18, Stone);
        const FVector2D Roof[] = {{0.f, Height + Rad * 3.4f}, {Rad * 1.45f, Height}, {Rad * .9f, Height - 60.f}};
        S.Lathe(C, Roof, 18, RoofBlue);
    };
    Spire(Hub + FVector(0, 0, Base + 600.f), 760.f, 7400.f);
    const FVector Lesser[] = {{1500, 300, 4600}, {-1250, 900, 3800}, {-600, -1400, 4300}, {1100, -1300, 3300}, {200, 1700, 2900}};
    for (const FVector& Ls : Lesser) Spire(Hub + FVector(Ls.X, Ls.Y, Base + 500.f), FMath::Lerp(360.f, 560.f, (Ls.Z - 2900.f) / 1700.f), Ls.Z);
    // The stair: one continuous band of cloud from the heart to the tower's cloud, its top stepped (a flat tread for each step,
    // a riser in the last tenth of it), its sides and underside billowing and smooth from step to step, so there is never a
    // gap or a seam between two steps. Billows along its sides and under it break its outline.
    const TArray<FStairStep>& St = Stair(); const int32 Ns = St.Num();
    auto Step = [&](float S, int32& I, float& F) { I = FMath::Clamp(int32(S), 0, Ns - 1); F = FMath::Clamp(S - I, 0.f, 1.f); };
    auto Centred = [&](float S, TFunctionRef<float(const FStairStep&)> Get) { const float U = FMath::Clamp(S - .5f, 0.f, Ns - 1.f); const int32 I = FMath::Min(int32(U), Ns - 2); return FMath::Lerp(Get(St[I]), Get(St[I + 1]), U - I); };
    auto Sweep = [&](FMapSurf& Into, float S0, float S1, bool bCap0, bool bCap1)
    {
        constexpr int32 PerStep = 12, Around = 20;
        const int32 Rings = FMath::Max(2, FMath::RoundToInt((S1 - S0) * PerStep) + 1);
        TArray<FVector> P, Out; TArray<FColor> Col; TArray<int32> Tri;
        for (int32 K = 0; K < Rings; ++K)
        {
            const float S = FMath::Lerp(S0, S1, float(K) / (Rings - 1)); int32 I; float F; Step(S, I, F);
            const float A = St[I].A + (F - .5f) * St[I].Da, R = Centred(S, [](const FStairStep& X) { return X.R; }), W = Centred(S, [](const FStairStep& X) { return X.W; });
            const float Top = I + 1 < Ns ? FMath::Lerp(St[I].Z, St[I + 1].Z, Smooth(.88f, 1.f, F)) : St[I].Z;
            const float Ramp = Centred(S, [](const FStairStep& X) { return X.Z; }), Bot = FMath::Min(Ramp - Centred(S, [](const FStairStep& X) { return X.Dp; }), Top - 120.f);
            const float Mid = (Top + Bot) * .5f, Hh = (Top - Bot) * .5f;
            const FVector Rad(FMath::Cos(A), FMath::Sin(A), 0.f), Ctr = Hub + Rad * R;
            for (int32 J = 0; J < Around; ++J)
            {
                const float Ph = 2 * PI * J / Around, Cx = FMath::Cos(Ph), Sy = FMath::Sin(Ph);
                const float X = W * FMath::Sign(Cx) * FMath::Pow(FMath::Abs(Cx), .85f), Y = Sy > 0.f ? Hh * FMath::Pow(Sy, .42f) : -Hh * FMath::Pow(-Sy, .8f);
                const FVector Dir = (Rad * Cx + FVector(0, 0, Sy * .8f)).GetSafeNormal();
                FVector Q = Ctr + Rad * X + FVector(0, 0, Mid + Y);
                const float Tread = Sy > 0.f ? Smooth(.5f, .9f, Sy) : 0.f, B = FeyCloud::Billow(Q / (W * 1.3f), 3.1f);
                Q += Dir * (W * .38f * (B - .72f) * 2.f * FMath::Lerp(1.f, .1f, Tread));
                P.Add(Q); Out.Add(Dir); Col.Add(FLinearColor(FMath::Clamp(.62f + .4f * Sy + .7f * (B - .72f), 0.f, 1.f), 1, 0, 1).ToFColor(false));
            }
            for (int32 J = 0; K + 1 < Rings && J < Around; ++J)
            {
                const int32 A0 = K * Around + J, A1 = K * Around + (J + 1) % Around, B0 = A0 + Around, B1 = A1 + Around;
                Tri.Append({A0, A1, B1, A0, B1, B0});
            }
        }
        // (Its ends closed by a point a little out from their middles.)
        for (int32 E = 0; E < 2; ++E)
        {
            if (!(E ? bCap1 : bCap0)) continue;
            const int32 Ring = E ? Rings - 1 : 0; FVector C = FVector::ZeroVector; for (int32 J = 0; J < Around; ++J) C += P[Ring * Around + J] / Around;
            const FVector Along = (P[(E ? Ring - 1 : 1) * Around] - P[Ring * Around]).GetSafeNormal2D() * -1.f;
            const int32 Cp = P.Add(C + Along * 60.f); Out.Add(Along); Col.Add(FLinearColor(.6f, 1, 0, 1).ToFColor(false));
            for (int32 J = 0; J < Around; ++J) { Tri.Append({Cp, Ring * Around + J, Ring * Around + (J + 1) % Around}); Out[Ring * Around + J] += Along * .6f; }
        }
        Into.Closed(P, Out, Tri, Col);
    };
    Sweep(Ct, 0.f, 24.f, true, false); Sweep(Cs, 24.f, float(Ns), false, true);   // (the foot apart, so it can cast shadows on the heart)
    for (int32 I = 0; I < Ns; ++I)
    {
        const FStairStep& Sp = St[I]; FMapSurf& Into = I < 24 ? Ct : Cs; const FVector Rad(FMath::Cos(Sp.A), FMath::Sin(Sp.A), 0.f);
        const float Side = I % 2 ? 1.f : -1.f, Bot = Sp.Z - Sp.Dp;
        for (const float Sd : {Side, -Side})   // (both sides, staggered, their tops a little under the tread)
            FeyCloud::Puff(Into, Hub + Rad * (Sp.R + Sd * Sp.W * .72f) + FVector(FMath::Sin(Sp.A), -FMath::Cos(Sp.A), 0.f) * (Sd * Sp.R * Sp.Da * .2f) + FVector(0, 0, Sp.Z - FMath::Max(70.f, Sp.Dp * .42f)), FVector(Sp.W * .6f, Sp.W * .6f, FMath::Max(60.f, Sp.Dp * .38f)), I < 24 ? 2 : 1, 200.f + I + (Sd > 0 ? 0.f : 500.f), -1e6f, .3f);
        if (I % 2 == 0) FeyCloud::Puff(Into, Hub + Rad * Sp.R + FVector(0, 0, Bot + Sp.Dp * .18f), FVector(Sp.W * .75f, Sp.W * .75f, Sp.Dp * .3f), I < 24 ? 2 : 1, 400.f + I, -1e6f, .3f);
    }
    auto* Mesh = Emit(TEXT("FeyTower"), S, Block, false, false); Mesh->bNeverDistanceCull = true; Mesh->SetBoundsScale(2.f);
    auto* Heap = Emit(TEXT("FeyTowerCloud"), Cs, CloudMat, false, false); Heap->bNeverDistanceCull = true; Heap->SetBoundsScale(2.f);
    auto* Foot = Emit(TEXT("FeyCloudStairFoot"), Ct, CloudMat, false, true); Foot->bNeverDistanceCull = true;
}

// The maze as the overlook sees it, from the same hedges (the real one has its collision and planting).
void AFeyMap::BuildStandIn()
{
    // (2026-11-21, the user: make the maze from above look better.) The same rounded hedges as within it, casting their shadows
    // down the corridors in the low sun, which is what draws the maze's pattern from the overlook.
    FMapSurf S, St; const TArray<FHedge>& Hs = MazeData().Hedges;
    for (int32 I = 0; I < Hs.Num(); ++I) { const FHedge& H = Hs[I]; if (H.Kind) St.Box(H.C, H.H, H.Yaw, Runes); else S.Hedge(H.C, H.H, H.Yaw, I * .618f); }
    StandIn = Emit(TEXT("FeyMazeStandIn"), S, LoadObject<UMaterialInterface>(nullptr, MatPaths[MHedge]), false, true);
    StandInStone = Emit(TEXT("FeyMazeStandInStone"), St, LoadObject<UMaterialInterface>(nullptr, MatPaths[MStone]), false, false);
}

// The ground between the Maze Overlook's brink and the maze (2026-11-21, the user: the maze from above). It was the painted
// distance's low, plain ground; now a valley falling from Highmeadow's edge and rising to the maze's, its own land, rocks and
// a few oaks kept off the overlook's line of sight, with the stone stair's ridge across it. Scenery only (out of his reach).
void AFeyMap::BuildBrink()
{
    const FDistrict& Hm = Table[Highmeadow]; const FDistrict& Mz = Table[Maze];
    const float Xa = Mz.X1, Xb = Hm.X0, Y0 = FMath::Max(Hm.Y0, Mz.Y0), Y1 = FMath::Min(Hm.Y1, Mz.Y1);
    TArray<FVector> Way; for (int32 I = 0; I <= 80; ++I) Way.Add(ThresholdAtT(9, I / 80.f));
    auto WayDist = [&](float X, float Y) { float B = 1e9f; for (int32 I = 0; I + 1 < Way.Num(); ++I) B = FMath::Min(B, SegDist(FVector2D(X, Y), FVector2D(Way[I]), FVector2D(Way[I + 1]))); return B; };
    auto Height = [&](float X, float Y)
    {
        const float S = (X - Xa) / (Xb - Xa), Ha = Land(Maze, Xa - 5.f, Y), Hb = Land(Highmeadow, Xb + 5.f, Y);
        return FMath::Lerp(Ha, Hb, Smooth(0.f, 1.f, S)) - 420.f * FMath::Sin(PI * FMath::Clamp(S, 0.f, 1.f)) + 70.f * Noise(X, Y, 900, 77) + 25.f * Noise(X, Y, 230, 78)
            - 300.f * Smooth(Half + 900.f, Half + 300.f, WayDist(X, Y));   // (well under the stair's floor)
    };
    FMapSurf L; constexpr float Step = 100.f;
    const int32 Nx = FMath::CeilToInt((Xb - Xa) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step);
    for (int32 I = 0; I <= Nx; ++I)
        for (int32 J = 0; J <= Ny; ++J)
        {
            const float X = FMath::Min(Xa + I * Step, Xb), Y = FMath::Min(Y0 + J * Step, Y1), E = 50.f, H = Height(X, Y);
            const FVector Nn = FVector(Height(X - E, Y) - Height(X + E, Y), Height(X, Y - E) - Height(X, Y + E), 2 * E).GetSafeNormal();
            const float Moss = FMath::Clamp(.7f + .5f * Noise(X, Y, 500, 21), 0.f, 1.f) * FMath::Lerp(.35f, 1.f, Nn.Z * Nn.Z);
            L.Vert(FVector(X, Y, H), Nn, FVector2D(X, Y) / 300.f, FLinearColor(0.f, Moss, .08f, 1.f).ToFColor(false));
        }
    for (int32 I = 0; I < Nx; ++I) for (int32 J = 0; J < Ny; ++J) { const int32 Q = I * (Ny + 1) + J, R = Q + Ny + 1; L.Quad(Q, Q + 1, R + 1, R); }
    Emit(TEXT("FeyBrink"), L, LoadObject<UMaterialInterface>(nullptr, MatPaths[MGround]), false, false);
    // Rocks along it and a few oaks at its ends (clear of the overlook's view down onto the maze).
    auto Kit = [&](int32 K)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, KitDefs[K].Path); Missing += !Mesh;
        auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); H->SetupAttachment(RootComponent); H->SetMobility(EComponentMobility::Movable);
        H->SetStaticMesh(Mesh); H->SetCastShadow(true); H->SetCollisionEnabled(ECollisionEnabled::NoCollision); H->RegisterComponent(); return H;
    };
    UHierarchicalInstancedStaticMeshComponent* Rocks[6]; for (int32 K = 0; K < 6; ++K) Rocks[K] = Kit(KRock0 + K);
    UHierarchicalInstancedStaticMeshComponent* Oaks[2] = {Kit(KOak0), Kit(KOak1)};
    FRandomStream R(616);
    for (int32 I = 0; I < 70; ++I)
    {
        const float X = R.FRandRange(Xa + 300.f, Xb - 300.f), Y = R.FRandRange(Y0 + 300.f, Y1 - 300.f);
        if (WayDist(X, Y) < Half + 1200.f) continue;
        Rocks[R.RandRange(0, 5)]->AddInstance(FTransform(FRotator(R.FRandRange(-10.f, 10.f), R.FRandRange(0.f, 360.f), R.FRandRange(-10.f, 10.f)), FVector(X, Y, Height(X, Y) - 30.f), FVector(R.FRandRange(.8f, 2.4f))));
    }
    for (int32 I = 0; I < 60; ++I)
    {
        const float X = R.FRandRange(Xa + 600.f, Xb - 600.f), Y = R.FRandRange(Y0 + 600.f, Y1 - 600.f);
        if (WayDist(X, Y) < Half + 2600.f || (Y > 3500.f && Y < 17500.f)) continue;
        Oaks[R.RandRange(0, 1)]->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, Height(X, Y) - 20.f), FVector(R.FRandRange(1.1f, 1.7f))));
    }
    for (int32 K = 0; K < 6; ++K) Rocks[K]->BuildTreeIfOutdated(false, true);
    Oaks[0]->BuildTreeIfOutdated(false, true); Oaks[1]->BuildTreeIfOutdated(false, true);
}

bool AFeyMap::IsLoaded(int32 D) const { return Districts.IsValidIndex(D) && Districts[D] != nullptr; }
bool AFeyMap::IsReady(int32 D) const
{
    if (!IsLoaded(D) || !Districts[D]->IsReady()) return false;
    if (D == Riverbank) { const AFeywildSetting* S = AFeywildSetting::Get(GetWorld()); return S && S->IsSliceLoaded(); }
    return true;
}
int32 AFeyMap::GetLoadedCount() const { int32 N = 0; for (const AFeyDistrict* D : Districts) N += D != nullptr; return N; }
int32 AFeyMap::GetMissing() const { int32 N = Missing; for (const AFeyDistrict* D : Districts) if (D) N += D->Missing; return N; }

// (2026-11-23, the wonder pass; docs/regions/FEYWILD.md#the-wonder-pass) Floating islands with thin falls, glimpsed high over
// Highmeadow, the overlook and the mere, bobbing and turning very slowly; and one giant jellyfish drifting on a wide slow loop
// high over Highmeadow, the Crossroads and the Lily Mere, pulsing (its material moves it), with one soft light under it. None
// collides or casts a shadow; all are always there, like the tower.
float AFeyMap::JellyTime = -1.f;
FVector AFeyMap::JellyAt(float Time)
{
    const float A = Time * (2 * PI / 540.f);   // (once round in nine minutes)
    return FVector(-84000.f + 17000.f * FMath::Cos(A), 12000.f + 13000.f * FMath::Sin(A) + 3000.f * FMath::Sin(2 * A), 9000.f + 900.f * FMath::Sin(A * 3.f));
}
FVector AFeyMap::GetJellyAt() const { return Jelly ? Jelly->GetRelativeLocation() : FVector::ZeroVector; }

void AFeyMap::BuildWonders()
{
    auto Mesh = [&](const TCHAR* Path) { UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path); Missing += !M; return M; };
    auto Part = [&](UStaticMesh* M, const FVector& At, float Yaw, float S)
    {
        auto* C = NewObject<UStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(M);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->bNeverDistanceCull = true;
        C->SetRelativeLocationAndRotation(At, FRotator(0, Yaw, 0)); C->SetRelativeScale3D(FVector(S)); C->RegisterComponent();
        return C;
    };
    UStaticMesh* Isle[3] = {Mesh(TEXT("/Game/Art/Feywild/SM_FeyIsland0.SM_FeyIsland0")), Mesh(TEXT("/Game/Art/Feywild/SM_FeyIsland1.SM_FeyIsland1")), Mesh(TEXT("/Game/Art/Feywild/SM_FeyIsland2.SM_FeyIsland2"))};
    struct FIsle { int32 Kind; FVector At; float Yaw, S; };
    const FIsle Isles_[] = {{0, {-108000, -22000, 11000}, 20.f, 1.6f}, {1, {-95000, 30000, 8800}, 140.f, 1.5f}, {2, {-118000, 26000, 7600}, 260.f, 1.1f},
                            {1, {-64000, 52000, 9500}, 70.f, .9f}, {2, {-56000, 46000, 8200}, 200.f, 1.f}, {0, {-160000, -20000, 14000}, 300.f, 1.3f}, {2, {-30000, -16000, 9000}, 120.f, 1.f}};
    for (const FIsle& I : Isles_) if (Isle[I.Kind]) { Isles.Add(Part(Isle[I.Kind], I.At, I.Yaw, I.S)); IsleAt.Add(FVector(I.At.X, I.At.Y, I.At.Z)); }
    if (UStaticMesh* J = Mesh(TEXT("/Game/Art/Feywild/SM_FeyJelly.SM_FeyJelly")))
    {
        Jelly = Part(J, JellyAt(0.f), 0.f, 2.f);   // (24 m across its bell: a wonder from far below) Jelly->SetTranslucentSortPriority(1);
        JellyLight = NewObject<UPointLightComponent>(this); JellyLight->SetupAttachment(Jelly); JellyLight->SetRelativeLocation(FVector(0, 0, -200.f));
        JellyLight->SetIntensityUnits(ELightUnits::Candelas); JellyLight->SetIntensity(6000.f); JellyLight->SetAttenuationRadius(12000.f); JellyLight->SetLightColor(FLinearColor(.75f, .35f, 1.f));
        JellyLight->SetCastShadows(false); JellyLight->SetSourceRadius(400.f); JellyLight->RegisterComponent();
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WONDERS_SKY isles=%d jelly=%d"), Isles.Num(), Jelly ? 1 : 0);
}

void AFeyMap::Want(int32 D)
{
    if (!Districts.IsValidIndex(D)) return;
    if (D == Riverbank) if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) if (!S->IsSliceLoaded()) S->LoadSlice();
    if (Districts[D]) return;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFeyDistrict* A = GetWorld()->SpawnActor<AFeyDistrict>(FeywildOrigin(), FRotator::ZeroRotator, P);
    A->Begin(D); Districts[D] = A; WantAt[D] = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DISTRICT_WANT name=%s loaded=%d"), Table[D].Name, GetLoadedCount());
}

void AFeyMap::Drop(int32 D)
{
    if (!IsLoaded(D)) return;
    if (D == Riverbank) if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) S->UnloadSlice();
    Districts[D]->Destroy(); Districts[D] = nullptr;
    if (BackTrees.IsValidIndex(D)) BackTrees[D]->SetVisibility(true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DISTRICT_DROPPED name=%s loaded=%d"), Table[D].Name, GetLoadedCount());
}

void AFeyMap::LoadNow(int32 D)
{
    for (int32 I = 0; I < FeyMap::Districts; ++I) if (I != D) Drop(I);
    Want(D);
    if (Districts[D]) Districts[D]->Finish();
    if (BackTrees.IsValidIndex(D)) BackTrees[D]->SetVisibility(false);
    if (StandIn) { StandIn->SetVisibility(!IsReady(Maze)); StandInStone->SetVisibility(!IsReady(Maze)); }
    Current = D;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LOAD_NOW name=%s ready=%d"), Table[D].Name, IsReady(D));
}

void AFeyMap::LoadPair(int32 A, int32 B)
{
    for (int32 I = 0; I < FeyMap::Districts; ++I) if (I != A && I != B) Drop(I);
    for (const int32 D : {A, B})
    {
        Want(D); if (Districts[D]) Districts[D]->Finish();
        if (BackTrees.IsValidIndex(D)) BackTrees[D]->SetVisibility(false);
    }
    if (StandIn) { StandIn->SetVisibility(!IsReady(Maze)); StandInStone->SetVisibility(!IsReady(Maze)); }
    Current = A;
}

int32 FeyMap::SeeThrough(UWorld* World, const AActor* Ignore, int32 Th, float T0, float T1, TArray<FLeak>& Out)
{
    const FWayField& F = WayField(Th);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FeySeeThrough), true); if (Ignore) Q.AddIgnoredActor(Ignore);
    int32 Rays = 0, Leaks = 0, Escapes = 0, Backs = 0, Sides = 0;
    for (float T = T0; T <= T1 + 1e-3f; T += .02f)
    {
        const FVector C = ThresholdAtT(Th, T), Tn = (ThresholdAtT(Th, T + .01f) - ThresholdAtT(Th, T - .01f)).GetSafeNormal2D(), Side(-Tn.Y, Tn.X, 0.f);
        for (const float Across : {-150.f, 0.f, 150.f})
            for (const float Up : {40.f, 160.f})
            {
                FVector O = C + Side * Across; float Gz;
                if (GroundAt(O.X, O.Y, Gz)) O.Z = Gz; O.Z += Up;
                for (int32 Yi = 0; Yi < 36; ++Yi)
                    for (const float Pitch : {-6.f, 0.f, 6.f, 14.f, 25.f})
                    {
                        const FVector Dv = FRotator(Pitch, Yi * 10.f, 0.f).Vector(); ++Rays;
                        // Through anything that does not show (bounds, collision shells), to the first thing that does.
                        float HitAt = 6000.f; const FVector From = FeyWorld(O), To = FeyWorld(O + Dv * 6000.f); FCollisionQueryParams Qr = Q;
                        for (int32 Guard = 0; Guard < 8; ++Guard)
                        {
                            FHitResult Hit;
                            if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Qr)) break;
                            const UPrimitiveComponent* Pc = Hit.GetComponent();
                            if (!Pc || (Pc->IsVisible() && !Pc->bHiddenInGame))
                            {
                                HitAt = FVector::Dist(From, Hit.ImpactPoint);
                                // (2026-11-21) The back of a one-sided surface does not show: the eye sees through it.
                                if (FVector::DotProduct(Hit.ImpactNormal, (To - From).GetSafeNormal()) > .05f && Pc->IsA<UProceduralMeshComponent>())
                                {
                                    ++Leaks; ++Backs; if (Out.Num() < 4000) Out.Add({O, Dv, -2.f - HitAt});
                                    HitAt = -1.f;
                                }
                                break;
                            }
                            Qr.AddIgnoredComponent(Pc);   // (an unseen bound or collision shell: look on past it)
                        }
                        // (Well short of what it struck, and well under the rock: the mesh's triangles and the field's
                        // bilinear surface differ a little on the steep feet of the walls.)
                        if (HitAt < 0.f) continue;
                        for (float S = 80.f; S < HitAt - 150.f; S += 30.f)
                        {
                            const FVector P = O + Dv * S; float Z; bool bPresent;
                            if (!FieldAt(F, P.X, P.Y, Z, bPresent)) break;
                            if (P.Z < Z - 80.f)
                            {
                                ++Leaks; if (Out.Num() < 4000) Out.Add({O, Dv, S});
                                break;
                            }
                        }
                        // (2026-11-21, the user's stills: a slit in a wall with the meadow beyond.) From within the way, a low
                        // sight-line may reach a district only through the way's own mouth.
                        if (T > .03f && T < .97f && HitAt > 0.f)
                        {
                            const FThreshold& Wy = Ways[Th];
                            for (float S = 60.f; S < FMath::Min(HitAt, 6000.f); S += 30.f)
                            {
                                const FVector P = O + Dv * S;
                                if (DistrictAt(P.X, P.Y) < 0) continue;
                                if (P.Z < 900.f + FMath::Max(Wy.ZA, Wy.ZB) && FMath::Min(FVector2D::Distance(FVector2D(P), Wy.PA), FVector2D::Distance(FVector2D(P), Wy.PB)) > Half + 500.f)
                                { ++Leaks; ++Sides; if (Out.Num() < 4000) Out.Add({O, Dv, -3.f - S}); }
                                break;
                            }
                        }
                        // (2026-11-21, the user again: views out of the canyons.) A low ray that strikes nothing and ends
                        // outside every district, in the gaps between them where only the painted distance lies (it has no
                        // collision), is a view out past the rock: also a leak.
                        if (HitAt >= 6000.f && Pitch <= 6.f)
                        {
                            const FVector E = O + Dv * 6000.f;
                            if (DistrictAt(E.X, E.Y) < 0) { ++Leaks; ++Escapes; if (Out.Num() < 4000) Out.Add({O, Dv, -1.f}); }
                        }
                    }
            }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_SEE_THROUGH threshold=%d t=%.2f..%.2f rays=%d leaks=%d escapes=%d backs=%d sides=%d"), Th, T0, T1, Rays, Leaks, Escapes, Backs, Sides);
    return Leaks;
}

void AFeyMap::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    // The islands bob and turn very slowly; the jellyfish drifts on its loop, turning to its way.
    for (int32 I = 0; I < Isles.Num(); ++I)
        Isles[I]->SetRelativeLocationAndRotation(IsleAt[I] + FVector(0, 0, 160.f * FMath::Sin(Clock * .11f + I * 1.7f)), FRotator(1.2f * FMath::Sin(Clock * .07f + I), Clock * (.25f + .05f * I) + I * 51.f, 1.2f * FMath::Cos(Clock * .08f + I * 2.f)));
    if (Jelly)
    {
        const float Tj = JellyTime >= 0.f ? JellyTime : Clock;
        const FVector P = JellyAt(Tj), V = JellyAt(Tj + 1.f) - P;
        Jelly->SetRelativeLocationAndRotation(P, FRotator(FMath::Clamp(-V.Z * .3f, -6.f, 6.f), FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)), 3.f * FMath::Sin(Tj * .2f)));
    }
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || Chuck->IsInFeyRiver() || !InFeywild(Chuck->GetActorLocation())) return;
    const FVector L = FeyLocal(Chuck->GetActorLocation());
    if (Chuck->GetCharacterMovement()->IsMovingOnGround()) FeyReachSpots(Chuck->GetActorLocation());
    TArray<int32, TInlineAllocator<2>> Keep; float Fog = 0;
    const int32 D = DistrictAt(L.X, L.Y);
    float T = 0; const int32 Th = D < 0 ? ThresholdAt(L, T) : -1;
    if (D >= 0) { Current = D; Want(D); Keep.Add(D); }
    else if (Th >= 0)
    {
        const FThreshold& W = Ways[Th];
        const int32 Near = T < .5f ? W.A : W.B, Far = T < .5f ? W.B : W.A;
        Want(Near); Keep.Add(Near); Current = Near;
        // Well past the bend, with the side ahead ready, the side behind goes.
        if (!((T > .7f || T < .3f) && IsReady(Near))) { Want(Far); Keep.Add(Far); }
        // Late (it should not happen): mist thickens toward the end not ready, and that end stays shut until it is.
        LateSeen.SetNum(Thresholds * 2);
        for (int32 E = 0; E < 2; ++E)
        {
            UBoxComponent* B = LateWalls[Th * 2 + E]; const bool bShut = !IsReady(E == 0 ? W.A : W.B), bClose = E == 0 ? T < .35f : T > .65f;
            if ((B->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bShut) B->SetCollisionEnabled(bShut ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            if (bShut && bClose) Fog = 1.f;
            const bool bAtIt = bShut && (E == 0 ? T < .15f : T > .85f);
            if (bAtIt && !LateSeen[Th * 2 + E]) { ++Late; UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_THRESHOLD_LATE threshold=%d t=%.2f district=%s"), Th, T, Table[E == 0 ? W.A : W.B].Name); }
            LateSeen[Th * 2 + E] = bAtIt;
        }
    }
    else return;
    for (int32 I = 0; I < FeyMap::Districts; ++I) if (IsLoaded(I) && !Keep.Contains(I)) Drop(I);
    MaxLoaded = FMath::Max(MaxLoaded, GetLoadedCount());
    for (int32 I = 0; I < FeyMap::Districts; ++I)
    {
        const bool bReady = IsReady(I);
        if (BackTrees.IsValidIndex(I) && BackTrees[I]->IsVisible() == bReady) BackTrees[I]->SetVisibility(!bReady);
        if (bReady && WantAt[I] > 0) { WorstReady = FMath::Max(WorstReady, float(FPlatformTime::Seconds() - WantAt[I])); WantAt[I] = 0; }
    }
    if (StandIn && StandIn->IsVisible() == IsReady(Maze)) { StandIn->SetVisibility(!IsReady(Maze)); StandInStone->SetVisibility(!IsReady(Maze)); }
    // The Deepwood's gloom (2026-11-20): darker and bluer, deeper fog, its own mist, coming on through its thresholds.
    float GloomWant = D == Deepwood ? 1.f : 0.f;
    if (Th >= 0) { const FThreshold& W = Ways[Th]; GloomWant = W.B == Deepwood ? Smooth(.1f, .9f, T) : W.A == Deepwood ? Smooth(.9f, .1f, T) : 0.f; }
    Gloom = FMath::FInterpConstantTo(Gloom, GloomWant, Dt, .5f);
    if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld()))
    {
        S->SetGloom(Gloom);
        const int32 Want = IsReady(Deepwood) ? Deepwood : IsReady(Maze) ? Maze : -1;   // (2026-11-23: the maze's low fog too)
        if (Want != VeilsOf) { VeilsOf = Want; S->SetExtraVeils(Want >= 0 ? Districts[Want]->Veils : TArray<FVector4>()); }
    }
    // The deep water: in over his depth, he is back on the last ground he stood on, with a splash.
    if (D == LilyMere && InDeepWater(L.X, L.Y) && L.Z < -26.f + Chuck->GetSimpleCollisionHalfHeight())
    {
        ++Drowned;
        if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) S->Splash(Chuck->GetActorLocation(), 1.f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DROWNED at=%s back=%s count=%d"), *L.ToCompactString(), *FeyLocal(LastSafe).ToCompactString(), Drowned);
        Chuck->ResetAtLocation(LastSafe);
    }
    else if (Chuck->GetCharacterMovement()->IsMovingOnGround() && !(D == LilyMere && (InDeepWater(L.X, L.Y) || IsPad(L.X, L.Y)))) LastSafe = Chuck->GetActorLocation();
    Mist = FMath::FInterpTo(Mist, Fog, Dt, 1.5f);
    if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) S->SetMistBoost(Mist);
    if (Clock >= ReportAt)
    {
        ReportAt = Clock + 10.f;
        FString Names; for (int32 I = 0; I < FeyMap::Districts; ++I) if (IsLoaded(I)) Names += FString::Printf(TEXT("%s%s%s"), Names.IsEmpty() ? TEXT("") : TEXT(","), Table[I].Name, IsReady(I) ? TEXT("") : TEXT("(loading)"));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_STATE at=%s in=%s threshold=%d loaded=%s mem_mb=%.0f"), *L.ToCompactString(), D >= 0 ? Table[D].Name : TEXT("-"), Th, *Names, FPlatformMemory::GetStats().UsedPhysical / 1048576.);
    }
}
