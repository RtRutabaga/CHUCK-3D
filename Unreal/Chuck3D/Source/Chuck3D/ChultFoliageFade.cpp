#include "ChultFoliageFade.h"
#include "ChuckCharacter.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "ChultLanding.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

bool AChultFoliageFade::bForceOff = false;

AChultFoliageFade::AChultFoliageFade()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved this frame
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AChultFoliageFade::Spawn(UWorld* World)
{
    if (!World) return;
    for (TActorIterator<AChultFoliageFade> It(World); It; ++It) return;
    World->SpawnActor<AChultFoliageFade>();
}

void AChultFoliageFade::BeginPlay()
{
    Super::BeginPlay();
    Collection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Art/ChultNature/MPC_ChultFoliageFade.MPC_ChultFoliageFade"));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_CHULT_FOLIAGE_FADE collection=%d"), Collection != nullptr);
}

void AChultFoliageFade::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Collection) UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Chuck"), FLinearColor(0, 0, -1.0e7f, 0));
    Super::EndPlay(Reason);
}

void AChultFoliageFade::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Collection) return;
    const auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    const bool bOn = Chuck && PC && PC->PlayerCameraManager && !Chuck->IsInFallCutscene() && !Chuck->IsHidden() && PC->GetViewTarget() == Chuck && !bForceOff;
    Strength = FMath::FInterpConstantTo(Strength, bOn ? 1.f : 0.f, DeltaSeconds, 4.f);
    if (!Chuck || !PC || !PC->PlayerCameraManager) return;
    const FVector At = Chuck->GetActorLocation(), Eye = PC->PlayerCameraManager->GetCameraLocation();
    UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Chuck"), FLinearColor(At.X, At.Y, At.Z, Strength));
    UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Eye"), FLinearColor(Eye.X, Eye.Y, Eye.Z, 0));
}

void ReviewChultFoliageFade(AActor* Owner, AChuckCharacter* Chuck, int32 Failures)
{
    UWorld* World = Owner->GetWorld();
    struct FSpot { const TCHAR* Name; FVector At; float Yaw, Pitch; };
    // Where the clearing's big fern and foreground elephant ear stand (ChultLanding/ChultDressing), and a route edge's fern line.
    const TArray<FSpot> Spots = {{TEXT("Clearing"), FVector(300, 250, 34.65f), 200.f, -12.f}, {TEXT("ClearingBack"), FVector(300, 250, 34.65f), 30.f, -8.f}};
    struct FRun { int32 Phase = 0; double Next = 0; int32 Shots = 0; float OnStrength = 0; FTimerHandle Timer; };
    auto Run = MakeShared<FRun>();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer, [World, Chuck, Run, Spots, Failures]()
    {
        const double Now = World->GetTimeSeconds();
        if (Now < Run->Next) return;
        const int32 I = Run->Phase / 4, Step = Run->Phase % 4;
        AChultFoliageFade* Fade = nullptr;
        for (TActorIterator<AChultFoliageFade> It(World); It; ++It) Fade = *It;
        if (I >= Spots.Num())
        {
            AChultFoliageFade::bForceOff = false;
            const int32 Fails = Failures + !(Fade && Fade->HasCollection()) + (Run->OnStrength < .99f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FOLIAGE_FADE_REVIEW failures=%d collection=%d strength_on=%.2f shots=%d"), Fails, Fade && Fade->HasCollection(), Run->OnStrength, Run->Shots);
            World->GetTimerManager().ClearTimer(Run->Timer);
            FPlatformMisc::RequestExitWithStatus(false, Fails ? 1 : 0);
            return;
        }
        const FSpot& S = Spots[I];
        if (Step == 0)
        {
            AChultFoliageFade::bForceOff = false;
            Chuck->ResetAtLocation(AChultLanding::FloorOrigin() + S.At); Chuck->SetActorRotation(FRotator(0, S.Yaw, 0));
            if (auto* PC = Cast<APlayerController>(Chuck->GetController())) { PC->SetControlRotation(FRotator(S.Pitch, S.Yaw, 0)); Chuck->DisableInput(PC); }
            Chuck->SetOrbitPitch(S.Pitch);
            Run->Next = Now + 2.0;
        }
        else if (Step == 1)
        {
            if (Fade) Run->OnStrength = FMath::Max(Run->OnStrength, Fade->GetStrength());
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("FoliageFade_%s_On.png"), S.Name), true, false); ++Run->Shots;
            Run->Next = Now + .6;
        }
        else if (Step == 2) { AChultFoliageFade::bForceOff = true; Run->Next = Now + 1.2; }
        else
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / FString::Printf(TEXT("FoliageFade_%s_Off.png"), S.Name), true, false); ++Run->Shots;
            Run->Next = Now + .6;
        }
        ++Run->Phase;
    }, .1f, true);
}
