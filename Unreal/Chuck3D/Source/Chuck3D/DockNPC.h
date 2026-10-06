#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.generated.h"

class UCapsuleComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class UMaterialInterface;

/**
 * A human NPC on the docks (user 2026-09-30: start with the dock worker by the
 * spawn; more NPCs will follow - the 2D game's guard and market woman).
 * A MakeHuman body dressed by Tools/build_npc_humans.py on the humans' shared
 * 53-bone skeleton (MPFB game_engine, with fingers), posed from its A-pose (a
 * relaxed standing pose solved per body: no arms held out):
 * breathing, a slow weight shift, idle glances, and his head turning to watch
 * Chuck when the rat comes near. Solid to Chuck but not climbable (a
 * pawn-only blocker: wall-run, ledge and camera traces ignore him).
 *
 * Talk: an NPC with Lines shows a prompt in range and Chuck talks to him
 * with F / Y (AChuckCharacter::Interact); lines advance on each press. One
 * with no lines is ambient (the worker, for now: no dialogue yet, user's call).
 * NPC speech only - Chuck never speaks (AGENTS.md).
 */
/** The human NPCs built by Tools/build_npc_humans.py (SourceAssets/NPCs/humans.json). */
enum class EDockHuman : uint8 { Worker, Guard, MarketWoman, GuardWoman, SideGuard, Zombie, Blacksmith, Dwarf, TavernKeeper, ElfElder, GnomeAlchemist, Sailor, Count };

