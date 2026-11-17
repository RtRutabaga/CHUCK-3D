// The bus stop in Phlegethos's wastes (Claude, 2026-11-17; docs/regions/PHLEGETHOS.md#the-bus-stop). Included at the end of
// PhlegethosSetting.cpp (it builds with the stage's FPhlegSurf and Emit).
//
// The user: "the modern city bus stop with business man Npc with small astral ruptures to a sensible place along the
// plethegos path, similar to 2d game. No umbrella for the business man." The 2D fractured way's tableau (a displaced
// modern bus shelter and its commuter, between the rubble pass and the fortress approach): here a fragment of a city
// street fallen into the wastes beside the path, half way across them - a strip of road with its lane markings and a bus
// box, a kerb, the pavement, a glass shelter with a lit advertising panel, a bench, the stop's sign and timetable, a bin.
// Its ends are broken off, the rebar showing; one of the small ruptures (Phleg::Rifts) has bitten off the road's end, two
// more cross the path either side. The commuter (EDockHuman::Businessman) is spawned with the creatures.
//
// Frame: u along the path (Phleg::BusStopFrame's T), v away from its centre line (N), z the stage's height; the fragment
// stands on the ground ApproachGround presses level there.

void APhlegethosSetting::BuildBusStop()
{
    FVector2D C, T, Nn; BusStopFrame(C, T, Nn);
    const float Zb = WastesPathLevel(static_cast<float>(C.X));
    auto P2 = [&](float U, float V) { return C + T * U + Nn * V; };
    auto P3 = [&](float U, float V, float Z) { const FVector2D Q = P2(U, V); return FVector(Q.X, Q.Y, Z); };
    auto UOf = [&](float X, float Y) { return static_cast<float>(FVector2D::DotProduct(FVector2D(X, Y) - C, T)); };
    const FVector T3(T.X, T.Y, 0), N3(Nn.X, Nn.Y, 0), Up = FVector::UpVector;
    // The fragment leans a little along its length, as it came down.
    auto Lean = [&](float U) { return .018f * U; };
    const float Zp = Zb + 14.f;   // the pavement's top at its middle; the road 14 cm under it

    UMaterialInterface* ConcreteMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_CityConcrete.M_CityConcrete"));
    UMaterialInterface* AsphaltMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_CityAsphalt.M_CityAsphalt"));
    UMaterialInterface* PaintMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_CityPaint.M_CityPaint"));
    UMaterialInterface* GlassMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_WindowGlass.M_WindowGlass"));
    FPhlegSurf Concrete, Asphalt, Paint, Glass, Lit, Rebar;

    // A ragged outline across the strip v0..v1 from u0 to u1, its two ends broken (jagged in u).
    auto Strip = [&](float U0, float U1, float V0, float V1, float Seed)
    {
        TArray<FVector2D> Out;
        constexpr int32 K = 5;
        for (int32 I = 0; I <= K; ++I) { const float V = FMath::Lerp(V0, V1, float(I) / K); Out.Add(P2(U1 + (I == 0 || I == K ? 0.f : 38.f * (Hash(V, Seed) - .5f)), V)); }
        for (int32 I = K; I >= 0; --I) { const float V = FMath::Lerp(V0, V1, float(I) / K); Out.Add(P2(U0 + (I == 0 || I == K ? 0.f : 44.f * (Hash(V + 3.f, Seed) - .5f)), V)); }
        return Out;
    };
    // The road: asphalt over its bed, its end at the rupture's lip.
    Asphalt.Paint = FColor(0, 0, 0, 255);
    Asphalt.Prism(Strip(-470, 430, 165, 485, 1), [&](float X, float Y) { return Zp - 14.f + Lean(UOf(X, Y)); }, Zb - 34, 2, 0, 41, 2);
    // Its markings: a dashed white line down the lane, a solid one by the kerb, the yellow bus box.
    auto Mark = [&](float U0, float U1, float V0, float V1, FColor Col)
    {
        Asphalt.Paint = Col;
        Asphalt.Flat(P3(U0, V0, Zp - 13.6f + Lean(U0)), P3(U1, V0, Zp - 13.6f + Lean(U1)), P3(U1, V1, Zp - 13.6f + Lean(U1)), P3(U0, V1, Zp - 13.6f + Lean(U0)), Up);
    };
    for (float U = -440; U < 400; U += 180) Mark(U, FMath::Min(U + 90.f, 400.f), 205, 216, FColor(255, 0, 0, 255));
    Mark(-450, 410, 462, 472, FColor(255, 0, 0, 255));
    Mark(-230, 230, 330, 340, FColor(0, 255, 0, 255)); Mark(-230, 230, 448, 456, FColor(0, 255, 0, 255));
    Mark(-230, -220, 340, 448, FColor(0, 255, 0, 255)); Mark(220, 230, 340, 448, FColor(0, 255, 0, 255));
    // The kerb and the pavement (60 cm flags), a little grimed.
    Concrete.Paint = FColor(80, 0, 0, 255);
    Concrete.Prism(Strip(-445, 455, 485, 517, 2), [&](float X, float Y) { return Zp + Lean(UOf(X, Y)); }, Zb - 34, 1, 0, 42, 1);
    Concrete.Paint = FColor(45, 255, 0, 255);
    Concrete.Prism(Strip(-430, 470, 517, 830, 3), [&](float X, float Y) { return Zp + Lean(UOf(X, Y)); }, Zb - 34, 3, 0, 43, 2);
    // Rebar out of the broken ends.
    Rebar.Paint = FColor(0, 0, 0, 255);
    for (int32 I = 0; I < 7; ++I)
    {
        const float V = 200.f + 85.f * I + 20.f * Hash(I, 7);
        const bool bEast = I % 2 == 0;
        const float U = bEast ? (V > 485 ? 470.f : 430.f) : (V > 485 ? -430.f : -470.f);
        const FVector A = P3(U - (bEast ? 20.f : -20.f), V, Zp - 20.f + Lean(U));
        Rebar.Spar(A, A + T3 * (bEast ? 1.f : -1.f) * (35.f + 40.f * Hash(I, 9)) + Up * (6.f + 25.f * Hash(I, 11)) + N3 * 12.f * (Hash(I, 13) - .5f), 1.1f, .9f, 5);
    }

    // The shelter: four aluminium posts, a dark roof with its fascia, glass at the back and one side, the advertising panel
    // at the other, a bench along the back.
    const FColor Alu(140, 143, 148, 230), Dark(24, 26, 29, 160), Green(9, 26, 16, 70), Blue(5, 24, 88, 0), White(205, 205, 200, 0);
    auto Z = [&](float U, float Up_) { return Zp + Lean(U) + Up_; };
    auto Bx = [&](FPhlegSurf& S, float U, float V, float Zc, float Hu, float Hv, float Hz) { S.Box(P3(U, V, Zc), T3 * Hu, N3 * Hv, Up * Hz); };
    Paint.Paint = Alu;
    for (float U : {-180.f, 180.f}) for (float V : {650.f, 786.f}) Bx(Paint, U, V, Z(U, 124), 4, 4, 124);
    Paint.Paint = Dark;
    Paint.TurnedBox(P3(0, 712, Z(0, 252)), FVector(205, 98, 5), FRotator(0, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X))), 0));
    Paint.Paint = Alu;
    Bx(Paint, 0, 613, Z(0, 252), 205, 3, 9);   // the fascia along its front
    Bx(Paint, 0, 788, Z(0, 17), 180, 2, 3); Bx(Paint, 0, 788, Z(0, 228), 180, 2, 3);
    for (float U : {-59.f, 59.f}) Bx(Paint, U, 788, Z(U, 122), 2, 2, 104);
    Bx(Paint, 182, 718, Z(182, 17), 2, 66, 3); Bx(Paint, 182, 718, Z(182, 228), 2, 66, 3);
    Glass.Paint = FColor(255, 255, 255, 255);
    for (float U0 : {-176.f, -57.f, 61.f}) Bx(Glass, U0 + 57.5f, 788, Z(U0 + 57.5f, 122), 57, .6f, 103);
    Bx(Glass, 182, 718, Z(182, 122), .6f, 64, 103);
    // The advertising panel: a lit lightbox in a dark frame, lit on both faces (no picture: a plain cold glow).
    Paint.Paint = Dark;
    Bx(Paint, -186, 718, Z(-186, 120), 9, 68, 97);
    Lit.Paint = FColor(62, 70, 84, 0);   // (a cold lightbox; at 205 it blew out white)
    for (float S : {-1.f, 1.f})
    {
        const float U = -186.f + S * 9.4f;
        Lit.Flat(P3(U, 660, Z(U, 31)), P3(U, 776, Z(U, 31)), P3(U, 776, Z(U, 209)), P3(U, 660, Z(U, 209)), T3 * S);
    }
    // The bench.
    Paint.Paint = Alu;
    for (int32 K = 0; K < 3; ++K) Bx(Paint, 0, 742 + K * 14, Z(0, 45), 100, 5.5f, 1.5f);
    for (float U : {-88.f, 88.f}) Bx(Paint, U, 756, Z(U, 22), 2.5f, 18, 22);
    Bx(Paint, 0, 780, Z(0, 70), 100, 1.5f, 6);
    // The stop's sign on its pole by the kerb, a timetable case on the pole; a bin.
    {
        const FVector Foot = P3(-292, 560, Z(-292, 0));
        Paint.Paint = Alu;
        Paint.Spar(Foot - Up * 10.f, Foot + Up * 292.f, 3.5f, 3.f, 8);
        Paint.Paint = Blue;
        Paint.Box(Foot + Up * 262.f, T3 * 31.f, N3 * 1.2f, Up * 23.f);
        Paint.Paint = White;
        for (float S : {-1.f, 1.f}) Paint.Flat(Foot + Up * 255.f + N3 * S * 1.3f - T3 * 25.f, Foot + Up * 255.f + N3 * S * 1.3f + T3 * 25.f, Foot + Up * 266.f + N3 * S * 1.3f + T3 * 25.f, Foot + Up * 266.f + N3 * S * 1.3f - T3 * 25.f, N3 * S);
        Paint.Paint = Alu;
        Paint.Box(Foot + Up * 148.f - N3 * 5.f, T3 * 17.f, N3 * 3.f, Up * 28.f);
        Lit.Paint = FColor(70, 70, 62, 0);
        Lit.Flat(Foot + Up * 124.f - N3 * 8.2f - T3 * 14.f, Foot + Up * 124.f - N3 * 8.2f + T3 * 14.f, Foot + Up * 172.f - N3 * 8.2f + T3 * 14.f, Foot + Up * 172.f - N3 * 8.2f - T3 * 14.f, -N3);
        Paint.Paint = Green;
        const FVector Bin = P3(262, 585, Z(262, 0));
        Paint.Spar(Bin, Bin + Up * 84.f, 22.f, 23.f, 14);
        Paint.Paint = Alu;
        Paint.Spar(Bin + Up * 84.f, Bin + Up * 88.f, 24.f, 24.f, 14);
    }
    Emit(TEXT("PhlegBusStopConcrete"), Concrete, ConcreteMat, true, true);
    Emit(TEXT("PhlegBusStopAsphalt"), Asphalt, AsphaltMat, true, true);
    Emit(TEXT("PhlegBusStopMetal"), Paint, PaintMat, true, true);
    if (auto* G2 = Emit(TEXT("PhlegBusStopGlass"), Glass, GlassMat, true, false)) G2->SetCastShadow(false);
    if (auto* L2 = Emit(TEXT("PhlegBusStopLit"), Lit, GlowMat, false, false)) L2->SetCastShadow(false);
    Emit(TEXT("PhlegBusStopRebar"), Rebar, IronMat, false, true);
    // Its lights: the shelter's own downlight under the roof, and the panel's cold glow on the pavement.
    auto Light = [&](const FVector& At, float Candelas, const FLinearColor& Col, float Radius, bool bShadow)
    {
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(RootComponent); L->RegisterComponent();
        L->SetWorldLocation(PhlegWorld(At)); L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(Candelas);
        L->SetLightColor(Col); L->SetAttenuationRadius(Radius); L->SetCastShadows(bShadow); L->SetSourceRadius(12.f);
    };
    Light(P3(0, 700, Z(0, 238)), 30.f, FLinearColor(1.f, .9f, .76f), 750.f, true);
    Light(P3(-150, 650, Z(-150, 120)), 9.f, FLinearColor(.75f, .85f, 1.f), 420.f, false);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BUSSTOP at=%s level=%.0f rifts=%d"), *FVector(C.X, C.Y, Zb).ToString(), Zb, RiftCount);
}
