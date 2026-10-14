#include "ChultRaptor.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ChultCanopy.h"
#include "ChultLanding.h"
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

// -ChuckRaptorTest (ChultRaptor.h): the real character and the raptor, trial by trial.
void StartChultRaptorReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    AChultRaptor* Raptor = nullptr;
    for (TActorIterator<AChultRaptor> It(World); It; ++It) Raptor = *It;
    if (!Chuck || !Raptor)
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_RAPTOR_TEST_COMPLETE failures=1 missing_raptor=%d"), Raptor == nullptr ? 1 : 0);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC); Chuck->SetLookLocked(true);
    const FVector G = AChultLanding::FloorOrigin();
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckRaptorCapture"));
    const float Radius = Raptor->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half = Raptor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

    // Static: its round is clear for its body, segment by segment.
    int32 Static = 0;
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(RaptorRoute), false, Raptor); Q.AddIgnoredActor(Chuck);
        const auto& P = AChultRaptor::PatrolPoints();
        for (int32 I = 1; I < P.Num(); ++I)
        {
            FHitResult Hit;
            const FVector A = G + P[I - 1] + FVector(0, 0, Half + 30), B = G + P[I] + FVector(0, 0, Half + 30);
            if (World->SweepSingleByChannel(Hit, A, B, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half - 25), Q))
            {
                ++Static;
                UE_LOG(LogTemp, Warning, TEXT("CHUCK_RAPTOR_ROUTE_BLOCK segment=%d by=%s at=%s"), I, *GetNameSafe(Hit.GetComponent()), *(Hit.ImpactPoint - G).ToString());
            }
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_ROUTE failures=%d segments=%d"), Static, P.Num() - 1);
    }
    enum EKind { KPatrol, KChase, KDodge, KUp, KOver, KTerritory, KDeath, KKill };
    struct FTrial { EKind Kind; const TCHAR* Name; };
    static const FTrial Trials[] = {{KPatrol, TEXT("patrol")}, {KChase, TEXT("notice_chase_bite")}, {KDodge, TEXT("dodge_the_tell")},
        {KUp, TEXT("escape_up_a_tree")}, {KOver, TEXT("brachiate_over_it")}, {KTerritory, TEXT("leave_its_territory")},
        {KDeath, TEXT("killed_returns_to_ruin_top")}, {KKill, TEXT("ten_scratches")}};
    static constexpr int32 Count = UE_ARRAY_COUNT(Trials);
    struct FRun
    {
        int32 Trial = 0, Failures = 0, Phase = 0, Bites = 0, Lunges = 0, Snaps = 0, Shot = 0;
        float Start = -1, Tick = -1, LastPress = -10, NextShot = 0;
        FVector From = FVector::ZeroVector;
        bool bSawChase = false, bSawProwl = false, bSawReturn = false;
        FTimerHandle Timer;
        ACameraActor* Camera = nullptr;
    };
    auto Run = MakeShared<FRun>(); Run->Failures = Static;
    World->GetTimerManager().SetTimer(Run->Timer, [World, Chuck, Raptor, PC, Run, G, bCapture]()
    {
        if (Run->Trial >= Count) return;
        const float Now = World->GetTimeSeconds(); if (Now == Run->Tick) return; Run->Tick = Now;
        const FTrial& Trial = Trials[Run->Trial];
        const auto& Pts = AChultRaptor::PatrolPoints();
        auto* Move = Chuck->GetCharacterMovement();
        const FVector P1 = G + Pts[1], P2 = G + Pts[2], Along = (P2 - P1).GetSafeNormal2D();
        const auto& Line = ChultCanopyChains()[5];   // tree 4 to tree 5
        const auto& T4 = ChultCanopyTrees()[4];
        if (Run->Start < 0)
        {
            Run->Start = Now; Run->Phase = 0; Run->LastPress = -10;
            Run->bSawChase = Run->bSawProwl = Run->bSawReturn = false;
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false); Chuck->SetSanity(5);
            auto Ground = [&](const FVector& At, const FVector& Facing)
            {
                Chuck->ResetAtLocation(At + FVector(0, 0, 34.65f)); Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling);
                Chuck->SetActorRotation(Facing.Rotation()); Chuck->Recenter();
            };
            switch (Trial.Kind)
            {
            case KPatrol:
                Ground(AChultLanding::Entrance() - FVector(0, 0, 34.65f), FVector::ForwardVector);
                Raptor->ResetTo(P1 + FVector(0, 0, 80), Along.Rotation().Yaw);
                break;
            case KChase: case KDeath: case KKill:
                Raptor->ResetTo(P1 + FVector(0, 0, 80), Along.Rotation().Yaw);
                Ground(P1 + Along * (Trial.Kind == KChase ? 900.f : 500.f), -Along);
                if (Trial.Kind == KDeath) Chuck->SetSanity(2);
                break;
            case KDodge:
                Raptor->ResetTo(P1 + FVector(0, 0, 80), Along.Rotation().Yaw);
                Ground(P1 + Along * 600.f, -Along);
                break;
            case KUp:
            {
                // On tree 4's limb toward tree 5, the raptor on the ground beside it.
                const FVector Limb = G + ChultCanopyLimbTop(4, 0, 230);
                Chuck->ResetAtLocation(Limb + FVector(0, 0, 36)); Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling);
                const FVector Side(-T4.Limbs[0].Y, T4.Limbs[0].X, 0);
                Raptor->ResetTo(FVector(Limb.X, Limb.Y, G.Z) + Side * 350.f + FVector(0, 0, 80), 0);
                break;
            }
            case KOver:
            {
                // Under the first grip of the chain from tree 4; across to tree 5, the raptor below.
                Chuck->ResetAtLocation(G + ChultCanopyLimbTop(4, Line.LimbA, FVector::Dist2D(Line.Paws[0], T4.Trunk)) + FVector(0, 0, 36));
                Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling);
                Chuck->SetActorRotation((Line.Paws[1] - Line.Paws[0]).GetSafeNormal2D().Rotation());
                const FVector Mid = G + (Line.Paws[0] + Line.Paws.Last()) * .5f;
                Raptor->ResetTo(FVector(Mid.X, Mid.Y, G.Z + 80), 0);
                break;
            }
            case KTerritory:
                Raptor->ResetTo(P1 + FVector(0, 0, 80), Along.Rotation().Yaw);
                Ground(P1 + Along * 700.f, -Along);
                break;
            }
            Run->Bites = Raptor->GetBites(); Run->Lunges = Raptor->GetLunges(); Run->Snaps = Raptor->GetSnaps(); Run->From = Raptor->GetActorLocation();
        }
        const float T = Now - Run->Start;
        const auto State = Raptor->GetState();
        Run->bSawChase |= State == AChultRaptor::EState::Chase;
        Run->bSawProwl |= State == AChultRaptor::EState::Prowl;
        Run->bSawReturn |= State == AChultRaptor::EState::Return;
        bool Pass = false, Done = false; float Limit = 20;
        switch (Trial.Kind)
        {
        case KPatrol:
            Pass = FVector::Dist2D(Raptor->GetActorLocation(), Run->From) > 400.f && !Run->bSawChase
                && (State == AChultRaptor::EState::Patrol);
            Done = T > 9.f;
            break;
        case KChase:
            // He stands still in the open: it notices, runs him down (faster than his run), gives its tell and bites.
            Pass = Run->bSawChase && Raptor->GetTopSpeed() > 300.f && Raptor->GetLastWindupSeconds() >= .25f
                && Raptor->GetBites() > Run->Bites && Chuck->GetSanity() == 5 - AChultRaptor::BiteDamage;
            Done = Pass;
            break;
        case KDodge:
            // A roll the moment the tell starts: the lunge misses.
            if (Run->Phase == 0 && State == AChultRaptor::EState::Windup) { Chuck->DodgeToward(FVector2D(1, 0)); Run->Phase = 1; }
            if (Run->Phase == 1 && State == AChultRaptor::EState::Recover) Run->Phase = 2;
            Pass = Run->Phase == 2 && Raptor->GetLunges() > Run->Lunges && Raptor->GetBites() == Run->Bites && Chuck->GetSanity() == 5;
            Done = Run->Phase == 2;
            break;
        case KUp:
            // Up on a limb: it prowls under him, snapping, and gives up without a bite.
            Pass = Run->bSawProwl && Raptor->GetSnaps() > Run->Snaps && Run->bSawReturn && Raptor->GetBites() == Run->Bites && Chuck->GetSanity() == 5;
            Done = Pass; Limit = 22;
            break;
        case KOver:
        {
            // Across a chain over it, stick let go: no bite on the way.
            const int32 Goal = GetChuckSwingGrips().IndexOfByPredicate([&](const FChuckSwingGrip& Gp) { return FVector::Dist(Gp.Grip, G + Line.Paws.Last()) < 1; });
            if (Run->Phase == 0 && T > .8f && Move->IsMovingOnGround()) { Chuck->JumpPressed(); Run->Phase = 1; Run->LastPress = T; }
            if (Chuck->IsSwinging() && Chuck->GetSwingGrip() != Goal && T - Run->LastPress > .45f) { Chuck->JumpPressed(); Run->LastPress = T; }
            Pass = Chuck->IsSwinging() && Chuck->GetSwingGrip() == Goal && Raptor->GetBites() == Run->Bites && Chuck->GetSanity() == 5;
            Done = Pass && T - Run->LastPress > .6f;
            break;
        }
        case KTerritory:
            // Chased, he leaves for the landing: it turns back.
            if (Run->Phase == 0 && Run->bSawChase && T > 1.5f)
            { Chuck->ResetAtLocation(AChultLanding::Entrance()); Move->StopMovementImmediately(); Run->Phase = 1; }
            Pass = Run->Phase == 1 && (State == AChultRaptor::EState::Return || State == AChultRaptor::EState::Patrol);
            Done = Pass;
            break;
        case KDeath:
            // Two sanity left: its bite kills him; he comes back on the ruin top, out of its reach.
            Pass = Raptor->GetBites() > Run->Bites && !Chuck->IsAstral() && Move->IsMovingOnGround()
                && FVector::Dist2D(Chuck->GetActorLocation(), ChultCanopyRespawn()) < 30 && !Raptor->CanReach(Chuck);
            Done = Pass && T > 1.f;
            break;
        case KKill:
        {
            // He stands his ground (kept sane) and scratches at it: ten, and it dies.
            Chuck->SetSanity(5);
            const FVector To = Raptor->GetActorLocation() - Chuck->GetActorLocation();
            // As a player would: in while it recovers from a lunge, a scratch whenever it is in reach.
            const float Reach = AChuckCharacter::SlashReach + AChultRaptor::HitRadius - 10.f;
            Chuck->SetTestStick(FVector2D::ZeroVector);
            if (!Raptor->IsDead() && State == AChultRaptor::EState::Recover && To.Size2D() > Reach - 15.f)
            { Chuck->SetTestStickWorld(To.GetSafeNormal2D()); Chuck->AddMovementInput(To.GetSafeNormal2D(), 1.f, true); }
            if (!Raptor->IsDead() && To.Size2D() < Reach && T - Run->LastPress > .32f && Move->IsMovingOnGround())
            { Chuck->SetActorRotation(To.GetSafeNormal2D().Rotation()); Chuck->Slash(); Chuck->SlashReleased(); Run->LastPress = T; }
            Pass = Raptor->IsDead() && Raptor->GetHitsTaken() >= AChultRaptor::Health;
            Done = Pass && T - Run->LastPress > 1.5f; Limit = 40;
            break;
        }
        }
        if (bCapture && T >= Run->NextShot)
        {
            if (!Run->Camera) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(60); }
            // On its round it fills the frame (a portrait); otherwise both, from the side.
            const bool bPortrait = Trial.Kind == KPatrol;
            const FVector Mid = bPortrait ? Raptor->GetActorLocation() + FVector(0, 0, 20) : (Raptor->GetActorLocation() + Chuck->GetActorLocation()) * .5f;
            FVector Side = bPortrait ? FVector::CrossProduct(Raptor->GetActorForwardVector(), FVector::UpVector)
                : FVector::CrossProduct((Raptor->GetActorLocation() - Chuck->GetActorLocation()).GetSafeNormal2D(), FVector::UpVector);
            if (Side.IsNearlyZero()) Side = FVector::RightVector;
            // Him up a tree over it: from farther out, level with the gap between them.
            const bool bHigh = Chuck->GetActorLocation().Z - Raptor->GetActorLocation().Z > 200.f;
            if (bHigh && FMath::Abs(FVector::DotProduct(Side, FVector::UpVector)) < .1f && (Raptor->GetActorLocation() - Chuck->GetActorLocation()).Size2D() < 80.f)
                Side = FVector::CrossProduct(Raptor->GetActorForwardVector(), FVector::UpVector);
            const FVector Eye = Mid + Side * (bPortrait ? 380.f : bHigh ? 1100.f : 650.f) + Raptor->GetActorForwardVector() * (bPortrait ? 120.f : 0.f) + FVector(0, 0, bPortrait ? 60 : bHigh ? -60 : 160);
            Run->Camera->SetActorLocationAndRotation(Eye, (Mid - Eye).Rotation()); PC->SetViewTarget(Run->Camera);
            const FString Folder = FPaths::ScreenShotDir() / TEXT("Raptor"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("T%d_%s_%03d.png"), Run->Trial, Trial.Name, Run->Shot++), false, false);
            Run->NextShot += .4f;
        }
        if (!Done && T < Limit) return;
        Run->Failures += !Pass;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_TRIAL trial=%d name=%s pass=%d time=%.2f state=%s bites=%d lunges=%d snaps=%d hits=%d windup=%.2f top_speed=%.0f sanity=%d chuck=%s raptor=%s"),
            Run->Trial, Trial.Name, Pass ? 1 : 0, T, Raptor->GetStateName(), Raptor->GetBites() - Run->Bites, Raptor->GetLunges() - Run->Lunges, Raptor->GetSnaps() - Run->Snaps,
            Raptor->GetHitsTaken(), Raptor->GetLastWindupSeconds(), Raptor->GetTopSpeed(), Chuck->GetSanity(),
            *(Chuck->GetActorLocation() - G).ToString(), *(Raptor->GetActorLocation() - G).ToString());
        ++Run->Trial; Run->Start = -1; Run->Shot = 0; Run->NextShot = 0;
        if (Run->Trial >= Count)
        {
            Run->Failures += Raptor->GetSoundsPlayed() < 10;   // footfalls, the call, the hiss, snaps, the death cry
            UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_TEST_COMPLETE failures=%d trials=%d sounds=%d"), Run->Failures, Count, Raptor->GetSoundsPlayed());
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }, .016f, true);
}
