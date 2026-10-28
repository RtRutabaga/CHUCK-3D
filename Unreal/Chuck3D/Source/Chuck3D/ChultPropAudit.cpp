// Props that sit wrongly in Chult: floating trees, open/walk-through logs, jars in stone (Claude, 2026-10-27;
// docs/CHULT-PROP-AUDIT.md). See ChultPropAudit.h.
#include "ChultPropAudit.h"
#include "ChuckCharacter.h"
#include "ClayJar.h"
#include "ChultTempleInterior.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    constexpr float MaxDrop = 160.f;   // more than this and the tree stands on a ledge: left as it is
    constexpr float Sink = 4.f;        // the foot let down this far into its lowest ground
    constexpr float Floating = 5.f;    // the audit's limit for a foot over air

    // The trees: where their foot meets the ground, in the mesh's own frame (cm, at scale 1).
    float FootRadius(const FString& Mesh)
    {
        if (Mesh == TEXT("SM_FallTree_A")) return 78.f * 1.5f;   // the flare is 2.1 times the trunk at the ground
        if (Mesh == TEXT("SM_FallTree_B")) return 62.f * 1.5f;
        if (Mesh == TEXT("SM_FallPalm")) return 18.f;
        if (Mesh == TEXT("SM_ChultIslandTree")) return 1.f;      // see FootPoints
        return 0.f;
    }
    const TArray<FVector>& FootPoints(const FString& Mesh)
    {
        // The scanned tree's trunk stands about 90 cm off its origin: the outline of its vertices below 50 cm, read
        // from the imported asset (Unreal's frame: the FBX's Y is flipped), and its middle.
        static const TArray<FVector> Island = {{-6, -90, 0}, {-44, -91, 0}, {-22, -58, 0}, {-2, -53, 0}, {17, -66, 0}, {25, -78, 0}, {31, -97, 0},
            {26, -118, 0}, {1, -123, 0}, {-19, -123, 0}, {-35, -117, 0}, {-41, -108, 0}};
        static TMap<FString, TArray<FVector>> Rings;
        if (Mesh == TEXT("SM_ChultIslandTree")) return Island;
        TArray<FVector>& Ring = Rings.FindOrAdd(Mesh);
        if (Ring.IsEmpty())
        {
            Ring.Add(FVector::ZeroVector);
            for (int32 K = 0; K < 12; ++K) Ring.Add(FRotator(0, K * 30.f, 0).Vector() * FootRadius(Mesh));
        }
        return Ring;
    }

    // What a tree's foot or a log stands on: the terrain and masonry, not other plants, rocks' instances, hidden
    // trunk/root blockers, characters or jars.
    bool IsGround(const UPrimitiveComponent* C)
    {
        if (!C) return false;
        if (C->IsA<UInstancedStaticMeshComponent>() || C->IsA<UShapeComponent>()) return false;
        if (const AActor* A = C->GetOwner()) if (A->IsA<APawn>() || A->IsA<AClayJar>()) return false;
        return !C->GetName().Contains(TEXT("Root"));
    }

    bool GroundZ(UWorld* World, const FVector& From, float Depth, float& Z)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPropGround), true);
        for (int32 I = 0; I < 12; ++I)
        {
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, Depth), ECC_Visibility, Q)) return false;
            if (!IsGround(Hit.GetComponent())) { Q.AddIgnoredComponent(Hit.GetComponent()); continue; }
            Z = Hit.ImpactPoint.Z; return true;
        }
        return false;
    }

    // How far the foot of a tree placed by T stands over the lowest ground under it (negative: sunk in). The
    // first foot point is its middle: the other points' ground counts only within 4 m of that (not a cliff below).
    struct FFoot { bool bGround = false; float Drop = 0; int32 OverDrop = 0; };
    FFoot Foot(UWorld* World, const FTransform& T, const FString& Mesh)
    {
        FFoot F; const TArray<FVector>& Points = FootPoints(Mesh);
        float Centre;
        const FVector Middle = T.TransformPosition(Points[0]);
        if (!GroundZ(World, Middle + FVector(0, 0, 400), 1100, Centre)) return F;
        F.bGround = true; F.Drop = Middle.Z - Centre;
        for (int32 K = 1; K < Points.Num(); ++K)
        {
            const FVector P = T.TransformPosition(Points[K]);
            float Z;
            // Traced from over both it and its middle's ground, so a foot already let down reads the same.
            if (GroundZ(World, FVector(P.X, P.Y, FMath::Max(P.Z, Centre) + 400), 1100 + FMath::Max(0.f, float(P.Z - Centre)), Z) && Z > Centre - 400) { if (Z < Centre + 400) F.Drop = FMath::Max(F.Drop, P.Z - Z); }
            else ++F.OverDrop;   // nothing within 4 m under this part of its foot: over a cliff or bank
        }
        return F;
    }

    struct FIssue { FString What; FVector At; float Size; };
}

