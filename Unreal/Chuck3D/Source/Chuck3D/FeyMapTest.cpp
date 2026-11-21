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
        bool bJumped = false, bShort = false, bPlaced = false, bFinished = false, bStillsOnly = false, bCracks = false; uint64 Frame = 0;
        int32 CrackStep = 0, CrackLeaks = 0, CrackShots = 0; TArray<FLeak> Shown;
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
    struct FShot { const TCHAR* Name; int32 D; FVector2D At, Look; };
    const FShot Shots[] = {
        {TEXT("CanyonOrchardEnd"), Orchard, {-30200.f, 900.f}, {-28000.f, 0.f}},           // the first threshold's end, from the orchard
        {TEXT("CanyonInside"), Riverbank, {-23300.f, 100.f}, {-25500.f, 0.f}},            // looking into it from the riverbank
        {TEXT("RootTunnelMouth"), Crossroads, {-62000.f, -8600.f}, {-62500.f, -11000.f}}, // a root tunnel's mouth in its bank
        {TEXT("CrossroadsArch"), Crossroads, {-62200.f, -3400.f}, {-67750.f, 0.f}},
        {TEXT("Pads"), LilyMere, {-73480.f, 16300.f}, {-72600.f, 19500.f}},
        {TEXT("PadsAcross"), LilyMere, {-72300.f, 19600.f}, {-73500.f, 15500.f}},
        {TEXT("DeepwoodPath"), Deepwood, {-68500.f, -25000.f}, {-75000.f, -30000.f}},
        {TEXT("DeepwoodHollow"), Deepwood, {-80300.f, -37300.f}, {-82000.f, -40000.f}},
        {TEXT("Overlook"), Highmeadow, {-111650.f, 12600.f}, {-134000.f, 5000.f}},
        {TEXT("MazeGate"), Maze, {-119700.f, 4300.f}, {-134000.f, 5000.f}},
        {TEXT("MazeCorridor"), Maze, {-134000.f + 11700.f, 5000.f + 2100.f}, {-134000.f, 5000.f}},
        // (2026-11-21) The hedges' leaves along the outer ring, and the Cloud Staircase's foot from the heart's plaza.
        {TEXT("MazeHedge"), Maze, {-121421.f, 6512.f}, {-121690.f, 7788.f}},
        {TEXT("StairFoot"), Maze, {-133389.f, 5222.f}, {-134077.f, 3903.f}},
        {TEXT("StairAlong"), Maze, {-135500.f, 5600.f}, {-134900.f, 3700.f}},
    };
    void In(FRun& R, int32 D, int32 Ta, int32 Tb) { AddLeg(R, FString::Printf(TEXT("%s_%d_%d"), District(D).Name, Ta, Tb), Walk(D, Ta, Tb)); }
}

