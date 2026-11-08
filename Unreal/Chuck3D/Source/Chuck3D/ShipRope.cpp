#include "ShipRope.h"

namespace ShipRope
{
namespace
{
    constexpr int32 ArcSteps = 8;      // every bend the same number of points (the mesh keeps its size frame to frame)
    constexpr int32 DroopSteps = 32;

    // A bend's arc: its tangent points' distance from the corner, its radius and centre (false: too nearly straight).
    bool Fillet(const FVector& Prev, const FVector& P, const FVector& Next, float Bend, float Hold, float MaxA, float MaxB, FVector& A, FVector& B, float& T, float& R, FVector& C)
    {
        A = Prev - P; B = Next - P;
        const float LA = static_cast<float>(A.Size()), LB = static_cast<float>(B.Size());
        if (Bend <= 0.f || LA < .01f || LB < .01f) return false;
        A /= LA; B /= LB;
        const float Theta = FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(A, B)), -1.f, 1.f));   // the inside angle
        if (Theta > PI - .01f) return false;
        const float Tan = FMath::Tan(Theta * .5f), Sin = FMath::Sin(Theta * .5f);
        // (The arc passes R (1 / sin - 1) from the corner.)
        if (Hold > 0.f && Sin < .999f) Bend = FMath::Min(Bend, Hold * Sin / (1.f - Sin));
        T = FMath::Min3(Bend / Tan, LA * MaxA, LB * MaxB);
        R = T * Tan;
        C = P + (A + B).GetSafeNormal() * (R / FMath::Sin(Theta * .5f));
        return true;
    }
}

void Bent(const TArray<FKnot>& Knots, TArray<FVector>& Out)
{
    const int32 N = Knots.Num();
    if (N == 0) return;
    Out.Add(Knots[0].P);
    for (int32 I = 1; I + 1 < N; ++I)
    {
        const FKnot& K = Knots[I];
        if (K.Bend <= 0.f) { Out.Add(K.P); continue; }
        // (Each side's length shared with the neighbouring bend, all of it toward an end.)
        const float MaxA = I - 1 == 0 || Knots[I - 1].Bend <= 0.f ? .9f : .5f, MaxB = I + 1 == N - 1 || Knots[I + 1].Bend <= 0.f ? .9f : .5f;
        FVector A, B, C; float T, R;
        if (Fillet(Knots[I - 1].P, K.P, Knots[I + 1].P, K.Bend, K.Hold, MaxA, MaxB, A, B, T, R, C))
        {
            const FVector U1 = (K.P + A * T - C) / R, U2 = (K.P + B * T - C) / R;
            const FQuat Turn = FQuat::FindBetweenNormals(U1.GetSafeNormal(), U2.GetSafeNormal());
            for (int32 S = 0; S <= ArcSteps; ++S) Out.Add(C + FQuat::Slerp(FQuat::Identity, Turn, S / float(ArcSteps)).RotateVector(U1) * R);
        }
        else
        {
            // Straight through it (or nothing to bend round): the same count of points, along the line.
            const FVector From = FMath::Lerp(K.P, Knots[I - 1].P, .05f), To = FMath::Lerp(K.P, Knots[I + 1].P, .05f);
            for (int32 S = 0; S <= ArcSteps; ++S) Out.Add(FMath::Lerp(From, To, S / float(ArcSteps)));
        }
    }
    if (N > 1) Out.Add(Knots.Last().P);
}

FVector ArcCentre(const FVector& Prev, const FVector& Corner, const FVector& Next, float Bend)
{
    FVector A, B, C; float T, R;
    return Fillet(Prev, Corner, Next, Bend, 0.f, .9f, .9f, A, B, T, R, C) ? C : Corner;
}

void Sheave(const FVector& In, const FVector& C, const FVector& Axle, float R, const FVector& Out, TArray<FVector>& Path)
{
    // In the sheave's plane: each end's tangent point on the far side from the other end, and the arc between them round
    // the side away from both (where the rope bears on the sheave).
    const FVector N = Axle.GetSafeNormal();
    FVector E1 = FVector::VectorPlaneProject(In - C, N).GetSafeNormal();
    if (E1.IsNearlyZero()) E1 = FVector::VectorPlaneProject(FVector::UpVector, N).GetSafeNormal();
    const FVector E2 = FVector::CrossProduct(N, E1);
    const auto Angle = [&](const FVector& P, float& D) { const FVector Q = P - C; D = static_cast<float>(FVector2D(FVector::DotProduct(Q, E1), FVector::DotProduct(Q, E2)).Size()); return FMath::Atan2(static_cast<float>(FVector::DotProduct(Q, E2)), static_cast<float>(FVector::DotProduct(Q, E1))); };
    float DIn, DOut;
    const float AIn = Angle(In, DIn), AOut = Angle(Out, DOut);
    const float SpreadIn = FMath::Acos(FMath::Min(R / FMath::Max(DIn, R), 1.f)), SpreadOut = FMath::Acos(FMath::Min(R / FMath::Max(DOut, R), 1.f));
    // (Of each end's two tangents, the one turned away from the other end.)
    const float AwayIn = FMath::FindDeltaAngleRadians(AOut, AIn) >= 0.f ? 1.f : -1.f;
    const float TIn = AIn + AwayIn * SpreadIn, TOut = AOut - AwayIn * SpreadOut;
    // Round the far side: from TIn on the way away from In's side round to TOut.
    float Sweep = FMath::FindDeltaAngleRadians(TIn, TOut);
    if (Sweep * AwayIn < 0.f) Sweep += AwayIn * 2.f * PI;
    constexpr int32 Steps = 14;
    for (int32 S = 0; S <= Steps; ++S)
    {
        const float A = TIn + Sweep * S / float(Steps);
        Path.Add(C + (E1 * FMath::Cos(A) + E2 * FMath::Sin(A)) * R);
    }
}

