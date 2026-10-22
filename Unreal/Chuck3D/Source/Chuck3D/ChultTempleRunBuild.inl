// The temple trap run's masonry and trap layout (Claude, 2026-10-22; ChultTempleRun.h,
// docs/CHULT-TEMPLE-RUN.md). Included into ChultTempleInterior.cpp's anonymous namespace,
// after its lining helpers: frame coordinates, faces looking inward, as the rest of the interior.
//
// Timing (the research behind the run): each run of traps is phased so that a player moving
// at Chuck's run speed meets every gap (the axes' crossings, the fire and the saw drops travel
// down the hall as a wave at run speed, the Rayman Legends idea), and the gauntlet puts all four
// on one beat. Stopping between traps always works too.

struct FRunAxe { FVector Pivot, Route; float Arm, Amplitude, Period, Phase; };
struct FRunJet { FVector Mouth, Dir, Route; float Reach, Period, Phase, Warn, Burn; };
struct FRunSaw { FVector Axle, Route; float Radius, Lift, Period, Phase, Ceiling; bool bDrops; };
struct FRunMovers { TArray<FRunAxe> Axes; TArray<FRunJet> Jets; TArray<FRunSaw> Saws; };

namespace TempleRunBuild
{
    using namespace TempleRun;
    // The interior's own layout has a Floor, an ExitT and a dart gallery's DS0..DT1: here they are the run's.
    using TempleRun::Floor; using TempleRun::ExitT; using TempleRun::ExitHalf;
    using TempleRun::DS0; using TempleRun::DS1; using TempleRun::DT0; using TempleRun::DT1;
    constexpr float RunPace = 225.f;   // ChuckClipData::RunSpeed: the pace the waves are tuned for
    constexpr float AxeArrive = .35f;  // s after an axe's blade has crossed the route that a runner should reach it
    constexpr float SawArrive = .4f;   // s into a dropping saw's raised spell
    constexpr float JetAfter = .15f;   // s after a jet has gone out

    // A bed of iron spikes on a pit's floor (the spike corridor's spikes, fixed and a little taller).
    void SpikeBed(FParts& P, float S0, float S1, float T0, float T1, float Z, float Tall, FRandomStream& R)
    {
        Box(P.Dark, S0, S1, T0, T1, Z - 1, Z);
        for (float S = S0 + 14; S < S1 - 6; S += 27.f) for (float T = T0 + 14; T < T1 - 6; T += 27.f)
        {
            const float Sj = S + R.FRandRange(-5, 5), Tj = T + R.FRandRange(-5, 5), H = Tall * R.FRandRange(.8f, 1.1f);
            const FVector Tip(Sj + R.FRandRange(-2, 2), Tj + R.FRandRange(-2, 2), Z + H);
            const FVector C[4] = {FVector(Sj - 5, Tj - 5, Z), FVector(Sj + 5, Tj - 5, Z), FVector(Sj + 5, Tj + 5, Z), FVector(Sj - 5, Tj + 5, Z)};
            for (int32 K = 0; K < 4; ++K)
            {
                const FVector A = C[K], B = C[(K + 1) % 4], Out = (A + B) * .5f + ((A + B) * .5f - FVector(Sj, Tj, Z)) * 4.f + FVector(0, 0, H * .3f);
                QuadToward(P.Iron, A, B, Tip, Tip, Out);
            }
        }
    }
    // A pit across a corridor: lined walls down to a bed of spikes, a few old bones.
    void SpikePit(FParts& P, TArray<FSkull>& Skulls, float S0, float S1, float T0, float T1, float Depth, FRandomStream& R, const TArray<FHole>& Holes = {})
    {
        FLining L; L.bCeiling = false; L.Dado = 0;
        Line(P, S0, S1, T0, T1, UZ - Depth, UZ, Holes, L);
        SpikeBed(P, S0 + 2, S1 - 2, T0 + 2, T1 - 2, UZ - Depth, 46, R);
        for (int32 K = 0; K < 2; ++K)
            Skulls.Add({FVector(R.FRandRange(S0 + 30, S1 - 30), R.FRandRange(T0 + 30, T1 - 30), UZ - Depth + 30), FRotator(R.FRandRange(-40, 40), R.FRandRange(0, 360), R.FRandRange(-70, 70))});
    }

