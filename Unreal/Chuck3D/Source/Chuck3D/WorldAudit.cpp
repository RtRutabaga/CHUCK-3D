// -ChuckWorldAudit[=<checkpoint>] (Claude, 2026-11-13; docs/AUTOMATION-DEFECT-CATALOG.md, categories 1 and 4).
// Starts at a dev checkpoint (Waterdeep when none is given; Chult builds the jungle and the temple; ShipArrival the
// ship), waits for the region to stand, then reads the scene and reports two kinds of fault a player has found by hand:
//
//  - Collision gaps: a visible mesh a rat on the floor could walk into (over 25 cm each way, its foot within 40 cm of
//    the floor under it) with no collision of its own and nothing solid inside it (CHUCK_WORLD_AUDIT_GAP). This is the
//    November 12 audit (-ChuckCollisionAudit, the side gate's jambs) for every region and every material.
//  - Floating props: a group of visible meshes over 10 cm that touch one another but nothing large and nothing solid
//    (CHUCK_WORLD_AUDIT_FLOAT): the coil below deck, the mop, the rubble that fell from nowhere.
//
// Intended cases (foliage, sails, fire sprites, the sanctum's floating piers) are listed with a reason in
// Content/Data/Audit/Allow.txt, so the audit stays at 0 and anything new stands out. Reads the scene only; quits.
// -ChuckCollisionAudit is kept as the Waterdeep run.
#include "CoreMinimal.h"
#include "ChultPropAudit.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/OverlapResult.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
    struct FAllow { FString Check, Pattern; };

    // Content/Data/Audit/Allow.txt: "<collision|float|any> <wildcard> # reason". The wildcard is matched against
    // "<mesh> <material> <actor>/<component>", so a line can name a mesh, a material or a builder's component.
    TArray<FAllow> LoadAllow()
    {
        TArray<FAllow> Out;
        TArray<FString> Lines;
        const FString File = FPaths::ProjectContentDir() / TEXT("Data/Audit/Allow.txt");
        if (!FFileHelper::LoadFileToStringArray(Lines, *File)) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_WORLD_AUDIT no allow list at %s"), *File); return Out; }
        for (FString L : Lines)
        {
            int32 Hash; if (L.FindChar(TEXT('#'), Hash)) L.LeftInline(Hash);
            L.TrimStartAndEndInline();
            TArray<FString> Parts; L.ParseIntoArrayWS(Parts);
            if (Parts.Num() >= 2) Out.Add({Parts[0].ToLower(), Parts[1]});
        }
        return Out;
    }

    bool Allowed(const TArray<FAllow>& List, const TCHAR* Check, const FString& Key)
    {
        for (const FAllow& A : List)
            if ((A.Check == Check || A.Check == TEXT("any")) && (Key.MatchesWildcard(A.Pattern) || Key.Contains(A.Pattern))) return true;
        return false;
    }

    // Two planted faults the audit must find each run (so a blind audit cannot pass): a 30 cm cube in the air over
    // Chuck, and a 60 cm cube with no collision on the floor beside him. Counted apart from the region's own.
    int32 GProbeGap = 0, GProbeFloat = 0;

    AActor* PlantProbes(UWorld* World)
    {
        const AActor* Chuck = UGameplayStatics::GetPlayerPawn(World, 0);
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Chuck || !Cube) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_WORLD_AUDIT no probes (chuck=%d cube=%d)"), Chuck != nullptr, Cube != nullptr); return nullptr; }
        FCollisionQueryParams Q(SCENE_QUERY_STAT(WorldAuditProbe), false, Chuck);
        FHitResult Floor;
        const FVector At = Chuck->GetActorLocation();
        const float FloorZ = World->LineTraceSingleByChannel(Floor, At, At - FVector(0, 0, 400), ECC_Pawn, Q) ? float(Floor.ImpactPoint.Z) : float(At.Z - 35);
        AActor* Probes = World->SpawnActor<AActor>();
        auto Add = [&](const TCHAR* Name, const FVector& Centre, float Size)
        {
            auto* M = NewObject<UStaticMeshComponent>(Probes, Name);
            M->SetStaticMesh(Cube); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            M->SetWorldLocation(Centre); M->SetWorldScale3D(FVector(Size / 100.f));
            if (!Probes->GetRootComponent()) Probes->SetRootComponent(M);
            M->RegisterComponent();
        };
        Add(TEXT("AuditProbeFloat"), FVector(At.X, At.Y, FloorZ + 20000), 30);   // 200 m up: clear of any canopy, inside only the sky
        // The gap probe on open floor near him: the first of eight directions round him, 90 cm out, where the floor is
        // within 20 cm of his and nothing solid stands in the cube (below decks a fixed spot fell inside the hull).
        FVector GapAt(At.X + 120, At.Y, FloorZ + 30);
        for (int32 K = 0; K < 8; ++K)
        {
            const FVector Try = At + FRotator(0, K * 45.f, 0).RotateVector(FVector(90, 0, 0));
            FHitResult Down;
            if (World->LineTraceSingleByChannel(Down, FVector(Try.X, Try.Y, FloorZ + 80), FVector(Try.X, Try.Y, FloorZ - 60), ECC_Pawn, Q) &&
                FMath::Abs(Down.ImpactPoint.Z - FloorZ) < 20 &&
                !World->OverlapBlockingTestByChannel(FVector(Try.X, Try.Y, Down.ImpactPoint.Z + 31), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(29)), Q))
            { GapAt = FVector(Try.X, Try.Y, Down.ImpactPoint.Z + 30); break; }
        }
        Add(TEXT("AuditProbeGap"), GapAt, 60);
        return Probes;
    }

    // A shot list of the findings (ChuckShots.h), for Tools/Take-Shots.ps1: Saved/Audit/<region>-findings.txt.
    TArray<FString> GShots;
    TArray<FString> GRows;   // every finding: Saved/Audit/<region>-findings.csv
    UWorld* GWorld_ = nullptr;
    void AddShot(const TCHAR* What, int32 N, const FVector& C, float Size)
    {
        // Of eight directions round it, slightly from above, the one with the longest clear view (a wall behind the
        // finding would otherwise fill the frame).
        const float D = FMath::Clamp(Size * 3.f, 120.f, 600.f);
        FVector Eye = C + FVector(-.62f, -.62f, .48f) * D;
        float Best = -1;
        for (int32 K = 0; K < 8 && GWorld_; ++K)
        {
            const float A = K * PI / 4;
            const FVector Dir = FVector(FMath::Cos(A) * .85f, FMath::Sin(A) * .85f, .52f).GetSafeNormal();
            FHitResult Hit;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(WorldAuditShot), true);
            const FVector Start = C + Dir * FMath::Min(Size * .6f + 5.f, D * .5f);
            const float Clear = GWorld_->LineTraceSingleByChannel(Hit, Start, C + Dir * D, ECC_Visibility, Q) ? float(Hit.Distance) : D;
            if (Clear > Best + 1) { Best = Clear; Eye = Start + Dir * FMath::Max(Clear - 15.f, 20.f); }
        }
        GShots.Add(FString::Printf(TEXT("shot %s-%02d %.0f %.0f %.0f  %.0f %.0f %.0f"), What, N, Eye.X, Eye.Y, Eye.Z, C.X, C.Y, C.Z));
    }

    struct FPiece
    {
        FVector C, Half;          // oriented box: centre, half extents along its axes
        FVector Ax[3];
        FBox Bounds;              // world-aligned box round it
        UPrimitiveComponent* Comp = nullptr;
        int32 Instance = 0;
        FString Key;              // "<mesh> <material> <actor>/<component>"
        bool bCandidate = false;  // a static mesh between 10 cm and 3 m: may float
        bool bAnchor = false;     // large, or a surface we cannot measure (landscape, procedural): holds what touches it
    };

    bool Overlaps(const FPiece& A, const FPiece& B, float Tol)
    {
        // Separating axis test of two oriented boxes, A grown by Tol.
        const FVector D = B.C - A.C;
        const FVector HA = A.Half + FVector(Tol);
        auto Sep = [&](const FVector& L)
        {
            if (L.SizeSquared() < 1e-6) return false;
            const double RA = HA.X * FMath::Abs(FVector::DotProduct(A.Ax[0], L)) + HA.Y * FMath::Abs(FVector::DotProduct(A.Ax[1], L)) + HA.Z * FMath::Abs(FVector::DotProduct(A.Ax[2], L));
            const double RB = B.Half.X * FMath::Abs(FVector::DotProduct(B.Ax[0], L)) + B.Half.Y * FMath::Abs(FVector::DotProduct(B.Ax[1], L)) + B.Half.Z * FMath::Abs(FVector::DotProduct(B.Ax[2], L));
            return FMath::Abs(FVector::DotProduct(D, L)) > RA + RB;
        };
        for (int32 I = 0; I < 3; ++I) { if (Sep(A.Ax[I]) || Sep(B.Ax[I])) return false; }
        for (int32 I = 0; I < 3; ++I) for (int32 J = 0; J < 3; ++J) if (Sep(FVector::CrossProduct(A.Ax[I], B.Ax[J]))) return false;
        return true;
    }

    // Every corner of A inside B shrunk by Margin.
    bool Inside(const FPiece& A, const FPiece& B, float Margin)
    {
        const FVector Lim = B.Half - FVector(Margin);
        if (Lim.GetMin() <= 0) return false;
        for (int32 C = 0; C < 8; ++C)
        {
            const FVector P = A.C + A.Ax[0] * ((C & 1) ? A.Half.X : -A.Half.X) + A.Ax[1] * ((C & 2) ? A.Half.Y : -A.Half.Y) + A.Ax[2] * ((C & 4) ? A.Half.Z : -A.Half.Z);
            const FVector D = P - B.C;
            if (FMath::Abs(FVector::DotProduct(D, B.Ax[0])) > Lim.X || FMath::Abs(FVector::DotProduct(D, B.Ax[1])) > Lim.Y || FMath::Abs(FVector::DotProduct(D, B.Ax[2])) > Lim.Z) return false;
        }
        return true;
    }

    FString PieceKey(const UPrimitiveComponent* P, const UStaticMesh* Mesh)
    {
        const UMaterialInterface* M = P->GetMaterial(0);
        return FString::Printf(TEXT("%s %s %s/%s"), Mesh ? *Mesh->GetName() : *P->GetClass()->GetName(), M ? *M->GetName() : TEXT("None"), *GetNameSafe(P->GetOwner()), *P->GetName());
    }

    // "<mesh> <material>": the group a fault is counted under (the builder's component names vary).
    FString Kind(const FString& Key)
    {
        int32 A = Key.Find(TEXT(" ")); if (A == INDEX_NONE) return Key;
        int32 B = Key.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, A + 1);
        return B == INDEX_NONE ? Key : Key.Left(B);
    }

    bool Shown(const AActor* A, const UPrimitiveComponent* P, const AActor* Chuck)
    {
        if (!A || A->IsHidden() || !P->IsVisible() || P->bHiddenInGame || P->IsA<UShapeComponent>()) return false;
        if (A == Chuck || A->GetAttachParentActor() == Chuck || A->IsA<APawn>()) return false;
        return true;
    }

    void Gather(UWorld* World, TArray<FPiece>& Out)
    {
        const AActor* Chuck = UGameplayStatics::GetPlayerPawn(World, 0);
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<UPrimitiveComponent*> Prims(*It);
            for (UPrimitiveComponent* P : Prims)
            {
                if (!Shown(*It, P, Chuck)) continue;
                auto* SM = Cast<UStaticMeshComponent>(P);
                if (auto* Proc = Cast<UProceduralMeshComponent>(P))
                {
                    // Procedural surfaces (timber, terrain, rock) as boxes round every 24 of their triangles: tight
                    // enough that the air over a terrain section does not hold anything. They hold; they never float.
                    const FTransform T = Proc->GetComponentTransform();
                    const FString Key = PieceKey(Proc, nullptr);
                    for (int32 S = 0; S < Proc->GetNumSections(); ++S)
                    {
                        const FProcMeshSection* Sec = Proc->GetProcMeshSection(S);
                        if (!Sec || !Sec->bSectionVisible) continue;
                        const TArray<uint32>& Idx = Sec->ProcIndexBuffer;
                        for (int32 First = 0; First + 2 < Idx.Num(); First += 72)
                        {
                            FBox B(ForceInit);
                            for (int32 K = First; K < FMath::Min(First + 72, Idx.Num()); ++K)
                                if (Sec->ProcVertexBuffer.IsValidIndex(Idx[K])) B += T.TransformPosition(FVector(Sec->ProcVertexBuffer[Idx[K]].Position));
                            if (!B.IsValid) continue;
                            FPiece Piece; Piece.Comp = Proc; Piece.Key = Key; Piece.Bounds = B; Piece.C = B.GetCenter(); Piece.Half = B.GetExtent() + FVector(.5f);
                            Piece.Ax[0] = FVector::ForwardVector; Piece.Ax[1] = FVector::RightVector; Piece.Ax[2] = FVector::UpVector;
                            Piece.bAnchor = true;
                            Out.Add(Piece);
                        }
                    }
                    continue;
                }
                if (!SM || P->IsA<USplineMeshComponent>() || !SM->GetStaticMesh())
                {
                    // Landscape and spline surfaces: their world box is far bigger than the surface (a long rope's
                    // box), so they hold props only through their collision (Floating's overlap test).
                    continue;
                }
                const FBox Local = SM->GetStaticMesh()->GetBoundingBox();
                auto* ISM = Cast<UInstancedStaticMeshComponent>(SM);
                const int32 Count = ISM ? ISM->GetInstanceCount() : 1;
                const FString Key = PieceKey(SM, SM->GetStaticMesh());
                for (int32 I = 0; I < Count; ++I)
                {
                    FTransform T = SM->GetComponentTransform();
                    if (ISM && !ISM->GetInstanceTransform(I, T, true)) continue;
                    FPiece S; S.Comp = SM; S.Instance = I; S.Key = Key;
                    const FVector Scale = T.GetScale3D().GetAbs();
                    S.Half = Local.GetExtent() * Scale;
                    if (S.Half.GetMax() < 2.f) continue;
                    S.C = T.TransformPosition(Local.GetCenter());
                    const FQuat Q = T.GetRotation();
                    S.Ax[0] = Q.GetAxisX(); S.Ax[1] = Q.GetAxisY(); S.Ax[2] = Q.GetAxisZ();
                    S.Bounds = Local.TransformBy(T);
                    const float Max = S.Half.GetMax() * 2;
                    S.bAnchor = Max >= 300.f;
                    S.bCandidate = !S.bAnchor && Max >= 10.f;
                    Out.Add(S);
                }
            }
        }
    }

    // A world-aligned grid of the pieces, for neighbours; the very large go on a list every query sees.
    struct FGrid
    {
        static constexpr float Cell = 400.f;
        TMap<FIntVector, TArray<int32>> Cells;
        TArray<int32> Huge;
        static FIntVector Of(const FVector& P) { return FIntVector(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell), FMath::FloorToInt(P.Z / Cell)); }
        void Add(const FBox& B, int32 Index)
        {
            const FIntVector A = Of(B.Min), Z = Of(B.Max);
            if (int64(Z.X - A.X + 1) * (Z.Y - A.Y + 1) * (Z.Z - A.Z + 1) > 64) { Huge.Add(Index); return; }
            for (int32 X = A.X; X <= Z.X; ++X) for (int32 Y = A.Y; Y <= Z.Y; ++Y) for (int32 W = A.Z; W <= Z.Z; ++W) Cells.FindOrAdd(FIntVector(X, Y, W)).Add(Index);
        }
        template <typename F> void Near(const FBox& B, F&& Visit) const
        {
            const FIntVector A = Of(B.Min), Z = Of(B.Max);
            for (int32 X = A.X; X <= Z.X; ++X) for (int32 Y = A.Y; Y <= Z.Y; ++Y) for (int32 W = A.Z; W <= Z.Z; ++W)
                if (const TArray<int32>* L = Cells.Find(FIntVector(X, Y, W))) for (int32 I : *L) Visit(I);
            for (int32 I : Huge) Visit(I);
        }
    };

    struct FFind
    {
        TArray<int32> Up;
        explicit FFind(int32 N) { Up.SetNum(N); for (int32 I = 0; I < N; ++I) Up[I] = I; }
        int32 Root(int32 I) { while (Up[I] != I) { Up[I] = Up[Up[I]]; I = Up[I]; } return I; }
        void Join(int32 A, int32 B) { A = Root(A); B = Root(B); if (A != B) Up[A] = B; }
    };

    int32 CollisionGaps(UWorld* World, const TArray<FAllow>& Allow, int32& Checked, int32& AllowedCount)
    {
        int32 Gaps = 0;
        TMap<FString, int32> ByKey;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(WorldAuditCollision), false);
        const AActor* Chuck = UGameplayStatics::GetPlayerPawn(World, 0);
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
            for (UStaticMeshComponent* SM : Meshes)
            {
                if (!SM->GetStaticMesh() || !Shown(*It, SM, Chuck)) continue;
                // Walk-through: no collision, or collision that does not block a pawn (query-only props, overlap triggers).
                if (SM->GetCollisionEnabled() != ECollisionEnabled::NoCollision && SM->GetCollisionEnabled() != ECollisionEnabled::ProbeOnly &&
                    SM->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block) continue;
                const FBox Local = SM->GetStaticMesh()->GetBoundingBox();
                auto* ISM = Cast<UInstancedStaticMeshComponent>(SM);
                const int32 Count = ISM ? ISM->GetInstanceCount() : 1;
                const FString Key = PieceKey(SM, SM->GetStaticMesh());
                for (int32 I = 0; I < Count; ++I)
                {
                    FTransform T = SM->GetComponentTransform();
                    if (ISM && !ISM->GetInstanceTransform(I, T, true)) continue;
                    const FBox B = Local.TransformBy(T);
                    const FVector Size = B.GetSize();
                    // Big enough to walk into: over 25 cm each way (and not a whole hillside).
                    if (Size.X < 25 || Size.Y < 25 || Size.Z < 25 || Size.X > 3000 || Size.Y > 3000) continue;
                    FHitResult Floor;
                    const FVector C = B.GetCenter();
                    // The floor under it: down from its own centre (from above it, below decks, the trace met the deck overhead).
                    if (!World->LineTraceSingleByChannel(Floor, C, FVector(C.X, C.Y, B.Min.Z - 400), ECC_Pawn, Q))
                    {
                        if (Key.Contains(TEXT("AuditProbe"))) UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_PROBE gap no_floor at=%s"), *C.ToString());
                        continue;
                    }
                    const float Feet = static_cast<float>(Floor.ImpactPoint.Z);
                    if (B.Min.Z > Feet + 40.f || B.Max.Z < Feet + 20.f)
                    {
                        if (Key.Contains(TEXT("AuditProbe"))) UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_PROBE gap off_floor at=%s floor=%.0f hit=%s"), *C.ToString(), Feet, *PieceKey(Floor.GetComponent(), nullptr));
                        continue;
                    }
                    ++Checked;
                    // Something solid inside it (its shell, a hidden blocker): fine. Looked for twice: in its own box
                    // (shrunk to 60%), and in the band a rat walks into, its lowest 40 cm over the floor, across the
                    // middle 60% of its footprint (a tree's trunk collider is far from its box's centre up in the crown).
                    const FVector Half = Local.GetExtent() * T.GetScale3D().GetAbs() - FVector(4.f);
                    if (Half.GetMin() <= 1.f) continue;
                    if (World->OverlapBlockingTestByChannel(T.TransformPosition(Local.GetCenter()), T.GetRotation(), ECC_Pawn, FCollisionShape::MakeBox(Half * .6f), Q)) continue;
                    {
                        const float Z0 = FMath::Max(float(B.Min.Z), Feet + 2.f), Z1 = FMath::Min(float(B.Max.Z), Feet + 42.f);
                        if (Z1 > Z0 + 2.f && World->OverlapBlockingTestByChannel(FVector(C.X, C.Y, (Z0 + Z1) * .5f), FQuat::Identity, ECC_Pawn,
                            FCollisionShape::MakeBox(FVector(Size.X * .3f, Size.Y * .3f, (Z1 - Z0) * .5f)), Q)) continue;
                    }
                    {
                        // And as a rat meets it: four rays 20 cm over the floor, from just outside its box in to its
                        // middle. A blocker of triangle-mesh collision (the ship's hidden boxes) is found only so: an
                        // overlap wholly inside a closed mesh touches none of its triangles. Two stopped rays: solid.
                        const float Z = FMath::Clamp(Feet + 20.f, float(B.Min.Z) + 2.f, float(B.Max.Z) - 2.f);
                        const FVector Mid(C.X, C.Y, Z);
                        int32 Stopped = 0;
                        for (const FVector& Dir : {FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0)})
                        {
                            const float Out = FMath::Abs(Dir.X) * Size.X * .5f + FMath::Abs(Dir.Y) * Size.Y * .5f + 6.f;
                            FHitResult Ray;
                            if (World->LineTraceSingleByChannel(Ray, Mid + Dir * Out, Mid, ECC_Pawn, Q) && !Ray.bStartPenetrating) ++Stopped;
                        }
                        if (Stopped >= 2) continue;
                    }
                    if (Key.Contains(TEXT("AuditProbe"))) { ++GProbeGap; continue; }
                    if (Allowed(Allow, TEXT("collision"), Key)) { ++AllowedCount; continue; }
                    ++Gaps;
                    GRows.Add(FString::Printf(TEXT("gap,%s,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f"), *Key.Replace(TEXT(","), TEXT(";")), I, C.X, C.Y, C.Z, Size.X, Size.Y, Size.Z, C.Z - Feet));
                    if (++ByKey.FindOrAdd(Kind(Key)) <= 3)
                    {
                        UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_GAP %s instance=%d centre=%s size=%s floor=%.0f"), *Key, I, *C.ToString(), *Size.ToString(), Feet);
                        AddShot(TEXT("gap"), Gaps, C, Size.GetMax());
                    }
                }
            }
        }
        for (const auto& P : ByKey) UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_GAP_GROUP count=%d %s"), P.Value, *P.Key);
        return Gaps;
    }

    int32 Floating(UWorld* World, const TArray<FAllow>& Allow, int32& Groups, int32& AllowedCount)
    {
        TArray<FPiece> Pieces;
        Gather(World, Pieces);
        FGrid Grid;
        for (int32 I = 0; I < Pieces.Num(); ++I) Grid.Add(Pieces[I].Bounds, I);
        // Groups of small pieces that touch; a group is held if any piece of it touches an anchor.
        FFind Find(Pieces.Num());
        TArray<bool> Held; Held.Init(false, Pieces.Num());
        TArray<int32> HeldBy; HeldBy.Init(INDEX_NONE, Pieces.Num());
        constexpr float Tol = 3.f;
        for (int32 I = 0; I < Pieces.Num(); ++I)
        {
            const FPiece& A = Pieces[I];
            if (!A.bCandidate) continue;
            const FBox Search = A.Bounds.ExpandBy(Tol);
            Grid.Near(Search, [&](int32 J)
            {
                if (J == I) return;
                const FPiece& B = Pieces[J];
                if (!B.Bounds.Intersect(Search) || !Overlaps(A, B, Tol)) return;
                // A piece wholly inside a big box (the sky dome, a hull or cave shell) is not held by it: only one that
                // reaches its surface is.
                if (B.bAnchor) { if (!Inside(A, B, 6.f)) { if (!Held[I]) HeldBy[I] = J; Held[I] = true; } }
                else if (B.bCandidate || B.Half.GetMax() >= 2.f) Find.Join(I, J);
            });
        }
        TMap<int32, TArray<int32>> Members;
        for (int32 I = 0; I < Pieces.Num(); ++I) if (Pieces[I].bCandidate) Members.FindOrAdd(Find.Root(I)).Add(I);
        int32 Count = 0;
        TMap<FString, int32> ByKey;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(WorldAuditFloat), false);
        for (auto& G : Members)
        {
            bool bHeld = false;
            for (int32 I : G.Value) bHeld |= Held[I];
            if (bHeld)
            {
                for (int32 I : G.Value) if (Pieces[I].Key.Contains(TEXT("AuditProbeFloat")))
                    for (int32 J : G.Value) if (Held[J]) { UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_PROBE float held_by=%s"), *Pieces[HeldBy[J]].Key); break; }
                continue;
            }
            // Something solid it touches that is not one of its own pieces (hidden blockers, collision-only floors).
            TSet<const UPrimitiveComponent*> Own;
            for (int32 I : G.Value) Own.Add(Pieces[I].Comp);
            for (int32 I : G.Value)
            {
                const FPiece& P = Pieces[I];
                TArray<FOverlapResult> Hits;
                World->OverlapMultiByObjectType(Hits, P.C, FQuat(FMatrix(P.Ax[0], P.Ax[1], P.Ax[2], FVector::ZeroVector)),
                    FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic)), FCollisionShape::MakeBox(P.Half + FVector(Tol)), Q);
                for (const FOverlapResult& H : Hits) if (H.GetComponent() && !Own.Contains(H.GetComponent()))
                {
                    bHeld = true;
                    if (P.Key.Contains(TEXT("AuditProbeFloat"))) UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_PROBE float held_by_collision=%s"), *PieceKey(H.GetComponent(), nullptr));
                    break;
                }
                if (bHeld) break;
            }
            if (bHeld) continue;
            // The group's biggest piece names it.
            int32 Big = G.Value[0];
            for (int32 I : G.Value) if (Pieces[I].Half.GetMax() > Pieces[Big].Half.GetMax()) Big = I;
            const FPiece& P = Pieces[Big];
            if (P.Key.Contains(TEXT("AuditProbe"))) { ++GProbeFloat; continue; }
            if (Allowed(Allow, TEXT("float"), P.Key)) { ++AllowedCount; continue; }
            ++Count;
            // How far it hangs above what is under it.
            FHitResult Under;
            const float Bottom = P.Bounds.Min.Z;
            const bool bUnder = World->LineTraceSingleByChannel(Under, FVector(P.C.X, P.C.Y, Bottom - 1), FVector(P.C.X, P.C.Y, Bottom - 5000), ECC_Visibility, Q);
            GRows.Add(FString::Printf(TEXT("float,%s,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.1f"), *P.Key.Replace(TEXT(","), TEXT(";")), P.Instance, P.C.X, P.C.Y, P.C.Z, P.Half.X * 2, P.Half.Y * 2, P.Half.Z * 2,
                bUnder ? Bottom - Under.ImpactPoint.Z : -1.0));
            if (++ByKey.FindOrAdd(Kind(P.Key)) <= 3)
                { UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_FLOAT %s instance=%d pieces=%d centre=%s size=%s above=%s"), *P.Key, P.Instance, G.Value.Num(),
                    *P.C.ToString(), *(P.Half * 2).ToString(), bUnder ? *FString::Printf(TEXT("%.1f"), Bottom - Under.ImpactPoint.Z) : TEXT("nothing")); AddShot(TEXT("float"), Count, P.C, P.Half.GetMax() * 2); }
        }
        Groups = Members.Num();
        for (const auto& P : ByKey) UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_FLOAT_GROUP count=%d %s"), P.Value, *P.Key);
        // (Groups is the number of touching groups of small pieces measured, held or not.)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_PIECES pieces=%d"), Pieces.Num());
        return Count;
    }

    void Run(UWorld* World, const FString& Region)
    {
        const TArray<FAllow> Allow = LoadAllow();
        GWorld_ = World;
        const double Start = FPlatformTime::Seconds();
        AActor* Probes = PlantProbes(World);
        int32 Checked = 0, AllowedGaps = 0, Groups = 0, AllowedFloats = 0;
        const int32 Gaps = CollisionGaps(World, Allow, Checked, AllowedGaps);
        const int32 Floats = Floating(World, Allow, Groups, AllowedFloats);
        if (Probes) Probes->Destroy();
        {
            GRows.Insert(TEXT("check,key,instance,x,y,z,sx,sy,sz,above"), 0);
            FFileHelper::SaveStringArrayToFile(GRows, *(FPaths::ProjectSavedDir() / TEXT("Audit") / Region + TEXT("-findings.csv")));
        }
        if (GShots.Num())
        {
            GShots.Insert(TEXT("fov 60"), 0);
            GShots.Insert(FString::Printf(TEXT("at %s"), *Region), 0);
            GShots.Insert(FString::Printf(TEXT("# -ChuckWorldAudit findings, %s (WorldAudit.cpp); Tools/Take-Shots.ps1 this file"), *FDateTime::Now().ToString()), 0);
            const FString File = FPaths::ProjectSavedDir() / TEXT("Audit") / Region + TEXT("-findings.txt");
            FFileHelper::SaveStringArrayToFile(GShots, *File);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_SHOTS %d %s"), GShots.Num() - 3, *FPaths::ConvertRelativePathToFull(File));
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT region=%s collision_checked=%d gaps=%d gaps_allowed=%d groups=%d floating=%d floating_allowed=%d probes_found=%d/2 probe_gap=%d probe_float=%d allow_lines=%d ms=%.0f"),
            *Region, Checked, Gaps, AllowedGaps, Groups, Floats, AllowedFloats, FMath::Min(GProbeGap, 1) + FMath::Min(GProbeFloat, 1), GProbeGap, GProbeFloat, Allow.Num(), (FPlatformTime::Seconds() - Start) * 1000);
        // The old Waterdeep line, for anything still reading it.
        if (Region == TEXT("Waterdeep")) UE_LOG(LogTemp, Display, TEXT("CHUCK_COLLISION_AUDIT checked=%d gaps=%d"), Checked, Gaps);
        FTimerHandle Quit;
        World->GetTimerManager().SetTimer(Quit, [](){ FPlatformMisc::RequestExit(false); }, 1.f, false);
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
        Run(World, Region);
    }
}

// Called by the game mode with the checkpoint already started (or Waterdeep).
void StartWorldAudit(UWorld* World, const FString& Region)
{
    UE_LOG(LogTemp, Display, TEXT("CHUCK_WORLD_AUDIT_START region=%s"), *Region);
    WaitThenRun(World, Region, World->GetTimeSeconds());
}
