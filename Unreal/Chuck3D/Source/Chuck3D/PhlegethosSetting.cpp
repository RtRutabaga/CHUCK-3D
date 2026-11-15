#include "PhlegethosSetting.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ClayJar.h"
#include "DockFire.h"
#include "FallCutscene.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

// Phlegethos (Claude, 2026-11-14; PhlegethosSetting.h, docs/regions/PHLEGETHOS.md). Built in the stage frame on
// procedural meshes, one per material: the CC0 basalts and flagstones (M_PhlegBasalt, M_PhlegPath), the molten lake and
// its falls (M_PhlegLava), the glowing fissures and eyes (M_PhlegGlow), iron (M_RustIron). Tools/create_phlegethos_assets.py.
using namespace Phleg;

namespace
{
    // ------------------------------------------------------------------ noise
    float Hash(float X, float Y) { return FMath::Frac(FMath::Sin(X * 127.1f + Y * 311.7f) * 43758.5453f); }
    float Noise(float X, float Y, float Scale, float Seed = 0) { return FMath::PerlinNoise2D(FVector2D(X / Scale + Seed * 3.71f + .31f, Y / Scale - Seed * 1.93f + .77f)); }
    float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3 - 2 * T); }

    enum ESound { RumbleLoop, GeyserRoar, GeyserHiss, Crack0, Crack1, Fall0, Fall1, Thunder0, Thunder1, Thunder2, Slam0, Slam1, SoundCount };
    const TCHAR* SoundNames[SoundCount] = {TEXT("SFX_ForgeLoop_00"), TEXT("SFX_TempleRunFireRoar_00"), TEXT("SFX_TempleRunFireHiss_00"), TEXT("SFX_TempleSlabCrack_00"),
        TEXT("SFX_TempleSlabCrack_01"), TEXT("SFX_TempleSlabFall_00"), TEXT("SFX_TempleSlabFall_01"), TEXT("SFX_StormThunderFar_00"), TEXT("SFX_StormThunderFar_03"),
        TEXT("SFX_StormThunderNear_01"), TEXT("SFX_StormSlam_01"), TEXT("SFX_StormSlam_03")};
}

// ------------------------------------------------------------------ mesh building
struct FPhlegSurf
{
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan; TArray<FColor> C;
    FColor Paint = FColor(0, 0, 0, 255);
    void Vert(const FVector& P, const FVector& Nn, const FVector2D& Uv)
    {
        V.Add(P); N.Add(Nn); UV.Add(Uv); C.Add(Paint);
        const FVector Ax = FMath::Abs(Nn.Z) > .95f ? FVector::ForwardVector : FVector::UpVector;
        Tan.Add(FProcMeshTangent(FVector::CrossProduct(Ax, Nn).GetSafeNormal(), false));
    }
    void Vert(const FVector& P, const FVector& Nn) { Vert(P, Nn, FMath::Abs(Nn.Z) > .7f ? FVector2D(P.X, P.Y) / 400 : FVector2D(P.X + P.Y, -P.Z) / 400); }
    void Face(int32 A, int32 B, int32 Cc, const FVector& Want)
    {
        const FVector F = FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]);
        if (FVector::DotProduct(F, Want) >= 0) T.Append({A, Cc, B}); else T.Append({A, B, Cc});
    }
    void Quad(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& ND)
    {
        const int32 I = V.Num(); Vert(A, NA); Vert(B, NB); Vert(Cc, NC); Vert(D, ND);
        const FVector W = NA + NB + NC + ND;
        Face(I, I + 1, I + 2, W); Face(I, I + 2, I + 3, W);
    }
    void Flat(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Out) { Quad(A, B, Cc, D, Out, Out, Out, Out); }
    void Tri(const FVector& A, const FVector& B, const FVector& Cc, const FVector& Out) { const int32 I = V.Num(); Vert(A, Out); Vert(B, Out); Vert(Cc, Out); Face(I, I + 1, I + 2, Out); }
    void Box(const FVector& Ce, const FVector& X, const FVector& Y, const FVector& Z)
    {
        auto P = [&](float A, float B, float Cc) { return Ce + X * A + Y * B + Z * Cc; };
        const FVector Xn = X.GetSafeNormal(), Yn = Y.GetSafeNormal(), Zn = Z.GetSafeNormal();
        Flat(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Xn); Flat(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -Xn);
        Flat(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), Yn); Flat(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Yn);
        Flat(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Zn); Flat(P(-1, -1, -1), P(1, -1, -1), P(1, 1, -1), P(-1, 1, -1), -Zn);
    }
    void B(float X0, float X1, float Y0, float Y1, float Z0, float Z1)
    {
        if (X1 - X0 < .01f || Y1 - Y0 < .01f || Z1 - Z0 < .01f) return;
        Box(FVector((X0 + X1) * .5f, (Y0 + Y1) * .5f, (Z0 + Z1) * .5f), FVector((X1 - X0) * .5f, 0, 0), FVector(0, (Y1 - Y0) * .5f, 0), FVector(0, 0, (Z1 - Z0) * .5f));
    }
    void TurnedBox(const FVector& Ce, const FVector& H, const FRotator& R)
    {
        const FQuat Q(R);
        Box(Ce, Q.RotateVector(FVector(H.X, 0, 0)), Q.RotateVector(FVector(0, H.Y, 0)), Q.RotateVector(FVector(0, 0, H.Z)));
    }
    void Spar(const FVector& A, const FVector& Bp, float RA, float RB, int32 Sides = 8)
    {
        const FVector D = (Bp - A).GetSafeNormal(); FVector X = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();
        if (X.IsNearlyZero()) X = FVector::RightVector;
        const FVector Y = FVector::CrossProduct(D, X);
        auto Dir = [&](float K) { const float An = K * 2 * PI / Sides; return X * FMath::Cos(An) + Y * FMath::Sin(An); };
        for (int32 I = 0; I < Sides; ++I)
        {
            const FVector N0 = Dir(I), N1 = Dir(I + 1);
            Quad(A + N0 * RA, A + N1 * RA, Bp + N1 * RB, Bp + N0 * RB, N0, N1, N1, N0);
            if (RA > .01f) Tri(A, A + N0 * RA, A + N1 * RA, -D);
            if (RB > .01f) Tri(Bp, Bp + N0 * RB, Bp + N1 * RB, D);
        }
    }
    // A rock: a sphere of Radii roughened by noise, its bottom flattened under Floor.
    void Blob(const FVector& Ce, const FVector& Radii, float Seed, float Floor = -1e9f, int32 Rings = 7, int32 Segs = 12)
    {
        auto Pt = [&](int32 I, int32 J)
        {
            const float Th = PI * I / Rings, Ph = 2 * PI * J / Segs;
            FVector D(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
            // (Jagged, as the goal art's broken basalt: a ridged term over the swell, flat facets between.)
            const float K = 1 + .3f * FMath::PerlinNoise3D(D * 1.7f + FVector(Seed, Seed * .7f, -Seed)) + .22f * (.5f - FMath::Abs(FMath::PerlinNoise3D(D * 3.3f + FVector(-Seed, Seed, Seed * .3f))));
            FVector P = Ce + D * Radii * K;
            P.Z = FMath::Max(P.Z, Floor);
            return P;
        };
        for (int32 I = 0; I < Rings; ++I)
            for (int32 J = 0; J < Segs; ++J)
            {
                const FVector A = Pt(I, J), Bp = Pt(I + 1, J), Cc = Pt(I + 1, J + 1), D = Pt(I, J + 1);
                FVector Nn = FVector::CrossProduct(Cc - A, D - Bp).GetSafeNormal();
                const FVector Out = ((A + Cc) * .5f - Ce).GetSafeNormal();
                if (FVector::DotProduct(Nn, Out) < 0) Nn = -Nn;
                if (Nn.IsNearlyZero()) Nn = Out;
                Quad(A, Bp, Cc, D, Nn, Nn, Nn, Nn);
            }
    }
    // A box seen as broken basalt (2026-11-15, the goal art's cliffs): each side (-X, +X, -Y, +Y) cut into facets pushed IN
    // by up to Depth cm, never out, so a plain collision box stands where it did and wall runs and jumps meet the same
    // plane; the pushes fade to nothing at the side's edges so the sides still meet, and the top stays flat to walk on.
    void Crag(float X0, float X1, float Y0, float Y1, float Z0, float Z1, const float Depth[4], float Seed)
    {
        Flat(FVector(X0, Y0, Z1), FVector(X1, Y0, Z1), FVector(X1, Y1, Z1), FVector(X0, Y1, Z1), FVector::UpVector);
        for (int32 F = 0; F < 4; ++F)
        {
            FVector O, U, Nn; float Lu;
            switch (F)
            {
            case 0: O = FVector(X0, Y1, 0); U = FVector(0, -1, 0); Lu = Y1 - Y0; Nn = FVector(-1, 0, 0); break;
            case 1: O = FVector(X1, Y0, 0); U = FVector(0, 1, 0); Lu = Y1 - Y0; Nn = FVector(1, 0, 0); break;
            case 2: O = FVector(X0, Y0, 0); U = FVector(1, 0, 0); Lu = X1 - X0; Nn = FVector(0, -1, 0); break;
            default: O = FVector(X1, Y1, 0); U = FVector(-1, 0, 0); Lu = X1 - X0; Nn = FVector(0, 1, 0); break;
            }
            const float Dp = Depth[F], S = Seed + F * 13.f;
            if (Dp <= 0.f) { Flat(O + FVector(0, 0, Z0), O + U * Lu + FVector(0, 0, Z0), O + U * Lu + FVector(0, 0, Z1), O + FVector(0, 0, Z1), Nn); continue; }
            const float Cell = FMath::Clamp(Dp * 1.4f, 40.f, 150.f);
            const int32 Nu = FMath::Clamp(FMath::CeilToInt(Lu / Cell), 1, 300), Nz = FMath::Clamp(FMath::CeilToInt((Z1 - Z0) / (Cell * .85f)), 1, 120);
            auto Pt = [&](int32 I, int32 K)
            {
                const float Uu = Lu * I / Nu, Z = FMath::Lerp(Z0, Z1, float(K) / Nz);
                const float A = Noise(Uu, Z, 240, S), Bb = .5f - FMath::Abs(Noise(Uu * 1.3f, Z * .7f, 110, S + 3));
                const float Strata = .5f + .5f * FMath::Sin(Z / 41.f + Noise(Uu, Z, 300, S + 5) * 3.f);
                const float K1 = FMath::Clamp(.4f + .6f * A + .7f * Bb + .25f * Strata, 0.f, 1.f);
                const float Edge = FMath::Min3(Uu, Lu - Uu, Z1 - Z);
                const float Push = Dp * K1 * FMath::Clamp(Edge / (Dp * 1.5f + 30.f), 0.f, 1.f);
                // (a little jitter along the face too, so the facets are not a grid)
                const float J = (I > 0 && I < Nu && K > 0 && K < Nz) ? Cell * .3f : 0.f;
                return O + U * (Uu + J * Noise(Uu, Z, 50, S + 7)) + FVector(0, 0, Z + J * Noise(Z, Uu, 50, S + 9)) - Nn * Push;
            };
            for (int32 K = 0; K < Nz; ++K)
                for (int32 I = 0; I < Nu; ++I)
                {
                    const FVector A = Pt(I, K), Bp = Pt(I + 1, K), Cc = Pt(I + 1, K + 1), D = Pt(I, K + 1);
                    // (flat-shaded facets: each triangle its own normal, as broken rock)
                    FVector N1 = FVector::CrossProduct(Bp - A, Cc - A).GetSafeNormal(); if (FVector::DotProduct(N1, Nn) < 0) N1 = -N1;
                    FVector N2 = FVector::CrossProduct(Cc - A, D - A).GetSafeNormal(); if (FVector::DotProduct(N2, Nn) < 0) N2 = -N2;
                    Tri(A, Bp, Cc, N1.IsNearlyZero() ? Nn : N1); Tri(A, Cc, D, N2.IsNearlyZero() ? Nn : N2);
                }
        }
    }
    // A solid stood on its outline (closed, star-shaped round its centroid): the top as rings to the rim, its height
    // Top(x, y); the sides down to Bottom with a few rows, pushed out by Jag (cm) of noise so they read as broken rock.
    // Top's rings get Rough cm of noise (0 for flat stepping surfaces).
    void Prism(const TArray<FVector2D>& Out, TFunctionRef<float(float, float)> Top, float Bottom, float Jag, float Rough, float Seed, int32 Rings = 3)
    {
        const int32 M = Out.Num();
        FVector2D Ce(0, 0); for (const FVector2D& P : Out) Ce += P; Ce /= M;
        auto Ring = [&](int32 K, int32 J)
        {
            const float A = float(K) / Rings;
            const FVector2D P = FMath::Lerp(Ce, Out[J % M], A);
            const float Edge = K == Rings ? 0.f : 1.f;
            return FVector(P.X, P.Y, Top(P.X, P.Y) + Edge * Rough * Noise(P.X, P.Y, 90, Seed));
        };
        for (int32 K = 0; K < Rings; ++K)
            for (int32 J = 0; J < M; ++J)
            {
                const FVector A = Ring(K, J), Bp = Ring(K + 1, J), Cc = Ring(K + 1, J + 1), D = Ring(K, J + 1);
                FVector Nn = FVector::CrossProduct(Bp - A, D - A) + FVector::CrossProduct(Cc - Bp, D - Bp);
                if (Nn.Z < 0) Nn = -Nn;
                Nn = Nn.GetSafeNormal(); if (Nn.IsNearlyZero()) Nn = FVector::UpVector;
                if (K == 0) Tri(A, Bp, Cc, Nn); else Quad(A, Bp, Cc, D, Nn, Nn, Nn, Nn);
            }
        // The sides: rows down from the rim, the middle ones pushed out.
        constexpr int32 Rows = 4;
        auto Side = [&](int32 J, int32 R)
        {
            const FVector2D P = Out[J % M];
            const float Zt = Top(P.X, P.Y), U = float(R) / Rows;
            const float Z = FMath::Lerp(Zt, Bottom, U);
            const FVector2D Dir = (P - Ce).GetSafeNormal();
            const float Push = (R == 0 ? 0.f : Jag * (.5f + Noise(P.X + Z, P.Y - Z, 70, Seed + 2)));
            const FVector2D Q = P + Dir * Push;
            return FVector(Q.X, Q.Y, Z);
        };
        for (int32 J = 0; J < M; ++J)
        {
            const FVector2D Mid = (Out[J] + Out[(J + 1) % M]) * .5f;
            FVector2D Nd(Out[(J + 1) % M].Y - Out[J].Y, Out[J].X - Out[(J + 1) % M].X); Nd.Normalize();
            if (FVector2D::DotProduct(Nd, Mid - Ce) < 0) Nd = -Nd;
            const FVector Nn(Nd.X, Nd.Y, 0);
            for (int32 R = 0; R < Rows; ++R) Quad(Side(J, R), Side(J + 1, R), Side(J + 1, R + 1), Side(J, R + 1), Nn, Nn, Nn, Nn);
        }
    }
};

