// -ChuckChultTest -ChuckPhlegethosTest (Claude, 2026-11-14; PhlegethosSetting.h): the real character through Phlegethos's
// challenges, each a trial placed at its start: the hills' path from the landing (2026-11-16), the road's hops and its sprint leap, the flameskulls and a fire snake (2026-11-16),
// a lemure's whole life, the spined devils' spines and their fatal scratch, the Pit Fiend noticing him and its blow (2026-11-16), every column hop
// of the lake (the wall run up the tall one), the chained wall run between the slabs, the gorge's side wall run and drop,
// the chains, the ramp, the crumbling steps, the chimney, the last leap, the shut gate; and stills of each stretch.
// -ChuckPhlegethosTest=<trial> runs one. -ChuckHellFallTest: the fall cutscene alone, its shots, then the handoff.
#include "PhlegethosSetting.h"
#include "PhlegethosFall.h"
#include "PhlegethosCreatures.h"
#include "TempleSnake.h"
#include "TempleSkeleton.h"
#include "TempleSanctumBattle.h"
#include "TempleHero.h"
#include "PhlegethosBleed.h"
#include "DockNPC.h"
#include "Components/CapsuleComponent.h"
#include "ChultTempleInterior.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
    // (FiendShow, after Count, only by name: -ChuckPhlegethosTest=fiend_show, the Pit Fiend's moves filmed whole for review)
    enum class ET : uint8 { Stills, Hills, RoadHops, RoadLeap, Skulls, FireSnakes, Lemures, Devils, Fiend, LakeHops, LakeSlabs, GorgeWall, GorgeDrop, Chains, Ramp, Steps, Chimney, Leap, Approach, Gate, Trio, BusStop, Bleed, Count, FiendShow };
    const TCHAR* Names[] = {TEXT("stills"), TEXT("hills"), TEXT("road_hops"), TEXT("road_leap"), TEXT("skulls"), TEXT("fire_snakes"), TEXT("lemures"), TEXT("devils"), TEXT("fiend"), TEXT("lake_hops"), TEXT("lake_slabs"), TEXT("gorge_wall"), TEXT("gorge_drop"), TEXT("chains"), TEXT("ramp"), TEXT("steps"), TEXT("chimney"), TEXT("leap"), TEXT("approach"), TEXT("gate"), TEXT("trio"), TEXT("bus_stop"), TEXT("bleed"), TEXT("-"), TEXT("fiend_show")};
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
                const bool bPass = S && S->IsLive() && FVector::Dist(Chuck->GetActorLocation(), PhlegethosSpawn(EPhlegSpawn::Landing)) < 60 && S->GetMissingMaterials() == 0;
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
    if (!Only.IsEmpty()) for (int32 I = 0; I <= int32(ET::FiendShow); ++I) if (Only == Names[I]) Run->Trial = I;
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
        auto Shot = [&](const FString& Name) { if (Run->Shots < 90) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Phlegethos") / (Name + TEXT(".png")), true, false); } };
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
        if (Run->Trial == int32(ET::Count))
        {
            Run->bDone = true;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEGETHOS_TEST failures=%d passed=%d trials=%s sunk=%d crumbled=%d splashes=%d seconds=%.0f"), Run->Failures, Run->Passed,
                *FString::Join(Run->Results, TEXT(" ")), Stage ? Stage->GetSunk() : 0, Stage ? Stage->GetCrumbled() : 0, Stage ? Stage->GetSplashes() : 0, Now - Run->Start);
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
            return;
        }
        if (!Run->bPlaced) { Run->bPlaced = true; Run->TrialStart = Now; Run->At = Now; Run->Sub = -1; }
        // (The flameskulls burn only in their own trial: the traversal trials are of the moves, and do not dodge.)
        APhlegFlameskull::bCalm = Run->Trial != int32(ET::Skulls) && Run->Trial != int32(ET::FireSnakes) && Run->Trial != int32(ET::Lemures) && Run->Trial != int32(ET::Devils) && Run->Trial != int32(ET::Fiend) && Run->Trial != int32(ET::FiendShow) && Run->Trial != int32(ET::Trio) && Run->Trial != int32(ET::Bleed);
        // (The Feywild bleedthrough: its own trial; the trio's watches the battle undisturbed. 2026-11-17)
        APhlegBleed::bHold = Run->Trial != int32(ET::Bleed);
        // A death ends any trial but the stills.
        if (Run->Trial != int32(ET::Stills) && Run->Trial != int32(ET::LakeHops) && Run->Trial != int32(ET::BusStop) && Run->Sub >= 0 && Deaths > 0 && Now - Run->At > .3f) { Next(false, FString::Printf(TEXT("died=1 sub=%d hop=%d %s"), Run->Sub, Run->Hop, *Run->Note)); return; }
        // (The wastes' walk is 155 m: about 70 s at his run; the stills are 13 spawns, 4 s each.)
        const bool bLong = Run->Trial == int32(ET::Approach) || Run->Trial == int32(ET::Stills) || Run->Trial == int32(ET::Hills) || Run->Trial == int32(ET::Trio) || Run->Trial == int32(ET::Bleed);
        if (Now - Run->TrialStart > (bLong ? 130.f : 75.f)) { Next(false, FString::Printf(TEXT("timeout=1 sub=%d hop=%d gait=%s"), Run->Sub, Run->Hop, Chuck->GetGaitName())); return; }
        switch (ET(Run->Trial))
        {
        case ET::Stills:
        {
            // Each stretch's start, looking on along the way (rat height, then raised).
            const int32 K = Run->Hop;
            // (Then a lava fall's foot from the gorge's shelf, across the river: four shots, so the spray shows in motion.
            // The geysers' shots were here until 2026-11-16.)
            if (K >= PhlegSpawns)
            {
                if (Run->Sub == -1) { Place(GorgeX + 1100, -95, G, 90); Chuck->SetOrbitPitch(-6); Run->Sub = 10; }
                else if (Run->Sub >= 10 && Run->Sub < 14 && T > 1.5f + .6f * (Run->Sub - 10)) { Shot(FString::Printf(TEXT("FallSplash_%d"), Run->Sub - 10)); ++Run->Sub; }
                else if (Run->Sub >= 14) Next(Stage && Stage->GetSplashes() > 0, FString::Printf(TEXT("stills=%d splashes=%d"), PhlegSpawns * 2 + 4, Stage ? Stage->GetSplashes() : 0));
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
        case ET::Hills:
        {
            // (2026-11-16) From the landing along the hills' winding path over the rises to the plain at a run, never off it.
            if (Run->Sub == -1) { Place(HillsLandX, 0, G, 0); Run->Sub = 0; Run->Hop = 0; Chuck->SetRunHeld(true); break; }
            const float Look = P.X + 380;
            Toward(Look, Look < HillsJoinX ? HillsPathY(Look) : 0.f);
            if (T > .3f && P.X > HillsLandX + 1300 && P.X < HillsJoinX) Run->Hop = FMath::Max(Run->Hop, FMath::RoundToInt(FMath::Abs(P.Y - HillsPathY(P.X))));
            for (int32 K = 0; K < 3; ++K) if (Run->Sub == K && P.X > HillsLandX + 600 + K * 3000) { Shot(FString::Printf(TEXT("Hills_%d"), K)); ++Run->Sub; }
            if (Ground && P.X > HillsJoinX + 600) Next(Run->Hop < 300 && P.Z - Stand > G - 80, FString::Printf(TEXT("worst_off_path_cm=%d"), Run->Hop));
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
            // Along the last long segment at a run, then a sprint at the long break and the leap.
            if (Run->Sub == -1) { Place(Spine[3].X0 + 300, SpineMid(Spine[3].X0 + 300), G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); }
            Toward(P.X + 200, SpineMid(P.X + 200));
            if (Run->Sub == 0 && P.X > Spine[3].X1 - 420) { if (Chuck->TrySprint()) Run->Sub = 1; }
            if (Run->Sub == 1 && Ground && P.X > Spine[3].X1 - 24) { Chuck->JumpPressed(); Run->Sub = 2; Run->Note = FString::Printf(TEXT("sprinting=%d"), Chuck->IsSprinting()); }
            if (Run->Sub == 2 && Now - Run->At > 0 && P.X > Spine[3].X1 + 40 && P.X < Spine[4].X0) Shot(TEXT("RoadLeap"));
            if (Run->Sub == 2 && Ground && P.X > Spine[4].X0 + 10) Next(true, Run->Note);
            break;
        }
        case ET::Skulls:
        {
            // (2026-11-16) The flameskulls: the road's four and the ramp's three weaving; then he stands in one's lane on the
            // spine. Pass: it burns him, he is thrown back along the spine and not off it, and no death.
            if (Run->Sub == -1)
            {
                Place(4150, SpineMid(4150), G, 0); Chuck->SetOrbitPitch(-10); Run->Sub = 0;
                Run->Chains0 = APhlegFlameskull::Burns; Run->Hop = PhlegethosFlameskullCount(true);
                break;
            }
            if (Run->Sub == 0 && T > .8f) { Shot(TEXT("Skulls_Road")); Run->Sub = 1; }
            if (Run->Sub == 1 && T > 1.4f) { Shot(TEXT("Skulls_Road2")); Place(4250 - 20, SpineMid(4250), G, 0); Run->Sub = 2; }
            if (Run->Sub == 2 && T > 8.f)
            {
                const int32 Burned = APhlegFlameskull::Burns - Run->Chains0;
                Next(Burned > 0 && Deaths == 0 && P.Z - Stand > G - 20 && Run->Hop == 7 && PhlegethosFlameskullCount(false) > Run->Hop,
                    FString::Printf(TEXT("burns=%d skulls=%d harmful=%d"), Burned, PhlegethosFlameskullCount(false), Run->Hop));
            }
            break;
        }
        case ET::FireSnakes:
        {
            // (2026-11-16) A fire snake in the hills: he stands near it; it sees him, comes, rears and strikes; then he
            // turns on it and scratches. Pass: it struck, and one scratch killed it.
            ATempleSnake* K = nullptr; int32 Fire = 0;
            for (const auto& E : ATempleSnake::All()) if (E.IsValid() && E->GetPost().bFire) { if (!K) K = E.Get(); ++Fire; }
            if (!K) { Next(false, TEXT("no_fire_snakes=1")); break; }
            if (Run->Sub == -1)
            {
                K->ResetToPost();
                const FVector F = PhlegLocal(K->GetPost().Feet);
                const FVector2D In = FVector2D(F.X - HillsLandX, F.Y).GetSafeNormal();   // from the clearing's side
                Place(F.X - In.X * 170, F.Y - In.Y * 170, F.Z, FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X))); Chuck->SetOrbitPitch(-14);
                Run->Sub = 0; Run->Chains0 = K->GetStrikes(); Run->SideRuns0 = K->GetBites(); Run->Hop = 0;
                break;
            }
            if (Run->Sub == 0 && K->GetState() == ATempleSnake::EState::Rear && Run->Shots < 60 && !Run->Note.Contains(TEXT("shot"))) { Shot(TEXT("FireSnake_Rear")); Run->Note += TEXT("shot "); }
            if (Run->Sub == 0 && K->GetStrikes() > Run->Chains0 && K->GetState() == ATempleSnake::EState::Recoil) { Run->Sub = 1; Run->At = Now; }
            if (Run->Sub == 0 && T > 14.f) { Next(false, FString::Printf(TEXT("no_strike=1 state=%s fire=%d"), K->GetStateName(), Fire)); break; }
            if (Run->Sub == 1)
            {
                // Face its head and scratch, a few times if need be.
                const FVector To = K->SnoutTip() - Chuck->GetActorLocation();
                Chuck->SetActorRotation(FRotator(0, To.Rotation().Yaw, 0));
                if (!K->IsDead() && Now >= Run->Next && M->IsMovingOnGround()) { Chuck->Slash(); Chuck->SlashReleased(); Run->Next = Now + .7; ++Run->Hop; }
                if (K->IsDead() && T > 2.f) { Shot(TEXT("FireSnake_Dead")); Next(true, FString::Printf(TEXT("strikes=%d bites=%d scratches=%d fire_snakes=%d"), K->GetStrikes() - Run->Chains0, K->GetBites() - Run->SideRuns0, Run->Hop, Fire)); }
                else if (T > 6.f) Next(false, FString::Printf(TEXT("not_killed=1 scratches=%d state=%s"), Run->Hop, K->GetStateName()));
            }
            break;
        }
        case ET::Lemures:
        {
            // (2026-11-16) A hills lemure: he stands before it; it sees him, shambles to him and - its first attack - picks him
            // up by the leg and throws him (the user: "same as the skeletons"); he goes back to it and scratches it down (his
            // sanity kept up: this is the lemure's life, not his) and it melts where it falls. Pass: it threw him, it took
            // its seventeen scratches and fell, and it melted.
            ATempleSkeleton* L = nullptr; int32 Count = 0;
            for (const auto& E : ATempleSkeleton::All()) if (E.IsValid() && E->IsLemure() && FCString::Strcmp(E->GetPost().Space, TEXT("fortress")) != 0) { ++Count; if (!L && !E->GetPost().bDormant) L = E.Get(); }
            if (!L) { Next(false, TEXT("no_lemures=1")); break; }
            if (Run->Sub == -1)
            {
                L->ResetToPost();
                const FVector F = PhlegLocal(L->GetPost().Feet);
                const FVector Fw = FRotator(0, L->GetPost().Yaw, 0).Vector();
                Place(F.X + Fw.X * 280, F.Y + Fw.Y * 280, F.Z, L->GetPost().Yaw + 180.f); Chuck->SetOrbitPitch(-12);
                Run->Sub = 0; Run->Chains0 = L->GetStrikes(); Run->Hop = 0; Run->Next = Now; Run->SideRuns0 = L->GetThrows();
                L->ForceAttack(ATempleSkeleton::EAttack::Grab);
                break;
            }
            Chuck->SetSanity(5);
            const FVector To = L->GetActorLocation() - Chuck->GetActorLocation();
            if (Run->Sub == 0 && T > 1.2f && !Run->Note.Contains(TEXT("near"))) { Shot(TEXT("Lemure_Near")); Run->Note += TEXT("near "); }
            if (Run->Sub == 0 && L->GetState() == ATempleSkeleton::EState::Attack && !Run->Note.Contains(TEXT("attack"))) { Shot(TEXT("Lemure_Attack")); Run->Note += TEXT("attack "); }
            if (Chuck->IsLegHeld() && !Run->Note.Contains(TEXT("held"))) { Run->Note += TEXT("held "); Run->At = Now; }
            if (Run->Note.Contains(TEXT("held")) && !Run->Note.Contains(TEXT("hung")) && Now - Run->At > .35) { Shot(TEXT("Lemure_Grab")); Run->Note += TEXT("hung "); }
            if (Chuck->IsThrown() && !Run->Note.Contains(TEXT("flying"))) { Shot(TEXT("Lemure_Throw")); Run->Note += TEXT("flying "); }
            if (Run->Sub == 0 && L->GetStrikes() > Run->Chains0) { Run->Sub = 1; Run->At = Now; }
            if (Run->Sub == 0 && T > 16.f) { Next(false, FString::Printf(TEXT("no_strike=1 state=%s notices=%d"), L->GetStateName(), L->GetNotices())); break; }
            if (Run->Sub == 1)
            {
                // At it: face it, close in when knocked off, scratch.
                Chuck->SetActorRotation(FRotator(0, To.Rotation().Yaw, 0));
                if (!L->IsDead() && To.Size2D() > 75.f) Push(To); else Stop();
                if (!L->IsDead() && Now >= Run->Next && M->IsMovingOnGround() && To.Size2D() < 95.f) { Chuck->Slash(); Chuck->SlashReleased(); Run->Next = Now + .5; }
                if (L->IsDead() && Run->Sub == 1) { Run->Sub = 2; Run->At = Now; Stop(); break; }
                else if (T > 40.f) Next(false, FString::Printf(TEXT("not_killed=1 hits=%d state=%s"), L->GetHitsTaken(), L->GetStateName()));
            }
            if (Run->Sub == 2 && T > 1.2f && !Run->Note.Contains(TEXT("down"))) { Shot(TEXT("Lemure_Down")); Run->Note += TEXT("down "); }
            if (Run->Sub == 2 && T > 6.f)
            {
                Shot(TEXT("Lemure_Melted"));
                const int32 Throws = L->GetThrows() - Run->SideRuns0;
                Next(L->GetHitsTaken() >= ATempleSkeleton::LemureHealth && L->GetMelt() > 2.f && Count == 8 && Throws > 0,
                    FString::Printf(TEXT("strikes=%d throws=%d hits=%d melt=%.1f lemures=%d top_speed=%.0f %s"), L->GetStrikes() - Run->Chains0, Throws, L->GetHitsTaken(), L->GetMelt(), Count, L->GetTopSpeed(), *Run->Note));
            }
            break;
        }
        case ET::Devils:
        {
            // (2026-11-16) The spined devils. He stands in the gorge shelf's first lane: its spines cross the shelf on their
            // beat and burn him. Then beside a wastes devil's column he scratches it: it is fatal. Pass: spines thrown, one
            // burned him, and the scratch took all his sanity.
            const auto& Ds = APhlegSpinedDevil::All();
            if (Ds.Num() < 6) { Next(false, FString::Printf(TEXT("devils=%d"), Ds.Num())); break; }
            APhlegSpinedDevil* Gorge = Ds[0].Get(); APhlegSpinedDevil* Waste = Ds[3].Get();
            if (Run->Sub == -1)
            {
                const FVector A = PhlegLocal(Gorge->GetPerch().Aim);
                Place(A.X + 15, -95, G, 90); Chuck->SetOrbitPitch(-8);
                Run->Sub = 0; Run->Chains0 = APhlegSpinedDevil::Thrown; Run->SideRuns0 = APhlegSpinedDevil::Hits; Run->Hop = 0;
                break;
            }
            if (Run->Sub == 0)
            {
                Chuck->SetSanity(5);
                for (int32 K = 0; K < 3; ++K) if (Run->Hop == K && T > 1.f + K * .55f) { Shot(FString::Printf(TEXT("Devil_Gorge_%d"), K)); ++Run->Hop; }
                if (T > 6.5f)
                {
                    Run->Note = FString::Printf(TEXT("thrown=%d hits=%d"), APhlegSpinedDevil::Thrown - Run->Chains0, APhlegSpinedDevil::Hits - Run->SideRuns0);
                    // Beside the wastes devil's column, facing it.
                    const FVector S = PhlegLocal(Waste->GetPerch().Seat), Aim = PhlegLocal(Waste->GetPerch().Aim);
                    const FVector2D Off = FVector2D(Aim.X - S.X, Aim.Y - S.Y).GetSafeNormal();
                    FHitResult Hit; const FVector Top = PhlegWorld(FVector(S.X + Off.X * 55, S.Y + Off.Y * 55, S.Z + 300));
                    float Gz = S.Z - 70;
                    if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 900), ECC_Visibility)) Gz = static_cast<float>(PhlegLocal(Hit.ImpactPoint).Z);
                    Place(S.X + Off.X * 55, S.Y + Off.Y * 55, Gz, FMath::RadiansToDegrees(FMath::Atan2(-Off.Y, -Off.X)));
                    Chuck->SetOrbitPitch(-14);
                    Run->Sub = 1; Run->Hop = APhlegSpinedDevil::Scratched;
                }
                break;
            }
            if (Run->Sub == 1 && T > 1.f && T < 1.2f) Shot(TEXT("Devil_Wastes"));
            if (Run->Sub == 1 && T > 1.5f && Run->Next < Now - 5) { Chuck->Slash(); Chuck->SlashReleased(); Run->Next = Now; }
            if (Run->Sub == 1 && T > 2.4f)
            {
                const int32 Scr = APhlegSpinedDevil::Scratched - Run->Hop;
                const bool bThrown = APhlegSpinedDevil::Thrown - Run->Chains0 > 1, bHit = APhlegSpinedDevil::Hits - Run->SideRuns0 > 0;
                Next(bThrown && bHit && Scr > 0 && (Chuck->GetSanity() == 0 || Chuck->IsAstral()), FString::Printf(TEXT("%s scratched=%d sanity=%d"), *Run->Note, Scr, Chuck->GetSanity()));
            }
            break;
        }
        case ET::Fiend:
        {
            // (2026-11-16) The Pit Fiend, on its round in the wastes: he stands 15 m before it. Pass: it sees him, roars,
            // strides to him and one blow takes all his sanity; a scratch did nothing to it.
            ATempleSkeleton* F = PhlegethosPitFiend();
            if (!F) { Next(false, TEXT("no_fiend=1")); break; }
            if (Run->Sub == -1)
            {
                // (he on the path, it turned to look his way)
                F->ResetToPost();
                const FVector Fl = PhlegLocal(F->GetPost().Feet);
                FVector At(Fl.X + 900.f, 0, 0); At.Y = ApproachPathY(At.X);
                FHitResult Hit; const FVector Top = PhlegWorld(FVector(At.X, At.Y, FortressTop + 600));
                float Gz = FortressTop;
                if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 1600), ECC_Visibility)) Gz = static_cast<float>(PhlegLocal(Hit.ImpactPoint).Z);
                const FVector ToHim = PhlegWorld(FVector(At.X, At.Y, Gz)) - F->GetActorLocation();
                F->SetActorRotation(FRotator(0, ToHim.Rotation().Yaw, 0));
                Place(At.X, At.Y, Gz, ToHim.Rotation().Yaw + 180.f); Chuck->SetOrbitPitch(4);
                Run->Note = FString::Printf(TEXT("start_dist=%.0f fiend_z=%.0f "), ToHim.Size2D(), Fl.Z);
                Run->Sub = 0; Run->Hop = F->GetNotices(); Run->Chains0 = 0;
                break;
            }
            const float Dist = static_cast<float>(FVector::Dist2D(F->GetActorLocation(), Chuck->GetActorLocation()));
            if (Run->Sub == 0 && T > .6f && !Run->Note.Contains(TEXT("seen"))) { Shot(TEXT("Fiend_Far")); Run->Note += TEXT("seen "); }
            if (Run->Sub == 0 && F->GetNotices() > Run->Hop) { Run->Sub = 1; Run->At = Now; Run->Note += FString::Printf(TEXT("noticed_at=%.0f "), Dist); }
            if (Run->Sub == 0 && T > 6.f) { Next(false, FString::Printf(TEXT("not_noticed=1 state=%s dist=%.0f"), F->GetStateName(), Dist)); break; }
            if (Run->Sub == 1 && T > .5f && !Run->Note.Contains(TEXT("roar"))) { Shot(TEXT("Fiend_Roar")); Run->Note += TEXT("roar "); }
            if (Run->Sub == 1 && Dist < 700.f && !Run->Note.Contains(TEXT("close"))) { Shot(TEXT("Fiend_Close")); Run->Note += TEXT("close "); Chuck->Slash(); Chuck->SlashReleased(); }
            if (Run->Sub == 1 && F->GetState() == ATempleSkeleton::EState::Attack && !Run->Note.Contains(TEXT("blow"))) { Run->Note += TEXT("blow "); Run->Next = Now; }
            if (Run->Sub == 1 && Run->Note.Contains(TEXT("blow")) && Now - Run->Next > .5 && !Run->Note.Contains(TEXT("strike"))) { Shot(TEXT("Fiend_Strike")); Run->Note += TEXT("strike "); }
            // (2026-11-16 third pass) The roar's spread wings, the mace at the top of its swing and in the ground.
            if (Run->Sub == 1 && F->IsRoaring() && F->GetStateTime() > 1.f && !Run->Note.Contains(TEXT("spread"))) { Shot(TEXT("Fiend_RoarWings")); Run->Note += TEXT("spread "); }
            if (Run->Sub == 1 && F->GetState() == ATempleSkeleton::EState::Attack && F->GetLastAttack() == ATempleSkeleton::EAttack::Smash)
            {
                if (F->GetStateTime() > 1.f && !Run->Note.Contains(TEXT("windup"))) { Shot(TEXT("Fiend_Windup")); Run->Note += TEXT("windup "); }
                if (F->GetStateTime() > 1.38f && !Run->Note.Contains(TEXT("impact"))) { Shot(TEXT("Fiend_Impact")); Run->Note += TEXT("impact "); }
            }
            if (Run->Sub == 1 && Run->Note.Contains(TEXT("roar")) && !Run->Note.Contains(TEXT("walking")) && F->GetState() == ATempleSkeleton::EState::Chase && !F->IsRoaring() && Dist < 1300.f) { Shot(TEXT("Fiend_Charge")); Run->Note += TEXT("walking "); }
            if (Run->Sub == 1 && (Chuck->GetSanity() == 0 || Chuck->IsAstral()))
            {
                // (and its body: the roar's two wing beats, its feet coming down, the planted feet on the ground, the tail kept
                // out of it)
                Next(F->GetHitsTaken() == 0 && F->GetWingBeats() >= 2 && F->GetFootfalls() >= 4,
                    FString::Printf(TEXT("%skilled=1 attack=%s top_speed=%.0f t=%.1f beats=%d footfalls=%d smashes=%d blasts=%d foot_gap=%.0f tail_under=%.0f"), *Run->Note,
                        ATempleSkeleton::AttackName(F->GetLastAttack()), F->GetTopSpeed(), T, F->GetWingBeats(), F->GetFootfalls(), F->GetSmashes(), F->GetSmashBlasts(), F->GetWorstFootGap(), F->GetTailUnder()));
                break;
            }
            if (Run->Sub == 1 && T > 40.f) Next(false, FString::Printf(TEXT("%snot_killed=1 state=%s dist=%.0f gap=%.0f"), *Run->Note, F->GetStateName(), Dist, F->GetLastStrikeGap()));
            break;
        }
        case ET::FiendShow:
        {
            // (2026-11-16, review only) The Pit Fiend filmed whole from 18 m off its side: on its round (folded wings, its
            // stride, its tail), its roar (reared, wings spread and beating), coming on, and its mace brought down on the
            // ground before it. Pass: it struck the ground and its wings beat.
            ATempleSkeleton* F = PhlegethosPitFiend();
            if (!F) { Next(false, TEXT("no_fiend=1")); break; }
            Chuck->SetSanity(5);
            if (Run->Sub == -1)
            {
                // (on the level forecourt before the gate, open ground, walking back toward its round; he behind it, out of
                // its sight)
                F->ResetToPost();
                const FVector ShowAt = PhlegWorld(FVector(41700, 0, FortressTop));
                F->SetActorLocation(ShowAt + FVector(0, 0, F->HalfHeightNow() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
                F->SetActorRotation(FRotator(0, (PhlegWorld(FVector(39000, 0, FortressTop)) - ShowAt).Rotation().Yaw, 0));
                Place(43000, 0, FortressTop, 180.f); Chuck->SetOrbitPitch(10);
                Run->Sub = 0; Run->Hop = 0;
                break;
            }
            {
                // A camera of its own off the fiend's right, at its waist's height, 14 m out, the whole of it in frame.
                static TWeakObjectPtr<ACameraActor> Cam;
                if (!Cam.IsValid())
                {
                    Cam = World->SpawnActor<ACameraActor>();
                    if (Cam.IsValid()) { Cam->GetCameraComponent()->SetFieldOfView(50.f); Cam->GetCameraComponent()->bConstrainAspectRatio = false; }
                    if (APlayerController* PC = World->GetFirstPlayerController(); PC && Cam.IsValid()) PC->SetViewTarget(Cam.Get());
                }
                if (Cam.IsValid())
                {
                    const FVector Fw = F->GetActorForwardVector(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
                    const FVector Look = F->GetActorLocation() + FVector(0, 0, 40.f) + Fw * 120.f;
                    // (raised over the rock between, not pulled in: a smoothed height so the view does not jump)
                    static float Lift = 250.f;
                    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(FiendShowCam), false, F); Q.AddIgnoredActor(Chuck);
                    float Want = 250.f;
                    while (Want < 2400.f && World->LineTraceSingleByChannel(Hit, Look, Look + (Rt * .93f + Fw * .37f).GetSafeNormal() * 1150.f + FVector(0, 0, Want), ECC_Visibility, Q)) Want += 200.f;
                    Lift = FMath::FInterpTo(Lift, Want, World->GetDeltaSeconds(), 2.f);
                    const FVector Eye = Look + (Rt * .93f + Fw * .37f).GetSafeNormal() * 1150.f + FVector(0, 0, Lift);
                    Cam->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
                }
            }
            if (Run->Sub == 0)
            {
                const float Ts[] = {1.2f, 1.6f, 2.0f, 2.4f, 2.8f};
                if (Run->Hop < 5 && T > Ts[Run->Hop]) { Shot(FString::Printf(TEXT("Show_Walk_%d"), Run->Hop)); ++Run->Hop; }
                if (T > 3.2f) { F->ShowRoar(); Run->Sub = 1; Run->Hop = 0; Run->At = Now; }
                break;
            }
            if (Run->Sub == 1)
            {
                const float Ts[] = {.3f, .8f, 1.2f, 1.6f, 2.0f, 2.4f, 3.2f, 3.7f, 4.2f};
                if (Run->Hop < 9 && T > Ts[Run->Hop]) { Shot(FString::Printf(TEXT("Show_%s_%d"), Run->Hop < 6 ? TEXT("Roar") : TEXT("Charge"), Run->Hop)); ++Run->Hop; }
                if (T > 4.5f)
                {
                    const FVector Ahead = F->GetActorLocation() + F->GetActorForwardVector() * 220.f;
                    FHitResult Hit; float Gz = F->GetActorLocation().Z - F->HalfHeightNow();
                    if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0, 0, 400), Ahead - FVector(0, 0, 900), ECC_Visibility)) Gz = static_cast<float>(Hit.ImpactPoint.Z);
                    F->ShowSmash(FVector(Ahead.X, Ahead.Y, Gz));
                    Run->Sub = 2; Run->Hop = 0; Run->At = Now;
                }
                break;
            }
            if (Run->Sub == 2)
            {
                const float Ts[] = {.45f, .9f, 1.12f, 1.24f, 1.32f, 1.4f, 1.6f, 2.1f, 2.6f};
                if (Run->Hop < 9 && T > Ts[Run->Hop]) { Shot(FString::Printf(TEXT("Show_Smash_%d"), Run->Hop)); ++Run->Hop; }
                if (T > 3.2f)
                {
                    Next(F->GetSmashes() > 0 && F->GetWingBeats() >= 2 && F->GetFootfalls() >= 3, FString::Printf(TEXT("smashes=%d beats=%d footfalls=%d foot_gap=%.0f tail_under=%.0f gap=%.0f"),
                        F->GetSmashes(), F->GetWingBeats(), F->GetFootfalls(), F->GetWorstFootGap(), F->GetTailUnder(), F->GetLastStrikeGap()));
                    F->ResetToPost();
                }
            }
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
        case ET::Ramp:
        {
            // Up the ramp at a run (its lava bombs gone, 2026-11-16). Pass: at the top.
            if (Run->Sub == -1) { Place(RampX0 + 50, 0, G, 0); Run->Sub = 0; Chuck->SetRunHeld(true); }
            Toward(P.X + 200, 0);
            if (P.X > RampX0 + 800 && Run->Sub == 0) { Shot(TEXT("Ramp")); Run->Sub = 1; }
            if (Ground && P.X > RampX1 - 120) Next(true, FString::Printf(TEXT("x=%.0f"), P.X));
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
            // (2026-11-17: round the bus stop's small ruptures on the path, the far side from where each sits off it)
            float Aside = 0.f;
            for (int32 Ri = 0; Ri < RiftCount; ++Ri)
            {
                const FVector2D Rc = RiftCentre(Ri);
                const float Off = static_cast<float>(Rc.Y) - ApproachPathY(static_cast<float>(Rc.X));
                if (FMath::Abs(Off) < 300.f && P.X > Rc.X - 520.f && P.X < Rc.X + 200.f) Aside = (Off >= 0.f ? -1.f : 1.f) * (Rifts[Ri].R * 1.4f + 75.f - FMath::Abs(Off));
            }
            Toward(Look, ApproachPathY(Look) + Aside);
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
        case ET::Trio:
        {
            // (2026-11-16) The trio's battle at the fortress: he walks out onto the forecourt; the entrance plays (the three
            // lines over the battle), then he stands at the forecourt's edge while it goes on. Pass: the entrance once with
            // its three lines, then arrows and bolts on the fiend, its mace at the fighter and its fire at the others, a
            // lemure put down, and the fiend never in the breach.
            ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(World);
            if (!B) { Next(false, TEXT("no_battle=1")); break; }
            if (Run->Sub == -1) { Place(ForecourtX - 600, 250, FortressTop, 20); Run->Sub = 0; break; }
            if (Run->Sub == 0)
            {
                // (its assets stream in as he crosses the wastes)
                if (!B->IsReady() || !B->GetFiend()) { if (T > 40.f) Next(false, TEXT("not_ready=1")); break; }
                Run->Sub = 1; Run->At = Now; Run->Chains0 = B->GetFortressEntrances();
                break;
            }
            ATempleSkeleton* F = B->GetFiend();
            const float S = static_cast<float>(Now - Run->At);
            if (Run->Sub == 1)
            {
                Toward(ForecourtX + 500, 950);   // (out onto the open ground right of the processional)
                if (B->IsFortressScenePlaying()) { Stop(); Run->Sub = 2; Run->At = Now; }
                else if (S > 15.f) Next(false, TEXT("no_entrance=1"));
                break;
            }
            if (Run->Sub == 2)
            {
                if (S > 1.6f && !Run->Note.Contains(TEXT("wide"))) { Shot(TEXT("Trio_Entrance_Wide")); Run->Note += TEXT("wide "); }
                if (S > 3.2f && !Run->Note.Contains(TEXT("smash"))) { Shot(TEXT("Trio_Entrance_Smash")); Run->Note += TEXT("smash "); }
                if (S > 6.5f && !Run->Note.Contains(TEXT("wizard"))) { Shot(TEXT("Trio_Entrance_Wizard")); Run->Note += TEXT("wizard "); }
                if (S > 10.5f && !Run->Note.Contains(TEXT("ranger"))) { Shot(TEXT("Trio_Entrance_Ranger")); Run->Note += TEXT("ranger "); }
                if (!B->IsFortressScenePlaying())
                {
                    Run->Sub = 3; Run->At = Now; Run->Note += FString::Printf(TEXT("scene=%.1f lines=%d "), S, B->GetFortressLinesSpoken());
                    // (his camera on the battle: the fiend a little right of centre)
                    if (F) Chuck->SetActorRotation(FRotator(0, (F->GetActorLocation() - Chuck->GetActorLocation()).Rotation().Yaw - 12.f, 0));
                    Chuck->Recenter(); Chuck->SetOrbitPitch(4);
                }
                else if (S > 20.f) Next(false, TEXT("scene_stuck=1"));
                break;
            }
            // Watching from the forecourt's edge (his own camera) while the battle goes on.
            const int32 K = static_cast<int32>(S / 5.f);
            if (K < 8 && !Run->Note.Contains(FString::Printf(TEXT("b%d "), K))) { Shot(FString::Printf(TEXT("Trio_Battle_%d"), K)); Run->Note += FString::Printf(TEXT("b%d "), K); }
            if (F && F->GetState() == ATempleSkeleton::EState::Attack && F->GetLastAttack() == ATempleSkeleton::EAttack::Hurl && F->GetStateTime() > .7f && !Run->Note.Contains(TEXT("hurl ")))
                { Shot(TEXT("Trio_Hurl")); Run->Note += TEXT("hurl "); }
            if (F && F->GetState() == ATempleSkeleton::EState::Attack && F->GetLastAttack() == ATempleSkeleton::EAttack::Smash && F->GetStateTime() > 1.36f && !Run->Note.Contains(TEXT("down ")))
                { Shot(TEXT("Trio_Smash")); Run->Note += TEXT("down "); }
            if (S < 45.f) break;
            const FVector Fl = F ? PhlegLocal(F->GetActorLocation()) : FVector::ZeroVector;
            const float InBreach = F ? BreachInside(Fl.X, Fl.Y) : 0.f;
            const bool bPass = B->GetFortressEntrances() == Run->Chains0 + 1 && B->GetFortressLinesSpoken() == 3 && B->GetFiendArrows() > 0 && B->GetFiendBolts() > 0
                && F && F->GetSmashes() > 0 && F->GetHurls() > 0 && B->GetFireBursts() > 0 && B->GetFallen() > 0 && InBreach < -100.f;
            Next(bPass, FString::Printf(TEXT("%sarrows=%d bolts=%d cuts=%d smashes=%d hurls=%d bursts=%d fire_hits=%d fire_dodges=%d smash_dodges=%d fallen=%d roars=%d hero_blows=%d breach=%.0f heroes_hit=%d/%d/%d"),
                *Run->Note, B->GetFiendArrows(), B->GetFiendBolts(), B->GetFiendCuts(), F ? F->GetSmashes() : 0, F ? F->GetHurls() : 0, B->GetFireBursts(), B->GetFireHeroHits(),
                B->GetFireDodges(), B->GetSmashDodges(), B->GetFallen(), F ? F->GetFortRoars() : 0, F ? F->GetHeroBlowsTaken() : 0, InBreach,
                B->GetHero(0) ? B->GetHero(0)->GetHitsTaken() : -1, B->GetHero(1) ? B->GetHero(1)->GetHitsTaken() : -1, B->GetHero(2) ? B->GetHero(2)->GetHitsTaken() : -1));
            break;
        }
        case ET::BusStop:
        {
            // (2026-11-17) The bus stop in the wastes: the fragment and its commuter from the path; he walks up to him (who
            // turns his head to the rat), then on into the small rupture on the path past the stop: an Astral death.
            ADockNPC* Man = nullptr;
            for (const TWeakObjectPtr<ADockNPC>& Np : ADockNPC::All()) if (Np.IsValid() && Np->Tags.Contains(TEXT("PhlegBusinessman"))) Man = Np.Get();
            FVector2D C, Tt, Nn; BusStopFrame(C, Tt, Nn);
            if (Run->Sub == -1)
            {
                const FVector2D S = C - Tt * 620.f;
                Place(static_cast<float>(S.X), static_cast<float>(S.Y), ApproachGround(static_cast<float>(S.X), static_cast<float>(S.Y)), FMath::RadiansToDegrees(FMath::Atan2(Tt.Y + Nn.Y * .5f, Tt.X + Nn.X * .5f)));
                Chuck->SetOrbitPitch(-6); Run->Sub = 0; break;
            }
            if (!Man) { Next(false, TEXT("no_businessman=1")); break; }
            if (Run->Sub == 0 && T > 2.f) { Shot(TEXT("BusStop_Path")); Chuck->SetOrbitPitch(-32); Run->Sub = 1; }
            else if (Run->Sub == 1 && T > 3.4f) { Shot(TEXT("BusStop_High")); Chuck->SetOrbitPitch(-4); Run->Sub = 2; Run->At = Now; }
            else if (Run->Sub == 2)
            {
                const FVector2D Goal = C + Tt * 90.f + Nn * 440.f;   // on the road below him by the kerb
                Toward(static_cast<float>(Goal.X), static_cast<float>(Goal.Y));
                if (FVector2D::Distance(FVector2D(P.X, P.Y), Goal) < 50.f || T > 14.f) { Stop(); Run->Sub = 3; Run->At = Now; }
            }
            else if (Run->Sub == 3 && T > 1.6f)
            {
                Chuck->SetActorRotation(FRotator(0, (Man->GetActorLocation() - Chuck->GetActorLocation()).Rotation().Yaw - 25.f, 0)); Chuck->Recenter(); Chuck->SetOrbitPitch(2);
                Run->Note += FString::Printf(TEXT("watching=%d mocap=%d talk=%d "), Man->IsWatchingChuck() ? 1 : 0, Man->HasMocap() ? 1 : 0, Man->CanTalk() ? 1 : 0);
                Run->Sub = 4; Run->At = Now;
            }
            else if (Run->Sub == 4 && T > 1.2f) { Shot(TEXT("BusStop_Businessman")); Run->Sub = 5; Run->At = Now; }
            else if (Run->Sub == 5)
            {
                // Into the rupture on the path beyond the stop.
                const FVector2D Rc = RiftCentre(2);
                Toward(static_cast<float>(Rc.X), static_cast<float>(Rc.Y));
                if (T > 1.f && !Run->Note.Contains(TEXT("ruptureshot"))) { Shot(TEXT("BusStop_Rupture")); Run->Note += TEXT("ruptureshot "); }
                if (Deaths > 0) { Stop(); Run->Note += TEXT("rupture_death=1 "); Run->Sub = 6; Run->At = Now; }
                else if (T > 18.f) Next(false, Run->Note + TEXT("no_rupture_death=1"));
            }
            else if (Run->Sub == 6 && T > 4.f)
            {
                const bool bPass = Run->Note.Contains(TEXT("watching=1")) && Run->Note.Contains(TEXT("mocap=1")) && Run->Note.Contains(TEXT("talk=1")) && Deaths == 1 && !Chuck->IsAstral();
                Next(bPass, Run->Note);
            }
            break;
        }
        case ET::Bleed:
        {
            // (2026-11-17) The Feywild bleedthrough: on the island after the trio's entrance, standing on the river's columns
            // while the moat and the waves fall (they never fall there); onto a whole slab before the river comes; in the
            // collapse, his slab cracks under him and he goes into the river: swept down it into the far tear, and back at
            // the forecourt's checkpoint with it all re-armed.
            ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(World);
            APhlegBleed* Bl = APhlegBleed::Get(World);
            if (!B || !Bl) { Next(false, TEXT("no_bleed=1")); break; }
            if (Run->Sub == -1) { Place(IslandX0 + 300, 300, FortressTop, 0); Run->Sub = 0; break; }
            const float Rx = RiverX0 + 110.f;
            const float Cl = Bl->GetClock();
            auto Face = [&](float Yaw, float Pitch) { Chuck->SetActorRotation(FRotator(0, Yaw, 0)); Chuck->Recenter(); Chuck->SetOrbitPitch(Pitch); };
            auto Once = [&](const FString& Tag) { if (Run->Note.Contains(Tag + TEXT(" "))) return false; Run->Note += Tag + TEXT(" "); return true; };
            if (Run->Sub == 0)
            {
                if (!B->IsReady() || !B->GetFiend()) { if (T > 40.f) Next(false, TEXT("not_ready=1")); break; }
                if (!B->HasFortressSceneRun()) { Toward(RiverX1 + 70.f, 300.f); if (B->IsFortressScenePlaying()) { Stop(); B->SkipFortressScene(); } else if (T > 30.f) Next(false, TEXT("no_scene=1")); break; }
                if (B->IsFortressScenePlaying()) break;
                Run->Sub = 1; Run->At = Now; Run->Chains0 = Bl->GetStarts();
                break;
            }
            if (Run->Sub == 1)
            {
                // Onto the river's columns (they never fall in the waves).
                if (FMath::Abs(P.X - Rx) > 30.f || FMath::Abs(P.Y - 300.f) > 40.f) Toward(Rx, 300.f); else Stop();
                if (Bl->GetStarts() > Run->Chains0) { Run->Sub = 2; Run->At = Now; }
                else if (T > 12.f) Next(false, TEXT("no_start=1"));
                break;
            }
            if (Run->Sub == 2)
            {
                if (FMath::Abs(P.X - Rx) > 30.f || FMath::Abs(P.Y - 300.f) > 40.f) Toward(Rx, 300.f); else Stop();
                if (Cl > 1.6f && Once(TEXT("faceback"))) Face(180.f, -10.f);
                if (Cl > 2.6f && Once(TEXT("moatshot"))) Shot(TEXT("Bleed_Moat_Back"));
                if (Cl > 4.f && Once(TEXT("facefront"))) Face(0.f, -6.f);
                if (Cl > 4.8f && Once(TEXT("frontshot"))) Shot(TEXT("Bleed_Moat_Front"));
                if (Cl > 5.6f && Once(TEXT("moat")))
                    Run->Note += FString::Printf(TEXT("moat=%d/%d/%d "), Bl->GetFallen(APhlegBleed::EZone::MoatBack), Bl->GetFallen(APhlegBleed::EZone::MoatSide), Bl->GetFallen(APhlegBleed::EZone::MoatFront));
                if (Cl > 7.f && Once(TEXT("faceneg"))) Face(-90.f, -14.f);
                if (Cl > APhlegBleed::Wave1At + 2.5f && Once(TEXT("w1shot"))) Shot(TEXT("Bleed_Wave1"));
                if (Cl > APhlegBleed::Wave2At - 1.f && Once(TEXT("facepos"))) Face(90.f, -14.f);
                if (Cl > APhlegBleed::Wave2At + 2.5f && Once(TEXT("w2shot"))) Shot(TEXT("Bleed_Wave2"));
                if (Cl > APhlegBleed::Wave3At + 2.f && Once(TEXT("w3shot"))) Shot(TEXT("Bleed_Wave3"));
                if (Cl > APhlegBleed::RiverAt - 2.f)
                {
                    Run->Note += FString::Printf(TEXT("land_fallen=%d "), Bl->GetFallen(APhlegBleed::EZone::Land));
                    FVector W;
                    if (!Bl->FindWhole(3, 300.f, W)) { Next(false, Run->Note + TEXT("no_whole_slab=1")); break; }
                    Run->Note += FString::Printf(TEXT("refuge=%.0f "), W.Y);
                    Run->Sub = 3; Run->At = Now; Run->Hop = FMath::RoundToInt(W.Y);
                }
                break;
            }
            if (Run->Sub == 3)
            {
                const float Wx = IslandX0 + FieldColW * 1.5f, Wy = static_cast<float>(Run->Hop);
                if (FMath::Abs(P.X - Wx) > 30.f || FMath::Abs(P.Y - Wy) > 40.f) Toward(Wx, Wy); else Stop();
                if (Cl > APhlegBleed::RiverAt + 1.6f && Once(TEXT("faceriver"))) Face(70.f, -8.f);
                if (Cl > APhlegBleed::RiverAt + 2.2f && Once(TEXT("rivershot0"))) Shot(TEXT("Bleed_River_Open"));
                if (Cl > APhlegBleed::RiverAt + 4.5f && Once(TEXT("faceriver2"))) Face(-60.f, -10.f);
                if (Cl > APhlegBleed::RiverAt + 5.5f && Once(TEXT("rivershot1"))) Shot(TEXT("Bleed_River"));
                if (Cl > APhlegBleed::CollapseAt - 1.f && Once(TEXT("facecollapse"))) Face(160.f, -16.f);
                if (Cl > APhlegBleed::CollapseAt + .3f)
                {
                    const int32 Under = Bl->SlabStateAt(P.X, P.Y);
                    Run->Note += FString::Printf(TEXT("collapse_under=%d front=%.0f "), Under, Bl->GetRiverFront());
                    Shot(TEXT("Bleed_Collapse"));
                    Run->Sub = 4; Run->At = Now;
                }
                break;
            }
            if (Run->Sub == 4)
            {
                // Into the water.
                Toward(Rx + 80.f, P.Y);
                if (Bl->GetStage() == APhlegBleed::EStage::Swept) { Run->Sub = 5; Run->At = Now; Run->Note += FString::Printf(TEXT("swept_after=%.2f "), T); }
                else if (T > 6.f) Next(false, Run->Note + TEXT("not_swept=1"));
                break;
            }
            if (Run->Sub == 5)
            {
                Stop();
                for (int32 K = 0; K < 3; ++K) if (T > .7f + K * 1.1f && Once(FString::Printf(TEXT("sweptshot%d"), K))) Shot(FString::Printf(TEXT("Bleed_Swept_%d"), K));
                if (Bl->GetSweepEnds() > 0) { Run->Sub = 6; Run->At = Now; Run->Note += FString::Printf(TEXT("sweep_s=%.1f "), T); }
                else if (T > 14.f) Next(false, Run->Note + TEXT("sweep_stuck=1"));
                break;
            }
            if (Run->Sub == 6)
            {
                if (Chuck->IsAstral() || !Ground || T < 1.f) { if (T > 10.f) Next(false, Run->Note + TEXT("no_return=1")); break; }
                Shot(TEXT("Bleed_Returned"));
                const bool bMoat = Run->Note.Contains(FString::Printf(TEXT("moat=%d/%d/%d"), FieldRows * MoatCols, (FieldCols - 2 * MoatCols) * 2 * MoatRows, FieldRows * MoatCols));
                int32 LandFallen = 0; { const int32 I = Run->Note.Find(TEXT("land_fallen=")); if (I >= 0) LandFallen = FCString::Atoi(*Run->Note.Mid(I + 12)); }
                const bool bPass = bMoat && LandFallen >= 15 && Bl->GetSweeps() == 1 && Bl->GetSweepEnds() == 1 && Bl->GetRearms() >= 1 && Deaths == 0
                    && OnIsland(P.X, P.Y) && Bl->GetMissing() == 0 && Bl->GetSlabCount() == FieldCols * FieldRows;
                Next(bPass, Run->Note + FString::Printf(TEXT("starts=%d rearms=%d pressed=%d at_return=%s"), Bl->GetStarts(), Bl->GetRearms(), Bl->GetPressedFalls(), *P.ToString()));
            }
            break;
        }
        default: Next(false, TEXT("unknown=1")); break;
        }
    }, 1.f / 60.f, true);
}
