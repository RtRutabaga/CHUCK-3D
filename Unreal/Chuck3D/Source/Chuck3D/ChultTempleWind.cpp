#include "ChultTempleWind.h"

// The Astral wind's ruptures (Claude, 2026-10-22; docs/CHULT-TEMPLE-WIND.md): the sewer's natural
// tears (the jungle wreck's outline: a pointed, ragged, slightly bent slit), torn through the
// tunnels' floors. Not traps: each lies at its own angle, off the middle, and there is always a way
// round it (the interior check walks every one). Smaller in the narrow legs, larger in the bends.
namespace
{
    using namespace TempleWind;
    // Segment (0-3 the legs, 4-6 the bends), along it from where he enters (U), across it from its middle
    // (V, + toward +S in a leg, +T in a bend), length, greatest half-width, turn from its axis (degrees).
    struct FTearSpot { int32 Seg; float U, V, Len, Wid, Turn; };
    // Distances along a leg from its start to its pit's ends.
    float LegStart(int32 L) { return Legs[L].Dir > 0 ? Legs[L].T0 : Legs[L].T1; }
    float PitEnd(int32 L) { return FMath::Abs(Pits[L].To - LegStart(L)); }
    TArray<FTearSpot> Spots()
    {
        return {
            // The legs: before each pit, one by a wall (he runs up to the lip along the other); past it, 5 m clear
            // where he lands (2-4 m beyond the far lip, by the wall of his last run), then one by the other wall.
            // Leg 1: a slalom of two before the pit; he lands by the +S wall.
            {0, 300, -32, 150, 19, 5}, {0, 560, 32, 140, 18, -6}, {0, PitEnd(0) + 520, -32, 120, 16, 7},
            // Leg 2: he lands by the -S wall.
            {1, 300, 32, 140, 18, -5}, {1, PitEnd(1) + 500, 32, 130, 17, -5},
            // Leg 3: its pit is started on the +S wall, so the tear before it is by the -S wall; he lands by the +S wall.
            {2, 260, -32, 150, 19, -5}, {2, PitEnd(2) + 500, -32, 130, 17, -6},
            // Leg 4: one before the pit; the way off its far side onto the gallery is clear.
            {3, 240, -32, 130, 17, 6},
            // Bend 1: a slalom across its width.
            {4, 330, 62, 240, 34, 18}, {4, 650, -60, 300, 40, -15}, {4, 950, 20, 150, 26, 65}, {4, 1260, -64, 280, 36, 12}, {4, 1570, 60, 220, 32, -22},
            // Bend 2: more, one lying across its middle.
            {5, 320, -55, 260, 38, -14}, {5, 600, 64, 200, 30, 20}, {5, 860, -12, 170, 26, 74}, {5, 1150, 58, 320, 42, -10}, {5, 1420, -62, 240, 34, 16}, {5, 1640, 34, 130, 20, -40},
            // Bend 3: the densest, one long tear down its middle.
            {6, 300, 60, 230, 34, -20}, {6, 580, -58, 280, 38, 12}, {6, 860, 62, 200, 30, 8}, {6, 1150, -10, 280, 40, 5}, {6, 1450, 62, 180, 28, -18}};
    }
    bool Inside(const TArray<FVector2D>& Poly, const FVector2D& P)
    {
        bool In = false;
        for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
        {
            const FVector2D& A = Poly[I]; const FVector2D& B = Poly[J];
            if (((A.Y > P.Y) != (B.Y > P.Y)) && (P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)) In = !In;
        }
        return In;
    }
    float EdgeDistance(const TArray<FVector2D>& Poly, const FVector2D& P)
    {
        float Best = 1e9f;
        for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
        {
            const FVector2D A = Poly[J], B = Poly[I], D = B - A;
            const float L = D.SizeSquared(), U = L > 0 ? FMath::Clamp(float(FVector2D::DotProduct(P - A, D) / L), 0.f, 1.f) : 0.f;
            Best = FMath::Min(Best, float(FVector2D::Distance(P, A + D * U)));
        }
        return Best;
    }
}

