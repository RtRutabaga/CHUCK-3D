// The Astral wind's tunnels and the sanctum (Claude, 2026-10-22; ChultTempleWind.h,
// docs/CHULT-TEMPLE-WIND.md). Included into ChultTempleInterior.cpp's anonymous namespace after
// ChultTempleRunBuild.inl: frame coordinates, faces looking inward, as the rest of the interior.
//
// The tunnels are older and rougher than the trap run: low mossy courses, corbelled, narrow. The
// ruptures are the sewer's (ChultWreck's form): a torn slit in the floor, its broken stone lip, the
// Astral glowing in it, oil-film veils rising off its long edges, a purple light. Each is at its own
// angle and off the middle, with cracks run out from its points into the flags, so they read as the
// floor torn open, not as cut pits. The spike pits are the opposite: square, dressed, deliberate.

struct FGlow { FVector At; float Intensity, Radius; };

namespace TempleWindBuild
{
    using namespace TempleWind;
    using TempleWind::UZ; using TempleWind::Top; using TempleWind::PitDepth; using TempleWind::Pits;

    // A tear's lip, the Astral in it, its veils, cracks out from its points, a few flakes of the floor
    // broken round it (visual), its light.
    void Tear(FParts& P, TArray<FGlow>& Glows, const FTempleWindTear& Tr, int32 K)
    {
        const int32 N = Tr.Edge.Num();
        TArray<FVector> E, Out;
        for (int32 I = 0; I < N; ++I)
        {
            const FVector2D D = (Tr.Edge[(I + 1) % N] - Tr.Edge[(I + N - 1) % N]).GetSafeNormal();
            FVector2D O(D.Y, -D.X); if (FVector2D::DotProduct(O, Tr.Edge[I] - Tr.C) < 0) O = -O;
            E.Add(FVector(Tr.Edge[I].X, Tr.Edge[I].Y, UZ)); Out.Add(FVector(O.X, O.Y, 0));
        }
        const FVector Centre(Tr.C.X, Tr.C.Y, UZ);
        for (int32 I = 0; I < N; ++I)
        {
            const int32 J = (I + 1) % N;
            // The broken flag edge: laps 13 cm over the cut floor, then breaks down inside, ragged.
            const FVector OA = E[I] + Out[I] * 13 + FVector(0, 0, .6f), OB = E[J] + Out[J] * 13 + FVector(0, 0, .6f);
            const FVector PA = E[I] + FVector(0, 0, 1.6f), PB = E[J] + FVector(0, 0, 1.6f);
            const FVector IA = E[I] - Out[I] * 4 - FVector(0, 0, 24 + 7 * FMath::Sin(I * 2.7f + K)), IB = E[J] - Out[J] * 4 - FVector(0, 0, 24 + 7 * FMath::Sin(J * 2.7f + K));
            QuadToward(P.Stone, OA, OB, PB, PA, (OA + PB) * .5f + FVector(0, 0, 100));
            QuadToward(P.Stone, PA, PB, IB, IA, (PA + IB) * .5f - (Out[I] + Out[J]) * 50);
            // The well below and the Astral's surface in it.
            const FVector WA = E[I] - FVector(0, 0, 18), WB = E[J] - FVector(0, 0, 18), BA = E[I] - FVector(0, 0, 300), BB = E[J] - FVector(0, 0, 300);
            QuadToward(P.Astral, WA, WB, BB, BA, (WA + BB) * .5f - (Out[I] + Out[J]) * 50);
            QuadToward(P.Astral, Centre - FVector(0, 0, 18), WA, WB, WB, Centre + FVector(0, 0, 100));
        }
        // Two upright, curved veils along the long edges, fading upward (the sewer's oil film).
        const float Wid = Tr.Radius > 0 ? FMath::Max(FVector2D::Distance(Tr.Edge[N / 4], Tr.Edge[N - N / 4]) * .5f, 15.f) : 20.f;
        const float Sc = FMath::Clamp(Wid / 58.f, .55f, 1.1f); constexpr int32 Levels = 8, Side = 18;
        for (int32 SideI = 0; SideI < 2; ++SideI)
        {
            TArray<int32> Idx;
            if (SideI == 0) for (int32 I = 0; I <= Side; ++I) Idx.Add(I);
            else { Idx.Add(Side); for (int32 I = Side + 1; I < N; ++I) Idx.Add(I); Idx.Add(0); }
            for (int32 Row = 1; Row < Idx.Num(); ++Row) for (int32 Lv = 0; Lv < Levels; ++Lv)
            {
                auto Pt = [&](int32 Rw, int32 Lev)
                {
                    const float U = Rw / float(Idx.Num() - 1), H = Lev / float(Levels);
                    const FVector Ed = E[Idx[Rw]], In = (Centre - Ed).GetSafeNormal2D();
                    return AtV(Ed + In * (H * H * 12 * Sc * FMath::Sin(U * 13 + SideI)) + FVector(0, 0, 3 + H * Sc * (72 + 10 * FMath::Sin(U * 11 + SideI))));
                };
                const FVector A = Pt(Row - 1, Lv), B = Pt(Row, Lv), C = Pt(Row, Lv + 1), D = Pt(Row - 1, Lv + 1);
                const float U0 = (Row - 1) / float(Idx.Num() - 1), U1 = Row / float(Idx.Num() - 1), V0 = Lv / float(Levels), V1 = (Lv + 1) / float(Levels);
                // Both faces; U along the edge, V up (the oil film's shader fades with V).
                for (int32 Face = 0; Face < 2; ++Face)
                {
                    const int32 First = P.Veil.V.Num();
                    if (Face == 0) P.Veil.Quad(A, B, C, D); else P.Veil.Quad(A, D, C, B);
                    if (P.Veil.V.Num() == First) continue;
                    const FVector2D UV[4] = {{U0, V0}, Face == 0 ? FVector2D(U1, V0) : FVector2D(U0, V1), {U1, V1}, Face == 0 ? FVector2D(U0, V1) : FVector2D(U1, V0)};
                    for (int32 Q = 0; Q < 4; ++Q) P.Veil.UV[First + Q] = UV[Q];
                }
            }
        }
        // Cracks run out from its two points into the flags: thin dark jagged lines just over the floor.
        FRandomStream R(9100 + K);
        for (int32 Tip : {0, Side})
        {
            const FVector2D From = Tr.Edge[Tip], Away = (Tr.Edge[Tip] - Tr.C).GetSafeNormal();
            for (int32 Branch = 0; Branch < 2; ++Branch)
            {
                FVector2D At = From, Dir = Away.GetRotated(R.FRandRange(-35, 35) + (Branch ? R.FRandRange(25, 50) * (R.FRand() < .5f ? -1 : 1) : 0));
                const int32 Steps = R.RandRange(3, 6); float W = Branch ? 1.6f : 2.4f;
                for (int32 S = 0; S < Steps; ++S)
                {
                    const FVector2D Next = At + Dir * R.FRandRange(12, 24);
                    if (ChultTempleWindTearAt(Next, 2)) break;
                    const FVector2D Side_ = FVector2D(-Dir.Y, Dir.X) * W * .5f;
                    QuadToward(P.Dark, FVector(At.X - Side_.X, At.Y - Side_.Y, UZ + .35f), FVector(Next.X - Side_.X, Next.Y - Side_.Y, UZ + .35f),
                        FVector(Next.X + Side_.X, Next.Y + Side_.Y, UZ + .35f), FVector(At.X + Side_.X, At.Y + Side_.Y, UZ + .35f), FVector(At.X, At.Y, UZ + 100));
                    At = Next; Dir = Dir.GetRotated(R.FRandRange(-30, 30)); W *= .8f;
                }
            }
        }
        // A few flakes of the floor lifted and tipped by the tear (no collision: they never trip him).
        for (int32 I = 0; I < 3; ++I)
        {
            const int32 E0 = R.RandRange(0, N - 1);
            const FVector2D At = Tr.Edge[E0] + FVector2D(Out[E0].X, Out[E0].Y) * R.FRandRange(18, 30);
            TurnedBox(P.Steps, FVector(At.X, At.Y, UZ + 1.5f), FVector(R.FRandRange(6, 11), R.FRandRange(5, 9), 1.8f), FRotator(R.FRandRange(-8, 8), R.FRandRange(0, 360), R.FRandRange(-8, 8)));
        }
        Glows.Add({FVector(Tr.Mid.X, Tr.Mid.Y, UZ + 30), Tr.Radius > 110 ? 1500.f : 950.f, Tr.Radius > 110 ? 480.f : 340.f});
    }

