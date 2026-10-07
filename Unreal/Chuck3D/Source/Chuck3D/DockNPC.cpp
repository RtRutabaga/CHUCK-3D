#include "DockNPC.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "NPCVoiceData.h"

namespace
{
    TArray<TWeakObjectPtr<ADockNPC>> NPCRegistry;
    constexpr float HalfHeight = 90.f;
    // The humans' shared skeleton: MPFB's game_engine rig (Unreal mannequin
    // names, three bones per finger). Each R bone directly follows its L bone.
    enum EBone { Pelvis, Spine1, Spine2, Chest, Neck, Head, ClavL, ClavR, UpperL, UpperR, LowerL, LowerR, HandL, HandR,
        ThumbL, ThumbR, MiddleL, MiddleR, ThighL, ThighR, CalfL, CalfR, FootL, FootR, BallL, BallR, BoneCount };
    const TCHAR* BoneNames[] = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r"),
        TEXT("thumb_01_l"), TEXT("thumb_01_r"), TEXT("middle_01_l"), TEXT("middle_01_r"),
        TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r") };
    // Fingers: thumb, index, middle, ring, little; three joints each.
    const TCHAR* FingerNames[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
    // Degrees each joint bends toward the palm: a hand at rest (the little
    // finger curls most, the index least), and closed round a shaft.
    constexpr float Relaxed[5][3] = { {6.f, 10.f, 10.f}, {10.f, 16.f, 12.f}, {14.f, 20.f, 14.f}, {18.f, 24.f, 16.f}, {24.f, 28.f, 18.f} };
    constexpr float Gripped[5][3] = { {25.f, 35.f, 25.f}, {62.f, 85.f, 55.f}, {68.f, 85.f, 55.f}, {70.f, 85.f, 55.f}, {74.f, 85.f, 55.f} };
    constexpr float SmithSpeakPause = .2f;   // s the smith waits, hammer down, before he speaks
    constexpr float ClipSmoothing = .1f;    // s: the motion capture's low-pass (its jitter is several times a second)
    constexpr float WristStraight = .65f;   // how much of the clips' unreliable wrist bend is taken out
    constexpr float ArmClear = 5.f;         // deg the clips' arms are eased out so the hands clear wider hips
    // The spear: its butt on the ground beside his right foot, a touch ahead,
    // leaning a little out; his fist closes on the leather wrap.
    // (ADockNPC::PoleAhead/PoleOut/PoleLean/PoleGrip: 16, 30, 4 deg, 108 cm up the shaft, whose wrap is 100-124.)
    // The dwarf's axe: closer in and a touch more upright (a shorter body), fist mid-wrap (66-86 cm).
    // Out and leaning away so the double head stays clear of his pauldrons as he turns (user 2026-10-04).
    constexpr float AxeAhead = 16.f, AxeOut = 33.f, AxeLean = 7.f, AxeGrip = 78.f;   // grip high on the wrap: his short arm keeps a bent elbow
    // The dwarf never looks down (his beard would go through his breastplate) and turns his head
    // less; the clip's own head and neck motion is damped to this much.
    constexpr float DwarfLookYaw = 15.f, DwarfClipHead = .25f;
    constexpr float FistReach = 8.f;           // cm from the wrist to the middle of a closed fist
    constexpr float PalmDepth = 3.f;           // cm from the knuckle line to the middle of the fist, palm side
    const TCHAR* MeshPaths[] = { TEXT("/Game/Characters/Humans/DockWorker/SK_DockWorker.SK_DockWorker"),
        TEXT("/Game/Characters/Humans/Guard/SK_Guard.SK_Guard"), TEXT("/Game/Characters/Humans/MarketWoman/SK_MarketWoman.SK_MarketWoman"),
        TEXT("/Game/Characters/Humans/GuardWoman/SK_GuardWoman.SK_GuardWoman"), TEXT("/Game/Characters/Humans/SideGuard/SK_SideGuard.SK_SideGuard"),
        TEXT("/Game/Characters/Humans/Zombie/SK_Zombie.SK_Zombie"), TEXT("/Game/Characters/Humans/Blacksmith/SK_Blacksmith.SK_Blacksmith"),
        TEXT("/Game/Characters/Humans/Dwarf/SK_Dwarf.SK_Dwarf"), TEXT("/Game/Characters/Humans/TavernKeeper/SK_TavernKeeper.SK_TavernKeeper"),
        TEXT("/Game/Characters/Humans/ElfElder/SK_ElfElder.SK_ElfElder"), TEXT("/Game/Characters/Humans/GnomeAlchemist/SK_GnomeAlchemist.SK_GnomeAlchemist"),
        TEXT("/Game/Characters/Humans/Sailor/SK_Sailor.SK_Sailor"),
        TEXT("/Game/Characters/Humans/Bobert/SK_Bobert.SK_Bobert") };
    // The sailor's pipe (component space: his feet, X forward, Y right). His
    // mouth on the model (SourceAssets/NPCs/Humans/manifest.json Sailor
    // mouth_cm); the bit sits in its right corner, the stem forward and a
    // little out and down. The bowl from the bit (Tools/build_sailor_props.py
    // bowl_cm); his fist cups it from under it.
    const FVector SailorMouth(14.7f, 0.f, 157.2f);
    constexpr float MouthCorner = 2.2f;
    const FVector PipeBowl(11.8f, 0.f, -1.8f), PipeCup(11.f, 0.f, -6.f);
    // Every PipePeriod s: the hand up (PipeRaise), held while he draws (to PipeLower), down; then he breathes out.
    constexpr float PipePeriod = 10.f, PipeRaise = .8f, PipeLower = 2.8f, PipeDown = 3.6f, PipeExhaleAt = 3.f, PipeExhaleLength = 1.4f;
    // The gnome's sleeves: each upper arm swung well forward (the elbow in front of his robe, a
    // little out), the forearm level across to just past the middle (into the other sleeve), the
    // left cuff over the right.
    constexpr float ElbowAhead = .65f, ElbowOut = .12f, SleeveCross = 2.f, SleeveStack = 1.5f;
    // The old elf on her bench (component space: the floor under her hip joints, X forward, Y right).
    constexpr float SitBone = 9.f;          // cm from her hip joints down to the bench under her (a slight woman, a skirt)
    constexpr float ShinLean = 6.f;         // deg her shins lean forward from upright, feet a little out in front
    constexpr float LapAlong = .55f;        // her hands rest this far from hip to knee
    constexpr float LapLift = 10.5f;        // and this far over the thigh bone (the thigh, the skirt, the palm under the fist's middle)
    const FVector Fountain(260.f, -3320.f, 0.f);   // DockPlaza.cpp's fountain, where her eyes go
    // The dock worker at his tavern table at night (component space as hers).
    // His mouth on the model (SourceAssets/NPCs/Humans/manifest.json DockWorker mouth_cm).
    const FVector WorkerMouth(15.7f, 0.f, 161.7f);
    constexpr float WorkerSitBone = 10.5f;     // a man in trousers: a little more under him than the elf
    const FVector TavernBar(-55.f, 905.f, 150.f);  // the keeper behind his counter (SpawnTownsfolk), where his eyes go
    // The tankard (Tools/build_keeper_props.py, its manifest entry): the handle's
    // middle 7.5 cm over its base; the rim 4.4 cm round, 5.5 cm over the grip.
    constexpr float TankardGripHeight = 7.5f, TankardRim = 4.4f, TankardRimHeight = 5.5f;
    // Resting on the table in front of him, a little to his right, his fist on the handle.
    constexpr float DrinkRestAhead = 34.f, DrinkRestOut = 14.f;
    // Every DrinkPeriod s: up to his lips (to DrinkRaise), tipped further as he
    // drinks (to DrinkLower), and down to the table (by DrinkDown). The tankard
    // tips from DrinkTipStart to DrinkTipEnd degrees off upright, his head back by DrinkHeadBack.
    constexpr float DrinkPeriod = 15.f, DrinkRaise = 1.3f, DrinkLower = 3.4f, DrinkDown = 4.7f;
    constexpr float DrinkTipStart = 62.f, DrinkTipEnd = 100.f, DrinkHeadBack = 16.f;
    // Bobert's barrel (Tools/build_bobert_barrel.py; its manifest entry has the
    // same numbers): lying along its X, mouth to +X, origin on the ground under
    // its middle (cm). The inside floor rises toward the ends with the bilge.
    constexpr float BarrelLength = 86.f, BarrelBelly = 38.f, BarrelEnd = 31.f, BarrelStave = 2.6f, BarrelAxisZ = 38.6f;
    constexpr float BarrelBedroll = -18.8f;   // the front of the rolled blanket against the back head (inside face at -37.3)
    float BarrelRadius(float X) { const float T = FMath::Min(1.f, FMath::Abs(X) / (BarrelLength * .5f)); return BarrelBelly - (BarrelBelly - BarrelEnd) * T * T; }
    float BarrelFloor(float X) { return BarrelAxisZ - (BarrelRadius(X) - BarrelStave); }
    // His sleeping pose (degrees; tried on his built body and barrel in Blender
    // before it was written here): the small of his back leaning back into the
    // blanket, slumping forward above it, the neck forward and the head bowed
    // and fallen to one side; knees up and a little apart, shins down and
    // forward to the floor; forearms folded over the knees. The head is bowed
    // only so far that his face still looks out of the barrel's mouth (user
    // 2026-10-06: "make his face easier to see"; it was 50, his face to his knees).
    constexpr float SleepLean = 14.f, SleepCurl = 22.f, SleepNeck = 26.f, SleepNod = 31.f, SleepTiltDeg = 14.f;
    // A faint fill inside the mouth, the daylight off the paving: in the barrel's
    // shade his face was black. No shadows and no glint on his closed lids. Kept
    // dim and short so it reads as bounce, not a lamp (user 2026-10-06: it was
    // 160 / 85 cm, "unnatural"); removed at night (DockReturn.cpp).
    const FVector BarrelFill(30.f, 0.f, 50.f);
    constexpr float BarrelFillIntensity = 50.f, BarrelFillRadius = 65.f;
    constexpr float SleepKnee = 45.f, SleepSpread = .22f;
    // Measured on the posed body, as fractions of his thigh bone: how far his
    // seat is below the hip joints (where his boots are put on the floor), how
    // far his lowest point is below them, and how far his back is behind them.
    constexpr float SleepSeatDrop = .283f, SleepLift = .322f, SleepBack = .439f;
    constexpr float SleepSquash = 2.5f;         // cm his back presses into the blanket
    constexpr float SleepBreath = 5.5f;         // s, one slow sleeping breath
    // The tavern keeper's work (component space: his feet, X forward, Y right).
    // His left fist on the tankard's handle (Tools/build_keeper_props.py: the
    // origin), the body TankardBody toward its +X, the mouth TankardMouth up.
    const FVector KeeperGrip(31.f, -9.f, 118.f);    // just above the counter's back edge (top at 108 cm)
    constexpr float TankardBody = 7.1f, TankardMouth = 5.6f, TankardRadius = 5.f;
    // A round: the rag inside the rim, then over the outside; every third round he holds it up to look it over.
    constexpr float PolishInside = 6.f, PolishOutside = 5.f, PolishInspect = 4.f, PolishBlend = .6f;
    EBone Of(EBone Left, int32 Side) { return static_cast<EBone>(Left + Side); }
    // Motion-capture clips: idles per kind of person, and gesturing while talking.
    enum EClip { ClipStandHip, ClipStandLook, ClipTalk, ClipReact, ClipZombieIdle, ClipZombieWalk, ClipZombieFall, ClipCount };
    const TCHAR* ClipPaths[] = { TEXT("/Game/Characters/Humans/Anim/AS_Human_StandHip.AS_Human_StandHip"),
        TEXT("/Game/Characters/Humans/Anim/AS_Human_StandLook.AS_Human_StandLook"), TEXT("/Game/Characters/Humans/Anim/AS_Human_Talk.AS_Human_Talk"),
        TEXT("/Game/Characters/Humans/Anim/AS_Human_React.AS_Human_React"), TEXT("/Game/Characters/Humans/Anim/AS_Human_ZombieIdle.AS_Human_ZombieIdle"),
        TEXT("/Game/Characters/Humans/Anim/AS_Human_ZombieWalk.AS_Human_ZombieWalk"), TEXT("/Game/Characters/Humans/Anim/AS_Human_ZombieFall.AS_Human_ZombieFall") };
    // The zombie's shamble, in place: the walk clip's own speed at the clip
    // skeleton's scale (Tools/build_npc_mocap.py CHUCK_WALK_LOOP; the hips are
    // scaled to each body); its collapse is a lay-down played fast.
    constexpr float ZombieClipSpeed = 37.5f;   // cm/s
    constexpr float ZombieFallRate = 1.8f;
    constexpr float ZombiePace = 1.25f;        // the shamble clip played this much faster, and so it covers ground faster
    constexpr float ZombieTurnRate = 75.f;     // deg/s while shambling
    // Looking down at the rat: hardly at all until it's right at his feet
    // (a grown man doesn't crane at a rat across the street).
    constexpr float LookDownFar = 6.f;      // deg, most he tips his head while it's further off
    constexpr float CloseFull = 45.f, CloseStart = 95.f;   // cm between them: full look down .. from here
    constexpr float ReactIn = .2f, ReactOut = .5f;
    // The smith (component space: his feet, X forward, Y right). The anvil
    // (Tools/build_smith_props.py) stands this far ahead of his feet, its horn
    // to his left; the bar of hot iron lies across the face in front of him.
    constexpr float AnvilAhead = 49.f, AnvilRight = 4.f;
    constexpr float AnvilFace = 80.f;                 // cm, the working face
    constexpr float BarHalf = .8f;                    // the bar's half thickness
    constexpr float HammerHead = 29.f, HammerFace = 6.6f;   // handle to the head's centre; the striking face ahead of it
    // The tongs' bar, in their own frame (Tools/build_smith_props.py: the jaws bent
    // up 20 degrees at the boss), and the reins sloping down as much from his fist.
    const FVector TongsBar(46.07f, 0.f, 5.3f);
    constexpr float ReinsSlope = 20.f;
    // The rhythm of drawing out hot iron (user 2026-10-04: "more natural and
    // realistic"): about a blow every 0.8 s. The hammer bounces off the work
    // straight into the lift (LiftTime, decelerating to about shoulder height),
    // turns over (TurnTime) and comes down fast (DownTime). After every second
    // blow it drops lightly on the bare heel (TapRight beside the work) before
    // the lift: TapLead covers the rebound, the drop (landing at TapAt) and its bounce.
    constexpr float FirstLift = .62f, LiftTime = .46f, TurnTime = .06f, DownTime = .24f;
    constexpr float TapAt = .24f, TapLead = .32f, TapLift = .4f, TapRight = 10.f;
    constexpr float SetPause = 2.6f, InspectPause = 4.2f, RestSwing = .12f;
    // The forge beside him (DockPlaza.cpp's forge niche, in his frame: behind and to his left).
    const FVector ForgeAt(-60.f, -170.f, 75.f);
    constexpr int32 StrikesPerSet = 8;
    constexpr float TurnRate = 70.f;      // deg/s when he turns his body to the rat
    // Component axes: X forward, Y right, Z up. + pitch tips a bone forward
    // (the head looks down); + yaw turns it to his right.
    FQuat Pitch(float Degrees) { return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Yaw(float Degrees) { return FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Roll(float Degrees) { return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Degrees)); }
}

ADockNPC::ADockNPC()
{
    PrimaryActorTick.bCanEverTick = true;
    Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
    SetRootComponent(Blocker);
    Blocker->InitCapsuleSize(24.f, HalfHeight);
    // Solid to Chuck, invisible to his traces: no wall run up a man, no ledge on his shoulders.
    Blocker->SetCollisionProfileName(TEXT("Custom"));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Blocker->SetCollisionObjectType(ECC_WorldDynamic);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Blocker->SetCanEverAffectNavigation(false);
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(Blocker);
    Body->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Every human is referenced here, so all of them are cooked.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Worker(MeshPaths[0]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Guard(MeshPaths[1]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Woman(MeshPaths[2]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> GuardWoman(MeshPaths[3]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SideGuard(MeshPaths[4]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Zombie(MeshPaths[5]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Smith(MeshPaths[6]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Dwarf(MeshPaths[7]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Keeper(MeshPaths[8]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Elf(MeshPaths[9]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Gnome(MeshPaths[10]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Sailor(MeshPaths[11]);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> BobertBody(MeshPaths[12]);
    HumanMeshes[0] = Worker.Object; HumanMeshes[1] = Guard.Object; HumanMeshes[2] = Woman.Object; HumanMeshes[3] = GuardWoman.Object;
    HumanMeshes[4] = SideGuard.Object; HumanMeshes[5] = Zombie.Object; HumanMeshes[6] = Smith.Object; HumanMeshes[7] = Dwarf.Object;
    HumanMeshes[8] = Keeper.Object; HumanMeshes[9] = Elf.Object; HumanMeshes[10] = Gnome.Object;
    HumanMeshes[11] = Sailor.Object;
    HumanMeshes[12] = BobertBody.Object;
    // Bobert's barrel (Tools/build_bobert_barrel.py).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BarrelAsset(TEXT("/Game/Characters/Humans/Props/SM_BobertBarrel.SM_BobertBarrel"));
    BarrelMesh = BarrelAsset.Object;
    // The smith's anvil, hammer and tongs (Tools/build_smith_props.py).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AnvilAsset(TEXT("/Game/Characters/Humans/Props/SM_Anvil.SM_Anvil"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HammerAsset(TEXT("/Game/Characters/Humans/Props/SM_SmithHammer.SM_SmithHammer"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TongsAsset(TEXT("/Game/Characters/Humans/Props/SM_Tongs.SM_Tongs"));
    SmithMeshes[0] = AnvilAsset.Object; SmithMeshes[1] = HammerAsset.Object; SmithMeshes[2] = TongsAsset.Object;
    // The tavern keeper's tankard and rag (Tools/build_keeper_props.py).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TankardAsset(TEXT("/Game/Characters/Humans/Props/SM_Tankard.SM_Tankard"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RagAsset(TEXT("/Game/Characters/Humans/Props/SM_Rag.SM_Rag"));
    KeeperMeshes[0] = TankardAsset.Object; KeeperMeshes[1] = RagAsset.Object;
    // The sailor's pipe, its smoke (Chuck's cigarette wisp and breath puffs, larger).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PipeAsset(TEXT("/Game/Characters/Humans/Props/SM_Pipe.SM_Pipe"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> WispAsset(TEXT("/Game/Characters/Chuck/V1/Cigarette/SM_CigaretteSmoke.SM_CigaretteSmoke"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PuffSphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PuffMat(TEXT("/Game/Characters/Chuck/V1/Cigarette/M_SmokePuff.M_SmokePuff"));
    SailorMeshes[0] = PipeAsset.Object; SailorMeshes[1] = WispAsset.Object; SailorMeshes[2] = PuffSphere.Object; PuffMaterial = PuffMat.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> StandHip(ClipPaths[ClipStandHip]), StandLook(ClipPaths[ClipStandLook]),
        Talk(ClipPaths[ClipTalk]), React(ClipPaths[ClipReact]), ZombieIdle(ClipPaths[ClipZombieIdle]), ZombieWalk(ClipPaths[ClipZombieWalk]),
        ZombieFall(ClipPaths[ClipZombieFall]);
    Clips[ClipStandHip] = StandHip.Object; Clips[ClipStandLook] = StandLook.Object; Clips[ClipTalk] = Talk.Object; Clips[ClipReact] = React.Object;
    Clips[ClipZombieIdle] = ZombieIdle.Object; Clips[ClipZombieWalk] = ZombieWalk.Object; Clips[ClipZombieFall] = ZombieFall.Object;
    Body->SetSkinnedAssetAndUpdate(Worker.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpearAsset(TEXT("/Game/Characters/Humans/Props/SM_Spear.SM_Spear"));
    SpearMesh = SpearAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AxeAsset(TEXT("/Game/Characters/Humans/Props/SM_BattleAxe.SM_BattleAxe"));
    AxeMesh = AxeAsset.Object;
    Spear = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spear"));
    Spear->SetupAttachment(Body);
    Spear->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spear->SetCanEverAffectNavigation(false);
    Spear->SetVisibility(false);
}

void ADockNPC::GiveSpear(int32 Side)
{
    bSpear = SpearMesh != nullptr;
    if (!bSpear) return;
    SpearSide = FMath::Clamp(Side, 0, 1);
    Spear->SetStaticMesh(SpearMesh);
    Spear->SetVisibility(true);
    Grip[SpearSide] = 1.f;   // that hand closed round it
    PlaceSpear();
}

void ADockNPC::GiveAxe(int32 Side)
{
    bSpear = bAxe = AxeMesh != nullptr;
    if (!bSpear) return;
    SpearSide = FMath::Clamp(Side, 0, 1);
    PoleAhead = AxeAhead; PoleOut = AxeOut; PoleLean = AxeLean; PoleGrip = AxeGrip;
    Spear->SetStaticMesh(AxeMesh);
    Spear->SetVisibility(true);
    Grip[SpearSide] = 1.f;
    PlaceSpear();
}

void ADockNPC::PlaceSpear()
{
    // Which way is out on the spear side (the left arm's side is ArmOut, from the rest solve).
    const float Out = SpearSide == 1 ? -ArmOut : ArmOut;
    const FRotator Lean(0.f, 0.f, Out * PoleLean);   // the top leans out, away from the body
    Spear->SetRelativeLocationAndRotation(FVector(PoleAhead, Out * PoleOut, 0.f), Lean);
    SpearGrip = FVector(PoleAhead, Out * PoleOut, 0.f) + Lean.RotateVector(FVector(0.f, 0.f, PoleGrip));
}

ADockNPC* ADockNPC::SpawnHuman(UWorld* World, EDockHuman Kind, const FVector& Feet, float Yaw)
{
    const FTransform At(FRotator(0, Yaw, 0), Feet + FVector(0, 0, HalfHeight));
    auto* NPC = World->SpawnActorDeferred<ADockNPC>(ADockNPC::StaticClass(), At);
    if (!NPC) return nullptr;
    NPC->Kind = Kind;
    NPC->FinishSpawning(At);
    return NPC;
}

ADockNPC* ADockNPC::SpawnZombie(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Zombie, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("SewerZombie"));
    NPC->DisplayName = TEXT("Zombie");
    return NPC;
}

const TCHAR* ADockNPC::GetZombieStateName() const
{
    static const TCHAR* Names[] = { TEXT("Idle"), TEXT("Shamble"), TEXT("Windup"), TEXT("Lunge"), TEXT("Recover"), TEXT("Hurt"), TEXT("Dead") };
    return Names[static_cast<int32>(ZState)];
}

float ADockNPC::GetZombieWalkSpeed() const { return ZombieClipSpeed * ZombiePace * HipScale; }

void ADockNPC::TurnZombie(const FVector& Toward, float DeltaSeconds, float Rate)
{
    const FVector To = (Toward - GetActorLocation()).GetSafeNormal2D();
    if (To.IsNearlyZero()) return;
    const float Current = static_cast<float>(GetActorRotation().Yaw);
    const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, static_cast<float>(To.Rotation().Yaw)), -Rate * DeltaSeconds, Rate * DeltaSeconds);
    SetActorRotation(FRotator(0.f, Current + Step, 0.f));
}

bool ADockNPC::StepZombie(const FVector& Direction, float Distance)
{
    // Kinematic: blocked by walls (a capsule held clear of the floor's bumps),
    // and only onto floor: never off an edge or over a gap in the stream bed.
    if (Distance <= 0.f) return false;
    const FVector From = GetActorLocation(), To = From + Direction.GetSafeNormal2D() * Distance;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ZombieStep), false, this);
    const FCollisionObjectQueryParams Static(ECC_WorldStatic);
    FHitResult Hit;
    if (GetWorld()->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, Static, FCollisionShape::MakeCapsule(22.f, HalfHeight - 20.f), Query))
    {
        UE_LOG(LogTemp, Verbose, TEXT("CHUCK_ZOMBIE_BLOCKED by=%s/%s start_in=%d at=%s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.bStartPenetrating, *Hit.ImpactPoint.ToString());
        return false;
    }
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByObjectType(Floor, To - FVector(0, 0, HalfHeight - 30.f), To - FVector(0, 0, HalfHeight + 30.f), Static, Query))
    {
        UE_LOG(LogTemp, Verbose, TEXT("CHUCK_ZOMBIE_NO_FLOOR at=%s"), *To.ToString());
        return false;
    }
    SetActorLocation(FVector(To.X, To.Y, Floor.ImpactPoint.Z + HalfHeight));
    return true;
}

void ADockNPC::TickZombie(float DeltaSeconds)
{
    ZTime += DeltaSeconds;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector Location = GetActorLocation();
    float Distance = 1e6f, Rise = 1e6f, Facing = 180.f;
    FVector ToChuck = FVector::ZeroVector;
    if (Chuck)
    {
        ToChuck = Chuck->GetActorLocation() - Location;
        Distance = static_cast<float>(ToChuck.Size2D());
        Rise = static_cast<float>(FMath::Abs((Chuck->GetActorLocation().Z - Chuck->GetSimpleCollisionHalfHeight()) - (Location.Z - HalfHeight)));
        Facing = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToChuck.GetSafeNormal2D())), -1.f, 1.f)));
    }
    // A rat it could reach: on its level, not gone into the light, near its place.
    const bool bThere = Chuck && !Chuck->IsAstral() && Rise < 80.f && FVector::Dist2D(Chuck->GetActorLocation(), ZHome) < ZombieLeash;
    const bool bSees = bThere && (Distance < ZombieNotice || (Distance < ZombieSight && Facing < 70.f));
    float Moved = 0.f;
    switch (ZState)
    {
    case EZombie::Idle:
        if (bSees) SetZombie(EZombie::Shamble);
        break;
    case EZombie::Shamble:
    {
        // After the rat while it's there, otherwise back to its place.
        const bool bChase = bThere && (bSees || ZTime < 6.f || Distance < ZombieSight);
        const FVector Goal = bChase ? Chuck->GetActorLocation() : ZHome;
        if (!bChase && FVector::Dist2D(Location, ZHome) < 40.f) { SetZombie(EZombie::Idle); break; }
        TurnZombie(Goal, DeltaSeconds, ZombieTurnRate);
        const FVector To = (Goal - Location).GetSafeNormal2D();
        const float Ahead = static_cast<float>(FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), To));
        // It turns before it walks, and stops short rather than pushing into him.
        const float Speed = GetZombieWalkSpeed() * FMath::Clamp((Ahead - .3f) / .5f, 0.f, 1.f);
        if (!(bChase && Distance < 55.f) && StepZombie(GetActorForwardVector(), Speed * DeltaSeconds)) Moved = Speed * DeltaSeconds;
        if (bChase && Distance < ZombieStrike && Facing < 30.f) SetZombie(EZombie::Windup);
        break;
    }
    case EZombie::Windup:
        // The tell: it stops and rears up, arms lifting, turning to the rat.
        if (Chuck) TurnZombie(Chuck->GetActorLocation(), DeltaSeconds, 45.f);
        if (ZTime >= ZombieWindup) { LungeDir = GetActorForwardVector().GetSafeNormal2D(); bBit = false; ++Lunges; SetZombie(EZombie::Lunge); }
        break;
    case EZombie::Lunge:
        // Down and forward at him; one bite, hit or miss.
        StepZombie(LungeDir, 380.f * FMath::Max(0.f, 1.f - ZTime / ZombieLungeTime) * DeltaSeconds);
        if (!bBit && ZTime >= .22f)
        {
            bBit = true;
            if (Chuck && Distance <= ZombieBiteRange && Rise < 60.f && FVector::DotProduct(LungeDir, ToChuck.GetSafeNormal2D()) > .25f
                && Chuck->TakeBite(Location, ZombieBite)) ++Bites;
        }
        if (ZTime >= ZombieLungeTime) SetZombie(EZombie::Recover);
        break;
    case EZombie::Recover:
        if (ZTime >= ZombieRecover) SetZombie(EZombie::Shamble);
        break;
    case EZombie::Hurt:
        if (ZTime >= .35f) SetZombie(EZombie::Shamble);
        break;
    case EZombie::Dead:
        DeathTime += DeltaSeconds;
        if (!bDropped && DeathTime >= 1.2f)
        {
            bDropped = true;
            ACigarettePickup::Burst(GetWorld(), Location, Cigarettes, static_cast<float>(Location.Z) - HalfHeight);
        }
        break;
    }
    // The walk clip runs with the ground covered, so the feet don't slide.
    WalkTime += Moved / FMath::Max(1.f, ZombieClipSpeed * HipScale);
    const auto Ease = [DeltaSeconds](float& Value, float Target, float Rate) { Value = FMath::FInterpTo(Value, Target, DeltaSeconds, Rate); };
    Ease(WalkBlend, Moved > 0.f ? 1.f : 0.f, 5.f);
    Ease(Rear, ZState == EZombie::Windup ? 1.f : 0.f, ZState == EZombie::Windup ? 3.5f : 6.f);
    Ease(Reach, ZState == EZombie::Lunge ? 1.f : 0.f, ZState == EZombie::Lunge ? 14.f : 2.5f);
    if (FlinchTime >= 0.f && (FlinchTime += DeltaSeconds) > .5f) FlinchTime = -1.f;
    Ease(Flinch, FlinchTime >= 0.f && FlinchTime < .2f ? 1.f : 0.f, 16.f);
    // It watches the rat it's after, down at the floor.
    FVector2D Target(0.f, 12.f);
    if (bThere && ZState != EZombie::Idle && ZState != EZombie::Dead)
    {
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(ToChuck - FVector(0, 0, EyeHeight - HalfHeight));
        Target = FVector2D(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -60.f, 60.f),
            FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))), -10.f, 45.f));
    }
    Look.X = FMath::FInterpTo(Look.X, Target.X, DeltaSeconds, 2.5f);
    Look.Y = FMath::FInterpTo(Look.Y, Target.Y, DeltaSeconds, 2.5f);
    bWatching = bThere && ZState != EZombie::Idle;
}

