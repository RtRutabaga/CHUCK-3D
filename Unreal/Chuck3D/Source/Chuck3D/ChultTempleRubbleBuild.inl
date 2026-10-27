// The rubble map (Claude, 2026-10-26; ChultTempleRubble.h, docs/CHULT-TEMPLE-RUBBLE.md). Included into
// ChultTempleInterior.cpp's anonymous namespace after ChultTempleWindBuild.inl: frame coordinates, faces looking inward.
//
// A chamber of the temple's own masonry whose vault has come down: the corbelled walls stand, the ceiling has a great
// ragged hole in it with earth and roots over it, and the floor is heaped with what fell (broken blocks, slabs of the
// vault, column drums) and torn through by the Astral (the sanctum's ruptures). A lane stays clear, lit by braziers
// and wall torches, to the crevice low in the -S wall: a crawl, lined in rough stone, bending once, ending at a crack
// with warm light and old timber behind it (the ship, later).

namespace TempleRubbleBuild
{
    using namespace TempleRubble;
    constexpr float FloorThick = 30;

    bool CaveAt(float S, float T, float Grow = 0) { return ChultTempleRubbleCaveAt(S, T, Grow) >= 0; }

    // Where the floor's kind changes along a ray from From outward (the rubble's ruptures).
    FVector2D EdgeAlong(const FVector2D& From, const FVector2D& Dir, float Max)
    {
        const bool Start = ChultTempleRubbleHoleAt(From) != 0;
        float A = 0, B = Max;
        for (float D = 5; D <= Max; D += 5) if ((ChultTempleRubbleHoleAt(From + Dir * D) != 0) != Start) { B = D; A = D - 5; break; }
        for (int32 K = 0; K < 10; ++K) { const float M = (A + B) * .5f; if ((ChultTempleRubbleHoleAt(From + Dir * M) != 0) == Start) A = M; else B = M; }
        return From + Dir * ((A + B) * .5f);
    }

    // The floor over B in 10 cm cells, the ruptures left out: tops, undersides, the 30 cm edge (the sanctum's BrokenFloor).
    void BrokenFloor(FParts& P, const FBox2D& B, float Z)
    {
        constexpr float C = 10;
        const int32 NS = FMath::RoundToInt((B.Max.X - B.Min.X) / C), NT = FMath::RoundToInt((B.Max.Y - B.Min.Y) / C);
        TArray<uint8> Solid; Solid.SetNumZeroed(NS * NT);
        for (int32 I = 0; I < NS; ++I) for (int32 J = 0; J < NT; ++J) Solid[I * NT + J] = ChultTempleRubbleHoleAt(B.Min + FVector2D(I * C + 5, J * C + 5)) == 0;
        auto Floor_ = [&](int32 I, int32 J) { return I < 0 || J < 0 || I >= NS || J >= NT || Solid[I * NT + J] != 0; };
        const float Lo = Z - FloorThick;
        for (int32 I = 0; I < NS; ++I)
        {
            const float S0 = B.Min.X + I * C, S1 = S0 + C, SM = (S0 + S1) * .5f;
            for (int32 J = 0; J < NT;)
            {
                if (!Solid[I * NT + J]) { ++J; continue; }
                int32 K = J; while (K < NT && Solid[I * NT + K]) ++K;
                const float T0 = B.Min.Y + J * C, T1 = B.Min.Y + K * C, TM = (T0 + T1) * .5f;
                QuadToward(P.Stone, {S0, T0, Z}, {S1, T0, Z}, {S1, T1, Z}, {S0, T1, Z}, {SM, TM, Z + 100});
                QuadToward(P.Steps, {S0, T0, Lo}, {S1, T0, Lo}, {S1, T1, Lo}, {S0, T1, Lo}, {SM, TM, Lo - 100});
                if (!Floor_(I, J - 1)) QuadToward(P.Steps, {S0, T0, Lo}, {S1, T0, Lo}, {S1, T0, Z}, {S0, T0, Z}, {SM, T0 - 100, Z - 15});
                if (!Floor_(I, K)) QuadToward(P.Steps, {S0, T1, Lo}, {S1, T1, Lo}, {S1, T1, Z}, {S0, T1, Z}, {SM, T1 + 100, Z - 15});
                J = K;
            }
            for (int32 Side : {-1, 1})
                for (int32 J = 0; J < NT;)
                {
                    if (!(Solid[I * NT + J] && !Floor_(I + Side, J))) { ++J; continue; }
                    int32 K = J; while (K < NT && Solid[I * NT + K] && !Floor_(I + Side, K)) ++K;
                    const float S = Side < 0 ? S0 : S1, T0 = B.Min.Y + J * C, T1 = B.Min.Y + K * C;
                    QuadToward(P.Steps, {S, T0, Lo}, {S, T1, Lo}, {S, T1, Z}, {S, T0, Z}, {S + Side * 100, (T0 + T1) * .5f, Z - 15});
                    J = K;
                }
        }
    }

