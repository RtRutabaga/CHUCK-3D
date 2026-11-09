// ShipDeck.inl (Claude, 2026-11-02; docs/SHIP.md, milestone 2): the weather deck in the open, its fittings, the rig,
// the sails' shapes and the climbs. Included in ShipSetting.cpp's anonymous namespace: it uses the hull's lines (OuterAt,
// InnerAt, PlankTopAt, BowspritAt) and the mesh helpers (FSurf, FParts).

float RailTopAt(float X) { return X < QuarterFore - .1f ? QuarterRailZ : RailZ; }

// A square-section length from A to B (a rail, a beam, a brace), turned to lie along it.
void Beam(FSurf& S, const FVector& A, const FVector& B, float HalfW, float HalfH)
{
    const FVector D = B - A; const float L = D.Size();
    if (L < .1f) return;
    const FVector X = D / L;
    FVector Y = FVector::CrossProduct(FVector::UpVector, X);
    if (Y.SizeSquared() < 1e-4f) Y = FVector::RightVector;
    Y.Normalize();
    const FVector Z = FVector::CrossProduct(X, Y);
    S.Box((A + B) * .5f, X * (L * .5f), Y * HalfW, Z * HalfH);
}

// A slab with rectangular holes cut out of it (the tops, round their masts and the lubber's hole).
void Slab(FSurf& S, float X0, float X1, float Y0, float Y1, float Z0, float Z1, const TArray<FHole>& Holes)
{
    TArray<float> Cuts = {X0, X1};
    for (const FHole& H : Holes) { Cuts.Add(FMath::Clamp(H.X0, X0, X1)); Cuts.Add(FMath::Clamp(H.X1, X0, X1)); }
    Cuts.Sort();
    for (int32 I = 0; I + 1 < Cuts.Num(); ++I)
    {
        const float A = Cuts[I], B = Cuts[I + 1];
        if (B - A < .5f) continue;
        TArray<TPair<float, float>> Spans = {{Y0, Y1}};
        for (const FHole& H : Holes)
        {
            if (A < H.X0 - .1f || B > H.X1 + .1f) continue;
            TArray<TPair<float, float>> Next;
            for (const auto& Sp : Spans) { if (H.Y0 > Sp.Key) Next.Add({Sp.Key, FMath::Min(H.Y0, Sp.Value)}); if (H.Y1 < Sp.Value) Next.Add({FMath::Max(H.Y1, Sp.Key), Sp.Value}); }
            Spans = Next;
        }
        for (const auto& Sp : Spans) S.B(A, B, Sp.Key, Sp.Value, Z0, Z1);
    }
}

// A rope that hangs between two points with a little sag.
void Sagging(FSurf& S, const FVector& A, const FVector& B, float R, float Sag, int32 Segments = 6)
{
    TArray<FVector> Path;
    for (int32 I = 0; I <= Segments; ++I)
    {
        const float T = I / float(Segments);
        Path.Add(FMath::Lerp(A, B, T) - FVector(0, 0, Sag * 4 * T * (1 - T)));
    }
    S.Cord(Path, R);
}

// ------------------------------------------------------------------ the rig's numbers
struct FMast { float X, Top, Brace; float CourseHalf, TopsailHalf, GallantHalf; };
const FMast& MainMast() { static const FMast M{MainMastX, MainTopZ, 0.f, MainYardHalf, 470, 320}; return M; }
const FMast& ForeMast() { static const FMast M{ForeMastX, ForeTopZ, 12.f, 600, 440, 300}; return M; }
// The lower masthead's radius at the top, the topmast's and topgallant's axes (forward of the one below), the yards'
// heights and where along X they hang.
constexpr float HeadR = 24, TopmastR = 17, GallantR = 10;
float TopmastX(const FMast& M) { return M.X + HeadR + TopmastR; }
float GallantX(const FMast& M) { return TopmastX(M) + TopmastR + GallantR - 4; }
float CourseZ(const FMast& M) { return M.Top - 55; }
float TopsailZ(const FMast& M) { return M.Top + 760; }
float GallantZ(const FMast& M) { return M.Top + 1230; }
FVector YardCentre(const FMast& M, int32 Which)
{
    switch (Which)
    {
    case 0: return FVector(M.X + 45, 0, CourseZ(M));
    case 1: return FVector(TopmastX(M) + 22, 0, TopsailZ(M));
    default: return FVector(GallantX(M) + 15, 0, GallantZ(M));
    }
}
float YardHalf(const FMast& M, int32 Which) { return Which == 0 ? M.CourseHalf : Which == 1 ? M.TopsailHalf : M.GallantHalf; }
// A yardarm's end (Side +1 port), the yard braced round by the mast's Brace about its centre.
FVector YardEnd(const FMast& M, int32 Which, float Side, float Inset = 0)
{
    const FQuat Q(FRotator(0, M.Brace, 0));
    return YardCentre(M, Which) + Q.RotateVector(FVector(0, Side * (YardHalf(M, Which) - Inset), 0));
}
FVector YardNormal(const FMast& M) { return FQuat(FRotator(0, M.Brace, 0)).RotateVector(FVector::ForwardVector); }
// The spanker's gaff and boom on the main mast, over the quarterdeck (the boom clears a standing man at the helm).
FVector SpankerThroat() { return FVector(MainMastX - MastR - 4, 0, MainTopZ - 230); }
FVector SpankerPeak() { return FVector(MainMastX - 640, 70, MainTopZ + 80); }
FVector SpankerTack() { return FVector(MainMastX - MastR - 4, 0, QuarterZ + 250); }
FVector SpankerClew() { return FVector(SternX + 70, 150, QuarterZ + 262); }
// The flag's size and where its hoist hangs from the gaff's peak (FlagPoint, below).
constexpr float FlagFly = 260, FlagDrop = 165;
FVector FlagHoist() { return SpankerPeak() + FVector(-10, 14, -8); }
// Where a hull side is at X (its post line) and how far up the planking goes.
float PostLine(float X) { return FMath::Max(0.f, OuterAt(X, PlankTopAt(X)) - 5); }
// The shrouds (2026-11-05: every ratline net is climbed, the rope ladder to the main top gone). The lower shrouds, four a
// side, from the deadeyes over the channel up to the top's rim, aft of the yard (the futtock shrouds carry them on under
// it to the mast); the topmast shrouds, three a side, from the rim just forward of them up to the topmast head.
float TopHalfOf(const FMast& M) { return M.X < 0 ? TopHalf : 120; }
FVector ShroudLow(const FMast& M, float Side, int32 K) { const float X = M.X - 125 + K * 50; return FVector(X, Side * (PostLine(X) + 28), WaistTopZ + 68); }
FVector ShroudHigh(const FMast& M, float Side, int32 K) { return FVector(M.X - 66 + K * 10, Side * (TopHalfOf(M) - 6), M.Top - 14); }
FVector TopShroudLow(const FMast& M, float Side, int32 K) { return FVector(M.X - 15 + K * 15, Side * (TopHalfOf(M) - 6), M.Top + 8); }
FVector TopShroudHigh(const FMast& M, float Side, int32 K) { return FVector(TopmastX(M) - 6 + K * 4, Side * 14, M.Top + 864); }
// The fore course's clews (2026-11-08, user: the sail must not pass through the fore ratlines). With the yard braced
// 12 degrees its port (lee) yardarm swings aft, and the clews used to hang out over the sea, the port one aft among the
// shrouds, so the canvas ran through the net. Now both clews come in to the ship's side, the lee one sheeted aft only as
// far as just forward of the foremost shroud (as a sharp-braced course bears on its lee rigging), the weather one tacked
// forward; the sail stands some 50 cm clear of every shroud and ratline (CHUCK_SHIP_COURSE). Side +1 port.
FVector CourseClew(float Side)
{
    const float X = ForeMastX + (Side > 0 ? 60.f : 200.f);
    return FVector(X, Side * (PostLine(X) - 10), DeckZ + 300);
}
// Where the clews' sheet (port, a little aft) and tack (starboard, forward) are made fast on the rail.
FVector CourseClewLead(float Side)
{
    const float X = ForeMastX + (Side > 0 ? 40.f : 290.f);
    return FVector(X, Side * (PostLine(X) - 4), RailZ + 3);
}
// The ratlines' foot on deck (2026-11-05): a short net from a sheer pole on the deck, inside the bulwark, out over the
// rail to the shrouds just above it (NetJoinZ); leg K under shroud K.
constexpr float NetIn = 45, NetJoinZ = 780;
FVector NetJoin(const FMast& M, float Side, int32 K)
{
    const FVector Lo = ShroudLow(M, Side, K), Hi = ShroudHigh(M, Side, K);
    return FMath::Lerp(Lo, Hi, (NetJoinZ - Lo.Z) / (Hi.Z - Lo.Z));
}
FVector NetFoot(const FMast& M, float Side, int32 K) { const float X = static_cast<float>(NetJoin(M, Side, K).X); return FVector(X, Side * (PostLine(X) - NetIn), DeckZ + 5); }

