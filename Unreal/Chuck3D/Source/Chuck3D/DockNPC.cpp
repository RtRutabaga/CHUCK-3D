#include "DockNPC.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<ADockNPC>> NPCRegistry;
    constexpr float HalfHeight = 90.f;
    // The humans' shared skeleton: MPFB's game_engine rig (Unreal mannequin
    // names, three bones per finger). Each R bone directly follows its L bone.
    enum EBone { Pelvis, Spine1, Spine2, Chest, Neck, Head, ClavL, ClavR, UpperL, UpperR, LowerL, LowerR, HandL, HandR,
        ThumbL, ThumbR, MiddleL, MiddleR, BoneCount };
    const TCHAR* BoneNames[] = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r"),
        TEXT("thumb_01_l"), TEXT("thumb_01_r"), TEXT("middle_01_l"), TEXT("middle_01_r") };
    // Fingers: thumb, index, middle, ring, little; three joints each.
    const TCHAR* FingerNames[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
    // Degrees each joint bends toward the palm: a hand at rest (the little
    // finger curls most, the index least), and closed round a shaft.
    constexpr float Relaxed[5][3] = { {6.f, 10.f, 10.f}, {10.f, 16.f, 12.f}, {14.f, 20.f, 14.f}, {18.f, 24.f, 16.f}, {24.f, 28.f, 18.f} };
    constexpr float Gripped[5][3] = { {25.f, 35.f, 25.f}, {62.f, 85.f, 55.f}, {68.f, 85.f, 55.f}, {70.f, 85.f, 55.f}, {74.f, 85.f, 55.f} };
    constexpr float WristStraight = .65f;   // how much of the clips' unreliable wrist bend is taken out
    constexpr float ArmClear = 5.f;         // deg the clips' arms are eased out so the hands clear wider hips
    // The spear: its butt on the ground beside his right foot, a touch ahead,
    // leaning a little out; his fist closes on the leather wrap.
    constexpr float SpearAhead = 16.f, SpearOut = 30.f, SpearLeanDeg = 4.f;
    constexpr float SpearGripHeight = 108.f;   // cm up the shaft (the wrap is 100-124)
    constexpr float FistReach = 8.f;           // cm from the wrist to the middle of a closed fist
    constexpr float PalmDepth = 3.f;           // cm from the knuckle line to the middle of the fist, palm side
    const TCHAR* MeshPaths[] = { TEXT("/Game/Characters/Humans/DockWorker/SK_DockWorker.SK_DockWorker"),
        TEXT("/Game/Characters/Humans/Guard/SK_Guard.SK_Guard"), TEXT("/Game/Characters/Humans/MarketWoman/SK_MarketWoman.SK_MarketWoman") };
    EBone Of(EBone Left, int32 Side) { return static_cast<EBone>(Left + Side); }
    // Motion-capture clips: idles per kind of person, and gesturing while talking.
    enum EClip { ClipStandHip, ClipStandLook, ClipTalk, ClipReact, ClipCount };
    const TCHAR* ClipPaths[] = { TEXT("/Game/Characters/Humans/Anim/AS_Human_StandHip.AS_Human_StandHip"),
        TEXT("/Game/Characters/Humans/Anim/AS_Human_StandLook.AS_Human_StandLook"), TEXT("/Game/Characters/Humans/Anim/AS_Human_Talk.AS_Human_Talk"),
        TEXT("/Game/Characters/Humans/Anim/AS_Human_React.AS_Human_React") };
    // Looking down at the rat: hardly at all until it's right at his feet
    // (a grown man doesn't crane at a rat across the street).
    constexpr float LookDownFar = 6.f;      // deg, most he tips his head while it's further off
    constexpr float CloseFull = 45.f, CloseStart = 95.f;   // cm between them: full look down .. from here
    constexpr float ReactIn = .2f, ReactOut = .5f;
    constexpr float TurnRate = 70.f;      // deg/s when he turns his body to the rat
    // Component axes: X forward, Y right, Z up. + pitch tips a bone forward
    // (the head looks down); + yaw turns it to his right.
    FQuat Pitch(float Degrees) { return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Yaw(float Degrees) { return FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Roll(float Degrees) { return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Degrees)); }
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
    // Every human is referenced here, so all of them are cooked.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Worker(MeshPaths[0]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Guard(MeshPaths[1]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Woman(MeshPaths[2]);
    HumanMeshes[0] = Worker.Object; HumanMeshes[1] = Guard.Object; HumanMeshes[2] = Woman.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> StandHip(ClipPaths[ClipStandHip]), StandLook(ClipPaths[ClipStandLook]),
        Talk(ClipPaths[ClipTalk]), React(ClipPaths[ClipReact]);
    Clips[ClipStandHip] = StandHip.Object; Clips[ClipStandLook] = StandLook.Object; Clips[ClipTalk] = Talk.Object; Clips[ClipReact] = React.Object;
    Body->SetSkinnedAssetAndUpdate(Worker.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpearAsset(TEXT("/Game/Characters/Humans/Props/SM_Spear.SM_Spear"));
    SpearMesh = SpearAsset.Object;
    Spear = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spear"));
    Spear->SetupAttachment(Body);
    Spear->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spear->SetCanEverAffectNavigation(false);
    Spear->SetVisibility(false);
}

void ADockNPC::GiveSpear()
{
    bSpear = SpearMesh != nullptr;
    if (!bSpear) return;
    Spear->SetStaticMesh(SpearMesh);
    Spear->SetVisibility(true);
    Grip[1] = 1.f;   // his right hand closed round it
    PlaceSpear();
}

void ADockNPC::PlaceSpear()
{
    // His right is the opposite side to the left arm (ArmOut, from the rest solve).
    const float Right = -ArmOut;
    const FRotator Lean(0.f, 0.f, Right * SpearLeanDeg);   // the top leans out, away from him
    Spear->SetRelativeLocationAndRotation(FVector(SpearAhead, Right * SpearOut, 0.f), Lean);
    SpearGrip = FVector(SpearAhead, Right * SpearOut, 0.f) + Lean.RotateVector(FVector(0.f, 0.f, SpearGripHeight));
}

ADockNPC* ADockNPC::SpawnHuman(UWorld* World, EDockHuman Kind, const FVector& Feet, float Yaw)
{
    const FTransform At(FRotator(0, Yaw, 0), Feet + FVector(0, 0, HalfHeight));
    auto* NPC = World->SpawnActorDeferred<ADockNPC>(ADockNPC::StaticClass(), At);
    if (!NPC) return nullptr;
    NPC->Kind = Kind;
    NPC->FinishSpawning(At);
    return NPC;
}

ADockNPC* ADockNPC::SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Worker, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("DockWorkerArt"));   // the human-scale reference the smoke test looks for
    NPC->DisplayName = TEXT("Dock worker");
    return NPC;
}