namespace
{
    TArray<FVector2D> Hex(float X, float Y, float R, float Turn)
    {
        TArray<FVector2D> P;
        for (int32 K = 0; K < 6; ++K) { const float A = Turn + K * PI / 3; P.Add(FVector2D(X + R * FMath::Cos(A), Y + R * FMath::Sin(A))); }
        return P;
    }
    TArray<FVector2D> Box2(float X0, float X1, float Y0, float Y1) { return {FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1)}; }
    // A broad irregular island round (X, Y): radii along X and Y, its edge wandering by Wobble.
    TArray<FVector2D> Isle(float X, float Y, float RX, float RY, float Wobble, float Seed, int32 Count = 40)
    {
        TArray<FVector2D> P;
        for (int32 K = 0; K < Count; ++K)
        {
            const float A = 2 * PI * K / Count;
            const float W = 1 + Wobble * Noise(FMath::Cos(A) * 300, FMath::Sin(A) * 300, 160, Seed);
            P.Add(FVector2D(X + RX * W * FMath::Cos(A), Y + RY * W * FMath::Sin(A)));
        }
        return P;
    }
    // The road's spine: along the centre line, with the width varying.
    float SpineHalf(const FSeg& S, float X) { return FMath::Lerp(S.W0, S.W1, FMath::Clamp((X - S.X0) / (S.X1 - S.X0), 0.f, 1.f)) * .5f; }
    float SpineY(const FSeg& S, float X) { return FMath::Lerp(S.Y0, S.Y1, Ease((X - S.X0) / (S.X1 - S.X0))); }
    TArray<FVector2D> SpineOutline(const FSeg& S)
    {
        TArray<FVector2D> L, R;
        const int32 Count = FMath::Max(2, FMath::CeilToInt((S.X1 - S.X0) / 120.f));
        for (int32 I = 0; I <= Count; ++I)
        {
            const float X = FMath::Lerp(S.X0, S.X1, float(I) / Count), H = SpineHalf(S, X), Y = SpineY(S, X);
            const float J = (I == 0 || I == Count) ? 0.f : 6.f * Noise(X, 0, 60, 4);
            R.Add(FVector2D(X, Y + H + J)); L.Insert(FVector2D(X, Y - H - J), 0);
        }
        R.Append(L);
        return R;
    }
    // Spawn points (local, his centre), facing.
    const FVector SpawnAt[PhlegSpawns] = {FVector(0, 0, G + 34.65f), FVector(RoadX + 200, 0, G + 34.65f), FVector(LakeX + 250, 0, G + 34.65f),
                                          FVector(LakeShoreX + 400, -90, G + 34.65f), FVector(AscentX + 250, 0, G + 34.65f), FVector(25600, 0, FortressTop + 34.65f)};
    const float SpawnYaw[PhlegSpawns] = {0, 0, 0, 0, 0, 0};
    const float StretchX[PhlegSpawns + 1] = {-1e9f, RoadX, LakeX, LakeShoreX + 150, AscentX, FortressX + 50, 1e9f};
}

FVector PhlegethosOrigin() { return AFallCutscene::LandingSpot() + FVector(0, -1600000, 0); }
bool InPhlegethos(const FVector& World)
{
    const FVector L = PhlegLocal(World);
    return L.X > -6000 && L.X < 33000 && FMath::Abs(L.Y) < 9000 && L.Z > -2000 && L.Z < 8000;
}
FVector PhlegethosLandingSpot() { return PhlegWorld(FVector(0, 0, G)); }
FVector PhlegethosSpawn(EPhlegSpawn S) { return PhlegWorld(SpawnAt[int32(S)]); }
float PhlegethosSpawnYaw(EPhlegSpawn S) { return SpawnYaw[int32(S)]; }
int32 PhlegethosStretchAt(const FVector& World)
{
    if (!InPhlegethos(World)) return -1;
    const float X = PhlegLocal(World).X;
    for (int32 I = 0; I < PhlegSpawns; ++I) if (X < StretchX[I + 1]) return I;
    return PhlegSpawns - 1;
}
const TCHAR* PhlegethosStretchName(int32 S)
{
    static const TCHAR* Names[] = {TEXT("Arrival"), TEXT("Road"), TEXT("Lake"), TEXT("Gorge"), TEXT("Ascent"), TEXT("Fortress")};
    return S >= 0 && S < PhlegSpawns ? Names[S] : TEXT("None");
}
bool PhlegethosRespawn(const FVector& World, FVector& Out)
{
    const int32 S = PhlegethosStretchAt(World);
    if (S < 0) return false;
    Out = PhlegethosSpawn(EPhlegSpawn(S));
    return true;
}
bool IsPhlegethosSpawn(const FVector& World, float& Yaw)
{
    for (int32 I = 0; I < PhlegSpawns; ++I) if (FVector::Dist(World, PhlegethosSpawn(EPhlegSpawn(I))) < 1.f) { Yaw = SpawnYaw[I]; return true; }
    return false;
}
bool PhlegethosLavaSwallows(const FVector& Feet) { return InPhlegethos(Feet) && PhlegLocal(Feet).Z < LavaZ + 30.f; }
bool PhlegethosIntense(const FVector& World, bool bWas)
{
    if (!InPhlegethos(World)) return false;
    return PhlegLocal(World).X > AscentX + (bWas ? -400.f : 300.f);
}

APhlegethosSetting::APhlegethosSetting()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Motes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Motes"));
    Motes->SetupAttachment(RootComponent);
    Motes->SetStaticMesh(Quad.Object);
    Motes->NumCustomDataFloats = 1;
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetCastShadow(false);
    Motes->SetMobility(EComponentMobility::Movable);
    BombBalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BombBalls"));
    BombBalls->SetupAttachment(RootComponent);
    BombBalls->SetStaticMesh(Ball.Object);
    BombBalls->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BombBalls->SetCastShadow(false);
    Smoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Smoke"));
    Smoke->SetupAttachment(RootComponent);
    Smoke->SetStaticMesh(Quad.Object);
    Smoke->NumCustomDataFloats = 3;
    Smoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Smoke->SetCastShadow(false);
    Smoke->bNeverDistanceCull = true;
    BombMarks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BombMarks"));
    BombMarks->SetupAttachment(RootComponent);
    BombMarks->SetStaticMesh(Ball.Object);
    BombMarks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BombMarks->SetCastShadow(false);
}

APhlegethosSetting* APhlegethosSetting::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<APhlegethosSetting> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

APhlegethosSetting* APhlegethosSetting::Enter(UWorld* World, bool bMakeLive)
{
    if (!World) return nullptr;
    if (APhlegethosSetting* Existing = Get(World)) { if (bMakeLive) Existing->SetLive(true); return Existing; }
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    // Whatever stage stands (the ship and its storm, or a checkpoint's Waterdeep) goes dark, still and silent, as
    // AShipSetting::Enter does: there is no way back from the Hells.
    int32 Hidden = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (A == Chuck || A->GetAttachParentActor() == Chuck || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>()) continue;
        const FString Class = A->GetClass()->GetName();
        if (Class.Contains(TEXT("PhlegethosFall")) || Class.Contains(TEXT("CameraActor")) || Class.Contains(TEXT("AstralSummon")) || Class.Contains(TEXT("ChuckFlames"))) continue;
        if (A->IsA<AInfo>())
        {
            TInlineComponentArray<UActorComponent*> Parts(A);
            for (auto* Part : Parts)
                if (Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>())
                    Cast<USceneComponent>(Part)->SetVisibility(false);
            continue;
        }
        if (auto* Volume = Cast<APostProcessVolume>(A)) Volume->bEnabled = false;
        A->SetActorHiddenInGame(true);
        A->SetActorEnableCollision(false);
        A->SetActorTickEnabled(false);
        TInlineComponentArray<UActorComponent*> Parts(A);
        for (auto* Part : Parts)
        {
            if (auto* Audio = Cast<UAudioComponent>(Part)) Audio->Stop();
            else if (auto* Post = Cast<UPostProcessComponent>(Part)) Post->bEnabled = false;
            else if (Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>())
                Cast<USceneComponent>(Part)->SetVisibility(false);
            if (Part != A->GetRootComponent()) Part->SetComponentTickEnabled(false);
        }
        ++Hidden;
    }
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Stage = World->SpawnActor<APhlegethosSetting>(PhlegethosOrigin(), FRotator::ZeroRotator, P);
    Stage->Build();
    Stage->SetLive(bMakeLive);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_ENTER hidden=%d live=%d triangles=%d missing=%d"), Hidden, bMakeLive, Stage->Triangles, Stage->Missing);
    return Stage;
}

void APhlegethosSetting::SetLive(bool bOn)
{
    if (bLive == bOn) return;
    bLive = bOn;
    if (bOn) ResetHazards();
    if (Rumble) { if (bOn) Rumble->FadeIn(2.f, .32f); else Rumble->FadeOut(.5f, 0.f); }
}

UProceduralMeshComponent* APhlegethosSetting::Emit(const TCHAR* Name, FPhlegSurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow)
{
    if (S.T.IsEmpty()) return nullptr;
    auto* Mesh = NewObject<UProceduralMeshComponent>(this, FName(Name)); Mesh->SetupAttachment(RootComponent);
    Mesh->SetWorldLocation(PhlegethosOrigin());
    Mesh->bUseComplexAsSimpleCollision = true; Mesh->RegisterComponent();
    Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, bSolid);
    Missing += !Material;
    Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCastShadow(bShadow);
    Mesh->ComponentTags.Add(TEXT("Phlegethos"));
    Triangles += S.T.Num() / 3;
    return Mesh;
}

