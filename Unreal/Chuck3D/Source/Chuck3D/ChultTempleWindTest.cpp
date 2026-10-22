// -ChuckChultTest -ChuckTempleWindTest (Claude, 2026-10-22; docs/CHULT-TEMPLE-WIND.md): stills of the
// Astral wind and the sanctum, then the real character with the moves a player uses: wall runs kicked
// back and forth between a leg's walls over each spike pit, walks round the ruptures, the stair down
// into the sanctum. Negative trials show the long pits ask for the extra kick, leg 3's must start on its
// +S wall, and that a rupture is an Astral death back at its part's start.
// -ChuckWindMeasure adds the reach trials (0-3 kicks, early and late) that set the pits' lengths.
#include "ChultTempleWind.h"
#include "ChultTempleInterior.h"
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
    using namespace TempleWind;
    FVector W(float S, float T, float Z) { return ChultTempleInteriorWorldOf(FVector(S, T, Z)); }
    FVector Dir(float DS, float DT) { return (W(DS, DT, 0) - W(0, 0, 0)).GetSafeNormal(); }
    constexpr float Stand = 34.65f;
    float LegMid(int32 L) { return (Legs[L].S0 + Legs[L].S1) * .5f; }
    // A pit crossing: the wall line he runs up along (frame S), and how far into each side run to kick (cm of travel).
    struct FPitPlan { int32 Leg; float WallS; int32 Kicks; float KickAt; };
    FPitPlan Plan(int32 L)
    {
        // Leg 1: one kick, on the -S wall (a rupture lies by the +S wall before it). Leg 2 two kicks from the -S wall.
        // Leg 3 two kicks, started on the +S wall (its -S wall is broken back at the lip). Leg 4 from the +S wall.
        switch (L)
        {
        case 0: return {0, Legs[0].S0 + 32, 1, 120};
        case 1: return {1, Legs[1].S0 + 32, 2, 150};
        case 2: return {2, Legs[2].S1 - 32, 2, 150};
        default: return {3, Legs[3].S1 - 32, 3, 150};
        }
    }
}

