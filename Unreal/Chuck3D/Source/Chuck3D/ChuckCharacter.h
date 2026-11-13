#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChuckCharacter.generated.h"

struct FChuckClimbable;

class USpringArmComponent;
class UCameraComponent;
class UGroomComponent;
class UAnimSequence;
class UChuckAnimInstance;
class USkeletalMesh;
class UStaticMeshComponent;
class USoundBase;
class UCurveFloat;
class UInstancedStaticMeshComponent;
class ADockNPC;

UCLASS()
class CHUCK3D_API AChuckCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AChuckCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    /** Snap the orbit to the other framing preset (tests and captures; players orbit freely). */
    void ToggleCamera();
    void ResetToDock();
    bool HasDemoEnded() const { return bDemoEnded; }
    /** Falling to Chult (FallCutscene.h): no control, Esc skips to the end. */
    bool IsInFallCutscene() const { return bCutscene; }
    /** The cutscene is over: the demo's end card. */
    void EndFallCutscene();
    /** The game so far ends here: the end card (the ship's ending, 2026-11-08, until the fall into Phlegethos exists). */
    void EndDemo();
    /** Take hold of swing grip Index now (GetChuckSwingGrips; the ship's ending cutscene puts him on the fore topsail yard). */
    void CatchSwingGrip(int32 Index);
    /** The ship's storm ending (2026-11-09): the cutscene holds him (no gait, physics or collision) and places him each
     *  frame (SetScenePose); he plays its clip: 0 losing his grip on the yard (SwingSlip), 1 falling (FallFlail). */
    void BeginSceneHold();
    void SetSceneClip(int32 Which, float Time, float FadeSeconds);
    float GetSceneClipLength(int32 Which) const;
    void SetScenePose(const FVector& Centre, const FQuat& Rotation);
    bool IsSceneHeld() const { return bSceneHold; }
    /** Begin at an explicit checkpoint with fresh traversal, fall and camera state. */
    void ResetAtLocation(const FVector& Location);
    /** Death/fall recovery uses the current area's entrance. R remains a dock reset. */
    void RespawnAtAreaStart();
    FVector GetAreaStartLocation() const;
    /** Low passages he goes through on all fours (2026-10-26: the temple rubble's crevice). A world box his capsule's
     *  centre enters to crawl (Box: the box's centre and turn in the world; Half its half-size along its own axes); out of
     *  every box he stands again once there's room. Registered by the setting's builder. */
    static void RegisterCrawlSpace(const UWorld* World, const FTransform& Box, const FVector& Half);
    static bool InCrawlSpace(const UWorld* World, const FVector& Location);
    /** Put him at Location (his standing capsule's centre) already down on all fours, in a crawl space too low to stand
     *  in (2026-11-01: the ship escape begins inside the crawlway through the masonry, ShipEscape.h). */
    void ResetCrawlingAt(const FVector& Location);
    bool IsCrawling() const;
    int32 GetCrawls() const { return Crawls; }
    /** Roll, or side jump when the stick (x right, y forward, camera-relative) is mostly sideways. */
    void DodgeToward(FVector2D Stick);
    /** Claw slash (LMB / X) pressed: a stepping slash when standing, the upper
     *  body over the stride on the move. Pressing again, or holding, chains
     *  another strike on a steady beat; which paw strikes is random (never three
     *  of one paw in a row), so a flurry isn't a metronomic left-right. */
    void Slash();
    /** Slash button released: the flurry ends after the current paw. */
    void SlashReleased() { bSlashHeld = false; }
    /** Jump pressed: a jump on the ground; on a wall (or just after leaving
     *  one) a wall jump; in the air, buffered for a wall reached just after. */
    void JumpPressed();
    bool IsWallRunning() const { return Gait == EGait::WallRun; }
    /** Running along a wall beside him (the side wall run), and how many he has done. */
    bool IsWallSideRunning() const { return Gait == EGait::WallSide; }
    /** On a ladder (or anything else climbable, ChuckClimbable.h), and how often he has taken hold of one. */
    bool IsOnLadder() const { return Gait == EGait::Ladder; }
    /** Speed vaults over low obstacles (a running jump at a bench or low wall). */
    bool IsVaulting() const { return Gait == EGait::Vault; }
    int32 GetVaults() const { return Vaults; }
    /** Why the last running jump didn't vault (0: it did; tests). */
    int32 GetVaultRefusal() const { return VaultRefusal; }
    int32 GetLadderMounts() const { return LadderMounts; }
    /** Falls off the map (each an Astral death: he's summoned back at the area's start). */
    int32 GetFallDeaths() const { return FallDeaths; }
    int32 GetWallSideRuns() const { return WallSideRuns; }
    /** Side wall runs caught in the air from a running jump (not started from the ground). */
    int32 GetWallSideAirCatches() const { return WallSideAirCatches; }
    /** The last side wall run: distance along the wall (cm) and height gained at its top (cm). */
    float GetWallSideTravel() const { return WallSideTravel; }
    float GetWallSideRise() const { return WallSideRise; }
    int32 GetWallRuns() const { return WallRuns; }
    int32 GetWallJumps() const { return WallJumps; }
    bool IsHanging() const { return Gait == EGait::Hang; }
    int32 GetHangs() const { return Hangs; }
    int32 GetPullUps() const { return PullUps; }
    /** Leaps that arrived a little high and scrambled straight onto the top. */
    int32 GetLedgeScrambles() const { return LedgeScrambles; }
    int32 GetSlides() const { return Slides; }
    int32 GetMantles() const { return Mantles; }
    /** Pull-ups onto a top too narrow to stand back from the edge (a rail, a wall's coping): he stands balanced on it. */
    int32 GetPerchClimbs() const { return PerchClimbs; }
    /** Hops from a hang up to a higher edge within reach (a double rail's middle bar to its cap). */
    int32 GetHangHops() const { return HangHops; }
    // Ledges: grabbed automatically when his paws reach a top edge while he is
    // moving up or into the wall; holding toward the wall this long pulls up.
    static constexpr float PullUpHold = .2f;
    // Hanging: stick sideways shimmies along the edge.
    static constexpr float ShimmySpeed = 45.f;   // cm/s
    // Falls from higher than this (cm, apex to landing) end in a roll.
    // User 2026-09-29: doubled from 80.
    static constexpr float RollFallHeight = 160.f;
    // Walking gently off an edge with at least this drop below grabs it and hangs (GTA-style).
    static constexpr float DropHangMinDrop = 60.f;
    int32 GetDropHangs() const { return DropHangs; }
    // Brachiation (user 2026-10-09, "look hard, be easy"): in the air near a
    // swing grip (ChuckClimbable.h: the high lanterns' rings) his paws close on
    // it and he hangs and swings from it. Jump flies him to the next ring the
    // way the stick points (straight on with the stick let go) and catches it
    // for him; with none that way it kicks him off along the stick (a wall in
    // reach is run up as after a wall jump). Dodge lets go. The stick along
    // the swing pumps it.
    static constexpr float SwingCatchReach = 45.f;   // cm sideways from his raised paws a ring is still caught
    static constexpr float SwingLeapRange = 240.f;   // cm to the next ring at most
    // Big leaps need a real swing (user 2026-10-31/11-01: swing back, then
    // forward, then jump, "like a real person trying to make a big
    // brachiation jump"). The stick pumps only in time with the swing (back
    // while it swings back, forward while it swings forward); held one way it
    // gains on one stroke what it loses on the other. A leap longer than
    // SwingEasyLeap goes as far as his release carries him: the swing's own
    // velocity at the moment he lets go (times SwingReachK, the legs' whip),
    // plus a throw from the arms, flown ballistically to the next grip's
    // height (SwingReachNow). A jump pressed early waits for the forward
    // stroke and he lets go at its best point; short of the grip he falls.
    // Every leap (11-01 later, "really feel the momentum") is a real flight:
    // his centre leaves at the swing's own velocity, the arms' throw adds to
    // it over SwingLeapRamp, then only his gravity acts until the catch.
    static constexpr float SwingEasyLeap = 165.f;
    static constexpr float SwingPumpGain = 8.f;   // rad/s^2 at the bottom of the swing
    static constexpr float SwingDamping = .45f;   // 1/s (was .9: the swing keeps its momentum)
    static constexpr float SwingAngleLimit = 1.15f;   // rad either way, about 66 degrees (was 1.0)
    static constexpr float SwingDeckLimit = .77f;   // rad, 44 degrees, under a deck: higher, his snout would meet the board
    static constexpr float SwingDeckLimitBack = .66f;   // rad, 38 degrees, swung back under a deck: higher, his head would rise between the boards
    static constexpr float SwingCatchCarry = 20.f;   // degrees a leap's catch swings him on to
    static constexpr float SwingCatchDeg = -8.f;   // where on the swing (behind the grip) a leap's catch takes him, moving forward
    static constexpr float SwingThrowAlong = 120.f;   // cm/s, the arms' throw along the leap
    static constexpr float SwingThrowUp = 100.f;   // cm/s, and up
    static constexpr float SwingReachK = 2.8f;   // the legs' whip: launch cm/s per cm/s of the swing
    static constexpr float SwingLeapRamp = .14f;   // s over which the throw builds after he lets go
    // (11-02, "make forward swing you more forward easier"): facing his way,
    // the forward beat drives harder than the back one, by up to
    // SwingForwardBeat more after a back swing of SwingBeatBackFull degrees
    // (none under SwingBeatBackFrom). One full swing back, then forward, makes
    // a big gap; held forward there is no back swing to drive from.
    // (11-12, "I barely go forward and I jump and make it"): the forward beat after a good back swing is much stronger
    // and drives him over a wider arc (SwingForwardWindow either side of the bottom, not 30), and on a big leap he lets go
    // near the top of the forward swing (its rate down to SwingReleaseRate of the stroke's peak, or past SwingReleaseDeg)
    // instead of just past the bottom. Whether he lands is still the stroke's best reach, so back-then-forward lands.
    static constexpr float SwingForwardBeat = 2.6f;
    static constexpr float SwingForwardWindow = 42.f;
    static constexpr float SwingReleaseRate = .45f;
    static constexpr float SwingReleaseDeg = 44.f;
    static constexpr float SwingReleaseDegDeck = 24.f;
    static constexpr float SwingBeatBackFrom = 10.f;
    static constexpr float SwingBeatBackFull = 20.f;
    static constexpr float SwingTopGrace = .15f;   // s after the forward swing tops out that a press still lets go with its best
    /** How far (cm, horizontally) a leap toward a grip Delta away (grip to grip) would carry him if he let go now. */
    float SwingReachNow(const FVector& Delta) const;
    /** The reach at a given swing rate and angle (SwingReachNow's model). */
    float SwingReachAt(const FVector& Delta, float Rate, float Angle) const;
    /** The best reach this swing's energy gives on its stroke toward Delta (released at its best point). */
    float SwingStrokeBest(const FVector& Delta) const;
    /** The swing's amplitude now (degrees, from its energy). */
    float GetSwingAmplitude() const;
    /** The swing's angular rate (rad/s; positive: swinging forward along his facing). */
    float GetSwingRate() const { return Gait == EGait::Swing ? SwingRate : 0.f; }
    int32 GetSwingShorts() const { return SwingShorts; }
    /** Route tests: the swing pumps itself in time (as a player would) and a big leap waits until the swing carries him there. */
    void SetTestSwingPump(bool bOn) { bTestSwingPump = bOn; }
    bool IsSwinging() const { return Gait == EGait::Swing; }
    bool IsSwingLeaping() const { return Gait == EGait::SwingLeap; }
    /** The grip he hangs from (GetChuckSwingGrips index), or -1. */
    int32 GetSwingGrip() const { return Gait == EGait::Swing ? SwingGrip : -1; }
    int32 GetSwingCatches() const { return SwingCatches; }
    int32 GetSwingLeaps() const { return SwingLeapCount; }
    int32 GetSwingDismounts() const { return SwingDismounts; }
    /** Edges caught from below an overhang (an eave jutting over the wall he came up). */
    int32 GetEaveGrabs() const { return EaveGrabs; }
    /** Strafe (hold Q/E or LT): facing held down the camera, sidestepping; jump = side jump. */
    bool IsStrafing() const { return Gait == EGait::Strafe; }
    int32 GetStrafeJumps() const { return StrafeJumps; }
    /** Tests: hold strafe mode without a key. */
    void SetStrafeHeld(bool bHeld) { bTestStrafe = bHeld; }
    /** Movement sound effects fired so far, by kind (tests): steps, jump, land, slash, roll. */
    enum class ESfx : uint8 { Step, Jump, Land, Slash, Roll, Climb, Num };
    int32 GetSfxCount(ESfx Kind) const { return SfxCounts[static_cast<int32>(Kind)]; }
    int32 GetSfxLoaded() const;
    int32 GetStreamStepCount() const { return StreamStepCount; }
    int32 GetSplashLoaded() const { return StreamSplashSounds.Num(); }
    /** Last side jump was the long (running) one. */
    bool WasLongSideJump() const { return bSideLong; }
    int32 GetOuterCorners() const { return OuterCorners; }
    int32 GetInnerCorners() const { return InnerCorners; }
    int32 GetLandingRolls() const { return LandingRolls; }
    // Knee-high ledges he walks into are mantled automatically (above his feet, cm).
    static constexpr float MantleMin = 6.f;
    static constexpr float MantleMax = 40.f;
    // Parkour tuning (user vision: "feels like you're doing something difficult
    // but it's not actually that hard"): three steps up a wall, a strong kick.
    static constexpr float WallRunRise = 45.f;     // cm climbed over the three steps
    static constexpr float WallRunTime = .45f;     // s (three 0.15 s steps)
    static constexpr float WallReach = 12.f;       // cm beyond the capsule a wall still catches him
    static constexpr float WallJumpOut = 260.f;    // cm/s away from the wall (side jump: 190)
    static constexpr float WallJumpUp = 230.f;     // cm/s up (standing jump: 170)
    static constexpr float WallCoyote = .15f;      // s after leaving a wall a jump still kicks off it
    // The side wall run (user 2026-10-03): a running jump with a wall right
    // beside him runs a low arc along it (a wall ahead still means climbing it).
    // User 2026-10-04: easier. A wall approached at up to WallSideAngle also
    // counts (steeper is still the head-on climb), and a running jump that
    // reaches such a wall in the air catches it there.
    static constexpr float WallSideReach = 35.f;   // cm beyond the capsule the wall may be (straight out from it)
    static constexpr float WallSideAngle = 50.f;   // deg between his run and the wall, at most
    static constexpr float WallSideAirTime = .45f; // s after a running takeoff the jump can still catch a wall
    static constexpr float WallSideTime = 1.1f;    // s at most on the wall
    static constexpr float WallSideUp = 260.f;     // cm/s up at the start of the arc
    static constexpr float WallSideGravity = 470.f;   // cm/s2: a lighter fall than his own while he runs it
    static constexpr float WallSideLean = 24.f;    // deg his body leans out from the wall
    // The chained wall run (user 2026-10-22, the temple trap run): a jump in a
    // side wall run kicks off across the corridor keeping his speed along it;
    // an opposite wall reached within WallChainAirTime is caught and run too
    // (left wall, kick, right wall). With no wall there it is just a leap off.
    static constexpr float WallChainOut = 230.f;      // cm/s away from the wall
    static constexpr float WallChainUp = 190.f;       // cm/s up
    static constexpr float WallChainAirTime = .7f;    // s after the kick the next wall can still be caught
    int32 GetWallChains() const { return WallChains; }
    /** In a dodge roll (low: the temple's saws pass over him). */
    bool IsRolling() const { return Gait == EGait::Roll; }
    static constexpr float LadderSpeed = 75.f;     // cm/s up or down a ladder
    // The speed vault (user 2026-10-04): a running jump at something low and
    // thin with floor beyond (a bench, a low wall) goes over it in stride.
    static constexpr float VaultMinHeight = 18.f;   // cm above his feet
    static constexpr float VaultMaxHeight = 60.f;
    static constexpr float VaultMaxDepth = 90.f;   // cm front to back: thicker is a step up, not a vault
    static constexpr float VaultReach = 85.f;      // cm in front of him the face may be when he jumps
    static constexpr float LadderStride = 30.f;    // cm climbed per cycle of the climb clip
    static constexpr float LadderSideSpeed = 55.f;  // cm/s across a net (the ratlines), or across a wall as he climbs it
    static constexpr float LockedSideRise = .35f;   // golden leaf, stick held across the wall: the share of the climb still going up
    static constexpr float WallBuffer = .15f;      // s a jump pressed before reaching a wall still counts
    // Flow between moves (user 2026-10-12: "too much pause between movements"):
    // a dodge pressed while he can't yet (in the air, pulling up, finishing a
    // dodge) goes off as soon as he can, within DodgeBuffer; a side jump, a
    // pull-up or mantle with the stick held runs straight on at pace; pull-ups
    // and mantles play ClimbRate faster.
    static constexpr float DodgeBuffer = .35f;     // s
    static constexpr float SideRunOn = .05f;       // s after a side jump's touchdown the stick runs on (was .15)
    static constexpr float ClimbRate = 1.15f;
    static constexpr float LandRunOn = .08f;       // s into a slow landing before the stick strides on (was .15)
    int32 GetBufferedDodges() const { return BufferedDodges; }
    /** Bitten by a rat at From: knocked back a step, then briefly safe from
     *  bites. Rolling or side-jumping dodges it; no bite reaches him on a wall.
     *  Returns whether it landed. (No health yet: the user's call.) */
    // bThroughDodge (the temple's spikes, 2026-10-20): a dodge roll gives no immunity.
    bool TakeBite(const FVector& From, int32 Amount = 1, bool bZombieHit = false, bool bThroughDodge = false);
    // Sanity (the 2D game, GAME-BIBLE.md): damage lowers it, cigarettes
    // restore it; at zero Chuck - a fey summon who can't die - quietly
    // vanishes into astral light and is summoned back at the spawn point.
    static constexpr int32 MaxSanity = 5;
    int32 GetSanity() const { return Sanity; }
    /** Tests. */
    void SetSanity(int32 Value) { Sanity = FMath::Clamp(Value, 0, MaxSanity); }
    /** Vanishing, away or being summoned back (or in a zombie's hands: out of play). */
    bool IsAstral() const { return Gait == EGait::Astral || bPendingVanish || Gait == EGait::Held; }
    // Seized by a zombie (user 2026-10-14: a quarter of its hits pick him up and
    // bite, taking all his sanity). The zombie carries him: each tick it sets
    // where his chest is and which way he faces. He struggles, the bite takes
    // everything, he goes limp and vanishes from its hands into the astral light.
    /** Seized by By (the same dodges and walls as a bite save him). Returns whether it held. */
    bool BeginHeld(AActor* By);
    /** The zombie's hands: his chest at Chest (world), facing Yaw. */
    void SetHeldPose(const FVector& Chest, float Yaw);
    /** The bite: all his sanity; limp, then the vanish from its hands. */
    void HeldBitten();
    /** Thrown out of its hands (a temple skeleton, 2026-11-12): flying at Velocity, briefly safe from another blow. */
    void ThrowFromHeld(const FVector& Velocity);
    // Picked up by the leg and hucked (user 2026-11-12, a temple skeleton): hung head-down by his left ankle from the
    // holder's hand, a pendulum it swings; thrown, he tumbles end over end with the swing's spin, crash-lands face down
    // and gets up, or slams back first into a wall, slides down it, slumps and gets up (a sanity).
    bool BeginLegHeld(AActor* By);
    /** Each tick while held by the leg: the hand (world) and the way he faces. */
    void SetLegHold(const FVector& Hand, float FaceYaw, float DeltaSeconds);
    /** Let go at the top of the throw: at the swing's own speed, at least MinSpeed along Dir and Up upward. */
    void ThrowFromLegHold(const FVector& Dir, float MinSpeed, float Up);
    bool IsLegHeld() const { return Gait == EGait::Held && bLegHeld; }
    bool IsThrown() const { return Gait == EGait::Thrown; }
    int32 GetCrashLands() const { return CrashLands; }
    int32 GetWallSlams() const { return WallSlams; }
    int32 GetThrows() const { return Throws; }
    int32 GetPierPushOuts() const { return PierPushOuts; }
    int32 GetTailZigzagFrames() const { return TailZigzagFrames; }
    bool IsHeld() const { return Gait == EGait::Held; }
    /** Held, or vanishing out of a zombie's hands (still drawn in its grip). */
    bool IsInGrip() const { return Gait == EGait::Held || bHeldVanish; }
    bool IsBodyHidden() const { return bAstralHidden; }
    int32 GetTimesHeld() const { return TimesHeld; }
    const TCHAR* GetAstralName() const;
    int32 GetRespawns() const { return Respawns; }
    /** Every cigarette picked up (refilling Sanity or counted). */
    int32 GetPickupsCollected() const { return PickupsCollected; }
    int32 GetBitesTaken() const { return BitesTaken; }
    static constexpr float BiteKnockback = 230.f;   // cm/s away
    static constexpr float BiteImmunity = 1.f;       // s
    /** Cigarettes collected (the currency; HUD counter). */
    int32 GetCigarettes() const { return CigaretteCount; }
    /** Restored from the save (ChuckSave.h). */
    void SetCigarettes(int32 Value) { CigaretteCount = FMath::Max(0, Value); }
    /** Picked up: each refills a point of Sanity first; once it's full they count up like coins. */
    void AddCigarettes(int32 Count);
    /** Breakables (grass tufts, jars, later small rats) broken by the slash so far. */
    int32 GetSlashBreaks() const { return SlashBreaks; }
    int32 GetSlashRatHits() const { return SlashRatHits; }
    int32 GetSlashNPCHits() const { return SlashNPCHits; }
    // The slash's reach for breakables: from his centre to a target's edge (cm),
    // ground to chest. Generous on purpose ("hard-looking but easy").
    static constexpr float SlashReach = 40.f;
    /** Strikes started so far (tests). */
    int32 GetSlashStrikes() const { return SlashStrikes; }
    /** Tests: a fixed paw sequence. */
    void SetSlashSeed(int32 Seed) { SlashRandom.Initialize(Seed); }
    /** "SlashRight"/"SlashLeft" while one plays (standing or layered), else "". */
    const TCHAR* GetSlashName() const;
    /** Run latch: tap Shift / LB to run, tap again or come to a stop to saunter. Tests set it directly. */
    void SetRunHeld(bool bHeld) { bRunHeld = bHeld; }
    /** Current saunter -> run blend (0..1). */
    float GetRunWeight() const { return RunWeight; }
    // The sprint (user 2026-10-07): Left Ctrl / left stick click while moving
    // drops him onto all fours for a brief burst at SprintSpeed. Stamina (the
    // purple ring in the HUD, user 2026-10-08): he can only start a sprint with
    // it full; sprinting drains it in SprintDuration, and anything else refills
    // it in SprintCooldown from empty (running and jumping too). A sprint that
    // ends early (a dodge, strafing, letting go, a fall) keeps what's left, so
    // half a sprint used refills in half the time.
    static constexpr float SprintDuration = 2.5f;   // s on all fours, from full stamina
    static constexpr float SprintCooldown = 10.f;   // s to refill from empty
    static constexpr float SprintAcceleration = 1100.f;   // cm/s2 into the burst (walk/run: 550)
    static constexpr float SprintDropGrace = .2f;   // s off the ground (a step down) before a fall ends it
    /** Start the sprint if he can (tests and the input). */
    bool TrySprint();
    bool IsSprinting() const { return bSprinting; }
    /** Sprint stamina 0..1 (1: a sprint can start). */
    float GetStamina() const { return Stamina; }
    /** World time stamina last came back to full (the HUD's brief shine). */
    float GetStaminaFullAt() const { return StaminaFullAt; }
    /** Tests: set the stamina directly. */
    void SetStamina(float Value) { Stamina = FMath::Clamp(Value, 0.f, 1.f); }
    // Climbing effort (user 2026-10-09): flavour only. Each wall run, wall
    // jump, side wall run and ring-to-ring leap takes a small sip of stamina,
    // and it doesn't refill while he's on the walls or swinging; it never
    // stops a climb (empty, he climbs and swings just the same). The tallest
    // chimney by the sewer costs about a quarter of the ring.
    // User 2026-10-12: climbing walls drains more (about 1.65x; the tallest
    // chimney now costs nearly a third of the ring).
    static constexpr float WallRunStaminaCost = .01f;
    static constexpr float WallJumpStaminaCost = .0065f;
    static constexpr float WallSideStaminaCost = .04f;
    static constexpr float SwingLeapStaminaCost = .012f;
    // Stamina lock (later, Chult: the golden "Premium Buhetian Halfling Leaf"
    // cigarettes): full and frozen for a while - the sprint never runs down,
    // a wall run climbs on past its three steps at this speed until the top
    // or he jumps off, and the side wall run holds a longer, flatter arc.
    static constexpr float LockedWallRunSpeed = 120.f;   // cm/s up
    static constexpr float LockedWallSideTime = 2.2f;    // s (unlocked: WallSideTime)
    static constexpr float LockedWallSideGravity = 235.f;   // cm/s2 (unlocked: WallSideGravity)
    static constexpr float GoldenLeafDuration = 45.f;
    uint32 GetResetGeneration() const { return ResetGeneration; }
    float GetStaminaLockRemaining() const { return IsStaminaLocked() ? StaminaLockUntil - GetWorldTime() : 0.f; }
    float GetWorldTime() const;
    void LockStamina(float Seconds);
    bool IsStaminaLocked() const;
    /** Run -> four-legged sprint pose blend (0..1). */
    float GetSprintWeight() const { return SprintWeight; }
    /** Seconds until stamina is full again (0: ready; a full refill while sprinting). */
    float GetSprintCooldownLeft() const;
    int32 GetSprints() const { return Sprints; }
    // The sprint leap (user 2026-10-07): a plain jump at a sprint keeps the
    // sprint going and is a long leap on all fours at the full sprint speed
    // with more lift (SprintLeapVerticalSpeed); he lands on his forepaws back
    // into the gallop. Vaults, side jumps and wall runs out of a sprint stay at
    // run speed.
    bool IsSprintLeaping() const { return Gait == EGait::Air && bSprintLeap; }
    // The leap needs a sprinting run-up (user 2026-10-08): this far in a straight
    // line on the ground at a sprint (a turn of more than 25 degrees or leaving
    // the ground starts it over); without it a jump is the ordinary running
    // jump. A metre - not much more than a crate's width (the docks' 60 cm,
    // the pantry's 56) - but longer than any straight run the pantry's cheese
    // island allows (68 cm measured, across its crate's diagonal), so a rat
    // that gets onto the cheese can't leap back off it. The leap itself costs
    // no stamina.
    static constexpr float SprintLeapRunup = 100.f;   // cm
    /** Straight sprinting run-up so far on the ground (cm). */
    float GetSprintRunup() const { return RunupLength; }
    int32 GetSprintLeaps() const { return SprintLeaps; }
    /** Tests (input disabled): the raw stick a dodge reads to choose its exit. */
    void SetTestStick(FVector2D Stick) { InputRight = Stick.X; InputForward = Stick.Y; }
    /** The test stick pushed along a world direction, whichever way the camera now looks. */
    void SetTestStickWorld(const FVector& Direction)
    {
        const FRotator View(0, ViewYaw, 0);
        InputForward = static_cast<float>(FVector::DotProduct(Direction, View.Vector()));
        InputRight = static_cast<float>(FVector::DotProduct(Direction, FRotationMatrix(View).GetUnitAxis(EAxis::Y)));
    }
    /** Tests that enable live input: ignore mouse/stick look so a real mouse cannot steer them. */
    void SetLookLocked(bool bLocked) { bLookLocked = bLocked; }
    /** In the air on a leap out of a run. */
    bool IsRunJumping() const { return Gait == EGait::Air && bRunJump; }
    void Recenter();
    /** True when the orbit sits in its upper (elevated) half. */
    bool IsElevated() const;
    /** Orbit look pitch (deg): -60 high, -48 elevated preset, -5 rat height, up to +30 looking up. */
    float GetLookPitch() const { return LookPitch; }
    /** Captures: set the orbit height directly. */
    void SetOrbitPitch(float Pitch) { LookPitch = SmoothLook = Pitch; }
    static FVector StartLocation() { return FVector(-240, -180, 36); }
    /** v1 animation instance on GetMesh(); null until play begins. */
    UChuckAnimInstance* GetChuckAnim() const;
    int32 GetGroomCount() const;
    /** Cigarette prop on socket_cigarette; null with -ChuckNoCigarette. */
    UStaticMeshComponent* GetCigarette() const { return Cigarette; }
    UStaticMeshComponent* GetCigaretteSmoke() const { return Smoke; }
    /** F / Y: talk to the NPC in front of him, or advance / close the conversation. */
    void Interact();
    bool IsTalking() const { return TalkingTo.IsValid(); }
    /** Which of the NPC's lines is showing (0 first). */
    int32 GetTalkLine() const { return TalkLine; }
    /** Who Chuck is talking to (they gesture while he listens). */
    const ADockNPC* GetTalkingTo() const { return TalkingTo.Get(); }
    /** Someone within reach and in front who has something to say (the HUD prompt). */
    ADockNPC* GetTalkPrompt() const;
    /** The line on screen: speaker and text (false when not talking). */
    bool GetDialogue(FString& Speaker, FString& Text) const;
    /** Breathe out a stream of smoke now (user 2026-09-30: he's always
     *  smoking; he exhales every 7-12 s on his own when calm). */
    void Exhale();
    int32 GetExhales() const { return Exhales; }
    int32 GetSmokePuffsSpawned() const { return SmokePuffsSpawned; }
    int32 GetSmokePuffsLive() const { return SmokePuffs.Num(); }
    static constexpr float ExhaleEvery = 7.f;      // s, plus up to 5 s more
    static constexpr float ExhaleLength = .7f;     // s of breath
    /** Current locomotion state for tests and captures: Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump, Slash, WallRun, Hang, Climb or Strafe. */
    const TCHAR* GetGaitName() const;
