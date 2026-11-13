// -ChuckCollisionAudit (Claude, 2026-11-12; user: "along the wall by the sewer there's a section of stone with no
// collision"): every visible box of stone, plaster or wood in Waterdeep that a rat on the ground could walk into but
// that has no collision of its own and no solid shape inside it. Logs each (CHUCK_COLLISION_AUDIT_GAP) and a total;
// then quits. Reads the scene only.
#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"

void StartWaterdeepCollisionAudit(UWorld* World)
{
    int32 Checked = 0, Gaps = 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CollisionAudit), false);
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        for (UActorComponent* AC : It->GetComponents())
        {
            auto* SM = Cast<UStaticMeshComponent>(AC);
            if (!SM || !SM->GetStaticMesh() || !SM->IsVisible() || SM->bHiddenInGame || SM->GetCollisionEnabled() != ECollisionEnabled::NoCollision) continue;
            const FString Mat = SM->GetMaterial(0) ? SM->GetMaterial(0)->GetName() : FString();
            if (!(Mat.Contains(TEXT("Stone")) || Mat.Contains(TEXT("Plaster")) || Mat.Contains(TEXT("Wood")) || Mat.Contains(TEXT("Brick")))) continue;
            const FBox Local = SM->GetStaticMesh()->GetBoundingBox();
            auto* ISM = Cast<UInstancedStaticMeshComponent>(SM);
            const int32 Count = ISM ? ISM->GetInstanceCount() : 1;
            for (int32 I = 0; I < Count; ++I)
            {
                FTransform T = SM->GetComponentTransform();
                if (ISM) ISM->GetInstanceTransform(I, T, true);
                const FBox B = Local.TransformBy(T);
                const FVector Size = B.GetSize();
                // Big enough to walk into and low enough to reach: over 25 cm each way, its foot within 40 cm of a floor.
                if (Size.X < 25 || Size.Y < 25 || Size.Z < 25 || Size.X > 3000 || Size.Y > 3000) continue;
                FHitResult Floor;
                const FVector C = B.GetCenter();
                if (!World->LineTraceSingleByChannel(Floor, FVector(C.X, C.Y, B.Max.Z + 400), FVector(C.X, C.Y, B.Min.Z - 400), ECC_Pawn, Q)) continue;
                ++Checked;
                // Something solid inside it (its shell, a hidden blocker): fine.
                const FVector Half = Size * .5f - FVector(4.f);
                if (Half.GetMin() <= 1.f) continue;
                if (World->OverlapBlockingTestByChannel(C, T.GetRotation(), ECC_Pawn, FCollisionShape::MakeBox(Half * .6f), Q)) continue;
                const float Feet = static_cast<float>(Floor.ImpactPoint.Z);
                if (B.Min.Z > Feet + 40.f || B.Max.Z < Feet + 20.f) continue;
                ++Gaps;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_COLLISION_AUDIT_GAP actor=%s comp=%s mat=%s instance=%d centre=%s size=%s floor=%.0f"),
                    *It->GetName(), *SM->GetName(), *Mat, I, *C.ToString(), *Size.ToString(), Feet);
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_COLLISION_AUDIT checked=%d gaps=%d"), Checked, Gaps);
    FTimerHandle Quit;
    World->GetTimerManager().SetTimer(Quit, [](){ FPlatformMisc::RequestExit(false); }, 1.f, false);
}
