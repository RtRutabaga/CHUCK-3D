#include "VaultCrates.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

namespace
{
    TArray<FVaultCrate> Crates;
}

const TArray<FVaultCrate>& GetVaultCrates() { return Crates; }

void SpawnVaultCrates(UWorld* World)
{
    // By the spawn on the quay, on the plaza's west side, in the court by the
    // tavern, and in the north end of Dock Street.
    Crates = {
        { FVector(-200, -300, 0), FVector(0, 1, 0), 45.f },
        { FVector(-300, -2600, 0), FVector(1, 0, 0), 42.f },
        { FVector(-300, 2100, 0), FVector(0, 1, 0), 48.f },
        { FVector(-800, 3700, 0), FVector(1, 0, 0), 40.f },
    };
    auto* CrateMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (const FVaultCrate& C : Crates)
    {
        // 60 cm across the run, 40 cm along it.
        const float Yaw = C.Run.X != 0.f ? 90.f : 0.f;
        const FVector Size(60.f, 40.f, C.Height);
        auto* Solid = World->SpawnActor<AStaticMeshActor>(C.Centre + FVector(0, 0, C.Height * .5f), FRotator(0, Yaw, 0));
        if (!Solid) continue;
        Solid->Tags.Add(TEXT("VaultCrate"));
        UStaticMeshComponent* Box = Solid->GetStaticMeshComponent();
        Box->SetMobility(EComponentMobility::Movable);
        Box->SetStaticMesh(Cube);
        Box->SetRelativeScale3D(Size / 100.f);
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetVisibility(!CrateMesh);   // the art carries the look; this is the collision
        Box->SetCastShadow(false);
        if (CrateMesh)
        {
            // The dock crate (60 x 65 x 60) squashed to a low one, its centre at the box's.
            auto* Art = NewObject<UStaticMeshComponent>(Solid);
            Art->SetupAttachment(Solid->GetRootComponent());
            Art->SetStaticMesh(CrateMesh);
            Art->SetCollisionProfileName(TEXT("NoCollision"));
            Art->SetUsingAbsoluteScale(true);
            Art->SetWorldScale3D(FVector(60.f / 60.f, 40.f / 65.f, C.Height / 60.f));
            Art->SetWorldLocationAndRotation(C.Centre + FVector(0, 0, C.Height * .5f), FRotator(0, Yaw, 0));
            Art->RegisterComponent();
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_VAULT_CRATES placed=%d"), Crates.Num());
}
