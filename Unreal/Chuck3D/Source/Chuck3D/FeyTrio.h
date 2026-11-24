#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyTrio.generated.h"

class USkeletalMeshComponent;
class UBoxComponent;
class UGroomComponent;

/**
 * The Crossroads trio (Claude, 2026-11-23; the user, with References/Feywild/crossroads-trio.png: "create these guys and
 * have them mill around next to the giant tree in crossroads (I'll give the dialogue later)"): a centaur carrying a
 * laughing satyr on his back, a scowling old gnome on the satyr's shoulders pointing the way. Tools/build_fey_trio.py
 * made them, Tools/build_fey_trio_anims.py their one loop (AS_<Name>_Mill, 60 s), played here together in lockstep from
 * one clock: the centaur's Root bone carries all three round a circle 4 m across (stand, walk half round, stand, walk
 * back, stand), so the actor stays where it was put. All three meshes share one place, raised by the centaur's lift (his
 * hooves are that far below the human's floor in the build). A box follows the centaur's body so he cannot be walked
 * through; their strand hair rides on their heads (ChuckGroom). Spawned by AFeyLife with the Crossroads, gone with it.
 * No lines yet: the user will supply them.
 */
UCLASS()
class CHUCK3D_API AFeyTrio : public AActor
{
    GENERATED_BODY()
public:
    AFeyTrio();
    virtual void Tick(float DeltaSeconds) override;
    /** The trio standing at World (on the ground), the start of its loop heading Yaw. */
    static AFeyTrio* Spawn(UWorld* World, const FVector& Ground, float Yaw);
    static constexpr float Lift = 45.3f;      // cm: the centaur's hooves below the human's floor (manifest lift_cm; 2026-11-24 horse)
    static constexpr float Loop = 60.f;       // the clip's length (s)
    /** Tests: where the centaur's body is now (world), and how many parts loaded. */
    FVector GetBodyAt() const;
    int32 GetLoaded() const { return Loaded; }
    /** Stills: hold the loop at this time (s); < 0 runs. */
    static float HoldAt;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TArray<USkeletalMeshComponent*> Parts;
    UPROPERTY() TArray<UGroomComponent*> Grooms;
    UPROPERTY() UBoxComponent* Body = nullptr;
    float Clock = 0.f;
    int32 Loaded = 0;
};
