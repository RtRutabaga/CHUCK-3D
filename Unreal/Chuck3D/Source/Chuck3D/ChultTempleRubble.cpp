#include "ChultTempleRubble.h"
#include "ChultTempleInterior.h"
#include "Misc/ScopeExit.h"

// The rubble map's layout (Claude, 2026-10-26; ChultTempleRubble.h, docs/CHULT-TEMPLE-RUBBLE.md). Since 2026-11-04 the
// hall is the sanctum fallen in: its own fittings (piers, plinths, braziers, the dais, the stairs, the gallery's standing
// runs) are kept clear or built on; the gallery's fallen middle, the stairs' chokes and the toppled piers are laid by
// hand; the rest of the fall is scattered in heaps by a seeded stream, the first under the vault's holes, kept off
// everything that must stay clear (the lane and a margin, the ruptures' rims, the fittings, the crevice's mouth, where
// he comes to).
namespace
{
    using namespace TempleRubble;
    namespace TW = TempleWind;

    float SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D D = B - A;
        const float L2 = static_cast<float>(D.SizeSquared());
        const float U = L2 > 0 ? FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - A, D)) / L2, 0.f, 1.f) : 0.f;
        return static_cast<float>(FVector2D::Distance(P, A + D * U));
    }

    // The sanctum's braziers (along the processional) and great fire bowls (before the dais), as its builder sets them.
    const TArray<FVector2D> Braziers = {{TW::HallMid - 420, -5080}, {TW::HallMid + 420, -5080}, {TW::HallMid - 420, -5975}, {TW::HallMid + 420, -5975}};
    const TArray<FVector2D> GreatBraziers = {{TW::HallMid - 600, TW::DaisT1 + 260}, {TW::HallMid + 600, TW::DaisT1 + 260}, {TW::HallMid - 1450, TW::DaisT1 + 420}, {TW::HallMid + 1450, TW::DaisT1 + 420}};
    const TArray<FVector2D> Jars = {{80270, -6600}, {77330, -6560}};

    // The hall's fittings as frame boxes the rubble keeps off: the piers' bases, the wall pilasters, the guardian idols'
    // plinths, the altar and the colossi's plinths on the dais, the dais's front steps, the stairs and their masonry
    // (only the chokes are laid on them), the pool's whole end (the gallery's fall is laid by hand).
    const TArray<FBox2D>& Fittings()
    {
        static TArray<FBox2D> Out;
        if (Out.Num()) return Out;
        auto Add = [&](float S, float T, float HS, float HT) { Out.Add(FBox2D(FVector2D(S - HS, T - HT), FVector2D(S + HS, T + HT))); };
        for (float Sg : {-1.f, 1.f})
        {
            for (float T : TW::PierT) Add(TW::HallMid + Sg * TW::NaveHalf, T, TW::PierHalf + 26, TW::PierHalf + 26);
            for (float T : TW::PierT) if (T < TW::StairFoot - 150) Add(Sg < 0 ? RS0 : RS1, T, 72, 98);
            for (float T : {-5250.f, -5650.f}) Add(TW::HallMid + Sg * 420, T, 60, 60);
            Add(TW::HallMid + Sg * 760, TW::DaisT0 + 330, 162, 162);
        }
        Add(TW::HallMid, TW::DaisT1 - 560, 302, 162);
        Out.Add(FBox2D(FVector2D(TW::DaisS0, TW::DaisT1), FVector2D(TW::DaisS1, TW::DaisT1 + 190)));
        Out.Add(FBox2D(FVector2D(RS0, TW::StairFoot - 80), FVector2D(RS0 + TW::StairW + 15, RT1)));
        Out.Add(FBox2D(FVector2D(RS1 - TW::StairW - 15, TW::StairFoot - 80), FVector2D(RS1, RT1)));
        Out.Add(FBox2D(FVector2D(RS0, TW::PoolT - TW::PoolJag - 60), FVector2D(RS1, RT1)));
        return Out;
    }

    bool Reserved(const FVector2D& P, float Radius)
    {
        for (const FVector2D& B : Braziers) if (FVector2D::Distance(P, B) < Radius + 70) return true;
        for (const FVector2D& B : GreatBraziers) if (FVector2D::Distance(P, B) < Radius + 120) return true;
        for (const FVector2D& J : Jars) if (FVector2D::Distance(P, J) < Radius + 50) return true;
        for (const FBox2D& F : Fittings()) if (F.ExpandBy(Radius).IsInside(P)) return true;
        if (P.X < PileS1 + 200 + Radius && FMath::Abs(P.Y - CrawlT) < 300 + Radius) return true;   // the crevice's pile and mouth
        if (FVector2D::Distance(P, FVector2D(Lane[0].S, Lane[0].T)) < 300 + Radius) return true;   // where he comes to
        // The way from the lane's first leg to the rupture past it (the rift-fall check walks it).
        if (SegmentDistance(P, FVector2D(Lane[1].S, Lane[1].T), FVector2D(FallRiftS, FallRiftT)) < LaneHalf + 35 + Radius) return true;
        return false;
    }

    // Is a block's footprint (centre, horizontal radius) clear of the lane, the ruptures and the reserved spots?
    bool Clear(const FVector2D& C, float Radius)
    {
        if (C.X < RS0 + 20 || C.X > RS1 - 20 || C.Y < RT0 + 20 || C.Y > RT1 - 20) return false;
        return ChultTempleRubbleLaneDistance(C) > LaneHalf + 35 + Radius && !ChultTempleRubbleHoleAt(C, Radius + 25) && !Reserved(C, Radius);
    }

    struct FRubble { TArray<FTempleRubbleBlock> Blocks; TArray<FTempleRubbleDrum> Drums; };
    bool GPileWall = false;   // while the crevice's pile is laid: the stair's face is the wall it leans on

    // What is under a point: the hall's own masonry (ChultTempleRubbleBaseAt), a drum's top, or the top face of a block
    // already down (its tilted plane, over its yaw-turned footprint). Every piece is set down onto this (user 2026-10-26:
    // "some of the rubble is impossible levitating blocks, make it natural").
    float TopAt(const FRubble& Out, const FVector2D& In, const FTempleRubbleBlock* Skip = nullptr)
    {
        // Past a wall, what is against the wall carries on (a block leans on the wall; it does not hang over the void).
        FVector2D P(FMath::Clamp(In.X, RS0 + 1.f, RS1 - 1.f), FMath::Clamp(In.Y, RT0 + 1.f, RT1 - 1.f));
        if (GPileWall && P.X < MouthS + 1) P.X = MouthS + 1;   // (by the crevice the stair's face is that wall)
        float Top = ChultTempleRubbleBaseAt(P);
        for (const FTempleRubbleDrum& D : Out.Drums)
        {
            const FVector2D A(D.A), B(D.B);
            const float Dist = SegmentDistance(P, A, B);
            if (Dist < D.Radius) Top = FMath::Max(Top, D.A.Z + FMath::Sqrt(D.Radius * D.Radius - Dist * Dist));
        }
        for (const FTempleRubbleBlock& K : Out.Blocks)
        {
            if (&K == Skip) continue;
            const FVector2D Off = P - FVector2D(K.Centre);
            const float Yaw = FMath::DegreesToRadians(K.Turn.Yaw);
            const float LX = Off.X * FMath::Cos(Yaw) + Off.Y * FMath::Sin(Yaw), LY = -Off.X * FMath::Sin(Yaw) + Off.Y * FMath::Cos(Yaw);
            if (FMath::Abs(LX) > K.Half.X * .95f || FMath::Abs(LY) > K.Half.Y * .95f) continue;
            const FVector N = K.Turn.RotateVector(FVector::UpVector);
            if (N.Z < .3f) continue;   // (a slab on its edge offers no top)
            const FVector C = K.Centre + N * K.Half.Z;
            Top = FMath::Max(Top, static_cast<float>(C.Z - (N.X * (P.X - C.X) + N.Y * (P.Y - C.Y)) / N.Z));
        }
        return Top;
    }

    // Set a turned box down on what is under it: its bottom corners and the middle of its underside each rest on or
    // above the support beneath them, the highest touching (sunk Sink cm). False if it would hang more than Overhang cm
    // anywhere under it (a block balanced on a point over nothing), or rest too high.
    bool Rest(const FRubble& Out, FTempleRubbleBlock& B, float Sink, float Overhang, float MaxHeap = 260.f)
    {
        TArray<FVector> Under;
        for (float X : {-1.f, 1.f}) for (float Y : {-1.f, 1.f}) Under.Add(B.Turn.RotateVector(FVector(X * B.Half.X, Y * B.Half.Y, -B.Half.Z)));
        for (float X : {-1.f, 1.f}) Under.Add(B.Turn.RotateVector(FVector(X * B.Half.X * .5f, 0, -B.Half.Z)));
        Under.Add(B.Turn.RotateVector(FVector(0, 0, -B.Half.Z)));
        float Z = -1e9f; TArray<float> Support;
        for (const FVector& O : Under)
        {
            const float Sup = TopAt(Out, FVector2D(B.Centre) + FVector2D(O));
            Support.Add(Sup);
            Z = FMath::Max(Z, static_cast<float>(Sup - O.Z));
        }
        Z -= Sink;
        float Gap = 0;
        for (int32 I = 0; I < Under.Num(); ++I) Gap = FMath::Max(Gap, static_cast<float>(Z + Under[I].Z - Support[I]));
        // Hanging more than Overhang somewhere: it settles into the debris under its high side, up to two fifths of its
        // height (a tipped block half-buried reads as fallen; one balanced on an edge over air does not).
        if (Gap > Overhang)
        {
            const float Settle = Gap - Overhang;
            if (Settle > .8f * B.Half.Z) return false;
            Z -= Settle;
        }
        if (Z - ChultTempleRubbleBaseAt(FVector2D(B.Centre)) > MaxHeap) return false;
        B.Centre.Z = Z;
        return true;
    }

    // Would a block reach into the crawl passage through the pile (with a 2 cm margin)? Sampled over the passage.
    bool IntoPassage(const FTempleRubbleBlock& B)
    {
        const FQuat Q = B.Turn.Quaternion();
        for (float S = MouthS; S <= PileS1 + 20; S += 8)
            for (float T = CrawlT - CrawlHalf - 2; T <= CrawlT + CrawlHalf + 2; T += 8)
                for (float Z = RZ; Z <= RZ + CrawlH + 2; Z += 6)
                {
                    const FVector L = Q.UnrotateVector(FVector(S, T, Z) - B.Centre);
                    if (FMath::Abs(L.X) < B.Half.X && FMath::Abs(L.Y) < B.Half.Y && FMath::Abs(L.Z) < B.Half.Z) return true;
                }
        return false;
    }

    // A piece laid by hand at (S, T): tipped to lie along what is under it, its own turn added (half its pitch and roll);
    // where it cannot rest it slides on along Slide (7 tries, 20 cm each). Ok: any further check (clear of the passage...).
    struct FUp { float S, T; FVector Half; FRotator Turn; bool bMoss; };
    bool Lay(FRubble& Out, const FUp& U, const FVector2D& Slide, float Sink, float Overhang, TFunctionRef<bool(const FTempleRubbleBlock&)> Ok, float MaxHeap = 260.f)
    {
        for (int32 Step = 0; Step < 7; ++Step)
        {
            const FVector2D At = FVector2D(U.S, U.T) + Slide * (Step * 20.f);
            const FVector2D Fw(FMath::Cos(FMath::DegreesToRadians(U.Turn.Yaw)), FMath::Sin(FMath::DegreesToRadians(U.Turn.Yaw))), Rt(-Fw.Y, Fw.X);
            const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Fw * U.Half.X * .8f) - TopAt(Out, At - Fw * U.Half.X * .8f), 1.6f * U.Half.X));
            const float Roll = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Rt * U.Half.Y * .8f) - TopAt(Out, At - Rt * U.Half.Y * .8f), 1.6f * U.Half.Y));
            for (float Sign : {1.f, -1.f})
            {
                FTempleRubbleBlock B; B.Centre = FVector(At.X, At.Y, RZ); B.Half = U.Half; B.bMoss = U.bMoss;
                B.Turn = FRotator(FMath::Clamp(Pitch, -40.f, 40.f) + U.Turn.Pitch * .5f, U.Turn.Yaw, FMath::Clamp(Sign * Roll, -40.f, 40.f) + U.Turn.Roll * .5f);
                if (Rest(Out, B, Sink, Overhang, MaxHeap) && Ok(B)) { Out.Blocks.Add(B); return true; }
            }
        }
        return false;
    }

    // The pile the crevice opens in (2026-10-26): blocks either side of the passage and slabs over it, set square to it
    // so their faces are its walls and roof; then the fall heaped on them, set down on what is under it, round and over.
    void Pile(FRubble& Out)
    {
        GPileWall = true;
        ON_SCOPE_EXIT { GPileWall = false; };
        const float AT0 = CrawlT - CrawlHalf, AT1 = CrawlT + CrawlHalf;
        auto Fixed = [&](float S, float T, float Z, const FVector& Half, const FRotator& Turn, bool bMoss)
        {
            FTempleRubbleBlock B; B.Centre = FVector(S, T, Z); B.Half = Half; B.Turn = Turn; B.bMoss = bMoss;
            Out.Blocks.Add(B);
        };
        // Its sides: big blocks on the floor, their inner faces the passage's walls.
        // (Turned a little about the vertical, each set out by what the turn swings its inner corners in, so the faces
        // stay clear of the passage.)
        auto Side = [&](float S, float Sg, float Gap, const FVector& Half, float Yaw, bool bMoss)
        {
            const float Rad = FMath::DegreesToRadians(FMath::Abs(Yaw));
            const float Reach = Half.X * FMath::Sin(Rad) + Half.Y * FMath::Cos(Rad);   // its half-width across the passage
            Fixed(S, CrawlT + Sg * (CrawlHalf + Gap + Reach), RZ + Half.Z - 2, Half, FRotator(0, Yaw, 0), bMoss);
        };
        Side(MouthS + 64, 1, 2, FVector(64, 58, 58), 9, false);
        Side(PileS1 - 40, 1, 4, FVector(44, 50, 36), -14, true);
        Side(MouthS + 70, -1, 2, FVector(70, 60, 50), -7, true);
        Side(PileS1 - 36, -1, 3, FVector(40, 46, 32), 16, false);
        Side(PileS1 + 6, 1, 30, FVector(34, 40, 24), 32, false);
        Side(PileS1 - 2, -1, 26, FVector(30, 36, 20), -28, true);
        // Over it: a slab of the vault across the gap, and a broken lip at the front, their undersides its roof.
        Fixed(MouthS + 78, CrawlT + 8, RZ + CrawlH + 1 + 22, FVector(78, 96, 22), FRotator(0, 7, 0), true);
        Fixed(PileS1 - 26, CrawlT - 12, RZ + CrawlH + 1 + 16, FVector(30, 72, 16), FRotator(0, -13, 0), false);
        // The fall heaped on and round them.
        const FUp Up[] = {
            {MouthS + 55, CrawlT + 45, {66, 100, 38}, {-6, 4, 5}, false}, {MouthS + 50, CrawlT - 90, {55, 80, 34}, {5, -10, -6}, true},
            {MouthS + 40, CrawlT - 10, {42, 62, 30}, {12, 20, 8}, false}, {MouthS + 95, AT1 + 190, {70, 60, 44}, {0, 25, 8}, true},
            {MouthS + 80, AT0 - 180, {60, 55, 38}, {6, -20, 0}, false}, {PileS1 + 15, AT1 + 150, {40, 38, 30}, {10, 35, -8}, true},
            {PileS1 + 5, AT0 - 140, {36, 40, 26}, {-8, -25, 10}, false}, {MouthS + 35, AT1 + 110, {45, 50, 32}, {14, 50, -10}, false},
            {MouthS + 30, AT0 - 100, {40, 48, 30}, {-12, -40, 9}, true}, {MouthS + 120, CrawlT + 70, {38, 44, 24}, {16, 70, -14}, false},
            {MouthS + 60, CrawlT + 10, {56, 70, 30}, {0, 35, 0}, true}, {MouthS + 45, CrawlT + 160, {50, 60, 32}, {0, -30, 0}, false},
            {MouthS + 45, CrawlT - 160, {48, 56, 30}, {0, 15, 0}, true}, {MouthS + 30, CrawlT + 40, {36, 44, 26}, {0, 70, 0}, false},
            {MouthS + 25, CrawlT - 50, {34, 40, 24}, {0, -55, 0}, true}, {MouthS + 90, CrawlT - 40, {30, 36, 20}, {0, 10, 0}, false}};
        int32 Laid = 0;
        for (const FUp& U : Up)
        {
            // Tipped to lie along what is under it, its own turn added; where it cannot rest, slid back toward the wall.
            bool bDone = false;
            for (int32 Slide = 0; Slide < 7 && !bDone; ++Slide)
            {
                const FVector2D At(FMath::Max(MouthS + U.Half.X * .5f, U.S - Slide * 20.f), U.T + (Slide % 2 ? 1.f : -1.f) * Slide * 8.f);
                const FVector2D Fw(FMath::Cos(FMath::DegreesToRadians(U.Turn.Yaw)), FMath::Sin(FMath::DegreesToRadians(U.Turn.Yaw))), Rt(-Fw.Y, Fw.X);
                const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Fw * U.Half.X * .8f) - TopAt(Out, At - Fw * U.Half.X * .8f), 1.6f * U.Half.X));
                const float Roll = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Rt * U.Half.Y * .8f) - TopAt(Out, At - Rt * U.Half.Y * .8f), 1.6f * U.Half.Y));
                for (float Sign : {1.f, -1.f})
                {
                    FTempleRubbleBlock B; B.Centre = FVector(At.X, At.Y, RZ); B.Half = U.Half; B.bMoss = U.bMoss;
                    B.Turn = FRotator(FMath::Clamp(Pitch, -40.f, 40.f) + U.Turn.Pitch * .5f, U.Turn.Yaw, FMath::Clamp(Sign * Roll, -40.f, 40.f) + U.Turn.Roll * .5f);
                    if (Rest(Out, B, 3.f, 30.f) && !IntoPassage(B)) { Out.Blocks.Add(B); bDone = true; ++Laid; break; }
                }
            }
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_PILE laid=%d of %d"), Laid, int32(UE_ARRAY_COUNT(Up)));
    }

    // Set down on what is under its corners (the highest of them touching): on a rim, the floor-side corners.
    void Plunge(FRubble& Out, float S, float T, const FVector& Half, const FRotator& Turn, bool bMoss)
    {
        FTempleRubbleBlock B; B.Centre = FVector(S, T, 0); B.Half = Half; B.Turn = Turn; B.bMoss = bMoss;
        float Z = -1e9f;
        for (float X : {-1.f, 1.f}) for (float Y : {-1.f, 1.f})
        {
            const FVector O = Turn.RotateVector(FVector(X * Half.X, Y * Half.Y, -Half.Z));
            Z = FMath::Max(Z, static_cast<float>(TopAt(Out, FVector2D(S, T) + FVector2D(O)) - O.Z));
        }
        B.Centre.Z = Z - 2;
        Out.Blocks.Add(B);
    }

    // The gallery's middle (2026-11-04, user: "rubble from the balcony"): 16 m of its slab, balustrade and finials fell
    // with the vault over it into the Astral pool 13 m below. Most of it is gone into the Astral; pieces lie on the
    // islands round the piers' feet, and what was flung across the pool lies at its far rim, some teetering over the edge. Nothing stands in the pit (the Astral reads as open void).
    void GalleryFall(FRubble& Out)
    {
        auto Any = [](const FTempleRubbleBlock&) { return true; };
        // (Its broken ends sag from the standing runs: ChultTempleRubbleBuild.inl.)
        int32 Laid = 0, Of = 0;
        // On the colonnade's two islands, in the ring round each pier's base: chunks of the slab, a finial.
        for (float Sg : {-1.f, 1.f})
        {
            const FVector2D C(TW::HallMid + Sg * TW::NaveHalf, TW::PierT[0]);
            const FUp Up[] = {{float(C.X + Sg * 170), float(C.Y + 40), {46, 40, 28}, {0, 25, 0}, false}, {float(C.X - Sg * 150), float(C.Y - 95), {40, 34, 24}, {8, -30, 6}, true},
                {float(C.X + Sg * 70), float(C.Y - 172), {52, 30, 22}, {-6, 80, 4}, false}, {float(C.X - Sg * 60), float(C.Y + 175), {18, 15, 13}, {20, 40, 10}, false}};
            for (const FUp& U : Up) { ++Of; Laid += Lay(Out, U, FVector2D::ZeroVector, 3.f, 12.f, Any); }
        }
        // On the broken gallery piers' islands, about their stumps: spalls and finials.
        int32 K = 0;
        for (float S : {78150.f, 78430.f, 78970.f, 79250.f})
        {
            const FVector2D C(S, TW::GalleryT + 50);
            for (float A : {.6f, 2.9f}) { ++Of; Laid += Lay(Out, {float(C.X + 84 * FMath::Cos(A + K)), float(C.Y + 84 * FMath::Sin(A + K)), {22, 17, 13}, {12, 50.f * K, -8}, (K % 2) == 1}, FVector2D::ZeroVector, 3.f, 10.f, Any); }
            ++K;
        }
        // Flung over the pool to its far rim: slabs teetering over the edge (their floor-side ends on the floor), blocks,
        // a balustrade length and a finial on the floor there.
        Plunge(Out, 78700, -4935, FVector(200, 160, 28), FRotator(-22, 90, 5), false);
        Plunge(Out, 78060, -4945, FVector(140, 120, 26), FRotator(-16, 75, -6), true);
        Plunge(Out, 79330, -4945, FVector(150, 130, 26), FRotator(-18, 100, 4), false);
        const FUp Floor[] = {{78520, -5085, {70, 60, 40}, {0, 20, 0}, false}, {78900, -5125, {60, 55, 35}, {6, -30, -4}, true},
            {78650, -5185, {170, 41, 13}, {0, 8, 0}, false}, {78400, -5150, {18, 15, 13}, {25, 35, 10}, false}, {79000, -5030, {44, 40, 26}, {10, 60, -8}, false}};
        for (const FUp& U : Floor) { ++Of; Laid += Lay(Out, U, FVector2D::ZeroVector, 3.f, 22.f, Any); }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_GALLERY laid=%d of %d"), Laid, Of);
    }

    // The stairs broken (2026-11-04): what fell from each flight's break went into the Astral rupture under it. A few
    // pieces of its treads and masonry lie on the hall's floor by the rupture's rim, one tipped over the edge.
    void StairBreakDebris(FRubble& Out)
    {
        int32 Laid = 0, Of = 0;
        const float Mid = (StairBreak0 + StairBreak1) * .5f;
        for (float Sg : {-1.f, 1.f})
        {
            const float Face = Sg < 0 ? RS0 + TW::StairW : RS1 - TW::StairW, In = -Sg;   // the flight's open face; In toward the hall
            Plunge(Out, Face + In * 95, Mid + 30 * Sg, FVector(105, 78, 26), FRotator(-16, Sg < 0 ? 180.f : 0.f, 5 * Sg), Sg > 0);
            ++Of; ++Laid;
            const FUp Up[] = {{Face + In * 240, Mid - 70, {66, 52, 34}, {0, 25, 0}, false}, {Face + In * 215, Mid + 120, {58, 46, 30}, {5, -35, 4}, true},
                {Face + In * 330, Mid + 20, {38, 34, 22}, {0, 60, 0}, false}, {Face + In * 290, Mid - 200, {30, 26, 18}, {8, 15, -6}, false},
                {Face + In * 180, Mid + 230, {26, 24, 16}, {0, -50, 0}, true}};
            for (const FUp& U : Up)
            {
                ++Of;
                Laid += Lay(Out, U, FVector2D(In, 0), 3.f, 22.f, [](const FTempleRubbleBlock& B)
                {
                    return !ChultTempleRubbleHoleAt(FVector2D(B.Centre), static_cast<float>(FVector2D(B.Half.X, B.Half.Y).Size()) * .6f);
                }, 120.f);
            }
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_STAIRS laid=%d of %d"), Laid, Of);
    }

    // The two piers brought down (FallenPiers): each lies broken in three lengths and its capital, out from its stump,
    // the way that keeps them clear of the lane and the fittings (tried round the compass).
    void FallenPierPieces(FRubble& Out, FRandomStream& R)
    {
        for (int32 F : FallenPiers)
        {
            const FVector2D Stump(TW::HallMid + (F / 5 ? 1.f : -1.f) * TW::NaveHalf, TW::PierT[F % 5]);
            const FVector Pieces[] = {{165, TW::PierHalf, TW::PierHalf}, {140, TW::PierHalf, TW::PierHalf}, {120, TW::PierHalf, TW::PierHalf}, {TW::PierHalf + 36, TW::PierHalf + 36, 25}};
            int32 Best = 0; float BestYaw = 0;
            for (int32 Try = 0; Try < 24; ++Try)
            {
                const float Yaw = Try * 15.f + (F == 3 ? 0.f : 7.f);
                const FVector2D D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
                float Along = TW::PierHalf + 60; int32 Fit = 0;
                for (const FVector& H : Pieces) { Along += H.X; Fit += Clear(Stump + D * Along, static_cast<float>(FVector2D(H.X, H.Y).Size())); Along += H.X + 25; }
                if (Fit > Best) { Best = Fit; BestYaw = Yaw; }
            }
            const FVector2D D(FMath::Cos(FMath::DegreesToRadians(BestYaw)), FMath::Sin(FMath::DegreesToRadians(BestYaw)));
            float Along = TW::PierHalf + 60; int32 Laid = 0;
            for (int32 K = 0; K < int32(UE_ARRAY_COUNT(Pieces)); ++K)
            {
                const FVector& H = Pieces[K]; Along += H.X;
                const FVector2D At = Stump + D * Along + FVector2D(-D.Y, D.X) * R.FRandRange(-25.f, 25.f) * K;
                Along += H.X + 25;
                if (!Clear(At, static_cast<float>(FVector2D(H.X, H.Y).Size()))) continue;
                Laid += Lay(Out, {static_cast<float>(At.X), static_cast<float>(At.Y), H, FRotator(R.FRandRange(-4, 4), BestYaw + R.FRandRange(-9, 9), R.FRandRange(-4, 4)), K == 1}, FVector2D::ZeroVector, 6.f, 30.f, [](const FTempleRubbleBlock&) { return true; });
            }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_PIER pier=%d yaw=%.0f laid=%d"), F, BestYaw, Laid);
        }
    }

    const FRubble& Rubble()
    {
        static FRubble Out;
        if (Out.Blocks.Num()) return Out;
        FRandomStream R(2026110401);
        Pile(Out);
        GalleryFall(Out);
        StairBreakDebris(Out);
        // The sea tear (2026-11-04): a slab of the vault from the hole over the -S row lies half over its +T side, tipped into it.
        Plunge(Out, TempleWind::SeaS + 10, TempleWind::SeaT + 165, FVector(130, 110, 24), FRotator(-14, -90, 6), true);
        FallenPierPieces(Out, R);
        // Heaps: a few big blocks round a centre, smaller ones on and against them, only under the vault's holes (what fell
        // through them; user 2026-11-04: no unexplained piles). (No fallen round columns since 2026-11-04: the sanctum's
        // piers are square.)
        int32 Heaps = 0, Rejected = 0;
        for (int32 Try = 0; Try < 12000 && Heaps < 60; ++Try)
        {
            const FCave& Cv = Caves[Try % UE_ARRAY_COUNT(Caves)];
            const FVector2D H(Cv.S + R.FRandRange(-1.f, 1.f) * Cv.RS, Cv.T + R.FRandRange(-1.f, 1.f) * Cv.RT);
            if (ChultTempleRubbleCaveAt(H.X, H.Y) < 0 || !Clear(H, 90)) continue;
            bool Near = false;
            for (const FTempleRubbleBlock& B : Out.Blocks) Near |= FVector2D::Distance(FVector2D(B.Centre), H) < 120;
            if (Near) continue;
            ++Heaps;
            constexpr float CaveK = 1.3f;
            const int32 Count = R.RandRange(4, 9);
            for (int32 K = 0, Attempt = 0; K < Count && Attempt < Count * 6; ++Attempt)   // (a piece that cannot rest there is tried again)
            {
                FTempleRubbleBlock B;
                const float Scale = (K == 0 ? 1.f : FMath::Lerp(1.f, .55f, K / float(Count))) * CaveK;
                // Broken masonry is wider than it is tall: a heap's pieces are squat, the collapses' a little taller.
                B.Half = FVector(R.FRandRange(60, 140) * Scale, R.FRandRange(45, 105) * Scale, R.FRandRange(28, 52) * FMath::Min(Scale, 1.15f));
                // Later, smaller pieces fall on and against the first (closer in), tipped more.
                const FVector2D Drop = H + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * (K == 0 ? 0.f : 110.f);
                const float Rad = static_cast<float>(FVector2D(B.Half.X, B.Half.Y).Size());
                // Where it cannot rest it slides on, out from the heap's middle, until it can (scree at the foot).
                const FVector2D Out_ = (Drop - H).IsNearlyZero() ? FVector2D(1, 0) : (Drop - H).GetSafeNormal();
                FTempleRubbleBlock Best; bool bAny = false;
                for (int32 Slide = 0; Slide < 6 && !bAny; ++Slide)
                {
                const FVector2D At = Drop + Out_ * (Slide * 35.f);
                if (!Clear(At, Rad)) break;
                // Tipped to lie along what is under it (pitch and roll fitted to the support at its ends and sides), a little more.
                const float Tip = K == 0 ? 5.f : 12.f, Yaw = R.FRandRange(0, 360);
                const FVector2D Fw(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw))), Rt(-Fw.Y, Fw.X);
                const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Fw * B.Half.X * .8f) - TopAt(Out, At - Fw * B.Half.X * .8f), 1.6f * B.Half.X));
                const float Roll = FMath::RadiansToDegrees(FMath::Atan2(TopAt(Out, At + Rt * B.Half.Y * .8f) - TopAt(Out, At - Rt * B.Half.Y * .8f), 1.6f * B.Half.Y));
                const float JP = R.FRandRange(-Tip, Tip), JR = R.FRandRange(-Tip, Tip);
                B.Centre = FVector(At.X, At.Y, RZ);
                B.bMoss = R.FRand() < .3f;
                // (Which way a roll raises a side is checked, not assumed: the better fit of the two is kept.)
                for (float Sign : {1.f, -1.f})
                {
                    FTempleRubbleBlock Try_ = B;
                    Try_.Turn = FRotator(FMath::Clamp(Pitch, -40.f, 40.f) + JP, Yaw, FMath::Clamp(Sign * Roll, -40.f, 40.f) + JR);
                    // (No more than 1.4 m up: what fell through a vault hole spreads; it does not stack into towers.)
                    if (Rest(Out, Try_, K == 0 ? R.FRandRange(4.f, 14.f) : 3.f, 22.f, 140.f) && (!bAny || Try_.Centre.Z < Best.Centre.Z)) { Best = Try_; bAny = true; }
                }
                }
                if (!bAny) { ++Rejected; continue; }
                B = Best;
                Out.Blocks.Add(B); ++K;
            }
        }
        // Slabs of the fallen vault, long and thin: one end on the floor (or what is there), the other up on what it
        // came down onto; lying flatter where there was nothing to lean on.
        for (int32 Try = 0, Made = 0; Try < 2400 && Made < 16; ++Try)
        {
            const FCave& Cv = Caves[Try % UE_ARRAY_COUNT(Caves)];
            const FVector2D At(Cv.S + R.FRandRange(-1.f, 1.f) * Cv.RS * 1.3f, Cv.T + R.FRandRange(-1.f, 1.f) * Cv.RT * 1.3f);
            FTempleRubbleBlock B; B.Half = FVector(R.FRandRange(150, 230), R.FRandRange(90, 130), R.FRandRange(18, 26));
            if (!Clear(At, B.Half.X * .8f)) continue;
            const float Yaw = R.FRandRange(0, 360);
            const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
            const float Lo = TopAt(Out, At - Dir * B.Half.X * .9f), Hi = TopAt(Out, At + Dir * B.Half.X * .9f);
            const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Hi - Lo, 2.f * B.Half.X * .9f));
            B.Turn = FRotator(FMath::Clamp(Pitch, -55.f, 55.f), Yaw, R.FRandRange(-6, 6));
            B.Centre = FVector(At.X, At.Y, RZ);
            B.bMoss = true;
            if (!Rest(Out, B, 3.f, 30.f)) { ++Rejected; continue; }
            Out.Blocks.Add(B); ++Made;
        }
        // Nothing in the passage through the pile or on the apron before it (the pile's own walls and roof are outside it).
        int32 InWay = 0;
        for (const FTempleRubbleBlock& B : Out.Blocks)
        {
            const FQuat Q = B.Turn.Quaternion(); bool bIn = false;
            for (float S = MouthS + 4; S <= PileS1 + 80 && !bIn; S += 8)
                for (float T = CrawlT - CrawlHalf + 4; T <= CrawlT + CrawlHalf - 4 && !bIn; T += 8)
                    for (float Z = RZ + 2; Z <= RZ + 36 && !bIn; Z += 6)
                    {
                        const FVector L = Q.UnrotateVector(FVector(S, T, Z) - B.Centre);
                        bIn = FMath::Abs(L.X) < B.Half.X && FMath::Abs(L.Y) < B.Half.Y && FMath::Abs(L.Z) < B.Half.Z;
                    }
            if (bIn) { ++InWay; UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_RUBBLE_IN_PASSAGE at=%s half=%s turn=%s"), *B.Centre.ToString(), *B.Half.ToString(), *B.Turn.ToString()); }
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_LAYOUT blocks=%d drums=%d heaps=%d rejected=%d in_passage=%d"), Out.Blocks.Num(), Out.Drums.Num(), Heaps, Rejected, InWay);
        return Out;
    }
}

