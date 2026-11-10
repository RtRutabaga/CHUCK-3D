#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultLanding.generated.h"

// Setting contract: same floor, origin, facing and near trees as FallCutscene.
// This first pass is ground exploration; canopy traversal belongs farther in.
UCLASS()
class CHUCK3D_API AChultLanding : public AActor
{
    GENERATED_BODY()
public:
    AChultLanding();
    static FVector FloorOrigin();
    static FVector Entrance();
    static bool Contains(const FVector& Position);
    static bool OnGroundRoute(const FVector& LocalPosition, float Margin);
    static AChultLanding* EnterPreview(UWorld* World);
    /** Environment plus set (the checkpoint and the end of the fall). */
    void Build();
    /** The fall cutscene builds the set early (its floor shots happen here) and
     *  brings in this lighting when he crashes through the canopy. Idempotent. */
    void BuildSet();
    void BuildEnvironment();
    void VerifyAndCapture();
    /** While true, Chult's procedural meshes cook their collision on worker
     *  threads (the fall cutscene: nothing touches the ground for 25 s). */
    static inline bool bAsyncCook=false;
    /** Runs Fn Delay seconds after every Chult collision mesh has finished
     *  cooking (at once plus Delay when nothing is pending). */
    static void AfterCollision(AActor* Owner,float Delay,TFunction<void()> Fn);
    static int32 PendingCollision(UWorld* World);
private:
    bool bSetBuilt=false, bEnvironmentBuilt=false;
};