void ADockNPC::SpawnTownsfolk(UWorld* World)
{
    // The guard before the closed city gate (the plaza's far wall), facing
    // back down the plaza: he keeps the rat from the way into the city.
    if (ADockNPC* Guard = SpawnHuman(World, EDockHuman::Guard, FVector(260, -4060, 0), 90.f))
    {
        Guard->Tags.Add(TEXT("DockGuard"));
        Guard->DisplayName = TEXT("Guard");
        Guard->Lines = { TEXT("Stick to the docks, rat.") };
        Guard->GiveSpear();
    }
    // The market woman at the end of the aisle between the red-canopied stalls.
    if (ADockNPC* Woman = SpawnHuman(World, EDockHuman::MarketWoman, FVector(148, -1240, 0), 180.f))
    {
        Woman->Tags.Add(TEXT("MarketWoman"));
        Woman->DisplayName = TEXT("Market woman");
        Woman->Lines = { TEXT("No handouts here. If you're hungry, you should check the sewer for scraps.") };
    }
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
    Phase = FMath::FRandRange(0.f, 30.f);
    NextGlance = FMath::FRandRange(1.f, 3.f);
    if (USkeletalMesh* Mesh = HumanMeshes[static_cast<int32>(Kind)]) Body->SetSkinnedAssetAndUpdate(Mesh);
    BoneIndex.Init(INDEX_NONE, BoneCount);
    Rest.Init(FQuat::Identity, BoneCount);
    if (const USkinnedAsset* Asset = Body->GetSkinnedAsset())
    {
        const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
        for (int32 I = 0; I < BoneCount; ++I) BoneIndex[I] = Ref.FindBoneIndex(BoneNames[I]);
        if (!BoneIndex.Contains(INDEX_NONE)) SolveRest();
        // Each finger joint's bending axis, from the model's pose (palms down,
        // fingers out): across the finger, bending it toward the palm.
        FingerBone.Init(INDEX_NONE, 30); FingerAxis.Init(FVector::ZeroVector, 30);
        TArray<FTransform> RefSpace;
        if (!BoneIndex.Contains(INDEX_NONE))
        {
            TArray<FQuat> None; None.Init(FQuat::Identity, BoneCount);
            Solve(None, RefSpace);
        }
        for (int32 Side = 0; Side < 2 && RefSpace.Num(); ++Side)
            for (int32 F = 0; F < 5; ++F)
                for (int32 J = 0; J < 3; ++J)
                {
                    const int32 I = (Side * 5 + F) * 3 + J;
                    FingerBone[I] = Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%02d_%s"), FingerNames[F], J + 1, Side == 0 ? TEXT("l") : TEXT("r")));
                }
        if (RefSpace.Num() && !FingerBone.Contains(INDEX_NONE))
        {
            const FVector Palm = -FVector::UpVector;
            for (int32 I = 0; I < 30; ++I)
            {
                const int32 B = FingerBone[I], J = I % 3;
                const FVector Along = J < 2 ? RefSpace[FingerBone[I + 1]].GetLocation() - RefSpace[B].GetLocation()
                                            : RefSpace[B].GetLocation() - RefSpace[FingerBone[I - 1]].GetLocation();
                const FVector Axis = FVector::CrossProduct(Along.GetSafeNormal(), Palm).GetSafeNormal();
                FingerAxis[I] = RefSpace[B].GetRotation().UnrotateVector(Axis);
            }
        }
        else FingerBone.Reset();
    }
    HomeYaw = static_cast<float>(GetActorRotation().Yaw);
    if (bSpear) PlaceSpear();   // given before play began: place it now the arms are known
    // The idle this person plays: the guard keeps looking about; the worker
    // and the market woman stand with weight on one leg, a hand to the hip
    // now and then (each from its own random point in the clip).
    IdleClip = Kind == EDockHuman::Guard ? ClipStandLook : ClipStandHip;
    bool bClips = !BoneIndex.Contains(INDEX_NONE);
    for (const auto& Clip : Clips) bClips &= Clip && Clip->GetSkeleton();
    if (bClips)
    {
        // The clips are changes of rotation from the skeleton's own rest (the
        // dock worker's); mapped by bone name onto this body.
        const FReferenceSkeleton& Src = Clips[0]->GetSkeleton()->GetReferenceSkeleton();
        const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
        SourceRest.SetNum(Src.GetNum());
        for (int32 B = 0; B < Src.GetNum(); ++B)
        {
            const int32 P = Src.GetParentIndex(B);
            SourceRest[B] = P >= 0 ? SourceRest[P] * Src.GetRefBonePose()[B].GetRotation() : Src.GetRefBonePose()[B].GetRotation();
        }
        SkelIndex.Init(INDEX_NONE, Ref.GetNum());
        for (int32 B = 0; B < Ref.GetNum(); ++B) SkelIndex[B] = Src.FindBoneIndex(Ref.GetBoneName(B));
        // Hips in component space (above them is the rig's root bone).
        const auto CompAt = [](const FReferenceSkeleton& Skel, int32 Bone)
        {
            FTransform T = FTransform::Identity;
            for (int32 B = Bone; B != INDEX_NONE; B = Skel.GetParentIndex(B)) T = T * Skel.GetRefBonePose()[B];
            return T.GetLocation();
        };
        SourceHips = CompAt(Src, Src.FindBoneIndex(BoneNames[Pelvis]));
        HipScale = static_cast<float>(CompAt(Ref, BoneIndex[Pelvis]).Z / FMath::Max(1., SourceHips.Z));
        if (SkelIndex.Contains(INDEX_NONE))
        {
            FString Missing;
            for (int32 B = 0; B < Ref.GetNum(); ++B) if (SkelIndex[B] == INDEX_NONE) Missing += Ref.GetBoneName(B).ToString() + TEXT(" ");
            UE_LOG(LogTemp, Warning, TEXT("CHUCK_NPC_BONES %s: no clip track for %s(mesh root %s, %d bones; skeleton root %s, %d bones)"), *GetName(), *Missing,
                *Ref.GetBoneName(0).ToString(), Ref.GetNum(), *Src.GetBoneName(0).ToString(), Src.GetNum());
            SkelIndex.Reset();
        }
    }
}

