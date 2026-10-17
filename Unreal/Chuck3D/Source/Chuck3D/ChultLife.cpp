#include "ChultLife.h"
#include "ChuckCharacter.h"
#include "ChultCanopy.h"
#include "ChultCanyon.h"
#include "ChultExpansion.h"
#include "ChultLanding.h"
#include "ChultRaptor.h"
#include "ChultRuins.h"
#include "DockNPC.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    constexpr int32 ZombieCount = 15;
    // The new raptors' starts on the canopy round (the first stands at point 1, going out):
    // spread along it, two of them walking it the other way.
    constexpr int32 RaptorStarts[][2] = {{0, 1}, {3, -1}, {5, 1}, {8, -1}};
    TArray<FVector> ZombieHomes;
    int32 NoFloor = 0, NotFloor = 0, Crowded = 0;
    FString LastBlocker;

    // Automated runs keep the worlds their checks were written against.
    bool Suppressed()
    {
        const TCHAR* Cmd = FCommandLine::Get();
        if (FParse::Param(Cmd, TEXT("ChuckJungleLifeTest")) || FParse::Param(Cmd, TEXT("ChuckZombieGrabTest")) || FParse::Param(Cmd, TEXT("ChuckRiverZombieTest"))) return false;
        const FString Line(Cmd);
        TArray<FString> Words; Line.ParseIntoArrayWS(Words);
        for (const FString& Word : Words)
            if (Word.StartsWith(TEXT("-Chuck")) && (Word.Contains(TEXT("Test")) || Word.Contains(TEXT("Capture")) || Word.Contains(TEXT("Review")))) return true;
        return false;
    }

    // Open jungle floor for a zombie's feet (local), or false.
    bool ZombieFloor(UWorld* World, const FVector& Local, FVector& Feet)
    {
        const FVector G = AChultLanding::FloorOrigin();
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultZombiePlace), false);
        const FCollisionObjectQueryParams Static(ECC_WorldStatic);
        FHitResult Hit;
        if (!World->LineTraceSingleByObjectType(Hit, G + Local + FVector(0, 0, 800), G + Local - FVector(0, 0, 300), Static, Q)) { ++NoFloor; return false; }
        if (FMath::Abs(Hit.ImpactPoint.Z - G.Z) > 60.f || Hit.ImpactNormal.Z < .85f) { ++NotFloor; LastBlocker = FString::Printf(TEXT("%s z=%.0f nz=%.2f"), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.Z - G.Z, Hit.ImpactNormal.Z); return false; }   // a trunk, root or rock, not the floor
        // Room to stand and turn: nothing solid round its body.
        if (World->OverlapAnyTestByObjectType(Hit.ImpactPoint + FVector(0, 0, 100), FQuat::Identity, Static, FCollisionShape::MakeCapsule(35.f, 60.f), Q)) { ++Crowded; return false; }
        Feet = Hit.ImpactPoint;
        return true;
    }
}