ADockNPC* ADockNPC::SpawnDockWorker(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Worker, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("DockWorkerArt"));   // the human-scale reference the smoke test looks for
    NPC->DisplayName = TEXT("Dock worker");
    NPC->SetupVoice(TEXT("Worker"));   // user 2026-10-06: his ElevenLabs line, keeping watch over Bobert
    return NPC;
}

ADockNPC* ADockNPC::SpawnBlacksmith(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Blacksmith, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("Blacksmith"));
    NPC->DisplayName = TEXT("Blacksmith");
    NPC->SetupVoice(TEXT("Blacksmith"));   // user 2026-10-06: his recorded line about Bobert replaces the text ones
    NPC->SetGrip(0, 1.f); NPC->SetGrip(1, 1.f);   // tongs and hammer
    // His anvil on its stump, in front of him; solid, so the rat can hop up on it.
    const FRotator Facing(0.f, Yaw, 0.f);
    AActor* Anvil = World->SpawnActor<AActor>();
    if (Anvil && NPC->SmithMeshes[0])
    {
        auto* Mesh = NewObject<UStaticMeshComponent>(Anvil, TEXT("Anvil"));
        Mesh->SetStaticMesh(NPC->SmithMeshes[0]);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetWorldLocationAndRotation(Feet + Facing.RotateVector(FVector(AnvilAhead, AnvilRight, 0.f)), FRotator(0.f, Yaw - 90.f, 0.f));
        Anvil->SetRootComponent(Mesh);
        Mesh->RegisterComponent();
        // Simple boxes: the stump, the feet on it, and the body from the waist to the face (heel to horn).
        const FVector Boxes[][2] = { { FVector(0, 0, 25), FVector(23, 23, 25) }, { FVector(0, 0, 53), FVector(17, 12.5f, 3) },
                                     { FVector(11, 0, 68), FVector(31, 6, 12) } };
        for (const auto& B : Boxes)
        {
            auto* Box = NewObject<UBoxComponent>(Anvil);
            Box->SetupAttachment(Mesh);
            Box->SetRelativeLocation(B[0]); Box->SetBoxExtent(B[1]);
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->SetCanEverAffectNavigation(false);
            Box->RegisterComponent();
        }
        Anvil->Tags.Add(TEXT("Anvil"));
        NPC->Anvil = Anvil;
    }
    // Hammer and tongs ride on his body; the runtime puts them in his fists.
    const auto Prop = [&](UStaticMesh* Mesh, const TCHAR* Name)
    {
        auto* C = NewObject<UStaticMeshComponent>(NPC, Name);
        C->SetStaticMesh(Mesh);
        C->SetupAttachment(NPC->Body);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCanEverAffectNavigation(false);
        C->RegisterComponent();
        return C;
    };
    NPC->Hammer = Prop(NPC->SmithMeshes[1], TEXT("Hammer"));
    NPC->Tongs = Prop(NPC->SmithMeshes[2], TEXT("Tongs"));
    // The bar glows: the forge's ember material.
    if (UMaterialInterface* Ember = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_FireEmber.M_FireEmber")))
    {
        const int32 Hot = NPC->Tongs->GetMaterialIndex(TEXT("Hot"));
        if (Hot != INDEX_NONE) NPC->Tongs->SetMaterial(Hot, Ember);
    }
    // Sparks off each blow: a few glowing specks thrown out and falling.
    NPC->Sparks = NewObject<UInstancedStaticMeshComponent>(NPC, TEXT("Sparks"));
    NPC->Sparks->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    if (UMaterialInterface* Ember = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_FireEmber.M_FireEmber"))) NPC->Sparks->SetMaterial(0, Ember);
    NPC->Sparks->SetupAttachment(NPC->GetRootComponent());
    NPC->Sparks->SetUsingAbsoluteLocation(true); NPC->Sparks->SetUsingAbsoluteRotation(true);
    NPC->Sparks->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    NPC->Sparks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NPC->Sparks->SetCastShadow(false);
    NPC->Sparks->RegisterComponent();
    NPC->StrikeLight = NewObject<UPointLightComponent>(NPC, TEXT("StrikeLight"));
    NPC->StrikeLight->SetupAttachment(NPC->Body);
    NPC->StrikeLight->SetRelativeLocation(FVector(AnvilAhead, AnvilRight, AnvilFace + 15.f));
    NPC->StrikeLight->SetLightColor(FLinearColor(1.f, .55f, .18f));
    NPC->StrikeLight->SetAttenuationRadius(320.f);
    NPC->StrikeLight->SetIntensity(0.f);
    NPC->StrikeLight->SetCastShadows(false);
    NPC->StrikeLight->RegisterComponent();
    // The forge's sounds (Tools/gen_anvil_sfx.py).
    const auto Load = [](TArray<TObjectPtr<USoundBase>>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(Sound);
    };
    Load(NPC->StrikeSounds, TEXT("SFX_AnvilStrike"), 4); Load(NPC->TapSounds, TEXT("SFX_AnvilTap"), 4); Load(NPC->ClinkSounds, TEXT("SFX_TongsClink"), 3);
    // Heard across the plaza's corner, not the docks.
    NPC->StrikeAttenuation = NewObject<USoundAttenuation>(NPC);
    NPC->StrikeAttenuation->Attenuation.bAttenuate = true;
    NPC->StrikeAttenuation->Attenuation.bSpatialize = true;
    NPC->StrikeAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    NPC->StrikeAttenuation->Attenuation.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
    NPC->StrikeAttenuation->Attenuation.FalloffDistance = 1600.f;
    // The forge roars and crackles beside him, heard close by.
    if (USoundBase* Loop = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_ForgeLoop_00.SFX_ForgeLoop_00")))
    {
        auto* ForgeAttenuation = NewObject<USoundAttenuation>(NPC);
        ForgeAttenuation->Attenuation.bAttenuate = true;
        ForgeAttenuation->Attenuation.bSpatialize = true;
        ForgeAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        ForgeAttenuation->Attenuation.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
        ForgeAttenuation->Attenuation.FalloffDistance = 900.f;
        NPC->ForgeAudio = NewObject<UAudioComponent>(NPC, TEXT("ForgeAudio"));
        NPC->ForgeAudio->SetupAttachment(NPC->Body);
        NPC->ForgeAudio->SetRelativeLocation(ForgeAt);
        NPC->ForgeAudio->SetSound(Loop);
        NPC->ForgeAudio->AttenuationSettings = ForgeAttenuation;
        NPC->ForgeAudio->SetVolumeMultiplier(.45f);
        NPC->ForgeAudio->RegisterComponent();
        NPC->ForgeAudio->Play(FMath::FRandRange(0.f, 7.f));
    }
    NPC->ForgeClock = FMath::FRandRange(0.f, 3.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SMITH_SPAWNED anvil=%d hammer=%d tongs=%d sounds=%d"), NPC->Anvil.IsValid() ? 1 : 0,
        NPC->Hammer && NPC->Hammer->GetStaticMesh() ? 1 : 0, NPC->Tongs && NPC->Tongs->GetStaticMesh() ? 1 : 0,
        NPC->StrikeSounds.Num() + NPC->TapSounds.Num() + NPC->ClinkSounds.Num() + (NPC->ForgeAudio ? 1 : 0));
    return NPC;
}

ADockNPC* ADockNPC::SpawnDwarf(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Dwarf, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("Dwarf"));
    NPC->DisplayName = TEXT("Dwarf");
    NPC->Lines = { TEXT("Keep clear of the edge, rat."), TEXT("He's had my other axe a week. Slow work, iron.") };
    NPC->SetupVoice(TEXT("Dwarf"));   // user 2026-10-05: his ElevenLabs line replaces these
    NPC->GiveAxe(1);
    return NPC;
}

ADockNPC* ADockNPC::SpawnTavernKeeper(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::TavernKeeper, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("TavernKeeper"));
    NPC->DisplayName = TEXT("Tavern keeper");
    NPC->Lines = { TEXT("We don't serve rats."), TEXT("And stay out of my cellar.") };
    NPC->SetGrip(0, 1.f); NPC->SetGrip(1, .75f);   // the handle; the rag bunched in his right
    const auto Prop = [&](UStaticMesh* Mesh, const TCHAR* Name)
    {
        auto* C = NewObject<UStaticMeshComponent>(NPC, Name);
        C->SetStaticMesh(Mesh);
        C->SetupAttachment(NPC->Body);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCanEverAffectNavigation(false);
        C->RegisterComponent();
        return C;
    };
    NPC->Tankard = Prop(NPC->KeeperMeshes[0], TEXT("Tankard"));
    NPC->Rag = Prop(NPC->KeeperMeshes[1], TEXT("Rag"));
    NPC->PolishClock = FMath::FRandRange(0.f, 4.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_KEEPER_SPAWNED tankard=%d rag=%d at=%s"), NPC->Tankard->GetStaticMesh() ? 1 : 0, NPC->Rag->GetStaticMesh() ? 1 : 0, *Feet.ToString());
    return NPC;
}

ADockNPC* ADockNPC::SpawnElfElder(UWorld* World, const FVector& Hips, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::ElfElder, Hips, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("ElfElder"));
    NPC->DisplayName = TEXT("Old elf");
    NPC->SetupVoice(TEXT("ElfElder"));   // user 2026-10-06: her recorded line replaces the old text one
    return NPC;
}

ADockNPC* ADockNPC::SpawnAlchemist(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::GnomeAlchemist, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("Alchemist"));
    NPC->DisplayName = TEXT("Alchemist");
    NPC->SetupVoice(TEXT("GnomeAlchemist"));   // user 2026-10-06: his recorded line (the shipment from Athkatla) replaces the text ones
    return NPC;
}

ADockNPC* ADockNPC::SpawnSailor(UWorld* World, const FVector& Feet, float Yaw)
{
    auto* NPC = SpawnHuman(World, EDockHuman::Sailor, Feet, Yaw);
    if (!NPC) return nullptr;
    NPC->Tags.Add(TEXT("Sailor"));
    NPC->DisplayName = TEXT("Old sailor");
    NPC->Lines = { TEXT("Weather's turning."), TEXT("Seen bigger rats than you in a ship's bilge.") };
    // user 2026-10-06: his ElevenLabs line (Matthew Schmitz, "Old Pirate Captain") replaces these.
    NPC->SetGrip(1, .8f);   // the hand that holds the bowl
    NPC->Pipe = NewObject<UStaticMeshComponent>(NPC, TEXT("Pipe"));
    NPC->Pipe->SetStaticMesh(NPC->SailorMeshes[0]);
    NPC->Pipe->SetupAttachment(NPC->Body);
    NPC->Pipe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NPC->Pipe->SetCanEverAffectNavigation(false);
    NPC->Pipe->RegisterComponent();
    if (UMaterialInterface* Ember = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_FireEmber.M_FireEmber")))
    {
        const int32 Hot = NPC->Pipe->GetMaterialIndex(TEXT("Hot"));
        if (Hot != INDEX_NONE) NPC->Pipe->SetMaterial(Hot, Ember);
    }
    // A thin wisp rising from the bowl, upright whatever his head does.
    NPC->PipeWisp = NewObject<UStaticMeshComponent>(NPC, TEXT("PipeWisp"));
    NPC->PipeWisp->SetStaticMesh(NPC->SailorMeshes[1]);
    NPC->PipeWisp->SetupAttachment(NPC->Pipe);
    NPC->PipeWisp->SetRelativeLocation(PipeBowl);
    NPC->PipeWisp->SetUsingAbsoluteRotation(true); NPC->PipeWisp->SetUsingAbsoluteScale(true);
    NPC->PipeWisp->SetWorldScale3D(FVector(2.f));
    NPC->PipeWisp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NPC->PipeWisp->SetCastShadow(false);
    NPC->PipeWisp->RegisterComponent();
    // Breath: soft puffs on instanced spheres, each fading on its own (M_SmokePuff, custom data 0 = opacity), left where breathed.
    NPC->PipeBreath = NewObject<UInstancedStaticMeshComponent>(NPC, TEXT("PipeBreath"));
    NPC->PipeBreath->SetStaticMesh(NPC->SailorMeshes[2]);
    NPC->PipeBreath->SetMaterial(0, NPC->PuffMaterial);
    NPC->PipeBreath->NumCustomDataFloats = 1;
    NPC->PipeBreath->SetupAttachment(NPC->GetRootComponent());
    NPC->PipeBreath->SetUsingAbsoluteLocation(true); NPC->PipeBreath->SetUsingAbsoluteRotation(true); NPC->PipeBreath->SetUsingAbsoluteScale(true);
    NPC->PipeBreath->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    NPC->PipeBreath->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NPC->PipeBreath->SetCastShadow(false);
    NPC->PipeBreath->RegisterComponent();
    NPC->PipeClock = FMath::FRandRange(3.f, 8.f);
    NPC->SetupVoice(TEXT("Sailor"));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SAILOR_SPAWNED pipe=%d smoke=%d puff=%d at=%s"), NPC->SailorMeshes[0] ? 1 : 0, NPC->SailorMeshes[1] ? 1 : 0,
        NPC->PuffMaterial ? 1 : 0, *Feet.ToString());
    return NPC;
}