const TArray<FTempleWindTear>& ChultTempleWindTears()
{
    static const TArray<FTempleWindTear> List = []
    {
        TArray<FTempleWindTear> Out;
        constexpr int32 Side = 18;
        int32 K = 0;
        for (const FTearSpot& Sp : Spots())
        {
            const bool bLeg = Sp.Seg < 4;
            const FSeg& G = bLeg ? Legs[Sp.Seg] : Bends[Sp.Seg - 4];
            if (Sp.U + Sp.Len * .5f > (G.Axis == 1 ? G.T1 - G.T0 : G.S1 - G.S0) - 60) continue;   // (never past its end)
            FVector2D Along, Across, C;
            if (G.Axis == 1)
            {
                Along = FVector2D(0, G.Dir); Across = FVector2D(1, 0);
                C = FVector2D((G.S0 + G.S1) * .5f + Sp.V, (G.Dir > 0 ? G.T0 : G.T1) + G.Dir * Sp.U);
            }
            else
            {
                Along = FVector2D(G.Dir, 0); Across = FVector2D(0, 1);
                C = FVector2D((G.Dir > 0 ? G.S0 : G.S1) + G.Dir * Sp.U, (G.T0 + G.T1) * .5f + Sp.V);
            }
            const float A = FMath::DegreesToRadians(Sp.Turn);
            const FVector2D Dir = Along * FMath::Cos(A) + Across * FMath::Sin(A), Perp(-Dir.Y, Dir.X);
            FTempleWindTear T; T.C = C; T.Seg = Sp.Seg;
            // The sewer's rupture edge (as ChultWreck): a pointed tear, ragged and a little bent.
            for (int32 S = 0; S < 2; ++S) for (int32 I = 0; I <= Side; ++I)
            {
                if (S == 1 && (I == 0 || I == Side)) continue;
                const float U = S == 0 ? I / float(Side) : 1 - I / float(Side), Sg = S == 0 ? -1.f : 1.f;
                const float Env = FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * U)), .65f);
                const float W = 5 + Env * Sp.Wid * (1 + .15f * FMath::Sin(U * 13 + Sg * 2.3f + K) + .1f * FMath::Sin(U * 29 + Sg + K * 1.7f));
                const float Bend = Sp.Len * .08f * FMath::Sin(PI * U) * FMath::Sin(K * 2.1f + .6f);
                T.Edge.Add(C + Dir * ((U - .5f) * Sp.Len) + Perp * (Bend + Sg * W));
            }
            T.Mid = (T.Edge[Side / 2] + T.Edge[Side + Side / 2]) * .5f;
            T.Box.Init();
            for (const FVector2D& P : T.Edge) { T.Box += P; T.Radius = FMath::Max(T.Radius, float(FVector2D::Distance(P, C))); }
            Out.Add(MoveTemp(T)); ++K;
        }
        return Out;
    }();
    return List;
}

bool ChultTempleWindTearAt(const FVector2D& F, float Grow)
{
    for (const FTempleWindTear& T : ChultTempleWindTears())
    {
        if (!T.Box.ExpandBy(Grow + 1).IsInside(F)) continue;
        if (Inside(T.Edge, F) || (Grow > 0 && EdgeDistance(T.Edge, F) < Grow)) return true;
    }
    return false;
}

// ---- The sanctum's broken floor (2026-10-24).
const TArray<FBox2D>& ChultTempleSanctumIslands()
{
    static const TArray<FBox2D> List = []
    {
        TArray<FBox2D> Out;
        // Ragged round slabs (the cellar's cheese island's kind): 1.2 m round each of the gallery's six piers (their bases
        // 104 cm square at GalleryT..+100; the closest pair stands 2.8 m apart), 2.3 m round the colonnades' first pair (242 cm).
        auto Round = [&](float S, float T, float R) { Out.Add(FBox2D(FVector2D(S - R, T - R), FVector2D(S + R, T + R))); };
        for (float S : {77550.f, 78150.f, 78430.f, 78970.f, 79250.f, 79850.f}) Round(S, GalleryT + 50, 120);
        for (float Sg : {-1.f, 1.f}) Round(HallMid + Sg * NaveHalf, PierT[0], 230);
        return Out;
    }();
    return List;
}

