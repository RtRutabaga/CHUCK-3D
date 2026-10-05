#include "DockGameMode.h"
#include "ChuckCharacter.h"
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

bool ADockGameMode::StartFromMenu(const FString& Point)
{
    auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!C || (Point!=TEXT("Waterdeep") && Point!=TEXT("Sewer") && Point!=TEXT("SewerJump") &&
              Point!=TEXT("Night") && Point!=TEXT("Pantry"))) return false;
    const FVector Location=Point==TEXT("Sewer")?DockSewerStartLocation():
        Point==TEXT("SewerJump")?DockSewerCheckpointLocation():
        Point==TEXT("Pantry")?DockPantryStartLocation():AChuckCharacter::StartLocation();
    C->ResetAtLocation(Location);
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
        Check(MenuWidget.IsValid() && MenuButtons.Num()==6 && PC && PC->IsPaused(),TEXT("checkpoint page keeps the world paused"));
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
        const TCHAR* Label=MenuTestPoint==TEXT("SewerJump")?TEXT("Sewer Jump"):
            MenuTestPoint==TEXT("Night")?TEXT("Waterdeep Night"):
            MenuTestPoint==TEXT("Pantry")?TEXT("Tavern Pantry"):*MenuTestPoint;
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
        const FVector Expected=MenuTestPoint==TEXT("Sewer")?DockSewerStartLocation():
            MenuTestPoint==TEXT("SewerJump")?DockSewerCheckpointLocation():
            MenuTestPoint==TEXT("Pantry")?DockPantryStartLocation():AChuckCharacter::StartLocation();
        if(C) UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_SPAWN point=%s actual=%s expected=%s distance=%.2f movement=%d possessed=%d"),
            *MenuTestPoint,*C->GetActorLocation().ToString(),*Expected.ToString(),FVector::Dist(C->GetActorLocation(),Expected),
            int32(C->GetCharacterMovement()->MovementMode),PC && PC->GetPawn()==C);
        Check(C && PC && PC->GetPawn()==C && C->GetCharacterMovement()->IsMovingOnGround() &&
            FVector::Dist(C->GetActorLocation(),Expected)<45.f,TEXT("chosen spawn settles on its floor and stays possessed"));
        Check(C && C->GetLandingRolls()==0,TEXT("checkpoint placement does not trigger a long-fall landing roll"));
        Check(C && FVector::Dist(C->GetAreaStartLocation(),Expected)<1.f,TEXT("chosen location has the correct death recovery point"));
        Check(GetWorld()->GetTimeSeconds()>5.f && MenuTestWorld.Get()==GetWorld(),TEXT("gameplay and region timers run after selection without reloading"));
        Check(HasExitedDockSewer()==(MenuTestPoint==TEXT("Night") || MenuTestPoint==TEXT("Pantry")),TEXT("destination has the expected morning or evening progression state"));
        // Outdoor sun/sky are intentionally disabled while the camera is in the
        // sewer, so the surface-lighting assertion is only valid above ground.
        if(MenuTestPoint!=TEXT("Sewer") && MenuTestPoint!=TEXT("SewerJump"))
            Check(CheckDockReturn(GetWorld(),MenuTestPoint==TEXT("Night") || MenuTestPoint==TEXT("Pantry")),TEXT("surface doors, hatch and lighting match the destination"));
        const FString Folder=FPaths::ScreenShotDir()/TEXT("MenuStarts");
        IFileManager::Get().MakeDirectory(*Folder,true);
        FScreenshotRequest::RequestScreenshot(Folder/MenuTestPoint+TEXT(".png"),true,false);
        MenuTestNext=FPlatformTime::Seconds()+.5;
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
        [SNew(SBox).WidthOverride(360.f)
            [SAssignNew(Rows,SVerticalBox)]];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,32)
        [SNew(STextBlock).Text(FText::FromString(TEXT("CHUCK 3D")))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",40)).Justification(ETextJustify::Center)];
    if(Checkpoints) Rows->AddSlot().AutoHeight().Padding(0,0,0,20)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Dev Checkpoints"))).Justification(ETextJustify::Center)];
    TSharedPtr<SButton> First;
    auto Add=[&](const TCHAR* Label,TFunction<FReply()> Action)
    {
        TSharedPtr<SButton> Button;
        Rows->AddSlot().AutoHeight().Padding(0,6)
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
        Add(TEXT("Waterdeep"),[Start](){return Start(TEXT("Waterdeep"));});
        Add(TEXT("Sewer"),[Start](){return Start(TEXT("Sewer"));});
        Add(TEXT("Sewer Jump"),[Start](){return Start(TEXT("SewerJump"));});
        Add(TEXT("Waterdeep Night"),[Start](){return Start(TEXT("Night"));});
        Add(TEXT("Tavern Pantry"),[Start](){return Start(TEXT("Pantry"));});
        Add(TEXT("Back"),[this](){ShowMenu(false);return FReply::Handled();});
    }
    GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(),100);
    PC->SetPause(true); PC->bShowMouseCursor=true;
    FInputModeUIOnly Input; Input.SetWidgetToFocus(First); PC->SetInputMode(Input);
    FSlateApplication::Get().SetKeyboardFocus(First);
    FSlateApplication::Get().SetUserFocus(0,First);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU page=%s"),Checkpoints?TEXT("checkpoints"):TEXT("title"));
}
