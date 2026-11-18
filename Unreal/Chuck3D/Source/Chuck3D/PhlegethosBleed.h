#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhlegethosBleed.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class ACameraActor;
class AChuckCharacter;
class APhlegethosSetting;

/**
 * The Feywild bleedthrough at Phlegethos's fortress (Claude, 2026-11-17; docs/regions/PHLEGETHOS.md#the-feywild-bleedthrough),
 * after the 2D Phase 8's fifth beat: "Initially Astral Sea blocks appear and cut off Chuck's retreat. As the battle
 * continues additional Astral blocks appear as fall hazards. Later moving Feywild river blocks begin flowing through the
 * arena. Eventually it becomes impossible to avoid both the Astral hazards and the River hazards."
 *
 * The user (2026-11-17): keep him in the staging area during the encounter; the appearing ruptures block the exit and are
 * an obstacle course, "maybe ... similar with how the pressure plate fall away to astral rupture section worked in the
 * temple". So the front of the forecourt (Phleg::FieldX0..) is a field of basalt slabs over the Astral, on a jittered
 * grid; each slab is its own mesh. When the trio's entrance scene releases him onto the island:
 *  - the moat round the island cracks (violet light in the cracks, a shudder) and falls away, tearing outward from him:
 *    behind him toward the wastes, in front toward the battle, at both sides. Too wide to jump.
 *  - waves of the island's slabs crack and fall: its -Y end, its +Y end, then scattered through it. A cracking slab he
 *    stands on falls .6 s after (the temple's slabs); the river's columns never fall.
 *  - the river: a tear opens over the +Y moat and Feywild water pours out of it down the island's middle columns to a
 *    second tear over the -Y moat, which swallows it.
 *  - the collapse: the rest of the island falls in from its far ends toward him; anything he stands on cracks at once.
 *    The river is all that is left.
 * Into the water (walked, jumped, from anywhere it runs): he is swept away down it and into the far tear, the view going
 * to black. (The river cutscene and the Feywild are later slices: for now he is returned to the forecourt's checkpoint,
 * as from an Astral death, and it all re-arms.)
 * (2026-11-17, the user: "when chuck dies keep going don't reset the astral fall away".) A death in it (a fall into the
 * Astral, the fiend) does not re-arm it: he comes back on the ground by the breach that holds, and it goes on.
 * The slabs under the trio, the fiend and the lemures hold while they stand on them; the river's columns never fall.
 */
UCLASS()
class CHUCK3D_API APhlegBleed : public AActor
{
    GENERATED_BODY()
public:
    APhlegBleed();
    virtual void Tick(float DeltaSeconds) override;
    static APhlegBleed* Get(const UWorld* World);
    /** Build the slab field, the Astral under it, the river and its tears (the stage's frame). */
    void Build(APhlegethosSetting* InStage);
    /** Every slab back, the river and its tears gone, waiting for him on the island again. */
    void Rearm();
    /** While it runs, where a death returns him (world, his centre): the ground by the breach that holds. */
    static bool ArenaRespawn(FVector& OutWorld);

