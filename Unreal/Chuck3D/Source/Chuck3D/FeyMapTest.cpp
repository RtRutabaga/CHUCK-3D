// -ChuckChultTest -ChuckFeyMapTest (Claude, 2026-11-19; FeyMap.h, docs/regions/FEYWILD.md stage 3): the real character
// walked along every route of the rough Feywild map, from the river's shore through all ten thresholds to the maze's heart,
// steering along the paths. It measures frame time and memory in each district against Chult's landing, how long each
// district takes to load, how many stand at once, and whether he is ever stuck or late at a threshold; then it starts at
// each district's checkpoint and takes a still there. -ChuckFeyMapTest=short walks only the shore to the orchard.
#include "FeyMap.h"
#include "FeyLife.h"
#include "FeyDelights.h"
#include "FeyCreatures.h"
#include "FeyHare.h"
#include "FeyJelly.h"
#include "ChultFoliageFade.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "TempleSkeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
        bool bJumped = false, bShort = false, bPlaced = false, bFinished = false, bStillsOnly = false, bCracks = false, bLife = false; uint64 Frame = 0;
        FVector PadAt = FVector::ZeroVector;   // (2026-11-24, the koi's pad)
        int32 LifeFails = 0, Cigs0 = 0, Hits0 = 0, Hits1 = 0, OrchidSub = 0, Aimed0 = 0; float MinDist = 1e9f; float SpeedOut = 0, SpeedIn = 0; int32 SpeedOutN = 0, SpeedInN = 0;
        int32 CrackStep = 0, CrackLeaks = 0, CrackShots = 0; TArray<FLeak> Shown;
        float PkS0 = 0, PkRate = -1; double PkT0 = -1, PkLastJump = 0; int32 PkGrip = -1;   // (2026-11-23) the parkour checks
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
        // (2026-11-21, the user's stills of slits in the first canyon's walls) Along it both ways, T from its riverbank mouth;
        // At.X < -1.9e5 marks one of these: At.Y is T, Look.X the direction (1 on towards the orchard, -1 back).
        {TEXT("Canyon_05w"), Riverbank, {-2e5f, .05f}, {1.f, 0.f}}, {TEXT("Canyon_18w"), Riverbank, {-2e5f, .18f}, {1.f, 0.f}}, {TEXT("Canyon_32w"), Riverbank, {-2e5f, .32f}, {1.f, 0.f}},
        {TEXT("Canyon_50w"), Riverbank, {-2e5f, .5f}, {1.f, 0.f}}, {TEXT("Canyon_68w"), Orchard, {-2e5f, .68f}, {1.f, 0.f}}, {TEXT("Canyon_85w"), Orchard, {-2e5f, .85f}, {1.f, 0.f}},
        {TEXT("Canyon_95e"), Orchard, {-2e5f, .95f}, {-1.f, 0.f}}, {TEXT("Canyon_80e"), Orchard, {-2e5f, .8f}, {-1.f, 0.f}}, {TEXT("Canyon_62e"), Orchard, {-2e5f, .62f}, {-1.f, 0.f}},
        {TEXT("Canyon_45e"), Riverbank, {-2e5f, .45f}, {-1.f, 0.f}}, {TEXT("Canyon_28e"), Riverbank, {-2e5f, .28f}, {-1.f, 0.f}}, {TEXT("Canyon_12e"), Riverbank, {-2e5f, .12f}, {-1.f, 0.f}},
        // (2026-11-21, stage 5) The first flower and its vines, a pollen bed, the orchids over the road, the moths.
        {TEXT("LifeFlower"), Riverbank, {-13650.f, -2350.f}, {-14300.f, -3300.f}}, {TEXT("LifePollen"), Orchard, {-30600.f, 400.f}, {-33500.f, 1300.f}},
        {TEXT("LifeOrchids"), Orchard, {-48700.f, -4300.f}, {-48259.f, -5603.f}}, {TEXT("LifeMoths"), LilyMere, {-67300.f, 30200.f}, {-66779.f, 31555.f}},
        {TEXT("TunnelMouthB"), Crossroads, {-2e5f, -2.f}, {5.f, 0.f}}, {TEXT("TunnelMouthA"), Deepwood, {-2e5f, -2.f}, {5.f, 1.f}},
    };
    void In(FRun& R, int32 D, int32 Ta, int32 Tb) { AddLeg(R, FString::Printf(TEXT("%s_%d_%d"), District(D).Name, Ta, Tb), Walk(D, Ta, Tb)); }
}

