#include "SewerLeapRift.h"
#include "DockSewer.h"
#include "ChuckCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void StartSewerLeapRiftReview(UWorld* World)
{
    const bool Test=FParse::Param(FCommandLine::Get(),TEXT("ChuckLeapRiftTest"));
    const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("ChuckLeapRiftCapture"));
    if(!Test && !Capture) return;
    const int32 First=DockSewerLeapRiftStart(),Last=DockSewerLeapRiftEnd();
    const FVector Near=DockSewerPoint(First),Far=DockSewerPoint(Last);
    const FVector Along=(Far-Near).GetSafeNormal2D();
    if(Capture)
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(80);
        const FVector Positions[]={Near-Along*210+FVector(0,0,85),Near-Along*90+FVector(0,0,160),Far+Along*180+FVector(0,0,85)};
        const FVector Targets[]={(Near+Far)*.5f+FVector(0,0,-15),(Near+Far)*.5f+FVector(0,0,20),Near+FVector(0,0,20)};
        for(int32 I=0;I<3;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Positions[I],T=Targets[I],Near,Along](){
                if(auto* PC=World->GetFirstPlayerController())
                {
                    if(auto* Chuck=Cast<AChuckCharacter>(PC->GetPawn())) Chuck->ResetAtLocation(Near-Along*300+FVector(0,0,35));
                    Camera->SetActorLocationAndRotation(P,(T-P).Rotation());PC->SetViewTarget(Camera);
                }
            },4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){
                const FString Folder=FPaths::ScreenShotDir()/TEXT("LeapRift");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle Follow,FollowShot;
        World->GetTimerManager().SetTimer(Follow,[World,Near,Along](){
            auto* PC=World->GetFirstPlayerController();auto* Chuck=Cast<AChuckCharacter>(PC->GetPawn());
            const FVector Across(-Along.Y,Along.X,0);
            Chuck->ResetAtLocation(Near-Along*60+FVector(0,0,35));
            Chuck->SetActorRotation((-Across).Rotation());Chuck->Recenter();PC->SetViewTarget(Chuck);
        },16.f,false);
        World->GetTimerManager().SetTimer(FollowShot,[](){
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("LeapRift/CameraWall.png"),false,false);
        },18.f,false);
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},20.f,false);
        return;
    }
    struct FRun
    {
        int32 Trial=0,Failures=0,Deaths=0,Leaps=0,Walls=0;
        float Time=0,StartTime=0,TickTime=-1,JumpAt=-1,NextShot=.1f,Peak=0,Runup=0;
        bool Started=false,Sprinting=false,SeenLeap=false,SeenRunJump=false,SeenWall=false,Crossed=false;
        FTimerHandle Timer;
    };
    auto Run=MakeShared<FRun>();FTimerHandle Start;
    World->GetTimerManager().SetTimer(Start,[World,Run,Near,Far,Along,First](){
        auto* PC=World->GetFirstPlayerController();auto* Chuck=Cast<AChuckCharacter>(PC->GetPawn());
        Chuck->DisableInput(PC);PC->SetViewTarget(Chuck);
        World->GetTimerManager().SetTimer(Run->Timer,[World,Run,Chuck,Near,Far,Along,First](){
            if(Run->Trial>=6) return;
            const float Now=World->GetTimeSeconds();if(Now==Run->TickTime) return;Run->TickTime=Now;
            if(!Run->Started)
            {
                Chuck->ResetToDock();Chuck->SetRunHeld(true);
                FVector P=Near-Along*280;
                if(Run->Trial>=4) P=DockSewerPoint(First-9);
                const float Side=Run->Trial==4?-1.f:Run->Trial==5?1.f:0.f;
                if(Side!=0)
                {
                    FHitResult Wall;FCollisionQueryParams Query;Query.AddIgnoredActor(Chuck);
                    const int32 Index=Run->Trial>=4?First-9:First-1;
                    const FVector Across=DockSewerSide(Index)*Side;
                    const FVector Probe=DockSewerPoint(Index)+FVector(0,0,50);
                    if(World->LineTraceSingleByChannel(Wall,Probe,Probe+Across*400,ECC_Visibility,Query))
                        P+=Across*(FVector::Dist2D(Probe,Wall.ImpactPoint)-23);
                    else P+=Across*(DockSewerHalfWidth(Index)-23);
                }
                Chuck->ResetAtLocation(P+FVector(0,0,34.65f));Chuck->SetRunHeld(true);
                Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Chuck->GetCharacterMovement()->StopMovementImmediately();
                Chuck->SetActorRotation(Along.Rotation());Chuck->Recenter();
                Run->Deaths=Chuck->GetFallDeaths();Run->Leaps=Chuck->GetSprintLeaps();Run->Walls=Chuck->GetWallSideRuns();
                Run->Time=0;Run->StartTime=Now;Run->JumpAt=-1;Run->NextShot=.1f;Run->Peak=0;Run->Runup=0;
                Run->Sprinting=Run->SeenLeap=Run->SeenRunJump=Run->SeenWall=Run->Crossed=false;Run->Started=true;
            }
            Run->Time=Now-Run->StartTime;
            const FVector P=Chuck->GetActorLocation();
            const float ToEdge=FVector::DotProduct(Near-P,Along);
            if(Run->Time>.3f && !Run->Crossed)
            {
                const int32 Index=DockSewerNearestSample(P);
                const FVector Heading=Run->Trial>=4 && ToEdge>325 && Index>=0 ? (DockSewerPoint(Index+2)-DockSewerPoint(Index)).GetSafeNormal2D() : Along;
                Chuck->SetTestStickWorld(Heading);
                if(!Chuck->IsWallSideRunning()) Chuck->AddMovementInput(Heading,1);
                if(Run->Trial==0 && !Run->Sprinting && Run->Time>.65f)
                {Chuck->TrySprint();Run->Sprinting=true;}
                const bool Launch=Run->Trial>=4 ? Run->Time>.7f && Chuck->GetVelocity().Size2D()>200 : ToEdge<=(Run->Trial==2?35.f:Run->Trial==3?55.f:12.f);
                if(Run->JumpAt<0 && Launch && Chuck->GetCharacterMovement()->IsMovingOnGround())
                {
                    Run->Runup=Chuck->GetSprintRunup();Chuck->JumpPressed();Run->JumpAt=Run->Time;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_LEAPRIFT_LAUNCH trial=%d to_edge_cm=%.1f runup_cm=%.1f gait=%s"),Run->Trial,ToEdge,Run->Runup,Chuck->GetGaitName());
                }
            }
            Run->SeenRunJump|=Chuck->IsRunJumping();Run->SeenLeap|=Chuck->IsSprintLeaping();Run->SeenWall|=Chuck->IsWallSideRunning();
            Run->Peak=FMath::Max(Run->Peak,static_cast<float>(P.Z-Near.Z-34.65f));
            const bool Beyond=FVector::DotProduct(P-Far,Along)>15 && P.Z>Near.Z+20 && P.Z<Near.Z+90
                && Chuck->GetCharacterMovement()->IsMovingOnGround();
            Run->Crossed|=Beyond;
            if(Run->Crossed) {Chuck->SetTestStick(FVector2D::ZeroVector);Chuck->GetCharacterMovement()->StopMovementImmediately();}
            if(Run->JumpAt>=0 && Run->Time-Run->JumpAt>=Run->NextShot && Run->NextShot<1.4f
                && FParse::Param(FCommandLine::Get(),TEXT("ChuckLeapRiftMotionCapture")))
            {
                const FString Folder=FPaths::ScreenShotDir()/TEXT("LeapRiftMotion");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("Trial%d_%03d.png"),Run->Trial,FMath::RoundToInt(Run->NextShot*100)),false,false);
                Run->NextShot+=.2f;
            }
            const bool Fell=Chuck->GetFallDeaths()>Run->Deaths || Chuck->IsAstral();
            if(Run->Crossed || Fell || Run->Time>6.f || (Run->JumpAt>=0 && Run->Time>Run->JumpAt+(Run->Trial>=4?5.f:3.f)))
            {
                const bool Pass=Run->Trial==0 ? Run->Crossed && Run->SeenLeap && !Fell && Run->Runup>=AChuckCharacter::SprintLeapRunup
                    : (Run->Crossed || Fell) && Run->JumpAt>=0 && Chuck->GetSprintLeaps()==Run->Leaps && (Run->Trial<4 ? Run->SeenRunJump : Run->SeenWall);
                Run->Failures+=!Pass;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_LEAPRIFT_TRIAL trial=%d pass=%d crossed=%d fell=%d leap=%d wall=%d run_jump=%d peak_cm=%.1f p=%s"),Run->Trial,Pass,Run->Crossed,Fell,Run->SeenLeap,Run->SeenWall,Run->SeenRunJump,Run->Peak,*P.ToString());
                ++Run->Trial;Run->Started=false;
                if(Run->Trial==6)
                {
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_LEAPRIFT_TEST_COMPLETE failures=%d trials=6 sprint=1 ordinary=3 wall_assisted=2 bypass_allowed=1"),Run->Failures);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            }
        },.001f,true);
    },3.f,false);
}
