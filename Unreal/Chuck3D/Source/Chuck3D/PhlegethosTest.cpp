// -ChuckChultTest -ChuckPhlegethosTest (Claude, 2026-11-14; PhlegethosSetting.h): the real character through Phlegethos's
// challenges, each a trial placed at its start: the road's hops and its sprint leap past the geysers, every column hop
// of the lake (the wall run up the tall one), the chained wall run between the slabs, the gorge's side wall run and drop,
// the chains, the bomb ramp, the crumbling steps, the chimney, the last leap, the shut gate; and stills of each stretch.
// -ChuckPhlegethosTest=<trial> runs one. -ChuckHellFallTest: the fall cutscene alone, its shots, then the handoff.
#include "PhlegethosSetting.h"
#include "PhlegethosFall.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

using namespace Phleg;

namespace
{
    constexpr float Stand = 34.65f;
    enum class ET : uint8 { Stills, RoadHops, RoadLeap, LakeHops, LakeSlabs, GorgeWall, GorgeDrop, Chains, Bombs, Steps, Chimney, Leap, Approach, Gate, Count };
    const TCHAR* Names[] = {TEXT("stills"), TEXT("road_hops"), TEXT("road_leap"), TEXT("lake_hops"), TEXT("lake_slabs"), TEXT("gorge_wall"), TEXT("gorge_drop"), TEXT("chains"), TEXT("bombs"), TEXT("steps"), TEXT("chimney"), TEXT("leap"), TEXT("approach"), TEXT("gate")};
    // The spine's centre line at X (the segment it is on).
    float SpineMid(float X) { for (const FSeg& S : Spine) if (X >= S.X0 - 60 && X <= S.X1 + 60) return FMath::Lerp(S.Y0, S.Y1, FMath::SmoothStep(0.f, 1.f, (X - S.X0) / (S.X1 - S.X0))); return 0.f; }
}

