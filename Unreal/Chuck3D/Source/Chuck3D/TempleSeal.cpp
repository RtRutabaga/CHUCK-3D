#include "TempleSeal.h"
#include "ChultTempleInterior.h"
#include "ChultTempleWind.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    using namespace TempleWind;
    constexpr int32 MotePool = 220;
    const FTransform Hidden(FQuat::Identity, FVector(0, 0, -1e6), FVector(.001f));
    const FLinearColor Sea(.22f, 1.f, .76f);      // the binding's sea-green (the wizard's gem and bolts)
    const FLinearColor Spray(.75f, .92f, 1.f);

    FTransform Billboard(const FVector& P, const FVector& Eye, float Size)
    {
        FVector Zv = Eye - P; if (!Zv.Normalize()) Zv = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Zv.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Zv).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Zv, X0);
        return FTransform(FMatrix(X0, Y0, Zv, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
}

ATempleSeal::ATempleSeal()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto Proc = [this](const TCHAR* Name)
    {
        auto* P = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
        P->SetupAttachment(RootComponent);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        P->SetCastShadow(false);
        P->SetLightingChannels(false, true, false);
        P->bUseAsyncCooking = false;
        return P;
    };
    WebMesh = Proc(TEXT("Web"));
    Threads = Proc(TEXT("Threads"));
    Motes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Motes"));
    Motes->SetupAttachment(RootComponent);
    Motes->SetUsingAbsoluteLocation(true); Motes->SetUsingAbsoluteRotation(true); Motes->SetUsingAbsoluteScale(true);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision); Motes->SetCastShadow(false);
    Motes->NumCustomDataFloats = 4; Motes->SetMobility(EComponentMobility::Movable);
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(RootComponent);
    Glow->SetLightColor(Sea); Glow->SetIntensity(0.f); Glow->SetAttenuationRadius(600.f); Glow->SetCastShadows(false);
    Glow->SetLightingChannels(true, true, false);
}

void ATempleSeal::Setup(ATempleSanctumBattle* InBattle, UMaterialInterface* Web, UMaterialInterface* Thread, UMaterialInterface* Spark, UStaticMesh* Plane)
{
    Battle = InBattle;
    Random.Initialize(20261025);
    Centre = ChultTempleInteriorWorldOf(FVector(SeaS, SeaT, HZ));
    SetActorLocation(Centre);
    Glow->SetWorldLocation(Centre + FVector(0, 0, 70));
    if (Web) WebMesh->SetMaterial(0, Web);
    if (Thread) Threads->SetMaterial(0, Thread);
    if (Plane && Spark)
    {
        Motes->SetStaticMesh(Plane); Motes->SetMaterial(0, Spark);
        TArray<FTransform> X; X.Init(Hidden, MotePool); Motes->AddInstances(X, false, true);
    }
    bReady = Web && Thread && Spark && Plane;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SEAL_SPAWN ready=%d at=%s"), bReady ? 1 : 0, *Centre.ToCompactString());
}

FVector ATempleSeal::Rim(float A, float Grow, float Z) const
{
    // The tear's own ragged outline (ChultTempleSanctumSeaRadius), in the hall's frame.
    const float R = ChultTempleSanctumSeaRadius(A) + Grow;
    return ChultTempleInteriorWorldOf(FVector(SeaS + R * FMath::Cos(A), SeaT + R * FMath::Sin(A), HZ + Z));
}

void ATempleSeal::Work(const FVector& InGem, const FVector& InPalm)
{
    Gem = InGem; Palm = InPalm; LastWork = T;
}

void ATempleSeal::Mote(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Grav)
{
    if (Parts.Num() >= MotePool) Parts.RemoveAt(0, 1, EAllowShrinking::No);
    Parts.Add({At, Vel, Col, Size, 0.f, Life, Grav});
}

