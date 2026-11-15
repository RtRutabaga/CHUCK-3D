#include "PhlegethosFall.h"
#include "PhlegethosSetting.h"
#include "AstralSummon.h"
#include "ChuckCharacter.h"
#include "ChuckClipData.h"
#include "DockGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"

// The fall into Phlegethos (Claude, 2026-11-14; PhlegethosFall.h). The pattern is FallCutscene.cpp's: the clock is song
// time, cuts land on the bars, Chuck is placed and posed each frame, and the stage is the playable one (here
// PhlegethosSetting, built under the dark at the start).
using namespace Phleg;

namespace
{
    float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
    float Span(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }
    const TCHAR* ShotName(float T)
    {
        using F = APhlegethosFall;
        return T < F::Reveal ? TEXT("Tumble") : T < F::CloseUp ? TEXT("OpenSky") : T < F::Deck ? TEXT("Face") : T < F::Volcano ? TEXT("AshDeck")
            : T < F::Rivers ? TEXT("Volcano") : T < F::FromGround ? TEXT("Rivers") : T < F::Rush ? TEXT("FromGround") : T < F::Impact ? TEXT("Rush")
            : T < F::Look ? TEXT("Floor") : T < F::Drag ? TEXT("Look") : T < F::Crane ? TEXT("Drag") : TEXT("Crane");
    }
    const float ShotTimes[] = {-3.f, 3.f, 10.f, 15.f, 18.5f, 20.8f, 23.f, 26.5f, 29.6f, 30.7f, 32.f, 35.f, 36.f, 39.5f, 43.5f, 47.f, 50.5f, 54.f, 58.f, 61.f};
    const FLinearColor Ember(.35f, .05f, .02f);
    // The volcano's crater (stage frame; PhlegethosSetting's land beyond).
    const FVector Crater(Phleg::VolcanoAt.X, Phleg::VolcanoAt.Y, Phleg::VolcanoTop);
}

APhlegethosFall::APhlegethosFall()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

APhlegethosFall* APhlegethosFall::Start(AChuckCharacter* Chuck)
{
    UWorld* World = Chuck ? Chuck->GetWorld() : nullptr;
    if (!World) return nullptr;
    auto* Scene = World->SpawnActor<APhlegethosFall>(FVector::ZeroVector, FRotator::ZeroRotator);
    Scene->Chuck = Chuck;
    Chuck->AddTickPrerequisiteActor(Scene);
    Scene->Camera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator);
    Scene->Camera->GetCameraComponent()->bConstrainAspectRatio = false;
    if (auto* PC = Cast<APlayerController>(Chuck->GetController()))
    {
        PC->SetViewTarget(Scene->Camera);
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 1.f, .01f, FLinearColor::Black, false, true);
    }
    if (auto* Mode = World->GetAuthGameMode<ADockGameMode>()) Mode->FadeOutMusic(.6f);
    if (auto* Song = LoadObject<USoundWave>(nullptr, TEXT("/Game/Art/Audio/SW_FallToChult.SW_FallToChult")))
        Scene->Music = UGameplayStatics::CreateSound2D(World, Song, .6f, 1.f, 0.f, nullptr, false, false);
    auto Loop = [World](const TCHAR* Name) -> UAudioComponent*
    {
        auto* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name));
        return Wave ? UGameplayStatics::CreateSound2D(World, Wave, 0.f, 1.f, 0.f, nullptr, false, false) : nullptr;
    };
    Scene->Wind = Loop(TEXT("SFX_FallRush_00"));
    Scene->Flap = Loop(TEXT("SFX_FallFlap_00"));
    Scene->Whistle = Loop(TEXT("SFX_FallWhistle_00"));
    Scene->ThudSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_FallThud_00.SFX_FallThud_00"));
    Scene->LandSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_Land_02.SFX_Land_02"));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_START music=%d wind=%d"), Scene->Music != nullptr, Scene->Wind != nullptr);
    return Scene;
}

bool APhlegethosFall::IsMusicPlaying() const { return Music && Music->IsPlaying(); }

void APhlegethosFall::Skip()
{
    if (bSkipping || bFinished) return;
    SkipAt = Clock; bSkipping = true;
    if (auto* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr)
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .45f, FLinearColor::Black, false, true);
    if (Music) Music->FadeOut(.5f, 0.f);
    for (UAudioComponent* A : {Wind, Flap, Whistle}) if (A) A->FadeOut(.3f, 0.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_SKIP at=%.2f"), Clock);
}

