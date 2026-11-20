#include "FeyMap.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Algo/Reverse.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include <atomic>
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
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

// The Feywild in rough form (Claude, 2026-11-19; FeyMap.h, docs/regions/FEYWILD.md stage 3).
using namespace FeyMap;

struct FMapSurf
{
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan; TArray<FColor> C;
    int32 Mat = 0; bool bSolid = true, bShadow = true, bHidden = false;
    void TriFacing(int32 A, int32 B, int32 Cc, const FVector& Nn) { if (FVector::DotProduct(FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]), Nn) > 0.f) T.Append({A, Cc, B}); else T.Append({A, B, Cc}); }
    int32 Vert(const FVector& P, const FVector& Nn, const FVector2D& Uv, const FColor& Col)
    {
        V.Add(P); N.Add(Nn); UV.Add(Uv); C.Add(Col);
        Tan.Add(FProcMeshTangent(FVector::CrossProduct(FMath::Abs(Nn.Z) > .95f ? FVector::RightVector : FVector::UpVector, Nn).GetSafeNormal(), false));
        return V.Num() - 1;
    }
    void Quad(int32 A, int32 B, int32 Cc, int32 D) { T.Append({A, B, Cc, A, Cc, D}); }
    // A quad facing Nn whatever order its corners come in (the engine's front faces wind clockwise seen from in front).
    void Face(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Nn, const FColor& Col, float UvScale = 200.f)
    {
        const FVector Tu = FVector::CrossProduct(FMath::Abs(Nn.Z) > .9f ? FVector::RightVector : FVector::UpVector, Nn).GetSafeNormal(), Tv = FVector::CrossProduct(Nn, Tu);
        auto Uv = [&](const FVector& P) { return FVector2D(FVector::DotProduct(P, Tu), FVector::DotProduct(P, Tv)) / UvScale; };
        const int32 I0 = Vert(A, Nn, Uv(A), Col), I1 = Vert(B, Nn, Uv(B), Col), I2 = Vert(Cc, Nn, Uv(Cc), Col), I3 = Vert(D, Nn, Uv(D), Col);
        if (FVector::DotProduct(FVector::CrossProduct(B - A, Cc - A), Nn) > 0.f) Quad(I0, I3, I2, I1); else Quad(I0, I1, I2, I3);
    }
    void Box(const FVector& Ce, const FVector& H, float Yaw, const FColor& Col)
    {
        const FQuat Q(FRotator(0.f, Yaw, 0.f));
        auto P = [&](float X, float Y, float Z) { return Ce + Q.RotateVector(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
        const FVector Xa = Q.RotateVector(FVector::ForwardVector), Ya = Q.RotateVector(FVector::RightVector);
        Face(P(-1, -1, 1), P(-1, 1, 1), P(1, 1, 1), P(1, -1, 1), FVector::UpVector, Col);
        Face(P(1, -1, -1), P(1, -1, 1), P(1, 1, 1), P(1, 1, -1), Xa, Col);
        Face(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -Xa, Col);
        Face(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), Ya, Col);
        Face(P(-1, -1, -1), P(-1, -1, 1), P(1, -1, 1), P(1, -1, -1), -Ya, Col);
    }
    // A surface of revolution from a profile given top to bottom (radius, height).
    void Lathe(const FVector& Ce, TArrayView<const FVector2D> Profile, int32 Segs, const FColor& Col, float Squash = 1.f)
    {
        for (int32 I = 0; I + 1 < Profile.Num(); ++I)
            for (int32 J = 0; J < Segs; ++J)
            {
                const float A0 = 2 * PI * J / Segs, A1 = 2 * PI * (J + 1) / Segs, Am = (A0 + A1) * .5f;
                auto P = [&](const FVector2D& Rz, float A) { return Ce + FVector(Rz.X * FMath::Cos(A), Rz.X * FMath::Sin(A) * Squash, Rz.Y); };
                const FVector2D& U = Profile[I]; const FVector2D& L = Profile[I + 1];
                const float Dr = L.X - U.X, Dz = L.Y - U.Y;
                FVector Nn(FMath::Cos(Am) * -Dz, FMath::Sin(Am) * -Dz, Dr); if (!Nn.Normalize()) Nn = FVector(FMath::Cos(Am), FMath::Sin(Am), 0);
                if (U.X < 1.f && L.X < 1.f) continue;
                Face(P(U, A0), P(U, A1), P(L, A1), P(L, A0), Nn, Col, 600.f);
            }
    }
};

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
        case Deepwood: H += 80.f * Noise(X, Y, 1100, 50) + 40.f * Noise(X, Y, 330, 51); break;
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
        H += (900.f + 500.f * Noise(X, Y, 1300, D + 80)) * Smooth(1900.f, 0.f, E) * (1.f - Notch) * (1.f - Open);
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
    struct FHedge { FVector C, H; float Yaw; int32 Kind; };   // 0 hedge, 1 stone (arch, statue)
    constexpr float MazeR0 = 1800.f, MazePitch = 380.f, MazeRmax = 13000.f, HedgeHalf = 60.f, HedgeTall = 840.f, MazeGap = 300.f;
    struct FMazeData { TArray<FHedge> Hedges; TArray<FVector2D> Route; TArray<FVector2D> DeadEnds; int32 Cells = 0; float Gate = 0; };
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
            TArray<TPair<int32, int32>> Front; for (int32 B : Adj[Gate]) Front.Add({Gate, B});
            while (Front.Num())
            {
                const int32 K = R.RandRange(0, Front.Num() - 1); const TPair<int32, int32> E = Front[K]; Front.RemoveAtSwap(K);
                if (In[E.Value]) continue;
                In[E.Value] = true; Links.Add(Key(E.Key, E.Value));
                for (int32 B : Adj[E.Value]) if (!In[B]) Front.Add({E.Value, B});
            }
            for (int32 C = 0; C < Total; ++C) for (int32 B : Adj[C]) if (B > C && R.FRand() < .035f) Links.Add(Key(C, B));   // (loops)
            const int32 Heart = Id(0, R.RandRange(0, N[0] - 1));
            // The shortest way from the gate to the heart.
            TArray<int32> From; From.Init(-1, Total); TArray<int32> Q; Q.Add(Gate); From[Gate] = Gate;
            for (int32 H = 0; H < Q.Num(); ++H) for (int32 B : Adj[Q[H]]) if (From[B] < 0 && Links.Contains(Key(Q[H], B))) { From[B] = Q[H]; Q.Add(B); }
            TArray<int32> Way; for (int32 C = Heart; ; C = From[C]) { Way.Insert(C, 0); if (C == Gate) break; }
            auto Mid = [&](int32 C) { const int32 I = Ring(C); return MazeAt(MazeR0 + (I + .5f) * MazePitch, (A0(C) + A1(C)) * .5f); };
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
            const float HeartA = (A0(Heart) + A1(Heart)) * .5f;
            Out.Route.Add(MazeAt(MazeR0, HeartA)); Out.Route.Add(MazeAt(MazeR0 - 700.f, HeartA));
            // The hedges: each ring's boundary in arcs, open where two cells are linked; the walls between neighbours.
            auto Arc = [&](float Rb, float Lo, float Hi)
            {
                const int32 Segs = FMath::Max(1, FMath::CeilToInt((Hi - Lo) * Rb / 300.f));
                for (int32 K = 0; K < Segs; ++K)
                {
                    const float A = FMath::Lerp(Lo, Hi, (K + .5f) / Segs), Len = (Hi - Lo) * Rb / Segs;
                    const FVector2D P = MazeAt(Rb, A);
                    Out.Hedges.Add({FVector(P.X, P.Y, HedgeTall * .5f), FVector(Len * .5f + 6.f, HedgeHalf, HedgeTall * .5f), FMath::RadiansToDegrees(A) + 90.f, 0});
                }
            };
            auto Open = [&](float Rb, float Lo, float Hi)   // a hedge over [Lo, Hi] with a way through its middle
            {
                const float Mid = (Lo + Hi) * .5f, HalfA = MazeGap * .5f / Rb;
                Arc(Rb, Lo, Mid - HalfA); Arc(Rb, Mid + HalfA, Hi);
            };
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
                        if (Links.Contains(Key(C, O))) Open(Rb, Lo, Hi); else Arc(Rb, Lo, Hi);
                    }
                }
            }
            for (int32 I = 0; I < Rings; ++I)
                for (int32 J = 0; J < N[I]; ++J)
                {
                    if (Links.Contains(Key(Id(I, J), Id(I, J + 1)))) continue;
                    const float A = A1(Id(I, J)); const FVector2D P = MazeAt(MazeR0 + (I + .5f) * MazePitch, A);
                    Out.Hedges.Add({FVector(P.X, P.Y, HedgeTall * .5f), FVector(MazePitch * .5f + HedgeHalf, HedgeHalf, HedgeTall * .5f), FMath::RadiansToDegrees(A), 0});
                }
            // Stone arches at the gate and the heart; dead ends (one way in) for the statues.
            for (const FVector2D& Arch : {FVector2D(MazeRmax, Out.Gate), FVector2D(MazeR0, HeartA)})
            {
                const float A = Arch.Y, D = FMath::RadiansToDegrees(A); const FVector2D C = MazeAt(Arch.X, A), Across(-FMath::Sin(A), FMath::Cos(A));
                for (const float Sd : {-1.f, 1.f}) { const FVector2D P = C + Across * Sd * 200.f; Out.Hedges.Add({FVector(P.X, P.Y, 470.f), FVector(70.f, 70.f, 470.f), D, 1}); }
                Out.Hedges.Add({FVector(C.X, C.Y, 990.f), FVector(70.f, 270.f, 60.f), D, 1});
            }
            for (int32 C = 0; C < Total; ++C) { int32 Exits = 0; for (int32 B : Adj[C]) Exits += Links.Contains(Key(C, B)); if (Exits == 1 && C != Gate && C != Heart) Out.DeadEnds.Add(Mid(C)); }
            Out.Cells = Total;
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
    };
    enum EKit { KOak0, KOak1, KCol0, KCol1, KAmber, KBlue, KViolet, KFern0, KBroad = 11, KCrimson, KFox, KCel0, KCel1, KGrass0, KGrass1, KPetals, KRock0, KPad = 25, KLily, KBell,
                KDeep0, KDeep1, KDCol0, KDCol1, KMoss, KPadBig, KitCount };
    static_assert(UE_ARRAY_COUNT(KitDefs) == KitCount, "a definition for every kit");
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround"), TEXT("/Game/Art/Feywild/M_FeyWater.M_FeyWater"), TEXT("/Game/Art/Feywild/M_FeyRiver.M_FeyRiver"),
                               TEXT("/Game/Art/Feywild/M_FeyBlock.M_FeyBlock"), TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot"), TEXT("/Game/Art/Feywild/M_FeyStone.M_FeyStone"),
                               TEXT("/Game/Art/Feywild/M_FeyHedge.M_FeyHedge"), TEXT("/Game/Art/Feywild/M_FeySign.M_FeySign")};
    enum EMat { MGround, MWater, MRiver, MBlock, MRoot, MStone, MHedge, MSign, MatCount };
    // Stone's vertex colour: R a seed, B 1 for glowing runes.
    const FColor Runes(80, 0, 255, 255), Plain(140, 0, 0, 255);
    const FColor Stone(198, 192, 178), HedgeGreen(34, 64, 24), Cloud(236, 240, 248), RoofBlue(38, 62, 150), Grey(146, 150, 146);

    int32 MouthNear(int32 D, const FVector2D& P, float R)
    {
        for (const FMouth& M : Data().Mouths[D]) if (FVector2D::Distance(P, M.P) < R) return M.Th;
        return -1;
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

bool FeyMap::IsPad(float X, float Y) { for (const FVector& P : MoatPads()) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(P)) < FMath::Square(P.Z)) return true; return false; }
bool FeyMap::InDeepWater(float X, float Y) { return MoatK(X, Y) > .5f; }
int32 FeyMap::MazeCells() { return MazeData().Cells; }
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
    return FeyCheckpointWorld(D < 0 ? Riverbank : D);
}

