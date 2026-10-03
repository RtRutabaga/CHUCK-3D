#pragma once
#include "CoreMinimal.h"
#include "ChuckBreakable.h"
#include "GrassTuft.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USoundBase;
class UMaterialInterface;

/**
 * A scruffy dock weed tuft (Tools/build_grass_tuft.py). Chuck walks through
 * it; a slash shreds it to stubble with a spray of clippings along the swing
 * and a crisp rustle. It stays cut for now (no regrowth).
 */
UCLASS()
class CHUCK3D_API AGrassTuft : public AChuckBreakable
{
    GENERATED_BODY()
public:
    AGrassTuft();
    virtual void Tick(float DeltaSeconds) override;
    virtual void Break(const FVector& Swing) override;
    /** 0..2: the three tuft shapes (and their matching stubble). */
    void SetVariant(int32 Index);
    int32 GetClippingsFlying() const { return Flying.Num(); }
    bool PlayedShredSound() const { return bSoundPlayed; }
    /** A tuft on the ground under XY (a downward trace), unless the ground there is higher than MaxZ. */
    static AGrassTuft* Plant(UWorld* World, const FVector2D& At, int32 Variant, float Yaw, float Scale, float MaxZ = 10.f);
    /** A tuft on the floor near Ground (traced down from just above it: under other floors, e.g. in the sewer). */
    static AGrassTuft* PlantAt(UWorld* World, const FVector& Ground, int32 Variant, float Yaw, float Scale);
    /** The sewer's version: damp moss and fungus (M_SewerMoss), squat, no wind. */
    void SetMoss();
    /** The docks' grass: patches along walls, the yard, the garden and the timber yard. */
    static void SpawnDockGrass(UWorld* World);
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() UStaticMeshComponent* Blades;
    UPROPERTY() UInstancedStaticMeshComponent* Clippings;
    UPROPERTY() TArray<UStaticMesh*> TuftMeshes;
    UPROPERTY() TArray<UStaticMesh*> StubMeshes;
    UPROPERTY() TArray<USoundBase*> ShredSounds;
    UPROPERTY() UMaterialInterface* MossMaterial = nullptr;
    int32 Variant = 0;
    bool bSoundPlayed = false;
    struct FClipping { FVector Position; FVector Velocity; FRotator Rotation; FRotator Spin; float Scale; bool bLanded; };
    TArray<FClipping> Flying;
    float FlyTime = 0;
};
