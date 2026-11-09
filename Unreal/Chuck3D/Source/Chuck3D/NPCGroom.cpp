#include "NPCGroom.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"

namespace
{
    FTransform RefComponentSpace(const FReferenceSkeleton& Ref, int32 Index)
    {
        FTransform T = FTransform::Identity;
        for (; Index != INDEX_NONE; Index = Ref.GetParentIndex(Index)) T = T * Ref.GetRefBonePose()[Index];
        return T;
    }

    // Voxels: wanted this frame by any caller (gathered over one frame, applied at the first call of the next).
    uint64 VoxFrame = 0;
    bool bVoxWanted = false, bVoxOn = false;
    // Strands are costly: drawn only within 25 m (every haired NPC drew from anywhere on the map, which
    // slowed the temple's frames).
    float GroomCull()
    {
        static const float V = []{ float C = 2500.f; FParse::Value(FCommandLine::Get(), TEXT("ChuckGroomCull="), C); return C; }();   // -ChuckGroomCull=<cm> to test
        return V;
    }
    void SetVoxels(bool bOn)
    {
        if (bOn == bVoxOn) return;
        bVoxOn = bOn;
        if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Voxelization"))) V->Set(bOn ? 1 : 0, ECVF_SetByCode);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_GROOM_VOXELS on=%d"), bOn ? 1 : 0);
    }
}

int32 ChuckGroom::Attach(AActor* Owner, USkinnedMeshComponent* Mesh, TArray<UGroomComponent*>& Out)
{
    const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
    if (!Owner || !Asset) return 0;
    // SK_TavernKeeper in .../Humans/TavernKeeper, SK_HeroFighter in .../Heroes/Fighter: the grooms are GR_<folder>_<group>.
    const FString Folder = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    const FString Name = FPaths::GetCleanFilename(Folder);
    const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
    static const TCHAR* Groups[] = { TEXT("Hair"), TEXT("Beard"), TEXT("BeardJaw"), TEXT("Braid"), TEXT("HairBack"), TEXT("BraidBack") };
    int32 Count = 0;
    for (const TCHAR* Group : Groups)
    {
        const FString Path = FString::Printf(TEXT("%s/GR_%s_%s.GR_%s_%s"), *Folder, *Name, Group, *Name, Group);
        UGroomAsset* Groom = LoadObject<UGroomAsset>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Groom) continue;
        const FString G(Group);
        const FName Bone = G.EndsWith(TEXT("Jaw")) ? FName(TEXT("jaw")) : G.EndsWith(TEXT("Back")) ? FName(TEXT("spine_03")) : FName(TEXT("head"));
        const int32 Index = Ref.FindBoneIndex(Bone);
        if (Index == INDEX_NONE) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_GROOM_NO_BONE %s %s"), *Name, *Bone.ToString()); continue; }
        auto* Hair = NewObject<UGroomComponent>(Owner, *FString::Printf(TEXT("Groom_%s"), Group));
        Hair->SimulationSettings.bOverrideSettings = true;
        Hair->SimulationSettings.SolverSettings.bEnableSimulation = false;
        Hair->GroomAsset = Groom;
        Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Hair->SetCullDistance(GroomCull());
        Hair->SetupAttachment(Mesh, Bone);
        // Authored in the mesh's rest pose: undo the bone's rest transform, then it rides on the bone.
        Hair->SetRelativeTransform(RefComponentSpace(Ref, Index).Inverse());
        Hair->RegisterComponent();
        Owner->AddInstanceComponent(Hair);
        Out.Add(Hair);
        ++Count;
    }
    if (Count) UE_LOG(LogTemp, Display, TEXT("CHUCK_GROOM_ATTACHED %s grooms=%d"), *Name, Count);
    return Count;
}

void ChuckGroom::Want(const AActor* Owner, const FVector& HairAt)
{
    if (GFrameCounter != VoxFrame) { if (VoxFrame) SetVoxels(bVoxWanted); bVoxWanted = false; VoxFrame = GFrameCounter; }
    const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->PlayerCameraManager) return;
    // A little past the draw distance, so it is on before the hair comes into view.
    if (FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), HairAt) < FMath::Square(GroomCull() + 500.f)) bVoxWanted = true;
}

void ChuckGroom::Release()
{
    SetVoxels(false); bVoxWanted = false; VoxFrame = 0;
}
