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
class AChuckCharacter;

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
enum class EDockHuman : uint8 { Worker, Guard, MarketWoman, GuardWoman, SideGuard, Zombie, Blacksmith, Dwarf, TavernKeeper, ElfElder, GnomeAlchemist, Sailor, Bobert,
    // The pirate ship's crew (Claude, 2026-11-02; docs/SHIP.md milestone 3; DockNPCPirate.inl, ShipCrew.cpp). Loaded only when
    // the ship spawns them (the others are referenced by every NPC).
    PirateChef, PirateSeated, PirateConcertina, PirateCheering, PirateDancer, PirateFencerA, PirateFencerB, Jeffries, PirateCaptain, Count };

/** A fencer's sword positions (2026-11-03). ShipCrew.cpp sets them out in time; DockNPCPirate.inl makes each a fist and blade.
 *  Wind-ups (Chamber*, SwingHigh) the blade sweeps through; blows (Cut*, Thrust, BeatAside, BindPress) land on his partner's
 *  blade wherever it is; parries are held where his partner's blow will come; Lunge and Feint aim at his partner's body. */
enum class EFencePose : uint8 { Guard, ChamberHigh, SwingHigh, CutHead, ChamberFlank, CutFlank, ChamberLow, CutLow, Thrust, Lunge, Feint,
    Disengage, ParryHigh, ParryIn, ParryOut, ParryLow, Knocked, BeatAside, BindHold, BindPress, Count };

/** A moment of a fencer's phrase: the sword position he is in at Beat, whether his sword passes through it (a wind-up flowing
 *  into the cut) or stops there (a guard, a parry, a blow landing), and his body then (lunge 0..1, cm stepped toward his
 *  partner, leaning back 0..1). */
struct FFenceKey
{
    float Beat = 0.f;
    EFencePose Pose = EFencePose::Guard;
    bool bFlow = false;
    float Lunge = 0.f, Off = 0.f, Lean = 0.f;
};

/** Cubic between keys K[1] and K[2] (K[0] and K[3] their neighbours) at beat Now: where he stops at a key it is still there,
 *  where he passes through it the motion carries on through it. V are the keys' values. */
template <typename T>
T FenceCurve(const FFenceKey K[4], const T V[4], float Now)
{
    const float H = FMath::Max(K[2].Beat - K[1].Beat, 1e-3f);
    const float S = FMath::Clamp((Now - K[1].Beat) / H, 0.f, 1.f), S2 = S * S, S3 = S2 * S;
    const T M0 = K[1].bFlow ? (V[2] - V[0]) * (H / FMath::Max(K[2].Beat - K[0].Beat, 1e-3f)) : V[1] * 0.f;
    const T M1 = K[2].bFlow ? (V[3] - V[1]) * (H / FMath::Max(K[3].Beat - K[1].Beat, 1e-3f)) : V[1] * 0.f;
    return V[1] * (2.f * S3 - 3.f * S2 + 1.f) + M0 * (S3 - 2.f * S2 + S) + V[2] * (3.f * S2 - 2.f * S3) + M1 * (S3 - S2);
}

/** What the ship's crew tells a fencing pirate each frame (ShipCrew.cpp): where to stand and what his sword does. */
struct FFenceCue
{
    FVector Centre = FVector::ZeroVector;   // world: the middle between his feet, on the deck
    FVector Facing = FVector::ForwardVector;// world, level: toward his partner
    FVector Target = FVector::ZeroVector;   // world: his partner's chest
    FVector Head = FVector::ZeroVector;     // world: his partner's head
    FVector PartnerBase = FVector::ZeroVector, PartnerTip = FVector::ZeroVector;   // world: his partner's blade (zero: none yet)
    FFenceKey Key[4];                       // his sword's keys round now (between Key[1] and Key[2])
    float Now = 0.f;                        // beats into the phrase
    float Lunge = 0, Lean = 0, Twist = 0, Crouch = 0;   // his body now (twist: the sword shoulder driven forward +, drawn back -)
    float Stride = 0;                       // cm the front foot leads (advancing) or trails (retreating) this moment
};

/** A sailor's work in the ship's storm (2026-11-04, DockNPCPirate.inl): hauling on a line along the deck with his mate,
 *  sweating a halyard up at the fife rail, or the captain at the helm. */