    // ---- 0. The collapse: the rubble stays, heaped as tumbled blocks he mantles up (each a
    //      34 cm step, his mantle takes 6-40 cm), to a chimney the old passage fell into.
    void Collapse(FParts& P, TArray<FFire>& Fires, FRandomStream& R)
    {
        const float T0 = ExitT - ExitHalf, T1 = ExitT + ExitHalf;
        FLining L; L.Dado = 110; L.Low = &P.Moss; L.High = &P.Stone;
        Line(P, ArchS, CollapseS1, T0, T1, Floor, CollapseTop, {{0, T0, T1, Floor, Floor + 280}, {1, ShaftT0, ShaftT1, RubbleTop, CollapseTop}}, L);
        // The climbing blocks, tumbled: on each step a flat block on the line up the middle (exactly a
        // step higher than the last, so each is one mantle), rougher ones either side at other heights,
        // all over a hidden core of rubble.
        for (int32 K = 0; K < Steps; ++K)
        {
            const float S0 = StepS0 + K * StepDepth + R.FRandRange(-5, 5), Z1 = Floor + (K + 1) * StepRise;
            const float A0 = ExitT - R.FRandRange(55, 85), A1 = ExitT + R.FRandRange(55, 85);
            Box(P.Stone, S0 + 14, CollapseS1, A0 + 6, A1 - 6, Floor, Z1 - 10);
            const float Deep = StepDepth + R.FRandRange(14, 30), Lane = R.FRandRange(34, 42);
            TurnedBox(R.FRand() < .3f ? P.Moss : P.Stone, FVector(S0 + Deep * .5f, ExitT + R.FRandRange(-4, 4), Z1 - 17), FVector(Deep * .5f, Lane, 17),
                FRotator(R.FRandRange(-1.2f, 1.2f), R.FRandRange(-5, 5), R.FRandRange(-1.2f, 1.2f)));
            for (float Sg : {-1.f, 1.f})
            {
                const float Inner = ExitT + Sg * (Lane - 4), Outer = Sg < 0 ? A0 : A1, Z = Z1 + R.FRandRange(-14, 10);
                const float SFront = S0 + R.FRandRange(-12, 10), D = FMath::Min(StepDepth + R.FRandRange(0, 26), CollapseS1 - 4 - SFront);   // never into the chimney
                TurnedBox(R.FRand() < .45f ? P.Moss : P.Stone, FVector(SFront + D * .5f, (Inner + Outer) * .5f, Z - 18), FVector(D * .5f, FMath::Abs(Outer - Inner) * .5f + 4, 18),
                    FRotator(R.FRandRange(-7, 7), R.FRandRange(-14, 14), R.FRandRange(-9, 9)));
            }
            // Heaped up against the walls beside the lane.
            for (float Sg : {-1.f, 1.f})
            {
                const float Edge = Sg < 0 ? A0 : A1, Wall = Sg < 0 ? T0 : T1, H = (K + 1) * StepRise + R.FRandRange(10, 50);
                TurnedBox(R.FRand() < .45f ? P.Moss : P.Stone, FVector(S0 + StepDepth * .5f, (Edge + Wall) * .5f, Floor + H * .5f),
                    FVector(StepDepth * .6f, FMath::Abs(Wall - Edge) * .5f + 10, H * .5f), FRotator(R.FRandRange(-10, 10), R.FRandRange(-25, 25), R.FRandRange(-10, 10)));
            }
        }
        // Loose stones on the steps' outer parts, off the line up the middle.
        for (int32 I = 0; I < 22; ++I)
        {
            const int32 K = R.RandRange(0, Steps - 1);
            const float S = StepS0 + K * StepDepth + R.FRandRange(10, StepDepth - 8), Sg = I % 2 ? 1.f : -1.f, H = R.FRandRange(6, 16);
            const float T = ExitT + Sg * R.FRandRange(52, 95);
            TurnedBox(R.FRand() < .4f ? P.Moss : P.Stone, FVector(S, T, Floor + (K + 1) * StepRise + H * .3f), FVector(R.FRandRange(7, 16), R.FRandRange(6, 14), H * .5f), FRotator(R.FRandRange(-30, 30), R.FRandRange(0, 360), R.FRandRange(-30, 30)));
        }
        // A broken lintel leaning on the heap, and a column drum fallen across its side.
        TurnedBox(P.Stone, FVector(68130, T0 + 38, Floor + 150), FVector(120, 18, 14), FRotator(-38, 4, 8));
        Drum(P.Moss, {67990, T1 - 30, Floor + 26}, {68120, T1 - 42, Floor + 34}, 26, 12);
        // The chimney's floor, the top of the heap; loose chunks under the arch and against the walls.
        Box(P.Stone, BS0, BS1, ShaftT0, ShaftT1, Floor, RubbleTop);
        for (int32 I = 0; I < 14; ++I)
        {
            const float S = R.FRandRange(ArchS + 40, CollapseS1 - 30), Sg = I % 2 ? 1.f : -1.f, H = R.FRandRange(12, 30);
            const float T = Sg < 0 ? R.FRandRange(T0 + 10, T0 + 26) : R.FRandRange(T1 - 26, T1 - 10);
            const float Under = Floor + FMath::Clamp((S - StepS0) / StepDepth, 0.f, float(Steps)) * StepRise * .6f;
            TurnedBox(R.FRand() < .4f ? P.Moss : P.Stone, FVector(S, T, Under + H * .5f), FVector(R.FRandRange(12, 26), R.FRandRange(10, 18), H * .5f), FRotator(R.FRandRange(-20, 20), R.FRandRange(0, 360), R.FRandRange(-20, 20)));
        }
        // Broken vault: slabs hanging from the fallen ceiling.
        for (int32 I = 0; I < 6; ++I)
            TurnedBox(P.Stone, FVector(R.FRandRange(ArchS + 80, CollapseS1 - 60), R.FRandRange(T0 + 40, T1 - 40), CollapseTop - R.FRandRange(10, 30)), FVector(R.FRandRange(25, 50), R.FRandRange(20, 40), R.FRandRange(10, 20)), FRotator(R.FRandRange(-25, 25), R.FRandRange(0, 360), R.FRandRange(-25, 25)));
        // The chimney: 1 m between its walls, 4 m up to the upper level's floor (no ceiling: it opens into the pit gallery).
        FLining Shaft; Shaft.Dado = 0; Shaft.bCeiling = false; Shaft.bFloor = false;
        Line(P, BS0, BS1, ShaftT0, ShaftT1, RubbleTop, UZ, {{0, ShaftT0, ShaftT1, RubbleTop, CollapseTop}}, Shaft);
        Torch(P, Fires, 1, T1, -1, ArchS + 120, Floor + 200, 3800, 620);
        Torch(P, Fires, 0, BS1, -1, (ShaftT0 + ShaftT1) * .5f, RubbleTop + 180, 3000, 520);
        Torch(P, Fires, 0, BS1, -1, (ShaftT0 + ShaftT1) * .5f, UZ - 90, 3000, 520);
    }