float APhlegethosFall::Altitude(float T)
{
    // Height of his feet over the landing (cm): vast and slow high up, fast through the ash deck (2.2-3.2 km), then the
    // plain coming up at him. Monotone cubic through the keys, as FallCutscene's.
    static const float Ts[] = {-Lead, Reveal, CloseUp, Deck, 16.8f, 19.5f, FromGround, Rush, Impact};
    static const float Hs[] = {540000.f, 505000.f, 430000.f, 360000.f, 300000.f, 205000.f, 62000.f, 13000.f, 0.f};
    constexpr int32 N = UE_ARRAY_COUNT(Ts);
    if (T <= Ts[0]) return Hs[0];
    if (T >= Ts[N - 1]) return Hs[N - 1];
    float D[N - 1], M[N];
    for (int32 I = 0; I < N - 1; ++I) D[I] = (Hs[I + 1] - Hs[I]) / (Ts[I + 1] - Ts[I]);
    M[0] = D[0]; M[N - 1] = D[N - 2];
    for (int32 I = 1; I < N - 1; ++I)
    {
        if (D[I - 1] * D[I] <= 0) { M[I] = 0; continue; }
        const float W1 = 2 * (Ts[I + 1] - Ts[I]) + (Ts[I] - Ts[I - 1]), W2 = (Ts[I + 1] - Ts[I]) + 2 * (Ts[I] - Ts[I - 1]);
        M[I] = (W1 + W2) / (W1 / D[I - 1] + W2 / D[I]);
    }
    int32 K = 0;
    while (T > Ts[K + 1]) ++K;
    const float H = Ts[K + 1] - Ts[K], S = (T - Ts[K]) / H;
    const float H00 = 2 * S * S * S - 3 * S * S + 1, H10 = S * S * S - 2 * S * S + S, H01 = -2 * S * S * S + 3 * S * S, H11 = S * S * S - S * S;
    return H00 * Hs[K] + H10 * H * M[K] + H01 * Hs[K + 1] + H11 * H * M[K + 1];
}

void APhlegethosFall::BuildStage()
{
    // The stage itself (not yet live: no bombs, no geysers), its light and air; the ship goes dark.
    APhlegethosSetting::Enter(GetWorld(), false);
    // (No volumetric ash deck: its layer sat under him, placed by another stage's hidden atmosphere, and read as dunes from
    // above. He falls through the haze toward the burning lake instead.)
    bStage = true;
}

FVector APhlegethosFall::ChuckTarget() const
{
    const AChuckCharacter* C = Chuck.Get();
    return C ? C->GetMesh()->GetComponentLocation() + C->GetMesh()->GetComponentQuat().RotateVector(FVector(0, 0, 32.f)) : FVector::ZeroVector;
}

void APhlegethosFall::Aim(const FVector& From, const FVector& At, float FocalMM, float FStop)
{
    UCameraComponent* Lens = Camera->GetCameraComponent();
    Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
    Lens->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(36.f / (2.f * FocalMM))));
    FPostProcessSettings& P = Lens->PostProcessSettings;
    Lens->PostProcessBlendWeight = 1.f;
    P.bOverride_DepthOfFieldFstop = true; P.DepthOfFieldFstop = FStop;
    P.bOverride_DepthOfFieldFocalDistance = true; P.DepthOfFieldFocalDistance = FMath::Max(10.f, static_cast<float>(FVector::Dist(From, At)));
    P.bOverride_DepthOfFieldSensorWidth = true; P.DepthOfFieldSensorWidth = 36.f;
}

