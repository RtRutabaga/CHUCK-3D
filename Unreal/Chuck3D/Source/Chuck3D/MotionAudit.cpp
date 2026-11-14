// -ChuckMotionAudit[=<checkpoint>] (Claude, 2026-11-13; docs/AUTOMATION-DEFECT-CATALOG.md, category 5).
// Starts at a dev checkpoint (Waterdeep when none is given), waits for the region to stand, then records the bones of
// every shown skeletal mesh within 60 m of Chuck, frame by frame at a fixed 30 fps, for fifteen seconds while Chuck
// stands, walks off, stops and slashes. The bones are read in each mesh's own space, so walking, the ship's roll and
// teleports don't count; only the pose itself moving does. Two kinds of fault a player has found by hand:
//
//  - Pops: one frame in which a bone's acceleration jumps to over four times anything near it and over 2.5 cm/frame^2,
//    and its travel that frame to over 2.5 times any frame near it (a cross-fade that snaps, a clip that starts on the
//    wrong frame, a bone reset; not a hammer stopping dead on the anvil). Bones of one mesh popping together are one.
//  - Jitter: five or more frames in a row in which a bone's acceleration turns round (over 120 degrees) at over
//    0.3 cm/frame^2: the tail squiggle, the motion-capture shake, the crew's phase bug.
//
// Before reading the scene the detector is run on a made-up track with one step and one shake in it, and must find
// exactly those (selftest=1); Chuck's own walk must register (chuck_moved=1), so a blind run cannot pass. Intended
// motion goes in Content/Data/Audit/Allow.txt as "motion <wildcard on '<mesh> <actor>/<component> <bone>'>".
// Writes Saved/Audit/<Region>-motion.csv. Reads the scene only (and walks Chuck); quits.
#include "CoreMinimal.h"
#include "ChuckCharacter.h"
#include "ChultPropAudit.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

bool ChuckAuditAllowed(const TCHAR* Check, const FString& Key);

namespace
{
    constexpr float Fps = 30.f;
    constexpr int32 Frames = 450;                          // fifteen seconds (a looping idle's seam shows once a loop)
    constexpr int32 WalkFrom = 90, StopAt = 135, SlashAt = 180;
    constexpr float PopMin = 2.5f, PopRatio = 4.f, SpeedRatio = 2.5f, JitMin = .3f;
    constexpr int32 JitRun = 5;

    struct FHit { int32 Frame; float Size; int32 Length; };

    // One bone's track: pops and jitter runs, by the rules above.
    void Detect(const TArray<FVector3f>& P, TArray<FHit>& Pops, TArray<FHit>& Jits)
    {
        const int32 N = P.Num();
        if (N < 12) return;
        TArray<FVector3f> A; A.SetNumZeroed(N);
        TArray<float> M; M.SetNumZeroed(N);
        TArray<float> S; S.SetNumZeroed(N);
        for (int32 T = 1; T < N; ++T) S[T] = (P[T] - P[T - 1]).Size();
        for (int32 T = 2; T < N; ++T) { A[T] = P[T] - 2 * P[T - 1] + P[T - 2]; M[T] = A[T].Size(); }
        for (int32 T = 2; T < N; ++T)
        {
            if (M[T] < PopMin || M[T] < M[T - 1]) continue;
            float Base = 0, SpeedBase = 0;
            for (int32 K = T - 7; K <= T + 9; ++K) if (K >= 2 && K < N && (K < T - 1 || K > T + 2)) Base = FMath::Max(Base, M[K]);
            // A jump in place, not a sharp stop: the frame's own travel must stand out too (a hammer meeting the anvil
            // stops dead, which is a jump in acceleration with no jump in travel).
            for (int32 K = T - 6; K <= T + 7; ++K) if (K >= 1 && K < N && K != T && K != T + 1) SpeedBase = FMath::Max(SpeedBase, S[K]);
            if (M[T] > PopRatio * Base && S[T] > SpeedRatio * SpeedBase) { Pops.Add({T, M[T], 1}); T += 2; }
        }
        int32 Run = 0, Start = 0; float Peak = 0;
        for (int32 T = 3; T <= N; ++T)
        {
            const bool bRev = T < N && M[T] > JitMin && M[T - 1] > JitMin && FVector3f::DotProduct(A[T], A[T - 1]) < -.5f * M[T] * M[T - 1];
            if (bRev) { if (!Run) Start = T; ++Run; Peak = FMath::Max(Peak, M[T]); }
            else { if (Run >= JitRun) Jits.Add({Start, Peak, Run}); Run = 0; Peak = 0; }
        }
    }

