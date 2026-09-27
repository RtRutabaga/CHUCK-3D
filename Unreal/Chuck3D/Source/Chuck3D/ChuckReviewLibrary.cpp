#include "ChuckReviewLibrary.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/World.h"

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