void ADockNPC::SampleClips(float Time, TArray<FQuat>& BoneDelta, FVector& HipsOffset) const
{
    // Each clip's local bone rotations at this time, blended (idle -> talk),
    // turned into each bone's change of rotation from the skeleton's rest,
    // then into the extra turn beyond its parent's (what Solve applies).
    const FReferenceSkeleton& Src = Clips[0]->GetSkeleton()->GetReferenceSkeleton();
    const int32 N = Src.GetNum();
    const auto Sample = [&](const UAnimSequence* Clip, TArray<FTransform>& Local)
    {
        Local.SetNum(N);
        const FAnimExtractContext Context(static_cast<double>(FMath::Fmod(Time, FMath::Max(Clip->GetPlayLength(), .1f))));
        for (int32 B = 0; B < N; ++B) Clip->GetBoneTransform(Local[B], FSkeletonPoseBoneIndex(B), Context, false);
    };
    TArray<FTransform> Local, Other;
    const auto Blend = [&](float W)
    {
        for (int32 B = 0; B < N; ++B)
        {
            Local[B].SetRotation(FQuat::Slerp(Local[B].GetRotation(), Other[B].GetRotation(), W));
            Local[B].SetLocation(FMath::Lerp(Local[B].GetLocation(), Other[B].GetLocation(), static_cast<double>(W)));
        }
    };
    Sample(Clips[IdleClip], Local);
    if (TalkBlend > 0.f) { Sample(Clips[ClipTalk], Other); Blend(TalkBlend); }
    if (ReactTime >= 0.f)
    {
        // The reaction plays once over whatever he was doing, eased in and out.
        const float Length = Clips[ClipReact]->GetPlayLength();
        const float W = FMath::Clamp(FMath::Min(ReactTime / ReactIn, (Length - ReactTime) / ReactOut), 0.f, 1.f);
        Other.SetNum(N);
        {
            const FAnimExtractContext Context(static_cast<double>(FMath::Min(ReactTime, Length)));
            for (int32 B = 0; B < N; ++B) Clips[ClipReact]->GetBoneTransform(Other[B], FSkeletonPoseBoneIndex(B), Context, false);
        }
        Blend(W);
    }
    TArray<FQuat> Comp; Comp.SetNum(N);
    TArray<FQuat> World; World.SetNum(N);
    for (int32 B = 0; B < N; ++B)
    {
        const int32 P = Src.GetParentIndex(B);
        Comp[B] = P >= 0 ? Comp[P] * Local[B].GetRotation() : Local[B].GetRotation();
        World[B] = Comp[B] * SourceRest[B].Inverse();
    }
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    BoneDelta.Init(FQuat::Identity, Ref.GetNum());
    for (int32 B = 0; B < Ref.GetNum(); ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        BoneDelta[B] = P >= 0 ? World[SkelIndex[B]] * World[SkelIndex[P]].Inverse() : World[SkelIndex[B]];
    }
    FTransform Hips = FTransform::Identity;   // the clip's hips in component space
    for (int32 B = SkelIndex[BoneIndex[Pelvis]]; B != INDEX_NONE; B = Src.GetParentIndex(B)) Hips = Hips * Local[B];
    HipsOffset = (Hips.GetLocation() - SourceHips) * HipScale;
}