    // ---- 1. The pit gallery: three spike pits across a 1.8 m corridor, crossed on its walls.
    void PitGallery(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, FRandomStream& R)
    {
        FLining L; L.Dado = 120;
        L.FloorHoles = {FBox2D(FVector2D(BS0, ShaftT0), FVector2D(BS1, ShaftT1))};
        for (const FPit& Pt : Pits) L.FloorHoles.Add(FBox2D(FVector2D(BS0, Pt.T0), FVector2D(BS1, Pt.T1)));
        Line(P, BS0, BS1, BT0, BT1, UZ, Top, {{3, BS0, BS1, UZ, UZ + 400}, {0, BreakT0, BreakT1, UZ, Top}}, L);
        Corbel(P, 1, BS0, BS1, BT0, BT1, Top, 3, 16, 28);
        for (const FPit& Pt : Pits)
            SpikePit(P, Skulls, BS0, BS1, Pt.T0, Pt.T1, PitDepth, R, Pt.T0 == BreakT0 ? TArray<FHole>{{0, BreakT0, BreakT1, UZ - PitDepth, UZ}} : TArray<FHole>{});
        // The third pit's broken wall: an alcove behind it, open over the spikes.
        {
            FLining A; A.Dado = 0; A.bCeiling = true;
            Line(P, BS0 - BreakDeep, BS0, BreakT0, BreakT1, UZ - PitDepth, Top, {{1, BreakT0, BreakT1, UZ - PitDepth, Top}}, A);
            SpikeBed(P, BS0 - BreakDeep + 2, BS0, BreakT0 + 2, BreakT1 - 2, UZ - PitDepth, 46, R);
            for (int32 I = 0; I < 5; ++I)
                TurnedBox(P.Stone, FVector(R.FRandRange(BS0 - BreakDeep + 15, BS0 - 20), R.FRandRange(BreakT0 + 30, BreakT1 - 30), UZ - PitDepth + R.FRandRange(8, 30)), FVector(R.FRandRange(10, 22), R.FRandRange(12, 26), R.FRandRange(6, 14)), FRotator(R.FRandRange(-30, 30), R.FRandRange(0, 360), R.FRandRange(-30, 30)));
            // The wall's broken ends.
            for (float T : {BreakT0, BreakT1}) TurnedBox(P.Stone, FVector(BS0 - 10, T, UZ + R.FRandRange(60, 200)), FVector(14, 12, R.FRandRange(30, 60)), FRotator(R.FRandRange(-12, 12), R.FRandRange(-10, 10), R.FRandRange(-12, 12)));
        }
        // Light: over the landing and on the floors between the pits (away from the walls he runs).
        Torch(P, Fires, 1, BT0, 1, (BS0 + BS1) * .5f, UZ + 230, 3800, 640);
        for (float T : {-7650.f, -6810.f, -5850.f, -4920.f})
        {
            Torch(P, Fires, 0, BS0, 1, T, UZ + 210, 3800, 640);
            Torch(P, Fires, 0, BS1, -1, T + 120, UZ + 210, 3800, 640);
        }
        Glyphs(P, R, 1, BT0, 1, BS0 + 20, BS1 - 20, UZ + 290, UZ + 370);
        Jars.Add({FVector(BS1 - 35, -7820, UZ), float(R.FRandRange(0, 360)), true, 3});
    }