void ADockNPC::SpawnTownsfolk(UWorld* World)
{
    // The old sailor near the outer end of the court pier, off its centre
    // line (the route and the slide's climb-out), looking out over the water.
    SpawnSailor(World, FVector(2180, 3175, 0), 60.f);
    // The gnome alchemist before his shop's left window (DockPlaza.cpp: the
    // shop at 1280,-3940, its front at y -3760, the door at x 1222..1338),
    // clear of the door, facing out over the plaza.
    SpawnAlchemist(World, AlchemistFeet, AlchemistYaw);
    // The old elf on her bench on the bay side of the fountain (DockPlaza.cpp
    // builds it from ElfBench), facing the fountain.
    SpawnElfElder(World, ElfHips, ElfYaw);
    // The tavern keeper behind his counter (DockTavern.cpp: counter y 810..878,
    // bottle shelves from y 941), between the barrels (east edge x -119) and
    // the cellar hatch (lid at x 14), facing the room.
    SpawnTavernKeeper(World, FVector(-55, 905, 0), -90.f);
    // A dwarf waiting on the smith beside the smith's quenching barrel
    // (DockForge.cpp's tub at -1110,-3520: at his left front), turned toward
    // the anvil; his axe at his right hand, away from the barrel.
    SpawnDwarf(World, DwarfFeet, DwarfYaw);
    // The smith at his anvil in front of the smithy, the forge at his left
    // hand (DockPlaza.cpp), facing out over the plaza.
    SpawnBlacksmith(World, FVector(-950, -3664, 0), 90.f);
    // Two guards either side of the closed city gate (the plaza's far wall;
    // the opening runs x 60..460 between the towers), facing back down the
    // plaza: they keep the rat from the way into the city. Each holds the spear
    // on the outer side, so the pair mirror each other.
    if (ADockNPC* Guard = SpawnHuman(World, EDockHuman::Guard, FVector(105, -4085, 0), 90.f))
    {
        Guard->Tags.Add(TEXT("DockGuard"));
        Guard->DisplayName = TEXT("Guard");
        Guard->Lines = { TEXT("Stick to the docks, rat.") };
        Guard->GiveSpear(1);   // west of the gate: his right hand is the outer one
        Guard->SetupVoice(TEXT("Guard"));   // user 2026-10-05: his ElevenLabs line, "Move along, rat!"
    }
    if (ADockNPC* Guard = SpawnHuman(World, EDockHuman::GuardWoman, FVector(415, -4085, 0), 90.f))
    {
        Guard->Tags.Add(TEXT("DockGuardB"));
        Guard->DisplayName = TEXT("Guard");
        Guard->Lines = { TEXT("Stick to the docks, rat.") };
        Guard->GiveSpear(0);   // east of the gate: her left
        Guard->SetupVoice(TEXT("GuardWoman"));   // user 2026-10-05: her ElevenLabs line, the same words
    }
    // A third guard at the Dock Street side gate in the west wall, beside the
    // sewer hatch: south of the gate (between it and the bench), facing into
    // the court, clear of the hatch and its approach from the east.
    if (ADockNPC* Guard = SpawnHuman(World, EDockHuman::SideGuard, FVector(-1690, 3470, 0), 0.f))
    {
        Guard->Tags.Add(TEXT("DockGuardC"));
        Guard->DisplayName = TEXT("Guard");
        Guard->Lines = { TEXT("Stick to the docks, rat.") };
        Guard->GiveSpear(1);   // his right, toward the gate
        Guard->SetupVoice(TEXT("SideGuard"));   // user 2026-10-05: his ElevenLabs line, "Stick to the docks, rat!"
    }
    // The market woman at the end of the aisle between the red-canopied stalls.
    if (ADockNPC* Woman = SpawnHuman(World, EDockHuman::MarketWoman, FVector(148, -1240, 0), 180.f))
    {
        Woman->Tags.Add(TEXT("MarketWoman"));
        Woman->DisplayName = TEXT("Market woman");
        Woman->SetupVoice(TEXT("MarketWoman"));   // user 2026-10-06: her recorded line, "No handouts. If you're hungry, check the sewer for scraps."
    }
}

const TArray<TWeakObjectPtr<ADockNPC>>& ADockNPC::All()
{
    NPCRegistry.RemoveAll([](const TWeakObjectPtr<ADockNPC>& Entry) { return !Entry.IsValid(); });
    return NPCRegistry;
}

void ADockNPC::BeginPlay()
{
    Super::BeginPlay();
    NPCRegistry.Add(this);
    Phase = FMath::FRandRange(0.f, 30.f);
    NextGlance = FMath::FRandRange(1.f, 3.f);
    if (USkeletalMesh* Mesh = HumanMeshes[static_cast<int32>(Kind)]) Body->SetSkinnedAssetAndUpdate(Mesh);
    BoneIndex.Init(INDEX_NONE, BoneCount);
    Rest.Init(FQuat::Identity, BoneCount);
    if (const USkinnedAsset* Asset = Body->GetSkinnedAsset())
    {
        const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
        for (int32 I = 0; I < BoneCount; ++I) BoneIndex[I] = Ref.FindBoneIndex(BoneNames[I]);
        if (!BoneIndex.Contains(INDEX_NONE)) SolveRest();
        if (Kind == EDockHuman::ElfElder && !BoneIndex.Contains(INDEX_NONE))
        {
            bSeated = true; SeatTop = ElfBench.Z; SeatBone = SitBone;
            SetupSeat(Fountain);   // her glances go about the fountain
        }
        // Each finger joint's bending axis, from the model's pose (palms down,
        // fingers out): across the finger, bending it toward the palm.
        FingerBone.Init(INDEX_NONE, 30); FingerAxis.Init(FVector::ZeroVector, 30);
        TArray<FTransform> RefSpace;
        if (!BoneIndex.Contains(INDEX_NONE))
        {
            TArray<FQuat> None; None.Init(FQuat::Identity, BoneCount);
            Solve(None, RefSpace);
            HeadRefRotation = RefSpace[BoneIndex[Head]].GetRotation();
            HeadRef = RefSpace[BoneIndex[Head]];
        }
        // Face bones, on the NPCs built with them (Tools/build_npc_humans.py add_face_rig).
        const TCHAR* FaceNames[] = { TEXT("jaw"), TEXT("lid_upper_l"), TEXT("lid_upper_r"), TEXT("brow_l"), TEXT("brow_r") };
        for (int32 I = 0; I < 5; ++I) FaceBone[I] = Ref.FindBoneIndex(FaceNames[I]);
        NextBlink = FMath::FRandRange(1.5f, 4.f);
        for (int32 Side = 0; Side < 2 && RefSpace.Num(); ++Side)
            for (int32 F = 0; F < 5; ++F)
                for (int32 J = 0; J < 3; ++J)
                {
                    const int32 I = (Side * 5 + F) * 3 + J;
                    FingerBone[I] = Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%02d_%s"), FingerNames[F], J + 1, Side == 0 ? TEXT("l") : TEXT("r")));
                }
        if (RefSpace.Num() && !FingerBone.Contains(INDEX_NONE))
        {
            const FVector Palm = -FVector::UpVector;
            for (int32 I = 0; I < 30; ++I)
            {
                const int32 B = FingerBone[I], J = I % 3;
                const FVector Along = J < 2 ? RefSpace[FingerBone[I + 1]].GetLocation() - RefSpace[B].GetLocation()
                                            : RefSpace[B].GetLocation() - RefSpace[FingerBone[I - 1]].GetLocation();
                const FVector Axis = FVector::CrossProduct(Along.GetSafeNormal(), Palm).GetSafeNormal();
                FingerAxis[I] = RefSpace[B].GetRotation().UnrotateVector(Axis);
            }
        }
        else FingerBone.Reset();
    }
    if (IsBobert() && !BoneIndex.Contains(INDEX_NONE)) SolveSleep();
    HomeYaw = static_cast<float>(GetActorRotation().Yaw);
    ZHome = GetActorLocation();
    if (bSpear) PlaceSpear();   // given before play began: place it now the arms are known
    // The idle this person plays: the guard keeps looking about; the worker
    // and the market woman stand with weight on one leg, a hand to the hip
    // now and then (each from its own random point in the clip).
    IdleClip = (Kind == EDockHuman::Guard || Kind == EDockHuman::GuardWoman || Kind == EDockHuman::SideGuard || Kind == EDockHuman::Dwarf || IsSeated()) ? ClipStandLook : ClipStandHip;
    bool bClips = !BoneIndex.Contains(INDEX_NONE);
    for (const auto& Clip : Clips) bClips &= Clip && Clip->GetSkeleton();
    if (bClips)
    {
        // The clips are changes of rotation from the skeleton's own rest (the
        // dock worker's); mapped by bone name onto this body.
        const FReferenceSkeleton& Src = Clips[0]->GetSkeleton()->GetReferenceSkeleton();
        const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
        SourceRest.SetNum(Src.GetNum());
        for (int32 B = 0; B < Src.GetNum(); ++B)
        {
            const int32 P = Src.GetParentIndex(B);
            SourceRest[B] = P >= 0 ? SourceRest[P] * Src.GetRefBonePose()[B].GetRotation() : Src.GetRefBonePose()[B].GetRotation();
        }
        SkelIndex.Init(INDEX_NONE, Ref.GetNum());
        for (int32 B = 0; B < Ref.GetNum(); ++B) SkelIndex[B] = Src.FindBoneIndex(Ref.GetBoneName(B));
        // Hips in component space (above them is the rig's root bone).
        const auto CompAt = [](const FReferenceSkeleton& Skel, int32 Bone)
        {
            FTransform T = FTransform::Identity;
            for (int32 B = Bone; B != INDEX_NONE; B = Skel.GetParentIndex(B)) T = T * Skel.GetRefBonePose()[B];
            return T.GetLocation();
        };
        SourceHips = CompAt(Src, Src.FindBoneIndex(BoneNames[Pelvis]));
        HipScale = static_cast<float>(CompAt(Ref, BoneIndex[Pelvis]).Z / FMath::Max(1., SourceHips.Z));
        if (SkelIndex.Contains(INDEX_NONE))
        {
            FString Missing;
            for (int32 B = 0; B < Ref.GetNum(); ++B) if (SkelIndex[B] == INDEX_NONE) Missing += Ref.GetBoneName(B).ToString() + TEXT(" ");
            UE_LOG(LogTemp, Warning, TEXT("CHUCK_NPC_BONES %s: no clip track for %s(mesh root %s, %d bones; skeleton root %s, %d bones)"), *GetName(), *Missing,
                *Ref.GetBoneName(0).ToString(), Ref.GetNum(), *Src.GetBoneName(0).ToString(), Src.GetNum());
            SkelIndex.Reset();
        }
    }
}

void ADockNPC::SampleClips(float Time, TArray<FQuat>& BoneDelta, FVector& HipsOffset) const
{
    // Each clip's local bone rotations at this time, blended (idle -> talk),
    // turned into each bone's change of rotation from the skeleton's rest,
    // then into the extra turn beyond its parent's (what Solve applies).
    const FReferenceSkeleton& Src = Clips[0]->GetSkeleton()->GetReferenceSkeleton();
    const int32 N = Src.GetNum();
    const auto Sample = [&](const UAnimSequence* Clip, TArray<FTransform>& Local)
    {
        Local.SetNum(N);
        const FAnimExtractContext Context(static_cast<double>(FMath::Fmod(Time, FMath::Max(Clip->GetPlayLength(), .1f))));
        for (int32 B = 0; B < N; ++B) Clip->GetBoneTransform(Local[B], FSkeletonPoseBoneIndex(B), Context, false);
    };
    TArray<FTransform> Local, Other;
    const auto Blend = [&](float W)
    {
        for (int32 B = 0; B < N; ++B)
        {
            Local[B].SetRotation(FQuat::Slerp(Local[B].GetRotation(), Other[B].GetRotation(), W));
            Local[B].SetLocation(FMath::Lerp(Local[B].GetLocation(), Other[B].GetLocation(), static_cast<double>(W)));
        }
    };
    if (Kind == EDockHuman::Zombie)
    {
        // Its stooped sway, the shamble over it as it walks, then the collapse (held at the end).
        Sample(Clips[ClipZombieIdle], Local);
        if (WalkBlend > .01f) { Sample(Clips[ClipZombieWalk], Other); Blend(WalkBlend); }
        if (DeathTime >= 0.f)
        {
            const float Length = Clips[ClipZombieFall]->GetPlayLength();
            Other.SetNum(N);
            const FAnimExtractContext Context(static_cast<double>(FMath::Min(DeathTime * ZombieFallRate, Length)));
            for (int32 B = 0; B < N; ++B) Clips[ClipZombieFall]->GetBoneTransform(Other[B], FSkeletonPoseBoneIndex(B), Context, false);
            Blend(FMath::Clamp(DeathTime / .15f, 0.f, 1.f));
        }
    }
    else Sample(Clips[IdleClip], Local);
    if (TalkBlend > 0.f) { Sample(Clips[ClipTalk], Other); Blend(TalkBlend); }
    if (ReactTime >= 0.f)
    {
        // The reaction plays once over whatever he was doing, eased in and out.
        const float Length = Clips[ClipReact]->GetPlayLength();
        const float W = FMath::Clamp(FMath::Min(ReactTime / ReactIn, (Length - ReactTime) / ReactOut), 0.f, 1.f);
        Other.SetNum(N);
        {
            const FAnimExtractContext Context(static_cast<double>(FMath::Min(ReactTime, Length)));
            for (int32 B = 0; B < N; ++B) Clips[ClipReact]->GetBoneTransform(Other[B], FSkeletonPoseBoneIndex(B), Context, false);
        }
        Blend(W);
    }
    TArray<FQuat> Comp; Comp.SetNum(N);
    TArray<FQuat> World; World.SetNum(N);
    for (int32 B = 0; B < N; ++B)
    {
        const int32 P = Src.GetParentIndex(B);
        Comp[B] = P >= 0 ? Comp[P] * Local[B].GetRotation() : Local[B].GetRotation();
        World[B] = Comp[B] * SourceRest[B].Inverse();
    }
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    BoneDelta.Init(FQuat::Identity, Ref.GetNum());
    for (int32 B = 0; B < Ref.GetNum(); ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        BoneDelta[B] = P >= 0 ? World[SkelIndex[B]] * World[SkelIndex[P]].Inverse() : World[SkelIndex[B]];
    }
    // The face bones have no track in the clips (a missing track samples as identity, not the
    // bone's rest, which would twist the face): they keep their rest under the head, and PoseFace moves them.
    for (const int32 B : FaceBone) if (BoneDelta.IsValidIndex(B)) BoneDelta[B] = FQuat::Identity;
    FTransform Hips = FTransform::Identity;   // the clip's hips in component space
    for (int32 B = SkelIndex[BoneIndex[Pelvis]]; B != INDEX_NONE; B = Src.GetParentIndex(B)) Hips = Hips * Local[B];
    HipsOffset = (Hips.GetLocation() - SourceHips) * HipScale;
}

void ADockNPC::PoseHands(TArray<FTransform>& Space, bool bStraightenWrists) const
{
    // Wrists: the clips' wrist data is poor (hands bent flat against the
    // legs), so most of it gives way to a straight wrist. Then every finger
    // joint is set relative to its parent: relaxed, or closed in a grip.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    if (bStraightenWrists && HandLocal.Num())
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const int32 Hand = BoneIndex[Of(HandL, Side)], Fore = BoneIndex[Of(LowerL, Side)];
            const FQuat Straight = (HandLocal[Side] * Space[Fore]).GetRotation();
            Space[Hand].SetRotation(FQuat::Slerp(Space[Hand].GetRotation(), Straight, WristStraight));
        }
    if (FingerBone.Num() != 30) return;
    for (int32 I = 0; I < 30; ++I)
    {
        const int32 Side = I / 15, F = (I / 3) % 5, J = I % 3, B = FingerBone[I];
        const float Degrees = FMath::Lerp(Relaxed[F][J], Gripped[F][J], Grip[Side]);
        FTransform Local = RefPose[B];
        Local.SetRotation(Local.GetRotation() * FQuat(FingerAxis[I], FMath::DegreesToRadians(Degrees)));
        Space[B] = Local * Space[Ref.GetParentIndex(B)];
    }
}

void ADockNPC::HoldSpear(TArray<FTransform>& Space) const
{
    // Two-bone IK for the spear arm: the fist on the grip, the elbow back and
    // out, the knuckles across the shaft with the thumb up. Children follow
    // through their local transforms (the gripped fingers come along).
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Count = Space.Num();
    TArray<FTransform> Local; Local.SetNum(Count);
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        Local[B] = P >= 0 ? Space[B].GetRelativeTransform(Space[P]) : Space[B];
    }
    const auto Descends = [&](int32 B, int32 From) { for (; B != INDEX_NONE; B = Ref.GetParentIndex(B)) if (B == From) return true; return false; };
    const auto Rotate = [&](int32 Bone, const FQuat& Q)
    {
        Space[Bone].SetRotation(Q * Space[Bone].GetRotation());
        for (int32 B = Bone + 1; B < Count; ++B)
            if (Descends(B, Bone)) Space[B] = Local[B] * Space[Ref.GetParentIndex(B)];
    };
    const int32 Upper = BoneIndex[Of(UpperL, SpearSide)], Lower = BoneIndex[Of(LowerL, SpearSide)], Hand = BoneIndex[Of(HandL, SpearSide)];
    const int32 Thumb = BoneIndex[Of(ThumbL, SpearSide)], Middle = BoneIndex[Of(MiddleL, SpearSide)];
    const float Out = SpearSide == 1 ? -ArmOut : ArmOut;
    const FVector S = Space[Upper].GetLocation();
    const float A = static_cast<float>(FVector::Dist(S, Space[Lower].GetLocation()));
    const float Bl = static_cast<float>(FVector::Dist(Space[Lower].GetLocation(), Space[Hand].GetLocation()));
    // The hand runs across the shaft, pointing from his shoulder toward it.
    FVector HandDir = SpearGrip - S; HandDir.Z = 0.f; HandDir = HandDir.GetSafeNormal();
    if (HandDir.IsNearlyZero()) HandDir = FVector::ForwardVector;
    // The shaft runs through the middle of the fist: along the hand from the
    // wrist, and a little toward the palm (thumb up, the palm faces in).
    const FVector Wrist = SpearGrip - HandDir * FistReach + FVector(0.f, Out * PalmDepth, 0.f);
    FVector ToWrist = Wrist - S;
    const float D = FMath::Clamp(static_cast<float>(ToWrist.Size()), FMath::Abs(A - Bl) + 1.f, A + Bl - .5f);
    const FVector Along = ToWrist.GetSafeNormal();
    const float CosA = FMath::Clamp((A * A + D * D - Bl * Bl) / (2.f * A * D), -1.f, 1.f);
    const FVector Pole = FVector::VectorPlaneProject(FVector(-1.f, Out * .7f, -.2f), Along).GetSafeNormal();
    const FVector Elbow = S + Along * (A * CosA) + Pole * (A * FMath::Sqrt(1.f - CosA * CosA));
    Rotate(Upper, FQuat::FindBetweenNormals((Space[Lower].GetLocation() - S).GetSafeNormal(), (Elbow - S).GetSafeNormal()));
    Rotate(Lower, FQuat::FindBetweenNormals((Space[Hand].GetLocation() - Space[Lower].GetLocation()).GetSafeNormal(), (S + Along * D - Space[Lower].GetLocation()).GetSafeNormal()));
    Rotate(Hand, FQuat::FindBetweenNormals((Space[Middle].GetLocation() - Space[Hand].GetLocation()).GetSafeNormal(), HandDir));
    const FVector ThumbSide = FVector::VectorPlaneProject(Space[Thumb].GetLocation() - Space[Hand].GetLocation(), HandDir).GetSafeNormal();
    const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, HandDir).GetSafeNormal();
    Rotate(Hand, FQuat(HandDir, FMath::Atan2(static_cast<float>(FVector::DotProduct(HandDir, FVector::CrossProduct(ThumbSide, Up))),
        static_cast<float>(FVector::DotProduct(ThumbSide, Up)))));
}

