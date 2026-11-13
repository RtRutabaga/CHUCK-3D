#include "ChuckPlacements.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

int32 ChuckPlacements::Spawn(UWorld* World, const TCHAR* Region)
{
    if (!World) return 0;
    const FString File = FPaths::ProjectContentDir() / TEXT("Data/Placements") / FString(Region) + TEXT(".txt");
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *File))
    {
        UE_LOG(LogTemp, Warning, TEXT("CHUCK_PLACEMENTS_MISSING %s"), *File);
        return 0;
    }
    AActor* Owner = World->SpawnActor<AActor>();
    Owner->Tags.Add(*FString::Printf(TEXT("Placements_%s"), Region));
    auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    int32 Placed = 0, Bad = 0;
    for (int32 I = 0; I < Lines.Num(); ++I)
    {
        FString Line = Lines[I];
        int32 Hash; if (Line.FindChar(TEXT('#'), Hash)) Line.LeftInline(Hash);
        TArray<FString> W; Line.ParseIntoArrayWS(W);
        if (!W.Num()) continue;
        if (W[0] != TEXT("prop") || W.Num() < 6) { ++Bad; UE_LOG(LogTemp, Warning, TEXT("CHUCK_PLACEMENT_BAD_LINE %s:%d"), Region, I + 1); continue; }
        FString Path = W[1];
        if (!Path.Contains(TEXT("."))) Path += TEXT(".") + FPaths::GetBaseFilename(Path);
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!Mesh) { ++Bad; UE_LOG(LogTemp, Warning, TEXT("CHUCK_PLACEMENT_NO_MESH %s:%d %s"), Region, I + 1, *Path); continue; }
        const FVector At(FCString::Atof(*W[2]), FCString::Atof(*W[3]), FCString::Atof(*W[4]));
        const FRotator Rot(0, FCString::Atof(*W[5]), 0);
        float Scale = 1.f; FVector Cyl = FVector::ZeroVector, Box = FVector::ZeroVector; FString Tag;
        for (int32 K = 6; K < W.Num(); ++K)
        {
            FString Key, Value;
            if (!W[K].Split(TEXT("="), &Key, &Value)) continue;
            TArray<FString> N; Value.ParseIntoArray(N, TEXT("x"));
            if (Key == TEXT("scale")) Scale = FCString::Atof(*Value);
            else if (Key == TEXT("cyl") && N.Num() == 2) Cyl = FVector(FCString::Atof(*N[0]), FCString::Atof(*N[0]), FCString::Atof(*N[1]));
            else if (Key == TEXT("box") && N.Num() == 3) Box = FVector(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2]));
            else if (Key == TEXT("tag")) Tag = Value;
        }
        auto* Art = NewObject<UStaticMeshComponent>(Owner);
        Art->SetupAttachment(Root); Art->SetStaticMesh(Mesh);
        Art->SetRelativeTransform(FTransform(Rot, At, FVector(Scale)));
        Art->SetCollisionProfileName(TEXT("NoCollision"));
        if (!Tag.IsEmpty()) Art->ComponentTags.Add(*Tag);
        Art->RegisterComponent();
        const FVector Proxy = !Cyl.IsZero() ? Cyl : Box;
        if (!Proxy.IsZero())
        {
            auto* Solid = NewObject<UStaticMeshComponent>(Owner);
            Solid->SetupAttachment(Root); Solid->SetStaticMesh(!Cyl.IsZero() ? Cylinder : Cube);
            Solid->SetRelativeTransform(FTransform(Rot, At + FVector(0, 0, Proxy.Z * .5f), Proxy / 100.f));
            Solid->SetCollisionProfileName(TEXT("BlockAll")); Solid->SetVisibility(false);
            if (!Tag.IsEmpty()) Solid->ComponentTags.Add(*Tag);
            Solid->RegisterComponent();
        }
        ++Placed;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PLACEMENT %s %s at=%s yaw=%.0f solid=%d"), Region, Tag.IsEmpty() ? *FPaths::GetBaseFilename(Path) : *Tag, *At.ToString(), Rot.Yaw, Proxy.IsZero() ? 0 : 1);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PLACEMENTS_READY region=%s placed=%d bad=%d"), Region, Placed, Bad);
    return Placed;
}