void ADockNPC::PoseHands(TArray<FTransform>& Space, bool bStraightenWrists) const
{
    // Wrists: the clips' wrist data is poor (hands bent flat against the
    // legs), so most of it gives way to a straight wrist. Then every finger
    // joint is set relative to its parent: relaxed, or closed in a grip.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    if (bStraightenWrists && HandLocal.Num())
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const int32 Hand = BoneIndex[Of(HandL, Side)], Fore = BoneIndex[Of(LowerL, Side)];
            const FQuat Straight = (HandLocal[Side] * Space[Fore]).GetRotation();
            Space[Hand].SetRotation(FQuat::Slerp(Space[Hand].GetRotation(), Straight, WristStraight));
        }
    if (FingerBone.Num() != 30) return;
    for (int32 I = 0; I < 30; ++I)
    {
        const int32 Side = I / 15, F = (I / 3) % 5, J = I % 3, B = FingerBone[I];
        const float Degrees = FMath::Lerp(Relaxed[F][J], Gripped[F][J], Grip[Side]);
        FTransform Local = RefPose[B];
        Local.SetRotation(Local.GetRotation() * FQuat(FingerAxis[I], FMath::DegreesToRadians(Degrees)));
        Space[B] = Local * Space[Ref.GetParentIndex(B)];
    }
}