    enum class EZone : uint8 { MoatBack, MoatSide, MoatFront, Land, River };
    enum class EStage : uint8 { Waiting, Running, Swept };
    EStage GetStage() const { return Stage; }
    /** Seconds since it started (the moat), or -1 waiting. */
    float GetClock() const { return Stage == EStage::Waiting ? -1.f : Clock; }
    /** Whether a local point (X, Y) is open to the Astral: its slab has fallen. */
    bool OpenAt(float X, float Y) const;
    bool IsRiverRunning() const { return RiverOpen > 0.f; }
    /** Local Y the river's front has reached (it runs from IslandY1 toward IslandY0). */
    float GetRiverFront() const { return RiverFront; }
    /** The water's surface (local Z). */
    float GetWaterZ() const;
    int32 GetSlabCount() const { return Slabs.Num(); }
    int32 GetFallen(EZone Zone) const;
    int32 GetWhole(EZone Zone) const;
    int32 GetStarts() const { return Starts; }
    int32 GetSweeps() const { return Sweeps; }
    int32 GetSweepEnds() const { return SweepEnds; }
    int32 GetRearms() const { return Rearms; }
    int32 GetPressedFalls() const { return PressedFalls; }
    int32 GetMissing() const { return Missing; }
    int32 GetKeptCells() const { return KeptCells; }
    int32 GetDeathsIn() const { return DeathsIn; }
    int32 GetHeldFalls() const { return HeldFalls; }
    FVector GetRespawnLocal() const { return RespawnLocal; }
    /** Tests: the slab under a local point (by the unjittered grid): 0 whole, 1 cracking, 2 falling or gone, -1 none. */
    int32 SlabStateAt(float X, float Y) const;
    /** Tests: the middle of the whole slab in column Col of the island nearest NearY (false if none). */
    bool FindWhole(int32 Col, float NearY, FVector& Out) const;
    /** Tests: hold it off (the trio trial watches the battle undisturbed). */
    static bool bHold;
    // The escalation, in seconds from the start.
    static constexpr float Wave1At = 9.f;
    static constexpr float Wave2At = 17.f;
    static constexpr float Wave3At = 24.f;
    static constexpr float RiverAt = 31.f;
    static constexpr float CollapseAt = 41.f;
    static constexpr float CollapseSpan = 9.f;
    static constexpr float RiverSpeed = 800.f;   // cm/s, its front down the island
    static constexpr float WaterDepth = 24.f;    // cm over the slabs
private:
    enum class ESlab : uint8 { Whole, Cracking, Falling, Gone };
    struct FSlab
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        FVector Centre = FVector::ZeroVector;   // local: the middle of its top
        int32 Col = 0, Row = 0;
        EZone Zone = EZone::Land;
        ESlab State = ESlab::Whole;
        float Due = -1.f, CrackFor = 1.4f, At = 0.f, PressedAt = -1.f;
        bool bForced = false;   // falls when due even with him on it (the moat, the collapse)
        float CollapseDue = -1.f;   // in the collapse, when it goes (if no one of the battle stands on it then)
        FVector Drop = FVector::ZeroVector, Vel = FVector::ZeroVector;
        FRotator Rot = FRotator::ZeroRotator, Spin = FRotator::ZeroRotator;
    };
    TArray<FSlab> Slabs;
    void PlaceSlab(FSlab& S) const;
    void Start(AChuckCharacter* Chuck);
    void TickSlabs(float Dt, AChuckCharacter* Chuck);
    void Crack(FSlab& S, float Due, float CrackFor, bool bForced);
    void Fall(FSlab& S);
    void TickRiver(float Dt, AChuckCharacter* Chuck);
    void UpdateRiverMesh();
    void TickSweep(float Dt, AChuckCharacter* Chuck);
    void PlayAt(USoundBase* Sound, const FVector& Local, float Volume, float Pitch = 1.f);
    int32 SlabIndexAt(float X, float Y) const;
    /** One of the battle (a hero, the fiend, a lemure) stands on it: it holds. */
    bool Held(const FSlab& S) const;
    void GatherHolders();
    TArray<FVector2D> Holders;
    TArray<int32> CellSlab;   // Col * FieldRows + Row: its slab's index, -1 kept ground
    FVector RespawnLocal = FVector::ZeroVector;
    bool bWasHidden = false;

    UPROPERTY() APhlegethosSetting* Setting = nullptr;
    UPROPERTY() UProceduralMeshComponent* Void = nullptr;
    UPROPERTY() UProceduralMeshComponent* Water = nullptr;
    UPROPERTY() UProceduralMeshComponent* TearIn = nullptr;
    UPROPERTY() UProceduralMeshComponent* TearOut = nullptr;
    UPROPERTY() TArray<UPointLightComponent*> Lights;   // 0 the source tear, 1 the swallowing tear, 2 along the river, 3.. the void
    UPROPERTY() UAudioComponent* RiverSound = nullptr;
    UPROPERTY() ACameraActor* SweepCam = nullptr;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> Sounds;   // see the .cpp
    EStage Stage = EStage::Waiting;
    float Clock = 0.f, OnFor = 0.f, RiverOpen = 0.f, RiverFront = 1e9f, LastDust = -1.f, WaveDone = 0.f;
    bool bCollapse = false, bTears = false, bMoatHeard = false;
    // The river's centre line (local): the spout out of the source tear, the run down the island, the pour into the far tear.
    TArray<FVector> Course;
    TArray<float> CourseS;   // metres along it
    int32 RunFirst = 0, RunLast = 0;
    // The sweep.
    float SweepT = 0.f, SweepSpeed = 0.f, FadeAt = -1.f;
    FVector SweepAt = FVector::ZeroVector;   // local, his capsule's centre
    int32 Starts = 0, Sweeps = 0, SweepEnds = 0, Rearms = 0, PressedFalls = 0, Missing = 0, KeptCells = 0, DeathsIn = 0, HeldFalls = 0;
};