float ADockNPC::GetSpearGripError() const
{
    if (!bSpear) return 1e3f;
    const FVector Wrist = Body->GetBoneLocation(BoneNames[Of(HandL, SpearSide)]), Knuckle = Body->GetBoneLocation(BoneNames[Of(MiddleL, SpearSide)]);
    const float Out = SpearSide == 1 ? -ArmOut : ArmOut;
    const FVector Fist = Wrist + (Knuckle - Wrist).GetSafeNormal() * FistReach
        - Body->GetComponentTransform().TransformVectorNoScale(FVector(0.f, Out * PalmDepth, 0.f));
    return static_cast<float>(FVector::Dist(Fist, Body->GetComponentTransform().TransformPosition(SpearGrip)));
}

float ADockNPC::GetAxeShoulderGap() const
{
    if (!HasAxe()) return 0.f;
    const FVector HeadAt = Spear->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 112.f));
    float Gap = 1e3f;
    for (int32 Side = 0; Side < 2; ++Side)
        Gap = FMath::Min(Gap, static_cast<float>(FVector::Dist2D(HeadAt, Body->GetBoneLocation(BoneNames[Of(UpperL, Side)]))));
    return Gap;
}

float ADockNPC::GetSpearLean() const
{
    return bSpear ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Spear->GetUpVector(), FVector::UpVector)), -1.f, 1.f))) : 90.f;
}

float ADockNPC::GetFingerCurl() const
{
    // The angle between each left finger's first and last joint directions.
    if (FingerBone.Num() != 30) return 0.f;
    float Sum = 0.f;
    for (int32 F = 1; F < 5; ++F)
    {
        const FName A = Body->GetBoneName(FingerBone[F * 3]), B = Body->GetBoneName(FingerBone[F * 3 + 1]), C = Body->GetBoneName(FingerBone[F * 3 + 2]);
        const FVector D1 = (Body->GetBoneLocation(B) - Body->GetBoneLocation(A)).GetSafeNormal();
        const FVector D2 = (Body->GetBoneLocation(C) - Body->GetBoneLocation(B)).GetSafeNormal();
        Sum += FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(D1, D2)), -1.f, 1.f)));
    }
    return Sum / 4.f;
}

void ADockNPC::TakeScratch(const FVector& From)
{
    if (Kind == EDockHuman::Zombie)
    {
        // It barely notices each one: a jolt, nothing that stops it coming.
        if (ZState == EZombie::Dead) return;
        ++Scratches; ++Hits;
        FlinchTime = 0.f;
        if (Hits >= ZombieHealth)
        {
            SetZombie(EZombie::Dead); DeathTime = 0.f;
            Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            return;
        }
        if (ZState == EZombie::Idle) SetZombie(EZombie::Shamble);
        return;
    }
    ++Scratches;
    if (ReactTime >= 0.f && ReactTime < .6f) return;   // already starting back
    ReactTime = 0.f;
    TurnHold = 2.f; bTurning = true;                    // and he turns to see what did it
}

float ADockNPC::GetBodyTurn() const
{
    return FMath::FindDeltaAngleDegrees(HomeYaw, static_cast<float>(GetActorRotation().Yaw));
}

void ADockNPC::UpdateTurn(float DeltaSeconds, float YawToChuck, bool bNear)
{
    // His head turns first; if the rat stays well off to the side (or he's
    // being talked to) he turns his body to face it, and back to his post
    // once it's gone.
    // Once he starts turning he carries on until he faces it.
    float Target = HomeYaw;
    const float Current = static_cast<float>(GetActorRotation().Yaw);
    if (bNear && (bTalking || FMath::Abs(YawToChuck) > 55.f)) TurnHold += DeltaSeconds; else if (!bTurning) TurnHold = 0.f;
    if (bNear && (bTalking || TurnHold > 1.2f)) bTurning = true;
    if (!bNear || FMath::Abs(YawToChuck) < 8.f) bTurning = false;
    if (bNear && (bTalking || bTurning)) Target = Current + YawToChuck;
    else if (bNear) Target = Current;                       // keep facing where he turned to while the rat's near
    const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, Target), -TurnRate * DeltaSeconds, TurnRate * DeltaSeconds);
    if (FMath::Abs(Step) > KINDA_SMALL_NUMBER) SetActorRotation(FRotator(0.f, Current + Step, 0.f));
}

void ADockNPC::Solve(const TArray<FQuat>& Delta, TArray<FTransform>& Space, const TArray<FQuat>* BoneDelta, const FVector& HipsOffset) const
{
    // Component space, parent first; each posed bone is turned by its delta
    // about its own joint, and its children follow.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    const int32 Count = Ref.GetNum();
    Space.SetNum(Count);
    TArray<int32> Which; Which.Init(INDEX_NONE, Count);
    for (int32 I = 0; I < BoneCount; ++I) Which[BoneIndex[I]] = I;
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 Parent = Ref.GetParentIndex(B);
        Space[B] = Parent >= 0 ? RefPose[B] * Space[Parent] : RefPose[B];
        if (B == BoneIndex[Pelvis]) Space[B].AddToTranslation(HipsOffset);
        if (BoneDelta) Space[B].SetRotation((*BoneDelta)[B] * Space[B].GetRotation());
        if (Which[B] != INDEX_NONE) Space[B].SetRotation(Delta[Which[B]] * Space[B].GetRotation());
    }
}

void ADockNPC::SolveRest()
{
    // Aim each part of the arm in turn from the model's A-pose, measuring the
    // posed skeleton after every step, so the pose holds for any body: upper
    // arm hanging just clear of the hip and a touch back, elbow softly bent,
    // palm turned to the thigh (thumb forward), wrist straight, fingers curled.
    TArray<FTransform> Space;
    const auto At = [&](EBone B) { return Space[BoneIndex[B]].GetLocation(); };
    const auto Dir = [&](EBone From, EBone To) { return (At(To) - At(From)).GetSafeNormal(); };
    const auto Turn = [&](EBone B, const FQuat& Q) { Rest[B] = Q * Rest[B]; Solve(Rest, Space); };
    Solve(Rest, Space);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const EBone Upper = Of(UpperL, Side), Lower = Of(LowerL, Side), Hand = Of(HandL, Side);
        const EBone Thumb = Of(ThumbL, Side), Middle = Of(MiddleL, Side);
        const float S = FMath::Sign(static_cast<float>(At(Upper).Y));   // which side of him this arm is on
        if (Side == 0) ArmOut = S;
        Turn(Upper, FQuat::FindBetweenNormals(Dir(Upper, Lower), FVector(-.03f, S * .13f, -1.f).GetSafeNormal()));
        Turn(Lower, FQuat::FindBetweenNormals(Dir(Lower, Hand), FVector(.11f, S * .03f, -1.f).GetSafeNormal()));
        // Twist the forearm about itself until the thumb points forward.
        const FVector Axis = Dir(Lower, Hand);
        const FVector ThumbSide = FVector::VectorPlaneProject(At(Thumb) - At(Hand), Axis).GetSafeNormal();
        const FVector Want = FVector::VectorPlaneProject(FVector(1.f, -S * .25f, 0.f), Axis).GetSafeNormal();
        Turn(Lower, FQuat(Axis, FMath::Atan2(static_cast<float>(FVector::DotProduct(Axis, FVector::CrossProduct(ThumbSide, Want))),
            static_cast<float>(FVector::DotProduct(ThumbSide, Want)))));
        Turn(Hand, FQuat::FindBetweenNormals(Dir(Hand, Middle), Dir(Lower, Hand)));   // a straight wrist
    }
    EyeHeight = static_cast<float>(At(Head).Z) + 9.f;   // the eyes, a hand above the skull's pivot
    // Keep the straight wrists to lay over the motion capture.
    HandLocal.SetNum(2);
    for (int32 Side = 0; Side < 2; ++Side)
        HandLocal[Side] = Space[BoneIndex[Of(HandL, Side)]].GetRelativeTransform(Space[BoneIndex[Of(LowerL, Side)]]);
}

float ADockNPC::GetWiderHandReach() const
{
    const FTransform& Actor = GetActorTransform();
    const auto Side = [&](EBone Hand) { return FMath::Abs(static_cast<float>(Actor.InverseTransformPosition(Body->GetBoneLocation(BoneNames[Hand])).Y)); };
    return FMath::Max(Side(HandL), Side(HandR));
}

float ADockNPC::GetStraightArmOut() const
{
    float Widest = 0.f;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector Shoulder = Body->GetBoneLocation(BoneNames[Of(UpperL, Side)]), Elbow = Body->GetBoneLocation(BoneNames[Of(LowerL, Side)]);
        const FVector Wrist = Body->GetBoneLocation(BoneNames[Of(HandL, Side)]);
        const float Bend = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct((Elbow - Shoulder).GetSafeNormal(), (Wrist - Elbow).GetSafeNormal())), -1.f, 1.f)));
        const float Out = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct((Elbow - Shoulder).GetSafeNormal(), -FVector::UpVector)), -1.f, 1.f)));
        if (Bend < 30.f) Widest = FMath::Max(Widest, Out);
    }
    return Widest;
}

float ADockNPC::GetHandsForward() const
{
    const FTransform& Actor = GetActorTransform();
    const auto Ahead = [&](EBone Hand) { return static_cast<float>(Actor.InverseTransformPosition(Body->GetBoneLocation(BoneNames[Hand])).X); };
    return FMath::Max(Ahead(HandL), Ahead(HandR));
}

void ADockNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ProbeFrames > 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_NPC_SHAKE who=%s %s"), Tags.Num() ? *Tags[0].ToString() : *DisplayName, *GetJitterReport());
    NPCRegistry.Remove(this);
    Super::EndPlay(Reason);
}

void ADockNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    if (Kind == EDockHuman::Zombie) { TickZombie(DeltaSeconds); UpdatePose(DeltaSeconds); return; }
    if (IsBobert()) { SleepClock += DeltaSeconds; UpdatePose(DeltaSeconds); return; }   // asleep: no looking about, no notice of the rat
    // Where he's looking: at Chuck when the rat's near, otherwise idle glances
    // (out over the harbour, down the quay) every few seconds.
    FVector2D Target = Glance;
    bWatching = false;
    if (const auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FVector Eye = GetActorLocation() + FVector(0, 0, EyeHeight - HalfHeight);
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Chuck->GetActorLocation() + FVector(0, 0, 10.f) - Eye);
        const float Across = static_cast<float>(Local.Size2D());
        if (Across < NoticeRange && !Chuck->IsAstral())   // not while he's away in the astral light
        {
            const float YawTo = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X)));
            const float PitchTo = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), Across));
            // Only if the rat's somewhere he can see without turning round.
            if (FMath::Abs(YawTo) < 110.f)
            {
                bWatching = true;
                const float Close = FMath::Clamp((CloseStart - Across) / (CloseStart - CloseFull), 0.f, 1.f);
                Target = FVector2D(FMath::Clamp(YawTo, -70.f, 70.f), FMath::Clamp(PitchTo, -20.f, FMath::Lerp(LookDownFar, 55.f, Close)));
            }
            if (HasMocap() && !IsSmith() && !IsKeeper() && !IsSeated()) UpdateTurn(DeltaSeconds, YawTo, true);   // the smith keeps to his anvil, the keeper behind his bar, the elf on her bench
        }
        else if (HasMocap() && !IsSmith() && !IsKeeper() && !IsSeated()) UpdateTurn(DeltaSeconds, 0.f, false);
        bTalking = Chuck->GetTalkingTo() == this;
    }
    if (NightStage > 0 && IsKeeper()) TickTavernNight(DeltaSeconds);
    TalkBlend = FMath::FInterpConstantTo(TalkBlend, bTalking ? 1.f : 0.f, DeltaSeconds, 2.5f);
    if (ReactTime >= 0.f && HasMocap() && (ReactTime += DeltaSeconds) > Clips[ClipReact]->GetPlayLength()) ReactTime = -1.f;
    if (!bWatching && (NextGlance -= DeltaSeconds) <= 0)
    {
        NextGlance = FMath::FRandRange(2.5f, 6.f);
        const float Spread = IsSeated() ? 18.f : 35.f;   // the old elf's eyes stay near the fountain
        Glance = GlanceCentre + FVector2D(FMath::FRandRange(-Spread, Spread), FMath::FRandRange(-6.f, 10.f));
    }
    if (IsSmith() && !bWatching) Target = FVector2D(0.f, 30.f - 16.f * Inspect);   // his eyes on the work
    if (IsSailor()) TickSailor(DeltaSeconds);
    if (IsKeeper())
    {
        PolishClock += DeltaSeconds;
        if (!bWatching) Target = FVector2D(-4.f, 30.f - 26.f * Inspect);   // on the tankard; up to it when he holds it to the light
    }
    if (bDrinker)
    {
        const float Before = FMath::Fmod(DrinkClock, DrinkPeriod);
        // While he rants, the tankard waits on the table (he drinks in the pauses).
        if (!(IsAmbientSpeaking() && Before >= DrinkDown)) DrinkClock += DeltaSeconds;
        const float U = FMath::Fmod(DrinkClock, DrinkPeriod);
        if (Before < DrinkRaise && U >= DrinkRaise) ++Drinks;
        // Drinking, he looks along the tankard, his head going back as it tips.
        if (DrinkLift > 0.f) Target = FMath::Lerp(Target, FVector2D(0.f, -DrinkHeadBack * FMath::SmoothStep(DrinkRaise, DrinkLower, U)), DrinkLift);
    }
    if (Kind == EDockHuman::Dwarf) Target = FVector2D(FMath::Clamp(Target.X, -DwarfLookYaw, DwarfLookYaw), FMath::Min(Target.Y, 0.f));
    const float Rate = bWatching ? 4.f : 2.f;
    Look.X = FMath::FInterpTo(Look.X, Target.X, DeltaSeconds, Rate);
    Look.Y = FMath::FInterpTo(Look.Y, Target.Y, DeltaSeconds, Rate);
    if (IsSmith()) TickSmith(DeltaSeconds);
    TickVoice(DeltaSeconds);
    UpdatePose(DeltaSeconds);
}

int32 ADockNPC::SetupVoice(const TCHAR* Npc)
{
    VoiceLines.Reset(); VoiceSounds.Reset();
    TArray<FString> Text;
    for (int32 I = 0; I < NPCVoiceData::LineCount; ++I)
    {
        const NPCVoiceData::FLine& L = NPCVoiceData::Lines[I];
        if (FCString::Strcmp(L.Npc, Npc) != 0 || !FString(L.Id).StartsWith(TEXT("talk_"))) continue;   // ambient lines: StartAmbient
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, L.Sound);
        if (!Sound) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_NPC_VOICE_MISSING %s %s"), Npc, L.Id); continue; }
        VoiceLines.Add(I); VoiceSounds.Add(Sound); Text.Add(L.Text);
    }
    if (Text.Num()) Lines = Text;
    TalkVoiceCount = VoiceLines.Num();
    if (VoiceSounds.Num()) EnsureVoiceAudio();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_NPC_VOICE %s lines=%d sounds=%d"), Npc, VoiceLines.Num(), VoiceSounds.Num());
    return VoiceLines.Num();
}

void ADockNPC::EnsureVoiceAudio()
{
    if (!VoiceAudio)
    {
        // From his mouth: heard across this corner of the plaza, clearly within a few metres.
        auto* Attenuation = NewObject<USoundAttenuation>(this);
        Attenuation->Attenuation.bAttenuate = true;
        Attenuation->Attenuation.bSpatialize = true;
        Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        Attenuation->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
        Attenuation->Attenuation.FalloffDistance = 1400.f;
        VoiceAudio = NewObject<UAudioComponent>(this, TEXT("VoiceAudio"));
        VoiceAudio->SetupAttachment(Body);
        VoiceAudio->SetRelativeLocation(FVector(10.f, 0.f, 120.f));
        VoiceAudio->AttenuationSettings = Attenuation;
        VoiceAudio->bAutoActivate = false;
        VoiceAudio->RegisterComponent();
    }
}

void ADockNPC::RequestVoiceLine(int32 Index)
{
    if (IsSmith() && VoiceSounds.IsValidIndex(Index)) { PendingVoice = Index; RestedFor = 0.f; SpeechRestLowest = 1.f; return; }
    StartVoiceLine(Index);
}

bool ADockNPC::StartVoiceLine(int32 Index)
{
    if (!VoiceSounds.IsValidIndex(Index) || !VoiceAudio) return false;
    VoiceLine = Index; VoiceTime = 0.f; MaxJawOpen = 0.f; bAmbientVoice = false;
    // A line whose audio isn't supplied yet (no sound) still runs, for its length, on its subtitles.
    if (VoiceSounds[Index]) { VoiceAudio->SetSound(VoiceSounds[Index]); VoiceAudio->Play(); }
    if (BlinkTime < 0.f) { BlinkTime = 0.f; ++Blinks; }   // a blink as he starts
    return true;
}

int32 ADockNPC::GetFaceBoneCount() const
{
    int32 N = 0;
    for (const int32 B : FaceBone) N += B != INDEX_NONE;
    return N;
}

float ADockNPC::VoiceLoudness() const
{
    if (VoiceTime < 0.f || !VoiceLines.IsValidIndex(VoiceLine)) return 0.f;
    const NPCVoiceData::FLine& L = NPCVoiceData::Lines[VoiceLines[VoiceLine]];
    if (L.Frames < 2) return 0.f;   // text only: no envelope yet
    const float F = VoiceTime * NPCVoiceData::EnvelopeRate;
    const int32 A = FMath::Clamp(FMath::FloorToInt(F), 0, L.Frames - 1), B = FMath::Min(A + 1, L.Frames - 1);
    return FMath::Lerp(static_cast<float>(L.Envelope[A]), static_cast<float>(L.Envelope[B]), F - FMath::FloorToFloat(F)) / 255.f;
}

void ADockNPC::TickVoice(float DeltaSeconds)
{
    // The line Chuck is on: each new one is spoken; leaving the conversation lets it trail off.
    if (TalkVoiceCount)
    {
        const auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
        const int32 Line = bTalking && Chuck && Chuck->GetTalkLine() < TalkVoiceCount ? Chuck->GetTalkLine() : -1;
        if (Line != HeardLine)
        {
            HeardLine = Line;
            if (Line >= 0) RequestVoiceLine(Line);
            else
            {
                PendingVoice = -1;
                if (IsSpeaking() && VoiceAudio) { VoiceAudio->FadeOut(.3f, 0.f); VoiceTime = -1.f; }
            }
        }
    }
    // The smith: hammer down first, a moment's pause, then he speaks.
    if (PendingVoice >= 0)
    {
        if (Resting > .98f) RestedFor += DeltaSeconds;
        if (RestedFor >= SmithSpeakPause) { PauseBeforeSpeech = RestedFor; const int32 Line = PendingVoice; PendingVoice = -1; StartVoiceLine(Line); }
    }
    const bool bWasSpeaking = IsSpeaking();
    if (IsSmith() && bWasSpeaking) SpeechRestLowest = FMath::Min(SpeechRestLowest, Resting);
    if (VoiceTime >= 0.f && VoiceLines.IsValidIndex(VoiceLine)
        && (VoiceTime += DeltaSeconds) > NPCVoiceData::Lines[VoiceLines[VoiceLine]].Seconds + .15f)
    {
        VoiceTime = -1.f;
        // Dougmund ends his rant: a drink in the pause before he starts again.
        if (bAmbientVoice && bDrinker && AmbientLine >= 0) DrinkIn(.3f);
    }
    if (bWasSpeaking && !IsSpeaking() && IsSmith()) StrikesAtSpeechEnd = Strikes;   // back to work from here
    // Ambient speech: stopped (cut off), or started again after its pause.
    if (AmbientStopIn >= 0.f && (AmbientStopIn -= DeltaSeconds) < 0.f)
    {
        AmbientLine = -1; AmbientStopIn = -1.f;
        if (IsAmbientSpeaking()) { if (VoiceAudio) VoiceAudio->FadeOut(AmbientFade, 0.f); VoiceTime = -1.f; }
    }
    if (AmbientLine >= 0 && !IsSpeaking() && (AmbientWait -= DeltaSeconds) <= 0.f)
    {
        StartVoiceLine(AmbientLine);
        bAmbientVoice = true; ++AmbientPlays;
        AmbientWait = AmbientPause;
        if (!bAmbientLoop) AmbientLine = -1;
    }
    // The jaw follows the voice's loudness (quick to open, a little slower to close), the brows lift on its peaks.
    const float Loud = VoiceLoudness();
    const float Want = (Kind == EDockHuman::Dwarf ? 9.f : 7.f) * FMath::Pow(Loud, .8f);   // his under a beard; lips that show open less
    JawOpen = FMath::FInterpTo(JawOpen, Want, DeltaSeconds, Want > JawOpen ? 28.f : 16.f);
    MaxJawOpen = FMath::Max(MaxJawOpen, JawOpen);
    BrowLift = FMath::FInterpTo(BrowLift, .55f * FMath::Clamp((Loud - .7f) / .3f, 0.f, 1.f), DeltaSeconds, 8.f);
    // Blinks every few seconds: lids down in 70 ms, held briefly, up in 110 ms.
    if (BlinkTime < 0.f && (NextBlink -= DeltaSeconds) <= 0.f) { BlinkTime = 0.f; ++Blinks; }
    if (BlinkTime >= 0.f)
    {
        BlinkTime += DeltaSeconds;
        Blink = BlinkTime < .07f ? BlinkTime / .07f : BlinkTime < .1f ? 1.f : FMath::Max(0.f, 1.f - (BlinkTime - .1f) / .11f);
        if (BlinkTime > .21f) { BlinkTime = -1.f; Blink = 0.f; NextBlink = FMath::FRandRange(2.f, 6.f); }
    }
}

