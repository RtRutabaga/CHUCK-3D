// The game's fires on flipbook quads (Claude, 2026-10-27; docs/FIRE.md). See ChuckFlames.h.
#include "ChuckFlames.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

using namespace ChuckFlames;

namespace
{
    constexpr float FlameFrames = 64.f, SmokeFrames = 64.f;
    float Smooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

    // A quad facing the eye (its +Z), turned Spin about that axis, Size cm across (ChultTempleJetFire.cpp).
    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Z = Eye - P; if (!Z.Normalize()) Z = FVector::UpVector;
        FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Z, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        const FVector X = X0 * C + Y0 * S, Y = Y0 * C - X0 * S;
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
    // An upright quad turned toward the eye about the vertical, Width x Height cm, its centre at P. The plane's V runs
    // along its +Y and a flipbook's top is V=0, so +Y points down.
    FTransform Upright(const FVector& P, const FVector& Eye, float Width, float Height)
    {
        FVector Z = Eye - P; Z.Z = 0; if (!Z.Normalize()) Z = FVector::ForwardVector;
        const FVector Y = -FVector::UpVector, X = FVector::CrossProduct(Y, Z);
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), P, FVector(Width / 100.f, Height / 100.f, 1.f));
    }
    UStaticMesh* GQuad = nullptr;
}

AChuckFlames::AChuckFlames()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved this frame
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // A hard reference, so the cooker takes the engine plane with the game.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    GQuad = Quad.Object;
}

AChuckFlames* AChuckFlames::Get(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AChuckFlames> It(World); It; ++It) return *It;
    AChuckFlames* A = World->SpawnActor<AChuckFlames>();
    if (!A) return nullptr;
    A->FlameFx = A->Pool(TEXT("ChuckFlameSheets"), TEXT("/Game/Art/Materials/Fire/M_ChuckFlame.M_ChuckFlame"), Near * Sheets + (Far - Near));
    A->SmokeFx = A->Pool(TEXT("ChuckFlameSmoke"), TEXT("/Game/Art/ChultTemple/Fire/M_TempleJetSmoke.M_TempleJetSmoke"), Near * SmokePer);
    A->SparkFx = A->Pool(TEXT("ChuckFlameSparks"), TEXT("/Game/Art/Materials/Fire/M_ChuckEmber.M_ChuckEmber"), Near * SparksPer);
    return A;
}

UInstancedStaticMeshComponent* AChuckFlames::Pool(const TCHAR* Name, const TCHAR* Material, int32 Count)
{
    auto* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
    C->SetStaticMesh(GQuad);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCastShadow(false);
    C->SetMobility(EComponentMobility::Movable);
    C->NumCustomDataFloats = 3;
    UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Material);
    MissingMaterials += M == nullptr || GQuad == nullptr;
    C->SetMaterial(0, M);
    C->SetupAttachment(RootComponent);
    C->RegisterComponent();
    TArray<FTransform> Zero; Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Count);
    C->AddInstances(Zero, false, true);
    return C;
}

void AChuckFlames::Add(AActor* FireOwner, const FVector& Base, float Width, float Height)
{
    FFlame& F = Flames.AddDefaulted_GetRef();
    F.Owner = FireOwner; F.Base = Base; F.Width = Width; F.Height = Height;
    F.Rand.Initialize(FCrc::MemCrc32(&Base, sizeof(Base)) ^ Flames.Num());
    F.Seed = F.Rand.FRand() * 100.f;
    for (int32 S = 0; S < Sheets; ++S) { F.Frames[S] = F.Rand.FRand() * FlameFrames; F.Rates[S] = F.Rand.FRandRange(24.f, 34.f); }
}

