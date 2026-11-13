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
#include "ShipSetting.h"
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
#include "ShipStorm.h"
#include "ShipEnding.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "ChuckSave.h"
#include "ChuckShots.h"
#include "ShipEscape.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

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
        {TEXT("ChultTempleSpikes"),TEXT("Temple Spikes")},{TEXT("ChultTemplePiers"),TEXT("Temple Piers")},{TEXT("ChultTempleDarts"),TEXT("Temple Darts")},{TEXT("ChultTempleSnakes"),TEXT("Temple Snakes")},
        // The trap run (2026-10-22), in a fourth column: in ETempleInteriorSpawn's order after Snakes.
        {TEXT("ChultTempleClimb"),TEXT("Temple Climb")},{TEXT("ChultTemplePits"),TEXT("Temple Pits")},{TEXT("ChultTempleAxes"),TEXT("Temple Axes")},
        {TEXT("ChultTempleFire"),TEXT("Temple Fire")},{TEXT("ChultTempleSaws"),TEXT("Temple Saws")},{TEXT("ChultTempleGauntlet"),TEXT("Temple Gauntlet")},
        // The Astral wind and the sanctum (2026-10-22), in a fifth column.
        {TEXT("ChultTempleWind1"),TEXT("Astral Wind 1")},{TEXT("ChultTempleWind2"),TEXT("Astral Wind 2")},{TEXT("ChultTempleWind3"),TEXT("Astral Wind 3")},
        {TEXT("ChultTempleWind4"),TEXT("Astral Wind 4")},{TEXT("ChultTempleWind5"),TEXT("Astral Wind 5")},{TEXT("ChultTempleWind6"),TEXT("Astral Wind 6")},
        {TEXT("ChultTempleWind7"),TEXT("Astral Wind 7")},{TEXT("ChultTempleSanctum"),TEXT("Temple Sanctum")},{TEXT("ChultTempleRubble"),TEXT("Temple Rubble")}};
    constexpr int32 TempleColumn=6,WindColumn=12;
    // The pirate ship (2026-11-01; ShipSetting.h), in a sixth column, in EShipSpawn's order.
    const FChultPoint ShipPoints[]={{TEXT("ShipArrival"),TEXT("Ship Arrival")},{TEXT("ShipHold"),TEXT("Ship Hold")},{TEXT("ShipQuarters"),TEXT("Ship Quarters")},
        {TEXT("ShipGalley"),TEXT("Ship Galley")},{TEXT("ShipCabin"),TEXT("Ship Cabin")},{TEXT("ShipDeck"),TEXT("Ship Deck")},
        // The storm (2026-11-03, ShipStorm.h): on deck with the ending's seven conditions met, as it rolls in.
        {TEXT("ShipStorm"),TEXT("Ship Storm (dev)")},
        // (2026-11-04) the same, some seconds before the storm's second part (the user's intense track).
        {TEXT("ShipStormIntense"),TEXT("Ship Storm Intense (dev)")},
        // The ending (2026-11-08, ShipEnding.h): the orders begun, the leap from the main top, the fore top.
        {TEXT("ShipReef"),TEXT("Ship Reef (dev)")},{TEXT("ShipLeap"),TEXT("Ship Foremast Leap (dev)")},{TEXT("ShipForeTop"),TEXT("Ship Fore Topmast (dev)")}};
    bool ShipPoint(const FString& Point,FVector& Location,float& Yaw)
    {
        for(int32 I=0;I<UE_ARRAY_COUNT(ShipPoints);++I)
            if(Point==ShipPoints[I].Point)
            {
                const EShipSpawn S=I<ShipSpawns?EShipSpawn(I):EShipSpawn::Deck;Location=ShipSpawn(S);Yaw=ShipSpawnYaw(S);
                // The ending's checkpoints aloft (2026-11-08): the main top's port run-up lane, the fore top.
                if(Point==TEXT("ShipLeap")) {Location=ShipWorld(FVector(Ship::MainMastX-40,70,Ship::MainTopZ+34.65f));Yaw=0;}
                if(Point==TEXT("ShipForeTop")) {Location=AShipEnding::ForeTopSpot(70.f)+FVector(0,0,1.15f);Yaw=90;}
                return true;
            }
        return false;
    }   // the interior's first six in the third column, the trap run's in the fourth, the wind's in the fifth
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
    FVector ShipAt;float ShipYaw=0;
    if(C && ShipPoint(Point,ShipAt,ShipYaw))
    {
        FadeOutMusic(.1f);
        AShipSetting::Enter(GetWorld());
        // A fresh start aboard: the ending's conditions forgotten (the chest shut, no storm); the storm checkpoint meets them all.
        ShipGate::Reset(GetWorld());
        if(Point==TEXT("ShipStorm") || Point==TEXT("ShipStormIntense")) ShipGate::OpenAll();
        if(Point==TEXT("ShipStormIntense")) if(AShipStorm* Storm=AShipStorm::Get(GetWorld())) Storm->SetClock(FMath::Max(1.f,Storm->GetIntenseAt()-8.f));
        const bool bEnding=Point==TEXT("ShipReef") || Point==TEXT("ShipLeap") || Point==TEXT("ShipForeTop");
        if(bEnding)
        {
            ShipGate::OpenAll();
            if(AShipStorm* Storm=AShipStorm::Get(GetWorld())) Storm->SetClock(Storm->GetIntenseAt()+(Point==TEXT("ShipReef") ? 6.f : 30.f));
            if(AShipEnding* End=AShipEnding::Get(GetWorld()))
                End->SkipTo(Point==TEXT("ShipReef") ? AShipEnding::EStep::Waiting : Point==TEXT("ShipLeap") ? AShipEnding::EStep::Leap : AShipEnding::EStep::ForeClimb);
        }
        C->ResetAtLocation(ShipAt);C->SetActorRotation(FRotator(0,ShipYaw,0));C->Recenter();
        UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU_START point=%s"),*Point);
        return true;
    }
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