void APhlegethosFall::UpdateChuck(float DeltaSeconds)
{
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    using EClip = AChuckCharacter::EClip;
    const float T = Clock;
    const FVector G0 = PhlegethosLandingSpot();
    const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (T < Rush + 1.4f)
    {
        // The long fall: out of the rupture tumbling, then spread belly down, turning slowly, drifting.
        const float Drift = 1.f - Ease(Span(T, Rivers, Rush + 1.4f));
        const FVector Off(FMath::Sin(T * .37f) * 520.f * Drift, FMath::Cos(T * .31f) * 440.f * Drift, Altitude(T) + Half);
        const float Wild = 1.f - Ease(Span(T, -Lead + 1.f, Reveal - .3f));
        const FRotator Body(-80.f + 8.f * FMath::Sin(T * 1.1f) + Wild * 150.f * FMath::Sin(T * 2.3f), 40.f + 9.f * T + Wild * 70.f * T, 9.f * FMath::Sin(T * .8f) + Wild * 100.f * FMath::Sin(T * 1.7f + 1.f));
        C->SetActorLocationAndRotation(G0 + Off, Body);
        if (C->Base != EClip::JumpLoop) C->SetClip(EClip::JumpLoop, 0, .2f);
        C->BaseTime += DeltaSeconds * .5f;
        return;
    }
    if (T < Impact)
    {
        // The last moments: righting toward the ground, flat for it.
        const float U = Span(T, Rush + 1.4f, Impact);
        const FVector Touchdown = FRotator(0, ProneYaw, 0).Vector() * -38.f * U;
        const FRotator Body = FMath::Lerp(FRotator(-80.f, 40.f + 9.f * T, 9.f * FMath::Sin(T * .8f)), FRotator(-80.f, ProneYaw, 25.f), Ease(U));
        C->SetActorLocationAndRotation(G0 + Touchdown + FVector(0, 0, Altitude(T) + FMath::Lerp(Half, ProneHeight, U)), Body);
        C->BaseTime += DeltaSeconds * .8f;
        return;
    }
    const float Clip = C->Clips[static_cast<int32>(EClip::Summon)]->GetPlayLength();
    const float Rise = Summon + AAstralSummon::SummonPeak;
    if (T < Rise - SwapLead)
    {
        // Flat on his front on the hot basalt: a hard bounce and a short skid, then still.
        const float Since = T - Impact;
        const float Lift = Since < .3f ? 24.f * FMath::Sin(PI * Since / .3f) : Since < .46f ? 6.f * FMath::Sin(PI * (Since - .3f) / .16f) : 0.f;
        const FVector Skid = FRotator(0, ProneYaw, 0).Vector() * -38.f * FMath::Exp(-Since * 7.f);
        if (C->Base != EClip::Hang) C->SetClip(EClip::Hang, 0, .06f);
        C->BaseTime = 0;
        const float Settle = FMath::Exp(-Since * 4.5f);
        const FRotator Body(-90.f + 10.f * Settle * FMath::Sin(Since * 22.f), ProneYaw + 12.f * (1.f - Settle), 14.f + 20.f * Settle * FMath::Cos(Since * 17.f));
        C->SetActorLocationAndRotation(G0 + Skid + FVector(0, 0, ProneHeight + Lift), Body);
        return;
    }
    // Back on his feet out of the light: he looks left, right, then down at the rock, and takes his drag.
    float Yaw = 0;
    const float TurnLength = C->Clips[static_cast<int32>(EClip::TurnLeft90)]->GetPlayLength();
    auto Turn = [&](float From, float Start, float Delta)
    {
        const float Frame = FMath::Clamp((T - Start) * 30.f, 0.f, static_cast<float>(ChuckClipData::TurnYawFrames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), ChuckClipData::TurnYawFrames - 1);
        return From + FMath::Lerp(ChuckClipData::TurnYaw[I], ChuckClipData::TurnYaw[I + 1], Frame - I) / 90.f * Delta;
    };
    constexpr float LookLeft = Look + .2f, LookRight = Look + 1.6f, LookBack = Look + 3.0f;
    if (T >= LookBack) Yaw = Turn(60.f, LookBack, -60.f);
    else if (T >= LookRight) Yaw = Turn(-70.f, LookRight, 130.f);
    else if (T >= LookLeft) Yaw = Turn(0.f, LookLeft, -70.f);
    C->SetActorLocationAndRotation(G0 + FVector(0, 0, Half), FRotator(0, Yaw, 0));
    if (T < Rise) { if (C->Base != EClip::Summon) C->SetClip(EClip::Summon, 0, 0.f); C->BaseTime = 0; }
    else if (T < Rise + Clip) { C->BaseTime = T - Rise; }
    else if (T >= LookLeft && T < LookLeft + TurnLength) { if (C->Base != EClip::TurnLeft90) C->SetClip(EClip::TurnLeft90, 0, .15f); C->BaseTime = T - LookLeft; }
    else if (T >= LookRight && T < LookRight + TurnLength) { if (C->Base != EClip::TurnRight90) C->SetClip(EClip::TurnRight90, 0, .15f); C->BaseTime = T - LookRight; }
    else if (T >= LookBack && T < LookBack + TurnLength) { if (C->Base != EClip::TurnLeft90) C->SetClip(EClip::TurnLeft90, 0, .15f); C->BaseTime = T - LookBack; }
    else { if (C->Base != EClip::Idle) C->SetClip(EClip::Idle, 0, .3f); else C->BaseTime += DeltaSeconds; }
}