void ReviewPhlegethos(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckHellFallTest")))
    {
        // The cutscene alone: from the ship's end into the fall, its shots, the handoff to the plain.
        if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->ConsoleCommand(TEXT("t.MaxFPS 60"));
        Chuck->BeginHellFall();
        struct FWait { FTimerHandle Timer; double Start = 0; bool bDone = false; };
        auto W = MakeShared<FWait>(); W->Start = World->GetTimeSeconds();
        Owner->GetWorldTimerManager().SetTimer(W->Timer, [Owner, Chuck, W, Failures]()
        {
            if (W->bDone) return;
            const double T = Owner->GetWorld()->GetTimeSeconds() - W->Start;
            if (!Chuck->IsInFallCutscene() && T > 2)
            {
                W->bDone = true;
                const APhlegethosSetting* S = APhlegethosSetting::Get(Owner->GetWorld());
                const FVector L = PhlegLocal(Chuck->GetActorLocation());
                const bool bPass = S && S->IsLive() && FVector::Dist(Chuck->GetActorLocation(), PhlegethosSpawn(EPhlegSpawn::Arrival)) < 60 && S->GetMissingMaterials() == 0;
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("HellFall/Handoff.png"), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_TEST failures=%d handoff=%d at=%s seconds=%.1f"), Failures + !bPass, bPass, *L.ToString(), T);
                FTimerHandle Quit; Owner->GetWorldTimerManager().SetTimer(Quit, [Failures, bPass]() { FPlatformMisc::RequestExitWithStatus(false, (Failures + !bPass) ? 1 : 0); }, 2.f, false);
            }
            else if (T > 90) { W->bDone = true; UE_LOG(LogTemp, Warning, TEXT("CHUCK_HELL_FALL_TEST failures=1 timeout=1")); FPlatformMisc::RequestExitWithStatus(false, 1); }
        }, .1f, true);
        return;
    }

    APhlegethosSetting* Stage = APhlegethosSetting::Enter(World, true);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }
    const bool bBuilt = Stage && Stage->GetMissingMaterials() == 0 && Stage->GetMoverCount(0) > 0 && Stage->GetMoverCount(1) == UE_ARRAY_COUNT(Steps);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_BUILD pass=%d missing=%d sinkers=%d steps=%d triangles=%d"), bBuilt, Stage ? Stage->GetMissingMaterials() : -1,
        Stage ? Stage->GetMoverCount(0) : 0, Stage ? Stage->GetMoverCount(1) : 0, Stage ? Stage->GetTriangles() : 0);
    Failures += !bBuilt;

    FString Only; FParse::Value(FCommandLine::Get(), TEXT("ChuckPhlegethosTest="), Only);
    struct FRun
    {
        int32 Trial = 0, Sub = 0, Hop = 0, Failures = 0, Deaths0 = 0, Shots = 0, Chains0 = 0, SideRuns0 = 0, Passed = 0, Struck = -1;
        double At = 0, Start = 0, TrialStart = 0, Next = 0;
        TArray<FString> Results; FTimerHandle Timer; bool bDone = false, bPlaced = false; FString Note;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Start = World->GetTimeSeconds();
    if (!Only.IsEmpty()) for (int32 I = 0; I < int32(ET::Count); ++I) if (Only == Names[I]) Run->Trial = I;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run, Only]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        APhlegethosSetting* Stage = APhlegethosSetting::Get(World);
        const double Now = World->GetTimeSeconds();
        const float T = static_cast<float>(Now - Run->At);
        auto* M = Chuck->GetCharacterMovement();
        const FVector P = PhlegLocal(Chuck->GetActorLocation());
        const bool Ground = M->IsMovingOnGround();
        auto Push = [&](const FVector& D) { const FVector N = D.GetSafeNormal2D(); Chuck->SetTestStickWorld(N); if (!Chuck->IsWallSideRunning()) Chuck->AddMovementInput(N, 1.f, true); };
        auto Toward = [&](float X, float Y) { Push(FVector(X - P.X, Y - P.Y, 0)); };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        auto Shot = [&](const FString& Name) { if (Run->Shots < 60) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Phlegethos") / (Name + TEXT(".png")), true, false); } };
        auto Place = [&](float X, float Y, float Z, float Yaw)
        {
            Chuck->ResetAtLocation(PhlegWorld(FVector(X, Y, Z + Stand)));
            Chuck->SetActorRotation(FRotator(0, Yaw, 0)); Chuck->SetRunHeld(false); Stop(); Chuck->Recenter();
            if (Stage) Stage->ResetHazards();
            Run->Deaths0 = Chuck->GetFallDeaths(); Run->At = Now; Run->Sub = 0;
        };
        const int32 Deaths = Chuck->GetFallDeaths() - Run->Deaths0;
        auto Next = [&](bool bPass, const FString& Note)
        {
            const TCHAR* Name = Names[Run->Trial];
            Run->Results.Add(FString::Printf(TEXT("%s=%d"), Name, bPass ? 1 : 0)); Run->Failures += !bPass; Run->Passed += bPass;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_TRIAL name=%s pass=%d %s at=%s deaths=%d sanity=%d t=%.1f"), Name, bPass ? 1 : 0, *Note, *P.ToString(), Deaths, Chuck->GetSanity(), Now - Run->TrialStart);
            Stop(); Chuck->SetRunHeld(false);
            ++Run->Trial; Run->Sub = 0; Run->Hop = 0; Run->bPlaced = false; Run->Note.Empty();
            if (!Only.IsEmpty()) Run->Trial = int32(ET::Count);
        };
        if (Run->Trial >= int32(ET::Count))
        {
            Run->bDone = true;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_TEST failures=%d passed=%d trials=%s sunk=%d crumbled=%d geyser_hits=%d bomb_hits=%d bombs=%d seconds=%.0f"), Run->Failures, Run->Passed,
                *FString::Join(Run->Results, TEXT(" ")), Stage ? Stage->GetSunk() : 0, Stage ? Stage->GetCrumbled() : 0, Stage ? Stage->GetGeyserHits() : 0, Stage ? Stage->GetBombHits() : 0, Stage ? Stage->GetBombs() : 0, Now - Run->Start);
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
            return;
        }
        if (!Run->bPlaced) { Run->bPlaced = true; Run->TrialStart = Now; Run->At = Now; Run->Sub = -1; }
        // A death ends any trial but the stills.
        if (Run->Trial != int32(ET::Stills) && Run->Trial != int32(ET::LakeHops) && Run->Sub >= 0 && Deaths > 0 && Now - Run->At > .3f) { Next(false, FString::Printf(TEXT("died=1 sub=%d hop=%d %s"), Run->Sub, Run->Hop, *Run->Note)); return; }
        // (The wastes' walk is 155 m: about 70 s at his run.)
        if (Now - Run->TrialStart > (Run->Trial == int32(ET::Approach) ? 130.f : 75.f)) { Next(false, FString::Printf(TEXT("timeout=1 sub=%d hop=%d gait=%s"), Run->Sub, Run->Hop, Chuck->GetGaitName())); return; }
        switch (ET(Run->Trial))
        {
        case ET::Stills:
        {
            // Each stretch's start, looking on along the way (rat height, then raised).
            const int32 K = Run->Hop;
            // (Then the geysers on the road: four shots over one period, so the warning mark and the fountain both show.)
            if (K >= PhlegSpawns)
            {
                if (Run->Sub == -1) { Place(4560, SpineMid(4560), G, 0); Chuck->SetOrbitPitch(-14); Run->Sub = 10; }
                else if (Run->Sub >= 10 && Run->Sub < 14 && T > 1.2f + .85f * (Run->Sub - 10)) { Shot(FString::Printf(TEXT("Geyser_%d"), Run->Sub - 10)); ++Run->Sub; }
                else if (Run->Sub >= 14) Next(true, FString::Printf(TEXT("stills=%d"), PhlegSpawns * 2 + 4));
                break;
            }
            if (Run->Sub == -1)
            {
                const FVector S = PhlegLocal(PhlegethosSpawn(EPhlegSpawn(K)));
                Place(S.X, S.Y, S.Z - Stand, 0); Chuck->SetOrbitPitch(-8); Run->Sub = 1;
            }
            // (Lightning ahead for the rat-height shot of every other stretch, so the stills show it.)
            else if (Run->Sub == 1 && T > 2.43f && Run->Struck != K && K % 2 == 0) { if (auto* S = APhlegethosSetting::Get(Chuck->GetWorld())) S->Strike(K == 4, 14.f); Run->Struck = K; }
            else if (Run->Sub == 1 && T > 2.5f) { Shot(FString::Printf(TEXT("%d_%s_Rat"), K, PhlegethosStretchName(K))); Chuck->SetOrbitPitch(-40); Run->Sub = 2; Run->At = Now; }
            else if (Run->Sub == 2 && T > 1.5f) { Shot(FString::Printf(TEXT("%d_%s_High"), K, PhlegethosStretchName(K))); ++Run->Hop; Run->Sub = -1; }
            break;
        }
        case ET::RoadHops:
        {
            // Along the spine at a run, jumping its two breaks.
            if (Run->Sub == -1) { Place(Spine[0].X0 + 1100, SpineMid(Spine[0].X0 + 1100), G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); break; }
            Toward(P.X + 200, SpineMid(P.X + 200));
            for (const float Lip : {Spine[0].X1, Spine[1].X1}) if (Ground && P.X > Lip - 26 && P.X < Lip) Chuck->JumpPressed();
            if (P.X > 3820 && P.X < 3860 && Run->Sub == 0) { Shot(TEXT("RoadHop")); Run->Sub = 1; }
            if (Run->Sub >= 0 && T > .5f && Ground && P.X > Spine[2].X0 + 300) Next(P.Z > G + 20, TEXT(""));
            break;
        }
        case ET::RoadLeap:
        {
            // Past the geysers at a run, then a sprint at the long break and the leap.
            if (Run->Sub == -1) { Place(Spine[3].X0 + 300, SpineMid(Spine[3].X0 + 300), G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); }
            Toward(P.X + 200, SpineMid(P.X + 200));
            if (Run->Sub == 0 && P.X > Spine[3].X1 - 420) { if (Chuck->TrySprint()) Run->Sub = 1; }
            if (Run->Sub == 1 && Ground && P.X > Spine[3].X1 - 24) { Chuck->JumpPressed(); Run->Sub = 2; Run->Note = FString::Printf(TEXT("sprinting=%d"), Chuck->IsSprinting()); }
            if (Run->Sub == 2 && Now - Run->At > 0 && P.X > Spine[3].X1 + 40 && P.X < Spine[4].X0) Shot(TEXT("RoadLeap"));
            if (Run->Sub == 2 && Ground && P.X > Spine[4].X0 + 10) Next(true, Run->Note);
            break;
        }
        case ET::LakeHops:
        {
            // Every column to the next, each hop placed: on the one, toward the next, a running jump at its edge; the
            // tall column's face is run up and its top pulled up onto. Pass: every hop lands.
            const int32 N = UE_ARRAY_COUNT(Columns);
            const int32 I = Run->Hop;
            if (I >= N - 1) { Next(Run->Note.IsEmpty(), FString::Printf(TEXT("hops=%d %s"), N - 1, *Run->Note)); break; }
            const FColumn& A = Columns[I], &B = Columns[I + 1];
            if (B.X - A.X > 300) { ++Run->Hop; Run->Sub = -1; break; }   // (the slabs' gap: its own trial)
            const FVector2D Dir = FVector2D(B.X - A.X, B.Y - A.Y).GetSafeNormal();
            if (Run->Sub == -1)
            {
                // Back from the far edge, so the run-up is the column's width.
                const FVector2D From = FVector2D(A.X, A.Y) - Dir * A.R * .55f;
                Place(From.X, From.Y, G + A.Top, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X))); Run->Sub = 0; Run->At = Now;
                if (FMath::Abs(Deaths) > 0) Run->Deaths0 = Chuck->GetFallDeaths();
            }
            else if (Run->Sub == 0 && T > .5f) { Chuck->SetRunHeld(true); Run->Sub = 1; Run->At = Now; }
            else if (Run->Sub >= 1)
            {
                Push(FVector(B.X - P.X, B.Y - P.Y, 0));
                if (B.Top - A.Top > 60 && Now >= Run->Next) { Run->Next = Now + .1; UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_TALL_TRACE sub=%d gait=%s at=%s vz=%.0f"), Run->Sub, Chuck->GetGaitName(), *P.ToString(), M->Velocity.Z); }
                const float FromA = FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(A.X, A.Y));
                if (Run->Sub == 1 && Ground && FromA > A.R * .62f) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; }
                if (Run->Sub >= 2 && Chuck->IsHanging() && T > .2f) { Chuck->JumpPressed(); }
                const bool bOnB = Ground && FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(B.X, B.Y)) < B.R && FMath::Abs(P.Z - Stand - (G + B.Top)) < 40;
                if (bOnB) { if (I == 7) Shot(TEXT("LakeTallColumn")); if (I == 2) Shot(TEXT("LakeHop")); ++Run->Hop; Run->Sub = -1; }
                else if (T > 6.f) { Run->Note += FString::Printf(TEXT("miss%d "), I); ++Run->Hop; Run->Sub = -1; }
            }
            if (Run->Sub >= 1 && Deaths > 0) { Run->Note += FString::Printf(TEXT("fell%d "), I); ++Run->Hop; Run->Sub = -1; Run->Deaths0 = Chuck->GetFallDeaths(); }
            break;
        }
        case ET::LakeSlabs:
        {
            // Between the slabs: in along the +Y wall, the jump at the column's edge onto it, a kick across to the -Y wall,
            // onto the column beyond.
            const FColumn& A = Columns[10], &B = Columns[11];
            const float WallY = SlabHalfGap - 32;
            if (Run->Sub == -1) { Place(A.X - 50, WallY, G + A.Top, 0); Run->Sub = 0; Run->SideRuns0 = Chuck->GetWallSideRuns(); Run->Chains0 = Chuck->GetWallChains(); }
            else if (Run->Sub == 0 && T > .5f) { Chuck->SetRunHeld(true); Run->Sub = 1; }
            else if (Run->Sub == 1) { Push(FVector(1, (WallY - P.Y) / 60.f, 0)); if (Ground && P.X > A.X + A.R * .72f - 14) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; } }
            else
            {
                Push(FVector(1, 0, 0));
                if (Run->Sub == 2 && Chuck->IsWallSideRunning()) { Shot(TEXT("SlabFirst")); Run->Sub = 3; }
                if (Run->Sub == 3 && Chuck->IsWallSideRunning() && P.X > A.X + A.R + 150) { Chuck->JumpPressed(); Run->Sub = 4; }
                if (Run->Sub == 4 && Chuck->GetWallChains() > Run->Chains0) { Shot(TEXT("SlabSecond")); Run->Sub = 5; }
                // Down off the wall onto the column beyond (or, carried on, the one after it): alive and on a column top.
                if (Ground && P.X > B.X - B.R && Now - Run->At > .3f)
                    Next(P.Z - Stand > G + 20, FString::Printf(TEXT("side_runs=%d chains=%d on_b=%d"), Chuck->GetWallSideRuns() - Run->SideRuns0, Chuck->GetWallChains() - Run->Chains0, FMath::Abs(P.X - B.X) < B.R));
            }
            break;
        }
        case ET::GorgeWall:
        {
            // Along the shelf beside the cliff, the jump at the break, a side wall run on the cliff to the shelf beyond.
            const float WallY = CliffY + 22;
            if (Run->Sub == -1) { Place(Break1X0 - 500, WallY, G, 0); Run->Sub = 0; Run->SideRuns0 = Chuck->GetWallSideRuns(); }
            else if (Run->Sub == 0 && T > .5f) { Chuck->SetRunHeld(true); Run->Sub = 1; }
            else if (Run->Sub == 1) { Push(FVector(1, (WallY - P.Y) / 60.f, 0)); if (Ground && P.X > Break1X0 - 22) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; } }
            else
            {
                Push(FVector(1, 0, 0));
                if (Run->Sub == 2 && Chuck->IsWallSideRunning()) { Shot(TEXT("GorgeWallRun")); Run->Sub = 3; }
                if (Ground && P.X > Break1X1 + 5 && Now - Run->At > .3f) Next(FMath::Abs(P.Z - Stand - G) < 30, FString::Printf(TEXT("side_runs=%d"), Chuck->GetWallSideRuns() - Run->SideRuns0));
            }
            break;
        }
        case ET::GorgeDrop:
        {
            if (Run->Sub == -1) { Place(Break2X0 - 400, -95, G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); }
            Toward(P.X + 200, -95);
            if (Run->Sub == 0 && Ground && P.X > Break2X0 - 24) { Chuck->JumpPressed(); Run->Sub = 1; Run->At = Now; }
            if (Run->Sub == 1 && Ground && P.X > Break2X1 + 5 && Now - Run->At > .3f) Next(FMath::Abs(P.Z - Stand - LowShelf) < 30, TEXT(""));
            break;
        }
        case ET::Chains:
        {
            // Off the low shelf's end into the first ring, hand over hand along the chains, down onto the far side.
            if (Run->Sub == -1) { Place(ChainX0 - 120, ChainY, LowShelf, 0); Run->Sub = 0; Run->Chains0 = Chuck->GetSwingLeaps(); }
            else if (Run->Sub == 0 && T > .5f) { Chuck->SetRunHeld(false); Run->Sub = 1; }
            else if (Run->Sub == 1) { Toward(ChainX0 + 60, ChainY); if (Ground && P.X > ChainX0 - 22) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; } }
            else if (Run->Sub == 2 && T > 3.f && !Chuck->IsSwinging() && Chuck->GetSwingLeaps() == Run->Chains0 && !Ground) Run->Note = TEXT("no_catch=1");
            else if (Run->Sub == 2)
            {
                if (Chuck->IsSwinging())
                {
                    if (Run->At > 0 && Now - Run->At > .2f) Shot(TEXT("Chains"));
                    const bool bLast = P.X > ChainX0 + 70 + (ChainGrips - 1) * ChainStep - 40;   // (on the last ring)
                    if (bLast) { Stop(); if (Now - Run->At > .6f) { Chuck->JumpPressed(); Run->At = Now; } }
                    else { Push(FVector(1, 0, 0)); if (Now - Run->At > .65f) { Chuck->JumpPressed(); Run->At = Now; } }
                }
                else Push(FVector(1, 0, 0));
                if (Ground && P.X > ChainX1) Next(true, FString::Printf(TEXT("leaps=%d"), Chuck->GetSwingLeaps() - Run->Chains0));
                if (Ground && P.X < ChainX0 && Now - Run->At > 1.5f) Next(false, TEXT("no_catch=1"));
            }
            break;
        }
        case ET::Bombs:
        {
            // Up the ramp at a run while the bombs come down. Pass: at the top alive (hits are counted, not failures).
            if (Run->Sub == -1) { Place(RampX0 + 50, 0, G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); Run->Chains0 = Stage ? Stage->GetBombs() : 0; }
            Toward(P.X + 200, 0);
            if (P.X > RampX0 + 800 && Run->Sub == 0) { Shot(TEXT("Bombs")); Run->Sub = 1; }
            if (Ground && P.X > RampX1 - 120) Next(Stage && Stage->GetBombs() > Run->Chains0, FString::Printf(TEXT("bombs=%d hits=%d"), Stage ? Stage->GetBombs() - Run->Chains0 : 0, Stage ? Stage->GetBombHits() : 0));
            break;
        }
        case ET::Steps:
        {
            // From the ramp's top across the crumbling steps without stopping, to the chimney's foot.
            const int32 N = UE_ARRAY_COUNT(Steps);
            if (Run->Sub == -1) { Place(RampX1 - 140, 0, RampTop, 0); Run->Sub = 0; Run->Hop = 0; Chuck->SetRunHeld(true); }
            const FVector2D Goal = Run->Hop < N ? FVector2D(Steps[Run->Hop].X, Steps[Run->Hop].Y) : FVector2D(23820, 0);
            Push(FVector(Goal.X - P.X, Goal.Y - P.Y, 0));
            // Jump at the edge of what he is on: the ramp, or the step before.
            const float Edge = Run->Hop == 0 ? RampX1 : Steps[Run->Hop - 1].X + Steps[Run->Hop - 1].Half * .55f;
            if (Ground && P.X > Edge - 20 && Run->Sub == 0) { Chuck->JumpPressed(); Run->Sub = 1; Run->At = Now; }
            if (Run->Sub == 1 && Ground && Now - Run->At > .15f)
            {
                if (Run->Hop == 2) Shot(TEXT("Steps"));
                ++Run->Hop; Run->Sub = 0;
            }
            if (Run->Hop > N && Ground) Next(P.X > 23720 && FMath::Abs(P.Z - Stand - ChimneyBase) < 30, FString::Printf(TEXT("crumbled=%d"), Stage ? Stage->GetCrumbled() : 0));
            break;
        }
        case ET::Chimney:
        {
            // In the slot, facing a wall: jump into it, kick off each wall a quarter second into its run, catch the lip, up.
            const float Mid = (ChimneyX0 + ChimneyX1) * .5f;
            if (Run->Sub == -1) { Place(Mid, -ChimneyHalf + 26, ChimneyBase, -90); Run->Sub = 0; }
            else if (Run->Sub == 0 && T > .5f) { Shot(TEXT("ChimneyFoot")); Chuck->SetActorRotation(FRotator(0, -90, 0)); Chuck->SetTestStickWorld(FVector(0, -1, 0)); Chuck->JumpPressed(); Run->Sub = 1; Run->At = -1; }
            else if (Run->Sub == 1)
            {
                const bool Wall = Chuck->IsWallRunning();
                if (Wall && Run->At < 0) { Run->At = Now; Stop(); }
                if (!Wall) Run->At = -1;
                if (Wall && Run->At > 0 && Now - Run->At > .25f) { Chuck->JumpPressed(); Run->At = -1; }
                if (Chuck->IsHanging()) { Shot(TEXT("ChimneyLip")); Run->Sub = 2; Run->At = Now; }
                if (Ground && P.Z < ChimneyBase + 60 && Now - Run->TrialStart > 6) { Run->Sub = 0; Run->At = Now; Place(Mid, -ChimneyHalf + 26, ChimneyBase, -90); Run->Note += TEXT("retry "); }
            }
            else if (Run->Sub == 2 && Now - Run->At > .3f) { Chuck->JumpPressed(); Run->Sub = 3; Run->At = Now; }
            else if (Run->Sub == 3 && Ground && Now - Run->At > .5f) Next(FMath::Abs(P.Z - Stand - MesaTop) < 30, Run->Note);
            break;
        }
        case ET::Leap:
        {
            // Along the mesa's top at a sprint, the leap over the last gap onto the approach.
            if (Run->Sub == -1) { Place(MesaX1 - 650, 0, MesaTop, 0); Run->Sub = 0; Chuck->SetRunHeld(true); }
            Toward(P.X + 200, 0);
            if (Run->Sub == 0 && P.X > MesaX1 - 450) { if (Chuck->TrySprint()) Run->Sub = 1; }
            if (Run->Sub == 1 && Ground && P.X > MesaX1 - 24) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; Run->Note = FString::Printf(TEXT("sprinting=%d"), Chuck->IsSprinting()); }
            if (Run->Sub == 2 && P.X > MesaX1 + 60 && P.X < FortressX) Shot(TEXT("LastLeap"));
            if (Run->Sub == 2 && Ground && P.X > FortressX + 10) Next(FMath::Abs(P.Z - Stand - FortressTop) < 30, Run->Note);
            break;
        }
        case ET::Approach:
        {
            // (2026-11-16) The wastes: from the landing along the winding path to the forecourt at a run, never off it.
            if (Run->Sub == -1) { Place(25600, 0, FortressTop, 0); Run->Sub = 0; Run->Hop = 0; Chuck->SetRunHeld(true); break; }
            const float Look = P.X + 380;
            Toward(Look, ApproachPathY(Look));
            if (T > .3f) Run->Hop = FMath::Max(Run->Hop, FMath::RoundToInt(FMath::Abs(P.Y - ApproachPathY(P.X))));
            for (int32 K = 0; K < 3; ++K) if (Run->Sub == K && P.X > FortressX + 3500 + K * 4500) { Shot(FString::Printf(TEXT("Wastes_%d"), K)); ++Run->Sub; }
            if (Ground && P.X > ForecourtX + 300) Next(Run->Hop < 300, FString::Printf(TEXT("worst_off_path_cm=%d"), Run->Hop));
            break;
        }
        case ET::Gate:
        {
            // Up the processional to the shut gate: he goes no further.
            // (break on the frame he is placed: T is still the trial before's, which ran long after the wastes came)
            if (Run->Sub == -1) { Place(GateX - 1200, 0, FortressTop, 0); Run->Sub = 0; Chuck->SetRunHeld(true); break; }
            Toward(P.X + 200, 0);
            if (T > 1.5f && Run->Sub == 0) { Shot(TEXT("Processional")); Run->Sub = 1; }
            if (T > 10.f) Next(P.X < GateX - 150 && P.X > GateX - 400, TEXT(""));
            break;
        }
        default: Next(false, TEXT("unknown=1")); break;
        }
    }, 1.f / 60.f, true);
}