void APhlegethosSetting::Build()
{
    if (bBuilt) return;
    bBuilt = true;
    const double Started = FPlatformTime::Seconds();
    auto Mat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
    BasaltMat = Mat(TEXT("/Game/Art/Phlegethos/M_PhlegBasalt.M_PhlegBasalt"));
    PathMat = Mat(TEXT("/Game/Art/Phlegethos/M_PhlegPath.M_PhlegPath"));
    LavaMat = Mat(TEXT("/Game/Art/Phlegethos/M_PhlegLava.M_PhlegLava"));
    GlowMat = Mat(TEXT("/Game/Art/Phlegethos/M_PhlegGlow.M_PhlegGlow"));
    IronMat = Mat(TEXT("/Game/Art/Materials/M_RustIron.M_RustIron"));
    DarkMat = Mat(TEXT("/Game/Art/Materials/M_Dark.M_Dark"));
    if (auto* Soot = Mat(TEXT("/Game/Art/ChultTemple/Fire/M_TempleJetSmoke.M_TempleJetSmoke"))) Smoke->SetMaterial(0, Soot); else ++Missing;
    if (auto* Ember = Mat(TEXT("/Game/Art/Phlegethos/M_PhlegEmber.M_PhlegEmber"))) { Motes->SetMaterial(0, Ember); }
    else ++Missing;
    BombBalls->SetMaterial(0, LavaMat);
    BombMarks->SetMaterial(0, LavaMat);
    for (int32 I = 0; I < SoundCount; ++I)
        Sounds.Add(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), SoundNames[I], SoundNames[I])));
    BuildEnvironment();

    FRandomStream R(8);
    FPhlegSurf Rock, Path, Lava, Glow, Iron, Dark, Far, Falls, Chain, RockVis, RockCol;
    auto Flat = [](float Z) { return [Z](float, float) { return Z; }; };
    // A sheer block of broken basalt: a plain collision box (hidden), seen as the faceted crag cut into it (FPhlegSurf::Crag).
    // Depths per side -X, +X, -Y, +Y (cm): a few where he runs or jumps on the face, deep where he never touches it.
    auto CragBox = [&](float X0, float X1, float Y0, float Y1, float Z0, float Z1, float DmX, float DpX, float DmY, float DpY)
    {
        const float D[4] = {DmX, DpX, DmY, DpY};
        RockCol.B(X0, X1, Y0, Y1, Z0, Z1);
        RockVis.Paint = Rock.Paint; RockVis.Crag(X0, X1, Y0, Y1, Z0, Z1, D, X0 * .001f + Y0 * .0007f);
    };

    // ---------------------------------------------------------------- the molten lake, to the horizon
    {
        // Rings of quads out from the route, coarser further out (the UVs stay world cm/400).
        const float Xc = 14000, Yc = 0;
        // (to 29 km, just inside the fog's cutoff: from the fall's height the sky dome's foot showed past a nearer edge)
        TArray<float> Rad = {0, 3000, 7000, 12000, 20000, 32000, 50000, 80000, 130000, 250000, 500000, 1000000, 1800000, 2900000};
        constexpr int32 Segs = 48;
        Lava.Paint = FColor(0, 8, 0, 255);
        for (int32 K = 0; K + 1 < Rad.Num(); ++K)
            for (int32 J = 0; J < Segs; ++J)
            {
                auto P = [&](int32 Kk, int32 Jj) { const float A = 2 * PI * Jj / Segs; return FVector(Xc + Rad[Kk] * FMath::Cos(A) * 1.6f, Yc + Rad[Kk] * FMath::Sin(A), LavaZ); };
                if (K == 0) Lava.Tri(P(0, 0), P(1, J), P(1, J + 1), FVector::UpVector);
                else Lava.Flat(P(K, J), P(K + 1, J), P(K + 1, J + 1), P(K, J + 1), FVector::UpVector);
            }
    }

    // ---------------------------------------------------------------- 0. the arrival plain
    {
        Rock.Paint = FColor(215, 40, 90, 255);   // (R: hot, its plates' cracks glow)
        const TArray<FVector2D> Plain = Isle(0, 0, 2600, 2300, .16f, 1, 56);
        // (Level round the landing, where he lies after the fall: the plain's swell only from 3 m out.)
        Rock.Prism(Plain, [](float X, float Y) { const float D = FVector2D(X, Y).Size(), K = Ease((D - 300.f) / 500.f); return G + K * (18.f * Noise(X, Y, 900, 3) + 6.f * Noise(X, Y, 200, 5)) - FMath::Max(0.f, D - 1700.f) * .05f; }, Rim, 60, 0, 1, 7);
        // The neck where the road leaves it.
        Rock.Prism(Box2(1900, 2600, -330, 330), Flat(G), Rim, 30, 0, 2);
        // The worn path from the landing to the neck: flagstones a hand above the basalt.
        Path.Paint = FColor(0, 0, 60, 255);
        for (float X = 420; X < 2500; X += 160)
        {
            const float Y = 160.f * FMath::Sin(X / 900.f) * (1 - X / 2600.f), W = 110 + 25 * Noise(X, 0, 400, 7);
            const float K = X < 1900 ? Ease((FVector2D(X, Y).Size() - 300.f) / 500.f) : 0.f;
            Path.Prism(Box2(X - 80, X + 80, Y - W, Y + W), Flat(G + 2.f + K * (18.f * Noise(X, Y, 900, 3) + 6.f * Noise(X, Y, 200, 5))), G - 30, 0, 0, 9, 1);
        }
        // Boulders to vault and mantle, and larger tumbled rocks round the edge.
        const FVector Boulders[] = {FVector(500, -420, 45), FVector(820, 360, 34), FVector(1250, -260, 55), FVector(-700, 600, 120), FVector(-1100, -800, 160),
                                    FVector(300, 1200, 140), FVector(-300, -1500, 200), FVector(1500, 900, 90), FVector(1700, -900, 110), FVector(-1800, 200, 230)};
        Rock.Paint = FColor(40, 120, 20, 255);
        int32 Seed = 11;
        for (const FVector& Bo : Boulders)
            Rock.Blob(FVector(Bo.X, Bo.Y, G + Bo.Z * .45f), FVector(Bo.Z * 1.2f, Bo.Z, Bo.Z * .75f), Seed++, G - 10);
        // Fissures: glowing seams across the plain.
        Glow.Paint = FColor(255, 95, 20, 255);
        for (int32 F = 0; F < 14; ++F)
        {
            FVector2D P(R.FRandRange(-2000, 1800), R.FRandRange(-1800, 1800));
            if (P.Size() < 260) continue;   // not under the landing
            FVector2D D = FVector2D(R.FRandRange(-1, 1), R.FRandRange(-1, 1)).GetSafeNormal();
            for (int32 S = 0; S < 9; ++S)
            {
                const FVector2D Q = P + D * R.FRandRange(60, 140);
                const float W = R.FRandRange(2.5f, 7.f);
                const FVector2D Sd(-D.Y, D.X);
                auto Z = [](const FVector2D& A) { return G + 18.f * Noise(A.X, A.Y, 900, 3) + 6.f * Noise(A.X, A.Y, 200, 5) + 1.2f; };
                Glow.Flat(FVector(P - Sd * W, Z(P)), FVector(Q - Sd * W, Z(Q)), FVector(Q + Sd * W, Z(Q)), FVector(P + Sd * W, Z(P)), FVector::UpVector);
                P = Q; D = (D + FVector2D(R.FRandRange(-.6f, .6f), R.FRandRange(-.6f, .6f))).GetSafeNormal();
                if (P.Size() > 2100) break;
            }
        }
    }

    // ---------------------------------------------------------------- 1. the lava road's spine
    Rock.Paint = FColor(175, 60, 40, 255);
    for (const FSeg& S : Spine) Rock.Prism(SpineOutline(S), Flat(G), Rim, 22, 0, S.X0 * .001f, 2);
    // Rocks standing out of the lava either side, so the spine is a road through a field of them.
    for (int32 I = 0; I < 26; ++I)
    {
        const float X = R.FRandRange(RoadX + 300, LakeX - 200), Side = (R.FRand() < .5f) ? 1.f : -1.f;
        const float Y = Side * R.FRandRange(420, 1600), S = R.FRandRange(60, 220);
        Rock.Blob(FVector(X, Y, R.FRandRange(-40, 80)), FVector(S * 1.3f, S, S * R.FRandRange(.8f, 1.6f)), 40 + I);
    }
    // The geysers' vents: low cones of crusted rock in the lava beside the spine.
    for (const FGeyser& Gy : Geysers)
    {
        // (the spine's edge there)
        const FSeg* Seg = nullptr; for (const FSeg& S : Spine) if (Gy.X >= S.X0 && Gy.X <= S.X1) Seg = &S;
        const float Edge = Seg ? SpineY(*Seg, Gy.X) + Gy.Side * SpineHalf(*Seg, Gy.X) : 0.f;
        const FVector Base(Gy.X, Edge + Gy.Side * 95.f, LavaZ + 40);
        Rock.Paint = FColor(255, 30, 60, 255);
        Rock.Blob(Base - FVector(0, 0, 30), FVector(80, 80, 70), 70 + Gy.X * .01f);
        FSpout Sp; Sp.Base = Base + FVector(0, 0, 30); Sp.Side = Gy.Side; Sp.Phase = Gy.Phase;
        Spouts.Add(Sp);
    }

    // ---------------------------------------------------------------- 2. the column lake
    {
        // The near shore (the road's widened end) and the far shore.
        Rock.Paint = FColor(150, 40, 90, 255);
        Rock.Prism(Isle(8150, 0, 330, 420, .1f, 21, 24), Flat(G), Rim, 40, 0, 21, 2);
        // Ranks of columns standing out of the lake well off the route, broken at different heights: the causeway's look.
        for (int32 I = 0; I < 140; ++I)
        {
            const float X = R.FRandRange(8300, 12800), Y = ((R.FRand() < .5f) ? 1.f : -1.f) * R.FRandRange(320, 2200);
            const float Rr = R.FRandRange(40, 75), H = R.FRandRange(-80, 420) * FMath::Clamp(1.4f - FMath::Abs(Y) / 2200.f, .2f, 1.f);
            Rock.Paint = FColor(R.RandRange(0, 50), R.RandRange(0, 255), 30, 255);
            Rock.Prism(Hex(X, Y, Rr, R.FRandRange(0, 1)), Flat(G + H), Rim, 3, 0, I, 1);
        }
        // The route's columns: fixed ones here, the sinking ones each their own mesh.
        int32 K = 0;
        for (const FColumn& Co : Columns)
        {
            const TArray<FVector2D> H = Hex(Co.X, Co.Y, Co.R, .3f * K);
            if (!Co.bSinks)
            {
                Rock.Paint = FColor(30, 80, 20, 255);
                Rock.Prism(H, Flat(G + Co.Top), Rim, 2, 0, 50 + K, 1);
            }
            else
            {
                FPhlegSurf One; One.Paint = FColor(255, 60, 10, 255);   // cracked through: its seams glow
                One.Prism(H, Flat(G + Co.Top), Rim - 20, 2, 0, 50 + K, 1);
                FMover Mv; Mv.Kind = 0; Mv.Top = FVector(Co.X, Co.Y, G + Co.Top); Mv.Half = Co.R;
                Mv.Mesh = Emit(*FString::Printf(TEXT("PhlegSinker_%d"), K), One, BasaltMat, true, true);
                Movers.Add(Mv);
            }
            ++K;
        }
        // The slabs either side of the wide gap: a chained wall run across.
        Rock.Paint = FColor(20, 160, 10, 255);
        for (float S : {-1.f, 1.f})
        {
            const float Y0 = S * SlabHalfGap, Y1 = S * (SlabHalfGap + SlabThick);
            // (the inner face is the wall run's)
            CragBox(SlabX0, SlabX1, FMath::Min(Y0, Y1), FMath::Max(Y0, Y1), Rim, SlabTop, 10, 10, S > 0 ? 8.f : 35.f, S > 0 ? 35.f : 8.f);
            // A broken crown over each.
            for (int32 I = 0; I < 5; ++I)
                Rock.TurnedBox(FVector(R.FRandRange(SlabX0 + 60, SlabX1 - 60), S * (SlabHalfGap + SlabThick * .5f), SlabTop + R.FRandRange(10, 50)), FVector(R.FRandRange(40, 90), SlabThick * .45f, R.FRandRange(20, 60)), FRotator(R.FRandRange(-20, 20), R.FRandRange(-6, 6), R.FRandRange(-8, 8)));
        }
        Rock.Paint = FColor(150, 40, 90, 255);
        Rock.Prism(Isle(LakeShoreX + 260, -60, 420, 520, .08f, 22, 24), Flat(G), Rim, 40, 0, 22, 2);
    }

    // ---------------------------------------------------------------- 3. the lava-fall gorge
    {
        // The cliff on the -Y side: its face flat and sheer along the shelf (he wall runs on it), its top broken.
        Rock.Paint = FColor(30, 90, 50, 255);
        CragBox(GorgeX - 200, ChainX0, -1600, CliffY, Rim, CliffTop, 70, 15, 0, 9);   // (+Y: the wall run's face)
        CragBox(ChainX1, AscentX + 900, -1600, CliffY - 200, Rim, CliffTop + 200, 30, 50, 0, 55);
        for (int32 I = 0; I < 40; ++I)
        {
            const float X = R.FRandRange(GorgeX - 100, AscentX + 800);
            Rock.TurnedBox(FVector(X, R.FRandRange(-1500, -400), CliffTop + R.FRandRange(0, 250)), FVector(R.FRandRange(150, 420), R.FRandRange(150, 400), R.FRandRange(80, 300)), FRotator(R.FRandRange(-12, 12), R.FRandRange(0, 90), R.FRandRange(-12, 12)));
        }
        // The shelf along its foot, broken twice; the second break drops to a lower shelf.
        Rock.Paint = FColor(120, 60, 40, 255);
        Rock.Prism(Box2(LakeShoreX + 600, Break1X0, CliffY, ShelfY1), Flat(G), Rim, 10, 0, 31, 1);
        Rock.Prism(Box2(Break1X1, Break2X0, CliffY, ShelfY1 - 10), Flat(G), Rim, 10, 0, 32, 1);
        Rock.Prism(Box2(Break2X1, ChainX0, CliffY, ShelfY1 + 10), Flat(LowShelf), Rim, 10, 0, 33, 1);
        // The cleft behind the chains, where a lava fall pours out of the cliff into the river.
        CragBox(ChainX0, ChainX1, -1600, -700, Rim, CliffTop + 100, 0, 0, 0, 70);
        // The broken arch: a pillar each end, the lintel between them, iron chains hanging to the grips.
        Rock.Paint = FColor(40, 200, 30, 255);
        const float LintelZ = ChainGripZ + 330;
        Rock.Prism(Hex(ChainX0 + 40, -265, 70, .2f), Flat(LintelZ + 60), Rim, 4, 0, 34, 1);
        Rock.Prism(Hex(ChainX1 - 40, -265, 70, .5f), Flat(LintelZ + 30), Rim, 4, 0, 35, 1);
        Rock.TurnedBox(FVector((ChainX0 + ChainX1) * .5f, -170, LintelZ + 30), FVector((ChainX1 - ChainX0) * .5f + 20, 95, 26), FRotator(.6f, 0, 0));
        int32 Grips = 0;
        for (int32 K = 0; K < ChainGrips; ++K)
        {
            const float X = ChainX0 + 70 + K * ChainStep;
            const FVector Grip(X, ChainY, ChainGripZ);
            // Links: alternating flat ovals hanging from the lintel down to a ring.
            const float Top = LintelZ + 4;
            bool bTurn = false;
            for (float Z = Top; Z > Grip.Z + 16; Z -= 11.f, bTurn = !bTurn)
            {
                const FVector C0(X, ChainY, Z), C1(X, ChainY, Z - 11);
                const FVector Off = bTurn ? FVector(3.5f, 0, 0) : FVector(0, 3.5f, 0);
                Chain.Spar(C0 + Off, C1 + Off, 1.3f, 1.3f, 5); Chain.Spar(C0 - Off, C1 - Off, 1.3f, 1.3f, 5);
            }
            // The ring he holds: its bottom bar across the line he swings along.
            Chain.Spar(Grip + FVector(0, -9, 0), Grip + FVector(0, 9, 0), 1.8f, 1.8f, 6);
            Chain.Spar(Grip + FVector(0, -9, 0), Grip + FVector(0, -6, 16), 1.6f, 1.6f, 6);
            Chain.Spar(Grip + FVector(0, 9, 0), Grip + FVector(0, 6, 16), 1.6f, 1.6f, 6);
            FChuckSwingGrip SG; SG.Grip = PhlegWorld(Grip); SG.Along = FVector::ForwardVector;
            AddChuckSwingGrip(SG);
            ++Grips;
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_CHAINS grips=%d"), Grips);
        // The far side of the gorge: a great ridge with lava falls pouring down it.
        Rock.Paint = FColor(30, 120, 80, 255);
        CragBox(GorgeX + 400, AscentX + 600, 1000, 2600, Rim, G + 1200, 110, 110, 150, 0);
        for (int32 I = 0; I < 30; ++I)
            Rock.TurnedBox(FVector(R.FRandRange(GorgeX + 400, AscentX + 600), R.FRandRange(1100, 2500), G + 1200 + R.FRandRange(0, 300)), FVector(R.FRandRange(200, 500), R.FRandRange(200, 500), R.FRandRange(100, 400)), FRotator(R.FRandRange(-10, 10), R.FRandRange(0, 90), R.FRandRange(-10, 10)));
        // Lava falls: a sheet down a face, flowing, with a churned pool at its foot.
        auto Fall = [&](float X, float Y, float Out, float Top, float Width)
        {
            Falls.Paint = FColor(0, 255, 0, 255);
            const int32 Rows = 10;
            for (int32 K = 0; K < Rows; ++K)
            {
                const float U0 = float(K) / Rows, U1 = float(K + 1) / Rows;
                const float Z0 = FMath::Lerp(Top, LavaZ - 10, U0), Z1 = FMath::Lerp(Top, LavaZ - 10, U1);
                // (bulging out from the face as it falls)
                const float O0 = Out * (40 + 160 * FMath::Sqrt(U0)), O1 = Out * (40 + 160 * FMath::Sqrt(U1));
                const float H0 = Width * (.5f + .25f * U0), H1 = Width * (.5f + .25f * U1);
                const FVector Nn(0, Out, 0);
                const int32 I = Falls.V.Num();
                Falls.Vert(FVector(X - H0, Y + O0, Z0), Nn, FVector2D(-H0 / 400, (Top - Z0) / 400));
                Falls.Vert(FVector(X + H0, Y + O0, Z0), Nn, FVector2D(H0 / 400, (Top - Z0) / 400));
                Falls.Vert(FVector(X + H1, Y + O1, Z1), Nn, FVector2D(H1 / 400, (Top - Z1) / 400));
                Falls.Vert(FVector(X - H1, Y + O1, Z1), Nn, FVector2D(-H1 / 400, (Top - Z1) / 400));
                Falls.Face(I, I + 1, I + 2, Nn); Falls.Face(I, I + 2, I + 3, Nn);
                // (and its back, so it reads from the cliff side too)
                Falls.Face(I, I + 2, I + 1, -Nn); Falls.Face(I, I + 3, I + 2, -Nn);
            }
        };
        for (float X : {GorgeX + 1100.f, GorgeX + 2600.f, 16900.f, 19200.f}) Fall(X, 1000, -1, G + 1200, 240 + 80 * Hash(X, 1));
        Fall((ChainX0 + ChainX1) * .5f + 200, -700, 1, CliffTop + 100, 300);
        Fall(AscentX + 400, CliffY - 200, 1, CliffTop + 200, 200);
        // The gorge's far end, where the shelf comes out onto the ascent's foot.
        Rock.Paint = FColor(160, 50, 80, 255);
        Rock.Prism(Box2(ChainX1, AscentX + 650, -400, 300), [](float X, float) { return FMath::Lerp(ChainLand, G, FMath::Clamp((X - ChainX1 - 150) / 900.f, 0.f, 1.f)); }, Rim, 30, 0, 36, 8);
    }

    // ---------------------------------------------------------------- 4. the ascent
    {
        // The bomb ramp up the mesa's flank, broad and open.
        Rock.Paint = FColor(150, 60, 110, 255);
        Rock.Prism(Box2(RampX0 - 10, RampX1, -RampHalf, RampHalf), [](float X, float) { return FMath::Lerp(G, RampTop, FMath::Clamp((X - RampX0) / (RampX1 - RampX0), 0.f, 1.f)); }, Rim, 30, 0, 41, 6);
        Path.Paint = FColor(0, 0, 120, 255);
        for (float X = RampX0 + 40; X < RampX1 - 60; X += 150)
        {
            auto Z = [](float Xx) { return FMath::Lerp(G, RampTop, FMath::Clamp((Xx - RampX0) / (RampX1 - RampX0), 0.f, 1.f)); };
            Path.Prism(Box2(X - 72, X + 72, -110, 110), [&](float Xx, float) { return Z(Xx) + 3.f; }, Z(X) - 30, 0, 0, 42, 1);
        }
        // Crumbling steps over the lava to the chimney's foot.
        int32 K = 0;
        for (const FStep& S : Steps)
        {
            FPhlegSurf One; One.Paint = FColor(200, 80, 20, 255);
            TArray<FVector2D> O;
            for (int32 J = 0; J < 8; ++J) { const float A = J * PI / 4 + .39f; const float Rr = S.Half * 1.08f * (1 + .06f * Hash(J, K)); O.Add(FVector2D(S.X + Rr * FMath::Cos(A), S.Y + Rr * FMath::Sin(A))); }
            One.Prism(O, Flat(RampTop), RampTop - 70, 8, 0, 60 + K, 1);
            FMover Mv; Mv.Kind = 1; Mv.Top = FVector(S.X, S.Y, RampTop); Mv.Half = S.Half;
            Mv.Mesh = Emit(*FString::Printf(TEXT("PhlegStep_%d"), K), One, BasaltMat, true, true);
            Movers.Add(Mv);
            ++K;
        }
        // The chimney's foot and the mesa it climbs, its slot cut 1 m wide.
        Rock.Paint = FColor(130, 70, 60, 255);
        Rock.Prism(Box2(23720, ChimneyX0 + 2, -260, 260), Flat(ChimneyBase), Rim, 20, 0, 43, 1);
        // (the slot's faces are wall-jumped: a few cm only)
        CragBox(ChimneyX0, ChimneyX1, -600, -ChimneyHalf, Rim, MesaTop, 25, 0, 45, 5);
        CragBox(ChimneyX0, ChimneyX1, ChimneyHalf, 600, Rim, MesaTop, 25, 0, 5, 45);
        Rock.B(ChimneyX0 - 2, ChimneyX1, -ChimneyHalf, ChimneyHalf, Rim, ChimneyBase);   // the slot's floor
        CragBox(ChimneyX1, MesaX1, -600, 600, Rim, MesaTop, 5, 45, 55, 55);
        for (int32 I = 0; I < 10; ++I)
            Rock.TurnedBox(FVector(R.FRandRange(ChimneyX1 + 300, MesaX1 - 200), ((R.FRand() < .5f) ? 1 : -1) * R.FRandRange(320, 560), MesaTop + R.FRandRange(10, 60)), FVector(R.FRandRange(50, 140), R.FRandRange(40, 120), R.FRandRange(30, 90)), FRotator(R.FRandRange(-15, 15), R.FRandRange(0, 90), R.FRandRange(-15, 15)));
        Path.Paint = FColor(0, 0, 80, 255);
        for (float X = ChimneyX1 + 120; X < MesaX1 - 40; X += 150)
            Path.Prism(Box2(X - 72, FMath::Min(X + 72, MesaX1 - 2.f), -100, 100), Flat(MesaTop + 3), MesaTop - 30, 0, 0, 44, 1);
    }

    // ---------------------------------------------------------------- 5. the fortress approach
    {
        const float Z = FortressTop;
        Rock.Paint = FColor(130, 90, 100, 255);
        Rock.Prism(Box2(FortressX, GateX + 600, -2600, 2600), Flat(Z), Rim, 40, 0, 51, 2);
        Path.Paint = FColor(0, 0, 40, 255);
        Path.Prism(Box2(FortressX + 40, GateX, -170, 170), Flat(Z + 4), Z - 30, 0, 0, 52, 1);
        // The idols: hulking horned figures on plinths either side of the processional, their eyes lit from within.
        int32 I = 0;
        for (float X = FortressX + 500; X < GateX - 300; X += 560, ++I)
            for (float S : {-1.f, 1.f})
            {
                const FVector Base(X, S * 430, Z);
                Dark.Paint = FColor(0, 0, 0, 255);
                Rock.Paint = FColor(60, 200, 40, 255);
                Rock.B(X - 110, X + 110, Base.Y - 110, Base.Y + 110, Z, Z + 120);                         // plinth
                Rock.TurnedBox(Base + FVector(0, 0, 330), FVector(70, 85, 210), FRotator(0, 0, -S * 3));  // body
                Rock.Blob(Base + FVector(0, 0, 590), FVector(62, 58, 66), 80 + I);                        // head
                for (float H : {-1.f, 1.f})
                {
                    // Horns sweeping up and back.
                    const FVector Root = Base + FVector(0, H * 40, 625);
                    Rock.Spar(Root, Root + FVector(-30, H * 70, 90), 16, 6, 6);
                    Rock.Spar(Root + FVector(-30, H * 70, 90), Root + FVector(-80, H * 80, 190), 6, .5f, 6);
                    // Shoulders and fists on the knees.
                    Rock.Blob(Base + FVector(0, H * 95, 480), FVector(55, 50, 50), 90 + I + H);
                    Rock.Blob(Base + FVector(60, H * 70, 200), FVector(42, 38, 44), 95 + I + H);
                    Glow.Paint = FColor(255, 70, 15, 255);
                    const FVector Eye = Base + FVector(-S * 0, H * 22, 600) + FVector(56, 0, 0);
                    Glow.Box(Eye, FVector(3, 0, 0), FVector(0, 10, 0), FVector(0, 0, 4));
                }
                // A brazier burning at its feet, toward the processional.
                const FVector Bz = Base + FVector(0, -S * 170, 0);
                Iron.Spar(Bz, Bz + FVector(0, 0, 80), 7, 5, 6);
                Iron.Spar(Bz + FVector(0, 0, 80), Bz + FVector(0, 0, 112), 36, 46, 10);
                AddDockFlame(this, PhlegWorld(Bz + FVector(0, 0, 108)), 48, 95);
            }
        // The fortress wall, iron-black, with towers, slits lit red, spikes along its top; the gate shut.
        const float W0 = GateX, W1 = GateX + 500, Top = Z + 2600;
        Dark.Paint = FColor(0, 0, 0, 255);
        Rock.Paint = FColor(10, 200, 0, 255);
        Rock.B(W0, W1, -4200, -260, Z - 400, Top);
        Rock.B(W0, W1, 260, 4200, Z - 400, Top);
        Rock.B(W0, W1, -260, 260, Z + 900, Top);   // over the gate
        for (float Y = -4100; Y <= 4100; Y += 160)
            if (FMath::Abs(Y) > 300) Rock.B(W0 - 10, W0 + 90, Y - 50, Y + 50, Top, Top + 120);
        for (float Y : {-3600.f, -1700.f, -520.f, 520.f, 1700.f, 3600.f})
        {
            TArray<FVector2D> T8;
            for (int32 K = 0; K < 8; ++K) { const float A = K * PI / 4 + PI / 8; T8.Add(FVector2D(W0 + 150 + 330 * FMath::Cos(A), Y + 330 * FMath::Sin(A))); }
            const float Ht = Top + (FMath::Abs(Y) < 600 ? 900.f : 600.f);
            Rock.Prism(T8, Flat(Ht), Z - 400, 0, 0, Y, 1);
            for (int32 K = 0; K < 8; ++K)
            {
                const FVector Sp(T8[K].X, T8[K].Y, Ht);
                Iron.Spar(Sp, Sp + FVector(0, 0, 260), 18, 1, 6);
            }
            Iron.Spar(FVector(W0 + 150, Y, Ht), FVector(W0 + 150, Y, Ht + 700), 40, 2, 8);
            Glow.Paint = FColor(255, 60, 12, 255);
            for (float Hz = Z + 600; Hz < Ht - 200; Hz += 420) Glow.Box(FVector(W0 + 150 - 332, Y, Hz), FVector(4, 0, 0), FVector(0, 14, 0), FVector(0, 0, 60));
        }
        Glow.Paint = FColor(255, 60, 12, 255);
        for (float Y = -3900; Y <= 3900; Y += 700) if (FMath::Abs(Y) > 900) Glow.Box(FVector(W0 - 3, Y, Z + 1500 + 300 * Hash(Y, 3)), FVector(4, 0, 0), FVector(0, 10, 0), FVector(0, 0, 70));
        // The gate: two iron leaves, studded, with a bar across.
        Iron.B(W0 + 40, W0 + 80, -260, 260, Z, Z + 900);
        for (float Y = -230; Y <= 230; Y += 46) for (float Hz = Z + 40; Hz < Z + 880; Hz += 70) Iron.Spar(FVector(W0 + 38, Y, Hz), FVector(W0 + 30, Y, Hz), 5, 2, 5);
        Iron.B(W0 + 20, W0 + 40, -290, 290, Z + 380, Z + 440);
        Iron.B(W0 + 20, W0 + 40, -3, 3, Z, Z + 900);
        // He goes no further: the gate's shut.
        FPhlegSurf Stop; Stop.B(W0 - 260, W0, -4200, 4200, Z, Z + 600);
        if (auto* Wall = Emit(TEXT("PhlegGateStop"), Stop, DarkMat, true, false)) Wall->SetHiddenInGame(true);
    }

    // ---------------------------------------------------------------- the land beyond: ridges, mesas, the volcano
    {
        // A ring of broken ranges round the lake, from 3 km out rising to about 550 m (a few degrees above his horizon); the
        // volcano 12 km off, 1.5 km high, its crater lit (the fall sees it climb into view as he drops below its height).
        // Units are cm, as everywhere. (2026-11-15: they stood from 600 m out to 800 m high and walled in the sky.)
        auto Volcano = [](float X, float Y) { const float D = FVector2D(X - VolcanoAt.X, Y - VolcanoAt.Y).Size(); return VolcanoTop * (1 - FMath::Pow(D / VolcanoBase, .75f)) - FMath::Max(0.f, CraterR - D) * 1.1f; };
        auto Ranges = [&](float X, float Y)
        {
            const float Dx = (X - 14000) / 1.3f, D = FVector2D(Dx, Y).Size();
            const float Out = Ease((D - 300000) / 400000.f);
            const float W = Noise(X, Y, 90000, 61) * 30000;
            // (sharp crests: ridged noise cubed, a second finer ridge on it, as the goal art's jagged ranges)
            const float Ridge = 1 - FMath::Abs(Noise(X + W, Y - W, 110000, 62));
            const float Ridge2 = 1 - FMath::Abs(Noise(X - W, Y + W, 38000, 67));
            const float H = Out * (Ridge * Ridge * Ridge * 42000 + Ridge2 * Ridge2 * 12000 * Ridge + 6000 * Noise(X, Y, 30000, 63) + 2500 * Noise(X, Y, 7000, 64) + 3000) - 2000 * (1 - Out);
            const float V = Volcano(X, Y);
            return FMath::Max(H, V) + (V > 0 ? 2500.f * Noise(X, Y, 15000, 65) + 9000.f * (.5f - FMath::Abs(Noise(X, Y, 26000, 66))) * FMath::Min(1.f, V / 60000.f) : 0.f);
        };
        constexpr int32 N = 170;
        TArray<float> Ax;
        for (int32 I = 0; I < N; ++I) { const float U = float(I) / (N - 1) * 2 - 1; Ax.Add(FMath::Sign(U) * FMath::Pow(FMath::Abs(U), 2.f) * 1200000); }
        TArray<FVector> P; P.SetNum(N * N);
        for (int32 J = 0; J < N; ++J) for (int32 I = 0; I < N; ++I)
        {
            const float X = 14000 + Ax[I] * 1.2f, Y = Ax[J];
            P[J * N + I] = FVector(X, Y, Ranges(X, Y));
        }
        // (dark, no ash: at B 160 the distant ranges read as beige dunes; no hot cracks, which shimmer at that size)
        Far.Paint = FColor(0, 128, 15, 255);
        auto Nrm = [&](int32 I, int32 J)
        {
            const FVector Dx = P[J * N + FMath::Min(I + 1, N - 1)] - P[J * N + FMath::Max(I - 1, 0)];
            const FVector Dy = P[FMath::Min(J + 1, N - 1) * N + I] - P[FMath::Max(J - 1, 0) * N + I];
            FVector Nn = FVector::CrossProduct(Dx, Dy).GetSafeNormal(); if (Nn.Z < 0) Nn = -Nn;
            return Nn.IsNearlyZero() ? FVector::UpVector : Nn;
        };
        for (int32 J = 0; J + 1 < N; ++J) for (int32 I = 0; I + 1 < N; ++I)
        {
            const FVector& A = P[J * N + I], &Bp = P[J * N + I + 1], &Cc = P[(J + 1) * N + I + 1], &D = P[(J + 1) * N + I];
            if (FMath::Max(FMath::Max(A.Z, Bp.Z), FMath::Max(Cc.Z, D.Z)) < LavaZ - 20) continue;
            Far.Quad(A, Bp, Cc, D, Nrm(I, J), Nrm(I + 1, J), Nrm(I + 1, J + 1), Nrm(I, J + 1));
        }
        // The crater's lake and the flows down its flanks.
        Lava.Paint = FColor(0, 60, 0, 255);
        const FVector Crater(VolcanoAt.X, VolcanoAt.Y, Volcano(VolcanoAt.X, VolcanoAt.Y) + 1500);
        for (int32 K = 0; K < 16; ++K)
        {
            const float A0 = 2 * PI * K / 16, A1 = 2 * PI * (K + 1) / 16;
            Lava.Tri(Crater, Crater + FVector(FMath::Cos(A0), FMath::Sin(A0), 0) * CraterR * .95f, Crater + FVector(FMath::Cos(A1), FMath::Sin(A1), 0) * CraterR * .95f, FVector::UpVector);
        }
        Lava.Paint = FColor(0, 40, 0, 255);
        for (int32 F = 0; F < 7; ++F)
        {
            const float A = -2.3f + F * .55f;
            FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
            FVector2D Q = FVector2D(Crater.X, Crater.Y) + Dir * CraterR;
            for (int32 S = 0; S < 16; ++S)
            {
                const FVector2D Nq = Q + Dir * 22000 + FVector2D(-Dir.Y, Dir.X) * 8000 * Noise(Q.X, Q.Y, 30000, F);
                const float W0 = 3000 + S * 600, W1 = 3000 + (S + 1) * 600;
                const FVector2D Sd(-Dir.Y, Dir.X);
                auto Zt = [&](const FVector2D& V) { return Ranges(V.X, V.Y) + 400; };
                Lava.Flat(FVector(Q - Sd * W0, Zt(Q)), FVector(Nq - Sd * W1, Zt(Nq)), FVector(Nq + Sd * W1, Zt(Nq)), FVector(Q + Sd * W0, Zt(Q)), FVector::UpVector);
                Q = Nq;
                if (Zt(Q) < 1000) break;
            }
        }
        // Mesas standing out of the lake nearer in, for depth.
        Rock.Paint = FColor(40, 128, 80, 255);
        for (int32 I = 0; I < 22; ++I)
        {
            const float A = R.FRandRange(0, 2 * PI), D = R.FRandRange(4500, 9000);
            const float X = 14000 + FMath::Cos(A) * D * 1.6f, Y = FMath::Sin(A) * D;
            if (FMath::Abs(Y) < 2400) continue;
            Far.Prism(Isle(X, Y, R.FRandRange(500, 1500), R.FRandRange(400, 1200), .25f, 70 + I, 18), Flat(R.FRandRange(600, 3000)), Rim, 160, 0, 70 + I, 1);
        }
    }


    // ---------------------------------------------------------------- the goal art: ruins, chains, spires, steam (2026-11-14)
    {
        auto HangChain = [&](const FVector& Top, float Length)
        {
            bool bTurn = false;
            for (float Z = 0; Z < Length; Z += 11.f, bTurn = !bTurn)
            {
                const FVector C0 = Top - FVector(0, 0, Z), C1 = Top - FVector(0, 0, Z + 11);
                const FVector Off = bTurn ? FVector(3.5f, 0, 0) : FVector(0, 3.5f, 0);
                Chain.Spar(C0 + Off, C1 + Off, 1.4f, 1.4f, 5); Chain.Spar(C0 - Off, C1 - Off, 1.4f, 1.4f, 5);
            }
        };
        auto Drape = [&](const FVector& A, const FVector& B, float Sag)
        {
            const int32 N = FMath::Max(4, FMath::CeilToInt(FVector::Dist(A, B) / 12.f));
            auto At = [&](float U) { return FMath::Lerp(A, B, U) - FVector(0, 0, Sag * 4 * U * (1 - U)); };
            for (int32 K = 0; K < N; ++K)
            {
                const FVector P0 = At(float(K) / N), P1 = At(float(K + 1) / N), Off = (K & 1) ? FVector(0, 0, 3.f) : FVector(0, 3.f, 0);
                Chain.Spar(P0 + Off, P1 + Off, 1.4f, 1.4f, 5); Chain.Spar(P0 - Off, P1 - Off, 1.4f, 1.4f, 5);
            }
        };
        // A ruined gate where the road leaves the plain: two broken pillars on plinths, the taller with the arch's stub
        // leaning out over the road, chains hanging from it.
        Rock.Paint = FColor(20, 180, 30, 255);
        const float PillarX = 2140;
        for (float S : {-1.f, 1.f})
        {
            const float Y = S * 320, H = S < 0 ? 780.f : 470.f;
            Rock.B(PillarX - 62, PillarX + 62, Y - 62, Y + 62, G - 30, G + 45);
            Rock.Prism(Box2(PillarX - 44, PillarX + 44, Y - 44, Y + 44), [H](float, float) { return G + H; }, G + 40, 5, 0, 90 + S, 1);
            Rock.TurnedBox(FVector(PillarX, Y, G + H + 16), FVector(60, 60, 16), FRotator(S * 3, 0, S * 4));
            for (int32 K = 0; K < 3; ++K) Rock.Blob(FVector(PillarX + R.FRandRange(-160, 160), Y + S * R.FRandRange(40, 140), G + 15), FVector(R.FRandRange(25, 45), R.FRandRange(20, 40), R.FRandRange(15, 30)), 300 + K + S, G - 5, 5, 9);
        }
        Rock.TurnedBox(FVector(PillarX, -170, G + 830), FVector(42, 170, 30), FRotator(0, 0, -14));
        HangChain(FVector(PillarX, -110, G + 840), 330);
        HangChain(FVector(PillarX + 6, -40, G + 822), 210);
        Drape(FVector(PillarX, 320, G + 470), FVector(PillarX + 520, 640, G - 10), 90);
        // A carved block fallen by the road.
        Rock.TurnedBox(FVector(1950, 420, G + 32), FVector(80, 50, 42), FRotator(4, 23, -6));
        // Toppled pillars lying half in the lava either side of the road, one still chained.
        const FVector Toppled[4][2] = {{FVector(3150, -560, -30), FVector(3720, -420, 95)}, {FVector(4450, 520, -40), FVector(4050, 820, 70)},
                                       {FVector(8600, -900, -40), FVector(9100, -1150, 110)}, {FVector(14200, 700, -40), FVector(14700, 560, 140)}};
        for (const auto& Tp : Toppled) { Rock.Spar(Tp[0], Tp[1], 44, 41, 10); Rock.Spar(Tp[1], Tp[1] + (Tp[1] - Tp[0]).GetSafeNormal() * 20, 52, 52, 10); }
        Drape(Toppled[0][1] + FVector(0, 0, 40), FVector(3500, -200, G - 4), 60);
        // Spires: tall jagged basalt standing out of the lake and off the cliffs, for the skyline.
        Rock.Paint = FColor(10, 160, 40, 255);
        for (int32 I = 0; I < 34; ++I)
        {
            const float X = R.FRandRange(-1500, 31000), Side = R.FRand() < .5f ? -1.f : 1.f;
            const float Y = Side * R.FRandRange(1500, 5200), Rr = R.FRandRange(90, 260), H = R.FRandRange(700, 2600);
            Rock.Blob(FVector(X, Y, H * .5f - 80), FVector(Rr, Rr * R.FRandRange(.7f, 1.1f), H * .5f), 500 + I, Rim, 6, 9);
            if (R.FRand() < .5f) Rock.Blob(FVector(X + Rr * .8f, Y, H * .3f), FVector(Rr * .6f, Rr * .5f, H * .3f), 600 + I, Rim, 5, 8);
        }
        // Crags along the gorge's cliff tops and the far ridge, so their tops are broken rock, not boxes.
        for (int32 I = 0; I < 46; ++I)
        {
            const bool bFar = (I & 1) != 0;
            const float X = R.FRandRange(GorgeX, AscentX + 800);
            const float Y = bFar ? R.FRandRange(1050, 2500) : R.FRandRange(-1500, -260);
            const float Top = bFar ? G + 1200 : CliffTop;
            const float Rr = R.FRandRange(120, 380);
            Rock.Blob(FVector(X, Y, Top + Rr * .3f), FVector(Rr, Rr * R.FRandRange(.6f, 1.f), Rr * R.FRandRange(.6f, 1.6f)), 700 + I, Top - 60, 6, 10);
        }
        // Steam vents in the lava beside the route.
        const FVector VentAt[] = {FVector(2900, 420, 0), FVector(4300, -380, 0), FVector(6000, 330, 0), FVector(8900, 450, 0), FVector(10900, -380, 0),
                                  FVector(13900, 380, 0), FVector(16200, 250, 0), FVector(19800, 480, 0), FVector(22000, -520, 0), FVector(24600, 900, 0)};
        for (const FVector& V : VentAt) Vents.Add(V);
    }

    Emit(TEXT("PhlegLava"), Lava, LavaMat, false, false);
    Emit(TEXT("PhlegFalls"), Falls, LavaMat, false, false);
    Emit(TEXT("PhlegRock"), Rock, BasaltMat, true, true);
    Emit(TEXT("PhlegCrags"), RockVis, BasaltMat, false, true);
    if (auto* Solid = Emit(TEXT("PhlegCragSolid"), RockCol, DarkMat, true, false)) Solid->SetHiddenInGame(true);
    Emit(TEXT("PhlegPath"), Path, PathMat, true, true);
    Emit(TEXT("PhlegGlow"), Glow, GlowMat, false, false);
    Emit(TEXT("PhlegIron"), Iron, IronMat, true, true);
    // (The chains and their rings without collision, as the docks' lantern rings: the catch traces to the ring.)
    Emit(TEXT("PhlegChains"), Chain, IronMat, false, true);
    Emit(TEXT("PhlegFar"), Far, BasaltMat, false, false);
    if (Dark.T.Num()) Emit(TEXT("PhlegDark"), Dark, DarkMat, true, true);
    // Jars on the safe ground off the route (traced onto the rock, so after it stands).
    {
        const FVector Jars[] = {FVector(-400, -900, G), FVector(-900, 300, G), FVector(1200, 700, G), FVector(LakeX + 200, 300, G), FVector(LakeShoreX + 500, 200, G),
                                FVector(AscentX + 300, -250, G), FVector(26500, 900, FortressTop), FVector(27600, -1100, FortressTop)};
        int32 Placed = 0;
        for (const FVector& J : Jars)
        {
            FHitResult Hit;
            const FVector Top = PhlegWorld(J + FVector(0, 0, 200));
            if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 600), ECC_Visibility) && AClayJar::PlaceAt(GetWorld(), Hit.ImpactPoint + FVector(0, 0, 1), J.X * 2.f, 1)) ++Placed;
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_JARS placed=%d"), Placed);
    }

    for (FSpout& Sp : Spouts)
    {
        FPhlegSurf Col; Col.Paint = FColor(0, 200, 0, 255);
        // A column of molten rock, its UV running up it; scaled from nothing to full when it bursts.
        for (int32 K = 0; K < 10; ++K)
        {
            const float A0 = 2 * PI * K / 10, A1 = 2 * PI * (K + 1) / 10;
            const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
            for (int32 H = 0; H < 6; ++H)
            {
                const float Z0 = H * 100.f, Z1 = (H + 1) * 100.f, R0 = 22 + 30 * H / 6.f, R1 = 22 + 30 * (H + 1) / 6.f;
                const int32 I = Col.V.Num();
                Col.Vert(D0 * R0 + FVector(0, 0, Z0), D0, FVector2D(K / 4.f, -Z0 / 400)); Col.Vert(D1 * R0 + FVector(0, 0, Z0), D1, FVector2D((K + 1) / 4.f, -Z0 / 400));
                Col.Vert(D1 * R1 + FVector(0, 0, Z1), D1, FVector2D((K + 1) / 4.f, -Z1 / 400)); Col.Vert(D0 * R1 + FVector(0, 0, Z1), D0, FVector2D(K / 4.f, -Z1 / 400));
                Col.Face(I, I + 1, I + 2, D0 + D1); Col.Face(I, I + 2, I + 3, D0 + D1);
            }
        }
        auto* M = NewObject<UProceduralMeshComponent>(this); M->SetupAttachment(RootComponent); M->RegisterComponent();
        M->CreateMeshSection(0, Col.V, Col.T, Col.N, Col.UV, Col.C, Col.Tan, false);
        M->SetMaterial(0, LavaMat); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(false);
        M->SetWorldLocation(PhlegWorld(Sp.Base)); M->SetWorldScale3D(FVector(1, 1, .01f));
        // (leaning over the spine)
        M->SetWorldRotation(FRotator(0, 0, Sp.Side * 22.f));
        Sp.Mesh = M;
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(RootComponent); L->RegisterComponent();
        // (above the spine's top: 10 cm under it, the light leaked through the joints of its pieces in bright crosses)
        L->SetWorldLocation(PhlegWorld(Sp.Base + FVector(0, -Sp.Side * 60, 330)));
        L->SetIntensity(0.f); L->SetLightColor(FLinearColor(1.f, .35f, .08f)); L->SetAttenuationRadius(900.f); L->SetCastShadows(true);
        L->SetIntensityUnits(ELightUnits::Candelas);
        Sp.Light = L;
    }
    if (Sounds[RumbleLoop])
    {
        Rumble = UGameplayStatics::CreateSound2D(this, Sounds[RumbleLoop], 0.f, .55f, 0.f, nullptr, true, false);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_BUILT triangles=%d missing=%d movers=%d spouts=%d ms=%.0f"), Triangles, Missing, Movers.Num(), Spouts.Num(), (FPlatformTime::Seconds() - Started) * 1000);
}

