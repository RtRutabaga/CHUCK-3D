#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChuckReviewLibrary.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UGroomAsset;
class UGroomBindingAsset;
class UMaterialInterface;
class USkeletalMesh;

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
    /** Each LOD's sections and their materials (slot names, as drawn: through the LOD's material map), and with bFix
     *  each generated LOD's map set so its sections draw with the material of the LOD 0 section they came from
     *  (2026-11-04: the heroes' reduced LODs drew the wrong materials - the wizard's skirt white, the fighter's beard
     *  shade as mail). Returns the report (before, and after when fixed). */
    UFUNCTION(BlueprintCallable, Category="Chuck|Review", meta=(DevelopmentOnly))
    static FString FixLODMaterials(USkeletalMesh* Mesh, bool bFix);
};
