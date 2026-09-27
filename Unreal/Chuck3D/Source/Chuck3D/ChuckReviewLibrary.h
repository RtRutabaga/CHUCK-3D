#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChuckReviewLibrary.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UGroomAsset;
class UGroomBindingAsset;
class UMaterialInterface;

/** Explicit refresh for isolated editor asset reviews, which have no game tick. */
UCLASS()
class CHUCK3D_API UChuckReviewLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool RefreshEditorPose(UPoseableMeshComponent* Component);
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool SetEditorAnimationPose(USkeletalMeshComponent* Component, UAnimSequence* Clip, float Time);
    /** Apply an evaluated pose (component-space transforms per bone name) in an editor review world. */
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool SetEditorComponentPose(USkeletalMeshComponent* Component, const TArray<FName>& Bones,
        const TArray<FTransform>& ComponentSpace);
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool ConfigureGroomMaterial(UGroomAsset* Groom, UMaterialInterface* Material, FName Slot);
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static bool RebuildGroomBinding(UGroomBindingAsset* Binding);
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static TArray<FVector> SampleGroomRoots(UGroomAsset* Groom);
};
