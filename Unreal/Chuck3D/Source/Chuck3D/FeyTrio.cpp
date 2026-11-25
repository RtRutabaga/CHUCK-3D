// The Crossroads trio (Claude, 2026-11-23). See FeyTrio.h.
#include "FeyTrio.h"
#include "NPCGroom.h"
#include "FeyLife.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GroomComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

float AFeyTrio::HoldAt = -1.f;

namespace
{
    const TCHAR* Names[] = {TEXT("FeyCentaur"), TEXT("FeySatyr"), TEXT("FeyGnome")};
}

AFeyTrio::AFeyTrio()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AFeyTrio* AFeyTrio::Spawn(UWorld* World, const FVector& Ground, float Yaw)
{
    if (!World) return nullptr;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<AFeyTrio>(Ground, FRotator(0, Yaw, 0), P);
}

int32 AFeyTrio::CountMissing()
{
    int32 N = 0;
    for (const TCHAR* Name : Names)
    {
        N += !LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Characters/FeyTrio/%s/SK_%s.SK_%s"), Name, Name, Name));
        N += !LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/FeyTrio/%s/Anim/AS_%s_Mill.AS_%s_Mill"), Name, Name, Name));
    }
    if (N) UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_TRIO_MISSING assets=%d (not cooked?)"), N);
    return N;
}

void AFeyTrio::BeginPlay()
{
    Super::BeginPlay();
    { float H; if (FParse::Value(FCommandLine::Get(), TEXT("-ChuckFeyTrioHold="), H)) HoldAt = H; }   // (stills: the loop held at that time)
    for (const TCHAR* Name : Names)
    {
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Characters/FeyTrio/%s/SK_%s.SK_%s"), Name, Name, Name));
        UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/FeyTrio/%s/Anim/AS_%s_Mill.AS_%s_Mill"), Name, Name, Name));
        if (!Mesh || !Clip) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_TRIO_MISSING %s mesh=%d clip=%d"), Name, Mesh != nullptr, Clip != nullptr); continue; }
        auto* Part = NewObject<USkeletalMeshComponent>(this, FName(Name));
        Part->SetupAttachment(RootComponent);
        Part->SetRelativeLocation(FVector(0, 0, Lift));
        Part->SetSkeletalMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(true);
        Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;   // (the box reads the centaur's bones)
        Part->RegisterComponent();
        Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Part->SetAnimation(Clip);
        Part->Play(false);   // (positioned from one clock below, all three together)
        Parts.Add(Part);
        ChuckGroom::Attach(this, Part, Grooms);
        ++Loaded;
    }
    if (AFeyLife::bCalm) { UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_TRIO_SPAWNED at=%s parts=%d grooms=%d calm=1"), *GetActorLocation().ToCompactString(), Loaded, Grooms.Num()); return; }   // (the route walker passes through them on the ring road)
    Body = NewObject<UBoxComponent>(this, TEXT("CentaurBody"));
    Body->SetupAttachment(RootComponent);
    Body->SetBoxExtent(FVector(95.f, 34.f, 60.f));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->RegisterComponent();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_TRIO_SPAWNED at=%s parts=%d grooms=%d"), *GetActorLocation().ToCompactString(), Loaded, Grooms.Num());
}

void AFeyTrio::EndPlay(const EEndPlayReason::Type Reason)
{
    ChuckGroom::Release();
    Super::EndPlay(Reason);
}

FVector AFeyTrio::GetBodyAt() const
{
    return Parts.Num() ? Parts[0]->GetBoneLocation(TEXT("h_front")) : GetActorLocation();
}

void AFeyTrio::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock = HoldAt >= 0.f ? HoldAt : Clock + DeltaSeconds;
    const float T = FMath::Fmod(Clock, Loop);
    for (USkeletalMeshComponent* Part : Parts)
        if (UAnimSingleNodeInstance* Single = Part->GetSingleNodeInstance()) Single->SetPosition(T, false);
    if (Parts.Num() && Body)
    {
        // The box on the horse's body: between the man's hips (its front) and the tail's root (its croup), level, turned his way.
        const FVector Front = Parts[0]->GetBoneLocation(TEXT("pelvis")), Back = Parts[0]->GetBoneLocation(TEXT("h_tail1"));
        const FVector Mid = (Front + Back) * .5f, Along = (Front - Back).GetSafeNormal2D();
        Body->SetWorldLocationAndRotation(Mid - FVector(0, 0, 10.f), Along.Rotation());
        ChuckGroom::Want(this, Parts[0]->GetBoneLocation(TEXT("head")));
    }
}
