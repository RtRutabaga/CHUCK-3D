#include "ChuckShots.h"
#include "ChuckCharacter.h"
#include "DockGameMode.h"
#include "SewerSlide.h"
#include "LightAudit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    struct FShot
    {
        FString Name;
        FVector Eye, Aim;
        float Fov = 75.f, Wait = 2.f;
        bool bHud = false, bPlaceChuck = false, bAudit = false;
    };

    bool Vectors(const TArray<FString>& W, int32 From, FVector& A, FVector& B)
    {
        if (W.Num() < From + 6) return false;
        A = FVector(FCString::Atof(*W[From]), FCString::Atof(*W[From + 1]), FCString::Atof(*W[From + 2]));
        B = FVector(FCString::Atof(*W[From + 3]), FCString::Atof(*W[From + 4]), FCString::Atof(*W[From + 5]));
        return true;
    }
}

bool ChuckShots::Start(AHUD* Hud)
{
    FString File;
    if (!Hud || !FParse::Value(FCommandLine::Get(), TEXT("ChuckShots="), File)) return false;
    UWorld* World = Hud->GetWorld();
    if (FPaths::IsRelative(File)) File = FPaths::Combine(FPaths::ProjectDir(), TEXT("../.."), File);   // relative to the repository
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *File))
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_SHOTS_COMPLETE failures=1 missing=%s"), *File);
        if (APlayerController* PC = World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
        return true;
    }
    FString At;
    bool bEvening = false;
    TArray<FShot> Shots;
    FShot Next;
    int32 Errors = 0;
    for (int32 I = 0; I < Lines.Num(); ++I)
    {
        FString Line = Lines[I];
        int32 Hash; if (Line.FindChar(TEXT('#'), Hash)) Line.LeftInline(Hash);
        TArray<FString> W; Line.ParseIntoArrayWS(W);
        if (!W.Num()) continue;
        const FString Op = W[0].ToLower();
        if (Op == TEXT("at") && W.Num() > 1) At = W[1];
        else if (Op == TEXT("evening")) bEvening = true;
        else if (Op == TEXT("audit")) { FShot A = Next; A.bAudit = true; A.Name = TEXT("audit"); Shots.Add(A); Next.Wait = 2.f; }
        else if (Op == TEXT("wait") && W.Num() > 1) Next.Wait = FCString::Atof(*W[1]);
        else if (Op == TEXT("fov") && W.Num() > 1) Next.Fov = FCString::Atof(*W[1]);
        else if (Op == TEXT("hud") && W.Num() > 1) Next.bHud = W[1].ToLower() == TEXT("on");
        else if ((Op == TEXT("shot") || Op == TEXT("chuck")) && W.Num() > 1 && Vectors(W, 2, Next.Eye, Next.Aim))
        {
            Next.Name = W[1]; Next.bPlaceChuck = Op == TEXT("chuck");
            Shots.Add(Next);
            Next.Wait = 2.f;
        }
        else { ++Errors; UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHOTS_BAD_LINE %d: %s"), I + 1, *Lines[I]); }
    }
    const FString Folder = FPaths::ScreenShotDir() / TEXT("Shots") / FPaths::GetBaseFilename(File);
    IFileManager::Get().DeleteDirectory(*Folder, false, true);
    IFileManager::Get().MakeDirectory(*Folder, true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHOTS file=%s steps=%d at=%s"), *File, Shots.Num(), At.IsEmpty() ? TEXT("-") : *At);

    TWeakObjectPtr<AHUD> WeakHud(Hud);
    float Clock = 2.f;
    World->GetTimerManager().SetTimerForNextTick([World, At, bEvening]()
    {
        if (auto* Mode = World->GetAuthGameMode<ADockGameMode>())
            if (!At.IsEmpty() && !Mode->StartFromMenu(At)) UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHOTS_BAD_POINT %s"), *At);
        if (bEvening) MarkDockSewerExited();
    });
    ACameraActor* Camera = World->SpawnActor<ACameraActor>();
    Clock += 4.f;   // let the region stream in and settle
    for (const FShot& Shot : Shots)
    {
        Clock += Shot.Wait;
        FTimerHandle View, Snap;
        if (Shot.bAudit)
        {
            World->GetTimerManager().SetTimer(Snap, [World, Name = FPaths::GetBaseFilename(File)]() { ChuckLightAudit::Run(World, *Name); }, Clock, false);
            continue;
        }
        World->GetTimerManager().SetTimer(View, [World, Camera, Shot, WeakHud]()
        {
            if (Shot.bPlaceChuck)
                if (auto* C = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
                {
                    FHitResult Hit;
                    const FVector Down = Shot.Aim + FVector(0, 0, 200);
                    FVector Floor = Shot.Aim;
                    FCollisionQueryParams Q; Q.AddIgnoredActor(C);
                    if (World->LineTraceSingleByChannel(Hit, Down, Shot.Aim - FVector(0, 0, 2000), ECC_Visibility, Q)) Floor = Hit.ImpactPoint;
                    C->ResetAtLocation(Floor + FVector(0, 0, C->GetSimpleCollisionHalfHeight() + 2.f));
                }
            Camera->GetCameraComponent()->SetFieldOfView(Shot.Fov);
            Camera->SetActorLocationAndRotation(Shot.Eye, (Shot.Aim - Shot.Eye).Rotation());
            if (APlayerController* PC = World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
            if (AHUD* H = WeakHud.Get()) H->bShowHUD = Shot.bHud;
        }, FMath::Max(.1f, Clock - Shot.Wait + .2f), false);
        World->GetTimerManager().SetTimer(Snap, [Folder, Shot]()
        {
            FScreenshotRequest::RequestScreenshot(Folder / Shot.Name + TEXT(".png"), false, false);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHOT %s eye=%s aim=%s"), *Shot.Name, *Shot.Eye.ToString(), *Shot.Aim.ToString());
        }, Clock, false);
    }
    FTimerHandle Quit;
    World->GetTimerManager().SetTimer(Quit, [World, Errors, Count = Shots.Num(), Folder]()
    {
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHOTS_COMPLETE failures=%d shots=%d folder=%s"), Errors, Count, *Folder);
        if (APlayerController* PC = World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
    }, Clock + 2.f, false);
    return true;
}
