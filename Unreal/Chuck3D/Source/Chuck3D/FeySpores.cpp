// The Feywild's spores (Claude, 2026-11-26). See FeySpores.h.
#include "FeySpores.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 MaxGrains = 1400, MaxClouds = 160;
    constexpr float NearDistance = 5000.f, CloudFrames = 64.f;
    float Smooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
    // A quad facing the eye (its +Z), turned Spin about that axis, Size cm across (as ChuckFlames.cpp).
    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Z = Eye - P; if (!Z.Normalize()) Z = FVector::UpVector;
        FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Z, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        const FVector X = X0 * C + Y0 * S, Y = Y0 * C - X0 * S;
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
    UStaticMesh* GSporeQuad = nullptr;
}

int32 AFeySpores::Puffs = 0;

AFeySpores::AFeySpores()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // (after the camera has moved this frame)
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    GSporeQuad = Quad.Object;
    Rand.Initialize(1126);
}

AFeySpores* AFeySpores::Get(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AFeySpores> It(World); It; ++It) return *It;
    AFeySpores* A = World->SpawnActor<AFeySpores>();
    if (!A) return nullptr;
    A->GrainFx = A->Pool(TEXT("SporeGrains"), TEXT("/Game/Art/Feywild/Spores/M_FeySporeMote.M_FeySporeMote"), MaxGrains);
    A->CloudFx = A->Pool(TEXT("SporeClouds"), TEXT("/Game/Art/Feywild/Spores/M_FeySporeCloud.M_FeySporeCloud"), MaxClouds);
    return A;
}

UInstancedStaticMeshComponent* AFeySpores::Pool(const TCHAR* Name, const TCHAR* Material, int32 Count)
{
    auto* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
    C->SetStaticMesh(GSporeQuad);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCastShadow(false);
    C->SetMobility(EComponentMobility::Movable);
    C->NumCustomDataFloats = 6;
    UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Material);
    Missing += M == nullptr || GSporeQuad == nullptr;
    C->SetMaterial(0, M);
    C->SetupAttachment(RootComponent);
    C->RegisterComponent();
    TArray<FTransform> Zero; Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Count);
    C->AddInstances(Zero, false, true);
    return C;
}

void AFeySpores::Grain(const FVector& At, const FVector& V, const FLinearColor& Col, float Life, float Size)
{
    if (Grains.Num() >= MaxGrains) return;
    FGrain G; G.P = At; G.V = V; G.Life = Life; G.Size = Size; G.C = Col; G.Seed = Rand.FRand() * 100.f; G.Buoy = Rand.FRandRange(-6.f, 10.f);
    Grains.Add(G);
}

void AFeySpores::Puff(const FVector& At, int32 Count, float Speed, const FLinearColor& Col, float Rise, int32 NClouds, float CloudSize)
{
    ++Puffs;
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector D = FVector(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-.1f, .6f)).GetSafeNormal();
        Grain(At + Rand.VRand() * Rand.FRandRange(1.f, 8.f), D * Speed * Rand.FRandRange(.3f, 1.f) + FVector(0, 0, Rise * Rand.FRandRange(.5f, 1.2f)),
              Col, Rand.FRandRange(2.4f, 4.6f), Rand.FRandRange(1.8f, 3.6f));
    }
    for (int32 I = 0; I < NClouds && Clouds.Num() < MaxClouds; ++I)
    {
        FCloud C; C.P = At + FVector(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(0.f, .6f)) * CloudSize * .25f;
        C.V = FVector(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f), 0.f) * Speed * .18f + FVector(0, 0, Rise * .35f + Rand.FRandRange(4.f, 14.f));
        C.Life = Rand.FRandRange(2.6f, 4.2f); C.Size0 = CloudSize * Rand.FRandRange(.35f, .55f); C.Size1 = CloudSize * Rand.FRandRange(1.3f, 1.9f);
        C.Frame = Rand.FRand() * CloudFrames; C.Rate = Rand.FRandRange(7.f, 12.f); C.Spin = Rand.FRand() * 2.f * PI; C.SpinRate = Rand.FRandRange(-.5f, .5f);
        C.Peak = Rand.FRandRange(.32f, .5f); C.C = Col;
        Clouds.Add(C);
    }
}

void AFeySpores::AddField(AActor* FieldOwner, const FVector& Centre, const FVector2D& Half, float Yaw, const FLinearColor& Col)
{
    FField F; F.Owner = FieldOwner; F.C = Centre; F.Half = Half; F.Yaw = Yaw; F.Col = Col;
    Fields.Add(F);
}