    // ---- 2. The axe hall: a causeway between spiked trenches, one axe alone, then a wave of three.
    void AxeHall(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, FRunMovers& M, FRandomStream& R)
    {
        FLining L; L.Dado = 160; L.Frieze = 70; L.Top = &P.Moss;
        L.FloorHoles = {FBox2D(FVector2D(CausewayS0, CT0), FVector2D(CausewayS1, CausewayT0)), FBox2D(FVector2D(CausewayS0, CausewayT1), FVector2D(CausewayS1, CT1))};
        Line(P, CS0, CS1, CT0, CT1, UZ, CTop, {{2, BS0, BS1, UZ, UZ + 400}, {2, DS0, DS1, UZ, UZ + 400}}, L);
        for (const FBox2D& Trench : L.FloorHoles)
        {
            FLining Tl; Tl.bCeiling = false; Tl.Dado = 0;
            Line(P, Trench.Min.X, Trench.Max.X, Trench.Min.Y, Trench.Max.Y, UZ - TrenchDepth, UZ, {}, Tl);
            SpikeBed(P, Trench.Min.X + 2, Trench.Max.X - 2, Trench.Min.Y + 2, Trench.Max.Y - 2, UZ - TrenchDepth, 52, R);
            for (int32 K = 0; K < 4; ++K)
                Skulls.Add({FVector(R.FRandRange(Trench.Min.X + 40, Trench.Max.X - 40), R.FRandRange(Trench.Min.Y + 30, Trench.Max.Y - 30), UZ - TrenchDepth + 34), FRotator(R.FRandRange(-40, 40), R.FRandRange(0, 360), R.FRandRange(-70, 70))});
        }
        // The causeway's kerbs: a pale edge so its line reads in the dark.
        for (float T : {CausewayT0, CausewayT1 - 8}) QuadToward(P.StepMoss, {CausewayS0, T, UZ + .5f}, {CausewayS1, T, UZ + .5f}, {CausewayS1, T + 8, UZ + .5f}, {CausewayS0, T + 8, UZ + .5f}, {CausewayS0, T + 4, UZ + 100});
        const float MidT = (CausewayT0 + CausewayT1) * .5f;
        for (int32 I = 0; I < UE_ARRAY_COUNT(AxeS); ++I)
        {
            const float S = AxeS[I];
            // A beam across the vault carrying each pivot; an iron hub under it.
            Box(P.Stone, S - 32, S + 32, CT0, CT1, CTop - 12, CTop);
            Box(P.Iron, S - 18, S + 18, MidT - 16, MidT + 16, AxePivotZ + 8, CTop - 12);
            // The wave: the second to the fourth reached in turn at run pace, each just after its blade has crossed.
            const float Phase = I == 0 ? 0.f : AxeArrive * -1.f + (S - AxeS[1]) / RunPace;
            M.Axes.Add({FVector(S, MidT, AxePivotZ), FVector(1, 0, 0), AxeArm, AxeAmplitude, AxePeriod, Phase});
        }
        // Light: both long walls (the blades sweep the middle), the ends.
        for (float S : {68600.f, 69100.f, 69700.f, 70700.f, 71200.f}) for (float Sg : {-1.f, 1.f})
        {
            const float U = S + (Sg < 0 ? 0 : 160);
            if (Sg < 0 && U > DS0 - 60 && U < DS1 + 60) continue;   // the fire hall's door
            Torch(P, Fires, 1, Sg < 0 ? CT0 : CT1, -Sg, U, UZ + 250, 4200, 700);
        }
        Torch(P, Fires, 0, CS0, 1, MidT, UZ + 240, 4200, 700);
        Torch(P, Fires, 0, CS1, -1, MidT, UZ + 240, 4200, 700);
        Glyphs(P, R, 0, CS1, -1, MidT - 200, MidT + 200, UZ + 300, UZ + 400);
        Jars.Add({FVector(CS0 + 40, CT1 - 40, UZ), float(R.FRandRange(0, 360)), true, 3});
        Jars.Add({FVector(CS1 - 40, CT1 - 40, UZ), float(R.FRandRange(0, 360)), false, 2});
    }

    // A fire mouth in a wall at S=W (Into the hall), firing across it.
    void FireMouth(FParts& P, FRunMovers& M, float W, float Into, float T, float Reach, float Period, float Phase, float Warn, float Burn, const FVector& Route)
    {
        SerpentMouth(P, 0, W, Into, T, FireZ - 4);
        M.Jets.Add({FVector(W + Into * 16, T, FireZ), FVector(Into, 0, 0), Route, Reach, Period, Phase, Warn, Burn});
    }

    // ---- 3. The fire hall: a lone mouth, a slalom of four alternating walls, two full-width pairs.
    void FireHall(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, FRunMovers& M, FRandomStream& R)
    {
        FLining L; L.Dado = 150;
        Line(P, DS0, DS1, DT0, DT1, UZ, UZ + 400, {{3, DS0, DS1, UZ, UZ + 400}, {2, DS0, DS1, UZ, ETop}}, L);
        Corbel(P, 1, DS0, DS1, DT0, DT1, UZ + 400, 3, 20, 28);
        const FVector Route(0, -1, 0);
        for (int32 I = 0; I < UE_ARRAY_COUNT(Jets); ++I)
        {
            const FJetSpot& J = Jets[I];
            // The lone mouth, then the slalom as a wave down the hall at run pace (start as the first
            // flares and weave with it, or as it dies and run straight); the pairs a beat apart.
            float Phase = 0;
            if (I >= 1 && I <= 4) Phase = 1.f + (Jets[1].T - J.T) / RunPace;
            if (I >= 5) Phase = 2.f + (Jets[5].T - J.T) / RunPace;
            FireMouth(P, M, J.bPlusWall ? DS1 : DS0, J.bPlusWall ? -1.f : 1.f, J.T, J.Reach, FirePeriod, Phase, FireWarn, FireBurn, Route);
        }
        for (float T : {-4900.f, -5650.f, -6950.f, -7550.f}) for (float Sg : {-1.f, 1.f})
            Torch(P, Fires, 0, Sg < 0 ? DS0 : DS1, -Sg, T + (Sg < 0 ? 0 : 140), UZ + 250, 3800, 640);
        Jars.Add({FVector(DS0 + 40, DT1 - 60, UZ), float(R.FRandRange(0, 360)), false, 2});
    }