void ADockNPC::HoldSpear(TArray<FTransform>& Space) const
{
    // Two-bone IK for the right arm: the fist on the grip, the elbow back and
    // out, the knuckles across the shaft with the thumb up. Children follow
    // through their local transforms (the gripped fingers come along).
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Count = Space.Num();
    TArray<FTransform> Local; Local.SetNum(Count);
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        Local[B] = P >= 0 ? Space[B].GetRelativeTransform(Space[P]) : Space[B];
    }
    const auto Descends = [&](int32 B, int32 From) { for (; B != INDEX_NONE; B = Ref.GetParentIndex(B)) if (B == From) return true; return false; };
    const auto Rotate = [&](int32 Bone, const FQuat& Q)
    {
        Space[Bone].SetRotation(Q * Space[Bone].GetRotation());
        for (int32 B = Bone + 1; B < Count; ++B)
            if (Descends(B, Bone)) Space[B] = Local[B] * Space[Ref.GetParentIndex(B)];
    };
    const int32 Upper = BoneIndex[UpperR], Lower = BoneIndex[LowerR], Hand = BoneIndex[HandR];
    const int32 Thumb = BoneIndex[ThumbR], Middle = BoneIndex[MiddleR];
    const float Right = -ArmOut;
    const FVector S = Space[Upper].GetLocation();
    const float A = static_cast<float>(FVector::Dist(S, Space[Lower].GetLocation()));
    const float Bl = static_cast<float>(FVector::Dist(Space[Lower].GetLocation(), Space[Hand].GetLocation()));
    // The hand runs across the shaft, pointing from his shoulder toward it.
    FVector HandDir = SpearGrip - S; HandDir.Z = 0.f; HandDir = HandDir.GetSafeNormal();
    if (HandDir.IsNearlyZero()) HandDir = FVector::ForwardVector;
    // The shaft runs through the middle of the fist: along the hand from the
    // wrist, and a little toward the palm (thumb up, the right palm faces his left).
    const FVector Wrist = SpearGrip - HandDir * FistReach + FVector(0.f, Right * PalmDepth, 0.f);
    FVector ToWrist = Wrist - S;
    const float D = FMath::Clamp(static_cast<float>(ToWrist.Size()), FMath::Abs(A - Bl) + 1.f, A + Bl - .5f);
    const FVector Along = ToWrist.GetSafeNormal();
    const float CosA = FMath::Clamp((A * A + D * D - Bl * Bl) / (2.f * A * D), -1.f, 1.f);
    const FVector Pole = FVector::VectorPlaneProject(FVector(-1.f, Right * .7f, -.2f), Along).GetSafeNormal();
    const FVector Elbow = S + Along * (A * CosA) + Pole * (A * FMath::Sqrt(1.f - CosA * CosA));
    Rotate(Upper, FQuat::FindBetweenNormals((Space[Lower].GetLocation() - S).GetSafeNormal(), (Elbow - S).GetSafeNormal()));
    Rotate(Lower, FQuat::FindBetweenNormals((Space[Hand].GetLocation() - Space[Lower].GetLocation()).GetSafeNormal(), (S + Along * D - Space[Lower].GetLocation()).GetSafeNormal()));
    Rotate(Hand, FQuat::FindBetweenNormals((Space[Middle].GetLocation() - Space[Hand].GetLocation()).GetSafeNormal(), HandDir));
    const FVector ThumbSide = FVector::VectorPlaneProject(Space[Thumb].GetLocation() - Space[Hand].GetLocation(), HandDir).GetSafeNormal();
    const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, HandDir).GetSafeNormal();
    Rotate(Hand, FQuat(HandDir, FMath::Atan2(static_cast<float>(FVector::DotProduct(HandDir, FVector::CrossProduct(ThumbSide, Up))),
        static_cast<float>(FVector::DotProduct(ThumbSide, Up)))));
}

