#include "CigarettePickup.h"
#include "DockSewer.h"
#include "SewerLife.h"
#include "ChuckCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float PickupScale = 1.6f;    // a touch larger than the one in his mouth: readable on the stones
    constexpr float PickupVolume = .35f;
    // The golden cigarette's aura (user 2026-10-15: our normal cigarette with a
    // glowing golden ethereal light round it, to stand out to a first-time player).
    constexpr int32 GoldenMotes = 9;
    constexpr float MoteRise = 110.f;      // cm a mote climbs before it fades
    const FLinearColor AuraGold(1.f, .64f, .2f);
}

ACigarettePickup::ACigarettePickup()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cigarette(TEXT("/Game/Characters/Chuck/V1/Cigarette/SM_Cigarette.SM_Cigarette"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paper(TEXT("/Game/Characters/Chuck/V1/Cigarette/M_Cigarette_Paper.M_Cigarette_Paper"));
    Mesh->SetStaticMesh(Cigarette.Object);
    // A fresh one: paper where Chuck's has ash and the ember.
    for (const TCHAR* Slot : {TEXT("Ash"), TEXT("Ember")})
    {
        const int32 Index = Mesh->GetMaterialIndex(FName(Slot));
        if (Index != INDEX_NONE) Mesh->SetMaterial(Index, Paper.Object);
    }
    Mesh->SetWorldScale3D(FVector(PickupScale));
    // The golden one's aura meshes (hard references, so the cook keeps the engine sphere).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mote(TEXT("/Game/Art/Props/Astral/SM_AstralMote.SM_AstralMote"));
    AuraSphere = Sphere.Object; AuraMote = Mote.Object;
}

void ACigarettePickup::BeginPlay()
{
    Super::BeginPlay();
    for (int32 I = 0; I < 3; ++I)
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_Pickup_%02d.SFX_Pickup_%02d"), I, I))) PickupSounds.Add(Sound);
}

ACigarettePickup* ACigarettePickup::Spawn(UWorld* World, const FVector& From, const FVector& Launch, float GroundZ)
{
    auto* Pickup = World->SpawnActor<ACigarettePickup>(From, FRotator(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(0.f, 360.f), 0));
    if (!Pickup) return nullptr;
    Pickup->Velocity = Launch;
    Pickup->Spin = FRotator(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-360.f, 360.f), 0);
    Pickup->Ground = GroundZ;
    Pickup->bLanded = Launch.IsNearlyZero();
    if (From.Z < -150.f && IsWithinDockSewer(From)) UseSewerLighting(Pickup);   // underground: lit by the sewer's own lights
    return Pickup;
}

ACigarettePickup* ACigarettePickup::SpawnGolden(UWorld* World, const FVector& At)
{
    auto* Pickup = Spawn(World, At, FVector::ZeroVector, At.Z - .8f);
    if (!Pickup) return nullptr;
    Pickup->bGolden = true;
    // A placed cigarette lies flat, never pitched into the soil; each at its own angle.
    Pickup->SetActorRotation(FRotator(0, FMath::Fmod(FMath::Abs(At.X * .37f + At.Y * .61f), 360.f), 0));
    Pickup->AddGoldenAura();
    return Pickup;
}

void ACigarettePickup::AddGoldenAura()
{
    UStaticMesh* Sphere = AuraSphere;   // 100 cm across
    UStaticMesh* MoteMesh = AuraMote;
    // Loaded here, not held by the class default: Tools/create_golden_leaf_aura_material.py
    // rebuilds it in the editor (/Game/Art is always cooked).
    UMaterialInterface* Aura = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Pickups/M_GoldenLeafAura.M_GoldenLeafAura"));
    auto Shell = [&](const TCHAR* Name, const FVector& Size, UMaterialInstanceDynamic*& Material)
    {
        auto* Part = NewObject<UStaticMeshComponent>(this, Name);
        Part->SetupAttachment(Mesh);
        Part->SetUsingAbsoluteScale(true);
        Part->SetStaticMesh(Sphere);
        Part->SetWorldScale3D(Size / 100.f);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        Part->SetCanEverAffectNavigation(false);
        Part->RegisterComponent();
        if (Aura) Material = Part->CreateDynamicMaterialInstance(0, Aura);
        return Part;
    };
    // A bright core along the cigarette and a wide faint haze; both fade where they meet the ground.
    Halo = Shell(TEXT("Halo"), FVector(34.f, 20.f, 20.f), HaloMaterial);
    Halo->SetRelativeLocation(FVector(0, 0, 1.f));
    OuterHalo = Shell(TEXT("OuterHalo"), FVector(80.f, 80.f, 64.f), OuterMaterial);
    OuterHalo->SetRelativeLocation(FVector(0, 0, 8.f));
    // A faint upright wisp that shows over the ferns from farther off.
    Wisp = Shell(TEXT("Wisp"), FVector(34.f, 34.f, 240.f), WispMaterial);
    Wisp->SetUsingAbsoluteRotation(true);
    Wisp->SetRelativeLocation(FVector(0, 0, 105.f));
    Motes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Motes"));
    Motes->SetupAttachment(Mesh);
    Motes->SetUsingAbsoluteRotation(true);
    Motes->SetUsingAbsoluteScale(true);
    Motes->SetStaticMesh(MoteMesh);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetCastShadow(false);
    Motes->SetCanEverAffectNavigation(false);
    Motes->RegisterComponent();
    if (Aura) MoteMaterial = Motes->CreateDynamicMaterialInstance(0, Aura);
    for (int32 I = 0; I < GoldenMotes; ++I) Motes->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.f)));
    // A small warm pool of light on the leaf litter round it.
    Glow = NewObject<UPointLightComponent>(this, TEXT("Glow"));
    Glow->SetupAttachment(Mesh);
    Glow->SetRelativeLocation(FVector(0, 0, 9.f));
    Glow->SetIntensityUnits(ELightUnits::Candelas);
    Glow->SetIntensity(6.f);
    Glow->SetAttenuationRadius(180.f);
    Glow->SetLightColor(AuraGold);
    Glow->SetCastShadows(false);
    Glow->SetSpecularScale(.3f);
    Glow->RegisterComponent();
    for (UMaterialInstanceDynamic* M : {HaloMaterial, OuterMaterial, WispMaterial, MoteMaterial})
        if (M) M->SetVectorParameterValue(TEXT("Color"), AuraGold);
    if (WispMaterial) WispMaterial->SetScalarParameterValue(TEXT("Rise"), 1.f);
    AuraPhase = FMath::FRandRange(0.f, 10.f);
    TickGoldenAura();
}

