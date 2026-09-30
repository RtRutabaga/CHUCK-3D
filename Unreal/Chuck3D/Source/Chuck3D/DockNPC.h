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
 * Procedurally posed on a plain 20-bone skeleton (Tools/build_dock_npc.py):
 * breathing, a slow weight shift, idle glances, and his head turning to watch
 * Chuck when the rat comes near. Solid to Chuck but not climbable (a
 * pawn-only blocker: wall-run, ledge and camera traces ignore him).
 *
 * Talk: an NPC with Lines shows a prompt in range and Chuck talks to him
 * with F / Y (AChuckCharacter::Interact); lines advance on each press. One
 * with no lines is ambient (the worker, for now: no dialogue yet, user's call).
 * NPC speech only - Chuck never speaks (AGENTS.md).
 */
UCLASS()
class CHUCK3D_API ADockNPC : public AActor
{
    GENERATED_BODY()
public:
    ADockNPC();
    virtual void Tick(float DeltaSeconds) override;
    /** The dock worker: the human-scale reference by the spawn (180 cm). */
    static ADockNPC* SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw);
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
    void UpdatePose(float DeltaSeconds);
};
