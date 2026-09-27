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
private:
    bool bSmokeTest = false;
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
    virtual void DrawHUD() override;
};