bool ADockGameMode::CurrentSavePoint(FString& Point,FString& Label) const
{
    const auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!C) return false;
    // Where a death would return him: the checkpoint he last reached. Match it to the nearest named
    // checkpoint (by the place each one recovers to) among the regions built so far.
    const FVector Back=C->GetAreaStartLocation();
    float Best=TNumericLimits<float>::Max();
    auto Consider=[&](const TCHAR* Name,const TCHAR* Text,const FVector& Recovery)
    {
        const float D=FVector::Dist(Back,Recovery);
        if(D<Best){Best=D;Point=Name;Label=Text;}
    };
    Consider(TEXT("Waterdeep"),TEXT("Waterdeep"),AChuckCharacter::StartLocation());
    Consider(TEXT("Sewer"),TEXT("Sewer"),DockSewerStartLocation());
    Consider(TEXT("SewerJump"),TEXT("Sewer Jump"),DockSewerCheckpointLocation());
    Consider(TEXT("SewerLongJump"),TEXT("Sewer Long Jump"),DockSewerLeapCheckpointLocation());
    Consider(TEXT("Pantry"),TEXT("Tavern Pantry"),DockPantryStartLocation());
    UWorld* World=GetWorld();
    bool bChult=false; for(TActorIterator<AChultLanding> It(World);It;++It) bChult=true;
    if(bChult)
    {
        auto Each=[&](const FChultPoint* Points,int32 Count)
        {
            for(int32 I=0;I<Count;++I)
            {
                FVector At,Recovery;float Yaw;
                if(ChultPoint(Points[I].Point,At,Yaw,Recovery)) Consider(Points[I].Point,Points[I].Label,Recovery);
            }
        };
        Each(ChultPoints,UE_ARRAY_COUNT(ChultPoints));
        Each(TemplePoints,UE_ARRAY_COUNT(TemplePoints));
    }
    if(AShipSetting::Get(World))
        for(const FChultPoint& P : ShipPoints)
        {
            FVector At;float Yaw;
            if(!ShipPoint(P.Point,At,Yaw)) continue;
            FVector Recovery=At,Deck; if(ShipRespawn(At,Deck)) Recovery=Deck;
            Consider(P.Point,P.Label,Recovery);
        }
    // The docks after the sewer are the evening's.
    if(HasExitedDockSewer() && Point==TEXT("Waterdeep")) {Point=TEXT("Night");Label=TEXT("Waterdeep Night");}
    // The dev markers are dropped from the label the player sees.
    Label.ReplaceInline(TEXT(" (dev)"),TEXT(""));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SAVE_POINT point=%s distance=%.1f recovery=%s"),*Point,Best,*Back.ToString());
    return !Point.IsEmpty();
}

