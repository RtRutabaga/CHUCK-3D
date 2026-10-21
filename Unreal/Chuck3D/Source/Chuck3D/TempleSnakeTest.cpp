#include "TempleSnake.h"
#include "ChuckCharacter.h"
#include "ChultTempleInterior.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// -ChuckSnakeTest (TempleSnake.h): the real character and the temple snakes, trial by trial.
void StartTempleSnakeReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    TArray<ATempleSnake*> All;
    for (TActorIterator<ATempleSnake> It(World); It; ++It) All.Add(*It);
    if (!Chuck || All.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_TEMPLE_SNAKE_TEST_COMPLETE failures=1 snakes=%d"), All.Num());
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC); Chuck->SetLookLocked(true);
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckSnakeCapture"));
    // The snake whose post is nearest a frame point (S, T).
    auto At = [All](float S, float T)
    {
        ATempleSnake* Best = nullptr; float BestD = 1e9f;
        for (ATempleSnake* K : All)
        {
            const FVector F = ChultTempleInteriorFrameOf(K->GetPost().Feet);
            const float D = FVector2D::Distance(FVector2D(F.X, F.Y), FVector2D(S, T));
            if (D < BestD) { BestD = D; Best = K; }
        }
        return Best;
    };
    enum EKind { KPlace, KChase, KStrike, KRoll, KSide, KScratch, KLeash };
    struct FTrial { EKind Kind; const TCHAR* Name; float S, T; };
    static const FTrial Trials[] = {{KPlace, TEXT("placement"), 0, 0}, {KChase, TEXT("notice_chase_crawl"), 66900, -6500},
        {KStrike, TEXT("strike_lands"), 66300, -7000}, {KRoll, TEXT("roll_dodges"), 66300, -7000}, {KSide, TEXT("step_aside_at_strike"), 66300, -7000},
        {KScratch, TEXT("one_scratch_kills"), 67300, -7100}, {KLeash, TEXT("kept_to_the_chamber"), 66900, -6200}};
    static constexpr int32 Count = UE_ARRAY_COUNT(Trials);
    struct FRun
    {
        int32 Trial = 0, Failures = 0, Phase = 0, Shot = 0, Bites = 0, Strikes = 0;
        float Start = -1, Tick = -1, Mark = 0, NextShot = 0, MinDist = 1e9f;
        bool bSawChase = false, bSawRear = false, bSawReturn = false, bPlaced = false;
        ATempleSnake* Subject = nullptr;
        FTimerHandle Timer;
        ACameraActor* Camera = nullptr;
    };
    auto Run = MakeShared<FRun>();
    World->GetTimerManager().SetTimer(Run->Timer, [World, Chuck, PC, Run, All, At, bCapture]()
    {
        if (Run->Trial >= Count) return;
        const float Now = World->GetTimeSeconds(); if (Now == Run->Tick) return; Run->Tick = Now;
        const FTrial& Trial = Trials[Run->Trial];
        auto* Move = Chuck->GetCharacterMovement();
        auto Ground = [&](const FVector& Feet, const FVector& Facing)
        {
            Chuck->ResetAtLocation(FVector(Feet.X, Feet.Y, Feet.Z) + FVector(0, 0, 34.65f)); Move->StopMovementImmediately();
            Chuck->SetActorRotation(Facing.GetSafeNormal2D().Rotation()); Chuck->Recenter();
        };
        if (Run->Start < 0)
        {
            Run->Start = Now; Run->Phase = 0; Run->MinDist = 1e9f; Run->bSawChase = Run->bSawRear = Run->bSawReturn = false;
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false); Chuck->SetSanity(5);
            for (ATempleSnake* K : All) { K->ResetToPost(); K->SetHeld(true); }
            Run->Subject = Trial.Kind == KPlace ? nullptr : At(Trial.S, Trial.T);
            if (ATempleSnake* K = Run->Subject)
            {
                K->SetHeld(false);
                const FVector Head = K->SnoutTip(), Out = (Head - K->GetPost().Feet).GetSafeNormal2D();
                const FVector Floor(Head.X, Head.Y, K->GetPost().Feet.Z);
                switch (Trial.Kind)
                {
                case KChase: case KLeash: Ground(Floor + Out * 340.f, -Out); break;
                case KStrike: case KRoll: case KSide: Ground(Floor + Out * 58.f, -Out); break;
                case KScratch:
                {
                    // Beside its coil, facing it: one scratch.
                    const FVector C = K->GetPost().Feet; Ground(C + Out * 52.f, -Out); break;
                }
                default: break;
                }
                Run->Bites = K->GetBites(); Run->Strikes = K->GetStrikes();
            }
        }
        const float T = Now - Run->Start;
        ATempleSnake* K = Run->Subject;
        bool Pass = false, Done = false; float Limit = 15;
        if (K)
        {
            const auto S = K->GetState();
            Run->bSawChase |= S == ATempleSnake::EState::Chase;
            Run->bSawRear |= S == ATempleSnake::EState::Rear;
            Run->bSawReturn |= S == ATempleSnake::EState::Return;
            Run->MinDist = FMath::Min(Run->MinDist, static_cast<float>(FVector::Dist2D(K->SnoutTip(), Chuck->GetActorLocation())));
        }
        const int32 Lost = 5 - Chuck->GetSanity();
        switch (Trial.Kind)
        {
        case KPlace:
        {
            // All twenty coiled at their posts: on the floor, in the chamber, clear of the piers.
            if (T < 1.5f) return;
            if (Run->Phase == 0)
            {
                int32 Bad = 0, Coiled = 0;
                TArray<FBox2D> Obstacles; ChultTempleSnakeObstacles(Obstacles);
                for (ATempleSnake* S : All)
                {
                    float Span = 0, Off = 0; bool bInside = false;
                    for (const FVector& P : S->BodyPoints())
                    {
                        Span = FMath::Max(Span, static_cast<float>(FVector::Dist2D(P, S->GetPost().Feet)));
                        Off = FMath::Max(Off, FMath::Abs(static_cast<float>(P.Z - S->GetPost().Feet.Z)));
                        const FVector F = ChultTempleInteriorFrameOf(P);
                        for (const FBox2D& B : Obstacles) bInside |= B.IsInside(FVector2D(F.X, F.Y));
                    }
                    const bool bOk = S->BodyPoints().Num() > 25 && Off < 2.f && !bInside && S->GetState() == ATempleSnake::EState::Rest;
                    Coiled += Span < 30.f; Bad += !bOk;
                    if (!bOk) UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_SNAKE_PLACE_BAD at=%s span=%.0f off=%.1f inside=%d"), *ChultTempleInteriorFrameOf(S->GetPost().Feet).ToString(), Span, Off, bInside);
                }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_PLACEMENT snakes=%d coiled=%d bad=%d"), All.Num(), Coiled, Bad);
                Run->bPlaced = All.Num() == 20 && Coiled == 20 && Bad == 0;
                Run->Phase = 1; Run->Mark = T;
            }
            Pass = Run->bPlaced; Done = !bCapture || T - Run->Mark > 2.f;
            if (bCapture)
            {
                if (!Run->Camera) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(50); }
                ATempleSnake* S = At(66900, -6500);
                const FVector C = S->GetPost().Feet;
                const FVector Eye = T - Run->Mark < 1.f ? C + FVector(90, 60, 55) : ChultTempleInteriorWorldOf(FVector(65980, -5700, -1460 + 240));
                const FVector Look = T - Run->Mark < 1.f ? C + FVector(0, 0, 4) : ChultTempleInteriorWorldOf(FVector(67200, -7600, -1460));
                Run->Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation()); PC->SetViewTarget(Run->Camera);
            }
            break;
        }
        case KChase:
            // He comes within its sense: it lifts its head, uncoils and comes for him at its pace, its body keeping to its track.
            Pass = Run->bSawChase && Run->bSawRear && K->GetTopSpeed() > 85.f && K->GetTopSpeed() < 105.f && K->GetSlipSamples() > 30 && K->GetSideSlip() < .3f;
            Done = Run->bSawRear; Limit = 12;
            break;
        case KStrike:
            // He stands his ground in front of it: the tell, the strike, the bite.
            Pass = K->GetStrikes() > Run->Strikes && K->GetBites() > Run->Bites && Lost == ATempleSnake::BiteDamage && K->GetLastTell() >= .3f;
            Done = Pass && K->GetState() != ATempleSnake::EState::Strike; Limit = 8;
            break;
        case KRoll:
            // A roll out as it rears: the strike finds nothing.
            if (Run->Phase == 0 && K->GetState() == ATempleSnake::EState::Rear) { Chuck->DodgeToward(FVector2D(1, 0)); Run->Phase = 1; }
            if (Run->Phase == 1 && K->GetStrikes() > Run->Strikes && K->GetState() == ATempleSnake::EState::Recoil) Run->Phase = 2;
            Pass = Run->Phase >= 1 && K->GetBites() == Run->Bites && Lost == 0;
            Done = Run->Phase == 2 || (Run->Phase == 1 && T > 4.f); Limit = 8;
            break;
        case KSide:
            // A step aside just as it strikes: committed, it bites the air where he was.
            if (Run->Phase == 0 && K->GetState() == ATempleSnake::EState::Strike)
            {
                const FVector Away = (Chuck->GetActorLocation() - K->SnoutTip()).GetSafeNormal2D();
                Ground(Chuck->GetActorLocation() - FVector(0, 0, 34.65f) + FVector(-Away.Y, Away.X, 0) * 45.f, -Away); Run->Phase = 1;
            }
            if (Run->Phase == 1 && K->GetState() == ATempleSnake::EState::Recoil) Run->Phase = 2;
            Pass = Run->Phase == 2 && K->GetBites() == Run->Bites && Lost == 0 && K->GetLastStrikeGap() > ATempleSnake::BiteSlack;
            Done = Run->Phase == 2; Limit = 8;
            break;
        case KScratch:
        {
            // One scratch kills it: it thrashes, rolls half over and lies still, mouth open.
            if (Run->Phase == 0 && T > .3f && Move->IsMovingOnGround()) { Chuck->Slash(); Chuck->SlashReleased(); Run->Phase = 1; Run->Mark = T; }
            float Off = 0; for (const FVector& P : K->BodyPoints()) Off = FMath::Max(Off, FMath::Abs(static_cast<float>(P.Z - K->GetPost().Feet.Z)));
            Pass = K->IsDead() && T - Run->Mark > 3.5f && Off < 2.f && K->GetGape() > .3f;
            int32 Others = 0; for (ATempleSnake* S : All) Others += S != K && S->IsDead();
            Pass &= Others == 0;
            Done = Run->Phase == 1 && T - Run->Mark > 4.f; Limit = 8;
            break;
        }
        case KLeash:
            // Chased, he leaves the chamber: it stops at its edge, then goes back to its place and lies there.
            if (Run->Phase == 0 && Run->bSawChase && T > 2.f)
            {
                Ground(ChultTempleInteriorWorldOf(FVector(66900, -4600, -1460)), FVector(0, 1, 0)); Run->Phase = 1;
            }
            Pass = Run->Phase == 1 && Run->bSawReturn && K->GetState() == ATempleSnake::EState::Rest && K->GetWorstLeash() < 5.f && Lost == 0;
            Done = Pass; Limit = 30;
            break;
        }
        if (bCapture && K && T >= Run->NextShot)
        {
            if (!Run->Camera) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(50); }
            // Low, from the side of the line between its head and him.
            const FVector Head = K->SnoutTip();
            const FVector Mid = (Head + Chuck->GetActorLocation() - FVector(0, 0, 25)) * .5f;
            FVector Side = FVector::CrossProduct((Head - Chuck->GetActorLocation()).GetSafeNormal2D(), FVector::UpVector);
            if (Side.IsNearlyZero()) Side = FVector::RightVector;
            const FVector Eye = Mid + Side * 190.f + FVector(0, 0, 45);
            Run->Camera->SetActorLocationAndRotation(Eye, (Mid - Eye).Rotation()); PC->SetViewTarget(Run->Camera);
            const FString Folder = FPaths::ScreenShotDir() / TEXT("Snake"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("T%d_%s_%03d.png"), Run->Trial, Trial.Name, Run->Shot++), false, false);
            const auto S = K->GetState();
            Run->NextShot += S == ATempleSnake::EState::Rear || S == ATempleSnake::EState::Strike || S == ATempleSnake::EState::Recoil ? .07f : .4f;
        }
        else if (bCapture && Trial.Kind == KPlace && Run->Phase == 1 && T >= Run->NextShot)
        {
            const FString Folder = FPaths::ScreenShotDir() / TEXT("Snake"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("T0_%s_%03d.png"), Trial.Name, Run->Shot++), false, false);
            Run->NextShot = T + .5f;
        }
        if (!Done && T < Limit) return;
        Run->Failures += !Pass;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_TRIAL trial=%d name=%s pass=%d time=%.2f state=%s strikes=%d bites=%d tell=%.2f gap=%.1f top_speed=%.0f slip=%.3f slip_samples=%d leash=%.1f min_dist=%.0f sanity=%d sounds=%d"),
            Run->Trial, Trial.Name, Pass ? 1 : 0, T, K ? K->GetStateName() : TEXT("-"), K ? K->GetStrikes() - Run->Strikes : 0, K ? K->GetBites() - Run->Bites : 0,
            K ? K->GetLastTell() : 0.f, K ? K->GetLastStrikeGap() : 0.f, K ? K->GetTopSpeed() : 0.f, K ? K->GetSideSlip() : 0.f, K ? K->GetSlipSamples() : 0,
            K ? K->GetWorstLeash() : 0.f, Run->MinDist, Chuck->GetSanity(), K ? K->GetSoundsPlayed() : 0);
        ++Run->Trial; Run->Start = -1; Run->Shot = 0; Run->NextShot = 0;
        if (Run->Trial >= Count)
        {
            int32 Sounds = 0; for (ATempleSnake* S : All) Sounds += S->GetSoundsPlayed();
            Run->Failures += Sounds < 10;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_TEST_COMPLETE failures=%d trials=%d sounds=%d"), Run->Failures, Count, Sounds);
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }, .016f, true);
}