void AFeySpores::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* World = GetWorld();
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->PlayerCameraManager || !GrainFx || !CloudFx) return;
    const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 1.f / 20.f);
    const float Time = static_cast<float>(World->GetTimeSeconds());
    // The beds near the camera: grains lifting slowly off them, and a faint low haze.
    Fields.RemoveAll([](const FField& F) { return !F.Owner.IsValid(); });
    for (FField& F : Fields)
    {
        if (FVector::Dist(Eye, F.C) > NearDistance + FMath::Max(F.Half.X, F.Half.Y)) continue;
        const FQuat Q = FRotator(0, F.Yaw, 0).Quaternion();
        const float Area = F.Half.X * F.Half.Y * 4.f / 10000.f;   // m^2
        F.Carry += Dt * Area * 3.2f;
        while (F.Carry >= 1.f)
        {
            F.Carry -= 1.f;
            const FVector L(Rand.FRandRange(-1.f, 1.f) * F.Half.X, Rand.FRandRange(-1.f, 1.f) * F.Half.Y, Rand.FRandRange(4.f, 40.f));
            Grain(F.C + Q.RotateVector(L), FVector(Rand.FRandRange(-6.f, 6.f), Rand.FRandRange(-6.f, 6.f), Rand.FRandRange(7.f, 18.f)), F.Col, Rand.FRandRange(4.5f, 8.f), Rand.FRandRange(1.4f, 2.6f));
        }
        F.HazeCarry += Dt * FMath::Max(.25f, Area * .05f);
        while (F.HazeCarry >= 1.f && Clouds.Num() < MaxClouds)
        {
            F.HazeCarry -= 1.f;
            FCloud C; C.P = F.C + Q.RotateVector(FVector(Rand.FRandRange(-.8f, .8f) * F.Half.X, Rand.FRandRange(-.8f, .8f) * F.Half.Y, Rand.FRandRange(25.f, 60.f)));
            C.V = FVector(Rand.FRandRange(-5.f, 5.f), Rand.FRandRange(-5.f, 5.f), Rand.FRandRange(2.f, 6.f));
            C.Life = Rand.FRandRange(6.f, 9.f); C.Size0 = Rand.FRandRange(120.f, 170.f); C.Size1 = C.Size0 * Rand.FRandRange(1.4f, 1.8f);
            C.Frame = Rand.FRand() * CloudFrames; C.Rate = Rand.FRandRange(2.5f, 4.f); C.Spin = Rand.FRand() * 2.f * PI; C.SpinRate = Rand.FRandRange(-.12f, .12f);
            C.Peak = Rand.FRandRange(.16f, .24f); C.C = F.Col;
            Clouds.Add(C);
        }
    }
    // Grains: the air slows them, they wander, the light ones lift and the heavy ones settle.
    for (int32 I = Grains.Num() - 1; I >= 0; --I)
    {
        FGrain& G = Grains[I]; G.Age += Dt;
        if (G.Age >= G.Life || FVector::Dist(Eye, G.P) > NearDistance * 1.2f) { Grains.RemoveAtSwap(I); continue; }
        G.V *= FMath::Exp(-1.6f * Dt);
        G.V += FVector(FMath::Sin(Time * 1.3f + G.Seed), FMath::Cos(Time * 1.1f + G.Seed * 1.7f), 0.f) * 22.f * Dt + FVector(0, 0, G.Buoy * Dt);
        G.P += G.V * Dt;
    }
    for (int32 I = Clouds.Num() - 1; I >= 0; --I)
    {
        FCloud& C = Clouds[I]; C.Age += Dt;
        if (C.Age >= C.Life || FVector::Dist(Eye, C.P) > NearDistance * 1.3f) { Clouds.RemoveAtSwap(I); continue; }
        C.V *= FMath::Exp(-.9f * Dt);
        C.P += C.V * Dt; C.Frame += C.Rate * Dt; C.Spin += C.SpinRate * Dt;
    }
    // Draw.
    const FTransform Hidden(FQuat::Identity, Eye, FVector::ZeroVector);
    TArray<FTransform> GX, CX; GX.Init(Hidden, GrainFx->GetInstanceCount()); CX.Init(Hidden, CloudFx->GetInstanceCount());
    int32 NG = 0, NC = 0;
    for (const FGrain& G : Grains)
    {
        if (NG >= GX.Num()) break;
        const float A = G.Age / G.Life;
        const int32 Index = NG++;
        GX[Index] = Billboard(G.P, Eye, 0.f, G.Size * (1.f - .3f * A));
        const float Tw = .65f + .35f * FMath::Sin(Time * 7.f + G.Seed * 3.f);
        const float Data[6] = {0.f, .25f + .35f * Tw, Smooth01(G.Age / .15f) * (1.f - Smooth01((A - .55f) / .45f)) * Tw, G.C.R, G.C.G, G.C.B};
        GrainFx->SetCustomData(Index, MakeArrayView(Data, 6), false);
    }
    for (const FCloud& C : Clouds)
    {
        if (NC >= CX.Num()) break;
        const float A = C.Age / C.Life;
        const int32 Index = NC++;
        CX[Index] = Billboard(C.P, Eye, C.Spin, FMath::Lerp(C.Size0, C.Size1, Smooth01(A * 1.3f)));
        const float Data[6] = {C.Frame, .35f * (1.f - A), C.Peak * Smooth01(A / .12f) * (1.f - Smooth01((A - .3f) / .7f)), C.C.R, C.C.G, C.C.B};
        CloudFx->SetCustomData(Index, MakeArrayView(Data, 6), false);
    }
    GrainFx->BatchUpdateInstancesTransforms(0, GX, true, false, true);
    CloudFx->BatchUpdateInstancesTransforms(0, CX, true, false, true);
    GrainFx->MarkRenderStateDirty(); CloudFx->MarkRenderStateDirty();
    GrainsDrawn = NG; CloudsDrawn = NC;
}