    // A tunnel's floor with its tears cut out: whole flags where there is none, 10 cm cells round them.
    void TornFloor(FParts& P, float S0, float S1, float T0, float T1, const TArray<FBox2D>& Holes)
    {
        const FBox2D Rect(FVector2D(S0, T0), FVector2D(S1, T1));
        TArray<FBox2D> Pieces = {Rect};
        for (const FBox2D& H : Holes) Subtract(Pieces, H);
        // The tears' boxes here, grown, merged where they meet, snapped to the 10 cm grid.
        TArray<FBox2D> Torn;
        for (const FTempleWindTear& T : ChultTempleWindTears())
        {
            FBox2D B = T.Box.ExpandBy(16);
            if (!B.Intersect(Rect)) continue;
            B.Min = FVector2D(S0 + FMath::FloorToFloat((FMath::Max(B.Min.X, S0) - S0) / 10) * 10, T0 + FMath::FloorToFloat((FMath::Max(B.Min.Y, T0) - T0) / 10) * 10);
            B.Max = FVector2D(S0 + FMath::CeilToFloat((FMath::Min(B.Max.X, S1) - S0) / 10) * 10, T0 + FMath::CeilToFloat((FMath::Min(B.Max.Y, T1) - T0) / 10) * 10);
            Torn.Add(B);
        }
        for (bool Merged = true; Merged;)
        {
            Merged = false;
            for (int32 I = 0; I < Torn.Num() && !Merged; ++I) for (int32 J = I + 1; J < Torn.Num(); ++J)
                if (Torn[I].Intersect(Torn[J])) { Torn[I] += Torn[J]; Torn.RemoveAt(J); Merged = true; break; }
        }
        for (const FBox2D& B : Torn) Subtract(Pieces, B);
        for (const FBox2D& Q : Pieces) QuadToward(P.Stone, {Q.Min.X, Q.Min.Y, UZ}, {Q.Max.X, Q.Min.Y, UZ}, {Q.Max.X, Q.Max.Y, UZ}, {Q.Min.X, Q.Max.Y, UZ}, FVector(Q.GetCenter().X, Q.GetCenter().Y, UZ + 100));
        auto Cut = [&](float S, float T)
        {
            for (const FBox2D& H : Holes) if (H.IsInside(FVector2D(S, T))) return true;
            return ChultTempleWindTearAt(FVector2D(S, T), 6);
        };
        for (const FBox2D& B : Torn)
            for (float S = B.Min.X; S < B.Max.X - .5f; S += 10)
            {
                float Run0 = 0; bool bIn = false;
                for (float T = B.Min.Y;; T += 10)
                {
                    const bool End = T >= B.Max.Y - .5f, Solid = !End && !Cut(S + 5, T + 5);
                    if (Solid && !bIn) { Run0 = T; bIn = true; }
                    if (!Solid && bIn)
                    {
                        QuadToward(P.Stone, {S, Run0, UZ}, {S + 10, Run0, UZ}, {S + 10, T, UZ}, {S, T, UZ}, {S + 5, (Run0 + T) * .5f, UZ + 100});
                        bIn = false;
                    }
                    if (End) break;
                }
            }
    }

