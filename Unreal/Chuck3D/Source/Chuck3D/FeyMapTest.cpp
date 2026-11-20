// -ChuckChultTest -ChuckFeyMapTest (Claude, 2026-11-19; FeyMap.h, docs/regions/FEYWILD.md stage 3): the real character
// walked along every route of the rough Feywild map, from the river's shore through all ten thresholds to the maze's heart,
// steering along the paths. It measures frame time and memory in each district against Chult's landing, how long each
// district takes to load, how many stand at once, and whether he is ever stuck or late at a threshold; then it starts at
// each district's checkpoint and takes a still there. -ChuckFeyMapTest=short walks only the shore to the orchard.
#include "FeyMap.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/App.h"
#include "RHI.h"

using namespace FeyMap;

namespace
{
    constexpr float Stand = 34.65f;
    struct FStat { double Ms = 0, Gpu = 0, Worst = 0; int32 Frames = 0; float MemMb = 0; };
    struct FRun
    {
        FTimerHandle Timer;
        TArray<FVector> Points; TArray<int32> PointLeg; TArray<FString> LegNames;
        int32 Next = 0, Stuck = 0, Void = 0, Phase = 0, Check = 0, CheckFails = 0;
        double Last = 0, PhaseStart = 0, LastGain = 0, Start = 0;
        float Best = 1e9f, Walked = 0; FVector Prev = FVector::ZeroVector;
        bool bJumped = false, bShort = false, bPlaced = false, bFinished = false; uint64 Frame = 0;
        FStat Chult, Fey[Districts + 1], Cross;
        float ChultMem = 0, BeforeMem = 0;
    };
    float MemMb() { return FPlatformMemory::GetStats().UsedPhysical / 1048576.f; }

    void AddLeg(FRun& R, const FString& Name, const TArray<FVector>& P)
    {
        const int32 Leg = R.LegNames.Add(Name);
        for (const FVector& Q : P) { if (R.Points.Num() && FVector::Dist2D(R.Points.Last(), Q) < 60.f) continue; R.Points.Add(Q); R.PointLeg.Add(Leg); }
    }
    void Way(FRun& R, int32 Th, bool bForward)
    {
        TArray<FVector> P;
        for (int32 I = 0; I <= 16; ++I) { const float T = bForward ? I / 16.f : 1.f - I / 16.f; P.Add(ThresholdAtT(Th, FMath::Lerp(-.1f, 1.1f, T))); }
        AddLeg(R, FString::Printf(TEXT("threshold%d%s"), Th, bForward ? TEXT("") : TEXT("_back")), P);
    }
    void In(FRun& R, int32 D, int32 Ta, int32 Tb) { AddLeg(R, FString::Printf(TEXT("%s_%d_%d"), District(D).Name, Ta, Tb), Walk(D, Ta, Tb)); }
}

