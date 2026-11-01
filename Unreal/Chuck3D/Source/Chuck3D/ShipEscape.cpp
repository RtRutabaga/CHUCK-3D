#include "ShipEscape.h"
#include "ShipSetting.h"
#include "ChuckCharacter.h"
#include "ChultTempleRubble.h"
#include "DockGameMode.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// The escape onto the ship (Claude, 2026-11-01; ShipEscape.h). Shots: "Passage" (his own crawl camera, behind him in
// the stone, the light ahead), "Mouth" (low in the compartment, the crawlway's mouth as he comes out and gets up), "Sea"
// (behind him as he goes to the broken planking, easing in on the open sea through it).
using namespace Ship;

namespace
{
    // Where he stops to look out (the arrival spawn), and the hole's middle, in the ship frame.
    const FVector2D Stand(235, 210);
    FVector HoleMid() { return FVector((HoleX0 + HoleX1) * .5f, ShipInnerHalf((HoleX0 + HoleX1) * .5f, (HoleZ0 + HoleZ1) * .5f), (HoleZ0 + HoleZ1) * .5f); }
}

AShipEscape::AShipEscape()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AShipEscape* AShipEscape::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AShipEscape> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipEscape* AShipEscape::Watch(UWorld* World)
{
    if (AShipEscape* E = Get(World)) return E;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AShipEscape* E = World->SpawnActor<AShipEscape>(FVector::ZeroVector, FRotator::ZeroRotator, P);
    if (E)
    {
        E->Camera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, P);
        if (E->Camera) { E->Camera->GetCameraComponent()->bConstrainAspectRatio = false; E->Camera->Tags.Add(TEXT("ShipEscape")); }
    }
    return E;
}

bool AShipEscape::IsLetterboxed(const UWorld* World)
{
    const AShipEscape* E = Get(World);
    return E && E->IsLetterboxed();
}

void AShipEscape::Start()
{
    Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (bPlaying || !Chuck.IsValid()) return;
    bPlaying = true; bReleased = bStaged = bOut = bAtHole = bSkipping = bDone = false;
    Clock = 0; OutAt = HoleAt = -1; Shot = TEXT(""); bCut = true;
    ++Plays;
    AChuckCharacter* C = Chuck.Get();
    if (auto* PC = Cast<APlayerController>(C->GetController()))
    {
        C->DisableInput(PC); C->SetTestStick(FVector2D::ZeroVector); C->SetRunHeld(false);
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, BlackAt, FLinearColor::Black, false, true);
    }
    if (ADockGameMode* Mode = GetWorld()->GetAuthGameMode<ADockGameMode>()) Mode->FadeOutMusic(1.2f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_START n=%d at=%s"), Plays, *C->GetActorLocation().ToString());
}

void AShipEscape::Stage()
{
    if (bStaged) return;
    bStaged = true;
    AShipSetting::Enter(GetWorld());
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    // In the passage, 70 cm from its blind end, facing along it toward the mouth and the light.
    C->ResetCrawlingAt(ShipWorld(FVector(SeamX0 + 70, SeamY, LowerZ + 32.5f)));
    C->SetActorRotation(FRotator(0, 0, 0)); C->Recenter();
    if (auto* PC = Cast<APlayerController>(C->GetController()))
    {
        PC->SetViewTarget(C);
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, bSkipping ? .5f : FadeInTime, FLinearColor::Black, false, false);
    }
    Shot = TEXT("Passage");
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_STAGE clock=%.2f crawling=%d at=%s"), Clock, C->IsCrawling(), *ShipLocal(C->GetActorLocation()).ToString());
}

void AShipEscape::Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov)
{
    if (!Camera) return;
    const bool bNew = FCString::Strcmp(Name, Shot) != 0;
    if (bNew)
    {
        bCut = true; Shot = Name;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_SHOT shot=%s clock=%.2f"), Name, Clock);
        if (AChuckCharacter* C = Chuck.Get()) if (auto* PC = Cast<APlayerController>(C->GetController())) PC->SetViewTarget(Camera);
    }
    const float Dt = GetWorld()->GetDeltaSeconds();
    Eye = bCut ? InEye : FMath::VInterpTo(Eye, InEye, Dt, 2.5f);
    Aim = bCut ? InAim : FMath::VInterpTo(Aim, InAim, Dt, 3.5f);
    bCut = false;
    Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void AShipEscape::Release()
{
    if (bReleased) return;
    bReleased = true;
    AChuckCharacter* C = Chuck.Get();
    if (auto* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr)
    {
        C->SetTestStick(FVector2D::ZeroVector);
        C->Recenter();
        PC->SetViewTargetWithBlend(C, bSkipping ? 0.f : 1.1f, VTBlend_EaseInOut, 2.f);
        C->EnableInput(PC);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_RELEASE clock=%.2f skipped=%d"), Clock, bSkipping);
}

void AShipEscape::Finish()
{
    Release();
    if (bDone) return;
    bDone = true; bPlaying = false; ++Finishes;
    const AChuckCharacter* C = Chuck.Get();
    const FVector L = C ? ShipLocal(C->GetActorLocation()) : FVector::ZeroVector;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_END clock=%.2f skipped=%d out=%d space=%s at=%s sanity=%d"), Clock, bSkipping, bOut,
        C ? ShipSpaceName(ShipSpaceAt(C->GetActorLocation())) : TEXT("none"), *L.ToString(), C ? C->GetSanity() : -1);
}

void AShipEscape::Skip()
{
    if (!bPlaying || bReleased || bSkipping) return;
    bSkipping = true; ++Skips;
    AChuckCharacter* C = Chuck.Get();
    if (!bStaged) Stage();
    else if (auto* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr)
        if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, .5f, FLinearColor::Black, false, false); }
    // Straight to where it ends: standing at the broken planking, looking out.
    if (C)
    {
        C->ResetAtLocation(ShipSpawn(EShipSpawn::Arrival));
        C->SetActorRotation(FRotator(0, ShipSpawnYaw(EShipSpawn::Arrival), 0));
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_SKIP clock=%.2f"), Clock);
    Finish();
}

