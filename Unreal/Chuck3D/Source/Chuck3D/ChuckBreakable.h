#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuckBreakable.generated.h"

/**
 * Anything Chuck's slash can break: grass tufts now; jars, crates and small
 * enemies later (user 2026-09-30: shred grass and break jars for the
 * cigarettes inside, Zelda-style). Breakables have no collision of their own
 * for the slash to find (grass must not catch paw, ledge or camera traces), so
 * each registers itself and the slash tests reach against this list.
 */
UCLASS(Abstract)
class CHUCK3D_API AChuckBreakable : public AActor
{
    GENERATED_BODY()
public:
    /** Every live breakable in play. */
    static const TArray<TWeakObjectPtr<AChuckBreakable>>& All();
    bool IsBroken() const { return bBroken; }
    /** Reach test footprint: radius around the actor origin and height above it (cm). */
    float GetHitRadius() const { return HitRadius; }
    float GetHitHeight() const { return HitHeight; }
    /** Cigarettes released when it breaks (pickups: next pass). */
    int32 Cigarettes = 0;
    /** Struck by a slash sweeping along Swing (world, unit). */
    virtual void Break(const FVector& Swing) { bBroken = true; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool bBroken = false;
    float HitRadius = 15.f;
    float HitHeight = 25.f;
};