UCLASS()
class CHUCK3D_API ADockNPC : public AActor
{
    GENERATED_BODY()
public:
    ADockNPC();
    virtual void Tick(float DeltaSeconds) override;
    /** The dock worker: the human-scale reference by the spawn (180 cm). */
    static ADockNPC* SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw);
    /** Any of the humans, standing at Feet facing Yaw. */
    static ADockNPC* SpawnHuman(UWorld* World, EDockHuman Kind, const FVector& Feet, float Yaw);
    /** The 2D game's guard at the closed city gate and the market woman by the red awning (References/Original/PHASE-2.md). */
    static void SpawnTownsfolk(UWorld* World);
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
    /** True once its motion-capture idle is playing (tests). */
    bool HasMocap() const { return SkelIndex.Num() > 0; }
    /** Chuck's slash caught him: a start, a lean back, a little shift of the feet, then he turns to the rat. */
    void TakeScratch(const FVector& From);
    int32 GetScratches() const { return Scratches; }
    bool IsReacting() const { return ReactTime >= 0.f; }
    /** Body yaw away from where it was placed (deg), for tests: turned toward Chuck. */
    float GetBodyTurn() const;
    /** How far out to the side his wider hand is (cm from his centre line), for
        tests: about 45 in the model's A-pose, about 25 with arms by his sides. */
    float GetWiderHandReach() const;
    /** The widest a nearly straight arm (elbow within 30 degrees) is held out from hanging (deg from straight down):
        about 45 in the model's A-pose, under 20 hanging by his side; a hand on the hip bends the elbow and doesn't count. */
    float GetStraightArmOut() const;
    /** How far in front of his body line his more forward hand is (cm), for tests: hands at his sides, not held out. */
    float GetHandsForward() const;
    /** Close a hand (0 relaxed .. 1 a fist round a shaft): the guard's spear hand, later. Side 0 = left. */
    void SetGrip(int32 Side, float Amount) { Grip[FMath::Clamp(Side, 0, 1)] = FMath::Clamp(Amount, 0.f, 1.f); }
    /** A guard's spear (Tools/build_spear.py): upright beside the foot on Side (0 left, 1 right), that fist round its grip. */
    void GiveSpear(int32 Side = 1);
    bool HasSpear() const { return bSpear; }
    int32 GetPoleSide() const { return SpearSide; }
    /**
     * The dwarf's battle axe (Tools/build_battle_axe.py: double-bitted, 127 cm):
     * held as the guards hold their spears, butt on the ground beside the foot
     * on Side, that fist round its leather wrap (GetSpearGripError/GetSpearLean
     * measure it too).
     */
    void GiveAxe(int32 Side = 1);
    bool HasAxe() const { return bSpear && bAxe; }
    /** Horizontal distance (cm) from the axe's haft at its head to the nearer shoulder joint, for tests. */
    float GetAxeShoulderGap() const;
    /**
     * A dwarf by the smithy (user 2026-10-04: "a dwarf NPC by the smithy,
     * wearing dwarven armor and holding a battle axe. Bearded."): 134 cm, in
     * mail, breastplate, pauldrons and a nasal helm edged in brass, a long
     * braided beard, his axe grounded at his right hand. He waits on the smith,
     * watching the rat; a couple of short lines.
     */
    static ADockNPC* SpawnDwarf(UWorld* World, const FVector& Feet, float Yaw);
    static inline const FVector DwarfFeet = FVector(-1110.f, -3425.f, 0.f);
    static constexpr float DwarfYaw = -30.f;
    /** How far the spear fist is from the spear's grip (cm), for tests. */
    float GetSpearGripError() const;
    /** How far the spear leans from upright (deg), for tests. */
    float GetSpearLean() const;
    /** Mean bend of the four fingers of the left hand (deg), for tests: curled, not straight. */
    float GetFingerCurl() const;
    /**
     * The sewer zombie (user 2026-10-03: in the wide chamber, "can just barely
     * be killed by the player but is best to avoid"). A gaunt dead man in rags
     * on the same skeleton (an old man's stooped motion capture). It stands
     * swaying until the rat comes near (closer behind it than in front), then
     * shambles after him at less than his walking pace, never far from where it
     * stood and never over a gap in the floor. In reach it rears up, arms
     * lifting (the tell), then lunges down at him: a bite costs two sanity.
     * Fourteen scratches put it down (user 2026-10-03: harder, hitting more often
     * and more easily); it crumples flat and leaves cigarettes.
     */
    static ADockNPC* SpawnZombie(UWorld* World, const FVector& Feet, float Yaw);
    bool IsHostile() const { return Kind == EDockHuman::Zombie; }
    bool IsDead() const { return ZState == EZombie::Dead; }
    int32 GetHitsTaken() const { return Hits; }
    int32 GetBitesLanded() const { return Bites; }
    int32 GetLunges() const { return Lunges; }
    const TCHAR* GetZombieStateName() const;
    /** Shamble speed (cm/s): the walk clip's own, at this body's scale. */
    float GetZombieWalkSpeed() const;
    int32 Cigarettes = 4;
    static constexpr int32 ZombieHealth = 14;
    static constexpr int32 ZombieBite = 2;          // sanity a bite costs
    static constexpr float ZombieNotice = 600.f;    // cm: it notices the rat this close, whichever way it faces
    static constexpr float ZombieSight = 900.f;     // and this far in front of it
    static constexpr float ZombieLeash = 1100.f;    // never further than this from where it stood
    static constexpr float ZombieStrike = 150.f;    // starts the lunge from here
    static constexpr float ZombieBiteRange = 130.f;
    static constexpr float ZombieWindup = .5f;
    static constexpr float ZombieLungeTime = .45f;
    static constexpr float ZombieRecover = .6f;
    /**
     * The plaza smith (user 2026-10-04: "a blacksmith character (gruff white
     * male) by the smithy and forge, make him an anvil that he'll be working
     * at"). Spawns his anvil (Tools/build_smith_props.py) on its stump in front
     * of him, then works at it: a cross-peen hammer in his right fist raised
     * and brought down on a bar of hot iron he holds on the face with tongs in
     * his left, sets of strikes with a pause to turn the bar between them. Each
     * blow rings (SFX_AnvilStrike) and throws a few sparks. He keeps to his
     * anvil: his head follows the rat, his body doesn't turn; talked to, he
     * rests the hammer until the rat goes. Both arms are placed by IK on the
     * motion-capture idle, so the hammer's face meets the bar wherever he
     * stands.
     */
    static ADockNPC* SpawnBlacksmith(UWorld* World, const FVector& Feet, float Yaw);
    /**
     * Spoken lines (docs/NPC-VOICE-PLAN.md; NPCVoiceData.h, generated by
     * Tools/build_npc_voice.py from SourceAssets/NPCs/dialogue.json): the NPC's
     * Lines become the voiced lines' text, and each line Chuck talks through is
     * played aloud from the head (spatial), its loudness opening the jaw and
     * lifting the brows. NPCs built with face bones (humans.json `face`: jaw,
     * upper lids, brows) also blink every few seconds. Returns the lines found.
     */
    int32 SetupVoice(const TCHAR* Npc);
    /** Speak line Index now (tests and talk); false without that voiced line. */
    bool StartVoiceLine(int32 Index);
    bool IsSpeaking() const { return VoiceTime >= 0.f; }
    int32 GetVoiceSoundCount() const { return VoiceSounds.Num(); }
    int32 GetFaceBoneCount() const;
    /** Jaw opening now and the widest since the line began (deg); blinks so far. */
    float GetJawOpen() const { return JawOpen; }
    float GetMaxJawOpen() const { return MaxJawOpen; }
    int32 GetBlinks() const { return Blinks; }
    bool IsSmith() const { return Kind == EDockHuman::Blacksmith; }
    /**
     * The tavern keeper (user 2026-10-04: like the blacksmith but with a
     * medium-length beard and a different apron, behind the counter between
     * the cellar hatch and the barrels, polishing a tankard; no sound, it's a
     * quiet task). Holds a pewter tankard by its handle in his left fist
     * (Tools/build_keeper_props.py) and works a linen rag round inside its rim,
     * then over the outside, turning it as he goes; every third round he holds
     * it up to look it over. Both arms by IK on the halved idle; his eyes on the
     * work unless the rat is near; he stays behind his bar.
     */
    static ADockNPC* SpawnTavernKeeper(UWorld* World, const FVector& Feet, float Yaw);
    bool IsKeeper() const { return Kind == EDockHuman::TavernKeeper; }
    /**
     * The old sailor on the court pier (user 2026-10-05: like the tavern keeper
     * but older with a shorter beard, smoking a pipe). His pipe
     * (Tools/build_sailor_props.py) is clenched in the corner of his mouth
     * and follows his head, a thin wisp rising from the bowl. Every 10 s his
     * right hand comes up to cup the bowl while he draws on it, then drops,
     * and he lets out a stream of smoke. He looks out over the water and turns
     * to the rat when it comes near.
     */
    static ADockNPC* SpawnSailor(UWorld* World, const FVector& Feet, float Yaw);
    bool IsSailor() const { return Kind == EDockHuman::Sailor; }
    int32 GetPipeDraws() const { return PipeDraws; }
    /** Worst distance (cm) of his right fist from the pipe's bowl while drawing, and the smoke puffs he has breathed out. */
    float GetPipeHoldError() const { return WorstPipeHold; }
    int32 GetPipePuffs() const { return PipePuffsSpawned; }
    /** Distance (cm) of the pipe's bit from his mouth, as posed this frame. */
    float GetPipeMouthError() const;
    /** How far his pipe is out of his mouth (0..1): it comes out while he speaks. */
    float GetPipeOut() const { return PipeOut; }
    /** Worst distance (cm, any frame after he settles) of his left fist from the tankard's handle, and of the rag fist from where the polishing wants it. */
    float GetTankardGripError() const { return WorstTankardGrip; }
    float GetRagReachError() const { return WorstRagReach; }
    /** Polishing passes (inside or outside) begun, for tests. */
    int32 GetPolishPasses() const { return PolishPasses; }
    /**
     * An old elf woman on the bench by the fountain (user 2026-10-05: "an
     * elderly elf woman npc sitting on a bench by the fountain, long braided
     * gray hair"). She only ever sits: hips on the bench's front edge, feet
     * flat on the paving (both legs by IK), hands resting on her lap, a little
     * rounded in the back; the motion-capture idle, damped, keeps her
     * breathing and shifting. Her eyes go to the fountain, or to the rat when
     * it comes near; she never turns her body. Hips is where her hip joints
     * sit, on the floor below them; Yaw the way she faces.
     */
    static ADockNPC* SpawnElfElder(UWorld* World, const FVector& Hips, float Yaw);
    bool IsSeated() const { return Kind == EDockHuman::ElfElder; }
    /** The plaza bench she sits on (DockPlaza.cpp: x -440, y -3350, 185 x 50, top 49 cm) and where she sits on it: toward its fountain end, facing the docks. */
    static inline const FVector ElfBench = FVector(-440.f, -3350.f, 49.f);
    static inline const FVector ElfHips = FVector(-410.f, -3341.f, 0.f);
    static constexpr float ElfYaw = 90.f;
    /** Height of her hip joints above the floor (cm, now), and the worst seen after she settles: of either heel off the floor (cm) and of either hand from its place on her lap (cm). */
    float GetSeatHeight() const;
    float GetFootLiftError() const { return WorstFootLift; }
    float GetLapHandError() const { return WorstLapHand; }
    /**
     * The alchemist (user 2026-10-05: "an alchemist vendor in front of the
     * alchemist shop, a gnome in black robes, hands together behind robe
     * sleeves so that they aren't visible, DnD 5e gnome height and facial
     * features"): a 100 cm gnome before the shop's left window, facing the
     * plaza. His forearms are held across in front of him (two-bone IK on
     * the damped idle) so the bell sleeves' cuffs meet; he has no hands to
     * show. He watches the rat and turns to it like the townsfolk.
     */
    static ADockNPC* SpawnAlchemist(UWorld* World, const FVector& Feet, float Yaw);
    bool IsAlchemist() const { return Kind == EDockHuman::GnomeAlchemist; }
    static inline const FVector AlchemistFeet = FVector(1120.f, -3685.f, 0.f);
    static constexpr float AlchemistYaw = 90.f;
    /** Worst distance (cm, after he settles) of either wrist from where his sleeves want it, and the wrists' distance apart now. */
    float GetSleeveReachError() const { return WorstSleeveReach; }
    float GetWristGap() const;
    bool IsForging() const { return IsSmith() && Resting < .5f; }
    int32 GetStrikes() const { return Strikes; }
    /** Light taps of the hammer on the bare face between blows, and how far the face was from it at the last one (cm). */
    int32 GetTaps() const { return Taps; }
    float GetWorstTapGap() const { return WorstTapGap; }
    /** True while the forge's roar is playing beside him. */
    bool IsForgeSounding() const;
    /** How far the hammer's face was from the top of the bar at the last blow (cm), for tests. */
    float GetStrikeGap() const { return StrikeGap; }
    /** The widest such gap over every blow after the first three (cm). */
    float GetWorstStrikeGap() const { return WorstStrikeGap; }
    /** How far the left fist has been from the tongs' reins (cm, the worst on any frame after the first blows), for tests. */
    float GetTongsGripError() const { return WorstTongsGap; }
    AActor* GetAnvil() const { return Anvil.Get(); }
    /** Eyes above the feet (cm), from this body's head bone. */
    float GetEyeHeight() const { return EyeHeight; }
    /** Shake probe (user 2026-10-06: the dwarf's "weird jittery shake"): after 8 s, the bone whose
        velocity most often reverses frame to frame (moving > 3 cm/s each way), how many times, and
        the body's yaw reversals. Smooth motion reverses only at the ends of a sway. */
    FString GetJitterReport() const;
    float GetShakeShare() const { return ProbeFrames ? static_cast<float>(GetWorstReversals()) / ProbeFrames : 0.f; }
    int32 GetWorstReversals() const;
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
    EDockHuman Kind = EDockHuman::Worker;
    UPROPERTY() TObjectPtr<USkeletalMesh> HumanMeshes[static_cast<int32>(EDockHuman::Count)];
    /** Motion-capture clips (CMU, Tools/build_npc_mocap.py) on the shared skeleton: idles and talk. */
    UPROPERTY() TObjectPtr<UAnimSequence> Clips[7];
    float ReactTime = -1.f;                       // since the rat scratched him (-1: not reacting)
    int32 Scratches = 0;
    int32 IdleClip = 0;
    float TalkBlend = 0;                          // 0 idle .. 1 the talk clip
    bool bTalking = false;
    float HomeYaw = 0, TurnHold = 0;              // body turn toward Chuck
    bool bTurning = false;
    TArray<int32> SkelIndex;                      // mesh bone -> skeleton bone
    TArray<FQuat> SourceRest;                     // skeleton rest, component space (the clips' rest)
    FVector SourceHips = FVector::ZeroVector;
    float HipScale = 1.f;
    TArray<FTransform> HandLocal;                 // each hand relative to its forearm in the rest solve (a straight wrist)
    TArray<int32> FingerBone;                     // [side][finger][joint] flattened, mesh bone indices
    TArray<FVector> FingerAxis;                   // each finger joint's bending axis, in its own frame
    float Grip[2] = { 0.f, 0.f };
    float ArmOut = 1.f;                           // which way is out for the left arm (+/-Y)
    void PoseHands(TArray<FTransform>& Space, bool bStraightenWrists) const;
    // Voice and face.
    UPROPERTY() TArray<TObjectPtr<USoundBase>> VoiceSounds;
    UPROPERTY() UAudioComponent* VoiceAudio = nullptr;
    TArray<int32> VoiceLines;                     // indices into NPCVoiceData::Lines, in talk order
    int32 VoiceLine = -1, HeardLine = -1, Blinks = 0;
    float VoiceTime = -1.f, JawOpen = 0.f, MaxJawOpen = 0.f, BrowLift = 0.f, Blink = 0.f, BlinkTime = -1.f, NextBlink = 3.f;
    int32 FaceBone[5] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };   // jaw, lid l/r, brow l/r
    FQuat HeadRefRotation = FQuat::Identity;      // the head's component-space rotation in the model's pose
    float VoiceLoudness() const;
    void TickVoice(float DeltaSeconds);
    void PoseFace(TArray<FTransform>& Space) const;
    UPROPERTY() UStaticMeshComponent* Spear = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> SpearMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> AxeMesh;
    bool bSpear = false, bAxe = false;
    // Where the pole stands and where the fist closes: the spear's, or the axe's (GiveAxe).
    float PoleAhead = 16.f, PoleOut = 30.f, PoleLean = 4.f, PoleGrip = 108.f;
    int32 SpearSide = 1;                          // 0 left hand, 1 right
    FVector SpearGrip = FVector::ZeroVector;      // component space: where his fist closes on the shaft
    void HoldSpear(TArray<FTransform>& Space) const;
    void PlaceSpear();
    void SampleClips(float Time, TArray<FQuat>& BoneDelta, FVector& HipsOffset) const;
    void UpdateTurn(float DeltaSeconds, float YawToChuck, bool bNear);
    /** Standing pose from the model's A-pose (component-space turn per posed
        bone): arms hanging, elbows soft, palms to the thighs, fingers curled. */
    TArray<FQuat> Rest;
    float EyeHeight = 167.f;                      // above the feet, from the head bone
    void SolveRest();
    void Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space, const TArray<FQuat>* BoneDelta = nullptr, const FVector& HipsOffset = FVector::ZeroVector) const;
    void UpdatePose(float DeltaSeconds);
    // The zombie.
    enum class EZombie : uint8 { Idle, Shamble, Windup, Lunge, Recover, Hurt, Dead };
    EZombie ZState = EZombie::Idle;
    float ZTime = 0, WalkTime = 0, WalkBlend = 0, Rear = 0, Reach = 0, Flinch = 0, FlinchTime = -1, DeathTime = -1;
    FVector ZHome = FVector::ZeroVector, LungeDir = FVector::ForwardVector;
    bool bBit = false, bDropped = false;
    int32 Hits = 0, Bites = 0, Lunges = 0;
    void SetZombie(EZombie State) { ZState = State; ZTime = 0; }
    void TickZombie(float DeltaSeconds);
    bool StepZombie(const FVector& Direction, float Distance);
    void TurnZombie(const FVector& Toward, float DeltaSeconds, float Rate);
    // The smith.
    UPROPERTY() UStaticMeshComponent* Hammer = nullptr;
    UPROPERTY() UStaticMeshComponent* Tongs = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* Sparks = nullptr;
    UPROPERTY() UPointLightComponent* StrikeLight = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> SmithMeshes[3];          // anvil, hammer, tongs
    UPROPERTY() TArray<TObjectPtr<USoundBase>> StrikeSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> TapSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> ClinkSounds;
    UPROPERTY() UAudioComponent* ForgeAudio = nullptr;
    UPROPERTY() USoundAttenuation* StrikeAttenuation = nullptr;
    TWeakObjectPtr<AActor> Anvil;
    float ForgeClock = 0, Resting = 0, Swing = .12f, BarRoll = 0, BarLift = 0, FlashTime = 1.f;
    float Cock = .12f;        // the hammer's turn in his fist (0 face down on the work .. 1 raised); lags Swing on the way down: the wrist snap
    float TapBlend = 1.f;     // 0 aimed at the bar .. 1 at the tap spot on the heel
    float Drive = 0.f;        // the downswing's effort (0..1), and the body behind it
    float SinceBlow = 10.f;   // s since the last heavy blow (the recoil)
    float Inspect = 0.f;      // 0..1 lifting the bar up to look at it
    float BarPull = 0.f;      // cm the bar is drawn back toward him
    int32 Taps = 0;
    float WorstTapGap = 0.f;
    bool bTapDue = false, bClinkDue = false;
    int32 Strikes = 0;
    bool bStrikeDue = false;
    float StrikeGap = 1e3f, WorstStrikeGap = 0.f, TongsGripError = 1e3f, WorstTongsGap = 0.f, HammerFistError = 0.f;
    float PalmSign[2] = { 1.f, -1.f };   // palm side relative to Along x Thumb, measured on the posed hand
    struct FSpark { FVector At, Velocity; float Age, Life; };
    TArray<FSpark> SparkState;
    void TickSmith(float DeltaSeconds);
    void PoseSmith(TArray<FTransform>& Space);
    void Strike();
    void Tap();
    void SmithEvents();
    // The tavern keeper.
    UPROPERTY() UStaticMeshComponent* Tankard = nullptr;
    UPROPERTY() UStaticMeshComponent* Rag = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> KeeperMeshes[2];          // tankard, rag
    float PolishClock = 0, WorstTankardGrip = 0, WorstRagReach = 0;
    int32 PolishPasses = 0, PolishPhase = -1;
    void PoseKeeper(TArray<FTransform>& Space);
    // The sailor.
    UPROPERTY() UStaticMeshComponent* Pipe = nullptr;
    UPROPERTY() UStaticMeshComponent* PipeWisp = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* PipeBreath = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> SailorMeshes[3];          // pipe, the cigarette's smoke wisp, a sphere for breath puffs
    UPROPERTY() TObjectPtr<UMaterialInterface> PuffMaterial;
    FTransform HeadRef = FTransform::Identity;                    // the head bone in the model's own pose (the mouth was measured on it)
    float PipeClock = 0, PipeHold = 0, WorstPipeHold = 0, PipeExhaleCarry = 0;
    float PipeOut = 0;   // 0 the pipe in his mouth .. 1 taken out and held at his chest while he speaks
    int32 PipeDraws = 0, PipePuffsSpawned = 0;
    bool bPipeHeld = false;
    struct FPuff { FVector At, Velocity; float Age, Life, Size; };
    TArray<FPuff> PipePuffs;
    void PoseSailor(TArray<FTransform>& Space);
    void TickSailor(float DeltaSeconds);
    // The old elf on her bench.
    float SeatDrop = 0.f;                 // cm her hips come down from standing to the bench
    FVector2D GlanceCentre = FVector2D::ZeroVector;   // idle glances about this (yaw, pitch): the old elf's fountain
    FQuat FootRest[2];                    // each foot flat, as in the standing rest pose
    float AnkleRest = 8.f;                // ankle height standing
    float WorstFootLift = 0.f, WorstLapHand = 0.f;
    void PoseSeated(TArray<FTransform>& Space);
    /** The clips, low-passed (ClipSmoothing): the CMU takes carry frame-to-frame noise that shook the legs. */
    TArray<FQuat> SmoothDelta;
    FVector SmoothHips = FVector::ZeroVector;
    TArray<FVector> ProbePrev, ProbeVel;
    TArray<int32> ProbeReversals;
    TArray<float> ProbeSwing;      // summed |velocity change| at each reversal (cm/s)
    int32 ProbeFrames = 0;
    float ProbeYawPrev = 0.f, ProbeYawVel = 0.f;
    int32 YawReversals = 0;
    void ProbeShake(const TArray<FTransform>& Space, float DeltaSeconds);
    // The gnome alchemist's sleeves.
    float WorstSleeveReach = 0.f;
    void PoseSleeves(TArray<FTransform>& Space);
    /** Two-bone IK: aim Upper and Lower so End lands at Target, the middle joint toward Pole (Space updated down the chain). */
    void TwoBone(TArray<FTransform>& Space, int32 Upper, int32 Lower, int32 End, const FVector& Target, const FVector& Pole) const;
    /** Two-bone IK: the fist of arm Side to Fist, the hand's knuckles along Along with its thumb side along Thumb. */
    void PlaceHand(TArray<FTransform>& Space, int32 Side, const FVector& Fist, const FVector& Along, const FVector& Thumb, const FVector& Pole);
    /** A posed hand's fist centre and axes (component space). */
    void HandFrame(const TArray<FTransform>& Space, int32 Side, FVector& Fist, FVector& Along, FVector& Thumb) const;
};
