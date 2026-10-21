#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "ChultLanding.h"
#include "ChultCanopy.h"
#include "ChultBridge.h"
#include "ChultUpriver.h"
#include "ChultTempleInterior.h"
#include "DockSewer.h"
#include "DockPantry.h"
#include "DockReturn.h"
#include "SewerSlide.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
    // Every Chult spawn point has a dev checkpoint (user 2026-10-15). Recovery
    // is where a death there returns him; Yaw<-900 keeps the default facing.
    struct FChultPoint {const TCHAR* Point;const TCHAR* Label;};
    const FChultPoint ChultPoints[]={{TEXT("Chult"),TEXT("Chult Landing")},{TEXT("ChultCanopy"),TEXT("Chult Canopy Ruins")},
        {TEXT("ChultBridge"),TEXT("Chult Bridge")},{TEXT("ChultFarBank"),TEXT("Chult Far Bank")},{TEXT("ChultGiants"),TEXT("Chult Giant Trees")},
        {TEXT("ChultGiantCanopy"),TEXT("Chult Giant Canopy (dev)")},{TEXT("ChultDeepJungle"),TEXT("Chult Deep Jungle")},{TEXT("ChultTemple"),TEXT("Chult Temple")}};
    // The temple interior's spaces (2026-10-20), in a third column.
    const FChultPoint TemplePoints[]={{TEXT("ChultTempleWell"),TEXT("Temple Well")},{TEXT("ChultTempleHall"),TEXT("Temple Hall")},
        {TEXT("ChultTempleSpikes"),TEXT("Temple Spikes")},{TEXT("ChultTemplePiers"),TEXT("Temple Piers")},{TEXT("ChultTempleDarts"),TEXT("Temple Darts")}};
    bool ChultPoint(const FString& Point,FVector& Location,float& Yaw,FVector& Recovery)
    {
        Yaw=-1000;
        if(Point==TEXT("Chult")){Location=Recovery=AChultLanding::Entrance();return true;}
        if(Point==TEXT("ChultCanopy")){Location=Recovery=ChultCanopyRespawn();Yaw=ChultCanopyRespawnYaw();return true;}
        for(int32 I=0;I<UE_ARRAY_COUNT(TemplePoints);++I)
            if(Point==TemplePoints[I].Point){const auto S=ETempleInteriorSpawn(I);Location=Recovery=ChultTempleInteriorSpawn(S);Yaw=ChultTempleInteriorSpawnYaw(S);return true;}
        // By the bridge on the near bank: a death there (not in the gorge) is the landing's.
        if(Point==TEXT("ChultBridge")){Location=ChultBridgeSpawn(false);Recovery=AChultLanding::Entrance();Yaw=ChultBridgeRespawnYaw();return true;}
        const EChultUpriverSpawn Spawn=Point==TEXT("ChultFarBank")?EChultUpriverSpawn::FarBank:Point==TEXT("ChultGiants")?EChultUpriverSpawn::Giants:
            Point==TEXT("ChultGiantCanopy")?EChultUpriverSpawn::GiantCanopy:Point==TEXT("ChultTemple")?EChultUpriverSpawn::Temple:EChultUpriverSpawn::DeepJungle;
        if(Point!=TEXT("ChultFarBank")&&Point!=TEXT("ChultGiants")&&Point!=TEXT("ChultGiantCanopy")&&Point!=TEXT("ChultDeepJungle")&&Point!=TEXT("ChultTemple"))return false;
        Location=ChultUpriverSpawn(Spawn);Yaw=ChultUpriverSpawnYaw(Spawn);
        // Up in the first giant (no way up there yet but the dev checkpoint): a death returns him below.
        Recovery=Spawn==EChultUpriverSpawn::GiantCanopy?ChultUpriverSpawn(EChultUpriverSpawn::Giants):Location;
        return true;
    }
}