    // The broken vault: the ceiling between the corbels in 40 cm cells with the collapses' holes left out, each hole's edge
    // 40 cm thick; over each a cavity of earth (lined in the mossy stone) with roots hanging through.
    void BrokenVault(FParts& P, float S0, float S1, float T0, float T1, float Z, FRandomStream& R)
    {
        constexpr float C = 40, Thick = 40;
        const int32 NS = FMath::RoundToInt((S1 - S0) / C), NT = FMath::RoundToInt((T1 - T0) / C);
        auto Solid = [&](int32 I, int32 J) { return I < 0 || J < 0 || I >= NS || J >= NT || !CaveAt(S0 + I * C + C * .5f, T0 + J * C + C * .5f); };
        for (int32 I = 0; I < NS; ++I)
        {
            const float A0 = S0 + I * C, A1 = A0 + C;
            for (int32 J = 0; J < NT;)
            {
                if (!Solid(I, J)) { ++J; continue; }
                int32 K = J; while (K < NT && Solid(I, K)) ++K;
                const float B0 = T0 + J * C, B1 = T0 + K * C;
                QuadToward(P.Stone, {A0, B0, Z}, {A1, B0, Z}, {A1, B1, Z}, {A0, B1, Z}, {(A0 + A1) * .5f, (B0 + B1) * .5f, Z - 100});
                J = K;
            }
            // The hole's edge: faces down into it from the solid cells, and the broken course's thickness.
            for (int32 J = 0; J < NT; ++J)
            {
                if (!Solid(I, J)) continue;
                const float B0 = T0 + J * C, B1 = B0 + C;
                if (!Solid(I, J - 1)) QuadToward(P.Steps, {A0, B0, Z}, {A1, B0, Z}, {A1, B0, Z + Thick}, {A0, B0, Z + Thick}, {(A0 + A1) * .5f, B0 - 100, Z + 20});
                if (!Solid(I, J + 1)) QuadToward(P.Steps, {A0, B1, Z}, {A1, B1, Z}, {A1, B1, Z + Thick}, {A0, B1, Z + Thick}, {(A0 + A1) * .5f, B1 + 100, Z + 20});
                if (!Solid(I - 1, J)) QuadToward(P.Steps, {A0, B0, Z}, {A0, B1, Z}, {A0, B1, Z + Thick}, {A0, B0, Z + Thick}, {A0 - 100, (B0 + B1) * .5f, Z + 20});
                if (!Solid(I + 1, J)) QuadToward(P.Steps, {A1, B0, Z}, {A1, B1, Z}, {A1, B1, Z + Thick}, {A1, B0, Z + Thick}, {A1 + 100, (B0 + B1) * .5f, Z + 20});
            }
        }
        for (int32 K = 0; K < int32(UE_ARRAY_COUNT(Caves)); ++K)
        {
            const FCave& Cv = Caves[K];
            const float X0 = Cv.S - Cv.RS * CaveRoom, X1 = Cv.S + Cv.RS * CaveRoom, Y0 = Cv.T - Cv.RT * CaveRoom, Y1 = Cv.T + Cv.RT * CaveRoom;
            // Broken courses hanging at the rim (visual).
            for (int32 Q = 0; Q < 10 + int32((Cv.RS + Cv.RT) / 60); ++Q)
            {
                const float A = R.FRandRange(0, UE_TWO_PI);
                FVector2D E(Cv.S, Cv.T);
                for (float D = 0; D < 1200; D += 10) { const FVector2D Q2(Cv.S + FMath::Cos(A) * D, Cv.T + FMath::Sin(A) * D); if (ChultTempleRubbleCaveAt(Q2.X, Q2.Y) != K) { E = Q2; break; } }
                TurnedBox(P.Steps, FVector(E.X, E.Y, Z - R.FRandRange(10, 30)), FVector(R.FRandRange(20, 45), R.FRandRange(14, 30), R.FRandRange(10, 22)),
                    FRotator(R.FRandRange(-25, 25), FMath::RadiansToDegrees(A) + R.FRandRange(-30, 30), R.FRandRange(-25, 25)));
            }
            // The cavity over it: earth, lined in the mossy stone, its top lost in the unlit black; roots hang through the hole.
            const float CZ = Z + 420 + Cv.RS * .2f;
            FLining Earth; Earth.bFloor = false; Earth.bCeiling = false; Earth.Dado = 0; Earth.Low = &P.Moss; Earth.High = &P.Moss;
            Line(P, X0, X1, Y0, Y1, Z + Thick, CZ, {}, Earth);
            QuadToward(P.Dark, {X0, Y0, CZ}, {X1, Y0, CZ}, {X1, Y1, CZ}, {X0, Y1, CZ}, {Cv.S, Cv.T, CZ - 100});
            // Its floor ring (from the vault's top to the cavity's walls), so nothing is seen through.
            const int32 NX = FMath::CeilToInt((X1 - X0) / C), NY = FMath::CeilToInt((Y1 - Y0) / C);
            for (int32 I = 0; I < NX; ++I) for (int32 J = 0; J < NY; ++J)
            {
                const float A0 = X0 + I * C, B0 = Y0 + J * C;
                if (ChultTempleRubbleCaveAt(A0 + C * .5f, B0 + C * .5f) == K) continue;
                QuadToward(P.Moss, {A0, B0, Z + Thick}, {A0 + C, B0, Z + Thick}, {A0 + C, B0 + C, Z + Thick}, {A0, B0 + C, Z + Thick}, {A0 + C * .5f, B0 + C * .5f, Z + 200});
            }
            for (int32 Q = 0; Q < 4 + int32((Cv.RS + Cv.RT) / 110); ++Q)
            {
                const float A = R.FRandRange(0, UE_TWO_PI), D = R.FRandRange(.1f, .85f);
                TArray<FVector> Path = {FVector(Cv.S + FMath::Cos(A) * Cv.RS * D, Cv.T + FMath::Sin(A) * Cv.RT * D, CZ)};
                const float Len = R.FRandRange(400, 900);
                FVector Dir(R.FRandRange(-.2f, .2f), R.FRandRange(-.2f, .2f), -1);
                for (int32 I = 1; I <= 8; ++I)
                {
                    Dir = (Dir + FVector(R.FRandRange(-.25f, .25f), R.FRandRange(-.25f, .25f), 0)).GetSafeNormal();
                    Path.Add(Path.Last() + Dir * (Len / 8));
                }
                PathTube(P.Moss, Path, R.FRandRange(4, 9), 1.f, 6);
            }
        }
    }

