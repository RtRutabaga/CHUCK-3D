#pragma once
#include "CoreMinimal.h"

class AActor;
class USkinnedMeshComponent;
class UGroomComponent;

// Strand hair of the humans and heroes (Tools/npc_strands.py, Tools/import_npc_grooms.py):
// GR_<Name>_<Group> beside the mesh, authored in the mesh's rest-pose component space. Each is
// attached rigidly to its bone (the NPCs are posed with a PoseableMeshComponent, which a groom
// binding cannot follow): Hair/Beard to the head, *Jaw to the jaw (a beard below the lips opens
// with the mouth), *Back to the upper spine (a braid down the back).
namespace ChuckGroom
{
    // Returns how many grooms were attached (0 for a human without strand hair).
    int32 Attach(AActor* Owner, USkinnedMeshComponent* Mesh, TArray<UGroomComponent*>& Out);
}
