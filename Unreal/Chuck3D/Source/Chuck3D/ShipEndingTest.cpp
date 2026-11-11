// -ChuckChultTest -ChuckShipEndingTest (Claude, 2026-11-08; ShipEnding.h): the ship's storm ending played by the real
// character. Straight onto the ship with the ending's gate open and the storm a few seconds from its second part: the
// captain's storm lines; the orders; standing about on deck (the jargon line, then the clear one after 30 s); out along the
// main yard to each work spot and the points tied, dropping to the deck; the fore ratlines carried away (their nets closed);
// a running jump off the main top without the golden leaf (short of the fore top); the leaf in the main top smoked; the long
// leap from the starboard lane and the larboard lane onto the fore top; up the fore topmast shrouds; the cutscene to its end (or,
// with -ChuckShipEndingSkip, skipped). Every one of the storm's 61 voice lines has its sound.
#include "ShipEnding.h"
#include "ShipEndingScene.h"
#include "ShipSetting.h"
#include "ShipStorm.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "NPCVoiceData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "Engine/OverlapResult.h"

using namespace Ship;

namespace
{
    FVector SW(float X, float Y, float Z) { return ShipWorld(FVector(X, Y, Z)); }
    constexpr float Stand = 34.65f;
}

void ReviewShipEnding(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    AShipSetting::Enter(World);
    ShipGate::Reset(World);
    ShipGate::OpenAll();
    Chuck->ResetAtLocation(ShipSpawn(EShipSpawn::Deck)); Chuck->SetActorRotation(FRotator(0, ShipSpawnYaw(EShipSpawn::Deck), 0));
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }
    // The lines: every storm line has its sound asset.
    int32 Lines = 0, Sounds = 0;
    for (int32 I = 0; I < NPCVoiceData::LineCount; ++I)
    {
        const NPCVoiceData::FLine& L = NPCVoiceData::Lines[I];
        const FString Npc = L.Npc;
        if (Npc != TEXT("PirateCaptain") && Npc != TEXT("PirateCrewman1") && Npc != TEXT("PirateCrewman2") && Npc != TEXT("Jeffries")) continue;
        if (FString(L.Id).StartsWith(TEXT("CAP_P")) || FString(L.Id).StartsWith(TEXT("JEF_P"))) continue;   // (their pre-storm talk, 2026-11-11)
        ++Lines; Sounds += *L.Sound && LoadObject<USoundBase>(nullptr, L.Sound) != nullptr;
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_LINES lines=%d sounds=%d pass=%d"), Lines, Sounds, Lines == 61 && Sounds == 61);
    Failures += !(Lines == 61 && Sounds == 61);

    struct FRun
    {
        int32 Phase = 0, Sub = 0, Failures = 0, Shots = 0, Leaps0 = 0;
        double At = 0, Start = 0, Next = 0;
        TArray<FString> Results; TSet<FString> ShotsSeen; bool bLeafShot = false, bCrossShot = false; float DuckMin = 1.f;
        FTimerHandle Timer; bool bDone = false, bStorm = false;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Start = Run->At = World->GetTimeSeconds();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        const double Now = World->GetTimeSeconds();
        const float T = static_cast<float>(Now - Run->At);
        AShipEnding* E = AShipEnding::Get(World);
        AShipStorm* Storm = AShipStorm::Get(World);
        if (!E || !Storm) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_ENDING_TEST no_ending=1")); Run->bDone = true; FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        auto* M = Chuck->GetCharacterMovement();
        const FVector L = ShipLocal(Chuck->GetActorLocation());
        const bool Ground = M->IsMovingOnGround();
        auto Push = [&](const FVector& D) { Chuck->SetTestStickWorld(D); Chuck->AddMovementInput(D, 1.f, true); };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        // The storm's sounds under the crew's lines (2026-11-09): the lowest they sank.
        if (const AShipStorm* Sounds = AShipStorm::Get(World)) Run->DuckMin = FMath::Min(Run->DuckMin, Sounds->GetSpeechDuck());
        auto Shot = [&](const FString& Name) { if (Run->Shots < 40) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("ShipEnding") / (Name + TEXT(".png")), true, false); } };
        auto Place = [&](float X, float Y, float Z, float Yaw) { Chuck->ResetAtLocation(SW(X, Y, Z)); Chuck->GetCharacterMovement()->StopMovementImmediately(); Chuck->SetActorRotation(FRotator(0, Yaw, 0)); Chuck->SetRunHeld(false); Stop(); Chuck->Recenter(); };
        auto Next = [&](const TCHAR* Name, bool bPass, const FString& Note)
        {
            Run->Results.Add(FString::Printf(TEXT("%s=%d"), Name, bPass ? 1 : 0)); Run->Failures += !bPass;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_TRIAL name=%s pass=%d %s step=%s where=%s at=%s t=%.1f"), Name, bPass ? 1 : 0, *Note, AShipEnding::StepName(E->GetStep()),
                AShipEnding::WhereName(E->GetWhere()), *L.ToString(), T);
            ++Run->Phase; Run->Sub = 0; Run->At = Now; Stop(); Chuck->SetRunHeld(false);
        };
        if (!Run->bStorm)
        {
            Run->bStorm = true; Storm->SetClock(FMath::Max(1.f, Storm->GetIntenseAt() - 9.f));
            // -ChuckShipEndingFrom=leap: straight to the leap (the orders, the reef and the carry-away skipped).
            FString From; FParse::Value(FCommandLine::Get(), TEXT("ChuckShipEndingFrom="), From);
            if (From == TEXT("leap")) { Storm->SetClock(Storm->GetIntenseAt() + 30.f); E->SkipTo(AShipEnding::EStep::Leap); Run->Phase = 4; Place(MainMastX - 55, 70, MainTopZ + Stand, 0); }
            return;
        }
        switch (Run->Phase)
        {
        case 0:
            // The storm's start lines, then the orders; then he stands about on deck: the jargon, then after 30 s the clear line.
            Stop();
            if (E->GetLineCount(TEXT("CAP_11")) > 0 || T > 95.f)
            {
                const bool bPass = E->GetLineCount(TEXT("CAP_S00")) == 1 && E->GetLineCount(TEXT("CAP_S01")) == 1 && E->GetLineCount(TEXT("CAP_01")) == 1 && E->GetLineCount(TEXT("CR1_01")) == 1
                    && E->GetLineCount(TEXT("CR2_01")) == 1 && E->GetLineCount(TEXT("CAP_02")) == 1 && E->GetLineCount(TEXT("CAP_10")) >= 1 && E->GetLineCount(TEXT("CAP_11")) >= 1;
                if (T > 20.f) Shot(TEXT("DeckOrders"));
                Next(TEXT("orders"), bPass, FString::Printf(TEXT("cap10=%d cap11=%d lines=%d guides=%d"), E->GetLineCount(TEXT("CAP_10")), E->GetLineCount(TEXT("CAP_11")), E->GetLinesSpoken(), E->GetGuides()));
                Place(MainYardX, 70, MainTopZ + Stand, 90);
            }
            break;
        case 1: case 2:
        {
            // Out along the main yard to the work spot (starboard, +Y, then larboard), hang there, let go onto the deck. The
            // larboard trial starts on the starboard side of the yard, by the top, and swings across its middle to the larboard
            // side: pumped back and forth, then the leap (user 2026-11-10).
            const float Side = Run->Phase == 1 ? 1.f : -1.f;
            const int32 S = Run->Phase == 1 ? 1 : 0;
            const TCHAR* Name = S == 0 ? TEXT("reef_larboard") : TEXT("reef_starboard");
            if (Run->Sub == 0)
            {
                const float Out = Run->Phase == 2 ? 1.f : Side;
                Push(FVector(0, Out, 0));
                if (FMath::Abs(L.Y) > 98 && Ground) { Chuck->JumpPressed(); Run->Sub = Run->Phase == 2 ? 10 : 1; Run->At = Now; }
                else if (T > 4) Next(Name, false, TEXT("no_yard=1"));
            }
            else if (Run->Sub == 10)
            {
                // On the yard's innermost starboard grip: the stick to larboard, the swing pumped in time, the jump pressed.
                if (!Chuck->IsSwinging()) Push(FVector(0, 1, 0)); else Stop();
                if (Chuck->IsSwinging() && T > .8f) { Chuck->SetTestSwingPump(true); Push(FVector(0, -1, 0)); Chuck->JumpPressed(); Run->Sub = 11; Run->At = Now; }
                else if (!Chuck->IsSwinging() && T > 3) Next(Name, false, TEXT("no_catch=1"));
            }
            else if (Run->Sub == 11)
            {
                Push(FVector(0, -1, 0));
                if (Chuck->IsSwinging() && L.Y < -150.f)
                {
                    Chuck->SetTestSwingPump(false);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_YARD_CROSS crossed=1 seconds=%.1f grip_y=%.0f"), T, L.Y);
                    Shot(TEXT("YardCrossed")); Run->Sub = 1; Run->At = Now;
                }
                else if (Chuck->IsSwingLeaping() && !Run->bCrossShot) { Run->bCrossShot = true; Shot(TEXT("YardCrossMid")); }
                else if ((!Chuck->IsSwinging() && !Chuck->IsSwingLeaping()) || T > 12.f)
                {
                    Chuck->SetTestSwingPump(false);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_YARD_CROSS crossed=0 seconds=%.1f y=%.0f"), T, L.Y);
                    Next(Name, false, TEXT("no_cross=1"));
                }
            }
            else if (Run->Sub == 1)
            {
                Push(FVector(0, Side, 0));
                if (Chuck->IsSwinging() && FMath::Abs(L.Y) > 330.f) { Stop(); Run->Sub = 2; Run->At = Now; Shot(S == 0 ? TEXT("ReefLarboard") : TEXT("ReefStarboard")); }
                else if (Chuck->IsSwinging() && T > .6f) { Chuck->JumpPressed(); Run->At = Now; }
                else if (!Chuck->IsSwinging() && !Chuck->IsSwingLeaping() && T > 3) Next(S == 0 ? TEXT("reef_larboard") : TEXT("reef_starboard"), false, TEXT("fell=1"));
            }
            else if (Run->Sub == 2)
            {
                if (E->IsSideDone(S) && T > 2.f) { Chuck->DodgeToward(FVector2D::ZeroVector); Run->Sub = 3; Run->At = Now; }
                else if (T > 5.f) Next(S == 0 ? TEXT("reef_larboard") : TEXT("reef_starboard"), false, TEXT("not_tied=1"));
            }
            else if (Ground && T > .5f)
            {
                const bool bPass = E->IsSideDone(S) && E->GetWhere() == AShipEnding::EWhere::Deck && E->GetLineCount(S == 0 ? TEXT("CR1_02L") : TEXT("CR1_02S")) <= 1;
                Next(S == 0 ? TEXT("reef_larboard") : TEXT("reef_starboard"), bPass, TEXT(""));
                if (S == 1) Place(MainYardX, 70, MainTopZ + Stand, 90);   // (the starboard trial done: the larboard one from the top)
            }
            else if (T > 6.f) Next(S == 0 ? TEXT("reef_larboard") : TEXT("reef_starboard"), false, TEXT("no_landing=1"));
            break;
        }
        case 3:
            // The trouble on the foremast: the calls, the sea, the fore ratlines carried away; the captain's call.
            if (E->GetStep() == AShipEnding::EStep::Leap && E->GetLineCount(TEXT("CAP_04")) + E->GetLineCount(TEXT("CAP_03")) > 0)
            {
                int32 Blocked = 0, Fore = 0;
                for (const FChuckClimbable& C : GetChuckClimbables())
                    if (ShipLocal(C.Foot).X > 0 && ShipLocal(C.Foot).Z < DeckZ + 5 && C.bTopOut && C.Line.Num() > 0) { ++Fore; Blocked += C.bBlocked; }
                bool bHidden = false;
                for (TActorIterator<AShipSetting> It(World); It; ++It)
                    for (UActorComponent* C : It->GetComponents())
                        if (C->GetFName() == TEXT("ShipForeRatlines")) bHidden = !Cast<USceneComponent>(C)->IsVisible();
                Shot(TEXT("ForeCarried"));
                const bool bPass = E->IsCarriedAway() && Fore == 2 && Blocked == 2 && bHidden && E->GetLineCount(TEXT("CR2_02")) == 1 && E->GetLineCount(TEXT("CR1_03")) == 1
                    && E->GetLineCount(TEXT("CR1_04")) == 1 && E->GetLineCount(TEXT("CR2_03")) == 1;
                Next(TEXT("fore_carry"), bPass, FString::Printf(TEXT("fore_nets=%d blocked=%d hidden=%d"), Fore, Blocked, bHidden));
                Place(MainMastX - 55, 70, MainTopZ + Stand, 0);
            }
            else if (T > 45.f) Next(TEXT("fore_carry"), false, TEXT("timeout=1"));
            break;
        case 4: case 6: case 7:
        {
            // A running jump off the main top's fore rim: without the leaf (4) short; with it, from each lane (6 starboard, 7 larboard).
            const TCHAR* Name = Run->Phase == 4 ? TEXT("jump_no_leaf") : Run->Phase == 6 ? TEXT("leap_starboard") : TEXT("leap_port");
            if (Run->Sub == 0)
            {
                if (T < .6f) { Run->Leaps0 = E->GetLeaps(); break; }
                Chuck->SetRunHeld(true); Push(FVector(1, 0, 0));
                if (L.X > TopX1 - 22.f && Ground) { Chuck->JumpPressed(); Run->Sub = 1; Run->At = Now; }
                else if (!Ground && L.X > TopX1 - 80.f) { Run->Sub = 1; Run->At = Now; }   // (over the rim in the run's stride)
                else if (T > 6.f) Next(Name, false, TEXT("no_takeoff=1"));
            }
            else
            {
                if (!Ground) Push(FVector(1, 0, 0)); else Stop();
                if (T > .45f && T < .55f && Run->Phase == 6) Shot(TEXT("LeapMid"));
                if ((Ground || Chuck->IsOnLadder() || Chuck->IsSwinging() || Chuck->IsHanging()) && T > .3f)
                {
                    const bool bLeapt = E->GetLeaps() > Run->Leaps0;
                    const bool bFore = E->GetWhere() == AShipEnding::EWhere::ForeTop;
                    const bool bPass = Run->Phase == 4 ? !bLeapt && !bFore : bLeapt && bFore && E->GetStep() == AShipEnding::EStep::ForeClimb;
                    if (Run->Phase != 4) Shot(Run->Phase == 6 ? TEXT("LandedStarboard") : TEXT("LandedPort"));
                    const int32 Was = Run->Phase;
                    Next(Name, bPass, FString::Printf(TEXT("leapt=%d fore=%d out_cm=%.0f to=%s"), bLeapt, bFore, FVector::Dist2D(E->GetLastLeapTakeoff(), L), *E->GetLastLeapLanding().ToString()));
                    if (Was == 4) Place(MainMastX + 70, -104, MainTopZ + Stand, 0);
                    else if (Was == 6) { const float Lock = Chuck->GetStaminaLockRemaining(); Place(MainMastX - 55, -70, MainTopZ + Stand, 0); if (Lock > 0.f) Chuck->LockStamina(Lock); }
                    else { Stop(); Chuck->GetCharacterMovement()->StopMovementImmediately(); }
                }
                else if (T > 5.f) Next(Name, false, TEXT("no_landing=1"));
            }
            break;
        }
        case 5:
            // The golden cigarette by the lookout's jar: walked over, smoked.
            if (T < .2f && !Run->bLeafShot) { Run->bLeafShot = true; Shot(TEXT("LeafMainTop")); }
            Push(FVector(1, 0, 0));
            if (Chuck->IsStaminaLocked() || T > 4.f)
            {
                const float Lock = Chuck->GetStaminaLockRemaining();
                Next(TEXT("leaf"), Chuck->IsStaminaLocked(), FString::Printf(TEXT("lock_s=%.1f"), Lock));
                Place(MainMastX - 55, 70, MainTopZ + Stand, 0);
                if (Lock > 0.f) Chuck->LockStamina(Lock);   // (the test's teleport resets it; a player walks there)
            }
            break;
        case 8:
        {
            // From where he landed on the fore top, to the foot of the larboard topmast shrouds and up them: at their head the cutscene.
            // (-ChuckShipSceneSide=1: the starboard ones, for the cutscene on that side.)
            static const float Up = []{ float V = -1.f; FParse::Value(FCommandLine::Get(), TEXT("ChuckShipSceneSide="), V); return V >= 0.f ? 1.f : -1.f; }();
            if (Run->Sub == 0)
            {
                const FVector2D To(ForeMastX, Up * 84.f);
                const FVector2D D = To - FVector2D(L.X, L.Y);
                if (D.Size() < 6.f) { Run->Sub = 1; Run->At = Now; }
                else Push(FVector(D.X, D.Y, 0).GetSafeNormal() * .5f);
                if (T > 8.f) Next(TEXT("fore_climb"), false, TEXT("no_foot=1"));
                break;
            }
            Push(FVector(0, Up, 0));
            if (E->GetStep() == AShipEnding::EStep::Scene) { Shot(TEXT("ForeHead")); Next(TEXT("fore_climb"), true, TEXT("")); }
            else if (T > 25.f) Next(TEXT("fore_climb"), false, FString::Printf(TEXT("no_scene=1 gait=%s"), Chuck->GetGaitName()));
            break;
        }
        case 9:
        {
            // The cutscene: its shots, Chuck along the yard, thrown, into the burning rupture; the end card.
            Stop();
            const AShipForeTopsailScene* S = AShipForeTopsailScene::Get(World);
            if (!S) { Next(TEXT("scene"), false, TEXT("none=1")); break; }
            const FString ShotName = S->GetShotName();
            if (!ShotName.IsEmpty() && !Run->ShotsSeen.Contains(ShotName) && S->GetClock() > 0.f)
            {
                // (A little into each shot, once its camera has settled.)
                static double ShotFirst = 0; static FString Pending;
                if (Pending != ShotName) { Pending = ShotName; ShotFirst = Now; }
                else if (Now - ShotFirst > (ShotName == TEXT("Grip") ? .6 : ShotName == TEXT("Below") ? .9 : 1.6)) { Run->ShotsSeen.Add(ShotName); Shot(TEXT("Scene_") + ShotName); }
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("ChuckShipEndingSkip")) && S->GetClock() > 8.f && !S->IsFinished()) const_cast<AShipForeTopsailScene*>(S)->Skip();
            if (S->IsFinished() && Chuck->HasDemoEnded())
            {
                const bool bSkip = FParse::Param(FCommandLine::Get(), TEXT("ChuckShipEndingSkip"));
                // (2026-11-09: the great sea knocks her over at least 20 degrees and he loses his grip: eleven shots.)
                const bool bPass = bSkip ? true : Run->ShotsSeen.Num() >= 10 && S->GetYardLeaps() == 3 && S->HasThrown() && S->HasPlunged() && S->GetPlungeMiss() < 300.f
                    && S->GetMaxHeel() >= 20.f && E->GetLineCount(TEXT("JEF_01")) == 1 && E->GetLineCount(TEXT("JEF_06")) == 1;
                Next(TEXT("scene"), bPass, FString::Printf(TEXT("shots=%d leaps=%d thrown=%d plunged=%d miss_cm=%.0f heel_deg=%.1f skipped=%d"), Run->ShotsSeen.Num(), S->GetYardLeaps(), S->HasThrown(), S->HasPlunged(), S->GetPlungeMiss(), S->GetMaxHeel(), bSkip));
                Run->Next = Now + 2.0;
            }
            else if (T > 70.f) Next(TEXT("scene"), false, TEXT("timeout=1"));
            break;
        }
        default:
            if (Now < Run->Next) break;
            if (Run->Sub == 0) { Shot(TEXT("EndCard")); Run->Sub = 1; }
            if (Now < Run->Next + 1.0) break;
            Run->bDone = true;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_TEST failures=%d trials=%s lines_spoken=%d leaps=%d guides=%d storm_duck_min=%.2f seconds=%.0f"), Run->Failures, *FString::Join(Run->Results, TEXT(" ")),
                E->GetLinesSpoken(), E->GetLeaps(), E->GetGuides(), Run->DuckMin, Now - Run->Start);
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
            break;
        }
    }, 1.f / 60.f, true);
}