bool ChultTempleRubbleSourced(const FVector2D& F)
{
    if (ChultTempleRubbleCaveAt(F.X, F.Y, 350.f) >= 0) return true;                                    // under or by a hole in the vault
    if (F.X > GalleryKeep0 - 300 && F.X < GalleryKeep1 + 300 && F.Y > TempleWind::PoolT - 300) return true;   // the gallery's fall, to the pool's rim
    for (int32 P : FallenPiers)                                                                        // a fallen pier, where it lies
        if (FVector2D::Distance(F, FVector2D(TempleWind::HallMid + (P / 5 ? 1.f : -1.f) * TempleWind::NaveHalf, TempleWind::PierT[P % 5])) < 1300) return true;
    const float BreakT = (StairBreak0 + StairBreak1) * .5f;                                            // a stair's break
    for (float S : {RS0 + TempleWind::StairW, RS1 - TempleWind::StairW}) if (FVector2D::Distance(F, FVector2D(S, BreakT)) < 550) return true;
    return F.X < PileS1 + 250 && FMath::Abs(F.Y - CrawlT) < 400;                                       // the crevice's pile
}

int32 ChultTempleRubbleUnsourced()
{
    int32 N = 0;
    for (const FTempleRubbleBlock& B : Rubble().Blocks)
        if (!ChultTempleRubbleSourced(FVector2D(B.Centre))) { ++N; UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_RUBBLE_UNSOURCED at=%s"), *B.Centre.ToString()); }
    return N;
}

int32 ChultTempleRubbleCaveAt(float S, float T, float Grow)
{
    for (int32 K = 0; K < int32(UE_ARRAY_COUNT(Caves)); ++K)
    {
        const FCave& C = Caves[K];
        const float DX = (S - C.S) / (C.RS + Grow), DY = (T - C.T) / (C.RT + Grow);
        const float Jag = 1.f + .12f * FMath::Sin(5.f * FMath::Atan2(DY, DX) + 1.3f + K) + .07f * FMath::Sin(13.f * FMath::Atan2(DY, DX) + 2.f * K);
        if (FMath::Sqrt(DX * DX + DY * DY) * Jag < 1.f) return K;
    }
    return -1;
}

int32 ChultTempleRubbleHoleAt(const FVector2D& F, float Grow)
{
    if (F.X < RS0 || F.X > RS1 || F.Y < RT0 || F.Y > RT1) return 0;
    for (const FRift& Rf : Rifts)
    {
        const float DX = (F.X - Rf.S) / (Rf.RS + Grow), DY = (F.Y - Rf.T) / (Rf.RT + Grow);
        const float Jag = 1.f + .14f * FMath::Sin(6.f * FMath::Atan2(DY, DX) + Rf.S * .1f) + .06f * FMath::Sin(11.f * FMath::Atan2(DY, DX) + Rf.T * .07f);
        if (FMath::Sqrt(DX * DX + DY * DY) * Jag < 1.f) return 1;
    }
    // The sanctum's own: the pool before the gallery, the sea tear and the aisles' ruptures still open (the rupture under
    // the crevice's pile lies buried). (ChultTempleSanctumHoleAt's rupture outline.)
    const int32 Sanctum = ChultTempleSanctumHoleAt(F, Grow);
    if (Sanctum == 2) return 1;   // the sea tear (open here: its fall is an Astral death)
    if (Sanctum != 1) return 0;
    for (int32 K = 0; K < int32(UE_ARRAY_COUNT(TempleWind::SanctumRifts)); ++K)
    {
        if (OpenSanctumRifts & (1u << K)) continue;
        const TempleWind::FSanctumRift& Rf = TempleWind::SanctumRifts[K];
        const float DX = (F.X - Rf.S) / (Rf.RS + Grow), DY = (F.Y - Rf.T) / (Rf.RT + Grow);
        const float Jag = 1.f + .14f * FMath::Sin(6.f * FMath::Atan2(DY, DX) + Rf.S * .1f);
        if (FMath::Sqrt(DX * DX + DY * DY) * Jag < 1.f) return 0;
    }
    return 1;
}

float ChultTempleRubbleBaseAt(const FVector2D& F)
{
    using namespace TempleWind;
    // The dais (its front steps as a ramp).
    if (F.X > DaisS0 && F.X < DaisS1)
    {
        if (F.Y < DaisT1) return RZ + DaisH;
        if (F.Y < DaisT1 + 180) return RZ + DaisH * (1.f - (F.Y - DaisT1) / 180.f);
    }
    // The stairs (their treads' line, the gallery's floor at their heads), not where they broke. (Nothing is set on the
    // gallery's standing runs over the pool: what fell is under them.)
    const bool bFlight = (F.X < RS0 + StairW || F.X > RS1 - StairW) && !(F.Y > StairBreak0 && F.Y < StairBreak1);
    if (bFlight && F.Y > StairFoot) return F.Y > GalleryT ? RUZ : RZ + (F.Y - StairFoot) * (UZ - HZ) / (GalleryT - StairFoot);
    // The Astral's pits (the pool, the ruptures): their bottoms.
    if (ChultTempleRubbleHoleAt(F)) return RZ - 330;
    return RZ;
}

float ChultTempleRubbleLaneDistance(const FVector2D& F)
{
    float Best = 1e9f;
    for (int32 I = 0; I + 1 < int32(UE_ARRAY_COUNT(Lane)); ++I)
        Best = FMath::Min(Best, SegmentDistance(F, FVector2D(Lane[I].S, Lane[I].T), FVector2D(Lane[I + 1].S, Lane[I + 1].T)));
    return Best;
}

const TArray<FTempleRubbleBlock>& ChultTempleRubbleBlocks() { return Rubble().Blocks; }
const TArray<FTempleRubbleDrum>& ChultTempleRubbleDrums() { return Rubble().Drums; }
const TArray<FVector2D>& ChultTempleRubbleBraziers() { return Braziers; }
const TArray<FVector2D>& ChultTempleRubbleJars() { return Jars; }

void ChultTempleRubbleCrawlRuns(TArray<FBox2D>& Out)
{
    Out.Reset();
    Out.Add(FBox2D(FVector2D(MouthS, CrawlT - CrawlHalf), FVector2D(PileS1, CrawlT + CrawlHalf)));   // through the pile
    Out.Add(FBox2D(FVector2D(CrawlBendS - CrawlHalf, CrawlT - CrawlHalf), FVector2D(MouthS, CrawlT + CrawlHalf)));   // under the stair, into the rock
    Out.Add(FBox2D(FVector2D(CrawlBendS - CrawlHalf, CrawlEndT), FVector2D(CrawlBendS + CrawlHalf, CrawlT + CrawlHalf)));
}

void ChultTempleRubbleCrawlSpaces(TArray<FBox>& Out)
{
    Out.Reset();
    TArray<FBox2D> Runs; ChultTempleRubbleCrawlRuns(Runs);
    for (const FBox2D& B : Runs) Out.Add(FBox(FVector(B.Min.X, B.Min.Y, RZ), FVector(B.Max.X, B.Max.Y, RZ + 80)));
    // The apron before the mouth: wider than the mouth, so he is down before the lintel however he comes at it.
    Out.Add(FBox(FVector(PileS1, CrawlT - 90, RZ), FVector(PileS1 + 80, CrawlT + 90, RZ + 80)));
}

bool InChultTempleRubble(const FVector& World)
{
    const FVector F = ChultTempleInteriorFrameOf(World);
    if (F.Z < RZ - 400 || F.Z > RTop + 600) return false;
    if (F.X > RS0 && F.X < RS1 && F.Y > RT0 && F.Y < RT1) return true;
    TArray<FBox2D> Runs; ChultTempleRubbleCrawlRuns(Runs);
    for (const FBox2D& B : Runs) if (B.ExpandBy(20).IsInside(FVector2D(F.X, F.Y))) return true;
    return false;
}

bool AtChultTempleCreviceEnd(const FVector& World, float Margin)
{
    const FVector F = ChultTempleInteriorFrameOf(World);
    return FVector2D::Distance(FVector2D(F.X, F.Y), FVector2D(CrawlBendS, CrawlEndT + CrawlHalf)) < Margin && F.Z < RZ + 120 && F.Z > RZ - 60;
}

int32 ChultTempleRubbleFloating(float Tolerance)
{
    // A block hangs if every point of its underside (corners, mid-edges, middle) is more than Tolerance above what is
    // under it (the hall's masonry, a drum, another block's top).
    const FRubble& Out = Rubble();
    int32 Floating = 0;
    for (const FTempleRubbleBlock& B : Out.Blocks)
    {
        float Least = 1e9f;
        for (float X : {-1.f, -.5f, 0.f, .5f, 1.f}) for (float Y : {-1.f, 0.f, 1.f})
        {
            const FVector O = B.Turn.RotateVector(FVector(X * B.Half.X, Y * B.Half.Y, -B.Half.Z));
            Least = FMath::Min(Least, static_cast<float>(B.Centre.Z + O.Z - TopAt(Out, FVector2D(B.Centre) + FVector2D(O), &B)));
        }
        if (Least > Tolerance) { ++Floating; UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_RUBBLE_FLOATING at=%s gap=%.0f"), *B.Centre.ToString(), Least); }
    }
    return Floating;
}