void APhlegethosSetting::BuildEnvironment()
{
    const FVector G0 = PhlegethosOrigin();
    auto Attach = [this, G0](USceneComponent* C, FVector At = FVector::ZeroVector, FRotator Rot = FRotator::ZeroRotator)
    { C->SetupAttachment(RootComponent); C->SetWorldLocationAndRotation(G0 + At, Rot); C->RegisterComponent(); };
    // A smoke-choked sky: the sun a dull ember low through the ash, the air thick and red.
    // (2026-11-14, the user's goal art: no bright atmosphere, a sky of black smoke lit red from below.) A dome of M_PhlegSky
    // round the stage, which the sky light captures.
    {
        FPhlegSurf Dome;
        constexpr int32 Rings = 16, Segs = 48; constexpr float Rad = 4000000.f;
        auto Pt = [&](int32 I, int32 J) { const float Th = (PI * .5f) * (float(I) / Rings) * 1.08f, Ph = 2 * PI * J / Segs; return FVector(FMath::Cos(Ph) * FMath::Cos(Th), FMath::Sin(Ph) * FMath::Cos(Th), FMath::Sin(Th)) * Rad - FVector(0, 0, .1f * Rad); };
        for (int32 I = 0; I < Rings; ++I) for (int32 J = 0; J < Segs; ++J)
        {
            const FVector A = Pt(I, J), Bp = Pt(I + 1, J), Cc = Pt(I + 1, J + 1), D = Pt(I, J + 1);
            Dome.Flat(A, Bp, Cc, D, -((A + Cc) * .5f).GetSafeNormal());
        }
        UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegSky.M_PhlegSky"));
        if (auto* Mesh = Emit(TEXT("PhlegSky"), Dome, SkyMat, false, false))
        {
            Mesh->SetWorldLocation(G0 + FVector(14000, 0, 0)); Mesh->bNeverDistanceCull = true; Mesh->SetBoundsScale(4.f);
            if (SkyMat) { SkyMid = UMaterialInstanceDynamic::Create(SkyMat, this); Mesh->SetMaterial(0, SkyMid); }
        }
    }
    // Hell lightning: three bolt meshes (rebuilt each strike), and the flash's light across the land.
    if (UMaterialInterface* BoltMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegBolt.M_PhlegBolt")))
        for (int32 I = 0; I < 3; ++I)
        {
            auto* B = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("PhlegBolt%d"), I));
            B->SetupAttachment(RootComponent); B->RegisterComponent(); B->SetWorldLocation(G0);
            B->SetCollisionEnabled(ECollisionEnabled::NoCollision); B->SetCastShadow(false); B->bNeverDistanceCull = true;
            auto* Mid = UMaterialInstanceDynamic::Create(BoltMat, this); Mid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
            B->SetMaterial(0, Mid); B->SetVisibility(false);
            Bolts.Add(B); BoltMids.Add(Mid);
        }
    else ++Missing;
    FlashLight = NewObject<UDirectionalLightComponent>(this);
    FlashLight->SetMobility(EComponentMobility::Movable); FlashLight->SetAtmosphereSunLight(false);
    FlashLight->SetIntensity(0.f); FlashLight->SetLightColor(FLinearColor(1.f, .45f, .38f)); FlashLight->SetCastShadows(false);
    FlashLight->SetVolumetricScatteringIntensity(1.5f);
    Attach(FlashLight, FVector::ZeroVector, FRotator(-35, 0, 0));
    Sun = NewObject<UDirectionalLightComponent>(this);
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetAtmosphereSunLight(false);
    // (Dim: the light here is the lava's; a strong low sun striped the ground between the spires' shadows.)
    Sun->SetIntensity(.9f); Sun->SetLightColor(FLinearColor(1.f, .33f, .2f));
    Sun->DynamicShadowDistanceMovableLight = 8000; Sun->DynamicShadowCascades = 3;
    Sun->SetVolumetricScatteringIntensity(2.5f); Sun->SetForwardShadingPriority(1);
    Attach(Sun, FVector::ZeroVector, FRotator(-16, 140, 0));
    auto* SkyLight = NewObject<USkyLightComponent>(this);
    SkyLight->SetMobility(EComponentMobility::Movable); SkyLight->bRealTimeCapture = true;
    SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
    // (Its lower half a set dull glow: captured, the lake below lit every rock face from under like a lamp.)
    // (Dim: its bright orange horizon band lit every vertical face yellow-white, 2026-11-14.)
    SkyLight->SetIntensity(.9f);
    SkyLight->SetLowerHemisphereColor(FLinearColor(.16f, .045f, .015f));
    SkyLight->bLowerHemisphereIsBlack = true;
    Attach(SkyLight);
    auto* Fog = NewObject<UExponentialHeightFogComponent>(this);
    // (Thick low over the lake, thinning with height: from the fall's altitude the volcano stands clear of it.)
    // (2026-11-15: thinner. At .016 the low haze hid everything a few km off from his eye - the volcano and the ranges the
    // goal art shows, every lightning bolt - and the sky seen was mostly fog. The sky dome itself is past the fog's cutoff.)
    Fog->SetFogDensity(.0045f); Fog->SetFogHeightFalloff(.35f); Fog->SetFogMaxOpacity(.8f);
    Fog->FogCutoffDistance = 3000000.f;
    Fog->SetFogInscatteringColor(FLinearColor(.08f, .016f, .006f));
    Fog->SetDirectionalInscatteringColor(FLinearColor(.25f, .07f, .02f));
    Fog->SetStartDistance(600.f);
    Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogScatteringDistribution(.6f);
    Fog->SetVolumetricFogExtinctionScale(.5f);
    Fog->SetVolumetricFogDistance(6000.f);
    Fog->SetVolumetricFogAlbedo(FColor(255, 170, 140));
    Fog->SetVolumetricFogEmissive(FLinearColor(.012f, .003f, .001f));
    Attach(Fog, FVector(0, 0, -200));    auto* Post = NewObject<UPostProcessComponent>(this);
    Post->bUnbound = true; Post->Priority = 50.f;
    FPostProcessSettings& P = Post->Settings;
    P.bOverride_AutoExposureMethod = true; P.AutoExposureMethod = AEM_Histogram;
    // (Not raised far in the dark: adapting down to EV -1, every dark view was lifted till the lava and the sky burned out;
    // the goal art is dark with the lava bright in it.)
    P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = .3f;
    P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = 2.5f;
    P.bOverride_AutoExposureBias = true; P.AutoExposureBias = -.2f;
    P.bOverride_BloomIntensity = true; P.BloomIntensity = .85f;
    P.bOverride_BloomThreshold = true; P.BloomThreshold = 1.5f;
    P.bOverride_VignetteIntensity = true; P.VignetteIntensity = .5f;
    P.bOverride_FilmGrainIntensity = true; P.FilmGrainIntensity = .06f;
    P.bOverride_SceneFringeIntensity = true; P.SceneFringeIntensity = .3f;
    P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(1.05f, .98f, .95f, 1.05f);
    P.bOverride_ColorContrast = true; P.ColorContrast = FVector4(1, 1, 1, 1.08f);
    P.bOverride_ColorGain = true; P.ColorGain = FVector4(1.05f, .97f, .92f, 1.f);
    P.bOverride_AmbientOcclusionIntensity = true; P.AmbientOcclusionIntensity = .65f;
    Attach(Post);
    // The lava's light on him from beside and below (shadowed, so it never lights his fur through his head).
    UnderLight = NewObject<UPointLightComponent>(this);
    UnderLight->SetIntensityUnits(ELightUnits::Candelas);
    UnderLight->SetIntensity(55.f); UnderLight->SetLightColor(FLinearColor(1.f, .5f, .25f));
    UnderLight->SetAttenuationRadius(1400.f); UnderLight->SetCastShadows(true); UnderLight->SetSourceRadius(60.f);
    UnderLight->SetVolumetricScatteringIntensity(0.f);
    Attach(UnderLight, FVector(0, 0, LavaZ + 80));
    AheadLight = NewObject<UPointLightComponent>(this);
    AheadLight->SetIntensityUnits(ELightUnits::Candelas);
    AheadLight->SetIntensity(40.f); AheadLight->SetLightColor(FLinearColor(1.f, .32f, .08f));
    AheadLight->SetAttenuationRadius(2400.f); AheadLight->SetCastShadows(true); AheadLight->SetSourceRadius(150.f);
    AheadLight->SetVolumetricScatteringIntensity(0.f);
    Attach(AheadLight, FVector(400, 0, LavaZ + 60));
}