bool ADockGameMode::StartFromMenu(const FString& Point)
{
    auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    FVector ChultAt=FVector::ZeroVector,ChultRecovery=FVector::ZeroVector;float ChultYaw=-1000;
    const bool bChult=Point.StartsWith(TEXT("Chult"));
    if(!C || (Point!=TEXT("Waterdeep") && Point!=TEXT("Sewer") && Point!=TEXT("SewerJump") && Point!=TEXT("SewerLongJump") &&
              Point!=TEXT("Night") && Point!=TEXT("Pantry") && !(bChult && ChultPoint(Point,ChultAt,ChultYaw,ChultRecovery)))) return false;
    if(bChult)
    {
        FadeOutMusic(.1f);
        AChultLanding::EnterPreview(GetWorld());
        // Upriver spawns depend on the built grove; read them once it stands.
        ChultPoint(Point,ChultAt,ChultYaw,ChultRecovery);
    }
    const FVector Location=Point==TEXT("Sewer")?DockSewerStartLocation():
        Point==TEXT("SewerJump")?DockSewerCheckpointLocation():
        Point==TEXT("SewerLongJump")?DockSewerLeapCheckpointLocation():
        Point==TEXT("Pantry")?DockPantryStartLocation():bChult?ChultAt:AChuckCharacter::StartLocation();
    C->ResetAtLocation(Location);
    if(bChult && ChultYaw>-900) {C->SetActorRotation(FRotator(0,ChultYaw,0));C->Recenter();}
    if(Point==TEXT("Night") || Point==TEXT("Pantry")) MarkDockSewerExited();
    UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_START point=%s"),*Point);
    return true;
}

// Source-only placeholder menu: no authored UI assets or save system.
void ADockHUD::BeginPlay()
{
    Super::BeginPlay();
    const auto* Mode=GetWorld()->GetAuthGameMode();
    // Scripted verification/capture entry points retain their direct startup.
    if(FString(FCommandLine::Get()).Contains(TEXT("-Chuck")) ||
       (Mode && UGameplayStatics::HasOption(Mode->OptionsString,TEXT("ChuckStart")))) return;
#if !UE_BUILD_SHIPPING
    if(FParse::Value(FCommandLine::Get(),TEXT("MenuTest="),MenuTestPoint))
    {
        PrimaryActorTick.bTickEvenWhenPaused=true;
        SetActorTickEnabled(true);
        MenuTestWorld=GetWorld();
        MenuTestNext=FPlatformTime::Seconds()+.75;
    }
#endif
    GetWorldTimerManager().SetTimerForNextTick(this,&ADockHUD::ShowTitleMenu);
}

void ADockHUD::ShowTitleMenu() { ShowMenu(FParse::Param(FCommandLine::Get(),TEXT("MenuCheckpoints"))); }

void ADockHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateTutorial();
#if !UE_BUILD_SHIPPING
    // Exercise real Slate button delegates, including page changes and the
    // paused-world -> gameplay transition that direct map-option tests missed.
    if(MenuTestPoint.IsEmpty() || FPlatformTime::Seconds()<MenuTestNext) return;
    MenuTestNext=FPlatformTime::Seconds()+.25;
    auto Check=[this](bool Passed,const TCHAR* What)
    {
        MenuTestFailures+=!Passed;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_TEST %s: %s"),Passed?TEXT("PASS"):TEXT("FAIL"),What);
    };
    auto Click=[this,&Check](const TCHAR* Label)
    {
        auto Button=MenuButtons.FindRef(Label).Pin();
        Check(Button.IsValid(),Label);
        if(Button.IsValid()) Button->SimulateClick();
    };
    auto* PC=GetOwningPlayerController();
    if(MenuTestStage==0)
    {
        Check(MenuWidget.IsValid() && PC && PC->IsPaused() && MenuButtons.Num()==2,TEXT("title pauses the fresh world"));
        Click(TEXT("Dev Checkpoints"));
    }
    else if(MenuTestStage==1)
    {
        Check(MenuWidget.IsValid() && MenuButtons.Num()==7+UE_ARRAY_COUNT(ChultPoints)+UE_ARRAY_COUNT(TemplePoints) && PC && PC->IsPaused(),TEXT("checkpoint page keeps the world paused"));
        Click(TEXT("Back"));
    }
    else if(MenuTestStage==2)
    {
        Check(MenuButtons.Num()==2,TEXT("Back returns to title"));
        if(MenuTestPoint==TEXT("NewGame")) { Click(TEXT("New Game")); ++MenuTestStage; }
        else Click(TEXT("Dev Checkpoints"));
    }
    else if(MenuTestStage==3)
    {
        const TCHAR* Label=MenuTestPoint==TEXT("SewerLongJump")?TEXT("Sewer Long Jump"):
            MenuTestPoint==TEXT("SewerJump")?TEXT("Sewer Jump"):
            MenuTestPoint==TEXT("Night")?TEXT("Waterdeep Night"):
            MenuTestPoint==TEXT("Pantry")?TEXT("Tavern Pantry"):
            *MenuTestPoint;
        for(const FChultPoint& Point:ChultPoints) if(MenuTestPoint==Point.Point) Label=Point.Label;
        for(const FChultPoint& Point:TemplePoints) if(MenuTestPoint==Point.Point) Label=Point.Label;
        Click(Label);
    }
    else if(MenuTestStage==4)
    {
        Check(MenuTestWorld.Get()==GetWorld(),TEXT("selection retains the loaded world"));
        Check(!MenuWidget.IsValid() && PC && !PC->IsPaused() && !PC->bShowMouseCursor && !PC->IsMoveInputIgnored(),TEXT("selection closes menu and resumes gameplay input"));
        MenuTestNext=FPlatformTime::Seconds()+7.;
    }
    else if(MenuTestStage==5)
    {
        auto* C=Cast<AChuckCharacter>(GetOwningPawn());
        FVector ChultAt=FVector::ZeroVector,ChultRecovery=FVector::ZeroVector;float ChultYaw=0;
        const bool bChult=ChultPoint(MenuTestPoint,ChultAt,ChultYaw,ChultRecovery);
        const FVector Expected=MenuTestPoint==TEXT("Sewer")?DockSewerStartLocation():
            MenuTestPoint==TEXT("SewerJump")?DockSewerCheckpointLocation():
            MenuTestPoint==TEXT("SewerLongJump")?DockSewerLeapCheckpointLocation():
            MenuTestPoint==TEXT("Pantry")?DockPantryStartLocation():bChult?ChultAt:AChuckCharacter::StartLocation();
        const FVector Recovery=bChult?ChultRecovery:Expected;
        if(C) UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_SPAWN point=%s actual=%s expected=%s distance=%.2f movement=%d possessed=%d"),
            *MenuTestPoint,*C->GetActorLocation().ToString(),*Expected.ToString(),FVector::Dist(C->GetActorLocation(),Expected),
            int32(C->GetCharacterMovement()->MovementMode),PC && PC->GetPawn()==C);
        Check(C && PC && PC->GetPawn()==C && C->GetCharacterMovement()->IsMovingOnGround() &&
            FVector::Dist(C->GetActorLocation(),Expected)<45.f,TEXT("chosen spawn settles on its floor and stays possessed"));
        Check(C && C->GetLandingRolls()==0,TEXT("checkpoint placement does not trigger a long-fall landing roll"));
        Check(C && FVector::Dist(C->GetAreaStartLocation(),Recovery)<1.f,TEXT("chosen location has the correct death recovery point"));
        if(bChult && ChultYaw>-900) Check(C && FMath::Abs(FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw,ChultYaw))<25.f,TEXT("chosen Chult spawn faces its way on"));
        Check(GetWorld()->GetTimeSeconds()>5.f && MenuTestWorld.Get()==GetWorld(),TEXT("gameplay and region timers run after selection without reloading"));
        Check(HasExitedDockSewer()==(MenuTestPoint==TEXT("Night") || MenuTestPoint==TEXT("Pantry")),TEXT("destination has the expected morning or evening progression state"));
        // Outdoor sun/sky are intentionally disabled while the camera is in the
        // sewer, so the surface-lighting assertion is only valid above ground.
        if(!bChult && MenuTestPoint!=TEXT("Sewer") && MenuTestPoint!=TEXT("SewerJump") && MenuTestPoint!=TEXT("SewerLongJump"))
            Check(CheckDockReturn(GetWorld(),MenuTestPoint==TEXT("Night") || MenuTestPoint==TEXT("Pantry")),TEXT("surface doors, hatch and lighting match the destination"));
        const FString Folder=FPaths::ScreenShotDir()/TEXT("MenuStarts");
        IFileManager::Get().MakeDirectory(*Folder,true);
        FScreenshotRequest::RequestScreenshot(Folder/MenuTestPoint+TEXT(".png"),true,false);
        MenuTestNext=FPlatformTime::Seconds()+.5;
    }
    else if(MenuTestStage==6 && MenuTestPoint==TEXT("SewerLongJump"))
    {
        auto* C=Cast<AChuckCharacter>(GetOwningPawn());
        if(C)
        {
            MenuTestFalls=C->GetFallDeaths();MenuTestRespawns=C->GetRespawns();
            C->ResetAtLocation(DockSewerPoint(DockSewerLeapRiftStart()+2)+FVector(0,0,35));
            C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
        MenuTestNext=FPlatformTime::Seconds()+12.;
    }
    else if(MenuTestStage==7 && MenuTestPoint==TEXT("SewerLongJump"))
    {
        auto* C=Cast<AChuckCharacter>(GetOwningPawn());
        Check(C && C->GetFallDeaths()>MenuTestFalls && C->GetRespawns()>MenuTestRespawns &&
            C->GetCharacterMovement()->IsMovingOnGround() && !C->IsAstral() &&
            FVector::Dist(C->GetActorLocation(),DockSewerLeapCheckpointLocation())<45.f,
            TEXT("falling into the long jump returns to its local checkpoint"));
    }
    else
    {
        UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_TEST_COMPLETE failures=%d point=%s"),MenuTestFailures,*MenuTestPoint);
        MenuTestPoint.Empty();
        FPlatformMisc::RequestExit(false);
        return;
    }
    ++MenuTestStage;
#endif
}

