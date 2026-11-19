#include "PhlegethosBleed.h"
#include "TempleBeholder.h"
#include "DockGameMode.h"
#include "PhlegethosSetting.h"
#include "TempleSanctumBattle.h"
#include "TempleHero.h"
#include "TempleSkeleton.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

// The Feywild bleedthrough (Claude, 2026-11-17; PhlegethosBleed.h, docs/regions/PHLEGETHOS.md#the-feywild-bleedthrough).
// Everything here is in Phlegethos's stage frame (PhlegethosSetting.h), placed at PhlegethosOrigin.

bool APhlegBleed::bHold = false;

namespace
{
    TWeakObjectPtr<APhlegBleed> TheBleed;
    float BleedNoise(float X, float Y, float Scale, float Seed) { return FMath::PerlinNoise2D(FVector2D(X / Scale + Seed * 3.71f + .31f, Y / Scale - Seed * 1.93f + .77f)); }
    float BleedEase(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3 - 2 * T); }
    float BleedHash(float A, float B) { return FMath::Frac(FMath::Sin(A * 127.1f + B * 311.7f) * 43758.5453f); }
    // The wastes' heightfield's colouring (PhlegethosSetting.cpp's PhlegWastes), so the slabs match the ground round them:
    // R hot cracks, G the second basalt, B ash.
    FColor BleedBasalt(float X, float Y, float H)
    {
        const float Dp = FMath::Abs(Y - Phleg::ApproachPathY(X));
        const float Hot = FMath::Max(1 - BleedEase((H - Phleg::LavaZ - 40) / 260.f), .8f * FMath::SmoothStep(.3f, .6f, BleedNoise(X, Y, 1900, 129))) * BleedEase((Dp - 300) / 500.f);
        const float Ash = .5f * FMath::SmoothStep(.1f, .5f, BleedNoise(X, Y, 1500, 130)) * (1 - BleedEase((Dp - 900) / 900.f));
        return FColor(uint8(255 * FMath::Clamp(Hot, 0.f, 1.f)), uint8(128 + 110 * BleedNoise(X, Y, 3000, 131)), uint8(255 * FMath::Clamp(Ash, 0.f, 1.f)), 255);
    }
    // A mesh being assembled (the stage's FPhlegSurf is private to its file).
    struct FBleedMesh
    {
        TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C; TArray<FProcMeshTangent> Tan;
        int32 Vert(const FVector& P, const FVector& Nn, const FVector2D& Uv, const FColor& Col)
        {
            V.Add(P); N.Add(Nn); UV.Add(Uv); C.Add(Col);
            const FVector Ax = FMath::Abs(Nn.Z) > .95f ? FVector::ForwardVector : FVector::UpVector;
            Tan.Add(FProcMeshTangent(FVector::CrossProduct(Ax, Nn).GetSafeNormal(), false));
            return V.Num() - 1;
        }
        void Tri(int32 A, int32 B, int32 Cc, const FVector& Want)
        {
            const FVector F = FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]);
            if (FVector::DotProduct(F, Want) >= 0) T.Append({A, Cc, B}); else T.Append({A, B, Cc});
        }
        void Quad(int32 A, int32 B, int32 Cc, int32 D, const FVector& Want) { Tri(A, B, Cc, Want); Tri(A, Cc, D, Want); }
        void Flat(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Out, const FColor& Col)
        {
            const int32 I = Vert(A, Out, FVector2D(A.X + A.Y, -A.Z) / 400, Col), J = Vert(B, Out, FVector2D(B.X + B.Y, -B.Z) / 400, Col);
            const int32 K = Vert(Cc, Out, FVector2D(Cc.X + Cc.Y, -Cc.Z) / 400, Col), L = Vert(D, Out, FVector2D(D.X + D.Y, -D.Z) / 400, Col);
            Quad(I, J, K, L, Out);
        }
    };
    enum EBleedSound { SndCrack0, SndCrack1, SndFall0, SndFall1, SndRumble, SndTear, SndRiver, SndSplash0, SndSplash1, SndVanish, SndCount };
    const TCHAR* BleedSoundNames[SndCount] = {TEXT("SFX_TempleSlabCrack_00"), TEXT("SFX_TempleSlabCrack_01"), TEXT("SFX_TempleSlabFall_00"), TEXT("SFX_TempleSlabFall_01"),
        TEXT("SFX_StormThunderNear_01"), TEXT("SFX_AstralSummon_00"), TEXT("SFX_ShipWater_00"), TEXT("SFX_StreamSplash_02"), TEXT("SFX_StreamSplash_04"), TEXT("SFX_AstralVanish_00")};
    constexpr float BleedRiverXc = (Phleg::RiverX0 + Phleg::RiverX1) * .5f;
    constexpr float BleedTearInY = Phleg::IslandY1 + 150.f, BleedTearOutY = Phleg::IslandY0 - 160.f;
}

APhlegBleed::APhlegBleed()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

APhlegBleed* APhlegBleed::Get(const UWorld* World)
{
    APhlegBleed* B = TheBleed.Get();
    return B && B->GetWorld() == World ? B : nullptr;
}

float APhlegBleed::GetWaterZ() const { return Phleg::FortressTop + WaterDepth; }