void Droop(const FVector& From, const FVector& Leaving, const FVector& Way, float Z, TArray<FVector>& Out, FVector& Touch)
{
    // A stiff rope let go: it goes on a hand's breadth the way it left the grip, then falls and lies down along the deck,
    // touching down further off the higher it hangs (a cubic: its tangents the way it left and the deck's way).
    const float H = FMath::Max(static_cast<float>(From.Z) - Z, 0.f);
    const FVector W = FVector(Way.X, Way.Y, 0.f).GetSafeNormal();
    // (Its controls kept in order along the deck's way and down, so it never folds back on itself.)
    const float Reach = .5f * H + 10.f, Lead = FMath::Min(6.f + .15f * H, 20.f);
    Touch = FVector(From.X, From.Y, Z) + W * Reach;
    const FVector P1 = From + Leaving.GetSafeNormal() * Lead;
    const FVector P2 = Touch - W * FMath::Min(.3f * H + 2.f, Reach - Lead - 4.f);
    for (int32 S = 1; S <= DroopSteps; ++S)
    {
        const float T = S / float(DroopSteps), U = 1.f - T;
        FVector P = From * (U * U * U) + P1 * (3.f * U * U * T) + P2 * (3.f * U * T * T) + Touch * (T * T * T);
        P.Z = FMath::Max(P.Z, static_cast<double>(Z));
        Out.Add(P);
    }
}

void Coil(const FVector& Start, const FVector& Way, const FVector& Side, float Outer, float Inner, TArray<FVector>& Out)
{
    const FVector W = FVector(Way.X, Way.Y, 0.f).GetSafeNormal(), S = FVector(Side.X, Side.Y, 0.f).GetSafeNormal();
    const FVector C = Start + S * Outer;
    constexpr float Pitch = 3.2f;   // a turn beside the last (the rope's thickness, a little loose)
    const float Turns = FMath::Max((Outer - Inner) / Pitch, 1.f);
    const int32 Steps = FMath::CeilToInt(Turns * 30.f);
    for (int32 K = 1; K <= Steps; ++K)
    {
        const float F = K / float(Steps), A = F * Turns * 2.f * PI, R = FMath::Lerp(Outer, Inner, F);
        Out.Add(C + (-S * FMath::Cos(A) + W * FMath::Sin(A)) * R);
    }
}

float Length(const TArray<FVector>& Path)
{
    float L = 0.f;
    for (int32 I = 1; I < Path.Num(); ++I) L += static_cast<float>(FVector::Dist(Path[I - 1], Path[I]));
    return L;
}

void FMesh::Into(UProceduralMeshComponent* Mesh, int32 Section, bool bUpdate) const
{
    if (!Mesh) return;
    const FProcMeshSection* Have = Mesh->GetProcMeshSection(Section);
    if (bUpdate && Have && Have->ProcVertexBuffer.Num() == V.Num() && Have->ProcIndexBuffer.Num() == T.Num())
        Mesh->UpdateMeshSection(Section, V, N, UV, TArray<FColor>(), Tan);
    else Mesh->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), Tan, false);
}

namespace
{
    // Front faces (as the engine draws them) wind so that (B - A) x (C - A) points into the surface.
    void Tri(FMesh& M, int32 A, int32 B, int32 C, const FVector& Outward)
    {
        const FVector Face = FVector::CrossProduct(M.V[B] - M.V[A], M.V[C] - M.V[A]);
        if (FVector::DotProduct(Face, Outward) > 0.f) M.T.Append({ A, C, B }); else M.T.Append({ A, B, C });
    }
}