void APhlegethosFall::UpdateCamera()
{
    const float T = Clock;
    const FVector G0 = PhlegethosLandingSpot();
    const FVector C = ChuckTarget();
    FVector From, At = C;
    float Focal = 35.f, Stop = 4.f;
    auto Orbit = [](float YawDeg, float PitchDeg, float Dist) { return FRotator(PitchDeg, YawDeg, 0).Vector() * Dist; };
    const FVector ToVolcano = (PhlegWorld(Crater) - G0).GetSafeNormal2D();
    const float VolcanoYaw = ToVolcano.Rotation().Yaw;
    if (T < Reveal)
    {
        // Out of the dark, tumbling, close, buffeted with him.
        const float U = Span(T, -Lead, Reveal);
        From = C + Orbit(200.f + 25.f * U, 14.f, FMath::Lerp(240.f, 300.f, U)) + FVector(FMath::Sin(T * 13.f), FMath::Sin(T * 11.f + 1.f), FMath::Sin(T * 17.f + 2.f)) * 4.f;
        Focal = 32.f;
    }
    else if (T < CloseUp)
    {
        // Small in a vast smoking sky, the ash deck far below.
        const float U = Ease(Span(T, Reveal, CloseUp));
        From = C + Orbit(VolcanoYaw + 160.f + 16.f * U, -2.f, FMath::Lerp(700.f, 480.f, U));
        At = C + FVector(0, 0, 60.f);
        Focal = 24.f; Stop = 11.f;
    }
    else if (T < Deck)
    {
        const FVector Face = Chuck->GetActorForwardVector(), Side = Chuck->GetActorRightVector();
        const float U = Span(T, CloseUp, Deck);
        From = C + Face * FMath::Lerp(120.f, 95.f, U) + Side * 25.f + FVector(0, 0, -10.f);
        At = C + Face * 20.f;
        Focal = 50.f; Stop = 2.2f;
    }
    else if (T < Volcano)
    {
        // Over his shoulder, down at the ash deck coming up.
        From = C + FVector(-70.f, 50.f, 240.f);
        At = C + FVector(30.f, -20.f, -500.f);
        Focal = 24.f; Stop = 11.f;
    }
    else if (T < Rivers)
    {
        // Under the deck: level with him, looking out at the volcano, which climbs into view as he drops (the 2D beat).
        const float U = Span(T, Volcano, Rivers);
        From = C - ToVolcano * FMath::Lerp(520.f, 640.f, U) + FVector(0, 0, FMath::Lerp(140.f, 90.f, U)) + FVector(-ToVolcano.Y, ToVolcano.X, 0) * 160.f;
        At = FMath::Lerp(C + ToVolcano * 2000.f + FVector(0, 0, 900), C + ToVolcano * 3000.f - FVector(0, 0, 400), Ease(U));
        Focal = 21.f; Stop = 16.f;
    }
    else if (T < FromGround)
    {
        // Looking down past him at the lake, the road's spine and the columns coming up.
        const float U = Span(T, Rivers, FromGround);
        From = C + Orbit(200.f + 30.f * U, 62.f, 420.f);
        At = C + FVector(1600.f, 0, -4000.f);
        Focal = 28.f; Stop = 11.f;
    }
    else if (T < Rush)
    {
        // From the plain, up at him: a speck falling out of the smoke.
        // (Low, tilted up enough to keep the plain's far edge and the lake along the bottom of the frame.)
        From = G0 + FVector(900.f, -650.f, 110.f);
        At = G0 + FVector(-250.f, 180.f, 1500.f);
        Focal = 24.f; Stop = 11.f;
    }
    else if (T < Impact)
    {
        // Beside him, the ground rushing up.
        const float U = Span(T, Rush, Impact);
        From = C + FVector(260.f, -220.f, FMath::Lerp(160.f, 60.f, U));
        if (From.Z < G0.Z + 25.f) From.Z = G0.Z + 25.f;
        Focal = 24.f; Stop = 5.6f;
    }
    else if (T < Look)
    {
        // Low on the rock where he lies, the glowing seams round him.
        const float U = Ease(Span(T, Impact + .3f, Look));
        From = FMath::Lerp(G0 + FVector(300.f, -110.f, 32.f), G0 + FVector(200.f, -75.f, 28.f), U);
        At = G0 + FVector(0, 0, 24.f);
        Focal = 24.f; Stop = 4.f;
    }
    else if (T < Drag)
    {
        const float U = Ease(Span(T, Look, Drag));
        From = G0 + FVector(FMath::Lerp(170.f, 140.f, U), FMath::Lerp(-95.f, -70.f, U), 44.f);
        At = C + FVector(0, 0, 10.f);
        Focal = 35.f; Stop = 2.8f;
    }
    else if (T < Crane)
    {
        const FVector Head = Chuck->GetMesh()->GetSocketLocation(TEXT("socket_cigarette"));
        const float U = Span(T, Drag, Crane);
        From = Head + FVector(FMath::Lerp(60.f, 52.f, U), FMath::Lerp(-75.f, -66.f, U), 6.f);
        At = Head + FVector(4.f, 0, 1.f);
        Focal = 50.f; Stop = 2.f;
    }
    else
    {
        // Up and back over him, turning to look north along the way on: the road over the lake, the columns, the
        // gorge's falls, the volcano erupting, the fortress on its mesa. He is a speck at the bottom of the frame.
        // (2026-11-16: higher, to 56 m, and wider, so the badlands and their rivers, the wastes and the path winding to the
        // fortress, the volcano on the left and the city of fire on the right are all in it.)
        const float U = Ease(Span(T, Crane, End));
        From = G0 + FMath::Lerp(FVector(-160.f, -60.f, 60.f), FVector(-4200.f, -2600.f, 5600.f), U);
        At = FMath::Lerp(G0 + FVector(0, 0, 30.f), PhlegWorld(FVector(30000, 400, 900)), Ease(Span(T, Crane, Crane + 9.f)));
        Focal = FMath::Lerp(35.f, 22.f, U); Stop = FMath::Lerp(4.f, 16.f, U);
    }
    if (T < ShakeUntil)
    {
        const float K = FMath::Square((ShakeUntil - T) / ShakeTime);
        From += FVector(FMath::Sin(T * 91.f), FMath::Sin(T * 77.f + 1.f), FMath::Sin(T * 113.f + 2.f)) * 14.f * K;
    }
    Aim(From, At, Focal, Stop);
}