void ATempleSeal::Surge()
{
    // The sea pushes back through it: spray bursts up out of the tear, the web is torn back from the middle, and the
    // wizard is wrenched toward it.
    ++Surges; SurgeAt = T; Flash = 1.f;
    const float Lost = Random.FRandRange(.12f, .24f);
    Coverage = FMath::Max(0.f, Coverage - Lost);
    for (int32 K = 0; K < 90; ++K)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), U = FMath::Sqrt(Random.FRand()) * .9f;
        const FVector P = FMath::Lerp(Centre, Rim(A, 0.f), U) - FVector(0, 0, 20);
        Mote(P, FVector(Random.FRandRange(-120.f, 120.f), Random.FRandRange(-120.f, 120.f), Random.FRandRange(350.f, 900.f)), Spray, Random.FRandRange(3.f, 8.f), Random.FRandRange(.6f, 1.3f), 980.f);
    }
    for (int32 K = 0; K < 30; ++K)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI);
        const FVector P = FMath::Lerp(Rim(A, 0.f), Centre, 1.f - Coverage);
        Mote(P, (P - Centre).GetSafeNormal() * Random.FRandRange(200.f, 500.f) + FVector(0, 0, 250), Sea, Random.FRandRange(2.f, 4.f), .6f, 600.f);
    }
    if (ATempleSanctumBattle* B = Battle.Get())
    {
        B->Sound(ATempleSanctumBattle::ESound::SealSurge, Centre, 1.f);
        if (ATempleHero* Wz = B->GetHero(1)) if (IsWorked()) Wz->SealSurge();
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SEAL_SURGE coverage=%.2f lost=%.2f"), Coverage, Lost);
}

void ATempleSeal::Break(const FVector& From)
{
    if (Coverage < .03f && T - BreakAt < 1.f) return;
    ++Breaks; BreakAt = T; Flash = 1.f;
    // The web flies apart in shards of light, away from the blow.
    for (int32 K = 0; K < 110; ++K)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), U = FMath::Lerp(1.f - Coverage, 1.f, Random.FRand());
        const FVector P = FMath::Lerp(Centre, Rim(A, 5.f), U) + FVector(0, 0, 5);
        const FVector Away = ((P - From).GetSafeNormal2D() + FVector(0, 0, .6f)).GetSafeNormal();
        Mote(P, Away * Random.FRandRange(250.f, 700.f) + FMath::VRand() * 80.f, Sea, Random.FRandRange(2.f, 5.f), Random.FRandRange(.5f, 1.1f), 500.f);
    }
    Coverage = 0.f;
    if (ATempleSanctumBattle* B = Battle.Get()) B->Sound(ATempleSanctumBattle::ESound::SealBreak, Centre, 1.f);
    NextSurge = T + Random.FRandRange(6.f, 9.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SEAL_BREAK breaks=%d"), Breaks);
}

