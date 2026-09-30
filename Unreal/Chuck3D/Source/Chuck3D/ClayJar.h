#pragma once
#include "CoreMinimal.h"
#include "ChuckBreakable.h"
#include "ClayJar.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UCapsuleComponent;
class USoundBase;

/**
 * A small glazed clay storage jar (Tools/build_clay_jar.py). Solid to Chuck
 * (a pawn-only blocker, so paw, ledge, mantle and camera traces ignore it: he
 * can't hop up onto it), broken by a slash into 12 shards that scatter along
 * the swing and settle, releasing the cigarettes inside. User 2026-09-30:
 * Chuck isn't strong - grass and jars (urns in later maps), never crates or
 * barrels.
 */
UCLASS()
class CHUCK3D_API AClayJar : public AChuckBreakable
{
    GENERATED_BODY()
public:
    AClayJar();
    virtual void Tick(float DeltaSeconds) override;
    virtual void Break(const FVector& Swing) override;
    int32 GetShardsFlying() const { return Shards.Num(); }
    bool PlayedBreakSound() const { return bSoundPlayed; }
    bool BlocksChuck() const;
    static AClayJar* Place(UWorld* World, const FVector2D& At, float Yaw, int32 Cigarettes, float MaxZ = 10.f);
    /** The docks' jars: by the tavern door, the warehouse, the market stalls, the timber yard. */
    static void SpawnDockJars(UWorld* World);
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() UCapsuleComponent* Blocker;
    UPROPERTY() UStaticMeshComponent* Body;
    UPROPERTY() TArray<UStaticMesh*> ShardMeshes;
    UPROPERTY() TArray<UStaticMeshComponent*> ShardParts;
    UPROPERTY() TArray<USoundBase*> BreakSounds;
    bool bSoundPlayed = false;
    struct FShard { FVector Position; FVector Velocity; FRotator Rotation; FRotator Spin; int32 Bounces; bool bLanded; };
    TArray<FShard> Shards;
    float BrokenTime = 0;
};