void ReviewFeyMap(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    auto R = MakeShared<FRun>();
    FString Mode; FParse::Value(FCommandLine::Get(), TEXT("ChuckFeyMapTest="), Mode);
    R->bShort = Mode == TEXT("short"); R->bStillsOnly = Mode == TEXT("stills") || Mode == TEXT("cracks"); R->bCracks = Mode == TEXT("cracks");
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
    if (R->bStillsOnly) { R->Points.Reset(); R->PointLeg.Reset(); }
    if (!R->bShort && !R->bStillsOnly)
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
            // The lily pads over the deep water: arrive near each one's middle, and jump for the next from a metre out.
            const bool bPad = IsPad(Target.X, Target.Y);
            if (bPad && Dist < 32.f) { ++R->Next; R->Best = 1e9f; R->LastGain = W->GetTimeSeconds(); R->bJumped = false; return; }
            if (bPad && Dist > 45.f && Dist < 90.f && Chuck->GetCharacterMovement()->IsMovingOnGround() && (IsPad(L.X, L.Y) || !InDeepWater(L.X, L.Y))) Chuck->Jump();
            if (!bPad && Dist < (bWay ? 130.f : 210.f)) { ++R->Next; R->Best = 1e9f; R->LastGain = W->GetTimeSeconds(); R->bJumped = false; return; }
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
        // ---- 2 (=cracks, 2026-11-21): each threshold looked through, with both its districts standing and with only the one
        // on either side (as when the one behind has been dropped past the bend); stills of the first canyon's leaks.
        if (R->Phase == 2 && R->bCracks)
        {
            constexpr int32 States = 3;
            if (R->CrackStep < Thresholds * States)
            {
                const int32 Th = R->CrackStep / States, St = R->CrackStep % States; const FThreshold& Wy = Threshold(Th);
                if (!R->bPlaced)
                {
                    if (St == 0) Map->LoadPair(Wy.A, Wy.B); else Map->LoadNow(St == 1 ? Wy.A : Wy.B);
                    Chuck->ResetAtLocation(FeyWorld(ThresholdAtT(Th, St == 0 ? .5f : St == 2 ? .9f : .1f) + FVector(0, 0, Stand + 4.f)));
                    R->bPlaced = true; R->PhaseStart = Now; return;
                }
                if (Since < 1.5f) return;
                TArray<FLeak> Leaks;
                const int32 N = SeeThrough(W, Chuck, Th, St == 2 ? .6f : -.2f, St == 1 ? .4f : 1.2f, Leaks);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_CRACKS threshold=%d state=%s leaks=%d"), Th, St == 0 ? TEXT("both") : St == 1 ? TEXT("a_only") : TEXT("b_only"), N);
                for (int32 I = 0; I < FMath::Min(6, Leaks.Num()); ++I)
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_CRACK at=%s dir=%s dist=%.0f"), *Leaks[I * Leaks.Num() / FMath::Min(6, Leaks.Num())].At.ToCompactString(), *Leaks[I * Leaks.Num() / FMath::Min(6, Leaks.Num())].Dir.ToCompactString(), Leaks[I * Leaks.Num() / FMath::Min(6, Leaks.Num())].Dist);
                R->CrackLeaks += N;
                if (Th == 0 && R->Shown.Num() < 4) for (int32 I = 0; I < Leaks.Num() && R->Shown.Num() < 4; I += FMath::Max(1, Leaks.Num() / 2)) R->Shown.Add(Leaks[I]);
                R->bPlaced = false; ++R->CrackStep;
                if (R->CrackStep == States) R->CrackStep = Thresholds * States + 100;   // (after the first canyon: its stills, then on)
                return;
            }
            if (R->CrackStep >= Thresholds * States + 100)
            {
                // Stills along the leaks found in the first canyon (him at the ray's start, turned along it), then the rest.
                if (R->CrackShots < R->Shown.Num())
                {
                    const FLeak& Lk = R->Shown[R->CrackShots];
                    if (!R->bPlaced)
                    {
                        Map->LoadNow(Threshold(0).B);
                        Chuck->ResetAtLocation(FeyWorld(FVector(Lk.At.X, Lk.At.Y, Lk.At.Z - (Lk.At.Z > 100.f ? 120.f : 0.f) + Stand - 36.f)));
                        Chuck->SetActorRotation(FRotator(0, Lk.Dir.Rotation().Yaw, 0)); Chuck->Recenter();
                        R->bPlaced = true; R->PhaseStart = Now; return;
                    }
                    if (Since < 2.5f) return;
                    FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyMap/Crack_%d.png"), R->CrackShots), false, false);
                    R->bPlaced = false; ++R->CrackShots; R->PhaseStart = Now; return;
                }
                if (Since < 1.f) return;
                R->CrackStep = States;   // the other thresholds
                R->CrackShots = 1 << 20; return;
            }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_CRACKS_DONE leaks=%d"), R->CrackLeaks);
            Done(R->CrackLeaks > 0 ? 1 : 0); return;
        }
        // ---- 2: each district's checkpoint, alone, with a still
        if (R->Phase == 2)
        {
            if (R->bShort && R->Check == 0) R->Check = SpotCount() + UE_ARRAY_COUNT(Shots);   // (the short walk skips them)
            if (R->Check >= SpotCount() && R->Check < SpotCount() + int32(UE_ARRAY_COUNT(Shots)))
            {
                // (2026-11-20) Stills of the art pass's places: him standing there, looking toward something.
                const FShot& Sh = Shots[R->Check - SpotCount()];
                if (!R->bPlaced)
                {
                    if (!Map->IsReady(Sh.D) || Map->GetLoadedCount() != 1) Map->LoadNow(Sh.D);
                    float Z = 0; GroundAt(Sh.At.X, Sh.At.Y, Z); if (IsPad(Sh.At.X, Sh.At.Y)) Z = 2.f;
                    Chuck->ResetAtLocation(FeyWorld(FVector(Sh.At.X, Sh.At.Y, Z + Stand + 4.f)));
                    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Sh.Look.Y - Sh.At.Y, Sh.Look.X - Sh.At.X));
                    Chuck->SetActorRotation(FRotator(0, Yaw, 0)); Chuck->Recenter();
                    R->bPlaced = true; R->PhaseStart = FPlatformTime::Seconds(); return;
                }
                if (Since < 3.5f) return;
                R->bPlaced = false;
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyMap/Art_%s.png"), Sh.Name), false, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_SHOT name=%s"), Sh.Name);
                ++R->Check; R->PhaseStart = Now; return;
            }
            if (R->bStillsOnly && R->Check < SpotCount()) { R->Check = SpotCount(); return; }
            if (R->Check >= SpotCount())
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
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_TEST failures=%d stuck=%d void=%d late=%d max_loaded=%d checkpoints_failed=%d missing=%d short=%d drowned=%d maze_way_m=%.0f"),
                    Failures + Fails, R->Stuck, R->Void, Map ? Map->GetLateCount() : -1, Map ? Map->GetMaxLoaded() : -1, R->CheckFails, Map ? Map->GetMissing() : -1, R->bShort, Map ? Map->GetDrowned() : -1, MazeWay() / 100.f);
                Done(Failures + Fails); return;
            }
            const int32 Sp = R->Check, D = Spot(Sp).D;   // (2026-11-21: every spot, the districts' own first)
            if (!R->bPlaced)
            {
                Map->LoadNow(D);
                Chuck->ResetAtLocation(FeySpotWorld(Sp)); Chuck->SetActorRotation(FRotator(0, SpotYaw(Sp), 0)); Chuck->Recenter();
                R->bPlaced = true; R->PhaseStart = FPlatformTime::Seconds();
                return;
            }
            if (Since < 4.f) return;
            R->bPlaced = false;
            const bool bOk = Chuck->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist(Chuck->GetActorLocation(), FeySpotWorld(Sp)) < 80.f && Map->IsReady(D) && Map->GetLoadedCount() == 1;
            R->CheckFails += !bOk;
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyMap/%d_%s.png"), Sp, Spot(Sp).Point), false, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAP_CHECKPOINT name=%s ok=%d loaded=%d mem_mb=%.0f"), Spot(Sp).Point, bOk, Map->GetLoadedCount(), MemMb());
            ++R->Check; R->PhaseStart = Now;
        }
    }, .02f, true);
}