int32 APhlegethosSetting::GetMoverCount(int32 Kind) const { int32 N = 0; for (const FMover& M : Movers) N += M.Kind == Kind; return N; }
float APhlegethosSetting::GetMoverDrop(int32 Kind, int32 Index) const
{
    for (const FMover& M : Movers) if (M.Kind == Kind && Index-- == 0) return M.Drop;
    return 0.f;
}

void APhlegethosSetting::ResetHazards()
{
    for (FMover& M : Movers)
    {
        M.Touched = -1; M.Drop = 0; M.DownFor = 0; M.bCracked = false;
        if (auto* Mesh = M.Mesh.Get()) { Mesh->SetWorldLocation(PhlegethosOrigin()); Mesh->SetVisibility(true); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
    }
    Bombs.Reset(); BombBalls->ClearInstances(); BombMarks->ClearInstances();
    NextBomb = Clock + 2.f;
}

void APhlegethosSetting::Erupt(float Strength)
{
    EruptUntil = Clock + 6.f; EruptStrength = Strength;
    if (Sounds[Thunder2]) UGameplayStatics::PlaySound2D(this, Sounds[Thunder2], .9f * Strength, .55f);
    if (Sounds[Thunder0]) UGameplayStatics::PlaySound2D(this, Sounds[Thunder0], .8f * Strength, .7f);
}

void APhlegethosSetting::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    auto* C = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    TickMotes(Dt);
    TickLight(Dt, C);
    if (bLive) TickHazards(Dt, C);
}

