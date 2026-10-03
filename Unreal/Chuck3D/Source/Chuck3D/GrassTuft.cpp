#include "GrassTuft.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 ClippingCount = 18;
    constexpr float ClippingGravity = 900.f;   // cm/s^2, a little floaty for light blades
    constexpr float ClippingLife = 1.3f;       // s until the last has faded
    constexpr float ShredVolume = .45f;        // under the 0.45 soundtrack, like the slash
}

AGrassTuft::AGrassTuft()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only while clippings fly
    Blades = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blades"));
    SetRootComponent(Blades);
    Blades->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Blades->SetCanEverAffectNavigation(false);
    Clippings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Clippings"));
    Clippings->SetupAttachment(Blades);
    Clippings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Clippings->SetCastShadow(false);
    Clippings->SetUsingAbsoluteLocation(true); Clippings->SetUsingAbsoluteRotation(true); Clippings->SetUsingAbsoluteScale(true);
    for (const TCHAR* Key : {TEXT("A"), TEXT("B"), TEXT("C")})
    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> Tuft(*FString::Printf(TEXT("/Game/Art/Props/Grass/SM_GrassTuft_%s.SM_GrassTuft_%s"), Key, Key));
        ConstructorHelpers::FObjectFinder<UStaticMesh> Stub(*FString::Printf(TEXT("/Game/Art/Props/Grass/SM_GrassStub_%s.SM_GrassStub_%s"), Key, Key));
        TuftMeshes.Add(Tuft.Object); StubMeshes.Add(Stub.Object);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Clip(TEXT("/Game/Art/Props/Grass/SM_GrassClipping.SM_GrassClipping"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Moss(TEXT("/Game/Art/Props/Grass/M_SewerMoss.M_SewerMoss"));
    MossMaterial = Moss.Object;
    Clippings->SetStaticMesh(Clip.Object);
    Blades->SetStaticMesh(TuftMeshes[0]);
    HitRadius = 18.f; HitHeight = 25.f;
}

void AGrassTuft::BeginPlay()
{
    Super::BeginPlay();
    for (int32 I = 0; I < 3; ++I)
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_Shred_%02d.SFX_Shred_%02d"), I, I))) ShredSounds.Add(Sound);
}

void AGrassTuft::SetVariant(int32 Index)
{
    Variant = FMath::Clamp(Index, 0, TuftMeshes.Num() - 1);
    Blades->SetStaticMesh(bBroken ? StubMeshes[Variant] : TuftMeshes[Variant]);
}

void AGrassTuft::Break(const FVector& Swing)
{
    if (bBroken) return;
    Super::Break(Swing);
    Blades->SetStaticMesh(StubMeshes[Variant]);
    // The cut blades fly along the swing, up and scattered, spinning.
    const float Scale = GetActorScale3D().X;
    const FVector Base = GetActorLocation();
    Flying.Reset();
    for (int32 I = 0; I < ClippingCount; ++I)
    {
        const FVector2D Spot = FMath::RandPointInCircle(8.f * Scale);
        FClipping Clip;
        Clip.Position = Base + FVector(Spot.X, Spot.Y, FMath::FRandRange(6.f, 20.f) * Scale);
        const FVector Scatter = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 70.f;
        Clip.Velocity = Swing * FMath::FRandRange(90.f, 200.f) + Scatter + FVector(0, 0, FMath::FRandRange(110.f, 230.f));
        Clip.Rotation = FRotator(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-90.f, 90.f));
        Clip.Spin = FRotator(FMath::FRandRange(-700.f, 700.f), FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-700.f, 700.f));
        Clip.Scale = FMath::FRandRange(1.5f, 2.3f);   // ~8-12 cm pieces: readable at play distance
        Clip.bLanded = false;
        Flying.Add(Clip);
        Clippings->AddInstance(FTransform(Clip.Rotation, Clip.Position, FVector(Clip.Scale)), true);
    }
    FlyTime = 0;
    SetActorTickEnabled(true);
    DropContents();
    if (ShredSounds.Num())
    {
        UGameplayStatics::PlaySound2D(this, ShredSounds[FMath::RandRange(0, ShredSounds.Num() - 1)], ShredVolume, FMath::FRandRange(.95f, 1.05f));
        bSoundPlayed = true;
    }
}