void Tube(FMesh& M, const TArray<FVector>& Path, float R, float UOffset, int32 Sides)
{
    const int32 N = Path.Num();
    if (N < 2) return;
    // Each point's way along the rope (its two segments' average), carried over a run of points standing on one spot.
    TArray<FVector> D; D.SetNum(N);
    for (int32 I = 0; I < N; ++I)
    {
        const FVector In = I > 0 ? (Path[I] - Path[I - 1]).GetSafeNormal() : FVector::ZeroVector;
        const FVector Out = I + 1 < N ? (Path[I + 1] - Path[I]).GetSafeNormal() : FVector::ZeroVector;
        D[I] = (In + Out).GetSafeNormal();
        if (D[I].IsNearlyZero()) D[I] = I > 0 ? D[I - 1] : FVector::ForwardVector;
    }
    for (int32 I = N - 2; I >= 0; --I) if ((Path[I + 1] - Path[I]).IsNearlyZero(1e-3)) D[I] = D[I + 1];
    // Its frame carried along without twisting (parallel transport), so the strands lie true round every bend.
    FVector X = FVector::CrossProduct(D[0], FVector::UpVector);
    if (X.SizeSquared() < 1e-4) X = FVector::CrossProduct(D[0], FVector::ForwardVector);
    X.Normalize();
    const int32 Base = M.V.Num(), Ring = Sides + 1;
    float Run = 0.f;
    for (int32 I = 0; I < N; ++I)
    {
        if (I > 0)
        {
            Run += static_cast<float>(FVector::Dist(Path[I], Path[I - 1]));
            X = FQuat::FindBetweenNormals(D[I - 1], D[I]).RotateVector(X);
            X = (X - D[I] * FVector::DotProduct(X, D[I])).GetSafeNormal();
        }
        const FVector Y = FVector::CrossProduct(X, D[I]);
        for (int32 S = 0; S <= Sides; ++S)   // (the seam's column twice, for its UVs)
        {
            const float A = 2.f * PI * S / Sides, Ca = FMath::Cos(A), Sa = FMath::Sin(A);
            const FVector O = X * Ca + Y * Sa, Round = Y * Ca - X * Sa;   // out, and the way V runs round
            M.V.Add(Path[I] + O * R); M.N.Add(O);
            M.UV.Add(FVector2D((Run + UOffset) / LayRepeat, S / float(Sides) * AroundRepeat));
            // The normal map's tangent along the rope (U); its bitangent must run with V round it.
            M.Tan.Add(FProcMeshTangent(D[I], FVector::DotProduct(FVector::CrossProduct(O, D[I]), Round) < 0.f));
        }
    }
    for (int32 I = 0; I + 1 < N; ++I)
        for (int32 S = 0; S < Sides; ++S)
        {
            const int32 A = Base + I * Ring + S, B = A + 1, C = A + Ring, E = C + 1;
            M.T.Append({ A, B, C, B, E, C });
        }
    // The ends cut off square (whipped).
    for (const int32 End : { 0, N - 1 })
    {
        const FVector Out = End == 0 ? -D[0] : D[N - 1];
        const int32 Hub = M.V.Num();
        M.V.Add(Path[End]); M.N.Add(Out); M.UV.Add(FVector2D(0.f, 0.f)); M.Tan.Add(FProcMeshTangent(X, false));
        const int32 First = Base + End * Ring;
        for (int32 S = 0; S < Sides; ++S) Tri(M, Hub, First + S, First + S + 1, Out);
    }
}

void Shell(FMesh& M, const FVector& C, const FVector& Long, const FVector& Cheek, const FVector& Radii)
{
    const FVector L = Long.GetSafeNormal(), Cn = FVector::VectorPlaneProject(Cheek, L).GetSafeNormal(), W = FVector::CrossProduct(Cn, L);
    constexpr int32 Lat = 10, Lon = 14;
    const int32 Base = M.V.Num();
    for (int32 I = 0; I <= Lat; ++I)
        for (int32 J = 0; J <= Lon; ++J)
        {
            const float A = PI * I / Lat, B = 2.f * PI * J / Lon;
            // (Its cheeks flattened: a block's shell is an oval slab, not an egg.)
            const float Ca = FMath::Cos(A), Sa = FMath::Sin(A), Cb = FMath::Cos(B), Sb = FMath::Sin(B);
            const float Flat = FMath::Sign(Sb) * FMath::Pow(FMath::Abs(Sb), .55f);
            const FVector Local(Radii.X * Ca, Radii.Y * Sa * Cb, Radii.Z * Sa * Flat);
            const FVector Grad(Local.X / (Radii.X * Radii.X), Local.Y / (Radii.Y * Radii.Y), Local.Z / (Radii.Z * Radii.Z));
            M.V.Add(C + L * Local.X + W * Local.Y + Cn * Local.Z);
            M.N.Add((L * Grad.X + W * Grad.Y + Cn * Grad.Z).GetSafeNormal());
            M.UV.Add(FVector2D(J / float(Lon), I / float(Lat)));
            M.Tan.Add(FProcMeshTangent(W, false));
        }
    for (int32 I = 0; I < Lat; ++I)
        for (int32 J = 0; J < Lon; ++J)
        {
            const int32 A = Base + I * (Lon + 1) + J, B = A + 1, Cc = A + Lon + 1, E = Cc + 1;
            const FVector Out = (M.V[A] + M.V[E]) * .5f - C;
            Tri(M, A, B, Cc, Out); Tri(M, B, E, Cc, Out);
        }
}
}
