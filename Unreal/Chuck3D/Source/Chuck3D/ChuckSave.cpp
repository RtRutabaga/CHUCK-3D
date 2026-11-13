#include "ChuckSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

FString ChuckSave::SlotPath()
{
    FString Slot = TEXT("Chuck");
    FParse::Value(FCommandLine::Get(), TEXT("ChuckSaveSlot="), Slot);
    return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("CHUCK3D"), TEXT("SaveGames"), Slot + TEXT(".sav"));
}

bool ChuckSave::Exists()
{
    return IFileManager::Get().FileExists(*SlotPath());
}

UChuckSaveGame* ChuckSave::Load()
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *SlotPath())) return nullptr;
    UChuckSaveGame* Save = Cast<UChuckSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!Save || Save->Point.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("CHUCK_SAVE_UNREADABLE path=%s"), *SlotPath());
        return nullptr;
    }
    return Save;
}

bool ChuckSave::Write(const FString& Point, const FString& Label, bool bEvening, int32 Cigarettes, int32 Sanity)
{
    UChuckSaveGame* Save = Cast<UChuckSaveGame>(UGameplayStatics::CreateSaveGameObject(UChuckSaveGame::StaticClass()));
    Save->Point = Point; Save->Label = Label; Save->bEvening = bEvening; Save->Cigarettes = Cigarettes; Save->Sanity = Sanity;
    Save->SavedAt = FDateTime::Now();
    TArray<uint8> Bytes;
    const FString Path = SlotPath();
    // Written beside, then moved over the old one, so a crash mid-write never loses the last good save.
    const FString Temp = Path + TEXT(".tmp");
    const bool bOk = UGameplayStatics::SaveGameToMemory(Save, Bytes) && FFileHelper::SaveArrayToFile(Bytes, *Temp)
        && IFileManager::Get().Move(*Path, *Temp, true, true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SAVE_WRITTEN ok=%d point=%s evening=%d cigarettes=%d path=%s"), bOk ? 1 : 0, *Point, bEvening ? 1 : 0, Cigarettes, *Path);
    return bOk;
}
