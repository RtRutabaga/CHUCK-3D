#include "ChultTempleRubbleGlow.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FLinearColor Gold(1.f, .64f, .2f);   // the golden cigarette's
}

AChultTempleRubbleGlow::AChultTempleRubbleGlow()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // Hard references, so the cook keeps them (as CigarettePickup's).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MoteF(TEXT("/Game/Art/Props/Astral/SM_AstralMote.SM_AstralMote"));
    Sphere = SphereF.Object; MoteMesh = MoteF.Object;
}

void AChultTempleRubbleGlow::Setup(const FVector& At, const FVector& Out)
{
    Base = At; Ahead = Out.GetSafeNormal2D();
    SetActorLocation(At);
    UMaterialInterface* Aura = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Pickups/M_GoldenLeafAura.M_GoldenLeafAura"));
    auto Shell = [&](const TCHAR* Name, const FVector& Size, const FVector& Offset, UMaterialInstanceDynamic*& Mat)
    {
        auto* C = NewObject<UStaticMeshComponent>(this, Name);
        C->SetupAttachment(RootComponent);
        C->SetStaticMesh(Sphere);
        C->SetWorldScale3D(Size / 100.f);
        C->SetWorldRotation(Ahead.Rotation());
        C->SetWorldLocation(At + Offset);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->SetCanEverAffectNavigation(false);
        C->RegisterComponent();
        if (Aura) { Mat = C->CreateDynamicMaterialInstance(0, Aura); Mat->SetVectorParameterValue(TEXT("Color"), Gold); }
        return C;
    };
    // A low haze filling the gap and spilling a little out of it; a faint wisp rising off the pile's face.
    Haze = Shell(TEXT("Haze"), FVector(120.f, 150.f, 80.f), Ahead * 10.f + FVector(0, 0, 22.f), HazeMat);
    Wisp = Shell(TEXT("Wisp"), FVector(70.f, 110.f, 320.f), Ahead * 30.f + FVector(0, 0, 150.f), WispMat);
    if (WispMat) WispMat->SetScalarParameterValue(TEXT("Rise"), 1.f);
    Motes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Motes"));
    Motes->SetupAttachment(RootComponent);
    Motes->SetStaticMesh(MoteMesh);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetCastShadow(false);
    Motes->SetCanEverAffectNavigation(false);
    Motes->SetUsingAbsoluteLocation(true); Motes->SetUsingAbsoluteRotation(true); Motes->SetUsingAbsoluteScale(true);
    Motes->RegisterComponent();
    if (Aura) { MoteMat = Motes->CreateDynamicMaterialInstance(0, Aura); MoteMat->SetVectorParameterValue(TEXT("Color"), Gold); MoteMat->SetScalarParameterValue(TEXT("Intensity"), 6.f); }
    for (int32 I = 0; I < MoteCount; ++I) Motes->AddInstance(FTransform(FRotator::ZeroRotator, At, FVector(0.f)), true);
    // A warm pool on the stones round the gap, and a little light some way in, so the passage reads as going somewhere.
    auto Lamp = [&](const TCHAR* Name, const FVector& Where, float Radius)
    {
        auto* L = NewObject<UPointLightComponent>(this, Name);
        L->SetupAttachment(RootComponent);
        L->SetWorldLocation(Where);
        L->SetIntensityUnits(ELightUnits::Candelas);
        L->SetAttenuationRadius(Radius);
        L->SetLightColor(Gold);
        L->SetCastShadows(false);
        L->SetSpecularScale(.3f);
        L->SetLightingChannels(true, true, false);   // (the interior is on channel 1)
        L->RegisterComponent();
        return L;
    };
    Light = Lamp(TEXT("Light"), At + Ahead * 45.f + FVector(0, 0, 30.f), 380.f);
    Inner = Lamp(TEXT("Inner"), At - Ahead * 120.f + FVector(0, 0, 22.f), 220.f);
    Tick(0.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_GLOW aura=%d motes=%d"), Aura != nullptr, MoteMesh != nullptr);
}

void AChultTempleRubbleGlow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    // The golden cigarette's slow breath (about 2.6 s), fainter: a glow to notice, not a beacon.
    const float Breath = .5f + .5f * FMath::Sin(Clock * 2.4f);
    if (HazeMat) HazeMat->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(.3f, .5f, Breath));
    if (WispMat) WispMat->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(.16f, .26f, Breath));
    if (Light) Light->SetIntensity(FMath::Lerp(14.f, 22.f, Breath));
    if (Inner) Inner->SetIntensity(FMath::Lerp(5.f, 8.f, Breath));
    if (!Motes) return;
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
    for (int32 I = 0; I < MoteCount; ++I)
    {
        const float Period = 3.2f + .41f * (I % 5);
        const float U = FMath::Frac(Clock / Period + I / static_cast<float>(MoteCount));
        const float Across = (FMath::Frac(I * .618f) - .5f) * 110.f + 10.f * FMath::Sin(Clock * .9f + I);
        const FVector At = Base + Ahead * (10.f + 40.f * FMath::Frac(I * .37f) + 20.f * U) + Side * Across + FVector(0, 0, 6.f + 170.f * U);
        const float Size = FMath::Lerp(1.4f, 2.4f, FMath::Frac(I * .29f)) * FMath::Sin(PI * U);
        Motes->UpdateInstanceTransform(I, FTransform(FRotator::ZeroRotator, At, FVector(Size)), true, I == MoteCount - 1);
    }
}
