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

    // Hair voxelization (r.HairStrands.Voxelization) is off for the town's frame rate (DefaultEngine.ini). Without
    // it, strands under a light that casts no shadow are lit right through, so dark hair reads grey and red hair
    // pale over its dark roots (the temple heroes by the sanctum's torches and the beholder's glow). An actor
    // whose hair must stay true calls Want each tick from where its hair is: voxelization is on while any such
    // hair is within the grooms' draw distance of the camera. Release from its EndPlay.
    void Want(const AActor* Owner, const FVector& HairAt);
    void Release();
}