void APhlegethosFall::UpdateAir()
{
    if (Clock >= Impact) return;
    const float Dt = GetWorld()->GetDeltaSeconds();
    const float Rush_ = Clock < Reveal ? .95f : Clock < CloseUp ? .3f : Clock < Deck ? .45f : Clock < Volcano ? .75f : Clock < FromGround ? .85f : .95f;
    if (Wind) Wind->SetVolumeMultiplier(FMath::FInterpTo(Wind->VolumeMultiplier, Rush_, Dt, 2.f));
    if (Flap) Flap->SetVolumeMultiplier(FMath::FInterpTo(Flap->VolumeMultiplier, Clock < Reveal ? 1.f : Clock < CloseUp ? .2f : Clock < Deck ? .6f : .6f, Dt, 2.f));
    if (Whistle) Whistle->SetVolumeMultiplier(FMath::FInterpTo(Whistle->VolumeMultiplier, Clock < Reveal ? .4f : Clock < CloseUp ? .08f : Clock < Volcano ? .3f : .55f, Dt, 2.f));
}

void APhlegethosFall::Events(float From, float To)
{
    auto Crossed = [&](float At) { return From < At && At <= To; };
    APlayerController* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr;
    APlayerCameraManager* Lens = PC ? PC->PlayerCameraManager : nullptr;
    auto Cut = [Lens]() { if (Lens) Lens->SetGameCameraCutThisFrame(); };
    if (!bStage)
    {
        BuildStage();
        for (UAudioComponent* A : {Wind, Flap, Whistle}) if (A) A->FadeIn(1.2f, A == Wind ? .95f : A == Flap ? 1.f : .4f);
        Cut();
    }
    if (Crossed(FadeUp) && Lens) Lens->StartCameraFade(1.f, 0.f, 1.4f, Ember, false, false);
    if (Crossed(0.f) && Music) { Music->Play(); UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_SCORE at=%.2f"), To); }
    for (float At : {Reveal, CloseUp, Deck, Volcano, Rivers, FromGround, Rush, Impact, Look, Drag, Crane}) if (Crossed(At)) Cut();
    // The ash deck is for the passage through it; from the plain it is the smoke overhead.
    if (Crossed(Impact))
    {
        // (The ash deck was for the passage through it: from the plain it read as a wall of cloud.)
        if (AshDeck) AshDeck->SetVisibility(false);
        for (UAudioComponent* A : {Wind, Flap, Whistle}) if (A) A->FadeOut(.25f, 0.f);
        if (ThudSound) UGameplayStatics::PlaySound2D(this, ThudSound, 1.f, .9f);
        if (LandSound) UGameplayStatics::PlaySound2D(this, LandSound, ThudSound ? .5f : 1.f, .6f);
        ShakeUntil = Impact + ShakeTime;
        bLanded = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_IMPACT at=%.2f"), To);
    }
    if (Crossed(Impact + .3f)) { if (ThudSound) UGameplayStatics::PlaySound2D(this, ThudSound, .4f, 1.1f); }
    AChuckCharacter* C = Chuck.Get();
    const FVector Body = ChuckTarget();
    const FVector Feet(Body.X, Body.Y, PhlegethosLandingSpot().Z + 1.f);
    if (C && Crossed(Summon + AAstralSummon::SummonPeak - SwapLead)) { C->SetAstralHidden(true); bVanished = true; }
    if (Crossed(Summon)) { AAstralSummon::Start(GetWorld(), Feet, true); bSummoned = true; }
    if (C && Crossed(Summon + AAstralSummon::SummonPeak)) C->SetAstralHidden(false);
    if (C && (Crossed(Drag + .55f) || Crossed(Drag + 3.0f))) { C->Exhale(); ++Exhales; }
    if (Crossed(Eruption)) { if (APhlegethosSetting* S = APhlegethosSetting::Get(GetWorld())) S->Erupt(1.f); bErupted = true; }
    if (Crossed(MusicFade) && Music) Music->FadeOut(End - MusicFade, 0.f);
    if (Crossed(Black) && Lens) Lens->StartCameraFade(0.f, 1.f, End - Black, FLinearColor::Black, false, true);
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckHellFallTest")))
    {
        for (float At : ShotTimes)
            if (Crossed(At))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("HellFall/T%04.1f_%s.png"), At, ShotName(At)), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_SHOT t=%.1f shot=%s chuck=%s"), At, ShotName(At), C ? *PhlegLocal(C->GetActorLocation()).ToString() : TEXT("-"));
            }
        if (Crossed(10.f)) UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_MUSIC playing=%d"), IsMusicPlaying());
    }
}