void ADockHUD::ShowMenu(bool Checkpoints)
{
    auto* PC=GetOwningPlayerController();
    if(!PC || !GEngine || !GEngine->GameViewport) return;
    if(MenuWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
    MenuButtons.Reset();
    TSharedPtr<SVerticalBox> Rows;
    MenuWidget=SNew(SBorder).BorderBackgroundColor(FLinearColor(.018f,.014f,.027f,1.f))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(Checkpoints?1100.f:360.f)
            [SAssignNew(Rows,SVerticalBox)]];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,32)
        [SNew(STextBlock).Text(FText::FromString(TEXT("CHUCK 3D")))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",40)).Justification(ETextJustify::Center)];
    if(Checkpoints) Rows->AddSlot().AutoHeight().Padding(0,0,0,20)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Dev Checkpoints"))).Justification(ETextJustify::Center)];
    TSharedPtr<SButton> First;
    // The checkpoint page has three columns: Waterdeep, Chult and the temple interior.
    TSharedPtr<SVerticalBox> Target=Rows,Left,Right,Third;
    if(Checkpoints) Rows->AddSlot().AutoHeight()
        [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[SAssignNew(Left,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,12,0)[SAssignNew(Right,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,0,0)[SAssignNew(Third,SVerticalBox)]];
    auto Add=[&](const TCHAR* Label,TFunction<FReply()> Action)
    {
        TSharedPtr<SButton> Button;
        Target->AddSlot().AutoHeight().Padding(0,6)
            [SAssignNew(Button,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20,14))
            .OnClicked_Lambda(MoveTemp(Action))
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",20))]];
        if(!First.IsValid()) First=Button;
        MenuButtons.Add(Label,Button);
    };
    auto Start=[this](const TCHAR* Point)
    {
        // This world is fresh and paused. Reloading it destroys actors while the
        // prototype's shared timer manager still has callbacks for the old world.
        auto* Mode=GetWorld()->GetAuthGameMode<ADockGameMode>();
        if(!Mode || !Mode->StartFromMenu(Point)) return FReply::Handled();
        GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
        MenuWidget.Reset();
        auto* Controller=GetOwningPlayerController();
        Controller->SetPause(false); Controller->bShowMouseCursor=false;
        Controller->SetInputMode(FInputModeGameOnly());
        return FReply::Handled();
    };
    if(!Checkpoints)
    {
        Add(TEXT("New Game"),[Start](){return Start(TEXT("Waterdeep"));});
        Add(TEXT("Dev Checkpoints"),[this](){ShowMenu(true);return FReply::Handled();});
    }
    else
    {
        Target=Left;
        Add(TEXT("Waterdeep"),[Start](){return Start(TEXT("Waterdeep"));});
        Add(TEXT("Sewer"),[Start](){return Start(TEXT("Sewer"));});
        Add(TEXT("Sewer Jump"),[Start](){return Start(TEXT("SewerJump"));});
        Add(TEXT("Sewer Long Jump"),[Start](){return Start(TEXT("SewerLongJump"));});
        Add(TEXT("Waterdeep Night"),[Start](){return Start(TEXT("Night"));});
        Add(TEXT("Tavern Pantry"),[Start](){return Start(TEXT("Pantry"));});
        Target=Right;
        for(const FChultPoint& Point:ChultPoints) {const TCHAR* Name=Point.Point;Add(Point.Label,[Start,Name](){return Start(Name);});}
        Target=Third;
        for(const FChultPoint& Point:TemplePoints) {const TCHAR* Name=Point.Point;Add(Point.Label,[Start,Name](){return Start(Name);});}
        Target=Rows;
        Add(TEXT("Back"),[this](){ShowMenu(false);return FReply::Handled();});
    }
    GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(),100);
    PC->SetPause(true); PC->bShowMouseCursor=true;
    FInputModeUIOnly Input; Input.SetWidgetToFocus(First); PC->SetInputMode(Input);
    FSlateApplication::Get().SetKeyboardFocus(First);
    FSlateApplication::Get().SetUserFocus(0,First);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU page=%s"),Checkpoints?TEXT("checkpoints"):TEXT("title"));
}