    // Chips of broken flag round a rupture's rim (visual).
    void RimChips(FParts& P, FRandomStream& R, const TArray<FVector2D>& Rim, const FVector2D& Centre, int32 Count)
    {
        for (int32 I = 0; I < Count && Rim.Num(); ++I)
        {
            const FVector2D E = Rim[R.RandRange(0, Rim.Num() - 1)], Out = (E - Centre).GetSafeNormal();
            const FVector2D At_ = E + Out * R.FRandRange(9, 26);
            if (ChultTempleRubbleHoleAt(At_, 6)) continue;
            TurnedBox(R.FRand() < .3f ? P.Dark : P.Steps, FVector(At_.X, At_.Y, RZ + 1.5f), FVector(R.FRandRange(3, 8), R.FRandRange(2.5f, 6), R.FRandRange(1, 2.5f)), FRotator(R.FRandRange(-8, 8), R.FRandRange(0, 180), R.FRandRange(-8, 8)));
        }
    }

    // The crevice: two runs lined in rough stone (straight faces he crawls along; lumps on them are visual), the mouth's
    // lintel and jambs in the chamber, the crack at its end with old timber and lamplight behind it.
    void Crevice(FParts& P, TArray<FGlow>& Glows, FRandomStream& R)
    {
        const float A0 = CrawlBendS + CrawlHalf, AT0 = CrawlT - CrawlHalf, AT1 = CrawlT + CrawlHalf;
        const float BS0 = CrawlBendS - CrawlHalf, BS1 = CrawlBendS + CrawlHalf;
        FLining L; L.Dado = 0; L.Low = &P.Stone; L.High = &P.Stone;
        // Run A, from the mouth (+S, open on the chamber) to the bend (-S, open on run B).
        Line(P, A0, RS0, AT0, AT1, RZ, RZ + CrawlH, {{0, AT0, AT1, RZ, RZ + CrawlH}, {1, AT0, AT1, RZ, RZ + CrawlH}}, L);
        // Through the pile before the wall (2026-10-26) the chamber's own floor runs on; the pile's blocks are its walls and roof.
        // Run B, from the bend (its +S wall open on run A) to its end: a crack in the end wall.
        Line(P, BS0, BS1, CrawlEndT, AT1, RZ, RZ + CrawlH, {{1, AT0, AT1, RZ, RZ + CrawlH}, {2, CrawlBendS - 7, CrawlBendS + 6, RZ + 5, RZ + 37}}, L);
        // Lumps and spalls on the walls and roof, a few pebbles on the floor (visual only).
        auto Lumps = [&](float S0, float S1, float T0, float T1, int32 Count)
        {
            for (int32 I = 0; I < Count; ++I)
            {
                const int32 Face = R.RandRange(0, 2);   // 0/1 the long walls, 2 the roof
                const bool AlongS = S1 - S0 > T1 - T0;
                const float U = AlongS ? R.FRandRange(S0 + 10, S1 - 10) : R.FRandRange(T0 + 10, T1 - 10);
                FVector C;
                if (Face == 2) C = AlongS ? FVector(U, R.FRandRange(T0 + 12, T1 - 12), RZ + CrawlH + 2) : FVector(R.FRandRange(S0 + 12, S1 - 12), U, RZ + CrawlH + 2);
                else if (AlongS) C = FVector(U, Face ? T1 + 2 : T0 - 2, R.FRandRange(RZ + 6, RZ + CrawlH - 4));
                else C = FVector(Face ? S1 + 2 : S0 - 2, U, R.FRandRange(RZ + 6, RZ + CrawlH - 4));
                TurnedBox(R.FRand() < .35f ? P.StepMoss : P.Steps, C, FVector(R.FRandRange(6, 14), R.FRandRange(5, 11), R.FRandRange(3, 6)), FRotator(R.FRandRange(-20, 20), R.FRandRange(0, 360), R.FRandRange(-20, 20)));
            }
            for (int32 I = 0; I < Count / 2; ++I)
                TurnedBox(P.Steps, FVector(R.FRandRange(S0 + 8, S1 - 8), R.FRandRange(T0 + 8, T1 - 8), RZ + .8f), FVector(R.FRandRange(1.5f, 4), R.FRandRange(1.5f, 3), 1.2f), FRotator(0, R.FRandRange(0, 360), 0));
        };
        Lumps(A0, RS0, AT0, AT1, 40);
        Lumps(BS0, BS1, CrawlEndT, AT0, 30);
        // The mouth: a spill of small stones and grit out of the gap in the pile (its blocks are the rubble's own).
        for (int32 I = 0; I < 14; ++I)
        {
            const float T = CrawlT + R.FRandRange(-170.f, 170.f);
            if (FMath::Abs(T - CrawlT) < 100) continue;   // (the apron he crawls in from stays clear)
            TurnedBox(P.Steps, FVector(PileS1 + R.FRandRange(-10, 90), T, RZ + 2), FVector(R.FRandRange(3, 10), R.FRandRange(3, 8), R.FRandRange(2, 5)), FRotator(R.FRandRange(-12, 12), R.FRandRange(0, 360), R.FRandRange(-12, 12)));
        }
        // Behind the crack: a hollow of old planks (the ship's timbers, for later) lit warm.
        {
            const float H0 = CrawlEndT - 70, H1 = CrawlEndT;
            FLining W; W.Dado = 0; W.Low = &P.Wood; W.High = &P.Wood; W.bFloor = true; W.bCeiling = true;
            Line(P, CrawlBendS - 45, CrawlBendS + 45, H0, H1, RZ - 10, RZ + 60, {{3, CrawlBendS - 7, CrawlBendS + 6, RZ + 5, RZ + 37}}, W);
            // Planks across it (the boards' seams), dark gaps between.
            for (float Z = RZ - 4; Z < RZ + 56; Z += 14) Box(P.Dark, CrawlBendS - 44, CrawlBendS + 44, H0 + .5f, H0 + 1.2f, Z, Z + 1.2f);
        }
        Glows.Add({FVector(CrawlBendS, CrawlEndT - 30, RZ + 25), 700.f, 240.f, FLinearColor(1.f, .72f, .4f)});
    }