void ReviewFeyMap(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    auto R = MakeShared<FRun>();
    FString Mode; FParse::Value(FCommandLine::Get(), TEXT("ChuckFeyMapTest="), Mode);
    R->bShort = Mode == TEXT("short");
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    if (PC) { PC->ConsoleCommand(TEXT("t.MaxFPS 60")); Chuck->DisableInput(PC); }

    // The route: the shore, the slice's path west, then every threshold, ending in the maze's heart.
    {
        TArray<FVector> Shore; const FVector S = Fey::ShoreSpot(); Shore.Add(S);
        for (float X = 4200.f; X >= -2200.f; X -= 700.f) Shore.Add(FVector(X, Fey::PathY(X), Fey::Ground(X, Fey::PathY(X))));
        AddLeg(*R, TEXT("slice"), Shore);
        In(*R, Riverbank, -3, 0);
    }
    Way(*R, 0, true); In(*R, Orchard, 0, R->bShort ? -1 : 1);
    if (!R->bShort)
    {
        Way(*R, 1, true); In(*R, Crossroads, 1, 2); Way(*R, 2, false); In(*R, Orchard, 2, 1); Way(*R, 1, true);
        In(*R, Crossroads, 1, 3); Way(*R, 3, true); In(*R, LilyMere, 3, 4); Way(*R, 4, false);
        In(*R, Crossroads, 4, 5); Way(*R, 5, true); In(*R, Deepwood, 5, -2); In(*R, Deepwood, -2, 6); Way(*R, 6, false);
        In(*R, Crossroads, 6, 8); Way(*R, 8, true); In(*R, Highmeadow, 8, 7); Way(*R, 7, false);
        In(*R, Deepwood, 7, -1); In(*R, Deepwood, -1, 7); Way(*R, 7, true);
        In(*R, Highmeadow, 7, -2); In(*R, Highmeadow, -2, 9); Way(*R, 9, true); In(*R, Maze, 9, -1);
    }
    float Length = 0; for (int32 I = 1; I < R->Points.Num(); ++I) Length += FVector::Dist2D(R->Points[I], R->Points[I - 1]);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_ROUTE points=%d legs=%d length_m=%.0f short=%d"), R->Points.Num(), R->LegNames.Num(), Length / 100.f, R->bShort);

    R->Start = R->Last = FPlatformTime::Seconds();
    Owner->GetWorldTimerManager().SetTimer(R->Timer, [Owner, Chuck, R, PC, Failures]()
    {
        UWorld* W = Owner->GetWorld();
        const double Now = FPlatformTime::Seconds(); R->Last = Now;
        // Once a frame (the timer can run more than once in one): the frame's real time and the GPU's.
        if (R->bFinished || GFrameCounter == R->Frame) return; R->Frame = GFrameCounter;
        const double FrameMs = FApp::GetDeltaTime() * 1000., GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
        const double Since = Now - R->PhaseStart;
        auto Done = [&](int32 Fails)
        {
            R->bFinished = true;   // (the timer runs on, doing nothing: it is not cleared from inside itself)
            FTimerHandle Quit; Owner->GetWorldTimerManager().SetTimer(Quit, [Fails]() { FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0); }, 2.f, false);
        };
        // ---- 0: Chult's landing, for the baseline
        if (R->Phase == 0)
        {
            if (R->PhaseStart == 0) { R->PhaseStart = Now; return; }
            if (Since > 2.f) { R->Chult.Ms += FrameMs; R->Chult.Gpu += GpuMs; R->Chult.Worst = FMath::Max(R->Chult.Worst, FrameMs); ++R->Chult.Frames; }
            if (Since < 8.f) return;
            R->ChultMem = MemMb();
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_BASELINE chult_ms=%.2f gpu_ms=%.2f worst_ms=%.1f mem_mb=%.0f"), R->Chult.Ms / FMath::Max(1, R->Chult.Frames), R->Chult.Gpu / FMath::Max(1, R->Chult.Frames), R->Chult.Worst, R->ChultMem);
            R->BeforeMem = R->ChultMem;
            AFeywildSetting::Enter(W);
            AFeyMap* Map = AFeyMap::Ensure(W); Map->LoadNow(Riverbank);
            Chuck->ResetAtLocation(FeywildArrival()); Chuck->SetActorRotation(FRotator(0, FeywildArrivalYaw(), 0)); Chuck->Recenter();
            if (PC) PC->ConsoleCommand(R->bShort ? TEXT("slomo 2") : TEXT("slomo 2.5"));
            R->Phase = 1; R->PhaseStart = Now; R->LastGain = W->GetTimeSeconds(); R->Prev = Chuck->GetActorLocation();
            return;
        }
        AFeyMap* Map = AFeyMap::Get(W);
        // ---- 1: the walk
        if (R->Phase == 1)
        {
            if (Since < 2.f) return;
            const FVector P = Chuck->GetActorLocation(), L = FeyLocal(P);
            R->Walked += FVector::Dist2D(P, R->Prev); R->Prev = P;
            int32 D = DistrictAt(L.X, L.Y); float T; const bool bWay = D < 0 && ThresholdAt(L, T) >= 0;
            FStat& S = bWay ? R->Cross : R->Fey[D < 0 ? Districts : D];
            S.Ms += FrameMs; S.Gpu += GpuMs; S.Worst = FMath::Max(S.Worst, FrameMs); ++S.Frames;
            if (D >= 0 && Map && Map->IsReady(D)) S.MemMb = FMath::Max(S.MemMb, MemMb());
            if (R->Next >= R->Points.Num())
            {
                Chuck->SetRunHeld(false);
                if (PC) PC->ConsoleCommand(TEXT("slomo 1"));
                const double Secs = Now - R->Start;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_WALKED legs=%d walked_m=%.0f seconds=%.0f stuck=%d void=%d late=%d max_loaded=%d worst_ready_s=%.2f missing=%d"),
                    R->LegNames.Num(), R->Walked / 100.f, Secs, R->Stuck, R->Void, Map ? Map->GetLateCount() : -1, Map ? Map->GetMaxLoaded() : -1, Map ? Map->GetWorstReadySeconds() : -1.f, Map ? Map->GetMissing() : -1);
                R->Phase = 2; R->PhaseStart = Now; return;
            }
            FVector Target = R->Points[R->Next]; Target.Z = L.Z;
            const float Dist = FVector::Dist2D(L, Target);
            if (Dist < (bWay ? 130.f : 210.f)) { ++R->Next; R->Best = 1e9f; R->LastGain = W->GetTimeSeconds(); R->bJumped = false; return; }
            if (Dist < R->Best - 30.f) { R->Best = Dist; R->LastGain = W->GetTimeSeconds(); }
            const float Stall = W->GetTimeSeconds() - R->LastGain;
            if (Stall > 2.5f && !R->bJumped) { Chuck->Jump(); R->bJumped = true; }
            if (Stall > 6.f)
            {
                ++R->Stuck;
                UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_MAP_STUCK leg=%s at=%s toward=%s"), *R->LegNames[R->PointLeg[R->Next]], *L.ToCompactString(), *R->Points[R->Next].ToCompactString());
                Chuck->ResetAtLocation(FeyWorld(R->Points[R->Next] + FVector(0, 0, Stand + 8.f)));
                ++R->Next; R->Best = 1e9f; R->LastGain = W->GetTimeSeconds(); R->bJumped = false; return;
            }
            float Gz; if (GroundAt(L.X, L.Y, Gz) && L.Z < Gz - 250.f)
            {
                ++R->Void; UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_MAP_VOID at=%s ground=%.0f"), *L.ToCompactString(), Gz);
                Chuck->ResetAtLocation(FeyWorld(R->Points[R->Next] + FVector(0, 0, Stand + 8.f))); return;
            }
            const FVector Dir = (FeyWorld(Target) - P).GetSafeNormal2D();
            Chuck->SetRunHeld(true); Chuck->SetTestStickWorld(Dir); Chuck->AddMovementInput(Dir, 1.f, true);
            return;
        }
        // ---- 2: each district's checkpoint, alone, with a still
        if (R->Phase == 2)
        {
            if (R->bShort && R->Check == 0) R->Check = Districts;   // (the short walk skips them)
            if (R->Check >= Districts)
            {
                int32 Fails = R->Stuck + R->Void + R->CheckFails + (Map ? (Map->GetMaxLoaded() > 2) + (Map->GetMissing() > 0) + Map->GetLateCount() : 1);
                FString Line;
                for (int32 D = 0; D < Districts; ++D)
                {
                    const FStat& S = R->Fey[D]; if (!S.Frames) continue;
                    Line += FString::Printf(TEXT(" %s=%.2fms/gpu%.2fms/%.0fmb"), District(D).Name, S.Ms / S.Frames, S.Gpu / S.Frames, S.MemMb);
                }
                const float ChultMs = R->Chult.Ms / FMath::Max(1, R->Chult.Frames), ChultGpu = R->Chult.Gpu / FMath::Max(1, R->Chult.Frames);
                int32 Slower = 0; for (int32 D = 0; D < Districts; ++D) if (R->Fey[D].Frames && (R->Fey[D].Ms / R->Fey[D].Frames > ChultMs + .5 || R->Fey[D].Gpu / R->Fey[D].Frames > ChultGpu + .5)) ++Slower;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_DISTRICTS chult=%.2fms/gpu%.2fms/%.0fmb%s thresholds=%.2fms worst_threshold_ms=%.1f slower_than_chult=%d"),
                    ChultMs, ChultGpu, R->ChultMem, *Line, R->Cross.Ms / FMath::Max(1, R->Cross.Frames), R->Cross.Worst, Slower);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_TEST failures=%d stuck=%d void=%d late=%d max_loaded=%d checkpoints_failed=%d missing=%d short=%d"),
                    Failures + Fails, R->Stuck, R->Void, Map ? Map->GetLateCount() : -1, Map ? Map->GetMaxLoaded() : -1, R->CheckFails, Map ? Map->GetMissing() : -1, R->bShort);
                Done(Failures + Fails); return;
            }
            const int32 D = R->Check;
            if (!R->bPlaced)
            {
                Map->LoadNow(D);
                Chuck->ResetAtLocation(FeyCheckpointWorld(D)); Chuck->SetActorRotation(FRotator(0, CheckpointYaw(D), 0)); Chuck->Recenter();
                R->bPlaced = true; R->PhaseStart = FPlatformTime::Seconds();
                return;
            }
            if (Since < 4.f) return;
            R->bPlaced = false;
            const bool bOk = Chuck->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist(Chuck->GetActorLocation(), FeyCheckpointWorld(D)) < 80.f && Map->IsReady(D) && Map->GetLoadedCount() == 1;
            R->CheckFails += !bOk;
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyMap/%d_%s.png"), D, District(D).Name), false, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_CHECKPOINT name=%s ok=%d loaded=%d mem_mb=%.0f"), District(D).Name, bOk, Map->GetLoadedCount(), MemMb());
            ++R->Check; R->PhaseStart = Now;
        }
    }, .02f, true);
}
