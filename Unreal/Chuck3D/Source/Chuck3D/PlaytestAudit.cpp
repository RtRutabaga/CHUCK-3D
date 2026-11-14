// -ChuckPlaytest[=<checkpoint>] (Claude, 2026-11-13; docs/AUTOMATION-DEFECT-CATALOG.md, category 2: Chuck inside
// geometry, stuck, or out of the world). A robot player: from a dev checkpoint (Waterdeep when none is given) it drives
// Chuck through the stick and buttons the way a curious player does for 90 s of game time at a fixed 30 fps: it heads
// for the least-visited ground within 60 m of the start, changes its mind every few seconds, jumps, sprints and dodges
// at random, leaps at some edges, and turns away when it is blocked. Every frame it checks the three faults a player
// has found by hand:
//
//  - inside: his capsule, shrunk by 4 cm, overlaps something solid (not a pawn, not a physics prop) for three frames;
//  - stuck: eight seconds in which he never gets 60 cm from where he was, although the robot has turned and jumped;
//  - void: four seconds of falling with no floor within 200 m under him.
//
// After each stuck or void it puts him back where he last stood (eight seconds earlier) and goes on. Falls the game
// itself catches (an Astral death, summoned back at the area's start) are listed too, as information. Each fault is
// written with where, when, his gait and what he was in, and the first eight get a screenshot. Before the run a solid
// cube is planted in the sky and the inside test must find it (and nothing just above it), so a blind run cannot pass;
// the robot must also cover ground (cells of 2 m visited). Intended cases go in Content/Data/Audit/Allow.txt as
// "playtest <wildcard on '<kind> <what or place> <gait>'>". Writes Saved/Audit/<Region>-playtest.csv and quits.
#include "CoreMinimal.h"
#include "ChuckCharacter.h"
#include "ChultPropAudit.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

bool ChuckAuditAllowed(const TCHAR* Check, const FString& Key);

namespace
{
    constexpr float Fps = 30.f;
    constexpr int32 Frames = 90 * 30;
    constexpr float Cell = 200.f, Leash = 6000.f, Shrink = 4.f;
    constexpr int32 InsideFrames = 3, StuckFrames = 8 * 30, BlockedFrames = 45, VoidFrames = 4 * 30, MaxShots = 8;
    constexpr float StuckRadius = 60.f, BlockedTravel = 25.f, MergeRadius = 300.f;

    struct FIncident { FString Kind, What, Gait; FVector At; float Time; int32 Count = 1; bool bAllowed = false; };

    struct FRun
    {
        UWorld* World = nullptr;
        FString Region;
        TWeakObjectPtr<AChuckCharacter> Chuck;
        FRandomStream Rng;
        double Start = 0;
        FDelegateHandle Hook;
        int32 Frame = 0;
        FVector Home = FVector::ZeroVector, Heading = FVector::ForwardVector, LastGround = FVector::ZeroVector;
        int32 HeadingSince = 0, HeadingUntil = 0, PutBackAt = 0, JumpRelease = -1, InsideRun = 0, FallingFor = 0, Shots = 0;
        TArray<FVector> Trail;          // his position each frame
        TArray<bool> Counted;           // frames he was the robot's (not in a cutscene, not just moved by the game)
        TArray<FVector> Safe;           // where he stood, every second
        TSet<FIntPoint> Cells;
        float Travel = 0;
        int32 Headings = 0, Blocked = 0, Jumps = 0, Sprints = 0, Dodges = 0, EdgeLeaps = 0, PutBack = 0, GameMoves = 0;
        int32 Falls0 = 0, Respawns0 = 0, Hangs0 = 0, Mantles0 = 0, Ladders0 = 0, WallRuns0 = 0, Vaults0 = 0;
        bool bProbeInside = false, bProbeClear = false;
        TArray<FIncident> Incidents;
    };