bool ADockGameMode::SaveGame(FString* SavedLabel)
{
    FString Point,Label;
    const auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!C || !CurrentSavePoint(Point,Label)) return false;
    if(SavedLabel) *SavedLabel=Label;
    return ChuckSave::Write(Point,Label,HasExitedDockSewer(),C->GetCigarettes(),C->GetSanity());
}

bool ADockGameMode::ContinueFromSave()
{
    const UChuckSaveGame* Save=ChuckSave::Load();
    auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Save || !C || !StartFromMenu(Save->Point)) return false;
    if(Save->bEvening) MarkDockSewerExited();
    C->SetCigarettes(Save->Cigarettes); C->SetSanity(Save->Sanity);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SAVE_CONTINUE point=%s evening=%d cigarettes=%d"),*Save->Point,Save->bEvening?1:0,C->GetCigarettes());
    return true;
}

// Source-only menus (Slate): the title, the dev checkpoints and the pause menu.
void ADockHUD::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    // -ChuckPauseTest[=<checkpoint>]: the pause menu, saving there and continuing from the title (TickPauseTest).
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPauseTest")) || FParse::Value(FCommandLine::Get(),TEXT("ChuckPauseTest="),PauseTestPoint))
    {
        if(PauseTestPoint.IsEmpty()) PauseTestPoint=TEXT("Waterdeep");
        IFileManager::Get().Delete(*ChuckSave::SlotPath());
        SetTickableWhenPaused(true);
        PauseTestNext=FPlatformTime::Seconds()+3.;
    }
#endif
    if(ChuckShots::Start(this)) return;   // -ChuckShots=<file>: stills from a shot list (ChuckShots.h)
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
    if(auto* PC=GetOwningPlayerController())
    {
        const bool bPausePressed=PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Gamepad_Special_Right);
        if(bPauseOpen) { if(bPausePressed) ClosePause(); }
        else if(bPausePressed && CanPause()) ShowPause();
    }
    if(!bPauseOpen) UpdateTutorial();