    // A made-up track the detector must read right: a slow swing, a 6 cm step at frame 60, a 1 cm shake 80..95.
    bool SelfTest()
    {
        TArray<FVector3f> Smooth, Faulty;
        for (int32 T = 0; T < 120; ++T)
        {
            const FVector3f S(10.f * FMath::Sin(T / Fps * 2 * PI), 4.f * FMath::Cos(T / Fps * PI), 0);
            Smooth.Add(S);
            Faulty.Add(S + FVector3f(0, 0, T >= 60 ? 6.f : 0.f) + FVector3f(T >= 80 && T <= 95 ? ((T & 1) ? 1.f : -1.f) : 0.f, 0, 0));
        }
        TArray<FHit> P0, J0, P1, J1;
        Detect(Smooth, P0, J0); Detect(Faulty, P1, J1);
        return P0.Num() == 0 && J0.Num() == 0 && P1.Num() == 1 && P1[0].Frame == 60 && J1.Num() == 1;
    }

    struct FTrack
    {
        TWeakObjectPtr<USkinnedMeshComponent> Comp;
        FString Key;
        bool bChuck = false;
        TArray<FName> Bones;
        TArray<TArray<FVector3f>> P;   // [bone][frame]
    };

    struct FRun
    {
        UWorld* World = nullptr;
        FString Region;
        TWeakObjectPtr<AChuckCharacter> Chuck;
        FVector Dir = FVector::ForwardVector;
        TArray<FTrack> Tracks;
        int32 Frame = 0;
        FDelegateHandle Hook;
        double Start = 0;
        bool bSelfTest = false;
    };

    const TCHAR* Phase(int32 F) { return F < WalkFrom ? TEXT("idle") : F < StopAt ? TEXT("walk") : F < SlashAt ? TEXT("stop") : TEXT("slash"); }

    bool Shown(const USkinnedMeshComponent* C)
    {
        const AActor* A = C->GetOwner();
        return C->IsRegistered() && C->GetSkinnedAsset() && A && !A->IsHidden() && C->IsVisible() && !C->bHiddenInGame && !C->LeaderPoseComponent.IsValid()
            && C->GetComponentSpaceTransforms().Num() > 0;
    }