    // A spike pit across a leg, from From to To along T.
    FBox2D PitBox(int32 L) { return FBox2D(FVector2D(Legs[L].S0, FMath::Min(Pits[L].From, Pits[L].To)), FVector2D(Legs[L].S1, FMath::Max(Pits[L].From, Pits[L].To))); }

    void Leg(FParts& P, TArray<FFire>& Fires, TArray<FSkull>& Skulls, int32 L, FRandomStream& R)
    {
        const FSeg& G = Legs[L];
        const float Mid = (G.S0 + G.S1) * .5f;
        FLining Ln; Ln.Dado = 110; Ln.Low = &P.Moss; Ln.bFloor = false;
        TArray<FHole> Holes;
        // Its ends: into a bend (up to the corbels), or the antechamber's exit passage, or the sanctum's gallery (an arch).
        Holes.Add({2, G.S0, G.S1, UZ, L == 0 ? UZ + 280 : L == 3 ? UZ + 280 : Spring});
        Holes.Add({3, G.S0, G.S1, UZ, Spring});
        if (L == 2) { Holes.Add({0, BreakMinusT0, BreakMinusT1, UZ, Top}); Holes.Add({1, BreakPlusT0, BreakPlusT1, UZ, Top}); }
        Line(P, G.S0, G.S1, G.T0, G.T1, UZ, Top, Holes, Ln);
        Corbel(P, 1, G.S0, G.S1, G.T0, G.T1, Top, 3, 16, 28);
        const FBox2D Pit = PitBox(L);
        TornFloor(P, G.S0, G.S1, G.T0, G.T1, {Pit});
        TArray<FHole> PitHoles;
        if (L == 2) { PitHoles.Add({0, BreakMinusT0, BreakMinusT1, UZ - PitDepth, UZ}); PitHoles.Add({1, BreakPlusT0, BreakPlusT1, UZ - PitDepth, UZ}); }
        TempleRunBuild::SpikePit(P, Skulls, G.S0, G.S1, Pit.Min.Y, Pit.Max.Y, PitDepth, R, PitHoles);
        // Leg 3's broken walls: an alcove behind each break, open over the spikes, the wall's broken ends.
        if (L == 2)
            for (int32 Sg : {0, 1})
            {
                const float W = Sg == 0 ? G.S0 : G.S1, Bk0 = Sg == 0 ? BreakMinusT0 : BreakPlusT0, Bk1 = Sg == 0 ? BreakMinusT1 : BreakPlusT1;
                const float A0 = Sg == 0 ? W - BreakDeep : W, A1 = Sg == 0 ? W : W + BreakDeep;
                FLining A; A.Dado = 0; A.Low = &P.Moss;
                Line(P, A0, A1, Bk0, Bk1, UZ - PitDepth, Top, {{Sg == 0 ? 1 : 0, Bk0, Bk1, UZ - PitDepth, Top}}, A);
                TempleRunBuild::SpikeBed(P, A0 + 2, A1 - 2, Bk0 + 2, Bk1 - 2, UZ - PitDepth, 46, R);
                for (int32 I = 0; I < 4; ++I)
                    TurnedBox(P.Stone, FVector(R.FRandRange(A0 + 15, A1 - 15), R.FRandRange(Bk0 + 30, Bk1 - 30), UZ - PitDepth + R.FRandRange(8, 30)), FVector(R.FRandRange(10, 22), R.FRandRange(12, 26), R.FRandRange(6, 14)), FRotator(R.FRandRange(-30, 30), R.FRandRange(0, 360), R.FRandRange(-30, 30)));
                for (float T : {Bk0, Bk1}) TurnedBox(P.Stone, FVector(Sg == 0 ? W - 10 : W + 10, T, UZ + R.FRandRange(60, 200)), FVector(14, 12, R.FRandRange(30, 60)), FRotator(R.FRandRange(-12, 12), R.FRandRange(-10, 10), R.FRandRange(-12, 12)));
            }
        // Light: on both walls, away from the pit (the walls he runs) and the ends.
        const float Len = G.T1 - G.T0;
        for (float U = 220; U < Len - 120; U += 420)
        {
            const float T = G.T0 + U;
            if (T > Pit.Min.Y - 180 && T < Pit.Max.Y + 180) continue;
            const bool PlusWall = int32(U / 420) % 2 == 1;
            Torch(P, Fires, 0, PlusWall ? G.S1 : G.S0, PlusWall ? -1.f : 1.f, T, UZ + 210, 3000, 560);
        }
        // Over the pit, torches either end of it, high on both walls.
        for (float T : {Pit.Min.Y - 90, Pit.Max.Y + 90}) for (float Sg : {-1.f, 1.f}) Torch(P, Fires, 0, Sg < 0 ? G.S0 : G.S1, -Sg, T, UZ + 250, 3200, 600);
        (void)Mid;
    }