    int32 BuildRubble(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, TArray<FGlow>& Glows)
    {
        FRandomStream R(2026102602);
        // The ruptures' boxes, snapped to the 10 cm grid: the floor is whole outside them.
        TArray<FBox2D> HoleBoxes;
        for (const FRift& Rf : Rifts)
        {
            const float GS = Rf.RS * 1.3f + 20, GT = Rf.RT * 1.3f + 20;
            HoleBoxes.Add(FBox2D(FVector2D(FMath::FloorToFloat((Rf.S - GS) / 10) * 10, FMath::FloorToFloat((Rf.T - GT) / 10) * 10), FVector2D(FMath::CeilToFloat((Rf.S + GS) / 10) * 10, FMath::CeilToFloat((Rf.T + GT) / 10) * 10)));
        }
        // The chamber's lining: the walls to the corbels, the crevice's mouth in the -S wall; the floor and vault apart.
        FLining L; L.Dado = 200; L.Frieze = 120; L.Top = &P.Moss; L.bFloor = true; L.bCeiling = false; L.FloorHoles = HoleBoxes;
        Line(P, RS0, RS1, RT0, RT1, RZ, RTop, {{0, CrawlT - CrawlHalf, CrawlT + CrawlHalf, RZ, RZ + CrawlH}}, L);
        constexpr int32 Courses = 5; constexpr float Step = 40, Course = 46;
        Corbel(P, 1, RS0, RS1, RT0, RT1, RTop, Courses, Step, Course);
        BrokenVault(P, RS0 + Courses * Step, RS1 - Courses * Step, RT0, RT1, RTop, R);
        // Pilasters down the long walls (the chamber's old order), some cracked off.
        for (float Sg : {-1.f, 1.f}) for (float T = RT0 + 450; T < RT1 - 300; T += 900)
        {
            const float W = Sg < 0 ? RS0 : RS1, In = -Sg;
            if (Sg < 0 && FMath::Abs(T - CrawlT) < 260) continue;
            const float Top_ = R.FRand() < .3f ? RZ + R.FRandRange(150, 400) : RTop - Courses * Course;
            Box(P.Stone, FMath::Min(W, W + In * 45), FMath::Max(W, W + In * 45), T - 70, T + 70, RZ, Top_);
            Box(P.Stone, FMath::Min(W, W + In * 62), FMath::Max(W, W + In * 62), T - 86, T + 86, RZ, RZ + 50);
        }
        // The broken floor round the ruptures, their wells, haze, chips and glow.
        for (const FBox2D& B : HoleBoxes) BrokenFloor(P, B, RZ);
        for (int32 K = 0; K < int32(UE_ARRAY_COUNT(Rifts)); ++K)
        {
            const FRift& Rf = Rifts[K];
            TempleWindBuild::Well(P.Astral, HoleBoxes[K], RZ - FloorThick, RZ - 330);
            TArray<FVector2D> Ring; const FVector2D C(Rf.S, Rf.T);
            for (int32 I = 0; I < 56; ++I) { const float A = I * UE_TWO_PI / 56; Ring.Add(EdgeAlong(C, FVector2D(FMath::Cos(A), FMath::Sin(A)), 420)); }
            TempleWindBuild::OilRing(P, Ring, true, RZ, 80, C, false);
            RimChips(P, R, Ring, C, 10);
            Glows.Add({FVector(Rf.S, Rf.T, RZ - 110), Rf.RS + Rf.RT > 350 ? 2400.f : 1600.f, (Rf.RS + Rf.RT) * 1.6f});
        }
        // The rubble: blocks (solid; mossy where they were the vault's), slabs and column drums.
        for (const FTempleRubbleBlock& B : ChultTempleRubbleBlocks()) TurnedBox(B.bMoss ? P.Moss : P.Stone, B.Centre, B.Half, B.Turn);
        for (const FTempleRubbleDrum& D : ChultTempleRubbleDrums())
        {
            Drum(P.Stone, D.A, D.B, D.Radius, 14);
            // Its broken ends: a rough cap a little proud of each.
            const FVector Ax = (D.B - D.A).GetSafeNormal();
            for (const FVector& E : {D.A, D.B}) TurnedBox(P.Steps, E + Ax * ((E == D.A) ? -2.f : 2.f), FVector(4, D.Radius * .8f, D.Radius * .7f), Ax.Rotation() + FRotator(0, 0, R.FRandRange(0, 90)));
        }
        // Gravel and spalls over the floor off the lane (visual): denser near the heaps.
        for (int32 I = 0; I < 520; ++I)
        {
            const FVector2D At_(R.FRandRange(RS0 + 15, RS1 - 15), R.FRandRange(RT0 + 15, RT1 - 15));
            if (ChultTempleRubbleHoleAt(At_, 12)) continue;
            const float Lane_ = ChultTempleRubbleLaneDistance(At_);
            if (Lane_ < LaneHalf - 10 && R.FRand() < .85f) continue;   // the lane mostly clear
            const float Big = Lane_ > LaneHalf + 60 && R.FRand() < .25f ? 2.2f : 1.f;
            TurnedBox(R.FRand() < .2f ? P.StepMoss : P.Steps, FVector(At_.X, At_.Y, RZ + 2.f * Big), FVector(R.FRandRange(3, 8), R.FRandRange(2.5f, 6), R.FRandRange(2.2f, 4.5f)) * Big,
                FRotator(R.FRandRange(-12, 12), R.FRandRange(0, 360), R.FRandRange(-12, 12)));
        }
        // Light: braziers along the lane, wall torches by it, a glyph panel over the crevice.
        for (const FVector2D& Bz : ChultTempleRubbleBraziers()) Brazier(P, Fires, Bz.X, Bz.Y, RZ);
        for (float T : {-5450.f, -6350.f, -7250.f, -8150.f}) Torch(P, Fires, 0, RS1, -1, T, RZ + 230, 3800, 800);
        for (float T : {-5750.f, -6950.f, -8000.f, -8500.f}) Torch(P, Fires, 0, RS0, 1, T, RZ + 230, 3800, 800);
        for (float S : {75100.f, 76100.f}) Torch(P, Fires, 1, RT0, 1, S, RZ + 260, 3800, 800);
        for (float S : {74900.f, 75900.f}) Torch(P, Fires, 1, RT1, -1, S, RZ + 260, 3800, 800);
        // Pots by the lane (cigarettes), old skulls.
        for (const FVector2D& J : ChultTempleRubbleJars()) Jars.Add({FVector(J.X, J.Y, RZ), float(R.FRandRange(0, 360)), false, 2});
        for (int32 I = 0, Made = 0; I < 60 && Made < 7; ++I)
        {
            const FVector2D At_(R.FRandRange(RS0 + 100, RS1 - 100), R.FRandRange(RT0 + 100, RT1 - 100));
            if (ChultTempleRubbleHoleAt(At_, 40) || ChultTempleRubbleLaneDistance(At_) < LaneHalf + 20) continue;
            Skulls.Add({FVector(At_.X, At_.Y, RZ + 6), FRotator(R.FRandRange(-15, 25), R.FRandRange(0, 360), R.FRandRange(-80, 80))}); ++Made;
        }
        Crevice(P, Glows, R);
        return 0;
    }
}
