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
#include "ShipStorm.h"
#include "CigarettePickup.h"
#include "DockGameMode.h"
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
        {TEXT("Shrouds"), {-250, 140, DeckZ + 120}, {MainMastX - 50, 330, 1100}},
        {TEXT("ShroudsOutside"), {-300, 1100, 900}, {MainMastX - 50, 300, 1150}},
        {TEXT("ShroudsTop"), {MainMastX - 60, 330, MainTopZ + 120}, {MainMastX - 50, 120, MainTopZ - 60}},
        {TEXT("ForeShroudsStarboard"), {ForeMastX - 60, 1100, 1000}, {ForeMastX - 50, 300, 1050}},
        {TEXT("ForeShroudsPort"), {ForeMastX - 300, -900, 900}, {ForeMastX - 50, -300, 1000}},
        {TEXT("NetFoot"), {MainMastX + 120, 180, DeckZ + 110}, {MainMastX - 50, 420, DeckZ + 80}},
        {TEXT("NetFootLow"), {MainMastX - 230, 250, DeckZ + 35}, {MainMastX - 50, 420, DeckZ + 70}},
        {TEXT("NetFootFore"), {ForeMastX + 150, -200, DeckZ + 120}, {ForeMastX - 50, -420, DeckZ + 90}},
        {TEXT("TopsailYard"), {MainMastX - 250, 500, MainTopZ + 700}, {MainMastX + 63, 250, MainTopZ + 740}},
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
        {TEXT("CrewDancerFace"), {79, 113, DeckZ + 160}, {215, 92, DeckZ + 145}},
        {TEXT("CrewCheering"), {0, 345, DeckZ + 150}, {100, 215, DeckZ + 150}},
        {TEXT("CrewDancer"), {-60, 150, DeckZ + 140}, {215, 92, DeckZ + 75}},
        {TEXT("CrewDancerLow"), {40, 200, DeckZ + 35}, {215, 92, DeckZ + 60}},
        {TEXT("CrewJeffries"), {-720, 90, DeckZ + 160}, {-534, 0, DeckZ + 130}},
        {TEXT("CrewJeffriesRat"), {-660, 50, DeckZ + 45}, {-534, 0, DeckZ + 140}},
        {TEXT("CrewJeffriesBack"), {-380, -150, DeckZ + 170}, {-534, 0, DeckZ + 120}},
        {TEXT("CrewFencers"), {875, 340, DeckZ + 170}, {875, 0, DeckZ + 100}},
        {TEXT("CrewFencersLow"), {680, -240, DeckZ + 45}, {880, 0, DeckZ + 120}},
        {TEXT("CrewSeated"), {-300, -40, LowerZ + 140}, {-465, -139, LowerZ + 105}},
        {TEXT("CrewChef"), {470, 140, LowerZ + 160}, {680, -115, LowerZ + 110}},
        // Their faces close to (the mustaches, user 2026-11-02).
        {TEXT("FaceConcertina"), {166, -116, DeckZ + 158}, {215, -165, DeckZ + 152}},
        {TEXT("FaceCheering"), {120, 95, DeckZ + 165}, {100, 215, DeckZ + 160}},
        {TEXT("FaceDancer"), {150, 100, DeckZ + 152}, {215, 92, DeckZ + 147}},
        {TEXT("FaceJeffries"), {-604, 0, DeckZ + 158}, {-534, 0, DeckZ + 154}},
        {TEXT("FaceSeated"), {-372, -166, LowerZ + 118}, {-465, -139, LowerZ + 112}},
        {TEXT("FaceChef"), {690, 15, LowerZ + 150}, {680, -108, LowerZ + 146}},
        // The captain on the quarterdeck (user 2026-11-02): fists on his hips, then his cutlass held up (each still sets his round).
        {TEXT("CaptainHips"), {-925, -20, QuarterZ + 95}, {-1065, -45, QuarterZ + 60}},
        {TEXT("CaptainHipsSide"), {-1050, -300, QuarterZ + 95}, {-1065, -45, QuarterZ + 55}},
        {TEXT("CaptainSword"), {-925, 40, QuarterZ + 110}, {-1065, -10, QuarterZ + 80}},
        {TEXT("CaptainSwordSide"), {-1055, 290, QuarterZ + 100}, {-1065, -25, QuarterZ + 70}},
        {TEXT("CaptainFace"), {-1005, -30, QuarterZ + 100}, {-1065, -45, QuarterZ + 95}},
        {TEXT("CaptainBoots"), {-985, -170, QuarterZ + 30}, {-1065, -45, QuarterZ + 15}},
        {TEXT("CaptainFromDeck"), {-640, -40, DeckZ + 45}, {-1065, -45, QuarterZ + 90}},
        // His hand taking the planted cutlass and drawing it (user 2026-11-02): reaching, gripping, drawing, lifting.
        {TEXT("CaptainReach"), {-975, 110, QuarterZ + 85}, {-1050, -20, QuarterZ + 55}},
        {TEXT("CaptainGrip"), {-975, 110, QuarterZ + 85}, {-1050, -20, QuarterZ + 55}},
        {TEXT("CaptainDraw"), {-975, 110, QuarterZ + 85}, {-1050, -20, QuarterZ + 60}},
        {TEXT("CaptainLift"), {-975, 110, QuarterZ + 90}, {-1055, -20, QuarterZ + 75}},
        {TEXT("CaptainGripClose"), {-1010, 45, QuarterZ + 70}, {-1048, -22, QuarterZ + 55}},
        {TEXT("CaptainGripBack"), {-1110, 50, QuarterZ + 75}, {-1050, -22, QuarterZ + 55}},
        {TEXT("CaptainHandReach"), {44, 28, 14}, {0, 0, -3}},
        {TEXT("CaptainHandGrip"), {44, 28, 14}, {0, 0, -3}},
        {TEXT("CaptainHandGripOut"), {8, 55, 8}, {0, 0, -3}},
        {TEXT("CaptainHandGripFront"), {55, -12, 10}, {0, 0, -3}},
        {TEXT("CaptainHandDraw"), {44, 28, 14}, {0, 0, 0}},
        {TEXT("CaptainHandUp"), {48, 30, 4}, {0, 0, 8}},
        {TEXT("CaptainHandUpOut"), {6, 58, 0}, {0, 0, 8}},
        {TEXT("CaptainHandLetGo"), {44, 28, 14}, {0, 0, -3}},
        // The flag at the spanker gaff's peak (user 2026-11-02: more prominent): the mustachioed skull and crossed cutlasses.
        {TEXT("Flag"), {-400, 450, 1500}, {-1030, 200, 1590}},
        {TEXT("FlagAft"), {-2100, 700, 1250}, {-1030, 200, 1590}},
        {TEXT("FlagFromDeck"), {-150, 260, DeckZ + 160}, {-1030, 200, 1590}},
        {TEXT("FlagFromWaist"), {200, -250, DeckZ + 150}, {-1030, 200, 1590}},
        // The lumber in the arrival compartment and the masonry's mouth in it (user 2026-11-02).
        {TEXT("ArrivalPile"), {250, -60, LowerZ + 150}, {-20, -330, LowerZ + 70}},
        {TEXT("ArrivalMouth"), {260, -270, LowerZ + 60}, {SeamX1, -300, LowerZ + 40}},
        // By the port break ladder, where a hammock's lanyard stood up through the deck (user 2026-11-02: "floating mop").
        {TEXT("BreakPort"), {-700, -120, DeckZ + 120}, {-880, -230, DeckZ + 70}},
        {TEXT("QuartersAft"), {-300, 0, LowerZ + 150}, {-900, 0, LowerZ + 110}},
        // The rope (user 2026-11-08): Jeffries' lashings round him and the mast, the rigging in the rope scan, the coil that floated.
        {TEXT("JeffriesRopes"), {-615, 35, DeckZ + 125}, {-530, 0, DeckZ + 115}},
        {TEXT("JeffriesRopesSide"), {-548, 95, DeckZ + 120}, {-515, 0, DeckZ + 110}},
        {TEXT("JeffriesWrists"), {-508, -78, DeckZ + 95}, {-522, 0, DeckZ + 95}},
        {TEXT("JeffriesWristsHigh"), {-545, -70, DeckZ + 150}, {-515, -12, DeckZ + 92}},
        {TEXT("JeffriesMastFore"), {-395, 30, DeckZ + 125}, {-490, 0, DeckZ + 110}},
        {TEXT("RopeDeadeyes"), {-450, 210, DeckZ + 150}, {-520, 380, DeckZ + 110}},
        {TEXT("RopeRatlines"), {-560, 240, DeckZ + 260}, {-560, 400, DeckZ + 320}},
        {TEXT("RopeStays"), {-200, -120, DeckZ + 300}, {300, 0, DeckZ + 700}},
        {TEXT("ArrivalCoil"), {120, 200, LowerZ + 110}, {-40, 350, LowerZ + 10}}};
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(74);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }

    // (The chest and the storm come last: the crew's trials meet six of the ending's seven conditions, the chest the last.)
    enum class ET : uint8 { Galley, Cabin, LadderDown, LadderUp, Sea, Seam, DeckUp, DeckDown, Quarterdeck, Rigging, YardDrop, Rail, Ratlines, MastClimb, CrewTalk, ChefChase, Fencers, Sounds, Chest, Storm, StormWork };
    static const ET AllTrials[] = {ET::Galley, ET::Cabin, ET::LadderDown, ET::LadderUp, ET::Sea, ET::Seam, ET::DeckUp, ET::DeckDown, ET::Quarterdeck, ET::Rigging, ET::YardDrop, ET::Rail,
        ET::Ratlines, ET::MastClimb, ET::CrewTalk, ET::ChefChase, ET::Fencers, ET::Sounds, ET::Chest, ET::Storm, ET::StormWork};
    static const TCHAR* Names[] = {TEXT("galley"), TEXT("cabin"), TEXT("ladder_down"), TEXT("ladder_up"), TEXT("sea"), TEXT("seam"),
        TEXT("deck_up"), TEXT("deck_down"), TEXT("quarterdeck"), TEXT("rigging"), TEXT("yard_drop"), TEXT("rail"), TEXT("ratlines"), TEXT("mast_climb"), TEXT("crew_talk"), TEXT("chef_chase"), TEXT("fencers"), TEXT("sounds"),
        TEXT("chest"), TEXT("storm"), TEXT("storm_work")};
    struct FRun
    {
        int32 Phase = 0, Trial = -1, Sub = 0, Point = 0, Failures = 0, Deaths0 = 0, Mounts0 = 0, Crawls0 = 0, Shots = 0, ShipFailures = -1, Catches0 = 0, Leaps0 = 0;
        double Next = 0, Start = 0, At = -1, EscapeAt = -1;
        bool bStarted = false, bPass = false, bDone = false, bEscaped = false, bViews = false, bMount = false, bTop = false;
        TArray<int32> Results; TArray<ET> Trials; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer;
        int32 EscapeOk = 0;
        // The crew's trials (milestone 3): what he heard, and counts at the start of a stage.
        TArray<FString> Heard; double Pressed = 0; int32 TalkOk = 0, TalkChecks = 0, Count0 = 0, Count1 = 0, Seen = 0;
        // The music's change of track at the companion (2026-11-02): when he arrived, and the switches before.
        double Arrived = -1; int32 Switches0 = 0;
        int32 FilmFrame = -1;   // -ChuckFenceFilm's stills taken
        // The mast climb (2026-11-08): the runs up past the topsail yards that reached the truck, and the highest of this run.
        int32 MastOk = 0; float MastTop = 0;
        // The sounds trial (2026-11-03): what was heard on deck.
        int32 DeckClash = 0, DeckFeet = 0, DeckRatsHeard = 0, DeckRatsHushed = 0, Creaks0 = 0; bool DeckMix = false;
        // The storm trial (2026-11-03): its stills, and what it measured on deck.
        int32 StormShot = 0, Strikes0 = 0, Thunders0 = 0, Splashes0 = 0, Sprays0 = 0, MaxDrops = 0; float MaxRoll = 0, WorstMs = 0; bool StormDeck = false;
        // The storm's work, rupture and second part (2026-11-04).
        int32 WorkShot = 0, Stations = 0, StationsOf = 0, Shoves0 = 0, Waves0 = 0, MaxPuffs = 0; float WorstWorkGrip = 0, WorstRopeGap = 0, WorstRopeTurn = 0, HelmMin = 0, HelmMax = 0, IntenseRoll = 0;
        TMap<FString, float> GripBy;
        bool Music31 = false, Music32 = false, RuptureDeath = false, Shoved = false; double WaveAt = -1;
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
                if (FString(Views[I].Name).StartsWith(TEXT("Captain")))   // his round: hips, or the cutlass up
                    if (const AShipCrew* Crew = AShipCrew::Get(World))
                        if (ADockNPC* Cap = Crew->Find(EDockHuman::PirateCaptain))
                        {
                            const FString V = Views[I].Name;
                            static const TPair<const TCHAR*, float> Times[] = {{TEXT("Sword"), 19.f}, {TEXT("Reach"), 16.15f}, {TEXT("Grip"), 16.7f},
                                {TEXT("Draw"), 16.95f}, {TEXT("Lift"), 17.55f}, {TEXT("HandUp"), 19.f}, {TEXT("LetGo"), 26.05f}};
                            float At = 6.f;
                            for (const auto& T : Times) if (V.Contains(T.Key)) At = T.Value;
                            Cap->SetCaptainTime(At, 1.6f);
                        }
                if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Run->Camera.Get());
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(SW(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z), (SW(Views[I].Aim.X, Views[I].Aim.Y, Views[I].Aim.Z) - SW(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z)).Rotation());
                ++Run->Phase; return;
            }
            // The captain's hand close to: these views' eye and aim are offsets from his right hand, as it is now.
            if (FString(Views[I].Name).StartsWith(TEXT("CaptainHand")) && Run->Camera.IsValid())
                if (const AShipCrew* Crew = AShipCrew::Get(World))
                    if (const ADockNPC* Cap = Crew->Find(EDockHuman::PirateCaptain))
                    {
                        const FVector H = Cap->GetHandWorld(1);
                        Run->Camera->SetActorLocationAndRotation(H + Views[I].Eye, (Views[I].Aim - Views[I].Eye).Rotation());
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
                // The captain: there, his planted cutlass's tip in the deck, his hand on its grip while he held it up.
                const ADockNPC* Cap = Crew->Find(EDockHuman::PirateCaptain);
                const bool bCaptain = Cap && Cap->HasMocap() && Cap->GetPirateProps() == 1 && Cap->GetCaptainTipDepth() > 1.f && Cap->GetCaptainTipDepth() < 8.f && Cap->GetWorstPropGrip() < 8.f;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CAPTAIN pass=%d phase=%s tip_depth_cm=%.1f grip_cm=%.1f"), bCaptain, Cap ? Cap->GetCaptainPhase() : TEXT("none"),
                    Cap ? Cap->GetCaptainTipDepth() : 0.f, Cap ? Cap->GetWorstPropGrip() : 0.f);
                Run->Failures += !bCaptain;
            }
        }
        // -ChuckFenceFilm: the fencers from the side, a still every 0.1 s for 9 s, to review their fight frame by frame.
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckFenceFilm")) && Run->FilmFrame < 90)
        {
            const AShipCrew* Crew = AShipCrew::Get(World);
            if (!Crew || Now < Run->Next) return;
            if (Run->FilmFrame < 0)
            {
                Chuck->SetActorHiddenInGame(true);
                if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Run->Camera.Get());
                Run->FilmFrame = 0;
            }
            else
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Ship") / TEXT("Film") / FString::Printf(TEXT("Fence_%03d.png"), Run->FilmFrame), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FENCE_FILM frame=%d beat=%.2f gap=%.1f"), Run->FilmFrame, Crew->GetBeat(), Crew->GetLastParryGap());
                ++Run->FilmFrame;
            }
            Run->Next = Now + .1;
            // From the side of their line, on the ship's middle side of them, the camera keeping its place once set.
            const ADockNPC* A = Crew->Find(EDockHuman::PirateFencerA);
            const ADockNPC* B = Crew->Find(EDockHuman::PirateFencerB);
            if (A && B && Run->Camera.IsValid() && Run->FilmFrame == 0)
            {
                const FVector Mid = (ShipLocal(A->GetActorLocation()) + ShipLocal(B->GetActorLocation())) * .5;
                const FVector Line = (ShipLocal(B->GetActorLocation()) - ShipLocal(A->GetActorLocation())).GetSafeNormal2D();
                FVector Side(-Line.Y, Line.X, 0.);
                if (FMath::Abs((Mid + Side * 330.).Y) > FMath::Abs((Mid - Side * 330.).Y)) Side = -Side;
                const FVector Eye = ShipWorld(FVector(Mid.X, Mid.Y, DeckZ + 130) + Side * 330.), Aim = ShipWorld(FVector(Mid.X, Mid.Y, DeckZ + 105));
                Run->Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
            }
            return;
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
                // Jeffries' lashings lie round him: no part of him more than a little inside a rope (2026-11-08).
                float RopeDepth = -100.f;
                for (TActorIterator<ADockNPC> It(World); It; ++It) if (It->GetKind() == EDockHuman::Jeffries) RopeDepth = FMath::Max(RopeDepth, It->GetRopeDepth());
                const bool bRopeOk = RopeDepth > -99.f && RopeDepth < .6f;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_JEFFRIES_ROPE depth_cm=%.2f pass=%d"), RopeDepth, bRopeOk ? 1 : 0);
                const int32 Fails = Run->Failures + (Run->Results.Num() - Passed) + !bRopeOk;
                int32 LiveRats = 0; for (const auto& R : AEnemyRat::All()) if (R.IsValid() && !R->IsDead() && InShip(R->GetActorLocation())) ++LiveRats;
                const AShipSetting* S = AShipSetting::Get(World);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TEST failures=%d trials=%d passed=%d ship_check=%d rats=%d jars=%d ladders=%d missing=%d tris=%d"), Fails, Run->Results.Num(), Passed, Run->ShipFailures,
                    LiveRats, S ? S->GetJarsPlaced() : -1, S ? S->GetLadders() : -1, S ? S->GetMissingMaterials() : -1, S ? S->GetTriangles() : -1);
                Run->bDone = true; FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0); return;
            }
            Run->bStarted = true; Run->Sub = 0; Run->Point = 0; Run->At = Now; Run->bPass = false; Run->bMount = false;
            Run->Deaths0 = Chuck->GetFallDeaths(); Run->Mounts0 = Chuck->GetLadderMounts(); Run->Crawls0 = Chuck->GetCrawls();
            Run->Catches0 = Chuck->GetSwingCatches(); Run->Leaps0 = Chuck->GetSwingLeaps(); Run->bTop = false; Run->Arrived = -1;
            switch (Run->Trials[Run->Trial])
            {
            case ET::Galley: case ET::Sea: case ET::Seam: Reset(EShipSpawn::Arrival); break;
            case ET::Cabin: Reset(EShipSpawn::Quarters); break;
            case ET::LadderDown: Reset(EShipSpawn::Arrival); break;
            case ET::LadderUp: Reset(EShipSpawn::Hold); break;
            case ET::DeckUp: Reset(EShipSpawn::Quarters); break;
            case ET::DeckDown: case ET::Quarterdeck: case ET::Rigging: case ET::Rail: case ET::Ratlines: Reset(EShipSpawn::Deck); break;
            case ET::YardDrop: Reset(EShipSpawn::Deck); Chuck->ResetAtLocation(SW(MainYardX, 70, MainTopZ + Stand)); Chuck->SetActorRotation(FRotator(0, 90, 0)); break;
            case ET::MastClimb: Reset(EShipSpawn::Deck); Run->MastOk = 0; break;
            case ET::CrewTalk: case ET::Fencers: case ET::Sounds: Reset(EShipSpawn::Deck); break;
            case ET::ChefChase: Reset(EShipSpawn::Arrival); break;
            case ET::Chest: Reset(EShipSpawn::Cabin); break;
            case ET::Storm: Reset(EShipSpawn::Deck); Run->StormShot = 0; break;
            case ET::StormWork: Reset(EShipSpawn::Deck); Run->WorkShot = 0; Run->WorstWorkGrip = 0; Run->WorstRopeGap = Run->WorstRopeTurn = 0; Run->HelmMin = Run->HelmMax = 0; Run->IntenseRoll = 0; Run->MaxPuffs = 0;
                Run->Music31 = Run->Music32 = Run->RuptureDeath = Run->Shoved = false; Run->WaveAt = -1; break;
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
        // Up on deck the concertina version must have taken over, below the ship track (each at the tune's matching moment).
        const ADockGameMode* GM = World->GetAuthGameMode<ADockGameMode>();
        auto MusicFollows = [&](ShipScore::ETrack Want) -> bool
        {
            const int32 Switches = GM ? GM->GetShipMusicSwitches() : 0;
            const bool bOk = ShipScore::IsPlaying() && ShipScore::Current() == Want && Switches > Run->Switches0;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_MUSIC_FOLLOWS pass=%d want=%s switches=%d position_s=%.2f concertina_near=%.2f"), bOk,
                Want == ShipScore::ETrack::Concertina ? TEXT("Concertina") : TEXT("Ship"), Switches - Run->Switches0, ShipScore::Position(World), GM ? GM->GetConcertinaNear() : 0.f);
            return bOk;
        };
        switch (Trial)
        {
        case ET::DeckUp:
        {
            // From the crew quarters into the companion ladder's foot (aft of it, walking forward), up onto the weather deck.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(CompX1 - 60, (CompY0 + CompY1) * .5f, 10)) { Run->Sub = 1; Run->Switches0 = GM ? GM->GetShipMusicSwitches() : 0; } }
            else if (Run->Sub == 1)
            {
                Push(FVector(1, 0, 0));
                if (Chuck->IsOnLadder() && L.Z > LowerZ + 150 && !Run->bMount) { Run->bMount = true; Shot(TEXT("CompanionClimb")); }
                if (Ground && !Chuck->IsOnLadder() && L.Z > DeckZ)
                {
                    Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0; Shot(TEXT("CompanionTop"));
                    Stop(); Run->Sub = 2; Run->Arrived = Now;
                }
            }
            else if (Now - Run->Arrived > 1.5) { Done = true; Run->bPass = MusicFollows(ShipScore::ETrack::Concertina) && Run->bPass; }
            break;
        }
        case ET::DeckDown:
        {
            // From the deck to the companion's fore edge, aft over its lip onto the ladder, down into the crew quarters.
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Steer(CompX1 + 30, (CompY0 + CompY1) * .5f, 12)) { Run->Sub = 1; Run->Switches0 = GM ? GM->GetShipMusicSwitches() : 0; } }
            else if (Run->Sub == 1) { Push(FVector(-1, 0, 0), .85f); if (Chuck->IsOnLadder()) Run->Sub = 2; }
            else if (Run->Sub == 2)
            {
                if (Chuck->IsOnLadder()) Push(FVector(-1, 0, 0)); else Stop();
                if (!Chuck->IsOnLadder() && Ground && L.Z < DeckZ - 100)
                {
                    Run->bPass = ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Quarters) && NewDeaths == 0 && Chuck->GetLadderMounts() > Run->Mounts0;
                    Stop(); Run->Sub = 3; Run->Arrived = Now;
                }
            }
            else if (Now - Run->Arrived > 1.5) { Done = true; Run->bPass = MusicFollows(ShipScore::ETrack::Ship) && Run->bPass; }
            break;
        }
        case ET::Quarterdeck:
        {
            // Up the starboard ladder at the break, across the quarterdeck by the helm, down the port ladder.
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
            // Up the starboard main shrouds' ratlines (out over the rail, round onto their outer face) and over the top's rim;
            // off its starboard side onto the main yard; hand over hand out to the yardarm; let go, into the sea; back on deck.
            Limit = 80;
            if (Time < .5f) break;
            if (Run->Sub == 0) { if (Route({{-160, 40}, {-430, 40}, {-430, 330}, {MainMastX - 50, 330}}, 10)) Run->Sub = 1; }
            else if (Run->Sub == 1)
            {
                Push(FVector(0, 1, 0));
                if (Chuck->IsOnLadder() && L.Z > DeckZ + 500 && !Run->bMount) { Run->bMount = true; Shot(TEXT("ShroudsClimb")); }
                if (Ground && !Chuck->IsOnLadder() && L.Z > MainTopZ) { Run->Sub = 2; Run->Point = 0; Run->bTop = true; Shot(TEXT("OnTheTop")); }
            }
            else if (Run->Sub == 2) { if (Route({{MainMastX - 20, 95}, {MainYardX, 70}}, 8)) { Run->Sub = 3; Run->At = Now; } }
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
        case ET::Ratlines:
        {
            // The ratlines (2026-11-05): up the port fore shrouds from the net at their foot on deck, across them both
            // ways, down and off at the deck (not straight back on, the stick still held out); from the main top out over
            // the rim onto the lower shrouds' heads, lowered onto them, down to the deck; from the main top up its starboard
            // topmast shrouds, up onto the topsail yard, three leaps out along it, let go onto the deck.
            Limit = 90;
            if (Time < .5f) break;
            const float OnX = static_cast<float>(L.X);
            if (Run->Sub == 0) { if (Route({{-80, -280}, {ForeMastX - 50, -280}, {ForeMastX - 50, -330}}, 10)) { Run->Sub = 1; Run->At = Now; } }
            else if (Run->Sub == 1)
            {
                Push(FVector(0, -1, 0));
                if (Chuck->IsOnLadder() && L.Z > DeckZ + 250) { Stop(); Run->Sub = 2; Run->At = Now; Run->Count0 = int32(OnX * 10); }
                else if (Time > 15) Done = true;
            }
            else if (Run->Sub == 2) { Push(FVector(1, 0, 0)); if (Time > .8f) { Run->Count1 = int32(OnX * 10); Run->Sub = 3; Run->At = Now; Shot(TEXT("RatlinesAcross")); } }
            else if (Run->Sub == 3)
            {
                Push(FVector(-1, 0, 0));
                if (Time > 1.6f)
                {
                    const float X0 = Run->Count0 * .1f, X1 = Run->Count1 * .1f;
                    Run->bMount = X1 - X0 > 15 && X1 - OnX > 35 && Chuck->IsOnLadder();   // (the net narrows upward: about 34 cm either way here)
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RATLINES_ACROSS ok=%d right=%.1f left=%.1f z=%.1f"), Run->bMount, X1 - X0, X1 - OnX, L.Z);
                    Stop(); Run->Sub = 4; Run->At = Now;
                }
            }
            else if (Run->Sub == 4)
            {
                // Down: away from the net (out over the side), held on through the turn at the rail, off at its foot.
                Push(FVector(0, -1, 0));
                if (!Chuck->IsOnLadder() && Ground && L.Z < DeckZ + 60 && Run->Arrived < 0) Run->Arrived = int32(Time * 100);
                if (Run->Arrived >= 0 && Time > Run->Arrived * .01f + 1.2f)
                {
                    const bool bOff = !Chuck->IsOnLadder() && Ground && L.Z < DeckZ + 60 && FMath::Abs(L.Y) < ShipInnerHalf(OnX, DeckZ + 20);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RATLINES_OFF ok=%d at=%s"), bOff, *L.ToString());
                    Run->bMount = Run->bMount && bOff;
                    Stop(); Chuck->ResetAtLocation(SW(MainMastX + 20, 60, MainTopZ + Stand)); Chuck->SetActorRotation(FRotator(0, 90, 0));
                    Run->Sub = 5; Run->At = Now; Run->Arrived = -1; Run->Point = 0;
                }
                else if (Time > 15) Done = true;
            }
            else if (Run->Sub == 5) { if (Route({{MainMastX - 50, 70}, {MainMastX - 51, 80}}, 6)) { Run->Sub = 6; Run->At = Now; } }
            else if (Run->Sub == 6)
            {
                // Out over the rim at the lower shrouds' heads: lowered onto them, down to the deck.
                Push(FVector(0, 1, 0));
                if (Chuck->IsOnLadder() && L.Z < MainTopZ - 200 && Run->Arrived < 0) { Run->Arrived = 0; Shot(TEXT("ShroudsDown")); }
                if (!Chuck->IsOnLadder() && Ground && L.Z < DeckZ + 60)
                {
                    Run->bMount = Run->bMount && FMath::Abs(L.Y) < ShipInnerHalf(OnX, DeckZ + 20);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RATLINES_DOWN ok=%d at=%s"), Run->bMount ? 1 : 0, *L.ToString());
                    Stop(); Chuck->ResetAtLocation(SW(MainMastX, 60, MainTopZ + Stand)); Chuck->SetActorRotation(FRotator(0, 90, 0));
                    Run->Sub = 7; Run->At = Now; Run->Arrived = -1; Run->Catches0 = Chuck->GetSwingCatches(); Run->Leaps0 = Chuck->GetSwingLeaps();
                }
                else if (Time > 25) Done = true;
            }
            else if (Run->Sub == 7)
            {
                // Up the starboard topmast shrouds; at their top, still climbing, he reaches up and takes the topsail yard.
                Push(FVector(0, 1, 0));
                if (Chuck->IsOnLadder() && L.Z > MainTopZ + 715 && Run->Arrived < 0) { Run->Arrived = 0; Shot(TEXT("TopmastShrouds")); }
                if (Chuck->IsSwinging()) { Run->bTop = true; Run->Sub = 8; Run->At = Now; Shot(TEXT("TopsailYardCatch")); }
                else if (Time > 20) Done = true;
            }
            else if (Run->Sub == 8)
            {
                // Hand over hand out along it, the topsail giving way round him.
                Push(FVector(0, 1, 0));
                if (Chuck->GetSwingLeaps() - Run->Leaps0 >= 3 && Chuck->IsSwinging()) { Shot(TEXT("TopsailYard")); Stop(); Run->Sub = 9; Run->At = Now; }
                else if (Chuck->IsSwinging() && Time > .5f) { Chuck->JumpPressed(); Run->At = Now; }
                else if (!Chuck->IsSwinging() && !Chuck->IsSwingLeaping() && Time > 2) Done = true;
            }
            else if (Run->Sub == 9) { if (Time > .6f && Chuck->IsSwinging()) { Chuck->DodgeToward(FVector2D::ZeroVector); Run->Sub = 10; Run->At = Now; } }
            else if (Run->Sub == 10)
            {
                // Let go: a long fall, onto the deck (no fall damage).
                if (Ground && Time > .5f)
                {
                    Done = true;
                    Run->bPass = Run->bMount && Run->bTop && NewDeaths == 0 && Chuck->GetLadderMounts() - Run->Mounts0 >= 3 && ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck);
                }
                else if (Time > 10) Done = true;
            }
            break;
        }
        case ET::MastClimb:
        {
            // On the golden leaf, from just over a topsail yard, straight up the mast (user 2026-11-08): the topmast's head,
            // over the doubling onto the topgallant, to the truck. Each mast, from aft, from the starboard side and from forward.
            // (The spars' axes: TopmastX and GallantX in ShipDeck.inl.)
            Limit = 80;
            const int32 Runs = 6, K = Run->Sub / 2;
            if (K >= Runs) { Done = true; Run->bPass = Run->MastOk == Runs && NewDeaths == 0; break; }
            const float MastX = K < 3 ? MainMastX : ForeMastX, Top = K < 3 ? MainTopZ : ForeTopZ;
            const float Tx = MastX + 24 + 17, Face = float(K % 3);
            const FVector Toward = Face == 0 ? FVector(1, 0, 0) : Face == 1 ? FVector(0, -1, 0) : FVector(-1, 0, 0);
            const TCHAR* FaceName = Face == 0 ? TEXT("aft") : Face == 1 ? TEXT("starboard") : TEXT("fore");
            if (Run->Sub % 2 == 0)
            {
                // Beside the topmast, a little over the topsail yard, facing it.
                const FVector From = Face == 0 ? FVector(Tx - 13.6f - 22, 0, Top + 790) : Face == 1 ? FVector(Tx, 13.6f + 22, Top + 790) : FVector(Tx + 31 + 22, 0, Top + 790);
                Chuck->ResetAtLocation(SW(From.X, From.Y, From.Z)); Chuck->SetActorRotation(Toward.Rotation());
                Chuck->LockStamina(30.f);
                Run->MastTop = From.Z; Run->Sub += 1; Run->At = Now;
                break;
            }
            // The stick toward the mast, as a player holds it (the topgallant stands 23 cm forward of the topmast's head).
            {
                const float Ax = FMath::Lerp(Tx, Tx + 23.f, FMath::Clamp((static_cast<float>(L.Z) - Top - 800.f) / 160.f, 0.f, 1.f));
                const FVector ToMast = FVector(Ax - L.X, -L.Y, 0).GetSafeNormal();
                Push(ToMast.IsNearlyZero() ? Toward : ToMast);
            }
            Run->MastTop = FMath::Max(Run->MastTop, static_cast<float>(L.Z));
            if (FParse::Param(FCommandLine::Get(), TEXT("ChuckMastTrace")) && FMath::Fmod(Time, .1f) < .017f)
                UE_LOG(LogTemp, Display, TEXT("CHUCK_MAST_TRACE k=%d t=%.2f rel=(%.1f,%.1f,%.1f) gait=%s"), K, Time, L.X - Tx, L.Y, L.Z - Top, Chuck->GetGaitName());
            const bool bTop = Run->MastTop > Top + 1300;
            if (bTop && Time > 1.f && Run->Arrived < 0 && K == 4) { Run->Arrived = 0; Shot(TEXT("MastTruck")); }
            if ((bTop && (Chuck->IsHanging() || Ground) && Time > 1.f) || Time > 12.f || L.Z < Top + 600)
            {
                Run->MastOk += bTop;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_MAST_CLIMB mast=%s face=%s top_cm=%.0f truck_cm=%.0f gait=%s ok=%d"), K < 3 ? TEXT("main") : TEXT("fore"), FaceName,
                    Run->MastTop - DeckZ, Top + 1362 - DeckZ, Chuck->GetGaitName(), bTop ? 1 : 0);
                Stop(); Run->Sub += 1; Run->At = Now; Run->Arrived = -1;
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
                    // (2026-11-03) and their blows land on each other's blades: steel meets steel at every one.
                    const bool bContact = Crew->GetFenceContacts() >= 4 && Crew->GetFenceTouching() >= Crew->GetFenceContacts() * .9f;
                    Run->bPass = Run->Count1 >= 1 && Far == 0 && Crew->GetWorstFenceOut() < 5.f && Crew->GetWorstHullOut() <= 0.f && !Chuck->IsAstral() && bContact;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_FENCERS near_hits=%d far_hits=%d fence_out=%.1f hull_out=%.1f parry_gap=%.1f contacts=%d touching=%d worst_contact_gap=%.1f phrases=%d %s"), Run->Count1, Far,
                        Crew->GetWorstFenceOut(), Crew->GetWorstHullOut(), Crew->GetLastParryGap(), Crew->GetFenceContacts(), Crew->GetFenceTouching(), Crew->GetWorstContactGap(), Crew->GetFencePhrases(), *Crew->GetFenceTally());
                }
            }
            break;
        }
        case ET::Sounds:
        {
            // The ship heard (2026-11-03): on deck, the sea along the hull and the rigging up and the lapping gone, the fight's
            // steel and the feet on the planking playing, the hold's rats silent; then down in the hold, the lapping up and the
            // rats heard. The creaks keep coming, quieter.
            Limit = 30;
            AShipSetting* S = AShipSetting::Get(World);
            AShipCrew* Crew = AShipCrew::Get(World);
            if (!S || !Crew) { Done = true; break; }
            Stop();
            if (Run->Sub == 0)
            {
                Run->Count0 = S->GetSoundsPlayed(TEXT("SFX_SwordClash")) + S->GetSoundsPlayed(TEXT("SFX_SwordBind"));
                Run->Count1 = S->GetSoundsPlayed(TEXT("SFX_DeckStep")) + S->GetSoundsPlayed(TEXT("SFX_DeckStamp"));
                Run->Seen = AEnemyRat::ShipSoundsHeard; Run->TalkChecks = AEnemyRat::ShipSoundsHushed; Run->Creaks0 = S->GetCreaks();
                Run->Sub = 1; Run->At = Now;
            }
            else if (Run->Sub == 1 && Time > 10.f)
            {
                const FVector4 A = S->GetAmbience();
                Run->DeckClash = S->GetSoundsPlayed(TEXT("SFX_SwordClash")) + S->GetSoundsPlayed(TEXT("SFX_SwordBind")) - Run->Count0;
                Run->DeckFeet = S->GetSoundsPlayed(TEXT("SFX_DeckStep")) + S->GetSoundsPlayed(TEXT("SFX_DeckStamp")) - Run->Count1;
                Run->DeckRatsHeard = AEnemyRat::ShipSoundsHeard - Run->Seen; Run->DeckRatsHushed = AEnemyRat::ShipSoundsHushed - Run->TalkChecks;
                Run->DeckMix = A.Y > .5f && A.W > .3f && A.Z < .05f && A.X < .25f;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SOUNDS_DECK clashes=%d feet=%d swishes=%d scrapes=%d rats_heard=%d rats_hushed=%d wash=%.2f water=%.2f lap=%.2f rigging=%.2f"),
                    Run->DeckClash, Run->DeckFeet, S->GetSoundsPlayed(TEXT("SFX_SwordSwish")), S->GetSoundsPlayed(TEXT("SFX_SwordScrape")), Run->DeckRatsHeard, Run->DeckRatsHushed, A.X, A.Y, A.Z, A.W);
                Reset(EShipSpawn::Hold); Stop();
                Run->Seen = AEnemyRat::ShipSoundsHeard; Run->Sub = 2; Run->At = Now;
            }
            else if (Run->Sub == 2 && Time > 10.f)
            {
                const FVector4 A = S->GetAmbience();
                const int32 HoldRats = AEnemyRat::ShipSoundsHeard - Run->Seen;
                const int32 Creaks = S->GetCreaks() - Run->Creaks0;
                const bool bHoldMix = A.Z > .9f && A.Y < .2f && A.W < .1f && A.X > .45f;
                Done = true;
                Run->bPass = Run->DeckClash >= 4 && Run->DeckFeet >= 10 && Run->DeckRatsHeard == 0 && Run->DeckRatsHushed > 0 && Run->DeckMix && HoldRats > 0 && bHoldMix && Creaks >= 2;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SOUNDS pass=%d deck_clashes=%d deck_feet=%d deck_rats_heard=%d deck_rats_hushed=%d deck_mix=%d hold_rats_heard=%d hold_mix=%d wash=%.2f water=%.2f lap=%.2f rigging=%.2f creaks=%d"),
                    Run->bPass, Run->DeckClash, Run->DeckFeet, Run->DeckRatsHeard, Run->DeckRatsHushed, Run->DeckMix, HoldRats, bHoldMix, A.X, A.Y, A.Z, A.W, Creaks);
            }
            break;
        }
        case ET::Chest:
        {
            // The captain's chest (2026-11-03): before it, facing it, he interacts; the lid swings open and a golden carton
            // drops out before it; he walks over it: 40 cigarettes and the golden leaf's stamina lock. It opens once (a
            // second interact does nothing). The gate opens with it only if the other six conditions were met.
            Limit = 25;
            AShipChest* Chest = AShipChest::Get(World);
            ACigarettePickup* Carton = nullptr;
            for (TActorIterator<ACigarettePickup> It(World); It; ++It) if (It->IsCarton() && !It->IsActorBeingDestroyed()) Carton = *It;
            if (!Chest) { Done = true; break; }
            if (Run->Sub == 0)
            {
                Run->Count0 = ShipGate::DoneCount(); Run->Count1 = Chuck->GetCigarettes();
                Chuck->ResetAtLocation(SW(-1185, 88, LowerZ + Stand + 1)); Chuck->SetActorRotation(FRotator(0, 90, 0)); Stop();
                Run->Sub = 1; Run->At = Now; break;
            }
            if (Run->Sub == 1) { Stop(); if (Time > .8f) { Run->bTop = Chest->CanInteract(Chuck); Chuck->Interact(); Run->Sub = 2; Run->At = Now; } break; }
            if (Run->Sub == 2)
            {
                Stop();
                if (Time > .45f && !Run->bMount) { Run->bMount = true; Shot(TEXT("ChestOpening")); }
                if (Time > 2.6f) { Shot(TEXT("ChestOpen")); Chuck->Interact(); Run->Sub = 3; Run->At = Now; Run->Point = 0; }
                break;
            }
            if (Run->Sub == 3)
            {
                if (Carton) { const FVector C = ShipLocal(Carton->GetActorLocation()); Steer(C.X, C.Y, 4.f); }
                if (Chuck->GetCigarettes() >= Run->Count1 + ACigarettePickup::CartonCigarettes || Time > 12.f) { Stop(); Run->Sub = 4; Run->At = Now; }
                break;
            }
            Stop();
            if (Time < 1.f) break;
            int32 Cartons = 0;
            for (TActorIterator<ACigarettePickup> It(World); It; ++It) Cartons += It->IsCarton() && !It->IsActorBeingDestroyed();
            const int32 Got = Chuck->GetCigarettes() - Run->Count1;
            const bool bGate = ShipGate::IsOpen() == (Run->Count0 + 1 == ShipGate::Steps);
            Done = true;
            Run->bPass = Run->bTop && Chest->IsOpen() && Chest->GetLidAngle() > 95.f && Got == ACigarettePickup::CartonCigarettes && Chuck->GetStaminaLockRemaining() > 30.f
                && ShipGate::Done(ShipGate::EStep::Chest) && bGate && Cartons == 0 && NewDeaths == 0;
            // (2026-11-05: the gate open below decks, the storm waits until he is up on deck.)
            const AShipStorm* WaitingStorm = AShipStorm::Get(World);
            const bool bStormWaits = !WaitingStorm || !WaitingStorm->HasStarted();
            Run->bPass = Run->bPass && bStormWaits;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CHEST_TRIAL pass=%d could_interact=%d open=%d lid=%.0f cigarettes=%d lock_s=%.1f gate_before=%d gate_now=%d open_gate=%d cartons_left=%d storm_waits=%d"),
                Run->bPass, Run->bTop, Chest->IsOpen(), Chest->GetLidAngle(), Got, Chuck->GetStaminaLockRemaining(), Run->Count0, ShipGate::DoneCount(), ShipGate::IsOpen(), Cartons, bStormWaits);
            break;
        }
        case ET::Storm:
        {
            // The storm (2026-11-03): stills as it rolls in (its clock set) and in full, one with a lightning strike; then he
            // walks the deck in it (rain, splashes, spray, a strike and its thunder, the rolling, the mix on deck) and goes
            // below (the rain overhead, the wind through the hull).
            Limit = 150;
            AShipStorm* Storm = AShipStorm::Get(World);
            AShipSetting* S = AShipSetting::Get(World);
            if (!Storm || !S) { Done = true; break; }
            if (!ShipGate::IsOpen()) ShipGate::OpenAll();
            struct FStill { const TCHAR* Name; FVector Eye, Aim; float Clock; bool bStrike; };
            static const TArray<FStill> Stills = {
                {TEXT("RollIn05"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 5, false},
                {TEXT("RollIn25"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 25, false},
                {TEXT("RollIn45"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 45, false},
                {TEXT("RollIn70"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 70, false},
                {TEXT("FullFromBow"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 80, false},
                {TEXT("FullFromQuarter"), {-1460, 260, QuarterZ + 190}, {600, -60, DeckZ + 230}, 81, false},
                {TEXT("FullHorizon"), {0, -150, DeckZ + 170}, {0, 2000, DeckZ + 170}, 82, false},
                {TEXT("FullWeatherSide"), {200, 250, DeckZ + 160}, {300, -2500, DeckZ - 200}, 83, false},
                {TEXT("FullRat"), {-40, 210, DeckZ + 40}, {500, -150, DeckZ + 110}, 84, false},
                {TEXT("FullCrew"), {-170, 260, DeckZ + 170}, {90, -40, DeckZ + 100}, 85, false},
                {TEXT("FullSails"), {900, -520, DeckZ + 160}, {-300, 60, 1900}, 86, false},
                {TEXT("FullOutside"), {2300, -3600, 1200}, {0, 0, 1000}, 87, false},
                {TEXT("FullBow"), {2600, -900, 650}, {1500, 0, 450}, 88, false},
                {TEXT("FullWaterline"), {300, -950, 390}, {300, -430, 300}, 89, false},
                {TEXT("Spray"), {250, 260, DeckZ + 150}, {450, -420, RailZ + 60}, 89.5f, true},
                {TEXT("Lightning"), {0, -150, DeckZ + 170}, {300, 2000, DeckZ + 600}, 90, true},
                {TEXT("LightningDeck"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, 91, true},
                {TEXT("FullQuarters"), {-200, 250, LowerZ + 160}, {-800, -100, LowerZ + 100}, 92, false},
                {TEXT("FullArrivalHole"), {150, -150, LowerZ + 90}, {125, 420, LowerZ + 80}, 93, false}};
            auto StormShot = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Ship") / TEXT("Storm") / FString::Printf(TEXT("Storm_%s.png"), Name), true, false); };
            if (Run->Sub == 0)
            {
                // Each still: set it up, let the light settle 2.5 s (a strike 2.3 s in), take it.
                const int32 Si = Run->StormShot / 3, Step = Run->StormShot % 3;
                if (Si >= Stills.Num())
                {
                    Chuck->SetActorHiddenInGame(false);
                    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
                    Reset(EShipSpawn::Deck); Storm->SetClock(80.f);
                    Run->Strikes0 = Storm->GetStrikes(); Run->Thunders0 = Storm->GetThunders(); Run->Splashes0 = Storm->GetSplashes(); Run->Sprays0 = Storm->GetSprays();
                    Run->MaxDrops = 0; Run->MaxRoll = 0; Run->WorstMs = 0; Run->Sub = 1; Run->At = Now; Run->Point = 0;
                    break;
                }
                const FStill& V = Stills[Si];
                // (-ChuckStormStills=a+b: only those stills.)
                FString Want; FParse::Value(FCommandLine::Get(), TEXT("ChuckStormStills="), Want);
                if (Step == 0 && !Want.IsEmpty() && !Want.Contains(V.Name)) { Run->StormShot += 3; break; }
                if (Step == 0)
                {
                    Stop(); Chuck->SetActorHiddenInGame(true);
                    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Run->Camera.Get());
                    if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(SW(V.Eye.X, V.Eye.Y, V.Eye.Z), (SW(V.Aim.X, V.Aim.Y, V.Aim.Z) - SW(V.Eye.X, V.Eye.Y, V.Eye.Z)).Rotation());
                    Storm->SetClock(V.Clock);
                    Run->Pressed = Now; ++Run->StormShot;
                }
                else if (Step == 1 && Now - Run->Pressed > (V.bStrike ? 2.3 : 2.5))
                {
                    if (V.bStrike && FCString::Strcmp(V.Name, TEXT("Spray")) == 0) Storm->SprayNow(SW(450, -ShipOuterHalf(450, RailZ) - 20, RailZ + 10), 1.2f);
                    else if (V.bStrike) Storm->StrikeNow(70000.f, FRotator((SW(V.Aim.X, V.Aim.Y, 0) - SW(V.Eye.X, V.Eye.Y, 0)).Rotation()).Yaw);
                    else StormShot(V.Name);
                    Run->Pressed = Now; ++Run->StormShot;
                }
                else if (Step == 2 && (!V.bStrike || Now - Run->Pressed > (FCString::Strcmp(V.Name, TEXT("Spray")) == 0 ? .55 : .02)))
                {
                    if (V.bStrike) StormShot(V.Name);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_STILL name=%s clock=%.0f wind=%.2f dark=%.2f sea=%.2f rain=%.2f flash=%.2f drops=%d sun=%.2f roll=%.1f frame_ms=%.1f"),
                        V.Name, Storm->GetClock(), Storm->GetWind(), Storm->GetDark(), Storm->GetSea(), Storm->GetRain(), Storm->GetFlash(), Storm->GetDropsShown(), S->GetSunIntensity(), S->GetSeaTilt().Y, GAverageMS);
                    Run->Next = Now + .4; ++Run->StormShot;
                }
                break;
            }
            if (Run->Sub == 1)
            {
                // Forward along the deck to the fore mast's lee, in the full storm; a strike near by part way.
                Run->MaxDrops = FMath::Max(Run->MaxDrops, Storm->GetDropsShown());
                Run->MaxRoll = FMath::Max(Run->MaxRoll, FMath::Abs(S->GetSeaTilt().Y));
                if (Time > 2.f) Run->WorstMs = FMath::Max(Run->WorstMs, GAverageMS);
                if (Run->Point == 0 && Time > 3.f) { Storm->StrikeNow(60000.f, 70.f); Run->Point = 1; }
                if (Time > 4.f && Time < 4.1f) Shot(TEXT("StormDeck"));
                const bool bThere = Steer(380, -120, 40.f);
                if ((bThere && Time > 9.f) || Time > 20.f)
                {
                    Stop();
                    const FVector4 Lp = Storm->GetLoopLevels();
                    Run->StormDeck = bThere && Lp.X > .4f && Lp.Z > .5f && Lp.Y < .05f && Storm->GetFlapLevel() > .2f && Storm->GetSeaLevel() > .4f;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_DECK there=%d rain=%.2f above=%.2f gale=%.2f below_wind=%.2f flap=%.2f sea=%.2f drops=%d splashes=%d sprays=%d strikes=%d thunders=%d roll=%.1f worst_ms=%.1f"),
                        bThere, Lp.X, Lp.Y, Lp.Z, Lp.W, Storm->GetFlapLevel(), Storm->GetSeaLevel(), Run->MaxDrops, Storm->GetSplashes() - Run->Splashes0, Storm->GetSprays() - Run->Sprays0,
                        Storm->GetStrikes() - Run->Strikes0, Storm->GetThunders() - Run->Thunders0, Run->MaxRoll, Run->WorstMs);
                    Reset(EShipSpawn::Quarters); Run->Sub = 2; Run->At = Now;
                }
                break;
            }
            Stop();
            if (Time < 6.f) break;
            const FVector4 Lp = Storm->GetLoopLevels();
            const bool bBelow = Lp.Y > .35f && Lp.X < .05f && Lp.W > .5f && Storm->GetDropsShown() >= 0;
            Done = true;
            Run->bPass = Run->StormDeck && bBelow && Run->MaxDrops > 1000 && Storm->GetSplashes() > Run->Splashes0 + 20 && Storm->GetStrikes() > Run->Strikes0
                && Storm->GetThunders() > Run->Thunders0 && Run->MaxRoll > 4.f && NewDeaths == 0;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM pass=%d deck=%d below=%d below_rain=%.2f deck_rain=%.2f below_wind=%.2f drops=%d splashes=%d sprays=%d strikes=%d thunders=%d roll=%.1f worst_ms=%.1f"),
                Run->bPass, Run->StormDeck, bBelow, Lp.Y, Lp.X, Lp.W, Run->MaxDrops, Storm->GetSplashes() - Run->Splashes0, Storm->GetSprays() - Run->Sprays0,
                Storm->GetStrikes() - Run->Strikes0, Storm->GetThunders() - Run->Thunders0, Run->MaxRoll, Run->WorstMs);
            break;
        }
        case ET::StormWork:
        {
            // The storm's work, its rupture and its second part (2026-11-04): the deck's crew go to their stations and work
            // there (stills, and their hands on their lines and the wheel); the Astral rupture fills the companion hatch, and
            // walking into it is an Astral death back on deck; once it intensifies the ship rolls deeper and a sea breaks aboard
            // and shoves him across the deck; the storm's two tracks play in turn.
            Limit = 170;
            AShipStorm* Storm = AShipStorm::Get(World);
            AShipSetting* S = AShipSetting::Get(World);
            AShipCrew* Crew = AShipCrew::Get(World);
            const ADockGameMode* Mode = World->GetAuthGameMode<ADockGameMode>();
            if (!Storm || !S || !Crew) { Done = true; break; }
            if (!ShipGate::IsOpen()) ShipGate::OpenAll();
            const int32 Region = Mode ? Mode->GetMusicRegion() : 0;
            Run->Music31 |= Region == 31; Run->Music32 |= Region == 32;
            Run->MaxPuffs = FMath::Max(Run->MaxPuffs, Storm->GetPuffsShown());
            if (const ADockNPC* Captain = Crew->Find(EDockHuman::PirateCaptain)) { Run->HelmMin = FMath::Min(Run->HelmMin, Captain->GetHelmAngle()); Run->HelmMax = FMath::Max(Run->HelmMax, Captain->GetHelmAngle()); }
            auto WorkShot = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Ship") / TEXT("Storm") / FString::Printf(TEXT("Work_%s.png"), Name), true, false); };
            const float FullAt = 72.f, IntenseFull = Storm->GetIntenseAt() + 40.f;
            if (Run->Sub == 0)
            {
                // The full storm (its first part), the crew on their way; on until they are all at work.
                if (Storm->GetClock() < FullAt) Storm->SetClock(FullAt);
                Run->Stations = Crew->GetAtStations(&Run->StationsOf);
                if ((Run->StationsOf > 0 && Run->Stations == Run->StationsOf && Time > 6.f) || Time > 35.f)
                {
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_WORK_STATIONS at=%d of=%d seconds=%.1f rupture=%.2f"), Run->Stations, Run->StationsOf, Time, Storm->GetRupture());
                    Run->Sub = 1; Run->At = Now; Run->Pressed = Now; Run->WorkShot = 0;
                }
                break;
            }
            struct FStill { const TCHAR* Name; FVector Eye, Aim; bool bIntense; float Wave; };
            static const TArray<FStill> Stills = {
                {TEXT("MainHaul"), {-110, -430, DeckZ + 175}, {-290, -230, DeckZ + 100}, false, -1},
                {TEXT("MainHaulFront"), {-560, -150, DeckZ + 150}, {-280, -230, DeckZ + 105}, false, -1},
                {TEXT("ForeHaul"), {740, 420, DeckZ + 170}, {480, 235, DeckZ + 100}, false, -1},
                {TEXT("ForeHaulFront"), {250, 330, DeckZ + 150}, {480, 235, DeckZ + 105}, false, -1},
                {TEXT("Sweat"), {-400, 330, DeckZ + 175}, {-505, 110, DeckZ + 150}, false, -1},
                // The lines close (2026-11-07): through the haulers' hands, round the lead block, the tail to its coil, the pin.
                {TEXT("RopeHands"), {-235, -345, DeckZ + 135}, {-280, -230, DeckZ + 92}, false, -1},
                {TEXT("RopeBlock"), {-370, -330, DeckZ + 75}, {-425, -230, DeckZ + 25}, false, -1},
                {TEXT("RopeTail"), {-20, -345, DeckZ + 120}, {-120, -215, DeckZ + 25}, false, -1},
                {TEXT("RopePin"), {-640, 190, DeckZ + 125}, {-535, 85, DeckZ + 60}, false, -1},
                {TEXT("Helm"), {-1250, -170, QuarterZ + 165}, {-1470, 0, QuarterZ + 85}, false, -1},
                {TEXT("HelmBehind"), {-1585, 140, QuarterZ + 185}, {-1440, 0, QuarterZ + 90}, false, -1},
                {TEXT("Rupture"), {-40, 340, DeckZ + 200}, {-250, 130, DeckZ + 40}, false, -1},
                {TEXT("RuptureClose"), {-140, 40, DeckZ + 140}, {-250, 130, DeckZ + 15}, false, -1},
                {TEXT("WholeDeck"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 300}, false, -1},
                {TEXT("IntenseBow"), {1500, -220, DeckZ + 190}, {-600, 40, DeckZ + 330}, true, -1},
                {TEXT("IntenseQuarter"), {-1460, 260, QuarterZ + 190}, {600, -60, DeckZ + 230}, true, -1},
                {TEXT("IntenseHorizon"), {0, -150, DeckZ + 170}, {0, 2000, DeckZ + 170}, true, -1},
                {TEXT("DeckWaveBurst"), {330, 340, DeckZ + 210}, {650, -260, DeckZ + 90}, true, .5f},
                {TEXT("DeckWaveSheet"), {330, 340, DeckZ + 210}, {650, -200, DeckZ + 20}, true, 1.3f},
                {TEXT("DeckWaveRat"), {480, 260, DeckZ + 40}, {650, -300, DeckZ + 70}, true, .9f}};
            if (Run->Sub == 1)
            {
                // Each still: the camera set, the storm at its part, 2.5 s to settle (a wave set off, then the still after its delay).
                const int32 Si = Run->WorkShot / 3, Step = Run->WorkShot % 3;
                // (Their hands on their work, meanwhile.)
                for (EDockHuman K : {EDockHuman::PirateConcertina, EDockHuman::PirateCheering, EDockHuman::PirateFencerA, EDockHuman::PirateFencerB, EDockHuman::PirateDancer, EDockHuman::PirateCaptain})
                    if (const ADockNPC* N = Crew->Find(K))
                        if (N->IsAtStormStation() && Time > 2.f)
                            for (int32 Side = 0; Side < 2; ++Side)
                            {
                                const float Gap = static_cast<float>(FVector::Dist(N->GetStormFist(Side), N->GetStormGripWant(Side)));
                                Run->WorstWorkGrip = FMath::Max(Run->WorstWorkGrip, Gap);
                                float& By = Run->GripBy.FindOrAdd(FString::Printf(TEXT("%s_%d_%d"), *N->GetName(), int32(K), Side)); By = FMath::Max(By, Gap);
                            }
                // (Their lines through the hands that grip them, and smooth all along.)
                if (Time > 2.f) { Run->WorstRopeGap = FMath::Max(Run->WorstRopeGap, Crew->GetLineFistGap()); Run->WorstRopeTurn = FMath::Max(Run->WorstRopeTurn, Crew->GetLineWorstTurn()); }
                if (Storm->GetIntense() > .9f) Run->IntenseRoll = FMath::Max(Run->IntenseRoll, FMath::Abs(S->GetSeaTilt().Y));
                if (Si >= Stills.Num())
                {
                    Chuck->SetActorHiddenInGame(false);
                    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
                    // Into the rupture: from before the ladder's lip, walking aft at it.
                    Chuck->ResetAtLocation(SW(CompX1 + 45, (CompY0 + CompY1) * .5f, DeckZ + Stand)); Chuck->SetActorRotation(FRotator(0, 180, 0));
                    Run->Deaths0 = Chuck->GetFallDeaths(); Run->Mounts0 = Chuck->GetLadderMounts();
                    Run->Sub = 2; Run->At = Now;
                    break;
                }
                const FStill& V = Stills[Si];
                FString Want; FParse::Value(FCommandLine::Get(), TEXT("ChuckStormStills="), Want);
                if (Step == 0 && !Want.IsEmpty() && !Want.Contains(V.Name)) { Run->WorkShot += 3; break; }
                if (Step == 0)
                {
                    Stop(); Chuck->SetActorHiddenInGame(true);
                    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Run->Camera.Get());
                    if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(SW(V.Eye.X, V.Eye.Y, V.Eye.Z), (SW(V.Aim.X, V.Aim.Y, V.Aim.Z) - SW(V.Eye.X, V.Eye.Y, V.Eye.Z)).Rotation());
                    if (V.bIntense && Storm->GetClock() < IntenseFull) Storm->SetClock(IntenseFull);
                    if (!V.bIntense && Storm->GetClock() > Storm->GetIntenseAt() - 6.f) Storm->SetClock(FullAt);   // (its first part)
                    Run->Pressed = Now; ++Run->WorkShot;
                }
                else if (Step == 1 && Now - Run->Pressed > 2.5)
                {
                    if (V.Wave >= 0.f) Storm->DeckWaveNow(650.f, 1.2f);
                    Run->Pressed = Now; ++Run->WorkShot;
                }
                else if (Step == 2 && Now - Run->Pressed > FMath::Max(V.Wave, .02f))
                {
                    WorkShot(V.Name);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_ROPE name=%s fist_gap=%.2f worst_turn=%.1f at=%s hauled=%.0f,%.0f,%.0f"), V.Name,
                        Crew->GetLineFistGap(), Crew->GetLineWorstTurn(), *Crew->GetLineWorstTurnAt().ToCompactString(), Crew->GetLineHauled(0), Crew->GetLineHauled(1), Crew->GetLineHauled(2));
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_WORK_STILL name=%s clock=%.0f intense=%.2f wind=%.2f roll=%.1f puffs=%d waves=%d helm=%.1f rupture=%.2f music=%d frame_ms=%.1f"),
                        V.Name, Storm->GetClock(), Storm->GetIntense(), ShipStormWind(), S->GetSeaTilt().Y, Storm->GetPuffsShown(), Storm->GetDeckWaves(),
                        Crew->Find(EDockHuman::PirateCaptain) ? Crew->Find(EDockHuman::PirateCaptain)->GetHelmAngle() : 0.f, Storm->GetRupture(), Region, GAverageMS);
                    Run->Pressed = Now; ++Run->WorkShot;
                }
                break;
            }
            if (Run->Sub == 2)
            {
                // Walking into the rupture: the ladder is not taken, he goes into the Astral and comes back on deck.
                if (Chuck->GetFallDeaths() > Run->Deaths0)
                {
                    if (Time > 4.5f)
                    {
                        Stop();
                        Run->RuptureDeath = Chuck->GetLadderMounts() == Run->Mounts0 && ShipSpaceAt(Chuck->GetActorLocation()) == int32(EShipSpawn::Deck);
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RUPTURE_TRIAL death=1 mounts=%d space=%s at=%s"), Chuck->GetLadderMounts() - Run->Mounts0,
                            ShipSpaceName(ShipSpaceAt(Chuck->GetActorLocation())), *L.ToString());
                        // Then a sea aboard, with him in its way.
                        Chuck->ResetAtLocation(SW(650, -120, DeckZ + Stand)); Chuck->SetActorRotation(FRotator(0, 90, 0));
                        Run->Shoves0 = Storm->GetWaveShoves(); Run->Waves0 = Storm->GetDeckWaves();
                        Run->Sub = 3; Run->At = Now; Run->WaveAt = -1;
                    }
                    else Stop();
                }
                else if (Time < 6.f) Push(FVector(-1, 0, 0));
                else
                {
                    Stop();
                    UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_RUPTURE_TRIAL death=0 mounts=%d at=%s gait=%s"), Chuck->GetLadderMounts() - Run->Mounts0, *L.ToString(), Chuck->GetGaitName());
                    Chuck->ResetAtLocation(SW(650, -120, DeckZ + Stand));
                    Run->Shoves0 = Storm->GetWaveShoves(); Run->Waves0 = Storm->GetDeckWaves();
                    Run->Sub = 3; Run->At = Now; Run->WaveAt = -1;
                }
                break;
            }
            if (Run->Sub == 3)
            {
                Stop();
                if (Run->WaveAt < 0 && Time > 1.5f) { Storm->DeckWaveNow(650.f, 1.2f); Run->WaveAt = Now; Shot(TEXT("WaveRat")); }
                if (Run->WaveAt > 0 && Now - Run->WaveAt > 4.f)
                {
                    Run->Shoved = Storm->GetWaveShoves() > Run->Shoves0 && L.Y > -100.f;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_WAVE_TRIAL shoved=%d at=%s"), Run->Shoved, *L.ToString());
                    Run->Sub = 4; Run->At = Now;
                }
                break;
            }
            // The intense track by now, and the storm's music through.
            if (Time < 3.f) break;
            Done = true;
            int32 Of = 0; const int32 At = Crew->GetAtStations(&Of);
            for (const auto& G : Run->GripBy) UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_WORK_GRIP who=%s worst_cm=%.1f"), *G.Key, G.Value);
            Run->bPass = Run->Stations == Run->StationsOf && Run->StationsOf == 6 && At == Of && Run->WorstWorkGrip < 8.f && Run->WorstRopeGap < 2.f && Run->WorstRopeTurn < 25.f && Run->HelmMax - Run->HelmMin > 12.f
                && Storm->GetRupture() >= 1.f && Run->RuptureDeath && Run->Shoved && Run->IntenseRoll > 8.f && Run->MaxPuffs > 10 && Storm->GetDeckWaves() > Run->Waves0
                && Run->Music31 && Run->Music32 && ShipRuptureOpen();
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_WORK pass=%d stations=%d/%d worst_grip_cm=%.1f rope_gap_cm=%.2f rope_turn_deg=%.1f helm_range=%.1f rupture=%.2f rupture_death=%d shoved=%d intense_roll=%.1f puffs=%d waves=%d music_storm=%d music_intense=%d"),
                Run->bPass, Run->Stations, Run->StationsOf, Run->WorstWorkGrip, Run->WorstRopeGap, Run->WorstRopeTurn, Run->HelmMax - Run->HelmMin, Storm->GetRupture(), Run->RuptureDeath, Run->Shoved, Run->IntenseRoll,
                Run->MaxPuffs, Storm->GetDeckWaves(), Run->Music31, Run->Music32);
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
