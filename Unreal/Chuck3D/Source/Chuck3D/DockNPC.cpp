#include "DockNPC.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<ADockNPC>> NPCRegistry;
    constexpr float HalfHeight = 90.f;
    // The humans' shared skeleton: MPFB's cmu_mb rig (CMU BVH bone names).
    enum EBone { Pelvis, Spine1, Spine2, Chest, Neck, Neck1, Head, ClavL, ClavR, UpperL, UpperR, LowerL, LowerR, HandL, HandR, BoneCount };
    const TCHAR* BoneNames[] = { TEXT("Hips"), TEXT("LowerBack"), TEXT("Spine"), TEXT("Spine1"), TEXT("Neck"), TEXT("Neck1"), TEXT("Head"),
        TEXT("LeftShoulder"), TEXT("RightShoulder"), TEXT("LeftArm"), TEXT("RightArm"), TEXT("LeftForeArm"), TEXT("RightForeArm"), TEXT("LeftHand"), TEXT("RightHand") };
    // Component axes: X forward, Y right, Z up. + pitch tips a bone forward
    // (the head looks down); + yaw turns it to his right.
    FQuat Pitch(float Degrees) { return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Yaw(float Degrees) { return FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Roll(float Degrees) { return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Degrees)); }
    constexpr float EyeHeight = 167.f;   // above his feet
}

ADockNPC::ADockNPC()
{
    PrimaryActorTick.bCanEverTick = true;
    Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
    SetRootComponent(Blocker);
    Blocker->InitCapsuleSize(24.f, HalfHeight);
    // Solid to Chuck, invisible to his traces: no wall run up a man, no ledge on his shoulders.
    Blocker->SetCollisionProfileName(TEXT("Custom"));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Blocker->SetCollisionObjectType(ECC_WorldDynamic);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Blocker->SetCanEverAffectNavigation(false);
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(Blocker);
    Body->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Worker(TEXT("/Game/Characters/Humans/DockWorker/SK_DockWorker.SK_DockWorker"));
    Body->SetSkinnedAssetAndUpdate(Worker.Object);
}

ADockNPC* ADockNPC::SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = World->SpawnActor<ADockNPC>(Feet + FVector(0, 0, HalfHeight), FRotator(0, Yaw, 0));
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("DockWorkerArt"));   // the human-scale reference the smoke test looks for
    NPC->DisplayName = TEXT("Dock worker");
    return NPC;
}

const TArray<TWeakObjectPtr<ADockNPC>>& ADockNPC::All()
{
    NPCRegistry.RemoveAll([](const TWeakObjectPtr<ADockNPC>& Entry) { return !Entry.IsValid(); });
    return NPCRegistry;
}

void ADockNPC::BeginPlay()
{
    Super::BeginPlay();
    NPCRegistry.Add(this);
    Phase = FMath::FRandRange(0.f, 10.f);
    NextGlance = FMath::FRandRange(1.f, 3.f);
    BoneIndex.Init(INDEX_NONE, BoneCount);
    if (const USkinnedAsset* Asset = Body->GetSkinnedAsset())
    {
        const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
        for (int32 I = 0; I < BoneCount; ++I) BoneIndex[I] = Ref.FindBoneIndex(BoneNames[I]);
        if (!BoneIndex.Contains(INDEX_NONE))
        {
            // The humans are modelled in an A-pose: find the turn that brings
            // each upper arm from there to hanging by his side (a little out,
            // a little forward), whatever this body's proportions.
            const FTransform Shoulder[2] = { ComponentSpaceRef(BoneIndex[UpperL]), ComponentSpaceRef(BoneIndex[UpperR]) };
            const FTransform Elbow[2] = { ComponentSpaceRef(BoneIndex[LowerL]), ComponentSpaceRef(BoneIndex[LowerR]) };
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const FVector Now = (Elbow[Side].GetLocation() - Shoulder[Side].GetLocation()).GetSafeNormal();
                const FVector Hang = FVector(.06f, FMath::Sign(Now.Y) * .1f, -1.f).GetSafeNormal();
                ArmDown[Side] = FQuat::FindBetweenNormals(Now, Hang);
                // Its forearm's own axis once lowered: the palm turns about that.
                const FTransform Wrist = ComponentSpaceRef(BoneIndex[Side == 0 ? HandL : HandR]);
                ForearmAxis[Side] = ArmDown[Side].RotateVector((Wrist.GetLocation() - Elbow[Side].GetLocation()).GetSafeNormal());
            }
        }
    }
}

FTransform ADockNPC::ComponentSpaceRef(int32 Bone) const
{
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    FTransform Out = FTransform::Identity;
    for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B)) Out = Out * Ref.GetRefBonePose()[B];
    return Out;
}

