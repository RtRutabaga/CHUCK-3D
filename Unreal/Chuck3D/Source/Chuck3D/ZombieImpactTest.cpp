#include "ChuckCharacter.h"
#include "DockNPC.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// Isolated real-character trials on a dedicated floor, away from the docks.
void StartZombieImpactReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    if (!Chuck) { FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Box = [World, Cube](FVector At, FVector Scale)
    {
        auto* Actor = World->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator);
        Actor->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
        Actor->SetActorScale3D(Scale);
        Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        return Actor;
    };
    const FVector Base(24000, 0, 500);
    Box(Base - FVector(0, 0, 25), FVector(30, 30, .5));
    Box(Base + FVector(-35, 0, 75), FVector(.2, 4, 1.5));
    auto* Zombie = ADockNPC::SpawnZombie(World, Base + FVector(100, 0, 0), 180);
    struct FRun { FTimerHandle Timer; int32 Trial = 0, Phase = 0, Failures = 0, Sounds = 0, Bites = 0; float Start = 0, MaxRise = 0; FVector From; };
    auto Run = MakeShared<FRun>();
    Chuck->SetTestStick(FVector2D::ZeroVector);
    World->GetTimerManager().SetTimer(Run->Timer, [World, Chuck, Zombie, Base, Run]()
    {
        if (Run->Phase == 4) return;
        const float Now = World->GetTimeSeconds();
        auto* Move = Chuck->GetCharacterMovement();
        // Walk into the incoming blow in the second trial: it must still displace him away.
        if (Run->Trial == 1 && Run->Phase == 2) Chuck->AddMovementInput(FVector(0, 1, 0), 1.f, true);
        if (Run->Phase == 0)
        {
            Zombie->SetActorTickEnabled(false);
            Chuck->SetSanity(5);
            Chuck->ResetAtLocation(Base + FVector(0, Run->Trial == 2 ? 0 : 400, 34.65));
            Move->SetMovementMode(MOVE_Falling);
            Run->Start = Now; Run->MaxRise = 0; Run->Phase = 1;
        }
        if (Run->Phase == 1 && Now - Run->Start > .4f)
        {
            Run->From = Chuck->GetActorLocation(); Run->Start = Now;
            if (Run->Trial == 3)
            {
                Zombie->SetActorLocation(Base + FVector(100, 400, Zombie->GetActorLocation().Z - Base.Z));
                Zombie->SetActorRotation(FRotator(0, 180, 0));
                Run->Sounds = Zombie->GetZombieSoundsPlayed();
                Run->Bites = Zombie->GetBitesLanded();
                Zombie->SetActorTickEnabled(true);
                Run->Phase = 3;
                return;
            }
            const FVector Source = Run->From + FVector(Run->Trial == 1 ? 0 : 100, Run->Trial == 1 ? 100 : 0, 0);
            const bool Accepted = Chuck->TakeBite(Source, 2, true);
            const bool Duplicate = Chuck->TakeBite(Source, 2, true);
            if (!Accepted || Duplicate || Chuck->GetSanity() != 3) ++Run->Failures;
            const int32 Before = Zombie->GetZombieSoundsPlayed();
            Zombie->TakeScratch(Run->From);
            if (Zombie->GetZombieSoundCount() != 12 || Zombie->GetZombieSoundsPlayed() != Before + 1) ++Run->Failures;
            Run->Phase = 2;
        }
        if (Run->Phase == 3)
        {
            if (Zombie->GetBitesLanded() == Run->Bites && Now - Run->Start < 3.f) return;
            const bool Pass = Zombie->GetBitesLanded() == Run->Bites + 1 && Chuck->GetSanity() == 3
                && Zombie->GetZombieSoundsPlayed() >= Run->Sounds + 2;
            if (!Pass) ++Run->Failures;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_CONTACT_TRIAL pass=%d bites=%d sounds=%d"), Pass,
                Zombie->GetBitesLanded() - Run->Bites, Zombie->GetZombieSoundsPlayed() - Run->Sounds);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_IMPACT_TEST failures=%d trials=4 sounds=%d"), Run->Failures, Zombie->GetZombieSoundCount());
            // Keep the executing delegate alive through its return; the world
            // owns cleanup on exit (clearing it here destroys its captures).
            Run->Phase = 4;
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
            return;
        }
        if (Run->Phase != 2) return;
        Run->MaxRise = FMath::Max(Run->MaxRise, float(Chuck->GetActorLocation().Z - Run->From.Z));
        if (Now - Run->Start < (Run->Trial == 1 ? .22f : .55f)) return;
        const FVector Delta = Chuck->GetActorLocation() - Run->From;
        const float Distance = Run->Trial == 1 ? -Delta.Y : -Delta.X;
        const bool Pass = Distance > (Run->Trial == 2 ? 5.f : 15.f) && Distance < (Run->Trial == 2 ? 18.f : 40.f)
            && Run->MaxRise < 2.f && Move->IsMovingOnGround();
        if (!Pass) ++Run->Failures;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_IMPACT_TRIAL index=%d pass=%d cm=%.2f rise=%.2f"), Run->Trial, Pass, Distance, Run->MaxRise);
        ++Run->Trial; Run->Phase = 0;
    }, .016f, true);
}
