#include "DockTutorial.h"
#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "DockSewer.h"
#include "SewerLife.h"
#include "SewerSlide.h"
#include "EnemyRat.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"

EDockTutorial FDockTutorial::Candidate(UWorld* World,const FVector& P,bool Night,uint32 Suppressed)
{
    const auto Ready=[&](EDockTutorial Id){return !(Suppressed&(1u<<uint8(Id)));};
    const auto Box=[&](float X0,float X1,float Y0,float Y1){return P.X>=X0 && P.X<=X1 && P.Y>=Y0 && P.Y<=Y1;};
    if(P.Z>-150 && P.Z<650 && !IsWithinDockSewer(P))
    {
        if(Ready(EDockTutorial::Run) && !Night && P.Z<120 && Box(-400,210,-260,320)) return EDockTutorial::Run;
        if(Ready(EDockTutorial::Jump) && P.Z<270 && Box(-600,-300,-610,-320)) return EDockTutorial::Jump;
        if(Ready(EDockTutorial::WallCargo) && P.Z<320 &&
            (Box(-625,-490,-940,-620) || Box(-25,65,-940,-600))) return EDockTutorial::WallCargo;
        if(Ready(EDockTutorial::WallLamps) && P.Y>=2450 && P.Y<=3160)
            for(float X : {-1790.f,-1110.f,-430.f}) if(FMath::Abs(P.X-X)<80) return EDockTutorial::WallLamps;
        if(Ready(EDockTutorial::Scratch) && !Night)
            for(TActorIterator<AEnemyRat> Rat(World);Rat;++Rat)
                if(!Rat->IsDead() && Rat->GetActorLocation().Z>-150 &&
                    FMath::Abs(P.Z-Rat->GetActorLocation().Z)<100 && FVector::Dist2D(P,Rat->GetActorLocation())<350)
                    return EDockTutorial::Scratch;
    }
    else if(P.Z<-650 && IsWithinDockSewer(P))
    {
        const int32 I=DockSewerNearestSample(P),First=GetSewerFirstRatsSample();
        if(Ready(EDockTutorial::SmallEnemy) && I>=First-6 && I<=First+10) return EDockTutorial::SmallEnemy;
        if(Ready(EDockTutorial::LargeEnemy) && I>=90 && I<=108) return EDockTutorial::LargeEnemy;
        if(Ready(EDockTutorial::Dodge) && DockSewerIsChamber(I)) return EDockTutorial::Dodge;
        if(Ready(EDockTutorial::WallRun) && I>=DockSewerCheckpointSample()-3 && I<DockSewerWallRiftEnd()) return EDockTutorial::WallRun;
        if(Ready(EDockTutorial::Leap) && I>=DockSewerLeapRiftStart()-7 && I<DockSewerLeapRiftEnd()) return EDockTutorial::Leap;
    }
    return EDockTutorial::None;
}
FString FDockTutorial::Text(EDockTutorial Id)
{
    switch(Id)
    {
    case EDockTutorial::Run:return TEXT("Tap Left Shift while moving to run.\nTap Left Shift again to walk.");
    case EDockTutorial::Jump:return TEXT("Press Space to jump.\nUse W, A, S and D to move onto the crates.");
    case EDockTutorial::WallCargo:
    case EDockTutorial::WallLamps:return TEXT("Move toward a wall and press Space to climb it.\nPress Space again to jump off; aim toward the opposite wall and repeat.");
    case EDockTutorial::Scratch:return TEXT("Left-click to scratch nearby enemies.");
    case EDockTutorial::SmallEnemy:return TEXT("Small enemies are easy to defeat");
    case EDockTutorial::LargeEnemy:return TEXT("Larger enemies are best to avoid");
    case EDockTutorial::Dodge:return TEXT("Dodge attacks: press C to roll.\nHold A or D and press C to side jump.");
    case EDockTutorial::WallRun:return TEXT("Tap Left Shift to run beside the wall, then press Space to wall run.\nKeep moving along the wall to cross the gap.");
    case EDockTutorial::Leap:return TEXT("While running, press Left Ctrl to sprint on all fours.\nSprint straight for at least a metre, then press Space near the edge to leap across.");
    default:return FString();
    }
}
FString FDockTutorial::Update(UWorld* World,const FVector& P,bool Night,float Time)
{
    const uint32 ActiveBit=Active==EDockTutorial::None?0:1u<<uint8(Active);
    const EDockTutorial Next=Candidate(World,P,Night,Seen&~ActiveBit);
    if(Next!=Active) {Active=EDockTutorial::None;Until=0;}
    if(Next!=EDockTutorial::None && !(Seen&(1u<<uint8(Next))))
    {Active=Next;Seen|=1u<<uint8(Next);Until=Time+10.f;}
    if(Time>=Until) Active=EDockTutorial::None;
    return Text(Active);
}
void ADockHUD::UpdateTutorial()
{
    auto* PC=GetOwningPlayerController();auto* Chuck=Cast<AChuckCharacter>(GetOwningPawn());
    if(!Chuck || !PC || PC->IsPaused() || MenuWidget.IsValid()) return;
    TutorialLine=Tutorial.Update(GetWorld(),Chuck->GetActorLocation(),HasExitedDockSewer(),GetWorld()->GetTimeSeconds());
}
void ADockHUD::DrawTutorial()
{
    if(TutorialLine.IsEmpty() || MenuWidget.IsValid()) return;
    const float UiScale=FMath::Clamp(Canvas->SizeY/1080.f,.5f,2.f);
    if(!SubtitleFont) {SubtitleFont=NewObject<UFont>(this);SubtitleFont->FontCacheType=EFontCacheType::Runtime;}
    const FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(30.f*UiScale));
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const float MaxWidth=Canvas->SizeX*.8f,RowH=Measure->GetMaxCharacterHeight(Font)*1.12f;
    TArray<FString> Rows,Paragraphs;TutorialLine.ParseIntoArray(Paragraphs,TEXT("\n"));
    for(const FString& Paragraph : Paragraphs)
    {
        FString Row;TArray<FString> Words;Paragraph.ParseIntoArrayWS(Words);
        for(const FString& Word : Words)
        {
            const FString Next=Row.IsEmpty()?Word:Row+TEXT(" ")+Word;
            if(!Row.IsEmpty() && Measure->Measure(Next,Font).X>MaxWidth) {Rows.Add(Row);Row=Word;} else Row=Next;
        }
        if(!Row.IsEmpty()) Rows.Add(Row);
    }
    const float Top=Canvas->SizeY*.105f;
    bool Fits=Top+Rows.Num()*RowH<Canvas->SizeY*.4f;
    for(int32 I=0;I<Rows.Num();++I)
    {
        const float W=Measure->Measure(Rows[I],Font).X;Fits&=W<=MaxWidth+.5f;
        FCanvasTextItem Item(FVector2D((Canvas->SizeX-W)*.5f,Top+I*RowH),FText::FromString(Rows[I]),Font,FLinearColor(.97f,.95f,.90f));
        Item.Font=SubtitleFont;Item.bOutlined=true;Item.OutlineColor=FLinearColor(0,0,0,.8f);
        Item.EnableShadow(FLinearColor(0,0,0,.65f),FVector2D(1,2)*UiScale);Canvas->DrawItem(Item);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckTutorialReview")) && TutorialReviewLine!=TutorialLine)
    {TutorialReviewLine=TutorialLine;UE_LOG(LogTemp,Display,TEXT("CHUCK_TUTORIAL_DRAW id=%d rows=%d fits=%d text=%s"),uint8(Tutorial.Active),Rows.Num(),Fits,*TutorialLine);}
}
void StartDockTutorialReview(UWorld* World)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckTutorialReview"))) return;
    auto Failures=MakeShared<int32>(0);
    // Ten actual HUD triggers; isolate overlapping lessons only in this review.
    const FVector Points[]={FVector(-240,-180,36),FVector(-440,-420,36),FVector(20,-720,36),
        FVector(-1110,2500,36),FVector(500,-500,36),DockSewerPoint(GetSewerFirstRatsSample())+FVector(0,0,35),
        DockSewerPoint(94)+FVector(0,0,35),DockSewerPoint(DockSewerSamples()/2-15)+FVector(0,0,35),
        DockSewerCheckpointLocation(),DockSewerPoint(DockSewerLeapRiftStart()-5)+FVector(0,0,35)};
    for(int32 I=0;I<10;++I)
    {
        FTimerHandle Move,Shot;
        World->GetTimerManager().SetTimer(Move,[World,Failures,I,P=Points[I]](){
            auto* PC=World->GetFirstPlayerController();auto* Chuck=Cast<AChuckCharacter>(PC->GetPawn());
            Chuck->DisableInput(PC);Chuck->ResetAtLocation(P);
            auto* HUD=Cast<ADockHUD>(PC->GetHUD());HUD->ReviewTutorial(uint32(1023)&~(1u<<I));
            if(FDockTutorial::Candidate(World,P,false,1023u&~(1u<<I))!=EDockTutorial(I)) ++*Failures;
        },3.f+I*2.f,false);
        World->GetTimerManager().SetTimer(Shot,[I](){
            const FString Dir=FPaths::ScreenShotDir()/TEXT("Tutorials");IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("Prompt%d.png"),I),false,false);
        },4.f+I*2.f,false);
    }
    FTimerHandle Finish;
    World->GetTimerManager().SetTimer(Finish,[World,Failures,P=Points[0],RatP=Points[4]](){
        FDockTutorial State;
        if(State.Update(World,P,false,0).IsEmpty()) ++*Failures;
        State.Update(World,FVector(0,400,36),false,1);
        if(!State.Update(World,P,false,2).IsEmpty()) ++*Failures;
        if(FDockTutorial::Candidate(World,RatP,true,1023u&~(1u<<4))!=EDockTutorial::None) ++*Failures;
        for(int32 Sample : {120,262,330})
            if(FDockTutorial::Candidate(World,DockSewerPoint(Sample)+FVector(0,0,35),false)!=EDockTutorial::None) ++*Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_TUTORIAL_TEST_COMPLETE failures=%d prompts=10 run_once=1 later_sewer_rats=0 night_rats=0"),*Failures);
        World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
    },25.f,false);
}
