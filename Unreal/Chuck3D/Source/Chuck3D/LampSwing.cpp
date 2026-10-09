#include "LampSwing.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
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

namespace
{
    // The three lamp alleys between the tall rear-row houses (DockSetting.cpp):
    // walls at Left+280 (west) and Left+400 (east), eaves of the two houses.
    float LaneLeft(int32 Lane) { return -1450.f + Lane * 680.f; }
    const float LaneEaves[3] = {790.f, 660.f, 660.f};   // the lower house of each lane: the roof a chimney climb reaches first
    struct FTrial { int32 Lane; float Y; const TCHAR* Name; };
    // 0-2: chimney climbs between the lanterns' reach, one per alley, onto the roof.
    // 3: a chimney climb under the first lantern catches its ring. 4: brachiation
    // down the alley, ring to ring, the stick let go. 5: back, stick held back.
    // 6: kicked off the ring toward the far wall, wall jumps on up onto the roof.
    const FTrial Trials[] = {{0, 2637.f, TEXT("chimney_lane1")}, {2, 3022.f, TEXT("chimney_lane3")}, {1, 2767.f, TEXT("chimney_lane2")},
        {0, 2570.f, TEXT("wall_jump_catch")}, {0, 0.f, TEXT("swing_forward")}, {0, 0.f, TEXT("swing_back")}, {0, 0.f, TEXT("kick_to_roof")}};
    constexpr int32 TrialCount = UE_ARRAY_COUNT(Trials);
}