namespace
{
    // The trees most let down, for the audit's stills (where they stood was the worst).
    TArray<FIssue> GMoved;
    bool GGroundingDone = false;
}

bool IsChultGroundingDone() { return GGroundingDone || FParse::Param(FCommandLine::Get(), TEXT("ChuckNoGrounding")); }

void GroundChultTrees(UWorld* World)
{
    GGroundingDone = false; GMoved.Reset();
    if (!World || FParse::Param(FCommandLine::Get(), TEXT("ChuckNoGrounding"))) return;
    // The work is spread over frames (a few milliseconds each): the set is built as the fall cutscene starts.
    struct FJob
    {
        TMap<FIntVector, TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>> Collision;
        TArray<TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>> Trees;
        TSet<TWeakObjectPtr<UInstancedStaticMeshComponent>> Dirty;
        TArray<TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>> Remove;
        int32 Next = 0, Lowered = 0, Ledges = 0, NoGround = 0, Paired = 0, Frames = 0, Pass = 0; float Deepest = 0; double Spent = 0, ResumeAt = 0;
        int32 Total = 0;
        TArray<TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>> Retry;   // no ground under it yet: tried again later
        FTimerHandle Timer;
    };
    auto Job = MakeShared<FJob>();
    auto Key = [](const FVector& P) { return FIntVector(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y), FMath::RoundToInt(P.Z)); };
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Sets(*It);
        for (UInstancedStaticMeshComponent* H : Sets)
        {
            if (!H->GetStaticMesh()) continue;
            const FString Name = H->GetStaticMesh()->GetName();
            if (Name == TEXT("SM_ChultIslandTrunkCollision"))
                for (int32 I = 0; I < H->GetInstanceCount(); ++I) { FTransform T; H->GetInstanceTransform(I, T, true); Job->Collision.Add(Key(T.GetLocation()), {H, I}); }
            else if (FootRadius(Name) > 0)
                for (int32 I = 0; I < H->GetInstanceCount(); ++I) Job->Trees.Add({H, I});
        }
    }
    Job->Total = Job->Trees.Num();
    World->GetTimerManager().SetTimer(Job->Timer, [World, Job, Key]()
    {
        if (World->GetTimeSeconds() < Job->ResumeAt) return;
        const double Start = FPlatformTime::Seconds();
        ++Job->Frames;
        while (Job->Next < Job->Trees.Num() && FPlatformTime::Seconds() - Start < .004)
        {
            const auto& Entry = Job->Trees[Job->Next++];
            UInstancedStaticMeshComponent* H = Entry.Key.Get();
            if (!H || !H->GetStaticMesh() || Entry.Value >= H->GetInstanceCount()) continue;
            FTransform T; H->GetInstanceTransform(Entry.Value, T, true);
            const FFoot F = Foot(World, T, H->GetStaticMesh()->GetName());
            if (!F.bGround) { if (Job->Pass < 2) Job->Retry.Add(Entry); else ++Job->NoGround; continue; }
            if (F.Drop > MaxDrop || F.OverDrop >= 2)
            {
                // Over a drop (a bank's or cliff's edge, the gorge's mist): taken out, with its trunk collision.
                ++Job->Ledges; Job->Remove.Add({H, Entry.Value});
                if (const auto* Pair = Job->Collision.Find(Key(T.GetLocation()))) Job->Remove.Add(*Pair);
                continue;
            }
            // The scanned tree's foot is a thick slab whose rim reads as a pedestal: it goes in to its top.
            const float In = H->GetStaticMesh()->GetName() == TEXT("SM_ChultIslandTree") ? 14.f * T.GetScale3D().Z : Sink;
            const float Down = F.Drop + In;   // its lowest foot point that far into the ground
            if (Down <= 1.f) continue;
            const FVector Old = T.GetLocation();
            T.SetLocation(Old - FVector(0, 0, Down));
            // A big move is measured again where it ends up (a cliff's edge can read as ground until the foot is
            // below its lip): over a drop there, it is taken out instead.
            if (Down > 30.f)
            {
                const FFoot After = Foot(World, T, H->GetStaticMesh()->GetName());
                if (After.bGround && (After.Drop > Floating || After.OverDrop >= 2))
                {
                    ++Job->Ledges; Job->Remove.Add({H, Entry.Value});
                    if (const auto* Pair = Job->Collision.Find(Key(Old))) Job->Remove.Add(*Pair);
                    continue;
                }
            }
            H->UpdateInstanceTransform(Entry.Value, T, true, false, true);
            Job->Dirty.Add(H);
            if (const auto* Pair = Job->Collision.Find(Key(Old)))
                if (UInstancedStaticMeshComponent* C = Pair->Key.Get())
                {
                    FTransform CT; C->GetInstanceTransform(Pair->Value, CT, true);
                    CT.SetLocation(CT.GetLocation() - FVector(0, 0, Down));
                    C->UpdateInstanceTransform(Pair->Value, CT, true, false, true);
                    Job->Dirty.Add(C); ++Job->Paired;
                }
            ++Job->Lowered; Job->Deepest = FMath::Max(Job->Deepest, Down);
            GMoved.Add({FString::Printf(TEXT("moved_%s"), *H->GetStaticMesh()->GetName()), T.TransformPosition(FootPoints(H->GetStaticMesh()->GetName())[0]), Down});
        }
        for (const auto& H : Job->Dirty) if (H.IsValid()) H->MarkRenderStateDirty();
        Job->Dirty.Reset();
        Job->Spent += FPlatformTime::Seconds() - Start;
        if (Job->Next < Job->Trees.Num()) return;
        // Some ground (cooked collision of the far settings) enters the physics scene later: those trees again.
        if (Job->Retry.Num() && Job->Pass < 2)
        {
            Job->Trees = MoveTemp(Job->Retry); Job->Retry.Reset(); Job->Next = 0; ++Job->Pass;
            Job->ResumeAt = World->GetTimeSeconds() + 2.5;
            return;
        }
        // Removed last, highest index first, so no index used above moves under another.
        Job->Remove.Sort([](const auto& A, const auto& B) { return A.Key.Get() != B.Key.Get() ? A.Key.Get() < B.Key.Get() : A.Value > B.Value; });
        for (const auto& R : Job->Remove) if (UInstancedStaticMeshComponent* H = R.Key.Get()) if (R.Value < H->GetInstanceCount()) H->RemoveInstance(R.Value);
        GMoved.Sort([](const FIssue& A, const FIssue& B) { return A.Size > B.Size; });
        if (GMoved.Num() > 12) GMoved.SetNum(12);
        GGroundingDone = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_GROUNDING trees=%d lowered=%d collision_lowered=%d deepest=%.0f removed_over_drops=%d no_ground=%d ms=%.0f frames=%d retries=%d"),
            Job->Total, Job->Lowered, Job->Paired, Job->Deepest, Job->Ledges, Job->NoGround, Job->Spent * 1000, Job->Frames, Job->Pass);
        // Last: clearing the timer destroys this lambda and what it holds.
        FTimerHandle Handle = Job->Timer;
        World->GetTimerManager().ClearTimer(Handle);
    }, .001f, true);
}