#if !UE_BUILD_SHIPPING
    if(!PauseTestPoint.IsEmpty()) TickPauseTest();
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
        Check(MenuWidget.IsValid() && PC && PC->IsPaused() && MenuButtons.Num()==(ChuckSave::Load()?3:2),TEXT("title pauses the fresh world"));
        Click(TEXT("Dev Checkpoints"));
    }
    else if(MenuTestStage==1)
    {
        Check(MenuWidget.IsValid() && MenuButtons.Num()==7+UE_ARRAY_COUNT(ChultPoints)+UE_ARRAY_COUNT(TemplePoints)+UE_ARRAY_COUNT(ShipPoints) && PC && PC->IsPaused(),TEXT("checkpoint page keeps the world paused"));
        // -MenuTestShot: a picture of the page (its five columns), held a moment before Back.
        if(FParse::Param(FCommandLine::Get(),TEXT("MenuTestShot")) && !bMenuShot)
        {
            bMenuShot=true;FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Menu_Checkpoints.png"),true,false);MenuTestNext=FPlatformTime::Seconds()+1.5;return;
        }
        Click(TEXT("Back"));
    }
    else if(MenuTestStage==2)
    {
        Check(MenuButtons.Num()==(ChuckSave::Load()?3:2),TEXT("Back returns to title"));
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
        for(const FChultPoint& Point:ShipPoints) if(MenuTestPoint==Point.Point) Label=Point.Label;
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
        bool bChult=ChultPoint(MenuTestPoint,ChultAt,ChultYaw,ChultRecovery);
        FVector ShipAt;float ShipYaw=0;
        if(ShipPoint(MenuTestPoint,ShipAt,ShipYaw))
        {
            bChult=true;ChultAt=ChultRecovery=ShipAt;ChultYaw=ShipYaw;   // (aboard, checked as Chult's points are)
            FVector Back; if(ShipRespawn(ShipAt,Back)) ChultRecovery=Back;   // (aloft, the ending's checkpoints: a death returns him to the deck)
        }
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
        [SNew(SBox).WidthOverride(Checkpoints?1900.f:360.f)   // six columns of checkpoints since 2026-11-01
            [SAssignNew(Rows,SVerticalBox)]];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,32)
        [SNew(STextBlock).Text(FText::FromString(TEXT("CHUCK 3D")))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",40)).Justification(ETextJustify::Center)];
    if(Checkpoints) Rows->AddSlot().AutoHeight().Padding(0,0,0,20)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Dev Checkpoints"))).Justification(ETextJustify::Center)];
    TSharedPtr<SButton> First;
    // The checkpoint page has five columns: Waterdeep, Chult, the temple interior, its trap run, the Astral wind.
    TSharedPtr<SVerticalBox> Target=Rows,Left,Right,Third,Fourth,Fifth,Sixth;
    if(Checkpoints) Rows->AddSlot().AutoHeight()
        [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[SAssignNew(Left,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,12,0)[SAssignNew(Right,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,12,0)[SAssignNew(Third,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,12,0)[SAssignNew(Fourth,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,12,0)[SAssignNew(Fifth,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,0,0)[SAssignNew(Sixth,SVerticalBox)]];
    auto Add=[&](const TCHAR* Label,TFunction<FReply()> Action)
    {
        TSharedPtr<SButton> Button;
        Target->AddSlot().AutoHeight().Padding(0,6)
            [SAssignNew(Button,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20,14))
            .OnClicked_Lambda(MoveTemp(Action))
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Checkpoints?18:20))]];
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
        if(ChuckSave::Load())
            Add(TEXT("Continue"),[this]()
            {
                auto* Mode=GetWorld()->GetAuthGameMode<ADockGameMode>();
                if(!Mode || !Mode->ContinueFromSave()) return FReply::Handled();
                GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
                MenuWidget.Reset();
                auto* Controller=GetOwningPlayerController();
                Controller->SetPause(false); Controller->bShowMouseCursor=false;
                Controller->SetInputMode(FInputModeGameOnly());
                return FReply::Handled();
            });
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
        for(int32 I=0;I<UE_ARRAY_COUNT(TemplePoints);++I)
        {
            Target=I<TempleColumn?Third:I<WindColumn?Fourth:Fifth;
            const TCHAR* Name=TemplePoints[I].Point;Add(TemplePoints[I].Label,[Start,Name](){return Start(Name);});
        }
        Target=Sixth;
        for(const FChultPoint& Point:ShipPoints) {const TCHAR* Name=Point.Point;Add(Point.Label,[Start,Name](){return Start(Name);});}
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

bool ADockHUD::CanPause() const
{
    const auto* PC=GetOwningPlayerController();
    const auto* C=Cast<AChuckCharacter>(GetOwningPawn());
    if(!PC || !C || MenuWidget.IsValid() || PC->IsPaused()) return false;
    // Esc skips cutscenes and ends the demo: only in play, his own camera on him.
    if(C->HasDemoEnded() || C->IsInFallCutscene() || AShipEscape::IsLetterboxed(GetWorld())) return false;
    if(PC->GetViewTarget()!=C) return false;
    // Scripted verification runs keep Esc for their own cutscene checks.
    if(PauseTestPoint.IsEmpty() && FString(FCommandLine::Get()).Contains(TEXT("-Chuck"))) return false;
    return true;
}

void ADockHUD::ShowPause()
{
    auto* PC=GetOwningPlayerController();
    if(!PC || !GEngine || !GEngine->GameViewport) return;
    MenuButtons.Reset();
    TSharedPtr<SVerticalBox> Rows;
    TSharedPtr<SButton> First;
    MenuWidget=SNew(SBorder).BorderBackgroundColor(FLinearColor(.018f,.014f,.027f,.82f))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(360.f)[SAssignNew(Rows,SVerticalBox)]];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,28)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Paused")))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",34)).Justification(ETextJustify::Center)];
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
    Add(TEXT("Resume"),[this](){ClosePause();return FReply::Handled();});
    Add(TEXT("Save Game"),[this]()
    {
        auto* Mode=GetWorld()->GetAuthGameMode<ADockGameMode>();
        FString Label;
        const bool bSaved=Mode && Mode->SaveGame(&Label);
        if(PauseStatus.IsValid()) PauseStatus->SetText(FText::FromString(bSaved ? FString::Printf(TEXT("Saved: %s"),*Label) : FString(TEXT("Could not save"))));
        return FReply::Handled();
    });
    // Later options (the tutorial text, the golden guide lights) go here, above Quit.
    Add(TEXT("Quit Game"),[this](){UKismetSystemLibrary::QuitGame(this,GetOwningPlayerController(),EQuitPreference::Quit,false);return FReply::Handled();});
    FString Last;
    if(const UChuckSaveGame* Save=ChuckSave::Load()) Last=FString::Printf(TEXT("Last save: %s"),*Save->Label);
    Rows->AddSlot().AutoHeight().Padding(0,18,0,0)
        [SAssignNew(PauseStatus,STextBlock).Text(FText::FromString(Last)).Justification(ETextJustify::Center)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(FLinearColor(.8f,.78f,.84f))];
    GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(),100);
    bPauseOpen=true;
    SetTickableWhenPaused(true);
    PC->SetPause(true); PC->bShowMouseCursor=true;
    // Game and UI, so Esc / Start still reach this HUD to close it.
    FInputModeGameAndUI Input; Input.SetWidgetToFocus(First); Input.SetHideCursorDuringCapture(false); PC->SetInputMode(Input);
    FSlateApplication::Get().SetKeyboardFocus(First);
    FSlateApplication::Get().SetUserFocus(0,First);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU page=pause"));
}

