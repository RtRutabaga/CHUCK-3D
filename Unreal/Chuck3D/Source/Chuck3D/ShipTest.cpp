// -ChuckChultTest -ChuckShipTest (Claude, 2026-11-01; docs/SHIP.md): the escape played for real from the rubble map's
// crevice (crawled to its end by the real character) onto the ship; the ship's checks; stills of every space; then the
// real character aboard: through the doorways to the galley, and through the crew quarters into the cabin; down the
// ladder into the hold and back up it; over the side into the sea (an Astral death, back where he went over); the
// escape skipped. Milestone 2 (2026-11-02): the weather deck's stills, inboard and outboard; up and down the companion
// ladder and the quarterdeck's; up the rope ladder to the main top, off its side onto the main yard and hand over hand
// out to the yardarm, let go into the sea and back on deck; the same drop from the yard onto the deck; the rail holds.
// -ChuckShipViewsOnly stops after the stills; -ChuckShipTrials=a+b runs some.
#include "ShipSetting.h"
#include "ShipEscape.h"
#include "ShipCrew.h"
#include "ChultTempleRubble.h"
#include "ChultTempleInterior.h"
#include "ChuckCharacter.h"
#include "EnemyRat.h"
#include "ClayJar.h"
#include "EngineUtils.h"
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
#include "Misc/Parse.h"
#include "Misc/Paths.h"

extern ENGINE_API float GAverageMS;   // the engine's smoothed frame time (UnrealEngine.cpp)

using namespace Ship;

namespace
{
    FVector SW(float X, float Y, float Z) { return ShipWorld(FVector(X, Y, Z)); }
    constexpr float Stand = 34.65f;
}