void APhlegBleed::Build(APhlegethosSetting* InStage)
{
    using namespace Phleg;
    Setting = InStage;
    TheBleed = this;
    const FVector Origin = PhlegethosOrigin();
    UMaterialInterface* Basalt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegBasalt.M_PhlegBasalt"));
    UMaterialInterface* PathMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegPath.M_PhlegPath"));
    UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegGlow.M_PhlegGlow"));
    UMaterialInterface* Depth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_AstralDepth.M_AstralDepth"));
    UMaterialInterface* RiverMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_FeyRiver.M_FeyRiver"));
    UMaterialInterface* TearMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_FeyTear.M_FeyTear"));
    Missing = !Basalt + !Glow + !Depth + !RiverMat + !TearMat + !PathMat;
    for (int32 I = 0; I < SndCount; ++I)
        Sounds.Add(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), BleedSoundNames[I], BleedSoundNames[I])));
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true; Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 3500.f;

    // ---- The slabs: a grid on the field, its inner corners jittered (the river's edges kept straight along X, the field's
    // edge straight, as the ground round it is cut).
    const int32 NX = FieldCols + 1, NY = FieldRows + 1;
    TArray<FVector2D> Grid; Grid.SetNum(NX * NY);
    FRandomStream R(117);
    for (int32 J = 0; J < NY; ++J) for (int32 I = 0; I < NX; ++I)
    {
        float X = FieldX0 + I * FieldColW, Y = FieldY0 + J * FieldRowW;
        const bool bEdgeX = I == 0 || I == NX - 1, bEdgeY = J == 0 || J == NY - 1;
        const bool bRiverEdge = I == RiverCol0 || I == RiverCol1 + 1;
        // (a corner of the kept ground round the breach stays on the grid: the ground's cut follows the grid)
        const bool bKeptCorner = FieldKept(I - 1, J - 1) || FieldKept(I, J - 1) || FieldKept(I - 1, J) || FieldKept(I, J);
        if (!bEdgeX && !bRiverEdge && !bEdgeY && !bKeptCorner) X += R.FRandRange(-55.f, 55.f);
        if (!bEdgeY && !bEdgeX && !bKeptCorner) Y += R.FRandRange(-60.f, 60.f);
        Grid[J * NX + I] = FVector2D(X, Y);
    }
    CellSlab.Init(-1, FieldCols * FieldRows);
    auto Ground = [](float X, float Y) { return ApproachGround(X, Y) + .5f; };
    constexpr float Thick = 160.f, Joint = 1.6f;
    const FColor Violet(175, 95, 255, 255);
    for (int32 Col = 0; Col < FieldCols; ++Col) for (int32 Row = 0; Row < FieldRows; ++Row)
    {
        if (FieldKept(Col, Row)) { ++KeptCells; continue; }   // (the ground round the breach)
        FVector2D Q[4] = {Grid[Row * NX + Col], Grid[Row * NX + Col + 1], Grid[(Row + 1) * NX + Col + 1], Grid[(Row + 1) * NX + Col]};
        FVector2D Cen(0, 0); for (const FVector2D& P : Q) Cen += P; Cen /= 4.f;
        for (FVector2D& P : Q) P += (Cen - P).GetSafeNormal() * Joint;
        auto At = [&](float S, float Tt) { return FMath::Lerp(FMath::Lerp(Q[0], Q[1], S), FMath::Lerp(Q[3], Q[2], S), Tt); };
        FBleedMesh M;
        // The top, following the ground.
        constexpr int32 Sub = 3;
        float Zmin = 1e9f;
        for (int32 Tj = 0; Tj <= Sub; ++Tj) for (int32 Si = 0; Si <= Sub; ++Si)
        {
            const FVector2D P = At(float(Si) / Sub, float(Tj) / Sub);
            const float Z = Ground(P.X, P.Y); Zmin = FMath::Min(Zmin, Z);
            M.Vert(FVector(P.X, P.Y, Z), FVector::UpVector, FVector2D(P.X, P.Y) / 400, BleedBasalt(P.X, P.Y, Z));
        }
        for (int32 Tj = 0; Tj < Sub; ++Tj) for (int32 Si = 0; Si < Sub; ++Si)
        {
            const int32 A = Tj * (Sub + 1) + Si;
            M.Quad(A, A + 1, A + Sub + 2, A + Sub + 1, FVector::UpVector);
        }
        // Its sides, down into the dark.
        const float Bottom = Zmin - Thick;
        for (int32 E = 0; E < 4; ++E)
        {
            const FVector2D A = Q[E], B = Q[(E + 1) % 4];
            FVector2D Out(B.Y - A.Y, A.X - B.X); Out.Normalize();
            if (FVector2D::DotProduct(Out, (A + B) * .5f - Cen) < 0) Out = -Out;
            const FVector On(Out.X, Out.Y, 0);
            for (int32 K = 0; K < Sub; ++K)
            {
                const FVector2D P0 = FMath::Lerp(A, B, float(K) / Sub), P1 = FMath::Lerp(A, B, float(K + 1) / Sub);
                const FColor Cs = BleedBasalt(P0.X, P0.Y, Bottom);
                M.Flat(FVector(P0.X, P0.Y, Ground(P0.X, P0.Y)), FVector(P1.X, P1.Y, Ground(P1.X, P1.Y)), FVector(P1.X, P1.Y, Bottom), FVector(P0.X, P0.Y, Bottom), On, Cs);
            }
        }
        // The cracks it shows before it goes (hidden till then): the Astral's light along its edges and in three
        // splitting lines from near its middle out to them.
        FBleedMesh K;
        auto Strip = [&](const FVector2D& A, const FVector2D& B, float W)
        {
            const FVector2D D = (B - A).GetSafeNormal(), Sd(-D.Y, D.X);
            auto P = [&](const FVector2D& Q2) { return FVector(Q2.X, Q2.Y, Ground(Q2.X, Q2.Y) + .45f); };
            const int32 I0 = K.Vert(P(A - Sd * W), FVector::UpVector, FVector2D(0, 0), Violet), I1 = K.Vert(P(B - Sd * W), FVector::UpVector, FVector2D(1, 0), Violet);
            const int32 I2 = K.Vert(P(B + Sd * W), FVector::UpVector, FVector2D(1, 1), Violet), I3 = K.Vert(P(A + Sd * W), FVector::UpVector, FVector2D(0, 1), Violet);
            K.Quad(I0, I1, I2, I3, FVector::UpVector);
        };
        for (int32 E = 0; E < 4; ++E)
        {
            const FVector2D A = Q[E] + (Cen - Q[E]).GetSafeNormal() * 2.f, B = Q[(E + 1) % 4] + (Cen - Q[(E + 1) % 4]).GetSafeNormal() * 2.f;
            Strip(A, B, 1.6f);
        }
        const FVector2D Start = Cen + FVector2D(R.FRandRange(-25.f, 25.f), R.FRandRange(-25.f, 25.f));
        for (int32 L = 0; L < 3; ++L)
        {
            const int32 E = (L + Col + Row) % 4;
            const FVector2D End = FMath::Lerp(Q[E], Q[(E + 1) % 4], R.FRandRange(.25f, .75f));
            FVector2D P = Start;
            for (int32 Sg = 1; Sg <= 5; ++Sg)
            {
                const FVector2D D = End - Start, Sd(-D.Y, D.X);
                FVector2D Nx = Start + D * (Sg / 5.f) + (Sg < 5 ? Sd.GetSafeNormal() * R.FRandRange(-18.f, 18.f) : FVector2D::ZeroVector);
                Strip(P, Nx, 1.4f - .15f * Sg);
                P = Nx;
            }
        }
        auto* Mesh = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("BleedSlab_%d_%d"), Col, Row));
        Mesh->SetupAttachment(RootComponent);
        Mesh->bUseComplexAsSimpleCollision = true;
        Mesh->RegisterComponent();
        Mesh->CreateMeshSection(0, M.V, M.T, M.N, M.UV, M.C, M.Tan, true);
        Mesh->CreateMeshSection(1, K.V, K.T, K.N, K.UV, K.C, K.Tan, false);
        // (the processional's two rows in its flagstones, to the gate)
        Mesh->SetMaterial(0, (Row == FieldRows / 2 - 1 || Row == FieldRows / 2) && PathMat ? PathMat : Basalt); Mesh->SetMaterial(1, Glow);
        Mesh->SetMeshSectionVisible(1, false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->ComponentTags.Add(TEXT("Phlegethos"));
        FSlab S;
        S.Mesh = Mesh; S.Col = Col; S.Row = Row;
        S.Centre = FVector(Cen.X, Cen.Y, Ground(Cen.X, Cen.Y));
        S.Zone = Col < MoatCols ? EZone::MoatBack : Col >= FieldCols - MoatFrontCols ? EZone::MoatFront : (Row < MoatRows || Row >= FieldRows - MoatRows) ? EZone::MoatSide
               : (Col == RiverCol0 || Col == RiverCol1) ? EZone::River : EZone::Land;
        CellSlab[Col * FieldRows + Row] = Slabs.Num();
        Slabs.Add(S);
        PlaceSlab(Slabs.Last());
    }

    // ---- The Astral under the field: its floor, and its walls up to the ground round the field's edge (unlit, as the
    // breach's).
    {
        FBleedMesh M;
        const FColor W(255, 255, 255, 255);
        auto Wall = [&](const FVector2D& A, const FVector2D& B, const FVector& In)
        {
            const int32 NSteps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 120.f));
            for (int32 K = 0; K < NSteps; ++K)
            {
                const FVector2D P0 = FMath::Lerp(A, B, float(K) / NSteps), P1 = FMath::Lerp(A, B, float(K + 1) / NSteps);
                M.Flat(FVector(P0.X, P0.Y, ApproachGround(P0.X, P0.Y) - 2.f), FVector(P1.X, P1.Y, ApproachGround(P1.X, P1.Y) - 2.f), FVector(P1.X, P1.Y, VoidZ), FVector(P0.X, P0.Y, VoidZ), In, W);
            }
        };
        Wall(FVector2D(FieldX0, FieldY0), FVector2D(FieldX1, FieldY0), FVector(0, 1, 0));
        Wall(FVector2D(FieldX0, FieldY1), FVector2D(FieldX1, FieldY1), FVector(0, -1, 0));
        Wall(FVector2D(FieldX0, FieldY0), FVector2D(FieldX0, FieldY1), FVector(1, 0, 0));
        Wall(FVector2D(FieldX1, FieldY0), FVector2D(FieldX1, FieldY1), FVector(-1, 0, 0));
        // (and round the kept ground by the breach, where it meets a slab)
        for (int32 Col = 0; Col < FieldCols; ++Col) for (int32 Row = 0; Row < FieldRows; ++Row)
        {
            if (!FieldKept(Col, Row)) continue;
            const float X0 = FieldX0 + Col * FieldColW, X1 = X0 + FieldColW, Y0 = FieldY0 + Row * FieldRowW, Y1 = Y0 + FieldRowW;
            if (Col > 0 && !FieldKept(Col - 1, Row)) Wall(FVector2D(X0, Y0), FVector2D(X0, Y1), FVector(-1, 0, 0));
            if (Col < FieldCols - 1 && !FieldKept(Col + 1, Row)) Wall(FVector2D(X1, Y0), FVector2D(X1, Y1), FVector(1, 0, 0));
            if (Row > 0 && !FieldKept(Col, Row - 1)) Wall(FVector2D(X0, Y0), FVector2D(X1, Y0), FVector(0, -1, 0));
            if (Row < FieldRows - 1 && !FieldKept(Col, Row + 1)) Wall(FVector2D(X0, Y1), FVector2D(X1, Y1), FVector(0, 1, 0));
        }
        // Where a death in it returns him: the kept ground nearest the river, clear of the breach's lip.
        float BestX = 1e9f;
        for (int32 Col = 0; Col < FieldCols; ++Col) for (int32 Row = 0; Row < FieldRows; ++Row)
        {
            if (!FieldKept(Col, Row)) continue;
            for (int32 I = 0; I < 5; ++I) for (int32 J = 0; J < 5; ++J)
            {
                const float X = FieldX0 + (Col + .1f + .2f * I) * FieldColW, Y = FieldY0 + (Row + .1f + .2f * J) * FieldRowW;
                if (BreachInside(X, Y) < -110.f && X < BestX) { BestX = X; RespawnLocal = FVector(X, Y, FortressTop); }
            }
        }
        M.Flat(FVector(FieldX0, FieldY0, VoidZ), FVector(FieldX1, FieldY0, VoidZ), FVector(FieldX1, FieldY1, VoidZ), FVector(FieldX0, FieldY1, VoidZ), FVector::UpVector, W);
        Void = NewObject<UProceduralMeshComponent>(this, TEXT("BleedVoid"));
        Void->SetupAttachment(RootComponent); Void->RegisterComponent();
        Void->SetWorldLocation(Origin);
        Void->CreateMeshSection(0, M.V, M.T, M.N, M.UV, M.C, M.Tan, false);
        Void->SetMaterial(0, Depth); Void->SetCastShadow(false);
        Void->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // ---- The river's course: out of the source tear over the +Y moat, down the island's middle columns, over the island's
    // -Y edge and down into the far tear.
    {
        const float Wz = GetWaterZ();
        auto Add = [&](float Y, float Z) { Course.Add(FVector(BleedRiverXc, Y, Z)); };
        for (int32 K = 0; K <= 7; ++K) { const float U = K / 7.f; Add(FMath::Lerp(BleedTearInY - 10.f, IslandY1 - 60.f, U), Wz + 125.f * FMath::Square(1 - U) - 3.f * U); }
        RunFirst = Course.Num();
        for (float Y = IslandY1 - 110.f; Y > IslandY0 + 80.f; Y -= 50.f) Add(Y, Wz);
        RunLast = Course.Num() - 1;
        for (int32 K = 0; K <= 7; ++K) { const float U = K / 7.f; Add(FMath::Lerp(IslandY0 + 40.f, BleedTearOutY + 20.f, U), Wz - 150.f * U * U); }
        float S = 0; CourseS.Add(0);
        for (int32 K = 1; K < Course.Num(); ++K) { S += static_cast<float>(FVector::Dist(Course[K], Course[K - 1])) / 100.f; CourseS.Add(S); }
        Water = NewObject<UProceduralMeshComponent>(this, TEXT("BleedRiver"));
        Water->SetupAttachment(RootComponent); Water->RegisterComponent();
        Water->SetWorldLocation(Origin);
        Water->SetMaterial(0, RiverMat); Water->SetCastShadow(false);
        Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UpdateRiverMesh();
        Water->SetVisibility(false);
        // (2026-11-18) The surge: the river down the whole island as one.
        Flood = NewObject<UProceduralMeshComponent>(this, TEXT("BleedFlood"));
        Flood->SetupAttachment(RootComponent); Flood->RegisterComponent();
        Flood->SetWorldLocation(Origin);
        Flood->SetMaterial(0, RiverMat); Flood->SetCastShadow(false);
        Flood->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Flood->SetVisibility(false);
    }
    // ---- The tears: ragged openings standing over the side moats, across the river's line.
    auto MakeTear = [&](const TCHAR* Name, float Y, float Zc, float Rb, float Seed)
    {
        FBleedMesh M;
        const int32 Nr = 48;
        const FColor Col(255, 255, 255, 255);
        const int32 Centre = M.Vert(FVector::ZeroVector, FVector(0, 1, 0), FVector2D(0, 0), Col);
        TArray<int32> Mid, Edge;
        for (int32 K = 0; K < Nr; ++K)
        {
            const float A = 2 * PI * K / Nr;
            const float Rr = Rb * (1 + .16f * FMath::Sin(3 * A + Seed) + .1f * FMath::Sin(7 * A + 2 * Seed) + .06f * FMath::Sin(13 * A + .5f * Seed));
            const FVector Dir(FMath::Cos(A), 0, FMath::Sin(A) * 1.25f);
            Mid.Add(M.Vert(Dir * Rr * .55f, FVector(0, 1, 0), FVector2D(FMath::Cos(A), FMath::Sin(A)) * .55f, Col));
            Edge.Add(M.Vert(Dir * Rr, FVector(0, 1, 0), FVector2D(FMath::Cos(A), FMath::Sin(A)), Col));
        }
        for (int32 K = 0; K < Nr; ++K)
        {
            const int32 K1 = (K + 1) % Nr;
            M.Tri(Centre, Mid[K], Mid[K1], FVector(0, 1, 0));
            M.Quad(Mid[K], Edge[K], Edge[K1], Mid[K1], FVector(0, 1, 0));
        }
        auto* T = NewObject<UProceduralMeshComponent>(this, Name);
        T->SetupAttachment(RootComponent); T->RegisterComponent();
        T->SetWorldLocation(Origin + FVector(BleedRiverXc, Y, Zc));
        T->CreateMeshSection(0, M.V, M.T, M.N, M.UV, M.C, M.Tan, false);
        T->SetMaterial(0, TearMat); T->SetCastShadow(false);
        T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        T->SetWorldScale3D(FVector(.01f)); T->SetVisibility(false);
        return T;
    };
    TearIn = MakeTear(TEXT("BleedTearIn"), BleedTearInY, GetWaterZ() + 150.f, 210.f, 1.3f);
    TearOut = MakeTear(TEXT("BleedTearOut"), BleedTearOutY, GetWaterZ() - 40.f, 250.f, 4.1f);
    // ---- Lights: each tear's, the river's (it follows him along it), the Astral's under the moat once it is open.
    auto Light = [&](const FVector& Local, const FLinearColor& Col, float Radius)
    {
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(RootComponent); L->RegisterComponent();
        L->SetWorldLocation(Origin + Local); L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f);
        L->SetLightColor(Col); L->SetAttenuationRadius(Radius); L->SetCastShadows(false); L->SetSourceRadius(60.f);
        Lights.Add(L);
    };
    Light(FVector(BleedRiverXc, BleedTearInY - 120.f, GetWaterZ() + 160.f), FLinearColor(.35f, 1.f, .75f), 2200.f);
    Light(FVector(BleedRiverXc, BleedTearOutY + 140.f, GetWaterZ() + 40.f), FLinearColor(.35f, 1.f, .75f), 2200.f);
    Light(FVector(BleedRiverXc, 0, GetWaterZ() + 120.f), FLinearColor(.3f, .95f, .8f), 1100.f);
    for (const FVector2D& P : {FVector2D((FieldX0 + IslandX0) * .5f, 0), FVector2D((IslandX1 + FieldX1) * .5f, 0), FVector2D((IslandX0 + IslandX1) * .5f, (FieldY0 + IslandY0) * .5f), FVector2D((IslandX0 + IslandX1) * .5f, (IslandY1 + FieldY1) * .5f)})
        Light(FVector(P.X, P.Y, VoidZ + 180.f), FLinearColor(.62f, .4f, 1.f), 2600.f);
    if (USoundBase* S = Sounds[SndRiver])
    {
        if (auto* Wave = Cast<USoundWave>(S)) Wave->bLooping = true;
        RiverSound = UGameplayStatics::SpawnSoundAtLocation(this, S, Origin + FVector(BleedRiverXc, 0, GetWaterZ()), FRotator::ZeroRotator, 1.f, 1.f, 0.f, Attenuation, nullptr, false);
        if (RiverSound) RiverSound->Stop();
    }
    FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SweepCam = GetWorld()->SpawnActor<ACameraActor>(Origin + FVector(BleedRiverXc, 0, 1500), FRotator::ZeroRotator, Sp);
    if (SweepCam && SweepCam->GetCameraComponent()) { SweepCam->GetCameraComponent()->SetFieldOfView(78.f); SweepCam->GetCameraComponent()->bConstrainAspectRatio = false; }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_BUILT slabs=%d course=%d missing=%d"), Slabs.Num(), Course.Num(), Missing);
}