// The rail's posts (stanchions) along one side (Part 0 round the quarterdeck, 1 the waist), every 55 cm along the post
// line, and that line resampled every 25 cm.
void RailPosts(float Side, int32 Part, TArray<FVector2D>& Line, TArray<FVector2D>& Posts)
{
    const float XA = Part == 0 ? SternX + 8 : QuarterFore, XB = Part == 0 ? QuarterFore : BowX - 6;
    TArray<FVector2D> Dense;
    for (float X = XA; X < XB; X += 4) Dense.Add(FVector2D(X, Side * PostLine(X)));
    Dense.Add(FVector2D(XB, Side * PostLine(XB - .2f)));
    Line = {Dense[0]}; Posts = {Dense[0]};
    float NextPost = 0;
    for (int32 I = 1; I < Dense.Num(); ++I)
    {
        NextPost += FVector2D::Distance(Dense[I - 1], Dense[I]);
        if (FVector2D::Distance(Line.Last(), Dense[I]) >= 25 || I == Dense.Num() - 1) Line.Add(Dense[I]);
        if (NextPost >= 55) { Posts.Add(Dense[I]); NextPost = 0; }
    }
    if (FVector2D::Distance(Posts.Last(), Dense.Last()) > 20) Posts.Add(Dense.Last());
}
// The ratlines' foot on deck (2026-11-05, user: start the climb from the deck, plainly): inside the bulwark, under each
// mast's shrouds, a short net rises from a sheer pole lying on the deck, lashed to the stanchions' feet, and leans out
// over the rail to join the shrouds just above it (JoinZ). Leg K under shroud K.
// (NetFoot, NetJoin: with the shrouds, above.)

// ------------------------------------------------------------------ the climbs
TArray<FChuckClimbable> DeckLadders()
{
    TArray<FChuckClimbable> Out;
    // The companion ladder from the crew quarters up to the weather deck (on the hatch's fore edge, as the hold's).
    {
        FChuckClimbable L; const float Yc = (CompY0 + CompY1) * .5f;
        L.Foot = ShipWorld(FVector(CompX1 - 26.5f, Yc, LowerZ)); L.TopZ = ShipOrigin().Z + DeckZ; L.Out = FVector(-1, 0, 0); L.HalfWidth = 22;
        L.Lip = ShipWorld(FVector(CompX1, Yc, DeckZ)); Out.Add(L);
    }
    // The two ladders up the quarterdeck's break, against its face.
    for (float Side : {1.f, -1.f})
    {
        FChuckClimbable L; const float E = QuarterFore + 8;
        L.Foot = ShipWorld(FVector(E + 26.5f, Side * BreakLadderY, DeckZ)); L.TopZ = ShipOrigin().Z + QuarterZ; L.Out = FVector(1, 0, 0); L.HalfWidth = 22;
        L.Lip = ShipWorld(FVector(E, Side * BreakLadderY, QuarterZ)); Out.Add(L);
    }
    return Out;
}
// The ratlines: each mast's lower shrouds and topmast shrouds, a net a side, climbed on their outer faces, and across
// them as well as up (ChuckClimbable Line/SideTravel). From the deck he goes up the bulwark's inside, out over the rail
// and turns round onto the lower shrouds, up to the top and over its rim; from the top, out over the rim onto the topmast
// shrouds, up to under the crosstrees (no lip there). Walking out over the rim at the lower shrouds' heads, he lowers
// himself onto them.
TArray<FChuckClimbable> DeckShrouds()
{
    TArray<FChuckClimbable> Out;
    constexpr float Off = 18;      // his centre off the net (the climb clip's paws on it), across the lean
    constexpr float Stand = 33.5f; // his centre over a floor (half height + 1)
    constexpr float HangDrop = 22; // ChuckClipData::HangDrop: his centre under a lip at the top of a climb
    for (const FMast* Mp : {&MainMast(), &ForeMast()})
        for (float Side : {1.f, -1.f})
        {
            const FMast& M = *Mp;
            const float H = TopHalfOf(M);
            const FVector O(0, Side, 0);
            {
                // The lower shrouds: the plane through the middle two, his line just outside it.
                const FVector Lo = (ShroudLow(M, Side, 1) + ShroudLow(M, Side, 2)) * .5f, Hi = (ShroudHigh(M, Side, 1) + ShroudHigh(M, Side, 2)) * .5f;
                auto On = [&](float Z) { const FVector P = FMath::Lerp(Lo, Hi, (Z - Lo.Z) / (Hi.Z - Lo.Z)); return FVector(P.X, P.Y, Z) + O * Off; };
                // The net at the foot (NetFoot..NetJoin), his line just inside it; over the join (clear of the rail's
                // cap, never through it) round onto the shrouds' outer face.
                const FVector NLo = (NetFoot(M, Side, 1) + NetFoot(M, Side, 2)) * .5f, NHi = (NetJoin(M, Side, 1) + NetJoin(M, Side, 2)) * .5f;
                auto Inside = [&](float Z) { const FVector P = FMath::Lerp(NLo, NHi, (Z - NLo.Z) / (NHi.Z - NLo.Z)); return FVector(P.X, P.Y, Z) - O * Off; };
                const FVector Start = Inside(DeckZ + Stand);
                FChuckClimbable L;
                L.Foot = ShipWorld(FVector(Start.X, Start.Y, DeckZ));
                L.Line = {ShipWorld(Start), ShipWorld(Inside(NetJoinZ - 45)), ShipWorld(On(NetJoinZ + 25)), ShipWorld(On(M.Top - HangDrop))};
                L.TopZ = ShipOrigin().Z + M.Top; L.Out = O; L.HalfWidth = 15;
                L.Lip = ShipWorld(FVector(Hi.X, Side * H, M.Top));
                L.Approach = O; L.TurnZ = ShipOrigin().Z + NetJoinZ - 10;
                L.SideTravel = 45; L.TopSideTravel = 0;
                Out.Add(L);
            }
            {
                // The topmast shrouds, from the top's floor out over its rim; they end under the crosstrees.
                const FVector Lo = TopShroudLow(M, Side, 1) + FVector(0, 0, 8), Hi = TopShroudHigh(M, Side, 1);
                auto On = [&](float Z) { const FVector P = FMath::Lerp(Lo, Hi, (Z - Lo.Z) / (Hi.Z - Lo.Z)); return FVector(P.X, P.Y, Z) + O * Off; };
                const float X = Lo.X, In = H - 21;
                FChuckClimbable L;
                L.Foot = ShipWorld(FVector(X, Side * In, M.Top));
                L.Line = {ShipWorld(FVector(X, Side * In, M.Top + Stand)), ShipWorld(FVector(X, Side * In, M.Top + 50)),
                    ShipWorld(FVector(X, Side * (H + 2), M.Top + 75)), ShipWorld(On(M.Top + 105)), ShipWorld(On(M.Top + 720))};
                L.TopZ = ShipOrigin().Z + M.Top + 720; L.Out = O; L.HalfWidth = 15;
                L.Lip = L.Line.Last();
                L.Approach = O; L.TurnZ = ShipOrigin().Z + M.Top + 75;
                L.SideTravel = 10; L.TopSideTravel = 0; L.bTopOut = false;
                Out.Add(L);
            }
        }
    return Out;
}
// The yards, hand over hand (2026-11-05: every yard he can reach, now the ratlines are climbed): each mast's course yard
// from either side of its top out to the yardarms (the main's as before, 12 grips), and its topsail yard from beside the
// topmast head, where the topmast shrouds end under it (reaching up from their top takes it), out to its yardarms.
// 90 cm apart (easy leaps); along the yard as it is braced (the fore's 12 degrees).
int32 YardGripCount(const FMast& M, int32 Which) { const float First = Which == 0 ? (M.X < 0 ? MainGrip0 : 160.f) : 75.f; return 1 + FMath::FloorToInt((YardHalf(M, Which) - 30 - First) / MainGripStep); }
TArray<FChuckSwingGrip> YardGrips()
{
    TArray<FChuckSwingGrip> Out;
    for (const FMast* Mp : {&MainMast(), &ForeMast()})
        for (int32 W = 0; W < 2; ++W)
        {
            const FMast& M = *Mp;
            const FQuat Q(FRotator(0, M.Brace, 0));
            const float First = W == 0 ? (M.X < 0 ? MainGrip0 : 160.f) : 75.f;
            for (float Side : {1.f, -1.f})
                for (int32 K = 0; K < YardGripCount(M, W); ++K)
                {
                    FChuckSwingGrip G;
                    G.Grip = ShipWorld(YardCentre(M, W) + Q.RotateVector(FVector(0, Side * (First + K * MainGripStep), 0)));
                    G.Along = Q.RotateVector(FVector(0, 1, 0)); G.Depth = 0;
                    Out.Add(G);
                }
        }
    return Out;
}