void APhlegethosSetting::TickLight(float Dt, AChuckCharacter* C)
{
    if (!C || !UnderLight) return;
    const FVector L = PhlegLocal(C->GetActorLocation());
    // Beside him toward the nearer lava and below his feet, so the glow comes up across him.
    const float Side = FMath::Sin(Clock * .21f) > 0 ? 1.f : -1.f;
    // (Only where they are over the lava: under the road they shone up through the seams between its pieces, and set on
    // the rock they threw lamp-like pools on it. Over rock they fade out.)
    auto OverLava = [&](const FVector& P)
    {
        FHitResult Hit; FCollisionQueryParams Q(TEXT("PhlegUnder"), false, C);
        return !GetWorld()->LineTraceSingleByChannel(Hit, PhlegWorld(FVector(P.X, P.Y, L.Z + 150)), PhlegWorld(P), ECC_Visibility, Q);
    };
    const FVector Want = FVector(L.X + 80, L.Y + Side * 260, FMath::Max(LavaZ + 60, L.Z - 220));
    UnderOn = FMath::FInterpTo(UnderOn, OverLava(Want) ? 1.f : 0.f, Dt, 4.f);
    const FVector Now = PhlegLocal(UnderLight->GetComponentLocation());
    UnderLight->SetWorldLocation(PhlegWorld(FMath::VInterpTo(Now, Want, Dt, 1.5f)));
    const float Flicker = .85f + .1f * FMath::Sin(Clock * 7.3f) + .05f * FMath::Sin(Clock * 17.1f);
    UnderLight->SetIntensity(55.f * Flicker * (L.Z > G + 500 ? .6f : 1.f) * UnderOn);
    if (AheadLight)
    {
        const FVector Fwd = C->GetActorForwardVector();
        const FVector At = FVector(L.X, L.Y, LavaZ + 60) + Fwd * 700;
        AheadOn = FMath::FInterpTo(AheadOn, OverLava(At) ? 1.f : 0.f, Dt, 4.f);
        AheadLight->SetWorldLocation(PhlegWorld(At));
        AheadLight->SetIntensity(40.f * AheadOn);
    }
    // The volcano's eruption: the sky flushes red, the light pulses.
    if (Sun)
    {
        const float E = Clock < EruptUntil ? EruptStrength * FMath::Clamp((EruptUntil - Clock) / 6.f, 0.f, 1.f) : 0.f;
        Sun->SetIntensity(.9f + 3.f * E * (.7f + .3f * FMath::Sin(Clock * 9.f)));
    }
}