    // ---- 4. The saw passage, under a low ceiling: a saw row that stays down, two more, then three
    //      that drop as a wave. Two discs fill each row.
    void SawRowDiscs(FParts& P, FRunMovers& M, float Line_, bool bAlongS, float Centre, float Half, float Radius, float Ceiling, bool bDrops, float Lift, float Period, float Phase)
    {
        const FVector Route = bAlongS ? FVector(1, 0, 0) : FVector(0, 1, 0);
        const float AxleZ = UZ + 30 + Radius;
        for (float Sg : {-1.f, 1.f})
        {
            const float C = Centre + Sg * Half * .5f;
            M.Saws.Add({bAlongS ? FVector(Line_, C, AxleZ) : FVector(C, Line_, AxleZ), Route, Radius, Lift, Period, Phase, Ceiling, bDrops});
        }
        // The slot it hangs from: a dark cut across the ceiling.
        if (bAlongS) QuadToward(P.Dark, {Line_ - 4, Centre - Half, Ceiling - .3f}, {Line_ + 4, Centre - Half, Ceiling - .3f}, {Line_ + 4, Centre + Half, Ceiling - .3f}, {Line_ - 4, Centre + Half, Ceiling - .3f}, {Line_, Centre, Ceiling - 100});
        else QuadToward(P.Dark, {Centre - Half, Line_ - 4, Ceiling - .3f}, {Centre - Half, Line_ + 4, Ceiling - .3f}, {Centre + Half, Line_ + 4, Ceiling - .3f}, {Centre + Half, Line_ - 4, Ceiling - .3f}, {Centre, Line_, Ceiling - 100});
    }
    void SawPassage(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, FRunMovers& M, FRandomStream& R)
    {
        FLining L; L.Dado = 90; L.Frieze = 0;
        Line(P, ES0, ES1, ET0, ET1, UZ, ETop, {{3, DS0, DS1, UZ, ETop}, {3, FS0, FS1, UZ, ETop}}, L);
        const float Mid = (ET0 + ET1) * .5f, Half = (ET1 - ET0) * .5f;
        const float FirstDrop = SawRows[3].S;
        for (const FSawRow& Row : SawRows)
        {
            // A dropping row is reached mid-way through its raised spell, in turn at run pace.
            const float Phase = Row.bDrops ? -SawArrive + (Row.S - FirstDrop) / RunPace : 0.f;
            SawRowDiscs(P, M, Row.S, true, Mid, Half, SawRadius, ETop, Row.bDrops, SawLift, 2.4f, Phase);
            // Iron straps on the walls either side of the row: the track it runs in.
            for (float Sg : {-1.f, 1.f})
            {
                const float W = Sg < 0 ? ET0 : ET1;
                Box(P.Iron, Row.S - 9, Row.S + 9, W - Sg * .5f, W - Sg * 3.5f, UZ + 4, ETop);
            }
        }
        for (float S : {71250.f, 71560.f, 72000.f, 72620.f, 73450.f}) for (float Sg : {-1.f, 1.f})
        {
            const float U = S + (Sg < 0 ? 0 : 110);
            if (Sg > 0 && ((U > DS0 - 60 && U < DS1 + 60) || (U > FS0 - 60 && U < FS1 + 60))) continue;   // the doors in the +T wall
            Torch(P, Fires, 1, Sg < 0 ? ET0 : ET1, -Sg, U, UZ + 170, 3200, 560);
        }
        Jars.Add({FVector(ES0 + 40, ET0 + 40, UZ), float(R.FRandRange(0, 360)), true, 3});
    }

