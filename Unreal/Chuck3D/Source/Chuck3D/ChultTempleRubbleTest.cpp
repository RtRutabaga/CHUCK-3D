// -ChuckChultTest -ChuckTempleRubbleTest (Claude, 2026-10-26; docs/CHULT-TEMPLE-RUBBLE.md): stills of the rubble map,
// then the real character: the lane walked from where he comes to to the crevice's mouth; the crevice crawled on all
// fours to its end and back out, standing again in the chamber; a rupture walked into (an Astral death, back at the
// lane's start). -ChuckRubbleTrials=a+b runs some; -ChuckTempleRubbleViewsOnly just the stills.
#include "ChultTempleRubble.h"
#include "ChultTempleInterior.h"
#include "ChuckCharacter.h"
#include "ChultTempleTraps.h"
#include "TempleSanctumBattle.h"
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
#include "Misc/Paths.h"


namespace
{
    using namespace TempleRubble;
    FVector W(float S, float T, float Z) { return ChultTempleInteriorWorldOf(FVector(S, T, Z)); }
    FVector Dir(float DS, float DT) { return (W(DS, DT, 0) - W(0, 0, 0)).GetSafeNormal(); }
    constexpr float Stand = 34.65f;
}

void ReviewChultTempleRubble(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    struct FView { const TCHAR* Name; FVector Eye, Aim; };
    const TArray<FView> Views = {
        {TEXT("RubbleArrival"), FVector(80150, -7950, RZ + 230), FVector(79850, -6000, RZ + 80)},
        {TEXT("RubbleHigh"), FVector(79900, -8500, RZ + 950), FVector(78500, -4700, RZ + 150)},
        {TEXT("RubbleGallery"), FVector(78700, -5700, RZ + 260), FVector(78700, -3700, RUZ - 250)},
        {TEXT("RubbleGalleryHigh"), FVector(78300, -5300, RZ + 900), FVector(78700, -3800, RZ)},
        {TEXT("RubbleStairMinus"), FVector(78150, -5850, RZ + 340), FVector(77000, -5290, RZ + 380)},
        {TEXT("RubbleStairPlus"), FVector(79250, -5850, RZ + 340), FVector(80400, -5290, RZ + 380)},
        {TEXT("RubbleNave"), FVector(78700, -5900, RZ + 220), FVector(78700, -7700, RZ + 160)},
        {TEXT("RubbleCave"), FVector(78700, -7400, RZ + 200), FVector(78550, -6700, RTop + 300)},
        {TEXT("RubblePier"), FVector(78450, -6250, RZ + 200), FVector(77900, -6900, RZ + 80)},
        {TEXT("RubblePierPlus"), FVector(79200, -5700, RZ + 200), FVector(79700, -5150, RZ + 100)},
        {TEXT("RubbleLane"), FVector(79950, -6450, RZ + 150), FVector(77300, -6250, RZ + 40)},
        {TEXT("RubbleMouth"), FVector(78000, -6150, RZ + 160), FVector(PileS1, CrawlT, RZ + 60)},
        {TEXT("RubbleMouthLow"), FVector(PileS1 + 220, CrawlT - 80, RZ + 45), FVector(PileS1, CrawlT, RZ + 22)},
        {TEXT("RubbleArch"), FVector(78700, -5100, RZ + 500), FVector(78700, RT1, RUZ + 140)},
        {TEXT("RubbleDais"), FVector(78100, -6400, RZ + 300), FVector(78700, -8600, RZ + 400)},
        {TEXT("RubbleSea"), FVector(78160, -6560, RZ + 600), FVector(78083, -6720, RZ - 300)},
        {TEXT("RubbleColossus"), FVector(78450, -7500, RZ + 260), FVector(77940, -8470, RZ + 450)},
        {TEXT("RubbleTorch"), FVector(77450, -7350, RZ + 140), FVector(77080, -7700, RZ + 40)},
        {TEXT("RubbleBowl"), FVector(78450, -6950, RZ + 220), FVector(78030, -7150, RZ + 30)},
        {TEXT("RubbleCracks"), FVector(78600, -6900, RZ + 350), FVector(77000, -7050, RZ + 650)}};
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(74);
    Chuck->SetActorHiddenInGame(true);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->SetViewTarget(Camera); PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }
    Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Rubble));

    enum class ET : uint8 { Lane, Crawl, RiftFall };
    static const ET Trials[] = {ET::Lane, ET::Crawl, ET::RiftFall};
    static const TCHAR* Names[] = {TEXT("lane"), TEXT("crawl"), TEXT("rift_fall")};
    struct FRun
    {
        int32 TrapSounds0 = -1, HallSounds0 = -1;
        int32 Phase = 0, Trial = -1, Sub = 0, Point = 0, Failures = 0, Deaths = 0, Shots = 0, Crawls0 = 0;
        double Next = 0, Start = 0, At = -1; float MinHalf = 99, MaxHalfIn = 0; int32 InFrames = 0, LowFrames = 0;
        bool bStarted = false, bPass = false, bDone = false, bEnd = false;
        TArray<int32> Results; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Failures; Run->Camera = Camera;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [Owner, Chuck, Run, Views]()
    {
        if (Run->bDone) return;
        UWorld* World = Owner->GetWorld();
        const double Now = World->GetTimeSeconds();
        // ---- The stills.
        const int32 I = Run->Phase / 2;
        if (I < Views.Num())
        {
            if (Now < Run->Next) return;
            Run->Next = Now + 1.4;
            if (Run->Phase % 2 == 0)
            {
                if (Run->Camera.IsValid()) Run->Camera->SetActorLocationAndRotation(W(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z), (W(Views[I].Aim.X, Views[I].Aim.Y, Views[I].Aim.Z) - W(Views[I].Eye.X, Views[I].Eye.Y, Views[I].Eye.Z)).Rotation());
                ++Run->Phase; return;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / (FString(Views[I].Name) + TEXT(".png")), true, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_VIEW name=%s"), Views[I].Name);
            ++Run->Phase; return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleRubbleViewsOnly"))) { Run->bDone = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_TEST views_only=1 failures=%d"), Run->Failures); FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0); return; }
        auto* M = Chuck->GetCharacterMovement();
        const FVector P = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        const bool Ground = M->IsMovingOnGround();
        const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        auto Push = [&](const FVector& D) { Chuck->SetTestStickWorld(D); Chuck->AddMovementInput(D, 1, true); };
        auto Steer = [&](float S, float T, float Within = 30.f) -> bool
        {
            if (FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(S, T)) < Within) return true;
            Push((W(S, T, P.Z) - Chuck->GetActorLocation()).GetSafeNormal2D()); return false;
        };
        auto Stop = [&]() { Chuck->SetTestStick(FVector2D::ZeroVector); };
        auto Shot = [&](const TCHAR* Name) { if (Run->Shots < 30) { ++Run->Shots; FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("RubblePlay_%s.png"), Name), true, false); } };
        auto Reset = [&](float S, float T, float DS, float DT)
        {
            Chuck->ResetAtLocation(W(S, T, RZ + Stand)); Chuck->SetActorRotation(Dir(DS, DT).Rotation());
            Chuck->SetRunHeld(false); Stop(); Chuck->Recenter();
        };

        // ---- Next trial.
        if (!Run->bStarted)
        {
            if (Now < Run->Next) return;
            ++Run->Trial;
            FString Only;
            while (Run->Trial < UE_ARRAY_COUNT(Trials) && FParse::Value(FCommandLine::Get(), TEXT("ChuckRubbleTrials="), Only) && !Only.Contains(Names[Run->Trial])) { Run->Results.Add(1); ++Run->Trial; }
            if (Run->Trial >= UE_ARRAY_COUNT(Trials))
            {
                const int32 Floating = ChultTempleRubbleFloating(), Unsourced = ChultTempleRubbleUnsourced();
                // Nothing of the trap run or the hall is heard while he is here (user 2026-10-26).
                int32 TrapSounds = 0, HallSounds = 0;
                for (TActorIterator<AChultTempleTraps> It(World); It; ++It) TrapSounds += It->SoundsPlayed;
                if (const ATempleSanctumBattle* Bt = ATempleSanctumBattle::Get(World)) HallSounds = Bt->GetSoundsPlayed();
                const int32 Heard = (TrapSounds - Run->TrapSounds0) + (HallSounds - Run->HallSounds0);
                int32 Fails = Run->Failures + (Floating > 0) + (Heard > 0) + (Unsourced > 0); FString Line = FString::Printf(TEXT(" floating=%d heard=%d"), Floating, Heard);
                for (int32 K = 0; K < Run->Results.Num(); ++K) { Fails += !Run->Results[K]; Line += FString::Printf(TEXT(" %s=%d"), Names[K], Run->Results[K]); }
                Run->bDone = true; Stop();
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_TEST failures=%d%s blocks=%d drums=%d rifts=%d unsourced=%d"), Fails, *Line, ChultTempleRubbleBlocks().Num(), ChultTempleRubbleDrums().Num(), int32(UE_ARRAY_COUNT(Rifts)), Unsourced);
                FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0);
                return;
            }
            if (Run->Camera.IsValid()) Run->Camera->Destroy();
            Chuck->SetActorHiddenInGame(false);
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Chuck);
            Run->Sub = Run->Point = 0; Run->bPass = Run->bEnd = false; Run->At = -1; Run->MinHalf = 99; Run->MaxHalfIn = 0; Run->InFrames = Run->LowFrames = 0;
            Run->Deaths = Chuck->GetFallDeaths(); Run->Crawls0 = Chuck->GetCrawls(); Chuck->SetSanity(AChuckCharacter::MaxSanity);
            switch (Trials[Run->Trial])
            {
            case ET::Lane: Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Rubble)); Stop(); break;
            case ET::Crawl:
            {
                Reset(PileS1 + 230, CrawlT, -1, 0);
                // The way in is open: a trace low along the passage from the apron to the bend meets nothing.
                FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(Chuck);
                for (float Z : {6.f, 20.f, 34.f})
                    if (World->LineTraceSingleByChannel(Hit, W(PileS1 + 60, CrawlT, RZ + Z), W(CrawlBendS + 10, CrawlT, RZ + Z), ECC_Pawn, Q))
                    { ++Run->Failures; UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_RUBBLE_PASSAGE_HIT z=%.0f at=%s comp=%s actor=%s"), Z, *ChultTempleInteriorFrameOf(Hit.ImpactPoint).ToString(), *GetNameSafe(Hit.GetComponent()), *GetNameSafe(Hit.GetActor())); }
                break;
            }
            case ET::RiftFall: Reset(Lane[1].S, Lane[1].T, 0, 1); break;
            }
            Run->Start = Now; Run->bStarted = true; Run->Next = Now;
            if (Run->TrapSounds0 < 0)
            {
                Run->TrapSounds0 = 0; Run->HallSounds0 = 0;
                for (TActorIterator<AChultTempleTraps> It(World); It; ++It) Run->TrapSounds0 += It->SoundsPlayed;
                if (const ATempleSanctumBattle* Bt = ATempleSanctumBattle::Get(World)) Run->HallSounds0 = Bt->GetSoundsPlayed();
            }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_TRIAL_START name=%s at=%s"), Names[Run->Trial], *ChultTempleInteriorFrameOf(Chuck->GetActorLocation()).ToString());
            return;
        }
        const float Time = static_cast<float>(Now - Run->Start);
        const int32 NewDeaths = Chuck->GetFallDeaths() - Run->Deaths;
        bool Done = false; float Limit = 60;
        switch (Trials[Run->Trial])
        {
        case ET::Lane:
        {
            // The lane's corners in turn, to just before the crevice's mouth (still standing).
            Limit = 90;
            if (Time < .6f) break;
            constexpr int32 N = UE_ARRAY_COUNT(Lane);
            const FVector2D Goal = Run->Point < N - 1 ? FVector2D(Lane[Run->Point + 1].S, Lane[Run->Point + 1].T) : FVector2D(PileS1 + 200, CrawlT);
            if (Run->Point == N - 2 && Steer(PileS1 + 200, CrawlT, 25)) { Done = true; Run->bPass = NewDeaths == 0 && !Chuck->IsCrawling(); }
            else if (Run->Point < N - 2 && Steer(Goal.X, Goal.Y, 35)) { ++Run->Point; if (Run->Point == 3) Shot(TEXT("LaneMid")); }
            if (NewDeaths) Done = true;
            if (Done) { Shot(TEXT("LaneEnd")); UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_LANE corner=%d deaths=%d seconds=%.1f"), Run->Point, NewDeaths, Time); }
            break;
        }
        case ET::Crawl:
        {
            // In at the mouth, along run A to the bend, along run B to the end; then back and out into the chamber.
            Limit = 90;
            if (Time < .6f) break;
            const bool bInPassage = P.X < PileS1 - 40;
            if (bInPassage) { ++Run->InFrames; Run->MaxHalfIn = FMath::Max(Run->MaxHalfIn, Half); Run->LowFrames += Chuck->IsCrawling(); }
            Run->MinHalf = FMath::Min(Run->MinHalf, Half);
            if (Run->Sub == 0) { if (Steer(CrawlBendS, CrawlT, 12)) Run->Sub = 1; if (P.X < MouthS - 80 && Run->At < 0) { Run->At = Now; Shot(TEXT("CrawlIn")); } }
            else if (Run->Sub == 1) { if (Steer(CrawlBendS, CrawlEndT + 50, 12)) { Run->Sub = 2; Run->bEnd = AtChultTempleCreviceEnd(Chuck->GetActorLocation()); Shot(TEXT("CrawlEnd")); Run->At = Now; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_CRAWL_END reached=%d seconds=%.1f half=%.1f"), Run->bEnd, Time, Half); } if (P.Y < CrawlT - 150 && P.Y > CrawlT - 200) Shot(TEXT("CrawlBend")); }
            else if (Run->Sub == 2) { Stop(); if (Now - Run->At > 1.5) Run->Sub = 3; }
            else if (Run->Sub == 3) { if (Steer(CrawlBendS, CrawlT, 12)) Run->Sub = 4; }
            else if (Run->Sub == 4) { if (Steer(PileS1 + 220, CrawlT, 20)) { Run->Sub = 5; Run->At = Now; } }
            else { Stop(); if (Now - Run->At > 1.2) Done = true; }
            if (NewDeaths) Done = true;
            if (Done || Time > Limit - .1f)
            {
                Shot(TEXT("CrawlOut"));
                // Down on all fours through the whole passage (low capsule), the end reached, up again outside.
                Run->bPass = NewDeaths == 0 && Run->bEnd && Chuck->GetCrawls() > Run->Crawls0 && Run->MaxHalfIn < 20 && Run->LowFrames == Run->InFrames && Run->InFrames > 60 && !Chuck->IsCrawling() && Half > 32;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_CRAWL crawls=%d end=%d max_half_inside=%.1f low_frames=%d/%d standing_after=%d deaths=%d seconds=%.1f"),
                    Chuck->GetCrawls() - Run->Crawls0, Run->bEnd, Run->MaxHalfIn, Run->LowFrames, Run->InFrames, !Chuck->IsCrawling() && Half > 32, NewDeaths, Time);
                Done = true;
            }
            break;
        }
        case ET::RiftFall:
        {
            // Into the sanctum's +S aisle rupture past the lane's first leg: an Astral death, back where he came to.
            Limit = 20;
            if (Run->Sub == 0) { Steer(FallRiftS, FallRiftT, 5); if (NewDeaths) { Run->Sub = 1; Run->At = Now; Stop(); } }
            else if (Now - Run->At > 1.5f)
            {
                const bool Home = FVector::Dist(Chuck->GetActorLocation(), ChultTempleInteriorSpawn(ETempleInteriorSpawn::Rubble)) < 40 && Ground && !Chuck->IsAstral();
                if (Home || Now - Run->At > 10) { Done = true; Run->bPass = Home && NewDeaths == 1; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_RIFT_FALL home=%d deaths=%d"), Home, NewDeaths); }
            }
            break;
        }
        }
        if (!Done && Time < Limit) return;
        Stop(); Chuck->SetRunHeld(false);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUBBLE_TRIAL name=%s pass=%d seconds=%.1f sub=%d deaths=%d gait=%s at=%s"),
            Names[Run->Trial], Run->bPass, Time, Run->Sub, NewDeaths, Chuck->GetGaitName(), *P.ToString());
        Run->Results.Add(Run->bPass ? 1 : 0); Run->bStarted = false; Run->Next = Now + 1.2;
    }, .016f, true);
}