float ChultTempleSanctumIslandRadius(int32 I, float A)
{
    const float R = static_cast<float>(ChultTempleSanctumIslands()[I].GetExtent().X), C = I * 1.7f;
    return R * (1.f + .07f * FMath::Sin(5.f * A + C) + .04f * FMath::Sin(9.f * A + C * 2.3f) + .025f * FMath::Sin(17.f * A + C));
}

float ChultTempleSanctumSeaRadius(float A)
{
    return SeaR + 7.f * FMath::Sin(5.f * A) + 5.f * FMath::Sin(11.f * A + 1.f) + 3.f * FMath::Sin(3.f * A + 2.f);
}

namespace
{
    float PoolEdgeT(float S) { return PoolT - PoolJag * .5f + PoolJag * .5f * (.6f * FMath::Sin(S * .0113f) + .4f * FMath::Sin(S * .031f + 1.3f)); }
    // On island I (Grow shrinks it).
    bool OnIsland(int32 I, const FVector2D& P, float Grow)
    {
        const FVector2D D = P - ChultTempleSanctumIslands()[I].GetCenter();
        return D.Size() < ChultTempleSanctumIslandRadius(I, FMath::Atan2(float(D.Y), float(D.X))) - Grow;
    }
}

int32 ChultTempleSanctumHoleAt(const FVector2D& F, float Grow)
{
    if (F.X > PoolS0 - Grow && F.X < PoolS1 + Grow && F.Y < HT1 + Grow && F.Y > PoolEdgeT(F.X) - Grow)
    {
        bool Island = false;
        for (int32 I = 0; I < ChultTempleSanctumIslands().Num(); ++I) Island |= OnIsland(I, F, Grow);
        if (!Island) return 1;
    }
    for (const FSanctumRift& R : SanctumRifts)
    {
        const float DX = (F.X - R.S) / (R.RS + Grow), DY = (F.Y - R.T) / (R.RT + Grow);
        const float Jag = 1.f + .14f * FMath::Sin(6.f * FMath::Atan2(DY, DX) + R.S * .1f);
        if (FMath::Sqrt(DX * DX + DY * DY) * Jag < 1.f) return 1;
    }
    const FVector2D D = F - FVector2D(SeaS, SeaT);
    if (D.Size() < ChultTempleSanctumSeaRadius(FMath::Atan2(float(D.Y), float(D.X))) + Grow) return 2;
    return 0;
}

