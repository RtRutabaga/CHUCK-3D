#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultLanding.generated.h"

// Setting contract: same floor, origin, facing and near trees as FallCutscene.
// This first pass is ground exploration; canopy traversal belongs farther in.
UCLASS()
class CHUCK3D_API AChultLanding : public AActor
{
    GENERATED_BODY()
public:
    AChultLanding();
    static FVector FloorOrigin();
    static FVector Entrance();
    static bool Contains(const FVector& Position);
    static AChultLanding* EnterPreview(UWorld* World);
    void Build();
    void VerifyAndCapture();
};