void ADockNPC::PoseFace(TArray<FTransform>& Space) const
{
    // Each face bone is turned (jaw, lids) or lifted (brows) about the head's own
    // axes as they are now: across his head (+ tips forward and down), and up.
    if (FaceBone[0] == INDEX_NONE || !Space.IsValidIndex(BoneIndex[Head])) return;
    const FQuat HeadNow = Space[BoneIndex[Head]].GetRotation() * HeadRefRotation.Inverse();
    const FVector Across = HeadNow.RotateVector(FVector::YAxisVector), Up = HeadNow.RotateVector(FVector::ZAxisVector);
    const auto Turn = [&](int32 B, float Degrees)
    {
        if (B != INDEX_NONE && Space.IsValidIndex(B)) Space[B].SetRotation(FQuat(Across, FMath::DegreesToRadians(Degrees)) * Space[B].GetRotation());
    };
    Turn(FaceBone[0], JawOpen);
    Turn(FaceBone[1], 38.f * Blink); Turn(FaceBone[2], 38.f * Blink);
    for (int32 I = 3; I < 5; ++I)
        if (FaceBone[I] != INDEX_NONE && Space.IsValidIndex(FaceBone[I])) Space[FaceBone[I]].AddToTranslation(Up * BrowLift);
}

void ADockNPC::UpdatePose(float DeltaSeconds)
{
    if (!Body->GetSkinnedAsset() || BoneIndex.Contains(INDEX_NONE)) return;
    if (IsBobert())
    {
        if (Sleep.Num() != BoneCount) return;
        TArray<FTransform> Space;
        PoseBobert(Space);
        const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
        for (int32 B = 0; B < Space.Num(); ++B) Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
        return;
    }
    const float T = Clock + Phase;
    if (HasMocap())
    {
        // Motion capture drives the body, neck and head carriage included (the
        // actors' necks balance their chests); where he looks is turned on top,
        // and the hands keep their curl.
        TArray<FQuat> BoneDelta; FVector HipsOffset;
        SampleClips(T, BoneDelta, HipsOffset);
        // The motion capture's own jitter (user 2026-10-06: the dwarf's "weird jittery shake"; every
        // mocap NPC's feet and calves reversed direction on 10-26% of frames): each bone's turn and
        // the hips eased toward the clip with a short time constant, which keeps the sway and glances.
        if (SmoothDelta.Num() != BoneDelta.Num()) { SmoothDelta = BoneDelta; SmoothHips = HipsOffset; }
        else
        {
            const float A = 1.f - FMath::Exp(-DeltaSeconds / ClipSmoothing);
            for (int32 B = 0; B < BoneDelta.Num(); ++B) SmoothDelta[B] = FQuat::Slerp(SmoothDelta[B], BoneDelta[B], A).GetNormalized();
            SmoothHips = FMath::Lerp(SmoothHips, HipsOffset, static_cast<double>(A));
        }
        BoneDelta = SmoothDelta; HipsOffset = SmoothHips;
        // The clip actors were slighter at the hip: ease each arm out a little
        // so the hands rest beside the thighs, not in them.
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float S = Side == 0 ? ArmOut : -ArmOut;
            BoneDelta[BoneIndex[Of(UpperL, Side)]] = Roll(S * ArmClear) * BoneDelta[BoneIndex[Of(UpperL, Side)]];
        }
        if (Kind == EDockHuman::Dwarf)
            for (const int32 B : { BoneIndex[Neck], BoneIndex[Head] }) BoneDelta[B] = FQuat::Slerp(FQuat::Identity, BoneDelta[B], DwarfClipHead);
        TArray<FQuat> Delta; Delta.Init(FQuat::Identity, BoneCount);
        Delta[Neck] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
        Delta[Head] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
        if (Kind == EDockHuman::Zombie)
        {
            // Over the clips: rearing up with the arms lifting (the tell), then
            // down and forward at the rat, arms reaching; a jolt when scratched.
            const float Alive = DeathTime < 0.f ? 1.f : FMath::Clamp(1.f - DeathTime / .2f, 0.f, 1.f);
            const float R = Rear * Alive, L = Reach * Alive, F = Flinch * Alive;
            // A deeper stoop than the clip's old man, the head hung forward.
            Delta[Spine2] = Pitch(5.f * Alive - 6.f * R + 16.f * L - 8.f * F);
            Delta[Chest] = Pitch(6.f * Alive - 8.f * R + 14.f * L - 6.f * F) * Yaw(9.f * F);
            Delta[Neck] = Pitch(8.f * Alive) * Delta[Neck];
            Delta[Head] = Pitch(-6.f * F) * Delta[Head];
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const float S = Side == 0 ? ArmOut : -ArmOut;
                Delta[Of(UpperL, Side)] = Pitch(-55.f * R - 35.f * L) * Roll(S * 8.f * R);
                Delta[Of(LowerL, Side)] = Pitch(-25.f * R + 10.f * L);
            }
        }
        if (IsSmith())
        {
            // Planted at the anvil: the idle's sway halved (his arms are the IK's anyway).
            for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, .5f);
            HipsOffset *= .5f;
            // Bent over the work, the chest turning to his right as the hammer goes
            // up, driving down into the blow, a small recoil and nod as it lands;
            // straighter when he lifts the bar to look at it.
            const float Recoil = FMath::Exp(-SinceBlow / .12f);
            Delta[Spine2] = Pitch(13.f - 6.f * Swing + 5.f * Drive - 2.f * Recoil - 6.f * Inspect) * Yaw(5.f * Swing - 2.f * Recoil);
            Delta[Chest] = Pitch(5.f - 3.f * Swing + 3.f * Drive - 3.f * Inspect) * Yaw(5.f * Swing - 2.f);
            Delta[Head] = Pitch(3.f * Recoil) * Delta[Head];
        }
        if (IsKeeper())
        {
            // Settled behind his bar, a little over the work; straighter as he holds it up.
            for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, .5f);
            HipsOffset *= .5f;
            Delta[Spine2] = Pitch(7.f - 5.f * Inspect);
            Delta[Chest] = Pitch(3.f - 2.f * Inspect);
        }
        if (IsAlchemist())
        {
            // Standing at his shop: the idle's sway halved (his arms are the IK's).
            for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, .5f);
            HipsOffset *= .5f;
        }
        if (IsSeated())
        {
            // Sitting: the idle much quieter (it was stood), her hips down on the
            // bench, the pelvis rocked back a little and the back rounded over it.
            for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, .35f);
            HipsOffset = HipsOffset * .25f + FVector(0.f, 0.f, SeatDrop);
            Delta[Pelvis] = Pitch(-7.f);
            Delta[Spine1] = Pitch(4.f);
            Delta[Spine2] = Pitch(5.f);
            Delta[Chest] = Pitch(4.f);
            Delta[Neck] = Pitch(-4.f) * Delta[Neck];
        }
        TArray<FTransform> Space;
        Solve(Delta, Space, &BoneDelta, HipsOffset);
        PoseHands(Space, true);
        if (bSpear) HoldSpear(Space);
        if (IsSmith()) PoseSmith(Space);
        if (IsKeeper()) PoseKeeper(Space);
        if (IsSailor()) PoseSailor(Space);
        if (IsSeated()) PoseSeated(Space);
        if (bDrinker) PoseDrink(Space);
        if (IsAlchemist()) PoseSleeves(Space);
        PoseFace(Space);
        ProbeShake(Space, DeltaSeconds);
        const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
        for (int32 B = 0; B < Space.Num(); ++B) Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
        SmithEvents();
        return;
    }
    const float Breath = FMath::Sin(T * UE_TWO_PI / 4.2f);          // one slow breath every 4.2 s
    const float Shift = FMath::Sin(T * UE_TWO_PI / 11.f);           // weight moving between the feet
    TArray<FQuat> Delta = Rest;                                     // the standing pose, then the life on top
    Delta[Pelvis] = Roll(1.4f * Shift) * Yaw(1.5f * Shift);
    Delta[Spine1] = Roll(-.8f * Shift);
    Delta[Spine2] = Pitch(-.6f * Breath);
    Delta[Chest] = Pitch(-1.1f * Breath) * Roll(-.6f * Shift);
    // The look is shared between the neck and the head.
    Delta[Neck] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y);
    Delta[Head] = Yaw(.5f * Look.X) * Pitch(.5f * Look.Y - .5f * Breath);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float S = Side == 0 ? -1.f : 1.f;   // L is the left (-Y in Unreal)
        const float Sway = FMath::Sin(T * UE_TWO_PI / 5.3f + Side * 1.7f);
        Delta[Of(ClavL, Side)] = Roll(S * .8f * Breath);
        Delta[Of(UpperL, Side)] = Pitch(-1.5f * Sway) * Rest[Of(UpperL, Side)];   // a small swing from the shoulder
    }
    TArray<FTransform> Space;
    Solve(Delta, Space, nullptr, FVector(0.f, 0.f, SeatDrop));
    PoseHands(Space, false);
    if (bSpear) HoldSpear(Space);
    if (IsSmith()) PoseSmith(Space);
    if (IsKeeper()) PoseKeeper(Space);
    if (IsSailor()) PoseSailor(Space);
    if (IsSeated()) PoseSeated(Space);
    if (bDrinker) PoseDrink(Space);
    if (IsAlchemist()) PoseSleeves(Space);
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    for (int32 B = 0; B < Space.Num(); ++B) Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
    SmithEvents();
}

void ADockNPC::HandFrame(const TArray<FTransform>& Space, int32 Side, FVector& Fist, FVector& Along, FVector& Thumb) const
{
    // Along: wrist to the middle knuckle. The palm is where the curled middle
    // finger's tip lies; the thumb axis is square to both, on the thumb's side.
    const FVector Hand = Space[BoneIndex[Of(HandL, Side)]].GetLocation();
    Along = (Space[BoneIndex[Of(MiddleL, Side)]].GetLocation() - Hand).GetSafeNormal();
    const FVector ThumbBone = FVector::VectorPlaneProject(Space[BoneIndex[Of(ThumbL, Side)]].GetLocation() - Hand, Along);
    FVector Palm = FVector::ZeroVector;
    if (FingerBone.Num() == 30)
    {
        const int32 Base = (Side * 5 + 2) * 3;   // the middle finger
        Palm = FVector::VectorPlaneProject(Space[FingerBone[Base + 2]].GetLocation() - Space[FingerBone[Base]].GetLocation(), Along).GetSafeNormal();
    }
    if (Palm.IsNearlyZero())
    {
        Thumb = ThumbBone.GetSafeNormal();
        Fist = Hand + Along * FistReach;
        return;
    }
    Thumb = FVector::CrossProduct(Palm, Along).GetSafeNormal();
    if (FVector::DotProduct(Thumb, ThumbBone) < 0.f) Thumb = -Thumb;
    Fist = Hand + Along * FistReach + Palm * PalmDepth;
}

void ADockNPC::PlaceHand(TArray<FTransform>& Space, int32 Side, const FVector& Fist, const FVector& Along, const FVector& Thumb, const FVector& Pole)
{
    // As HoldSpear: aim the upper arm and forearm so the wrist lands where the
    // fist needs it (two-bone IK, the elbow toward Pole), then turn the hand
    // onto Along and twist it about Along until its thumb axis is Thumb.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Count = Space.Num();
    TArray<FTransform> Local; Local.SetNum(Count);
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        Local[B] = P >= 0 ? Space[B].GetRelativeTransform(Space[P]) : Space[B];
    }
    const auto Descends = [&](int32 B, int32 From) { for (; B != INDEX_NONE; B = Ref.GetParentIndex(B)) if (B == From) return true; return false; };
    const auto Rotate = [&](int32 Bone, const FQuat& Q)
    {
        Space[Bone].SetRotation(Q * Space[Bone].GetRotation());
        for (int32 B = Bone + 1; B < Count; ++B)
            if (Descends(B, Bone)) Space[B] = Local[B] * Space[Ref.GetParentIndex(B)];
    };
    const int32 Upper = BoneIndex[Of(UpperL, Side)], Lower = BoneIndex[Of(LowerL, Side)], Hand = BoneIndex[Of(HandL, Side)];
    const FVector Palm = FVector::CrossProduct(Along, Thumb).GetSafeNormal() * PalmSign[Side];
    const FVector Wrist = Fist - Along * FistReach - Palm * PalmDepth;
    const FVector S = Space[Upper].GetLocation();
    const float A = static_cast<float>(FVector::Dist(S, Space[Lower].GetLocation()));
    const float Bl = static_cast<float>(FVector::Dist(Space[Lower].GetLocation(), Space[Hand].GetLocation()));
    const FVector ToWrist = Wrist - S;
    const float D = FMath::Clamp(static_cast<float>(ToWrist.Size()), FMath::Abs(A - Bl) + 1.f, A + Bl - .5f);
    const FVector Dir = ToWrist.GetSafeNormal();
    const float CosA = FMath::Clamp((A * A + D * D - Bl * Bl) / (2.f * A * D), -1.f, 1.f);
    const FVector Bend = FVector::VectorPlaneProject(Pole, Dir).GetSafeNormal();
    const FVector Elbow = S + Dir * (A * CosA) + Bend * (A * FMath::Sqrt(1.f - CosA * CosA));
    Rotate(Upper, FQuat::FindBetweenNormals((Space[Lower].GetLocation() - S).GetSafeNormal(), (Elbow - S).GetSafeNormal()));
    Rotate(Lower, FQuat::FindBetweenNormals((Space[Hand].GetLocation() - Space[Lower].GetLocation()).GetSafeNormal(), (S + Dir * D - Space[Lower].GetLocation()).GetSafeNormal()));
    FVector F, Al, Th;
    HandFrame(Space, Side, F, Al, Th);
    Rotate(Hand, FQuat::FindBetweenNormals(Al, Along));
    HandFrame(Space, Side, F, Al, Th);
    const FVector Want = FVector::VectorPlaneProject(Thumb, Along).GetSafeNormal();
    Rotate(Hand, FQuat(Along, FMath::Atan2(static_cast<float>(FVector::DotProduct(Along, FVector::CrossProduct(Th, Want))), static_cast<float>(FVector::DotProduct(Th, Want)))));
    // Which side of Along x Thumb this hand's palm is (a property of the skeleton: measured, not assumed).
    if (FingerBone.Num() == 30)
    {
        const int32 Base = (Side * 5 + 2) * 3;
        const FVector Curl = Space[FingerBone[Base + 2]].GetLocation() - Space[FingerBone[Base]].GetLocation();
        PalmSign[Side] = FVector::DotProduct(Curl, FVector::CrossProduct(Along, Thumb)) >= 0.f ? 1.f : -1.f;
    }
}

void ADockNPC::TickSmith(float DeltaSeconds)
{
    // Talked to, or startled by a scratch: he rests the hammer on the anvil until it's over.
    const bool bPause = bTalking || ReactTime >= 0.f || PendingVoice >= 0 || IsSpeaking();   // and until his line is done
    Resting = FMath::FInterpConstantTo(Resting, bPause ? 1.f : 0.f, DeltaSeconds, 2.5f);
    const float Before = ForgeClock;
    if (Resting < .5f) ForgeClock += DeltaSeconds;
    // A set's blows: cycle K ends with blow K + 1 (K = 0 starts from rest);
    // the cycles after blows 2, 4 and 6 open with a tap.
    const auto Tapping = [](int32 K) { return K >= 2 && K % 2 == 0 && K < StrikesPerSet; };
    const auto Lift = [&](int32 K) { return K == 0 ? FirstLift : Tapping(K) ? TapLead + TapLift : LiftTime; };
    const auto Length = [&](int32 K) { return Lift(K) + TurnTime + DownTime; };
    float Window = 0.f;
    for (int32 K = 0; K < StrikesPerSet; ++K) Window += Length(K);
    // Which cycle a time in the window falls in (-1 past the last blow), and how far into it.
    const auto CycleOf = [&](float T, float& C)
    {
        for (int32 K = 0; K < StrikesPerSet; ++K) { if (T < Length(K)) { C = T; return K; } T -= Length(K); }
        C = T; return -1;
    };
    // Three sets make a round: two with a short pause to turn the work, the
    // third with a longer one, lifting the bar up to look at it.
    const float Round = 3.f * Window + 2.f * SetPause + InspectPause;
    const auto Where = [&](float Clock, int32& Set, float& Local, float& Pause)
    {
        const int32 R = FMath::FloorToInt(Clock / Round);
        float T = Clock - R * Round;
        for (int32 I = 0; I < 3; ++I)
        {
            Pause = I == 2 ? InspectPause : SetPause;
            if (T < Window + Pause || I == 2) { Set = R * 3 + I; Local = T; return; }
            T -= Window + Pause;
        }
    };
    int32 Set = 0, SetBefore = 0; float Local = 0, Previous = 0, Pause = SetPause, PauseBefore = SetPause;
    Where(ForgeClock, Set, Local, Pause);
    Where(Before, SetBefore, Previous, PauseBefore);
    if (SetBefore != Set) Previous = -1.f;   // a new set began this frame: no event carried over
    const float Turned = (Set % 2) ? 8.f : -8.f;   // each set the bar lies turned a little from the last
    float S = RestSwing, R = -1.f, TapW = 0.f, Push = 0.f, Peer = 0.f, Pull = 0.f;
    BarLift = 0.f; BarRoll = Turned;
    const auto Ease = [](float A, float B, float X) { return FMath::SmoothStep(A, B, X); };
    const auto Out = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - (1.f - X) * (1.f - X); };   // fast, then slowing
    float C = 0.f, PreviousC = 0.f;
    const int32 K = Local < Window ? CycleOf(Local, C) : -1;
    const int32 PreviousK = Previous >= 0.f && Previous < Window ? CycleOf(Previous, PreviousC) : -2;
    if (K >= 0)
    {
        // How high this blow goes: no two quite alike.
        const float H = .8f + .2f * FMath::Frac(FMath::Sin((Set * 17 + K) * 12.9898f) * 43758.5453f);
        const float L = Lift(K);
        if (C >= L + TurnTime)
        {
            // The blow: the arm gathers speed; the head lags a touch, then the wrist brings it through.
            const float U = (C - L - TurnTime) / DownTime;
            S = H * (1.f - U * U); R = H * (1.f - FMath::Pow(U, 2.4f)); Push = U * U;
        }
        else if (C >= L) S = H;   // turning over at the top, no hold
        else if (K == 0)
        {
            // From rest on the heel, up into the first blow.
            S = FMath::Lerp(RestSwing, H, Ease(0.f, L, C)); R = FMath::Pow(S, 1.2f);
            TapW = 1.f - Ease(0.f, .6f * L, C);
        }
        else if (Tapping(K) && C < TapLead)
        {
            // A small rebound drifting to the heel, the light drop (leaning into it) and its bounce.
            if (C < .08f) { S = .18f * FMath::Sin(HALF_PI * C / .08f); TapW = Ease(0.f, .08f, C); }
            else if (C < TapAt) { const float V = (C - .08f) / (TapAt - .08f); S = .18f * (1.f - V * V); TapW = 1.f; Push = V * V; }
            else { S = .1f * FMath::Sin(HALF_PI * (C - TapAt) / (TapLead - TapAt)); TapW = 1.f; Push = 1.f - Ease(TapAt, TapLead, C); }
        }
        else if (Tapping(K))
        {
            const float X = (C - TapLead) / TapLift;
            S = FMath::Lerp(.1f, H, Out(X)); R = FMath::Pow(S, 1.2f); TapW = 1.f - Ease(0.f, .5f, X);
        }
        else { S = H * Out(C / L); R = FMath::Pow(S, 1.2f); }   // bouncing off the work straight into the lift
        if (Tapping(K) && C >= TapAt && (PreviousK != K || PreviousC < TapAt) && Previous <= Local && Resting < .1f) bTapDue = true;
    }
    else
    {
        // The pause: the hammer bounces off the last blow and is set down on the heel.
        const float P = Local - Window, U = P / Pause;
        S = P < .12f ? .2f * Ease(0.f, .12f, P) : FMath::Lerp(.2f, .03f, Ease(.12f, .4f, P));
        TapW = Ease(0.f, .3f, P);
        if (Pause > SetPause)
        {
            // Every third pause he lifts the bar up and draws it back to look at it, turning it.
            Peer = Ease(.12f, .3f, U) * (1.f - Ease(.7f, .88f, U));
            BarLift = 20.f * Peer; Pull = 12.f * Peer;
            BarRoll = Turned + ((Set % 2) ? -16.f : 16.f) * Ease(.3f, .65f, U) + 10.f * Peer * FMath::Sin(P * 5.f);
        }
        else
        {
            BarLift = 3.f * FMath::Sin(PI * FMath::Clamp((U - .15f) / .7f, 0.f, 1.f));
            BarRoll = Turned + ((Set % 2) ? -16.f : 16.f) * Ease(.25f, .75f, U);
        }
        // The tongs clink as the bar comes off the face and goes back on it.
        const float PrevP = Previous - Window;
        for (const float At : { .15f * Pause, .88f * Pause })
            if (Previous >= Window && PrevP < At && P >= At) bClinkDue = true;
    }
    if (R < 0.f) R = S;
    Swing = FMath::Lerp(S, .03f, Resting);
    Cock = FMath::Lerp(R, .03f, Resting);
    TapBlend = FMath::Lerp(TapW, 1.f, Resting);
    Drive = Push * (1.f - Resting);
    Inspect = Peer; BarPull = Pull;
    SinceBlow += DeltaSeconds;
    // A blow lands as each cycle ends; it is struck once the pose is solved this
    // frame (UpdatePose), and that frame shows the contact
    // (a new cycle began, or the window ended with the set's last blow).
    if (Resting < .1f && Previous >= 0.f && Local >= Previous && PreviousK >= 0 && (K > PreviousK || K < 0)) bStrikeDue = true;
    if (bStrikeDue) { Swing = Cock = TapBlend = 0.f; Drive = 1.f; }
    else if (bTapDue) { Swing = Cock = 0.f; TapBlend = 1.f; Drive = 1.f; }
    // Sparks fly and fall; the flash dies away.
    for (int32 I = SparkState.Num() - 1; I >= 0; --I)
    {
        FSpark& P = SparkState[I];
        P.Age += DeltaSeconds;
        if (P.Age > P.Life) { SparkState.RemoveAtSwap(I); continue; }
        P.Velocity.Z -= 980.f * DeltaSeconds;
        P.At += P.Velocity * DeltaSeconds;
    }
    if (Sparks)
    {
        Sparks->ClearInstances();
        for (const FSpark& P : SparkState)
        {
            const float Size = .009f * (1.f - P.Age / P.Life) + .002f;   // the cube is 100 cm: about a centimetre, shrinking
            Sparks->AddInstance(FTransform(P.Velocity.Rotation(), P.At, FVector(Size * 2.5f, Size, Size)), true);
        }
    }
    FlashTime += DeltaSeconds;
    if (StrikeLight) StrikeLight->SetIntensity(9000.f * FMath::Exp(-FlashTime * 22.f));
}