void ReviewShip(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    struct FView { const TCHAR* Name; FVector Eye, Aim; };
    const TArray<FView> Views = {
        {TEXT("ArrivalSeam"), {290, -120, LowerZ + 120}, {SeamX1, SeamY, LowerZ + 40}},
        {TEXT("ArrivalHole"), {150, -150, LowerZ + 90}, {125, 420, LowerZ + 80}},
        {TEXT("ArrivalHatch"), {-60, 200, LowerZ + 170}, {HatchX1, 0, LowerZ}},
        {TEXT("ArrivalAft"), {300, 100, LowerZ + 110}, {-170, 0, LowerZ + 80}},
        {TEXT("ArrivalLow"), {280, 180, LowerZ + 35}, {0, -200, LowerZ + 40}},
        {TEXT("Galley"), {370, -250, LowerZ + 150}, {950, 100, LowerZ + 60}},
        {TEXT("GalleyHearth"), {700, 250, LowerZ + 90}, {950, 0, LowerZ + 70}},
        {TEXT("GalleyLow"), {400, 260, LowerZ + 30}, {900, 300, LowerZ + 50}},
        {TEXT("Quarters"), {-200, 250, LowerZ + 160}, {-800, -100, LowerZ + 100}},
        {TEXT("QuartersLow"), {-220, 120, LowerZ + 30}, {-700, 0, LowerZ + 120}},
        {TEXT("QuartersTable"), {-180, -60, LowerZ + 110}, {-340, -175, LowerZ + 70}},
        {TEXT("Cabin"), {-1110, 200, LowerZ + 170}, {-1550, -100, LowerZ + 90}},
        {TEXT("CabinWindows"), {-1250, -40, LowerZ + 60}, {SternX, 0, LowerZ + 140}},
        {TEXT("CabinChest"), {-1080, 60, LowerZ + 80}, {-1185, 178, LowerZ + 30}},
        {TEXT("Hold"), {150, 150, HoldZ + 170}, {-700, -100, HoldZ + 40}},
        {TEXT("HoldFore"), {0, -150, HoldZ + 170}, {900, 100, HoldZ + 40}},
        {TEXT("HoldLadder"), {-60, 120, HoldZ + 80}, {HatchX1, 0, HoldZ + 160}},
        {TEXT("HoldLow"), {-100, 100, HoldZ + 30}, {-600, -150, HoldZ + 40}},
        // The weather deck (milestone 2).
        {TEXT("DeckFromBow"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}},
        {TEXT("DeckFromQuarter"), {-1460, 260, QuarterZ + 190}, {600, -60, DeckZ + 230}},
        {TEXT("DeckRat"), {-40, 210, DeckZ + 40}, {500, -150, DeckZ + 110}},
        {TEXT("DeckRail"), {300, 330, DeckZ + 45}, {300, 1200, DeckZ - 250}},
        {TEXT("DeckCompanion"), {-120, 240, DeckZ + 150}, {-250, 130, DeckZ}},
        {TEXT("Helm"), {-1180, 190, QuarterZ + 150}, {HelmX, 0, QuarterZ + 100}},
        {TEXT("BreakLadders"), {-560, 40, DeckZ + 160}, {-900, -200, DeckZ + 80}},
        {TEXT("Sails"), {900, -520, DeckZ + 160}, {-300, 60, 1900}},
        {TEXT("RopeLadder"), {-250, 140, DeckZ + 120}, {TopLadderX, 0, 1200}},
        {TEXT("MainTop"), {MainMastX + 160, -90, MainTopZ + 75}, {MainYardX, 520, MainYardZ}},
        {TEXT("TopDown"), {MainMastX + 150, 100, MainTopZ + 110}, {MainMastX + 380, 120, DeckZ}},
        {TEXT("Yardarm"), {MainYardX + 60, 560, MainYardZ + 40}, {MainYardX + 600, 1600, 300}},
        {TEXT("Waterline"), {300, -950, 390}, {300, -430, 300}},
        {TEXT("Bow"), {2600, -900, 650}, {1500, 0, 450}},
        {TEXT("Stern"), {-3100, -700, 820}, {-1600, 0, 520}},
        {TEXT("Outside"), {2300, -3600, 1200}, {0, 0, 1000}},
        {TEXT("OutsideLow"), {-2600, 2400, 420}, {0, 0, 700}},
        {TEXT("Horizon"), {0, -150, DeckZ + 170}, {0, 2000, DeckZ + 170}},
        // The crew (milestone 3, ShipCrew.cpp).
        {TEXT("CrewDeck"), {-170, 260, DeckZ + 170}, {90, -40, DeckZ + 100}},
        {TEXT("CrewRatEye"), {-80, 160, DeckZ + 55}, {100, -20, DeckZ + 120}},
        {TEXT("CrewConcertina"), {335, -55, DeckZ + 150}, {215, -165, DeckZ + 125}},
        {TEXT("CrewConcertinaFront"), {109, -59, DeckZ + 150}, {215, -165, DeckZ + 125}},
        {TEXT("CrewDancerFace"), {-66, 44, DeckZ + 160}, {70, -20, DeckZ + 145}},
        {TEXT("CrewCheering"), {50, 300, DeckZ + 150}, {150, 170, DeckZ + 150}},
        {TEXT("CrewDancer"), {230, 60, DeckZ + 110}, {70, -20, DeckZ + 75}},
        {TEXT("CrewDancerLow"), {180, 110, DeckZ + 35}, {70, -20, DeckZ + 60}},
        {TEXT("CrewJeffries"), {-720, 90, DeckZ + 160}, {-534, 0, DeckZ + 130}},
        {TEXT("CrewJeffriesRat"), {-660, 50, DeckZ + 45}, {-534, 0, DeckZ + 140}},
        {TEXT("CrewJeffriesBack"), {-380, -150, DeckZ + 170}, {-534, 0, DeckZ + 120}},
        {TEXT("CrewFencers"), {875, 340, DeckZ + 170}, {875, 0, DeckZ + 100}},
        {TEXT("CrewFencersLow"), {680, -240, DeckZ + 45}, {880, 0, DeckZ + 120}},
        {TEXT("CrewSeated"), {-300, -40, LowerZ + 140}, {-465, -139, LowerZ + 105}},
        {TEXT("CrewChef"), {470, 140, LowerZ + 160}, {680, -115, LowerZ + 110}},
        // Their faces close to (the mustaches, user 2026-11-02).
        {TEXT("FaceConcertina"), {166, -116, DeckZ + 158}, {215, -165, DeckZ + 152}},
        {TEXT("FaceCheering"), {95, 45, DeckZ + 165}, {150, 170, DeckZ + 160}},
        {TEXT("FaceDancer"), {7, 10, DeckZ + 152}, {70, -20, DeckZ + 147}},
        {TEXT("FaceJeffries"), {-604, 0, DeckZ + 158}, {-534, 0, DeckZ + 154}},
        {TEXT("FaceSeated"), {-372, -166, LowerZ + 118}, {-465, -139, LowerZ + 112}},
        {TEXT("FaceChef"), {690, 15, LowerZ + 150}, {680, -108, LowerZ + 146}}};
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(74);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }

    enum class ET : uint8 { Galley, Cabin, LadderDown, LadderUp, Sea, Seam, DeckUp, DeckDown, Quarterdeck, Rigging, YardDrop, Rail, CrewTalk, ChefChase, Fencers };
    static const ET AllTrials[] = {ET::Galley, ET::Cabin, ET::LadderDown, ET::LadderUp, ET::Sea, ET::Seam, ET::DeckUp, ET::DeckDown, ET::Quarterdeck, ET::Rigging, ET::YardDrop, ET::Rail,
        ET::CrewTalk, ET::ChefChase, ET::Fencers};
    static const TCHAR* Names[] = {TEXT("galley"), TEXT("cabin"), TEXT("ladder_down"), TEXT("ladder_up"), TEXT("sea"), TEXT("seam"),
        TEXT("deck_up"), TEXT("deck_down"), TEXT("quarterdeck"), TEXT("rigging"), TEXT("yard_drop"), TEXT("rail"), TEXT("crew_talk"), TEXT("chef_chase"), TEXT("fencers")};
    struct FRun
    {
        int32 Phase = 0, Trial = -1, Sub = 0, Point = 0, Failures = 0, Deaths0 = 0, Mounts0 = 0, Crawls0 = 0, Shots = 0, ShipFailures = -1, Catches0 = 0, Leaps0 = 0;
        double Next = 0, Start = 0, At = -1, EscapeAt = -1;
        bool bStarted = false, bPass = false, bDone = false, bEscaped = false, bViews = false, bMount = false, bTop = false;
        TArray<int32> Results; TArray<ET> Trials; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer;
        int32 EscapeOk = 0;
        // The crew's trials (milestone 3): what he heard, and counts at the start of a stage.
        TArray<FString> Heard; double Pressed = 0; int32 TalkOk = 0, TalkChecks = 0, Count0 = 0, Count1 = 0, Seen = 0;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Camera = Camera;
    FString Only; FParse::Value(FCommandLine::Get(), TEXT("ChuckShipTrials="), Only);
    for (int32 I = 0; I < UE_ARRAY_COUNT(AllTrials); ++I) if (Only.IsEmpty() || Only.Contains(Names[I])) Run->Trials.Add(AllTrials[I]);

    // ---- The escape: to the crevice's end on all fours, as a player would come.
    {
        using namespace TempleRubble;
        Chuck->ResetCrawlingAt(ChultTempleInteriorWorldOf(FVector(CrawlBendS, CrawlEndT + 170, RZ + 32.5f)));
        Run->Start = World->GetTimeSeconds();
    }
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run, Views]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        const double Now = World->GetTimeSeconds();
        auto Push = [&](const FVector& D, float Scale = 1.f) { Chuck->SetTestStickWorld(D * Scale); Chuck->AddMovementInput(D, Scale, true); };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        auto Shot = [&](const TCHAR* Name) { if (Run->Shots < 40) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Ship") / FString::Printf(TEXT("ShipPlay_%s.png"), Name), true, false); } };
        AShipEscape* Escape = AShipEscape::Get(World);
        // ---- 1. The escape, played through.
        if (!Run->bEscaped)
        {
            const double T = Now - Run->Start;
            if (!Escape || (!Escape->IsPlaying() && Escape->GetPlays() == 0))
            {
                Push((ChultTempleInteriorWorldOf(FVector(TempleRubble::CrawlBendS, TempleRubble::CrawlEndT, 0)) - ChultTempleInteriorWorldOf(FVector(TempleRubble::CrawlBendS, TempleRubble::CrawlEndT + 1, 0))).GetSafeNormal2D());
                if (T > 20) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_ESCAPE_NO_START")); Run->bEscaped = true; ++Run->Failures; }
                return;
            }
            // Pictures of its shots as it plays.
            static const TCHAR* Seen[] = {TEXT("Passage"), TEXT("Mouth"), TEXT("Sea")};
            for (int32 K = 0; K < 3; ++K)
                if (FCString::Strcmp(Escape->GetShotName(), Seen[K]) == 0 && !(Run->EscapeOk & (1 << K)) && Escape->GetClock() > AShipEscape::StageAt + 1.5f + K * .3f)
                { Run->EscapeOk |= 1 << K; Shot(*FString::Printf(TEXT("Escape_%s"), Seen[K])); }
            if (Escape->IsPlaying() && Escape->HasCrawledOut() && !(Run->EscapeOk & 8)) { Run->EscapeOk |= 8; }
            if (Escape->IsPlaying() || T < 3) { if (T > 60) { Run->bEscaped = true; ++Run->Failures; } return; }
            Run->bEscaped = true; Run->EscapeAt = Now;
            Shot(TEXT("Escape_Control"));
            const int32 Space = ShipSpaceAt(Chuck->GetActorLocation());
            const bool bPass = Escape->GetFinishes() == 1 && Escape->GetSkips() == 0 && Escape->HasCrawledOut() && Space == int32(EShipSpawn::Arrival) && !Chuck->IsCrawling()
                && Chuck->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist2D(Chuck->GetActorLocation(), ShipSpawn(EShipSpawn::Arrival)) < 60;
            Run->Failures += !bPass;
            if (const AShipSetting* S = AShipSetting::Get(World)) { Run->ShipFailures = S->Check(); Run->Failures += Run->ShipFailures; }
            else ++Run->Failures;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_TEST pass=%d plays=%d finishes=%d out=%d space=%s shots=%d seconds=%.1f"), bPass, Escape->GetPlays(), Escape->GetFinishes(),
                Escape->HasCrawledOut(), ShipSpaceName(Space), FMath::CountBits(uint64(Run->EscapeOk & 7)), Now - Run->Start);
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) Chuck->DisableInput(PC);
            Run->Next = Now + 1;
            return;
        }
        // ---- 2. The stills.
        const int32 I = Run->Phase / 2;
        if (I < Views.Num())
        {
            if (Now < Run->Next) return;
            Run->Next = Now + 1.2;
            if (Run->Phase % 2 == 0)
            {
                Chuck->SetActorHiddenInGame(true);
                if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Run->Camera.Get());
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(SW(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z), (SW(Views[I].Aim.X, Views[I].Aim.Y, Views[I].Aim.Z) - SW(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z)).Rotation());
                ++Run->Phase; return;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Ship") / (FString(Views[I].Name) + TEXT(".png")), true, false);
            {
                const AShipSetting* S = AShipSetting::Get(World);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_VIEW name=%s tilt_pitch=%.2f tilt_roll=%.2f frame_ms=%.1f"), Views[I].Name, S ? S->GetSeaTilt().X : 0.f, S ? S->GetSeaTilt().Y : 0.f, GAverageMS);
            }
            ++Run->Phase;
            return;
        }
        if (!Run->bViews)
        {
            // (A while after the last still, so its picture is taken before his own view comes back.)
            if (Now < Run->Next) return;
            Run->bViews = true; Run->Next = Now + .5;
            Chuck->SetActorHiddenInGame(false);
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
            if (const AShipCrew* Crew = AShipCrew::Get(World))
            {
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CREW count=%d beat=%.1f score=%d phrases=%d fence_out=%.1f parry_gap=%.1f"), Crew->GetCount(), Crew->GetBeat(),
                    ShipScore::IsPlaying(), Crew->GetFencePhrases(), Crew->GetWorstFenceOut(), Crew->GetLastParryGap());
                for (const TWeakObjectPtr<ADockNPC>& E : ADockNPC::All())
                    if (E.IsValid() && E->IsPirate())
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_PIRATE %s mocap=%d props=%d grip_cm=%.1f sink_cm=%.1f bellows=%.1f toast=%.2f dance=%.1f/%.1f strain=%.2f chef=%s lines=%d"),
                            *E->DisplayName, E->HasMocap(), E->GetPirateProps(), E->GetWorstPropGrip(), E->GetWorstFootSink(), E->GetBellows(), E->GetToast(),
                            E->GetDanceFeet().X, E->GetDanceFeet().Y, E->GetStrain(), E->GetChefStateName(), E->Lines.Num());
            }
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckShipViewsOnly"))) { Run->bDone = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TEST views_only=1 failures=%d"), Run->Failures); FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0); return; }
        if (Now < Run->Next) return;

        // ---- 3. The trials.
        auto* M = Chuck->GetCharacterMovement();
        const FVector L = ShipLocal(Chuck->GetActorLocation());
        const bool Ground = M->IsMovingOnGround();
        auto Reset = [&](EShipSpawn S) { Chuck->ResetAtLocation(ShipSpawn(S)); Chuck->SetActorRotation(FRotator(0, ShipSpawnYaw(S), 0)); Chuck->SetRunHeld(false); Stop(); Chuck->Recenter(); };
        auto Steer = [&](float X, float Y, float Within = 28.f) -> bool
        {
            if (FVector2D::Distance(FVector2D(L.X, L.Y), FVector2D(X, Y)) < Within) return true;
            Push((SW(X, Y, L.Z) - Chuck->GetActorLocation()).GetSafeNormal2D()); return false;
        };
        if (!Run->bStarted)
        {
            ++Run->Trial;
            if (Run->Trial >= Run->Trials.Num())
            {
                int32 Passed = 0; for (int32 R : Run->Results) Passed += R;
                const int32 Fails = Run->Failures + (Run->Results.Num() - Passed);
                int32 LiveRats = 0; for (const auto& R : AEnemyRat::All()) if (R.IsValid() && !R->IsDead() && InShip(R->GetActorLocation())) ++LiveRats;
                const AShipSetting* S = AShipSetting::Get(World);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TEST failures=%d trials=%d passed=%d ship_check=%d rats=%d jars=%d ladders=%d missing=%d tris=%d"), Fails, Run->Results.Num(), Passed, Run->ShipFailures,
                    LiveRats, S ? S->GetJarsPlaced() : -1, S ? S->GetLadders() : -1, S ? S->GetMissingMaterials() : -1, S ? S->GetTriangles() : -1);
                Run->bDone = true; FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0); return;
            }
            Run->bStarted = true; Run->Sub = 0; Run->Point = 0; Run->At = Now; Run->bPass = false; Run->bMount = false;
            Run->Deaths0 = Chuck->GetFallDeaths(); Run->Mounts0 = Chuck->GetLadderMounts(); Run->Crawls0 = Chuck->GetCrawls();
            Run->Catches0 = Chuck->GetSwingCatches(); Run->Leaps0 = Chuck->GetSwingLeaps(); Run->bTop = false;
            switch (Run->Trials[Run->Trial])
            {
            case ET::Galley: case ET::Sea: case ET::Seam: Reset(EShipSpawn::Arrival); break;
            case ET::Cabin: Reset(EShipSpawn::Quarters); break;
            case ET::LadderDown: Reset(EShipSpawn::Arrival); break;
            case ET::LadderUp: Reset(EShipSpawn::Hold); break;
            case ET::DeckUp: Reset(EShipSpawn::Quarters); break;
            case ET::DeckDown: case ET::Quarterdeck: case ET::Rigging: case ET::Rail: Reset(EShipSpawn::Deck); break;
            case ET::YardDrop: Reset(EShipSpawn::Deck); Chuck->ResetAtLocation(SW(MainYardX, 70, MainTopZ + Stand)); Chuck->SetActorRotation(FRotator(0, 90, 0)); break;
            case ET::CrewTalk: case ET::Fencers: Reset(EShipSpawn::Deck); break;
            case ET::ChefChase: Reset(EShipSpawn::Arrival); break;
            }
            Run->Heard.Reset(); Run->TalkOk = Run->TalkChecks = Run->Count0 = Run->Count1 = Run->Seen = 0; Run->Pressed = 0;
            Chuck->SetSanity(AChuckCharacter::MaxSanity);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TRIAL_START name=%s"), Names[int32(Run->Trials[Run->Trial])]);
            return;
        }
        const float Time = static_cast<float>(Now - Run->At);
        const int32 NewDeaths = Chuck->GetFallDeaths() - Run->Deaths0;
        bool Done = false; float Limit = 40;
        const ET Trial = Run->Trials[Run->Trial];
        switch (Trial)
        {
        case ET::Galley:
        {
            // Out of the arrival compartment by its fore door, round the work table to the hearth.
            const FVector2D Pts[] = {{290, GalleyDoorY}, {420, GalleyDoorY}, {520, 140}, {840, 120}, {850, -40}};
            if (Time < .5f) break;
            if (Steer(Pts[Run->Point].X, Pts[Run->Point].Y)) { if (++Run->Point == UE_ARRAY_COUNT(Pts)) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Galley) && NewDeaths == 0; Shot(TEXT("Galley")); } }
            break;
        }
        case ET::Cabin:
        {
            // Along the crew quarters past the mast, through the cabin's door, to the stern windows.
            const FVector2D Pts[] = {{-420, 90}, {-560, 90}, {-900, 50}, {-1040, 0}, {-1130, 0}, {-1480, 30}};
            if (Time < .5f) break;
            if (Steer(Pts[Run->Point].X, Pts[Run->Point].Y)) { if (Run->Point == 2) Shot(TEXT("Quarters")); if (++Run->Point == UE_ARRAY_COUNT(Pts)) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Cabin) && NewDeaths == 0; Shot(TEXT("Cabin")); } }
            break;
        }
        case ET::LadderDown:
        {
            // To the hatch's fore edge, then aft over its lip: he turns and lowers himself onto the ladder; down it; off.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(200, 0, 12)) Run->Sub = 1; }
            else if (Run->Sub == 1)
            {
                if (!Chuck->IsOnLadder()) Push(FVector(-1, 0, 0), .6f);
                if (Chuck->IsOnLadder()) { Run->Sub = 2; Run->bMount = true; Shot(TEXT("LadderTop")); }
            }
            else if (Run->Sub == 2)
            {
                if (Chuck->IsOnLadder()) Push(FVector(-1, 0, 0)); else Stop();
                if (!Chuck->IsOnLadder() && Ground && L.Z < LowerZ - 100) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Hold) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0; Shot(TEXT("LadderFoot")); }
            }
            break;
        }
        case ET::LadderUp:
        {
            // Into the ladder's foot, up it (the stick toward it), over the lip onto the arrival compartment's floor.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(HatchX1 - 40, 0, 10)) Run->Sub = 1; }
            else
            {
                Push(FVector(1, 0, 0));
                if (Chuck->IsOnLadder() && L.Z > HoldZ + 150 && Run->Shots < 40 && !Run->bMount) { Run->bMount = true; Shot(TEXT("LadderClimb")); }
                if (Ground && !Chuck->IsOnLadder() && L.Z > LowerZ) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Arrival) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0; }
            }
            break;
        }
        case ET::Sea:
        {
            // Put out over the side beside the hole: he falls into the sea - an Astral death - and is back at the compartment's spawn.
            Limit = 20;
            if (Run->Sub == 0) { Chuck->ResetAtLocation(SW(125, ShipOuterHalf(125, LowerZ) + 70, LowerZ + Stand + 20)); Run->Sub = 1; Run->At = Now; break; }
            if (Run->Sub == 1 && NewDeaths) { Run->Sub = 2; Run->At = Now; }
            if (Run->Sub == 2 && Time > 1.5f)
            {
                const bool Home = FVector::Dist(Chuck->GetActorLocation(), ShipSpawn(EShipSpawn::Arrival)) < 40 && Ground && !Chuck->IsAstral();
                if (Home || Time > 10) { Done = true; Run->bPass = Home && NewDeaths == 1; }
            }
            break;
        }
        case ET::Seam:
        {
            // Back into the crawlway through the masonry (on all fours) to its blind end, and out again on his feet.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(SeamX1 + 80, SeamY, 15)) Run->Sub = 1; }
            else if (Run->Sub == 1) { Push(FVector(-1, 0, 0)); if (L.X < SeamX0 + 40) { Run->Sub = 2; Shot(TEXT("SeamEnd")); } }
            else { Push(FVector(1, 0, 0)); if (L.X > SeamX1 + 90 && !Chuck->IsCrawling()) { Done = true; Run->bPass = Chuck->GetCrawls() > Run->Crawls0 && NewDeaths == 0; } }
            break;
        }
        }
        // ---- Milestone 2: the weather deck and the rig.
        // (A run along waypoints; true when the last is reached.)
        auto Route = [&](std::initializer_list<FVector2D> Pts, float Within = 14.f) -> bool
        {
            const FVector2D* P = Pts.begin() + FMath::Min(Run->Point, int32(Pts.size()) - 1);
            if (Steer(P->X, P->Y, Within)) { if (++Run->Point >= int32(Pts.size())) return true; }
            return false;
        };
        const AShipSetting* ShipNow = AShipSetting::Get(World);
        (void)ShipNow;
        switch (Trial)
        {
        case ET::DeckUp:
        {
            // From the crew quarters into the companion ladder's foot (aft of it, walking forward), up onto the weather deck.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(CompX1 - 60, (CompY0 + CompY1) * .5f, 10)) Run->Sub = 1; }
            else
            {
                Push(FVector(1, 0, 0));
                if (Chuck->IsOnLadder() && L.Z > LowerZ + 150 && !Run->bMount) { Run->bMount = true; Shot(TEXT("CompanionClimb")); }
                if (Ground && !Chuck->IsOnLadder() && L.Z > DeckZ) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0; Shot(TEXT("CompanionTop")); }
            }
            break;
        }
        case ET::DeckDown:
        {
            // From the deck to the companion's fore edge, aft over its lip onto the ladder, down into the crew quarters.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(CompX1 + 30, (CompY0 + CompY1) * .5f, 12)) Run->Sub = 1; }
            else if (Run->Sub == 1) { Push(FVector(-1, 0, 0), .85f); if (Chuck->IsOnLadder()) Run->Sub = 2; }
            else
            {
                if (Chuck->IsOnLadder()) Push(FVector(-1, 0, 0)); else Stop();
                if (!Chuck->IsOnLadder() && Ground && L.Z < DeckZ - 100) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Quarters) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0; }
            }
            break;
        }
        case ET::Quarterdeck:
        {
            // Up the port ladder at the break, across the quarterdeck by the helm, down the starboard ladder.
            Limit = 50;
            if (Time < .5f) break;
            const float E = QuarterFore + 8;
            if (Run->Sub == 0) { if (Route({{-150, 260}, {-800, 260}, {E + 47, BreakLadderY}}, 10)) Run->Sub = 1; }
            else if (Run->Sub == 1)
            {
                Push(FVector(-1, 0, 0));
                if (Ground && !Chuck->IsOnLadder() && L.Z > QuarterZ) { Run->Sub = 2; Run->Point = 0; Shot(TEXT("Quarterdeck")); }
            }
            else if (Run->Sub == 2) { if (Route({{-1000, 120}, {-1000, -150}, {E - 33, -BreakLadderY}}, 10)) Run->Sub = 3; }
            else if (Run->Sub == 3) { Push(FVector(1, 0, 0), .85f); if (Chuck->IsOnLadder()) Run->Sub = 4; }
            else
            {
                if (Chuck->IsOnLadder()) Push(FVector(1, 0, 0)); else Stop();
                if (!Chuck->IsOnLadder() && Ground && L.Z < QuarterZ - 60) { Done = true; Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck) && L.X > E && NewDeaths == 0 && Chuck->GetLadderMounts() >= Run->Mounts0 + 2; }
            }
            break;
        }
        case ET::Rigging:
        {
            // Up the rope ladder through the lubber's hole onto the main top; off its port side onto the main yard; hand
            // over hand out to the yardarm; let go, into the sea; back on deck.
            Limit = 70;
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Route({{-160, 40}, {TopLadderX - 50, 40}, {TopLadderX - 50, 0}}, 10)) Run->Sub = 1; }
            else if (Run->Sub == 1)
            {
                Push(FVector(1, 0, 0));
                if (Chuck->IsOnLadder() && L.Z > DeckZ + 500 && !Run->bMount) { Run->bMount = true; Shot(TEXT("RopeLadderClimb")); }
                if (Ground && !Chuck->IsOnLadder() && L.Z > MainTopZ) { Run->Sub = 2; Run->Point = 0; Run->bTop = true; Shot(TEXT("OnTheTop")); }
            }
            else if (Run->Sub == 2) { if (Route({{TopLadderX + 25, 70}, {MainYardX, 70}}, 8)) { Run->Sub = 3; Run->At = Now; } }
            else if (Run->Sub == 3)
            {
                // To the top's side over the yard, and jump off it, still pushing out.
                Push(FVector(0, 1, 0));
                if (L.Y > 98 && Ground) { Chuck->JumpPressed(); Run->Sub = 4; Run->At = Now; }
            }
            else if (Run->Sub == 4)
            {
                Push(FVector(0, 1, 0));
                if (Chuck->IsSwinging()) { Run->Sub = 5; Run->At = Now; Shot(TEXT("YardCatch")); }
                else if (Time > 3) { Done = true; }
            }
            else if (Run->Sub == 5)
            {
                // Out along the yard: a jump toward the yardarm each time he hangs.
                Push(FVector(0, 1, 0));
                if (Chuck->IsSwinging() && L.Y > MainGrip0 + (MainGrips - 1) * MainGripStep - 60) { Run->Sub = 6; Run->At = Now; Shot(TEXT("Yardarm")); }
                else if (Chuck->IsSwinging() && Time > .5f) { Chuck->JumpPressed(); Run->At = Now; }
                else if (!Chuck->IsSwinging() && !Chuck->IsSwingLeaping() && Time > 2) Done = true;
            }
            else if (Run->Sub == 6)
            {
                Stop();
                if (Time > .6f && Chuck->IsSwinging()) { Chuck->DodgeToward(FVector2D::ZeroVector); Run->Sub = 7; Run->At = Now; }
            }
            else if (Run->Sub == 7)
            {
                if (NewDeaths && Time > 1.f && Run->Sub == 7) { Run->Sub = 8; Run->At = Now; }
                else if (Time > 8) Done = true;
            }
            else if (Time > 1.5f)
            {
                const bool Home = FVector::Dist(Chuck->GetActorLocation(), ShipSpawn(EShipSpawn::Deck)) < 40 && Ground && !Chuck->IsAstral();
                if (Home || Time > 10)
                {
                    Done = true;
                    Run->bPass = Home && Run->bTop && NewDeaths == 1 && Chuck->GetSwingCatches() > Run->Catches0 && Chuck->GetSwingLeaps() - Run->Leaps0 >= MainGrips - 2;
                }
            }
            break;
        }
        case ET::YardDrop:
        {
            // From the main top onto the yard, then let go over the deck: a long fall onto it, no death, still aboard.
            Limit = 20;
            if (Run->Sub == 0) { Push(FVector(0, 1, 0)); if (L.Y > 98 && Ground) { Chuck->JumpPressed(); Run->Sub = 1; Run->At = Now; } }
            else if (Run->Sub == 1) { Push(FVector(0, 1, 0)); if (Chuck->IsSwinging()) { Stop(); Run->Sub = 2; Run->At = Now; } else if (Time > 3) Done = true; }
            else if (Run->Sub == 2) { if (Time > .5f) { Chuck->DodgeToward(FVector2D::ZeroVector); Run->Sub = 3; Run->At = Now; } }
            else if (Ground && Time > .5f)
            {
                Done = true;
                Run->bPass = NewDeaths == 0 && ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck) && FMath::Abs(L.Z - (DeckZ + Stand)) < 8;
                Shot(TEXT("YardDropLanded"));
            }
            break;
        }
        case ET::CrewTalk:
        {
            // Each pirate with lines, talked to twice: the first conversation is the 2D game's *_first lines, word for
            // word, the second its *_repeat lines (they stay the repeat ones after).
            Limit = 90;
            struct FTalker { EDockHuman Kind; TArray<FString> First, Repeat; };
            static const TArray<FTalker> Talkers = {
                {EDockHuman::PirateSeated, {TEXT("Well I'll be. Never seen a rat wearin' a coat before.")}, {TEXT("You seem like a good listener, I bet all of the crew would love to bend your ear")}},
                {EDockHuman::PirateConcertina, {TEXT("Requests are considered in the order they are shouted.")}, {TEXT("Still considering it.")}},
                {EDockHuman::PirateCheering, {TEXT("Don't mind Jeffries over there tied to the mast. He's gone mad. Too much sun, I think.")},
                    {TEXT("You're not smoking the captain's cigarettes, are you!?"), TEXT("Don't let the captain catch you sniffing around in his quarters!")}},
                {EDockHuman::PirateDancer, {TEXT("Deck's moving. Saves me half the work.")}, {TEXT("Other half's mine.")}},
                {EDockHuman::Jeffries, {TEXT("The sea is wrong, the land is wrong. Open your eyes!"), TEXT("Half the crew is gone... lost to the purple. They don't even remember them.")},
                    {TEXT("Don't go into the purple! DON'T GO INTO THE PURPLE!"), TEXT("Why don't they see!?")}}};
            const AShipCrew* Crew = AShipCrew::Get(World);
            if (!Crew || Run->Point >= Talkers.Num())
            {
                Done = true;
                Run->bPass = Crew && Run->TalkChecks == Talkers.Num() * 2 && Run->TalkOk == Run->TalkChecks && NewDeaths == 0;
                break;
            }
            const FTalker& T = Talkers[Run->Point];
            ADockNPC* N = Crew->Find(T.Kind);
            if (!N) { ++Run->TalkChecks; ++Run->Point; break; }
            if (Run->Sub == 0)
            {
                // Before him (beside the seated one, who faces his table), turned to him.
                const FVector To = T.Kind == EDockHuman::PirateSeated ? -N->GetActorRightVector() : N->GetActorForwardVector();
                const FVector Feet = N->GetActorLocation() - FVector(0, 0, 90.f) + To * 85.f;
                Chuck->ResetAtLocation(Feet + FVector(0, 0, Stand + 2.f));
                Chuck->SetActorRotation(FRotator(0, (N->GetActorLocation() - Feet).Rotation().Yaw, 0));
                Stop(); Run->Sub = 1; Run->At = Now; Run->Heard.Reset(); Run->Pressed = 0;
                break;
            }
            if (Time < .6f || Now - Run->Pressed < .3) break;
            if (Chuck->IsTalking())
            {
                FString Speaker, Text;
                if (Chuck->GetDialogue(Speaker, Text) && (Run->Heard.IsEmpty() || Run->Heard.Last() != Text)) Run->Heard.Add(Text);
                Chuck->Interact(); Run->Pressed = Now;
            }
            else if (Run->Heard.IsEmpty())
            {
                Chuck->Interact(); Run->Pressed = Now;
                if (!Chuck->IsTalking() && Time > 3.f) { ++Run->TalkChecks; UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_CREW_TALK %s no_prompt=1"), *N->DisplayName); Run->Sub = 0; ++Run->Point; }
            }
            else
            {
                // A conversation over: what he heard against the lines it should have been.
                const TArray<FString>& Want = Run->Sub == 1 ? T.First : T.Repeat;
                const bool bOk = Run->Heard == Want && N->GetConversations() == Run->Sub;
                ++Run->TalkChecks; Run->TalkOk += bOk;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CREW_TALK %s %s ok=%d lines=%d conversations=%d first_heard=\"%s\""), *N->DisplayName, Run->Sub == 1 ? TEXT("first") : TEXT("repeat"),
                    bOk, Run->Heard.Num(), N->GetConversations(), Run->Heard.Num() ? *Run->Heard[0] : TEXT(""));
                if (Run->Sub == 1 && Run->Point == 0) Shot(TEXT("CrewTalk"));
                Run->Heard.Reset();
                if (Run->Sub == 1) { Run->Sub = 2; Run->At = Now; }
                else { Run->Sub = 0; ++Run->Point; }
            }
            break;
        }
        case ET::ChefChase:
        {
            // Into the galley: the chef sees him, calls out, and comes after him with the cleaver. Under the work table he
            // can't be reached; the chef gives up and goes back to his work; then out of the galley the way he came.
            Limit = 60;
            const AShipCrew* Crew = AShipCrew::Get(World);
            ADockNPC* C = Crew ? Crew->Find(EDockHuman::PirateChef) : nullptr;
            if (!C) { Done = true; break; }
            if (Time < .5f) break;
            const FString State = C->GetChefStateName();
            if (Run->Sub == 0) { Run->Count0 = C->GetChefNotices(); Run->Sub = 1; Run->Point = 0; }
            else if (Run->Sub == 1) { if (Route({{290, GalleyDoorY}, {450, 150}}, 14)) { Run->Sub = 2; Run->At = Now; } }
            else if (Run->Sub == 2)
            {
                Stop();
                if (C->IsShouting()) Run->Seen |= 1;
                if (State == TEXT("Chase")) { Run->Seen |= 2; Run->Sub = 3; Run->Point = 0; Run->At = Now; Shot(TEXT("ChefChase")); }
                else if (Time > 5) Done = true;
            }
            else if (Run->Sub == 3)
            {
                if (C->IsShouting()) Run->Seen |= 1;
                Chuck->SetRunHeld(true);
                if (Route({{520, 90}, {600, 40}, {680, 0}}, 12)) { Stop(); Chuck->SetRunHeld(false); Run->Sub = 4; Run->At = Now; Run->Count1 = C->GetChefHits(); }
                else if (Time > 8) Done = true;
            }
            else if (Run->Sub == 4)
            {
                // Under the table: nothing reaches him; the chef gives up.
                Stop();
                if (Time > 1.f && Time < 1.1f) Shot(TEXT("ChefUnderTable"));
                if (State == TEXT("Lost") || State == TEXT("Return") || State == TEXT("Work")) Run->Seen |= 4;
                if (C->GetChefHits() != Run->Count1) Run->Seen |= 8;   // hit under the table: a failure
                if (Run->Seen & 4 && Time > 9.f) { Run->Sub = 5; Run->Point = 0; Run->At = Now; }
                else if (Time > 14) Done = true;
            }
            else
            {
                // Out from under, back through the galley door into the arrival compartment.
                Chuck->SetRunHeld(true);
                if (Route({{560, 120}, {450, 150}, {340, GalleyDoorY}, {250, 120}, {150, 60}}, 14))
                {
                    Done = true; Chuck->SetRunHeld(false);
                    Run->bPass = C->GetChefNotices() >= 1 && (Run->Seen & 6) == 6 && !(Run->Seen & 8) && NewDeaths == 0 && !Chuck->IsAstral()
                        && ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Arrival);
                }
                else if (Time > 15) Done = true;
            }
            if (Done) UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CHEF notices=%d swings=%d hits=%d seen=%d state=%s sanity=%d"), C->GetChefNotices(), C->GetChefSwings(), C->GetChefHits(), Run->Seen, *State, Chuck->GetSanity());
            break;
        }
        case ET::Fencers:
        {
            // Stood in a fencer's stance he is trodden on (Sanity lost); well off their line nothing reaches him. Their ground
            // and the crew's bodies stay inside the hull.
            Limit = 30;
            AShipCrew* Crew = AShipCrew::Get(World);
            if (!Crew) { Done = true; break; }
            if (Run->Sub == 0)
            {
                FVector Back, Front;
                const ADockNPC* A = Crew->Find(EDockHuman::PirateFencerA);
                // In front of his front foot, where it lands as he advances or lunges.
                const FVector At = A && A->GetFeet(Back, Front) ? Front + A->GetActorForwardVector() * 28.f : Crew->GetFencePair();
                Chuck->ResetAtLocation(FVector(At.X, At.Y, Crew->GetFencePair().Z) + FVector(0, 0, Stand + 2.f)); Stop();
                Run->Count0 = Crew->GetFencerHits(); Run->Sub = 1; Run->At = Now;
            }
            else if (Run->Sub == 1)
            {
                Stop();
                if (Chuck->GetSanity() < 2) Chuck->SetSanity(AChuckCharacter::MaxSanity);
                if (Time > 1.f && Time < 1.1f) Shot(TEXT("Fencers"));
                if (Time > 7.f)
                {
                    Run->Count1 = Crew->GetFencerHits() - Run->Count0;
                    const FVector Pair = Crew->GetFencePair();
                    const FVector Off = ShipWorld(FVector(ShipLocal(Pair).X, ShipLocal(Pair).Y > 0 ? -270.f : 270.f, DeckZ));
                    Chuck->SetSanity(AChuckCharacter::MaxSanity);
                    Chuck->ResetAtLocation(Off + FVector(0, 0, Stand + 2.f)); Stop();
                    Run->Count0 = Crew->GetFencerHits(); Run->Sub = 2; Run->At = Now;
                }
            }
            else
            {
                Stop();
                if (Time > 7.f)
                {
                    const int32 Far = Crew->GetFencerHits() - Run->Count0;
                    Done = true;
                    Run->bPass = Run->Count1 >= 1 && Far == 0 && Crew->GetWorstFenceOut() < 5.f && Crew->GetWorstHullOut() <= 0.f && !Chuck->IsAstral();
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_FENCERS near_hits=%d far_hits=%d fence_out=%.1f hull_out=%.1f parry_gap=%.1f phrases=%d"), Run->Count1, Far,
                        Crew->GetWorstFenceOut(), Crew->GetWorstHullOut(), Crew->GetLastParryGap(), Crew->GetFencePhrases());
                }
            }
            break;
        }
        case ET::Rail:
        {
            // Walking hard at the rail between its posts: it holds him on deck.
            Limit = 6;
            if (Run->Sub == 0) { Chuck->ResetAtLocation(SW(300, 300, DeckZ + Stand)); Run->Sub = 1; Run->At = Now; break; }
            Push(FVector(0, 1, 0));
            if (Time > 3)
            {
                Done = true;
                Run->bPass = NewDeaths == 0 && Ground && L.Y < ShipInnerHalf(300, DeckZ + 20) && ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck);
            }
            break;
        }
        default: break;
        }
        if (!Done && Time < Limit) return;
        Stop(); Chuck->SetRunHeld(false);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TRIAL name=%s pass=%d seconds=%.1f sub=%d point=%d deaths=%d mounts=%d gait=%s space=%s at=%s"),
            Names[int32(Trial)], Run->bPass, Time, Run->Sub, Run->Point, NewDeaths, Chuck->GetLadderMounts() - Run->Mounts0, Chuck->GetGaitName(),
            ShipSpaceName(ShipSpaceAt(Chuck->GetActorLocation())), *L.ToString());
        Run->Results.Add(Run->bPass ? 1 : 0); Run->bStarted = false; Run->Next = Now + 1.0;
    }, .016f, true);
}
