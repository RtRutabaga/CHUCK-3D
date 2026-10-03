#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.generated.h"

class UCapsuleComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UStaticMesh;
class UStaticMeshComponent;

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
enum class EDockHuman : uint8 { Worker, Guard, MarketWoman, GuardWoman, SideGuard, Zombie, Count };

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
    /** A guard's spear (Tools/build_spear.py): upright beside the foot on Side (0 left, 1 right), that fist round its grip. */
    void GiveSpear(int32 Side = 1);
    bool HasSpear() const { return bSpear; }
    /** How far the spear fist is from the spear's grip (cm), for tests. */
    float GetSpearGripError() const;
    /** How far the spear leans from upright (deg), for tests. */
    float GetSpearLean() const;
    /** Mean bend of the four fingers of the left hand (deg), for tests: curled, not straight. */
    float GetFingerCurl() const;
    /**
     * The sewer zombie (user 2026-10-03: in the wide chamber, "can just barely
     * be killed by the player but is best to avoid"). A gaunt dead man in rags
     * on the same skeleton (an old man's stooped motion capture). It stands
     * swaying until the rat comes near (closer behind it than in front), then
     * shambles after him at less than his walking pace, never far from where it
     * stood and never over a gap in the floor. In reach it rears up, arms
     * lifting (the tell), then lunges down at him: a bite costs two sanity.
     * Nine scratches put it down; it crumples and leaves cigarettes.
     */
    static ADockNPC* SpawnZombie(UWorld* World, const FVector& Feet, float Yaw);
    bool IsHostile() const { return Kind == EDockHuman::Zombie; }
    bool IsDead() const { return ZState == EZombie::Dead; }
    int32 GetHitsTaken() const { return Hits; }
    int32 GetBitesLanded() const { return Bites; }
    int32 GetLunges() const { return Lunges; }
    const TCHAR* GetZombieStateName() const;
    /** Shamble speed (cm/s): the walk clip's own, at this body's scale. */
    float GetZombieWalkSpeed() const;
    int32 Cigarettes = 4;
    static constexpr int32 ZombieHealth = 9;
    static constexpr int32 ZombieBite = 2;          // sanity a bite costs
    static constexpr float ZombieNotice = 480.f;    // cm: it notices the rat this close, whichever way it faces
    static constexpr float ZombieSight = 750.f;     // and this far in front of it
    static constexpr float ZombieLeash = 1100.f;    // never further than this from where it stood
    static constexpr float ZombieStrike = 115.f;    // starts the lunge from here
    static constexpr float ZombieBiteRange = 95.f;
    static constexpr float ZombieWindup = .8f;
    static constexpr float ZombieLungeTime = .45f;
    static constexpr float ZombieRecover = 1.3f;
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
    UPROPERTY() TObjectPtr<UAnimSequence> Clips[7];
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
    UPROPERTY() UStaticMeshComponent* Spear = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> SpearMesh;
    bool bSpear = false;
    int32 SpearSide = 1;                          // 0 left hand, 1 right
    FVector SpearGrip = FVector::ZeroVector;      // component space: where his fist closes on the shaft
    void HoldSpear(TArray<FTransform>& Space) const;
    void PlaceSpear();
    void SampleClips(float Time, TArray<FQuat>& BoneDelta, FVector& HipsOffset) const;
    void UpdateTurn(float DeltaSeconds, float YawToChuck, bool bNear);
    /** Standing pose from the model's A-pose (component-space turn per posed
        bone): arms hanging, elbows soft, palms to the thighs, fingers curled. */
    TArray<FQuat> Rest;
    float EyeHeight = 167.f;                      // above the feet, from the head bone
    void SolveRest();
    void Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space, const TArray<FQuat>* BoneDelta = nullptr, const FVector& HipsOffset = FVector::ZeroVector) const;
    void UpdatePose(float DeltaSeconds);
    // The zombie.
    enum class EZombie : uint8 { Idle, Shamble, Windup, Lunge, Recover, Hurt, Dead };
    EZombie ZState = EZombie::Idle;
    float ZTime = 0, WalkTime = 0, WalkBlend = 0, Rear = 0, Reach = 0, Flinch = 0, FlinchTime = -1, DeathTime = -1;
    FVector ZHome = FVector::ZeroVector, LungeDir = FVector::ForwardVector;
    bool bBit = false, bDropped = false;
    int32 Hits = 0, Bites = 0, Lunges = 0;
    void SetZombie(EZombie State) { ZState = State; ZTime = 0; }
    void TickZombie(float DeltaSeconds);
    bool StepZombie(const FVector& Direction, float Distance);
    void TurnZombie(const FVector& Toward, float DeltaSeconds, float Rate);
};