void APhlegethosFall::Finish()
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HELL_FALL_END at=%.2f skipped=%d landed=%d vanished=%d summoned=%d erupted=%d exhales=%d music_playing=%d"),
        Clock, bSkipping, bLanded, bVanished, bSummoned, bErupted, Exhales, IsMusicPlaying());
    if (Music) Music->Stop();
    for (UAudioComponent* A : {Wind, Flap, Whistle}) if (A) A->Stop();
    if (AshDeck) AshDeck->DestroyComponent();
    if (!APhlegethosSetting::Get(GetWorld())) APhlegethosSetting::Enter(GetWorld(), true);
    if (AChuckCharacter* C = Chuck.Get()) C->EndHellFall();
    if (auto* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr)
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FLinearColor::Black, false, false);
    if (Camera) Camera->Destroy();
    Destroy();
}

void APhlegethosFall::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    const float Before = Clock;
    // (The first frame builds the stage under the dark: its hitch is not song time.)
    Clock += bStage ? DeltaSeconds : 0.f;
    Events(Before, Clock);
    if (bSkipping && Clock - SkipAt >= .5f) { Finish(); return; }
    if (Clock >= End) { Finish(); return; }
    UpdateChuck(DeltaSeconds);
    UpdateAir();
    if (Camera && Chuck.IsValid()) UpdateCamera();
}
