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

    const TArray<FVector2D> Braziers = {{75380, -5800}, {75720, -6870}, {76330, -7550}, {75170, -7880}, {74480, -8050}, {74480, -8450}};
    const TArray<FVector2D> Jars = {{76560, -5850}, {75700, -7880}};

    bool Reserved(const FVector2D& P, float Radius)
    {
        for (const FVector2D& B : Braziers) if (FVector2D::Distance(P, B) < Radius + 70) return true;
        for (const FVector2D& J : Jars) if (FVector2D::Distance(P, J) < Radius + 50) return true;
        if (P.X < RS0 + 300 + Radius && FMath::Abs(P.Y - CrawlT) < 260 + Radius) return true;   // the crevice's mouth
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
    const FRubble& Rubble()
    {
        static FRubble Out;
        if (Out.Blocks.Num()) return Out;
        FRandomStream R(2026102601);
        // Heaps: a few big blocks round a centre, smaller ones on and against them; bigger under the fallen vault.
        int32 Heaps = 0;
        // (2026-10-26, user: "add more rubble": 46 heaps became about 70, the first tries under the collapses.)
        int32 CaveFirst = 0;
        for (int32 Try = 0; Try < 6000 && Heaps < 72; ++Try)
        {
            FVector2D H(R.FRandRange(RS0 + 60, RS1 - 60), R.FRandRange(RT0 + 60, RT1 - 60));
            if (CaveFirst < 40) { const FCave& Cv = Caves[CaveFirst % UE_ARRAY_COUNT(Caves)]; H = FVector2D(Cv.S + R.FRandRange(-1.f, 1.f) * Cv.RS, Cv.T + R.FRandRange(-1.f, 1.f) * Cv.RT); ++CaveFirst; }
            if (!Clear(H, 90)) continue;
            bool Near = false;
            for (const FTempleRubbleBlock& B : Out.Blocks) Near |= FVector2D::Distance(FVector2D(B.Centre), H) < 140;
            if (Near) continue;
            ++Heaps;
            const float CaveK = ChultTempleRubbleCaveAt(H.X, H.Y, 150.f) >= 0 ? 1.4f : 1.f;
            const int32 Count = R.RandRange(4, 9) + (CaveK > 1.f ? 3 : 0);
            float Layer = 0;
            for (int32 K = 0; K < Count; ++K)
            {
                FTempleRubbleBlock B;
                const float Scale = (K == 0 ? 1.f : FMath::Lerp(1.f, .55f, K / float(Count))) * CaveK;
                B.Half = FVector(R.FRandRange(60, 140), R.FRandRange(45, 105), R.FRandRange(32, 80)) * Scale;
                const FVector2D At = H + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * (K == 0 ? 0.f : 150.f);
                const float Rad = static_cast<float>(FVector2D(B.Half.X, B.Half.Y).Size());
                if (!Clear(At, Rad)) continue;
                // The first lie sunk in the floor; the rest on top of what is there, tipped.
                const bool bOnTop = K > 0 && R.FRand() < .6f;
                const float Z = bOnTop ? RZ + Layer + B.Half.Z * .55f : RZ + B.Half.Z * R.FRandRange(.55f, .8f);
                B.Centre = FVector(At.X, At.Y, Z);
                B.Turn = FRotator(R.FRandRange(-1.f, 1.f) * (bOnTop ? 24.f : 9.f), R.FRandRange(0, 360), R.FRandRange(-1.f, 1.f) * (bOnTop ? 20.f : 7.f));
                B.bMoss = R.FRand() < .3f;
                Layer = FMath::Max(Layer, Z - RZ + B.Half.Z * .6f);
                Out.Blocks.Add(B);
            }
        }
        // Slabs of the fallen vault, long and thin, leaning steeply where they came down.
        for (int32 Try = 0, Made = 0; Try < 1600 && Made < 16; ++Try)
        {
            const FCave& Cv = Caves[Try % UE_ARRAY_COUNT(Caves)];
            const FVector2D At(Cv.S + R.FRandRange(-1.f, 1.f) * Cv.RS * 1.3f, Cv.T + R.FRandRange(-1.f, 1.f) * Cv.RT * 1.3f);
            FTempleRubbleBlock B; B.Half = FVector(R.FRandRange(150, 230), R.FRandRange(90, 130), R.FRandRange(18, 26));
            if (!Clear(At, B.Half.X * .8f)) continue;
            B.Turn = FRotator(R.FRandRange(28, 52) * (R.FRand() < .5f ? -1.f : 1.f), R.FRandRange(0, 360), R.FRandRange(-8, 8));
            B.Centre = FVector(At.X, At.Y, RZ + FMath::Sin(FMath::DegreesToRadians(FMath::Abs(B.Turn.Pitch))) * B.Half.X * .85f);
            B.bMoss = true;
            Out.Blocks.Add(B); ++Made;
        }
        // Fallen columns: drums lying where they rolled, in two or three pieces each.
        for (int32 Try = 0, Made = 0; Try < 1600 && Made < 10; ++Try)
        {
            const FVector2D C(R.FRandRange(RS0 + 200, RS1 - 200), R.FRandRange(RT0 + 200, RT1 - 200));
            const float Yaw = R.FRandRange(0, UE_TWO_PI), Len = R.FRandRange(300, 480), Rad = R.FRandRange(40, 55);
            const FVector2D D(FMath::Cos(Yaw), FMath::Sin(Yaw));
            bool Ok = true;
            for (float U = -.5f; U <= .5f && Ok; U += .125f) Ok &= Clear(C + D * (U * Len), Rad);
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
    Out.Add(FBox2D(FVector2D(CrawlBendS - CrawlHalf, CrawlT - CrawlHalf), FVector2D(RS0, CrawlT + CrawlHalf)));
    Out.Add(FBox2D(FVector2D(CrawlBendS - CrawlHalf, CrawlEndT), FVector2D(CrawlBendS + CrawlHalf, CrawlT + CrawlHalf)));
}

void ChultTempleRubbleCrawlSpaces(TArray<FBox>& Out)
{
    Out.Reset();
    TArray<FBox2D> Runs; ChultTempleRubbleCrawlRuns(Runs);
    for (const FBox2D& B : Runs) Out.Add(FBox(FVector(B.Min.X, B.Min.Y, RZ), FVector(B.Max.X, B.Max.Y, RZ + 80)));
    // The apron before the mouth: wider than the mouth, so he is down before the lintel however he comes at it.
    Out.Add(FBox(FVector(RS0, CrawlT - 90, RZ), FVector(RS0 + 80, CrawlT + 90, RZ + 80)));
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
