#include "AstralSummon.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

int32 AAstralSummon::Started = 0;

namespace
{
    constexpr int32 MoteCount = 26;
    constexpr float AstralVolume = .5f;
    // 0 -> 1 -> 0 over [A, Peak, B] with smooth shoulders.
    float Bell(float T, float A, float Peak, float B)
    {
        if (T <= A || T >= B) return 0.f;
        return T < Peak ? FMath::SmoothStep(A, Peak, T) : 1.f - FMath::SmoothStep(Peak, B, T);
    }
}

AAstralSummon::AAstralSummon()
{
    PrimaryActorTick.bCanEverTick = true;
    auto Part = [this](const TCHAR* Name) {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        return Mesh;
    };
    Circle = Part(TEXT("Circle"));
    SetRootComponent(Circle);
    Column = Part(TEXT("Column"));
    Column->SetupAttachment(Circle);
    Motes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Motes"));
    Motes->SetupAttachment(Circle);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CircleMesh(TEXT("/Game/Art/Props/Astral/SM_AstralCircle.SM_AstralCircle"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ColumnMesh(TEXT("/Game/Art/Props/Astral/SM_AstralColumn.SM_AstralColumn"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MoteMesh(TEXT("/Game/Art/Props/Astral/SM_AstralMote.SM_AstralMote"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Summon(TEXT("/Game/Art/Audio/SFX/SFX_AstralSummon_00.SFX_AstralSummon_00"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Vanish(TEXT("/Game/Art/Audio/SFX/SFX_AstralVanish_00.SFX_AstralVanish_00"));
    Circle->SetStaticMesh(CircleMesh.Object);
    Column->SetStaticMesh(ColumnMesh.Object);
    Motes->SetStaticMesh(MoteMesh.Object);
    SummonSound = Summon.Object; VanishSound = Vanish.Object;
}

AAstralSummon* AAstralSummon::Start(UWorld* World, const FVector& Ground, bool bSummon)
{
    auto* Fx = World->SpawnActor<AAstralSummon>(Ground, FRotator::ZeroRotator);
    if (!Fx) return nullptr;
    Fx->bSummon = bSummon;
    Fx->CircleGlow = Fx->Circle->CreateDynamicMaterialInstance(0);
    Fx->ColumnGlow = Fx->Column->CreateDynamicMaterialInstance(0);
    Fx->MoteGlow = Fx->Motes->CreateDynamicMaterialInstance(0);
    // A narrower column for the vanish: it rises from him, not round a circle.
    Fx->Circle->SetWorldScale3D(FVector(bSummon ? 1.f : .7f));
    Fx->Column->SetRelativeScale3D(FVector(bSummon ? 1.f : .9f, bSummon ? 1.f : .9f, bSummon ? .95f : .85f));
    for (int32 I = 0; I < MoteCount; ++I)
    {
        FMote Mote{FMath::FRandRange(0.f, UE_TWO_PI), FMath::FRandRange(18.f, 42.f), FMath::FRandRange(.9f, 1.8f) * (FMath::RandBool() ? 1.f : -1.f),
                   FMath::FRandRange(55.f, 95.f), FMath::FRandRange(0.f, .5f)};
        Fx->MoteSeeds.Add(Mote);
        Fx->Motes->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.f)));
    }
    if (USoundBase* Sound = bSummon ? Fx->SummonSound : Fx->VanishSound)
        UGameplayStatics::PlaySound2D(Fx, Sound, AstralVolume);
    ++Started;
    return Fx;
}

void AAstralSummon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    const float Length = bSummon ? SummonLength : VanishLength;
    const float Peak = bSummon ? SummonPeak : VanishPeak;
    // Circle: in first and out last. Column: swells to the peak, then gone.
    // Motes: spiral up and inward toward the peak, then drift up and fade.
    const float CircleLevel = bSummon ? Bell(Clock, 0.f, .45f, Length) : .6f * Bell(Clock, 0.f, .3f, Length);
    const float ColumnLevel = Bell(Clock, bSummon ? .35f : .1f, Peak, Peak + (bSummon ? .8f : .6f));
    const float MoteLevel = Bell(Clock, 0.f, Peak * .8f, Length);
    if (CircleGlow) CircleGlow->SetScalarParameterValue(TEXT("Intensity"), 1.6f * CircleLevel);
    if (ColumnGlow) ColumnGlow->SetScalarParameterValue(TEXT("Intensity"), .9f * ColumnLevel);   // a haze, not a pillar
    if (MoteGlow) MoteGlow->SetScalarParameterValue(TEXT("Intensity"), 9.f * MoteLevel);
    for (int32 I = 0; I < MoteSeeds.Num(); ++I)
    {
        const FMote& Mote = MoteSeeds[I];
        const float T = FMath::Max(0.f, Clock - Mote.Delay);
        const float U = FMath::Clamp(T / (Length - Mote.Delay), 0.f, 1.f);
        const float Radius = Mote.Radius * (bSummon ? FMath::Lerp(1.f, .35f, FMath::SmoothStep(0.f, Peak / Length, U)) : FMath::Lerp(.5f, 1.2f, U));
        const float Angle = Mote.Angle + Mote.Speed * T;
        const FVector At(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Mote.Rise * U);
        const float Size = T > 0 ? FMath::Lerp(1.6f, 2.8f, FMath::Frac(Mote.Angle * 3.f)) * (1.f - .5f * U) : 0.f;   // 4-7 cm points: readable at play distance
        Motes->UpdateInstanceTransform(I, FTransform(FRotator(0, FMath::RadiansToDegrees(Angle) * 2.f, 0), At, FVector(Size)), false, I == MoteSeeds.Num() - 1);
    }
    if (Clock >= Length) Destroy();
}