// A ladder of rungs between two rails (or ropes) on a hatch's edge: rails at Y +-24 round Yc, rungs every Step.
void LadderAt(FSurf& Rails, FSurf& Rungs, float EdgeX, float Dir, float Yc, float Z0, float Z1, float RailTop, float Step = 28, bool bRope = false)
{
    // Dir: which way from the edge the rungs stand (-1 aft of it, +1 forward).
    const float Xa = EdgeX + Dir * 3, Xb = EdgeX + Dir * 10;
    for (float Y : {Yc - 24, Yc + 24})
    {
        if (bRope) Rails.Cord({FVector((Xa + Xb) * .5f, Y, Z0), FVector((Xa + Xb) * .5f, Y, RailTop)}, 1.6f);
        else Rails.B(FMath::Min(Xa, Xb) + 1, FMath::Max(Xa, Xb), Y - 3, Y + 3, Z0, RailTop);
    }
    for (float Z = Z0 + 24; Z <= Z1 - 4; Z += Step) Rungs.B(FMath::Min(Xa, Xb), FMath::Max(Xa, Xb), Yc - 27, Yc + 27, Z - 2.5f, Z + 2.5f);
}

// ------------------------------------------------------------------ the weather deck
void BuildWeatherDeck(FParts& P)
{
    // ---- Posts and rails along both sides above the planking (the reference's posts along the rail): the sea shows
    // between them, and the gaps are too low for him to pass.
    for (float Side : {1.f, -1.f})
        for (int32 Part = 0; Part < 2; ++Part)
        {
            TArray<FVector2D> Line, Posts;
            RailPosts(Side, Part, Line, Posts);
            const float Base = Part == 0 ? QuarterTopZ : WaistTopZ, Top = Part == 0 ? QuarterRailZ : RailZ;
            for (const FVector2D& Pt : Posts) P.Wood.B(Pt.X - 5, Pt.X + 5, Pt.Y - 5, Pt.Y + 5, Base, Top - 8);
            for (int32 I = 1; I < Line.Num(); ++I)
            {
                const FVector A(Line[I - 1], 0), B(Line[I], 0);
                Beam(P.Wood, A + FVector(0, 0, Top - 4), B + FVector(0, 0, Top - 4), 7, 4);
                Beam(P.Wood, A + FVector(0, 0, (Base + Top) * .5f), B + FVector(0, 0, (Base + Top) * .5f), 3, 3);
            }
        }
    // The quarterdeck's rail along its fore edge, open at its two ladders.
    {
        const float X = QuarterFore + 2, W = InnerAt(QuarterFore, QuarterZ + 20) - 4;
        TArray<TPair<float, float>> Runs = {{-W, -BreakLadderY - 34}, {-BreakLadderY + 34, BreakLadderY - 34}, {BreakLadderY + 34, W}};
        for (const auto& R : Runs)
        {
            for (float Y = R.Key; Y <= R.Value + .1f; Y += (R.Value - R.Key) / FMath::Max(1, FMath::RoundToInt((R.Value - R.Key) / 42))) P.Wood.B(X - 4, X + 4, Y - 4, Y + 4, QuarterZ, QuarterZ + 90);
            P.Wood.B(X - 6, X + 6, R.Key - 4, R.Value + 4, QuarterZ + 88, QuarterZ + 96);
            P.Wood.B(X - 3, X + 3, R.Key, R.Value, QuarterZ + 44, QuarterZ + 50);
        }
    }
    // ---- The ladders up the break, and their handrails.
    for (float Side : {1.f, -1.f})
        LadderAt(P.Wood, P.Wood, QuarterFore + 8, 1, Side * BreakLadderY, DeckZ, QuarterZ, QuarterZ + 55);
    // ---- The companion hatch: its coaming on three sides (the fore edge is the ladder's lip), lining, ladder.
    {
        const float Z = DeckZ, Yc = (CompY0 + CompY1) * .5f;
        P.Wood.B(CompX0 - 8, CompX0, CompY0 - 8, CompY1 + 8, Z, Z + 12);
        P.Wood.B(CompX0 - 8, CompX1, CompY0 - 8, CompY0, Z, Z + 12);
        P.Wood.B(CompX0 - 8, CompX1, CompY1, CompY1 + 8, Z, Z + 12);
        P.Wood.B(CompX0 - 4, CompX0, CompY0, CompY1, DeckBeam, Z);
        P.Wood.B(CompX0, CompX1, CompY0 - 4, CompY0, DeckBeam, Z);
        P.Wood.B(CompX0, CompX1, CompY1, CompY1 + 4, DeckBeam, Z);
        P.Wood.B(CompX1, CompX1 + 4, CompY0, CompY1, DeckBeam, Z);
        LadderAt(P.Wood, P.Wood, CompX1, -1, Yc, LowerZ, DeckZ, DeckZ + 15);
        // Its cover, unshipped and leaning against the coaming's aft side.
        P.WoodDecor.TurnedBox(FVector(CompX0 - 30, Yc, Z + 52), FVector(3, 62, 50), FRotator(-14, 0, 0));
        P.Hidden.B(CompX0 - 46, CompX0 - 10, Yc - 62, Yc + 62, Z, Z + 100);
    }
    // ---- The fife rails at the masts' feet (the main's open fore and aft), with
    // belaying pins; the pin rails inside the rail by the shrouds.
    auto Pins = [&](float X0, float X1, float Y, float Z)
    {
        for (float X = X0 + 9; X < X1 - 4; X += 17) P.WoodDecor.Spar(FVector(X, Y, Z - 14), FVector(X, Y, Z + 16), 1.6f, 1.3f, 6);
    };
    {
        const float MX = MainMastX, Z = DeckZ;
        for (float Y : {-75.f, 75.f})
        {
            P.Wood.B(MX - 64, MX + 34, Y - 5, Y + 5, Z + 70, Z + 78);
            for (float X : {MX - 64.f, MX + 26.f}) P.Wood.B(X, X + 8, Y - 5, Y + 5, Z, Z + 70);
            Pins(MX - 64, MX + 34, Y, Z + 78);
        }
    }
    {
        const float FX = ForeMastX, Z = DeckZ;
        for (float Y : {-75.f, 75.f}) { P.Wood.B(FX - 80, FX + 80, Y - 5, Y + 5, Z + 70, Z + 78); Pins(FX - 80, FX + 80, Y, Z + 78); }
        for (float X : {-75.f, 75.f}) { P.Wood.B(FX + X - 5, FX + X + 5, -70, 70, Z + 70, Z + 78); }
        for (float X : {-80.f, 72.f}) for (float Y : {-80.f, 72.f}) P.Wood.B(FX + X, FX + X + 8, Y, Y + 8, Z, Z + 70);
        // The ship's bell in a small belfry on its fore side.
        for (float Y : {-24.f, 24.f}) P.Wood.B(FX + 76, FX + 82, Y - 3, Y + 3, Z + 78, Z + 160);
        P.Wood.B(FX + 74, FX + 84, -30, 30, Z + 160, Z + 168);
        P.Brass.Spar(FVector(FX + 79, 0, Z + 152), FVector(FX + 79, 0, Z + 128), 6, 13, 14);
        P.Iron.Rope({FVector(FX + 79, 0, Z + 128), FVector(FX + 79, 0, Z + 118)}, 1.2f);
    }
    for (const FMast* M : {&MainMast(), &ForeMast()})
        for (float Side : {1.f, -1.f})
        {
            const float X0 = M->X - 130, X1 = M->X + 70, Y = Side * (PostLine(M->X) - 14);
            P.Wood.B(X0, X1, Y - 6, Y + 6, RailZ - 34, RailZ - 26);
            Pins(X0, X1, Y, RailZ - 26);
            // A coil hung on every third pin.
            for (float X = X0 + 26; X < X1 - 10; X += 51)
            {
                TArray<FVector> Loop;
                for (int32 K = 0; K <= 12; ++K) { const float A = K / 12.f * 2 * PI; Loop.Add(FVector(X + 9 * FMath::Sin(A), Y - Side * 3, RailZ - 46 - 16 * (1 - FMath::Cos(A)) * .5f - 4)); }
                P.Hemp.Cord(Loop, 1.4f);
            }
        }
    // ---- The capstan between the main hatch and the fore mast.
    {
        const FVector C(270, 0, DeckZ);
        P.Wood.Spar(C, C + FVector(0, 0, 62), 36, 30, 12);
        for (int32 K = 0; K < 8; ++K)
        {
            const float A = K * PI / 4;
            P.Wood.TurnedBox(C + FVector(33 * FMath::Cos(A), 33 * FMath::Sin(A), 31), FVector(4, 5, 30), FRotator(0, FMath::RadiansToDegrees(A), 0));
        }
        P.Wood.Spar(C + FVector(0, 0, 62), C + FVector(0, 0, 80), 50, 50, 16);
        for (int32 K = 0; K < 8; ++K) { const float A = K * PI / 4 + PI / 8; P.Dark.TurnedBox(C + FVector(50 * FMath::Cos(A), 50 * FMath::Sin(A), 71), FVector(1, 5, 5), FRotator(0, FMath::RadiansToDegrees(A), 0)); }
        P.Iron.Spar(C + FVector(0, 0, 80), C + FVector(0, 0, 82), 14, 14, 12);
    }
    // ---- The bitts at the bowsprit's heel, the windlass for the anchors, the catheads and the anchors themselves.
    {
        for (float Y : {-30.f, 30.f}) P.Wood.B(BowX - 210, BowX - 190, Y - 10, Y + 10, DeckZ, DeckZ + 95);
        P.Wood.B(BowX - 212, BowX - 188, -42, 42, DeckZ + 70, DeckZ + 84);
        const float WX = 1330;
        P.Wood.Spar(FVector(WX, -112, DeckZ + 40), FVector(WX, 112, DeckZ + 40), 22, 22, 8);
        for (float Y : {-128.f, 128.f}) P.Wood.B(WX - 14, WX + 14, Y - 8, Y + 8, DeckZ, DeckZ + 72);
        for (float Side : {1.f, -1.f})
        {
            // The cable from the windlass out through its hawse hole.
            const float HX = 1560, HY = Side * (PostLine(1560) - 4);
            P.Rigging.Cord({FVector(WX + 6, Side * 60, DeckZ + 40), FVector((WX + HX) * .5f, Side * 70, DeckZ + 8), FVector(HX, HY, DeckZ + 14)}, 4.5f);
            // A cathead out over the side, and the anchor hung from it.
            const float CX = 1470, In = PostLine(CX) - 30, Out = PostLine(CX) + 75;
            Beam(P.Wood, FVector(CX, Side * In, WaistTopZ + 30), FVector(CX + 12, Side * Out, WaistTopZ + 46), 9, 9);
            const FVector Ring(CX + 12, Side * (Out - 8), WaistTopZ + 30);
            P.Iron.Rope({Ring, Ring - FVector(0, 0, 20)}, 2.f);
            const FVector Shank0 = Ring - FVector(0, 0, 22), Crown = Shank0 - FVector(0, 0, 190);
            P.Iron.Spar(Shank0, Crown, 5, 6, 8);
            P.Wood.TurnedBox(Shank0 - FVector(0, 0, 14), FVector(6, 90, 6), FRotator(0, 0, 0));   // the stock, across
            for (float F : {-1.f, 1.f})
            {
                const FVector Arm1 = Crown + FVector(F * 40, 0, 18), Arm2 = Crown + FVector(F * 62, 0, 62);
                P.Iron.Rope({Crown, Arm1, Arm2}, 5.f);
                P.Iron.TurnedBox(Arm2 + FVector(F * -4, 0, -10), FVector(3, 16, 14), FRotator(F * 28, 0, 0));
            }
        }
        // The bobstay under the bowsprit to the stem.
        P.Rigging.Cord({BowspritAt(.62f), FVector(BowX + 10, 0, WaterZ + 70)}, 2.4f);
    }
    // ---- The quarterdeck: the helm (its wheel facing forward, the binnacle before it) and the cabin's skylight.
    {
        const float Z = QuarterZ, HX = HelmX;
        for (float Y : {-34.f, 34.f}) P.Wood.B(HX - 6, HX + 6, Y - 6, Y + 6, Z, Z + 122);
        P.Wood.B(HX - 8, HX + 8, -40, 40, Z + 112, Z + 122);
        // (2026-11-04: the wheel on the posts' aft side, so the helmsman stands behind it looking forward over it at the
        // binnacle; its rim, spokes and handles are their own part (P.Wheel, about the hub), turned by the captain in the storm.)
        P.Iron.Spar(FVector(HX + 10, 0, Z + 106), FVector(HX - 30, 0, Z + 106), 3.5f, 3.5f, 8);
        const FVector Hub = HelmHub();
        P.Wood.Spar(Hub - FVector(8, 0, 0), Hub + FVector(8, 0, 0), 14, 14, 14);
        P.Brass.Spar(Hub - FVector(8, 0, 0), Hub - FVector(10, 0, 0), 8, 6, 12);
        TArray<FVector> Rim;
        for (int32 K = 0; K <= 28; ++K) { const float A = K / 28.f * 2 * PI; Rim.Add(FVector(0, 70 * FMath::Cos(A), 70 * FMath::Sin(A))); }
        P.Wheel.Rope(Rim, 4.5f);
        for (int32 K = 0; K < HelmSpokes; ++K)
        {
            const FVector Dir = HelmSpoke(K, 0.f);
            P.Wheel.Spar(Dir * 12, Dir * 70, 3.2f, 2.4f, 6);
            P.Wheel.Spar(Dir * 74, Dir * 92, 2.8f, 2.f, 8);
        }
        P.Hidden.B(Hub.X - 8, Hub.X + 8, -92, 92, Z + 16, Z + 198);
        // The binnacle: a cabinet and its brass hood.
        P.Wood.B(HX + 70, HX + 116, -23, 23, Z, Z + 92);
        P.Brass.Spar(FVector(HX + 93, 0, Z + 92), FVector(HX + 93, 0, Z + 116), 19, 5, 14);
        // The skylight: a coaming, a glazed ridge roof with its bars, solid to stand on.
        const float X0 = SkyX0, X1 = SkyX1, H = SkyHalf;
        P.Wood.B(X0 - 8, X1 + 8, -H - 8, -H, Z, Z + 42); P.Wood.B(X0 - 8, X1 + 8, H, H + 8, Z, Z + 42);
        P.Wood.B(X0 - 8, X0, -H, H, Z, Z + 42); P.Wood.B(X1, X1 + 8, -H, H, Z, Z + 42);
        for (float S : {-1.f, 1.f})
        {
            const FVector A(X0, S * H, Z + 42), B(X1, S * H, Z + 42), C(X1, 0, Z + 64), D(X0, 0, Z + 64);
            const FVector Nn = FVector(0, S * 22, 50).GetSafeNormal();
            P.Glass.Flat(A, B, C, D, Nn); P.Glass.Flat(A, B, C, D, -Nn);
            for (float X = X0 + 4; X <= X1 - 3; X += 26.5f) Beam(P.WoodDecor, FVector(X, S * H, Z + 43), FVector(X, 0, Z + 65), 1.5f, 1.5f);
        }
        P.WoodDecor.B(X0 - 4, X1 + 4, -3, 3, Z + 62, Z + 68);
        for (float X : {X0 + 2.f, X1 - 2.f}) for (float S : {-1.f, 1.f}) P.WoodDecor.B(X - 2, X + 2, S > 0 ? 0 : -H, S > 0 ? H : 0, Z + 40, Z + 44);
        P.Hidden.B(X0 - 8, X1 + 8, -H - 8, H + 8, Z, Z + 62);
        // The stern lantern on its bracket over the taffrail (unlit by day).
        const FVector L(SternX - 34, 0, QuarterRailZ + 58);
        Beam(P.Iron, FVector(SternX - 4, 0, QuarterRailZ + 4), FVector(SternX - 34, 0, QuarterRailZ + 26), 3, 3);
        P.Iron.B(L.X - 16, L.X + 16, -16, 16, L.Z - 26, L.Z - 22); P.Iron.B(L.X - 14, L.X + 14, -14, 14, L.Z + 22, L.Z + 26);
        for (float X : {-13.f, 13.f}) for (float Y : {-13.f, 13.f}) P.Iron.B(L.X + X - 1.5f, L.X + X + 1.5f, Y - 1.5f, Y + 1.5f, L.Z - 22, L.Z + 22);
        P.Glass.B(L.X - 12, L.X + 12, -12, 12, L.Z - 22, L.Z + 22);
        P.Brass.Spar(L + FVector(0, 0, 26), L + FVector(0, 0, 44), 16, 3, 10);
    }
}

