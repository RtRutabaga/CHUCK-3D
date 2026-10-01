#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.generated.h"

class UCapsuleComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class UAnimSequence;

/**
 * A human NPC on the docks (user 2026-09-30: start with the dock worker by the
 * spawn; more NPCs will follow - the 2D game's guard and market woman).
 * A MakeHuman body dressed by Tools/build_npc_humans.py on the humans' shared
 * 53-bone skeleton (MPFB game_engine, with fingers), posed from its A-pose (a
 * relaxed standing pose solved per body: no arms held out):
 * breathing, a slow weight shift, idle glances, and his head turning to watch
 * Chuck when the rat comes near. Solid to Chuck but not climbable (a
 * pawn-only blocker: wall-run, ledge and camera traces ignore him).
 *
 * Talk: an NPC with Lines shows a prompt in range and Chuck talks to him
 * with F / Y (AChuckCharacter::Interact); lines advance on each press. One
 * with no lines is ambient (the worker, for now: no dialogue yet, user's call).
 * NPC speech only - Chuck never speaks (AGENTS.md).
 */
/** The human NPCs built by Tools/build_npc_humans.py (SourceAssets/NPCs/humans.json). */
enum class EDockHuman : uint8 { Worker, Guard, MarketWoman, Count };

UCLASS()
class CHUCK3D_API ADockNPC : public AActor
{
    GENERATED_BODY()
public:
    ADockNPC();
    virtual void Tick(float DeltaSeconds) override;
    /** The dock worker: the human-scale reference by the spawn (180 cm). */
    static ADockNPC* SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw);
    /** Any of the humans, standing at Feet facing Yaw. */
    static ADockNPC* SpawnHuman(UWorld* World, EDockHuman Kind, const FVector& Feet, float Yaw);
    /** The 2D game's guard at the closed city gate and the market woman by the red awning (References/Original/PHASE-2.md). */
    static void SpawnTownsfolk(UWorld* World);
    /** Every NPC in play (Chuck looks here for someone to talk to). */
    static const TArray<TWeakObjectPtr<ADockNPC>>& All();
    FString DisplayName;
    TArray<FString> Lines;
    bool CanTalk() const { return Lines.Num() > 0; }
    static constexpr float TalkRadius = 120.f;     // cm from Chuck's centre to the NPC's
    static constexpr float NoticeRange = 450.f;    // he watches Chuck within this
    /** Head turn now (deg; + looks right / + looks down), for tests. */
    FVector2D GetLookAngles() const { return Look; }
    bool IsWatchingChuck() const { return bWatching; }
    /** True once its motion-capture idle is playing (tests). */
    bool HasMocap() const { return SkelIndex.Num() > 0; }
    /** Chuck's slash caught him: a start, a lean back, a little shift of the feet, then he turns to the rat. */
    void TakeScratch(const FVector& From);
    int32 GetScratches() const { return Scratches; }
    bool IsReacting() const { return ReactTime >= 0.f; }
    /** Body yaw away from where it was placed (deg), for tests: turned toward Chuck. */
    float GetBodyTurn() const;
    /** How far out to the side his wider hand is (cm from his centre line), for
        tests: about 45 in the model's A-pose, about 25 with arms by his sides. */
    float GetWiderHandReach() const;
    /** The widest a nearly straight arm (elbow within 30 degrees) is held out from hanging (deg from straight down):
        about 45 in the model's A-pose, under 20 hanging by his side; a hand on the hip bends the elbow and doesn't count. */
    float GetStraightArmOut() const;
    /** How far in front of his body line his more forward hand is (cm), for tests: hands at his sides, not held out. */
    float GetHandsForward() const;
    /** Close a hand (0 relaxed .. 1 a fist round a shaft): the guard's spear hand, later. Side 0 = left. */
    void SetGrip(int32 Side, float Amount) { Grip[FMath::Clamp(Side, 0, 1)] = FMath::Clamp(Amount, 0.f, 1.f); }
    /** Mean bend of the four fingers of the left hand (deg), for tests: curled, not straight. */
    float GetFingerCurl() const;
    /** Eyes above the feet (cm), from this body's head bone. */
    float GetEyeHeight() const { return EyeHeight; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UCapsuleComponent* Blocker;
    UPROPERTY() UPoseableMeshComponent* Body;
    TArray<int32> BoneIndex;
    FVector2D Look = FVector2D::ZeroVector;       // yaw, pitch (deg) of the head
    FVector2D Glance = FVector2D::ZeroVector;     // idle target
    float NextGlance = 0;
    float Clock = 0;
    float Phase = 0;                              // per-NPC offset so a crowd doesn't breathe in step
    bool bWatching = false;
    EDockHuman Kind = EDockHuman::Worker;
    UPROPERTY() TObjectPtr<USkeletalMesh> HumanMeshes[static_cast<int32>(EDockHuman::Count)];
    /** Motion-capture clips (CMU, Tools/build_npc_mocap.py) on the shared skeleton: idles and talk. */
    UPROPERTY() TObjectPtr<UAnimSequence> Clips[4];
    float ReactTime = -1.f;                       // since the rat scratched him (-1: not reacting)
    int32 Scratches = 0;
    int32 IdleClip = 0;
    float TalkBlend = 0;                          // 0 idle .. 1 the talk clip
    bool bTalking = false;
    float HomeYaw = 0, TurnHold = 0;              // body turn toward Chuck
    bool bTurning = false;
    TArray<int32> SkelIndex;                      // mesh bone -> skeleton bone
    TArray<FQuat> SourceRest;                     // skeleton rest, component space (the clips' rest)
    FVector SourceHips = FVector::ZeroVector;
    float HipScale = 1.f;
    TArray<FTransform> HandLocal;                 // each hand relative to its forearm in the rest solve (a straight wrist)
    TArray<int32> FingerBone;                     // [side][finger][joint] flattened, mesh bone indices
    TArray<FVector> FingerAxis;                   // each finger joint's bending axis, in its own frame
    float Grip[2] = { 0.f, 0.f };
    float ArmOut = 1.f;                           // which way is out for the left arm (+/-Y)
    void PoseHands(TArray<FTransform>& Space, bool bStraightenWrists) const;
    void SampleClips(float Time, TArray<FQuat>& BoneDelta, FVector& HipsOffset) const;
    void UpdateTurn(float DeltaSeconds, float YawToChuck, bool bNear);
    /** Standing pose from the model's A-pose (component-space turn per posed
        bone): arms hanging, elbows soft, palms to the thighs, fingers curled. */
    TArray<FQuat> Rest;
    float EyeHeight = 167.f;                      // above the feet, from the head bone
    void SolveRest();
    void Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space, const TArray<FQuat>* BoneDelta = nullptr, const FVector& HipsOffset = FVector::ZeroVector) const;
    void UpdatePose(float DeltaSeconds);
};