void ADockNPC::Strike()
{
    ++Strikes;
    SinceBlow = 0.f;
    const FTransform& Comp = Body->GetComponentTransform();
    const FVector Top = Comp.TransformPosition(FVector(AnvilAhead - 1.f, AnvilRight, AnvilFace + 2.f * BarHalf));
    if (Hammer)
        StrikeGap = static_cast<float>(FVector::Dist(Hammer->GetComponentTransform().TransformPosition(FVector(HammerFace, 0.f, HammerHead)), Top));
    if (StrikeSounds.Num())
        UGameplayStatics::PlaySoundAtLocation(this, StrikeSounds[FMath::RandRange(0, StrikeSounds.Num() - 1)], Top, .45f, FMath::FRandRange(.97f, 1.03f), 0.f, StrikeAttenuation);
    for (int32 I = 0; I < 9; ++I)
    {
        const float Yaw = FMath::FRandRange(0.f, 360.f);
        const FVector Out = FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(140.f, 360.f) + FVector(0, 0, FMath::FRandRange(60.f, 260.f));
        SparkState.Add({ Top, Out, 0.f, FMath::FRandRange(.22f, .55f) });
    }
    FlashTime = 0.f;
    if (Strikes > 3) WorstStrikeGap = FMath::Max(WorstStrikeGap, StrikeGap);   // past the loading hitch
    if (Strikes <= 3 || Strikes % 10 == 0)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SMITH_STRIKE n=%d gap_cm=%.1f worst_cm=%.1f taps=%d worst_tap_cm=%.1f tongs_grip_cm=%.1f"),
            Strikes, StrikeGap, WorstStrikeGap, Taps, WorstTapGap, TongsGripError);
}

void ADockNPC::Tap()
{
    // The light ring of the hammer dropped on the bare face, on the heel beside the work.
    ++Taps;
    const FVector At = Body->GetComponentTransform().TransformPosition(FVector(AnvilAhead - 4.f, AnvilRight + TapRight, AnvilFace));
    if (Hammer && Taps > 3)
        WorstTapGap = FMath::Max(WorstTapGap, static_cast<float>(FVector::Dist(Hammer->GetComponentTransform().TransformPosition(FVector(HammerFace, 0.f, HammerHead)), At)));
    if (Hammer && Taps > 3 && FVector::Dist(Hammer->GetComponentTransform().TransformPosition(FVector(HammerFace, 0.f, HammerHead)), At) > 3.f)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SMITH_TAP_MISS n=%d gap_cm=%.1f fist_error_cm=%.1f swing=%.2f cock=%.2f tap=%.2f resting=%.2f strike_due=%d"),
            Taps, static_cast<float>(FVector::Dist(Hammer->GetComponentTransform().TransformPosition(FVector(HammerFace, 0.f, HammerHead)), At)),
            HammerFistError, Swing, Cock, TapBlend, Resting, bStrikeDue ? 1 : 0);
    if (TapSounds.Num())
        UGameplayStatics::PlaySoundAtLocation(this, TapSounds[FMath::RandRange(0, TapSounds.Num() - 1)], At, .2f, FMath::FRandRange(.98f, 1.02f), 0.f, StrikeAttenuation);
}

bool ADockNPC::IsForgeSounding() const { return ForgeAudio && ForgeAudio->IsPlaying(); }

void ADockNPC::PoseSmith(TArray<FTransform>& Space)
{
    const float Right = -ArmOut;                                    // which way is his right (+Y)
    // Where the hammer's face comes down: the bar, or the bare heel beside it.
    const FVector Work(AnvilAhead - 1.f - BarPull, AnvilRight, AnvilFace + 2.f * BarHalf + BarLift);
    const FVector Heel(AnvilAhead - 4.f, AnvilRight + TapRight, AnvilFace);
    const FVector Top = FMath::Lerp(FVector(Work.X + BarPull, Work.Y, AnvilFace + 2.f * BarHalf), Heel, TapBlend);
    // The hammer, as a frame: Along (the face's way, his knuckles) and Thumb
    // (up the handle to the head). Down: face down, handle level, coming in
    // from his right. Raised: the fist about shoulder height in front of the
    // right shoulder, the handle near upright, the head just behind it.
    // Cock turns the hammer between the two; Swing carries the fist.
    const FVector ThumbDown = FVector(1.f, -.22f * Right, .05f).GetSafeNormal();
    const FVector AlongDown = FVector::VectorPlaneProject(-FVector::UpVector, ThumbDown).GetSafeNormal();
    const FVector ThumbUp = FVector(-.2f, .1f * Right, 1.f).GetSafeNormal();
    const FVector AlongUp = FVector::VectorPlaneProject(FVector(1.f, 0.f, .1f), ThumbUp).GetSafeNormal();
    const FQuat Down = FRotationMatrix::MakeFromXZ(AlongDown, ThumbDown).ToQuat(), Up = FRotationMatrix::MakeFromXZ(AlongUp, ThumbUp).ToQuat();
    const FQuat Q = FQuat::Slerp(Down, Up, Cock);
    const FVector FistDown = Top - AlongDown * HammerFace - ThumbDown * HammerHead;
    const FVector FistUp(16.f, 25.f * Right, 140.f);
    const FVector Fist = FMath::Lerp(FistDown, FistUp, Swing) + FVector(-5.f, 3.f * Right, 5.f) * FMath::Sin(PI * Swing);   // an arc, not a straight line
    const FVector Pole = FMath::Lerp(FVector(-.35f, Right, -.6f), FVector(-.2f, Right, -.15f), Swing);
    PlaceHand(Space, 1, Fist, Q.GetAxisX(), Q.GetAxisZ(), Pole);
    // The tongs: from his left fist at the waist, sloping down across the face,
    // the bar flat on it under the blow; flatter when he lifts it to look.
    // His forearm hangs; the reins leave the fist on the thumb side.
    const float Slope = FMath::DegreesToRadians(FMath::Lerp(ReinsSlope, 6.f, Inspect));
    const FVector Level = FVector(.81f, .59f * Right, 0.f).GetSafeNormal();
    const FVector Reins = Level * FMath::Cos(Slope) - FVector::UpVector * FMath::Sin(Slope);
    const FQuat Roll(Reins, FMath::DegreesToRadians(BarRoll));
    const FQuat TongsQ = Roll * FRotationMatrix::MakeFromXZ(Reins, FVector::UpVector).ToQuat();
    // A heavy blow jars the bar on the face for a moment.
    const FVector Bar = Work - FVector(0.f, 0.f, BarHalf + .5f * FMath::Exp(-SinceBlow / .05f));
    const FVector LeftFist = Bar - TongsQ.RotateVector(TongsBar);
    const FVector LeftAlong = Roll.RotateVector(FVector::VectorPlaneProject(FVector(.1f, -.15f * Right, -1.f), Reins).GetSafeNormal());
    PlaceHand(Space, 0, LeftFist, LeftAlong, Reins, FVector(-.4f, -Right * .6f, -.3f));
    // Props in the fists as posed (if an arm fell short, the prop shows it and the test measures it).
    FVector F, A, T;
    HandFrame(Space, 1, F, A, T);
    HammerFistError = static_cast<float>(FVector::Dist(F, Fist));
    if (Hammer) Hammer->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXZ(A, T).ToQuat(), F));
    HandFrame(Space, 0, F, A, T);
    TongsGripError = static_cast<float>(FVector::Dist(F, LeftFist));
    if (Strikes > 3) WorstTongsGap = FMath::Max(WorstTongsGap, TongsGripError);   // every frame, the inspections included
    if (Tongs) Tongs->SetRelativeTransform(FTransform(TongsQ, F));
}

void ADockNPC::SmithEvents()
{
    // Sounds and sparks once the frame's pose (and the hammer) is in place.
    if (bStrikeDue) { bStrikeDue = false; bTapDue = false; Strike(); }
    if (bTapDue) { bTapDue = false; Tap(); }
    if (bClinkDue)
    {
        bClinkDue = false;
        if (ClinkSounds.Num() && Tongs)
            UGameplayStatics::PlaySoundAtLocation(this, ClinkSounds[FMath::RandRange(0, ClinkSounds.Num() - 1)],
                Tongs->GetComponentTransform().TransformPosition(TongsBar), .35f, FMath::FRandRange(.97f, 1.03f), 0.f, StrikeAttenuation);
    }
}

void ADockNPC::PoseKeeper(TArray<FTransform>& Space)
{
    // Where the round is: inside the rim, over the outside, (every third
    // round) holding it up to look it over; each blends into the next.
    const float Round = PolishInside + PolishOutside, Cycle = 3.f * Round + PolishInspect;
    const float T = FMath::Fmod(PolishClock, Cycle);
    const auto PhaseAt = [&](float X, float& Into) -> int32
    {
        for (int32 R = 0; R < 3; ++R)
        {
            if (X < PolishInside) { Into = X; return 0; } X -= PolishInside;
            if (X < PolishOutside) { Into = X; return 1; } X -= PolishOutside;
        }
        Into = X; return 2;
    };
    float Into = 0.f;
    const int32 Now = PhaseAt(T, Into);
    if (Now != PolishPhase) { if (Now < 2 && PolishPhase >= 0) ++PolishPasses; PolishPhase = Now; }
    const float Right = -ArmOut;
    struct FPose { FVector Grip, Along, Up, Fist, FistAlong, FistThumb; float Look = 0.f; };
    const auto Pose = [&](int32 Which, float U) -> FPose
    {
        FPose P;
        // The tankard turns a little in his hand as he works; its mouth tipped toward him.
        const float Turn = FMath::DegreesToRadians(-12.f + 14.f * FMath::Sin(PolishClock * .7f));
        FVector Tip(-.14f, 0.f, 1.f);
        P.Grip = KeeperGrip;
        if (Which == 2)
        {
            // Held up to the light, turned slowly, the rag hand down at his side.
            const float Up = FMath::SmoothStep(0.f, .25f, U / PolishInspect) * (1.f - FMath::SmoothStep(.75f, 1.f, U / PolishInspect));
            P.Grip += FVector(4.f, 4.f * Right, 15.f) * Up;   // up and out from him, clear of his beard
            Tip = FMath::Lerp(Tip, FVector(-.65f, 0.f, 1.f), Up);
            P.Look = Up;
        }
        P.Up = Tip.GetSafeNormal();
        const FVector Side = FVector(-FMath::Sin(Turn), FMath::Cos(Turn) * Right, 0.f);   // from the handle toward the body: to his right
        P.Along = FVector::VectorPlaneProject(Side, P.Up).GetSafeNormal();
        const FVector Centre = P.Grip + P.Along * TankardBody;
        const FVector Mouth = Centre + P.Up * TankardMouth;
        const FVector Across = FVector::CrossProduct(P.Up, P.Along).GetSafeNormal();
        if (Which == 0)
        {
            // The rag pushed into the rim, small circles and a twist of the wrist.
            const float W = UE_TWO_PI / .85f * PolishClock;
            P.Fist = Mouth + P.Up * 2.5f + (P.Along * FMath::Cos(W) + Across * FMath::Sin(W)) * 1.6f;
            P.FistAlong = -P.Up;
            P.FistThumb = FQuat(P.Up, FMath::DegreesToRadians(30.f * FMath::Sin(W))).RotateVector(-P.Along);
        }
        else if (Which == 1)
        {
            // Rubbing the outside, the palm to the pewter, round the side nearest his
            // free hand (his right, toward him: the far side is out of his reach) and up and down.
            const FVector Out = FQuat(P.Up, FMath::DegreesToRadians(Right * (25.f + 30.f * FMath::Sin(PolishClock * 1.4f)))).RotateVector(P.Along);
            P.Fist = Centre + Out * (TankardRadius + 3.5f) + P.Up * (.5f + 2.5f * FMath::Sin(PolishClock * 2.6f));
            P.FistAlong = P.Up;
            P.FistThumb = FVector::CrossProduct(P.FistAlong, Out) * PalmSign[1];   // so the palm faces the tankard
        }
        else
        {
            P.Fist = FVector(10.f, 24.f * Right, 98.f);
            P.FistAlong = FVector(.15f, 0.f, -1.f).GetSafeNormal();
            P.FistThumb = FVector(1.f, 0.f, .15f).GetSafeNormal();
        }
        return P;
    };
    FPose P = Pose(Now, Into);
    if (Into < PolishBlend && PolishClock > PolishBlend)
    {
        // Ease from where the last phase left off.
        float Back = 0.f;
        const int32 Last = PhaseAt(FMath::Fmod(T - Into - .001f + Cycle, Cycle), Back);
        const FPose L = Pose(Last, Back);
        const float A = FMath::SmoothStep(0.f, PolishBlend, Into);
        P.Grip = FMath::Lerp(L.Grip, P.Grip, A); P.Fist = FMath::Lerp(L.Fist, P.Fist, A); P.Look = FMath::Lerp(L.Look, P.Look, A);
        P.Along = FMath::Lerp(L.Along, P.Along, A).GetSafeNormal(); P.Up = FMath::Lerp(L.Up, P.Up, A).GetSafeNormal();
        P.FistAlong = FMath::Lerp(L.FistAlong, P.FistAlong, A).GetSafeNormal(); P.FistThumb = FMath::Lerp(L.FistThumb, P.FistThumb, A).GetSafeNormal();
    }
    Inspect = P.Look;
    // The left fist closes on the handle, knuckles toward the body, thumb on top.
    PlaceHand(Space, 0, P.Grip, P.Along, P.Up, FVector(-.3f, -Right, -.6f));
    PlaceHand(Space, 1, P.Fist, P.FistAlong, P.FistThumb, FVector(-.3f, Right, -.6f));
    FVector F, A, Th;
    HandFrame(Space, 0, F, A, Th);
    const float GripError = static_cast<float>(FVector::Dist(F, P.Grip));
    if (GetWorld()->GetTimeSeconds() > 8.f) WorstTankardGrip = FMath::Max(WorstTankardGrip, GripError);
    if (GripError > 2.f) UE_LOG(LogTemp, Verbose, TEXT("CHUCK_KEEPER_GRIP phase=%d into=%.2f grip_cm=%.1f grip=%s"), Now, Into, GripError, *P.Grip.ToString());
    if (Tankard) Tankard->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXZ(P.Along, P.Up).ToQuat(), F));
    HandFrame(Space, 1, F, A, Th);
    const float RagError = static_cast<float>(FVector::Dist(F, P.Fist));
    if (GetWorld()->GetTimeSeconds() > 8.f) WorstRagReach = FMath::Max(WorstRagReach, RagError);
    if (RagError > 3.f) UE_LOG(LogTemp, Verbose, TEXT("CHUCK_KEEPER_REACH phase=%d into=%.2f rag_cm=%.1f fist=%s"), Now, Into, RagError, *P.Fist.ToString());
    // The rag bunched in his fist, its tail falling toward him.
    if (Rag) Rag->SetRelativeTransform(FTransform(FRotator(0.f, 90.f, 0.f), F));
}

float ADockNPC::GetSeatHeight() const
{
    if (BoneIndex.Contains(INDEX_NONE)) return 0.f;
    const float Floor = static_cast<float>(GetActorLocation().Z) - HalfHeight;
    return static_cast<float>(Body->GetBoneLocation(BoneNames[ThighL]).Z + Body->GetBoneLocation(BoneNames[ThighR]).Z) * .5f - Floor;
}

void ADockNPC::TwoBone(TArray<FTransform>& Space, int32 Upper, int32 Lower, int32 End, const FVector& Target, const FVector& Pole) const
{
    // As PlaceHand's arm: the law of cosines for the middle joint, bent toward Pole; children follow.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Count = Space.Num();
    TArray<FTransform> Local; Local.SetNum(Count);
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 P = Ref.GetParentIndex(B);
        Local[B] = P >= 0 ? Space[B].GetRelativeTransform(Space[P]) : Space[B];
    }
    const auto Descends = [&](int32 B, int32 From) { for (; B != INDEX_NONE; B = Ref.GetParentIndex(B)) if (B == From) return true; return false; };
    const auto Rotate = [&](int32 Bone, const FQuat& Q)
    {
        Space[Bone].SetRotation(Q * Space[Bone].GetRotation());
        for (int32 B = Bone + 1; B < Count; ++B)
            if (Descends(B, Bone)) Space[B] = Local[B] * Space[Ref.GetParentIndex(B)];
    };
    const FVector S = Space[Upper].GetLocation();
    const float A = static_cast<float>(FVector::Dist(S, Space[Lower].GetLocation()));
    const float Bl = static_cast<float>(FVector::Dist(Space[Lower].GetLocation(), Space[End].GetLocation()));
    const FVector To = Target - S;
    const float D = FMath::Clamp(static_cast<float>(To.Size()), FMath::Abs(A - Bl) + 1.f, A + Bl - .2f);
    const FVector Dir = To.GetSafeNormal();
    const float CosA = FMath::Clamp((A * A + D * D - Bl * Bl) / (2.f * A * D), -1.f, 1.f);
    const FVector Bend = FVector::VectorPlaneProject(Pole, Dir).GetSafeNormal();
    const FVector Mid = S + Dir * (A * CosA) + Bend * (A * FMath::Sqrt(1.f - CosA * CosA));
    Rotate(Upper, FQuat::FindBetweenNormals((Space[Lower].GetLocation() - S).GetSafeNormal(), (Mid - S).GetSafeNormal()));
    Rotate(Lower, FQuat::FindBetweenNormals((Space[End].GetLocation() - Space[Lower].GetLocation()).GetSafeNormal(), (S + Dir * D - Space[Lower].GetLocation()).GetSafeNormal()));
}

