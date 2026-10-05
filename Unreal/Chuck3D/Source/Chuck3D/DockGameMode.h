#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "DockGameMode.generated.h"

UCLASS()
class CHUCK3D_API ADockGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADockGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    /** Start the chosen location in the fresh world already waiting behind the title menu. */
    bool StartFromMenu(const FString& Point);
private:
    bool bSmokeTest = false;
    // -ChuckNPCCapture: portraits of each NPC (front, three-quarter, back,
    // face) to Saved/Screenshots/Windows/NPC_*.png, then quit. For art review.
    bool bNPCCapture = false;
    float NPCCaptureTime = 0;
    int32 NPCShot = -1;
    bool bNPCShotTaken = false;
    TWeakObjectPtr<class ACameraActor> NPCCamera;
    void TickNPCCapture(float DeltaSeconds);
    // -ChuckSmithCapture: the blacksmith at work, a frame every 60 ms from a fixed three-quarter view.
    bool bSmithCapture = false;
    FName FilmTag = TEXT("Blacksmith");   // -ChuckKeeperCapture films the tavern keeper instead
    float SmithCaptureTime = 0, SmithNextFrame = 0;
    int32 SmithFrame = 0;
    void TickSmithCapture(float DeltaSeconds);
    // -ChuckDwarfCapture: Chuck at the dwarf's feet from five directions; the dwarf turns to him (never looking
    // down), two views of each; logs his look and the axe head's clearance from his shoulders.
    bool bDwarfCapture = false;
    float DwarfCaptureTime = 0;
    int32 DwarfShot = -1;
    void TickDwarfCapture(float DeltaSeconds);
    int32 TestStage = 0;
    float StageTime = 0;
    float MaxJumpZ = 0;
    float MaxAirFootLift = 0;
    void Check(bool Passed, const TCHAR* Description);
    int32 TestFailures = 0;
    int32 GapRuns = 0;
    FVector ProbeFoot[2] = {FVector::ZeroVector,FVector::ZeroVector};
    bool bProbeReady = false;
    bool bProbeLocked[2] = {false,false};
    int32 ProbeEvaluations = -1;
    // Stop/turn locomotion probes (stages 50-53) and frame timing.
    FVector LocoFoot[2] = {FVector::ZeroVector,FVector::ZeroVector};
    bool bLocoLocked[2] = {false,false};
    int32 LocoEvaluations = -1;
    int32 LocoSamples = 0;
    float LocoMaxSlip = 0;
    int32 LocoReleases = 0;
    FVector LocoStart = FVector::ZeroVector;
    FVector LocoPrevious = FVector::ZeroVector;
    float LocoValue = 0;
    bool bLocoFlag = false;
    double PerfFrameMs = 0;
    double PerfGpuMs = 0;
    int32 PerfFrames = 0;
    float CameraMinZ = 0;
    float CameraMaxZ = 0;
    float LookReached = 0;
    float KeySide = 0;
    float KeyJumpSide = 0;
    TArray<TWeakObjectPtr<class AGrassTuft>> TestTufts;
    int32 BreaksBase = 0;
    TWeakObjectPtr<class AClayJar> TestJar;
    int32 PickupsBase = 0;
    int32 CigsBase = 0;
    TWeakObjectPtr<class AEnemyRat> TestRat;
    int32 BitesBase = 0;
    int32 RatHitsBase = 0;
    int32 AstralBase = 0;
    int32 RespawnsBase = 0;
    int32 AstralSeen = 0;
    TWeakObjectPtr<class ADockNPC> Worker;
    TWeakObjectPtr<class ADockNPC> TalkNPC;
    float NearLookDown = 0;
    int32 ScratchBase = 0;
    int32 PoseHumans = 0;
    bool bPoseOK = false;
    int32 TalkSeen = 0;
    int32 StrafeJumpsBase = 0;
    int32 DropHangsBase = 0;
    bool bKeyMeasured = false;
    float SlashMin = 0, SlashMax = 0, SlashSpeed = 0, SlashLeftMin = 0, SlashLeftMax = 0;
    FVector SlashPrevious = FVector::ZeroVector;
    bool bSlashHave = false;
    TArray<float> FlurryStarts;
    TArray<FString> FlurryNames;
    int32 SlashStrikeBase = 0;
    FVector WallStart = FVector::ZeroVector;
    int32 WallRunsBase = 0, WallJumpsBase = 0;
    float WallEnterZ = 0, WallPeakZ = 0, WallEnterAt = -1, WallLeaveAt = -1;
    int32 SlidesBefore = 0, PullUpsBefore = 0;
    // The sewer zombie test.
    float ZombieStartDistance = 0, ZombieClosest = 1e6f, ZombieKillAt = -1;
    bool bZombieWindup = false, bZombieAliveAtEight = false;
    int32 ZombieSanityBefore = 0, ZombieCigarettesBefore = 0, ZombieHitsGiven = 0;
    bool bSlideOnly = false, bZombieOnly = false, bWallSideOnly = false, bPantryOnly = false, bVaultOnly = false;
    // The speed vault tests (on the court pier, with test obstacles).
    TArray<TWeakObjectPtr<AActor>> VaultBlocks;
    int32 VaultSub = 0, VaultsBefore = 0;
    float VaultJumpAt = -1, VaultMaxZ = 0;
    bool bVaultBench = false, bVaultStairs = false, bVaultWalk = false;
    int32 CrateIndex = 0, CratesVaulted = 0;
    // The pantry tests: the ladder up and down, a fall into a rupture, the jump at the cheese.
    int32 LadderMountsBefore = 0, LadderPullUpsBefore = 0, FallsBefore = 0, RespawnsBefore = 0;
    float LadderTopZ = -1e6f, PantryJumpAt = -1, IslandClosest = 1e6f;
    bool bLadderSeen = false, bLadderUp = false, bLadderDown = false, bRiftDeath = false, bReachedIsland = false;
    // The side wall run test (in a narrow stretch of the sewer).
    FVector SideStart = FVector::ZeroVector; FRotator SideFacing = FRotator::ZeroRotator;
    int32 SideSub = 0, SideRunsBefore = 0, SideClimbsBefore = 0, SideRunsAtWalk = 0;
    float SideJumpAt = -1, SideTravel = 0, SideRise = 0, SideAngle = 0;
    bool bSideRan = false, bSideInSewer = false;
    bool bWallLanded = false;
    TArray<int32> WallSides;
    int32 HangsBase = 0, PullUpsBase = 0, MantlesBase = 0;
    float HangAt = -1, HangZ = 0, HangZ2 = 0;
    bool bStillHanging = false;
    float ShimmyX0 = 0, ShimmyX1 = 0, ShimmyZ0 = 0, ShimmyZ1 = 0, CameraYawAtHang = 0;
    int32 InnerBase = 0, OuterBase = 0, RollsBase = 0;
    float CornerYaw = 999, CornerAt = 0;
    TArray<float> ChimneyCamYaws;
    bool bSideEntry = false;
    FString PrevGait;
    void ProbeLockedPaws(class AChuckCharacter* Chuck,float DeltaSeconds);
    int32 ProbeSamples = 0;
    double ProbeSlip = 0;
    float ProbeMaxSpeed = 0;
    float ProbeReachExcess = 0;
    int32 MotionFrame = 0;
    bool bMotionJump = false;
};

UCLASS()
class CHUCK3D_API ADockHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void DrawHUD() override;
private:
    TSharedPtr<class SWidget> MenuWidget;
    TMap<FString,TWeakPtr<class SButton>> MenuButtons;
    FString MenuTestPoint;
    int32 MenuTestStage = 0, MenuTestFailures = 0;
    double MenuTestNext = 0;
    TWeakObjectPtr<UWorld> MenuTestWorld;
    void ShowTitleMenu();
    void ShowMenu(bool Checkpoints);
};