    void BendRoom(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, int32 B, FRandomStream& R)
    {
        const FSeg& G = Bends[B];
        FLining Ln; Ln.Dado = 120; Ln.Low = &P.Moss; Ln.bFloor = false;
        TArray<FHole> Holes;
        // The legs it joins: 1 and 2 meet bend 0 on its -T wall, 2 and 3 bend 1 on its +T wall, 3 and 4 bend 2 on its -T wall.
        const int32 Face = B == 1 ? 3 : 2;
        for (int32 L : {B, B + 1}) Holes.Add({Face, Legs[L].S0, Legs[L].S1, UZ, Spring});
        Line(P, G.S0, G.S1, G.T0, G.T1, UZ, Top, Holes, Ln);
        Corbel(P, 0, G.S0, G.S1, G.T0, G.T1, Top, 3, 20, 28);
        TornFloor(P, G.S0, G.S1, G.T0, G.T1, {});
        // Heaps of fallen stone in its corners and against its walls (visual only; the tears are the hazard).
        for (int32 I = 0; I < 8; ++I)
        {
            const bool Plus = I % 2 == 0;
            const float S = R.FRandRange(G.S0 + 40, G.S1 - 40), H = R.FRandRange(8, 22);
            const float T = Plus ? R.FRandRange(G.T1 - 26, G.T1 - 10) : R.FRandRange(G.T0 + 10, G.T0 + 26);
            if (ChultTempleWindTearAt(FVector2D(S, T), 30)) continue;
            bool AtLeg = false; for (int32 L : {B, B + 1}) AtLeg |= S > Legs[L].S0 - 30 && S < Legs[L].S1 + 30;
            if (AtLeg) continue;
            TurnedBox(R.FRand() < .4f ? P.StepMoss : P.Steps, FVector(S, T, UZ + H * .45f), FVector(R.FRandRange(14, 30), R.FRandRange(10, 18), H * .5f), FRotator(R.FRandRange(-20, 20), R.FRandRange(0, 360), R.FRandRange(-20, 20)));
        }
        // A fallen column drum at its far end, along the wall opposite the legs' openings.
        {
            const float W = B == 1 ? G.T0 + 30 : G.T1 - 30;
            bool Clear = true;
            for (float S = G.S1 - 170; S <= G.S1 - 40; S += 20) Clear &= !ChultTempleWindTearAt(FVector2D(S, W), 40);
            if (Clear) Drum(P.Moss, {G.S1 - 45, W, UZ + 24}, {G.S1 - 165, W + (B == 1 ? 8.f : -8.f), UZ + 24}, 24, 12);
        }
        // Light: torches along the walls between the tears, one at each end.
        for (float U = 420; U < G.S1 - G.S0 - 300; U += 520)
        {
            const bool Plus = int32(U / 520) % 2 == 0;
            Torch(P, Fires, 1, Plus ? G.T1 : G.T0, Plus ? -1.f : 1.f, G.S0 + U, UZ + 220, 3000, 560);
        }
        Torch(P, Fires, 0, G.S1, -1, (G.T0 + G.T1) * .5f, UZ + 230, 3200, 600);
        Glyphs(P, R, 0, G.S1, -1, (G.T0 + G.T1) * .5f - 60, (G.T0 + G.T1) * .5f + 60, UZ + 290, UZ + 330);
        Jars.Add({FVector(G.S1 - 50, B == 1 ? G.T0 + 40 : G.T1 - 40, UZ), float(R.FRandRange(0, 360)), B != 1, 2 + B % 2});
    }