    // ---- 5. The gauntlet, all four on one beat; 6. the antechamber.
    // A runner leaving the first pit's lip on the beat reaches each trap here this many seconds later.
    float GauntletArrive(float T) { return (T - GPits[0].T0) / RunPace; }
    void Gauntlet(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, TArray<FIdol>& Idols, FRunMovers& M, FRandomStream& R)
    {
        const float Mid = (FS0 + FS1) * .5f;
        const FVector Route(0, 1, 0);
        // F1: from the saw passage to the axe chamber, over the first pit.
        {
            FLining L; L.Dado = 120;
            L.FloorHoles = {FBox2D(FVector2D(FS0, GPits[0].T0), FVector2D(FS1, GPits[0].T1))};
            Line(P, FS0, FS1, FT0, GAxeT0, UZ, FTop, {{2, FS0, FS1, UZ, ETop}, {3, FS0, FS1, UZ, FTop}}, L);
            SpikePit(P, Skulls, FS0, FS1, GPits[0].T0, GPits[0].T1, PitDepth, R);
            Torch(P, Fires, 0, FS0, 1, -7600, UZ + 220, 3800, 640);
            Torch(P, Fires, 0, FS1, -1, -6950, UZ + 220, 3800, 640);
        }
        // F2: the axe chamber: a causeway between trenches, two axes.
        {
            FLining L; L.Dado = 160; L.Frieze = 70; L.Top = &P.Moss;
            const float C0 = Mid - 80, C1 = Mid + 80, A0 = GAxeT0 + 50, A1 = GAxeT1 - 50;
            L.FloorHoles = {FBox2D(FVector2D(GAxeS0, A0), FVector2D(C0, A1)), FBox2D(FVector2D(C1, A0), FVector2D(GAxeS1, A1))};
            Line(P, GAxeS0, GAxeS1, GAxeT0, GAxeT1, UZ, UZ + 420, {{2, FS0, FS1, UZ, FTop}, {3, FS0, FS1, UZ, GSawTop}}, L);
            for (const FBox2D& Trench : L.FloorHoles)
            {
                FLining Tl; Tl.bCeiling = false; Tl.Dado = 0;
                Line(P, Trench.Min.X, Trench.Max.X, Trench.Min.Y, Trench.Max.Y, UZ - TrenchDepth, UZ, {}, Tl);
                SpikeBed(P, Trench.Min.X + 2, Trench.Max.X - 2, Trench.Min.Y + 2, Trench.Max.Y - 2, UZ - TrenchDepth, 52, R);
            }
            for (float S : {C0, C1 - 8}) QuadToward(P.StepMoss, {S, A0, UZ + .5f}, {S + 8, A0, UZ + .5f}, {S + 8, A1, UZ + .5f}, {S, A1, UZ + .5f}, {S + 4, A0, UZ + 100});
            for (float T : GAxeT)
            {
                Box(P.Stone, GAxeS0, GAxeS1, T - 32, T + 32, UZ + 408, UZ + 420);
                Box(P.Iron, Mid - 16, Mid + 16, T - 18, T + 18, AxePivotZ + 8, UZ + 408);
                M.Axes.Add({FVector(Mid, T, AxePivotZ), Route, AxeArm, AxeAmplitude, GauntletBeat, GauntletArrive(T) - AxeArrive});
            }
            for (float Sg : {-1.f, 1.f}) for (float T : {-6700.f, -6200.f}) Torch(P, Fires, 0, Sg < 0 ? GAxeS0 : GAxeS1, -Sg, T, UZ + 250, 4000, 680);
        }
        // F3: the saws, under a low ceiling.
        {
            FLining L; L.Dado = 90;
            Line(P, FS0, FS1, GSawT0, GSawT1, UZ, GSawTop, {{2, FS0, FS1, UZ, GSawTop}, {3, FS0, FS1, UZ, GSawTop}}, L);
            for (const FGSaw& Sw : GSaws)
            {
                SawRowDiscs(P, M, Sw.T, false, Mid, (FS1 - FS0) * .5f, GSawRadius, GSawTop, Sw.bDrops, 100, GauntletBeat, GauntletArrive(Sw.T) - SawArrive);
                for (float Sg : {-1.f, 1.f}) { const float W = Sg < 0 ? FS0 : FS1; Box(P.Iron, W - Sg * .5f, W - Sg * 3.5f, Sw.T - 9, Sw.T + 9, UZ + 4, GSawTop); }
            }
            Torch(P, Fires, 0, FS0, 1, -5750, UZ + 170, 3000, 520);
        }
        // F4: fire, then the last pit (the chained wall run), into the antechamber.
        {
            FLining L; L.Dado = 120;
            L.FloorHoles = {FBox2D(FVector2D(FS0, GPits[1].T0), FVector2D(FS1, GPits[1].T1))};
            Line(P, FS0, FS1, GSawT1, FT1, UZ, FTop, {{2, FS0, FS1, UZ, GSawTop}, {3, FS0, FS1, UZ, FTop}}, L);
            SpikePit(P, Skulls, FS0, FS1, GPits[1].T0, GPits[1].T1, PitDepth, R);
            for (int32 I = 0; I < UE_ARRAY_COUNT(GJetT); ++I)
            {
                const bool Plus = I % 2 == 0;
                FireMouth(P, M, Plus ? FS1 : FS0, Plus ? -1.f : 1.f, GJetT[I], FS1 - FS0 - 12, GauntletBeat, GauntletArrive(GJetT[I]) - (.6f + 1.f + JetAfter), .6f, 1.f, Route);
            }
            Torch(P, Fires, 0, FS0, 1, -5200, UZ + 220, 3800, 640);
            Torch(P, Fires, 0, FS1, -1, -4650, UZ + 220, 3800, 640);
            Torch(P, Fires, 0, FS0, 1, -3950, UZ + 220, 3800, 640);
        }
        // The antechamber: rest, pots, two serpent idols, the way on. Choked until the Astral wind
        // (October 22, ChultTempleWindBuild.inl): its fall is cleared to heaps against the walls.
        {
            FLining L; L.Dado = 160; L.Frieze = 100; L.Top = &P.Plaster;
            Line(P, AnteS0, AnteS1, AnteT0, AnteT1, UZ, AnteTop, {{2, FS0, FS1, UZ, FTop}, {3, FS0, FS1, UZ, UZ + 280}}, L);
            Line(P, FS0, FS1, AnteT1, AnteT1 + 380, UZ, UZ + 280, {{2, FS0, FS1, UZ, UZ + 280}, {3, FS0, FS1, UZ, UZ + 280}}, FLining());
            for (float Sg : {-1.f, 1.f}) Box(P.Stone, Sg < 0 ? FS0 : FS1 - 30, Sg < 0 ? FS0 + 30 : FS1, AnteT1 - 30, AnteT1 + 30, UZ, UZ + 280);
            Box(P.Plaster, FS0 + 30, FS1 - 30, AnteT1 - 36, AnteT1 - 30, UZ + 260, UZ + 280);
            // What is left of the fall: low heaps along both walls (visual only), a broken lintel.
            for (int32 I = 0; I < 14; ++I)
            {
                const float Sg = I % 2 ? 1.f : -1.f, T = R.FRandRange(AnteT1 + 50, AnteT1 + 350), H = R.FRandRange(10, 26);
                const float S = Sg < 0 ? R.FRandRange(FS0 + 12, FS0 + 34) : R.FRandRange(FS1 - 34, FS1 - 12);
                TurnedBox(R.FRand() < .3f ? P.StepMoss : P.Steps, FVector(S, T, UZ + H * .45f), FVector(R.FRandRange(14, 26), R.FRandRange(14, 30), H * .5f), FRotator(R.FRandRange(-20, 20), R.FRandRange(0, 360), R.FRandRange(-20, 20)));
            }
            TurnedBox(P.Steps, FVector(FS0 + 30, AnteT1 + 200, UZ + 16), FVector(18, 110, 14), FRotator(4, 3, -9));
            for (float Sg : {-1.f, 1.f})
            {
                const float S = Mid + Sg * 160, T = AnteT1 - 90;
                Box(P.Stone, S - 45, S + 45, T - 45, T + 45, UZ, UZ + 85); Box(P.Stone, S - 52, S + 52, T - 52, T + 52, UZ + 85, UZ + 93);
                Idols.Add({FVector(S, T, UZ + 93), YawOf(0, -1), 20});
                Torch(P, Fires, 1, AnteT1, -1, S + Sg * 70, UZ + 240, 3800, 640);
            }
            Torch(P, Fires, 0, AnteS0, 1, -3500, UZ + 230, 3800, 640);
            Torch(P, Fires, 0, AnteS1, -1, -3500, UZ + 230, 3800, 640);
            Glyphs(P, R, 1, AnteT1, -1, Mid - 90, Mid + 90, UZ + 300, UZ + 400);
            Jars.Add({FVector(AnteS0 + 45, AnteT0 + 60, UZ), float(R.FRandRange(0, 360)), true, 4});
            Jars.Add({FVector(AnteS1 - 45, AnteT0 + 60, UZ), float(R.FRandRange(0, 360)), false, 2});
            Jars.Add({FVector(AnteS1 - 45, AnteT1 - 160, UZ), float(R.FRandRange(0, 360)), true, 3});
        }
    }