void StartLampSwingReview(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ChuckLampSwingTest"))) return;
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckLampSwingCapture"));
    struct FRun
    {
        int32 Trial = 0, Failures = 0, Deaths = 0, PullUps = 0, Catches = 0, Leaps = 0, Kicks = 0, Eaves = 0, Jumps = 0;
        float Time = 0, StartTime = 0, TickTime = -1, NextShot = 0, LastJump = -1, MaxZ = 0;
        bool Started = false, WallSeen = false, OnRoof = false;
        FString Detail;
        FTimerHandle Timer;
        ACameraActor* Camera = nullptr;
    };
    auto Run = MakeShared<FRun>();
    FTimerHandle Start;
    World->GetTimerManager().SetTimer(Start, [World, Run, bCapture]() {
        auto* PC = World->GetFirstPlayerController(); auto* Chuck = Cast<AChuckCharacter>(PC->GetPawn());
        Chuck->DisableInput(PC); PC->SetViewTarget(Chuck);
        if (bCapture) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(70); }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_LAMPSWING_GRIPS grips=%d"), GetChuckSwingGrips().Num());
        World->GetTimerManager().SetTimer(Run->Timer, [World, Run, Chuck, PC, bCapture]() {
            if (Run->Trial >= TrialCount) return;
            const float Now = World->GetTimeSeconds(); if (Now == Run->TickTime) return; Run->TickTime = Now;
            const FTrial& T = Trials[Run->Trial];
            const float Left = LaneLeft(T.Lane), Mid = Left + 340.f;
            const int32 First = T.Lane * 5;   // the lane's lanterns, registered five per lane
            auto* Movement = Chuck->GetCharacterMovement();
            if (!Run->Started)
            {
                // 4-6 carry on from where the last trial left him (hanging from a ring).
                if (T.Y > 0.f)
                {
                    Chuck->ResetToDock();
                    Chuck->ResetAtLocation(FVector(Left + 306.f, T.Y, 34.65f));   // beside the west wall, facing it
                    Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
                    Chuck->SetActorRotation(FRotator(0, 180.f, 0)); Chuck->Recenter();
                }
                Run->Deaths = Chuck->GetFallDeaths(); Run->PullUps = Chuck->GetPullUps(); Run->Catches = Chuck->GetSwingCatches();
                Run->Leaps = Chuck->GetSwingLeaps(); Run->Kicks = Chuck->GetSwingDismounts(); Run->Eaves = Chuck->GetEaveGrabs();
                Run->Time = 0; Run->StartTime = Now; Run->NextShot = 0; Run->LastJump = -1; Run->Jumps = 0; Run->MaxZ = 0;
                Run->WallSeen = Run->OnRoof = false; Run->Started = true;
            }
            Run->Time = Now - Run->StartTime;
            const FVector P = Chuck->GetActorLocation();
            Run->MaxZ = FMath::Max(Run->MaxZ, static_cast<float>(P.Z));
            const FString Gait = Chuck->GetGaitName();
            const bool bGround = Movement->IsMovingOnGround() && Gait != TEXT("Climb");
            const bool bRoof = bGround && P.Z > LaneEaves[T.Lane] + 20.f;
            auto Press = [&]() { Chuck->JumpPressed(); Run->LastJump = Run->Time; ++Run->Jumps; };
            bool bDone = false, bPass = false;
            if (Run->Trial <= 2 || Run->Trial == 3 || Run->Trial == 6)
            {
                // Chimney: the stick toward the west wall for the first jump, then let go; a
                // jump on each wall a moment after reaching it, and on the edge once caught.
                if (Run->Trial == 6 && !Run->WallSeen && Run->Time < .05f)
                {
                    Chuck->SetTestStickWorld(FVector(1, 0, 0));   // toward the far (east) wall: no ring that way
                    Press();
                }
                else if (Run->Trial != 6 && !Run->WallSeen)
                {
                    Chuck->SetTestStickWorld(FVector(-1, 0, 0));
                    if (Run->Time > .3f && Run->Jumps == 0 && bGround) Press();
                }
                if (Chuck->IsWallRunning()) { if (!Run->WallSeen) Chuck->SetTestStick(FVector2D::ZeroVector); Run->WallSeen = true; }
                if (Run->WallSeen && Run->Time - Run->LastJump > .12f && ((Chuck->IsWallRunning() && Run->Time - Run->LastJump > .2f) || Chuck->IsHanging())) Press();
                if (Run->Trial == 3)
                {
                    bPass = Chuck->GetSwingGrip() == First && Chuck->GetSwingCatches() > Run->Catches;
                    bDone = bPass && Run->Time - Run->LastJump > .6f;
                }
                else
                {
                    bPass = bRoof && Chuck->GetPullUps() > Run->PullUps && (Run->Trial != 6 || Chuck->GetSwingDismounts() > Run->Kicks)
                        && (Run->Trial == 6 || Chuck->GetSwingCatches() == Run->Catches);
                    bDone = bPass && Run->Time - Run->LastJump > .6f;
                    if (bPass && !Run->OnRoof) { Run->OnRoof = true; Chuck->SetTestStick(FVector2D::ZeroVector); }
                }
                if (Run->WallSeen && bGround && P.Z < 60.f && Run->Time > 1.f) bDone = true;   // fell back to the paving: a failure
            }
            else
            {
                // Brachiation: forward with the stick let go, back with it held back (south).
                const int32 Goal = Run->Trial == 4 ? First + 4 : First;
                if (Run->Trial == 5) Chuck->SetTestStickWorld(FVector(0, -1, 0)); else Chuck->SetTestStick(FVector2D::ZeroVector);
                if (Chuck->IsSwinging() && Chuck->GetSwingGrip() != Goal && Run->Time - Run->LastJump > .45f) Press();
                bPass = Chuck->GetSwingGrip() == Goal && Chuck->GetSwingLeaps() - Run->Leaps == 4;
                bDone = (bPass && Run->Time - Run->LastJump > .8f) || (!Chuck->IsSwinging() && !Chuck->IsSwingLeaping());
            }
            if (bCapture && Run->Time >= Run->NextShot && Run->NextShot < 12.f)
            {
                // Brachiation from the alley's end (rings receding along it); the climbs from his own camera.
                if (Run->Trial >= 3 && Run->Trial <= 5)
                {
                    const FVector Eye(Mid, Run->Trial == 5 ? 3200.f : 2460.f, 560.f), Look(Mid, 2830.f, 520.f);
                    Run->Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation()); PC->SetViewTarget(Run->Camera);
                }
                else PC->SetViewTarget(Chuck);
                const FString Folder = FPaths::ScreenShotDir() / TEXT("LampSwing"); IFileManager::Get().MakeDirectory(*Folder, true);
                FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("Trial%d_%04d.png"), Run->Trial, FMath::RoundToInt(Run->NextShot * 100)), false, false);
                Run->NextShot += .15f;
            }
            const bool bFell = Chuck->GetFallDeaths() > Run->Deaths || Chuck->IsAstral();
            if (bDone || bFell || Run->Time > 20.f)
            {
                bPass = bPass && !bFell;
                Run->Failures += !bPass;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_LAMPSWING_TRIAL trial=%d name=%s pass=%d time=%.2f jumps=%d gait=%s grip=%d catches=%d leaps=%d kicks=%d hangs_eave=%d pullups=%d max_z=%.0f p=%s"),
                    Run->Trial, T.Name, bPass ? 1 : 0, Run->Time, Run->Jumps, *Gait, Chuck->GetSwingGrip(), Chuck->GetSwingCatches() - Run->Catches, Chuck->GetSwingLeaps() - Run->Leaps,
                    Chuck->GetSwingDismounts() - Run->Kicks, Chuck->GetEaveGrabs() - Run->Eaves, Chuck->GetPullUps() - Run->PullUps, Run->MaxZ, *P.ToString());
                // A failed swing leaves nothing to carry on from: the later swing trials start from a fresh catch.
                if (!bPass && Run->Trial >= 3 && Run->Trial <= 5) { Run->Failures += TrialCount - 1 - Run->Trial; Run->Trial = TrialCount; }
                else ++Run->Trial;
                Run->Started = false;
                if (Run->Trial >= TrialCount)
                {
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_LAMPSWING_TEST_COMPLETE failures=%d trials=%d chimneys=3 catch=1 swings=2 kick=1"), Run->Failures, TrialCount);
                    PC->ConsoleCommand(TEXT("quit"));
                }
            }
        }, .001f, true);
    }, 3.f, false);
}