float ADockNPC::GetSpearGripError() const
{
    if (!bSpear) return 1e3f;
    const FVector Wrist = Body->GetBoneLocation(BoneNames[HandR]), Knuckle = Body->GetBoneLocation(BoneNames[MiddleR]);
    const FVector Fist = Wrist + (Knuckle - Wrist).GetSafeNormal() * FistReach
        - Body->GetComponentTransform().TransformVectorNoScale(FVector(0.f, -ArmOut * PalmDepth, 0.f));
    return static_cast<float>(FVector::Dist(Fist, Body->GetComponentTransform().TransformPosition(SpearGrip)));
}

float ADockNPC::GetSpearLean() const
{
    return bSpear ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Spear->GetUpVector(), FVector::UpVector)), -1.f, 1.f))) : 90.f;
}

float ADockNPC::GetFingerCurl() const
{
    // The angle between each left finger's first and last joint directions.
    if (FingerBone.Num() != 30) return 0.f;
    float Sum = 0.f;
    for (int32 F = 1; F < 5; ++F)
    {
        const FName A = Body->GetBoneName(FingerBone[F * 3]), B = Body->GetBoneName(FingerBone[F * 3 + 1]), C = Body->GetBoneName(FingerBone[F * 3 + 2]);
        const FVector D1 = (Body->GetBoneLocation(B) - Body->GetBoneLocation(A)).GetSafeNormal();
        const FVector D2 = (Body->GetBoneLocation(C) - Body->GetBoneLocation(B)).GetSafeNormal();
        Sum += FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(D1, D2)), -1.f, 1.f)));
    }
    return Sum / 4.f;
}

void ADockNPC::TakeScratch(const FVector& From)
{
    ++Scratches;
    if (ReactTime >= 0.f && ReactTime < .6f) return;   // already starting back
    ReactTime = 0.f;
    TurnHold = 2.f; bTurning = true;                    // and he turns to see what did it
}

float ADockNPC::GetBodyTurn() const
{
    return FMath::FindDeltaAngleDegrees(HomeYaw, static_cast<float>(GetActorRotation().Yaw));
}

void ADockNPC::UpdateTurn(float DeltaSeconds, float YawToChuck, bool bNear)
{
    // His head turns first; if the rat stays well off to the side (or he's
    // being talked to) he turns his body to face it, and back to his post
    // once it's gone.
    // Once he starts turning he carries on until he faces it.
    float Target = HomeYaw;
    const float Current = static_cast<float>(GetActorRotation().Yaw);
    if (bNear && (bTalking || FMath::Abs(YawToChuck) > 55.f)) TurnHold += DeltaSeconds; else if (!bTurning) TurnHold = 0.f;
    if (bNear && (bTalking || TurnHold > 1.2f)) bTurning = true;
    if (!bNear || FMath::Abs(YawToChuck) < 8.f) bTurning = false;
    if (bNear && (bTalking || bTurning)) Target = Current + YawToChuck;
    else if (bNear) Target = Current;                       // keep facing where he turned to while the rat's near
    const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, Target), -TurnRate * DeltaSeconds, TurnRate * DeltaSeconds);
    if (FMath::Abs(Step) > KINDA_SMALL_NUMBER) SetActorRotation(FRotator(0.f, Current + Step, 0.f));
}