void APhlegethosSetting::TickMotes(float Dt)
{
    // Embers rising off the lake and ash falling, round the camera.
    APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!Cam || !Motes) return;
    const FVector Eye = Cam->GetCameraLocation();
    if (!InPhlegethos(Eye) && FVector::Dist(Eye, PhlegethosOrigin()) > 800000) { if (Motes->GetInstanceCount()) Motes->ClearInstances(); return; }
    TickSmoke(Dt, Eye);
    TickLightning(Dt, Eye);
    Dt = FMath::Min(Dt, .05f);
    constexpr int32 Embers = 260, Ashes = 220;
    FRandomStream R(FMath::Rand());
    int32 E = 0, A = 0;
    for (const FMote& M : MoteList) (M.Heat > 0 ? E : A) += 1;
    const FVector LEye = PhlegLocal(Eye);
    const float EruptK = Clock < EruptUntil ? 2.f : 1.f;
    while (E < Embers * EruptK)
    {
        FMote M; M.Heat = R.FRandRange(.4f, 1.f);
        M.P = LEye + FVector(R.FRandRange(-2500, 2500), R.FRandRange(-2500, 2500), 0); M.P.Z = R.FRandRange(LavaZ, LavaZ + 900);
        M.V = FVector(R.FRandRange(-30, 30), R.FRandRange(-30, 30), R.FRandRange(40, 140));
        M.Life = R.FRandRange(3, 8); M.Age = R.FRandRange(0, M.Life * .6f); M.Size = R.FRandRange(1.2f, 4.f);
        MoteList.Add(M); ++E;
    }
    while (A < Ashes)
    {
        FMote M; M.Heat = 0;
        M.P = LEye + FVector(R.FRandRange(-1800, 1800), R.FRandRange(-1800, 1800), R.FRandRange(-200, 900));
        M.V = FVector(R.FRandRange(20, 60), R.FRandRange(-20, 20), R.FRandRange(-60, -25));
        M.Life = R.FRandRange(5, 12); M.Age = R.FRandRange(0, M.Life * .6f); M.Size = R.FRandRange(2.f, 5.f);
        MoteList.Add(M); ++A;
    }
    TArray<FTransform> Xf; Xf.Reserve(MoteList.Num());
    TArray<float> Data; Data.Reserve(MoteList.Num());
    for (int32 I = MoteList.Num() - 1; I >= 0; --I)
    {
        FMote& M = MoteList[I];
        M.Age += Dt;
        const FVector Wander(FMath::Sin(Clock * 1.3f + I) * 25, FMath::Cos(Clock * 1.1f + I * .7f) * 25, 0);
        M.P += (M.V + Wander) * Dt;
        if (M.Age > M.Life || FVector::DistSquared2D(M.P, LEye) > FMath::Square(2800.f)) { MoteList.RemoveAtSwap(I); continue; }
        const float Fade = FMath::Clamp(M.Age / .6f, 0.f, 1.f) * FMath::Clamp((M.Life - M.Age) / 1.2f, 0.f, 1.f);
        const float Heat = M.Heat > 0 ? M.Heat * Fade * (1 - .6f * M.Age / M.Life) : 0.f;
        const FVector W = PhlegWorld(M.P);
        FVector Z = Eye - W; if (!Z.Normalize()) Z = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Z, X0);
        const float S = M.Size * (M.Heat > 0 ? 1.f : Fade) / 100.f;
        Xf.Add(FTransform(FMatrix(X0, Y0, Z, FVector::ZeroVector).ToQuat(), W, FVector(S)));
        Data.Add(M.Heat > 0 ? FMath::Max(Heat, .02f) : 0.f);
    }
    // (Ash renders dim: heat 0 is near-black additive, so it shows only against the glow.)
    if (Motes->GetInstanceCount() != Xf.Num())
    {
        Motes->ClearInstances();
        Motes->AddInstances(Xf, false, true);
    }
    else Motes->BatchUpdateInstancesTransforms(0, Xf, true, true);
    for (int32 I = 0; I < Data.Num(); ++I) Motes->SetCustomDataValue(I, 0, Data[I], I == Data.Num() - 1);
}

namespace
{
    // A bolt's path from A to B by midpoint displacement: 2^Depth segments, jagged at every scale.
    void BoltPath(FRandomStream& R, const FVector& A, const FVector& B, float Jag, int32 Depth, TArray<FVector>& Out)
    {
        Out = {A, B};
        for (int32 D = 0; D < Depth; ++D, Jag *= .9f)
        {
            TArray<FVector> Nx; Nx.Reserve(Out.Num() * 2);
            for (int32 I = 0; I + 1 < Out.Num(); ++I)
            {
                const FVector P = Out[I], Q = Out[I + 1], Dir = (Q - P).GetSafeNormal();
                FVector Side = FVector::CrossProduct(Dir, FVector::UpVector); if (!Side.Normalize()) Side = FVector::RightVector;
                const FVector Up2 = FVector::CrossProduct(Side, Dir);
                const float L = FVector::Dist(P, Q);
                Nx.Add(P); Nx.Add((P + Q) * .5f + (Side * R.FRandRange(-1, 1) + Up2 * R.FRandRange(-.6f, .6f)) * L * Jag);
            }
            Nx.Add(Out.Last()); Out = MoveTemp(Nx);
        }
    }
    // A ribbon along a path, turned to face the eye: UV V across it, width tapering to the tip; vertex colour R its strength.
    void BoltRibbon(FPhlegSurf& S, const TArray<FVector>& P, const FVector& Eye, float Width, float Taper, uint8 Strength)
    {
        S.Paint = FColor(Strength, 0, 0, 255);
        const int32 Base = S.V.Num(), M = P.Num();
        for (int32 I = 0; I < M; ++I)
        {
            const FVector T = (P[FMath::Min(I + 1, M - 1)] - P[FMath::Max(I - 1, 0)]).GetSafeNormal();
            const FVector ToEye = (Eye - P[I]).GetSafeNormal();
            FVector Side = FVector::CrossProduct(T, ToEye); if (!Side.Normalize()) Side = FVector::RightVector;
            const float W = Width * .5f * FMath::Lerp(1.f, Taper, float(I) / (M - 1));
            S.Vert(P[I] - Side * W, ToEye, FVector2D(I, 0)); S.Vert(P[I] + Side * W, ToEye, FVector2D(I, 1));
        }
        for (int32 I = 0; I + 1 < M; ++I)
        {
            const int32 A = Base + I * 2;
            S.T.Append({A, A + 1, A + 3}); S.T.Append({A, A + 3, A + 2});   // (M_PhlegBolt is two-sided)
        }
    }
}

void APhlegethosSetting::Strike(bool bPlume, float YawSpread)
{
    APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!Cam || Bolts.IsEmpty()) return;
    FRandomStream R(FMath::Rand());
    const FVector Eye = PhlegLocal(Cam->GetCameraLocation());
    FVector A, B; float Jag = .3f;
    if (bPlume)
    {
        // In the volcano's plume: the ash cloud over the crater crackling (a dirty thunderstorm).
        const FVector Crater(VolcanoAt.X, VolcanoAt.Y, VolcanoTop + 1500);
        A = Crater + FVector(R.FRandRange(-50000, 50000), R.FRandRange(-50000, 50000), R.FRandRange(60000, 200000));
        B = R.FRand() < .5f ? Crater + FVector(R.FRandRange(-25000, 25000), R.FRandRange(-25000, 25000), R.FRandRange(-4000, 15000))
                            : A + FVector(R.FRandRange(-90000, 90000), R.FRandRange(-90000, 90000), R.FRandRange(-90000, 0));
        Jag = .34f;
    }
    else
    {
        // Out across the smoke sky, ahead of the eye or to the sides: down to the land, or cloud to cloud.
        const float Yaw = FMath::DegreesToRadians(Cam->GetCameraRotation().Yaw + R.FRandRange(-YawSpread, YawSpread));
        // (Low under the smoke and within about 12 km: higher and further, they were off the top of his view and a pixel wide.)
        const float D = R.FRandRange(450000, 1200000);
        const FVector2D At = FVector2D(Eye.X, Eye.Y) + FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)) * D;
        A = FVector(At.X, At.Y, R.FRandRange(150000, 280000));
        if (R.FRand() < .6f) B = FVector(At.X + R.FRandRange(-50000, 50000), At.Y + R.FRandRange(-50000, 50000), R.FRandRange(0, 20000));
        else B = A + FVector(R.FRandRange(-150000, 150000), R.FRandRange(-150000, 150000), R.FRandRange(-50000, 10000));    }
    const float Dist = FVector::Dist(Eye, (A + B) * .5f);
    // (Wide and soft: about 12 px across with a white core of 3 or 4 - a real width of a few metres was smeared to
    // nothing by the temporal AA at any distance.)
    const float Width = FMath::Clamp(Dist * .016f, 1500.f, 25000.f);
    FPhlegSurf S;
    TArray<FVector> Trunk; BoltPath(R, A, B, Jag, 7, Trunk);
    BoltRibbon(S, Trunk, Eye, Width, .45f, 255);
    // Branches forking off the trunk, shorter and dimmer, heading on the trunk's way.
    const int32 Forks = R.RandRange(3, 6);
    for (int32 K = 0; K < Forks; ++K)
    {
        const int32 From = R.RandRange(Trunk.Num() / 8, Trunk.Num() * 3 / 4);
        const FVector Dir = (B - A).GetSafeNormal();
        FVector Off = FVector::CrossProduct(Dir, FVector(R.FRandRange(-1, 1), R.FRandRange(-1, 1), R.FRandRange(-1, 1))).GetSafeNormal();
        const float L = FVector::Dist(A, B) * R.FRandRange(.12f, .32f);
        TArray<FVector> Fork; BoltPath(R, Trunk[From], Trunk[From] + (Dir * .7f + Off * .8f).GetSafeNormal() * L, .32f, 5, Fork);
        BoltRibbon(S, Fork, Eye, Width * .55f, .1f, 110);
    }
    const int32 I = NextBolt++ % Bolts.Num();
    UProceduralMeshComponent* Mesh = Bolts[I];
    Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, false);
    Mesh->SetVisibility(true);
    FFlash F; F.Bolt = I;
    F.Dir = ((A + B) * .5f - Eye).GetSafeNormal();
    F.Power = FMath::Clamp(1.4f - Dist / 1400000.f, .35f, 1.2f);
    // Return strokes: two to four flickers in the first third of a second.
    const int32 Pulses = R.RandRange(2, 4);
    for (int32 K = 0; K < 4; ++K) { F.Pulse[K] = K == 0 ? 0.f : F.Pulse[K - 1] + R.FRandRange(.06f, .14f); F.Gain[K] = K < Pulses ? (K == 0 ? 1.f : R.FRandRange(.35f, .95f)) : 0.f; }
    F.Life = F.Pulse[Pulses - 1] + .35f;
    // The thunder after it (sped up: the real delay from 10 km is half a minute), deeper the nearer.
    F.Thunder = FMath::Clamp(Dist / 100000.f * .3f, .5f, 4.f);
    F.Sound = Dist < 700000.f ? Thunder2 : (R.FRand() < .5f ? Thunder0 : Thunder1);
    F.Volume = FMath::Clamp(1.1f - Dist / 1600000.f, .3f, .9f);
    Flashes.Add(F);
    ++Strikes;
    FVector2D Screen(-1, -1);
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ProjectWorldLocationToScreen(PhlegWorld((A + B) * .5f), Screen);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_STRIKE n=%d plume=%d dist_m=%.0f pulses=%d screen=%.0f,%.0f width_m=%.0f"), Strikes, bPlume ? 1 : 0, Dist / 100.f, Pulses, Screen.X, Screen.Y, Width / 100.f);
}

void APhlegethosSetting::TickLightning(float Dt, const FVector& Eye)
{
    const FVector LEye = PhlegLocal(Eye);
    // Now and then, more often on the ascent and while the volcano erupts.
    const bool bErupt = Clock < EruptUntil;
    if (Clock >= NextStrike)
    {
        if (NextStrike > 0.f) Strike(FMath::FRand() < (bErupt ? .75f : .35f));
        const bool bHigh = LEye.X > AscentX - 300;
        NextStrike = Clock + (bErupt ? FMath::FRandRange(1.2f, 3.5f) : bHigh ? FMath::FRandRange(3.5f, 8.f) : FMath::FRandRange(6.f, 13.f));
    }
    float Sky = 0.f; FVector SkyDir = FVector::ForwardVector;
    for (int32 K = Flashes.Num() - 1; K >= 0; --K)
    {
        FFlash& F = Flashes[K];
        F.Age += Dt;
        if (!F.bHeard && F.Age >= F.Thunder)
        {
            F.bHeard = true;
            if (Sounds.IsValidIndex(F.Sound) && Sounds[F.Sound]) UGameplayStatics::PlaySound2D(this, Sounds[F.Sound], F.Volume, FMath::FRandRange(.68f, .8f));
        }
        float Env = 0.f;
        for (int32 P = 0; P < 4; ++P) if (F.Age >= F.Pulse[P]) Env = FMath::Max(Env, F.Gain[P] * FMath::Exp(-(F.Age - F.Pulse[P]) / .05f));
        // (a faint afterglow so it never pops off)
        Env = FMath::Max(Env, .12f * FMath::Clamp(1.f - F.Age / F.Life, 0.f, 1.f));
        const bool bLit = F.Age < F.Life;
        if (Bolts.IsValidIndex(F.Bolt))
        {
            BoltMids[F.Bolt]->SetScalarParameterValue(TEXT("Intensity"), bLit ? Env * 70.f : 0.f);
            if (!bLit) Bolts[F.Bolt]->SetVisibility(false);
        }
        if (bLit && Env * F.Power > Sky) { Sky = Env * F.Power; SkyDir = F.Dir; }
        if (!bLit && F.bHeard) Flashes.RemoveAtSwap(K);
    }
    if (SkyMid)
    {
        SkyMid->SetScalarParameterValue(TEXT("Flash"), Sky);
        SkyMid->SetVectorParameterValue(TEXT("FlashDir"), FLinearColor(SkyDir.X, SkyDir.Y, SkyDir.Z, 0.f));
    }
    if (FlashLight)
    {
        FlashLight->SetIntensity(Sky * 1.4f);
        if (Sky > 0.f) FlashLight->SetWorldRotation(FRotator(-FMath::Clamp(FMath::RadiansToDegrees(FMath::Asin(SkyDir.Z)) + 10.f, 18.f, 60.f), (-SkyDir).Rotation().Yaw, 0.f));
    }
}