namespace
{
    int32 Measure(UWorld* World, bool bLog, TArray<FIssue>* Out)
    {
        TArray<FIssue> Issues, Ledges;
        int32 TreesChecked = 0, FloatingTrees = 0, LedgeTrees = 0, Logs = 0, LogsOpen = 0, LogsFloating = 0, Jars = 0, JarsInStone = 0, JarsFloating = 0, JarsTouching = 0;
        float WorstTree = 0;
        TMap<FString, int32> FloatingBy;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<UInstancedStaticMeshComponent*> Sets(*It);
            for (UInstancedStaticMeshComponent* H : Sets)
            {
                if (!H->GetStaticMesh() || !H->IsVisible() || H->bHiddenInGame) continue;
                const FString Name = H->GetStaticMesh()->GetName();
                const float Radius = FootRadius(Name);
                if (Radius > 0)
                    for (int32 I = 0; I < H->GetInstanceCount(); ++I)
                    {
                        FTransform T; H->GetInstanceTransform(I, T, true); ++TreesChecked;
                        const FFoot F = Foot(World, T, Name);
                        if (!F.bGround) continue;
                        if (F.Drop > MaxDrop || F.OverDrop >= 2) { ++LedgeTrees; Ledges.Add({FString::Printf(TEXT("ledge_%s %s/%s"), *Name, *GetNameSafe(H->GetOwner()), *H->GetName()), T.TransformPosition(FootPoints(Name)[0]), F.Drop}); continue; }
                        if (F.Drop > Floating)
                        {
                            ++FloatingTrees; ++FloatingBy.FindOrAdd(Name); WorstTree = FMath::Max(WorstTree, F.Drop);
                            Issues.Add({FString::Printf(TEXT("tree_%s"), *Name), T.TransformPosition(FootPoints(Name)[0]), F.Drop});
                        }
                    }
                if (Name == TEXT("SM_FallLog"))
                    for (int32 I = 0; I < H->GetInstanceCount(); ++I)
                    {
                        // The log lies along its local X (-350..350), its axis 48 cm up, 55 cm thick at the butt.
                        FTransform T; H->GetInstanceTransform(I, T, true); ++Logs;
                        int32 Open = 0; float Over = 0;
                        for (float U : {-.42f, -.2f, 0.f, .2f, .42f})
                        {
                            const FVector Axis = T.TransformPosition(FVector(700 * U, 0, 48));
                            FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPropLog), false);
                            Open += !World->OverlapAnyTestByChannel(Axis, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(12), Q);
                            const float R = 55 * (1 - .25f * (U + .5f)) * T.GetScale3D().Z;
                            float Z; if (GroundZ(World, Axis + FVector(0, 0, 200), 600, Z)) Over = FMath::Max(Over, Axis.Z - R - Z);
                        }
                        LogsOpen += Open > 0; LogsFloating += Over > Floating;
                        if (Open) Issues.Add({TEXT("log_walk_through"), T.GetLocation(), float(Open)});
                        if (Over > Floating) Issues.Add({TEXT("log_floating"), T.GetLocation(), Over});
                        if (bLog) UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_LOG at=%s open_points=%d over_ground=%.1f"), *T.GetLocation().ToString(), Open, Over);
                    }
            }
        }
        // Jars and urns: nothing of the masonry inside them, standing on what is under them.
        for (TActorIterator<AClayJar> It(World); It; ++It)
        {
            AClayJar* Jar = *It;
            if (!Jar->BlocksChuck()) continue;
            const UCapsuleComponent* Cap = Jar->FindComponentByClass<UCapsuleComponent>();
            if (!Cap) continue;
            ++Jars;
            const float R = Cap->GetScaledCapsuleRadius(), HH = Cap->GetScaledCapsuleHalfHeight();
            const FVector Base = Cap->GetComponentLocation() - FVector(0, 0, HH);
            FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPropJar), true, Jar);
            TArray<FOverlapResult> Hits;
            const float H2 = HH - 4;
            World->OverlapMultiByObjectType(Hits, Base + FVector(0, 0, 6 + H2), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeCapsule(R * .85f, H2), Q);
            FString In;
            for (const FOverlapResult& O : Hits)
            {
                const UPrimitiveComponent* C = O.GetComponent();
                if (!C || C->IsA<UShapeComponent>() || C->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
                if (const auto* ISM = Cast<UInstancedStaticMeshComponent>(C)) if (ISM->GetStaticMesh() && ISM->GetStaticMesh()->GetName().StartsWith(TEXT("SM_ChultFern"))) continue;
                In += FString::Printf(TEXT("%s/%s "), *GetNameSafe(C->GetOwner()), *C->GetName());
            }
            // Another jar crowding it (their blockers only answer the pawn channel).
            for (TActorIterator<AClayJar> J(World); J; ++J)
                if (*J != Jar && J->BlocksChuck() && FVector::Dist2D(J->GetActorLocation(), Jar->GetActorLocation()) < R + J->FindComponentByClass<UCapsuleComponent>()->GetScaledCapsuleRadius() - 1.f
                    && FMath::Abs(J->GetActorLocation().Z - Jar->GetActorLocation().Z) < HH * 2) { ++JarsTouching; Issues.Add({TEXT("jar_in_jar"), Base, 0}); break; }
            float Z = Base.Z; const bool bGround = GroundZ(World, Base + FVector(0, 0, 8), 80, Z);
            const float Gap = bGround ? Base.Z - Z : 80.f;
            if (!In.IsEmpty()) { ++JarsInStone; Issues.Add({FString::Printf(TEXT("jar_in_stone %s"), *In), Base, 0}); }
            if (Gap > 2.5f) { ++JarsFloating; Issues.Add({TEXT("jar_floating"), Base, Gap}); }
            if (bLog && (!In.IsEmpty() || Gap > 2.5f)) UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_JAR %s at=%s interior=%s radius=%.1f gap=%.1f in=%s"), *Jar->GetClass()->GetName(), *Base.ToString(), *ChultTempleInteriorFrameOf(Base).ToCompactString(), R, Gap, *In);
        }
        Issues.Sort([](const FIssue& A, const FIssue& B) { return A.Size > B.Size; });
        if (bLog)
        {
            FString By; for (const auto& P : FloatingBy) By += FString::Printf(TEXT(" %s=%d"), *P.Key, P.Value);
            for (int32 I = 0; I < FMath::Min(Issues.Num(), 40); ++I) UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_ISSUE %s at=%s size=%.1f"), *Issues[I].What, *Issues[I].At.ToString(), Issues[I].Size);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_COUNTS trees=%d floating_trees=%d worst_tree=%.1f ledge_trees=%d logs=%d logs_walk_through=%d logs_floating=%d jars=%d jars_in_stone=%d jars_floating=%d jars_touching=%d by:%s"),
                TreesChecked, FloatingTrees, WorstTree, LedgeTrees, Logs, LogsOpen, LogsFloating, Jars, JarsInStone, JarsFloating, JarsTouching, *By);
        }
        if (Out) { *Out = Issues; Ledges.Sort([](const FIssue& A, const FIssue& B) { return A.Size > B.Size; }); for (int32 I = 0; I < FMath::Min(Ledges.Num(), 3); ++I) Out->Add(Ledges[I]); }
        if (bLog) for (const FIssue& L : Ledges) UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_LEDGE %s at=%s drop=%.0f"), *L.What, *L.At.ToString(), L.Size);
        return FloatingTrees + LedgeTrees + LogsOpen + LogsFloating + JarsInStone + JarsFloating + JarsTouching;
    }
}

