#include "ClayJar.h"
#include "ClayJarData.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float ShardGravity = 980.f;
    constexpr float ShardRest = 2.4f;      // s the pieces lie there
    constexpr float ShardFade = .5f;       // then sink away
    constexpr float BreakVolume = .5f;
}

AClayJar::AClayJar()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only while shards are out
    Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
    SetRootComponent(Blocker);
    Blocker->InitCapsuleSize(ClayJarData::Radius - .5f, ClayJarData::Height * .5f);
    // Solid to Chuck only: his paw, ledge, mantle and camera traces ignore it.
    Blocker->SetCollisionProfileName(TEXT("Custom"));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Blocker->SetCollisionObjectType(ECC_WorldDynamic);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Blocker->SetCanEverAffectNavigation(false);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(Blocker);
    Body->SetRelativeLocation(FVector(0, 0, -ClayJarData::Height * .5f));   // mesh origin at its base
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Jar(TEXT("/Game/Art/Props/Jar/SM_ClayJar.SM_ClayJar"));
    Body->SetStaticMesh(Jar.Object);
    for (int32 I = 0; I < ClayJarData::ShardCount; ++I)
    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> Shard(*FString::Printf(TEXT("/Game/Art/Props/Jar/SM_JarShard_%02d.SM_JarShard_%02d"), I, I));
        ShardMeshes.Add(Shard.Object);
    }
    HitRadius = ClayJarData::Radius; HitHeight = ClayJarData::Height; bCentred = true;
}

void AClayJar::BeginPlay()
{
    Super::BeginPlay();
    for (int32 I = 0; I < 3; ++I)
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_JarBreak_%02d.SFX_JarBreak_%02d"), I, I))) BreakSounds.Add(Sound);
}

bool AClayJar::BlocksChuck() const { return !bBroken && Blocker->GetCollisionEnabled() != ECollisionEnabled::NoCollision; }

void AClayJar::Break(const FVector& Swing)
{
    if (bBroken) return;
    Super::Break(Swing);
    Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetVisibility(false);
    // The pieces start exactly where they sat in the jar, then burst outward
    // (and on along the swing), tumbling, bounce once and settle.
    const FTransform Jar = Body->GetComponentTransform();
    for (int32 I = 0; I < ClayJarData::ShardCount; ++I)
    {
        const FVector Local(ClayJarData::ShardOffset[I][0], ClayJarData::ShardOffset[I][1], ClayJarData::ShardOffset[I][2]);
        FShard Shard;
        Shard.Position = Jar.TransformPosition(Local);
        const FVector Out = Jar.TransformVector(FVector(Local.X, Local.Y, 0)).GetSafeNormal();
        Shard.Velocity = Out * FMath::FRandRange(50.f, 130.f) + Swing * FMath::FRandRange(40.f, 110.f) + FVector(0, 0, FMath::FRandRange(40.f, 160.f) * (Local.Z / ClayJarData::Height + .3f));
        Shard.Rotation = Jar.Rotator();
        Shard.Spin = FRotator(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-400.f, 400.f));
        Shard.Bounces = 0; Shard.bLanded = false;
        Shards.Add(Shard);
        auto* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetStaticMesh(ShardMeshes[I]);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetUsingAbsoluteLocation(true); Part->SetUsingAbsoluteRotation(true);
        Part->SetupAttachment(Blocker);
        Part->RegisterComponent();
        Part->SetWorldLocationAndRotation(Shard.Position, Shard.Rotation);
        ShardParts.Add(Part);
    }
    BrokenTime = 0;
    SetActorTickEnabled(true);
    if (BreakSounds.Num())
    {
        UGameplayStatics::PlaySound2D(this, BreakSounds[FMath::RandRange(0, BreakSounds.Num() - 1)], BreakVolume, FMath::FRandRange(.95f, 1.05f));
        bSoundPlayed = true;
    }
    DropContents();
}

void AClayJar::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    BrokenTime += DeltaSeconds;
    const float Ground = static_cast<float>(GetActorLocation().Z) - ClayJarData::Height * .5f + 1.f;
    const float Sink = FMath::Clamp((BrokenTime - ShardRest) / ShardFade, 0.f, 1.f);
    for (int32 I = 0; I < Shards.Num(); ++I)
    {
        FShard& Shard = Shards[I];
        if (!Shard.bLanded)
        {
            Shard.Velocity.Z -= ShardGravity * DeltaSeconds;
            Shard.Position += Shard.Velocity * DeltaSeconds;
            Shard.Rotation += Shard.Spin * DeltaSeconds;
            if (Shard.Position.Z <= Ground && Shard.Velocity.Z < 0)
            {
                Shard.Position.Z = Ground;
                if (Shard.Bounces++ == 0 && Shard.Velocity.Z < -120.f)
                {
                    Shard.Velocity = FVector(Shard.Velocity.X * .4f, Shard.Velocity.Y * .4f, -Shard.Velocity.Z * .25f);
                    Shard.Spin *= .4f;
                }
                else
                {
                    // Lie on the stones, curved side down.
                    Shard.bLanded = true;
                    Shard.Rotation = FRotator(FMath::Clamp(Shard.Rotation.Pitch, -20.f, 20.f) * .3f, Shard.Rotation.Yaw, 0.f);
                }
            }
        }
        ShardParts[I]->SetWorldLocationAndRotation(Shard.Position - FVector(0, 0, 4.f * Sink), Shard.Rotation);
        ShardParts[I]->SetWorldScale3D(FVector(1.f - Sink));
    }
    if (Sink >= 1.f)
    {
        for (UStaticMeshComponent* Part : ShardParts) Part->DestroyComponent();
        ShardParts.Reset(); Shards.Reset();
        SetActorTickEnabled(false);
    }
}

AClayJar* AClayJar::Place(UWorld* World, const FVector2D& At, float Yaw, int32 Cigarettes, float MaxZ)
{
    FHitResult Hit;
    const FVector Top(At.X, At.Y, 400.f);
    if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 520.f), ECC_Visibility) || Hit.ImpactNormal.Z < .9f || Hit.ImpactPoint.Z > MaxZ) return nullptr;
    auto* Jar = World->SpawnActor<AClayJar>(Hit.ImpactPoint + FVector(0, 0, ClayJarData::Height * .5f), FRotator(0, Yaw, 0));
    if (Jar) Jar->Cigarettes = Cigarettes;
    return Jar;
}

void AClayJar::SpawnDockJars(UWorld* World)
{
    // Where a dock would keep them: by the tavern door, along the warehouse,
    // in front of the market stalls and in the timber yard. Each holds one to
    // three cigarettes (seeded: the same every run).
    const FVector2D Spots[] = {
        {95, 298}, {118, 285}, {-15, 300},          // tavern door
        {-425, -150}, {-418, -122},                  // warehouse wall
        {-125, -1200}, {-82, -1206}, {68, -1200},    // market stalls, Chandlers' Row
        {640, -440}, {662, -462},                    // timber yard
    };
    FRandomStream Random(20260930);
    int32 Placed = 0;
    for (const FVector2D& Spot : Spots)
        if (Place(World, Spot, Random.FRandRange(0.f, 360.f), Random.RandRange(1, 3))) ++Placed;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_JARS_PLACED %d"), Placed);
}
