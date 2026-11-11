#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipEndingScene.generated.h"

class ACameraActor;
class AChuckCharacter;
class UProceduralMeshComponent;
class UPointLightComponent;

/**
 * The fore topsail cutscene (Claude, 2026-11-08; docs/regions/SHIP-STORM-ENDING.md step 5). At the head of the fore topmast
 * shrouds he takes the fore topsail yard and goes out along it toward the jammed clewline; the whole brig in the storm; Astral
 * ruptures opening in the sea round her, sliding past on the current; Jeffries at the mainmast sees them (his voiced lines,
 * JEF_01 to JEF_06, and his strain, JEF_X01); a burning rupture, Phlegethos, drifting toward the bow; a great sea comes
 * down on her from the far side, breaks over her and knocks her over; hanging from the yardarm he swings out and loses his
 * grip, and falls straight down, clear of her side, into it (user 2026-11-09: never a jump; and wide shots of her in the
 * storm). Letterboxed, no control; Esc skips. The fall into Phlegethos is not made yet, so it ends on the end card.
 */
UCLASS()
class CHUCK3D_API AShipForeTopsailScene : public AActor
{
    GENERATED_BODY()
public:
    AShipForeTopsailScene();
    virtual void Tick(float DeltaSeconds) override;
    /** Begin now (he is at the head of the fore topmast shrouds on Side, +1 starboard). */
    static AShipForeTopsailScene* Play(UWorld* World, float Side);
    static AShipForeTopsailScene* Get(const UWorld* World);
    void Skip();
    bool IsLetterboxed() const { return bPlaying; }
    bool IsFinished() const { return bFinished; }
    float GetClock() const { return Clock; }
    const TCHAR* GetShotName() const { return Shot; }
    int32 GetYardLeaps() const { return YardLeaps; }
    bool HasThrown() const { return bThrown; }
    bool HasPlunged() const { return bPlunged; }
    /** How far from the burning rupture's middle he went into the sea (cm). */
    float GetPlungeMiss() const { return PlungeMiss; }
    /** The most she has been knocked over (degrees of heel). */
    float GetMaxHeel() const { return MaxHeel; }
    // Beats (s from the start).
    static constexpr float WideAt = 5.5f;        // "SeaLevel": low over the swell, off her lee bow
    static constexpr float AerialAt = 9.f;       // "Aerial": high and far off her weather quarter
    static constexpr float JeffriesAt = 12.5f;
    static constexpr float RupturesAt = 19.f;
    static constexpr float ClewAt = 24.f;
    static constexpr float AsternAt = 27.5f;     // "Astern": low, far astern to weather
    static constexpr float BurningAt = 31.f;
    static constexpr float ComberAt = 35.f;      // "Comber": the great sea coming down on her
    static constexpr float WaveHit = 37.3f;      // ...breaking on her side; she goes over
    static constexpr float GripAt = 37.6f;       // "Grip": close on him at the yardarm
    static constexpr float SlipAt = 37.7f;       // ...the yard jolts; he loses his grip (SwingSlip)
    static constexpr float BelowAt = 39.f;
    static constexpr float KnockDeg = 26.f;      // her heel in the knock-down
    static constexpr float Drift = 140.f;        // cm/s the ruptures slide aft past her on the current
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    UPROPERTY() UProceduralMeshComponent* Ruptures = nullptr;
    UPROPERTY() TArray<UPointLightComponent*> Lights;
    UPROPERTY() UProceduralMeshComponent* Comber = nullptr;
    // Losing his grip: held at the yardarm's grip (world), his facing, the pendulum about it (rad, rad/s; + outboard),
    // then falling straight down (along the sea's up) from where he let go.
    bool bHeld = false, bHitDone = false;
    FVector SlipGrip = FVector::ZeroVector, PrevCentre = FVector::ZeroVector, FallP = FVector::ZeroVector, FallV = FVector::ZeroVector;
    FQuat Facing = FQuat::Identity, FallRot = FQuat::Identity;
    float Phi = 0.f, PhiRate = 0.f, ReleasedAt = -1.f, KnockRoll = 0.f, MaxHeel = 0.f;
    int32 StrikeNext = 0;
    void HoldChuck(float Dt);
    void BuildComber();
    void Knock();
    /** Where (ship frame) and how soon a fall from P at V lands on the sea knocked over to KnockRoll. */
    float PredictFall(const FVector& P, const FVector& V, FVector& Out) const;
    float Side = 1.f, Clock = 0.f, EndAt = -1.f, LandAt = 0.f, PlungeMiss = 0.f, VolumeNow = 1.f;
    bool bPlaying = false, bFinished = false, bCaught = false, bPlaced = false, bLetGo = false, bThrown = false, bPlunged = false, bSkipping = false;
    int32 YardLeaps = 0, Next = 0;
    float LeapWait = 0.f;
    const TCHAR* Shot = TEXT("");
    FVector Eye = FVector::ZeroVector, Aim = FVector::ZeroVector, Land = FVector::ZeroVector;
    bool bCut = true;
    struct FRupture { FVector2D At; float R, Seed; bool bBurning; };
    TArray<FRupture> Rifts;
    void Begin(float InSide);
    /** bLevel: the horizon held level (the camera off her, at sea: she is seen to roll and heel); else the ship's frame. */
    void Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov, bool bLevel = false);
    void DriveChuck(float Dt);
    void Lines();
    void BuildRuptures();
    FVector2D RiftAt(const FRupture& R) const;
    // -ChuckShipSceneShots: stills of the ruptures for review (Saved/Screenshots/ShipRuptures).
    bool bShots = false;
    int32 ShotNext = 0;
    void Finish();
};
