// -ChuckChultTest -ChuckTempleRunTest (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md): stills of the
// temple trap run, then the real character through each section, with the moves a player uses:
// mantles up the rubble, the chimney's wall jumps, side and chained wall runs over the pits, rolls
// under the saws, and the timing of the axes and the fire read the way a player reads them (wait,
// then run straight through). Negative trials show the pits ask for the wall runs (a sprint leap or
// one side run falls short) and that standing in a trap hurts.
#include "ChultTempleRun.h"
#include "ChultTempleInterior.h"
#include "ChultTempleTraps.h"
#include "ChuckCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

namespace
{
    using namespace TempleRun;
    FVector W(float S, float T, float Z) { return ChultTempleInteriorWorldOf(FVector(S, T, Z)); }
    FVector Dir(float DS, float DT) { return (W(DS, DT, 0) - W(0, 0, 0)).GetSafeNormal(); }
    constexpr float Stand = 34.65f;   // capsule centre over the floor at a spawn
    constexpr float Pace = 225.f;
    const float BM = (BS0 + BS1) * .5f, CM = (CausewayT0 + CausewayT1) * .5f, DM = (DS0 + DS1) * .5f, EM = (ET0 + ET1) * .5f, FM = (FS0 + FS1) * .5f;
}

void ReviewChultTempleRun(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    struct FView { const TCHAR* Name; FVector Eye, Aim; };
    const TArray<FView> Views = {
        {TEXT("RunCollapse"), FVector(ArchS - 300, ExitT - 90, Floor + 140), FVector(68250, ExitT, Floor + 170)},
        {TEXT("RunRubble"), FVector(ArchS + 40, ExitT + 80, Floor + 90), FVector(CollapseS1, ExitT - 20, RubbleTop + 40)},
        {TEXT("RunChimney"), FVector(BS0 + 25, (ShaftT0 + ShaftT1) * .5f, RubbleTop + 35), FVector(BS1 - 30, (ShaftT0 + ShaftT1) * .5f, UZ + 60)},
        {TEXT("RunPits"), FVector(BM, -7880, UZ + 120), FVector(BM, -6000, UZ - 20)},
        {TEXT("RunPitWall"), FVector(BS0 + 25, -7180, UZ + 150), FVector(BS1 - 10, -6350, UZ - 30)},
        {TEXT("RunPitTwist"), FVector(BM, -5820, UZ + 120), FVector(BS0 - 30, -5350, UZ - 40)},
        {TEXT("RunAxes"), FVector(68820, CM, UZ + 170), FVector(70400, CM, UZ + 70)},
        {TEXT("RunAxeClose"), FVector(69180, CausewayT1 + 30, UZ + 90), FVector(69390, CM, UZ + 60)},
        {TEXT("RunFire"), FVector(DM, -5120, UZ + 140), FVector(DM, -6700, UZ + 30)},
        {TEXT("RunFireClose"), FVector(DS1 - 40, -5600, UZ + 80), FVector(DS0 + 60, -5950, UZ + 40)},
        {TEXT("RunSaws"), FVector(71320, EM, UZ + 120), FVector(73300, EM, UZ + 40)},
        {TEXT("RunSawClose"), FVector(71610, ET1 - 30, UZ + 60), FVector(71780, EM, UZ + 55)},
        {TEXT("RunGauntlet"), FVector(FM, -7660, UZ + 150), FVector(FM, -6400, UZ + 50)},
        {TEXT("RunGauntletAxes"), FVector(FS0 + 20, -6900, UZ + 170), FVector(FM, -6250, UZ + 60)},
        {TEXT("RunGauntletEnd"), FVector(FM, -5250, UZ + 120), FVector(FM, -4000, UZ + 40)},
        {TEXT("RunAnte"), FVector(FM, AnteT0 + 30, UZ + 150), FVector(FM, AnteT1, UZ + 120)}};
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(74);
    Chuck->SetActorHiddenInGame(true);
    // The bot's stick is the test stick: live input would zero it every frame.
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->SetViewTarget(Camera); PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }
    Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Axes));

    enum class ET : uint8 { Climb, PitSingle, PitChain, PitTwist, PitFall, LeapShort, SideShort, Axes, AxeHit, Fire, FireStrip, FireHit, Saws, SawHit, Gauntlet, Flow };
    static const ET Trials[] = {ET::Climb, ET::PitSingle, ET::PitChain, ET::PitTwist, ET::PitFall, ET::LeapShort, ET::SideShort, ET::Axes, ET::AxeHit, ET::Fire, ET::FireStrip, ET::FireHit, ET::Saws, ET::SawHit, ET::Gauntlet, ET::Flow};
    static const TCHAR* Names[] = {TEXT("climb"), TEXT("pit_single"), TEXT("pit_chain"), TEXT("pit_twist"), TEXT("pit_fall"), TEXT("leap_short"), TEXT("side_short"), TEXT("axes"), TEXT("axe_hit"), TEXT("fire"), TEXT("fire_strip"), TEXT("fire_hit"), TEXT("saws"), TEXT("saw_hit"), TEXT("gauntlet"), TEXT("gauntlet_flow")};
    struct FRun
    {
        int32 Phase = 0, Trial = -1, Sub = 0, Leg = 0, Failures = 0, Deaths = 0, Mark = 0, Shots = 0;
        int32 SideRuns = 0, Chains = 0, AxeHits = 0, JetHits = 0, SawHits = 0, Rolls = 0, WallJumps = 0, Mantles = 0, Hangs = 0;
        double Next = 0, Start = 0, At = -1, WaitFrom = -1, ViewClock = 0, Lip = -1; float Waited = 0, Furthest = -1e9f, Travel = 0, LastY = 0;
        bool bStarted = false, bPass = false, bDone = false, bFlag = false;
        TArray<int32> Results; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Camera = Camera;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run, Views]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        const double Now = World->GetTimeSeconds();
        AChultTempleTraps* Traps = AChultTempleTraps::Find(World);
        // ---- The stills, each waiting (up to 4 s) for its moment.
        const int32 I = FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleRunNoViews")) ? Views.Num() : Run->Phase / 2;
        if (I < Views.Num())
        {
            if (Now < Run->Next) return;
            Run->Next = Now + 1.4;
            const FString Name = Views[I].Name;
            if (Run->Phase % 2 == 0)
            {
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(ChultTempleInteriorWorldOf(Views[I].Eye), (ChultTempleInteriorWorldOf(Views[I].Aim) - ChultTempleInteriorWorldOf(Views[I].Eye)).Rotation());
                Run->ViewClock = Now; ++Run->Phase; return;
            }
            bool Ready = true;
            if (Traps && (Name == TEXT("RunFire") || Name == TEXT("RunFireClose"))) Ready = Traps->JetStage(1, Now) == 2 && (Name != TEXT("RunFire") || Traps->JetStage(3, Now) == 2);
            if (Traps && Name == TEXT("RunAxeClose")) Ready = FMath::Abs(Traps->AxeTheta(0, Now)) < 6.f;
            if (Traps && Name == TEXT("RunSawClose")) Ready = true;
            if (!Ready && Now - Run->ViewClock < 5) { Run->Next = Now; return; }
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / (Name + TEXT(".png")), true, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_VIEW name=%s ready=%d"), *Name, Ready);
            ++Run->Phase; return;
        }
        // -ChuckTempleFireFrames: after the stills, a sequence through one fire cycle from the close
        // view (the pilot flame in the tell, the stream reaching out, the fuel cut, the smoke), then exit.
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleFireFrames")) && Traps)
        {
            static const float Marks[] = {.2f, .5f, .74f, .8f, .9f, 1.1f, 1.6f, 1.95f, 2.15f, 2.6f};
            const int32 K = Run->Phase - 2 * Views.Num();
            if (K == 0)
            {
                // Wait for jet 1's tell to start, from the close view.
                const FView& V = Views[9];
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(ChultTempleInteriorWorldOf(V.Eye), (ChultTempleInteriorWorldOf(V.Aim) - ChultTempleInteriorWorldOf(V.Eye)).Rotation());
                if (Traps->JetStage(1, Now) == 1 && Traps->JetStage(1, Now - .05) == 0) { Run->At = Now; ++Run->Phase; }
                return;
            }
            if (K - 1 < UE_ARRAY_COUNT(Marks))
            {
                if (Now - Run->At < Marks[K - 1]) return;
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("RunFireFrame_%02d.png"), K - 1), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_FIRE_FRAME k=%d t=%.2f stage=%d puffs=%d"), K - 1, Now - Run->At, Traps->JetStage(1, Now), Traps->JetPuffsLive());
                ++Run->Phase; return;
            }
            Run->bDone = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_TEST fire_frames=1")); FPlatformMisc::RequestExitWithStatus(false, 0); return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleRunViewsOnly"))) { Run->bDone = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_TEST views_only=1")); FPlatformMisc::RequestExitWithStatus(false, 0); return; }
        if (!Traps) { Run->bDone = true; UE_LOG(LogTemp, Error, TEXT("CHUCK_TEMPLE_RUN_TEST failures=1 traps_missing=1")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        auto* M = Chuck->GetCharacterMovement();
        const FVector P = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Radius = Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const bool Ground = M->IsMovingOnGround();
        auto Push = [&](const FVector& D) { Chuck->SetTestStickWorld(D); if (!Chuck->IsWallSideRunning()) Chuck->AddMovementInput(D, 1, true); };
        auto Steer = [&](float S, float T, float Within = 30.f) -> bool
        {
            if (FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(S, T)) < Within) return true;
            Push((W(S, T, P.Z) - Chuck->GetActorLocation()).GetSafeNormal2D()); return false;
        };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        auto Shot = [&](const TCHAR* Name) { if (Run->Shots < 40) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("RunPlay_%s.png"), Name), true, false); } };
        auto Reset = [&](float S, float T, float Z, float DS, float DT)
        {
            Chuck->ResetAtLocation(W(S, T, Z + Stand)); Chuck->SetActorRotation(Dir(DS, DT).Rotation());
            Chuck->SetRunHeld(false); Stop(); Chuck->Recenter();
        };
        // A straight run from A to B (frame, on the upper floor) starting Delay from now, from a standstill
        // (a 0.4 s ramp to his run pace): does it meet an axe's blade or a burning jet? Checked a little early and late too.
        auto Clear = [&](const FVector2D& A, const FVector2D& B, double Delay) -> bool
        {
            const float Len = FVector2D::Distance(A, B); const FVector2D U = (B - A).GetSafeNormal();
            for (const float J : {-.15f, 0.f, .15f})
                for (float T = 0;; T += .03f)
                {
                    const float D = T < .4f ? .5f * Pace / .4f * T * T : Pace * .2f + Pace * (T - .4f);
                    if (D > Len + 30) break;
                    const FVector2D Q = A + U * D;
                    const FVector Wp = W(Q.X, Q.Y, UZ + Stand);
                    const double Tm = Now + Delay + J + T;
                    for (int32 K = 0; K < Traps->Axes.Num(); ++K) if (Traps->AxeMeets(K, Tm, Wp, Half, Radius + 6)) return false;
                    for (int32 K = 0; K < Traps->Jets.Num(); ++K) if (Traps->JetMeets(K, Tm, Wp, Half, Radius + 6)) return false;
                }
            return true;
        };
        // Wait at A until the straight run to B is clear, then run it (the way a player reads a trap). True on arrival.
        auto Timed = [&](float S0, float T0, float S1, float T1) -> bool
        {
            if (Run->Sub % 2 == 0)
            {
                Chuck->SetRunHeld(false);
                if (!Steer(S0, T0, 25.f)) return false;
                Stop();
                if (M->Velocity.Size2D() > 15.f) return false;
                if (Run->WaitFrom < 0) Run->WaitFrom = Now;
                if (!Clear(FVector2D(S0, T0), FVector2D(S1, T1), 0)) return false;
                Run->Waited += Now - Run->WaitFrom; Run->WaitFrom = -1;
                Chuck->SetRunHeld(true); ++Run->Sub; return false;
            }
            if (Steer(S1, T1, 35.f)) { ++Run->Sub; return true; }
            return false;
        };
        // A pit along +T crossed on the wall at S=WallS: run in along it, jump at the lip; with a chain,
        // kick across once KickAt (a T) is passed. Done once he stands beyond the far lip.
        auto PitRun = [&](float WallS, float Lip, float Far, float From, bool bChain, float KickAt) -> int32
        {
            if (Run->Sub == 0) { Chuck->SetRunHeld(true); Run->Sub = 1; Run->SideRuns = Chuck->GetWallSideRuns(); Run->Chains = Chuck->GetWallChains(); }
            if (Run->Sub == 1)
            {
                Push(Dir(0, 1) + Dir(WallS - P.X, 0) * FMath::Clamp((WallS - P.X) / 40.f, -1.f, 1.f) * .5f);
                if (P.Y > Lip - 22 && Ground) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; Run->Travel = P.Y; }
                return 0;
            }
            Push(Dir(0, 1));
            if (Run->Sub == 2 && Chuck->IsWallSideRunning()) { Shot(bChain ? TEXT("ChainFirst") : TEXT("SideRun")); Run->Sub = 3; }
            if (Run->Sub == 3 && bChain && Chuck->IsWallSideRunning() && P.Y > KickAt) { Chuck->JumpPressed(); Run->Sub = 4; Run->At = Now; }
            if (Run->Sub == 4 && Chuck->GetWallChains() > Run->Chains) { Shot(TEXT("ChainSecond")); Run->Sub = 5; }
            if (Ground && P.Y > Far + 5 && Now - Run->At > .3f)
            {
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_PIT lip=%.0f far=%.0f landed_t=%.0f side_runs=%d chains=%d side_travel=%.0f from_takeoff=%.0f"),
                    Lip, Far, P.Y, Chuck->GetWallSideRuns() - Run->SideRuns, Chuck->GetWallChains() - Run->Chains, Chuck->GetWallSideTravel(), P.Y - Run->Travel);
                return 1;
            }
            return 0;
        };

        // ---- Next trial.
        if (!Run->bStarted)
        {
            if (Now < Run->Next) return;
            ++Run->Trial;
            FString Only;
            if (FParse::Value(FCommandLine::Get(), TEXT("ChuckRunTrials="), Only))
                while (Run->Trial < UE_ARRAY_COUNT(Trials) && !Only.Contains(Names[Run->Trial])) { Run->Results.Add(1); ++Run->Trial; }
            if (Run->Trial >= UE_ARRAY_COUNT(Trials))
            {
                int32 Fails = Run->Failures; FString Line;
                for (int32 K = 0; K < Run->Results.Num(); ++K) { Fails += !Run->Results[K]; Line += FString::Printf(TEXT(" %s=%d"), Names[K], Run->Results[K]); }
                Run->bDone = true; Stop();
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_TEST failures=%d%s"), Fails, *Line);
                FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0);
                return;
            }
            if (Run->Camera.IsValid()) Run->Camera->Destroy();
            Chuck->SetActorHiddenInGame(false);
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
            Run->Sub = Run->Leg = 0; Run->bPass = Run->bFlag = false; Run->At = Run->WaitFrom = Run->Lip = -1; Run->Waited = 0; Run->Furthest = -1e9f;
            Run->Deaths = Chuck->GetFallDeaths(); Run->AxeHits = Traps->AxeHits; Run->JetHits = Traps->JetHits; Run->SawHits = Traps->SawHits; Run->Rolls = Traps->SawRollUnders;
            Run->WallJumps = Chuck->GetWallJumps(); Run->Mantles = Chuck->GetMantles(); Run->Hangs = Chuck->GetHangs();
            switch (Trials[Run->Trial])
            {
            case ET::Climb: Reset(ArchS + 52, ExitT, Floor, 1, 0); break;
            case ET::PitSingle: Reset(BS1 - 32, -7650, UZ, 0, 1); break;
            case ET::PitChain: Reset(BS1 - 32, -6900, UZ, 0, 1); break;
            case ET::PitTwist: Reset(BS1 - 32, -5980, UZ, 0, 1); break;
            case ET::PitFall: Reset(BM, -7500, UZ, 0, 1); break;
            case ET::LeapShort: Reset(BM, -7060, UZ, 0, 1); break;
            case ET::SideShort: Reset(BS1 - 32, -6900, UZ, 0, 1); break;
            case ET::Axes: Reset(68700, CM, UZ, 1, 0); break;
            case ET::AxeHit: Reset(AxeS[1] - 120, CM, UZ, 1, 0); break;
            case ET::Fire: Reset(DM, -4900, UZ, 0, -1); break;
            case ET::FireStrip: Reset(Jets[1].bPlusWall ? DS0 + 40 : DS1 - 40, Jets[1].T + 100, UZ, 0, -1); break;
            case ET::FireHit: Reset(DM, Jets[0].T + 100, UZ, 0, -1); break;
            case ET::Saws: case ET::SawHit: Reset(DM, EM, UZ, 1, 0); break;
            case ET::Gauntlet: case ET::Flow: Reset(FS1 - 32, -7600, UZ, 0, 1); break;
            }
            Chuck->SetOrbitPitch(-14);
            Run->Start = Now; Run->bStarted = true; return;
        }
        const float Time = Now - Run->Start;
        const int32 NewDeaths = Chuck->GetFallDeaths() - Run->Deaths;
        Run->Furthest = FMath::Max(Run->Furthest, static_cast<float>(P.Y));
        bool Done = false; float Limit = 40;
        switch (Trials[Run->Trial])
        {
        case ET::Climb:
        {
            // Up the rubble (mantles), into the chimney, wall jumps back and forth to its lip, up onto the gallery.
            Limit = 40;
            const float Mid = (ShaftT0 + ShaftT1) * .5f;
            if (Run->Sub >= 2 && Now >= Run->Next) { Run->Next = Now + .25; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_CLIMB_TRACE sub=%d gait=%s at=%s wall_runs=%d wall_jumps=%d"), Run->Sub, Chuck->GetGaitName(), *P.ToString(), Chuck->GetWallRuns(), Chuck->GetWallJumps()); }
            if (Run->Sub == 0) { Chuck->SetRunHeld(false); if (Steer(CollapseS1 - 25, ExitT, 22)) Run->Sub = 1; }
            else if (Run->Sub == 1) { if (Steer(BM, ShaftT0 + 26, 12)) { Stop(); Run->Sub = 2; Run->At = Now; } }
            else if (Run->Sub == 2 && Now - Run->At > .4f)
            {
                Shot(TEXT("ChimneyFoot"));
                Chuck->SetActorRotation(Dir(0, -1).Rotation()); Chuck->SetTestStickWorld(Dir(0, -1)); Chuck->JumpPressed();
                Run->Sub = 3; Run->At = -1;
            }
            else if (Run->Sub == 3)
            {
                // Kick off each wall a quarter second into its run; at the lip, a jump pulls him up.
                const bool Wall = Chuck->IsWallRunning();
                if (Wall && Run->At < 0) { Run->At = Now; Stop(); }
                if (!Wall) Run->At = -1;
                if (Wall && Run->At > 0 && Now - Run->At > .25f) { Chuck->JumpPressed(); Run->At = -1; }
                if (Chuck->IsHanging()) { Shot(TEXT("ChimneyLip")); Run->Sub = 4; Run->At = Now; }
                if (Ground && Chuck->GetWallJumps() - Run->WallJumps > 0 && P.Z < UZ - 100 && Now - Run->Start > 8)
                {
                    // Back on the chimney floor: go again.
                    Run->Sub = 2; Run->At = Now;
                }
            }
            else if (Run->Sub == 4 && Now - Run->At > .3f) { Chuck->JumpPressed(); Run->Sub = 5; Run->At = Now; }
            else if (Run->Sub == 5 && Ground && Now - Run->At > .5f)
            {
                Push(Dir(0, 1));
                if (P.Y > -7750)
                {
                    Done = true; Stop();
                    Run->bPass = FMath::Abs(P.Z - Stand - UZ) < 12 && NewDeaths == 0 && Chuck->GetMantles() - Run->Mantles >= Steps - 1;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_CLIMB mantles=%d wall_jumps=%d hangs=%d seconds=%.1f at=%s"), Chuck->GetMantles() - Run->Mantles, Chuck->GetWallJumps() - Run->WallJumps, Chuck->GetHangs() - Run->Hangs, Time, *P.ToString());
                }
            }
            (void)Mid;
            break;
        }
        case ET::PitSingle:
        {
            Limit = 15;
            if (PitRun(BS1 - 32, Pits[0].T0, Pits[0].T1, -7650, false, 0)) { Done = true; Run->bPass = NewDeaths == 0 && Chuck->GetWallSideRuns() > Run->SideRuns; }
            if (NewDeaths) Done = true;
            break;
        }
        case ET::PitChain:
        {
            Limit = 15;
            if (PitRun(BS1 - 32, Pits[1].T0, Pits[1].T1, -6900, true, Pits[1].T0 + 120)) { Done = true; Run->bPass = NewDeaths == 0 && Chuck->GetWallChains() > Run->Chains; }
            if (NewDeaths) Done = true;
            break;
        }
        case ET::PitTwist:
        {
            // The -S wall is broken back over the first part: start on the +S wall, kick only once the far wall is there.
            Limit = 15;
            if (PitRun(BS1 - 32, Pits[2].T0, Pits[2].T1, -5980, true, BreakT1 - 100)) { Done = true; Run->bPass = NewDeaths == 0 && Chuck->GetWallChains() > Run->Chains; }
            if (NewDeaths) Done = true;
            break;
        }
        case ET::PitFall:
        {
            // Walk into the first pit: on the spikes, back at the gallery's start.
            Limit = 15;
            if (Run->Sub == 0) { Chuck->SetRunHeld(true); Push(Dir(0, 1)); if (NewDeaths) { Run->Sub = 1; Run->At = Now; Stop(); } }
            else if (Now - Run->At > 1.5f)
            {
                const bool Home = FVector::Dist(Chuck->GetActorLocation(), ChultTempleInteriorSpawn(ETempleInteriorSpawn::Pits)) < 40 && Ground && !Chuck->IsAstral();
                if (Home || Now - Run->At > 8) { Done = true; Run->bPass = Home && NewDeaths == 1; }
            }
            break;
        }
        case ET::LeapShort:
        {
            // A sprint leap from the middle of the corridor at the long pit's lip falls short: the pit asks for the chained run.
            Limit = 12;
            if (Run->Sub == 0) { Chuck->SetRunHeld(true); Push(Dir(0, 1)); if (Time > .5f) { Chuck->SetStamina(1.f); Chuck->TrySprint(); Run->Sub = 1; } }
            else if (Run->Sub == 1) { Push(Dir(0, 1)); if (P.Y > Pits[1].T0 - 15 && Ground) { Chuck->JumpPressed(); Run->Sub = 2; Run->bFlag = Chuck->IsSprinting(); } }
            else { Push(Dir(0, 1)); if (NewDeaths || (Ground && P.Y > Pits[1].T1)) { Done = true; Run->bPass = NewDeaths == 1 && Run->bFlag; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_LEAP_SHORT sprinting=%d furthest_t=%.0f deaths=%d"), Run->bFlag, Run->Furthest, NewDeaths); } }
            break;
        }
        case ET::SideShort:
        {
            // One side wall run, no kick: short of the long pit's far lip.
            Limit = 12;
            const int32 R = PitRun(BS1 - 32, Pits[1].T0, Pits[1].T1, -6900, false, 0);
            if (R || NewDeaths) { Done = true; Run->bPass = NewDeaths == 1 && Chuck->GetWallSideRuns() > Run->SideRuns; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_SIDE_SHORT furthest_t=%.0f side_travel=%.0f deaths=%d"), Run->Furthest, Chuck->GetWallSideTravel(), NewDeaths); }
            break;
        }
        case ET::Axes:
        {
            // The lone axe, then the wave of three in one go: no hits, no falls.
            Limit = 60;
            if (Run->Leg == 0) { if (Timed(69040, CM, 69700, CM)) { Run->Leg = 1; Run->Sub = 0; Shot(TEXT("AxeOne")); } }
            else if (Run->Leg == 1) { if (Timed(69700, CM, 70660, CM)) { Run->Leg = 2; Shot(TEXT("AxeWave")); } }
            else { Done = true; Run->bPass = Traps->AxeHits == Run->AxeHits && NewDeaths == 0; }
            if (NewDeaths) Done = true;
            if (Done) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_AXES hits=%d deaths=%d waited=%.1f seconds=%.1f"), Traps->AxeHits - Run->AxeHits, NewDeaths, Run->Waited, Time);
            break;
        }
        case ET::AxeHit:
        {
            // Stand in the wave's first axe's path: it hits once and knocks him back along the causeway.
            Limit = 15;
            if (Run->Sub == 0) { Chuck->SetRunHeld(false); if (Steer(AxeS[1], CM, 12)) { Stop(); Run->Sub = 1; Run->At = Now; } }
            else if (Traps->AxeHits > Run->AxeHits || Now - Run->At > 5)
            {
                Done = true; Run->bPass = Traps->AxeHits == Run->AxeHits + 1 && Chuck->GetSanity() == AChuckCharacter::MaxSanity - 1 && NewDeaths == 0;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_AXE_HIT hits=%d sanity=%d deaths=%d"), Traps->AxeHits - Run->AxeHits, Chuck->GetSanity(), NewDeaths);
            }
            break;
        }
        case ET::Fire:
        {
            // The lone mouth, the slalom straight down the middle once its wave has passed, then the paired jets.
            Limit = 70;
            if (Run->Leg == 0) { if (Timed(DM, -5100, DM, -5620)) { Run->Leg = 1; Run->Sub = 0; } }
            else if (Run->Leg == 1) { if (Timed(DM, -5620, DM, -6820)) { Run->Leg = 2; Run->Sub = 0; Shot(TEXT("FireSlalom")); } }
            else if (Run->Leg == 2) { if (Timed(DM, -6820, DM, -7560)) Run->Leg = 3; }
            else { Done = true; Run->bPass = Traps->JetHits == Run->JetHits && NewDeaths == 0; }
            if (Done) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_FIRE hits=%d waited=%.1f seconds=%.1f fired=%d"), Traps->JetHits - Run->JetHits, Run->Waited, Time, Traps->JetsFired);
            break;
        }
        case ET::FireStrip:
        {
            // Beside a slalom jet, in the strip it doesn't reach (the weave's line), through two bursts: no hit.
            Limit = 20;
            const float Strip = Jets[1].bPlusWall ? DS0 + 40 : DS1 - 40;
            if (Run->Sub == 0) { if (Steer(Strip, Jets[1].T, 10)) { Stop(); Run->Sub = 1; Run->At = Now; Run->Mark = Traps->JetsFired; } }
            else if (Now - Run->At > 2 * FirePeriod + .2f)
            {
                Done = true; Run->bPass = Traps->JetHits == Run->JetHits && Traps->JetsFired > Run->Mark;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_FIRE_STRIP hits=%d bursts=%d"), Traps->JetHits - Run->JetHits, Traps->JetsFired - Run->Mark);
            }
            break;
        }
        case ET::FireHit:
        {
            Limit = 20;
            if (Run->Sub == 0) { if (Steer(DM, Jets[0].T, 10)) { Stop(); Run->Sub = 1; Run->At = Now; } }
            else if (Traps->JetHits > Run->JetHits || Now - Run->At > FirePeriod + .5f)
            {
                Done = true; Run->bPass = Traps->JetHits == Run->JetHits + 1 && Chuck->GetSanity() == AChuckCharacter::MaxSanity - 1;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_FIRE_HIT hits=%d sanity=%d"), Traps->JetHits - Run->JetHits, Chuck->GetSanity());
            }
            break;
        }
        case ET::Saws:
        {
            // Down the passage at a run, rolling under each row: no hits.
            Limit = 40;
            Chuck->SetRunHeld(true);
            if (Run->Leg < UE_ARRAY_COUNT(SawRows))
            {
                Push((W(SawRows[Run->Leg].S + 200, EM, UZ) - Chuck->GetActorLocation()).GetSafeNormal2D());
                const float To = SawRows[Run->Leg].S - P.X;
                if (To < 70 && To > 0 && !Chuck->IsRolling() && Ground) { Chuck->DodgeToward(FVector2D::ZeroVector); if (Run->Leg == 0) Shot(TEXT("SawRoll")); }
                if (To < -40) ++Run->Leg;
            }
            else if (Steer(ES1 - 100, EM, 40)) { Done = true; Run->bPass = Traps->SawHits == Run->SawHits && NewDeaths == 0 && Traps->SawRollUnders > Run->Rolls; }
            if (Done || Time > Limit - .1f) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_SAWS hits=%d roll_unders=%d rows=%d drops=%d seconds=%.1f"), Traps->SawHits - Run->SawHits, Traps->SawRollUnders - Run->Rolls, Run->Leg, Traps->SawDrops, Time);
            break;
        }
        case ET::SawHit:
        {
            // Walk under the first (always lowered) row without rolling: its teeth find him.
            Limit = 15;
            if (Run->Sub == 0) { Chuck->SetRunHeld(false); Push(Dir(1, 0)); if (Traps->SawHits > Run->SawHits) { Run->Sub = 1; Run->At = Now; } if (P.X > SawRows[0].S + 80) Run->Sub = 1; }
            else { Stop(); Done = true; Run->bPass = Traps->SawHits == Run->SawHits + 1 && Chuck->GetSanity() == AChuckCharacter::MaxSanity - 1; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_SAW_HIT hits=%d sanity=%d"), Traps->SawHits - Run->SawHits, Chuck->GetSanity()); }
            break;
        }
        case ET::Gauntlet:
        {
            // All of it in one go: the pit on the wall, the axes read and run, roll under the saws, the fire read
            // and run, the chained run over the last pit, into the antechamber.
            Limit = 90;
            switch (Run->Leg)
            {
            case 0: if (PitRun(FS1 - 32, GPits[0].T0, GPits[0].T1, -7600, false, 0)) { Run->Leg = 1; Run->Sub = 0; } break;
            case 1: if (Timed(FM, -6980, FM, -6150)) { Run->Leg = 2; Run->Sub = 0; Shot(TEXT("GauntletAxes")); } break;
            case 2:
            {
                Chuck->SetRunHeld(true); Push(Dir(0, 1));
                for (const FGSaw& Sw : GSaws) { const float To = Sw.T - P.Y; if (To < 70 && To > 0 && !Chuck->IsRolling() && Ground) Chuck->DodgeToward(FVector2D::ZeroVector); }
                if (P.Y > GSawT1 - 60) { Run->Leg = 3; Run->Sub = 0; }
                break;
            }
            case 3: if (Timed(FM, GFireT0 + 40, FM, -4700)) { Run->Leg = 4; Run->Sub = 0; } break;
            case 4: if (Run->Sub == 0) { if (Steer(FS1 - 32, -4680, 15)) { Run->Sub = 0; Run->Leg = 5; } } break;
            case 5: if (PitRun(FS1 - 32, GPits[1].T0, GPits[1].T1, -4680, true, GPits[1].T0 + 120)) Run->Leg = 6; break;
            default: Chuck->SetRunHeld(false); if (Steer(FM, AnteT0 + 150, 40)) { Done = true; Run->bPass = NewDeaths == 0; } break;
            }
            if (NewDeaths) Done = true;
            if (Done || Time > Limit - .1f) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_GAUNTLET leg=%d deaths=%d axe_hits=%d jet_hits=%d saw_hits=%d rolls=%d waited=%.1f seconds=%.1f"),
                Run->Leg, NewDeaths, Traps->AxeHits - Run->AxeHits, Traps->JetHits - Run->JetHits, Traps->SawHits - Run->SawHits, Traps->SawRollUnders - Run->Rolls, Run->Waited, Time);
            break;
        }
        case ET::Flow:
        {
            // The gauntlet on its beat without a stop (its traps are phased for a runner who leaves the first
            // pit's lip on the beat and keeps his pace): the side run, straight on through the axes, a roll under
            // the saws when they are down, through the fire, the chained run. Each trap's crossing is logged.
            Limit = 40;
            struct FMarkT { float T; const TCHAR* Name; };
            static const FMarkT Marks[] = {{GAxeT[0], TEXT("axe0")}, {GAxeT[1], TEXT("axe1")}, {GSaws[0].T, TEXT("saw0")}, {GSaws[1].T, TEXT("saw1")}, {GJetT[0], TEXT("jet0")}, {GJetT[1], TEXT("jet1")}, {GPits[1].T0, TEXT("pit1")}};
            if (Run->Leg == 0)
            {
                // Leave so as to reach the lip on the beat (about 1.05 s from a standstill here).
                if (FMath::Fmod(static_cast<float>(Now) + 1.05f, GauntletBeat) < .02f) { Run->Leg = 1; Run->Sub = 0; Run->LastY = P.Y; }
            }
            else
            {
                if (Run->Lip < 0 && P.Y > GPits[0].T0) Run->Lip = Now;
                for (int32 K = 0; K < UE_ARRAY_COUNT(Marks); ++K)
                    if (Run->LastY < Marks[K].T && P.Y >= Marks[K].T && Run->Lip > 0)
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_FLOW mark=%s t=%.2f lip_beat=%.2f"), Marks[K].Name, Now - Run->Lip, FMath::Fmod(static_cast<float>(Run->Lip), GauntletBeat));
                Run->LastY = P.Y;
                if (Run->Leg == 1) { if (PitRun(FS1 - 32, GPits[0].T0, GPits[0].T1, -7600, false, 0)) { Run->Leg = 2; Run->Sub = 0; } }
                else if (Run->Leg == 2)
                {
                    Chuck->SetRunHeld(true);
                    // Straight down the middle, then back to the +S wall for the last pit.
                    const float Line = P.Y > GFireT1 - 100 ? FS1 - 32 : FM;
                    Push((Dir(0, 1) + Dir(Line - P.X, 0) * FMath::Clamp(FMath::Abs(Line - P.X) / 40.f, 0.f, 1.f) * .6f).GetSafeNormal());
                    for (int32 K = 0; K < Traps->Saws.Num(); ++K)
                    {
                        const FVector F = ChultTempleInteriorFrameOf(Traps->Saws[K].Axle);
                        const float To = F.Y - P.Y;
                        if (FMath::Abs(F.X - FM) < 100 && To < 75 && To > 0 && !Chuck->IsRolling() && Ground && Traps->SawHeight(K, Now + To / Pace) < 40.f) Chuck->DodgeToward(FVector2D::ZeroVector);
                    }
                    if (P.Y > GPits[1].T0 - 260) { Run->Leg = 3; Run->Sub = 1; Run->SideRuns = Chuck->GetWallSideRuns(); Run->Chains = Chuck->GetWallChains(); }
                }
                else if (Run->Leg == 3) { if (PitRun(FS1 - 32, GPits[1].T0, GPits[1].T1, -4760, true, GPits[1].T0 + 120)) Run->Leg = 4; }
                else { Done = true; Run->bPass = NewDeaths == 0; }
            }
            if (NewDeaths) Done = true;
            if (Done || Time > Limit - .1f) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_FLOW_RESULT leg=%d deaths=%d axe_hits=%d jet_hits=%d saw_hits=%d rolls=%d seconds_from_lip=%.1f"),
                Run->Leg, NewDeaths, Traps->AxeHits - Run->AxeHits, Traps->JetHits - Run->JetHits, Traps->SawHits - Run->SawHits, Traps->SawRollUnders - Run->Rolls, Run->Lip > 0 ? Now - Run->Lip : -1.0);
            break;
        }
        }
        if (!Done && Time < Limit) return;
        Stop(); Chuck->SetRunHeld(false);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_TRIAL name=%s pass=%d seconds=%.1f leg=%d sub=%d deaths=%d sanity=%d gait=%s at=%s"),
            Names[Run->Trial], Run->bPass, Time, Run->Leg, Run->Sub, NewDeaths, Chuck->GetSanity(), Chuck->GetGaitName(), *P.ToString());
        Run->Results.Add(Run->bPass ? 1 : 0); Run->bStarted = false; Run->Next = Now + 1.2;
    }, .016f, true);
}