enum class EStormWork : uint8 { None, Haul, Sweat, Helm };

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
    FString GetTalkSubtitle(int32 Index) const;
    /** Not while an ambient line is running (the tavern at night): those aren't conversations. */
    bool CanTalk() const { return Lines.Num() > 0 && AmbientLine < 0 && !IsAmbientSpeaking() && StormWork == EStormWork::None; }   // (busy in the storm)
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
    /** Where a hand's bone is in the world (Side 0 left, 1 right), for tests' cameras. */
    FVector GetHandWorld(int32 Side) const;
    /** Wrist to middle knuckle in his standing pose (cm), and the scale of the fist the hand code holds things in (1: a
        grown man's; the halfling captain's hand is smaller, so what he holds sits in his fist, not past his fingers). */
    float GetHandLength() const;
    void SetFistScale(float Scale) { FistScale = FMath::Clamp(Scale, .4f, 1.2f); }
    float GetFistScale() const { return FistScale; }
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
    int32 GetZombieSoundCount() const { return ZombieSounds.Num(); }
    /** Its grabs so far, and the widest gap between its hands and his chest while he was held (cm). */
    int32 GetGrabs() const { return Grabs; }
    float GetWorstHoldGap() const { return WorstHoldGap; }
    bool IsGrabbing() const { return ZState == EZombie::Grab; }
    /** The chance a landed hit becomes the grab: 0.25 in play; 0 in automated
     *  runs (they keep their worlds), 1 in -ChuckZombieGrabTest. */
    static float GetZombieGrabChance();
    int32 GetZombieSoundsPlayed() const { return ZombieSoundsPlayed; }
    int32 GetLunges() const { return Lunges; }
    // Opt-in ambient movement; the original home remains the pursuit anchor.
    void SetZombieMilling(const TArray<FVector>& FloorPoints,uint32 Seed);
    bool HasZombieMilling() const { return ZMillPoints.Num()>1; }
    float GetZombieMillingTravel() const { return ZMillTravel; }
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
    /** A line asked for in conversation: spoken at once, except by the smith (user 2026-10-06), who first
        rests his hammer on the anvil, pauses a moment, then speaks; he goes back to work once it's done. */
    void RequestVoiceLine(int32 Index);
    bool IsVoicePending() const { return PendingVoice >= 0; }
    /** The smith's last spoken line, for tests: how long he paused with the hammer down before speaking (s),
        the least his hammer was rested while he spoke (0..1), and his blows since he finished. */
    float GetPauseBeforeSpeech() const { return PauseBeforeSpeech; }
    float GetSpeechRestLowest() const { return SpeechRestLowest; }
    int32 GetStrikesSinceSpeech() const { return StrikesAtSpeechEnd >= 0 ? Strikes - StrikesAtSpeechEnd : 0; }
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
    /** Most rag lines crossing the tankard's wall below its rim in any frame after he settles (0: it never goes through the pewter). */
    int32 GetRagThroughTankard() const { return WorstRagThrough; }
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
    bool IsSeated() const { return bSeated; }
    /** Her bench (built by DockPlaza.cpp from these: 185 x 50, top 49 cm) on the bay side of the
        fountain, north-east of it toward the harbour's edge and well clear of the alchemist (user
        2026-10-06), turned to face the fountain; she sits a little off its middle, near its front edge. */
    static inline const FVector ElfBench = FVector(930.f, -2560.f, 49.f);
    static constexpr float ElfYaw = -131.4f;   // toward the fountain at (260,-3320)
    static inline const FVector ElfHips = FVector(ElfBench.X, ElfBench.Y, 0.f) + FRotator(0.f, ElfYaw, 0.f).RotateVector(FVector(9.f, -30.f, 0.f));
    /** Height of her hip joints above the floor (cm, now), and the worst seen after she settles: of either heel off the floor (cm) and of either hand from its place on her lap (cm). */
    float GetSeatHeight() const;
    float GetFootLiftError() const { return WorstFootLift; }
    float GetLapHandError() const { return WorstLapHand; }
    /**
     * The dock worker's night (user 2026-10-06: "move the npc by the tavern into
     * the tavern after sewer when its night ... sitting ... by the table, drink
     * in hand"). Once Chuck is out of the sewer (DockReturn.cpp's evening), he
     * sits on the south bench of the west table by the bar (DockTavern.cpp:
     * table at -215,705, 130 x 85, top 79; that bench's top 47.5 cm), facing the
     * bar, as the old elf sits: legs under the table, left hand on his thigh. His
     * right fist holds a tankard by its handle, standing on the table; every
     * DrinkPeriod s he lifts it to his lips, tips it and his head back and
     * drinks, then sets it down. His eyes go to the bar, or to the rat near him.
     */
    void SitInTavern();
    static inline const FVector TavernHips = FVector(-215.f, 636.f, 0.f);
    static constexpr float TavernBenchTop = 47.5f;
    static constexpr float TavernTableTop = 79.f;
    static constexpr float TavernYaw = 90.f;
    bool IsDrinker() const { return bDrinker; }
    /** Drinks taken; how far the tankard is up (0 on the table .. 1 at his lips). */
    int32 GetDrinks() const { return Drinks; }
    float GetDrinkLift() const { return DrinkLift; }
    /** Worst distance (cm, after he settles) of his right fist from the tankard's handle, and, while it is up, of its rim from his lips. */
    float GetDrinkGripError() const { return WorstDrinkGrip; }
    float GetDrinkLipError() const { return WorstDrinkLip; }
    /** Start the next drink in Seconds (tests and the night capture). */
    void DrinkIn(float Seconds);
    /**
     * The tavern at night (user 2026-10-06), started by the evening switch
     * after SitInTavern: no talking to anyone. Dougmund (the dock worker) rants
     * his ambient line over and over from his seat, drinking in each pause,
     * heard from his mouth: quiet through the walls outside, full inside. Once
     * Chuck has been inside the room TavernInterrupt s, the keeper cuts him
     * off with his own line, once; TavernResume s after the keeper has
     * finished, Dougmund takes up his rant again on its loop (user 2026-10-06).
     * Subtitles for these only while Chuck is in the room (GetAmbientSubtitle).
     */
    static void StartTavernNight();
    /** 0 before the night, 1 Dougmund ranting, 2 the keeper has cut in, 3 Dougmund ranting again. */
    static int32 GetTavernNightStage();
    static constexpr float TavernResume = 45.f;        // s after the keeper's line before Dougmund starts again
    /** From the crate staircase to the tavern (user 2026-10-06): Dougmund heard, the music down, at night. */
    static inline const FBox TavernApproach = FBox(FVector(-560.f, -800.f, -100.f), FVector(420.f, 965.f, 600.f));
    static bool NearTavernAtNight(const FVector& At) { return GetTavernNightStage() > 0 && TavernApproach.IsInsideOrOn(At); }
    /** The distance volume of this NPC's voice at At (0..1, before occlusion), for tests. */
    float GetVoiceVolumeAt(const FVector& At) const;
    static inline const FBox TavernRoom = FBox(FVector(-314.f, 400.f, -40.f), FVector(294.f, 965.f, 450.f));
    static bool InTavern(const FVector& At) { return TavernRoom.IsInsideOrOn(At); }
    static constexpr float TavernInterrupt = 3.5f;     // s inside before the keeper speaks up
    /** Speak this NPC's ambient line Id (NPCVoiceData), after Delay s; with bLoop, again Pause s after each time. */
    bool StartAmbient(const TCHAR* Id, bool bLoop, float Pause = 0.f, float Delay = 0.f);
    /** Speak the scripted line Npc/Id (NPCVoiceData) now, from his mouth with Attenuation (null: his usual), his jaw moving
     *  with it (the ship's storm ending, 2026-11-08: the voices are the script's, the bodies the crew's). Its length in
     *  seconds, or -1 if there is no such line. */
    float SpeakLine(const TCHAR* Npc, const TCHAR* Id, class USoundAttenuation* Attenuation = nullptr);
    /** Stop whatever he is saying, fading over Fade s. */
    void StopSpeaking(float Fade = .2f);
    /** Scales this NPC's voice from now on (the ship's storm: shouted over the gale). */
    void SetVoiceVolume(float Volume);
    /** Stop the ambient line (and its loop) after Delay s, fading out over Fade s. */
    void StopAmbient(float Delay = 0.f, float Fade = .25f);
    bool IsAmbientSpeaking() const { return bAmbientVoice && IsSpeaking(); }
    bool HasAmbientLoop() const { return AmbientLine >= 0 && bAmbientLoop; }
    int32 GetAmbientPlays() const { return AmbientPlays; }
    /** While Chuck (At) is in the tavern: who is speaking an ambient line and the part of it being spoken now. */
    static bool GetAmbientSubtitle(const FVector& At, FString& Speaker, FString& Text);
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
    /**
     * Bobert (user 2026-10-05: "Bobert in his barrel asleep like in the 2d
     * game, positioned by starting point"; GAME-BIBLE.md: Chuck's home is
     * Bobert's barrel, he is asleep inside, never wakes, is never named and is
     * never interacted with). A late-middle-aged halfling, 97 cm, asleep
     * sitting up in an old cask lying on its side (Tools/build_bobert_barrel.py):
     * his back to a rolled blanket against the closed head, knees up, forearms
     * folded over them, head bowed and fallen to one side, boots at the mouth.
     * The pose is aimed bone by bone from the model's A-pose (SolveSleep), so it
     * fits whatever body the build gives him; he breathes slowly and deeply and
     * now and then his head nods a little lower. No lines, no notice of the rat,
     * no turning; the barrel is the solid part (Chuck can climb it, not get in).
     * BarrelAt is the ground under the barrel's middle; Yaw is the way its mouth faces.
     */
    static ADockNPC* SpawnBobert(UWorld* World, const FVector& BarrelAt, float Yaw);
    /** Beside Chuck's spawn (AChuckCharacter::StartLocation, -240,-180), as in the 2D game ('B' beside the
        start), its mouth turned to the start and the first camera; clear of the harbour wall's ledge and the
        smoke test's crate stacks. */
    static inline const FVector BobertBarrelAt = FVector(-150.f, -250.f, 0.f);
    static constexpr float BobertBarrelYaw = 140.f;
    bool IsBobert() const { return Kind == EDockHuman::Bobert; }
    AActor* GetBarrel() const { return Barrel.Get(); }
    /** How far (cm) the worst of his head, hands, knees and feet is outside the barrel's inside (0: all in), for tests. */
    float GetSleepFitError() const;
    /** Head bowed below level (deg) and breaths taken, for tests. */
    float GetHeadBow() const;
    /** Where his head bone is (world), for the review camera. */
    FVector GetHeadLocation() const;
    int32 GetBreaths() const { return BreathCount; }
    /** Worst distance (cm, any frame after he settles) of either hand from where it rests on his knees. */
    float GetRestingHandError() const { return WorstRestHand; }
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
    /** The head's largest one-frame change of turning speed after 8 s (deg/s^2): a twitch. */
    float GetPeakHeadAccel() const { return PeakHeadAccel; }
    // ---- The pirate crew (Claude, 2026-11-02; docs/SHIP.md milestone 3; DockNPCPirate.inl). Placed by AShipCrew.
    bool IsPirate() const { return Kind >= EDockHuman::PirateChef && Kind < EDockHuman::Count; }
    EDockHuman GetKind() const { return Kind; }
    /** The 2D game's first and repeat lines: Lines are the first ones until a conversation through them ends, then the repeat ones. */
    void SetPirateLines(const TArray<FString>& First, const TArray<FString>& Repeat);
    /** The same, voiced (user 2026-11-11, the pre-storm takes): the lines are NPCVoiceData's Npc/Id, their text the subtitles.
     *  First for the first talk, Repeat for the second, After (if any: the "Arrgh!") from the third on, else Repeat again. */
    void SetPirateVoice(const TCHAR* Npc, const TArray<FString>& First, const TArray<FString>& Repeat, const TArray<FString>& After = {});
    /** The chef's shout when he first sees the rat, voiced if his take exists (else on its subtitle for Seconds). */
    void ShoutLine(const TCHAR* Npc, const TCHAR* Id, float Seconds);
    int32 GetConversations() const { return Conversations; }
    bool HasTalkedOnce() const { return Conversations > 0; }
    /** The beat the deck keeps time to (beats since the ship score's first beat), set each frame by the crew. */
    void SetBeat(float InBeat) { Beat = InBeat; }
    /** The seated pirate on his stool at the mess table, his tankard by him on it (heights are the stool's and table's tops, world). */
    void SitAtTable(const FVector& Hips, float Yaw, float StoolTop, float TableTop, const FVector& LookAt);
    /** A fencer's stance and sword this frame (ShipCrew.cpp). */
    void SetFenceCue(const FFenceCue& Cue) { Fence = Cue; bFenceCue = true; }
    /** A fencer's blade now (world), base and tip; false if he has none. */
    bool GetBlade(FVector& Base, FVector& Tip) const;
    /** A fencer's planted feet (world ankles): his left behind, his right ahead; false before he has stood. */
    bool GetFeet(FVector& Back, FVector& Front) const;
    /** Jeffries at the main mast: his back to it (its centre, world) and the ropes round him and it. */
    void TieToMast(const FVector& MastCentre, float MastRadius);
    /** Jeffries: the furthest any of his ropes has passed into his body (cm; < 0 never touched it). Measured in ship tests. */
    float GetRopeDepth() const { return RopeDepth; }
    /** The chef: his place at the work table (feet, world) and the galley he keeps to (ship-frame box, X0 X1 Y0 Y1). */
    void SetupChef(const FVector& WorkFeet, float WorkYaw, const FBox2D& Galley, float FloorZ);
    const TCHAR* GetChefStateName() const;
    int32 GetChefNotices() const { return ChefNotices; }
    int32 GetChefSwings() const { return ChefSwings; }
    int32 GetChefHits() const { return ChefHits; }
    float GetChefSpeed() const { return ChefSpeed; }
    /** The chef's line, and others the crew calls out: shown as a subtitle near him for its time. */
    void Shout(const FString& Text, float Seconds);
    bool IsShouting() const;
    /** A pirate's performance this frame, for tests: the concertina's bellows (cm between the ends), the toast's lift (0..1),
        the dancer's feet above the deck (cm), Jeffries' strain (0..1). */
    float GetBellows() const { return BellowsGap; }
    float GetToast() const { return ToastLift; }
    FVector2D GetDanceFeet() const { return DanceFeet; }
    float GetStrain() const { return Strain; }
    /** The props a pirate holds (tests): count, and the worst fist-to-handle gap since he settled (cm). */
    int32 GetPirateProps() const { return PirateProps.Num(); }
    float GetWorstPropGrip() const { return WorstPropGrip; }
    /** The lowest any of his feet went below the deck (cm), after settling. */
    float GetWorstFootSink() const { return WorstFootSink; }
    /** The captain (2026-11-02): his time in his round (s; tests set it to see each stance), what he is doing ("Hips",
        "Draw", "Display", "Return") and how far his cutlass's tip goes into the deck where it stands planted (cm; 0 above it). */
    void SetCaptainTime(float Seconds, float HoldFor = 0.f) { CaptainT = Seconds; CaptainHold = HoldFor; }
    const TCHAR* GetCaptainPhase() const;
    float GetCaptainTipDepth() const { return CaptainTipDepth; }
    /** The storm (2026-11-04): he leaves what he was doing (props stowed, the fencing stopped) and goes along Way (world, his
     *  feet, the last point his station) to his work there, facing Yaw. Rope: the line he works (world): for Haul the hauling
     *  part's two ends (the lead block, then toward its tail), for Sweat its foot and the block aloft, for Helm the wheel's hub
     *  and its axle's way aft. Phase (0..1) puts him in step with his mates. None: back to his performance (a calm, the menu). */
    void SetStormWork(EStormWork Work, const TArray<FVector>& Way, float Yaw, const FVector& RopeA, const FVector& RopeB, float Phase);
    EStormWork GetStormWork() const { return StormWork; }
    bool IsAtStormStation() const { return StormWork != EStormWork::None && StormArrived; }
    /** How hard the storm is (0 calm .. 1 the full gale, up to 2 once it has intensified): his effort and the helm's swing. */
    void SetStormForce(float Force) { StormForce = Force; }
    /** The helm: the wheel's turn now (degrees, about its axle), steered by the captain. */
    float GetHelmAngle() const { return HelmAngle; }
    /** His hands' fists now (world, from the last pose): the tests check they are on his line or the wheel. */
    FVector GetStormFist(int32 Side) const { return StormFistW[FMath::Clamp(Side, 0, 1)]; }
    FVector GetStormGripWant(int32 Side) const { return StormWantW[FMath::Clamp(Side, 0, 1)]; }
    /** How far he has settled into his work (0 walking there .. 1 at it), and how firmly each hand holds his line now (0 let
     *  go, reaching on along it .. 1 gripping): the storm's lines run through the hands that hold them (AShipCrew). */
    float GetStormBlend() const { return StormBlend; }
    float GetStormHold(int32 Side) const { return StormHoldW[FMath::Clamp(Side, 0, 1)]; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UCapsuleComponent* Blocker;
    UPROPERTY() UPoseableMeshComponent* Body;
    UPROPERTY() TArray<class UGroomComponent*> Grooms;   // strand hair on its bones (NPCGroom.h)
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
    UPROPERTY() TObjectPtr<UAnimSequence> Clips[9];
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
    float FistScale = 1.f;
    float ArmOut = 1.f;                           // which way is out for the left arm (+/-Y)
    void PoseHands(TArray<FTransform>& Space, bool bStraightenWrists) const;
    // Voice and face.
    UPROPERTY() TArray<TObjectPtr<USoundBase>> VoiceSounds;
    UPROPERTY() UAudioComponent* VoiceAudio = nullptr;
    TArray<int32> VoiceLines;                     // indices into NPCVoiceData::Lines, in talk order
    int32 VoiceLine = -1, HeardLine = -1, Blinks = 0;
    int32 PendingVoice = -1, StrikesAtSpeechEnd = -1;
    float RestedFor = 0.f, PauseBeforeSpeech = -1.f, SpeechRestLowest = 1.f;
    int32 TalkVoiceCount = 0;                     // VoiceLines' first entries are the talk lines; ambient ones follow
    // Ambient speech (the tavern at night).
    int32 AmbientLine = -1, AmbientPlays = 0, NightStage = 0;
    bool bAmbientLoop = false, bAmbientVoice = false;
    float AmbientPause = 0.f, AmbientWait = 0.f, AmbientStopIn = -1.f, AmbientFade = .25f, InsideTime = 0.f, ResumeIn = -1.f;
    UPROPERTY() TObjectPtr<USoundAttenuation> RoomAttenuation;
    void EnsureVoiceAudio();
    void TickTavernNight(float DeltaSeconds);
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
    enum class EZombie : uint8 { Idle, Shamble, Windup, Lunge, Recover, Hurt, Dead, Grab };
    EZombie ZState = EZombie::Idle;
    float ZTime = 0, WalkTime = 0, WalkBlend = 0, Rear = 0, Reach = 0, Flinch = 0, FlinchTime = -1, DeathTime = -1;
    FVector ZHome = FVector::ZeroVector, LungeDir = FVector::ForwardVector;
    TArray<FVector> ZMillPoints;
    FRandomStream ZMillRandom;
    int32 ZMillIndex=0;
    float ZMillWaitUntil=0,ZMillBlocked=0,ZMillTravel=0;
    bool bBit = false, bDropped = false;
    int32 Hits = 0, Bites = 0, Lunges = 0;
    void SetZombie(EZombie State) { ZState = State; ZTime = 0; }
    void TickZombie(float DeltaSeconds);
    void PlayZombieSound(const TCHAR* Stem, int32 Count, const FVector& At, float Volume);
    UPROPERTY() TMap<FString, TObjectPtr<USoundBase>> ZombieSounds;
    UPROPERTY() TObjectPtr<USoundAttenuation> ZombieAttenuation;
    float NextZombieRasp = 0;
    int32 ZombieSoundsPlayed = 0;
    bool StepZombie(const FVector& Direction, float Distance);
    void TurnZombie(const FVector& Toward, float DeltaSeconds, float Rate);
    // The grab (user 2026-10-14): a quarter of its hits seize him instead,
    // lift him to its mouth and bite (DockNPC.cpp, TickZombie and PoseZombieGrab).
    TWeakObjectPtr<AChuckCharacter> Seized;
    FVector GrabFrom = FVector::ZeroVector;       // his chest where it seized him (world)
    FVector HoldPoint = FVector::ZeroVector;      // his chest in its hands now (component space)
    FVector LastHold = FVector::ZeroVector;       // ... last frame
    float GrabStoop = 0, GrabArms = 0, GrabHead = 0, GrabShake = 0, ZJaw = 0, ReleaseTime = -1;
    bool bGrabBit = false;
    int32 Grabs = 0;
    float WorstHoldGap = 0;
    void TickGrab(float DeltaSeconds, AChuckCharacter* Chuck);
    void PoseZombieGrab(TArray<FTransform>& Space) const;
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
    // Bobert.
    UPROPERTY() TObjectPtr<UStaticMesh> BarrelMesh;
    TWeakObjectPtr<AActor> Barrel;
    TArray<FQuat> Sleep;                          // the sleeping pose (SolveSleep), as Rest is the standing one
    FVector HeadUpLocal = FVector::UpVector;      // the head's up in its own frame (neck -> head at rest)
    float SleepTilt = 1.f;                        // which way his head has fallen (+-Y)
    float SleepClock = 0, WorstRestHand = 0;
    int32 BreathCount = 0, LastBreath = -1;
    void SolveSleep();
    void PoseBobert(TArray<FTransform>& Space);
    // The tavern keeper.
    UPROPERTY() UStaticMeshComponent* Tankard = nullptr;
    UPROPERTY() UStaticMeshComponent* Rag = nullptr;
    UPROPERTY() TObjectPtr<UStaticMesh> KeeperMeshes[2];          // tankard, rag
    float PolishClock = 0, WorstTankardGrip = 0, WorstRagReach = 0;
    int32 PolishPasses = 0, PolishPhase = -1, WorstRagThrough = 0;
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
    // The old elf on her bench (and the dock worker on his, at night).
    bool bSeated = false;
    float SeatTop = 0.f, SeatBone = 9.f;  // the bench's top (cm) and how far the hip joints sit above it
    void SetupSeat(const FVector& LookAt);
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
    float PeakAccel = 0.f, PeakClipAt = -1.f;
    float PeakHeadAccel = 0.f, ProbeHeadSpeed = 0.f;
    FQuat ProbeHeadPrev = FQuat::Identity;
    int32 PeakBone = -1;
    void ProbeShake(const TArray<FTransform>& Space, float DeltaSeconds);
    // The dock worker's drink.
    bool bDrinker = false;
    float DrinkClock = 0.f, DrinkLift = 0.f, WorstDrinkGrip = 0.f, WorstDrinkLip = 0.f;
    int32 Drinks = 0;
    void PoseDrink(TArray<FTransform>& Space);
    // The gnome alchemist's sleeves.
    float WorstSleeveReach = 0.f;
    void PoseSleeves(TArray<FTransform>& Space);
    /** Two-bone IK: aim Upper and Lower so End lands at Target, the middle joint toward Pole (Space updated down the chain). */
    void TwoBone(TArray<FTransform>& Space, int32 Upper, int32 Lower, int32 End, const FVector& Target, const FVector& Pole) const;
    /** Two-bone IK: the fist of arm Side to Fist, the hand's knuckles along Along with its thumb side along Thumb. */
    void PlaceHand(TArray<FTransform>& Space, int32 Side, const FVector& Fist, const FVector& Along, const FVector& Thumb, const FVector& Pole);
    /** A posed hand's fist centre and axes (component space). */
    void HandFrame(const TArray<FTransform>& Space, int32 Side, FVector& Fist, FVector& Along, FVector& Thumb) const;
    // ---- The pirate crew (DockNPCPirate.inl).
    TArray<FString> RepeatLines;
    FString PirateVoiceNpc;                       // SetPirateVoice: whose takes, and the ids for each talk
    TArray<FString> PirateVoiceIds[3];
    /** Make Ids (NPCVoiceData PirateVoiceNpc/Id) the talk lines, voiced; a scripted line he is saying keeps going. */
    void UsePirateVoice(const TArray<FString>& Ids);
    int32 Conversations = 0;
    bool bInConversation = false;
    float Beat = 0.f;
    FFenceCue Fence;
    bool bFenceCue = false;
    UPROPERTY() TArray<UStaticMeshComponent*> PirateProps;
    UPROPERTY() class UProceduralMeshComponent* Ropes = nullptr;
    FVector MastLocal = FVector::ZeroVector;     // Jeffries: the mast's centre in his component space
    float MastR = 30.f;
    float RopeDepth = -100.f;                     // Jeffries: the furthest a rope has sunk into him (cm, measured in tests)
    bool bMeasureRope = false;
    int32 RopeFrame = 0;
    FVector PirateMouth = FVector::ZeroVector;   // component space, from the manifest (a drinker's lips)
    float DrinkTableTop = 0.f;                   // the table his tankard rests on (component space; the tavern's by default)
    float BellowsGap = 0.f, ToastLift = 0.f, Strain = 0.f, StrainKick = 0.f, NextStrain = 2.f, WorstPropGrip = 0.f, WorstFootSink = 0.f;
    int32 ToastCycle = -1;
    FVector2D DanceFeet = FVector2D::ZeroVector;
    FVector AnkleStand[2];                       // each ankle standing (component space, from the rest pose)
    // Planted feet (the fencers): world ankles, the step under way.
    FVector Plant[2], StepFrom[2], StepTo[2];
    float StepT[2] = { -1.f, -1.f };
    bool bPlanted = false;
    int32 DanceBeatHeard = MIN_int32;           // the dancer's last beat heard landing on the deck
    // The chef.
    enum class EChef : uint8 { Work, Notice, Chase, Swing, Lost, Return };
    EChef Chef = EChef::Work;
    float ChefTime = 0.f, ChefSpeed = 0.f, JogTime = 0.f, JogBlend = 0.f, ChopClock = 0.f, SwingDrive = 0.f, ShoutUntil = -1.f;
    FVector ChefHome = FVector::ZeroVector, ChefAim = FVector::ZeroVector;
    float ChefHomeYaw = 0.f, ChefFloor = 0.f, UnreachableFor = 0.f;
    FBox2D ChefBox = FBox2D(ForceInit);
    int32 ChefNotices = 0, ChefSwings = 0, ChefHits = 0;
    bool bChefStruck = false, bChefSaidLine = false, bChefHitPause = false;
    FString ShoutText;
    FVector BladeBaseW = FVector::ZeroVector, BladeTipW = FVector::ZeroVector;
    float StrainT = 99.f, PirateDip = 0.f, ChefTableTop = 86.f;
    int32 ChefRespawnsSeen = -1;
    // Where the rat last appeared (a spawn, respawn or checkpoint): until he moves from there the chef doesn't see him.
    FVector ChefRatArrival = FVector::ZeroVector, ChefRatLast = FVector(1e9f);
    bool bChefRatStill = false;
    // The captain: his round, the cutlass planted in the deck at his right (component space: the fist's place on its grip
    // and the blade's direction) and its scale (a halfling's cutlass, its tip a few cm into the planking).
    float CaptainT = 0.f, CutlassScale = 1.f, CaptainTipDepth = 0.f, CaptainHold = 0.f;   // (Hold: s a test keeps him at CaptainT)
    FVector CaptainHilt = FVector::ZeroVector, CaptainBlade = FVector(0.f, 0.f, -1.f);
    int32 CaptainPhase = 0;
    bool PirateTurns() const { return !IsPirate() || (StormWork == EStormWork::None && (Kind == EDockHuman::PirateConcertina || Kind == EDockHuman::PirateCheering || Kind == EDockHuman::PirateDancer)); }
    // The storm's work (2026-11-04).
    EStormWork StormWork = EStormWork::None;
    TArray<FVector> StormWay;
    int32 StormLeg = 0;
    bool StormArrived = false;
    float StormYaw = 0.f, StormPhase = 0.f, StormClock = 0.f, StormBlend = 0.f, StormForce = 1.f, HelmAngle = 0.f, StormSpeed = 0.f;
    FVector StormRopeA = FVector::ZeroVector, StormRopeB = FVector::ZeroVector;
    FTransform StormHome = FTransform::Identity;   // where he was before the storm (back there in a calm)
    float StormHomeYaw = 0.f;
    FVector StormFistW[2] = { FVector::ZeroVector, FVector::ZeroVector }, StormWantW[2] = { FVector::ZeroVector, FVector::ZeroVector };
    float StormHoldW[2] = { 0.f, 0.f };
    void TickStormWork(float DeltaSeconds);
    void StormDelta(TArray<FQuat>& Delta, TArray<FQuat>& BoneDelta, FVector& HipsOffset);
    void PoseStorm(TArray<FTransform>& Space);
    bool ChefCanReach(const class AChuckCharacter* Chuck) const;
    void SetupPirate();
    void TickPirate(float DeltaSeconds);
    void TickChef(float DeltaSeconds);
    bool ChefStep(const FVector& Direction, float Distance);
    void PirateDelta(TArray<FQuat>& Delta, TArray<FQuat>& BoneDelta, FVector& HipsOffset);
    void PosePirate(TArray<FTransform>& Space, float DeltaSeconds);
    void KeepFeetOn(TArray<FTransform>& Space, const FVector Ankles[2], const FQuat Feet[2], const FVector& KneeAhead);
    UStaticMeshComponent* AddPirateProp(const TCHAR* Path, const TCHAR* Name);
    void PlaceProp(int32 Index, const FTransform& At);
    void HoldTankard(TArray<FTransform>& Space, int32 Side, const FVector& Handle, const FVector& Along, const FVector& Up, const FVector& Pole);
    void UpdateRopes(const TArray<FTransform>& Space);
    void BakeRopeBody() const;
};