void APhlegethosSetting::TickSmoke(float Dt, const FVector& Eye)
{
    if (!Smoke) return;
    Dt = FMath::Min(Dt, .05f);
    FRandomStream R(FMath::Rand());
    const FVector LEye = PhlegLocal(Eye);
    // Steam: from the vents within 60 m, low and slow, rising and spreading.
    VentCarry += Dt * 1.3f;
    while (VentCarry >= 1.f)
    {
        VentCarry -= 1.f;
        for (const FVector& V : Vents)
        {
            if (FVector::DistSquared2D(V, LEye) > FMath::Square(6000.f) || R.FRand() > .55f) continue;
            FPuff Pf; Pf.P = V + FVector(R.FRandRange(-60, 60), R.FRandRange(-60, 60), 20); Pf.V = FVector(R.FRandRange(-15, 15), R.FRandRange(-15, 15), R.FRandRange(50, 90));
            Pf.Life = R.FRandRange(6, 10); Pf.Size0 = R.FRandRange(120, 220); Pf.Size1 = R.FRandRange(600, 1000); Pf.Frame = R.FRandRange(0, 64); Pf.Heat = .6f; Pf.Spin = R.FRandRange(0, 6.28f);
            Puffs.Add(Pf);
        }
    }
    // The volcano's plume: great billows from the crater, more while it erupts.
    const FVector Crater(VolcanoAt.X, VolcanoAt.Y, VolcanoTop + 1500);
    PlumeCarry += Dt * (Clock < EruptUntil ? 7.f : 3.f);
    while (PlumeCarry >= 1.f)
    {
        PlumeCarry -= 1.f;
        // (a column a few hundred metres across rising a kilometre and more, leaning off downwind, as the goal art's; many
        // smaller puffs, as a few huge ones read as one dark ball over the crater)
        FPuff Pf; Pf.P = Crater + FVector(R.FRandRange(-10000, 10000), R.FRandRange(-10000, 10000), 0);
        Pf.V = FVector(R.FRandRange(300, 1400), R.FRandRange(-400, 400), R.FRandRange(2600, 4200) * (Clock < EruptUntil ? 1.6f : 1.f));
        Pf.Life = R.FRandRange(30, 45); Pf.Size0 = R.FRandRange(14000, 26000); Pf.Size1 = R.FRandRange(70000, 130000); Pf.Frame = R.FRandRange(0, 64); Pf.Heat = 1.f; Pf.Spin = R.FRandRange(0, 6.28f);
        Puffs.Add(Pf);
    }
    TArray<FTransform> Xf; Xf.Reserve(Puffs.Num());
    TArray<float> Data; Data.Reserve(Puffs.Num() * 3);
    for (int32 I = Puffs.Num() - 1; I >= 0; --I)
    {
        FPuff& Pf = Puffs[I];
        Pf.Age += Dt;
        if (Pf.Age > Pf.Life || Puffs.Num() > 600) { Puffs.RemoveAtSwap(I); continue; }
        const float A = Pf.Age / Pf.Life;
        // (The plume's puffs keep rising, buoyant, and drift downwind: slowed like the steam, they climbed 280 m while growing
        // to a kilometre and piled into a dark ball over the crater.)
        if (Pf.Size0 > 5000) { Pf.V.X = FMath::FInterpTo(Pf.V.X, 1800.f, Dt, .15f); Pf.V.Z *= FMath::Exp(-.02f * Dt); }
        else Pf.V *= FMath::Exp(-.12f * Dt);
        Pf.P += Pf.V * Dt; Pf.Frame += Pf.Rate * Dt;
        const FVector W = PhlegWorld(Pf.P);
        FVector Z = Eye - W; if (!Z.Normalize()) Z = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Z, X0);
        float Sn, Cs; FMath::SinCos(&Sn, &Cs, Pf.Spin + Pf.Age * .05f);
        const float Size = FMath::Lerp(Pf.Size0, Pf.Size1, FMath::Sqrt(A)) / 100.f;
        Xf.Add(FTransform(FMatrix(X0 * Cs + Y0 * Sn, Y0 * Cs - X0 * Sn, Z, FVector::ZeroVector).ToQuat(), W, FVector(Size)));
        const float Opacity = FMath::Clamp(A / .12f, 0.f, 1.f) * FMath::Clamp((1 - A) / .4f, 0.f, 1.f) * (Pf.Size0 > 5000 ? 1.f : .55f);
        Data.Append({Pf.Frame, Pf.Heat * (1 - A), Opacity});
    }
    if (Smoke->GetInstanceCount() != Xf.Num()) { Smoke->ClearInstances(); Smoke->AddInstances(Xf, false, true); }
    else Smoke->BatchUpdateInstancesTransforms(0, Xf, true, true);
    for (int32 I = 0; I < Xf.Num(); ++I) for (int32 K = 0; K < 3; ++K) Smoke->SetCustomDataValue(I, K, Data[I * 3 + K], I == Xf.Num() - 1 && K == 2);
}

void APhlegethosSetting::TickHazards(float Dt, AChuckCharacter* C)
{
    if (!C) return;
    const bool bAstral = C->IsAstral();
    // A death (or a summons back) puts everything back.
    if (bWasAstral && !bAstral) ResetHazards();
    bWasAstral = bAstral;
    const FVector Body = PhlegLocal(C->GetActorLocation());
    const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Feet = Body - FVector(0, 0, Half);
    const bool bGrounded = C->GetCharacterMovement() && C->GetCharacterMovement()->IsMovingOnGround();

    // ---- sinking columns and crumbling steps
    for (FMover& M : Movers)
    {
        UProceduralMeshComponent* Mesh = M.Mesh.Get();
        if (!Mesh) continue;
        const bool bOn = !bAstral && bGrounded && FVector::Dist2D(Feet, M.Top) < M.Half + 15 && FMath::Abs(Feet.Z - (M.Top.Z - M.Drop)) < 25;
        if (bOn && M.Touched < 0) { M.Touched = Clock; }
        if (M.Touched >= 0)
        {
            const float Since = Clock - M.Touched;
            if (M.Kind == 0 && Since > SinkDelay)
            {
                if (M.Drop == 0 && Sounds[Fall0]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[Fall0], PhlegWorld(M.Top), .5f, .6f);
                if (M.Drop == 0) ++SunkCount;
                M.Drop = FMath::Min(M.Drop + SinkSpeed * Dt * FMath::Min(1.f, (Since - SinkDelay) * 2.f), M.Top.Z + 80.f);
            }
            if (M.Kind == 1)
            {
                if (!M.bCracked && Since > StepCrack) { M.bCracked = true; if (Sounds[Crack0]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[(M.Top.X > 23000) ? Crack1 : Crack0], PhlegWorld(M.Top), .7f); }
                if (Since > StepFall)
                {
                    if (M.Drop == 0) { ++CrumbledCount; if (Sounds[Fall1]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[Fall1], PhlegWorld(M.Top), .8f); }
                    const float T = Since - StepFall;
                    M.Drop = FMath::Min(490.f * T * T + 20.f * T, M.Top.Z + 120.f);
                }
            }
            // Shaking before it goes.
            FVector Shake = FVector::ZeroVector;
            if ((M.Kind == 0 && Since < SinkDelay + .4f) || (M.Kind == 1 && M.bCracked && Since < StepFall))
                Shake = FVector(FMath::Sin(Clock * 61.f), FMath::Cos(Clock * 53.f), 0) * (M.Kind == 1 ? 2.2f : 1.2f);
            Mesh->SetWorldLocation(PhlegethosOrigin() + Shake - FVector(0, 0, M.Drop));
            // Down for good once under; back up when he is well away (and not looking straight at it).
            if (M.Drop >= M.Top.Z + 60.f)
            {
                M.DownFor += Dt;
                if (Mesh->IsCollisionEnabled()) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                if (M.DownFor > 8.f && FVector::Dist2D(Body, M.Top) > 1500.f)
                {
                    M.Touched = -1; M.Drop = 0; M.DownFor = 0; M.bCracked = false;
                    Mesh->SetWorldLocation(PhlegethosOrigin()); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                }
            }
        }
    }

    // ---- geysers on the lava road
    for (FSpout& Sp : Spouts)
    {
        const float U = FMath::Fmod(Clock + Sp.Phase, GeyserPeriod);
        const int32 Stage = U < GeyserPeriod - GeyserWarn - GeyserBurn ? 0 : U < GeyserPeriod - GeyserBurn ? 1 : 2;
        const float Near = FVector::Dist(Body, Sp.Base);
        if (Stage != Sp.Stage && Near < 2500)
        {
            if (Stage == 1 && Sounds[GeyserHiss]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[GeyserHiss], PhlegWorld(Sp.Base), .55f, .7f);
            if (Stage == 2 && Sounds[GeyserRoar]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[GeyserRoar], PhlegWorld(Sp.Base), .9f, .75f);
        }
        Sp.Stage = Stage;
        float H = 0;
        if (Stage == 1) H = .05f + .04f * FMath::Sin(Clock * 30.f);                  // bubbling up
        if (Stage == 2)
        {
            const float B = (U - (GeyserPeriod - GeyserBurn)) / GeyserBurn;         // 0..1 over the burst
            H = FMath::Clamp(B * 6.f, 0.f, 1.f) * FMath::Clamp((1 - B) * 4.f, 0.f, 1.f) * (1 + .08f * FMath::Sin(Clock * 40.f));
        }
        if (auto* M = Sp.Mesh.Get()) M->SetWorldScale3D(FVector(.8f + .4f * H, .8f + .4f * H, FMath::Max(.01f, H * .8f)));
        if (auto* L = Sp.Light.Get()) L->SetIntensity(H * 900.f);
        // The spray comes down across the spine: a burn if he is in it.
        if (Stage == 2 && H > .6f && !bAstral)
        {
            const FVector Hit = Sp.Base + FVector(0, -Sp.Side * GeyserReach, G - Sp.Base.Z);
            if (FMath::Abs(Body.X - Hit.X) < GeyserSpread && FMath::Abs(Body.Y - Hit.Y) < 200 && Body.Z < G + 260)
                // (Knocked back along the spine, not across it: a burn should not throw him into the lava.)
                if (C->TakeBite(PhlegWorld(Body + FVector(90, 0, 0)), 1)) ++GeyserHits;
        }
    }

    // ---- lava bombs on the ascent's ramp and steps
    // (On the ramp only: over the crumbling steps a blast knocked him back onto a fallen one.)
    const bool bBombing = !bAstral && Body.X > RampX0 - 200 && Body.X < RampX1 - 60;
    if (bBombing && Clock >= NextBomb)
    {
        // Aimed at where he will be when it comes down, with some lead and some scatter; one in three straight at him.
        FRandomStream R(int32(Clock * 1000));
        const FVector V = C->GetVelocity();
        FBomb Bm; Bm.Flight = R.FRandRange(1.6f, 2.1f);
        FVector Aim = Body + FVector(V.X, V.Y, 0) * Bm.Flight * R.FRandRange(.5f, 1.f);
        if (R.FRand() > .34f) Aim += FVector(R.FRandRange(-260, 260), R.FRandRange(-200, 200), 0);
        FHitResult Hit; const FVector Top = PhlegWorld(FVector(Aim.X, Aim.Y, RampTop + 600));
        FCollisionQueryParams Q(TEXT("PhlegBomb"), false, C);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 1200), ECC_Visibility, Q)) Aim = PhlegLocal(Hit.ImpactPoint);
        else Aim.Z = LavaZ;
        Bm.To = Aim; Bm.From = Aim + FVector(-3000, -1600, 4200);
        Bombs.Add(Bm); ++BombsFired;
        NextBomb = Clock + R.FRandRange(1.1f, 1.9f);
        if (BombsFired % 4 == 1) Erupt(.6f);
    }
    TArray<FTransform> Balls, Marks;
    for (int32 I = Bombs.Num() - 1; I >= 0; --I)
    {
        FBomb& Bm = Bombs[I];
        Bm.Age += Dt;
        const float U = FMath::Clamp(Bm.Age / Bm.Flight, 0.f, 1.f);
        if (!Bm.bLanded)
        {
            FVector P = FMath::Lerp(Bm.From, Bm.To, U); P.Z += 1500.f * FMath::Sin(PI * U) * (1 - U);
            Balls.Add(FTransform(FRotator(Clock * 200, Clock * 150, 0), PhlegWorld(P), FVector(.5f)));
            // The mark on the ground: a ring of glow growing as it comes.
            const float K = .3f + .9f * U;
            Marks.Add(FTransform(FRotator::ZeroRotator, PhlegWorld(Bm.To + FVector(0, 0, 2)), FVector(1.1f * K, 1.1f * K, .02f)));
            if (U >= 1)
            {
                Bm.bLanded = true;
                const float D = FVector::Dist(Body, Bm.To + FVector(0, 0, Half));
                if (Sounds[Slam0]) UGameplayStatics::PlaySoundAtLocation(this, Sounds[(I & 1) ? Slam1 : Slam0], PhlegWorld(Bm.To), .8f, .8f);
                if (D < 120 && !bAstral && C->TakeBite(PhlegWorld(Body + FVector(90, 0, 0)), 1)) ++BombHits;
            }
        }
        else
        {
            Bm.After += Dt;
            if (Bm.After < .6f) Balls.Add(FTransform(FRotator::ZeroRotator, PhlegWorld(Bm.To), FVector(.5f + Bm.After * 2.f, .5f + Bm.After * 2.f, .2f)));
            if (Bm.After > .6f) Bombs.RemoveAtSwap(I);
        }
    }
    BombBalls->ClearInstances(); if (Balls.Num()) BombBalls->AddInstances(Balls, false, true);
    BombMarks->ClearInstances(); if (Marks.Num()) BombMarks->AddInstances(Marks, false, true);
}