void ADockNPC::Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space, const TArray<FQuat>* BoneDelta, const FVector& HipsOffset) const
{
    // Component space, parent first; each posed bone is turned by its delta
    // about its own joint, and its children follow.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    const int32 Count = Ref.GetNum();
    Space.SetNum(Count);
    TArray<int32> Which; Which.Init(INDEX_NONE, Count);
    for (int32 I = 0; I < BoneCount; ++I) Which[BoneIndex[I]] = I;
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 Parent = Ref.GetParentIndex(B);
        Space[B] = Parent >= 0 ? RefPose[B] * Space[Parent] : RefPose[B];
        if (B == BoneIndex[Pelvis]) Space[B].AddToTranslation(HipsOffset);
        if (BoneDelta) Space[B].SetRotation((*BoneDelta)[B] * Space[B].GetRotation());
        if (Which[B] != INDEX_NONE) Space[B].SetRotation(Delta[Which[B]] * Space[B].GetRotation());
    }
}

void ADockNPC::SolveRest()
{
    // Aim each part of the arm in turn from the model's A-pose, measuring the
    // posed skeleton after every step, so the pose holds for any body: upper
    // arm hanging just clear of the hip and a touch back, elbow softly bent,
    // palm turned to the thigh (thumb forward), wrist straight, fingers curled.
    TArray<FTransform> Space;
    const auto At = [&](EBone B) { return Space[BoneIndex[B]].GetLocation(); };
    const auto Dir = [&](EBone From, EBone To) { return (At(To) - At(From)).GetSafeNormal(); };
    const auto Turn = [&](EBone B, const FQuat& Q) { Rest[B] = Q * Rest[B]; Solve(Rest, Space); };
    Solve(Rest, Space);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const EBone Upper = Of(UpperL, Side), Lower = Of(LowerL, Side), Hand = Of(HandL, Side);
        const EBone Thumb = Of(ThumbL, Side), Middle = Of(MiddleL, Side);
        const float S = FMath::Sign(static_cast<float>(At(Upper).Y));   // which side of him this arm is on
        if (Side == 0) ArmOut = S;
        Turn(Upper, FQuat::FindBetweenNormals(Dir(Upper, Lower), FVector(-.03f, S * .13f, -1.f).GetSafeNormal()));
        Turn(Lower, FQuat::FindBetweenNormals(Dir(Lower, Hand), FVector(.11f, S * .03f, -1.f).GetSafeNormal()));
        // Twist the forearm about itself until the thumb points forward.
        const FVector Axis = Dir(Lower, Hand);
        const FVector ThumbSide = FVector::VectorPlaneProject(At(Thumb) - At(Hand), Axis).GetSafeNormal();
        const FVector Want = FVector::VectorPlaneProject(FVector(1.f, -S * .25f, 0.f), Axis).GetSafeNormal();
        Turn(Lower, FQuat(Axis, FMath::Atan2(static_cast<float>(FVector::DotProduct(Axis, FVector::CrossProduct(ThumbSide, Want))),
            static_cast<float>(FVector::DotProduct(ThumbSide, Want)))));
        Turn(Hand, FQuat::FindBetweenNormals(Dir(Hand, Middle), Dir(Lower, Hand)));   // a straight wrist
    }
    EyeHeight = static_cast<float>(At(Head).Z) + 9.f;   // the eyes, a hand above the skull's pivot
    // Keep the straight wrists to lay over the motion capture.
    HandLocal.SetNum(2);
    for (int32 Side = 0; Side < 2; ++Side)
        HandLocal[Side] = Space[BoneIndex[Of(HandL, Side)]].GetRelativeTransform(Space[BoneIndex[Of(LowerL, Side)]]);
}

float ADockNPC::GetWiderHandReach() const
{
    const FTransform& Actor = GetActorTransform();
    const auto Side = [&](EBone Hand) { return FMath::Abs(static_cast<float>(Actor.InverseTransformPosition(Body->GetBoneLocation(BoneNames[Hand])).Y)); };
    return FMath::Max(Side(HandL), Side(HandR));
}

float ADockNPC::GetStraightArmOut() const
{
    float Widest = 0.f;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector Shoulder = Body->GetBoneLocation(BoneNames[Of(UpperL, Side)]), Elbow = Body->GetBoneLocation(BoneNames[Of(LowerL, Side)]);
        const FVector Wrist = Body->GetBoneLocation(BoneNames[Of(HandL, Side)]);
        const float Bend = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct((Elbow - Shoulder).GetSafeNormal(), (Wrist - Elbow).GetSafeNormal())), -1.f, 1.f)));
        const float Out = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct((Elbow - Shoulder).GetSafeNormal(), -FVector::UpVector)), -1.f, 1.f)));
        if (Bend < 30.f) Widest = FMath::Max(Widest, Out);
    }
    return Widest;
}