// ------------------------------------------------------------------ the rig
void BuildRig(FParts& P)
{
    for (const FMast* Mp : {&MainMast(), &ForeMast()})
    {
        const FMast& M = *Mp;
        const bool bMain = M.X < 0;
        // The lower mast above the deck (solid: he walks round it), its bands; the masthead above the top, its cap.
        P.Wood.Spar(FVector(M.X, 0, DeckZ), FVector(M.X, 0, M.Top + 130), MastR, HeadR, 14);
        for (float Z : {DeckZ + 90, DeckZ + 210, DeckZ + 330, DeckZ + 450, DeckZ + 570}) P.Iron.Spar(FVector(M.X, 0, Z), FVector(M.X, 0, Z + 5), MastR + 1.2f - (Z - DeckZ) / (M.Top + 130 - DeckZ) * (MastR - HeadR), MastR + 1.2f - (Z - DeckZ) / (M.Top + 130 - DeckZ) * (MastR - HeadR), 14);
        P.WoodDecor.B(M.X - 28, TopmastX(M) + 19, -24, 24, M.Top + 128, M.Top + 146);
        // The topmast, forward of the masthead, from just over the top; its cap; the topgallant mast and its truck.
        P.WoodDecor.Spar(FVector(TopmastX(M), 0, M.Top + 2), FVector(TopmastX(M), 0, M.Top + 900), TopmastR, 12, 12);
        P.WoodDecor.B(TopmastX(M) - 14, GallantX(M) + 12, -14, 14, M.Top + 880, M.Top + 894);
        P.WoodDecor.Spar(FVector(GallantX(M), 0, M.Top + 800), FVector(GallantX(M), 0, M.Top + 1350), GallantR, 6, 10);
        P.WoodDecor.Spar(FVector(GallantX(M), 0, M.Top + 1350), FVector(GallantX(M), 0, M.Top + 1362), 9, 9, 10);
        // The masts above the top are solid too (user 2026-11-08: on the golden leaf he can climb them right to the truck).
        // One hidden, seamless shell round the topmast and topgallant (overlapping closed shapes would leave caps inside
        // that his ledge test takes for edges): each section the hull of the two spars' circles there. Over the doubling,
        // where the topgallant stands forward of the topmast's head, it fairs each face of one into the other with no ledge
        // or overhang to stop a climb: the fore face slants out onto the topgallant's heel, the aft face and sides lean
        // forward onto it over the topmast's head. The truck's top is the last hold.
        {
            const float Tx = TopmastX(M), Gx = GallantX(M);
            auto Smooth = [](float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3 - 2 * T); };
            auto Section = [&](float Z, float& Ca, float& Ra, float& Cb, float& Rb)
            {
                const float H = Z - M.Top;
                const float Rt = 17.f - 5.f * (H - 2) / 898.f, Rg = H < 800 ? GallantR : GallantR - 4.f * (H - 800) / 550.f;
                const float In = Smooth((H - 680) / 120), Out = Smooth((H - 860) / 130);   // the topgallant coming in, the topmast going out
                Ca = FMath::Lerp(Tx, Gx, Out); Ra = FMath::Lerp(Rt, Rg, Out);
                Cb = FMath::Lerp(Tx, Gx, In); Rb = FMath::Lerp(Rt, Rg, In);
                if (H > 1350) { Ra = Rb = 9; Ca = Cb = Gx; }   // the truck
            };
            TArray<float> Zs;
            for (float H = -12; H < 680; H += 50) Zs.Add(M.Top + H);
            for (float H = 680; H < 1000; H += 10) Zs.Add(M.Top + H);
            for (float H = 1000; H < 1350; H += 50) Zs.Add(M.Top + H);
            Zs.Append({M.Top + 1350, M.Top + 1350.5f, M.Top + 1362});
            constexpr int32 Around = 24;
            auto Ring = [&](float Z, TArray<FVector>& Out)
            {
                float Ca, Ra, Cb, Rb; Section(Z, Ca, Ra, Cb, Rb);
                Out.Reset();
                for (int32 K = 0; K < Around; ++K)
                {
                    const float An = K * 2 * PI / Around; const FVector D(FMath::Cos(An), FMath::Sin(An), 0);
                    const bool bA = Ca * D.X + Ra >= Cb * D.X + Rb;
                    Out.Add(FVector(bA ? Ca : Cb, 0, Z) + D * (bA ? Ra : Rb));
                }
            };
            TArray<FVector> Lo, Hi;
            Ring(Zs[0], Lo);
            for (int32 I = 1; I < Zs.Num(); ++I)
            {
                Ring(Zs[I], Hi);
                for (int32 K = 0; K < Around; ++K)
                {
                    const int32 K1 = (K + 1) % Around;
                    auto Dir = [](int32 J) { const float An = J * 2 * PI / Around; return FVector(FMath::Cos(An), FMath::Sin(An), 0); };   // (outward)
                    const FVector N0 = Dir(K), N1 = Dir(K1);
                    P.Hidden.Quad(Lo[K], Lo[K1], Hi[K1], Hi[K], N0, N1, N1, N0);
                }
                Lo = Hi;
            }
            // Its ends: the heel under the top's floor, the truck's top.
            for (int32 K = 0; K < Around; ++K)
            {
                const int32 K1 = (K + 1) % Around;
                P.Hidden.Tri(FVector(Gx, 0, M.Top + 1362), Lo[K], Lo[K1], FVector::UpVector);
            }
            Ring(Zs[0], Lo);
            for (int32 K = 0; K < Around; ++K) P.Hidden.Tri(FVector(Tx, 0, Zs[0]), Lo[K], Lo[(K + 1) % Around], -FVector::UpVector);
        }
        // Crosstrees at the topmast head.
        P.WoodDecor.B(TopmastX(M) - 10, TopmastX(M) + 10, -90, 90, M.Top + 868, M.Top + 876);
        P.WoodDecor.B(TopmastX(M) - 40, TopmastX(M) + 40, -40, -32, M.Top + 868, M.Top + 876); P.WoodDecor.B(TopmastX(M) - 40, TopmastX(M) + 40, 32, 40, M.Top + 868, M.Top + 876);

        // ---- The top: a platform on its trestletrees round the mast; a rim.
        const float X0 = bMain ? TopX0 : M.X - 70, X1 = bMain ? TopX1 : M.X + 130, H = bMain ? TopHalf : 120;
        TArray<FHole> Holes = {{M.X - 27, TopmastX(M) + 19, -27, 27}};
        Slab(P.Wood, X0, X1, -H, H, M.Top - 12, M.Top, Holes);
        for (float Y : {-H, H - 4}) P.WoodDecor.B(X0, X1, Y, Y + 4, M.Top, M.Top + 5);
        P.WoodDecor.B(X0, X0 + 4, -H, H, M.Top, M.Top + 5); P.WoodDecor.B(X1 - 4, X1, -H, H, M.Top, M.Top + 5);
        for (float Y : {-32.f, 32.f}) P.WoodDecor.B(X0 + 10, X1 - 10, Y - 6, Y + 6, M.Top - 30, M.Top - 12);   // trestletrees
        for (float X : {M.X - 40, M.X + 100}) P.WoodDecor.B(X - 5, X + 5, -H + 8, H - 8, M.Top - 24, M.Top - 12);   // crosstrees under it
        // A clay jar the lookout keeps up there (the main top), on its fore starboard corner (clear of the nets' heads).
        if (bMain) GJars.Add(FVector(M.X + 150, -92, M.Top));

        // ---- The yards (tapered both ways from their slings), the main course furled on its yard, the footropes.
        for (int32 W = 0; W < 3; ++W)
        {
            const FVector C = YardCentre(M, W);
            const float R0 = W == 0 ? 12 : W == 1 ? 9 : 6;
            for (float Side : {1.f, -1.f}) P.WoodDecor.Spar(C, YardEnd(M, W, Side), R0, R0 * .5f, 10);
            for (float Side : {1.f, -1.f})
            {
                const FVector E = YardEnd(M, W, Side, 30);
                Sagging(P.Rigging, C + FVector(-6, 0, -4) + (E - C).GetSafeNormal() * 40, E + FVector(-6, 0, -6), 1.1f, W == 0 ? 110 : 70, 8);
            }
        }
        if (bMain)
        {
            // The main course, furled: a roll of canvas along the top of the yard, lashed with gaskets.
            const FVector C = YardCentre(M, 0);
            P.CanvasDecor.Spar(C + FVector(-2, -M.CourseHalf + 40, 16), C + FVector(-2, M.CourseHalf - 40, 16), 15, 15, 12);
            for (float Side : {1.f, -1.f}) P.CanvasDecor.Spar(C + FVector(-2, Side * (M.CourseHalf - 40), 16), C + FVector(-2, Side * (M.CourseHalf - 18), 12), 15, 7, 12);
            for (float Y = -M.CourseHalf + 60; Y <= M.CourseHalf - 50; Y += 75)
            {
                TArray<FVector> Loop;
                for (int32 K = 0; K <= 10; ++K) { const float A = K / 10.f * 2 * PI; Loop.Add(C + FVector(-2 + 17 * FMath::Cos(A), Y, 8 + 17 * FMath::Sin(A))); }
                P.Rigging.Cord(Loop, .9f);
            }
        }

        // ---- Standing rigging: the shrouds from the channels to the masthead, with ratlines; the futtock shrouds; the
        // topmast shrouds from the top's rim to the topmast head, with ratlines; backstays.
        for (float Side : {1.f, -1.f})
        {
            const float CX0 = M.X - 150, CX1 = M.X + 70;
            for (float X = CX0; X < CX1; X += 22) P.Wood.B(X, X + 22, Side > 0 ? PostLine(X + 11) + 4 : -PostLine(X + 11) - 36, Side > 0 ? PostLine(X + 11) + 36 : -PostLine(X + 11) - 4, WaistTopZ - 20, WaistTopZ - 10);   // the channel
            FVector Low[4], High[4];
            for (int32 K = 0; K < 4; ++K)
            {
                const FVector Dead = ShroudLow(M, Side, K) - FVector(0, 0, 54);
                const float Xk = static_cast<float>(Dead.X);
                P.WoodDecor.Spar(Dead - FVector(0, 0, 9), Dead + FVector(0, 0, 9), 9, 9, 10);           // the lower deadeye
                P.WoodDecor.Spar(Dead + FVector(0, 0, 36), Dead + FVector(0, 0, 54), 9, 9, 10);         // the upper
                for (float O : {-4.f, 0.f, 4.f}) P.Hemp.Cord({Dead + FVector(O, 0, 8), Dead + FVector(O, 0, 37)}, .7f);   // lanyards
                P.Iron.Rope({Dead - FVector(0, 0, 9), FVector(Xk, Side * (PostLine(Xk) + 8), WaistTopZ - 110)}, 1.6f);       // chainplate
                Low[K] = ShroudLow(M, Side, K);
                High[K] = ShroudHigh(M, Side, K);
                P.Rigging.Cord({Low[K], High[K]}, 2.2f);
                P.Iron.Spar(High[K] - FVector(0, 0, 5), High[K] + FVector(0, 0, 3), 3.4f, 3.4f, 8);   // its band at the rim
            }
            // The net at the foot: its legs from the sheer pole on deck out to the shrouds over the rail, its ratlines, the
            // pole lashed to the nearest stanchions' feet (and seized to the shrouds at the join).
            {
                FVector Foot[4], Join[4];
                for (int32 K = 0; K < 4; ++K)
                {
                    Foot[K] = NetFoot(M, Side, K); Join[K] = NetJoin(M, Side, K);
                    P.Rigging.Cord({Foot[K], Join[K]}, 1.9f);
                    P.Rigging.Cord({Join[K] - FVector(0, 0, 4), Join[K] + FVector(0, 0, 4)}, 3.2f);   // the seizing
                }
                for (float Z = DeckZ + 36; Z < NetJoinZ - 12; Z += 36)
                {
                    TArray<FVector> Line;
                    for (int32 K = 0; K < 4; ++K) Line.Add(FMath::Lerp(Foot[K], Join[K], (Z - Foot[K].Z) / (Join[K].Z - Foot[K].Z)));
                    (bMain ? P.Rigging : P.ForeRatlines).Cord(Line, .9f);   // (the fore's carry away in the storm: ShipEnding.cpp)
                }
                const FVector PoleA = Foot[0] - FVector(18, 0, 0), PoleB = Foot[3] + FVector(18, 0, 0);
                P.WoodDecor.Spar(PoleA, PoleB, 4.5f, 4.5f, 8);
                for (const FVector& E : {PoleA, PoleB}) P.WoodDecor.Spar(E - FVector(0, 0, 5), E + FVector(0, 0, -1), 5.5f, 5.5f, 8);   // its chocks
                TArray<FVector2D> RailLine, Posts;
                RailPosts(Side, 1, RailLine, Posts);
                for (const FVector2D& Pt : Posts)
                {
                    if (Pt.X < PoleA.X - 30 || Pt.X > PoleB.X + 30) continue;
                    const FVector Pole(FMath::Clamp(static_cast<float>(Pt.X), static_cast<float>(PoleA.X) + 4, static_cast<float>(PoleB.X) - 4), PoleA.Y, PoleA.Z);
                    const FVector PostFoot(Pt.X, Pt.Y - Side * 6, WaistTopZ + 5);
                    P.Iron.Spar(PostFoot + FVector(0, 0, -3), PostFoot + FVector(0, 0, 3), 6.f, 6.f, 8);           // a band round the post's foot
                    for (float O : {-2.f, 2.f}) P.Rigging.Cord({Pole + FVector(O, 0, 3), PostFoot + FVector(O, 0, 0)}, .8f);   // the lashing
                }
            }
            for (float Z = Low[0].Z + 38; Z < M.Top - 30; Z += 38)
            {
                TArray<FVector> Line;
                for (int32 K = 0; K < 4; ++K) { const float T = (Z - Low[K].Z) / (High[K].Z - Low[K].Z); Line.Add(FMath::Lerp(Low[K], High[K], T)); }
                (bMain ? P.Rigging : P.ForeRatlines).Cord(Line, .9f);
            }
            // Futtock shrouds under the top (aft of the yard), from its rim to the mast below.
            for (float Dx : {-55.f, -20.f}) P.Rigging.Cord({FVector(M.X + Dx, Side * (H - 6), M.Top - 10), FVector(M.X + Dx * .4f, Side * (MastR - 2), M.Top - 170)}, 1.6f);
            // Topmast shrouds and their ratlines.
            FVector TL[3], TH[3];
            for (int32 K = 0; K < 3; ++K)
            {
                TL[K] = TopShroudLow(M, Side, K);
                TH[K] = TopShroudHigh(M, Side, K);
                P.WoodDecor.Spar(TL[K] - FVector(0, 0, 4), TL[K] + FVector(0, 0, 8), 6, 6, 8);
                P.Rigging.Cord({TL[K] + FVector(0, 0, 8), TH[K]}, 1.6f);
            }
            for (float Z = M.Top + 50; Z < M.Top + 800; Z += 36)
            {
                TArray<FVector> Line;
                for (int32 K = 0; K < 3; ++K) { const float T = (Z - TL[K].Z) / (TH[K].Z - TL[K].Z); Line.Add(FMath::Lerp(TL[K] + FVector(0, 0, 8), TH[K], T)); }
                P.Rigging.Cord(Line, .7f);
            }
            // Backstays from the topmast and topgallant heads down to the side aft of the shrouds.
            const float BX = M.X - 230;
            P.Rigging.Cord({FVector(TopmastX(M), Side * 12, M.Top + 870), FVector(BX, Side * (PostLine(BX) + 22), WaistTopZ + 30)}, 1.8f);
            P.Rigging.Cord({FVector(GallantX(M), Side * 8, M.Top + 1330), FVector(BX - 60, Side * (PostLine(BX - 60) + 22), WaistTopZ + 30)}, 1.4f);
            P.WoodDecor.Spar(FVector(BX, Side * (PostLine(BX) + 22), WaistTopZ + 20), FVector(BX, Side * (PostLine(BX) + 22), WaistTopZ + 38), 8, 8, 10);
        }

        // ---- Running rigging: lifts to the yardarms, the halyards down to the fife rail, braces aft to the rail.
        for (int32 W = 0; W < 3; ++W)
        {
            const float LiftZ = W == 0 ? M.Top + 120 : W == 1 ? M.Top + 1250 : M.Top + 1345;
            const float LiftX = W == 0 ? M.X + 10 : W == 1 ? TopmastX(M) : GallantX(M);
            for (float Side : {1.f, -1.f})
            {
                const FVector E = YardEnd(M, W, Side, 6);
                P.Hemp.Cord({FVector(LiftX, Side * 10, LiftZ), E + FVector(0, 0, 4)}, .9f);
                // The brace: from the yardarm aft (the fore's to the main mast, the main's to the quarterdeck's rail).
                const FVector To = bMain ? FVector(-1340, Side * (PostLine(-1340) - 6), QuarterRailZ - 6) : FVector(MainMastX + 20, Side * (HeadR + 6), MainTopZ - 120 + W * 260);
                P.Hemp.Cord({E, To}, 1.f);
            }
            const FVector C = YardCentre(M, W);
            P.Hemp.Cord({C + FVector(-14, 0, 10), FVector(M.X + MastR + 6, 6, DeckZ + 80)}, 1.1f);
        }
        // The fore course's sheet and tack, from its clews down to a block on the rail.
        if (!bMain)
            for (float Side : {1.f, -1.f})
            {
                const FVector Clew = CourseClew(Side), Lead = CourseClewLead(Side);
                P.Hemp.Cord({Clew, Lead + FVector(0, 0, 6)}, 1.2f);
                P.WoodDecor.Spar(Lead + FVector(0, 0, 1), Lead + FVector(0, 0, 11), 4.5f, 4.f, 8);   // the block
            }
    }
    // ---- The stays: fore stay and fore topmast stay to the bowsprit (the staysails ride them), the main stays forward.
    const FMast& F = ForeMast(); const FMast& Mm = MainMast();
    P.Rigging.Cord({FVector(F.X + 22, 0, F.Top + 116), BowspritAt(.5f)}, 3.f);
    P.Rigging.Cord({FVector(TopmastX(F) + 10, 0, F.Top + 866), BowspritAt(1.f)}, 2.2f);
    P.Rigging.Cord({FVector(GallantX(F) + 6, 0, F.Top + 1330), BowspritAt(1.f) + FVector(40, 0, 14)}, 1.6f);
    P.Rigging.Cord({FVector(Mm.X + 22, 0, Mm.Top + 132), FVector(F.X - 26, 0, F.Top - 30)}, 3.f);
    P.Rigging.Cord({FVector(TopmastX(Mm) + 10, 0, Mm.Top + 866), FVector(F.X + 24, 0, F.Top + 128)}, 2.2f);
    P.Rigging.Cord({FVector(GallantX(Mm) + 6, 0, Mm.Top + 1330), FVector(TopmastX(F), 0, F.Top + 870)}, 1.6f);
    // The spanker's gaff and boom on the main mast, over the quarterdeck.
    P.WoodDecor.Spar(SpankerThroat(), SpankerPeak(), 8, 5, 10);
    P.WoodDecor.Spar(SpankerTack(), SpankerClew(), 9, 7, 10);
    P.Hemp.Cord({SpankerClew(), FVector(SternX + 30, 0, QuarterRailZ + 4)}, 1.1f);
    P.Hemp.Cord({SpankerPeak(), FVector(Mm.X + 10, 0, Mm.Top + 120)}, 1.f);
    // The flag's halyard: from a block at the peak down the flag's hoist, its tail hanging below it.
    P.Hemp.Cord({SpankerPeak() + FVector(-4, 6, -6), FlagHoist(), FlagHoist() - FVector(0, 0, FlagDrop), FlagHoist() - FVector(2, -3, FlagDrop + 70)}, .7f);
}

