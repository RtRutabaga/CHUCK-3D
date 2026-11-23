// -ChuckChultTest -ChuckFeyMapTest (Claude, 2026-11-19; FeyMap.h, docs/regions/FEYWILD.md stage 3): the real character
// walked along every route of the rough Feywild map, from the river's shore through all ten thresholds to the maze's heart,
// steering along the paths. It measures frame time and memory in each district against Chult's landing, how long each
// district takes to load, how many stand at once, and whether he is ever stuck or late at a threshold; then it starts at
// each district's checkpoint and takes a still there. -ChuckFeyMapTest=short walks only the shore to the orchard.
#include "FeyMap.h"
#include "FeyLife.h"
#include "FeyCreatures.h"
#include "TempleSkeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
        int32 LifeFails = 0, Cigs0 = 0, Hits0 = 0, Hits1 = 0; float MinDist = 1e9f; float SpeedOut = 0, SpeedIn = 0; int32 SpeedOutN = 0, SpeedInN = 0;
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
        // ---- 2 (=life, 2026-11-21): stage 5's first mechanics and hazards, each tried.
        if (R->Phase == 2 && R->bLife)
        {
            auto LifeOf = [&](int32 D) -> AFeyLife* { for (const TWeakObjectPtr<AFeyLife>& P : AFeyLife::All()) if (P.IsValid() && P->GetDistrict() == D) return P.Get(); return nullptr; };
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
            case 12:   // an orchid's seed, standing in its lane
            {
                const FFeyOrchid& O = Life->GetOrchids()[0];
                if (!R->bPlaced) { Put(O.Pos + O.Dir * 450.f, -O.Dir); R->Hits0 = AFeyLife::OrchidHits; R->bPlaced = true; R->PhaseStart = Now; return; }
                if (Since < 6.f) return;
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
                const TArray<FVector2D>& De = MazeDeadEnds(); const FVector2D Sq = De.IsValidIndex(3) ? De[3] : FVector2D::ZeroVector;
                if (!R->bPlaced) { R->Hits0 = AFeyLife::StatueTurns; Put(FVector(Sq, 0.f) + FVector(220.f, 0.f, 0.f), FVector(1, 0, 0)); R->bPlaced = true; R->PhaseStart = Now; return; }
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
                const TArray<FMazeParkour>& Pks = MazeParkour(); int32 Pi = -1; for (int32 I = 0; I < Pks.Num() && Pi < 0; ++I) if (Pks[I].Kind == Want && (Step != 60 || Pks[I].bOffWay) && (Step != 63 || !Pks[I].bOffWay)) Pi = I;
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
                if (R->Frame % 15 == 0) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_GAP through=%.0f gait=%s"), Through, Chuck->GetGaitName());
                if (Chuck->IsCrawling() && !R->bShort) { Shot(TEXT("MazeRootGap")); R->bShort = true; }
                const FVector Side(-Th.Y, Th.X, 0.f);
                if (Since < 6.f && Through < 110.f) { Drive(Th + Side * FMath::Clamp(FVector::DotProduct(C - L, Side) / 30.f, -.5f, .5f)); return; }
                Check(Chuck->GetCrawls() > R->Hits1, TEXT("gap_crawls"));
                Check(Through > 80.f, TEXT("gap_through"));
                R->bPlaced = false; R->CrackStep = 63; R->PhaseStart = Now; return;
            }
            default:
                if (Since < 1.f) return;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_TEST failures=%d toggles=%d orchid_shots=%d orchid_hits=%d moth_touches=%d mite_bites=%d mite_kills=%d beast_notices=%d lashes=%d lash_hits=%d paces=%d"), R->LifeFails, AFeyLife::FlowerToggles, AFeyLife::OrchidShots, AFeyLife::OrchidHits, AFeyLife::MothTouches,
                    AFeyMite::Bites, AFeyMite::Kills, AFeyDisplacer::Notices, AFeyDisplacer::Lashes, AFeyDisplacer::LashHits, AFeyDisplacer::Paces);
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