namespace
{
    void PlaceChultZombies(TWeakObjectPtr<UWorld> WorldRef)
    {
        UWorld* World = WorldRef.Get();
        if (!World) return;
        for (TActorIterator<ADockNPC> It(World); It; ++It) if (It->ActorHasTag(TEXT("ChultZombie"))) return;   // once
        const FVector G = AChultLanding::FloorOrigin();
        // Zombies: off to either side of the landing's ground loop (where he walks
        // first), never near the trees' reserve (beyond their 11 m leash) nor on top
        // of where he arrives.
        static const FVector Loop[] = {{0,0,0},{700,0,0},{1600,500,0},{2450,300,0},{3300,-150,0},{4200,500,0},{4600,1350,0},
            {3900,2200,0},{2800,2450,0},{1700,1900,0},{900,1500,0},{100,1100,0},{0,0,0}};   // ChultLanding.cpp's Route
        ZombieHomes.Reset();
        FRandomStream R(20261014);
        int32 Tries = 0;
        while (ZombieHomes.Num() < ZombieCount && Tries++ < 6000)
        {
            const int32 Leg = R.RandRange(1, UE_ARRAY_COUNT(Loop) - 1);
            const FVector A = Loop[Leg - 1], B = Loop[Leg], Along = (B - A).GetSafeNormal2D();
            const FVector Side(-Along.Y, Along.X, 0);
            const FVector Local = FMath::Lerp(A, B, R.FRand()) + Side * (R.FRandRange(450.f, 2200.f) * (R.FRand() < .5f ? -1.f : 1.f));
            if (Local.Size2D() < 2000.f || InChultCanopyReserve(Local, 1400.f) || InChultRuinsReserve(Local, 600.f)
                || InChultExpansion(Local) || InChultCanyon(Local)) continue;
            bool bSpaced = true;
            for (const FVector& Other : ZombieHomes) bSpaced &= FVector::Dist2D(Other, Local) > 800.f;
            if (!bSpaced) continue;
            FVector Feet;
            if (!ZombieFloor(World, Local, Feet)) continue;
            // Turned toward the loop, give or take.
            const FVector Nearest = FMath::ClosestPointOnSegment(Local, A, B);
            const float Yaw = static_cast<float>((Nearest - Local).Rotation().Yaw) + R.FRandRange(-60.f, 60.f);
            if (ADockNPC* Zombie = ADockNPC::SpawnZombie(World, Feet, Yaw))
            {
                Zombie->Tags.Add(TEXT("ChultZombie"));
                ZombieHomes.Add(Feet - G);
            }
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_LIFE zombies=%d tries=%d no_floor=%d not_floor=%d crowded=%d last=%s"), ZombieHomes.Num(), Tries, NoFloor, NotFloor, Crowded, *LastBlocker);
    }
}

const TArray<FVector>& ChultZombieHomes() { return ZombieHomes; }

void SpawnChultLife(UWorld* World)
{
    if (!World || Suppressed()) return;
    for (TActorIterator<AChultRaptor> It(World); It; ++It) if (It->Tags.Contains(TEXT("ChultPack"))) return;   // once
    const FVector G = AChultLanding::FloorOrigin();

    // Raptors: four more on the trees' round.
    const auto& Round = AChultRaptor::PatrolPoints();
    int32 Raptors = 0;
    for (const auto& Start : RaptorStarts)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector At = G + Round[Start[0]] + FVector(0, 0, 110);
        const FVector Next = Round[FMath::Clamp(Start[0] + Start[1], 0, Round.Num() - 1)] - Round[Start[0]];
        const float Yaw = static_cast<float>(Next.Rotation().Yaw);
        if (auto* Raptor = World->SpawnActor<AChultRaptor>(At, FRotator(0, Yaw, 0), Params))
        {
            Raptor->Tags.Add(TEXT("ChultPack"));
            Raptor->ResetTo(At, Yaw);
            Raptor->SetRoundStep(Start[1]);
            ++Raptors;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_LIFE raptors_added=%d"), Raptors);
    // The zombies once the set's collision can be queried (it was built this frame).
    FTimerHandle Later;
    World->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateStatic(&PlaceChultZombies, TWeakObjectPtr<UWorld>(World)), .5f, false);
    // After the road resolves at .6 s and its infill enters the physics scene.
    FTimerHandle RiverLater;
    World->GetTimerManager().SetTimer(RiverLater, FTimerDelegate::CreateStatic(&SpawnChultRiverZombies, World), 1.2f, false);
}

namespace
{
    struct FLifeRun
    {
        int32 Phase = 0, Failures = 0, Frames = 0;   // Phase 5: done
        float Time = 0, FrameTime = 0, WorstFrame = 0, Tick = -1;
        TArray<TWeakObjectPtr<ADockNPC>> Zombies;
        TArray<FVector> RaptorStart;
        TWeakObjectPtr<ADockNPC> Target;
        FTimerHandle Timer;
    };

    void Check(FLifeRun& Run, bool bOk, const TCHAR* What)
    {
        Run.Failures += !bOk;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_LIFE_CHECK %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), What);
    }
}

void StartChultLifeReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    if (!Chuck) { UE_LOG(LogTemp, Error, TEXT("CHUCK_CHULT_LIFE_TEST_COMPLETE failures=1 missing_chuck=1")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC); Chuck->SetLookLocked(true);
    const FVector G = AChultLanding::FloorOrigin();
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckJungleLifeCapture"));
    TSharedRef<FLifeRun> Run = MakeShared<FLifeRun>();

    // Counts and placements.
    for (TActorIterator<ADockNPC> It(World); It; ++It) if (It->ActorHasTag(TEXT("ChultZombie"))) Run->Zombies.Add(*It);
    int32 ZombiesClear = 0, ZombiesOnFloor = 0, ZombiesAway = 0;
    for (const auto& Z : Run->Zombies)
    {
        const FVector Local = Z->GetActorLocation() - G;
        ZombiesClear += !InChultCanopyReserve(Local, 1300.f);
        ZombiesOnFloor += FMath::Abs(Local.Z - 90.f) < 70.f;
        ZombiesAway += Local.Size2D() > 2000.f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_ZOMBIE at=%s yaw=%.0f"), *Local.ToString(), Z->GetActorRotation().Yaw);
    }
    int32 RaptorsIn = 0;
    for (const auto& Raptor : AChultRaptor::All())
    {
        if (!Raptor.IsValid()) continue;
        const FVector Local = Raptor->GetActorLocation() - G;
        RaptorsIn += InChultCanopyReserve(Local, 0.f);
        Run->RaptorStart.Add(Raptor->GetActorLocation());
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_RAPTOR at=%s state=%s"), *Local.ToString(), Raptor->GetStateName());
    }
    Check(*Run, Run->Zombies.Num() == ZombieCount, *FString::Printf(TEXT("zombies=%d of %d"), Run->Zombies.Num(), ZombieCount));
    Check(*Run, ZombiesClear == Run->Zombies.Num(), *FString::Printf(TEXT("zombies clear of the brachiation trees=%d"), ZombiesClear));
    Check(*Run, ZombiesOnFloor == Run->Zombies.Num(), *FString::Printf(TEXT("zombies standing on the floor=%d"), ZombiesOnFloor));
    Check(*Run, ZombiesAway == Run->Zombies.Num(), *FString::Printf(TEXT("zombies away from the landing=%d"), ZombiesAway));
    Check(*Run, Run->RaptorStart.Num() == 5, *FString::Printf(TEXT("raptors=%d of 5"), Run->RaptorStart.Num()));
    Check(*Run, RaptorsIn == Run->RaptorStart.Num(), *FString::Printf(TEXT("raptors among the brachiation trees=%d"), RaptorsIn));

    // Left alone at the landing: the zombies stay at home, the raptors walk their round.
    // Then he is set down near the nearest zombie, in its view, and it comes for him.
    World->GetTimerManager().SetTimer(Run->Timer, FTimerDelegate::CreateLambda([World, Chuck, Run, G, bCapture]()
    {
        if (Run->Phase >= 5) return;
        const float Now = World->GetTimeSeconds(); if (Now == Run->Tick) return; Run->Tick = Now;
        const float Dt = World->GetDeltaSeconds();
        Run->Time += Dt;
        if (Run->Time > .5f) { ++Run->Frames; Run->FrameTime += Dt; Run->WorstFrame = FMath::Max(Run->WorstFrame, Dt); }
        Chuck->SetSanity(5);
        if (Run->Phase == 0 && Run->Time > 10.f)
        {
            int32 Idle = 0, Walked = 0, Patrolling = 0;
            for (const auto& Z : Run->Zombies) Idle += Z.IsValid() && FCString::Strcmp(Z->GetZombieStateName(), TEXT("Idle")) == 0;
            int32 I = 0;
            for (const auto& Raptor : AChultRaptor::All())
            {
                if (!Raptor.IsValid() || !Run->RaptorStart.IsValidIndex(I)) continue;
                Walked += FVector::Dist2D(Raptor->GetActorLocation(), Run->RaptorStart[I++]) > 150.f;
                Patrolling += Raptor->GetState() == AChultRaptor::EState::Patrol;
            }
            Check(*Run, Idle == Run->Zombies.Num(), *FString::Printf(TEXT("zombies idle while he is away=%d"), Idle));
            Check(*Run, Walked == Run->RaptorStart.Num() && Patrolling == Run->RaptorStart.Num(), *FString::Printf(TEXT("raptors on their round walked=%d patrol=%d"), Walked, Patrolling));
            // The nearest zombie to the landing.
            float Best = 1e9f;
            for (const auto& Z : Run->Zombies) if (Z.IsValid() && FVector::Dist2D(Z->GetActorLocation(), G) < Best) { Best = FVector::Dist2D(Z->GetActorLocation(), G); Run->Target = Z; }
            if (ADockNPC* Z = Run->Target.Get())
            {
                const FVector Front = Z->GetActorLocation() + Z->GetActorForwardVector().GetSafeNormal2D() * 550.f;
                Chuck->SetActorLocation(FVector(Front.X, Front.Y, G.Z + 40.f), false, nullptr, ETeleportType::TeleportPhysics);
                const FRotator Facing = (Z->GetActorLocation() - Front).GetSafeNormal2D().Rotation();
                Chuck->SetActorRotation(Facing);
                Chuck->Recenter(); Chuck->SetOrbitPitch(-12);
            }
            Run->Phase = 1; Run->Time = 10.f;
        }
        else if (Run->Phase == 1 && Run->Time > 11.2f)
        {
            if (bCapture) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult/Life_Zombie.png"), true, false);
            Run->Phase = 2;
        }
        else if (Run->Phase == 2 && Run->Time > 13.f)
        {
            const ADockNPC* Z = Run->Target.Get();
            const bool bComing = Z && FCString::Strcmp(Z->GetZombieStateName(), TEXT("Idle")) != 0;
            Check(*Run, bComing, *FString::Printf(TEXT("the nearest zombie comes for him state=%s"), Z ? Z->GetZombieStateName() : TEXT("missing")));
            // A look at the raptors' round from beside its first point.
            const FVector At = G + AChultRaptor::PatrolPoints()[2] + FVector(-900, -700, 0);
            Chuck->SetActorLocation(FVector(At.X, At.Y, G.Z + 40.f), false, nullptr, ETeleportType::TeleportPhysics);
            Chuck->SetSanity(5);
            const FRotator Facing = (G + AChultRaptor::PatrolPoints()[3] - At).GetSafeNormal2D().Rotation();
            Chuck->SetActorRotation(Facing); Chuck->Recenter(); Chuck->SetOrbitPitch(-12);
            Run->Phase = 3;
        }
        else if (Run->Phase == 3 && Run->Time > 14.5f)
        {
            if (bCapture) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult/Life_Raptors.png"), true, false);
            Run->Phase = 4;
        }
        else if (Run->Phase == 4 && Run->Time > 15.5f)
        {
            const float Average = Run->Frames ? Run->FrameTime / Run->Frames : 0.f;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_LIFE_FRAMES average_ms=%.1f worst_ms=%.1f frames=%d"), Average * 1000.f, Run->WorstFrame * 1000.f, Run->Frames);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_LIFE_TEST_COMPLETE failures=%d"), Run->Failures);
            Run->Phase = 5;
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }), .016f, true);
}

