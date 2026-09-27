#include "ChuckReviewLibrary.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "HairStrandsDatas.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

bool UChuckReviewLibrary::RefreshEditorPose(UPoseableMeshComponent* Component)
{
#if WITH_EDITOR
    if (IsValid(Component) && Component->GetWorld() && !Component->GetWorld()->IsGameWorld())
    {
        Component->RefreshBoneTransforms();
        return true;
    }
#endif
    return false;
}

bool UChuckReviewLibrary::SetEditorAnimationPose(USkeletalMeshComponent* Component, UAnimSequence* Clip, float Time)
{
#if WITH_EDITOR
    if (IsValid(Component) && Clip && Component->GetWorld() && !Component->GetWorld()->IsGameWorld())
    {
        Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Component->SetAnimation(Clip);
        Component->SetPosition(Time, false);
        Component->SetPlayRate(0);
        Component->TickAnimation(0, false);
        Component->RefreshBoneTransforms();
        return true;
    }
#endif
    return false;
}

bool UChuckReviewLibrary::SetEditorComponentPose(USkeletalMeshComponent* Component, const TArray<FName>& Bones,
    const TArray<FTransform>& ComponentSpace)
{
#if WITH_EDITOR
    // Writes an evaluated pose straight into the skeletal mesh's bone buffer.
    // Unlike SetEditorAnimationPose this does not depend on an anim instance
    // updating in a world that never ticks, and it keeps a real
    // USkeletalMeshComponent, which groom bindings require.
    if (!IsValid(Component) || !Component->GetWorld() || Component->GetWorld()->IsGameWorld() ||
        Bones.Num() != ComponentSpace.Num())
    {
        return false;
    }
    Component->SetUpdateAnimationInEditor(false);
    Component->bNoSkeletonUpdate = true;  // keep animation evaluation from overwriting the pose
    // Single buffering makes the editable transforms the ones read and rendered.
    Component->SetComponentSpaceTransformsDoubleBuffering(false);
    TArray<FTransform>& Space = Component->GetEditableComponentSpaceTransforms();
    for (int32 Index = 0; Index < Bones.Num(); ++Index)
    {
        const int32 Bone = Component->GetBoneIndex(Bones[Index]);
        if (Bone == INDEX_NONE || !Space.IsValidIndex(Bone))
        {
            return false;
        }
        Space[Bone] = ComponentSpace[Index];
    }
    Component->FinalizeBoneTransform();  // public: flips the edited buffer into use
    Component->InvalidateCachedBounds();
    Component->UpdateBounds();
    Component->MarkRenderTransformDirty();
    Component->MarkRenderDynamicDataDirty();
    return true;
#else
    return false;
#endif
}

bool UChuckReviewLibrary::ConfigureGroomMaterial(UGroomAsset* Groom, UMaterialInterface* Material, FName Slot)
{
#if WITH_EDITOR
    if (Groom && Material && Groom->GetHairGroupsRendering().Num() == 1)
    {
        TArray<FHairGroupsMaterial> Materials;
        FHairGroupsMaterial Entry;
        Entry.Material = Material;
        Entry.SlotName = Slot;
        Materials.Add(Entry);
        Groom->SetHairGroupsMaterials(Materials);
        TArray<FHairGroupsRendering> Rendering = Groom->GetHairGroupsRendering();
        Rendering[0].MaterialSlotName = Slot;
        Groom->SetHairGroupsRendering(Rendering);
        Groom->MarkPackageDirty();
        return true;
    }
#endif
    return false;
}

bool UChuckReviewLibrary::RebuildGroomBinding(UGroomBindingAsset* Binding)
{
#if WITH_EDITOR
    if (Binding)
    {
        // Build() clears bulk data even when its cached key is unchanged in 5.7.
        // InvalidateBinding checks dependencies/keys without destroying valid data.
        Binding->SetNumInterpolationPoints(33);
        Binding->InvalidateBinding();
        FAssetCompilingManager::Get().FinishAllCompilation();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_GROOM_BINDING groups=%d render=%d guides=%d"), Binding->GetGroupInfos().Num(),
            Binding->GetGroupInfos().Num() ? Binding->GetGroupInfos()[0].RenRootCount : -1,
            Binding->GetGroupInfos().Num() ? Binding->GetGroupInfos()[0].SimRootCount : -1);
        return Binding->GetGroupInfos().Num() == 1 && Binding->GetGroupInfos()[0].RenRootCount > 0;
    }
#endif
    return false;
}

TArray<FVector> UChuckReviewLibrary::SampleGroomRoots(UGroomAsset* Groom)
{
    TArray<FVector> Roots;
#if WITH_EDITOR
    FHairStrandsDatas Strands, Guides;
    if (Groom && Groom->GetHairStrandsDatas(0, Strands, Guides))
    {
        const uint32 Step = FMath::Max(1u, Strands.GetNumCurves() / 234u);
        for (uint32 Index = 0; Index < Strands.GetNumCurves(); Index += Step)
        {
            Roots.Add(FVector(Strands.StrandsPoints.PointsPosition[Strands.StrandsCurves.CurvesOffset[Index]]));
        }
    }
#endif
    return Roots;
}
