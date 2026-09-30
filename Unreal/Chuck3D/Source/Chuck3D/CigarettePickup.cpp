#include "CigarettePickup.h"
#include "ChuckCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float PickupScale = 1.6f;    // a touch larger than the one in his mouth: readable on the stones
    constexpr float PickupVolume = .35f;
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
    return Pickup;
}

int32 ACigarettePickup::CountInWorld(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<ACigarettePickup> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++Count;
    return Count;
}

void ACigarettePickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
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
    const FVector To = Chuck->GetActorLocation() - GetActorLocation();
    if (To.Size2D() <= CollectRadius && FMath::Abs(To.Z) < 60.f)
    {
        Chuck->AddCigarettes(1);
        if (PickupSounds.Num()) UGameplayStatics::PlaySound2D(this, PickupSounds[FMath::RandRange(0, PickupSounds.Num() - 1)], PickupVolume, FMath::FRandRange(.95f, 1.05f));
        Destroy();
    }
}