int32 CheckChultProps(UWorld* World, bool bLog) { return Measure(World, bLog, nullptr); }

void ReviewChultProps(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    if (!IsChultGroundingDone())
    {
        FTimerHandle Again;
        Owner->GetWorldTimerManager().SetTimer(Again, FTimerDelegate::CreateWeakLambda(Owner, [Owner, Chuck, Failures]() { ReviewChultProps(Owner, Chuck, Failures); }), .5f, false);
        return;
    }
    TArray<FIssue> Issues;
    const int32 Problems = Measure(World, true, &Issues);
    // Stills of the worst: up to six trees, every log and jar issue (at most 14 in all), and the two logs.
    TArray<FIssue> Views;
    int32 TreesShown = 0;
    for (const FIssue& I : Issues)
    {
        const bool bTree = I.What.StartsWith(TEXT("tree_"));
        if (I.What.StartsWith(TEXT("ledge_")) && TreesShown >= 9) continue;
        if (bTree && TreesShown >= 6) continue;
        TreesShown += bTree; Views.Add(I);
        if (Views.Num() >= 14) break;
    }
    for (int32 I = 0; I < FMath::Min(GMoved.Num(), 4); ++I) Views.Add(GMoved[I]);
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Sets(*It);
        for (UInstancedStaticMeshComponent* H : Sets)
            if (H->GetStaticMesh() && H->GetStaticMesh()->GetName() == TEXT("SM_FallLog"))
                for (int32 I = 0; I < H->GetInstanceCount(); ++I) { FTransform T; H->GetInstanceTransform(I, T, true); Views.Add({TEXT("log_view"), T.TransformPosition(FVector(-350, 0, 0)), -1}); }
    }
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(60);
    Chuck->SetActorHiddenInGame(true);
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->SetViewTarget(Camera); Chuck->DisableInput(PC); }
    struct FRun { int32 Phase = 0; double Next = 0; FTimerHandle Timer; };
    auto Run = MakeShared<FRun>();
    const FString Tag = FParse::Param(FCommandLine::Get(), TEXT("ChuckNoGrounding")) ? TEXT("Before") : TEXT("After");
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [World, Chuck, Camera, Run, Views, Tag, Problems, Failures]()
    {
        const double Now = World->GetTimeSeconds();
        if (Now < Run->Next) return;
        Run->Next = Now + 1.5;
        const int32 I = Run->Phase / 2;
        if (I < Views.Num())
        {
            const FIssue& V = Views[I];
            const bool bJar = V.What.StartsWith(TEXT("jar"));
            const bool bLogView = V.What.StartsWith(TEXT("log"));
            if (Run->Phase % 2 == 0)
            {
                // Looking at the foot from a little above it, from the open side (the lowest ground's direction is unknown: a fixed quarter).
                const float Back = bJar ? 140.f : bLogView ? 170.f : 600.f, Up = bJar ? 70.f : bLogView ? 70.f : 140.f;
                const FVector Aim = V.At + FVector(0, 0, bJar ? 15.f : 40.f);
                const FVector Eye = Aim + FRotator(0, bLogView ? 200.f : 35.f + 50.f * (I % 3), 0).Vector() * Back + FVector(0, 0, Up);
                Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
                // The log's ends are shut in by undergrowth: it is hidden for these stills.
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    TInlineComponentArray<UInstancedStaticMeshComponent*> Sets(*It);
                    for (UInstancedStaticMeshComponent* H : Sets)
                        if (H->GetStaticMesh() && (H->GetStaticMesh()->GetName().Contains(TEXT("Fern")) || H->GetStaticMesh()->GetName().Contains(TEXT("Shrub")) || H->GetStaticMesh()->GetName().Contains(TEXT("ElephantEar")) || H->GetStaticMesh()->GetName().Contains(TEXT("Palm"))))
                            H->SetVisibility(!bLogView);
                }
                Chuck->SetActorLocation(Eye + FVector(0, 0, 5000));
                ++Run->Phase; return;
            }
            const FString File = FString::Printf(TEXT("PropAudit_%s_%02d_%s.png"), *Tag, I, *V.What.Left(V.What.Find(TEXT(" ")) > 0 ? V.What.Find(TEXT(" ")) : V.What.Len()));
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / File, true, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT_VIEW file=%s what=%s at=%s"), *File, *V.What, *V.At.ToString());
            ++Run->Phase; return;
        }
        const int32 Fails = Failures + Problems;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PROP_AUDIT failures=%d problems=%d setting_failures=%d grounding=%s"), Fails, Problems, Failures, *Tag);
        World->GetTimerManager().ClearTimer(Run->Timer);
        FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0);
    }, .1f, true);
}