    // A standing fire bowl on a stepped plinth, taller and wider than a brazier (the sanctum's great fires).
    void GreatBrazier(FParts& P, TArray<FFire>& Fires, float S, float T, float Z)
    {
        Box(P.Stone, S - 70, S + 70, T - 70, T + 70, Z, Z + 24); Box(P.Stone, S - 56, S + 56, T - 56, T + 56, Z + 24, Z + 44);
        Drum(P.Stone, {S, T, Z + 44}, {S, T, Z + 150}, 20, 12); Drum(P.Stone, {S, T, Z + 150}, {S, T, Z + 162}, 34, 14);
        Drum(P.Stone, {S, T, Z + 162}, {S, T, Z + 196}, 52, 16); Drum(P.Dark, {S, T, Z + 196}, {S, T, Z + 197}, 46, 16);
        Fires.Add({{S, T, Z + 194}, {S, T, Z + 270}, 58, 84, 9000, 1500});
    }

    // ---- The sanctum: a grand hall, the gallery he enters on, twin stairs down its side walls, a nave
    //      between two colonnades of great piers to a broad dais, two colossi over it. Bigger and grander
    //      on 2026-10-23 (user): 54 x 34 m, 17.5 m high. Empty of foes until the battle (TempleSanctumBattle).
    int32 Sanctum(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FIdol>& Idols, FRandomStream& R)
    {
        int32 Steps = 0;
        FLining L; L.Dado = 260; L.Frieze = 180; L.Top = &P.Moss;
        Line(P, HS0, HS1, HT0, HT1, HZ, HTop, {{3, Legs[3].S0, Legs[3].S1, UZ, UZ + 280}}, L);
        constexpr int32 Courses = 6; constexpr float CourseH = 46;
        Corbel(P, 1, HS0, HS1, HT0, HT1, HTop, Courses, 40, CourseH);
        const float VaultZ = HTop - Courses * CourseH;
        // The arch he comes in by: jambs and a lintel on the hall's side.
        for (float S : {Legs[3].S0, Legs[3].S1}) Box(P.Stone, S - 22, S + 22, HT1 - 24, HT1, UZ, UZ + 300);
        Box(P.Plaster, Legs[3].S0 - 30, Legs[3].S1 + 30, HT1 - 26, HT1, UZ + 280, UZ + 312);
        // The gallery: a slab across the hall's +T end on six square piers, a balustrade with finials between the stairs' heads.
        Box(P.Stone, HS0, HS1, GalleryT, HT1, UZ - 60, UZ);
        Box(P.Plaster, HS0, HS1, GalleryT - 4, GalleryT, UZ - 70, UZ - 50);
        for (float S : {77550.f, 78150.f, 78430.f, 78970.f, 79250.f, 79850.f})
        {
            Box(P.Stone, S - 40, S + 40, GalleryT + 10, GalleryT + 90, HZ, UZ - 60);
            Box(P.Stone, S - 52, S + 52, GalleryT, GalleryT + 100, HZ, HZ + 36);
            Box(P.Stone, S - 50, S + 50, GalleryT + 2, GalleryT + 98, UZ - 110, UZ - 60);
        }
        Box(P.Stone, HS0 + StairW, HS1 - StairW, GalleryT, GalleryT + 26, UZ, UZ + 82);
        Box(P.Plaster, HS0 + StairW - 4, HS1 - StairW + 4, GalleryT - 4, GalleryT + 30, UZ + 82, UZ + 92);
        for (float S = HS0 + StairW + 250; S < HS1 - StairW - 100; S += 500) Box(P.Stone, S - 18, S + 18, GalleryT - 2, GalleryT + 28, UZ + 92, UZ + 118);
        // The stairs down the side walls, from the gallery's ends to the floor (13 m down, 25 m long).
        Steps += Run(P, 1, StairFoot, GalleryT, HS0, HS0 + StairW, HS0 + StairW, HZ, UZ, HZ, R);
        Steps += Run(P, 1, StairFoot, GalleryT, HS1 - StairW, HS1, HS1 - StairW, HZ, UZ, HZ, R);
        // The processional: a paved way with pale kerbs from under the gallery to the dais (visual over the floor).
        const float PW = 190;
        QuadToward(P.Moss, {HallMid - PW, GalleryT, HZ + .6f}, {HallMid + PW, GalleryT, HZ + .6f}, {HallMid + PW, DaisT1, HZ + .6f}, {HallMid - PW, DaisT1, HZ + .6f}, {HallMid, -5400, HZ + 100});
        for (float S : {HallMid - PW - 16, HallMid + PW}) Box(P.Steps, S, S + 16, GalleryT, DaisT1, HZ, HZ + 3);
        // The colonnades: two rows of great piers (base, shaft, a moss band, banded capital) carrying an architrave
        // along each row at 11 m, under the gallery (13 m: nothing crosses the view from it to the dais), and an
        // upper colonnade of pilasters over them to a beam under the ceiling. A torch on each pier's nave face.
        const float ArchZ = HZ + 1100;
        for (float Sg : {-1.f, 1.f})
        {
            const float S = HallMid + Sg * NaveHalf;
            for (float T : PierT)
            {
                Box(P.Stone, S - PierHalf - 26, S + PierHalf + 26, T - PierHalf - 26, T + PierHalf + 26, HZ, HZ + 30);
                Box(P.Stone, S - PierHalf - 14, S + PierHalf + 14, T - PierHalf - 14, T + PierHalf + 14, HZ + 30, HZ + 70);
                Box(P.Stone, S - PierHalf, S + PierHalf, T - PierHalf, T + PierHalf, HZ + 70, ArchZ - 120);
                Box(P.Moss, S - PierHalf - 6, S + PierHalf + 6, T - PierHalf - 6, T + PierHalf + 6, HZ + 520, HZ + 548);
                Box(P.Plaster, S - PierHalf - 8, S + PierHalf + 8, T - PierHalf - 8, T + PierHalf + 8, ArchZ - 120, ArchZ - 90);
                Box(P.Stone, S - PierHalf - 22, S + PierHalf + 22, T - PierHalf - 22, T + PierHalf + 22, ArchZ - 90, ArchZ - 40);
                Box(P.Stone, S - PierHalf - 36, S + PierHalf + 36, T - PierHalf - 36, T + PierHalf + 36, ArchZ - 40, ArchZ);
                Torch(P, Fires, 0, S - Sg * PierHalf, -Sg, T, HZ + 380, 4400, 1000);
                // the upper colonnade: a pilaster over the pier up to the ceiling's beam
                Box(P.Stone, S - 50, S + 50, T - 70, T + 70, ArchZ + 70, HTop - 60);
            }
            // the architrave along the row, and a glyph frieze on its nave face over the lane
            const float A0 = PierT[UE_ARRAY_COUNT(PierT) - 1] - PierHalf - 36, A1 = PierT[0] + PierHalf + 36;
            Box(P.Stone, S - PierHalf - 36, S + PierHalf + 36, A0, A1, ArchZ, ArchZ + 70);
            Box(P.Stone, S - 70, S + 70, A0, A1, HTop - 60, HTop);   // the beam along the row under the ceiling
            Glyphs(P, R, 0, S - Sg * (PierHalf + 36), -Sg, -6500, -5850, ArchZ - 180, ArchZ - 16);
        }
        // Pilasters on the side walls opposite the piers beyond the stairs, from the floor to the corbels.
        for (float Sg : {-1.f, 1.f}) for (float T : PierT) if (T < StairFoot - 150)
        {
            const float W = Sg < 0 ? HS0 : HS1, In = -Sg;
            Box(P.Stone, FMath::Min(W, W + In * 50), FMath::Max(W, W + In * 50), T - 80, T + 80, HZ, VaultZ);
            Box(P.Stone, FMath::Min(W, W + In * 70), FMath::Max(W, W + In * 70), T - 96, T + 96, HZ, HZ + 60);
        }
        // Guardian idols on plinths along the processional's first half, facing it.
        for (float Sg : {-1.f, 1.f}) for (float T : {-4875.f, -5425.f})
        {
            const float S = HallMid + Sg * 420;
            Box(P.Stone, S - 52, S + 52, T - 52, T + 52, HZ, HZ + 110); Box(P.Plaster, S - 58, S + 58, T - 58, T + 58, HZ + 110, HZ + 120);
            Idols.Add({FVector(S, T, HZ + 120), YawOf(-Sg, 0), 36});
        }
        // Fires: braziers along the processional, great fire bowls flanking the dais and in the aisles before it.
        for (float Sg : {-1.f, 1.f})
        {
            Brazier(P, Fires, HallMid + Sg * 420, -4300, HZ);
            Brazier(P, Fires, HallMid + Sg * 420, -5975, HZ);
            GreatBrazier(P, Fires, HallMid + Sg * 600, DaisT1 + 260, HZ);
            GreatBrazier(P, Fires, HallMid + Sg * 1450, DaisT1 + 420, HZ);
        }
        // The dais across the -T end: risen 96 cm, five steps down its front (visual) over a hidden ramp.
        Box(P.Stone, DaisS0, DaisS1, DaisT0, DaisT1, HZ, HZ + DaisH);
        Box(P.Plaster, DaisS0 - 4, DaisS1 + 4, DaisT1 - 6, DaisT1 + 2, HZ + DaisH - 8, HZ + DaisH);
        constexpr int32 Risers = 6; constexpr float Tread = 30, Rise_ = DaisH / Risers;
        for (int32 K = 1; K < Risers; ++K)
        {
            Box(P.Steps, DaisS0 + 20, DaisS1 - 20, DaisT1 + (K - 1) * Tread, DaisT1 + K * Tread, HZ, HZ + DaisH - K * Rise_);
            ++Steps;
        }
        {
            const float T0 = DaisT1 + (Risers - .5f) * Tread, T1 = DaisT1, T2 = DaisT1 - 40;
            QuadToward(P.Ramp, {DaisS0 + 22, T0, HZ}, {DaisS1 - 22, T0, HZ}, {DaisS1 - 22, T1, HZ + DaisH}, {DaisS0 + 22, T1, HZ + DaisH}, {HallMid, (T0 + T1) * .5f, HZ + 200});
            QuadToward(P.Ramp, {DaisS0 + 22, T1, HZ + DaisH}, {DaisS1 - 22, T1, HZ + DaisH}, {DaisS1 - 22, T2, HZ + DaisH}, {DaisS0 + 22, T2, HZ + DaisH}, {HallMid, T1, HZ + 200});
        }
        // On the dais: an altar on a stepped base, two colossal serpent idols on tall plinths, a great carved disc
        // in the end wall over a broad glyph wall between stone bands.
        const float DZ = HZ + DaisH, AT = DaisT1 - 560;
        Box(P.Stone, HallMid - 300, HallMid + 300, AT - 160, AT + 160, DZ, DZ + 18);
        Box(P.Stone, HallMid - 250, HallMid + 250, AT - 125, AT + 125, DZ + 18, DZ + 36);
        Box(P.Stone, HallMid - 190, HallMid + 190, AT - 100, AT + 100, DZ + 36, DZ + 128);
        Box(P.Plaster, HallMid - 206, HallMid + 206, AT - 116, AT + 116, DZ + 128, DZ + 140);
        for (float Sg : {-1.f, 1.f})
        {
            const float S = HallMid + Sg * 760, T = DaisT0 + 330;
            Box(P.Stone, S - 160, S + 160, T - 160, T + 160, DZ, DZ + 40);
            Box(P.Stone, S - 140, S + 140, T - 140, T + 140, DZ + 40, DZ + 230);
            Box(P.Plaster, S - 150, S + 150, T - 150, T + 150, DZ + 230, DZ + 246);
            Glyphs(P, R, 1, T + 140, 1, S - 110, S + 110, DZ + 70, DZ + 200);
            Idols.Add({FVector(S, T, DZ + 246), YawOf(0, 1), 125});
        }
        {
            const float Cz = DZ + 950;
            Drum(P.Stone, {HallMid, HT0, Cz}, {HallMid, HT0 + 26, Cz}, 330, 40);
            Drum(P.Plaster, {HallMid, HT0 + 26, Cz}, {HallMid, HT0 + 34, Cz}, 300, 40);
            Drum(P.Stone, {HallMid, HT0 + 34, Cz}, {HallMid, HT0 + 52, Cz}, 230, 36);
            Drum(P.Moss, {HallMid, HT0 + 52, Cz}, {HallMid, HT0 + 58, Cz}, 150, 32);
            Drum(P.Dark, {HallMid, HT0 + 58, Cz}, {HallMid, HT0 + 62, Cz}, 64, 24);
            for (int32 K = 0; K < 16; ++K)
            {
                const float A = K * 2 * PI / 16;
                TurnedBox(P.Stone, FVector(HallMid + FMath::Cos(A) * 268, HT0 + 40, Cz + FMath::Sin(A) * 268), FVector(14, 14, 40), FRotator(FMath::RadiansToDegrees(A) - 90, 0, 0));
            }
            Glyphs(P, R, 1, HT0, 1, HallMid - 900, HallMid + 900, DZ + 260, DZ + 560);
            for (float Z : {DZ + 240.f, DZ + 580.f}) Box(P.Stone, HallMid - 940, HallMid + 940, HT0, HT0 + 14, Z - 10, Z + 10);
        }
        // Light: torches down both long walls (beyond the stairs, and over the stairs along their line), on the end
        // wall, the gallery and its back wall.
        for (float Sg : {-1.f, 1.f})
        {
            const float W = Sg < 0 ? HS0 : HS1;
            for (float T : {-7025.f, -7700.f, -8400.f}) Torch(P, Fires, 0, W, -Sg, T, HZ + 360, 4400, 1000);
            for (float K : {.25f, .55f, .85f})
            {
                const float T = FMath::Lerp(GalleryT, StairFoot, K), Zs = FMath::Lerp(UZ, HZ, K);
                Torch(P, Fires, 0, W, -Sg, T, Zs + 280, 3800, 800);
            }
            Torch(P, Fires, 0, W, -Sg, HT1 - 250, UZ + 230, 3600, 700);
            Torch(P, Fires, 1, HT0, 1, HallMid + Sg * 1250, HZ + 420, 5200, 1100);
        }
        for (float S : {77500.f, 78150.f, 79250.f, 79900.f}) Torch(P, Fires, 1, HT1, -1, S, UZ + 230, 3600, 700);
        for (float Sg : {-1.f, 1.f}) for (float T : {-8050.f}) Glyphs(P, R, 0, Sg < 0 ? HS0 : HS1, -Sg, T - 160, T + 160, HZ + 420, HZ + 560);
        // Pots: on the gallery and in the hall's far corners.
        Jars.Add({FVector(HS0 + StairW + 40, HT1 - 40, UZ), float(R.FRandRange(0, 360)), true, 3});
        Jars.Add({FVector(HS1 - StairW - 40, HT1 - 40, UZ), float(R.FRandRange(0, 360)), false, 2});
        Jars.Add({FVector(HS0 + 90, HT0 + 700, HZ), float(R.FRandRange(0, 360)), true, 3});
        Jars.Add({FVector(HS1 - 90, HT0 + 700, HZ), float(R.FRandRange(0, 360)), false, 2});
        return Steps;
    }

    int32 BuildWind(FParts& P, TArray<FFire>& Fires, TArray<FJarSpot>& Jars, TArray<FSkull>& Skulls, TArray<FIdol>& Idols, TArray<FGlow>& Glows)
    {
        FRandomStream R(2026102202);
        for (int32 L = 0; L < 4; ++L) Leg(P, Fires, Skulls, L, R);
        for (int32 B = 0; B < 3; ++B) BendRoom(P, Fires, Jars, B, R);
        const TArray<FTempleWindTear>& Tears = ChultTempleWindTears();
        for (int32 K = 0; K < Tears.Num(); ++K) Tear(P, Glows, Tears[K], K);
        return Sanctum(P, Fires, Jars, Idols, R);
    }
}
