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
    void Check(bool Passed, const TCHAR* Description);
    int32 TestFailures = 0;
};

UCLASS()
class CHUCK3D_API ADockHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