void AShipEscape::Tick(float Dt)
{
    Super::Tick(Dt);
    AChuckCharacter* C = Chuck.Get();
    if (!bPlaying)
    {
        // Watching the crevice's end (once a session; never from aboard). The rubble map's own test crawls to the end
        // and back out again: the escape stands aside for it.
        if (Plays > 0 || FParse::Param(FCommandLine::Get(), TEXT("ChuckTempleRubbleTest"))) return;
        Watch_ -= Dt;
        if (Watch_ > 0) return;
        Watch_ = .1f;
        auto* Pawn = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
        if (Pawn && !Pawn->IsAstral() && AtChultTempleCreviceEnd(Pawn->GetActorLocation())) Start();
        return;
    }
    if (!C) { bPlaying = false; return; }
    Clock += Dt;
    if (auto* PC = Cast<APlayerController>(C->GetController()))
        if (!bReleased && PC->WasInputKeyJustPressed(EKeys::Escape)) { Skip(); return; }
    if (bReleased) { if (Clock > (HoleAt > 0 ? HoleAt : Clock) + SeaHold + 1.2f || bSkipping) Finish(); return; }
    if (!bStaged) { if (Clock >= StageAt) Stage(); return; }

    const FVector L = ShipLocal(C->GetActorLocation());
    if (!bOut)
    {
        // Crawling on along the passage, toward the light at its mouth.
        C->SetTestStickWorld(FVector(1, 0, 0)); C->AddMovementInput(FVector(1, 0, 0), 1.f, true);
        if (L.X > SeamX1 - 90)
            Frame(ShipWorld(FVector(SeamX1 + 150, SeamY + 170, LowerZ + 40)), FVector(C->GetActorLocation().X, C->GetActorLocation().Y, ShipOrigin().Z + LowerZ + 26), TEXT("Mouth"), 52.f);
        if (L.X > SeamX1 + 62 && !C->IsCrawling()) { bOut = true; OutAt = Clock; UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_OUT clock=%.2f"), Clock); }
        // Stuck somehow: put him out of it and go on.
        if (Clock > StageAt + 16.f) { C->ResetAtLocation(ShipWorld(FVector(SeamX1 + 90, SeamY + 40, LowerZ + 34.65f))); bOut = true; OutAt = Clock; UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_ESCAPE_UNSTUCK clock=%.2f"), Clock); }
        return;
    }
    // Up on his feet among the sailcloth, then over to the broken planking.
    const FVector Hole = ShipWorld(HoleMid());
    if (!bAtHole)
    {
        const bool bRise = Clock - OutAt < 1.1f;
        if (bRise)
        {
            C->SetTestStick(FVector2D::ZeroVector);
            Frame(ShipWorld(FVector(SeamX1 + 150, SeamY + 170, LowerZ + 48)), C->GetActorLocation() + FVector(0, 0, 18), TEXT("Mouth"), 52.f);
            return;
        }
        const FVector Goal = ShipWorld(FVector(Stand.X, Stand.Y, L.Z));
        const FVector To = (Goal - C->GetActorLocation()).GetSafeNormal2D();
        C->SetTestStickWorld(To * .8f); C->AddMovementInput(To, .8f, true);
        Frame(ShipWorld(FVector(298, -40, LowerZ + 105)), FMath::Lerp(C->GetActorLocation() + FVector(0, 0, 20), Hole, .55f), TEXT("Sea"), 55.f);
        if (FVector::Dist2D(C->GetActorLocation(), Goal) < 22.f || Clock - OutAt > 14.f)
        {
            bAtHole = true; HoleAt = Clock;
            C->SetTestStick(FVector2D::ZeroVector);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ESCAPE_AT_HOLE clock=%.2f dist=%.0f"), Clock, FVector::Dist2D(C->GetActorLocation(), Goal));
        }
        return;
    }
    // He looks out; the view eases in past him to the sea through the hole.
    C->SetActorRotation(FMath::RInterpTo(C->GetActorRotation(), FRotator(0, (Hole - C->GetActorLocation()).Rotation().Yaw, 0), Dt, 3.f));
    const float U = FMath::SmoothStep(0.f, SeaHold, Clock - HoleAt);
    Frame(FMath::Lerp(ShipWorld(FVector(298, -40, LowerZ + 105)), ShipWorld(FVector(250, 130, LowerZ + 92)), U), FMath::Lerp(C->GetActorLocation() + FVector(0, 0, 20), Hole, .55f + .45f * U), TEXT("Sea"), FMath::Lerp(55.f, 46.f, U));
    if (Clock - HoleAt > SeaHold) Release();
}