float ADockNPC::GetHigherHandHeight() const
{
    const float Feet = static_cast<float>(GetActorLocation().Z) - HalfHeight;
    return FMath::Max(static_cast<float>(Body->GetBoneLocation(BoneNames[HandL]).Z), static_cast<float>(Body->GetBoneLocation(BoneNames[HandR]).Z)) - Feet;
}

void ADockNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    NPCRegistry.Remove(this);
    Super::EndPlay(Reason);
}

void ADockNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    // Where he's looking: at Chuck when the rat's near, otherwise idle glances
    // (out over the harbour, down the quay) every few seconds.
    FVector2D Target = Glance;
    bWatching = false;
    if (const auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FVector Eye = GetActorLocation() + FVector(0, 0, EyeHeight - HalfHeight);
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Chuck->GetActorLocation() + FVector(0, 0, 10.f) - Eye);
        const float Across = static_cast<float>(Local.Size2D());
        if (Across < NoticeRange && !Chuck->IsAstral())   // not while he's away in the astral light
        {
            const float YawTo = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X)));
            const float PitchTo = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), Across));
            // Only if the rat's somewhere he can see without turning round.
            if (FMath::Abs(YawTo) < 110.f)
            {
                bWatching = true;
                Target = FVector2D(FMath::Clamp(YawTo, -70.f, 70.f), FMath::Clamp(PitchTo, -20.f, 55.f));
            }
        }
    }
    if (!bWatching && (NextGlance -= DeltaSeconds) <= 0)
    {
        NextGlance = FMath::FRandRange(2.5f, 6.f);
        Glance = FVector2D(FMath::FRandRange(-35.f, 35.f), FMath::FRandRange(-6.f, 10.f));
    }
    const float Rate = bWatching ? 4.f : 2.f;
    Look.X = FMath::FInterpTo(Look.X, Target.X, DeltaSeconds, Rate);
    Look.Y = FMath::FInterpTo(Look.Y, Target.Y, DeltaSeconds, Rate);
    UpdatePose(DeltaSeconds);
}

void ADockNPC::UpdatePose(float DeltaSeconds)
{
    if (!Body->GetSkinnedAsset() || BoneIndex.Contains(INDEX_NONE)) return;
    const float T = Clock + Phase;
    const float Breath = FMath::Sin(T * UE_TWO_PI / 4.2f);          // one slow breath every 4.2 s
    const float Shift = FMath::Sin(T * UE_TWO_PI / 11.f);           // weight moving between his feet
    TArray<FQuat> Delta; Delta.Init(FQuat::Identity, BoneCount);
    Delta[Pelvis] = Roll(1.4f * Shift) * Yaw(1.5f * Shift);
    Delta[Spine1] = Roll(-.8f * Shift);
    Delta[Spine2] = Pitch(-.6f * Breath);
    Delta[Chest] = Pitch(-1.1f * Breath) * Roll(-.6f * Shift);
    // The look is shared between the two neck bones and the head.
    Delta[Neck] = Yaw(.25f * Look.X) * Pitch(.25f * Look.Y);
    Delta[Neck1] = Yaw(.25f * Look.X) * Pitch(.25f * Look.Y);
    Delta[Head] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y - .5f * Breath);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float S = Side == 0 ? -1.f : 1.f;   // L is his left (-Y in Unreal)
        const float Sway = FMath::Sin(T * UE_TWO_PI / 5.3f + Side * 1.7f);
        Delta[Side == 0 ? ClavL : ClavR] = Roll(S * .8f * Breath);
        Delta[Side == 0 ? UpperL : UpperR] = Pitch(-2.f * Sway) * ArmDown[Side];
        // A slight bend at the elbow, the forearm turned so the palm faces his thigh.
        Delta[Side == 0 ? LowerL : LowerR] = Pitch(-6.f - 2.f * Sway) * FQuat(ForearmAxis[Side], FMath::DegreesToRadians(S * 40.f));
    }
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    const int32 Count = Ref.GetNum();
    TArray<FTransform> Space; Space.SetNum(Count);
    TArray<int32> Which; Which.Init(INDEX_NONE, Count);
    for (int32 I = 0; I < BoneCount; ++I) Which[BoneIndex[I]] = I;
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 Parent = Ref.GetParentIndex(B);
        Space[B] = Parent >= 0 ? RefPose[B] * Space[Parent] : RefPose[B];
        if (Which[B] != INDEX_NONE) Space[B].SetRotation(Delta[Which[B]] * Space[B].GetRotation());
        Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
    }
}