void ReviewChultTempleWind(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    struct FView { const TCHAR* Name; FVector Eye, Aim; };
    const float FM = LegMid(0);
    const TArray<FView> Views = {
        {TEXT("WindArch"), FVector(FM, TempleRun::AnteT0 + 60, UZ + 150), FVector(FM, -2400, UZ + 80)},
        {TEXT("WindLeg1"), FVector(FM - 40, -3050, UZ + 120), FVector(FM, -2100, UZ - 10)},
        {TEXT("WindPit1"), FVector(FM + 50, -2350, UZ + 150), FVector(FM, -1500, UZ - 30)},
        {TEXT("WindBend1"), FVector(73700, -640, UZ + 140), FVector(75200, -720, UZ - 20)},
        {TEXT("WindTearClose"), FVector(73700, -800, UZ + 90), FVector(73890, -650, UZ - 10)},
        {TEXT("WindPit2"), FVector(LegMid(1) - 40, -940, UZ + 150), FVector(LegMid(1), -2300, UZ - 20)},
        {TEXT("WindBend2"), FVector(75520, -3220, UZ + 140), FVector(77100, -3130, UZ - 20)},
        {TEXT("WindTwist"), FVector(LegMid(2) + 50, -2900, UZ + 150), FVector(LegMid(2) - 30, -1800, UZ + 20)},
        {TEXT("WindBend3"), FVector(77220, -640, UZ + 140), FVector(78600, -720, UZ - 20)},
        {TEXT("WindPit4"), FVector(LegMid(3) - 40, -940, UZ + 150), FVector(LegMid(3), -2300, UZ - 20)},
        {TEXT("SanctumArch"), FVector(LegMid(3), -2950, UZ + 120), FVector(HallMid, HT1 - 900, HZ + 300)},
        {TEXT("SanctumGallery"), FVector(HallMid, HT1 - 80, UZ + 150), FVector(HallMid, DaisT1, HZ + 200)},
        {TEXT("SanctumFloor"), FVector(HallMid + 450, GalleryT - 200, HZ + 160), FVector(HallMid, DaisT1 - 500, HZ + 250)},
        {TEXT("SanctumStair"), FVector(HS0 + 500, StairFoot - 260, HZ + 120), FVector(HS0 + 80, GalleryT - 300, HZ + 380)},
        {TEXT("SanctumDais"), FVector(HallMid - 300, DaisT1 - 100, HZ + 260), FVector(HallMid, HT1 - 200, UZ + 100)},
        {TEXT("SanctumHigh"), FVector(HS0 + 200, HT1 - 150, UZ + 300), FVector(HS1 - 600, DaisT1 + 300, HZ)}};
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(74);
    Chuck->SetActorHiddenInGame(true);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->SetViewTarget(Camera); PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }
    Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum));

    enum class ET : uint8 { Reach, Pit1, Pit2, Pit2One, Pit3, Pit3Wrong, Pit4, Pit4Short, TearFall, Bends, Stairs, Full };
    static const ET Trials[] = {ET::Reach, ET::Pit1, ET::Pit2, ET::Pit2One, ET::Pit3, ET::Pit3Wrong, ET::Pit4, ET::Pit4Short, ET::TearFall, ET::Bends, ET::Stairs, ET::Full};
    static const TCHAR* Names[] = {TEXT("reach"), TEXT("pit1"), TEXT("pit2"), TEXT("pit2_one_kick"), TEXT("pit3"), TEXT("pit3_wrong_wall"), TEXT("pit4"), TEXT("pit4_short"), TEXT("tear_fall"), TEXT("bends"), TEXT("stairs"), TEXT("full")};
    struct FRun
    {
        int32 Phase = 0, Trial = -1, Sub = 0, Leg = 0, Failures = 0, Deaths = 0, Shots = 0, Kicks = 0, Chains = 0, SideRuns = 0, Point = 0, Probe = 0;
        double Next = 0, Start = 0, At = -1; float Takeoff = 0, Furthest = 0, Rise = 0;
        bool bStarted = false, bPass = false, bDone = false, bKicked = false;
        TArray<int32> Results; TArray<FVector2D> Points; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Camera = Camera;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run, Views]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        const double Now = World->GetTimeSeconds();
        // ---- The stills.
        const int32 I = FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleWindNoViews")) ? Views.Num() : Run->Phase / 2;
        if (I < Views.Num())
        {
            if (Now < Run->Next) return;
            Run->Next = Now + 1.4;
            if (Run->Phase % 2 == 0)
            {
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(ChultTempleInteriorWorldOf(Views[I].Eye), (ChultTempleInteriorWorldOf(Views[I].Aim) - ChultTempleInteriorWorldOf(Views[I].Eye)).Rotation());
                ++Run->Phase; return;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / (FString(Views[I].Name) + TEXT(".png")), true, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_VIEW name=%s"), Views[I].Name);
            ++Run->Phase; return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleWindViewsOnly"))) { Run->bDone = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_TEST views_only=1")); FPlatformMisc::RequestExitWithStatus(false, 0); return; }
        auto* M = Chuck->GetCharacterMovement();
        const FVector P = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        const bool Ground = M->IsMovingOnGround();
        auto Push = [&](const FVector& D) { Chuck->SetTestStickWorld(D); if (!Chuck->IsWallSideRunning()) Chuck->AddMovementInput(D, 1, true); };
        auto Steer = [&](float S, float T, float Within = 30.f) -> bool
        {
            if (FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(S, T)) < Within) return true;
            Push((W(S, T, P.Z) - Chuck->GetActorLocation()).GetSafeNormal2D()); return false;
        };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        auto Shot = [&](const TCHAR* Name) { if (Run->Shots < 30) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("WindPlay_%s.png"), Name), true, false); } };
        auto Reset = [&](float S, float T, float Z, float DS, float DT)
        {
            Chuck->ResetAtLocation(W(S, T, Z + Stand)); Chuck->SetActorRotation(Dir(DS, DT).Rotation());
            Chuck->SetRunHeld(false); Stop(); Chuck->Recenter();
        };
        // Follow Run->Points in turn. True once at the last.
        auto Follow = [&](float Within = 28.f) -> bool
        {
            while (Run->Point < Run->Points.Num() - 1 && FVector2D::Distance(FVector2D(P.X, P.Y), Run->Points[Run->Point]) < Within) ++Run->Point;
            if (Run->Points.IsEmpty()) return true;
            const FVector2D& G = Run->Points[Run->Point];
            return Steer(G.X, G.Y, Within) && Run->Point == Run->Points.Num() - 1;
        };
        // A pit along T crossed on its walls: run up along WallS, jump at the lip, and in each side run
        // kick across once KickAt cm along it, Kicks times. 1 once he stands beyond its far end, -1 on a death.
        auto PitRun = [&](int32 L, float WallS, int32 Kicks, float KickAt) -> int32
        {
            const float D = Legs[L].Dir, Lip = Pits[L].From, Far = Pits[L].To;
            const float Along = (P.Y - Lip) * D;
            if (Run->Sub == 0) { if (Now - Run->Start < .6) return 0; Chuck->SetRunHeld(true); Run->Sub = 1; Run->SideRuns = Chuck->GetWallSideRuns(); Run->Chains = Chuck->GetWallChains(); Run->Kicks = 0; Run->bKicked = false; Run->Furthest = -1e9f; Run->Rise = -1e9f; }
            if (Run->Sub == 1)
            {
                Push((Dir(0, D) + Dir(WallS - P.X, 0) * FMath::Clamp(FMath::Abs(WallS - P.X) / 40.f, 0.f, 1.f) * .5f).GetSafeNormal());
                if (Along > -22 && Ground) { Chuck->JumpPressed(); Run->Sub = 2; Run->At = Now; Run->Takeoff = P.Y; }
                return 0;
            }
            Push(Dir(0, D));
            if (Now >= Run->Next) { Run->Next = Now + .05; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_PIT_TRACE leg=%d gait=%s t=%.0f s=%.0f z=%.0f travel=%.0f kicks=%d runs=%d vel=%.0f"), L + 1, Chuck->GetGaitName(), P.Y, P.X, P.Z - Stand - UZ, Chuck->GetWallSideTravel(), Run->Kicks, Chuck->GetWallSideRuns() - Run->SideRuns, M->Velocity.Size2D()); }
            Run->Furthest = FMath::Max(Run->Furthest, (P.Y - Run->Takeoff) * D);
            Run->Rise = FMath::Max(Run->Rise, float(P.Z - Stand - UZ));
            if (Run->Sub == 2 && Chuck->IsWallSideRunning()) { Run->Sub = 3; Shot(*FString::Printf(TEXT("Leg%d_First"), L + 1)); }
            if (Chuck->GetWallSideRuns() > Run->SideRuns + Run->Probe * 0 && Chuck->IsWallSideRunning() && !Run->bKicked && Chuck->GetWallSideTravel() < 8)
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_CATCH leg=%d run=%d at_t=%.0f at_s=%.0f"), L + 1, Chuck->GetWallSideRuns() - Run->SideRuns, P.Y, P.X);
            if (Chuck->IsWallSideRunning())
            {
                if (!Run->bKicked && Run->Kicks < Kicks && Chuck->GetWallSideTravel() >= KickAt) { Chuck->JumpPressed(); ++Run->Kicks; Run->bKicked = true; }
            }
            else Run->bKicked = false;
            if (Chuck->GetFallDeaths() > Run->Deaths) return -1;
            if (Ground && (P.Y - Far) * D > 5 && Now - Run->At > .3f)
            {
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_PIT leg=%d lip=%.0f far=%.0f landed_t=%.0f from_takeoff=%.0f side_runs=%d chains=%d kicks=%d rise=%.0f"),
                    L + 1, Lip, Far, P.Y, (P.Y - Run->Takeoff) * D, Chuck->GetWallSideRuns() - Run->SideRuns, Chuck->GetWallChains() - Run->Chains, Run->Kicks, Run->Rise);
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
            const bool Measure = FParse::Param(FCommandLine::Get(), TEXT("ChuckWindMeasure"));
            while (Run->Trial < UE_ARRAY_COUNT(Trials) && ((FParse::Value(FCommandLine::Get(), TEXT("ChuckWindTrials="), Only) && !Only.Contains(Names[Run->Trial])) || (Trials[Run->Trial] == ET::Reach && !Measure)))
            { Run->Results.Add(1); ++Run->Trial; }
            if (Run->Trial >= UE_ARRAY_COUNT(Trials))
            {
                int32 Fails = Run->Failures; FString Line;
                for (int32 K = 0; K < Run->Results.Num(); ++K) { Fails += !Run->Results[K]; if (Trials[K] != ET::Reach) Line += FString::Printf(TEXT(" %s=%d"), Names[K], Run->Results[K]); }
                Run->bDone = true; Stop();
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_TEST failures=%d%s"), Fails, *Line);
                FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0);
                return;
            }
            if (Run->Camera.IsValid()) Run->Camera->Destroy();
            Chuck->SetActorHiddenInGame(false);
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
            Run->Sub = Run->Leg = Run->Point = 0; Run->bPass = false; Run->At = -1; Run->Points.Reset();
            Run->Deaths = Chuck->GetFallDeaths(); Chuck->SetSanity(AChuckCharacter::MaxSanity);
            const float Up = 250;   // run-up for the pit trials
            switch (Trials[Run->Trial])
            {
            case ET::Reach: Reset(Plan(3).WallS, Pits[3].From + Up + 100, UZ, 0, -1); break;
            case ET::Pit1: Reset(Plan(0).WallS, Pits[0].From - Up, UZ, 0, 1); break;
            case ET::Pit2: case ET::Pit2One: Reset(Plan(1).WallS, Pits[1].From + Up + 60, UZ, 0, -1); break;
            case ET::Pit3: Reset(Plan(2).WallS, Pits[2].From - Up - 60, UZ, 0, 1); break;
            case ET::Pit3Wrong: Reset(Legs[2].S0 + 32, Pits[2].From - Up - 60, UZ, 0, 1); break;
            case ET::Pit4: case ET::Pit4Short: Reset(Plan(3).WallS, Pits[3].From + Up + 100, UZ, 0, -1); break;
            case ET::TearFall: { const FVector S = ChultTempleInteriorFrameOf(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Wind2)); Reset(S.X, S.Y, UZ, 1, 0); break; }
            case ET::Bends:
            {
                const FVector S = ChultTempleInteriorFrameOf(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Wind2)); Reset(S.X, S.Y, UZ, 1, 0);
                ChultTempleWindPath(FVector2D(S.X, S.Y), FVector2D(LegMid(1), (Bends[0].T0 + Bends[0].T1) * .5f), 26, Run->Points);
                break;
            }
            case ET::Stairs: Reset(HallMid, HT1 - 220, UZ, 0, -1); Run->Points = {{HallMid, GalleryT + 80}, {HS0 + 80, GalleryT + 80}, {HS0 + 80, StairFoot - 60}, {77900, StairFoot - 60}, {77900, StairFoot + 65}, {HallMid, StairFoot + 65}, {HallMid, DaisT1 + 300}, {HallMid, DaisT1 - 200}}; break;
            case ET::Full:
            {
                Reset(LegMid(0), -3000, UZ, 0, 1);
                break;
            }
            }
            Chuck->SetOrbitPitch(-14);
            Run->Start = Now; Run->bStarted = true; return;
        }
        const float Time = Now - Run->Start;
        const int32 NewDeaths = Chuck->GetFallDeaths() - Run->Deaths;
        bool Done = false; float Limit = 20;
        switch (Trials[Run->Trial])
        {
        case ET::Reach:
        {
            // How far each plan carries him past the lip of the long pit (it falls in or lands beyond):
            // 0-3 kicks, kicked early (120 cm into each side run) or late (200 cm).
            static const FIntPoint Probes[] = {{0, 0}, {1, 120}, {1, 200}, {2, 120}, {2, 200}, {3, 120}, {3, 200}};
            Limit = 120;
            if (Run->Probe >= UE_ARRAY_COUNT(Probes)) { Done = true; Run->bPass = true; break; }
            if (Run->Leg == 0)
            {
                const int32 R = PitRun(3, Plan(3).WallS, Probes[Run->Probe].X, Probes[Run->Probe].Y);
                if (R != 0 || (Run->Sub >= 2 && Now - Run->At > 4))
                {
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_REACH kicks=%d kick_at=%d reach=%.0f rise=%.0f landed=%d chains=%d"),
                        Probes[Run->Probe].X, Probes[Run->Probe].Y, Run->Furthest, Run->Rise, R > 0, Chuck->GetWallChains() - Run->Chains);
                    Run->Leg = 1; Run->At = Now; Stop(); Chuck->SetRunHeld(false);
                }
            }
            else if (Now - Run->At > 1.5f)
            {
                ++Run->Probe; Run->Leg = 0; Run->Sub = 0; Run->Deaths = Chuck->GetFallDeaths();
                Reset(Plan(3).WallS, Pits[3].From + 350, UZ, 0, -1);
            }
            break;
        }
        case ET::Pit1: case ET::Pit2: case ET::Pit3: case ET::Pit4:
        {
            const int32 L = Trials[Run->Trial] == ET::Pit1 ? 0 : Trials[Run->Trial] == ET::Pit2 ? 1 : Trials[Run->Trial] == ET::Pit3 ? 2 : 3;
            const FPitPlan Pl = Plan(L);
            const int32 R = PitRun(L, Pl.WallS, Pl.Kicks, Pl.KickAt);
            if (R) { Done = true; Run->bPass = R > 0 && NewDeaths == 0 && Run->Kicks == Pl.Kicks && Chuck->GetWallChains() - Run->Chains >= Pl.Kicks; }
            break;
        }
        case ET::Pit2One: case ET::Pit4Short:
        {
            // One kick fewer than the pit asks: he falls in.
            const int32 L = Trials[Run->Trial] == ET::Pit2One ? 1 : 3;
            const FPitPlan Pl = Plan(L);
            const int32 R = PitRun(L, Pl.WallS, Pl.Kicks - 1, 200);
            if (R) { Done = true; Run->bPass = R < 0; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_SHORT leg=%d kicks=%d furthest=%.0f pit=%.0f"), L + 1, Pl.Kicks - 1, Run->Furthest, FMath::Abs(Pits[L].To - Pits[L].From)); }
            break;
        }
        case ET::Pit3Wrong:
        {
            // Leg 3 started on its -S wall: that wall is broken back at the lip, so no side run; he falls.
            const int32 R = PitRun(2, Legs[2].S0 + 32, 2, 150);
            if (R) { Done = true; Run->bPass = R < 0; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_WRONG_WALL side_runs=%d furthest=%.0f"), Chuck->GetWallSideRuns() - Run->SideRuns, Run->Furthest); }
            break;
        }
        case ET::TearFall:
        {
            // Walk straight into bend 1's first rupture: an Astral death, back at the bend's start.
            const FTempleWindTear* First = nullptr;
            for (const FTempleWindTear& T : ChultTempleWindTears()) if (T.Seg == 4) { First = &T; break; }
            if (!First) { Done = true; break; }
            if (Run->Sub == 0) { Steer(First->Mid.X, First->Mid.Y, 5); if (NewDeaths) { Run->Sub = 1; Run->At = Now; Stop(); } }
            else if (Now - Run->At > 1.5f)
            {
                const bool Home = FVector::Dist(Chuck->GetActorLocation(), ChultTempleInteriorSpawn(ETempleInteriorSpawn::Wind2)) < 40 && Ground && !Chuck->IsAstral();
                if (Home || Now - Run->At > 8) { Done = true; Run->bPass = Home && NewDeaths == 1; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_TEAR_FALL home=%d deaths=%d"), Home, NewDeaths); }
            }
            break;
        }
        case ET::Bends:
        {
            // Each bend at a walk from its start to the next leg's, round its ruptures.
            Limit = 90;
            Chuck->SetRunHeld(false);
            if (NewDeaths) { Done = true; break; }
            if (Follow())
            {
                if (Run->Leg == 0) Shot(TEXT("Bend"));
                ++Run->Leg;
                if (Run->Leg >= 3) { Done = true; Run->bPass = true; break; }
                const FVector S = ChultTempleInteriorFrameOf(ChultTempleInteriorSpawn(Run->Leg == 1 ? ETempleInteriorSpawn::Wind4 : ETempleInteriorSpawn::Wind6));
                Reset(S.X, S.Y, UZ, 1, 0); Run->Points.Reset(); Run->Point = 0;
                ChultTempleWindPath(FVector2D(S.X, S.Y), FVector2D(LegMid(Run->Leg + 1), (Bends[Run->Leg].T0 + Bends[Run->Leg].T1) * .5f), 26, Run->Points);
            }
            if (Done || Time > Limit - .1f) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_BENDS bends=%d deaths=%d seconds=%.1f"), Run->Leg, NewDeaths, Time);
            break;
        }
        case ET::Stairs:
        {
            // Off the gallery, down the -S stair, along the processional and up onto the dais.
            Limit = 90; Chuck->SetRunHeld(true);
            if (Follow(35))
            {
                Done = true; Stop();
                Run->bPass = NewDeaths == 0 && FMath::Abs(P.Z - Stand - (HZ + DaisH)) < 8;
                Shot(TEXT("Dais"));
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_STAIRS at=%s deaths=%d seconds=%.1f"), *P.ToString(), NewDeaths, Time);
            }
            if (NewDeaths) Done = true;
            break;
        }
        case ET::Full:
        {
            // From the antechamber's old choke to the sanctum's gallery: each leg's lane to its pit's run-up,
            // the pit on the walls, the lane on through the bend to the next leg.
            Limit = 150;
            const int32 L = Run->Leg / 2;
            if (L >= 4)
            {
                if (Run->Points.IsEmpty()) { Run->Points = {{LegMid(3), Legs[3].T0 + 60}, {HallMid, GalleryT + 100}}; Run->Point = 0; }
                Chuck->SetRunHeld(false);
                if (Follow(30)) { Done = true; Run->bPass = NewDeaths == 0; }
            }
            else if (Run->Leg % 2 == 0)
            {
                // To the run-up's start: 2.5 m before the lip, on the plan's wall line.
                if (Run->Points.IsEmpty())
                {
                    // On down the last leg past its ruptures, round the bend, along this leg.
                    const float D = Legs[L].Dir;
                    if (!ChultTempleWindPath(FVector2D(P.X, P.Y), FVector2D(Plan(L).WallS, Pits[L].From - D * 250), 26, Run->Points)) { Done = true; break; }
                    Run->Point = 0;
                }
                Chuck->SetRunHeld(false);
                if (Follow(25)) { ++Run->Leg; Run->Sub = 0; }
            }
            else
            {
                const FPitPlan Pl = Plan(L);
                const int32 R = PitRun(L, Pl.WallS, Pl.Kicks, Pl.KickAt);
                if (R < 0) Done = true;
                if (R > 0) { ++Run->Leg; Run->Points.Reset(); Chuck->SetRunHeld(false); Stop(); }
            }
            if (NewDeaths) Done = true;
            if (Done || Time > Limit - .1f) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_FULL stage=%d deaths=%d seconds=%.1f"), Run->Leg, NewDeaths, Time);
            break;
        }
        }
        if (!Done && Time < Limit) return;
        Stop(); Chuck->SetRunHeld(false);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_WIND_TRIAL name=%s pass=%d seconds=%.1f leg=%d sub=%d deaths=%d gait=%s at=%s"),
            Names[Run->Trial], Run->bPass, Time, Run->Leg, Run->Sub, NewDeaths, Chuck->GetGaitName(), *P.ToString());
        Run->Results.Add(Run->bPass ? 1 : 0); Run->bStarted = false; Run->Next = Now + 1.2;
    }, .016f, true);
}