// ------------------------------------------------------------------ the sails (built each frame: they breathe)

// A sail: four corners (A, B along the head; D, C along the foot: a staysail's head is A = B), the way it fills, its
// depth there, which edges are held (a square sail's head and both clews; a staysail's luff on its stay; the spanker's
// luff, head and foot), and its own phase in the breathing.
// A square sail's foot is cut in a curve (its roach), so the topsails' feet sweep clear over the tops.
struct FSailDef { FVector A, B, C, D, Fill; float Depth, Phase; int32 Kind; float Roach = 0; };   // Kind 0 square, 1 staysail, 2 spanker
TArray<FSailDef> SailDefs()
{
    TArray<FSailDef> Out;
    const FMast& F = ForeMast(); const FMast& Mm = MainMast();
    auto Square = [&](const FMast& M, int32 Head, float Inset, FVector Dl, FVector Dr, float Phase, float Roach)
    {
        const FVector A = YardEnd(M, Head, 1, Inset) + FVector(6, 0, 8), B = YardEnd(M, Head, -1, Inset) + FVector(6, 0, 8);
        const float W = FVector::Dist(A, B);
        Out.Add({A, B, Dr, Dl, (YardNormal(M) + FVector(0, .28f, 0)).GetSafeNormal(), W * .12f, Phase, 0, Roach});
    };
    // The fore course: its clews at the ship's side (CourseClew), its foot high enough over the deck for the crew.
    Square(F, 0, 20, CourseClew(1), CourseClew(-1), 0, 30);
    // The topsails and topgallants: their clews at the yardarms below.
    Square(F, 1, 14, YardEnd(F, 0, 1, 18) + FVector(4, 0, 10), YardEnd(F, 0, -1, 18) + FVector(4, 0, 10), 1.1f, 160);
    Square(F, 2, 10, YardEnd(F, 1, 1, 14) + FVector(4, 0, 8), YardEnd(F, 1, -1, 14) + FVector(4, 0, 8), 2.3f, 40);
    Square(Mm, 1, 14, YardEnd(Mm, 0, 1, 26) + FVector(4, 0, 12), YardEnd(Mm, 0, -1, 26) + FVector(4, 0, 12), .6f, 175);
    Square(Mm, 2, 10, YardEnd(Mm, 1, 1, 14) + FVector(4, 0, 8), YardEnd(Mm, 1, -1, 14) + FVector(4, 0, 8), 1.8f, 40);
    // The staysails on the fore topmast stay and the jib stay, their clews trailing aft to port (leeward).
    {
        const FVector S0(TopmastX(F) + 10, 0, F.Top + 866), S1 = BowspritAt(1.f);
        const FVector Head = FMath::Lerp(S0, S1, .1f), Tack = FMath::Lerp(S0, S1, .84f);
        Out.Add({Head, Head, FVector(F.X + 420, 160, DeckZ + 330), Tack, FVector(.15f, 1, 0).GetSafeNormal(), 85, 2.9f, 1});
        const FVector J0(GallantX(F) + 6, 0, F.Top + 1330), J1 = BowspritAt(1.f) + FVector(40, 0, 14);
        const FVector JHead = FMath::Lerp(J0, J1, .08f), JTack = FMath::Lerp(J0, J1, .9f);
        Out.Add({JHead, JHead, FVector(F.X + 760, 210, DeckZ + 420), JTack, FVector(.15f, 1, 0).GetSafeNormal(), 75, .4f, 1});
    }
    // The spanker on its gaff and boom, filled to port.
    Out.Add({SpankerThroat() + FVector(-6, 0, -10), SpankerPeak() + FVector(0, 0, -8), SpankerClew() + FVector(0, 0, 10), SpankerTack() + FVector(-6, 0, 10), FVector(0, 1, 0), 70, 3.7f, 2});
    return Out;
}
// A point of the sail at (U across, V down) at time T: the bilinear sheet pushed out along its fill, held at its fixed
// edges, breathing with the swell and fluttering a little at its free edge.
// The storm's wind (0..1) and the canvas's own clock, which runs faster as the wind rises (AShipSetting::UpdateSails sets
// both): in the gale the sails stand fuller and their free edges thrum, the flag streams out flat and cracks.
float GSailWind = 0, GFlutter = 0;
// Chuck, as a thick segment (ship frame; radius 0: not near): the canvas gives way round him, pushed out to the far side
// of him in a soft dome (2026-11-05: he hangs from the topsail and fore yards right where their sails hang).
FVector GSailBodyA = FVector::ZeroVector, GSailBodyB = FVector::ZeroVector;
float GSailBodyR = 0;
FVector SailGiveWay(const FSailDef& S, const FVector& P)
{
    if (GSailBodyR <= 0) return P;
    const FVector Q = FMath::ClosestPointOnSegment(P, GSailBodyA, GSailBodyB);
    const FVector D = P - Q;
    const float Along = static_cast<float>(FVector::DotProduct(D, S.Fill));
    const float Lat2 = FMath::Max(0.f, static_cast<float>(D.SizeSquared()) - Along * Along);
    const float Reach = GSailBodyR * 2.8f;
    if (Lat2 >= Reach * Reach) return P;
    const float Dome = GSailBodyR * 1.15f * (1.f - Lat2 / (Reach * Reach));
    if (Along >= 0) return Along < Dome ? P + S.Fill * (Dome - Along) : P;
    return -Along < Dome ? P - S.Fill * (Dome + Along) : P;
}
FVector SailPoint(const FSailDef& S, float U, float V, float T)
{
    const FVector Top = FMath::Lerp(S.A, S.B, U), Bottom = FMath::Lerp(S.D, S.C, U) + FVector(0, 0, S.Roach * FMath::Sin(PI * U));
    FVector P = FMath::Lerp(Top, Bottom, V);
    float Shape, Free;
    switch (S.Kind)
    {
    case 0: Shape = FMath::Sin(PI * U) * FMath::Sin(PI * .6f * V); Free = V * V; break;                                    // head and clews held; the foot free
    case 1: Shape = FMath::Sin(PI * FMath::Min(1.f, U * .62f + .38f * U * U)) * FMath::Sin(PI * V); Free = U * U; break;   // luff on the stay; the leech free
    default: Shape = FMath::Sin(PI * U * .62f) * FMath::Sin(PI * V); Free = U * U; break;                                 // luff, head, foot held; the leech free
    }
    const float Breath = 1 + .07f * FMath::Sin(T * 2 * PI / 7.3f + S.Phase) + .025f * FMath::Sin(T * 2 * PI / 2.1f + S.Phase * 2.3f);
    const float F = GFlutter;
    const float Flutter = (1.6f + 5.5f * GSailWind) * Free * (FMath::Sin(F * 7.f + U * 11.f + V * 7.f + S.Phase) + .45f * GSailWind * FMath::Sin(F * 17.f + U * 23.f - V * 9.f + S.Phase * 3.f)) * FMath::Sin(PI * V);
    const float Gust = 1.f + GSailWind * (.22f + .06f * FMath::Sin(T * 2 * PI / 4.1f + S.Phase));
    P += S.Fill * (S.Depth * Shape * Breath * Gust + Flutter);
    return SailGiveWay(S, P);
}
// The fore course against the fore lower shrouds and their ratlines (2026-11-08): how many times any of them passes
// through the canvas, and the canvas's least clearance from them, over calm and gale and the sail's breathing.
void CourseNetClearance(int32& Crossings, float& Clearance)
{
    const FSailDef S = SailDefs()[0];
    const float Wind0 = GSailWind, Flutter0 = GFlutter, Body0 = GSailBodyR;
    GSailBodyR = 0;
    struct FSeg { FVector A, B; float R; };
    TArray<FSeg> Segs;
    const FMast& M = ForeMast();
    for (float Side : {1.f, -1.f})
    {
        FVector Low[4], High[4];
        for (int32 K = 0; K < 4; ++K) { Low[K] = ShroudLow(M, Side, K); High[K] = ShroudHigh(M, Side, K); Segs.Add({Low[K], High[K], 2.2f}); }
        for (float Z = Low[0].Z + 38; Z < M.Top - 30; Z += 38)
            for (int32 K = 0; K < 3; ++K)
            {
                auto At = [&](int32 J) { return FMath::Lerp(Low[J], High[J], (Z - Low[J].Z) / (High[J].Z - Low[J].Z)); };
                Segs.Add({At(K), At(K + 1), .9f});
            }
    }
    constexpr int32 NU = 32, NV = 24;
    const float States[][3] = {{0, 0, 0}, {0, 3, 1}, {1, 1, 2}, {1, 5, 7.7f}, {1, 2.2f, 13}};   // wind, time, the canvas's clock
    Crossings = 0; Clearance = 1e9f;
    TArray<FVector> G; G.SetNum((NU + 1) * (NV + 1));
    for (const auto& St : States)
    {
        GSailWind = St[0]; GFlutter = St[2];
        for (int32 J = 0; J <= NV; ++J) for (int32 I = 0; I <= NU; ++I) G[J * (NU + 1) + I] = SailPoint(S, I / float(NU), J / float(NV), St[1]);
        for (const FSeg& Sg : Segs)
        {
            const FBox Box = FBox(TArray<FVector>{Sg.A, Sg.B}).ExpandBy(5);
            for (const FVector& P : G) Clearance = FMath::Min(Clearance, static_cast<float>(FMath::PointDistToSegment(P, Sg.A, Sg.B)) - Sg.R);
            for (int32 J = 0; J < NV; ++J)
                for (int32 I = 0; I < NU; ++I)
                {
                    const FVector& A = G[J * (NU + 1) + I]; const FVector& B = G[J * (NU + 1) + I + 1];
                    const FVector& C = G[(J + 1) * (NU + 1) + I + 1]; const FVector& D = G[(J + 1) * (NU + 1) + I];
                    if (!Box.Intersect(FBox(TArray<FVector>{A, B, C, D}))) continue;
                    FVector Hit, N;
                    Crossings += FMath::SegmentTriangleIntersection(Sg.A, Sg.B, A, B, C, Hit, N) || FMath::SegmentTriangleIntersection(Sg.A, Sg.B, A, C, D, Hit, N);
                }
        }
    }
    GSailWind = Wind0; GFlutter = Flutter0; GSailBodyR = Body0;
}
// The flag (user 2026-11-02: more prominent): flown as a ship's colours were under sail, from the spanker gaff's peak over
// the quarterdeck, hoisted on its halyard just aft of the peak and streaming to leeward (port, as the spanker fills), clear
// of the sail's leech. 2.6 x 1.65 m, about a big ensign's size, and some 10 m over the quarterdeck where it is seen.
FVector FlagPoint(float U, float V, float T)
{
    const FVector Along = FVector(.42f, .9f, -.05f).GetSafeNormal(), Across(-Along.Y, Along.X, 0);
    const FVector Fly = Along * FlagFly * U;
    const float Tf = GFlutter, W = GSailWind;
    const float Wave = (14 + 10 * W) * U * FMath::Sin(U * 5.f - Tf * 6.5f) + (4 + 9 * W) * U * FMath::Sin(U * 11.f - Tf * 9.f + V * 2.f)
        + 6 * W * U * U * FMath::Sin(U * 19.f - Tf * 14.f + V * 3.f);
    return FlagHoist() + Fly + FVector(0, 0, -V * FlagDrop - U * U * 14 * (1 - .75f * W)) + Across * Wave;
}
