// -ChuckTailCapture (Claude, 2026-11-12; user: "Chuck routinely turns squiggly ... much worse now"): in Waterdeep he
// stands, walks, stops, turns, runs and stops while a camera beside his hips saves a frame every 0.15 s
// (Saved/Screenshots/.../TailCapture), and the tail's bones are logged with each (CHUCK_TAIL_FRAME); then quits.
#include "ChuckCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void StartChuckTailCapture(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    auto* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!Chuck || !PC) { FPlatformMisc::RequestExit(false); return; }
    auto* Cam = World->SpawnActor<ACameraActor>();
    Cam->GetCameraComponent()->SetFieldOfView(40.f);
    PC->SetViewTarget(Cam);
    struct FRun { float Start = -1; int32 Shot = 0; float NextShot = 0; FTimerHandle Timer; };
    auto Run = MakeShared<FRun>();
    World->GetTimerManager().SetTimer(Run->Timer, [World, Chuck, Cam, Run]()
    {
        const float Now = World->GetTimeSeconds();
        if (Run->Start < 0) { Run->Start = Now; Run->NextShot = Now + .5f; }
        const float T = Now - Run->Start;
        // The script: stand, walk, stop, turn left, walk, run, stop, turn right, stand.
        FVector2D Stick(0, 0);
        static const bool bActions = FParse::Param(FCommandLine::Get(), TEXT("ChuckTailActions"));
        if (bActions)
        {
            // The rest of his moves: a turn in place, a jump and landing, a roll, a slash, a side jump, a turn back.
            static int32 Did = 0;
            auto Once = [&](int32 Bit, float At) { if (T >= At && !(Did & (1 << Bit))) { Did |= 1 << Bit; return true; } return false; };
            if (T > .8f && T < 1.3f) Stick = FVector2D(1, -.2f);
            if (Once(0, 2.f)) Chuck->JumpPressed();
            if (Once(1, 3.6f)) Chuck->DodgeToward(FVector2D(0, 1));
            if (Once(2, 5.f)) Chuck->Slash();
            if (Once(3, 6.2f)) { Chuck->SetStrafeHeld(true); Chuck->DodgeToward(FVector2D(1, 0)); }
            if (Once(4, 7.4f)) Chuck->SetStrafeHeld(false);
            if (T > 8.f && T < 8.5f) Stick = FVector2D(-1, -.3f);
            if (Once(5, 9.2f)) Chuck->JumpPressed();
        }
        else if (T > 1.f && T < 2.6f) Stick = FVector2D(0, 1);
        else if (T > 2.6f && T < 3.4f) Stick = FVector2D(0, 0);
        else if (T > 3.4f && T < 4.f) Stick = FVector2D(-1, 0);
        else if (T > 4.f && T < 5.f) Stick = FVector2D(0, 1);
        else if (T > 5.f && T < 6.2f) { Stick = FVector2D(0, 1); Chuck->SetRunHeld(true); }
        else if (T > 7.f && T < 7.6f) { Stick = FVector2D(1, 0); Chuck->SetRunHeld(false); }
        // (as the other tests drive him: the stick in the world, from the way he faced at the start)
        static FVector Fwd0 = FVector::ZeroVector;
        if (Fwd0.IsNearlyZero()) Fwd0 = Chuck->GetActorForwardVector().GetSafeNormal2D();
        const FVector Right0 = FVector::CrossProduct(FVector::UpVector, Fwd0);
        const FVector Dir = Fwd0 * Stick.Y + Right0 * Stick.X;
        if (Dir.IsNearlyZero()) Chuck->SetTestStick(FVector2D::ZeroVector);
        else { Chuck->SetTestStickWorld(Dir); Chuck->AddMovementInput(Dir, 1.f, true); }
        // Beside his hips and a little behind, at tail height, looking at the tail's root.
        const FVector Fwd = Chuck->GetActorForwardVector().GetSafeNormal2D(), Right = FVector::CrossProduct(FVector::UpVector, Fwd);
        const FVector Root = Chuck->GetMesh()->GetBoneLocation(TEXT("tail_2"));
        const FVector Eye = Root + Right * 85.f - Fwd * 35.f + FVector(0, 0, 18.f);
        Cam->SetActorLocationAndRotation(Eye, (Root - Eye).Rotation());
        if (Now >= Run->NextShot && Run->Shot < 110)
        {
            Run->NextShot = Now + (bActions ? .1f : .15f);
            const FString Name = FString::Printf(TEXT("%s/T%02d_%.2f_%s.png"), FParse::Param(FCommandLine::Get(), TEXT("ChuckTailActions")) ? TEXT("TailActions") : FParse::Param(FCommandLine::Get(), TEXT("ChuckTailTwistFix")) ? TEXT("TailCaptureFix") : TEXT("TailCapture"), Run->Shot, T, Chuck->GetGaitName());
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / Name, false, false);
            FString Bones;
            for (int32 I = 0; I < 6; ++I)
            {
                const FTransform B = Chuck->GetMesh()->GetBoneTransform(Chuck->GetMesh()->GetBoneIndex(*FString::Printf(TEXT("tail_%d"), I)));
                Bones += FString::Printf(TEXT(" t%d=(%s)"), I, *B.GetRotation().Rotator().ToCompactString());
            }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TAIL_FRAME shot=%d t=%.2f gait=%s%s"), Run->Shot, T, Chuck->GetGaitName(), *Bones);
            ++Run->Shot;
        }
        if (T > 11.f) FPlatformMisc::RequestExit(false);
    }, 1.f / 60.f, true);
}
