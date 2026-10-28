#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultFoliageFade.generated.h"

class UMaterialParameterCollection;

// See-through undergrowth in Chult (Claude, 2026-10-27; docs/CHULT-FOLIAGE-FADE.md): the jungle's leaf materials
// (Tools/chult_foliage_fade.py) dither out about two thirds of their leaf round Chuck and between the camera and
// him, up to a little over his head. This sets where he and the camera are (MPC_ChultFoliageFade) every frame,
// and switches it off in the fall cutscene and whenever he is hidden.
UCLASS()
class CHUCK3D_API AChultFoliageFade : public AActor
{
    GENERATED_BODY()
public:
    AChultFoliageFade();
    virtual void Tick(float DeltaSeconds) override;
    static void Spawn(UWorld* World);
    bool HasCollection() const { return Collection != nullptr; }
    /** How strongly it is on now (0..1; it eases in and out over a quarter second). */
    float GetStrength() const { return Strength; }
    /** Held off whatever happens (the review's comparison stills). */
    static bool bForceOff;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UMaterialParameterCollection> Collection;
    float Strength = 0.f;
};

// -ChuckChultTest -ChuckFoliageFadeReview: Chuck in the clearing's ferns and elephant ears, stills with the fade on
// and off from two places; logs CHUCK_FOLIAGE_FADE_REVIEW and quits.
void ReviewChultFoliageFade(AActor* Owner, class AChuckCharacter* Chuck, int32 Failures);