void AGrassTuft::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FlyTime += DeltaSeconds;
    const float Ground = static_cast<float>(GetActorLocation().Z) + .4f;
    // Fade (shrink) over the last 0.4 s.
    const float Fade = 1.f - FMath::Clamp((FlyTime - (ClippingLife - .4f)) / .4f, 0.f, 1.f);
    for (int32 I = 0; I < Flying.Num(); ++I)
    {
        FClipping& Clip = Flying[I];
        if (!Clip.bLanded)
        {
            Clip.Velocity.Z -= ClippingGravity * DeltaSeconds;
            Clip.Velocity *= FMath::Max(0.f, 1.f - 1.8f * DeltaSeconds);   // air drag on a light blade
            Clip.Position += Clip.Velocity * DeltaSeconds;
            Clip.Rotation += Clip.Spin * DeltaSeconds;
            if (Clip.Position.Z <= Ground && Clip.Velocity.Z < 0)
            {
                // Settle flat on the ground where it fell.
                Clip.Position.Z = Ground; Clip.bLanded = true;
                Clip.Rotation = FRotator(0, Clip.Rotation.Yaw, 0);
            }
        }
        Clippings->UpdateInstanceTransform(I, FTransform(Clip.Rotation, Clip.Position, FVector(Clip.Scale * Fade)), true, I == Flying.Num() - 1);
    }
    if (FlyTime >= ClippingLife)
    {
        Clippings->ClearInstances();
        Flying.Reset();
        SetActorTickEnabled(false);
    }
}

AGrassTuft* AGrassTuft::Plant(UWorld* World, const FVector2D& At, int32 Variant, float Yaw, float Scale, float MaxZ)
{
    FHitResult Hit;
    const FVector Top(At.X, At.Y, 400.f);
    if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 520.f), ECC_Visibility) || Hit.ImpactNormal.Z < .9f || Hit.ImpactPoint.Z > MaxZ) return nullptr;
    auto* Tuft = World->SpawnActor<AGrassTuft>(Hit.ImpactPoint, FRotator(0, Yaw, 0));
    if (!Tuft) return nullptr;
    Tuft->SetActorScale3D(FVector(Scale));
    Tuft->SetVariant(Variant);
    return Tuft;
}

AGrassTuft* AGrassTuft::PlantAt(UWorld* World, const FVector& Ground, int32 Variant, float Yaw, float Scale)
{
    FHitResult Hit;
    const FVector Top = Ground + FVector(0, 0, 60.f);
    if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 150.f), ECC_Visibility) || Hit.ImpactNormal.Z < .8f) return nullptr;
    auto* Tuft = World->SpawnActor<AGrassTuft>(Hit.ImpactPoint, FRotator(0, Yaw, 0));
    if (!Tuft) return nullptr;
    Tuft->SetActorScale3D(FVector(Scale));
    Tuft->SetVariant(Variant);
    return Tuft;
}

void AGrassTuft::SetMoss()
{
    // Squat and spread like a clump of moss rather than standing grass.
    SetActorScale3D(GetActorScale3D() * FVector(1.25f, 1.25f, .5f));
    if (!MossMaterial) return;
    for (int32 I = 0; I < Blades->GetNumMaterials(); ++I) Blades->SetMaterial(I, MossMaterial);
    for (int32 I = 0; I < Clippings->GetNumMaterials(); ++I) Clippings->SetMaterial(I, MossMaterial);
}

void AGrassTuft::SpawnDockGrass(UWorld* World)
{
    // Weeds where they would really grow: along building bases and walls, the
    // yard's corners, the quay edge, the Chandlers' Row garden and the timber
    // yard, plus a patch by the start so they're found straight away. Seeded
    // per patch: the same layout every run. Ground level only (MaxZ).
    struct FPatch { FVector2D Center; FVector2D Extent; int32 Count; int32 Seed; };
    const FPatch Patches[] = {
        {{-235, -95}, {35, 25}, 6, 1},        // by the start
        {{-235, 300}, {95, 10}, 7, 2},        // along the tavern front's base
        {{-425, 120}, {10, 110}, 6, 3},       // along the warehouse's back wall
        {{-310, -290}, {95, 12}, 6, 4},       // between and around the cargo stacks
        {{150, -365}, {40, 15}, 4, 5},        // round the mooring plinth
        {{185, 200}, {10, 120}, 5, 6},        // along the quay edge by the pier
        {{265, -1400}, {25, 230}, 12, 7},     // the Chandlers' Row garden, east of its wall
        {{430, -470}, {60, 40}, 6, 8},        // timber yard, by the lumber
        {{840, -880}, {45, 60}, 6, 9},        // timber yard, far corner
    };
    int32 Planted = 0;
    for (const FPatch& Patch : Patches)
    {
        FRandomStream Random(20260930 + Patch.Seed);
        for (int32 I = 0; I < Patch.Count; ++I)
        {
            const FVector2D At = Patch.Center + FVector2D(Random.FRandRange(-1.f, 1.f) * Patch.Extent.X, Random.FRandRange(-1.f, 1.f) * Patch.Extent.Y);
            const int32 Shape = Random.RandRange(0, 2);
            const float Yaw = Random.FRandRange(0.f, 360.f), Size = Random.FRandRange(.85f, 1.2f);
            // About one tuft in three hides a cigarette (Zelda's grass and rupees).
            const bool bCigarette = Random.FRand() < .33f;
            if (AGrassTuft* Tuft = Plant(World, At, Shape, Yaw, Size)) { Tuft->Cigarettes = bCigarette ? 1 : 0; ++Planted; }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_GRASS_PLANTED %d"), Planted);
}
