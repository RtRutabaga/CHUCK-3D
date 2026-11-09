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
 * JEF_01 to JEF_06, and his strain, JEF_X01); a burning rupture, Phlegethos, drifting toward the bow; a gust throws Chuck off
 * the yard, clear of the side, and he falls into it. Letterboxed, no control; Esc skips. The fall into Phlegethos is not made
 * yet, so it ends on the end card.
 */
UCLASS()
class CHUCK3D_API AShipForeTopsailScene : public AActor
{
    GENERATED_BODY()
public:
    AShipForeTopsailScene();
    virtual void Tick(float DeltaSeconds) override;
    /** Begin now (he is at the head of the fore topmast shrouds on Side, +1 larboard). */
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
    // Beats (s from the start).
    static constexpr float WideAt = 5.5f;
    static constexpr float JeffriesAt = 10.5f;
    static constexpr float RupturesAt = 19.f;
    static constexpr float ClewAt = 25.f;
    static constexpr float BurningAt = 30.f;
    static constexpr float GustAt = 35.5f;
    static constexpr float BelowAt = 36.6f;
    static constexpr float Drift = 140.f;        // cm/s the ruptures slide aft past her on the current
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    UPROPERTY() UProceduralMeshComponent* Ruptures = nullptr;
    UPROPERTY() TArray<UPointLightComponent*> Lights;
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
    void Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov);
    void DriveChuck(float Dt);
    void Lines();
    void BuildRuptures();
    FVector2D RiftAt(const FRupture& R) const;
    // -ChuckShipSceneShots: stills of the ruptures for review (Saved/Screenshots/ShipRuptures).
    bool bShots = false;
    int32 ShotNext = 0;
    void Finish();
};