void AChuckFlames::Simulate(FFlame& F, float Dt, float Time)
{
    FRandomStream& R = F.Rand;
    const float Size = FMath::Clamp(F.Width / 20.f, .5f, 2.5f);
    // ---- Sparks: a few a second, more from a big fire; up on the heat with a wander, cooling.
    F.SparkCarry += 2.2f * Size * Dt;
    while (F.SparkCarry >= 1.f)
    {
        F.SparkCarry -= 1.f;
        if (F.Sparks.Num() >= SparksPer) continue;
        FMote M;
        const float A = R.FRand() * 2.f * PI, D = R.FRand() * F.Width * .25f;
        M.P = F.Base + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, F.Height * R.FRandRange(.2f, .5f));
        M.V = FVector(R.FRandRange(-25.f, 25.f), R.FRandRange(-25.f, 25.f), R.FRandRange(60.f, 150.f) * FMath::Sqrt(Size));
        M.Life = R.FRandRange(.7f, 1.6f); M.Size0 = R.FRandRange(2.f, 3.4f); M.Size1 = M.Size0 * .5f; M.Seed = R.FRand() * 100.f;
        F.Sparks.Add(M);
    }
    for (int32 K = F.Sparks.Num() - 1; K >= 0; --K)
    {
        FMote& M = F.Sparks[K];
        M.Age += Dt;
        if (M.Age >= M.Life) { F.Sparks.RemoveAtSwap(K); continue; }
        M.V *= FMath::Exp(-1.2f * Dt);
        M.V.Z += 40.f * Dt;
        M.V += FVector(FMath::Sin(M.P.Z * .07f + Time * 5.f + M.Seed), FMath::Sin(M.P.Z * .06f + Time * 4.3f + M.Seed * 1.9f), 0) * 160.f * Dt;
        M.P += M.V * Dt;
    }
    // ---- Smoke: thin wisps from the tip, rising, spreading and thinning.
    F.SmokeCarry += 1.4f * FMath::Clamp(Size, .6f, 2.f) * Dt;
    while (F.SmokeCarry >= 1.f)
    {
        F.SmokeCarry -= 1.f;
        if (F.Smoke.Num() >= SmokePer) continue;
        FMote M;
        M.P = F.Base + FVector(R.FRandRange(-1.f, 1.f) * F.Width * .15f, R.FRandRange(-1.f, 1.f) * F.Width * .15f, F.Height * R.FRandRange(.8f, 1.f));
        M.V = FVector(R.FRandRange(-8.f, 8.f), R.FRandRange(-8.f, 8.f), R.FRandRange(35.f, 55.f) * FMath::Sqrt(Size));
        M.Life = R.FRandRange(2.2f, 3.4f); M.Size0 = F.Width * .8f; M.Size1 = F.Width * R.FRandRange(1.9f, 2.6f);
        M.Frame = R.FRand() * SmokeFrames; M.FrameRate = R.FRandRange(10.f, 16.f);
        M.Spin = R.FRand() * 2.f * PI; M.SpinRate = R.FRandRange(-.4f, .4f); M.Seed = R.FRand() * 100.f;
        F.Smoke.Add(M);
    }
    for (int32 K = F.Smoke.Num() - 1; K >= 0; --K)
    {
        FMote& M = F.Smoke[K];
        M.Age += Dt;
        if (M.Age >= M.Life) { F.Smoke.RemoveAtSwap(K); continue; }
        M.V *= FMath::Exp(-.5f * Dt);
        M.V += FVector(FMath::Sin(M.P.Z * .02f + Time * 1.3f + M.Seed), FMath::Sin(M.P.Z * .018f + Time * 1.1f + M.Seed * 1.4f), 0) * 30.f * Dt;
        M.P += M.V * Dt;
        M.Frame += M.FrameRate * Dt; M.Spin += M.SpinRate * Dt;
    }
}