bool IsFeyCheckpoint(const FVector& World, float& OutYaw)
{
    for (int32 D = 0; D < Districts; ++D) if (FVector::Dist(World, FeyCheckpointWorld(D)) < 1.f) { OutYaw = CheckpointYaw(D); return true; }
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
        constexpr float Tile = 3200.f; constexpr int32 Cells = 26;
        const int32 Tx = FMath::CeilToInt((Ds.X1 - Ds.X0) / Tile), Ty = FMath::CeilToInt((Ds.Y1 - Ds.Y0) / Tile);
        const float Gloom = D == Deepwood ? .62f : 1.f;
        for (int32 I = 0; I < Tx; ++I)
            for (int32 J = 0; J < Ty; ++J)
            {
                FMapSurf& L = Plan.Surfs.AddDefaulted_GetRef(); L.Mat = MGround;
                const float X0 = Ds.X0 + I * Tile, Y0 = Ds.Y0 + J * Tile, X1 = FMath::Min(X0 + Tile, Ds.X1), Y1 = FMath::Min(Y0 + Tile, Ds.Y1);
                for (int32 A = 0; A <= Cells; ++A)
                    for (int32 B = 0; B <= Cells; ++B)
                    {
                        const float X = FMath::Lerp(X0, X1, float(A) / Cells), Y = FMath::Lerp(Y0, Y1, float(B) / Cells), E = 40.f;
                        float Pk, Wet; const float H = Land(D, X, Y, &Pk, &Wet);
                        const FVector Nn = FVector(Land(D, X - E, Y) - Land(D, X + E, Y), Land(D, X, Y - E) - Land(D, X, Y + E), 2 * E).GetSafeNormal();
                        const float Moss = FMath::Clamp(.75f - Pk * 1.3f + .5f * Noise(X, Y, 500, 21) + (D == Deepwood ? .25f : 0.f), 0.f, 1.f) * FMath::Lerp(.35f, 1.f, Nn.Z * Nn.Z);
                        L.Vert(FVector(X, Y, H), Nn, FVector2D(X, Y) / 300.f, FLinearColor(Pk, Moss, FMath::Max(Wet, Pk * .4f), Gloom).ToFColor(false));
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
            Plan.Surfs.Reserve(Plan.Surfs.Num() + 16);   // (references into it are held below)
            FMapSurf& St = Plan.Surfs.AddDefaulted_GetRef(); St.Mat = MStone;
            FMapSurf& Sg = Plan.Surfs.AddDefaulted_GetRef(); Sg.Mat = MSign;
            FMapSurf& Cl = Plan.Surfs.AddDefaulted_GetRef(); Cl.Mat = MBlock;
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
                // The hedges in eight parts (each its own mesh and collision, put in place one a frame).
                const FMazeData& Mz = MazeData(); TArray<FMapSurf*> Parts;
                for (int32 K = 0; K < 8; ++K) { FMapSurf& H = Plan.Surfs.AddDefaulted_GetRef(); H.Mat = MHedge; Parts.Add(&H); }
                for (const FHedge& Hd : Mz.Hedges)
                {
                    const FVector C = Hd.C + FVector(0, 0, Land(D, Hd.C.X, Hd.C.Y) - 6.f);
                    if (Hd.Kind) { St.Box(C, Hd.H, Hd.Yaw, Runes); continue; }
                    const float A = FMath::Atan2(Hd.C.Y - Table[Maze].Hub.Y, Hd.C.X - Table[Maze].Hub.X) + PI;
                    Parts[FMath::Clamp(int32(A / (2 * PI) * 8.f), 0, 7)]->Box(C, Hd.H, Hd.Yaw, FColor::White);
                }
                // Mossy statues standing in some of the dead ends.
                for (int32 I = 0; I < Mz.DeadEnds.Num(); I += FMath::Max(1, Mz.DeadEnds.Num() / 14))
                {
                    const FVector2D P = Mz.DeadEnds[I]; const float Z = Land(D, P.X, P.Y), Yaw = float(I * 37 % 360);
                    St.Box(FVector(P.X, P.Y, Z + 40.f), FVector(60.f, 60.f, 40.f), Yaw, Plain);
                    St.Box(FVector(P.X, P.Y, Z + 175.f), FVector(28.f, 38.f, 95.f), Yaw + 20.f, Plain);
                    St.Box(FVector(P.X, P.Y, Z + 295.f), FVector(22.f, 22.f, 25.f), Yaw + 35.f, Plain);
                }
                // The Cloud Staircase's first steps out of the maze's heart (stage 7 builds the stair; here it stops).
                for (int32 I = 0; I < 8; ++I)
                {
                    const float A = FMath::DegreesToRadians(200.f + 22.f * I); const FVector2D P = MazeAt(1100.f, A); const float Top = 34.f * (I + 1);
                    Cl.Box(FVector(P.X, P.Y, Top * .5f - 10.f), FVector(150.f, 95.f, Top * .5f + 10.f), 200.f + 22.f * I + 90.f, Cloud);
                }
                const float Ae = FMath::DegreesToRadians(200.f + 22.f * 8.5f); const FVector2D End = MazeAt(1100.f, Ae);
                Plan.Walls.Add({FVector(End.X, End.Y, 1500.f), FVector(40.f, 200.f, 1300.f), FMath::RadiansToDegrees(Ae) + 90.f});
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
                        if (Rl.FRand() < .55f) Plan.Lights.Add(FVector4(P.X, P.Y, Land(D, P.X, P.Y) + Rl.FRandRange(40.f, 160.f), Rl.FRandRange(45.f, 80.f)));
                    }
            for (int32 I = 0; I < 90; ++I)
            {
                const float X = Rl.FRandRange(Ds.X0 + 1500.f, Ds.X1 - 1500.f), Y = Rl.FRandRange(Ds.Y0 + 1500.f, Ds.Y1 - 1500.f);
                Plan.Veils.Add(FVector4(X, Y, Land(D, X, Y) + Rl.FRandRange(60.f, 320.f), Rl.FRandRange(700.f, 1600.f)));
            }
        }

        // ------------------------------ planting
        FRandomStream R(1000 + D);
        const float Ha = (Ds.X1 - Ds.X0) * (Ds.Y1 - Ds.Y0) / 1e8f;
        TArray<FVector> Oaks;   // (X, Y, scale)
        auto Put = [&](int32 K, float X, float Y, float S, float Yaw, float Sink, float Tilt)
        {
            Plan.Inst[K].Add(FTransform(FRotator(R.FRandRange(-Tilt, Tilt), Yaw, R.FRandRange(-Tilt, Tilt)), FVector(X, Y, Land(D, X, Y) - Sink), FVector(S)));
        };
        auto Edge = [&](float X, float Y) { return D == Riverbank ? FMath::Min(X - Ds.X0, FMath::Min(Y - Ds.Y0, Ds.Y1 - Y)) : FMath::Min(FMath::Min(X - Ds.X0, Ds.X1 - X), FMath::Min(Y - Ds.Y0, Ds.Y1 - Y)); };
        auto Free = [&](float X, float Y, float Clear)
        {
            if (Edge(X, Y) < 120.f || PathDist(D, X, Y) < Clear || MouthNear(D, FVector2D(X, Y), 1600.f) >= 0) return false;
            float Wet; Land(D, X, Y, nullptr, &Wet); if (Wet > .3f) return false;
            if (D == Crossroads && FVector2D::Distance(FVector2D(X, Y), Ds.Hub) < 2600.f) return false;
            if (D == Maze && MazeRadius(X, Y) < (Clear > 300.f ? 15200.f : 13700.f)) return false;   // (trees clear of the outer hedge)
            if (D == Highmeadow && FVector2D::Distance(FVector2D(X, Y), Overlook) < 1200.f) return false;
            if (D == Deepwood && FVector2D::Distance(FVector2D(X, Y), Redcap) < 2200.f && Clear > 300.f) return false;
            return true;
        };
        auto Oak = [&](float X, float Y, float S, float Gap)
        {
            for (const FVector& O : Oaks) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(O)) < FMath::Square(Gap * FMath::Max(S, O.Z) * .5f + Gap * .5f)) return false;
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
            const FTransform Xf(FRotator(0, 110.f, 0), FVector(Wisteria.X, Wisteria.Y, Land(D, Wisteria.X, Wisteria.Y) - 30.f), FVector(3.2f));   // (the great wisteria's stand-in)
            Plan.Inst[KOak1].Add(Xf); Plan.Inst[KCol1].Add(Xf);
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
        for (const FKitDef& K : KitDefs)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, K.Path); Missing += !Mesh;
            auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); H->SetupAttachment(RootComponent);
            H->SetMobility(EComponentMobility::Movable); H->SetStaticMesh(Mesh); H->SetCastShadow(K.bShadow);
            H->InstanceStartCullDistance = int32(K.C0); H->InstanceEndCullDistance = int32(K.C1); H->WorldPositionOffsetDisableDistance = 4500;
            H->SetCollisionEnabled(K.bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            if (K.bSolid) H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetHiddenInGame(K.bInvisible); H->bAutoRebuildTreeOnInstanceChanges = false;
            H->RegisterComponent(); Kits.Add(H);
        }
        if (FPlatformTime::Seconds() > Until) return false;
    }
    while (FPlatformTime::Seconds() < Until)
    {
        if (Stage == 0)   // the surfaces, one each time round
        {
            if (Item >= Plan->Surfs.Num()) { Stage = 1; Item = 0; continue; }
            FMapSurf& S = Plan->Surfs[Item++];
            auto* Mesh = NewObject<UProceduralMeshComponent>(this); Mesh->SetupAttachment(RootComponent);
            Mesh->bUseComplexAsSimpleCollision = true; Mesh->bUseAsyncCooking = false; Mesh->RegisterComponent();
            Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, S.bSolid);
            Mesh->SetMaterial(0, Mats.IsValidIndex(S.Mat) ? Mats[S.Mat] : nullptr);
            Mesh->SetCollisionEnabled(S.bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            if (S.bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            Mesh->SetCastShadow(S.bShadow); Mesh->SetHiddenInGame(S.bHidden);
            Triangles += S.T.Num() / 3;
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
                L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(float(Lt.W)); L->SetLightColor(FLinearColor(.28f, .55f, 1.f)); L->SetAttenuationRadius(1100.f);
                L->SetCastShadows(false); L->SetVolumetricScatteringIntensity(2.f); L->RegisterComponent();
            }
            Veils = Plan->Veils;
            Stage = 3;
        }
        else
        {
            for (UHierarchicalInstancedStaticMeshComponent* H : Kits) if (H->GetInstanceCount() && !H->IsTreeFullyBuilt()) return false;
            bReady = true; BuildSeconds = float(FPlatformTime::Seconds() - Started); Plan.Reset();
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DISTRICT_READY name=%s seconds=%.2f plan=%.2f worst_step_ms=%.1f triangles=%d planted=%d missing=%d mem_mb=%.0f"),
                Table[District].Name, BuildSeconds, PlanSeconds, WorstStepMs, Triangles, Planted, Missing, FPlatformMemory::GetStats().UsedPhysical / 1048576.);
            return true;
        }
    }
    return false;
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
    Map->Districts.SetNum(FeyMap::Districts);
    Map->BuildThresholds(); Map->BuildDistance(); Map->BuildTower(); Map->BuildStandIn();
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
        FMapSurf Mass, Tube; constexpr int32 Steps = 72; constexpr float Ta = -.24f, Tb = 1.24f;
        const bool bTube = W.Style == RootTunnel;
        auto Frame = [&](float T, FVector& C, FVector& Side)
        {
            C = ThresholdAtT(Th, T) - FVector(0, 0, 4.f);
            const FVector Tn = (ThresholdAtT(Th, T + .01f) - ThresholdAtT(Th, T - .01f)).GetSafeNormal2D(); Side = FVector(-Tn.Y, Tn.X, 0);
        };
        // How much of the masses stands: all of them in the gap, sinking to nothing a little way past each mouth.
        auto Rise = [](float T) { return FMath::Min(Smooth(-.24f, -.02f, T), Smooth(1.24f, 1.02f, T)); };
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
            const TArray<FVector2D> Prof = bTube ? TArray<FVector2D>{{Half, 0}, {Half + 40, 0}, {Half + 130, 590}, {Half + 600, 720}, {Half + 1150, 380}, {Half + 1650, -250}}
                                                 : TArray<FVector2D>{{Half, 0}, {Half + 140, 300}, {Half + 520, 1150}, {Half + 950, 1230}, {Half + 1450, 650}, {Half + 1950, -250}};
            auto Mass2 = [&](float D) -> float { if (D <= Prof[0].X) return 0.f; for (int32 I = 0; I + 1 < Prof.Num(); ++I) if (D <= Prof[I + 1].X) return FMath::Lerp(Prof[I].Y, Prof[I + 1].Y, (D - Prof[I].X) / (Prof[I + 1].X - Prof[I].X)); return Prof.Last().Y; };
            TArray<FVector> Curve; TArray<float> CurveT; FBox2D Box(ForceInit);
            for (int32 I = 0; I <= 500; ++I) { const float T = FMath::Lerp(Ta, Tb, I / 500.f); Curve.Add(ThresholdAtT(Th, T)); CurveT.Add(T); Box += FVector2D(Curve.Last()); }
            Box = Box.ExpandBy(Prof.Last().X + 250.f);
            auto InRect = [](float X, float Y, int32& Which) { for (int32 D = 0; D < FeyMap::Districts; ++D) { const FDistrict& Ds = Table[D]; if (X > Ds.X0 + 1.f && X < Ds.X1 - 1.f && Y > Ds.Y0 + 1.f && Y < Ds.Y1 - 1.f) { Which = D; return true; } } Which = -1; return false; };
            constexpr float Step = 60.f; const int32 Nx = FMath::CeilToInt(Box.GetSize().X / Step), Ny = FMath::CeilToInt(Box.GetSize().Y / Step);
            TArray<float> H; TArray<uint8> Skip, IsFloor; H.SetNum((Nx + 1) * (Ny + 1)); Skip.SetNum(H.Num()); IsFloor.SetNum(H.Num());
            ParallelFor(Nx + 1, [&](int32 I)
            {
                for (int32 J = 0; J <= Ny; ++J)
                {
                    const float X = Box.Min.X + I * Step, Y = Box.Min.Y + J * Step; float Best = 1e12f; int32 Bi = 0;
                    for (int32 K = 0; K < Curve.Num(); K += 4) { const float Dd = FVector::DistSquared2D(Curve[K], FVector(X, Y, 0)); if (Dd < Best) { Best = Dd; Bi = K; } }
                    for (int32 K = FMath::Max(0, Bi - 4); K <= FMath::Min(Curve.Num() - 1, Bi + 4); ++K) { const float Dd = FVector::DistSquared2D(Curve[K], FVector(X, Y, 0)); if (Dd < Best) { Best = Dd; Bi = K; } }
                    const float D = FMath::Sqrt(Best), T = CurveT[Bi], Fz = Curve[Bi].Z - 4.f, M = Mass2(D), Up = M > 0.f ? M * Rise(T) : M;
                    const int32 Ix = I * (Ny + 1) + J; int32 Which; const bool bIn = InRect(X, Y, Which);
                    IsFloor[Ix] = D <= Half + 20.f;
                    if (bIn) { Skip[Ix] = IsFloor[Ix] || Up < 2.f; H[Ix] = FMath::Max(Fz + Up, Land(Which, X, Y) + 3.f); }
                    else { Skip[Ix] = 0; H[Ix] = Fz + Up; }
                }
            });
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
            Strip(Mass, {{-Half - 140, 600}, {0, 650}, {Half + 140, 600}}, 1.f, -.18f, 1.18f, false, false, Mossy);
            const TArray<FVector2D> Ring = {{Half, 0}, {Half + 70, 170}, {Half - 10, 360}, {130, 500}, {0, 530}, {-130, 500}, {-Half + 10, 360}, {-Half - 70, 170}, {-Half, 0}};
            Strip(Tube, Ring, 1.f, -.18f, 1.18f, false, true, [](float U, int32) { return FLinearColor(.4f, FMath::Lerp(.45f, 1.f, FMath::Max(Smooth(.25f, -.12f, U), Smooth(.75f, 1.12f, U))), 0, 1).ToFColor(false); });
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
    FMapSurf S; const FVector Hub(Table[Maze].Hub.X, Table[Maze].Hub.Y, 0.f);
    constexpr float Base = 38000.f;
    auto Blob = [&](const FVector& C, float Rx, float Rz, float Squash)
    {
        TArray<FVector2D> P; for (int32 I = 0; I <= 8; ++I) { const float Ph = PI * I / 8; P.Add(FVector2D(Rx * FMath::Sin(Ph), Rz * FMath::Cos(Ph))); }
        S.Lathe(C, P, 14, Cloud, Squash);
    };
    FRandomStream R(52);
    Blob(Hub + FVector(0, 0, Base), 6200.f, 1500.f, .8f);
    for (int32 I = 0; I < 11; ++I) { const float A = R.FRandRange(0.f, 2 * PI), Rr = R.FRandRange(2500.f, 5600.f); Blob(Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A) * .8f, Base + R.FRandRange(-700.f, 500.f)), R.FRandRange(1600.f, 3200.f), R.FRandRange(700.f, 1300.f), R.FRandRange(.7f, 1.f)); }
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
    // The stair: lumps of cloud winding up from the maze's heart (unlit and plain: it does not glow).
    for (int32 I = 0; I < 70; ++I)
    {
        const float U = I / 69.f, A = FMath::DegreesToRadians(200.f) + U * 2 * PI * 3.3f, Rr = FMath::Lerp(1300.f, 4200.f, FMath::Sqrt(U));
        Blob(Hub + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), FMath::Lerp(400.f, Base - 900.f, U)), FMath::Lerp(260.f, 900.f, U), FMath::Lerp(90.f, 300.f, U), 1.f);
    }
    auto* Mesh = Emit(TEXT("FeyTower"), S, Block, false, false); Mesh->bNeverDistanceCull = true; Mesh->SetBoundsScale(2.f);
}

// The maze as the overlook sees it, from the same hedges (the real one has its collision and planting).
void AFeyMap::BuildStandIn()
{
    FMapSurf S, St; for (const FHedge& H : MazeData().Hedges) (H.Kind ? St : S).Box(H.C, H.H, H.Yaw, H.Kind ? Runes : FColor::White);
    StandIn = Emit(TEXT("FeyMazeStandIn"), S, LoadObject<UMaterialInterface>(nullptr, MatPaths[MHedge]), false, false);
    StandInStone = Emit(TEXT("FeyMazeStandInStone"), St, LoadObject<UMaterialInterface>(nullptr, MatPaths[MStone]), false, false);
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

void AFeyMap::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || Chuck->IsInFeyRiver() || !InFeywild(Chuck->GetActorLocation())) return;
    const FVector L = FeyLocal(Chuck->GetActorLocation());
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
        const bool bWant = IsReady(Deepwood);
        if (bWant != bVeils) { bVeils = bWant; S->SetExtraVeils(bWant ? Districts[Deepwood]->Veils : TArray<FVector4>()); }
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
