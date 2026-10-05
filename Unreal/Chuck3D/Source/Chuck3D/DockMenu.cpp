#include "DockGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
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

// Source-only placeholder menu: no authored UI assets or save system.
void ADockHUD::BeginPlay()
{
    Super::BeginPlay();
    const auto* Mode=GetWorld()->GetAuthGameMode();
    // Scripted verification/capture entry points retain their direct startup.
    if(FString(FCommandLine::Get()).Contains(TEXT("-Chuck")) ||
       (Mode && UGameplayStatics::HasOption(Mode->OptionsString,TEXT("ChuckStart")))) return;
    GetWorldTimerManager().SetTimerForNextTick(this,&ADockHUD::ShowTitleMenu);
}

void ADockHUD::ShowTitleMenu() { ShowMenu(FParse::Param(FCommandLine::Get(),TEXT("MenuCheckpoints"))); }

void ADockHUD::ShowMenu(bool Checkpoints)
{
    auto* PC=GetOwningPlayerController();
    if(!PC || !GEngine || !GEngine->GameViewport) return;
    if(MenuWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
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
    };
    auto Start=[this](const TCHAR* Point)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
        MenuWidget.Reset();
        auto* Controller=GetOwningPlayerController();
        Controller->SetPause(false); Controller->bShowMouseCursor=false;
        Controller->SetInputMode(FInputModeGameOnly());
        UGameplayStatics::OpenLevel(this,TEXT("WaterdeepDocks"),true,FString(TEXT("ChuckStart="))+Point);
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