    void BuildRun(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, TArray<FIdol>& Idols, FRunMovers& M)
    {
        FRandomStream R(2026102201);
        Collapse(P, Fires, R);
        PitGallery(P, Fires, Jars, Skulls, R);
        AxeHall(P, Fires, Jars, Skulls, M, R);
        FireHall(P, Fires, Jars, M, R);
        SawPassage(P, Fires, Jars, M, R);
        Gauntlet(P, Fires, Jars, Skulls, Idols, M, R);
    }

    // ---- The moving parts' meshes, local to their pivot / axle / mouth.
    void LocalQuad(FSurf& M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Toward)
    {
        if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Toward - A) >= 0) M.Quad(A, B, C, D); else M.Quad(D, C, B, A);
    }
    void LocalBox(FSurf& M, const FVector& Lo, const FVector& Hi)
    {
        const FVector C = (Lo + Hi) * .5f;
        auto V = [&](int32 X, int32 Y, int32 Z) { return FVector(X ? Hi.X : Lo.X, Y ? Hi.Y : Lo.Y, Z ? Hi.Z : Lo.Z); };
        const int32 F[6][4][3] = {{{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{0,0,0},{0,1,0},{1,1,0},{1,0,0}}, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
            {{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, {{1,0,0},{1,1,0},{1,1,1},{1,0,1}}};
        for (const auto& Face : F)
        {
            const FVector A = V(Face[0][0], Face[0][1], Face[0][2]), B = V(Face[1][0], Face[1][1], Face[1][2]), Cc = V(Face[2][0], Face[2][1], Face[2][2]), D = V(Face[3][0], Face[3][1], Face[3][2]);
            LocalQuad(M, A, B, Cc, D, (A + Cc) * .5f * 2.f - C);
        }
    }
    // A great axe hanging from its pivot (the origin): X along the route, Y across, Z up. The
    // crescent's back and edge as AChultTempleTraps::AxePoints samples them.
    void AxeMesh(FSurf& Blade, FSurf& Haft, float Arm)
    {
        float MidBack, MidEdge; AChultTempleTraps::AxeBladeRadii(0.f, Arm, MidBack, MidEdge);
        LocalBox(Haft, FVector(-4, -5, -(MidBack + 6)), FVector(4, 5, -12));
        LocalBox(Blade, FVector(-11, -11, -11), FVector(11, 11, 11));   // the hub
        const float Span = AChultTempleTraps::AxeBladeHalf / Arm;
        const int32 N = 12;
        TArray<FVector> Back, Edge;
        for (int32 K = 0; K <= N; ++K)
        {
            const float U = -1.f + 2.f * K / N, A = U * Span;
            float Rb, Re; AChultTempleTraps::AxeBladeRadii(U, Arm, Rb, Re);
            Back.Add(FVector(0, FMath::Sin(A) * Rb, -FMath::Cos(A) * Rb));
            Edge.Add(FVector(0, FMath::Sin(A) * Re, -FMath::Cos(A) * Re));
        }
        for (int32 K = 0; K < N; ++K)
            for (float Side : {-1.f, 1.f})
            {
                // Faces: thick at the back (3 cm), thin at the edge (0.6 cm).
                const FVector B0 = Back[K] + FVector(3 * Side, 0, 0), B1 = Back[K + 1] + FVector(3 * Side, 0, 0);
                const FVector E0 = Edge[K] + FVector(.6f * Side, 0, 0), E1 = Edge[K + 1] + FVector(.6f * Side, 0, 0);
                LocalQuad(Blade, B0, B1, E1, E0, (B0 + E1) * .5f + FVector(Side * 50, 0, 0));
                // The edge and the back.
                if (Side > 0)
                {
                    const FVector Mid = (Back[K] + Edge[K]) * .5f;
                    LocalQuad(Blade, Edge[K] + FVector(.6f, 0, 0), Edge[K + 1] + FVector(.6f, 0, 0), Edge[K + 1] - FVector(.6f, 0, 0), Edge[K] - FVector(.6f, 0, 0), Edge[K] + (Edge[K] - Mid) * 2.f);
                    LocalQuad(Blade, Back[K] + FVector(3, 0, 0), Back[K + 1] + FVector(3, 0, 0), Back[K + 1] - FVector(3, 0, 0), Back[K] - FVector(3, 0, 0), Back[K] - (Edge[K] - Back[K]) * 2.f);
                }
            }
        // The blade's two horns' ends.
        for (int32 K : {0, N}) LocalQuad(Blade, Back[K] + FVector(3, 0, 0), Edge[K] + FVector(.6f, 0, 0), Edge[K] - FVector(.6f, 0, 0), Back[K] - FVector(3, 0, 0), (Back[K] + Edge[K]) * .5f + FVector(0, K ? 50 : -50, 0));
        // A collar where the haft meets the blade.
        LocalBox(Blade, FVector(-6, -9, -(MidBack + 10)), FVector(6, 9, -(MidBack - 10)));
    }
    // A saw disc about its axle (the origin, along X): toothed rim, a hub; and its hanger.
    void SawMesh(FSurf& Disc, float Radius)
    {
        const int32 Teeth = 30;
        const float Inner = Radius - 9.f, Hub = 12.f;
        for (int32 K = 0; K < Teeth; ++K)
        {
            const float A0 = K * 2 * PI / Teeth, A1 = (K + 1) * 2 * PI / Teeth, Am = A0 + (A1 - A0) * .72f;
            auto P = [](float A, float R, float X) { return FVector(X, FMath::Cos(A) * R, FMath::Sin(A) * R); };
            for (float Side : {-1.f, 1.f})
            {
                const float X = Side * 1.1f;
                LocalQuad(Disc, P(A0, Hub, X), P(A1, Hub, X), P(A1, Inner, X), P(A0, Inner, X), P(A0, Inner, X) + FVector(Side * 50, 0, 0));
                // The tooth: up from the rim to its point, raked forward.
                LocalQuad(Disc, P(A0, Inner, X), P(A1, Inner, X), P(Am, Radius, X), P(Am, Radius, X), P(Am, Inner, X) + FVector(Side * 50, 0, 0));
            }
            LocalQuad(Disc, P(A0, Inner, 1.1f), P(Am, Radius, 1.1f), P(Am, Radius, -1.1f), P(A0, Inner, -1.1f), P((A0 + Am) * .5f, Radius * 2, 0));
            LocalQuad(Disc, P(Am, Radius, 1.1f), P(A1, Inner, 1.1f), P(A1, Inner, -1.1f), P(Am, Radius, -1.1f), P((A1 + Am) * .5f, Radius * 2, 0));
        }
        LocalBox(Disc, FVector(-5, -9, -9), FVector(5, 9, 9));
    }
    void SawHanger(FSurf& Arm, float Length)
    {
        // Two straps either side of the disc from its hub up through the ceiling slot.
        for (float Side : {-1.f, 1.f}) LocalBox(Arm, FVector(Side * 4 - 1.5f, -4, -4), FVector(Side * 4 + 1.5f, 4, Length));
    }
}
