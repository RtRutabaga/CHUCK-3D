#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.generated.h"

class UCapsuleComponent;
class UPoseableMeshComponent;
class USkeletalMesh;

/**
 * A human NPC on the docks (user 2026-09-30: start with the dock worker by the
 * spawn; more NPCs will follow - the 2D game's guard and market woman).
 * A MakeHuman body dressed by Tools/build_npc_humans.py on the humans' shared
 * 31-bone skeleton (MPFB cmu_mb), procedurally posed from its A-pose (a
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
    /** How far out to the side his wider hand is (cm from his centre line), for
        tests: about 45 in the model's A-pose, about 25 with arms by his sides. */
    float GetWiderHandReach() const;
    /** How far in front of his body line his more forward hand is (cm), for tests: hands at his sides, not held out. */
    float GetHandsForward() const;
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
    /** Standing pose from the model's A-pose (component-space turn per posed
        bone): arms hanging, elbows soft, palms to the thighs, fingers curled. */
    TArray<FQuat> Rest;
    float EyeHeight = 167.f;                      // above the feet, from the head bone
    void SolveRest();
    void Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space) const;
    void UpdatePose(float DeltaSeconds);
};
