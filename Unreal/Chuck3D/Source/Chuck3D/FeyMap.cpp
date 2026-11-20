#include "FeyMap.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Algo/Reverse.h"
#include "Async/Async.h"
#include <atomic>
#include "Components/BoxComponent.h"
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
    int32 Mat = 0; bool bSolid = true, bShadow = true;
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
        {Highmeadow, Maze, {-113000, 4000}, {-119000, 4000}, 2000, 0, 1100, StoneStair},
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
        {Highmeadow, 8, 2, {{-89000, 3500}, {-95000, 2500}}}, {Highmeadow, 7, 3, {{-89500, -3000}, {-93500, 1500}, {-95000, 2500}}}, {Highmeadow, 9, 1, {{-107000, 4500}}},
        {Maze, 9, 1, {{-120500, 4600}}},
    };
    // From a district's ring out to a landmark.
    struct FSpur { int32 D, N; FVector2D P[3]; };
    const FSpur Spurs[] = {
        {Deepwood, 2, {{-80000, -37000}, {-82000, -40000}}},     // Redcap Hollow
        {Highmeadow, 2, {{-106000, 9000}, {-111000, 12500}}},    // the Maze Overlook
        {LilyMere, 1, {{-66500, 35000}}},                         // the fen
    };
    const FVector Pools[] = {{-67000, 28000, 1050}, {-59500, 32500, 2300}, {-75500, 35500, 2700}, {-66000, 39300, 1500}, {-57500, 21500, 1500}, {-77000, 23000, 1400}};
    const FVector2D Overlook(-111000, 12500), Redcap(-82000, -40000), Wisteria(-110300, 14800);
    constexpr float MazeR[] = {13000, 10000, 7000, 4000};
    constexpr float MazeOpen[] = {0, 90, 180, 270};
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
    FVector2D MazeAt(float R, float Deg) { const float A = FMath::DegreesToRadians(Deg); return Table[Maze].Hub + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R; }
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
                else if (R.D == Maze) L.Add(MazeAt(MazeR[0] + 900.f, 0.f));
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
        if (D == Riverbank) H = FMath::Lerp(H, Fey::Ground(X, Y), Smooth(-7500.f, -4300.f, X));   // (into the look slice's own land)
        if (OutPath) *OutPath = Pk;
        if (OutWet) *OutWet = Wet;
        return H;
    }

    // ---------------------------------------------------------------- the maze's hedges
    struct FHedge { FVector C, H; float Yaw; int32 Kind; };   // 0 hedge, 1 arch stone
    void MazeHedges(TArray<FHedge>& Out)
    {
        const FVector2D Hub = Table[Maze].Hub;
        auto Opening = [](int32 Ring, float Deg)
        {
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, MazeOpen[Ring])) < 2.6f * 13000.f / MazeR[Ring]) return true;
            if (Ring > 0 && FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, MazeOpen[Ring] + 180.f)) < 2.6f * 13000.f / MazeR[Ring]) return true;   // (a second way through)
            return false;
        };
        for (int32 K = 0; K < UE_ARRAY_COUNT(MazeR); ++K)
        {
            const float R = MazeR[K]; const int32 N = FMath::CeilToInt(2 * PI * R / 380.f);
            for (int32 I = 0; I < N; ++I)
            {
                const float Deg = 360.f * (I + .5f) / N;
                if (Opening(K, Deg)) continue;
                const float Rw = R + 120.f * Noise(Deg * 40.f, K * 1000.f, 900, 71), A = FMath::DegreesToRadians(Deg);
                Out.Add({FVector(Hub.X + Rw * FMath::Cos(A), Hub.Y + Rw * FMath::Sin(A), 330.f), FVector(PI * R / N + 14.f, 65.f, 360.f), Deg + 90.f, 0});
            }
            // A rune arch over each way through.
            for (const float Deg : {MazeOpen[K], K > 0 ? MazeOpen[K] + 180.f : -1000.f})
            {
                if (Deg < -900.f) continue;
                const FVector2D C = MazeAt(R, Deg), Across(-FMath::Sin(FMath::DegreesToRadians(Deg)), FMath::Cos(FMath::DegreesToRadians(Deg)));
                for (const float S : {-1.f, 1.f}) { const FVector2D P = C + Across * S * 290.f; Out.Add({FVector(P.X, P.Y, 300.f), FVector(55.f, 55.f, 300.f), Deg, 1}); }
                Out.Add({FVector(C.X, C.Y, 640.f), FVector(55.f, 350.f, 45.f), Deg, 1});
            }
            // Behind each way in, a hedge across the corridor makes the other way round a dead end.
            if (K + 1 < UE_ARRAY_COUNT(MazeR))
            {
                const float Deg = MazeOpen[K] - 35.f, Rm = (MazeR[K] + MazeR[K + 1]) * .5f, Dr = (MazeR[K] - MazeR[K + 1]) * .5f;
                const FVector2D C = MazeAt(Rm, Deg);
                Out.Add({FVector(C.X, C.Y, 330.f), FVector(Dr + 20.f, 65.f, 360.f), Deg, 0});
            }
        }
    }
    TArray<FVector2D> MazeRoute()
    {
        TArray<FVector2D> P; P.Add(MazeAt(MazeR[0] + 900.f, 0.f));
        for (int32 K = 0; K + 1 < UE_ARRAY_COUNT(MazeR); ++K)
        {
            const float Rm = (MazeR[K] + MazeR[K + 1]) * .5f;
            P.Add(MazeAt(Rm, MazeOpen[K]));
            for (float Deg = MazeOpen[K] + 10.f; Deg < MazeOpen[K + 1]; Deg += 10.f) P.Add(MazeAt(Rm, Deg));
            P.Add(MazeAt(Rm, MazeOpen[K + 1]));
        }
        P.Add(MazeAt(2200.f, MazeOpen[3]));
        return P;
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
    };
    enum EKit { KOak0, KOak1, KCol0, KCol1, KAmber, KBlue, KViolet, KFern0, KBroad = 11, KCrimson, KFox, KCel0, KCel1, KGrass0, KGrass1, KPetals, KRock0, KPad = 25, KLily, KBell, KitCount };
    static_assert(UE_ARRAY_COUNT(KitDefs) == KitCount, "a definition for every kit");
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround"), TEXT("/Game/Art/Feywild/M_FeyWater.M_FeyWater"), TEXT("/Game/Art/Feywild/M_FeyRiver.M_FeyRiver"),
                               TEXT("/Game/Art/Feywild/M_FeyBlock.M_FeyBlock"), TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot")};
    enum EMat { MGround, MWater, MRiver, MBlock, MRoot, MatCount };
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
    if (D == Maze) { const FVector2D P = MazeAt(MazeR[0] + 1300.f, 4.f); return FVector(P.X, P.Y, Land(D, P.X, P.Y)); }
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
        if (D == Maze) { TArray<FVector2D> L = MazeRoute(); Algo::Reverse(L); return L; }
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
    for (const FVector2D& Q : P) Out.Add(FVector(Q.X, Q.Y, Land(D, Q.X, Q.Y)));
    return Out;
}

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
        // ------------------------------ blockout: arches, the maze's hedges, statues, the cloud stair's foot
        {
            FMapSurf& Bk = Plan.Surfs.AddDefaulted_GetRef(); Bk.Mat = MBlock;
            auto Arch = [&](const FVector2D& C, float Yaw, float Z)
            {
                const float A = FMath::DegreesToRadians(Yaw); const FVector2D Across(-FMath::Sin(A), FMath::Cos(A));
                for (const float S : {-1.f, 1.f}) { const FVector2D P = C + Across * S * 300.f; Bk.Box(FVector(P.X, P.Y, Z + 250.f), FVector(60.f, 60.f, 300.f), Yaw, Stone); }
                Bk.Box(FVector(C.X, C.Y, Z + 590.f), FVector(60.f, 370.f, 50.f), Yaw, Stone);
            };
            if (D == Crossroads)
                for (const FMouth& M : Data().Mouths[D]) { const FVector2D P = M.P + M.In * 1000.f; Arch(P, FMath::RadiansToDegrees(FMath::Atan2(M.In.Y, M.In.X)), Land(D, P.X, P.Y)); }   // (on the straight run in)
            if (D == Maze)
            {
                TArray<FHedge> Hedges; MazeHedges(Hedges);
                for (const FHedge& Hd : Hedges) Bk.Box(Hd.C + FVector(0, 0, Land(D, Hd.C.X, Hd.C.Y) - 4.f), Hd.H, Hd.Yaw, Hd.Kind ? Stone : HedgeGreen);
                const FVector2D Gate = MazeAt(MazeR[0] + 700.f, 0.f); Arch(Gate, 0.f, Land(D, Gate.X, Gate.Y));
                // Mossy statues along the corridors.
                FRandomStream Rs(900);
                for (int32 I = 0; I < 10; ++I)
                {
                    const int32 K = Rs.RandRange(0, 2); const float Rm = (MazeR[K] + MazeR[K + 1]) * .5f + Rs.FRandRange(-900.f, 900.f), Deg = Rs.FRandRange(0.f, 360.f);
                    const FVector2D P = MazeAt(Rm, Deg); const float Z = Land(D, P.X, P.Y);
                    if (FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, MazeOpen[K])) < 14.f || FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, MazeOpen[K + 1])) < 14.f) continue;
                    Bk.Box(FVector(P.X, P.Y, Z + 40.f), FVector(70.f, 70.f, 40.f), Deg, Grey);
                    Bk.Box(FVector(P.X, P.Y, Z + 170.f), FVector(32.f, 42.f, 90.f), Deg + 20.f, Grey);
                    Bk.Box(FVector(P.X, P.Y, Z + 285.f), FVector(24.f, 24.f, 25.f), Deg + 35.f, Grey);
                }
                // The Cloud Staircase's first steps out of the maze's heart (stage 7 builds the stair; here it stops).
                for (int32 I = 0; I < 8; ++I)
                {
                    const FVector2D P = MazeAt(1300.f, 200.f + 22.f * I); const float Top = 34.f * (I + 1);
                    Bk.Box(FVector(P.X, P.Y, Top * .5f - 10.f), FVector(150.f, 95.f, Top * .5f + 10.f), 200.f + 22.f * I + 90.f, Cloud);
                }
                const FVector2D End = MazeAt(1300.f, 200.f + 22.f * 8.5f);
                Plan.Walls.Add({FVector(End.X, End.Y, 1500.f), FVector(40.f, 200.f, 1300.f), 200.f + 22.f * 8.5f + 90.f});
            }
            if (D == Deepwood)   // Redcap Hollow: a ring of stones round a cold fire pit
                for (int32 I = 0; I < 9; ++I)
                {
                    const float A = 2 * PI * I / 9; const FVector2D P = Redcap + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 160.f;
                    Bk.Box(FVector(P.X, P.Y, Land(D, P.X, P.Y) + 8.f), FVector(22.f, 16.f, 12.f), FMath::RadiansToDegrees(A), Grey);
                }
            if (Bk.V.IsEmpty()) Plan.Surfs.Pop();
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
            if (D == Maze && MazeRadius(X, Y) < (Clear > 300.f ? 16500.f : 14200.f)) return false;   // (trees clear of the outer hedge)
            if (D == Highmeadow && FVector2D::Distance(FVector2D(X, Y), Overlook) < 1200.f) return false;
            if (D == Deepwood && FVector2D::Distance(FVector2D(X, Y), Redcap) < 2200.f && Clear > 300.f) return false;
            return true;
        };
        auto Oak = [&](float X, float Y, float S, float Gap)
        {
            for (const FVector& O : Oaks) if (FVector2D::DistSquared(FVector2D(X, Y), FVector2D(O)) < FMath::Square(Gap * FMath::Max(S, O.Z) * .5f + Gap * .5f)) return false;
            const int32 Kind = R.RandRange(0, 1); const float Yaw = R.FRandRange(0.f, 360.f);
            const FTransform Xf(FRotator(0, Yaw, 0), FVector(X, Y, Land(D, X, Y) - 14.f), FVector(S));
            Plan.Inst[Kind ? KOak1 : KOak0].Add(Xf); Plan.Inst[Kind ? KCol1 : KCol0].Add(Xf); Oaks.Add(FVector(X, Y, S));
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
            const float S0 = D == Deepwood ? 1.6f : 1.15f, S1 = D == Deepwood ? 2.4f : 1.8f;
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
                Oak(P.X, P.Y, R.FRandRange(S0, S1), 700.f);
            }
        }
        const float OaksPerHa = D == Deepwood ? 42.f : D == Riverbank ? 12.f : D == Highmeadow ? 4.f : D == Maze ? 14.f : D == Orchard ? 4.f : 8.f;
        for (int32 I = 0, N = FMath::RoundToInt(OaksPerHa * Ha * 1.6f); I < N; ++I)
        {
            const float X = R.FRandRange(Ds.X0, Ds.X1), Y = R.FRandRange(Ds.Y0, Ds.Y1);
            const float S = D == Deepwood ? R.FRandRange(1.5f, 2.4f) : R.FRandRange(1.f, 1.6f);
            if (!Free(X, Y, 350.f + 420.f * S) || Edge(X, Y) < 1800.f) continue;   // (its roots and low limbs clear of the path)
            Oak(X, Y, S, D == Deepwood ? 900.f : 1300.f);
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
                    Plan.Inst[KOak1].Add(Xf); Plan.Inst[KCol1].Add(Xf); Oaks.Add(FVector(Px, Py, S));
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
            for (int32 K = 0; K < UE_ARRAY_COUNT(MazeR); ++K)
                for (int32 I = 0, N = FMath::RoundToInt(MazeR[K] / 60.f); I < N; ++I)
                {
                    const float Deg = Rf.FRandRange(0.f, 360.f), Side = Rf.RandRange(0, 1) ? 1.f : -1.f; const FVector2D P = MazeAt(MazeR[K] + Side * Rf.FRandRange(110.f, 200.f), Deg);
                    Put(Rf.FRand() < .5f ? KBlue : KFern0 + Rf.RandRange(0, 3), P.X, P.Y, Rf.FRandRange(.8f, 1.8f), Rf.FRandRange(0.f, 360.f), 1.f, 5.f);
                }
            break;
        }
        default: break;
        }
        for (int32 I = 0; I < 4; ++I) Scatter(KFern0 + I, D == Deepwood ? 620.f : 360.f, 1.f, 1.9f, 2.f, 8.f, 170.f, Clumps(620, I + 3 + D));
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
            Mesh->SetCastShadow(S.bShadow);
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
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_BUILT seconds=%.2f triangles=%d missing=%d"), FPlatformTime::Seconds() - T0, Map->Triangles, Map->Missing);
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
void AFeyMap::BuildThresholds()
{
    UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, MatPaths[MGround]), *Root = LoadObject<UMaterialInterface>(nullptr, MatPaths[MRoot]);
    for (int32 Th = 0; Th < Thresholds; ++Th)
    {
        const FThreshold& W = Ways[Th]; const FVector2D Dd = Dir(Th);
        FMapSurf Floor, Walls; constexpr int32 Steps = 56;
        const bool bTube = W.Style == RootTunnel;
        // The cross-section, left to right: (across, up) from the floor's middle.
        TArray<FVector2D> Prof;
        if (bTube) Prof = {{-Half, 0}, {-Half - 70, 170}, {-Half + 10, 360}, {-130, 500}, {0, 530}, {130, 500}, {Half - 10, 360}, {Half + 70, 170}, {Half, 0}, {-Half, 0}};   // (closed by its floor)
        else Prof = {{-Half - 560, 1150}, {-Half - 140, 300}, {-Half, 0}, {Half, 0}, {Half + 140, 300}, {Half + 560, 1150}};
        auto At = [&](float T, float Across, float Up, float& OutZ)
        {
            const FVector C = ThresholdAtT(Th, T);
            const FVector Tn = (ThresholdAtT(Th, T + .01f) - ThresholdAtT(Th, T - .01f)).GetSafeNormal2D(); const FVector Side(-Tn.Y, Tn.X, 0);
            const float Rough = bTube ? 0.f : (FMath::Abs(Across) > Half + 1.f ? 160.f * Noise(T * 4000.f, Across, 700, Th + 30) : 0.f);
            OutZ = C.Z - 4.f;
            return C + Side * (Across + (Across > 0 ? Rough * .3f : -Rough * .3f)) + FVector(0, 0, Up + (Up > 400.f ? Rough : 0.f) - 4.f);
        };
        for (int32 K = 0; K < Steps; ++K)
        {
            const float T0 = FMath::Lerp(-.18f, 1.18f, float(K) / Steps), T1 = FMath::Lerp(-.18f, 1.18f, float(K + 1) / Steps);
            const float Light = bTube ? FMath::Lerp(.45f, 1.f, FMath::Max(Smooth(.25f, -.12f, T0), Smooth(.75f, 1.12f, T0))) : 1.f;
            for (int32 M = 0; M + 1 < Prof.Num(); ++M)
            {
                float Z;
                const FVector A = At(T0, Prof[M].X, Prof[M].Y, Z), B = At(T0, Prof[M + 1].X, Prof[M + 1].Y, Z), Cc = At(T1, Prof[M + 1].X, Prof[M + 1].Y, Z), D = At(T1, Prof[M].X, Prof[M].Y, Z);
                const bool bFloor = Prof[M].Y == 0 && Prof[M + 1].Y == 0;
                const FVector Mid = (A + B + Cc + D) * .25f, Axis = ThresholdAtT(Th, (T0 + T1) * .5f) + FVector(0, 0, 260.f);
                FVector In = Axis - Mid; In.Normalize();
                const FVector Nn = bFloor ? FVector::UpVector : FVector::CrossProduct(B - A, D - A).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(B - A, D - A), In) > 0 ? 1.f : -1.f);
                if (bFloor) Floor.Face(A, B, Cc, D, Nn, FLinearColor(1.f, .2f, .3f, 1.f).ToFColor(false), 300.f);
                else
                {
                    const FColor Col = bTube ? FLinearColor(.4f, Light, 0, 1).ToFColor(false) : FLinearColor(0, .45f, .1f, 1).ToFColor(false);
                    Walls.Face(A, B, Cc, D, Nn, Col, 300.f);
                    if (!bTube) Walls.Face(A, B, Cc, D, -Nn, Col, 300.f);   // (and its back, seen from beyond the rim)
                }
            }
        }
        Emit(*FString::Printf(TEXT("FeyThresholdFloor%d"), Th), Floor, Ground, true, true);
        Emit(*FString::Printf(TEXT("FeyThresholdWalls%d"), Th), Walls, bTube ? Root : Ground, true, true);
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
    TArray<FHedge> Hedges; MazeHedges(Hedges);
    FMapSurf S; for (const FHedge& H : Hedges) S.Box(H.C, H.H, H.Yaw, H.Kind ? Stone : HedgeGreen);
    StandIn = Emit(TEXT("FeyMazeStandIn"), S, LoadObject<UMaterialInterface>(nullptr, MatPaths[MBlock]), false, false);
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
    if (StandIn) StandIn->SetVisibility(!IsReady(Maze));
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
    if (StandIn && StandIn->IsVisible() == IsReady(Maze)) StandIn->SetVisibility(!IsReady(Maze));
    Mist = FMath::FInterpTo(Mist, Fog, Dt, 1.5f);
    if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) S->SetMistBoost(Mist);
    if (Clock >= ReportAt)
    {
        ReportAt = Clock + 10.f;
        FString Names; for (int32 I = 0; I < FeyMap::Districts; ++I) if (IsLoaded(I)) Names += FString::Printf(TEXT("%s%s%s"), Names.IsEmpty() ? TEXT("") : TEXT(","), Table[I].Name, IsReady(I) ? TEXT("") : TEXT("(loading)"));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_STATE at=%s in=%s threshold=%d loaded=%s mem_mb=%.0f"), *L.ToCompactString(), D >= 0 ? Table[D].Name : TEXT("-"), Th, *Names, FPlatformMemory::GetStats().UsedPhysical / 1048576.);
    }
}