float ADockNPC::GetHandsForward() const
{
    const FTransform& Actor = GetActorTransform();
    const auto Ahead = [&](EBone Hand) { return static_cast<float>(Actor.InverseTransformPosition(Body->GetBoneLocation(BoneNames[Hand])).X); };
    return FMath::Max(Ahead(HandL), Ahead(HandR));
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
                const float Close = FMath::Clamp((CloseStart - Across) / (CloseStart - CloseFull), 0.f, 1.f);
                Target = FVector2D(FMath::Clamp(YawTo, -70.f, 70.f), FMath::Clamp(PitchTo, -20.f, FMath::Lerp(LookDownFar, 55.f, Close)));
            }
            if (HasMocap()) UpdateTurn(DeltaSeconds, YawTo, true);
        }
        else if (HasMocap()) UpdateTurn(DeltaSeconds, 0.f, false);
        bTalking = Chuck->GetTalkingTo() == this;
    }
    TalkBlend = FMath::FInterpConstantTo(TalkBlend, bTalking ? 1.f : 0.f, DeltaSeconds, 2.5f);
    if (ReactTime >= 0.f && HasMocap() && (ReactTime += DeltaSeconds) > Clips[ClipReact]->GetPlayLength()) ReactTime = -1.f;
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
    if (HasMocap())
    {
        // Motion capture drives the body, neck and head carriage included (the
        // actors' necks balance their chests); where he looks is turned on top,
        // and the hands keep their curl.
        TArray<FQuat> BoneDelta; FVector HipsOffset;
        SampleClips(T, BoneDelta, HipsOffset);
        // The clip actors were slighter at the hip: ease each arm out a little
        // so the hands rest beside the thighs, not in them.
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float S = Side == 0 ? ArmOut : -ArmOut;
            BoneDelta[BoneIndex[Of(UpperL, Side)]] = Roll(S * ArmClear) * BoneDelta[BoneIndex[Of(UpperL, Side)]];
        }
        TArray<FQuat> Delta; Delta.Init(FQuat::Identity, BoneCount);
        Delta[Neck] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
        Delta[Head] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
        TArray<FTransform> Space;
        Solve(Delta, Space, &BoneDelta, HipsOffset);
        PoseHands(Space, true);
        if (bSpear) HoldSpear(Space);
        const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
        for (int32 B = 0; B < Space.Num(); ++B) Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
        return;
    }
    const float Breath = FMath::Sin(T * UE_TWO_PI / 4.2f);          // one slow breath every 4.2 s
    const float Shift = FMath::Sin(T * UE_TWO_PI / 11.f);           // weight moving between the feet
    TArray<FQuat> Delta = Rest;                                     // the standing pose, then the life on top
    Delta[Pelvis] = Roll(1.4f * Shift) * Yaw(1.5f * Shift);
    Delta[Spine1] = Roll(-.8f * Shift);
    Delta[Spine2] = Pitch(-.6f * Breath);
    Delta[Chest] = Pitch(-1.1f * Breath) * Roll(-.6f * Shift);
    // The look is shared between the neck and the head.
    Delta[Neck] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
    Delta[Head] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y - .5f * Breath);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float S = Side == 0 ? -1.f : 1.f;   // L is the left (-Y in Unreal)
        const float Sway = FMath::Sin(T * UE_TWO_PI / 5.3f + Side * 1.7f);
        Delta[Of(ClavL, Side)] = Roll(S * .8f * Breath);
        Delta[Of(UpperL, Side)] = Pitch(-1.5f * Sway) * Rest[Of(UpperL, Side)];   // a small swing from the shoulder
    }
    TArray<FTransform> Space;
    Solve(Delta, Space);
    PoseHands(Space, false);
    if (bSpear) HoldSpear(Space);
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    for (int32 B = 0; B < Space.Num(); ++B) Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
}