void ADockHUD::ClosePause()
{
    auto* PC=GetOwningPlayerController();
    if(MenuWidget.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
    MenuWidget.Reset(); MenuButtons.Reset(); PauseStatus.Reset();
    bPauseOpen=false;
    if(PauseTestPoint.IsEmpty()) SetTickableWhenPaused(false);
    if(PC) { PC->SetPause(false); PC->bShowMouseCursor=false; PC->SetInputMode(FInputModeGameOnly()); }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_MENU page=closed"));
}

void ADockHUD::TickPauseTest()
{
#if !UE_BUILD_SHIPPING
    if(FPlatformTime::Seconds()<PauseTestNext) return;
    PauseTestNext=FPlatformTime::Seconds()+.5;
    auto* PC=GetOwningPlayerController();
    auto* C=Cast<AChuckCharacter>(GetOwningPawn());
    auto* Mode=GetWorld()->GetAuthGameMode<ADockGameMode>();
    auto Check=[this](bool Passed,const TCHAR* What)
    {
        PauseTestFailures+=!Passed;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PAUSE_TEST %s: %s"),Passed?TEXT("PASS"):TEXT("FAIL"),What);
    };
    auto Press=[PC](){ if(PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape,IE_Pressed,1)); };
    auto Click=[this,&Check](const TCHAR* Label)
    {
        auto Button=MenuButtons.FindRef(Label).Pin();
        Check(Button.IsValid(),Label);
        if(Button.IsValid()) Button->SimulateClick();
    };
    switch(PauseTestStage++)
    {
    case 0:
        if(PauseTestPoint!=TEXT("Waterdeep")) Check(Mode && Mode->StartFromMenu(PauseTestPoint),TEXT("the test checkpoint starts"));
        if(C) C->SetCigarettes(3);
        PauseTestNext=FPlatformTime::Seconds()+5.;
        break;
    case 1: Press(); break;
    case 2:
        Check(bPauseOpen && PC && PC->IsPaused() && MenuButtons.Num()==3,TEXT("Esc opens the pause menu and pauses the world"));
        Click(TEXT("Save Game"));
        break;
    case 3:
    {
        const UChuckSaveGame* Save=ChuckSave::Load();
        Check(Save!=nullptr,TEXT("Save Game writes the save"));
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PAUSE_TEST saved=%s expected=%s"),Save?*Save->Point:TEXT("-"),*PauseTestPoint);
        Check(Save && Save->Point==PauseTestPoint && Save->Cigarettes==(C?C->GetCigarettes():-1),TEXT("the save holds this checkpoint and the cigarette count"));
        Check(bPauseOpen && PC && PC->IsPaused(),TEXT("saving keeps the menu open"));
        const FString Folder=FPaths::ScreenShotDir()/TEXT("Pause");
        IFileManager::Get().MakeDirectory(*Folder,true);
        FScreenshotRequest::RequestScreenshot(Folder/PauseTestPoint+TEXT("_menu.png"),true,false);
        break;
    }
    case 4: Click(TEXT("Resume")); break;
    case 5:
        Check(!bPauseOpen && PC && !PC->IsPaused() && !PC->bShowMouseCursor,TEXT("Resume closes the menu and resumes play"));
        Press();
        break;
    case 6: Check(bPauseOpen,TEXT("Esc opens it again")); Press(); break;
    case 7:
        Check(!bPauseOpen && PC && !PC->IsPaused(),TEXT("Esc closes it"));
        ShowMenu(false);   // the title, as on a fresh start
        break;
    case 8:
        Check(MenuButtons.Contains(TEXT("Continue")),TEXT("the title offers Continue"));
        if(C) C->SetCigarettes(0);   // a fresh start's count, which Continue restores
        Click(TEXT("Continue"));
        PauseTestNext=FPlatformTime::Seconds()+5.;
        break;
    case 9:
    {
        FString Point,Label;
        const bool bHere=Mode && Mode->CurrentSavePoint(Point,Label);
        Check(!MenuWidget.IsValid() && PC && !PC->IsPaused(),TEXT("Continue closes the title and plays"));
        Check(bHere && Point==PauseTestPoint && C && C->GetCharacterMovement()->IsMovingOnGround(),TEXT("Continue starts at the saved checkpoint, on its floor"));
        Check(C && C->GetCigarettes()==3,TEXT("Continue restores the cigarette count"));
        const FString Folder=FPaths::ScreenShotDir()/TEXT("Pause");
        IFileManager::Get().MakeDirectory(*Folder,true);
        FScreenshotRequest::RequestScreenshot(Folder/PauseTestPoint+TEXT("_continue.png"),true,false);
        PauseTestNext=FPlatformTime::Seconds()+1.;
        break;
    }
    default:
        IFileManager::Get().Delete(*ChuckSave::SlotPath());
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PAUSE_TEST_COMPLETE failures=%d point=%s"),PauseTestFailures,*PauseTestPoint);
        PauseTestPoint.Empty();
        FPlatformMisc::RequestExit(false);
    }
#endif
}