void ADockNPC::SetupSeat(const FVector& LookAt)
{
    // Sitting: the hips come down onto the bench; the eyes with them.
    // The feet keep their standing rest (flat on the floor).
    TArray<FTransform> Standing;
    Solve(Rest, Standing);
    const float HipZ = static_cast<float>(Standing[BoneIndex[ThighL]].GetLocation().Z + Standing[BoneIndex[ThighR]].GetLocation().Z) * .5f;
    SeatDrop = SeatTop + SeatBone - HipZ;
    EyeHeight += SeatDrop;
    for (int32 Side = 0; Side < 2; ++Side) FootRest[Side] = Standing[BoneIndex[Of(FootL, Side)]].GetRotation();
    AnkleRest = static_cast<float>(Standing[BoneIndex[FootL]].GetLocation().Z);
    // Idle glances about LookAt, from where the NPC sits.
    const FVector To = GetActorTransform().InverseTransformVectorNoScale(LookAt - GetActorLocation());
    GlanceCentre = FVector2D(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), -35.f, 35.f), 4.f);
}

void ADockNPC::SitInTavern()
{
    if (bSeated || BoneIndex.Contains(INDEX_NONE)) return;
    // From outside the door to the bench: the switch happens while the slide's view is black.
    SetActorLocationAndRotation(TavernHips + FVector(0.f, 0.f, HalfHeight), FRotator(0.f, TavernYaw, 0.f));
    HomeYaw = TavernYaw; bTurning = false; TurnHold = 0.f;
    bSeated = true; SeatTop = TavernBenchTop; SeatBone = WorkerSitBone;
    SetupSeat(TavernBar);
    Glance = GlanceCentre;
    IdleClip = ClipStandLook;   // the quieter idle, as the elf's
    bDrinker = true;
    SetGrip(1, 1.f);            // his right fist round the handle
    Tankard = NewObject<UStaticMeshComponent>(this, TEXT("Tankard"));
    Tankard->SetStaticMesh(KeeperMeshes[0]);
    Tankard->SetupAttachment(Body);
    Tankard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tankard->SetCanEverAffectNavigation(false);
    Tankard->RegisterComponent();
    DrinkClock = FMath::FRandRange(DrinkDown, DrinkPeriod - 4.f);   // between drinks
    UE_LOG(LogTemp, Display, TEXT("CHUCK_WORKER_TAVERN seated=1 tankard=%d at=%s seat_drop=%.1f"), KeeperMeshes[0] ? 1 : 0, *TavernHips.ToString(), SeatDrop);
}

void ADockNPC::DrinkIn(float Seconds)
{
    DrinkClock = DrinkPeriod - FMath::Clamp(Seconds, 0.f, DrinkPeriod - DrinkDown);
}

namespace
{
    ADockNPC* FindNPC(const TCHAR* Tag)
    {
        for (const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All()) if (Entry.IsValid() && Entry->ActorHasTag(Tag)) return Entry.Get();
        return nullptr;
    }
}

void ADockNPC::StartTavernNight()
{
    ADockNPC* Worker = FindNPC(TEXT("DockWorkerArt"));
    ADockNPC* Keeper = FindNPC(TEXT("TavernKeeper"));
    if (!Worker || !Keeper || Keeper->NightStage > 0) return;
    // Nobody to talk to tonight: Dougmund holds forth (his day line is gone), the keeper waits for the rat.
    Worker->DisplayName = TEXT("Dougmund");
    Worker->Lines.Reset();
    Worker->StartAmbient(TEXT("night_00"), true, 5.5f, 1.f);
    Keeper->NightStage = 1;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TAVERN_NIGHT_START rant=%d keeper_line=%d"), Worker->HasAmbientLoop() ? 1 : 0,
        Keeper->VoiceLines.Num() > 0 ? 1 : 0);
}

int32 ADockNPC::GetTavernNightStage()
{
    const ADockNPC* Keeper = FindNPC(TEXT("TavernKeeper"));
    return Keeper ? Keeper->NightStage : 0;
}

void ADockNPC::TickTavernNight(float DeltaSeconds)
{
    if (NightStage != 1) return;
    const auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    InsideTime = Chuck && !Chuck->IsAstral() && InTavern(Chuck->GetActorLocation()) ? InsideTime + DeltaSeconds : 0.f;
    if (InsideTime < TavernInterrupt) return;
    // "Awright, awright, that'll do, Dougmund": he's cut off a moment after the keeper starts.
    NightStage = 2;
    StartAmbient(TEXT("night_00"), false);
    if (ADockNPC* Worker = FindNPC(TEXT("DockWorkerArt"))) Worker->StopAmbient(.35f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TAVERN_NIGHT_INTERRUPT chuck=%s"), *Chuck->GetActorLocation().ToString());
}

bool ADockNPC::StartAmbient(const TCHAR* Id, bool bLoop, float Pause, float Delay)
{
    // The line's index among this NPC's voice lines (added after the talk lines on first use).
    const FString Npc = Kind == EDockHuman::Worker ? TEXT("Worker") : Kind == EDockHuman::TavernKeeper ? TEXT("TavernKeeper") : DisplayName.Replace(TEXT(" "), TEXT(""));
    int32 Index = INDEX_NONE;
    for (int32 I = 0; I < VoiceLines.Num(); ++I)
        if (Npc == NPCVoiceData::Lines[VoiceLines[I]].Npc && FCString::Strcmp(NPCVoiceData::Lines[VoiceLines[I]].Id, Id) == 0) Index = I;
    for (int32 I = 0; Index == INDEX_NONE && I < NPCVoiceData::LineCount; ++I)
    {
        const NPCVoiceData::FLine& L = NPCVoiceData::Lines[I];
        if (Npc != L.Npc || FCString::Strcmp(L.Id, Id) != 0) continue;
        USoundBase* Sound = *L.Sound ? LoadObject<USoundBase>(nullptr, L.Sound) : nullptr;
        if (*L.Sound && !Sound) UE_LOG(LogTemp, Warning, TEXT("CHUCK_NPC_VOICE_MISSING %s %s"), *Npc, Id);
        Index = VoiceLines.Add(I); VoiceSounds.Add(Sound);
    }
    if (Index == INDEX_NONE) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_NPC_AMBIENT_MISSING %s %s"), *Npc, Id); return false; }
    EnsureVoiceAudio();
    // In the room: full a few metres round, falling away over the room and the street,
    // and muffled by walls (heard outside through the open door, quietly through the plaster).
    // Dougmund is loud: his rant carries well up the street as Chuck approaches.
    if (!RoomAttenuation)
    {
        const bool bLoud = Kind == EDockHuman::Worker;
        RoomAttenuation = NewObject<USoundAttenuation>(this);
        FSoundAttenuationSettings& A = RoomAttenuation->Attenuation;
        A.bAttenuate = true; A.bSpatialize = true;
        A.AttenuationShape = EAttenuationShape::Sphere;
        A.AttenuationShapeExtents = FVector(bLoud ? 450.f : 250.f, 0.f, 0.f);
        A.FalloffDistance = bLoud ? 2300.f : 1300.f;
        A.bEnableOcclusion = true;
        A.OcclusionTraceChannel = ECC_Visibility;
        A.OcclusionVolumeAttenuation = bLoud ? .45f : .3f;
        A.OcclusionLowPassFilterFrequency = 1200.f;
        A.OcclusionInterpolationTime = .3f;
    }
    VoiceAudio->AttenuationSettings = RoomAttenuation;
    AmbientLine = Index; bAmbientLoop = bLoop; AmbientPause = Pause; AmbientWait = Delay; AmbientStopIn = -1.f;
    return true;
}

void ADockNPC::StopAmbient(float Delay, float Fade)
{
    AmbientStopIn = FMath::Max(Delay, 0.f); AmbientFade = Fade;
}

bool ADockNPC::GetAmbientSubtitle(const FVector& At, FString& Speaker, FString& Text)
{
    if (!InTavern(At)) return false;
    // The latest to start speaking (the keeper over Dougmund as he cuts in).
    const ADockNPC* Who = nullptr;
    for (const TWeakObjectPtr<ADockNPC>& Entry : All())
        if (Entry.IsValid() && Entry->IsAmbientSpeaking() && (!Who || Entry->VoiceTime < Who->VoiceTime)) Who = Entry.Get();
    if (!Who) return false;
    const NPCVoiceData::FLine& L = NPCVoiceData::Lines[Who->VoiceLines[Who->VoiceLine]];
    // Sentences, gathered into parts of a readable length, each shown for its share of the line by length.
    TArray<FString> Parts;
    FString Part;
    const FString Whole(L.Text);
    for (int32 I = 0; I < Whole.Len(); ++I)
    {
        Part.AppendChar(Whole[I]);
        const bool bEnd = FString(TEXT(".!?\u2026")).Contains(FString::Chr(Whole[I])) && (I + 1 == Whole.Len() || Whole[I + 1] == TEXT(' '));
        if ((bEnd && Part.TrimStartAndEnd().Len() >= 45) || I + 1 == Whole.Len()) { Parts.Add(Part.TrimStartAndEnd()); Part.Reset(); }
    }
    int32 Total = 0;
    for (const FString& P : Parts) Total += P.Len();
    float Into = Who->VoiceTime / FMath::Max(L.Seconds, .1f) * Total;
    for (const FString& P : Parts)
    {
        Text = P;
        if ((Into -= P.Len()) < 0.f) break;
    }
    Speaker = Who->DisplayName;
    return true;
}

void ADockNPC::PoseDrink(TArray<FTransform>& Space)
{
    const float Right = -ArmOut;
    const float U = FMath::Fmod(DrinkClock, DrinkPeriod);
    const float A = FMath::SmoothStep(0.f, DrinkRaise, U) * (1.f - FMath::SmoothStep(DrinkLower, DrinkDown, U));
    DrinkLift = A;
    // On the table: standing on its base, its body toward his left.
    const FVector RestGrip(DrinkRestAhead, DrinkRestOut * Right, TavernTableTop + TankardGripHeight + .2f);
    const FVector RestAlong = FVector(.35f, -Right, 0.f).GetSafeNormal();
    // At his lips: its mouth tipped to him, the rim's lower edge on his lower lip,
    // the handle to his right. His lips from the head as posed (tipped back while he drinks).
    const FVector Lips = Space[BoneIndex[Head]].TransformPosition(HeadRef.InverseTransformPosition(WorkerMouth));
    const float Tip = FMath::DegreesToRadians(FMath::Lerp(DrinkTipStart, DrinkTipEnd, FMath::SmoothStep(DrinkRaise, DrinkLower, U)));
    const FVector DrinkUp(-FMath::Sin(Tip), 0.f, FMath::Cos(Tip));
    const FVector DrinkAlong(0.f, -Right, 0.f);
    const auto RimLow = [](const FVector& Up) { return FVector::VectorPlaneProject(-FVector::UpVector, Up).GetSafeNormal(); };
    const FVector Contact = Lips + FVector(.6f, 0.f, -.8f);
    const FVector DrinkGrip = Contact - DrinkAlong * TankardBody - DrinkUp * TankardRimHeight - RimLow(DrinkUp) * TankardRim;
    const FVector Handle = FMath::Lerp(RestGrip, DrinkGrip, A);
    const FVector Up = FMath::Lerp(FVector::UpVector, DrinkUp, A).GetSafeNormal();
    const FVector Along = FVector::VectorPlaneProject(FMath::Lerp(RestAlong, DrinkAlong, A), Up).GetSafeNormal();
    // The right fist on the handle, knuckles toward the tankard, thumb along its up; the elbow out and down, up as it lifts.
    PlaceHand(Space, 1, Handle, Along, Up, FVector(-.3f, Right, FMath::Lerp(-.7f, -.15f, A)));
    FVector F, FA, FT;
    HandFrame(Space, 1, F, FA, FT);
    const bool bSettled = GetWorld()->GetTimeSeconds() > 8.f;
    if (bSettled) WorstDrinkGrip = FMath::Max(WorstDrinkGrip, static_cast<float>(FVector::Dist(F, Handle)));
    if (bSettled && A > .95f)
        WorstDrinkLip = FMath::Max(WorstDrinkLip, static_cast<float>(FVector::Dist(F + Along * TankardBody + Up * TankardRimHeight + RimLow(Up) * TankardRim, Contact)));
    if (Tankard) Tankard->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXZ(Along, Up).ToQuat(), F));
}

void ADockNPC::PoseSeated(TArray<FTransform>& Space)
{
    // Her legs: thighs forward over the bench's edge, shins down to her feet
    // flat on the paving a little in front of her knees, knees a touch apart.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const bool bSettled = GetWorld()->GetTimeSeconds() > 8.f;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const int32 Thigh = BoneIndex[Of(ThighL, Side)], Calf = BoneIndex[Of(CalfL, Side)], Foot = BoneIndex[Of(FootL, Side)];
        const FVector Hip = Space[Thigh].GetLocation();
        const float Lt = static_cast<float>(FVector::Dist(Hip, Space[Calf].GetLocation()));
        const float Lc = static_cast<float>(FVector::Dist(Space[Calf].GetLocation(), Space[Foot].GetLocation()));
        const float KneeZ = AnkleRest + Lc * FMath::Cos(FMath::DegreesToRadians(ShinLean));
        const float Rise = static_cast<float>(Hip.Z) - KneeZ;
        const float KneeX = static_cast<float>(Hip.X) + FMath::Sqrt(FMath::Max(Lt * Lt - Rise * Rise, 1.f));
        const FVector Ankle(KneeX + Lc * FMath::Sin(FMath::DegreesToRadians(ShinLean)), Hip.Y * 1.15f, AnkleRest);
        TwoBone(Space, Thigh, Calf, Foot, Ankle, FVector(1.f, 0.f, .4f));
        // The foot as it stood: flat on the floor (the toes follow it).
        const int32 Count = Space.Num();
        TArray<FTransform> Local; Local.SetNum(Count);
        for (int32 B = Foot; B < Count; ++B) Local[B] = Space[B].GetRelativeTransform(Space[Ref.GetParentIndex(B)]);
        Space[Foot].SetRotation(FootRest[Side]);
        for (int32 B = Foot + 1; B < Count; ++B)
            for (int32 P = Ref.GetParentIndex(B); P != INDEX_NONE; P = Ref.GetParentIndex(P))
                if (P == Foot) { Space[B] = Local[B] * Space[Ref.GetParentIndex(B)]; break; }
        if (bSettled) WorstFootLift = FMath::Max(WorstFootLift, FMath::Abs(static_cast<float>(Space[Foot].GetLocation().Z) - AnkleRest));
    }
    // Her hands resting on her lap, one on each thigh, fingers forward and in, palms down.
    for (int32 Side = 0; Side < (bDrinker ? 1 : 2); ++Side)   // the drinker's right hand is on his tankard (PoseDrink)
    {
        const float Out = Side == 0 ? ArmOut : -ArmOut;   // +Y is out on this side
        const FVector Hip = Space[BoneIndex[Of(ThighL, Side)]].GetLocation(), Knee = Space[BoneIndex[Of(CalfL, Side)]].GetLocation();
        const FVector Lap = FMath::Lerp(Hip, Knee, LapAlong) + FVector(0.f, -Out * 2.f, LapLift);
        const FVector Along = FVector(.85f, -Out * .42f, -.2f).GetSafeNormal();
        const FVector Palm = -FVector::UpVector;
        const FVector Thumb = FVector::CrossProduct(Palm * PalmSign[Side], Along).GetSafeNormal();
        PlaceHand(Space, Side, Lap, Along, Thumb, FVector(-.4f, Out, -.3f));
        FVector F, A, Th;
        HandFrame(Space, Side, F, A, Th);
        if (bSettled) WorstLapHand = FMath::Max(WorstLapHand, static_cast<float>(FVector::Dist(F, Lap)));
    }
}

void ADockNPC::PoseSleeves(TArray<FTransform>& Space)
{
    // Forearms across in front of him, the wrists meeting a little past the
    // middle (the left cuff just above the right), elbows down at his sides:
    // each hand inside the other's sleeve, as far as anyone can see.
    // From this body's own arm lengths, so the elbows bend about square whatever his size.
    const bool bSettled = GetWorld()->GetTimeSeconds() > 8.f;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Out = Side == 0 ? ArmOut : -ArmOut;   // +Y is out on this side
        const int32 Upper = BoneIndex[Of(UpperL, Side)], Lower = BoneIndex[Of(LowerL, Side)], Hand = BoneIndex[Of(HandL, Side)];
        const FVector S = Space[Upper].GetLocation();
        const float A = static_cast<float>(FVector::Dist(S, Space[Lower].GetLocation()));
        const float B = static_cast<float>(FVector::Dist(Space[Lower].GetLocation(), Space[Hand].GetLocation()));
        const FVector Elbow = S + FVector(ElbowAhead, Out * ElbowOut, -.75f).GetSafeNormal() * A;
        const float Across = FMath::Abs(static_cast<float>(Elbow.Y) + Out * SleeveCross);
        const FVector Wrist(Elbow.X + FMath::Sqrt(FMath::Max(B * B - Across * Across, .09f * B * B)), -Out * SleeveCross, Elbow.Z + 2.f + (Side == 0 ? SleeveStack : -SleeveStack));
        TwoBone(Space, Upper, Lower, Hand, Wrist, Elbow - (S + Wrist) * .5f);
        if (bSettled) WorstSleeveReach = FMath::Max(WorstSleeveReach, static_cast<float>(FVector::Dist(Space[Hand].GetLocation(), Wrist)));
    }
}

float ADockNPC::GetWristGap() const
{
    return BoneIndex.Contains(INDEX_NONE) ? 1e3f : static_cast<float>(FVector::Dist(Body->GetBoneLocation(BoneNames[HandL]), Body->GetBoneLocation(BoneNames[HandR])));
}

void ADockNPC::PoseSailor(TArray<FTransform>& Space)
{
    // The pipe in the right corner of his mouth, riding his head.
    const float Right = -ArmOut;
    const FTransform PipeRest(FRotationMatrix::MakeFromXZ(FVector(1.f, .2f * Right, -.12f), FVector::UpVector).ToQuat(),
        SailorMouth + FVector(-.5f, MouthCorner * Right, 0.f));
    const FTransform PipeNow = PipeRest.GetRelativeTransform(HeadRef) * Space[BoneIndex[Head]];
    // His hand: up to cup the bowl (a draw, or to take the pipe out to speak), then out
    // to his chest with it while he talks; blended from the idle's own hand.
    const float HandUp = FMath::Max(PipeHold, FMath::Min(1.f, PipeOut * 2.f));
    const float Out = FMath::Clamp(PipeOut * 2.f - 1.f, 0.f, 1.f);
    if (HandUp <= 0.f) { if (Pipe) Pipe->SetRelativeTransform(PipeNow); return; }
    FVector F, A, T;
    HandFrame(Space, 1, F, A, T);
    const FVector Cup = PipeNow.TransformPosition(PipeCup);
    const FVector ChestAlong = FVector(1.f, -.25f * Right, .35f).GetSafeNormal();
    const FVector ChestThumb = FVector::VectorPlaneProject(FVector(-.2f, 0.f, 1.f), ChestAlong).GetSafeNormal();
    const FVector Want = FMath::Lerp(Cup, FVector(28.f, 8.f * Right, 130.f), Out);
    const FVector WantAlong = FMath::Lerp(PipeNow.GetUnitAxis(EAxis::X), ChestAlong, Out).GetSafeNormal();
    const FVector WantThumb = FMath::Lerp(PipeNow.GetUnitAxis(EAxis::Z), ChestThumb, Out).GetSafeNormal();
    PlaceHand(Space, 1, FMath::Lerp(F, Want, HandUp), FMath::Lerp(A, WantAlong, HandUp).GetSafeNormal(), FMath::Lerp(T, WantThumb, HandUp).GetSafeNormal(), FVector(-.3f, Right, -.6f));
    // The pipe: in his teeth, or (once out) in his fist, the bowl cupped.
    HandFrame(Space, 1, F, A, T);
    if (Pipe)
    {
        if (Out <= 0.f) Pipe->SetRelativeTransform(PipeNow);
        else
        {
            const FQuat Held = FRotationMatrix::MakeFromXZ(A, T).ToQuat();
            Pipe->SetRelativeTransform(FTransform(Held, F - Held.RotateVector(PipeCup)));
        }
    }
    if (PipeHold > .95f)
    {
        HandFrame(Space, 1, F, A, T);
        if (GetWorld()->GetTimeSeconds() > 8.f) WorstPipeHold = FMath::Max(WorstPipeHold, static_cast<float>(FVector::Dist(F, Cup)));
        if (!bPipeHeld) { bPipeHeld = true; ++PipeDraws; }
    }
    else bPipeHeld = false;
}