namespace
{
    struct FGrabRun
    {
        int32 Phase = 0, Failures = 0, Shot = 0, RespawnsBefore = 0;
        float Time = 0, Tick = -1, GrabAt = -1, StartZ = 0, TopZ = -1e9f;
        bool bHeldSeen = false, bBittenWhileHeld = false, bVanished = false;
        TWeakObjectPtr<ADockNPC> Zombie;
        TWeakObjectPtr<ACameraActor> Camera;
        FTimerHandle Timer;
    };
}

void StartZombieGrabReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    const FVector G = AChultLanding::FloorOrigin();
    ADockNPC* Zombie = nullptr; float Best = 1e9f;
    for (TActorIterator<ADockNPC> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("ChultZombie")) && FVector::Dist2D(It->GetActorLocation(), G) < Best) { Best = FVector::Dist2D(It->GetActorLocation(), G); Zombie = *It; }
    if (!Chuck || !Zombie)
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_ZOMBIE_GRAB_TEST_COMPLETE failures=1 missing=%d"), Zombie == nullptr ? 1 : 2);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC); Chuck->SetLookLocked(true);
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckZombieGrabCapture"));
    TSharedRef<FGrabRun> Run = MakeShared<FGrabRun>();
    Run->Zombie = Zombie;
    Run->RespawnsBefore = Chuck->GetRespawns();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_GRAB_CHANCE %.2f"), ADockNPC::GetZombieGrabChance());
    // Set down in front of it, in its view, a step out of its reach: it comes and strikes.
    const FVector Ahead = Zombie->GetActorForwardVector().GetSafeNormal2D();
    const FVector At = Zombie->GetActorLocation() + Ahead * 300.f;
    Chuck->SetActorLocation(FVector(At.X, At.Y, G.Z + 40.f), false, nullptr, ETeleportType::TeleportPhysics);
    Chuck->SetActorRotation((-Ahead).Rotation());
    Chuck->Recenter(); Chuck->SetOrbitPitch(-12);
    Chuck->SetSanity(AChuckCharacter::MaxSanity);
    Run->StartZ = static_cast<float>(G.Z + 34.65f);   // his centre standing on the floor
    if (bCapture)
    {
        // From its side, a little below its head.
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
        const FVector Look = Zombie->GetActorLocation() + Ahead * 60.f + FVector(0, 0, 30.f);
        auto* Cam = World->SpawnActor<ACameraActor>(Look + Side * 330.f + Ahead * 40.f + FVector(0, 0, 20.f), FRotator::ZeroRotator);
        Cam->SetActorRotation((Look - Cam->GetActorLocation()).Rotation());
        if (PC) PC->SetViewTarget(Cam);
        Run->Camera = Cam;
    }
    World->GetTimerManager().SetTimer(Run->Timer, FTimerDelegate::CreateLambda([World, Chuck, Run, G, bCapture, PC]()
    {
        if (Run->Phase >= 9) return;
        const float Now = World->GetTimeSeconds(); if (Now == Run->Tick) return; Run->Tick = Now;
        Run->Time += World->GetDeltaSeconds();
        ADockNPC* Z = Run->Zombie.Get();
        if (!Z) { Run->Phase = 8; }
        if (Run->Phase == 0)
        {
            if (Z->IsGrabbing()) { Run->GrabAt = Run->Time; Run->Phase = 1; }
            else if (Run->Time > 12.f) { UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_GRAB_CHECK FAIL no grab in 12 s state=%s"), Z->GetZombieStateName()); ++Run->Failures; Run->Phase = 8; }
        }
        if (Run->Phase >= 1 && Run->Phase < 8)
        {
            const float T = Run->Time - Run->GrabAt;
            Run->bHeldSeen |= Chuck->IsHeld();
            if (Chuck->IsInGrip()) Run->TopZ = FMath::Max(Run->TopZ, static_cast<float>(Chuck->GetActorLocation().Z));
            Run->bBittenWhileHeld |= Chuck->IsInGrip() && Chuck->GetSanity() == 0;
            Run->bVanished |= Chuck->IsBodyHidden();
            static const float Shots[] = {.3f, .8f, 1.2f, 1.47f, 1.75f, 2.1f};
            if (bCapture && Run->Shot < 6 && T >= Shots[Run->Shot])
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("Chult/ZombieGrab_%d.png"), Run->Shot), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_GRAB_FRAME %d t=%.2f state=%s chuck=%s"), Run->Shot, T, Z->GetZombieStateName(), *(Chuck->GetActorLocation() - G).ToString());
                ++Run->Shot;
            }
            if (bCapture && Run->Shot >= 6 && PC && Run->Camera.IsValid() && PC->GetViewTarget() == Run->Camera.Get()) PC->SetViewTarget(Chuck);
            // Back whole at the landing, and the zombie let go.
            if (Chuck->GetRespawns() > Run->RespawnsBefore && !Chuck->IsAstral() && T > 3.f) Run->Phase = 8;
            else if (T > 12.f) Run->Phase = 8;
        }
        if (Run->Phase == 8)
        {
            auto Check = [&](bool bOk, const FString& What) { Run->Failures += !bOk; UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_GRAB_CHECK %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What); };
            const float Lift = Run->TopZ - Run->StartZ;
            Check(Z && Z->GetGrabs() == 1, FString::Printf(TEXT("seized once grabs=%d"), Z ? Z->GetGrabs() : -1));
            Check(Run->bHeldSeen, TEXT("held"));
            Check(Lift > 70.f, FString::Printf(TEXT("lifted to its face lift=%.0f cm"), Lift));
            Check(Run->bBittenWhileHeld, TEXT("the bite took all his sanity in its hands"));
            Check(Run->bVanished, TEXT("vanished from its hands"));
            Check(Chuck->GetRespawns() > Run->RespawnsBefore && Chuck->GetSanity() == AChuckCharacter::MaxSanity && !Chuck->IsAstral(),
                FString::Printf(TEXT("summoned back whole respawns=%d sanity=%d"), Chuck->GetRespawns() - Run->RespawnsBefore, Chuck->GetSanity()));
            Check(FVector::Dist2D(Chuck->GetActorLocation(), AChultLanding::Entrance()) < 150.f, FString::Printf(TEXT("at the landing dist=%.0f"), FVector::Dist2D(Chuck->GetActorLocation(), AChultLanding::Entrance())));
            Check(Z && !Z->IsGrabbing(), FString::Printf(TEXT("it let go state=%s"), Z ? Z->GetZombieStateName() : TEXT("missing")));
            Check(Z && Z->GetWorstHoldGap() < 18.f, FString::Printf(TEXT("hands on him worst_gap=%.1f cm"), Z ? Z->GetWorstHoldGap() : -1.f));
            UE_LOG(LogTemp, Display, TEXT("CHUCK_ZOMBIE_GRAB_TEST_COMPLETE failures=%d"), Run->Failures);
            Run->Phase = 9;
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }), .016f, true);
}
