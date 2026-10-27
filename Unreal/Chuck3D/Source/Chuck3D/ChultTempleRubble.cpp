#include "ChultTempleRubble.h"
#include "ChultTempleInterior.h"

// The rubble map's layout (Claude, 2026-10-26; ChultTempleRubble.h, docs/CHULT-TEMPLE-RUBBLE.md): the ruptures are
// placed by hand off the lane; the rubble is scattered in heaps by a seeded stream and kept off everything that must
// stay clear (the lane and a margin, the ruptures' rims, the braziers and pots, the crevice's mouth, where he comes to).
namespace
{
    using namespace TempleRubble;

    float SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D D = B - A;
        const float L2 = static_cast<float>(D.SizeSquared());
        const float U = L2 > 0 ? FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - A, D)) / L2, 0.f, 1.f) : 0.f;
        return static_cast<float>(FVector2D::Distance(P, A + D * U));
    }

    const TArray<FVector2D> Braziers = {{75380, -5800}, {75720, -6870}, {76330, -7550}, {75170, -7880}};   // (the two at the crevice went 2026-10-26: its own glow marks it)
    const TArray<FVector2D> Jars = {{76560, -5850}, {75700, -7880}};

    bool Reserved(const FVector2D& P, float Radius)
    {
        for (const FVector2D& B : Braziers) if (FVector2D::Distance(P, B) < Radius + 70) return true;
        for (const FVector2D& J : Jars) if (FVector2D::Distance(P, J) < Radius + 50) return true;
        if (P.X < PileS1 + 200 + Radius && FMath::Abs(P.Y - CrawlT) < 300 + Radius) return true;   // the crevice's pile and mouth
        if (FVector2D::Distance(P, FVector2D(Lane[0].S, Lane[0].T)) < 300 + Radius) return true;   // where he comes to
        return false;
    }

    // Is a block's footprint (centre, horizontal radius) clear of the lane, the ruptures and the reserved spots?
    bool Clear(const FVector2D& C, float Radius)
    {
        if (C.X < RS0 + 20 || C.X > RS1 - 20 || C.Y < RT0 + 20 || C.Y > RT1 - 20) return false;
        return ChultTempleRubbleLaneDistance(C) > LaneHalf + 35 + Radius && !ChultTempleRubbleHoleAt(C, Radius + 25) && !Reserved(C, Radius);
    }

    struct FRubble { TArray<FTempleRubbleBlock> Blocks; TArray<FTempleRubbleDrum> Drums; };

    // What is under a point: the floor, a drum's top, or the top face of a block already down (its tilted plane, over
    // its yaw-turned footprint). Every piece is set down onto this (user 2026-10-26: "some of the rubble is impossible
    // levitating blocks, make it natural"): it used to be stacked on the heap's height wherever it landed.
    float TopAt(const FRubble& Out, const FVector2D& In, const FTempleRubbleBlock* Skip = nullptr)
    {
        // Past a wall, what is against the wall carries on (a block leans on the wall; it does not hang over the void).
        const FVector2D P(FMath::Clamp(In.X, RS0 + 1.f, RS1 - 1.f), FMath::Clamp(In.Y, RT0 + 1.f, RT1 - 1.f));
        float Top = RZ;
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
    bool Rest(const FRubble& Out, FTempleRubbleBlock& B, float Sink, float Overhang)
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
        if (Z - RZ > 260.f) return false;
        B.Centre.Z = Z;
        return true;
    }

    // Would a block reach into the crawl passage through the pile (with a 2 cm margin)? Sampled over the passage.
    bool IntoPassage(const FTempleRubbleBlock& B)
    {
        const FQuat Q = B.Turn.Quaternion();
        for (float S = RS0; S <= PileS1 + 20; S += 8)
            for (float T = CrawlT - CrawlHalf - 2; T <= CrawlT + CrawlHalf + 2; T += 8)
                for (float Z = RZ; Z <= RZ + CrawlH + 2; Z += 6)
                {
                    const FVector L = Q.UnrotateVector(FVector(S, T, Z) - B.Centre);
                    if (FMath::Abs(L.X) < B.Half.X && FMath::Abs(L.Y) < B.Half.Y && FMath::Abs(L.Z) < B.Half.Z) return true;
                }
        return false;
    }

    // The pile the crevice opens in (2026-10-26): blocks either side of the passage and slabs over it, set square to it
    // so their faces are its walls and roof; then the fall heaped on them, set down on what is under it, round and over.
    void Pile(FRubble& Out)
    {
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
        Side(RS0 + 64, 1, 2, FVector(64, 58, 58), 9, false);
        Side(PileS1 - 40, 1, 4, FVector(44, 50, 36), -14, true);
        Side(RS0 + 70, -1, 2, FVector(70, 60, 50), -7, true);
        Side(PileS1 - 36, -1, 3, FVector(40, 46, 32), 16, false);
        Side(PileS1 + 6, 1, 30, FVector(34, 40, 24), 32, false);
        Side(PileS1 - 2, -1, 26, FVector(30, 36, 20), -28, true);
        // Over it: a slab of the vault across the gap, and a broken lip at the front, their undersides its roof.
        Fixed(RS0 + 78, CrawlT + 8, RZ + CrawlH + 1 + 22, FVector(78, 96, 22), FRotator(0, 7, 0), true);
        Fixed(PileS1 - 26, CrawlT - 12, RZ + CrawlH + 1 + 16, FVector(30, 72, 16), FRotator(0, -13, 0), false);
        // The fall heaped on and round them.
        struct FUp { float S, T; FVector Half; FRotator Turn; bool bMoss; };
        const FUp Up[] = {
            {RS0 + 55, CrawlT + 45, {66, 100, 38}, {-6, 4, 5}, false}, {RS0 + 50, CrawlT - 90, {55, 80, 34}, {5, -10, -6}, true},
            {RS0 + 40, CrawlT - 10, {42, 62, 30}, {12, 20, 8}, false}, {RS0 + 95, AT1 + 190, {70, 60, 44}, {0, 25, 8}, true},
            {RS0 + 80, AT0 - 180, {60, 55, 38}, {6, -20, 0}, false}, {PileS1 + 15, AT1 + 150, {40, 38, 30}, {10, 35, -8}, true},
            {PileS1 + 5, AT0 - 140, {36, 40, 26}, {-8, -25, 10}, false}, {RS0 + 35, AT1 + 110, {45, 50, 32}, {14, 50, -10}, false},
            {RS0 + 30, AT0 - 100, {40, 48, 30}, {-12, -40, 9}, true}, {RS0 + 120, CrawlT + 70, {38, 44, 24}, {16, 70, -14}, false},
            {RS0 + 60, CrawlT + 10, {56, 70, 30}, {0, 35, 0}, true}, {RS0 + 45, CrawlT + 160, {50, 60, 32}, {0, -30, 0}, false},
            {RS0 + 45, CrawlT - 160, {48, 56, 30}, {0, 15, 0}, true}, {RS0 + 30, CrawlT + 40, {36, 44, 26}, {0, 70, 0}, false},
            {RS0 + 25, CrawlT - 50, {34, 40, 24}, {0, -55, 0}, true}, {RS0 + 90, CrawlT - 40, {30, 36, 20}, {0, 10, 0}, false}};
        int32 Laid = 0;
        for (const FUp& U : Up)
        {
            // Tipped to lie along what is under it, its own turn added; where it cannot rest, slid back toward the wall.
            bool bDone = false;
            for (int32 Slide = 0; Slide < 7 && !bDone; ++Slide)
            {
                const FVector2D At(FMath::Max(RS0 + U.Half.X * .5f, U.S - Slide * 20.f), U.T + (Slide % 2 ? 1.f : -1.f) * Slide * 8.f);
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

    const FRubble& Rubble()
    {
        static FRubble Out;
        if (Out.Blocks.Num()) return Out;
        FRandomStream R(2026102601);
        Pile(Out);
        // Fallen columns first, lying on the floor where they rolled, in two or three pieces each.
        for (int32 Try = 0, Made = 0; Try < 1600 && Made < 10; ++Try)
        {
            const FVector2D C(R.FRandRange(RS0 + 200, RS1 - 200), R.FRandRange(RT0 + 200, RT1 - 200));
            const float Yaw = R.FRandRange(0, UE_TWO_PI), Len = R.FRandRange(300, 480), Rad = R.FRandRange(40, 55);
            const FVector2D D(FMath::Cos(Yaw), FMath::Sin(Yaw));
            bool Ok = true;
            for (float U = -.5f; U <= .5f && Ok; U += .125f) Ok &= Clear(C + D * (U * Len), Rad) && TopAt(Out, C + D * (U * Len)) < RZ + 1.f;
            if (!Ok) continue;
            const int32 Pieces = R.RandRange(2, 3);
            for (int32 K = 0; K < Pieces; ++K)
            {
                const float U0 = -.5f + K / float(Pieces), U1 = U0 + 1.f / Pieces - .03f;
                const FVector2D Kink = FVector2D(-D.Y, D.X) * R.FRandRange(-20, 20);
                FTempleRubbleDrum Dr;
                Dr.A = FVector(C.X + D.X * U0 * Len + Kink.X, C.Y + D.Y * U0 * Len + Kink.Y, RZ + Rad * .92f);
                Dr.B = FVector(C.X + D.X * U1 * Len + Kink.X, C.Y + D.Y * U1 * Len + Kink.Y, RZ + Rad * .92f);
                Dr.Radius = Rad;
                Out.Drums.Add(Dr);
            }
            ++Made;
        }
        // Heaps: a few big blocks round a centre, smaller ones on and against them; bigger under the collapses.
        // (2026-10-26, user: "add more rubble": about 70 heaps, the first tries under the collapses.)
        int32 Heaps = 0, CaveFirst = 0, Rejected = 0;
        for (int32 Try = 0; Try < 9000 && Heaps < 100; ++Try)
        {
            FVector2D H(R.FRandRange(RS0 + 60, RS1 - 60), R.FRandRange(RT0 + 60, RT1 - 60));
            if (CaveFirst < 40) { const FCave& Cv = Caves[CaveFirst % UE_ARRAY_COUNT(Caves)]; H = FVector2D(Cv.S + R.FRandRange(-1.f, 1.f) * Cv.RS, Cv.T + R.FRandRange(-1.f, 1.f) * Cv.RT); ++CaveFirst; }
            if (!Clear(H, 90)) continue;
            bool Near = false;
            for (const FTempleRubbleBlock& B : Out.Blocks) Near |= FVector2D::Distance(FVector2D(B.Centre), H) < 120;
            if (Near) continue;
            ++Heaps;
            const float CaveK = ChultTempleRubbleCaveAt(H.X, H.Y, 150.f) >= 0 ? 1.4f : 1.f;
            const int32 Count = R.RandRange(4, 9) + (CaveK > 1.f ? 3 : 0);
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
                    if (Rest(Out, Try_, K == 0 ? R.FRandRange(4.f, 14.f) : 3.f, 22.f) && (!bAny || Try_.Centre.Z > Best.Centre.Z)) { Best = Try_; bAny = true; }
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
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_LAYOUT blocks=%d drums=%d heaps=%d rejected=%d"), Out.Blocks.Num(), Out.Drums.Num(), Heaps, Rejected);
        return Out;
    }
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
    return 0;
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
    Out.Add(FBox2D(FVector2D(RS0, CrawlT - CrawlHalf), FVector2D(PileS1, CrawlT + CrawlHalf)));   // through the pile
    Out.Add(FBox2D(FVector2D(CrawlBendS - CrawlHalf, CrawlT - CrawlHalf), FVector2D(RS0, CrawlT + CrawlHalf)));
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
    // under it (the floor, a drum, another block's top).
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