void ReviewFeyMap(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    auto R = MakeShared<FRun>();
    FString Mode; FParse::Value(FCommandLine::Get(), TEXT("ChuckFeyMapTest="), Mode);
    R->bShort = Mode == TEXT("short"); R->bStillsOnly = Mode == TEXT("stills") || Mode == TEXT("cracks") || Mode == TEXT("life"); R->bCracks = Mode == TEXT("cracks"); R->bLife = Mode == TEXT("life");
    AFeyLife::bCalm = !R->bLife;   // (the walker crosses orchids and moths: they do not hurt him then)
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
            // (2026-11-23) By a root gap on the maze's way, each point is met closely, so he lines up with the hole and crawls
            // straight through (counted a metre and more short, he cut across at it and stuck on the hedge beside it).
            bool bByGap = false; for (const FMazeGap& G : MazeGaps()) bByGap |= FVector2D::Distance(FVector2D(Target), G.C) < 400.f;
            if (!bPad && Dist < (bByGap ? 35.f : bWay ? 130.f : 210.f)) { ++R->Next; R->Best = 1e9f; R->LastGain = W->GetTimeSeconds(); R->bJumped = false; return; }
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
        // ---- 2 (=life, 2026-11-21): stage 5's first mechanics and hazards, each tried.
        if (R->Phase == 2 && R->bLife)
        {
            auto LifeOf = [&](int32 D) -> AFeyLife* { for (const TWeakObjectPtr<AFeyLife>& P : AFeyLife::All()) if (P.IsValid() && P->GetDistrict() == D) return P.Get(); return nullptr; };
            auto DelightsOf = [&](int32 D) -> AFeyDelights* { for (const TWeakObjectPtr<AFeyDelights>& P : AFeyDelights::All()) if (P.IsValid() && P->GetDistrict() == D) return P.Get(); return nullptr; };
            auto Put = [&](const FVector& L, const FVector& Face) { float Z = L.Z; GroundAt(L.X, L.Y, Z); Chuck->ResetAtLocation(FeyWorld(FVector(L.X, L.Y, Z + Stand + 4.f))); Chuck->SetActorRotation(FRotator(0, Face.Rotation().Yaw, 0)); Chuck->Recenter(); Chuck->SetSanity(AChuckCharacter::MaxSanity); };
            auto Drive = [&](const FVector& Dir) { const FVector Dw = Dir.GetSafeNormal2D(); Chuck->SetRunHeld(true); Chuck->SetTestStickWorld(Dw); Chuck->AddMovementInput(Dw, 1.f, true); };
            auto Check = [&](bool bOk, const TCHAR* What) { R->LifeFails += !bOk; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_CHECK %s ok=%d"), What, bOk); };
            const int32 Step = R->CrackStep;
            if (Step == 0)
            {
                // (2026-11-23, a developer's shortcut, not used by Verify-Package) CHUCK_FEY_LIFE_FROM=50 starts at that step.
                const int32 From = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("CHUCK_FEY_LIFE_FROM")));
                if (From > 0) { R->CrackStep = From; R->PhaseStart = Now; return; }
                Map->LoadNow(Riverbank); R->CrackStep = 1; R->PhaseStart = Now; return;
            }
            AFeyLife* Life = nullptr;
            if (Step < 10) Life = LifeOf(Riverbank); else if (Step < 20) Life = LifeOf(Orchard); else if (Step < 30) Life = LifeOf(LilyMere); else if (Step < 40) Life = LifeOf(Deepwood); else if (Step < 50) Life = LifeOf(Highmeadow); else Life = LifeOf(Maze);
            if (!Life && Step % 10 != 0 && Step < 60) { if (Since > 5.f) { Check(false, TEXT("life_built")); R->CrackStep = (Step / 10 + 1) * 10; } return; }
            auto Shot = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyMap/Life_%s.png"), Name), false, false); };
            // (part two) A free camera on a creature for its still (local eye and aim); Uncam gives the view back to him.
            static TWeakObjectPtr<ACameraActor> GCam;
            auto Cam = [&](const FVector& Eye, const FVector& Aim)
            {
                if (!GCam.IsValid()) GCam = Chuck->GetWorld()->SpawnActor<ACameraActor>();
                if (!GCam.IsValid()) return;
                GCam->GetCameraComponent()->SetFieldOfView(55.f);
                GCam->SetActorLocationAndRotation(FeyWorld(Eye), (Aim - Eye).Rotation());
                if (APlayerController* PC = Chuck->GetWorld()->GetFirstPlayerController()) if (PC->GetViewTarget() != GCam.Get()) PC->SetViewTarget(GCam.Get());
            };
            auto Uncam = [&]() { if (APlayerController* PC = Chuck->GetWorld()->GetFirstPlayerController()) PC->SetViewTarget(Chuck); };
            auto FaceAt = [&](const FVector& L) { const FVector D = (L - FeyLocal(Chuck->GetActorLocation())).GetSafeNormal2D(); if (!D.IsNearlyZero()) { Chuck->SetActorRotation(FRotator(0, D.Rotation().Yaw, 0)); Chuck->Recenter(); } };
            switch (Step)
            {
            case 1:   // the vines shut the nook; the flower opens them
            {
                const FFeyVines& V = Life->GetVines()[0]; const FFeyFlower& F = Life->GetFlowers()[0];
                if (!R->bPlaced) { Put(V.C + FVector(0, 160.f, 0), FVector(0, -1, 0)); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 1.5f) { Drive(FVector(0, -1, 0)); return; }
                Check(FeyLocal(Chuck->GetActorLocation()).Y > V.C.Y + 20.f, TEXT("vines_block"));
                Put(F.Pos + (V.C - F.Pos).GetSafeNormal2D() * 45.f, F.Pos - (F.Pos + (V.C - F.Pos).GetSafeNormal2D() * 45.f));
                FeyScratch(Chuck, Chuck->GetActorLocation(), (FeyWorld(F.Pos) - Chuck->GetActorLocation()).GetSafeNormal2D(), 120.f);
                R->bPlaced = false; R->CrackStep = 2; R->PhaseStart = Now; return;
            }
            case 2:
            {
                if (Since < 4.f) return;
                const FFeyVines& V = Life->GetVines()[0];
                Check(Life->GetFlowers()[0].Open > .95f && V.Up < .05f, TEXT("flower_opens_vines"));
                R->Cigs0 = Chuck->GetCigarettes();
                Put(V.C + FVector(0, 160.f, 0), FVector(0, -1, 0)); R->CrackStep = 3; R->PhaseStart = Now; return;
            }
            case 3:
            {
                const FVector Nook = Life->GetVines()[0].C - FVector(0, 440.f, 0);
                if (Since < 4.f) { Drive(Nook - FeyLocal(Chuck->GetActorLocation())); return; }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_NOOK at=%s nook=%s"), *FeyLocal(Chuck->GetActorLocation()).ToCompactString(), *Nook.ToCompactString());
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("FeyMap/Life_Nook.png"), false, false);
                Check(FVector::Dist2D(FeyLocal(Chuck->GetActorLocation()), Nook) < 260.f, TEXT("walks_into_nook"));
                Check(Chuck->GetCigarettes() > R->Cigs0, TEXT("cigarettes_inside"));
                Map->LoadNow(Orchard); Chuck->ResetAtLocation(FeyCheckpointWorld(Orchard)); R->CrackStep = 10; R->PhaseStart = Now; return;
            }
            case 10: { if (Since < 1.f) return; R->CrackStep = 11; R->bPlaced = false; return; }
            case 11:   // pollen: half speed on the ground in a bed
            {
                const FFeyPollen& Pb = Life->GetPollen()[1]; const FVector Ax = FQuat(FRotator(0, Pb.Yaw, 0)).GetForwardVector(), C0(Pb.C.X, Pb.C.Y, 0.f);
                if (!R->bPlaced) { Put(C0 - Ax * (Pb.R.X + 700.f), Ax); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 6.f)
                {
                    Drive(Ax); const float Sp = Chuck->GetVelocity().Size2D(); const FVector L = FeyLocal(Chuck->GetActorLocation()) - FVector(0, 0, Stand);
                    if (Since > 1.5f && Chuck->GetCharacterMovement()->IsMovingOnGround()) { if (Life->InPollen(L)) { R->SpeedIn += Sp; ++R->SpeedInN; } else if (FVector::Dist2D(L, C0) > Pb.R.X + 150.f) { R->SpeedOut += Sp; ++R->SpeedOutN; } }
                    return;
                }
                const float In = R->SpeedIn / FMath::Max(1, R->SpeedInN), Out = R->SpeedOut / FMath::Max(1, R->SpeedOutN);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_POLLEN out=%.0f in=%.0f samples=%d/%d"), Out, In, R->SpeedOutN, R->SpeedInN);
                Check(R->SpeedInN > 10 && R->SpeedOutN > 10 && In < Out * .65f, TEXT("pollen_slows"));
                R->bPlaced = false; R->CrackStep = 12; R->PhaseStart = Now; return;
            }
            case 12:   // (2026-11-23) an orchid wakes, turns to him off its lane and aims; then a seed, standing in its lane
            {
                const FFeyOrchid& O = Life->GetOrchids()[0];
                const FVector Road(-O.Dir.Y, O.Dir.X, 0.f);
                if (R->OrchidSub == 0)
                {
                    const FVector At = O.Pos + O.Dir * 450.f + Road * 380.f;
                    if (!R->bPlaced) { AFeyLife::bOrchidSure = false; Put(At, O.Pos - At); R->Aimed0 = AFeyLife::OrchidAimed; R->bPlaced = true; R->PhaseStart = Now; return; }
                    if (Since < 4.5f) return;
                    const FVector To = (At - O.Pos).GetSafeNormal2D();
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_ORCHID_AIM aimed=%d look_dot=%.2f lane_dot=%.2f"), AFeyLife::OrchidAimed - R->Aimed0, FVector::DotProduct(O.Look.GetSafeNormal2D(), To), FVector::DotProduct(O.Dir, To));
                    Check(AFeyLife::OrchidAimed > R->Aimed0, TEXT("orchid_aims_off_lane"));
                    Check(FVector::DotProduct(O.Look.GetSafeNormal2D(), To) > .9f, TEXT("orchid_turns_to_him"));
                    Shot(TEXT("OrchidAim")); R->OrchidSub = 1; R->bPlaced = false; R->PhaseStart = Now;
                    return;
                }
                if (!R->bPlaced) { AFeyLife::bOrchidSure = true; Put(O.Pos + O.Dir * 450.f, -O.Dir); R->Hits0 = AFeyLife::OrchidHits; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 6.f && AFeyLife::OrchidHits == R->Hits0) return;
                AFeyLife::bOrchidSure = false;
                Check(AFeyLife::OrchidShots > 0 && AFeyLife::OrchidHits > R->Hits0, TEXT("orchid_hits"));
                R->bPlaced = false; R->CrackStep = 13; R->PhaseStart = Now; return;
            }
            case 13:   // (part two) a thorn mite runs at him and bites
            {
                if (Life->GetMites().IsEmpty()) { Check(false, TEXT("mites_built")); R->CrackStep = 14; return; }
                AFeyMite* M = Life->GetMites()[0];
                if (!R->bPlaced) { M->ResetMite(); Put(M->GetLocal() + FVector(220.f, 0, 0), FVector(-1, 0, 0)); R->Hits0 = AFeyMite::Bites; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.f && Since < 1.05f) Shot(TEXT("Mite"));
                if (AFeyMite::Bites == R->Hits0 && Since < 6.f) return;
                Check(AFeyMite::Bites > R->Hits0, TEXT("mite_bites"));
                R->bPlaced = false; R->CrackStep = 14; R->PhaseStart = Now; return;
            }
            case 14:   // one scratch kills it
            {
                if (Since < .5f || Life->GetMites().IsEmpty()) { if (Life->GetMites().IsEmpty()) R->CrackStep = 15; return; }
                AFeyMite* M = Life->GetMites()[0]; M->ResetMite();
                const FVector At = M->GetLocal() + FVector(32.f, 0, 0); Put(At, FVector(-1, 0, 0));
                FeyScratch(Chuck, Chuck->GetActorLocation(), FVector(-1, 0, 0), AChuckCharacter::SlashReach);
                Check(M->IsDead(), TEXT("mite_dies"));
                R->bPlaced = false; R->CrackStep = 15; R->PhaseStart = Now; return;
            }
            case 15:   // a still of a living mite, close
            {
                if (Life->GetMites().Num() > 1)
                {
                    const FVector M = Life->GetMites()[1]->GetLocal();
                    Cam(M + FVector(55.f, 40.f, 30.f), M + FVector(0, 0, 6.f));
                    if (Since < 1.5f) return;
                    if (!R->bPlaced) { Shot(TEXT("MiteClose")); R->bPlaced = true; }
                    if (Since < 2.1f) return;
                    R->bPlaced = false;
                }
                Uncam(); R->CrackStep = 16; R->PhaseStart = Now; return;
            }
            case 16: { Map->LoadNow(LilyMere); Chuck->ResetAtLocation(FeyCheckpointWorld(LilyMere)); R->bPlaced = false; R->CrackStep = 20; R->PhaseStart = Now; return; }
            case 20: { if (Since < 1.f) return; R->CrackStep = 21; return; }
            case 21:   // a moth's touch, standing under its haunt
            {
                const FFeyMoth& M = Life->GetMoths()[0];
                if (!R->bPlaced) { Put(M.Home - FVector(0, 0, 72.f), M.Axis); R->Hits0 = AFeyLife::MothTouches; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 9.f) return;
                Check(AFeyLife::MothTouches > R->Hits0, TEXT("moth_touches"));
                R->CrackStep = 22; R->bPlaced = false; R->PhaseStart = Now;
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("FeyMap/Life_Moth.png"), false, false);
                return;
            }
            case 22:   // (part two) a flower raises pads out of the deep water
            {
                if (Life->GetFlowers().IsEmpty() || Life->GetPadRises().IsEmpty()) { Check(false, TEXT("pad_flower_built")); R->CrackStep = 24; return; }
                const FFeyFlower& F = Life->GetFlowers()[0]; const FVector To = (Life->GetIslet() - F.Pos).GetSafeNormal2D();
                if (!R->bPlaced) { Put(F.Pos + To * 45.f, -To); FeyScratch(Chuck, Chuck->GetActorLocation(), -To, 120.f); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 1.f) return;
                if (Since < 5.f) { if (Since > 2.f) FaceAt(Life->GetIslet()); if (Since > 3.f) { const FVector Is = Life->GetIslet(); Cam(F.Pos - To * 250.f + FVector(0, 0, 240.f), FVector(Is.X, Is.Y, 0.f) - To * 150.f); } if (Since > 4.3f && Since < 4.36f) Shot(TEXT("Pads")); return; }
                Uncam();
                Check(Life->GetPadRises()[0].Up > .95f, TEXT("pads_rise"));
                R->Cigs0 = Chuck->GetCigarettes(); R->bPlaced = false; R->CrackStep = 23; R->PhaseStart = Now; return;
            }
            case 23:   // from the last pad onto the islet
            {
                const FVector Is = Life->GetIslet(); const FVector& Pd = Life->GetPadRises()[0].Pads.Last();
                if (!R->bPlaced) { Chuck->ResetAtLocation(FeyWorld(FVector(Pd.X, Pd.Y, 2.f + Stand + 6.f))); FaceAt(Is); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 3.f) { Drive(FVector(Is.X, Is.Y, 0.f) - FeyLocal(Chuck->GetActorLocation())); return; }
                const FVector L = FeyLocal(Chuck->GetActorLocation());
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_ISLET at=%s islet=%s drowned=%d"), *L.ToCompactString(), *Is.ToCompactString(), Map->GetDrowned());
                Check(FVector::Dist2D(L, Is) < Is.Z && L.Z > -10.f, TEXT("reaches_islet"));
                Check(Chuck->GetCigarettes() > R->Cigs0, TEXT("islet_cigarettes"));
                R->CrackStep = 24; R->PhaseStart = Now; return;
            }
            case 24: { Map->LoadNow(Deepwood); Chuck->ResetAtLocation(FeyCheckpointWorld(Deepwood)); R->bPlaced = false; R->CrackStep = 30; R->PhaseStart = Now; return; }
            case 30: { if (Since < 1.f) return; R->CrackStep = 31; R->bPlaced = false; return; }
            case 31:   // a redcap at the camp sees him and comes
            {
                if (Life->GetRedcaps().IsEmpty()) { Check(false, TEXT("redcaps_built")); R->CrackStep = 34; return; }
                ATempleSkeleton* Rc = Life->GetRedcaps()[0];
                const FVector Rl = FeyLocal(Rc->GetActorLocation()), Fw = Rc->GetActorForwardVector().GetSafeNormal2D();
                if (!R->bPlaced) { Put(Rl + Fw * 620.f, -Fw); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 2.f && Since < 2.05f) Shot(TEXT("Redcap"));
                const FString St = Rc->GetStateName();
                if (Since < 5.f && St != TEXT("Chase") && St != TEXT("Attack")) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_REDCAP state=%s"), *St);
                Check(St == TEXT("Chase") || St == TEXT("Attack") || St == TEXT("Recover"), TEXT("redcap_chases"));
                R->bPlaced = false; R->CrackStep = 32; R->PhaseStart = Now; return;
            }
            case 32:   // under the low root into the hollow: they cannot follow
            {
                const FVector H = Life->GetCache();
                if (!R->bPlaced) { R->Cigs0 = Chuck->GetCigarettes(); Put(FVector(H.X, H.Y, 0.f), (Life->GetCamp() - H).GetSafeNormal2D()); R->MinDist = 1e9f; R->bPlaced = true; R->PhaseStart = Now; return; }
                Chuck->SetSanity(AChuckCharacter::MaxSanity);
                for (ATempleSkeleton* Rc : Life->GetRedcaps()) if (IsValid(Rc)) R->MinDist = FMath::Min(R->MinDist, (float)FVector::Dist2D(FeyLocal(Rc->GetActorLocation()), H));
                if (Since > 3.f && Since < 3.05f) Shot(TEXT("Hollow"));
                if (Since < 8.f) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_HOLLOW closest_redcap=%.0f"), R->MinDist);
                Check(R->MinDist > 240.f, TEXT("redcaps_kept_out"));
                Check(Chuck->GetCigarettes() > R->Cigs0, TEXT("hollow_cigarettes"));
                R->CrackStep = 33; R->PhaseStart = Now; return;
            }
            case 33:   // five scratches put one down
            {
                ATempleSkeleton* Rc = nullptr; for (ATempleSkeleton* Q : Life->GetRedcaps()) if (IsValid(Q) && !Q->IsDead()) { Rc = Q; break; }
                if (!Rc) { Check(false, TEXT("redcap_dies")); R->CrackStep = 34; return; }
                const FVector Sw = (Rc->GetActorLocation() - Chuck->GetActorLocation()).GetSafeNormal2D();
                for (int32 K = 0; K < ATempleSkeleton::RedcapHealth; ++K) Rc->TakeSlash(Sw);
                Check(Rc->IsDead(), TEXT("redcap_dies"));
                R->bPlaced = false; R->CrackStep = 35; R->PhaseStart = Now; return;
            }
            case 35:   // stills: a living redcap close, then the camp
            {
                ATempleSkeleton* Rc = nullptr; for (ATempleSkeleton* Q : Life->GetRedcaps()) if (IsValid(Q) && !Q->IsDead()) { Rc = Q; break; }
                const FVector Cp = Life->GetCamp();
                if (Rc && Since < 2.1f)
                {
                    const FVector Rl = FeyLocal(Rc->GetActorLocation()) - FVector(0, 0, 50.f), Fw = Rc->GetActorForwardVector().GetSafeNormal2D(), Rt(-Fw.Y, Fw.X, 0.f);
                    Cam(Rl + Fw * 210.f + Rt * 90.f + FVector(0, 0, 75.f), Rl + FVector(0, 0, 62.f));
                    if (Since > 1.5f && !R->bPlaced) { Shot(TEXT("RedcapClose")); R->bPlaced = true; }
                    return;
                }
                const FVector Out(FVector2D(Life->GetCache() - Cp).GetSafeNormal(), 0.f);
                Cam(Cp - Out * 900.f + FVector(0, 0, 260.f), Cp + FVector(0, 0, 60.f));
                if (Since < 3.6f) { R->bPlaced = false; return; }
                if (!R->bPlaced) { Shot(TEXT("Camp")); R->bPlaced = true; }
                if (Since < 4.2f) return;
                R->bPlaced = false; Uncam(); R->CrackStep = 34; R->PhaseStart = Now; return;
            }
            case 34: { Map->LoadNow(Highmeadow); Chuck->ResetAtLocation(FeyCheckpointWorld(Highmeadow)); R->bPlaced = false; R->CrackStep = 40; R->PhaseStart = Now; return; }
            case 40: { if (Since < 1.f) return; R->CrackStep = 41; R->bPlaced = false; return; }
            case 41:   // the beast sees him from far off, roars, and comes
            {
                AFeyDisplacer* B = Life->GetBeast(); if (!B) { Check(false, TEXT("beast_built")); R->CrackStep = 50; return; }
                if (!R->bPlaced) { B->ResetBeast(); Put(B->GetLocal() + FVector(1200.f, -300.f, 0.f), FVector(-1, .25f, 0)); R->Hits0 = AFeyDisplacer::Notices; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 2.2f && Since < 2.25f) Shot(TEXT("Beast"));
                if (AFeyDisplacer::Notices == R->Hits0 && Since < 6.f) return;
                Check(AFeyDisplacer::Notices > R->Hits0, TEXT("beast_notices"));
                R->Hits0 = AFeyDisplacer::LashHits; R->CrackStep = 42; R->PhaseStart = Now; return;
            }
            case 42:   // standing in the open, he is lashed
            {
                AFeyDisplacer* B = Life->GetBeast(); FaceAt(B->GetImageLocal());
                if (Since > 1.5f && Since < 1.55f) Shot(TEXT("BeastNear"));
                if (AFeyDisplacer::LashHits == R->Hits0 && Since < 12.f) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_BEAST state=%s lashes=%d hits=%d"), B->GetStateName(), AFeyDisplacer::Lashes, AFeyDisplacer::LashHits);
                Check(AFeyDisplacer::LashHits > R->Hits0, TEXT("beast_lashes"));
                Chuck->SetSanity(AChuckCharacter::MaxSanity); R->bPlaced = false; R->CrackStep = 43; R->PhaseStart = Now; return;
            }
            case 43:   // in a hollow log he is safe; it paces outside and gives up
            {
                AFeyDisplacer* B = Life->GetBeast(); const FFeyRefuge Rf = Life->GetRefuges()[2];
                if (!R->bPlaced) { Put(FVector(Rf.C.X, Rf.C.Y, 0.f), (B->GetLocal() - FVector(Rf.C, 0.f)).GetSafeNormal2D()); R->Hits0 = AFeyDisplacer::LashHits; R->Hits1 = AFeyDisplacer::Paces; R->MinDist = 1e9f; R->bPlaced = true; R->PhaseStart = Now; return; }
                Chuck->SetSanity(AChuckCharacter::MaxSanity);
                R->MinDist = FMath::Min(R->MinDist, (float)FVector::Dist2D(B->GetLocal(), FVector(Rf.C, 0.f)));
                if (Since > 4.f && Since < 4.05f) Shot(TEXT("Log"));
                if (Since < 10.f) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_REFUGE closest=%.0f state=%s paces=%d"), R->MinDist, B->GetStateName(), AFeyDisplacer::Paces);
                Check(R->MinDist > Rf.R, TEXT("beast_kept_out"));
                Check(AFeyDisplacer::LashHits == R->Hits0, TEXT("safe_in_log"));
                Check(AFeyDisplacer::Paces > R->Hits1, TEXT("beast_paces"));
                R->bPlaced = false; R->CrackStep = 44; R->PhaseStart = Now; return;
            }
            case 44:   // stills of the beast walking: from its side, then ahead of it
            {
                AFeyDisplacer* B = Life->GetBeast(); const FVector I = B->GetImageLocal();
                const FVector Fw = FRotator(0, B->GetActorRotation().Yaw, 0).Vector(), Rt(-Fw.Y, Fw.X, 0.f);
                if (Since < 2.5f) { Cam(I + Rt * 720.f + Fw * 120.f + FVector(0, 0, 170.f), I + FVector(0, 0, 105.f)); if (Since > 1.8f && !R->bPlaced) { Shot(TEXT("BeastSide")); R->bPlaced = true; } return; }
                if (Since < 2.6f) { R->bPlaced = false; return; }
                if (Since < 5.f) { Cam(I + Fw * 650.f + Rt * 260.f + FVector(0, 0, 150.f), I + FVector(0, 0, 110.f)); if (Since > 4.3f && !R->bPlaced) { Shot(TEXT("BeastFront")); R->bPlaced = true; } return; }
                R->bPlaced = false;
                Uncam(); R->CrackStep = 50; R->PhaseStart = Now; return;
            }
            // ---- (stage 6 and 7, 2026-11-22) the maze and the stair
            case 50: { AFeyLife::RevealAfter = 4.f; AFeyLife::RevealEvery = 6.f; Map->LoadNow(Maze); Chuck->ResetAtLocation(FeySpotWorld(SpotOf(TEXT("FeyMazeMiddle")))); R->bPlaced = false; R->CrackStep = 51; R->PhaseStart = Now; return; }
            case 51:   // lost a while, hit plants appear
            {
                if (Since < 1.f) { R->Hits0 = AFeyLife::Reveals; return; }
                if (AFeyLife::Reveals == R->Hits0 && Since < 12.f) return;
                Check(AFeyLife::Reveals > R->Hits0, TEXT("lost_reveals"));
                AFeyLife::RevealAfter = 240.f; AFeyLife::RevealEvery = 150.f; R->bPlaced = false; R->CrackStep = 52; R->PhaseStart = Now; return;
            }
            case 52:   // a revealed plant opens its shortcut, and he walks through
            {
                // (the plant is chosen once: another may be revealed while this one is tried)
                static int32 Fi = -1; if (!R->bPlaced && Since < .1f) { Fi = -1; for (int32 I = 0; I < Life->GetFlowers().Num(); ++I) if (Life->GetFlowers()[I].Sc >= 0 && !Life->GetFlowers()[I].bHidden) { Fi = I; break; } }
                if (Fi < 0) { Check(false, TEXT("shortcut_opens")); R->CrackStep = 53; return; }
                const FFeyFlower& F = Life->GetFlowers()[Fi]; const FMazeShortcut& Sc = MazeShortcuts()[F.Sc]; const FVector Mid(Sc.Mid, 0.f);
                const FVector To = (Mid - F.Pos).GetSafeNormal2D();
                if (!R->bPlaced) { if (Since < 2.5f) return; Put(F.Pos - To * 45.f, To); FeyScratch(Chuck, Chuck->GetActorLocation(), To, 120.f); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.5f && Since < 1.55f) Shot(TEXT("MazeShortcut"));
                if (Since < 4.5f) return;
                if (Since < 4.6f) { Check(Life->GetShortcutUp()[F.Sc] < .05f, TEXT("shortcut_opens")); Put(F.Pos - To * 30.f, To); return; }
                if (Since < 7.5f) { Drive(To); return; }
                const float Past = FVector::DotProduct(FeyLocal(Chuck->GetActorLocation()) - Mid, To);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_SHORTCUT past=%.0f"), Past);
                Check(Past > 60.f, TEXT("walks_through_shortcut"));
                R->bPlaced = false; R->CrackStep = 53; R->PhaseStart = Now; return;
            }
            case 53:   // the heart behind its vines: blocked, then opened by its plant
            {
                const FFeyVines& V = Life->GetVines()[0]; const FFeyFlower& F = Life->GetFlowers()[0];
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                const FVector In = (FVector(Hub, 0.f) - V.C).GetSafeNormal2D();
                if (!R->bPlaced) { Put(V.C - In * 150.f, In); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 2.f) { Drive(In); return; }
                if (Since < 2.1f) { Check(FVector2D::Distance(FVector2D(FeyLocal(Chuck->GetActorLocation())), Hub) > R0 - 20.f, TEXT("heart_vines_block")); Shot(TEXT("MazeHeartGate")); return; }
                if (Since < 2.6f) return;
                if (Since < 2.7f) { const FVector Fd = (F.Pos - V.C).GetSafeNormal2D(); Put(F.Pos - Fd * 45.f, Fd); FeyScratch(Chuck, Chuck->GetActorLocation(), Fd, 120.f); return; }
                if (Since < 7.f) return;
                if (Since < 7.1f) { Check(V.Up < .05f, TEXT("heart_opens")); Put(V.C - In * 150.f, In); return; }
                if (Since < 10.f) { Drive(In); return; }
                Check(FVector2D::Distance(FVector2D(FeyLocal(Chuck->GetActorLocation())), Hub) < R0 - 120.f, TEXT("walks_into_heart"));
                R->bPlaced = false; R->CrackStep = 54; R->PhaseStart = Now; return;
            }
            case 54:   // on a hedge top, the bees put him back down
            {
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                const FVector2D Q = Hub + FVector2D(FMath::Cos(1.1f), FMath::Sin(1.1f)) * (R0 + 8.f * Pitch); const float Gz = LandZ(Maze, Q.X, Q.Y);
                if (!R->bPlaced) { R->Hits0 = AFeyLife::BeeKnocks; Chuck->ResetAtLocation(FeyWorld(FVector(Q.X, Q.Y, Gz + Tall + Stand + 8.f))); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.f && Since < 1.05f) Shot(TEXT("MazeBees"));
                if (Since < 5.f) return;
                const float Fz = FeyLocal(Chuck->GetActorLocation()).Z - Stand;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_BEES knocks=%d feet=%.0f ground=%.0f"), AFeyLife::BeeKnocks, Fz, Gz);
                Check(AFeyLife::BeeKnocks > R->Hits0, TEXT("bees_knock"));
                Check(Fz < Gz + 200.f, TEXT("bees_put_him_down"));
                R->bPlaced = false; R->CrackStep = 55; R->PhaseStart = Now; return;
            }
            case 55:   // a statue turns while he looks away
            {
                // (2026-11-23) the first turning statue on the way through: him in the corridor's middle 2.6 m along, facing away
                FMazeMark Mk; for (const FMazeMark& M : MazeMarks()) if (M.Kind == 1) { Mk = M; break; }
                const FVector2D Tn(-Mk.In.Y, Mk.In.X), Sq = Mk.P + Mk.In * 82.f + Tn * 260.f;
                if (!R->bPlaced) { R->Hits0 = AFeyLife::StatueTurns; Put(FVector(Sq, 0.f), FVector(Tn, 0.f)); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 2.f) return;
                Check(AFeyLife::StatueTurns > R->Hits0, TEXT("statue_turns"));
                R->bPlaced = false; R->CrackStep = 56; R->PhaseStart = Now; return;
            }
            case 56:   // the stair: up from the heart, onto the landing, and no further
            {
                const int32 Ls = SpotOf(TEXT("FeyCloudLanding")); const FVector Land = SpotAt(Ls);
                if (!R->bPlaced) { Chuck->ResetAtLocation(FeyWorld(Land + FVector(0, 0, Stand + 6.f))); Chuck->SetActorRotation(FRotator(0, SpotYaw(Ls), 0)); Chuck->Recenter(); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.5f && Since < 1.55f) Shot(TEXT("CloudLanding"));
                if (Since < 2.5f) return;
                if (Since < 2.6f) { const float Fz = FeyLocal(Chuck->GetActorLocation()).Z - Stand; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_LANDING feet=%.0f landing=%.0f"), Fz, Land.Z); Check(FMath::Abs(Fz - Land.Z) < 40.f, TEXT("landing_holds")); return; }
                // Walk on along the stair's way: the boundary stops him.
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                const FVector2D Rd = (FVector2D(FeyLocal(Chuck->GetActorLocation())) - Hub).GetSafeNormal(); const FVector Tan(-Rd.Y, Rd.X, 0.f);
                if (Since < 9.f) { Drive(Tan); if (R->Frame % 20 == 0) { const FVector L = FeyLocal(Chuck->GetActorLocation()); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_LANDWALK r=%.0f a=%.1f z=%.0f"), FVector2D::Distance(FVector2D(L), Hub), FMath::RadiansToDegrees(FMath::Atan2(L.Y - Hub.Y, L.X - Hub.X)), L.Z - Stand); } return; }
                const float Fz = FeyLocal(Chuck->GetActorLocation()).Z - Stand;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_BOUNDARY feet=%.0f at=%s"), Fz, *FeyLocal(Chuck->GetActorLocation()).ToCompactString());
                Check(Fz > Land.Z - 60.f, TEXT("boundary_holds"));
                R->bPlaced = false; R->CrackStep = 57; R->PhaseStart = Now; return;
            }
            case 57:   // and the steps climb: from the heart, along the stair, up
            {
                const int32 Hs = SpotOf(TEXT("FeyMazeHeart"));
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                // (from just before the first step, facing up the stair: it begins at 200 degrees, 11 m out, and turns anticlockwise)
                const float A0 = FMath::DegreesToRadians(200.f) - 600.f / 1100.f; const FVector Foot(Hub + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * 1100.f, SpotAt(Hs).Z);
                if (!R->bPlaced) { Put(Foot, FVector(-FMath::Sin(A0), FMath::Cos(A0), 0.f)); R->MinDist = FeyLocal(Chuck->GetActorLocation()).Z; R->bPlaced = true; R->PhaseStart = Now; return; }
                const FVector L = FeyLocal(Chuck->GetActorLocation()); const FVector2D Rd = (FVector2D(L) - Hub).GetSafeNormal();
                const float Rn = FVector2D::Distance(FVector2D(L), Hub);
                // follow the spiral: along it, a little outward as it widens
                if (Since < 24.f && R->Frame % 20 == 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_STAIRWALK r=%.0f a=%.1f z=%.0f"), Rn, FMath::RadiansToDegrees(FMath::Atan2(Rd.Y, Rd.X)), L.Z - Stand);
                if (Since < 24.f) { Drive(FVector(-Rd.Y, Rd.X, 0.f) + FVector(Rd, 0.f) * FMath::Clamp((1100.f + 1100.f * FMath::Clamp((L.Z - 30.f) / 1900.f, 0.f, 1.f) - Rn) / 300.f, -.6f, .6f)); return; }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_STAIR from=%.0f to=%.0f"), R->MinDist, L.Z);
                Check(L.Z > R->MinDist + 250.f, TEXT("stair_climbs"));
                R->bPlaced = false; R->CrackStep = 58; R->PhaseStart = Now; return;
            }
            // ---- (2026-11-23) the maze's parkour and thorns: 58 a short bed (slow, pricks), 59 the trellis run over a bed, 60 the
            // ivy hoops, 61 the walkway (the ladder up, along, the steps down), 62 a root gap crawled through.
            case 58: case 59: case 60: case 61: case 63:   // (63: the hoops on the way through itself, three of them)
            {
                const int32 Want = Step == 58 ? 3 : Step == 59 ? 0 : Step == 60 || Step == 63 ? 1 : 2;
                static const TCHAR* Names[] = {TEXT("trellis"), TEXT("hoops"), TEXT("walkway"), TEXT("thorns")};
                const TArray<FMazeParkour>& Pks = MazeParkour(); int32 Pi = -1; for (int32 I = 0; I < Pks.Num() && Pi < 0; ++I) if (Pks[I].Kind == Want && (Want != 1 || (Step == 63) == (Pks[I].NHoop == 3))) Pi = I;
                if (Pi < 0) { Check(false, *FString::Printf(TEXT("%s_built"), Names[Want])); R->bPlaced = false; R->CrackStep = Step + 1; R->PhaseStart = Now; return; }
                const FMazeParkour& P = Pks[Pi];
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                auto Pt = [&](float S, float X) { const FVector2D Q = MazeParkourAt(P, S, X); return FVector(Q.X, Q.Y, LandZ(Maze, Q.X, Q.Y)); };
                auto Tan = [&](float S) { return (Pt(S + 5.f, 0.f) - Pt(S - 5.f, 0.f)).GetSafeNormal2D(); };
                const FVector L = FeyLocal(Chuck->GetActorLocation()), Feet = L - FVector(0, 0, Stand);
                const float S = FMath::FindDeltaAngleRadians(P.Ac, FMath::Atan2(L.Y - Hub.Y, L.X - Hub.X)) * P.R * P.Dir, Across = FVector2D::Distance(FVector2D(L), Hub) - P.R;
                const float Up = Feet.Z - LandZ(Maze, Feet.X, Feet.Y);
                auto Keep = [&](float X) { const FVector Out = (Pt(S, 10.f) - Pt(S, 0.f)).GetSafeNormal2D(); Drive(Tan(S) + Out * FMath::Clamp((X - Across) / 60.f, -.5f, .5f)); };   // along the way, held X across
                const float Lane = Want == 0 ? P.Side * (TrellisFace - 38.f) : Want == 2 ? WalkAcross : 0.f;
                const float From = Want == 0 ? P.Bed0 - 300.f : Want == 1 ? -(P.NHoop - 1) * .5f * HoopStep : Want == 2 ? LadderFoot - 60.f : P.Bed0 - 100.f;
                if (!R->bPlaced)
                {
                    Put(Pt(From, Lane), Tan(From)); R->Hits0 = AFeyLife::ThornPricks; R->Hits1 = Want == 0 ? Chuck->GetWallSideRuns() : Want == 1 ? Chuck->GetSwingCatches() : Chuck->GetLadderMounts();
                    R->MinDist = -1e9f; R->PkT0 = -1.; R->PkRate = -1.f; R->PkGrip = -1; R->PkLastJump = 0.; R->bJumped = false; R->bShort = false; R->bPlaced = true; R->PhaseStart = Now; return;
                }
                R->MinDist = FMath::Max(R->MinDist, Up);   // the highest his feet went
                if (R->Frame % 15 == 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_PARKOUR %s s=%.0f across=%.0f up=%.0f gait=%s pricks=%d"), Names[Want], S, Across, Up, Chuck->GetGaitName(), AFeyLife::ThornPricks - R->Hits0);
                auto ShotOnce = [&](bool bNow, const TCHAR* Name) { if (bNow && !R->bShort) { Shot(Name); R->bShort = true; } };
                bool bGoing = false;
                if (Want == 3)   // into the thorns and on through: how fast he gets across
                {
                    bGoing = Since < 5.f && S < P.Bed1 + 60.f;
                    if (bGoing) Keep(0.f);
                    const bool bIn = InMazeThorns(Feet);
                    if (bIn && R->PkT0 < 0.) { R->PkT0 = Now; R->PkS0 = S; }
                    if (R->PkT0 >= 0. && R->PkRate < 0.f && (S > P.Bed1 || !bGoing)) R->PkRate = (S - R->PkS0) / FMath::Max(.1f, float(Now - R->PkT0));
                    ShotOnce(bIn && Now - R->PkT0 > .4f, TEXT("MazeThorns"));
                }
                else if (Want == 0)   // run beside the trellis, jump onto it just before the thorns
                {
                    bGoing = Since < 6.f && S < P.Bed1 + 120.f;
                    if (bGoing) Keep(Lane);
                    if (!R->bJumped && S > P.Bed0 - 50.f) { Chuck->JumpPressed(); R->bJumped = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_TRELLIS_JUMP s=%.0f across=%.0f gait=%s"), S, Across, Chuck->GetGaitName()); }
                    ShotOnce(Chuck->GetWallSideRuns() > R->Hits1 && FCString::Strcmp(Chuck->GetGaitName(), TEXT("WallSide")) == 0, TEXT("MazeTrellisRun"));
                }
                else if (Want == 1)   // a hop up to the first hoop, then hoop to hoop, and off the end
                {
                    bGoing = Since < 14.f && S < P.Bed1 + 120.f;
                    if (bGoing && R->bJumped) Drive(Tan(S));
                    if (!R->bJumped && Since > .8f && Chuck->GetCharacterMovement()->IsMovingOnGround()) { Chuck->JumpPressed(); R->bJumped = true; R->PkLastJump = Now; }
                    else if (Chuck->IsSwinging() && Chuck->GetSwingGrip() != R->PkGrip && Now - R->PkLastJump > .55f) { R->PkGrip = Chuck->GetSwingGrip(); Chuck->JumpPressed(); R->PkLastJump = Now; }
                    ShotOnce(Chuck->IsSwinging() && Chuck->GetSwingCatches() - R->Hits1 >= 2, Step == 63 ? TEXT("MazeWayHoops") : TEXT("MazeHoops"));
                }
                else   // into the ladder's foot, up, along the walkway and down its steps
                {
                    bGoing = Since < 16.f && S < WalkTo + 320.f;
                    if (bGoing) Keep(Lane);
                    ShotOnce(Up > WalkUp - 20.f && S > WalkFrom + 120.f, TEXT("MazeWalkway"));
                }
                if (bGoing || (Since < 18.f && !Chuck->GetCharacterMovement()->IsMovingOnGround())) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_PARKOUR_END %s s=%.0f bed=%.0f..%.0f pricks=%d moves=%d top=%.0f rate=%.0f"), Names[Want], S, P.Bed0, P.Bed1, AFeyLife::ThornPricks - R->Hits0,
                    (Want == 0 ? Chuck->GetWallSideRuns() : Want == 1 ? Chuck->GetSwingCatches() : Chuck->GetLadderMounts()) - R->Hits1, R->MinDist, R->PkRate);
                if (Want == 3)
                {
                    Check(AFeyLife::ThornPricks > R->Hits0, TEXT("thorns_prick"));
                    Check(R->PkRate > 0.f && R->PkRate < 160.f, TEXT("thorns_slow"));
                }
                else
                {
                    const int32 Moves = (Want == 0 ? Chuck->GetWallSideRuns() : Want == 1 ? Chuck->GetSwingCatches() : Chuck->GetLadderMounts()) - R->Hits1;
                    Check(Want == 1 ? Moves >= P.NHoop - 1 : Moves > 0, *FString::Printf(TEXT("%s%s_move"), Step == 63 ? TEXT("way_") : TEXT(""), Names[Want]));
                    if (Want == 2) Check(R->MinDist > WalkUp - 20.f, TEXT("walkway_up"));
                    Check(AFeyLife::ThornPricks == R->Hits0, *FString::Printf(TEXT("%s%s_clears_thorns"), Step == 63 ? TEXT("way_") : TEXT(""), Names[Want]));
                    Check(S > P.Bed1, *FString::Printf(TEXT("%s%s_past_thorns"), Step == 63 ? TEXT("way_") : TEXT(""), Names[Want]));
                }
                R->bPlaced = false; R->CrackStep = Step + 1; R->PhaseStart = Now; return;
            }
            case 62:   // a root gap: crawled through, low under the hedge
            {
                if (!MazeGaps().Num()) { Check(false, TEXT("gap_built")); R->CrackStep = 63; return; }
                const FMazeGap& G = MazeGaps()[0]; const float Yr = FMath::DegreesToRadians(G.Yaw + 90.f); const FVector Th(FMath::Cos(Yr), FMath::Sin(Yr), 0.f), C(G.C, LandZ(Maze, G.C.X, G.C.Y));
                if (!R->bPlaced) { Put(C - Th * 105.f, Th); R->Hits1 = Chuck->GetCrawls(); R->bShort = false; R->bPlaced = true; R->PhaseStart = Now; return; }
                const FVector L = FeyLocal(Chuck->GetActorLocation()); const float Through = FVector::DotProduct(L - C, Th);
                if (R->Frame % 15 == 0)
                {
                    FHitResult Hb; FCollisionQueryParams Qb(SCENE_QUERY_STAT(FeyGapProbe), false, Chuck);
                    const FVector Fw = Chuck->GetActorLocation();
                    const bool bHb = Chuck->GetWorld()->LineTraceSingleByChannel(Hb, Fw, Fw + FVector(Th.X, Th.Y, 0.f) * 60.f, ECC_Pawn, Qb);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_GAP through=%.0f gait=%s ahead=%s at=%.0f z=%.0f"), Through, Chuck->GetGaitName(), bHb && Hb.GetComponent() ? *Hb.GetComponent()->GetName() : TEXT("-"), bHb ? Hb.Distance : -1.f, L.Z - C.Z);
                }
                if (Chuck->IsCrawling() && !R->bShort) { Shot(TEXT("MazeRootGap")); R->bShort = true; }
                const FVector Side(-Th.Y, Th.X, 0.f);
                if (Since < 6.f && Through < 110.f) { Drive(Th + Side * FMath::Clamp(FVector::DotProduct(C - L, Side) / 30.f, -.5f, .5f)); return; }
                Check(Chuck->GetCrawls() > R->Hits1, TEXT("gap_crawls"));
                Check(Through > 80.f, TEXT("gap_through"));
                R->bPlaced = false; R->CrackStep = 63; R->PhaseStart = Now; return;
            }
            case 64:   // (2026-11-23) a squeeze: wall jumps from bulge to bulge up to the hedge tops, and the bees knock him back down
            {
                if (!MazeSqueezes().Num()) { Check(false, TEXT("squeeze_built")); R->CrackStep = 65; return; }
                const FMazeMark& Sq = MazeSqueezes()[0]; const FVector C(Sq.P, LandZ(Maze, Sq.P.X, Sq.P.Y)), In(Sq.In, 0.f);
                const FVector L = FeyLocal(Chuck->GetActorLocation()); const float Up = L.Z - Stand - C.Z;
                if (!R->bPlaced) { Put(C - In * (SqueezeGap * .5f - 28.f), In); R->PkLastJump = 0.f; R->Hits0 = AFeyLife::BeeKnocks; R->Hits1 = Chuck->GetWallJumps(); R->MinDist = 0.f; R->PkT0 = -1.f; R->bShort = false; R->bPlaced = true; R->PhaseStart = Now; return; }
                R->MinDist = FMath::Max(R->MinDist, Up);
                if (R->Frame % (Up > 150.f ? 2 : 15) == 0)
                {
                    const FVector V = Chuck->GetVelocity(); const FVector2D Tn(-Sq.In.Y, Sq.In.X);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_SQUEEZE up=%.0f gait=%s wall_jumps=%d knocks=%d across=%.0f along=%.0f v=%.0f,%.0f,%.0f"), Up, Chuck->GetGaitName(), Chuck->GetWallJumps() - R->Hits1, AFeyLife::BeeKnocks - R->Hits0,
                        FVector2D::DotProduct(FVector2D(L) - Sq.P, Sq.In), FVector2D::DotProduct(FVector2D(L) - Sq.P, Tn), FVector2D::DotProduct(FVector2D(V), Sq.In), FVector2D::DotProduct(FVector2D(V), Tn), V.Z);
                }
                if (Since > .4f && !Chuck->IsWallRunning() && Chuck->GetCharacterMovement()->IsMovingOnGround() && Now - R->PkLastJump > 1.2f) { Chuck->SetTestStickWorld(In); Chuck->AddMovementInput(In, 1.f, true); Chuck->JumpPressed(); R->PkLastJump = Now; }
                // Kick off each wall soon into its run; at the top, on and up. Each kick carries him a little along the slot,
                // so the stick leans back toward its middle, as a player steers.
                { const FVector2D Tn(-Sq.In.Y, Sq.In.X); const float Al = FVector2D::DotProduct(FVector2D(L) - Sq.P, Tn); if (Since > .5f) Chuck->SetTestStickWorld((In - FVector(Tn, 0.f) * FMath::Clamp(Al / 50.f, -1.f, 1.f)).GetSafeNormal()); }
                const bool bWall = Chuck->IsWallRunning();
                if (bWall && R->PkT0 < 0.f) R->PkT0 = Now;
                if (!bWall) R->PkT0 = -1.f;
                if (bWall && R->PkT0 > 0.f && Now - R->PkT0 > .12f) { Chuck->JumpPressed(); R->PkT0 = -1.f; }
                // (in a kick's flight, jump held ready: the next wall is kicked off as soon as it is caught, as a quick player does)

                if (Chuck->IsHanging()) Chuck->JumpPressed();
                if (Up > 600.f && !R->bShort) { Shot(TEXT("MazeSqueeze")); R->bShort = true; }
                if (Since < 25.f && AFeyLife::BeeKnocks == R->Hits0) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_SQUEEZE_END top=%.0f wall_jumps=%d knocks=%d"), R->MinDist, Chuck->GetWallJumps() - R->Hits1, AFeyLife::BeeKnocks - R->Hits0);
                Check(R->MinDist > 650.f, TEXT("squeeze_climbs"));
                Check(AFeyLife::BeeKnocks > R->Hits0, TEXT("squeeze_bees_knock"));
                R->bPlaced = false; R->CrackStep = 65; R->PhaseStart = Now; return;
            }
            case 65:   // (2026-11-23) stills of the second round's things: a smiling head, a satyr, a frowning head, a lantern, low fog
            {
                // Each seen from along its corridor (they are 2.6 m wide): In toward the corridor's middle, Over cm out that way.
                struct FView { const TCHAR* Name; FVector2D P, In; float Over, Along, Up, AimUp; };
                TArray<FView> Views; int32 Smiles = 0;
                for (const FMazeMark& Mk : MazeMarks()) if (Mk.Kind == 0)
                {
                    const int32 K = Smiles++;
                    if (K == 0) Views.Add({TEXT("MazeSmile"), Mk.P, Mk.In, 160.f, 90.f, 150.f, 155.f});
                    if (K == 1) Views.Add({TEXT("MazeSatyr"), Mk.P, Mk.In, 150.f, 260.f, 130.f, 120.f});
                }
                if (MazeFrowns().Num())
                {
                    const FVector4& F = MazeFrowns()[0]; Views.Add({TEXT("MazeFrown"), FVector2D(F.X, F.Y), FVector2D(F.Z, F.W), 120.f, 0.f, 135.f, 155.f});
                    Views.Add({TEXT("MazeFrownWide"), FVector2D(F.X, F.Y), FVector2D(F.Z, F.W), 330.f, 0.f, 260.f, 100.f});
                    if (Since < .05f) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_FROWN at=%.0f,%.0f face=%.2f,%.2f"), F.X, F.Y, F.Z, F.W);
                }
                for (const FVector4& Dc : MazeDecor()) if (int32(Dc.W) == 0) { Views.Add({TEXT("MazeLantern"), FVector2D(Dc.X, Dc.Y), FVector2D(FMath::Cos(Dc.Z), FMath::Sin(Dc.Z)), 160.f, 380.f, 110.f, 180.f}); break; }
                for (const FVector4& Dc : MazeDecor()) if (int32(Dc.W) == 4) { Views.Add({TEXT("MazeFog"), FVector2D(Dc.X, Dc.Y), FVector2D::ZeroVector, 0.f, 700.f, 70.f, 20.f}); break; }
                const int32 Vi = int32(Since / 1.6f);
                if (Vi >= Views.Num()) { Uncam(); R->CrackStep = 66; R->PhaseStart = Now; return; }
                FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                const FView& V = Views[Vi]; const float Z = LandZ(Maze, V.P.X, V.P.Y);
                const FVector2D Rad = (V.P - Hub).GetSafeNormal(), Tn(-Rad.Y, Rad.X);
                const FVector2D E2 = V.P + V.In * V.Over + Tn * V.Along;
                Cam(FVector(E2, Z + V.Up), FVector(V.P, Z + V.AimUp));
                const float Ph = Since - Vi * 1.6f;
                if (Ph > .8f && Ph < .8f + FApp::GetDeltaTime() * 1.5f) Shot(V.Name);
                return;
            }
            // ---- (2026-11-24) the small delights (FeyDelights.h): each found and tried, with stills.
            case 66: case 70: case 73: case 76:   // load the district for the next tries; wait for its delights
            {
                const int32 Dd = Step == 66 ? Riverbank : Step == 70 ? LilyMere : Step == 73 ? Highmeadow : Deepwood;
                if (!R->bPlaced) { Uncam(); Map->LoadNow(Dd); Chuck->ResetAtLocation(FeyCheckpointWorld(Dd)); R->bPlaced = true; R->PhaseStart = Now; return; }
                AFeyDelights* Dl = DelightsOf(Dd);
                if ((!Dl || !Dl->IsBuilt()) && Since < 10.f) return;
                Check(Dl && Dl->IsBuilt(), TEXT("delights_built"));
                R->bPlaced = false; R->CrackStep = Dl ? Step + 1 : (Step == 66 ? 70 : Step == 70 ? 73 : Step == 73 ? 76 : 80); R->PhaseStart = Now; R->OrchidSub = 0; return;
            }
            case 67:   // a fairy ring: walked into, the music dips, a bell, motes
            {
                AFeyDelights* Dl = DelightsOf(Riverbank);
                if (!Dl->GetRings().Num()) { Check(false, TEXT("ring_built")); R->CrackStep = 68; return; }
                const AFeyDelights::FRing& Rg = Dl->GetRings()[0];
                if (!R->bPlaced) { Put(Rg.C + FVector(Rg.R + 260.f, 0, 0), FVector(-1, 0, 0)); R->Hits0 = AFeyDelights::RingVisits; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 2.4f && FVector::Dist2D(FeyLocal(Chuck->GetActorLocation()), Rg.C) > 30.f) { Drive(Rg.C - FeyLocal(Chuck->GetActorLocation())); return; }
                if (Since < 4.5f) { if (Since > 3.4f && !R->bShort) { Cam(Rg.C + FVector(420.f, 260.f, 230.f), Rg.C + FVector(0, 0, 30.f)); } if (Since > 4.2f && !R->bShort) { Shot(TEXT("FairyRing")); R->bShort = true; } return; }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_RING visits=%d dip=%.2f"), AFeyDelights::RingVisits - R->Hits0, AFeyDelights::MusicDip);
                Check(AFeyDelights::RingVisits > R->Hits0, TEXT("ring_entered"));
                Check(AFeyDelights::MusicDip < .8f, TEXT("ring_music_dips"));
                Uncam(); R->bShort = false; R->bPlaced = false; R->CrackStep = 68; R->PhaseStart = Now; return;
            }
            case 68:   // a puffball scratched, and one walked through: gold spores
            {
                AFeyDelights* Dl = DelightsOf(Riverbank);
                if (Dl->GetPuffs().Num() < 2) { Check(false, TEXT("puffs_built")); R->CrackStep = 69; return; }
                const FVector P0 = Dl->GetPuffs()[0].P;
                if (!R->bPlaced) { Put(P0 + FVector(70.f, 0, 0), FVector(-1, 0, 0)); R->Hits0 = AFeyDelights::Bursts; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > .6f && R->OrchidSub == 0) { FeyScratch(Chuck, Chuck->GetActorLocation(), (FeyWorld(P0) - Chuck->GetActorLocation()).GetSafeNormal2D(), 120.f); R->OrchidSub = 1; }
                if (Since > .7f && Since < 1.6f) Cam(P0 + FVector(260.f, 180.f, 120.f), P0 + FVector(0, 0, 40.f));
                if (Since > 1.4f && !R->bShort) { Shot(TEXT("Spores")); R->bShort = true; }
                if (Since < 2.f) return;
                Check(AFeyDelights::Bursts > R->Hits0, TEXT("puff_scratched_bursts"));
                Uncam(); R->bShort = false; R->bPlaced = false; R->CrackStep = 69; R->PhaseStart = Now; return;
            }
            case 69:
            {
                // (one standing, away from the first, which the scratch may have taken with its neighbours)
                AFeyDelights* Dl = DelightsOf(Riverbank); int32 Pi = -1;
                for (int32 I = 1; I < Dl->GetPuffs().Num() && Pi < 0; ++I) if (Dl->GetPuffs()[I].Gone < 0.f && FVector::Dist2D(Dl->GetPuffs()[I].P, Dl->GetPuffs()[0].P) > 400.f) Pi = I;
                if (Pi < 0) { Check(false, TEXT("puff_to_walk")); R->bPlaced = false; R->CrackStep = 70; R->PhaseStart = Now; return; }
                const FVector P1 = Dl->GetPuffs()[Pi].P;
                if (!R->bPlaced) { Put(P1 + FVector(260.f, 0, 0), FVector(-1, 0, 0)); R->Hits0 = AFeyDelights::Bursts; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 3.f && AFeyDelights::Bursts == R->Hits0) { Drive(P1 - FeyLocal(Chuck->GetActorLocation())); return; }
                Check(AFeyDelights::Bursts > R->Hits0, TEXT("puff_walked_bursts"));
                R->bPlaced = false; R->CrackStep = 70; R->PhaseStart = Now; return;
            }
            case 71:   // the wisps: each time he nears the first, they drift on; at the end, cigarettes by the hidden pool
            {
                AFeyDelights* Dl = DelightsOf(LilyMere);
                if (Dl->GetWisps().Num() != 3 || Dl->GetWispWay().Num() < 3) { Check(false, TEXT("wisps_built")); R->CrackStep = 72; return; }
                if (!R->bPlaced) { const FVector W0 = Dl->GetWispWay()[0]; Put(W0 + FVector(900.f, -900.f, 0), FVector(-1, 1, 0)); R->Hits0 = AFeyDelights::WispMoves; R->Hits1 = 0; R->MinDist = 2.f; R->Cigs0 = Chuck->GetCigarettes(); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.f && Since < 1.9f) { const FVector A = Dl->GetWisps()[0].At; Cam(A + FVector(500.f, -380.f, 140.f), A); }
                if (Since > 1.5f && !R->bShort) { Shot(TEXT("Wisps")); R->bShort = true; }
                if (Since < 2.f) return;
                if (Since < 2.05f) Uncam();
                // He follows the first light wherever it has got to.
                const int32 Last = Dl->GetWispWay().Num() - 1;
                if (Since < 50.f && (Dl->GetWispLead() < Last || Chuck->GetCigarettes() == R->Cigs0))
                {
                    const FVector Want = Dl->GetWispLead() < Last ? Dl->GetWisps()[0].At : Dl->GetWispWay().Last();
                    const FVector L = FeyLocal(Chuck->GetActorLocation());
                    if (FVector::Dist2D(L, Want) > 40.f) Drive(Want - L);
                    // (stuck on a root or a pool's edge for 8 s: logged, and put a little nearer, as a player would go round)
                    if (Dl->GetWispLead() != R->Hits1) { R->Hits1 = Dl->GetWispLead(); R->MinDist = Since; }
                    else if (Since - R->MinDist > 8.f)
                    {
                        FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(Chuck);
                        const FVector From = Chuck->GetActorLocation(), Dir = FeyWorld(Want) - FeyWorld(L); 
                        const bool bHit = Chuck->GetWorld()->SweepSingleByChannel(Hit, From, From + Dir.GetSafeNormal2D() * 150.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(25.f), Q);
                        const FVector HL = bHit ? FeyLocal(Hit.ImpactPoint) : FVector::ZeroVector;
                        const auto* WIsm = bHit ? Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()) : nullptr;
                        if (WIsm && Hit.Item >= 0) { FTransform T; WIsm->GetInstanceTransform(Hit.Item, T, true); const FVector C = FeyLocal(T.GetLocation());
                            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_WISPS_BLOCKER centre=%.0f,%.0f scale=%.2f"), C.X, C.Y, T.GetScale3D().X); }
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_WISPS_BLOCK mesh=%s hit=%d actor=%s comp=%s at=%.0f,%.0f,%.0f nz=%.2f feet_z=%.0f"), WIsm && WIsm->GetStaticMesh() ? *WIsm->GetStaticMesh()->GetName() : TEXT("-"), bHit, bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("-"), bHit && Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("-"), HL.X, HL.Y, HL.Z, Hit.ImpactNormal.Z, L.Z);
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_WISPS_ASSIST lead=%d at=%.0f,%.0f"), R->Hits1, L.X, L.Y); Put(Want + (L - Want).GetSafeNormal2D() * 450.f, Want - L); R->MinDist = Since;
                    }
                    return;
                }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_WISPS moves=%d lead=%d cigarettes=%d"), AFeyDelights::WispMoves - R->Hits0, Dl->GetWispLead(), Chuck->GetCigarettes() - R->Cigs0);
                Check(Dl->GetWispLead() == Last, TEXT("wisps_lead_on"));
                Check(Chuck->GetCigarettes() > R->Cigs0, TEXT("wisps_cigarettes"));
                { const FVector E = Dl->GetWispWay().Last(); Cam(E + FVector(380.f, 300.f, 190.f), E + FVector(0, 0, 60.f)); }
                R->bShort = false; R->bPlaced = false; R->CrackStep = 72; R->PhaseStart = Now; return;
            }
            case 72:   // the koi: he lands on a pad near them and they scatter
            {
                AFeyDelights* Dl = DelightsOf(LilyMere);
                if (Since > .5f && !R->bShort) { Shot(TEXT("WispsEnd")); R->bShort = true; }
                if (Since < 1.f) return;
                if (!Dl->GetKoi().Num()) { Check(false, TEXT("koi_built")); Uncam(); R->bShort = false; R->CrackStep = 73; return; }
                const AFeyDelights::FKoi& K = Dl->GetKoi()[0];
                if (!R->bPlaced)
                {
                    Uncam(); FVector Pad(K.Home, 0.f); float Bd = 1e9f;
                    for (const FVector& Pd : FeyMap::LilyPads()) { const float Dp = FVector2D::Distance(FVector2D(Pd), K.At); if (Dp < Bd) { Bd = Dp; Pad = FVector(Pd.X, Pd.Y, 0.f); } }
                    R->PadAt = Pad; Chuck->ResetAtLocation(FeyWorld(Pad + FVector(0, 0, 220.f))); R->Hits0 = AFeyDelights::KoiScatters; R->bPlaced = true; R->PhaseStart = Now; return;
                }
                if (Since > .9f && Since < 1.6f) Cam(R->PadAt + FVector(-450.f, 320.f, 420.f), R->PadAt);
                if (Since > 1.4f && R->OrchidSub == 0) { Shot(TEXT("Koi")); R->OrchidSub = 1; }
                if (Since < 2.5f) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_KOI koi=%d scattered=%d"), Dl->GetKoi().Num(), AFeyDelights::KoiScatters - R->Hits0);
                Check(AFeyDelights::KoiScatters > R->Hits0, TEXT("koi_scatter"));
                Uncam(); R->bShort = false; R->bPlaced = false; R->CrackStep = 73; R->PhaseStart = Now; return;
            }
            case 74:   // the swans below the overlook; the rainbow in the rapids' spray, with the sun behind and against it
            {
                AFeyDelights* Dl = DelightsOf(Highmeadow);
                Check(Dl->GetSwanCount() == 7, TEXT("swans_built")); Check(!Dl->GetRainbow().IsZero(), TEXT("rainbow_built"));
                // The middle swan straight out from the overlook: its time into the crossing.
                const FVector2D O = FeyMap::OverlookAt(); const float Oz = LandZ(Highmeadow, O.X, O.Y);
                float Best = 1e9f, Tb = 0.f; for (float T = 0.f; T < 130.f; T += .5f) { const float D = FMath::Abs(AFeyDelights::SwanAt(3, T).Y - O.Y + 1500.f); if (D < Best) { Best = D; Tb = T; } }
                AFeyDelights::SwanHold = Tb;
                const FVector Mid = AFeyDelights::SwanAt(3, Tb);
                {   // (2026-11-25) nothing under the crossing within 2.5 m of a swan: traced down along the whole line
                    float Clear = 1e9f; FVector Worst = FVector::ZeroVector;
                    FCollisionQueryParams Q; Q.AddIgnoredActor(Dl);
                    for (float T = 0.f; T < 160.f; T += 1.f) for (int32 I = 0; I < 7; ++I)
                    {
                        const FVector Sw = AFeyDelights::SwanAt(I, T); if (FMath::Abs(Sw.Y - O.Y) > 16000.f) continue;
                        FHitResult Hit; const FVector Sww = FeyWorld(Sw);
                        if (Chuck->GetWorld()->LineTraceSingleByChannel(Hit, Sww + FVector(0, 0, 300.f), Sww - FVector(0, 0, 6000.f), ECC_Visibility, Q))
                        {
                            const float C = Sw.Z - FeyLocal(Hit.ImpactPoint).Z; if (C < Clear) { Clear = C; Worst = Sw; }
                            if (C < 250.f && I == 0) { const auto* Ism = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent());
                                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_SWANS_LOW t=%.0f at=%.0f,%.0f,%.0f clear=%.0f land=%.0f hit=%s"), T, Sw.X, Sw.Y, Sw.Z, C, LandZ(FeyMap::DistrictAt(Sw.X, Sw.Y), Sw.X, Sw.Y),
                                    Ism && Ism->GetStaticMesh() ? *Ism->GetStaticMesh()->GetName() : Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("-")); }
                        }
                    }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_SWANS_CLEAR min_cm=%.0f at=%.0f,%.0f,%.0f out_m=%.0f"), Clear, Worst.X, Worst.Y, Worst.Z, (O.X - Mid.X) / 100.f);
                    Check(Clear > 250.f, TEXT("swans_clear"));
                }
                Cam(FVector(O.X - 150.f, O.Y - 200.f, Oz + 120.f), Mid);
                R->CrackStep = 75; R->bShort = false; R->PhaseStart = Now; return;
            }
            case 75:
            {
                AFeyDelights* Dl = DelightsOf(Highmeadow);
                if (Since > 1.2f && R->OrchidSub == 0) { Shot(TEXT("Swans")); R->OrchidSub = 1; }
                if (Since > 1.5f && R->OrchidSub == 1)
                {
                    AFeyDelights::SwanHold = -1.f;
                    // The sun behind the eye: the spray 41 degrees round from the point opposite the sun, 18 m off.
                    const FVector A = FRotator(-10.f, 168.f, 0.f).Vector(), Ring = FRotator(0.f, 41.f, 0.f).RotateVector(FVector(A.X, A.Y, 0.f).GetSafeNormal());
                    const FVector Sp = Dl->GetRainbow(); Cam(Sp - Ring * 1800.f + FVector(0, 0, 80.f), Sp); R->OrchidSub = 2;
                }
                if (Since > 2.6f && R->OrchidSub == 2) { Shot(TEXT("Rainbow")); R->OrchidSub = 3; }
                if (Since > 2.9f && R->OrchidSub == 3) { const FVector A = FRotator(-10.f, 168.f, 0.f).Vector(), Sp = Dl->GetRainbow(); Cam(Sp + FVector(A.X, A.Y, 0.f).GetSafeNormal() * 1800.f + FVector(0, 0, 80.f), Sp); R->OrchidSub = 4; }
                if (Since > 4.f && R->OrchidSub == 4) { Shot(TEXT("RainbowAgainstSun")); R->OrchidSub = 5; }
                if (Since < 4.3f) return;
                Uncam();
                // A signpost turns while he looks away from it.
                if (!Dl->GetSigns().Num()) { Check(false, TEXT("signs_turn_built")); R->CrackStep = 76; R->bPlaced = false; R->PhaseStart = Now; return; }
                const FVector S0 = Dl->GetSigns()[0].At; Put(S0 + FVector(600.f, 0, 0), FVector(1, 0, 0)); R->Hits0 = AFeyDelights::SignTurns; R->CrackStep = 79; R->PhaseStart = Now; return;
            }
            case 79:
            {
                if (Since < 20.f && AFeyDelights::SignTurns == R->Hits0) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_SIGNS turns=%d after=%.1f"), AFeyDelights::SignTurns - R->Hits0, Since);
                Check(AFeyDelights::SignTurns > R->Hits0, TEXT("sign_turns_unseen"));
                R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 76; R->PhaseStart = Now; return;
            }
            case 77:   // the Deepwood's crows: perched and watching; one taking off when he comes close
            {
                AFeyDelights* Dl = DelightsOf(Deepwood);
                if (!R->bPlaced) { UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_CROWS crows=%d"), Dl->GetCrows().Num()); Check(Dl->GetCrows().Num() >= 6, TEXT("crows_built")); }
                if (!Dl->GetCrows().Num()) { R->CrackStep = 80; return; }
                const FVector Cw = Dl->GetCrows()[0].At;
                if (!R->bPlaced) { Put(Cw + FVector(900.f, 0, 0), FVector(-1, 0, 0)); R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since > 1.2f && Since < 2.1f) Cam(Cw + FVector(240.f, 160.f, 90.f), Cw + FVector(0, 0, 20.f));
                if (Since > 1.8f && R->OrchidSub == 0) { Shot(TEXT("Crow")); R->OrchidSub = 1; }
                if (Since < 2.2f) return;
                Uncam();
                R->Hits0 = AFeyDelights::CrowFlights; R->CrackStep = 78; R->bPlaced = false; R->PhaseStart = Now; return;
            }
            case 78:
            {
                // (a crow still on its perch: one may have flown off on a whim meanwhile)
                AFeyDelights* Dl = DelightsOf(Deepwood);
                if (!R->bPlaced) { R->Hits1 = 0; for (int32 I = 0; I < Dl->GetCrows().Num(); ++I) if (Dl->GetCrows()[I].Fly < 0.f) { R->Hits1 = I; break; } }
                const FVector Cw = Dl->GetCrows()[R->Hits1].Perch;
                if (!R->bPlaced) { Put(Cw + FVector(700.f, 0, 0), FVector(-1, 0, 0)); R->Hits0 = AFeyDelights::CrowFlights; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 6.f && AFeyDelights::CrowFlights == R->Hits0) { Drive(Cw - FeyLocal(Chuck->GetActorLocation())); return; }
                // Its flight (or the give-up) began: a still of it in the air 0.8 s on.
                if (R->OrchidSub == 1) { R->OrchidSub = 2; R->MinDist = Since; return; }
                if (R->OrchidSub >= 2 && Since - R->MinDist < 1.2f) { const FVector A = Dl->GetCrows()[R->Hits1].At; Cam(A + FVector(500.f, 300.f, 150.f), A); if (Since - R->MinDist > .8f && R->OrchidSub == 2) { Shot(TEXT("CrowFlight")); R->OrchidSub = 3; } return; }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHT_CROW_FLIGHT flights=%d"), AFeyDelights::CrowFlights - R->Hits0);
                Check(AFeyDelights::CrowFlights > R->Hits0, TEXT("crow_takes_off"));
                Uncam(); R->bPlaced = false; R->CrackStep = 81; R->PhaseStart = Now; return;
            }
            case 81:   // (2026-11-25) the plants react to him: a Deepwood fern from a fixed camera, him 20 m off, then in it
            {
                if (!R->bPlaced)
                {
                    // The fern nearest the crows' first perch, of the Feywild's own (M_FeyFern), on the Deepwood's ground.
                    const FVector Near = DelightsOf(Deepwood)->GetCrows().Num() ? DelightsOf(Deepwood)->GetCrows()[0].Perch : FVector(-70000.f, -33000.f, 0.f);
                    float Best = 1e18f; R->PadAt = FVector::ZeroVector;
                    for (TObjectIterator<UHierarchicalInstancedStaticMeshComponent> It; It; ++It)
                    {
                        if (It->GetWorld() != Chuck->GetWorld() || !It->GetMaterial(0) || !It->GetMaterial(0)->GetName().Contains(TEXT("FeyFern"))) continue;
                        for (int32 I = 0; I < It->GetInstanceCount(); I += 3)
                        {
                            FTransform T; It->GetInstanceTransform(I, T, true); const FVector Lc = FeyLocal(T.GetLocation());
                            if (FeyMap::DistrictAt(Lc.X, Lc.Y) != Deepwood) continue;
                            const float D2 = FVector::DistSquared2D(Lc, Near); if (D2 < Best) { Best = D2; R->PadAt = Lc; }
                        }
                    }
                    if (R->PadAt.IsZero()) { Check(false, TEXT("fern_found")); R->CrackStep = 80; return; }
                    AChultFoliageFade::bTestKeepOn = true;
                    Put(R->PadAt + FVector(2000.f, 0, 0), FVector(-1, 0, 0)); R->bPlaced = true; R->OrchidSub = 20; R->PhaseStart = Now; return;
                }
                const FVector F = R->PadAt, Eye = F + FVector(170.f, 230.f, 120.f);
                Cam(Eye, F + FVector(0, 0, 25.f));
                if (R->OrchidSub == 20 && Since > 1.2f) { Shot(TEXT("FernFar")); R->OrchidSub = 21; }
                if (R->OrchidSub == 21 && Since > 1.5f) { Put(F, FVector(1, 1, 0)); R->OrchidSub = 22; R->MinDist = Since; }
                if (R->OrchidSub == 22 && Since - R->MinDist > 1.2f) { Shot(TEXT("FernNear")); R->OrchidSub = 23; }
                if (Since < 3.2f) return;
                AChultFoliageFade::bTestKeepOn = false; R->OrchidSub = 0;
                Uncam(); R->bPlaced = false; R->CrackStep = 80; R->PhaseStart = Now; return;
            }
            case 80:   // (2026-11-24, the user: the redcaps too dark, the maze's most) A maze redcap, lit by its own fill
            {
                if (!R->bPlaced) { Map->LoadNow(Maze); Chuck->ResetAtLocation(FeyCheckpointWorld(Maze)); R->bPlaced = true; R->PhaseStart = Now; return; }
                AFeyLife* Ml = LifeOf(Maze);
                if ((!Ml || !Ml->GetRedcaps().Num()) && Since < 10.f) return;
                if (!Ml || !Ml->GetRedcaps().Num()) { Check(false, TEXT("maze_redcaps")); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 92; return; }
                const FVector Rc = FeyLocal(Ml->GetRedcaps()[0]->GetActorLocation());
                if (R->OrchidSub < 10) { Ml->GetRedcaps()[0]->ResetToPost(); Cam(Rc + FVector(260.f, 120.f, 60.f), Rc + FVector(0, 0, 10.f)); R->OrchidSub = 10; R->PhaseStart = Now; return; }
                if (Since > 1.f && R->OrchidSub == 10) { Shot(TEXT("MazeRedcap")); R->OrchidSub = 11; }
                if (Since < 1.3f) return;
                Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 92; R->PhaseStart = Now; return;
            }
            // ---- (2026-11-25, docs/regions/FEYWILD-HARE.md) The great hares, in Highmeadow: 82 the herds stand (a still of one
            // grazing), 83 a run at one makes it shy, 84 a walk up to one and the talk key puts him on it, 85 the bound and the
            // leap, 86 pollen halves it, 87 it refuses a threshold's mouth, 88 on the way to the maze it throws him (no Sanity
            // lost) and bolts, 89 Roll slides him off and the talk key puts him back on, 90 a death puts every hare home.
            case 92:   // (the hares) the Crossroads' herd stands on its open ground
            {
                if (!R->bPlaced) { Map->LoadNow(FeyMap::Crossroads); Chuck->ResetAtLocation(FeyCheckpointWorld(FeyMap::Crossroads)); R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                AFeyLife* Cl = LifeOf(FeyMap::Crossroads);
                const int32 N = Cl ? Cl->GetHares().Num() : 0;
                if (N < 2 && Since < 12.f) return;
                if (R->OrchidSub == 0) { UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_CROSSROADS hares=%d"), N); Check(N >= 4, TEXT("hares_crossroads")); R->OrchidSub = 1; R->MinDist = Since; }
                if (N >= 1) { const FVector L = FeyLocal(Cl->GetHares()[0]->GetActorLocation()); Cam(L + FVector(260.f, 200.f, 90.f), L + FVector(0, 0, 30.f)); }
                if (R->OrchidSub == 1 && Since - R->MinDist > 1.5f) { Shot(TEXT("HareCrossroads")); R->OrchidSub = 2; }
                if (Since - R->MinDist < 1.8f) return;
                Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 82; R->PhaseStart = Now; return;
            }
            case 82: case 83: case 84: case 85: case 86: case 87: case 88: case 89: case 90: case 91: case 93:
            {
                if (Step == 82 && !R->bPlaced) { Map->LoadNow(Highmeadow); Chuck->ResetAtLocation(FeyCheckpointWorld(Highmeadow)); AFeyHare::bTestStick = false; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                AFeyLife* Hl = LifeOf(Highmeadow);
                const TArray<AFeyHare*> Hares = Hl ? Hl->GetHares() : TArray<AFeyHare*>();
                if (Hares.Num() < 4 && Step == 82 && Since < 12.f) return;
                if (Hares.Num() < 2) { Check(false, TEXT("hares_spawned")); R->CrackStep = 99; return; }
                auto HL = [&](int32 I) { return FeyLocal(Hares[I]->GetActorLocation()); };
                const FVector C = FeyLocal(Chuck->GetActorLocation());
                auto Behind = [&](AFeyHare* H, float Back, float Up) { const FVector L = FeyLocal(H->GetActorLocation()), F = H->GetActorForwardVector(), Rt = H->GetActorRightVector(); Cam(L - F * Back + Rt * Back * .7f + FVector(0, 0, Up), L + FVector(0, 0, 20.f)); };
                switch (Step)
                {
                case 82:
                    if (R->OrchidSub == 0)
                    {
                        Check(Hares.Num() >= 4, TEXT("hares_spawned"));
                        // (2026-11-26) a herd just inside each way in from the Crossroads and the Deepwood
                        for (int32 Th = 0; Th < Thresholds; ++Th)
                        {
                            const FThreshold& Wy = Threshold(Th);
                            if (Wy.B != Highmeadow || (Wy.A != FeyMap::Crossroads && Wy.A != FeyMap::Deepwood)) continue;
                            float Near = 1e9f; for (int32 I = 0; I < Hares.Num(); ++I) Near = FMath::Min(Near, static_cast<float>(FVector2D::Distance(FVector2D(FeyLocal(Hares[I]->GetHome())), Wy.PB)));
                            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_ENTRANCE from=%s nearest=%.0f hares=%d"), FeyMap::District(Wy.A).Name, Near, Hares.Num());
                            Check(Near < 4500.f, TEXT("hare_near_entrance"));
                        }
                        R->OrchidSub = 1; R->MinDist = Since;
                    }
                    Behind(Hares[0], 260.f, 40.f);
                    if (Since - R->MinDist > 2.5f && R->OrchidSub == 1) { Shot(TEXT("HareGraze")); R->OrchidSub = 2; }
                    if (Since - R->MinDist < 3.f) return;
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = FPlatformMisc::GetEnvironmentVariable(TEXT("CHUCK_FEY_HARE_POSES")).IsEmpty() ? 83 : 91; R->PhaseStart = Now; return;
                case 91:   // (developer use: CHUCK_FEY_HARE_POSES=1) hare 0 held in each pose, seen from its side
                {
                    struct FP { AFeyHare::EState S; float V, Ph; bool bAir; const TCHAR* N; };
                    static const FP Poses[] = {{AFeyHare::EState::Graze, 0, 0, false, TEXT("Graze")}, {AFeyHare::EState::Alert, 0, 0, false, TEXT("Alert")}, {AFeyHare::EState::Hop, 150, 0, false, TEXT("Hop0")}, {AFeyHare::EState::Hop, 150, .5f, false, TEXT("Hop50")},
                                               {AFeyHare::EState::Ridden, 550, 0, false, TEXT("Bound00")}, {AFeyHare::EState::Ridden, 550, .2f, false, TEXT("Bound20")}, {AFeyHare::EState::Ridden, 550, .4f, false, TEXT("Bound40")}, {AFeyHare::EState::Ridden, 550, .55f, false, TEXT("Bound55")},
                                               {AFeyHare::EState::Ridden, 550, .7f, false, TEXT("Bound70")}, {AFeyHare::EState::Ridden, 550, .85f, false, TEXT("Bound85")}, {AFeyHare::EState::Ridden, 550, .5f, true, TEXT("Air")}, {AFeyHare::EState::Rear, 0, 0, false, TEXT("Rear")}};
                    const int32 K = R->OrchidSub;
                    if (K >= UE_ARRAY_COUNT(Poses)) { AFeyHare::bPoseHold = false; Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 83; R->PhaseStart = Now; return; }
                    AFeyHare::bPoseHold = true; AFeyHare::PoseState = Poses[K].S; AFeyHare::PoseSpeed = Poses[K].V; AFeyHare::PosePhase = Poses[K].Ph; AFeyHare::bPoseAir = Poses[K].bAir;
                    AFeyHare* H = Hares[0]; const FVector L = HL(0);
                    Cam(L + H->GetActorRightVector() * 330.f + FVector(0, 0, 25.f), L + FVector(0, 0, 15.f));
                    if (Since > 1.6f) { Shot(*FString::Printf(TEXT("HarePose_%s"), Poses[K].N)); ++R->OrchidSub; R->PhaseStart = Now; }
                    return;
                }
                case 83:   // (2026-11-26, the user: no running away) run straight at hare 0 from 5.5 m: it stays, and lets him ride
                    if (!R->bPlaced) { if (Since < 1.f || (Since < 8.f && Hares[0]->GetState() != AFeyHare::EState::Graze && Hares[0]->GetState() != AFeyHare::EState::Alert)) return; const FVector H = HL(0); FVector From = FVector(550.f, 0, 0);
                        // (a clear run in: the first of eight ways with nothing between him and it)
                        for (int32 K = 0; K < 8; ++K)
                        {
                            const FVector D = FRotator(0, 45.f * K, 0).Vector() * 550.f; FHitResult Hb; FCollisionQueryParams Qb(SCENE_QUERY_STAT(HareRunIn), false, Hares[0]); Qb.AddIgnoredActor(Chuck);
                            if (!Chuck->GetWorld()->SweepSingleByChannel(Hb, FeyWorld(H + D + FVector(0, 0, 60.f)), FeyWorld(H + D * .2f + FVector(0, 0, 60.f)), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 50.f), Qb)) { From = D; break; }
                        }
                        Put(H + From, -From); R->PadAt = H; R->Aimed0 = 0; R->Hits0 = AFeyHare::Shies; R->MinDist = 1e9f; R->bPlaced = true; R->PhaseStart = Now; return; }
                    R->MinDist = FMath::Min(R->MinDist, static_cast<float>(FVector::Dist2D(C, HL(0)))); if (Hares[0]->GetState() == AFeyHare::EState::Shy || Hares[0]->GetState() == AFeyHare::EState::Bolt) R->Aimed0 = 1;
                    if (Since < 4.5f && FVector::Dist2D(C, HL(0)) > 110.f && R->OrchidSub == 0) { Drive(HL(0) - C); return; }
                    if (R->OrchidSub == 0) { R->OrchidSub = 1; R->MinDist = FMath::Min(R->MinDist, static_cast<float>(FVector::Dist2D(C, HL(0)))); R->PhaseStart = Now; return; }   // (he has arrived: a still, then the checks)
                    if (Since < .5f) { Chuck->SetRunHeld(false); Chuck->SetTestStickWorld(FVector::ZeroVector); Behind(Hares[0], 300.f, 60.f); if (R->OrchidSub == 1 && Since > .3f) { Shot(TEXT("HareStays")); R->OrchidSub = 2; } return; }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_STAYS shies=%d ran=%d closest=%.0f state=%s moved=%.0f"), AFeyHare::Shies - R->Hits0, R->Aimed0, R->MinDist, Hares[0]->GetStateName(), FVector::Dist2D(HL(0), R->PadAt));
                    Check(AFeyHare::Shies == R->Hits0 && R->MinDist < 200.f && R->Aimed0 == 0, TEXT("hare_stays_when_run_at"));   // (it may hop about as it grazes; it must not run)
                    Uncam(); Chuck->SetRunHeld(false); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 84; R->PhaseStart = Now; return;
                case 84:   // a calm hare: walk up beside it, the prompt, the talk key, up its side and seated
                {
                    AFeyHare* H = Hares[1];
                    if (!R->bPlaced) { if (Since < 1.f || (Since < 8.f && H->GetState() != AFeyHare::EState::Graze && H->GetState() != AFeyHare::EState::Alert)) return; const FVector L = HL(1); Put(L - H->GetActorRightVector() * 95.f, H->GetActorForwardVector()); R->Hits0 = AFeyHare::Mounts; R->bPlaced = true; R->PhaseStart = Now; return; }
                    if (R->OrchidSub == 0 && Since > .6f)
                    {
                        const bool bPrompt = Chuck->GetMountPrompt() == H;
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_PROMPT prompt=%d hare_state=%s dist=%.0f"), bPrompt ? 1 : 0, H->GetStateName(), FVector::Dist2D(C, HL(1)));
                        Check(bPrompt, TEXT("hare_prompt"));
                        Chuck->Interact(); R->OrchidSub = 1; R->MinDist = Since;
                    }
                    if (R->OrchidSub >= 1) { const FVector L = HL(1); Cam(L + H->GetActorRightVector() * -230.f + H->GetActorForwardVector() * 60.f + FVector(0, 0, 60.f), L + FVector(0, 0, 50.f)); }
                    if (R->OrchidSub == 1 && Since - R->MinDist > .45f) { Shot(TEXT("HareMounting")); R->OrchidSub = 2; }
                    if (R->OrchidSub == 2 && Since - R->MinDist > 1.6f) { Shot(TEXT("HareMounted")); R->OrchidSub = 3; }
                    if (Since - R->MinDist < 2.f || R->OrchidSub < 3) return;
                    Check(AFeyHare::Mounts > R->Hits0 && Chuck->IsRiding() && !Chuck->IsRideMounting() && Chuck->GetRideMount() == H, TEXT("hare_mount"));
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = Chuck->IsRiding() ? 85 : 89; R->PhaseStart = Now; return;
                }
                case 85:   // the bound toward the other herd (open meadow), then the leap
                {
                    AFeyHare* H = Chuck->GetRideMount(); if (!H) { Check(false, TEXT("hare_bound")); R->CrackStep = 89; return; }
                    const FVector Way = (HL(2) - HL(1)).GetSafeNormal2D();
                    if (!R->bPlaced) { AFeyHare::TopSpeed = 0.f; R->Hits0 = AFeyHare::Leaps; R->SpeedOut = 0.f; R->SpeedOutN = 0; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                    if (Since > .5f) { R->SpeedOut += FApp::GetDeltaTime() * 1000.f; ++R->SpeedOutN; }   // (the frame time while he rides, for the record)
                    if (Since < 5.f && R->Frame % 20 == 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_RIDE t=%.1f speed=%.0f state=%s run=%d at=%s visible=%d cam=%.0f"), Since, H->GetVelocity().Size2D(), H->GetStateName(), Chuck->IsRunToggled() ? 1 : 0, *FeyLocal(H->GetActorLocation()).ToCompactString(), Chuck->GetMesh()->IsVisible() ? 1 : 0, FVector::Dist(Chuck->GetActorLocation(), Chuck->GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation()));
                    if (Since < 5.f) { Drive(Way); const FVector L = FeyLocal(H->GetActorLocation()); Cam(L - Way * 380.f + FVector(Way.Y, -Way.X, 0) * 120.f + FVector(0, 0, 260.f), L + FVector(0, 0, 50.f)); }
                    if (R->OrchidSub == 0 && Since > 2.6f) { Shot(TEXT("HareBound")); R->OrchidSub = 1; }
                    if (R->OrchidSub == 1 && Since > 3.4f) { Chuck->JumpPressed(); R->OrchidSub = 2; R->MinDist = Since; }
                    if (R->OrchidSub == 2 && Since - R->MinDist > .3f) { Shot(TEXT("HareLeap")); R->OrchidSub = 3; }
                    if (Since < 5.f) return;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_BOUND top=%.0f leaps=%d frame_ms=%.2f"), AFeyHare::TopSpeed, AFeyHare::Leaps - R->Hits0, R->SpeedOutN ? R->SpeedOut / R->SpeedOutN : 0.f);
                    Check(AFeyHare::TopSpeed > 480.f, TEXT("hare_bound"));
                    Check(AFeyHare::Leaps > R->Hits0, TEXT("hare_leap"));
                    Uncam(); Chuck->SetTestStickWorld(FVector::ZeroVector); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 86; R->PhaseStart = Now; return;
                }
                case 86:   // across a pollen bed: slowed to half
                {
                    AFeyHare* H = Chuck->GetRideMount(); if (!H || !Hl->GetPollen().Num()) { Check(false, TEXT("hare_pollen")); R->CrackStep = 87; return; }
                    const FFeyPollen& Pb = Hl->GetPollen()[0];
                    const FVector Along = FRotator(0, Pb.Yaw, 0).Vector();
                    const FVector Start = FVector(Pb.C, 0.f) - Along * (Pb.R.X + 700.f);
                    if (!R->bPlaced) { if (Since < .6f) { Chuck->SetTestStickWorld(FVector::ZeroVector); return; } float Z = 0; GroundAt(Start.X, Start.Y, Z); H->SetActorLocationAndRotation(FeyWorld(FVector(Start.X, Start.Y, Z + AFeyHare::HalfHeight + 4.f)), Along.Rotation(), false, nullptr, ETeleportType::TeleportPhysics); R->SpeedIn = R->SpeedOut = 0; R->SpeedInN = R->SpeedOutN = 0; R->bPlaced = true; R->PhaseStart = Now; return; }
                    Drive(FVector(Pb.C, 0.f) + Along * 2000.f - FeyLocal(H->GetActorLocation()));
                    const FVector Feet = H->GetActorLocation() - FVector(0, 0, AFeyHare::HalfHeight - 5.f);
                    const float V = static_cast<float>(H->GetVelocity().Size2D());
                    if (Since > 1.2f) { if (FeyGroundSpeedScale(Feet) < 1.f) { R->SpeedIn += V; ++R->SpeedInN; } else if (R->SpeedInN == 0) { R->SpeedOut += V; ++R->SpeedOutN; } }
                    if (Since < 4.5f) return;
                    const float In = R->SpeedInN ? R->SpeedIn / R->SpeedInN : 0.f, Out = R->SpeedOutN ? R->SpeedOut / R->SpeedOutN : 0.f;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_POLLEN out=%.0f in=%.0f frames_in=%d"), Out, In, R->SpeedInN);
                    Check(R->SpeedInN > 5 && In < Out * .7f, TEXT("hare_pollen"));
                    Chuck->SetTestStickWorld(FVector::ZeroVector); R->bPlaced = false; R->CrackStep = 87; R->PhaseStart = Now; return;
                }
                case 87:   // toward the mouth of the threshold from the Crossroads: it will not go
                {
                    AFeyHare* H = Chuck->GetRideMount(); if (!H) { Check(false, TEXT("hare_refuses")); R->CrackStep = 89; return; }
                    FVector2D Mouth = FVector2D::ZeroVector; for (int32 Th = 0; Th < Thresholds; ++Th) if (Threshold(Th).A == FeyMap::Crossroads && Threshold(Th).B == Highmeadow) Mouth = Threshold(Th).PB;
                    const FVector Start = FVector(Mouth + FVector2D(-1500.f, 300.f), 0.f);
                    if (!R->bPlaced) { if (Since < .6f) { Chuck->SetTestStickWorld(FVector::ZeroVector); return; } float Z = 0; GroundAt(Start.X, Start.Y, Z); H->SetActorLocationAndRotation(FeyWorld(FVector(Start.X, Start.Y, Z + AFeyHare::HalfHeight + 4.f)), (FVector(Mouth, 0.f) - Start).Rotation(), false, nullptr, ETeleportType::TeleportPhysics); R->Hits0 = AFeyHare::Refusals; R->Hits1 = AFeyHare::Throws; R->MinDist = 1e9f; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                    const FVector L = FeyLocal(H->GetActorLocation());
                    R->MinDist = FMath::Min(R->MinDist, FVector2D::Distance(FVector2D(L), Mouth));
                    if (Since < 5.f) { Drive(FVector(Mouth, 0.f) - L); Cam(L + FVector(-300.f, -380.f, 160.f), L + FVector(0, 0, 40.f)); if (R->OrchidSub == 0 && H->GetState() == AFeyHare::EState::Refuse) { Shot(TEXT("HareRefuse")); R->OrchidSub = 1; } return; }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_REFUSE refusals=%d closest=%.0f riding=%d"), AFeyHare::Refusals - R->Hits0, R->MinDist, Chuck->IsRiding() ? 1 : 0);
                    Check(AFeyHare::Refusals > R->Hits0 && R->MinDist > 450.f && Chuck->IsRiding() && AFeyHare::Throws == R->Hits1, TEXT("hare_refuses"));
                    Uncam(); Chuck->SetTestStickWorld(FVector::ZeroVector); R->bPlaced = false; R->CrackStep = 93; R->PhaseStart = Now; return;
                }
                case 93:   // (2026-11-26) at a run toward the overlook's brink: it refuses (thump, turn away), never over, never a throw
                {
                    AFeyHare* H = Chuck->GetRideMount(); if (!H) { Check(false, TEXT("hare_refuses_brink")); R->CrackStep = 89; return; }
                    const FVector2D Ov = OverlookAt();
                    if (!R->bPlaced)
                    {
                        if (Since < .6f) { Chuck->SetTestStickWorld(FVector::ZeroVector); return; }
                        const FVector2D Start = Ov + FVector2D(3200.f, -600.f);
                        float Z = 0; GroundAt(Start.X, Start.Y, Z);
                        H->SetActorLocationAndRotation(FeyWorld(FVector(Start.X, Start.Y, Z + AFeyHare::HalfHeight + 4.f)), FRotator(0, 180.f, 0), false, nullptr, ETeleportType::TeleportPhysics);
                        R->Hits0 = AFeyHare::Refusals; R->Hits1 = AFeyHare::Throws; R->MinDist = 1e9f; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    const FVector L = FeyLocal(H->GetActorLocation());
                    R->MinDist = FMath::Min(R->MinDist, static_cast<float>(L.X - FeyMap::District(Highmeadow).X0));
                    if (Since < 6.f) { Chuck->SetRunHeld(true); Drive(FVector(-1.f, 0.f, 0.f)); Cam(L + FVector(250.f, -420.f, 170.f), L + FVector(0, 0, 50.f)); if (R->OrchidSub == 0 && H->GetState() == AFeyHare::EState::Refuse) { Shot(TEXT("HareBrink")); R->OrchidSub = 1; } return; }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_BRINK refusals=%d throws=%d from_edge=%.0f riding=%d"), AFeyHare::Refusals - R->Hits0, AFeyHare::Throws - R->Hits1, R->MinDist, Chuck->IsRiding() ? 1 : 0);
                    Check(AFeyHare::Refusals > R->Hits0 && AFeyHare::Throws == R->Hits1 && Chuck->IsRiding() && R->MinDist > 600.f, TEXT("hare_refuses_brink"));
                    Uncam(); Chuck->SetTestStickWorld(FVector::ZeroVector); Chuck->SetRunHeld(false); R->bPlaced = false; R->CrackStep = 88; R->PhaseStart = Now; return;
                }
                case 88:   // put on the way down to the maze itself (its refusals keep it from there in play): it rears and throws him; no Sanity lost; it bolts home
                {
                    AFeyHare* H = Chuck->GetRideMount();
                    int32 Way = -1; for (int32 Th = 0; Th < Thresholds; ++Th) if (Threshold(Th).A == Highmeadow && Threshold(Th).B == FeyMap::Maze) Way = Th;
                    if (!R->bPlaced)
                    {
                        if (!H || Way < 0) { Check(false, TEXT("hare_throws_leaving")); R->CrackStep = 89; return; }
                        if (Since < .6f) { Chuck->SetTestStickWorld(FVector::ZeroVector); return; }
                        const FVector Start = ThresholdAtT(Way, .12f), Next = ThresholdAtT(Way, .2f);
                        H->SetActorLocationAndRotation(FeyWorld(Start + FVector(0, 0, AFeyHare::HalfHeight + 4.f)), (Next - Start).GetSafeNormal2D().Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
                        R->Hits0 = AFeyHare::Throws; R->Hits1 = Chuck->GetCrashLands(); R->Aimed0 = Chuck->GetSanity(); R->PadAt = Start; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    const FVector Ov = Way >= 0 ? ThresholdAtT(Way, .5f) : FVector::ZeroVector;
                    AFeyHare* Thrower = nullptr; for (AFeyHare* Q : Hares) if (Q->GetState() == AFeyHare::EState::Rear || Q->GetState() == AFeyHare::EState::Bolt) Thrower = Q;
                    if (H) { const FVector L = FeyLocal(H->GetActorLocation()); Drive(Ov - L); Cam(L + FVector(250.f, -420.f, 170.f), L + FVector(0, 0, 50.f)); if (R->OrchidSub == 0 && H->GetState() == AFeyHare::EState::Rear) { Shot(TEXT("HareRear")); R->OrchidSub = 1; } }
                    else if (R->OrchidSub < 2) { const FVector L = FeyLocal(Chuck->GetActorLocation()); Cam(L + FVector(250.f, -420.f, 170.f), L); if (Chuck->GetCrashLands() > R->Hits1) { Shot(TEXT("HareThrown")); R->OrchidSub = 2; } }
                    if (Since < 9.f && (Chuck->IsRiding() || Chuck->IsThrown())) return;
                    if (Since < 9.f && R->OrchidSub < 2) return;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_THROW throws=%d why=%s crash=%d sanity=%d->%d thrower=%s"), AFeyHare::Throws - R->Hits0, *AFeyHare::LastThrow, Chuck->GetCrashLands() - R->Hits1, R->Aimed0, Chuck->GetSanity(), Thrower ? Thrower->GetStateName() : TEXT("none"));
                    Check(AFeyHare::Throws > R->Hits0 && AFeyHare::LastThrow == TEXT("leaving"), TEXT("hare_throws_leaving"));
                    Check(Chuck->GetCrashLands() > R->Hits1, TEXT("hare_throw_crashland"));
                    Check(Chuck->GetSanity() == R->Aimed0, TEXT("hare_throw_no_sanity"));
                    Check(Thrower && Thrower->GetState() == AFeyHare::EState::Bolt, TEXT("hare_bolts"));
                    Uncam(); Chuck->SetTestStickWorld(FVector::ZeroVector); Chuck->SetRunHeld(false); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 89; R->PhaseStart = Now; return;
                }
                case 89:   // Roll slides him off; the hare stays calm; the talk key puts him back on
                {
                    AFeyHare* H = Hares[Hares.Num() - 1];
                    if (!R->bPlaced) { if (Since < 2.5f) return; const FVector L = HL(Hares.Num() - 1); Put(L - H->GetActorRightVector() * 95.f, H->GetActorForwardVector()); R->Hits0 = AFeyHare::Dismounts; R->Hits1 = AFeyHare::Mounts; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                    if (R->OrchidSub == 0 && Since > .6f) { Chuck->Interact(); R->OrchidSub = 1; R->MinDist = Since; }
                    if (R->OrchidSub == 1 && Since - R->MinDist > 1.6f) { Chuck->DodgeToward(FVector2D::ZeroVector); R->OrchidSub = 2; R->MinDist = Since; }
                    if (R->OrchidSub == 2 && Since - R->MinDist > 1.2f)
                    {
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_OFF dismounts=%d riding=%d gait=%s hare=%s"), AFeyHare::Dismounts - R->Hits0, Chuck->IsRiding() ? 1 : 0, Chuck->GetGaitName(), H->GetStateName());
                        Check(AFeyHare::Dismounts > R->Hits0 && !Chuck->IsRiding() && Chuck->GetCharacterMovement()->IsMovingOnGround(), TEXT("hare_slide_off"));
                        Check(H->GetState() == AFeyHare::EState::Alert || H->GetState() == AFeyHare::EState::Graze, TEXT("hare_stays_calm"));
                        FaceAt(HL(Hares.Num() - 1));
                        Chuck->Interact(); R->OrchidSub = 3; R->MinDist = Since;
                    }
                    if (R->OrchidSub == 3 && Since - R->MinDist > 1.4f)
                    {
                        Check(AFeyHare::Mounts >= R->Hits1 + 2 && Chuck->IsRiding(), TEXT("hare_remount"));
                        R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 90; R->PhaseStart = Now;
                    }
                    return;
                }
                case 90:   // a death (his respawn): every hare home, unridden
                {
                    if (!R->bPlaced) { Chuck->SetSanity(1); Chuck->TakeBite(Chuck->GetActorLocation() + FVector(50.f, 0, 0), 1, false, true); R->bPlaced = true; R->PhaseStart = Now; return; }
                    if (Since < 6.f) return;
                    int32 Home = 0; for (AFeyHare* Q : Hares) Home += FVector::Dist2D(Q->GetActorLocation(), Q->GetHome()) < 120.f && !Q->GetRider();
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_TEST_RESET home=%d of %d riding=%d respawns=%d"), Home, Hares.Num(), Chuck->IsRiding() ? 1 : 0, Chuck->GetRespawns());
                    Check(!Chuck->IsRiding() && Home == Hares.Num(), TEXT("hare_reset"));
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARES_TEST mounts=%d dismounts=%d throws=%d refusals=%d thumps=%d shies=%d leaps=%d top=%.0f"), AFeyHare::Mounts, AFeyHare::Dismounts, AFeyHare::Throws, AFeyHare::Refusals, AFeyHare::Thumps, AFeyHare::Shies, AFeyHare::Leaps, AFeyHare::TopSpeed);
                    R->bPlaced = false; R->CrackStep = 101; R->PhaseStart = Now; return;
                }
                }
                return;
            }
            // ---- (2026-11-26) the Deepwood's jellyfish (FeyJelly.h): 101 they drift at their homes, 102 he runs under one and
            // takes hold, 103 full push (5 m/s, his feet clear of the ground over roots), the swing at a stop, 104 straight at
            // a trunk it goes round, 105 at a way out it drifts back, 106 put on the way out it shakes him off, 107 a bite
            // and he holds on, Roll lets go, the talk key takes hold again, 108 a death sends every one home.
            case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108:
            {
                if (Step == 101 && !R->bPlaced) { Map->LoadNow(Deepwood); Chuck->ResetAtLocation(FeyCheckpointWorld(Deepwood)); AFeyJelly::bTestStick = false; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                AFeyLife* Dl = LifeOf(Deepwood);
                const TArray<AFeyJelly*> Js = Dl ? Dl->GetJellies() : TArray<AFeyJelly*>();
                if (Js.Num() < 4 && Step == 101 && Since < 14.f) return;
                if (Js.Num() < 2) { Check(false, TEXT("jellies_spawned")); R->CrackStep = 99; return; }
                auto JL = [&](int32 I) { return FeyLocal(Js[I]->GetActorLocation()); };
                const FVector C = FeyLocal(Chuck->GetActorLocation());
                auto Side = [&](AFeyJelly* J, float Back, float Up) { const FVector L = FeyLocal(J->GetActorLocation()); const FVector F = FRotator(0, J->GetFacingYaw(), 0).Vector(), Rt(-F.Y, F.X, 0); Cam(L + Rt * Back - F * Back * .35f + FVector(0, 0, Up), L - FVector(0, 0, 90.f)); };
                AFeyJelly* Held = Chuck->GetJellyMount();
                switch (Step)
                {
                case 101:
                {
                    if (R->OrchidSub == 0)
                    {
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_SPAWN jellies=%d"), Js.Num());
                        Check(Js.Num() >= 4, TEXT("jellies_spawned"));
                        // (one near each way in)
                        for (int32 Th = 0; Th < Thresholds; ++Th)
                        {
                            const FThreshold& Wy = Threshold(Th);
                            if (Wy.A != Deepwood && Wy.B != Deepwood) continue;
                            const FVector2D Mouth = Wy.A == Deepwood ? Wy.PA : Wy.PB;
                            float Near = 1e9f; for (AFeyJelly* J : Js) Near = FMath::Min(Near, static_cast<float>(FVector2D::Distance(FVector2D(FeyLocal(J->GetHome())), Mouth)));
                            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_ENTRANCE mouth=%.0f,%.0f nearest=%.0f"), Mouth.X, Mouth.Y, Near);
                            Check(Near < 4000.f, TEXT("jelly_near_entrance"));
                        }
                        R->OrchidSub = 1; R->MinDist = Since;
                    }
                    Side(Js[0], 420.f, 60.f);
                    if (R->OrchidSub == 1 && Since - R->MinDist > 2.5f) { Shot(TEXT("JellyDrift")); R->OrchidSub = 2; }
                    if (Since - R->MinDist < 3.f) return;
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 102; R->PhaseStart = Now; return;
                }
                case 102:   // he runs in under jelly 0 from 4 m and takes hold
                {
                    AFeyJelly* J = Js[0];
                    if (!R->bPlaced)
                    {
                        if (Since < .5f) return;
                        const FVector G = FeyLocal(J->GetGrip()); FVector From = FVector(400.f, 0, 0);
                        for (int32 K = 0; K < 8; ++K)
                        {
                            const FVector D = FRotator(0, 45.f * K, 0).Vector() * 400.f; FHitResult Hb; FCollisionQueryParams Qb(SCENE_QUERY_STAT(JellyRunIn), false, J); Qb.AddIgnoredActor(Chuck);
                            if (!Chuck->GetWorld()->SweepSingleByChannel(Hb, FeyWorld(FVector(G.X, G.Y, C.Z) + D + FVector(0, 0, 30.f)), FeyWorld(FVector(G.X, G.Y, C.Z) + D * .15f + FVector(0, 0, 30.f)), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 50.f), Qb)) { From = D; break; }
                        }
                        Put(FVector(G.X, G.Y, 0.f) + From, -From); R->Hits0 = AFeyJelly::Holds; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    if (R->OrchidSub == 0)
                    {
                        if (Chuck->GetJellyPrompt() == J || Since > 6.f)
                        {
                            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_PROMPT prompt=%d dist=%.0f"), Chuck->GetJellyPrompt() == J ? 1 : 0, FVector::Dist2D(FeyLocal(J->GetGrip()), C));
                            Check(Chuck->GetJellyPrompt() == J, TEXT("jelly_prompt"));
                            Chuck->SetRunHeld(false); Chuck->SetTestStickWorld(FVector::ZeroVector);
                            Chuck->Interact(); R->OrchidSub = 1; R->MinDist = Since;
                        }
                        else Drive(FeyLocal(J->GetGrip()) - C);
                        return;
                    }
                    Side(J, 300.f, 20.f);
                    if (R->OrchidSub == 1 && Since - R->MinDist > .3f) { Shot(TEXT("JellyTakeHold")); R->OrchidSub = 2; }
                    if (R->OrchidSub == 2 && Since - R->MinDist > 2.2f) { Shot(TEXT("JellyHeld")); R->OrchidSub = 3; }
                    if (Since - R->MinDist < 2.5f) return;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_HOLD holds=%d hanging=%d state=%s feet=%.0f"), AFeyJelly::Holds - R->Hits0, Chuck->IsJellyHanging() ? 1 : 0, J->GetStateName(), J->GetFeetClearance());
                    Check(AFeyJelly::Holds > R->Hits0 && Chuck->GetJellyMount() == J && J->GetState() == AFeyJelly::EState::Held, TEXT("jelly_hold"));
                    Check(J->GetFeetClearance() > 80.f && J->GetFeetClearance() < 170.f, TEXT("jelly_lifts_him"));
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = Chuck->IsJellyHanging() ? 103 : 107; R->PhaseStart = Now; return;
                }
                case 103:   // full push toward the middle of the Deepwood: 5 m/s at most, his feet clear of the ground; then let go of the stick
                {
                    if (!Held) { Check(false, TEXT("jelly_speed")); R->CrackStep = 107; return; }
                    if (!R->bPlaced) { R->Aimed0 = 0; AFeyJelly::TopSpeed = 0.f; R->SpeedIn = 1e9f; R->SpeedOut = 0.f; R->SpeedOutN = 0; R->MinDist = 0.f; R->Hits0 = 0; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return; }
                    const FVector L = FeyLocal(Held->GetActorLocation());
                    if (Since < 6.f)
                    {
                        const FVector Way = (FVector(FeyMap::District(Deepwood).Hub, 0.f) - L).GetSafeNormal2D();
                        AFeyJelly::bTestStick = true; AFeyJelly::TestStick = Way;
                        if (Since > .5f) { R->SpeedOut += FApp::GetDeltaTime() * 1000.f; ++R->SpeedOutN; R->SpeedIn = FMath::Min(R->SpeedIn, Held->GetFeetClearance()); R->MinDist = FMath::Max(R->MinDist, Held->GetFeetClearance()); }
                        if (Since > 2.f && Since < 3.2f) { Uncam(); if (R->Aimed0 == 0 && Since > 2.9f) { Shot(TEXT("JellyPlayerView")); R->Aimed0 = 1; } }
                        else Side(Held, 380.f, 40.f);
                        if (R->OrchidSub == 0 && Since > 4.f) { Shot(TEXT("JellySpeed")); R->OrchidSub = 1; }
                        if (R->Frame % 30 == 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_RIDE t=%.1f speed=%.0f feet=%.0f swing=%.0f at=%s"), Since, Held->GetDriftVelocity().Size2D(), Held->GetFeetClearance(), Held->GetSwingForwardDeg(), *L.ToCompactString());
                        return;
                    }
                    AFeyJelly::TestStick = FVector::ZeroVector;
                    Side(Held, 380.f, 40.f);
                    if (R->OrchidSub == 1 && Since > 6.45f) { Shot(TEXT("JellyStopSwing")); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_STOP swing=%.0f speed=%.0f"), Held->GetSwingForwardDeg(), Held->GetDriftVelocity().Size2D()); R->Hits0 = FMath::RoundToInt(Held->GetSwingForwardDeg()); R->OrchidSub = 2; }
                    if (Since < 8.f) return;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_SPEED top=%.0f feet_min=%.0f feet_max=%.0f stop_swing=%d frame_ms=%.2f blocks=%d dips=%d"), AFeyJelly::TopSpeed, R->SpeedIn, R->MinDist, R->Hits0, R->SpeedOutN ? R->SpeedOut / R->SpeedOutN : 0.f, AFeyJelly::Blocks, AFeyJelly::Dips);
                    Check(AFeyJelly::TopSpeed > 470.f && AFeyJelly::TopSpeed < 506.f, TEXT("jelly_speed"));
                    Check(R->SpeedIn >= 25.f && R->MinDist < 200.f, TEXT("jelly_feet_clear"));
                    Check(R->Hits0 > 4, TEXT("jelly_swing_on_stop"));
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 104; R->PhaseStart = Now; return;
                }
                case 104:   // straight at a trunk: it goes round
                {
                    if (!Held) { Check(false, TEXT("jelly_round_trunk")); R->CrackStep = 107; return; }
                    if (!R->bPlaced)
                    {
                        AFeyJelly::TestStick = FVector::ZeroVector;
                        if (Since < .5f) return;
                        // (the nearest solid thing at his height, within 15 m round)
                        const FVector At = Held->GetActorLocation() - FVector(0, 0, 60.f);
                        float Best = 1e9f; FVector Hit = FVector::ZeroVector, Dir = FVector::ZeroVector;
                        FCollisionQueryParams Qb(SCENE_QUERY_STAT(JellyTrunk), false, Held); Qb.AddIgnoredActor(Chuck);
                        for (int32 K = 0; K < 24; ++K)
                        {
                            const FVector D = FRotator(0, 15.f * K, 0).Vector(); FHitResult Hb;
                            if (Chuck->GetWorld()->LineTraceSingleByChannel(Hb, At, At + D * 1500.f, ECC_Pawn, Qb) && FMath::Abs(Hb.ImpactNormal.Z) < .5f && Hb.Distance < Best && Hb.Distance > 350.f) { Best = Hb.Distance; Hit = Hb.ImpactPoint; Dir = D; }
                        }
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_TRUNK found=%d dist=%.0f"), Best < 1e8f ? 1 : 0, Best);
                        if (Best > 1e8f) { R->CrackStep = 105; R->PhaseStart = Now; return; }
                        R->PadAt = FeyLocal(Hit); R->Aimed0 = 0; R->SpeedIn = 0; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now;
                        R->Hits1 = FMath::RoundToInt(Dir.Rotation().Yaw); return;
                    }
                    const FVector Dir = FRotator(0, static_cast<float>(R->Hits1), 0).Vector();
                    const FVector L = FeyLocal(Held->GetActorLocation());
                    // (the way past it: how far beyond the trunk's face it has got, along the push)
                    R->SpeedIn = FMath::Max(R->SpeedIn, static_cast<float>(FVector::DotProduct(L - R->PadAt, Dir)));
                    if (Since < 7.f) { AFeyJelly::bTestStick = true; AFeyJelly::TestStick = Dir; Cam(L - Dir * 380.f + FVector(-Dir.Y, Dir.X, 0) * 300.f + FVector(0, 0, 140.f), L - FVector(0, 0, 60.f)); if (R->OrchidSub == 0 && Since > 2.f) { Shot(TEXT("JellyTrunk")); R->OrchidSub = 1; } return; }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_ROUND beyond=%.0f"), R->SpeedIn);
                    Check(R->SpeedIn > 100.f, TEXT("jelly_round_trunk"));
                    AFeyJelly::TestStick = FVector::ZeroVector; Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 105; R->PhaseStart = Now; return;
                }
                case 105:   // at a way out (toward the Crossroads): it drifts back, he stays on
                {
                    if (!Held) { Check(false, TEXT("jelly_refuses")); R->CrackStep = 107; return; }
                    FVector2D Mouth = FVector2D::ZeroVector; for (int32 Th = 0; Th < Thresholds; ++Th) if (Threshold(Th).A == FeyMap::Crossroads && Threshold(Th).B == Deepwood) Mouth = Threshold(Th).PB;
                    if (!R->bPlaced)
                    {
                        AFeyJelly::TestStick = FVector::ZeroVector;
                        if (Since < .5f) return;
                        const FVector2D Start = Mouth + FVector2D(300.f, -1600.f);
                        float Z = 0; GroundAt(Start.X, Start.Y, Z);
                        Held->SetActorLocation(FeyWorld(FVector(Start.X, Start.Y, Z + AFeyJelly::RideGripUp + AFeyJelly::ArmDrop)), false, nullptr, ETeleportType::TeleportPhysics);
                        R->Hits0 = AFeyJelly::Refusals; R->Hits1 = AFeyJelly::Shakes; R->MinDist = 1e9f; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    const FVector L = FeyLocal(Held->GetActorLocation());
                    R->MinDist = FMath::Min(R->MinDist, static_cast<float>(FVector2D::Distance(FVector2D(L), Mouth)));
                    if (Since < 6.f) { AFeyJelly::bTestStick = true; AFeyJelly::TestStick = (FVector(Mouth, 0.f) - L).GetSafeNormal2D(); Cam(L + FVector(-450.f, -300.f, 160.f), L - FVector(0, 0, 60.f)); return; }
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_REFUSE refusals=%d shakes=%d closest=%.0f hanging=%d"), AFeyJelly::Refusals - R->Hits0, AFeyJelly::Shakes - R->Hits1, R->MinDist, Chuck->IsJellyHanging() ? 1 : 0);
                    Check(AFeyJelly::Refusals > R->Hits0 && AFeyJelly::Shakes == R->Hits1 && Chuck->IsJellyHanging() && R->MinDist > 450.f, TEXT("jelly_refuses"));
                    AFeyJelly::TestStick = FVector::ZeroVector; Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 106; R->PhaseStart = Now; return;
                }
                case 106:   // put on the way out itself: it shakes him off; no Sanity lost; he lands on his feet
                {
                    int32 Way = -1; for (int32 Th = 0; Th < Thresholds; ++Th) if (Threshold(Th).A == FeyMap::Crossroads && Threshold(Th).B == Deepwood) Way = Th;
                    if (!R->bPlaced)
                    {
                        if (!Held || Way < 0) { Check(false, TEXT("jelly_shakes_leaving")); R->CrackStep = 107; return; }
                        AFeyJelly::TestStick = FVector::ZeroVector;
                        if (Since < .5f) return;
                        const FVector P = ThresholdAtT(Way, .85f);
                        Held->SetActorLocation(FeyWorld(P + FVector(0, 0, AFeyJelly::RideGripUp + AFeyJelly::ArmDrop)), false, nullptr, ETeleportType::TeleportPhysics);
                        R->Hits0 = AFeyJelly::Shakes; R->Aimed0 = Chuck->GetSanity(); R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    Cam(C + FVector(-350.f, -300.f, 150.f), C);
                    if (R->OrchidSub == 0 && Since > .35f) { Shot(TEXT("JellyShaken")); R->OrchidSub = 1; }
                    if (Since < 3.f) return;
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_SHAKE shakes=%d hanging=%d ground=%d sanity=%d->%d"), AFeyJelly::Shakes - R->Hits0, Chuck->IsJellyHanging() ? 1 : 0, Chuck->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0, R->Aimed0, Chuck->GetSanity());
                    Check(AFeyJelly::Shakes > R->Hits0 && !Chuck->IsJellyHanging() && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetSanity() == R->Aimed0, TEXT("jelly_shakes_leaving"));
                    Uncam(); R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 107; R->PhaseStart = Now; return;
                }
                case 107:   // under another: hold on; a bite, he holds on; Roll lets go; the talk key takes hold again
                {
                    AFeyJelly* J = Js[Js.Num() > 1 ? 1 : 0];
                    if (!R->bPlaced)
                    {
                        AFeyJelly::bTestStick = false;
                        if (Since < 1.f || J->GetRider()) return;
                        const FVector G = FeyLocal(J->GetGrip());
                        Put(FVector(G.X + 20.f, G.Y, 0.f), FVector(-1, 0, 0)); R->Hits0 = AFeyJelly::Releases; R->Hits1 = AFeyJelly::Holds; R->bPlaced = true; R->OrchidSub = 0; R->PhaseStart = Now; return;
                    }
                    if (R->OrchidSub == 0 && Since > .6f) { Chuck->Interact(); R->OrchidSub = 1; R->MinDist = Since; R->Aimed0 = Chuck->GetSanity(); }
                    if (R->OrchidSub == 1 && Since - R->MinDist > 2.f)
                    {
                        Chuck->TakeBite(Chuck->GetActorLocation() + FVector(60.f, 0, 0), 1);
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_BITE hanging=%d sanity=%d->%d"), Chuck->IsJellyHanging() ? 1 : 0, R->Aimed0, Chuck->GetSanity());
                        Check(Chuck->IsJellyHanging() && Chuck->GetSanity() == R->Aimed0 - 1, TEXT("jelly_holds_through_bite"));
                        R->OrchidSub = 2; R->MinDist = Since;
                    }
                    if (R->OrchidSub == 2 && Since - R->MinDist > .8f) { Chuck->DodgeToward(FVector2D::ZeroVector); R->OrchidSub = 3; R->MinDist = Since; }
                    if (R->OrchidSub == 3 && Since - R->MinDist > 1.6f)
                    {
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_OFF releases=%d hanging=%d ground=%d"), AFeyJelly::Releases - R->Hits0, Chuck->IsJellyHanging() ? 1 : 0, Chuck->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0);
                        Check(AFeyJelly::Releases > R->Hits0 && !Chuck->IsJellyHanging() && Chuck->GetCharacterMovement()->IsMovingOnGround(), TEXT("jelly_let_go"));
                        R->OrchidSub = 4; R->MinDist = Since;
                    }
                    if (R->OrchidSub == 4 && Since - R->MinDist > 1.2f)
                    {
                        if (Chuck->GetJellyPrompt() != J) { if (Since - R->MinDist < 5.f) { Drive(FeyLocal(J->GetGrip()) - FeyLocal(Chuck->GetActorLocation())); return; } }
                        Chuck->SetRunHeld(false); Chuck->SetTestStickWorld(FVector::ZeroVector);
                        Chuck->Interact(); R->OrchidSub = 5; R->MinDist = Since;
                    }
                    if (R->OrchidSub == 5 && Since - R->MinDist > 1.f)
                    {
                        Check(AFeyJelly::Holds >= R->Hits1 + 2 && Chuck->IsJellyHanging(), TEXT("jelly_hold_again"));
                        R->bPlaced = false; R->OrchidSub = 0; R->CrackStep = 108; R->PhaseStart = Now;
                    }
                    return;
                }
                case 108:   // a death: every jellyfish home, him on his feet
                {
                    if (!R->bPlaced) { Chuck->SetSanity(1); Chuck->TakeBite(Chuck->GetActorLocation() + FVector(50.f, 0, 0), 1, false, true); R->bPlaced = true; R->PhaseStart = Now; return; }
                    if (Since < 6.f) return;
                    int32 Home = 0; for (AFeyJelly* J : Js) Home += FVector::Dist2D(J->GetActorLocation(), J->GetHome()) < 150.f && !J->GetRider();
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_TEST_RESET home=%d of %d hanging=%d"), Home, Js.Num(), Chuck->IsJellyHanging() ? 1 : 0);
                    Check(!Chuck->IsJellyHanging() && Home == Js.Num(), TEXT("jelly_reset"));
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLIES_TEST holds=%d releases=%d shakes=%d refusals=%d blocks=%d dips=%d top=%.0f"), AFeyJelly::Holds, AFeyJelly::Releases, AFeyJelly::Shakes, AFeyJelly::Refusals, AFeyJelly::Blocks, AFeyJelly::Dips, AFeyJelly::TopSpeed);
                    AFeyJelly::bTestStick = false; R->bPlaced = false; R->CrackStep = 99; R->PhaseStart = Now; return;
                }
                }
                return;
            }
            default:
                if (Since < 1.f) return;
                Check(AFeyLife::GroundSamples > 0 && AFeyLife::GroundBad == 0, TEXT("creature_ground"));   // (2026-11-25: the 11-24 beasts' rounds, the roaming redcaps' leashes)
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_TEST failures=%d toggles=%d orchid_shots=%d orchid_hits=%d moth_touches=%d mite_bites=%d mite_kills=%d beast_notices=%d lashes=%d lash_hits=%d paces=%d"), R->LifeFails, AFeyLife::FlowerToggles, AFeyLife::OrchidShots, AFeyLife::OrchidHits, AFeyLife::MothTouches,
                    AFeyMite::Bites, AFeyMite::Kills, AFeyDisplacer::Notices, AFeyDisplacer::Lashes, AFeyDisplacer::LashHits, AFeyDisplacer::Paces);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHTS_TEST rings=%d wisp_moves=%d koi_scatters=%d bursts=%d sign_turns=%d crow_flights=%d"), AFeyDelights::RingVisits, AFeyDelights::WispMoves, AFeyDelights::KoiScatters, AFeyDelights::Bursts, AFeyDelights::SignTurns, AFeyDelights::CrowFlights);
                Done(R->LifeFails); return;
            }
        }
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
                const int32 N = SeeThrough(W, Chuck, Th, St == 2 ? .6f : -.44f, St == 1 ? .4f : 1.44f, Leaks);
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
                    FVector2D At = Sh.At, Look = Sh.Look;
                    if (At.X < -1.9e5f && At.Y > -1.f)   // along the first canyon
                    {
                        const FVector P = ThresholdAtT(0, At.Y), Q = ThresholdAtT(0, At.Y + .03f * Look.X);
                        At = FVector2D(P); Look = FVector2D(P) + (FVector2D(Q) - FVector2D(P)).GetSafeNormal() * 1000.f;
                    }
                    else if (At.X < -1.9e5f)   // 4 m out from a root tunnel's mouth (Look.X its number, Look.Y 0 at its A end, 1 at B), facing it
                    {
                        const int32 Tt = int32(Look.X); const float Te = Look.Y < .5f ? -.12f : 1.12f;
                        const FVector P = ThresholdAtT(Tt, Te), M = ThresholdAtT(Tt, Look.Y < .5f ? 0.f : 1.f);
                        At = FVector2D(P + (P - M).GetSafeNormal2D() * 200.f); Look = FVector2D(M);
                    }
                    float Z = 0; GroundAt(At.X, At.Y, Z); if (IsPad(At.X, At.Y)) Z = 2.f;
                    Chuck->ResetAtLocation(FeyWorld(FVector(At.X, At.Y, Z + Stand + 4.f)));
                    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Look.Y - At.Y, Look.X - At.X));
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
