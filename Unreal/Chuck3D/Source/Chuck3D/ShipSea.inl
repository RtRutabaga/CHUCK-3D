// ShipSea.inl (Claude, 2026-11-02; docs/SHIP.md, milestone 2): the sea's mesh, in the ship's frame. Included in
// ShipSetting.cpp's anonymous namespace (it uses the hull's lines). M_ShipSea (Tools/create_ship_sea.py) moves it.
//
// Rings round the hull's waterline: the first on the planking itself (4 cm inside its outer face), then out by growing
// steps, about a metre apart to 60 m (where the waves fade), 2 m apart to 150 m (the storm's swell) and then ever wider to 400 km, past any horizon. Every ring
// is the waterline pushed straight out along its normals, the transom's corners filled with fans, so the rings stay
// round and never cross. Inside the first ring a curtain runs down inside the planking to a metre under the water. Per
// vertex: UV1 = (distance out from the waterline, the hull's flare dO/dZ there; the curtain -100), UV2 = the outward
// normal.

struct FSeaMesh
{
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV0, UV1, UV2;
};

void BuildSeaMesh(FSeaMesh& M)
{
    constexpr float Inset = 4;
    struct FEdge { FVector2D P, Nn; float Slope; FVector2D Curtain; float CurtainZ; };
    TArray<FEdge> Loop;
    auto SideAt = [](float X, float Z) { return FMath::Max(0.f, OuterAt(X, Z) - Inset); };
    // Port side, stern to stem, finer toward the round bow.
    TArray<float> Xs;
    for (float X = SternX; X < 1400; X += 25) Xs.Add(X);
    for (float X = 1400; X < BowX - 30; X += 10) Xs.Add(X);
    for (float X = BowX - 30; X < BowX; X += 3) Xs.Add(X);
    Xs.Add(BowX);
    auto Curtain = [](float X) { return FMath::Max(WaterZ - 100, BottomZ(X) + 5); };
    TArray<FEdge> Port;
    for (float X : Xs)
    {
        const float Cz = Curtain(X);
        Port.Add({FVector2D(X, SideAt(X, WaterZ)), FVector2D::ZeroVector, (OuterAt(X, WaterZ + 10) - OuterAt(X, WaterZ - 10)) / 20, FVector2D(X, SideAt(X, Cz)), Cz});
    }
    for (int32 I = 0; I < Port.Num(); ++I)
    {
        const FVector2D A = Port[FMath::Max(0, I - 1)].P, B = Port[FMath::Min(Port.Num() - 1, I + 1)].P;
        const FVector2D Tn = (B - A).GetSafeNormal();
        Port[I].Nn = FVector2D(-Tn.Y, Tn.X);   // outward on the port side (Y > 0) as X grows
        if (Port[I].Nn.Y < 0 && I < Port.Num() - 1) Port[I].Nn *= -1;
    }
    Port.Last().Nn = FVector2D(1, 0);   // the stem
    // The transom's corner: from the transom's face (-X) round to the side's normal.
    const float StH = SideAt(SternX + 1, WaterZ), StX = SternX - 12 + Inset;
    auto Corner = [&](float Side, bool bToSide)
    {
        const FVector2D SideN(Port[0].Nn.X, Side * Port[0].Nn.Y);
        const float Cz = Curtain(SternX);
        for (int32 K = 1; K < 8; ++K)
        {
            const float U = bToSide ? K / 8.f : 1 - K / 8.f;
            const FVector2D Nn = (FVector2D(-1, 0) * (1 - U) + SideN * U).GetSafeNormal();
            Loop.Add({FVector2D(StX, Side * StH), Nn, Port[0].Slope * U, FVector2D(StX, Side * SideAt(SternX + 1, Cz)), Cz});
        }
    };
    // The loop: port side stern to stem, starboard back, round the starboard corner, across the transom, round the port corner.
    for (const FEdge& E : Port) Loop.Add(E);
    for (int32 I = Port.Num() - 2; I >= 0; --I)
    {
        FEdge E = Port[I]; E.P.Y *= -1; E.Nn.Y *= -1; E.Curtain.Y *= -1; Loop.Add(E);
    }
    Corner(-1, false);
    {
        const float Cz = Curtain(SternX);
        const float CH = SideAt(SternX + 1, Cz);
        for (float Y = -StH; Y <= StH + .1f; Y += StH / 12)
            Loop.Add({FVector2D(StX, Y), FVector2D(-1, 0), 0, FVector2D(StX, FMath::Clamp(Y, -CH, CH)), Cz});
    }
    Corner(1, true);

    // The rings' distances out (cm). The first is the planking; the curtain is ring -1.
    TArray<float> Ds = {0, 12, 28, 48, 72, 100, 135, 175, 220, 270, 330, 400};
    for (float D = 480; D <= 6000; D += 100) Ds.Add(D);
    // (2 m apart on to 150 m, 2026-11-03: the storm's swell, M_ShipSea, keeps its shape to there.)
    for (float D = 6200; D <= 15000; D += 200) Ds.Add(D);
    for (float D = 19000; D < 4.2e7f; D *= 1.32f) Ds.Add(D);
    const int32 Np = Loop.Num();
    // The curtain.
    for (const FEdge& E : Loop)
    {
        M.V.Add(FVector(E.Curtain.X, E.Curtain.Y, E.CurtainZ)); M.N.Add(FVector::UpVector);
        M.UV0.Add(E.Curtain / 100); M.UV1.Add(FVector2D(-100, 0)); M.UV2.Add(E.Nn);
    }
    for (float D : Ds)
        for (const FEdge& E : Loop)
        {
            const FVector2D Pt = E.P + E.Nn * D;
            M.V.Add(FVector(Pt.X, Pt.Y, WaterZ)); M.N.Add(FVector::UpVector);
            M.UV0.Add(Pt / 100); M.UV1.Add(FVector2D(D, E.Slope)); M.UV2.Add(E.Nn);
        }
    const int32 Rings = Ds.Num() + 1;
    for (int32 R = 0; R + 1 < Rings; ++R)
        for (int32 I = 0; I < Np; ++I)
        {
            const int32 J = (I + 1) % Np;
            const int32 A = R * Np + I, B = R * Np + J, C = (R + 1) * Np + J, D = (R + 1) * Np + I;
            // Faces up (the engine's front faces wind the other way round from the cross product).
            for (const FIntVector& Tri : {FIntVector(A, B, C), FIntVector(A, C, D)})
            {
                const FVector Cr = FVector::CrossProduct(M.V[Tri.Y] - M.V[Tri.X], M.V[Tri.Z] - M.V[Tri.X]);
                // (The curtain's quads stand on edge: they face out, the same rule by their outward normal.)
                const FVector Want = R == 0 ? FVector(M.UV2[A].X, M.UV2[A].Y, 0) : FVector::UpVector;
                if (FVector::DotProduct(Cr, Want) >= 0) M.T.Append({Tri.X, Tri.Z, Tri.Y}); else M.T.Append({Tri.X, Tri.Y, Tri.Z});
            }
        }
    // Two points far over and under it that no triangle uses: its bounds must hold the rocking far rings (the tilt lifts
    // the horizon by kilometres).
    for (float Z : {-1.3e6f, 1.3e6f}) { M.V.Add(FVector(0, 0, Z)); M.N.Add(FVector::UpVector); M.UV0.Add(FVector2D::ZeroVector); M.UV1.Add(FVector2D(1e9f, 0)); M.UV2.Add(FVector2D::ZeroVector); }
}
