#include "TempleSkeleton.h"
#include "ChuckCharacter.h"
#include "ChultTempleInterior.h"
#include "SkeletonClipData.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

// -ChuckSkeletonTest (TempleSkeleton.h): the real character and the temple skeletons, trial by trial.
namespace { constexpr float SkeletonFallSeconds = SkeletonClipData::CollapseLength / ATempleSkeleton::CollapseRate; }   // the fall, played fast
void StartTempleSkeletonReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    TArray<ATempleSkeleton*> All;
    for (TActorIterator<ATempleSkeleton> It(World); It; ++It) All.Add(*It);
    if (!Chuck || All.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_TEMPLE_SKELETON_TEST_COMPLETE failures=1 skeletons=%d"), All.Num());
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC); Chuck->SetLookLocked(true);
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckSkeletonCapture"));
    // The skeleton nearest a frame position (S, T).
    auto At = [All](float S, float T)
    {
        ATempleSkeleton* Best = nullptr; float BestD = 1e9f;
        for (ATempleSkeleton* K : All)
        {
            const FVector F = ChultTempleInteriorFrameOf(K->GetPost().Feet);
            const float D = FVector2D::Distance(FVector2D(F.X, F.Y), FVector2D(S, T));
            if (D < BestD) { BestD = D; Best = K; }
        }
        return Best;
    };
    enum EKind { KPlace, KWake, KKick, KGrab, KSwat, KStepBack, KDodge, KLeash, KRings, KKill };
    struct FTrial { EKind Kind; const TCHAR* Name; float S, T; };
    // The skeletons used (their posts, ChultTempleSkeletonPosts): a lying one and a standing one before the rift,
    // the one by the rings, and the first gap of the spike corridor.
    static const FTrial Trials[] = {{KPlace, TEXT("placement"), 0, 0}, {KWake, TEXT("wake_rise_chase"), 68650, 1100},
        {KKick, TEXT("kick_lands"), 68250, 1150}, {KGrab, TEXT("grab_lands"), 68250, 1150}, {KSwat, TEXT("swat_lands"), 68250, 1150},
        {KStepBack, TEXT("step_back_misses"), 68250, 1150}, {KDodge, TEXT("roll_dodges"), 68250, 1150},
        {KLeash, TEXT("kept_to_its_gap"), 72350, 1230}, {KRings, TEXT("rings_out_of_reach"), 68250, 2350}, {KKill, TEXT("ten_scratches_fall"), 68250, 1150}};
    static constexpr int32 Count = UE_ARRAY_COUNT(Trials);
    struct FRun
    {
        int32 Trial = 0, Failures = 0, Phase = 0, Shot = 0, Landed = 0, Attacks = 0, Settles = 0;
        float Start = -1, Tick = -1, LastPress = -10, NextShot = 0, PelvisZ = 0, MinDist = 1e9f;
        bool bSawRise = false, bSawChase = false, bSawReturn = false, bSawAttack = false, bPlaced = false;
        ATempleSkeleton* Subject = nullptr;
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
        const ATempleSkeleton::EAttack AttackOf = Trial.Kind == KGrab ? ATempleSkeleton::EAttack::Grab : Trial.Kind == KSwat ? ATempleSkeleton::EAttack::Swat : ATempleSkeleton::EAttack::Kick;
        auto Ground = [&](const FVector& Feet, const FVector& Facing)
        {
            Chuck->ResetAtLocation(Feet + FVector(0, 0, 34.65f)); Move->StopMovementImmediately();
            Chuck->SetActorRotation(Facing.GetSafeNormal2D().Rotation()); Chuck->Recenter();
        };
        if (Run->Start < 0)
        {
            Run->Start = Now; Run->Phase = 0; Run->LastPress = -10; Run->MinDist = 1e9f;
            Run->bSawRise = Run->bSawChase = Run->bSawReturn = Run->bSawAttack = false;
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false); Chuck->SetSanity(5);
            for (ATempleSkeleton* K : All) { K->ResetToPost(); K->SetHeld(true); }
            Run->Subject = Trial.Kind == KPlace ? nullptr : At(Trial.S, Trial.T);
            ATempleSkeleton* K = Run->Subject;
            if (K)
            {
                K->SetHeld(false);
                K->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;   // its bones measured off screen too
                const FVector Fwd = FRotator(0, K->GetPost().Yaw, 0).Vector();
                switch (Trial.Kind)
                {
                case KWake: Ground(K->GetPost().Feet + Fwd * 330.f, -Fwd); break;
                case KKick: case KStepBack: case KDodge: Ground(K->GetPost().Feet + Fwd * 76.f, -Fwd); K->ForceAttack(ATempleSkeleton::EAttack::Kick); break;
                case KGrab: Ground(K->GetPost().Feet + Fwd * 84.f, -Fwd); K->ForceAttack(ATempleSkeleton::EAttack::Grab); break;
                case KSwat: Ground(K->GetPost().Feet + Fwd * 46.f, -Fwd); K->ForceAttack(ATempleSkeleton::EAttack::Swat); break;
                case KLeash: Ground(K->GetPost().Feet + Fwd * 150.f, -Fwd); break;
                case KRings:
                    // At the rift's lip under the first ring (frame S 68030 + 15, T 2100), facing across: one jump catches it.
                    Ground(ChultTempleInteriorWorldOf(FVector(68045, 2100, -1460)), ChultTempleInteriorWorldOf(FVector(67000, 2100, -1460)) - ChultTempleInteriorWorldOf(FVector(68045, 2100, -1460)));
                    break;
                case KKill: Ground(K->GetPost().Feet + Fwd * 50.f, -Fwd); break;
                default: break;
                }
                Run->Landed = K->GetLanded(AttackOf); Run->Attacks = K->GetAttacks(AttackOf); Run->Settles = K->GetSettles();
            }
        }
        const float T = Now - Run->Start;
        ATempleSkeleton* K = Run->Subject;
        bool Pass = false, Done = false; float Limit = 20;
        if (K)
        {
            const auto State = K->GetState();
            Run->bSawRise |= State == ATempleSkeleton::EState::Rise;
            Run->bSawChase |= State == ATempleSkeleton::EState::Chase;
            Run->bSawReturn |= State == ATempleSkeleton::EState::Return || State == ATempleSkeleton::EState::Settle;
            Run->bSawAttack |= State == ATempleSkeleton::EState::Attack;
            Run->MinDist = FMath::Min(Run->MinDist, static_cast<float>(FVector::Dist2D(K->GetActorLocation(), Chuck->GetActorLocation())));
        }
        const int32 Lost = K ? 5 - Chuck->GetSanity() : 0;
        switch (Trial.Kind)
        {
        case KPlace:
        {
            // Every post's skeleton on its floor, in its own space, with its clips, sounds and bodies.
            if (T < 2.f) return;
            if (Run->Phase == 0)
            {
                int32 Bad = 0, Spikes = 0, Piers = 0, Lying = 0;
                for (ATempleSkeleton* S : All)
                {
                    const FVector Feet = S->GetActorLocation() - FVector(0, 0, ATempleSkeleton::HalfHeight);
                    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(SkeletonFloor), false, S); Q.AddIgnoredActor(Chuck);
                    const bool bFloor = World->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 40), Feet - FVector(0, 0, 40), ECC_Visibility, Q);
                    const float FloorGap = bFloor ? static_cast<float>(Feet.Z - Hit.ImpactPoint.Z) : 999.f;
                    const bool bOk = bFloor && FMath::Abs(FloorGap) < 6.f && S->InLeash(S->GetActorLocation())
                        && FVector::Dist2D(S->GetActorLocation(), S->GetPost().Feet) < 10.f;
                    Bad += !bOk;
                    Spikes += FCString::Strcmp(S->GetPost().Space, TEXT("Spikes")) == 0; Piers += FCString::Strcmp(S->GetPost().Space, TEXT("Piers")) == 0;
                    Lying += S->GetState() == ATempleSkeleton::EState::Dormant;
                    if (!bOk) UE_LOG(LogTemp, Warning, TEXT("CHUCK_TEMPLE_SKELETON_PLACE_BAD space=%s floor=%d gap=%.1f leash=%d at=%s"), S->GetPost().Space, bFloor, FloorGap, S->InLeash(S->GetActorLocation()), *ChultTempleInteriorFrameOf(S->GetActorLocation()).ToString());
                }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_PLACEMENT skeletons=%d spikes=%d piers=%d lying=%d bad=%d"), All.Num(), Spikes, Piers, Lying, Bad);
                Run->bPlaced = All.Num() == 17 && Spikes == 5 && Piers == 12 && Lying == 11 && Bad == 0;
                Run->Phase = 1; Run->LastPress = T;
            }
            Pass = Run->bPlaced;
            Done = !bCapture;
            if (bCapture && T - Run->LastPress > .35f)
            {
                // Portraits by torchlight: a standing one front three-quarter, close on its skull and ribs, and the hall.
                if (!Run->Camera) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(55); }
                ATempleSkeleton* G = At(68250, 1150);
                const FVector Fwd = G->GetActorForwardVector(), Side = FVector::CrossProduct(FVector::UpVector, Fwd), C = G->GetActorLocation();
                const int32 View = (Run->Phase - 1) / 2;
                const FVector Eyes[] = {C + Fwd * 260.f + Side * 110.f + FVector(0, 0, -20), C + Fwd * 120.f - Side * 80.f + FVector(0, 0, 50), C + Fwd * 900.f + Side * 300.f + FVector(0, 0, 160)};
                const FVector Looks[] = {C + FVector(0, 0, -5), C + FVector(0, 0, 45), C - Fwd * 300.f};
                const TCHAR* Names[] = {TEXT("Portrait_Front"), TEXT("Portrait_Close"), TEXT("Hall")};
                if (View >= 3) { Done = true; break; }
                if ((Run->Phase - 1) % 2 == 0) { Run->Camera->SetActorLocationAndRotation(Eyes[View], (Looks[View] - Eyes[View]).Rotation()); PC->SetViewTarget(Run->Camera); }
                else
                {
                    const FString Folder = FPaths::ScreenShotDir() / TEXT("Skeleton"); IFileManager::Get().MakeDirectory(*Folder, true);
                    FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("T0_%s.png"), Names[View]), false, false);
                }
                ++Run->Phase; Run->LastPress = T;
            }
            break;
        }
        case KWake:
            // He comes within reach of a lying one: it gets up (the whole clip), then comes for him faster than his walk.
            Pass = Run->bSawRise && Run->bSawChase && K->GetRises() > 0 && K->GetTopSpeed() > 70.f && K->GetTopSpeed() < 100.f && Run->MinDist < 110.f;
            Done = Pass; Limit = 14;
            break;
        case KKick: case KGrab: case KSwat:
            // He stands where that attack reaches: it winds up, aims and strikes, and the bones meet him.
            Pass = K->GetAttacks(AttackOf) > Run->Attacks && K->GetLanded(AttackOf) > Run->Landed
                && Lost == (AttackOf == ATempleSkeleton::EAttack::Kick ? ATempleSkeleton::KickDamage : 1);
            Done = Pass && K->GetState() != ATempleSkeleton::EState::Attack; Limit = 10;
            break;
        case KStepBack:
            // The kick's wind-up, then a step back out of its reach before the strike: the foot passes short of him.
            if (Run->Phase == 0 && K->GetState() == ATempleSkeleton::EState::Attack && K->GetAnimEvaluations() > 0)
            {
                Run->Phase = 1; const FVector Away = (Chuck->GetActorLocation() - K->GetActorLocation()).GetSafeNormal2D();
                Ground(Chuck->GetActorLocation() - FVector(0, 0, 34.65f) + Away * 90.f, -Away);
            }
            if (Run->Phase == 1 && K->GetState() == ATempleSkeleton::EState::Recover) Run->Phase = 2;
            Pass = Run->Phase == 2 && K->GetLanded(AttackOf) == Run->Landed && Lost == 0 && K->GetLastStrikeGap() > ATempleSkeleton::HitSlack;
            Done = Run->Phase == 2; Limit = 10;
            break;
        case KDodge:
            // A roll out to the side as the kick winds up: no hit.
            if (Run->Phase == 0 && K->GetState() == ATempleSkeleton::EState::Attack) { Chuck->DodgeToward(FVector2D(1, 0)); Run->Phase = 1; }
            if (Run->Phase == 1 && K->GetState() == ATempleSkeleton::EState::Recover) Run->Phase = 2;
            Pass = Run->Phase == 2 && K->GetLanded(AttackOf) == Run->Landed && Lost == 0;
            Done = Run->Phase == 2; Limit = 10;
            break;
        case KLeash:
            // The corridor's first gap: woken, it follows him across the next band only as far as the band's edge,
            // gives up, walks back and lies down again.
            if (Run->Phase == 0 && K->GetState() == ATempleSkeleton::EState::Chase && T > 1.f)
            {
                Run->Phase = 1;   // he goes on over the band into the next gap
                Ground(ChultTempleInteriorWorldOf(FVector(71650, 1100, -1460)), ChultTempleInteriorWorldOf(FVector(70000, 1100, -1460)) - ChultTempleInteriorWorldOf(FVector(71650, 1100, -1460)));
            }
            Pass = Run->Phase == 1 && Run->bSawReturn && K->GetState() == ATempleSkeleton::EState::Dormant && K->GetSettles() > Run->Settles && K->GetWorstLeash() < 8.f && Lost == 0;
            Done = Pass; Limit = 30;
            break;
        case KRings:
            // He hangs from the first ring over the rift: it comes to the lip and stands there; no blow reaches him.
            if (Run->Phase == 0 && T > .6f && Move->IsMovingOnGround()) { Chuck->JumpPressed(); Run->Phase = 1; }
            if (Run->Phase == 1 && Chuck->IsSwinging()) Run->Phase = 2;
            Pass = Run->Phase == 2 && Chuck->IsSwinging() && Run->bSawChase && Lost == 0 && K->GetWorstLeash() < 8.f && T > 7.f;
            Done = Pass || (Run->Phase == 2 && !Chuck->IsSwinging()); Limit = 12;
            break;
        case KKill:
        {
            // He stands his ground (kept sane) and scratches at its shins: ten, and it goes down.
            Chuck->SetSanity(5);
            const FVector To = K->GetActorLocation() - Chuck->GetActorLocation();
            // As a player would: back in after a knock-back while it is not mid-blow, a scratch whenever it is in reach.
            const float Reach = AChuckCharacter::SlashReach + ATempleSkeleton::Radius - 6.f;
            Chuck->SetTestStick(FVector2D::ZeroVector);
            if (!K->IsDead() && To.Size2D() > Reach - 8.f && K->GetState() != ATempleSkeleton::EState::Attack)
            { Chuck->SetTestStickWorld(To.GetSafeNormal2D()); Chuck->AddMovementInput(To.GetSafeNormal2D(), 1.f, true); }
            if (!K->IsDead() && To.Size2D() < Reach && T - Run->LastPress > .34f && Move->IsMovingOnGround())
            { Chuck->SetActorRotation(To.GetSafeNormal2D().Rotation()); Chuck->Slash(); Chuck->SlashReleased(); Run->LastPress = T; }
            if (K->IsDead() && Run->Phase == 0) { Run->Phase = 1; Run->PelvisZ = static_cast<float>(K->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z); Run->LastPress = T; }
            // Down and lying: every limb on the floor (Chuck's feet), nothing through it.
            const float Floor = static_cast<float>(Chuck->GetActorLocation().Z - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            const float Drop = Run->Phase ? Run->PelvisZ - static_cast<float>(K->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z) : 0.f;
            const float Skull = static_cast<float>(K->GetMesh()->GetBoneLocation(TEXT("head")).Z) - Floor;
            float Lowest = 1e9f, Highest = -1e9f; FString Limbs;
            for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("hand_l"), TEXT("hand_r"), TEXT("upperarm_r"), TEXT("head")})
            {
                const float Z = static_cast<float>(K->GetMesh()->GetBoneLocation(Bone).Z) - Floor;
                Lowest = FMath::Min(Lowest, Z); Highest = FMath::Max(Highest, Z); Limbs += FString::Printf(TEXT(" %s=%.0f"), Bone, Z);
            }
            Pass = K->IsDead() && K->GetHitsTaken() >= ATempleSkeleton::Health && Drop > 60.f && Lowest > -8.f && Highest < 40.f;
            Done = Run->Phase && T - Run->LastPress > SkeletonFallSeconds + 1.f; Limit = 40;
            if (Done) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_COLLAPSE pelvis_drop=%.0f skull_above_floor=%.0f lowest=%.0f highest=%.0f jaw=%.0f%s"), Drop, Skull, Lowest, Highest, K->GetJaw(), *Limbs);
            break;
        }
        }
        if (bCapture && K && T >= Run->NextShot)
        {
            if (!Run->Camera) { Run->Camera = World->SpawnActor<ACameraActor>(); Run->Camera->GetCameraComponent()->SetFieldOfView(55); }
            // From the side of the line between them, a little above Chuck's eye.
            // After the collapse: on the heap, from above and to the side.
            const FVector Mid = K->IsDead() ? K->GetMesh()->GetBoneLocation(TEXT("spine_02")) : (K->GetActorLocation() + Chuck->GetActorLocation()) * .5f;
            FVector Side = FVector::CrossProduct((K->GetActorLocation() - Chuck->GetActorLocation()).GetSafeNormal2D(), FVector::UpVector);
            if (Side.IsNearlyZero()) Side = FVector::RightVector;
            const FVector Eye = Mid + Side * (K->IsDead() ? 200.f : 330.f) + FVector(0, 0, K->IsDead() ? 150.f : 20.f);
            Run->Camera->SetActorLocationAndRotation(Eye, (Mid + FVector(0, 0, K->IsDead() ? 0.f : -10.f) - Eye).Rotation()); PC->SetViewTarget(Run->Camera);
            const FString Folder = FPaths::ScreenShotDir() / TEXT("Skeleton"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("T%d_%s_%03d.png"), Run->Trial, Trial.Name, Run->Shot++), false, false);
            Run->NextShot += K->GetState() == ATempleSkeleton::EState::Attack ? .12f : .5f;
        }
        if (!Done && T < Limit) return;
        Run->Failures += !Pass;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_TRIAL trial=%d name=%s pass=%d time=%.2f state=%s attacks=%d landed=%d strike_gap=%.1f sanity=%d rises=%d settles=%d top_speed=%.0f worst_leash=%.1f min_dist=%.0f hits=%d sounds=%d"),
            Run->Trial, Trial.Name, Pass ? 1 : 0, T, K ? K->GetStateName() : TEXT("-"), K ? K->GetAttacks(AttackOf) - Run->Attacks : 0, K ? K->GetLanded(AttackOf) - Run->Landed : 0,
            K ? K->GetLastStrikeGap() : 0.f, Chuck->GetSanity(), K ? K->GetRises() : 0, K ? K->GetSettles() : 0, K ? K->GetTopSpeed() : 0.f, K ? K->GetWorstLeash() : 0.f, Run->MinDist,
            K ? K->GetHitsTaken() : 0, K ? K->GetSoundsPlayed() : 0);
        ++Run->Trial; Run->Start = -1; Run->Shot = 0; Run->NextShot = 0;
        if (Run->Trial >= Count)
        {
            int32 Sounds = 0; for (ATempleSkeleton* S : All) Sounds += S->GetSoundsPlayed();
            Run->Failures += Sounds < 20;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_TEST_COMPLETE failures=%d trials=%d sounds=%d"), Run->Failures, Count, Sounds);
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }, .016f, true);
}
