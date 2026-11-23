#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

// The Feywild's procedural mesh builder (Claude, 2026-11-19; moved out of FeyMap.cpp 2026-11-21 for FeyLife.cpp): vertices,
// normals, UVs, tangents and colours for a UProceduralMeshComponent section, with boxes, faces, lathes, pipes, hedges and
// closed smooth shapes.
struct FMapSurf
{
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan; TArray<FColor> C;
    int32 Mat = 0; bool bSolid = true, bShadow = true, bHidden = false;
    void TriFacing(int32 A, int32 B, int32 Cc, const FVector& Nn) { if (FVector::DotProduct(FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]), Nn) > 0.f) T.Append({A, Cc, B}); else T.Append({A, B, Cc}); }
    int32 Vert(const FVector& P, const FVector& Nn, const FVector2D& Uv, const FColor& Col)
    {
        V.Add(P); N.Add(Nn); UV.Add(Uv); C.Add(Col);
        Tan.Add(FProcMeshTangent(FVector::CrossProduct(FMath::Abs(Nn.Z) > .95f ? FVector::RightVector : FVector::UpVector, Nn).GetSafeNormal(), false));
        return V.Num() - 1;
    }
    void Quad(int32 A, int32 B, int32 Cc, int32 D) { T.Append({A, B, Cc, A, Cc, D}); }
    // A quad facing Nn whatever order its corners come in (the engine's front faces wind clockwise seen from in front).
    void Face(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Nn, const FColor& Col, float UvScale = 200.f)
    {
        const FVector Tu = FVector::CrossProduct(FMath::Abs(Nn.Z) > .9f ? FVector::RightVector : FVector::UpVector, Nn).GetSafeNormal(), Tv = FVector::CrossProduct(Nn, Tu);
        auto Uv = [&](const FVector& P) { return FVector2D(FVector::DotProduct(P, Tu), FVector::DotProduct(P, Tv)) / UvScale; };
        const int32 I0 = Vert(A, Nn, Uv(A), Col), I1 = Vert(B, Nn, Uv(B), Col), I2 = Vert(Cc, Nn, Uv(Cc), Col), I3 = Vert(D, Nn, Uv(D), Col);
        if (FVector::DotProduct(FVector::CrossProduct(B - A, Cc - A), Nn) > 0.f) Quad(I0, I3, I2, I1); else Quad(I0, I1, I2, I3);
    }
    void Box(const FVector& Ce, const FVector& H, float Yaw, const FColor& Col)
    {
        const FQuat Q(FRotator(0.f, Yaw, 0.f));
        auto P = [&](float X, float Y, float Z) { return Ce + Q.RotateVector(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
        const FVector Xa = Q.RotateVector(FVector::ForwardVector), Ya = Q.RotateVector(FVector::RightVector);
        Face(P(-1, -1, 1), P(-1, 1, 1), P(1, 1, 1), P(1, -1, 1), FVector::UpVector, Col);
        Face(P(1, -1, -1), P(1, -1, 1), P(1, 1, 1), P(1, 1, -1), Xa, Col);
        Face(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -Xa, Col);
        Face(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), Ya, Col);
        Face(P(-1, -1, -1), P(-1, -1, 1), P(1, -1, 1), P(1, -1, -1), -Ya, Col);
    }
    // A surface of revolution from a profile given top to bottom (radius, height).
    void Lathe(const FVector& Ce, TArrayView<const FVector2D> Profile, int32 Segs, const FColor& Col, float Squash = 1.f)
    {
        for (int32 I = 0; I + 1 < Profile.Num(); ++I)
            for (int32 J = 0; J < Segs; ++J)
            {
                const float A0 = 2 * PI * J / Segs, A1 = 2 * PI * (J + 1) / Segs, Am = (A0 + A1) * .5f;
                auto P = [&](const FVector2D& Rz, float A) { return Ce + FVector(Rz.X * FMath::Cos(A), Rz.X * FMath::Sin(A) * Squash, Rz.Y); };
                const FVector2D& U = Profile[I]; const FVector2D& L = Profile[I + 1];
                const float Dr = L.X - U.X, Dz = L.Y - U.Y;
                FVector Nn(FMath::Cos(Am) * -Dz, FMath::Sin(Am) * -Dz, Dr); if (!Nn.Normalize()) Nn = FVector(FMath::Cos(Am), FMath::Sin(Am), 0);
                if (U.X < 1.f && L.X < 1.f) continue;
                Face(P(U, A0), P(U, A1), P(L, A1), P(L, A0), Nn, Col, 600.f);
            }
    }
    // A hedge section (2026-11-21, the user: the maze from above): straight sides, a rounded and lumpy crown (clipped, not
    // squared), its ends closed. UVs in 2 m units along its length and round its profile (M_FeyHedge); VC R a tint per hedge.
    void Hedge(const FVector& Ce, const FVector& H, float Yaw, float Seed)
    {
        const FQuat Q(FRotator(0.f, Yaw, 0.f));
        const FVector2D Prof[] = {{-1.f, -1.f}, {-1.f, .9f}, {-.86f, .975f}, {-.48f, 1.01f}, {0.f, 1.02f}, {.48f, 1.01f}, {.86f, .975f}, {1.f, .9f}, {1.f, -1.f}};
        constexpr int32 Np = UE_ARRAY_COUNT(Prof);
        const int32 Nl = FMath::Max(1, FMath::RoundToInt(2.f * H.X / 150.f));
        const uint8 Tint = uint8(FMath::Clamp(128.f + 110.f * FMath::PerlinNoise2D(FVector2D(Ce.X / 2300.f + .3f, Ce.Y / 2300.f + .7f)) + 30.f * FMath::Frac(Seed * 7.13f), 0.f, 255.f));
        TArray<FVector> P, Out; TArray<FVector2D> Uv;
        float Around[Np]; Around[0] = 0; for (int32 J = 1; J < Np; ++J) Around[J] = Around[J - 1] + FVector2D::Distance(Prof[J] * FVector2D(H.Y, H.Z), Prof[J - 1] * FVector2D(H.Y, H.Z));
        for (int32 I = 0; I <= Nl; ++I)
        {
            const float Lx = FMath::Lerp(-H.X, H.X, float(I) / Nl);
            for (int32 J = 0; J < Np; ++J)
            {
                FVector L(Lx, Prof[J].X * H.Y, Prof[J].Y * H.Z);
                const FVector W0 = Ce + Q.RotateVector(L);
                if (Prof[J].Y > 0.f)   // lumps on the crown and the upper sides
                {
                    const float N1 = FMath::PerlinNoise2D(FVector2D(W0.X / 140.f + Seed, W0.Y / 140.f - Seed)), N2 = FMath::PerlinNoise2D(FVector2D(W0.X / 47.f - Seed, W0.Y / 47.f + 3.1f));
                    L.Z += (J >= 2 && J <= Np - 3 ? 16.f : 5.f) * N1 + 5.f * N2; L.Y += Prof[J].X * (7.f * N1 + 3.f * N2);
                }
                P.Add(Ce + Q.RotateVector(L)); Out.Add(Q.RotateVector(FVector(0.f, Prof[J].X, Prof[J].Y * .6f)).GetSafeNormal()); Uv.Add(FVector2D(Lx, Around[J]) / 200.f);
            }
        }
        TArray<FVector> Nn; Nn.Init(FVector::ZeroVector, P.Num()); TArray<int32> Tri;
        for (int32 I = 0; I < Nl; ++I)
            for (int32 J = 0; J + 1 < Np; ++J)
            {
                const int32 A = I * Np + J, B = A + Np;
                const int32 Tris[2][3] = {{A, B, B + 1}, {A, B + 1, A + 1}};
                for (const auto& Tr : Tris)
                {
                    int32 X = Tr[0], Y = Tr[1], Z = Tr[2]; FVector Cr = FVector::CrossProduct(P[Y] - P[X], P[Z] - P[X]);
                    if (FVector::DotProduct(Cr, Out[X] + Out[Y] + Out[Z]) < 0.f) { Swap(Y, Z); Cr = -Cr; }
                    Nn[X] += Cr; Nn[Y] += Cr; Nn[Z] += Cr; Tri.Append({X, Z, Y});
                }
            }
        const int32 Base = V.Num();
        for (int32 I = 0; I < P.Num(); ++I) Vert(P[I], Nn[I].GetSafeNormal(), Uv[I], FColor(Tint, 0, 0, 255));
        for (const int32 I : Tri) T.Add(Base + I);
        for (const float E : {-1.f, 1.f})   // the ends
        {
            const FVector En = Q.RotateVector(FVector(E, 0.f, 0.f)); const int32 Row = E < 0.f ? 0 : Nl;
            const int32 Cb = Vert(Ce + Q.RotateVector(FVector(E * H.X, 0.f, 0.f)), En, FVector2D(0.f, H.Z / 200.f), FColor(Tint, 0, 0, 255));
            TArray<int32> Ring; for (int32 J = 0; J < Np; ++J) Ring.Add(Vert(P[Row * Np + J], En, FVector2D(Prof[J].X * H.Y, Prof[J].Y * H.Z + H.Z) / 200.f, FColor(Tint, 0, 0, 255)));
            for (int32 J = 0; J + 1 < Np; ++J) TriFacing(Cb, Ring[J], Ring[J + 1], En);
            TriFacing(Cb, Ring.Last(), Ring[0], En);
        }
    }
    // A pipe through points (a Catmull-Rom curve, Steps a span), its radius along it from Radii (start to end); lumpy bark.
    void Pipe(TArrayView<const FVector> Pts, float R0, float R1, int32 Sides, int32 Steps, const FColor& Col, float Seed)
    {
        TArray<FVector> Path;
        for (int32 I = 0; I + 1 < Pts.Num(); ++I)
            for (int32 K = 0; K < Steps; ++K)
            {
                const float U = float(K) / Steps; const FVector& P0 = Pts[FMath::Max(I - 1, 0)]; const FVector& P1 = Pts[I]; const FVector& P2 = Pts[I + 1]; const FVector& P3 = Pts[FMath::Min(I + 2, Pts.Num() - 1)];
                Path.Add(.5f * (2.f * P1 + (P2 - P0) * U + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * U * U + (3.f * P1 - P0 - 3.f * P2 + P3) * U * U * U));
            }
        Path.Add(Pts.Last());
        TArray<FVector> P, Out; TArray<FColor> Cs; TArray<int32> Tri; FVector Ref = FVector::UpVector;
        for (int32 I = 0; I < Path.Num(); ++I)
        {
            const FVector Tg = (Path[FMath::Min(I + 1, Path.Num() - 1)] - Path[FMath::Max(I - 1, 0)]).GetSafeNormal();
            if (FMath::Abs(FVector::DotProduct(Tg, Ref)) > .95f) Ref = FVector::ForwardVector;
            const FVector Ax = FVector::CrossProduct(Tg, Ref).GetSafeNormal(), Ay = FVector::CrossProduct(Ax, Tg);
            const float U = float(I) / (Path.Num() - 1), R = FMath::Lerp(R0, R1, U);
            for (int32 J = 0; J < Sides; ++J)
            {
                const float A = 2 * PI * J / Sides; const FVector D = Ax * FMath::Cos(A) + Ay * FMath::Sin(A);
                const float Lump = 1.f + .18f * FMath::PerlinNoise3D(FVector(U * 7.f + Seed, FMath::Cos(A) * 1.3f, FMath::Sin(A) * 1.3f));
                P.Add(Path[I] + D * R * Lump); Out.Add(D); Cs.Add(Col);
                if (I + 1 < Path.Num()) { const int32 A0 = I * Sides + J, A1 = I * Sides + (J + 1) % Sides; Tri.Append({A0, A1, A1 + Sides, A0, A1 + Sides, A0 + Sides}); }
            }
        }
        Closed(P, Out, Tri, Cs);
    }
    // A closed smooth-shaded mesh: Out is roughly outward at each point (it only picks each triangle's facing).
    void Closed(const TArray<FVector>& P, const TArray<FVector>& Out, TArray<int32> Tri, const TArray<FColor>& Col)
    {
        TArray<FVector> Nn; Nn.Init(FVector::ZeroVector, P.Num());
        for (int32 I = 0; I + 2 < Tri.Num(); I += 3)
        {
            int32& A = Tri[I]; int32& B = Tri[I + 1]; int32& Cc = Tri[I + 2];
            FVector X = FVector::CrossProduct(P[B] - P[A], P[Cc] - P[A]);
            if (FVector::DotProduct(X, Out[A] + Out[B] + Out[Cc]) < 0.f) { Swap(B, Cc); X = -X; }
            Nn[A] += X; Nn[B] += X; Nn[Cc] += X;
        }
        const int32 Base = V.Num();
        for (int32 I = 0; I < P.Num(); ++I) { FVector N1 = Nn[I].GetSafeNormal(); if (N1.IsNearlyZero()) N1 = Out[I].GetSafeNormal(); Vert(P[I], N1, FVector2D(P[I].X + P[I].Z, P[I].Y - P[I].Z) / 400.f, Col[I]); }
        for (int32 I = 0; I + 2 < Tri.Num(); I += 3) T.Append({Base + Tri[I], Base + Tri[I + 2], Base + Tri[I + 1]});   // (front faces wind clockwise)
    }
};