float ADockNPC::GetPipeMouthError() const
{
    if (!Pipe || !Body->GetSkinnedAsset()) return 1e3f;
    if (PipeOut > 0.f) return 0.f;   // out in his hand while he speaks
    const FTransform& Comp = Body->GetComponentTransform();
    const FVector Corner = Comp.TransformPosition(SailorMouth + FVector(-.5f, MouthCorner * -ArmOut, 0.f));
    // The mouth as the head now carries it, against where the pipe's bit actually is.
    const FTransform HeadNow = Body->GetBoneTransform(Body->GetBoneIndex(BoneNames[Head]));
    const FVector MouthNow = HeadNow.TransformPosition(HeadRef.InverseTransformPosition(Comp.InverseTransformPosition(Corner)));
    return static_cast<float>(FVector::Dist(MouthNow, Pipe->GetComponentLocation()));
}

void ADockNPC::TickSailor(float DeltaSeconds)
{
    // Talking, he takes the pipe out and holds it at his chest; his smoking waits until it's back in.
    const bool bSpeak = bTalking || IsSpeaking();
    PipeOut = FMath::FInterpConstantTo(PipeOut, bSpeak ? 1.f : 0.f, DeltaSeconds, 1.6f);
    const bool bSmoking = !bSpeak && PipeOut <= 0.f;
    if (bSmoking) PipeClock += DeltaSeconds;
    const float C = FMath::Fmod(PipeClock, PipePeriod);
    PipeHold = !bSmoking ? 0.f : C < PipeRaise ? FMath::SmoothStep(0.f, PipeRaise, C) : C < PipeLower ? 1.f : C < PipeDown ? 1.f - FMath::SmoothStep(PipeLower, PipeDown, C) : 0.f;
    // Breathing out after the draw: a stream of puffs from the corner of his mouth, forward and a little down.
    if (Pipe && bSmoking && C >= PipeExhaleAt && C < PipeExhaleAt + PipeExhaleLength)
    {
        const float Left = 1.f - (C - PipeExhaleAt) / PipeExhaleLength;
        PipeExhaleCarry += DeltaSeconds * 14.f;
        const FVector Mouth = Pipe->GetComponentLocation();
        const FVector Out = (GetActorForwardVector() + FVector(0, 0, -.2f)).GetSafeNormal();
        while (PipeExhaleCarry >= 1.f)
        {
            PipeExhaleCarry -= 1.f;
            PipePuffs.Add({ Mouth + Out * 3.f, Out * FMath::FRandRange(45.f, 75.f) * (.4f + .6f * Left) + FMath::VRand() * 9.f, 0.f, FMath::FRandRange(2.f, 2.8f), FMath::FRandRange(.8f, 1.2f) });
            ++PipePuffsSpawned;
        }
    }
    // Each puff slows, swells, rises and thins away on the harbour breeze.
    if (!PipeBreath) return;
    PipeBreath->ClearInstances();
    for (int32 I = PipePuffs.Num() - 1; I >= 0; --I)
    {
        FPuff& P = PipePuffs[I];
        P.Age += DeltaSeconds;
        if (P.Age >= P.Life) { PipePuffs.RemoveAtSwap(I); continue; }
        P.Velocity *= FMath::Max(0.f, 1.f - 1.5f * DeltaSeconds);
        P.Velocity.Z += (P.Age > .25f ? 18.f : 0.f) * DeltaSeconds;
        P.At += P.Velocity * DeltaSeconds + FVector(6.f, 3.f, 0.f) * DeltaSeconds;
    }
    for (const FPuff& P : PipePuffs)
    {
        const float U = P.Age / P.Life;
        const float Diameter = FMath::Lerp(5.f, 34.f, 1.f - FMath::Square(1.f - U)) * P.Size;
        const int32 Index = PipeBreath->AddInstance(FTransform(FRotator(0, P.Age * 40.f, 0), P.At, FVector(Diameter / 100.f)), true);
        PipeBreath->SetCustomDataValue(Index, 0, .28f * FMath::SmoothStep(0.f, .1f, P.Age) * FMath::Pow(1.f - U, 1.5f), false);
    }
    PipeBreath->MarkRenderStateDirty();
}

void ADockNPC::ProbeShake(const TArray<FTransform>& Space, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f) return;
    const bool bCount = GetWorld()->GetTimeSeconds() > 8.f;
    if (ProbePrev.Num() != Space.Num())
    {
        ProbePrev.SetNum(Space.Num()); ProbeVel.Init(FVector::ZeroVector, Space.Num()); ProbeReversals.Init(0, Space.Num()); ProbeSwing.Init(0.f, Space.Num());
        for (int32 B = 0; B < Space.Num(); ++B) ProbePrev[B] = Space[B].GetLocation();
        ProbeYawPrev = static_cast<float>(GetActorRotation().Yaw);
        return;
    }
    for (int32 B = 0; B < Space.Num(); ++B)
    {
        const FVector V = (Space[B].GetLocation() - ProbePrev[B]) / DeltaSeconds;
        if (bCount && V.Size() > 3.f && ProbeVel[B].Size() > 3.f && FVector::DotProduct(V, ProbeVel[B]) < 0.f)
        { ++ProbeReversals[B]; ProbeSwing[B] += static_cast<float>((V - ProbeVel[B]).Size()); }
        ProbeVel[B] = V; ProbePrev[B] = Space[B].GetLocation();
    }
    ProbeFrames += bCount;
    const float Yaw = static_cast<float>(GetActorRotation().Yaw);
    const float YawV = FMath::FindDeltaAngleDegrees(ProbeYawPrev, Yaw) / DeltaSeconds;
    if (bCount && FMath::Abs(YawV) > 2.f && FMath::Abs(ProbeYawVel) > 2.f && YawV * ProbeYawVel < 0.f) ++YawReversals;
    ProbeYawVel = YawV; ProbeYawPrev = Yaw;
}

int32 ADockNPC::GetWorstReversals() const
{
    int32 Worst = 0;
    for (const int32 R : ProbeReversals) Worst = FMath::Max(Worst, R);
    return FMath::Max(Worst, YawReversals);
}

FString ADockNPC::GetJitterReport() const
{
    // The six bones with the most reversals: name, share of frames reversing (%), mean velocity jump (cm/s).
    TArray<int32> Order;
    for (int32 B = 0; B < ProbeReversals.Num(); ++B) if (ProbeReversals[B]) Order.Add(B);
    Order.Sort([&](int32 A, int32 B) { return ProbeReversals[A] > ProbeReversals[B]; });
    FString Out = FString::Printf(TEXT("frames=%d yaw_reversals=%d"), ProbeFrames, YawReversals);
    for (int32 I = 0; I < FMath::Min(6, Order.Num()); ++I)
    {
        const int32 B = Order[I];
        Out += FString::Printf(TEXT(" %s=%.0f%%/%.0f"), *Body->GetSkinnedAsset()->GetRefSkeleton().GetBoneName(B).ToString(),
            100.f * ProbeReversals[B] / FMath::Max(1, ProbeFrames), ProbeSwing[B] / ProbeReversals[B]);
    }
    return Out;
}

ADockNPC* ADockNPC::SpawnBobert(UWorld* World, const FVector& BarrelAt, float Yaw)
{
    // His barrel first: the art, and a hidden cylinder of the same size lying
    // along it that is the solid part (the dock barrels' way); Chuck can climb
    // on it but not in, so the old man is left alone.
    AActor* Cask = World->SpawnActor<AActor>(BarrelAt, FRotator(0.f, Yaw, 0.f));
    auto* NPC = SpawnHuman(World, EDockHuman::Bobert, BarrelAt, Yaw);
    if (!NPC) { if (Cask) Cask->Destroy(); return nullptr; }
    if (Cask)
    {
        auto* Root = NewObject<USceneComponent>(Cask, TEXT("Root"));
        Cask->SetRootComponent(Root);
        Root->RegisterComponent();
        Cask->SetActorLocationAndRotation(BarrelAt, FRotator(0.f, Yaw, 0.f));
        if (NPC->BarrelMesh)
        {
            auto* Art = NewObject<UStaticMeshComponent>(Cask, TEXT("Art"));
            Art->SetMobility(EComponentMobility::Movable);
            Art->SetStaticMesh(NPC->BarrelMesh);
            Art->SetupAttachment(Root);
            Art->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Art->SetCanEverAffectNavigation(false);
            Art->RegisterComponent();
        }
        if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
        {
            auto* Solid = NewObject<UStaticMeshComponent>(Cask, TEXT("Solid"));
            Solid->SetMobility(EComponentMobility::Movable);
            Solid->SetStaticMesh(Cylinder);
            Solid->SetupAttachment(Root);
            // The engine cylinder is 100 cm, standing on Z: laid down along the barrel's X.
            Solid->SetRelativeLocationAndRotation(FVector(0.f, 0.f, BarrelAxisZ), FRotator(90.f, 0.f, 0.f));
            Solid->SetRelativeScale3D(FVector(2.f * BarrelBelly, 2.f * BarrelBelly, BarrelLength) / 100.f);
            Solid->SetCollisionProfileName(TEXT("BlockAll"));
            Solid->SetVisibility(false);
            Solid->RegisterComponent();
        }
        auto* Fill = NewObject<UPointLightComponent>(Cask, TEXT("Fill"));
        Fill->SetupAttachment(Root);
        Fill->SetRelativeLocation(BarrelFill);
        Fill->SetIntensity(BarrelFillIntensity);
        Fill->SetAttenuationRadius(BarrelFillRadius);
        Fill->SetLightColor(FLinearColor(.96f, .95f, .92f));   // the paving's daylight, not a warm glow
        Fill->SetSourceRadius(40.f);
        Fill->SetCastShadows(false);
        Fill->SetSpecularScale(0.f);
        Fill->RegisterComponent();
        Cask->Tags.Add(TEXT("BobertBarrel"));
        NPC->Barrel = Cask;
    }
    NPC->Tags.Add(TEXT("Bobert"));
    NPC->DisplayName.Empty();   // never named (GAME-BIBLE.md), never talked to: no lines
    // He isn't the solid part; the barrel is.
    NPC->Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NPC->SetGrip(0, .3f); NPC->SetGrip(1, .3f);   // hands loose, a little curled
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BOBERT_SPAWNED body=%d barrel=%d at=%s yaw=%.0f"), NPC->HumanMeshes[static_cast<int32>(EDockHuman::Bobert)] ? 1 : 0,
        NPC->BarrelMesh ? 1 : 0, *BarrelAt.ToString(), Yaw);
    return NPC;
}

void ADockNPC::SolveSleep()
{
    // As SolveRest: aim each bone in turn from the model's A-pose (component
    // space, turned about its own joint, children following), measuring the
    // posed skeleton after every step. Then seat the body in the barrel.
    Sleep.Init(FQuat::Identity, BoneCount);
    TArray<FTransform> Space;
    const auto At = [&](EBone B) { return Space[BoneIndex[B]].GetLocation(); };
    const auto Turn = [&](EBone B, const FQuat& Q) { Sleep[B] = Q * Sleep[B]; Solve(Sleep, Space); };
    const auto Aim = [&](EBone B, EBone Child, const FVector& Want)
    {
        Turn(B, FQuat::FindBetweenNormals((At(Child) - At(B)).GetSafeNormal(), Want.GetSafeNormal()));
    };
    const auto Rad = [](float Degrees) { return FMath::DegreesToRadians(Degrees); };
    Solve(Sleep, Space);
    const float S = FMath::Sign(static_cast<float>(At(UpperL).Y));   // the side his left limbs are on
    const float Side[2] = { S, -S };
    SleepTilt = S;
    HeadUpLocal = Space[BoneIndex[Head]].GetRotation().UnrotateVector((At(Head) - At(Neck)).GetSafeNormal());
    const float Thigh = static_cast<float>(FVector::Dist(At(ThighL), At(CalfL)));
    const float Calf = static_cast<float>(FVector::Dist(At(CalfL), At(FootL)));
    const float Ankle = static_cast<float>(At(FootL).Z);   // the ankle above his soles, standing
    FVector SoleRest = At(BallL) - At(FootL);
    SoleRest = FVector(SoleRest.Size2D(), 0.f, SoleRest.Z).GetSafeNormal();
    // The back: leaning into the blanket low, slumping forward higher up; the head bowed and fallen to his left.
    Aim(Spine1, Spine2, FVector(-FMath::Sin(Rad(SleepLean)), 0.f, FMath::Cos(Rad(SleepLean))));
    Aim(Spine2, Chest, FVector(-FMath::Sin(Rad(SleepLean * .4f)), 0.f, FMath::Cos(Rad(SleepLean * .4f))));
    Aim(Chest, Neck, FVector(FMath::Sin(Rad(SleepCurl)), S * .05f, FMath::Cos(Rad(SleepCurl))));
    Aim(Neck, Head, FVector(FMath::Sin(Rad(SleepNeck)), S * FMath::Sin(Rad(SleepTiltDeg * .5f)), FMath::Cos(Rad(SleepNeck))));
    {
        const FVector Up = Space[BoneIndex[Head]].GetRotation().RotateVector(HeadUpLocal);
        Turn(Head, FQuat::FindBetweenNormals(Up, FVector(FMath::Sin(Rad(SleepNod)), S * FMath::Sin(Rad(SleepTiltDeg)), FMath::Cos(Rad(SleepNod))).GetSafeNormal()));
    }
    // Knees up and a little apart.
    for (int32 L = 0; L < 2; ++L)
        Aim(Of(ThighL, L), Of(CalfL, L), FVector(FMath::Cos(Rad(SleepKnee)), Side[L] * SleepSpread, FMath::Sin(Rad(SleepKnee))));
    // Where he'll sit in the barrel (its frame: X along it, mouth +X): his back in the blanket.
    const FVector Hips = (At(ThighL) + At(ThighR)) * .5f;
    const float SeatX = BarrelBedroll - SleepSquash + SleepBack * Thigh + 1.f;
    const float SeatFloor = BarrelFloor(SeatX);
    const float Seat = static_cast<float>(Hips.Z) - SleepSeatDrop * Thigh;   // his seat, in the body's own space
    // Shins down and forward to the floor, which rises toward the mouth; feet flat, toes turned out.
    for (int32 L = 0; L < 2; ++L)
    {
        const FVector Knee = At(Of(CalfL, L));
        const float FootX = SeatX + static_cast<float>(Knee.X - Hips.X) + .85f * Calf;
        const float Want = Seat + Ankle + (BarrelFloor(FootX) - SeatFloor);
        const float Dz = FMath::Clamp((Want - static_cast<float>(Knee.Z)) / Calf, -1.f, 1.f);
        Aim(Of(CalfL, L), Of(FootL, L), FVector(FMath::Sqrt(FMath::Max(0.f, 1.f - Dz * Dz)), Side[L] * .04f, Dz));
        Aim(Of(FootL, L), Of(BallL, L), FVector(SoleRest.X, Side[L] * .2f * SoleRest.X, SoleRest.Z));
    }
    // Into the barrel: hips over the seat, the lowest of him on the floor (the arms are placed each frame).
    Body->SetRelativeLocation(FVector(SeatX - Hips.X, -Hips.Y, -HalfHeight + SeatFloor + .4f + SleepLift * Thigh - Hips.Z));
    Body->SetRelativeRotation(FRotator::ZeroRotator);
    EyeHeight = static_cast<float>(At(Head).Z) + 9.f;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BOBERT_POSE thigh_cm=%.1f calf_cm=%.1f seat_x_cm=%.1f seat_floor_cm=%.1f"), Thigh, Calf, SeatX, SeatFloor);
}

void ADockNPC::PoseBobert(TArray<FTransform>& Space)
{
    // Asleep: slow deep breaths (the chest rising, the shoulders with it), and
    // every so often the head nods a little lower and comes back.
    const float T = SleepClock + Phase;
    const float Breath = FMath::Sin(T * UE_TWO_PI / SleepBreath);
    const float Cycle = FMath::Fmod(T, 23.f);
    const float Nod = Cycle < 3.f ? FMath::Sin(Cycle / 3.f * UE_PI) : 0.f;
    TArray<FQuat> Delta = Sleep;
    Delta[Spine2] = Pitch(-.8f * Breath) * Sleep[Spine2];
    Delta[Chest] = Pitch(-1.8f * Breath) * Sleep[Chest];
    Delta[Neck] = Pitch(.6f * Breath + 3.f * Nod) * Sleep[Neck];
    Delta[Head] = Pitch(1.f * Breath + 5.f * Nod) * Sleep[Head];
    for (int32 L = 0; L < 2; ++L) Delta[Of(ClavL, L)] = Roll((L == 0 ? 1.f : -1.f) * SleepTilt * 1.2f * Breath) * Sleep[Of(ClavL, L)];
    Solve(Delta, Space);
    PoseHands(Space, false);
    // Forearms folded over the knees: each hand on the other knee, the right a little higher, over the left.
    const FVector Knees = (Space[BoneIndex[CalfL]].GetLocation() + Space[BoneIndex[CalfR]].GetLocation()) * .5f;
    for (int32 L = 0; L < 2; ++L)
    {
        const float Mine = L == 0 ? SleepTilt : -SleepTilt, Other = -Mine;
        const FVector Wrist = Knees + (L == 0 ? FVector(2.f, Other * 4.f, 4.5f) : FVector(4.5f, Other * 4.f, 7.5f));
        const FVector Along = FVector(.25f, Other, -.35f).GetSafeNormal();
        const FVector Thumb = FVector(.5f, 0.f, .85f).GetSafeNormal();
        const FVector Palm = FVector::CrossProduct(Along, Thumb).GetSafeNormal() * PalmSign[L];
        const FVector Fist = Wrist + Along * FistReach + Palm * PalmDepth;
        PlaceHand(Space, L, Fist, Along, Thumb, FVector(-.3f, Mine, -.4f));
        if (SleepClock > 2.f)
            WorstRestHand = FMath::Max(WorstRestHand, static_cast<float>(FVector::Dist(Space[BoneIndex[Of(HandL, L)]].GetLocation(), Wrist)));
    }
    // Count breaths (each time the chest starts to rise).
    const int32 Taken = FMath::FloorToInt(T / SleepBreath);
    if (Taken > LastBreath) { if (LastBreath >= 0) ++BreathCount; LastBreath = Taken; }
}

float ADockNPC::GetSleepFitError() const
{
    // Each joint (with its flesh round it) must be inside the barrel: within its
    // inner radius of the axis and between the blanket's back and the mouth
    // (his boots may reach the rim).
    const AActor* Cask = Barrel.Get();
    if (!Cask || !Body->GetSkinnedAsset()) return 1e3f;
    const FTransform Frame = Cask->GetActorTransform();
    struct FJoint { EBone Bone; float Flesh; };
    const FJoint Joints[] = { { Head, 9.f }, { Chest, 8.f }, { HandL, 3.f }, { HandR, 3.f }, { CalfL, 4.f }, { CalfR, 4.f }, { FootL, 3.f }, { FootR, 3.f } };
    float Worst = 0.f;
    for (const FJoint& J : Joints)
    {
        const FVector P = Frame.InverseTransformPosition(Body->GetBoneLocation(BoneNames[J.Bone]));
        const float X = static_cast<float>(P.X);
        const float Radial = static_cast<float>(FVector2D(P.Y, P.Z - BarrelAxisZ).Size()) + J.Flesh;
        Worst = FMath::Max(Worst, Radial - (BarrelRadius(X) - BarrelStave));
        Worst = FMath::Max(Worst, FMath::Max(-37.3f - X, X - BarrelLength * .5f));
    }
    return Worst;
}

FVector ADockNPC::GetHeadLocation() const
{
    return Body->GetBoneLocation(BoneNames[Head]);
}

float ADockNPC::GetHeadBow() const
{
    const FTransform HeadT = Body->GetBoneTransformByName(BoneNames[Head], EBoneSpaces::ComponentSpace);
    const FVector Up = HeadT.GetRotation().RotateVector(HeadUpLocal);
    return FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Up.X), static_cast<float>(Up.Z)));
}