void ATempleSeal::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bReady) return;
    Dt = FMath::Min(Dt, .1f);
    T += Dt;
    if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0)) CamEye = Cam->GetCameraLocation();
    const bool bWorked = IsWorked();
    // It knits inward from the rim while he works it (a little over 20 s to its most), and frays when he stops.
    if (bWorked) Coverage = FMath::Min(MaxKnit, Coverage + .045f * Dt);
    else Coverage = FMath::Max(0.f, Coverage - .03f * Dt);
    MaxCoverage = FMath::Max(MaxCoverage, Coverage);
    // The tear fights it: a surge every 6-10 s once there is something to fight.
    if (bWorked && Coverage > .12f && T >= NextSurge) { Surge(); NextSurge = T + Random.FRandRange(6.f, 10.f); }
    else if (Coverage <= .12f) NextSurge = FMath::Max(NextSurge, T + 5.f);   // it gathers itself while the web is new
    Flash = FMath::Max(0.f, Flash - Dt * 2.5f);
    // Sparkle along the knitting edge; frayed threads drifting off it when it is left alone.
    if (Coverage > .02f)
        for (int32 K = 0; K < (bWorked ? 3 : 1); ++K)
        {
            const float A = Random.FRandRange(0.f, 2.f * PI);
            const FVector P = FMath::Lerp(Rim(A, 0.f), Centre, Coverage);
            Mote(P, bWorked ? FVector(0, 0, Random.FRandRange(10.f, 40.f)) : FVector(0, 0, 60) + FMath::VRand() * 30.f, Sea, Random.FRandRange(1.5f, 3.f), Random.FRandRange(.3f, .7f));
        }
    if (bWorked && Random.FRand() < .6f)
    {
        // Light running down the threads from the gem into the edge.
        const float A = Random.FRandRange(0.f, 2.f * PI);
        const FVector P = FMath::Lerp(Rim(A, 0.f), Centre, Coverage);
        Mote(Gem, (P - Gem) * 1.6f, Sea, 2.5f, .6f);
    }
    for (FMote& M : Parts) { M.Age += Dt; M.Vel.Z -= M.Grav * Dt; M.Vel *= FMath::Exp(-1.2f * Dt); M.Pos += M.Vel * Dt; }
    Parts.RemoveAll([](const FMote& M) { return M.Age > M.Life; });
    Glow->SetLightColor(FMath::Lerp(Sea, Spray, Flash));
    Glow->SetIntensity((bWorked ? 900.f + 4200.f * Coverage : 400.f + 1500.f * Coverage) * (.85f + .15f * FMath::Sin(T * 9.f)) + 26000.f * Flash * Flash);
    DrawWeb();
    DrawThreads();
    DrawMotes();
}

void ATempleSeal::DrawWeb()
{
    if (Coverage < .005f && Flash < .01f) { if (WebMesh->GetNumSections()) WebMesh->ClearAllMeshSections(); return; }
    // A disc over the tear that follows its ragged rim (a little beyond it). UV is the disc's own plane (0..1, centre .5);
    // vertex colour: R the surge's flash, G how far it is knitted, A its strength.
    constexpr int32 Rings = 14, Around = 72;
    const FTransform& X = WebMesh->GetComponentTransform();
    TArray<FVector> V, N; TArray<int32> I; TArray<FLinearColor> C; TArray<FVector2D> UV;
    const float Strength = (IsWorked() ? 1.f : .6f) * (.85f + .15f * FMath::Sin(T * 5.3f));
    for (int32 J = 0; J <= Rings; ++J)
    {
        const float R = J / static_cast<float>(Rings);
        for (int32 K = 0; K <= Around; ++K)
        {
            const float A = K / static_cast<float>(Around) * 2.f * PI;
            V.Add(X.InverseTransformPosition(FMath::Lerp(Centre + FVector(0, 0, 3), Rim(A, 9.f), R)));
            N.Add(FVector::UpVector);
            UV.Add(FVector2D(.5f + .5f * R * FMath::Cos(A), .5f + .5f * R * FMath::Sin(A)));
            C.Add(FLinearColor(Flash, Coverage, 0.f, Strength));
        }
    }
    for (int32 J = 0; J < Rings; ++J)
        for (int32 K = 0; K < Around; ++K)
        {
            const int32 A0 = J * (Around + 1) + K, B0 = A0 + Around + 1;
            I.Append({A0, A0 + 1, B0 + 1, A0, B0 + 1, B0});
        }
    WebMesh->CreateMeshSection_LinearColor(0, V, I, N, UV, C, TArray<FProcMeshTangent>(), false);
}