void APhlegBleed::PlaceSlab(FSlab& S) const
{
    UProceduralMeshComponent* M = S.Mesh.Get();
    if (!M) return;
    // Its vertices are in the stage's frame (the basalt's triplanar and its heat by height read them): turned about its
    // own middle and dropped.
    const FQuat Q(S.Rot);
    M->SetWorldLocationAndRotation(PhlegethosOrigin() + S.Centre + S.Drop - Q.RotateVector(S.Centre), Q);
}

void APhlegBleed::Rearm()
{
    for (FSlab& S : Slabs)
    {
        S.State = ESlab::Whole; S.Due = -1.f; S.PressedAt = -1.f; S.bForced = false; S.CollapseDue = -1.f;
        S.Drop = S.Vel = FVector::ZeroVector; S.Rot = S.Spin = FRotator::ZeroRotator;
        if (UProceduralMeshComponent* M = S.Mesh.Get())
        {
            PlaceSlab(S);
            M->SetVisibility(true); M->SetMeshSectionVisible(1, false);
            M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
    RiverOpen = 0.f; RiverFront = 1e9f; bTears = false; bCollapse = false; bMoatHeard = false; WaveDone = 0.f;
    bSurge = false; bSeeked = false; SurgeFront = 1e9f;
    if (Water) Water->SetVisibility(false);
    if (Flood) Flood->SetVisibility(false);
    for (UProceduralMeshComponent* T : {TearIn, TearOut}) if (T) { T->SetVisibility(false); T->SetWorldScale3D(FVector(.01f)); }
    for (UPointLightComponent* L : Lights) if (L) L->SetIntensity(0.f);
    if (RiverSound) RiverSound->Stop();
    if (Stage != EStage::Waiting) ++Rearms;
    Stage = EStage::Waiting; Clock = 0.f; OnFor = 0.f; FadeAt = -1.f;
}

int32 APhlegBleed::GetFallen(EZone Zone) const
{
    int32 N = 0;
    for (const FSlab& S : Slabs) N += S.Zone == Zone && (S.State == ESlab::Falling || S.State == ESlab::Gone);
    return N;
}

int32 APhlegBleed::GetWhole(EZone Zone) const
{
    int32 N = 0;
    for (const FSlab& S : Slabs) N += S.Zone == Zone && (S.State == ESlab::Whole || S.State == ESlab::Cracking);
    return N;
}

int32 APhlegBleed::SlabIndexAt(float X, float Y) const
{
    if (!Phleg::InSlabField(X, Y)) return -1;
    const int32 Cell = Phleg::FieldColAt(X) * Phleg::FieldRows + Phleg::FieldRowAt(Y);
    return CellSlab.IsValidIndex(Cell) ? CellSlab[Cell] : -1;
}

bool APhlegBleed::ArenaRespawn(FVector& OutWorld)
{
    const APhlegBleed* B = TheBleed.Get();
    if (!B || B->Stage != EStage::Running || B->RespawnLocal.IsZero()) return false;
    OutWorld = PhlegWorld(B->RespawnLocal + FVector(0, 0, 34.65f + 2.f));
    return true;
}

bool APhlegBleed::OpenAt(float X, float Y) const
{
    const int32 I = SlabIndexAt(X, Y);
    return Slabs.IsValidIndex(I) && (Slabs[I].State == ESlab::Falling || Slabs[I].State == ESlab::Gone);
}

int32 APhlegBleed::SlabStateAt(float X, float Y) const
{
    const int32 I = SlabIndexAt(X, Y);
    if (!Slabs.IsValidIndex(I)) return -1;
    return Slabs[I].State == ESlab::Whole ? 0 : Slabs[I].State == ESlab::Cracking ? 1 : 2;
}

bool APhlegBleed::FindWhole(int32 Col, float NearY, FVector& Out) const
{
    float Best = 1e9f;
    for (const FSlab& S : Slabs)
        if (S.Col == Col && S.State == ESlab::Whole && S.Row >= Phleg::MoatRows + 1 && S.Row < Phleg::FieldRows - Phleg::MoatRows - 1 && FMath::Abs(S.Centre.Y - NearY) < Best)
        { Best = static_cast<float>(FMath::Abs(S.Centre.Y - NearY)); Out = S.Centre; }
    return Best < 1e8f;
}

void APhlegBleed::GatherHolders()
{
    // The battle's feet (local XY): the trio, the fiend, the lemures on theirs.
    Holders.Reset();
    if (const ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(GetWorld()))
        for (int32 R = 0; R < 3; ++R) if (const ATempleHero* H = B->GetHero(R)) { const FVector L = PhlegLocal(H->GetActorLocation()); Holders.Add(FVector2D(L.X, L.Y)); }
    for (const TWeakObjectPtr<ATempleSkeleton>& S : ATempleSkeleton::All())
        if (S.IsValid() && FCString::Strcmp(S->GetPost().Space, TEXT("fortress")) == 0 && !S->IsHidden())
        { const FVector L = PhlegLocal(S->GetActorLocation()); Holders.Add(FVector2D(L.X, L.Y)); }
}

bool APhlegBleed::Held(const FSlab& S) const
{
    for (const FVector2D& P : Holders) if (FMath::Abs(P.X - S.Centre.X) < Phleg::FieldColW * .5f + 70.f && FMath::Abs(P.Y - S.Centre.Y) < Phleg::FieldRowW * .5f + 70.f) return true;
    return false;
}

void APhlegBleed::PlayAt(USoundBase* Sound, const FVector& Local, float Volume, float Pitch)
{
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, PhlegWorld(Local), Volume, Pitch, 0.f, Attenuation);
}

void APhlegBleed::Crack(FSlab& S, float Due, float CrackFor, bool bForced)
{
    if (S.State != ESlab::Whole) return;
    S.State = ESlab::Cracking; S.At = Clock; S.Due = Due; S.CrackFor = CrackFor; S.bForced = bForced; S.PressedAt = -1.f;
    if (UProceduralMeshComponent* M = S.Mesh.Get()) M->SetMeshSectionVisible(1, true);
}

void APhlegBleed::Fall(FSlab& S)
{
    S.State = ESlab::Falling; S.At = Clock;
    S.Vel = FVector(0, 0, -70.f);
    S.Spin = FRotator(FMath::FRandRange(-50.f, 50.f), FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-50.f, 50.f));
    if (UProceduralMeshComponent* M = S.Mesh.Get()) M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlayAt(Sounds[FMath::RandBool() ? SndFall0 : SndFall1], S.Centre, S.Zone == EZone::Land ? .9f : .55f, FMath::FRandRange(.8f, 1.f));
    if (Setting && Clock - LastDust > .18f) { LastDust = Clock; Setting->GroundBlow(PhlegWorld(S.Centre), .2f, false); }
}