protected:
    virtual void BeginPlay() override;
private:
    friend class AFallCutscene;
    bool bDemoEnded = false;
    bool bCutscene = false;
    bool bSceneHold = false;
    TWeakObjectPtr<class AFallCutscene> FallScene;
    void BeginFallCutscene();
    /** In the cutscene the pose is the clip it sets (Base/BaseTime), with no gait logic or foot IK. */
    void UpdateCutscenePose(float DeltaSeconds);
    bool bSewerRespawn = false;
    bool bIgnoringAstralFloor = false;
    static float AreaStartYaw(const FVector& Location);
    bool bPantryRespawn = false;   // came down into the tavern pantry: deaths there return to its ladder
    bool bChultRespawn = false;    // landing preview uses its local entrance
    UPROPERTY() USpringArmComponent* Boom;
    UPROPERTY() UCameraComponent* Camera;
    UPROPERTY() TArray<UGroomComponent*> Grooms;
    UPROPERTY() USkeletalMesh* PlainMesh;
    UPROPERTY() UStaticMeshComponent* Cigarette;
    UPROPERTY() UStaticMeshComponent* Smoke;
    UPROPERTY() UInstancedStaticMeshComponent* ExhaleSmoke;
    UPROPERTY() TArray<USoundBase*> ExhaleSounds;
    UPROPERTY() TArray<USoundBase*> ClimbSounds;   // claws on bark, each paw contact up a wall
    struct FSmokePuff { FVector Position; FVector Velocity; float Age; float Life; float Size; };
    TArray<FSmokePuff> SmokePuffs;
    float NextExhaleAt = 5.f;
    float ExhaleUntil = -1.f;
    float ExhaleCarry = 0.f;
    int32 Exhales = 0;
    TWeakObjectPtr<ADockNPC> TalkingTo;
    int32 TalkLine = 0;
    int32 SmokePuffsSpawned = 0;
    void UpdateExhale(float DeltaSeconds);
    float ViewYaw = 0;
    /** One continuous GTA-style orbit instead of two switched cameras. */
    float LookPitch = -48;
    float SmoothLook = -48;
    /** Seconds since the last look input; the orbit drifts behind Chuck after a while. */
    float LookIdle = 0;
    /** Camera pivot height: holds through jumps, follows real level changes. */
    float FollowZ = 0;
    bool bFollowReady = false;

    // v1 clips (docs/RIG-CONTRACT-V1.md, SourceAssets/Chuck/V1/Animations/manifest.json).
    enum class EClip : uint8 { Idle, WalkStart, WalkLoop, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand, Roll, SideJumpLeft, SideJumpRight, RunLoop, RunJump, SlashRight, SlashLeft, WallRun, WallKick, Hang, PullUp, Mantle, ShimmyLeft, ShimmyRight, StrafeLeft, StrafeRight, StrafeRunLeft, StrafeRunRight, SlashLowRight, SlashLowLeft, Summon, SpeedVault, SprintLoop, SprintLeap, Swing, SwingLeapLeft, SwingLeapRight, Crawl, SwingSlip, FallFlail, HeldLeg, Tumble, CrashLand, WallSlam, Num };
    UPROPERTY() TArray<UAnimSequence*> Clips;
    enum class EGait : uint8 { Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump, Slash, WallRun, Hang, Climb, Strafe, Astral, WallSide, Ladder, Vault, Swing, SwingLeap, Held, Crawl, Thrown };
    EGait Gait = EGait::Idle;
    EClip Base = EClip::Idle;
    float BaseTime = 0;
    EClip Fading = EClip::Idle;
    float FadingTime = 0;
    float FadeWeight = 0;
    bool bFadeFromSnapshot = false; int32 FadeSnapshotSerial = 0;   // (SetClip: a fade over a fade starts from the pose shown)
    float FadeRate = 0;
    float StateTime = 0;
    float StartDistance = 0;
    float WalkPhase = 0;
    float StopTravel = 0;
    bool bStopPending = false;
    bool bStopMirror = false;
    float TurnStartYaw = 0;
    float TurnDelta = 0;
    float MeshDrop = 0;
    // Dodges (roll / side jump) and the raw stick that chooses between them.
    float InputForward = 0;
    float InputRight = 0;
    FVector DodgeDirection = FVector::ForwardVector;
    bool bDodgeLaunched = false;
    float DodgeBufferedAt = -1e3f;
    FVector2D DodgeBufferedStick = FVector2D::ZeroVector;
    int32 BufferedDodges = 0;
    /** Whether a dodge can start right now (the checks DodgeToward makes). */
    bool CanDodgeNow() const;
    /** Out of a move with the stick held: straight into the stride at walking or running pace; otherwise FinishDodge. */
    void RunOn();
    bool bDodgeLanded = false;
    float RollDone = 0;  // capsule distance actually covered in this roll
    bool bRunHeld = false;
    float RunWeight = 0;
    bool bRunJump = false;
    bool bSprinting = false;
    float Stamina = 1.f;
    float StaminaFullAt = -1e9f;
    float SprintWeight = 0;
    int32 Sprints = 0;
    bool bSprintLeapPending = false;   // jump pressed at a sprint, not yet off the ground
    bool bSprintLeap = false;
    FVector RunupFrom = FVector::ZeroVector, RunupDir = FVector::ZeroVector;
    float RunupLength = 0;
    float SprintAirTime = 0;   // off the ground at a sprint, not leaping
    int32 SprintLeaps = 0;
    void SprintPressed() { TrySprint(); }
    uint32 ResetGeneration = 0;
    float StaminaLockUntil = -1.f;
    void SpendStamina(float Cost);
    /** On the walls or swinging (wall jump flights included): stamina doesn't refill. */
    bool IsClimbing() const;
    void StartSprintLeap();
    /** Ends the sprint and starts the recovery; bShed: back down to run speed at once (jumps). */
    void EndSprint(bool bShed);
    bool bLookLocked = false;
    // Slash: the standing gait uses Base; on the move a layer plays (time < 0: none).
    bool bSlashQueued = false;
    bool bSlashHeld = false;
    FRandomStream SlashRandom;
    int32 SlashStrikes = 0;
    int32 SlashBreaks = 0;
    int32 SlashRatHits = 0;
    int32 SlashNPCHits = 0;
    int32 CigaretteCount = 0;
    int32 BitesTaken = 0;
    int32 Sanity = MaxSanity;
    int32 PickupsCollected = 0;
    enum class EAstral : uint8 { None, Vanishing, Away, Summoning, SlideDown, SlideAway };   // the last two: the sewer's water slide out
    EAstral AstralPhase = EAstral::None;
    float AstralClock = 0;
    bool bPendingVanish = false;
    float PendingVanishAt = 0;
    bool bAstralHidden = false;
    bool bAstralFaded = false;
    // In a zombie's grip.
    TWeakObjectPtr<AActor> HeldBy;
    float HeldClock = 0, HeldBiteAt = -1, HeldLimp = 0;
    bool bHeldVanish = false;
    int32 TimesHeld = 0, Throws = 0, CrashLands = 0, WallSlams = 0;
    enum class EThrown : uint8 { Flight, Crash, Slam };
    EThrown ThrownPhase = EThrown::Flight;
    bool bLegHeld = false, bBodyQuatLive = false;
    FQuat BodyQuat = FQuat::Identity, BodyQuatFrom = FQuat::Identity;
    FVector LegHoldFix = FVector::ZeroVector, PendC = FVector::ZeroVector, PendPrev = FVector::ZeroVector, SpinAxis = FVector::RightVector, ThrowSkid = FVector::ZeroVector, ThrowPrevVel = FVector::ZeroVector, SlamFrom = FVector::ZeroVector;
    float PendDt = 1.f / 60.f, SpinRate = 0.f, SpinAngle = 0.f, ThrownClock = 0.f, SlamFloorZ = 0.f;
    static constexpr float LegHoldLength = 29.f;   // cm, the held ankle to his centre
    static constexpr float WallSlamSlide0 = .1f;   // s into WallSlam he slides down the wall, to WallSlamSlide1 (the clip's slide_s)
    static constexpr float WallSlamSlide1 = .5f;
    static constexpr float MaxLegThrowSpeed = 950.f;   // cm/s along the throw
    void UpdateThrown(float DeltaSeconds);
    void BeginCrashLand();
    void BeginWallSlam(const FHitResult& Wall);
    void UpdateHeld(float DeltaSeconds);
    // The sewer's water slide (SewerSlide.cpp): carried down, out at the pier.
    void BeginSlide(float Into);
    bool FindPierExit();
    void HoldSlideCamera();
    void RestoreSlideCamera();
    float SlideS = 0, SlideSpeed = 0;
    FVector SlideFrom = FVector::ZeroVector, SlideCameraAt = FVector::ZeroVector, SavedCameraRelative = FVector::ZeroVector;
    FRotator SavedCameraRotation = FRotator::ZeroRotator;
    FVector ExitNormal = FVector::ZeroVector, ExitEdge = FVector::ZeroVector;
    bool bSlideCamera = false, bAutoClimb = false, bExitFound = false;
    int32 PierPushOuts = 0;
    int32 TailZigzagFrames = 0; double TailZigzagLoggedAt = -1.0;   // the squiggle detector (2026-11-12)
    FVector TailPelvisPrev = FVector::ZeroVector, TailHeadPrev = FVector::ZeroVector, TailBasePrev = FVector::ZeroVector;
    int32 TailJumpFrames = 0; FVector TailTipPrev = FVector::ZeroVector, TailMidPrev = FVector::ZeroVector; EGait TailGaitPrev = EGait::Idle;
    float GorgeFallClock = -1.f;                  // falling into the canyon gorge past the line (2026-11-12)
    static constexpr float GorgeFallTime = 1.6f;   // seconds of it before the Astral takes him
    int32 Slides = 0;
    int32 Respawns = 0;
    void BeginVanish();
    void UpdateAstral(float DeltaSeconds);
    void SetAstralHidden(bool bHide);
    void CameraFade(float From, float To, float Seconds, const FLinearColor& Colour = FLinearColor(.015f, .015f, .05f));
    float BiteImmuneUntil = -1;
    float ImpactAt = -1;
    UPROPERTY() TObjectPtr<UCurveFloat> HitForceCurve;
    /** Something low and breakable is in the strike's reach: rake low (user 2026-09-30). */
    bool LowTargetInReach() const;
    float SlashHitAt = -1;   // world time of the pending strike's cut
    void SlashHit();
    int32 SamePawRun = 0;
    EClip PickPaw(bool bFirst, EClip Previous);
    float SlashDone = 0;
    EClip LayerClip = EClip::SlashRight;
    float LayerTime = -1;
    EClip FadingLayerClip = EClip::SlashRight;
    float FadingLayerTime = -1;
    float FadingLayerWeight = 0;
    bool OwnsCapsule() const { return IsDodging() || Gait == EGait::Astral || Gait == EGait::Slash || Gait == EGait::WallRun || Gait == EGait::WallSide || Gait == EGait::Ladder || Gait == EGait::Vault || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::Swing || Gait == EGait::SwingLeap || Gait == EGait::Held || Gait == EGait::Thrown; }
    // Ledge state.
    FVector HangNormal = FVector::ZeroVector;
    FVector HangEdge = FVector::ZeroVector;   // the top edge on the wall face
    FVector HangFrom = FVector::ZeroVector;   // snap-in start
    float HangClock = 0;
    float HangHold = 0;
    float ShimmyPhase = 0;
    float HangSnapTime = .12f;     // snap-in (grab) or turn (corner) duration
    float HangYawFrom = 0;
    // Round a corner the stick you were shimmying with keeps meaning "carry on
    // this way" (the camera lags the turn), until you move the stick.
    bool bCornerCarry = false;
    float CornerCarrySide = 0;
    FVector2D CornerCarryStick = FVector2D::ZeroVector;
    int32 OuterCorners = 0;
    int32 InnerCorners = 0;
    float AirApexZ = 0;            // highest point of the current fall
    int32 LandingRolls = 0;
    // Strafe state.
    float StrafeKeys = 0;          // Q/E axis
    float StrafeTrigger = 0;       // LT axis
    bool bTestStrafe = false;
    float StrafePhase = 0;
    // Crawling (2026-10-26): the Crawl clip's phase, advanced by travel; how many crawls.
    float CrawlPhase = 0;
    float CrawlCam = 0;   // 0..1: the camera's crawl framing
    int32 Crawls = 0;
    void EnterCrawl();
    int32 StrafeJumps = 0;
    bool bSideLong = false;
    bool StrafeHeld() const { return StrafeKeys != 0 || StrafeTrigger > .3f || bTestStrafe; }
    /** Sideways stick including Q/E. */
    float SideInput() const { return FMath::Clamp(InputRight + StrafeKeys, -1.f, 1.f); }
    void StrafeKeysAxis(float Value);
    void StrafeTriggerAxis(float Value) { StrafeTrigger = Value; }
    // Drop to hang: the stick that walked him off is ignored until released.
    bool bHangNeedsRelease = false;
    int32 DropHangs = 0;
    bool TryDropHang();
    // Movement SFX (SourceAssets/Audio/SFX, Tools/gen_chuck_sfx.py), 2D under the soundtrack.
    UPROPERTY() TArray<USoundBase*> StepWalkWood;
    UPROPERTY() TArray<USoundBase*> StepWalkStone;
    UPROPERTY() TArray<USoundBase*> StreamSplashSounds;
    int32 StreamStepCount = 0;
    UPROPERTY() TArray<USoundBase*> StepRunWood;
    UPROPERTY() TArray<USoundBase*> StepRunStone;
    UPROPERTY() TArray<USoundBase*> JumpSounds;
    UPROPERTY() TArray<USoundBase*> LandSounds;
    UPROPERTY() TArray<USoundBase*> SlashSounds;
    UPROPERTY() TArray<USoundBase*> RollSounds;
    int32 SfxCounts[static_cast<int32>(ESfx::Num)] = {};
    bool bPrevStance[2] = {false, false};
    void PlaySfx(const TArray<USoundBase*>& Set, ESfx Kind, float Volume, float StartTime = 0.f);
    void PlayStep(const FVector& Paw, float Speed);
    bool TryHangCorner(const FVector& Along, float Side);
    void TurnHangCorner(const FVector& Normal, const FVector& Edge, bool bRoom, float Side);
    void LandingRoll();
    bool bHangRoom = false;
    FVector ClimbStart = FVector::ZeroVector;
    FVector ClimbDir = FVector::ZeroVector;
    float ClimbRise = 0;
    float ClimbAdvance = 0;
    bool bClimbMantle = false;
    float LedgeCooldownUntil = -1;
    int32 Hangs = 0;
    int32 PullUps = 0;
    int32 LedgeScrambles = 0;
    int32 Mantles = 0;
    // bThin: a top only 2.5 cm deep counts too (a rail's bar; 2026-11-11). Off for the automatic ups that step straight over
    // onto whatever stands beyond (mantle, scramble), so walking at a bulwark never carries him over it.
    bool FindLedge(const FVector& Normal, const FVector& FacePoint, float MinAbove, float MaxAbove, FVector& OutEdge, bool& bRoom, bool bThin = true) const;
    /** Where he ends up standing after climbing over Edge: the first clear spot
     *  inward from it (In cm first, then further), on whatever slope is there. */
    bool FindStand(const FVector& Normal, const FVector& Edge, float In, FVector& OutStand) const;
    /** The capsule while hanging from Edge, kept clear of any eave overhanging the face. */
    FVector HangHoldAt(const FVector& Edge) const;
    float HangOut = 0, HangLower = 0;   // extra clearance from the face / below the edge for this grab
    void EnterHang(const FVector& Normal, const FVector& Edge, bool bRoom);
    void DropFromHang();
    void StartClimb(bool bMantle, const FVector& Normal, const FVector& Edge, const FVector* StandAt = nullptr);
    bool TryMantle();
    // Up from a hang (user 2026-11-11, "like other games"): onto a top with room to stand; else balanced on a narrow top
    // (FindPerch); else up to a higher edge in reach (TryHangHop). False: nothing there to go up to.
    bool TryHangUp();
    /** A narrow walkable top beyond Edge (at least 4 cm deep) with the capsule clear standing centred on it. */
    bool FindPerch(const FVector& Normal, const FVector& Edge, FVector& OutStand) const;
    bool TryHangHop();
    int32 PerchClimbs = 0, HangHops = 0;
    /** Any top edge his paws reach ahead along Probe (an eave over his head, a sloped cut end included). */
    bool TryGrabEdge(const FVector& Probe);
    bool bHangClimbQueued = false;   // a jump pressed as he caught the edge: up and over once he's hanging
    int32 EaveGrabs = 0;
    // Brachiation state.
    int32 SwingGrip = -1, SwingTarget = -1, SwingLeft = -1;   // hanging from / flying to / last let go of
    float SwingAngle = 0, SwingRate = 0;   // pendulum (rad, rad/s; positive: his body ahead of the grip)
    float SwingClock = 0, SwingBlendTime = .15f, SwingRegrabAt = -1, SwingYawFrom = 0;
    float SwingLeapClock = 0, SwingLeapTime = .5f, SwingLeapFromAngle = 0, SwingLeapYaw = 0;
    FVector SwingDir = FVector::ForwardVector, SwingFrom = FVector::ZeroVector, SwingLeapFrom = FVector::ZeroVector, SwingLeapDir = FVector::ForwardVector;
    bool bSwingJumpQueued = false;
    int32 SwingCatches = 0, SwingLeapCount = 0, SwingDismounts = 0;
    // Where his paws hold the current grip (a board's near edge), and fly to.
    FVector SwingHold = FVector::ZeroVector, SwingLeapTo = FVector::ZeroVector;
    // The flight: his centre's start, its velocity on the swing as he let go,
    // the launch velocity the throw builds to; the body's spin as he let go
    // (deg/s); his velocity at the catch, carried briefly onto the new swing.
    FVector SwingLeapC0 = FVector::ZeroVector, SwingLeapVs = FVector::ZeroVector, SwingLeapV0 = FVector::ZeroVector, SwingFromVel = FVector::ZeroVector;
    float SwingLeapSpin = 0;
    FVector SwingLeapAt(float T) const;
    FVector SwingCentreFacing(const FVector& Grip, float BodyDeg, const FVector& Forward) const;
    // A jump waiting for the swing to build (since when; whether it has swung toward the target yet).
    float SwingWaitSince = -1, SwingReachPrev = 0; int32 SwingWaitTarget = -1; bool bSwingWaitToward = false;
    float SwingStrokePeak = 0;   // the fastest this stroke toward a waited leap's grip has gone (rad/s)
    float SwingBackAmp = 0, SwingTurnedAt = -1, SwingTurnSign = 0;   // the last back swing's height (deg); when and which way the swing last turned
    // A leap without the swing for it: falls short (and that grip can't be caught on the way down).
    bool bSwingLeapShort = false; int32 SwingMissed = -1; float SwingMissUntil = -1; int32 SwingShorts = 0;
    bool bTestSwingPump = false;
    float DuckBlend = 0;   // ears laid back, chin up: under a board
    FVector SwingHoldPoint(int32 Index, const FVector& Facing) const;
    // The whole body pitched about the capsule centre (the swing), easing back upright after.
    float BodyPitch = 0;
    FVector BodyPitchAxis = FVector::RightVector;
    bool bBodyPitchLive = false;
    bool TrySwingCatch();
    void EnterSwing(int32 Index, bool bFromLeap);
    void SwingJump();
    int32 FindSwingTarget(const FVector& Want) const;
    void LeaveSwing(const FVector& Velocity, bool bKick);
    /** Capsule centre hanging from Grip with the body pitched BodyDeg about its line (current facing). */
    FVector SwingCentre(const FVector& Grip, float BodyDeg) const;
    // Wall run / wall jump state.
    FVector WallNormal = FVector::ZeroVector;      // horizontal, out of the wall
    FVector LastWallNormal = FVector::ZeroVector;  // the wall last run: no fresh steps until another wall or the ground
    float WallRunClock = 0;
    float WallPhase = 0;
    float WallPrevZ = 0;
    bool bWallAuto = false;        // caught after a wall jump: the stick isn't needed
    bool bWallJumpFlight = false;  // in the air from a wall jump
    bool bChimney = false;         // another wall faces this one behind him: frame it side-on
    float WallCoyoteUntil = -1;
    float AirJumpPressedAt = -1e3f;
    int32 WallRuns = 0;
    int32 WallJumps = 0;
    // Side wall run state.
    FVector WallSideAlong = FVector::ZeroVector;   // horizontal, along the wall the way he runs
    float WallSideGap = 0;     // cm still between capsule and wall: closed over the first frames, not snapped
    float RunTakeoffAt = -1;   // when the current running jump left the ground
    float WallSideSpeed = 0, WallSideClock = 0, WallSideStartZ = 0, WallSideTravel = 0, WallSideRise = 0, WallSideTilt = 0;
    int32 WallSideRuns = 0, WallSideAirCatches = 0;
    bool bWallChainFlight = false;   // the wall jump left a side wall run: the next wall is run along, not up
    float WallChainAt = -1;
    float SideLeftAt = -1e3f;        // when the last side wall run ended (its coyote kick chains too)
    int32 WallChains = 0;
    // Ladders (ChuckClimbable).
    int32 LadderIndex = -1, LadderMounts = 0, FallDeaths = 0;
    float LadderPhase = 0, LadderClock = 0, LadderCooldownUntil = -1;
    FVector LadderFrom = FVector::ZeroVector, ClimbFrom = FVector::ZeroVector;
    bool bClimbReverse = false;    // lowering himself from the floor above onto a ladder: the pull-up played backward
    float LadderSide = 0;          // across a net: how far to his right of its line (ChuckClimbable SideTravel)
    // A net he turns round on (Approach): the stick's way when it set climbing up or down, kept while it stays that way
    // (held out over the rail he goes on up as he turns to face back in; climbing down, on to the deck).
    FVector LadderLatchDir = FVector::ZeroVector;
    float LadderLatchSign = 0, LadderLatchUp = 0;
    // Stepped off such a net at its foot, the stick still held out over the rail: not straight back on.
    int32 LadderNoRemount = -1;
    FVector LadderNoRemountDir = FVector::ZeroVector;
    FVector LadderLeanUp = FVector::UpVector;   // the mesh's up on a slanted net (eased)
    float LadderBottomOf(const FChuckClimbable& C) const;
    float LadderTopOf(const FChuckClimbable& C) const;
    bool TryMountLadder();
    // Speed vault.
    bool TryVault();
    FVector VaultStart = FVector::ZeroVector, VaultDir = FVector::ForwardVector;
    float VaultTotal = 0, VaultRise = 0, VaultLift = 0, VaultIn = .25f, VaultOut = .75f, VaultTime = .5f, VaultSpeed = 0, VaultClock = 0;
    int32 Vaults = 0, VaultRefusal = 0;
    void EnterLadder(int32 Index);
    void FallToDeath();
    bool TryWallSideRun(bool bInAir = false);
    void LeaveWallSide();
    bool ProbeSideWall(const FVector& From, const FVector& Side, float Reach, FVector& OutNormal, FVector& OutPoint) const;
    FVector StickWorld() const;
    bool TryEnterWallRun();
    void EnterWallRun(const FHitResult& Hit, const FVector& Normal);
    void LeaveWall();
    void WallJump();
    void UpdateSlashLayer(float DeltaSeconds);
    bool bHardLanding = false;
    // A latch, not a hold: holding Shift while pressing Space (and a direction)
    // can exceed what many keyboards register at once.
    void RunPressed() { bRunHeld = !bRunHeld; }
    bool IsDodging() const { return Gait == EGait::Roll || Gait == EGait::SideJump; }
    void Dodge();
    void FinishDodge();
    FVector PreviousMotionLocation = FVector::ZeroVector;
    void SetClip(EClip Clip, float Time, float FadeSeconds);
    float Period(EClip Clip) const;
    float FindGround(const FVector& Near, float Fallback) const;

    void Forward(float Value);
    void Right(float Value);
    void MouseLook(float Value);
    void Turn(float Value);
    void MousePitch(float Value);
    void StickPitch(float Value);
    void Quit();
    void UpdateCamera(float DeltaSeconds = 0);
    void UpdateMotion(float DeltaSeconds);
};