bool ChultTempleWindPath(const FVector2D& From, const FVector2D& To, float Clear, TArray<FVector2D>& Out)
{
    // A 10 cm grid of the tunnels' floors (the exit passage, legs and bends; never a pit or a rupture),
    // a cell free where a disc of Clear round it stands on floor; searched 8-way from From to To within
    // 4 m of the box round them, then thinned to the corners of the line.
    using namespace TempleWind;
    Out.Reset();
    TArray<FBox2D> Floors = {FBox2D(FVector2D(TempleRun::FS0, TempleRun::AnteT1 - 300), FVector2D(TempleRun::FS1, ExitT1))};
    for (const FSeg& G : Legs) Floors.Add(FBox2D(FVector2D(G.S0, G.T0), FVector2D(G.S1, G.T1)));
    for (const FSeg& G : Bends) Floors.Add(FBox2D(FVector2D(G.S0, G.T0), FVector2D(G.S1, G.T1)));
    TArray<FBox2D> PitBoxes;
    for (int32 L = 0; L < 4; ++L) PitBoxes.Add(FBox2D(FVector2D(Legs[L].S0, FMath::Min(Pits[L].From, Pits[L].To)), FVector2D(Legs[L].S1, FMath::Max(Pits[L].From, Pits[L].To))));
    FBox2D Area(ForceInit); for (const FBox2D& B : Floors) Area += B;
    const int32 NX = FMath::CeilToInt((Area.Max.X - Area.Min.X) / 10), NY = FMath::CeilToInt((Area.Max.Y - Area.Min.Y) / 10);
    auto Centre = [&](int32 X, int32 Y) { return Area.Min + FVector2D(X * 10 + 5, Y * 10 + 5); };
    auto FreeAt = [&](const FVector2D& C)
    {
        for (int32 K = 0; K < 9; ++K)
        {
            const FVector2D P = K == 8 ? C : C + FVector2D(FMath::Cos(K * PI / 4), FMath::Sin(K * PI / 4)) * Clear;
            bool On = false; for (const FBox2D& B : Floors) On |= B.IsInside(P);
            if (!On) return false;
            for (const FBox2D& B : PitBoxes) if (B.IsInside(P)) return false;
        }
        return !ChultTempleWindTearAt(C, Clear);
    };
    auto CellOf = [&](const FVector2D& P) { return FIntPoint(FMath::Clamp(FMath::FloorToInt((P.X - Area.Min.X) / 10), 0, NX - 1), FMath::Clamp(FMath::FloorToInt((P.Y - Area.Min.Y) / 10), 0, NY - 1)); };
    FBox2D Window(ForceInit); Window += From; Window += To; Window = Window.ExpandBy(900);
    TMap<int32, bool> FreeCache;
    auto IsFree = [&](const FIntPoint& C)
    {
        const int32 Id = C.X * NY + C.Y;
        if (const bool* F = FreeCache.Find(Id)) return *F;
        const FVector2D P = Centre(C.X, C.Y);
        const bool F = Window.IsInside(P) && FreeAt(P);
        FreeCache.Add(Id, F); return F;
    };
    // (Ends a little off the free cells, by a wall or a lip, start from the nearest free one.)
    auto Nearest = [&](FIntPoint C)
    {
        for (int32 R = 0; R <= 4; ++R) for (int32 DX = -R; DX <= R; ++DX) for (int32 DY = -R; DY <= R; ++DY)
        {
            const FIntPoint N(C.X + DX, C.Y + DY);
            if (N.X >= 0 && N.Y >= 0 && N.X < NX && N.Y < NY && IsFree(N)) return N;
        }
        return FIntPoint(-1, -1);
    };
    const FIntPoint A = Nearest(CellOf(From)), B = Nearest(CellOf(To));
    if (A.X < 0 || B.X < 0) return false;
    TMap<int32, int32> Prev; TArray<FIntPoint> Open = {A}; Prev.Add(A.X * NY + A.Y, -1);
    bool Found = A == B;
    for (int32 I = 0; I < Open.Num() && !Found; ++I)
    {
        const FIntPoint C = Open[I];
        for (int32 DX = -1; DX <= 1 && !Found; ++DX) for (int32 DY = -1; DY <= 1; ++DY)
        {
            if (!DX && !DY) continue;
            const FIntPoint N(C.X + DX, C.Y + DY);
            if (N.X < 0 || N.Y < 0 || N.X >= NX || N.Y >= NY || Prev.Contains(N.X * NY + N.Y) || !IsFree(N)) continue;
            if (DX && DY && (!IsFree(FIntPoint(C.X + DX, C.Y)) || !IsFree(FIntPoint(C.X, C.Y + DY)))) continue;
            Prev.Add(N.X * NY + N.Y, C.X * NY + C.Y); Open.Add(N);
            if (N == B) { Found = true; break; }
        }
    }
    if (!Found) return false;
    TArray<FVector2D> Cells;
    for (int32 Id = B.X * NY + B.Y; Id >= 0; Id = Prev[Id]) Cells.Insert(Centre(Id / NY, Id % NY), 0);
    auto Sees = [&](const FVector2D& P, const FVector2D& Q)
    {
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(P, Q) / 5));
        for (int32 K = 0; K <= Steps; ++K) if (!IsFree(CellOf(FMath::Lerp(P, Q, K / float(Steps))))) return false;
        return true;
    };
    Out.Add(Cells[0]);
    for (int32 I = 1; I < Cells.Num(); ++I)
        if (I == Cells.Num() - 1 || !Sees(Out.Last(), Cells[I + 1])) Out.Add(Cells[I]);
    return true;
}
