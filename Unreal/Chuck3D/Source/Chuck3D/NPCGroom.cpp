#include "NPCGroom.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Actor.h"
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
        // Strands are costly: drawn only within 25 m (every haired NPC drew from anywhere on the map, which
        // slowed the temple's frames).
        static const float Cull=[]{ float V=2500.f; FParse::Value(FCommandLine::Get(),TEXT("ChuckGroomCull="),V); return V; }();   // -ChuckGroomCull=<cm> to test
        Hair->SetCullDistance(Cull);
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