void AChuckFlames::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Entering Chult hides every actor of the docks' world (AChultLanding::EnterPreview): this one draws for both.
    if (IsHidden()) SetActorHiddenInGame(false);
    UWorld* World = GetWorld();
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->PlayerCameraManager || !FlameFx || !SmokeFx || !SparkFx) return;
    const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
    const FVector Right = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector).GetSafeNormal2D();
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 1.f / 20.f);
    const float Time = static_cast<float>(World->GetTimeSeconds());

    // The fires to draw, nearest first.
    TArray<TPair<float, int32>> Order;
    for (int32 I = 0; I < Flames.Num(); ++I)
    {
        FFlame& F = Flames[I];
        const AActor* O = F.Owner.Get();
        const float D = FVector::Dist(Eye, F.Base);
        if (!O || O->IsHidden() || D > FarDistance) { F.Sparks.Reset(); F.Smoke.Reset(); continue; }
        Order.Add({D, I});
    }
    Order.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });

    const FTransform Hidden(FQuat::Identity, Eye, FVector::ZeroVector);
    TArray<FTransform> FlameX, SmokeX, SparkX;
    FlameX.Init(Hidden, FlameFx->GetInstanceCount()); SmokeX.Init(Hidden, SmokeFx->GetInstanceCount()); SparkX.Init(Hidden, SparkFx->GetInstanceCount());
    int32 NF = 0, NS = 0, NK = 0;
    for (int32 Rank = 0; Rank < Order.Num() && Rank < Far; ++Rank)
    {
        FFlame& F = Flames[Order[Rank].Value];
        const float D = Order[Rank].Key;
        const bool bNear = Rank < Near && D < NearDistance;
        if (bNear) Simulate(F, Dt, Time); else { F.Sparks.Reset(); F.Smoke.Reset(); }
        const float Fade = 1.f - Smooth01((D - FarDistance * .75f) / (FarDistance * .25f));
        // ---- The flame: sheets side by side, each on its own frame and speed, breathing on its own beats.
        const int32 Count = bNear ? Sheets : 1;
        const float Wide = FMath::Clamp((F.Width / FMath::Max(F.Height, 1.f)) / .55f, .75f, 1.5f);
        for (int32 S = 0; S < Count && NF < FlameX.Num(); ++S)
        {
            F.Frames[S] += F.Rates[S] * Dt;
            const float Scale = S == 0 ? 1.f : S == 1 ? .8f : .66f;
            const float Breath = 1.f + .07f * FMath::Sin(Time * 2.3f + F.Seed + S * 2.1f) + .05f * FMath::Sin(Time * 5.1f + F.Seed * 1.7f + S);
            const float H = F.Height * 1.85f * Scale * Breath, W = H * .5f * Wide;   // the flipbook's flame fills about half its cell
            const float Off = S == 0 ? 0.f : S == 1 ? F.Width * .22f : -F.Width * .25f;
            const FVector Centre = F.Base + Right * Off + FVector(0, 0, H * .42f - F.Height * .04f);
            const int32 Index = NF++;
            FlameX[Index] = Upright(Centre, Eye, W, H);
            const float Data[3] = {F.Frames[S], S == 0 ? .95f : .82f, Fade * (S == 0 ? 1.f : .8f)};
            FlameFx->SetCustomData(Index, MakeArrayView(Data, 3), false);
        }
        if (!bNear) continue;
        for (const FMote& M : F.Sparks)
        {
            if (NK >= SparkX.Num()) break;
            const float A = M.Age / M.Life;
            const int32 Index = NK++;
            SparkX[Index] = Billboard(M.P, Eye, 0.f, FMath::Lerp(M.Size0, M.Size1, A));
            const float Data[3] = {0.f, 1.f - A, Smooth01(M.Age / .06f) * (1.f - Smooth01((A - .6f) / .4f)) * (.7f + .3f * FMath::Sin(Time * 31.f + M.Seed))};
            SparkFx->SetCustomData(Index, MakeArrayView(Data, 3), false);
        }
        for (const FMote& M : F.Smoke)
        {
            if (NS >= SmokeX.Num()) break;
            const float A = M.Age / M.Life;
            const int32 Index = NS++;
            SmokeX[Index] = Billboard(M.P, Eye, M.Spin, FMath::Lerp(M.Size0, M.Size1, A));
            const float Data[3] = {M.Frame, .5f * FMath::Max(0.f, 1.f - A * 3.f), .2f * Smooth01(A / .2f) * (1.f - Smooth01((A - .35f) / .65f))};
            SmokeFx->SetCustomData(Index, MakeArrayView(Data, 3), false);
        }
    }
    FlameFx->BatchUpdateInstancesTransforms(0, FlameX, true, false, true);
    SmokeFx->BatchUpdateInstancesTransforms(0, SmokeX, true, false, true);
    SparkFx->BatchUpdateInstancesTransforms(0, SparkX, true, false, true);
    FlameFx->MarkRenderStateDirty(); SmokeFx->MarkRenderStateDirty(); SparkFx->MarkRenderStateDirty();
    DrawnLast = NF; SparksLast = NK;
}