void ACigarettePickup::TickGoldenAura()
{
    // A slow breathing pulse (about 2.6 s), never off; motes drift up round it.
    const float Clock = Age + AuraPhase;
    const float Breath = .5f + .5f * FMath::Sin(Clock * 2.4f);
    if (HaloMaterial) HaloMaterial->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(1.1f, 1.8f, Breath));   // the cigarette stays readable inside it
    if (WispMaterial) WispMaterial->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(.4f, .6f, Breath));
    if (OuterMaterial) OuterMaterial->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(.35f, .6f, Breath));
    if (MoteMaterial) MoteMaterial->SetScalarParameterValue(TEXT("Intensity"), 7.f);
    if (Glow) Glow->SetIntensity(FMath::Lerp(4.5f, 7.f, Breath));
    if (!Motes) return;
    for (int32 I = 0; I < GoldenMotes; ++I)
    {
        const float Period = 2.6f + .37f * (I % 4);
        const float U = FMath::Frac(Clock / Period + I / static_cast<float>(GoldenMotes));
        const float Angle = I * 2.399f + Clock * (I % 2 ? .7f : -.55f);
        const float Radius = FMath::Lerp(9.f, 19.f, FMath::Frac(I * .618f)) * (1.f - .35f * U);
        const FVector At(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 2.f + MoteRise * U);
        const float Size = FMath::Lerp(1.1f, 1.8f, FMath::Frac(I * .37f)) * FMath::Sin(PI * U);   // swell in, shrink out
        Motes->UpdateInstanceTransform(I, FTransform(FRotator::ZeroRotator, At, FVector(Size)), false, I == GoldenMotes - 1);
    }
}

void ACigarettePickup::Burst(UWorld* World, const FVector& Base, int32 Count, float Ground)
{
    const float Start = FMath::FRandRange(0.f, 360.f);
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector Out = FRotator(0, Start + 360.f * I / FMath::Max(1, Count) + FMath::FRandRange(-25.f, 25.f), 0).Vector();
        Spawn(World, FVector(Base.X, Base.Y, Ground + 10.f), Out * FMath::FRandRange(45.f, 85.f) + FVector(0, 0, FMath::FRandRange(170.f, 230.f)), Ground);
    }
}
int32 ACigarettePickup::CountInWorld(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<ACigarettePickup> It(World); It; ++It) if (!It->IsActorBeingDestroyed() && !It->bGolden) ++Count;
    return Count;
}

void ACigarettePickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    if (bGolden && !bCollected) TickGoldenAura();
    if (!bLanded)
    {
        Velocity.Z -= 980.f * DeltaSeconds;
        FVector At = GetActorLocation() + Velocity * DeltaSeconds;
        FRotator Turn = GetActorRotation() + Spin * DeltaSeconds;
        if (At.Z <= Ground + .8f && Velocity.Z < 0)
        {
            // Lands flat on its side (the mesh runs along X), a small skid, stays.
            At.Z = Ground + .8f; bLanded = true;
            Turn = FRotator(0, Turn.Yaw, 0);
        }
        SetActorLocationAndRotation(At, Turn);
    }
    if (Age < CollectDelay) return;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck) return;
    if (bCollected)
    {
        if (Chuck->GetResetGeneration() == CollectedGeneration) return;
        bCollected = false;
        SetActorHiddenInGame(false);
        if (Glow) Glow->SetVisibility(true);
    }
    const FVector To = Chuck->GetActorLocation() - GetActorLocation();
    if (To.Size2D() <= CollectRadius && FMath::Abs(To.Z) < 60.f)
    {
        if (bGolden)
        {
            Chuck->LockStamina(AChuckCharacter::GoldenLeafDuration);
            bCollected = true; CollectedGeneration = Chuck->GetResetGeneration();
            SetActorHiddenInGame(true);
            if (Glow) Glow->SetVisibility(false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_GOLDEN_LEAF collected=1 duration=%.0f"), AChuckCharacter::GoldenLeafDuration);
        }
        else Chuck->AddCigarettes(1);
        if (PickupSounds.Num()) UGameplayStatics::PlaySound2D(this, PickupSounds[FMath::RandRange(0, PickupSounds.Num() - 1)], PickupVolume, FMath::FRandRange(.95f, 1.05f));
        if (!bGolden) Destroy();
    }
}