    // The clearest of eight directions for the walk: no wall within 4.5 m at his waist, and floor under the path.
    FVector WalkDirection(UWorld* World, const AChuckCharacter* Chuck)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(MotionAuditWalk), false, Chuck);
        const FVector At = Chuck->GetActorLocation();
        FVector Best = Chuck->GetActorForwardVector().GetSafeNormal2D(); float BestClear = -1;
        for (int32 K = 0; K < 8; ++K)
        {
            const FVector D = Chuck->GetActorForwardVector().GetSafeNormal2D().RotateAngleAxis(45.f * K, FVector::UpVector);
            FHitResult Hit;
            float Clear = World->LineTraceSingleByChannel(Hit, At, At + D * 450, ECC_Pawn, Q) ? Hit.Distance : 450.f;
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, At + D * 250, At + D * 250 - FVector(0, 0, 160), ECC_Pawn, Q)) Clear *= .3f;
            if (Clear > BestClear + 1) { BestClear = Clear; Best = D; }
        }
        return Best;
    }

    void Finish(TSharedRef<FRun> R)
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(R->Hook);
        TArray<FString> Rows{TEXT("kind,key,bone,frame,time,phase,size_cm,frames")};
        int32 PopMeshes = 0, JitterMeshes = 0, Pops = 0, Jitter = 0, AllowedPops = 0, AllowedJitter = 0, ChuckPops = 0, ChuckJitter = 0, Bones = 0;
        float ChuckWalkTravel = 0;
        TMap<FString, int32> ByKey;
        for (const FTrack& T : R->Tracks)
        {
            Bones += T.Bones.Num();
            // Events of one mesh: bones popping within two frames of each other, or shaking over the same frames, are one.
            struct FEvt { int32 Frame, Length; float Size; FName Bone; };
            TArray<FEvt> P, J;
            for (int32 B = 0; B < T.Bones.Num(); ++B)
            {
                const TArray<FVector3f>& Track = T.P[B];
                if (T.bChuck)
                    for (int32 F = WalkFrom + 1; F < StopAt && F < Track.Num(); ++F) ChuckWalkTravel = FMath::Max(ChuckWalkTravel, (Track[F] - Track[WalkFrom]).Size());
                TArray<FHit> HP, HJ;
                Detect(Track, HP, HJ);
                for (const FHit& H : HP) P.Add({H.Frame, 1, H.Size, T.Bones[B]});
                for (const FHit& H : HJ) J.Add({H.Frame, H.Length, H.Size, T.Bones[B]});
            }
            auto Merge = [](TArray<FEvt>& E, int32 Gap)
            {
                E.Sort([](const FEvt& A, const FEvt& B) { return A.Frame < B.Frame; });
                TArray<FEvt> Out;
                for (const FEvt& X : E)
                {
                    if (Out.Num() && X.Frame <= Out.Last().Frame + Out.Last().Length + Gap)
                    {
                        FEvt& L = Out.Last();
                        L.Length = FMath::Max(L.Frame + L.Length, X.Frame + X.Length) - L.Frame;
                        if (X.Size > L.Size) { L.Size = X.Size; L.Bone = X.Bone; }
                    }
                    else Out.Add(X);
                }
                E = MoveTemp(Out);
            };
            Merge(P, 2); Merge(J, 0);
            auto Report = [&](const TArray<FEvt>& E, const TCHAR* Kind, int32& Count, int32& Allowed, int32& ChuckCount)
            {
                for (const FEvt& X : E)
                {
                    const FString Key = T.Key + TEXT(" ") + X.Bone.ToString();
                    if (ChuckAuditAllowed(TEXT("motion"), Key)) { ++Allowed; continue; }
                    ++Count; if (T.bChuck) ++ChuckCount;
                    Rows.Add(FString::Printf(TEXT("%s,%s,%s,%d,%.2f,%s,%.1f,%d"), Kind, *T.Key.Replace(TEXT(","), TEXT(";")), *X.Bone.ToString(), X.Frame, X.Frame / Fps,
                        Phase(X.Frame), X.Size, X.Length));
                    if (++ByKey.FindOrAdd(FString(Kind) + TEXT(" ") + T.Key) <= 3)
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_MOTION_AUDIT_%s %s bone=%s frame=%d t=%.2f phase=%s size=%.1fcm frames=%d"), Kind, *T.Key, *X.Bone.ToString(), X.Frame,
                            X.Frame / Fps, Phase(X.Frame), X.Size, X.Length);
                }
            };
            const int32 P0 = Pops, J0 = Jitter;
            Report(P, TEXT("POP"), Pops, AllowedPops, ChuckPops);
            Report(J, TEXT("JITTER"), Jitter, AllowedJitter, ChuckJitter);
            PopMeshes += Pops > P0; JitterMeshes += Jitter > J0;
        }
        for (const auto& K : ByKey) UE_LOG(LogTemp, Display, TEXT("CHUCK_MOTION_AUDIT_GROUP count=%d %s"), K.Value, *K.Key);
        FFileHelper::SaveStringArrayToFile(Rows, *(FPaths::ProjectSavedDir() / TEXT("Audit") / R->Region + TEXT("-motion.csv")));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_MOTION_AUDIT region=%s components=%d bones=%d frames=%d pop_meshes=%d jitter_meshes=%d pops=%d jitter=%d pops_allowed=%d jitter_allowed=%d chuck_pops=%d chuck_jitter=%d chuck_moved=%d chuck_walk_cm=%.0f selftest=%d ms=%.0f"),
            *R->Region, R->Tracks.Num(), Bones, R->Frame, PopMeshes, JitterMeshes, Pops, Jitter, AllowedPops, AllowedJitter, ChuckPops, ChuckJitter, ChuckWalkTravel > 10.f ? 1 : 0, ChuckWalkTravel,
            R->bSelfTest ? 1 : 0, (FPlatformTime::Seconds() - R->Start) * 1000);
        FTimerHandle Quit;
        R->World->GetTimerManager().SetTimer(Quit, [](){ FPlatformMisc::RequestExit(false); }, .5f, false);
    }

    // Each frame after the actors (and their animation) have ticked: read every pose, then drive Chuck for the next.
    void Sample(TSharedRef<FRun> R, UWorld* World)
    {
        if (World != R->World || R->Frame >= Frames) return;
        for (FTrack& T : R->Tracks)
        {
            const USkinnedMeshComponent* C = T.Comp.Get();
            const TArray<FTransform>* Space = C ? &C->GetComponentSpaceTransforms() : nullptr;
            const FVector Scale = C ? C->GetComponentScale() : FVector::OneVector;
            for (int32 B = 0; B < T.Bones.Num(); ++B)
            {
                const int32 I = C ? C->GetBoneIndex(T.Bones[B]) : INDEX_NONE;
                const FVector Pos = Space && Space->IsValidIndex(I) ? (*Space)[I].GetLocation() * Scale : (T.P[B].Num() ? FVector(T.P[B].Last()) : FVector::ZeroVector);
                T.P[B].Add(FVector3f(Pos));
            }
        }
        ++R->Frame;
        if (AChuckCharacter* Chuck = R->Chuck.Get())
        {
            if (R->Frame >= WalkFrom && R->Frame < StopAt) { Chuck->SetTestStickWorld(R->Dir); Chuck->AddMovementInput(R->Dir, 1.f, true); }
            else Chuck->SetTestStickWorld(FVector::ZeroVector);
            if (R->Frame == SlashAt) Chuck->Slash();
            if (R->Frame == SlashAt + 1) Chuck->SlashReleased();
        }
        if (R->Frame >= Frames) Finish(R);
    }

    void Begin(UWorld* World, const FString& Region)
    {
        TSharedRef<FRun> R = MakeShared<FRun>();
        R->World = World; R->Region = Region; R->Start = FPlatformTime::Seconds();
        R->bSelfTest = SelfTest();
        AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
        R->Chuck = Chuck;
        if (Chuck) R->Dir = WalkDirection(World, Chuck);
        const FVector Centre = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
        int32 Seen = 0, Hidden = 0, Far = 0;
        for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
        {
            USkinnedMeshComponent* C = *It;
            if (C->GetWorld() != World) continue;
            ++Seen;
            if (!Shown(C)) { ++Hidden; continue; }
            if (FVector::Dist(C->GetComponentLocation(), Centre) > 6000) { ++Far; continue; }
            // Read every frame as it plays: no skipping frames off screen or far away (those skips would read as pops).
            C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            C->bEnableUpdateRateOptimizations = false;
            FTrack& T = R->Tracks.AddDefaulted_GetRef();
            T.Comp = C;
            T.bChuck = C->GetOwner() == Chuck;
            T.Key = FString::Printf(TEXT("%s %s/%s"), *C->GetSkinnedAsset()->GetName(), *C->GetOwner()->GetName(), *C->GetName());
            const FReferenceSkeleton& Ref = C->GetSkinnedAsset()->GetRefSkeleton();
            for (int32 B = 0; B < Ref.GetNum(); ++B) T.Bones.Add(Ref.GetBoneName(B));
            T.P.SetNum(T.Bones.Num());
            for (TArray<FVector3f>& Track : T.P) Track.Reserve(Frames);
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_MOTION_AUDIT_BEGIN region=%s components=%d of=%d hidden_or_following=%d far=%d walk_dir=%s selftest=%d"), *Region, R->Tracks.Num(), Seen, Hidden, Far, *R->Dir.ToString(), R->bSelfTest ? 1 : 0);
        // A fixed 30 fps from here on, so a slow machine reads the same as a fast one.
        FApp::SetUseFixedTimeStep(true);
        FApp::SetFixedDeltaTime(1.0 / Fps);
        R->Hook = FWorldDelegates::OnWorldPostActorTick.AddLambda([R](UWorld* W, ELevelTick, float) { Sample(R, W); });
    }

    void WaitThenRun(UWorld* World, FString Region, double Since)
    {
        const bool bChult = Region.StartsWith(TEXT("Chult"));
        if ((bChult && !IsChultGroundingDone()) || World->GetTimeSeconds() - Since < 6.0)
        {
            FTimerHandle Again;
            World->GetTimerManager().SetTimer(Again, [World, Region, Since]() { WaitThenRun(World, Region, Since); }, 1.f, false);
            return;
        }
        Begin(World, Region);
    }
}

// Called by the game mode with the checkpoint already started (or Waterdeep).
void StartMotionAudit(UWorld* World, const FString& Region)
{
    UE_LOG(LogTemp, Display, TEXT("CHUCK_MOTION_AUDIT_START region=%s"), *Region);
    WaitThenRun(World, Region, World->GetTimeSeconds());
}
