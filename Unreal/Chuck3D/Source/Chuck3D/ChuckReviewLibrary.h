#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChuckReviewLibrary.generated.h"

class UPoseableMeshComponent;

/** Explicit refresh for isolated editor asset reviews, which have no game tick. */
UCLASS()
class CHUCK3D_API UChuckReviewLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool RefreshEditorPose(UPoseableMeshComponent* Component);
};