void APhlegBleed::Start(AChuckCharacter* Chuck)
{
    using namespace Phleg;
    Stage = EStage::Running; Clock = 0.f; ++Starts; bCollapse = false; bTears = false; WaveDone = 0.f;
    const FVector Him = Chuck ? PhlegLocal(Chuck->GetActorLocation()) : FVector((IslandX0 + IslandX1) * .5f, 0, 0);
    // The moat tears outward from where he stands: behind him first, then the sides, then in front.
    for (FSlab& S : Slabs)
    {
        const float Hash = BleedHash(S.Col * 1.37f, S.Row * 2.11f);
        switch (S.Zone)
        {
        case EZone::MoatBack: Crack(S, FMath::Abs(S.Centre.Y - Him.Y) / 2400.f + .2f * Hash, 1.1f, true); break;
        case EZone::MoatSide: Crack(S, .6f + FMath::Abs(S.Centre.X - Him.X) / 2400.f + .2f * Hash, 1.1f, true); break;
        case EZone::MoatFront: Crack(S, 1.1f + FMath::Abs(S.Centre.Y - Him.Y) / 2400.f + .2f * Hash, 1.1f, true); break;
        default: break;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_START at=%s"), *Him.ToString());
    // (2026-11-18) Its schedule, fitted to the encounter's track: its end the track's next end at least MinRun off; the
    // surge on the track's last return before it; what comes before the surge scaled to the time there is (a little
    // faster or slower), any more waited out first (the fight goes on).
    const ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(GetWorld());
    double Now = 0; float Len = TrackSeconds;
    bMusicTimed = ADockGameMode::GetEncounterMusicTime(GetWorld(), Now, &Len) && Len > 30.f;
    if (!bMusicTimed) { Len = TrackSeconds; Now = B ? FMath::Max(0.0, B->GetEncounterTime()) : 0.0; }
    // (only its place in its loop is trusted: the count of loops can be one off from its first report)
    if (bMusicTimed) Now = FMath::Fmod(Now, static_cast<double>(Len));
    TrackLength = Len; StartMusic = Now; MusicRun = 0;
    constexpr float MinScale = .65f, MaxScale = 1.25f;   // (the entrance is about 26 s: he is on the island by about 30 s of the track)
    const double MinRun = SurgeAt * MinScale + SurgeToEnd;
    const double End = Len * FMath::Max(1.0, FMath::CeilToDouble((Now + MinRun) / Len));
    AtEnd = static_cast<float>(End - Now); AtEdge = AtEnd - EdgeToEnd; AtSurge = AtEnd - SurgeToEnd;
    PlanScale = FMath::Clamp(AtSurge / SurgeAt, MinScale, MaxScale);
    PlanDelay = FMath::Max(0.f, AtSurge - SurgeAt * PlanScale);
    AtWave[0] = PlanDelay + Wave1At * PlanScale; AtWave[1] = PlanDelay + Wave2At * PlanScale; AtWave[2] = PlanDelay + Wave3At * PlanScale;
    AtRiver = PlanDelay + RiverAt * PlanScale; AtCollapse = PlanDelay + CollapseAt * PlanScale; SpanCollapse = CollapseSpan * PlanScale;
    bSweptBySurge = false; bSeeked = false; SweepEndClock = SweepEndMusic = -1.f;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_PLAN music=%d start=%.2f track=%.2f end_in=%.2f scale=%.2f delay=%.2f river=%.1f collapse=%.1f surge=%.2f edge=%.2f"),
        bMusicTimed ? 1 : 0, Now, Len, AtEnd, PlanScale, PlanDelay, AtRiver, AtCollapse, AtSurge, AtEdge);
}

float APhlegBleed::SurgeFrontAt(float At) const
{
    // Out of the widened tear slowly, then quicker and quicker down the island to the far edge at AtEdge.
    const float U = FMath::Clamp((At - AtSurge) / FMath::Max(1.f, AtEdge - AtSurge), 0.f, 1.f);
    return FMath::Lerp(Phleg::IslandY1 + 60.f, Phleg::IslandY0 + 40.f, U * (.55f + .45f * U));
}

void APhlegBleed::Tick(float Dt)
{
    Super::Tick(Dt);
    using namespace Phleg;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || !InPhlegethos(Chuck->GetActorLocation())) return;
    const FVector L = PhlegLocal(Chuck->GetActorLocation());
    if (Stage == EStage::Waiting)
    {
        if (bHold) return;
        const ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(GetWorld());
        const bool bReady = B && B->HasFortressSceneRun() && !B->IsFortressScenePlaying() && B->IsFortressFighting();
        const UCharacterMovementComponent* Mv = Chuck->GetCharacterMovement();
        const bool bOn = OnIsland(static_cast<float>(L.X), static_cast<float>(L.Y)) && Mv && Mv->IsMovingOnGround() && !Chuck->IsAstral();
        OnFor = bReady && bOn ? OnFor + Dt : 0.f;
        if (OnFor > .4f) Start(Chuck);
        return;
    }
    Clock += Dt;
    // (2026-11-18) The clock follows the encounter's track (paused with the game, and steady through a hitch).
    if (bMusicTimed && !bSeeked)
    {
        double M = 0;
        if (ADockGameMode::GetEncounterMusicTime(GetWorld(), M))
        {
            // Its place in its loop, followed on from the start (round its end and on).
            const double In = FMath::Fmod(M, static_cast<double>(TrackLength));
            double Step = In - FMath::Fmod(StartMusic + MusicRun, static_cast<double>(TrackLength));
            if (Step < -TrackLength * .5) Step += TrackLength; else if (Step > TrackLength * .5) Step -= TrackLength;
            MusicRun += Step;
            const float Want = static_cast<float>(MusicRun);
            Clock = FMath::Abs(Want - Clock) > 1.5f ? Want : Clock + (Want - Clock) * FMath::Min(1.f, Dt * 4.f);
        }
    }
    // A death in it (into the Astral, the fiend's blows): it goes on (2026-11-17, the user; it used to re-arm). He comes
    // back on the ground by the breach (ArenaRespawn).
    const bool bGoneNow = Stage == EStage::Running && Chuck->IsBodyHidden() && !Chuck->IsCarried();
    if (bGoneNow && !bWasHidden) { ++DeathsIn; UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_DEATH n=%d clock=%.1f"), DeathsIn, Clock); }
    bWasHidden = bGoneNow;
    GatherHolders();
    TickSlabs(Dt, Chuck);
    TickRiver(Dt, Chuck);
    TickSurge(Dt, Chuck);
    if (Stage == EStage::Swept) TickSweep(Dt, Chuck);
}

void APhlegBleed::TickSlabs(float Dt, AChuckCharacter* Chuck)
{
    using namespace Phleg;
    const FVector Him = PhlegLocal(Chuck->GetActorLocation());
    UPrimitiveComponent* Floor = nullptr;
    if (Chuck->GetCharacterMovement()->IsMovingOnGround()) Floor = Chuck->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent();
    // The waves, scheduled as each comes (the island's -Y end, its +Y end, then scattered); the collapse.
    auto Wave = [&](float At, int32 Index)
    {
        if (Clock < At || WaveDone >= Index) return;
        WaveDone = Index;
        int32 N = 0;
        for (FSlab& S : Slabs)
        {
            if (S.Zone != EZone::Land || S.State != ESlab::Whole) continue;
            const float H1 = BleedHash(S.Col * 1.37f + Index, S.Row * 2.11f), H2 = BleedHash(S.Row * 3.3f + Index, S.Col * .7f);
            const int32 R = S.Row - MoatRows, Rows = FieldRows - 2 * MoatRows;   // 0..19 across the island
            float Due = -1.f;
            if (Index == 1 && R < 7 && H1 < .62f) Due = At + R * .22f + .4f * H2;
            else if (Index == 2 && R >= Rows - 7 && H1 < .62f) Due = At + (Rows - 1 - R) * .22f + .4f * H2;
            else if (Index == 3 && ((R >= 7 && R < Rows - 7 && H1 < .38f) || ((R < 7 || R >= Rows - 7) && H1 < .55f))) Due = At + 3.f * H2;
            if (Due < 0 || Held(S)) continue;   // (not from under the battle's feet)
            Crack(S, Due, 1.6f, false); ++N;
        }
        if (USoundBase* S = Sounds[SndRumble]) UGameplayStatics::PlaySound2D(this, S, .45f, .5f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_WAVE %d slabs=%d"), Index, N);
    };
    Wave(AtWave[0], 1); Wave(AtWave[1], 2); Wave(AtWave[2], 3);
    if (!bCollapse && Clock >= AtCollapse)
    {
        bCollapse = true;
        // What is left of the island falls in from its far ends toward him.
        float Far = 1.f;
        for (const FSlab& S : Slabs) if (S.Zone == EZone::Land && S.State == ESlab::Whole) Far = FMath::Max(Far, static_cast<float>(FMath::Abs(S.Centre.Y - Him.Y) + .5f * FMath::Abs(S.Centre.X - Him.X)));
        int32 N = 0;
        for (FSlab& S : Slabs)
        {
            if (S.Zone != EZone::Land || S.State != ESlab::Whole) continue;
            const float D = static_cast<float>(FMath::Abs(S.Centre.Y - Him.Y) + .5f * FMath::Abs(S.Centre.X - Him.X));
            S.CollapseDue = AtCollapse + SpanCollapse * (1.f - D / Far); ++N;   // (cracked when due, if no one of the battle stands on it)
        }
        if (USoundBase* S = Sounds[SndRumble]) UGameplayStatics::PlaySound2D(this, S, .6f, .42f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_COLLAPSE slabs=%d"), N);
    }
    if (!bMoatHeard && Clock > .9f)
    {
        bMoatHeard = true;
        if (USoundBase* S = Sounds[SndRumble]) UGameplayStatics::PlaySound2D(this, S, .7f, .45f);
        for (int32 K = 3; K < Lights.Num(); ++K) if (Lights[K]) Lights[K]->SetIntensity(28.f);
    }
    for (FSlab& S : Slabs)
    {
        UProceduralMeshComponent* M = S.Mesh.Get();
        if (!M) continue;
        const bool bOnIt = Floor == M;
        switch (S.State)
        {
        case ESlab::Whole:
            // In the collapse, whatever he stands on cracks at once (and goes in under a second); the rest as each is due,
            // unless one of the battle stands on it.
            if (bCollapse && bOnIt && S.Zone == EZone::Land) Crack(S, Clock, .9f, true);
            else if (bCollapse && S.CollapseDue >= 0.f && Clock >= S.CollapseDue && !Held(S)) Crack(S, Clock, 1.3f, true);
            break;
        case ESlab::Cracking:
        {
            if (Clock < S.Due) { break; }
            if (bOnIt && S.PressedAt < 0) S.PressedAt = Clock;
            const float Age = Clock - S.Due;
            if (Age < Dt * 1.5f) PlayAt(Sounds[FMath::RandBool() ? SndCrack0 : SndCrack1], S.Centre, S.Zone == EZone::Land ? .8f : .5f, FMath::FRandRange(.85f, 1.1f));
            // A shudder growing toward the fall, sinking a little.
            const float J = 1.4f * FMath::Min(1.f, Age / S.CrackFor);
            S.Drop = FVector(FMath::FRandRange(-J, J), FMath::FRandRange(-J, J), -Age * 3.f);
            S.Rot = FRotator(FMath::FRandRange(-J, J) * .3f, 0, FMath::FRandRange(-J, J) * .3f);
            PlaceSlab(S);
            const bool bPressed = S.PressedAt >= 0.f && Clock - S.PressedAt >= .6f;
            if (bPressed) ++PressedFalls;
            // (one of the battle has stepped onto it: it holds a while longer)
            if (Age >= S.CrackFor && !bPressed && !bOnIt && Held(S)) { S.CrackFor = Age + .5f; ++HeldFalls; break; }
            if (Age >= S.CrackFor || bPressed) Fall(S);
            break;
        }
        case ESlab::Falling:
            S.Vel.Z -= 980.f * Dt;
            S.Drop += S.Vel * Dt; S.Rot += S.Spin * Dt;
            PlaceSlab(S);
            if (Clock - S.At > 2.4f) { S.State = ESlab::Gone; M->SetVisibility(false); }
            break;
        default: break;
        }
        // (the crack's light shows only from its due time)
        if (S.State == ESlab::Cracking && Clock >= S.Due && !M->IsMeshSectionVisible(1)) M->SetMeshSectionVisible(1, true);
        if (S.State == ESlab::Cracking && Clock < S.Due && M->IsMeshSectionVisible(1)) M->SetMeshSectionVisible(1, false);
    }
}

void APhlegBleed::TickRiver(float Dt, AChuckCharacter* Chuck)
{
    using namespace Phleg;
    if (Clock < AtRiver) return;
    if (!bTears)
    {
        bTears = true;
        for (UProceduralMeshComponent* T : {TearIn, TearOut}) if (T) T->SetVisibility(true);
        if (Water) Water->SetVisibility(true);
        PlayAt(Sounds[SndTear], FVector(BleedRiverXc, BleedTearInY, GetWaterZ() + 150.f), 1.f, .55f);
        PlayAt(Sounds[SndTear], FVector(BleedRiverXc, BleedTearOutY, GetWaterZ()), .9f, .5f);
        if (RiverSound) { RiverSound->Play(); RiverSound->SetVolumeMultiplier(0.f); }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_RIVER open"));
    }
    const float T = Clock - AtRiver;
    // The tears open over a second (the far one a little later), the water out of the first after it.
    const float OpenIn = BleedEase(T / 1.1f), OpenOut = BleedEase((T - .5f) / 1.1f);
    if (TearIn && !bSurge) TearIn->SetWorldScale3D(FVector(FMath::Max(.01f, OpenIn)));
    if (TearOut) TearOut->SetWorldScale3D(FVector(FMath::Max(.01f, OpenOut)));
    RiverOpen = BleedEase((T - .8f) / .6f);
    const float Run = FMath::Max(0.f, T - 1.2f) * RiverSpeed;
    RiverFront = IslandY1 - Run;
    const float Flicker = .9f + .1f * FMath::Sin(Clock * 5.3f);
    if (Lights.Num() > 2)
    {
        if (Lights[0]) Lights[0]->SetIntensity(45.f * OpenIn * Flicker);
        if (Lights[1]) Lights[1]->SetIntensity(45.f * OpenOut * Flicker);
        if (Lights[2])
        {
            const float Y = FMath::Clamp(static_cast<float>(PhlegLocal(Chuck->GetActorLocation()).Y), FMath::Max(RiverFront, IslandY0), IslandY1);
            Lights[2]->SetWorldLocation(PhlegWorld(FVector(BleedRiverXc, Y, GetWaterZ() + 130.f)));
            Lights[2]->SetIntensity(12.f * RiverOpen);
        }
    }
    if (RiverSound)
    {
        const float Y = FMath::Clamp(static_cast<float>(PhlegLocal(Chuck->GetActorLocation()).Y), FMath::Max(RiverFront, IslandY0 - 150.f), IslandY1 + 150.f);
        RiverSound->SetWorldLocation(PhlegWorld(FVector(BleedRiverXc, Y, GetWaterZ())));
        RiverSound->SetVolumeMultiplier(1.1f * RiverOpen);
    }
    UpdateRiverMesh();
    // Into it: swept away.
    if (Stage == EStage::Running && RiverOpen > .5f && !Chuck->IsAstral())
    {
        const FVector F = PhlegLocal(Chuck->GetActorLocation()) - FVector(0, 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const bool bIn = F.X > RiverX0 - 12.f && F.X < RiverX1 + 12.f && F.Y < IslandY1 + 40.f && F.Y > RiverFront - 20.f && F.Z < GetWaterZ() + 22.f && F.Z > FortressTop - 120.f;
        if (bIn) BeginSweep(Chuck, false);
    }
}

void APhlegBleed::UpdateRiverMesh()
{
    using namespace Phleg;
    if (!Water || Course.Num() < 2) return;
    const float Wz = GetWaterZ();
    const float Half = (RiverX1 - RiverX0) * .5f - 6.f;
    constexpr int32 Across = 5;
    FBleedMesh M;
    const float Front = FMath::Clamp(RiverFront, static_cast<float>(Course.Last().Y), IslandY1);
    const float Tm = Clock;
    const int32 Rows = Course.Num();
    for (int32 K = 0; K < Rows; ++K)
    {
        FVector P = Course[K];
        float Width = Half;
        if (K < RunFirst) Width = Half * FMath::Lerp(.72f, 1.f, float(K) / RunFirst);   // narrower out of the tear
        // Beyond the front: drawn up to it (the tongue of water still coming).
        bool bBeyond = P.Y < Front;
        if (bBeyond) P = FVector(P.X, Front, Wz - 18.f);
        const bool bRun = K >= RunFirst && K <= RunLast;
        const float ToFront = static_cast<float>(P.Y - Front);
        const float Tongue = bRun || bBeyond ? BleedEase(ToFront / 70.f) : 1.f;   // its leading edge rounded down
        const float Wave = bRun ? 2.5f * FMath::Sin(.012f * P.Y - 7.f * Tm) + 1.4f * FMath::Sin(.031f * P.Y - 11.f * Tm + 1.3f) : 0.f;
        const float Foam = FMath::Clamp(FMath::Max(K < RunFirst + 2 ? 1.f : .28f, FMath::Max(1.f - ToFront / 170.f, K > RunLast ? .85f : 0.f)), 0.f, 1.f);
        const uint8 Fr = uint8(255 * Foam), Gv = uint8(255 * FMath::Clamp(RiverOpen, 0.f, 1.f));
        const float V = CourseS[K];
        for (int32 A = 0; A <= Across; ++A)
        {
            const float U = float(A) / Across;
            const float X = P.X + (U * 2 - 1) * Width;
            const float Crown = bRun ? 2.f * (1 - FMath::Square(U * 2 - 1)) : 0.f;
            const float Z = bRun || bBeyond ? FMath::Lerp(FortressTop + 1.f, P.Z + Wave + Crown, Tongue) : P.Z;
            M.Vert(FVector(X, P.Y, Z), FVector::UpVector, FVector2D(U, V), FColor(Fr, Gv, 0, 255));
        }
    }
    for (int32 K = 0; K + 1 < Rows; ++K)
        for (int32 A = 0; A < Across; ++A)
        {
            const int32 I = K * (Across + 1) + A;
            M.Quad(I, I + 1, I + Across + 2, I + Across + 1, FVector::UpVector);
        }
    // Its sides: walls of water down to the slabs along the run (a hard-edged current, as the 2D's river blocks).
    const int32 Base = M.V.Num();
    for (int32 K = RunFirst; K <= RunLast + 1 && K < Rows; ++K)
        for (int32 Sd = 0; Sd < 2; ++Sd)
        {
            const FVector Top = M.V[K * (Across + 1) + (Sd ? Across : 0)];
            const FVector Out(Sd ? 1 : -1, 0, 0);
            const uint8 Gv = uint8(255 * FMath::Clamp(RiverOpen, 0.f, 1.f));
            M.Vert(Top, Out, FVector2D(Sd ? 1.f : 0.f, CourseS[K]), FColor(90, Gv, 0, 255));
            M.Vert(FVector(Top.X, Top.Y, FortressTop - 2.f), Out, FVector2D(Sd ? .9f : .1f, CourseS[K]), FColor(90, Gv, 0, 255));
        }
    for (int32 K = 0; K + 1 < (FMath::Min(RunLast + 1, Rows - 1) - RunFirst + 1); ++K)
        for (int32 Sd = 0; Sd < 2; ++Sd)
        {
            const int32 I = Base + (K * 2 + Sd) * 2, J = Base + ((K + 1) * 2 + Sd) * 2;
            M.Quad(I, J, J + 1, I + 1, FVector(Sd ? 1 : -1, 0, 0));
        }
    if (Water->GetNumSections() == 0) Water->CreateMeshSection(0, M.V, M.T, M.N, M.UV, M.C, M.Tan, false);
    else Water->UpdateMeshSection(0, M.V, M.N, M.UV, M.C, M.Tan);
}

void APhlegBleed::BeginSweep(AChuckCharacter* Chuck, bool bBySurge)
{
    using namespace Phleg;
    Stage = EStage::Swept; SweepT = 0.f; FadeAt = -1.f; ++Sweeps; bSweptBySurge = bBySurge;
    SweepSpeed = FMath::Max(200.f, static_cast<float>(Chuck->GetVelocity().Size2D()) * .5f);
    SweepAt = PhlegLocal(Chuck->GetActorLocation()); SweepFromY = static_cast<float>(SweepAt.Y);
    const FVector F = SweepAt - FVector(0, 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    // (2026-11-18) How long to the far tear's lip: to AtEdge, the track's fall, when that is a fair ride off (the surge
    // carries him on its front to it); else EarlySweep, and the track brought to its end with him.
    const float Left = AtEdge - Clock;
    bSeeked = false;
    if (!bBySurge && (Left < 3.5f || Left > 13.f))
    {
        SweepLen = EarlySweep;
        if (bMusicTimed)
        {
            bSeeked = true;
            constexpr float Dip = .5f;
            ADockGameMode::SeekEncounterMusic(GetWorld(), TrackLength - EdgeToEnd - SweepLen + Dip, Dip);
        }
    }
    else SweepLen = FMath::Max(1.f, Left);
    Chuck->BeginCarried();
    PlayAt(Sounds[FMath::RandBool() ? SndSplash0 : SndSplash1], F, 1.f, .8f);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController()); PC && SweepCam)
    {
        const FVector Eye0 = Chuck->GetActorLocation() + FVector(120, 290, 140);
        SweepCam->SetActorLocationAndRotation(Eye0, (Chuck->GetActorLocation() - Eye0).Rotation());
        PC->SetViewTargetWithBlend(SweepCam, .45f);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_SWEPT at=%s clock=%.1f by=%s ride=%.2f seek=%d"), *F.ToString(), Clock, bBySurge ? TEXT("surge") : TEXT("river"), SweepLen, bSeeked ? 1 : 0);
}

void APhlegBleed::TickSurge(float Dt, AChuckCharacter* Chuck)
{
    using namespace Phleg;
    if (Stage == EStage::Waiting || Clock < AtSurge) return;
    if (!bSurge)
    {
        bSurge = true;
        // The source tear tears wide, a roar of water, the ground shaken; the river comes down the whole island.
        if (TearIn) TearIn->SetVisibility(true);
        if (Flood) Flood->SetVisibility(true);
        PlayAt(Sounds[SndTear], FVector(BleedRiverXc, BleedTearInY, GetWaterZ() + 150.f), 1.f, .42f);
        PlayAt(Sounds[SndSplash0], FVector((IslandX0 + IslandX1) * .5f, IslandY1, GetWaterZ()), 1.f, .55f);
        if (USoundBase* S = Sounds[SndRumble]) UGameplayStatics::PlaySound2D(this, S, .8f, .38f);
        BeholderShake(GetWorld(), PhlegWorld(FVector((IslandX0 + IslandX1) * .5f, IslandY1, FortressTop)), .55f, 9000.f, 1);
        if (RiverSound) { if (!RiverSound->IsPlaying()) RiverSound->Play(); }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_SURGE clock=%.2f edge=%.2f"), Clock, AtEdge);
    }
    const float T = Clock - AtSurge;
    SurgeFront = SurgeFrontAt(Clock);
    const float Wide = 1.f + 1.7f * BleedEase(T / 1.6f);
    if (TearIn) TearIn->SetWorldScale3D(FVector(Wide, 1.f, FMath::Lerp(1.f, 1.6f, BleedEase(T / 1.6f))));
    if (Lights.Num() > 2)
    {
        if (Lights[0]) Lights[0]->SetIntensity(80.f * (.9f + .1f * FMath::Sin(Clock * 6.1f)));
        if (Lights[2])
        {
            Lights[2]->SetWorldLocation(PhlegWorld(FVector((IslandX0 + IslandX1) * .5f, SurgeFront + 250.f, GetWaterZ() + 160.f)));
            Lights[2]->SetAttenuationRadius(2200.f); Lights[2]->SetIntensity(30.f);
        }
    }
    if (RiverSound)
    {
        const FVector Him = PhlegLocal(Chuck->GetActorLocation());
        RiverSound->SetWorldLocation(PhlegWorld(FVector(FMath::Clamp(static_cast<float>(Him.X), IslandX0, IslandX1), FMath::Max(static_cast<float>(Him.Y), SurgeFront), GetWaterZ())));
        RiverSound->SetVolumeMultiplier(1.6f);
    }
    UpdateFloodMesh();
    // Behind its front it is everywhere: he is taken wherever he stands (back from a death too, on the kept ground).
    if (Stage == EStage::Running && !Chuck->IsAstral() && !Chuck->IsBodyHidden())
    {
        const FVector F = PhlegLocal(Chuck->GetActorLocation()) - FVector(0, 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const bool bIn = F.X > IslandX0 - 30.f && F.X < IslandX1 + 30.f && F.Y > SurgeFront - 30.f && F.Y < IslandY1 + 60.f && F.Z < GetWaterZ() + 60.f && F.Z > FortressTop - 120.f;
        if (bIn) BeginSweep(Chuck, true);
    }
}

void APhlegBleed::UpdateFloodMesh()
{
    using namespace Phleg;
    if (!Flood) return;
    // A sheet of the river over the island behind the front, a foaming crest at the front and a face down to the ground;
    // sides falling to the moats; open over the breach.
    const float Wz = GetWaterZ(), Front = SurgeFront, Tm = Clock;
    constexpr int32 Across = 14;
    TArray<float> Ds = {0.f, 18.f, 45.f, 85.f, 140.f, 220.f, 320.f};
    for (float D = 440.f; Front + D < IslandY1 + 60.f; D += 120.f) Ds.Add(D);
    Ds.Add(IslandY1 + 60.f - Front);
    FBleedMesh M;
    const int32 Rows = Ds.Num();
    for (int32 K = 0; K < Rows; ++K)
    {
        const float D = FMath::Max(0.f, Ds[K]);
        const float Y = Front + D;
        const float Crest = D < 1.f ? 48.f : 66.f * FMath::Exp(-D / 150.f);
        const float Foam = FMath::Clamp(1.f - D / 260.f, .3f, 1.f);
        for (int32 A = 0; A <= Across; ++A)
        {
            const float U = float(A) / Across;
            const float X = FMath::Lerp(IslandX0, IslandX1, U);
            const float Wave = 3.f * FMath::Sin(.011f * Y - 7.5f * Tm + 1.7f * U) + 1.6f * FMath::Sin(.027f * X + .019f * Y - 10.f * Tm);
            M.Vert(FVector(X, Y, Wz + 6.f + Crest + Wave), FVector::UpVector, FVector2D((X - IslandX0) / 200.f, (IslandY1 - Y) / 100.f), FColor(uint8(255 * Foam), 255, 0, 255));
        }
    }
    for (int32 K = 0; K + 1 < Rows; ++K)
        for (int32 A = 0; A < Across; ++A)
        {
            const int32 I = K * (Across + 1) + A;
            const float Cx = (M.V[I].X + M.V[I + 1].X) * .5f, Cy = (M.V[I].Y + M.V[I + Across + 1].Y) * .5f;
            if (BreachInside(Cx, Cy) > -40.f) continue;   // (it pours into the breach)
            M.Quad(I, I + 1, I + Across + 2, I + Across + 1, FVector::UpVector);
        }
    // The front's face, leaning forward down to the ground; the sides down past the island's edge.
    const int32 Top = M.V.Num();
    for (int32 A = 0; A <= Across; ++A)
    {
        const FVector P = M.V[A];
        M.Vert(P, FVector(0, -1, 0), FVector2D(P.X / 200.f, 0.f), FColor(255, 255, 0, 255));
        M.Vert(FVector(P.X, P.Y - 35.f, FortressTop - 4.f), FVector(0, -1, 0), FVector2D(P.X / 200.f, .7f), FColor(255, 255, 0, 255));
    }
    for (int32 A = 0; A < Across; ++A) M.Quad(Top + A * 2, Top + A * 2 + 2, Top + A * 2 + 3, Top + A * 2 + 1, FVector(0, -1, 0));
    for (int32 Sd = 0; Sd < 2; ++Sd)
    {
        const int32 Base = M.V.Num();
        for (int32 K = 0; K < Rows; ++K)
        {
            const FVector P = M.V[K * (Across + 1) + (Sd ? Across : 0)];
            const FVector Out(Sd ? 1 : -1, 0, 0);
            M.Vert(P, Out, FVector2D((IslandY1 - P.Y) / 100.f, 0.f), FColor(150, 255, 0, 255));
            M.Vert(FVector(P.X + Out.X * 30.f, P.Y, FortressTop - 140.f), Out, FVector2D((IslandY1 - P.Y) / 100.f, 1.2f), FColor(150, 255, 0, 255));
        }
        for (int32 K = 0; K + 1 < Rows; ++K) M.Quad(Base + K * 2, Base + K * 2 + 2, Base + K * 2 + 3, Base + K * 2 + 1, FVector(Sd ? 1 : -1, 0, 0));
    }
    Flood->ClearMeshSection(0);
    Flood->CreateMeshSection(0, M.V, M.T, M.N, M.UV, M.C, M.Tan, false);
}

void APhlegBleed::TickSweep(float Dt, AChuckCharacter* Chuck)
{
    using namespace Phleg;
    SweepT += Dt;
    const float Wz = GetWaterZ();
    const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // Down the current toward the far tear, drawn to its middle, rolled and turned by it: on the surge's front, or paced to
    // reach the lip in SweepLen (2026-11-18, the track's fall); past the lip, down into the tear.
    const float Lip = IslandY0 + 40.f;
    if (SweepAt.Y > Lip)
    {
        if (bSweptBySurge) SweepAt.Y = FMath::Min(static_cast<float>(SweepAt.Y), FMath::Max(SurgeFrontAt(Clock) - 70.f, Lip - 1.f));
        else { const float U = FMath::Clamp(SweepT / SweepLen, 0.f, 1.f); SweepAt.Y = FMath::Max(FMath::Lerp(SweepFromY, Lip - 1.f, U * (.6f + .4f * U)), Lip - 1.f); }
    }
    else SweepAt.Y -= 260.f * Dt;
    SweepAt.X = FMath::FInterpTo(SweepAt.X, BleedRiverXc + 60.f * FMath::Sin(SweepT * 1.7f), Dt, 2.2f);
    float Z = Wz + Half - 30.f + 5.f * FMath::Sin(SweepT * 6.3f);
    // (on the surge he rides its crest, not under it: UpdateFloodMesh's height where he is)
    if (bSweptBySurge && bSurge && SweepAt.Y > Lip) Z += 6.f + 66.f * FMath::Exp(-FMath::Max(0.f, static_cast<float>(SweepAt.Y) - SurgeFront) / 150.f);
    if (SweepAt.Y < Lip)
    {
        // Over the edge and down into the tear.
        const float U = FMath::Clamp((IslandY0 + 40.f - SweepAt.Y) / (IslandY0 + 40.f - BleedTearOutY), 0.f, 1.f);
        Z -= 150.f * U * U;
        if (FadeAt < 0.f)
        {
            FadeAt = SweepT;
            if (auto* PC = Cast<APlayerController>(Chuck->GetController()); PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .7f, FLinearColor::Black, false, true);
            PlayAt(Sounds[SndVanish], SweepAt, .8f, .7f);
        }
    }
    SweepAt.Z = Z;
    const FRotator Facing(-12.f + 8.f * FMath::Sin(SweepT * 2.6f), -90.f + 45.f * FMath::Sin(SweepT * 1.25f), 14.f * FMath::Sin(SweepT * 2.1f));
    Chuck->SetCarried(PhlegWorld(SweepAt), Facing);
    if (SweepCam)
    {
        // Upstream of him and above, watching him go; it stops at the island's edge and watches him into the tear.
        const FVector Him = PhlegWorld(SweepAt);
        const FVector WantEye = PhlegWorld(FVector(SweepAt.X + 120.f, FMath::Max(static_cast<float>(SweepAt.Y) + 290.f, IslandY0 + 260.f), Wz + (bSweptBySurge ? 230.f : 140.f)));
        const FVector Eye = FMath::VInterpTo(SweepCam->GetActorLocation(), WantEye, Dt, 9.f);
        SweepCam->SetActorLocationAndRotation(Eye, (Him - Eye).Rotation());
    }
    // (dark from the lip as the track falls away; gone as it ends)
    if (FadeAt >= 0.f && SweepT - FadeAt > EdgeToEnd)
    {
        // Into the tear, gone: the river carries him on into the Feywild (2026-11-19, FeyRiverScene.h; stage 2 of
        // docs/regions/FEYWILD.md), out of the black the fall left on screen.
        ++SweepEnds;
        SweepEndClock = Clock; SweepEndMusic = -1.f;
        double M = 0; float Len = TrackLength;
        if (ADockGameMode::GetEncounterMusicTime(GetWorld(), M, &Len) && Len > 0.f) SweepEndMusic = static_cast<float>(FMath::Fmod(M, static_cast<double>(Len)));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BLEED_SWEEP_END seconds=%.1f by=%s clock=%.2f end_at=%.2f music_loop=%.2f track=%.2f"), SweepT, bSweptBySurge ? TEXT("surge") : TEXT("river"),
            Clock, AtEnd, SweepEndMusic, Len);
        Rearm();
        Chuck->BeginFeyRiver();
    }
}
