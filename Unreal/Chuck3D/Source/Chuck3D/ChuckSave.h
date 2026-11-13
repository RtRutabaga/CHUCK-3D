#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ChuckSave.generated.h"

// The player's save (user 2026-11-13: "a save option in a pause menu"). One slot. It records the checkpoint
// Chuck last reached (his death-recovery point, matched to the nearest named checkpoint, the same ones the dev
// checkpoint page starts from), the docks' morning or evening, the cigarette count and his sanity. Continue on the title
// menu starts from it exactly as that checkpoint does.
//
// The file lives outside the build, under %LOCALAPPDATA%/CHUCK3D/SaveGames, so promoting a new build
// (which moves the old build folder aside) keeps it. -ChuckSaveSlot=<name> uses another slot (the tests).
UCLASS()
class CHUCK3D_API UChuckSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString Point;        // a checkpoint name, as ADockGameMode::StartFromMenu takes
    UPROPERTY() FString Label;        // what the menus show ("Chult Bridge")
    UPROPERTY() bool bEvening = false;
    UPROPERTY() int32 Cigarettes = 0;
    UPROPERTY() int32 Sanity = 5;
    UPROPERTY() FDateTime SavedAt;
};

namespace ChuckSave
{
    FString SlotPath();
    bool Exists();
    UChuckSaveGame* Load();
    bool Write(const FString& Point, const FString& Label, bool bEvening, int32 Cigarettes, int32 Sanity);
}