    FCollisionQueryParams Query(const AChuckCharacter* Chuck)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ChuckPlaytest), false, Chuck);
        return Q;
    }

    // What solid his (shrunk) capsule is in at At, or empty. Pawns and physics props he pushes don't count.
    FString Embedded(UWorld* World, const AChuckCharacter* Chuck, const FVector& At, float Radius, float Half)
    {
        TArray<FOverlapResult> Hits;
        World->OverlapMultiByChannel(Hits, At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, FMath::Max(Half, Radius)), Query(Chuck));
        for (const FOverlapResult& H : Hits)
        {
            const UPrimitiveComponent* C = H.GetComponent();
            const AActor* O = C ? C->GetOwner() : nullptr;
            if (!C || !H.bBlockingHit || C->IsSimulatingPhysics() || (O && (O->IsA<APawn>() || O->GetAttachParentActor() == Chuck))) continue;
            const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
            return FString::Printf(TEXT("%s %s/%s"), SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : *C->GetClass()->GetName(), O ? *O->GetName() : TEXT("-"), *C->GetName());
        }
        return FString();
    }

    bool Floor(UWorld* World, const AChuckCharacter* Chuck, const FVector& From, float Depth, FHitResult* Out = nullptr)
    {
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, Depth), ECC_Pawn, Query(Chuck));
        if (Out) *Out = Hit;
        return bHit;
    }

    // Toward open, least-visited ground; back toward the start once 60 m out. Avoid: the way that just failed.
    void ChooseHeading(FRun& R, AChuckCharacter* Chuck, const FVector* Avoid)
    {
        const FVector At = Chuck->GetActorLocation();
        FVector Best = R.Heading; float BestScore = -1e9f;
        for (int32 K = 0; K < 12; ++K)
        {
            const FVector D = FVector::ForwardVector.RotateAngleAxis(30.f * K + R.Rng.FRandRange(-10.f, 10.f), FVector::UpVector);
            if (Avoid && FVector::DotProduct(D, *Avoid) > .5f) continue;
            FHitResult Hit;
            const float Clear = R.World->LineTraceSingleByChannel(Hit, At, At + D * 300, ECC_Pawn, Query(Chuck)) ? Hit.Distance : 300.f;
            float Score = Clear + R.Rng.FRandRange(0.f, 150.f);
            // Mostly keep to floor; now and then go for the edge anyway (a player leaps off things).
            if (!Floor(R.World, Chuck, At + D * FMath::Min(Clear, 200.f), 400.f) && R.Rng.FRand() > .15f) Score -= 250.f;
            const FVector Ahead = At + D * 400;
            if (R.Cells.Contains(FIntPoint(FMath::FloorToInt32(Ahead.X / Cell), FMath::FloorToInt32(Ahead.Y / Cell)))) Score -= 120.f;
            if (FVector::Dist2D(At, R.Home) > Leash) Score += 400.f * FVector::DotProduct(D, (R.Home - At).GetSafeNormal2D());
            if (Score > BestScore) { BestScore = Score; Best = D; }
        }
        R.Heading = Best; R.HeadingSince = R.Frame; R.HeadingUntil = R.Frame + R.Rng.RandRange(45, 120); ++R.Headings;
    }

    void Report(FRun& R, const TCHAR* Kind, const FString& What, AChuckCharacter* Chuck, const FVector& At)
    {
        const FString Gait = Chuck->GetGaitName();
        for (FIncident& I : R.Incidents)
            if (I.Kind == Kind && FVector::Dist(I.At, At) < MergeRadius && (I.What == What || FCString::Strcmp(Kind, TEXT("inside")))) { ++I.Count; return; }
        FIncident& I = R.Incidents.AddDefaulted_GetRef();
        I.Kind = Kind; I.What = What; I.Gait = Gait; I.At = At; I.Time = R.Frame / Fps;
        const FString Place = FString::Printf(TEXT("%s %.0f,%.0f"), *R.Region, FMath::RoundToFloat(At.X / 400.f) * 4, FMath::RoundToFloat(At.Y / 400.f) * 4);
        I.bAllowed = ChuckAuditAllowed(TEXT("playtest"), FString::Printf(TEXT("%s %s %s"), Kind, What.IsEmpty() ? *Place : *What, *Gait));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PLAYTEST_%s at=(%.0f,%.0f,%.0f) t=%.1f gait=%s what=%s allowed=%d"), *FString(Kind).ToUpper(), At.X, At.Y, At.Z, I.Time, *Gait,
            What.IsEmpty() ? TEXT("-") : *What, I.bAllowed ? 1 : 0);
        // How he was held: enough to tell a movement fault from a level's.
        const UCharacterMovementComponent* Move = Chuck->GetCharacterMovement();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PLAYTEST_STATE kind=%s mode=%s crouched=%d half=%.1f velocity=%.0f floor=%d floor_what=%s heading=%s collision=%d"), Kind,
            *Move->GetMovementName(), Chuck->bIsCrouched ? 1 : 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Move->Velocity.Size(),
            Move->CurrentFloor.IsWalkableFloor() ? 1 : 0, Move->CurrentFloor.HitResult.GetComponent() ? *Move->CurrentFloor.HitResult.GetComponent()->GetName() : TEXT("-"),
            *R.Heading.ToCompactString(), Chuck->GetActorEnableCollision() ? 1 : 0);
        if (!I.bAllowed && FCString::Strcmp(Kind, TEXT("fall")) && R.Shots < MaxShots)
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Playtest") / FString::Printf(TEXT("%s-%d-%s.png"), *R.Region, ++R.Shots, Kind), false, false);
    }

    // Back to where he stood eight seconds ago (or the start), and the windows begin again.
    void PutBack(FRun& R, AChuckCharacter* Chuck)
    {
        // Never back onto the fault itself (a run started at a finding with -ChuckPlaytestAt): if nowhere else is known, stop.
        const FVector Fault = Chuck->GetActorLocation();
        FVector To = R.Home;
        if (R.Safe.Num() > 8) To = R.Safe[R.Safe.Num() - 9];
        for (int32 K = R.Safe.Num() - 9; FVector::Dist(To, Fault) < 150.f && K-- > 0;) To = R.Safe[K];
        if (FVector::Dist(To, Fault) < 150.f) { R.Frame = Frames - 1; return; }
        Chuck->ResetAtLocation(To);
        ++R.PutBack;
        R.InsideRun = R.FallingFor = 0;
        ChooseHeading(R, Chuck, nullptr);
    }

    void Finish(TSharedRef<FRun> R)
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(R->Hook);
        FApp::SetUseFixedTimeStep(false);
        AChuckCharacter* Chuck = R->Chuck.Get();
        TArray<FString> Rows{TEXT("kind,what,gait,x,y,z,time,count,allowed")};
        int32 Inside = 0, Stuck = 0, Void = 0, Falls = 0, Allowed = 0;
        for (const FIncident& I : R->Incidents)
        {
            Rows.Add(FString::Printf(TEXT("%s,%s,%s,%.0f,%.0f,%.0f,%.1f,%d,%d"), *I.Kind, *I.What.Replace(TEXT(","), TEXT(";")), *I.Gait, I.At.X, I.At.Y, I.At.Z, I.Time, I.Count, I.bAllowed ? 1 : 0));
            if (I.bAllowed) { ++Allowed; continue; }
            Inside += I.Kind == TEXT("inside"); Stuck += I.Kind == TEXT("stuck"); Void += I.Kind == TEXT("void"); Falls += I.Kind == TEXT("fall");
        }
        FFileHelper::SaveStringArrayToFile(Rows, *(FPaths::ProjectSavedDir() / TEXT("Audit") / R->Region + TEXT("-playtest.csv")));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PLAYTEST region=%s seconds=%.0f cells=%d travel_m=%.0f headings=%d blocked=%d jumps=%d sprints=%d dodges=%d edge_leaps=%d hangs=%d mantles=%d ladders=%d wall_runs=%d vaults=%d inside=%d stuck=%d void=%d falls=%d allowed=%d put_back=%d game_moves=%d probe=%d ms=%.0f"),
            *R->Region, R->Frame / Fps, R->Cells.Num(), R->Travel / 100.f, R->Headings, R->Blocked, R->Jumps, R->Sprints, R->Dodges, R->EdgeLeaps,
            Chuck ? Chuck->GetHangs() - R->Hangs0 : 0, Chuck ? Chuck->GetMantles() - R->Mantles0 : 0, Chuck ? Chuck->GetLadderMounts() - R->Ladders0 : 0,
            Chuck ? Chuck->GetWallRuns() + Chuck->GetWallSideRuns() - R->WallRuns0 : 0, Chuck ? Chuck->GetVaults() - R->Vaults0 : 0,
            Inside, Stuck, Void, Falls, Allowed, R->PutBack, R->GameMoves, (R->bProbeInside ? 1 : 0) + (R->bProbeClear ? 1 : 0), (FPlatformTime::Seconds() - R->Start) * 1000);
        FTimerHandle Quit;
        R->World->GetTimerManager().SetTimer(Quit, [](){ FPlatformMisc::RequestExit(false); }, 1.f, false);
    }

    void Step(TSharedRef<FRun> R, UWorld* World)
    {
        if (World != R->World || R->Frame >= Frames) return;
        AChuckCharacter* Chuck = R->Chuck.Get();
        if (!Chuck) { R->Frame = Frames; Finish(R); return; }
        UCharacterMovementComponent* Move = Chuck->GetCharacterMovement();
        const FVector At = Chuck->GetActorLocation();
        const FVector Prev = R->Trail.Num() ? R->Trail.Last() : At;
        // Cutscenes, holds and the game's own teleports (a respawn, a region's door) are not his; the windows restart.
        const bool bJump = FVector::Dist(At, Prev) > 500.f;
        const bool bOurs = !bJump && !Chuck->IsInFallCutscene() && !Chuck->IsSceneHeld() && !Chuck->HasDemoEnded() && !Chuck->IsAstral();
        if (bJump) ++R->GameMoves;
        R->Trail.Add(At); R->Counted.Add(bOurs);
        // The last frame always finishes (a fault found on it returns early below).
        if (++R->Frame >= Frames) { Finish(R); return; }
        if (bOurs) R->Travel += FVector::Dist(At, Prev);
        R->Cells.Add(FIntPoint(FMath::FloorToInt32(At.X / Cell), FMath::FloorToInt32(At.Y / Cell)));
        if (Move->IsMovingOnGround()) { R->LastGround = At; if (R->Frame % 30 == 0) R->Safe.Add(At); }

        // The game caught a fall (an Astral death): information, at the last place he stood.
        // A fault was just written: hold him there two frames so its screenshot shows it, then put him back.
        if (R->PutBackAt > 0) { if (R->Frame >= R->PutBackAt) { R->PutBackAt = 0; PutBack(*R, Chuck); } return; }
        if (Chuck->GetFallDeaths() > R->Falls0) { R->Falls0 = Chuck->GetFallDeaths(); Report(*R, TEXT("fall"), FString(), Chuck, R->LastGround); }

        // Inside only counts where his movement sweeps him. The hang, the climb onto a top, the vault, the swing and the
        // ladder place him along their paths and may pass through an edge on the way (the climb nudges him out at its
        // end); left inside after one of them, he is caught the moment he walks or falls again.
        const FString GaitNow = Chuck->GetGaitName();
        static const TSet<FString> Placed{TEXT("Hang"), TEXT("Climb"), TEXT("Vault"), TEXT("Swing"), TEXT("SwingLeap"), TEXT("Ladder"), TEXT("Held"), TEXT("Thrown"), TEXT("Astral")};
        if (Placed.Contains(GaitNow)) R->InsideRun = 0;
        else if (bOurs && Chuck->GetActorEnableCollision())
        {
            const UCapsuleComponent* Cap = Chuck->GetCapsuleComponent();
            const float Radius = Cap->GetScaledCapsuleRadius() - Shrink, Half = Cap->GetScaledCapsuleHalfHeight() - Shrink;
            const FString In = Embedded(World, Chuck, Cap->GetComponentLocation(), Radius, Half);
            R->InsideRun = In.IsEmpty() ? 0 : R->InsideRun + 1;
            if (R->InsideRun == InsideFrames) { Report(*R, TEXT("inside"), In, Chuck, At); R->PutBackAt = R->Frame + 2; return; }
        }
        if (bOurs && Move->IsFalling())
        {
            if (++R->FallingFor >= VoidFrames && !Floor(World, Chuck, At, 20000.f)) { Report(*R, TEXT("void"), FString(), Chuck, At); R->PutBackAt = R->Frame + 2; return; }
        }
        else R->FallingFor = 0;
        const int32 T = R->Trail.Num() - 1;
        if (T >= StuckFrames)
        {
            bool bAll = true; float Far = 0;
            for (int32 K = T - StuckFrames; K <= T && bAll; ++K) { bAll = R->Counted[K]; Far = FMath::Max(Far, static_cast<float>(FVector::Dist(R->Trail[K], At))); }
            if (bAll && Far < StuckRadius) { Report(*R, TEXT("stuck"), FString(), Chuck, At); R->PutBackAt = R->Frame + 2; return; }
        }

        // The robot's hands. Blocked (hardly moved since it chose this way): another way, and a jump half the time.
        if (R->Frame - R->HeadingSince > BlockedFrames && T >= BlockedFrames && R->Counted[T - BlockedFrames] && FVector::Dist(At, R->Trail[T - BlockedFrames]) < BlockedTravel)
        {
            ++R->Blocked; const FVector Was = R->Heading; ChooseHeading(*R, Chuck, &Was);
            if (R->Rng.FRand() < .5f) { Chuck->JumpPressed(); R->JumpRelease = R->Frame + R->Rng.RandRange(3, 15); ++R->Jumps; }
        }
        else if (R->Frame >= R->HeadingUntil) ChooseHeading(*R, Chuck, nullptr);
        Chuck->SetTestStickWorld(R->Heading);
        Chuck->AddMovementInput(R->Heading, 1.f, true);
        if (R->JumpRelease >= 0 && R->Frame >= R->JumpRelease) { Chuck->StopJumping(); R->JumpRelease = -1; }
        if (R->JumpRelease < 0)
        {
            // At an edge with ground out of reach below, a leap now and then.
            if (Move->IsMovingOnGround() && !Floor(World, Chuck, At + R->Heading * 90.f, 150.f) && R->Rng.FRand() < .04f) { Chuck->JumpPressed(); R->JumpRelease = R->Frame + 12; ++R->Jumps; ++R->EdgeLeaps; }
            else if (R->Rng.FRand() < 1.f / (Fps * 2.5f)) { Chuck->JumpPressed(); R->JumpRelease = R->Frame + R->Rng.RandRange(3, 15); ++R->Jumps; }
        }
        if (R->Rng.FRand() < 1.f / (Fps * 12.f)) { Chuck->TrySprint(); ++R->Sprints; }
        if (R->Rng.FRand() < 1.f / (Fps * 15.f)) { Chuck->DodgeToward(FVector2D(R->Rng.FRandRange(-1.f, 1.f), 1.f)); ++R->Dodges; }
    }

    // A solid cube in the sky: the inside test must find it, and nothing 4 m above it.
    void Probe(FRun& R, AChuckCharacter* Chuck)
    {
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Cube) return;
        const FVector At = R.Home + FVector(0, 0, 30000);
        AActor* Box = R.World->SpawnActor<AActor>();
        auto* M = NewObject<UStaticMeshComponent>(Box, TEXT("PlaytestProbe"));
        M->SetStaticMesh(Cube); M->SetMobility(EComponentMobility::Movable); M->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetRootComponent(M); M->RegisterComponent();
        M->SetWorldLocationAndRotation(At, FRotator::ZeroRotator); M->SetWorldScale3D(FVector(2));
        const UCapsuleComponent* Cap = Chuck->GetCapsuleComponent();
        const float Radius = Cap->GetScaledCapsuleRadius() - Shrink, Half = Cap->GetScaledCapsuleHalfHeight() - Shrink;
        R.bProbeInside = !Embedded(R.World, Chuck, At, Radius, Half).IsEmpty();
        R.bProbeClear = Embedded(R.World, Chuck, At + FVector(0, 0, 400), Radius, Half).IsEmpty();
        Box->Destroy();
    }

    void Begin(UWorld* World, const FString& Region)
    {
        TSharedRef<FRun> R = MakeShared<FRun>();
        R->World = World; R->Region = Region; R->Start = FPlatformTime::Seconds();
        int32 Seed = static_cast<int32>(GetTypeHash(Region));
        FParse::Value(FCommandLine::Get(), TEXT("ChuckPlaytestSeed="), Seed);
        R->Rng.Initialize(Seed);
        AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
        if (!Chuck) { UE_LOG(LogTemp, Error, TEXT("CHUCK_PLAYTEST region=%s no_chuck=1"), *Region); FPlatformMisc::RequestExit(false); return; }
        R->Chuck = Chuck;
        // -ChuckPlaytestAt=X,Y,Z: start where an earlier run found something, to see it again.
        FString AtText; TArray<FString> Parts;
        if (FParse::Value(FCommandLine::Get(), TEXT("ChuckPlaytestAt="), AtText, false) && AtText.ParseIntoArray(Parts, TEXT(",")) == 3)
            Chuck->ResetAtLocation(FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])));
        R->Home = R->LastGround = Chuck->GetActorLocation();
        Probe(*R, Chuck);
        // The robot's stick only: no real mouse or pad can steer the run.
        if (APlayerController* PC = Cast<APlayerController>(Chuck->GetController())) { Chuck->DisableInput(PC); PC->SetViewTarget(Chuck); }
        Chuck->SetLookLocked(true);
        R->Falls0 = Chuck->GetFallDeaths(); R->Respawns0 = Chuck->GetRespawns(); R->Hangs0 = Chuck->GetHangs(); R->Mantles0 = Chuck->GetMantles();
        R->Ladders0 = Chuck->GetLadderMounts(); R->WallRuns0 = Chuck->GetWallRuns() + Chuck->GetWallSideRuns(); R->Vaults0 = Chuck->GetVaults();
        ChooseHeading(*R, Chuck, nullptr);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PLAYTEST_BEGIN region=%s seed=%d home=%s probe_inside=%d probe_clear=%d"), *Region, Seed, *R->Home.ToString(), R->bProbeInside ? 1 : 0, R->bProbeClear ? 1 : 0);
        FApp::SetUseFixedTimeStep(true);
        FApp::SetFixedDeltaTime(1.0 / Fps);
        R->Hook = FWorldDelegates::OnWorldPostActorTick.AddLambda([R](UWorld* W, ELevelTick, float) { Step(R, W); });
    }

    void WaitThenRun(UWorld* World, FString Region, double Since)
    {
        if ((Region.StartsWith(TEXT("Chult")) && !IsChultGroundingDone()) || World->GetTimeSeconds() - Since < 6.0)
        {
            FTimerHandle Again;
            World->GetTimerManager().SetTimer(Again, [World, Region, Since]() { WaitThenRun(World, Region, Since); }, 1.f, false);
            return;
        }
        Begin(World, Region);
    }
}

// Called by the game mode with the checkpoint already started (or Waterdeep).
void StartPlaytest(UWorld* World, const FString& Region)
{
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PLAYTEST_START region=%s"), *Region);
    WaitThenRun(World, Region, World->GetTimeSeconds());
}