void ATempleSeal::DrawThreads()
{
    if (!IsWorked()) { if (Threads->GetNumSections()) Threads->ClearAllMeshSections(); return; }
    // Threads of light from the gem (five) and from the open hand (three) arcing down to the knitting edge, swaying,
    // plucked taut in a surge.
    const FTransform& X = Threads->GetComponentTransform();
    TArray<FVector> V, N; TArray<int32> I; TArray<FLinearColor> C; TArray<FVector2D> UV;
    const float Taut = FMath::Clamp(1.f - (T - SurgeAt) / .8f, 0.f, 1.f);
    auto Strip = [&](const TArray<FVector>& P, float Width, float Alpha, float Seed)
    {
        const int32 Base = V.Num(); float Along = Seed;
        for (int32 K = 0; K < P.Num(); ++K)
        {
            const FVector Tan = (P[FMath::Min(K + 1, P.Num() - 1)] - P[FMath::Max(K - 1, 0)]).GetSafeNormal();
            const FVector Side = FVector::CrossProduct(Tan, (CamEye - P[K]).GetSafeNormal()).GetSafeNormal() * Width * .5f;
            if (K) Along += static_cast<float>(FVector::Dist(P[K], P[K - 1])) / 100.f;
            const float Ends = FMath::Min(1.f, FMath::Min(K, P.Num() - 1 - K) / 3.f + .3f);
            V.Add(X.InverseTransformPosition(P[K] - Side)); V.Add(X.InverseTransformPosition(P[K] + Side));
            C.Add(FLinearColor(Sea.R, Sea.G, Sea.B, Alpha * Ends)); C.Add(FLinearColor(Sea.R, Sea.G, Sea.B, Alpha * Ends));
            UV.Add(FVector2D(0, Along)); UV.Add(FVector2D(1, Along));
            N.Add(FVector::UpVector); N.Add(FVector::UpVector);
        }
        for (int32 K = 0; K + 1 < P.Num(); ++K) { const int32 A = Base + K * 2; I.Append({A, A + 1, A + 3, A, A + 3, A + 2}); }
    };
    auto Thread = [&](const FVector& From, float Angle, float Sag, float Seed)
    {
        const FVector To = FMath::Lerp(Rim(Angle, 0.f, 4.f), Centre + FVector(0, 0, 4), Coverage);
        const FVector Mid = (From + To) * .5f + FVector(0, 0, Sag * (1.f - Taut));
        const FVector Across = FVector::CrossProduct((To - From).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
        TArray<FVector> P;
        constexpr int32 Segs = 18;
        for (int32 K = 0; K <= Segs; ++K)
        {
            const float U = K / static_cast<float>(Segs);
            const FVector Q = FMath::Lerp(FMath::Lerp(From, Mid, U), FMath::Lerp(Mid, To, U), U);
            const float Sway = FMath::Sin(U * PI) * (4.f * FMath::Sin(T * 3.f + Seed * 7.f + U * 6.f) + 7.f * Taut * FMath::Sin(T * 45.f + Seed));
            P.Add(Q + Across * Sway);
        }
        Strip(P, 6.f, .12f, Seed);
        Strip(P, 1.3f, .55f, Seed + 3.f);
    };
    for (int32 K = 0; K < 5; ++K) Thread(Gem, T * .35f + K * 2.f * PI / 5.f, 18.f, K * 1.7f);
    for (int32 K = 0; K < 3; ++K) Thread(Palm, -T * .5f + K * 2.f * PI / 3.f + .5f, 15.f, 10.f + K * 2.3f);
    Threads->CreateMeshSection_LinearColor(0, V, I, N, UV, C, TArray<FProcMeshTangent>(), false);
}

void ATempleSeal::DrawMotes()
{
    TArray<FTransform> X; X.Init(Hidden, MotePool);
    for (int32 K = 0; K < MotePool; ++K)
    {
        float D[4] = {0, 0, 0, 0};
        if (K < Parts.Num())
        {
            const FMote& M = Parts[K];
            const float U = M.Age / M.Life;
            X[K] = Billboard(M.Pos, CamEye, M.Size * (1.f - .3f * U));
            D[0] = M.Col.R; D[1] = M.Col.G; D[2] = M.Col.B; D[3] = FMath::Clamp(U / .1f, 0.f, 1.f) * (1.f - U);
        }
        Motes->SetCustomData(K, MakeArrayView(D, 4), false);
    }
    Motes->BatchUpdateInstancesTransforms(0, X, true, true);
}
