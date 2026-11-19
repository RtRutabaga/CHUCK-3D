#include "FeyRiverScene.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "DockGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"

// The river into the Feywild (Claude, 2026-11-19; FeyRiverScene.h). The pattern is PhlegethosFall.cpp's: the clock is scene
// time, the cuts land on the score's bars, Chuck is placed and posed each frame, and the stage is the playable one
// (FeywildSetting, with the river's set added), built under the dark at the start.
using namespace Fey;

namespace
{
    float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
    float Span(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }
    using F = AFeyRiverScene;
    const TCHAR* ShotName(float T)
    {
        return T < F::Score ? TEXT("Dark") : T < F::Out ? TEXT("Glimpse") : T < F::Face ? TEXT("Mouth") : T < F::Ahead ? TEXT("Face") : T < F::Falls ? TEXT("Ahead")
            : T < F::Shelf ? TEXT("Lip") : T < F::Drift ? TEXT("Plunge") : T < F::Wash ? TEXT("Drift") : T < F::Last ? TEXT("Shore") : TEXT("Glade");
    }
    const float ShotTimes[] = {2.5f, 5.5f, 7.5f, 8.6f, 9.6f, 11.f, 12.8f, 15.f, 18.5f, 20.6f, 21.6f, 22.9f, 23.4f, 24.8f, 25.9f, 26.75f, 27.7f, 29.f, 30.7f, 31.9f, 33.5f, 34.6f, 36.1f};
    // His head goes under here (the first seconds' dunks, at no steady rhythm, and the plunge).
    const FVector2D Dunks[] = {{0.f, 1.15f}, {2.05f, 2.9f}, {3.55f, 4.95f}, {5.6f, 6.05f}, {F::Plunge, F::Surface}};
    // The slice's stream above the falls, its shelf, and the pond (local): the water's surface and his centre on it.
    constexpr float UpperZ = Ledge2Top - 14.f, ShelfZ = LedgeTop - 14.f, Float = -10.f;   // (his centre against the surface: head and shoulders out)
    constexpr float StreamX0 = 7050.f, UpperLipX = Ledge2X + 46.f, ShelfX = 6146.f, LowerLipX = LedgeX + 46.f, PlungeX = 4737.f, SurfaceX = 4590.f;
    constexpr float Gravity = 490.f;   // (half of g, cm/s2)
}

AFeyRiverScene::AFeyRiverScene()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AFeyRiverScene* AFeyRiverScene::Start(AChuckCharacter* Chuck)
{
    UWorld* World = Chuck ? Chuck->GetWorld() : nullptr;
    if (!World) return nullptr;
    auto* Scene = World->SpawnActor<AFeyRiverScene>(FVector::ZeroVector, FRotator::ZeroRotator);
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
    Scene->Rush = Loop(TEXT("SFX_ShipWater_00"));
    Scene->Roar = Loop(TEXT("SFX_FeyFalls_00"));
    for (int32 I = 0; I < 6; ++I)
        if (auto* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_StreamSplash_%02d.SFX_StreamSplash_%02d"), I, I))) Scene->Splashes.Add(S);
    Scene->LandSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_Land_02.SFX_Land_02"));
    Scene->Glow = NewObject<UPointLightComponent>(Scene); Scene->Glow->SetupAttachment(Scene->RootComponent);
    Scene->Glow->SetMobility(EComponentMobility::Movable); Scene->Glow->SetIntensityUnits(ELightUnits::Candelas); Scene->Glow->SetIntensity(0.f);
    Scene->Glow->SetLightColor(FLinearColor(.45f, .66f, 1.f)); Scene->Glow->SetAttenuationRadius(420.f); Scene->Glow->SetCastShadows(false); Scene->Glow->SetVolumetricScatteringIntensity(0.f);
    Scene->Glow->RegisterComponent();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_START music=%d rush=%d roar=%d splashes=%d"), Scene->Music != nullptr, Scene->Rush != nullptr, Scene->Roar != nullptr, Scene->Splashes.Num());
    return Scene;
}

bool AFeyRiverScene::IsMusicPlaying() const { return Music && Music->IsPlaying(); }

void AFeyRiverScene::Skip()
{
    if (bSkipping || bFinished) return;
    SkipAt = Clock; bSkipping = true;
    if (auto* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr)
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .45f, FLinearColor::Black, false, true);
    if (Music) Music->FadeOut(.5f, 0.f);
    for (UAudioComponent* A : {Rush, Roar}) if (A) A->FadeOut(.3f, 0.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_SKIP at=%.2f"), Clock);
}

float AFeyRiverScene::RiverS(float T)
{
    // Slow in the tunnel, gathering speed out of it, quick toward the falls.
    if (T <= 6.f) return 250.f + 180.f * T;
    if (T <= 8.f) { const float U = T - 6.f; return 1330.f + 180.f * U + 30.f * U * U; }
    const float U = T - 8.f; return 1810.f + 300.f * U + 110.f * U * U / (Falls - 8.f);
}

bool AFeyRiverScene::IsUnder(float T) const
{
    for (const FVector2D& D : Dunks) if (T >= D.X && T < D.Y) return true;
    return false;
}

void AFeyRiverScene::BuildStage()
{
    AFeywildSetting* S = AFeywildSetting::Enter(GetWorld());
    if (S) { S->BuildRiver(); S->SetShade(.05f); }
    ShoreAt = FeyWorld(ShoreSpot()); ShoreYawDeg = ShoreYaw();
    for (UAudioComponent* A : {Rush, Roar}) if (A) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(420.f); }
    if (Rush) Rush->FadeIn(.4f, .55f);
    if (Roar) { Roar->SetPitchMultiplier(.72f); Roar->FadeIn(.4f, .25f); }
    bStage = true;
}

FVector AFeyRiverScene::ChuckTarget() const
{
    const AChuckCharacter* C = Chuck.Get();
    return C ? C->GetActorLocation() + FVector(0, 0, 10.f) : FVector::ZeroVector;
}

void AFeyRiverScene::Aim(const FVector& From, const FVector& At, float FocalMM, float FStop, float Bias)
{
    UCameraComponent* Lens = Camera->GetCameraComponent();
    Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
    Lens->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(36.f / (2.f * FocalMM))));
    FPostProcessSettings& P = Lens->PostProcessSettings;
    Lens->PostProcessBlendWeight = 1.f;
    P.bOverride_DepthOfFieldFstop = true; P.DepthOfFieldFstop = FStop;
    P.bOverride_DepthOfFieldFocalDistance = true; P.DepthOfFieldFocalDistance = FMath::Max(10.f, static_cast<float>(FVector::Dist(From, At)));
    P.bOverride_DepthOfFieldSensorWidth = true; P.DepthOfFieldSensorWidth = 36.f;
    P.bOverride_AutoExposureBias = true; P.AutoExposureBias = Bias;
}

void AFeyRiverScene::UpdateChuck(float DeltaSeconds)
{
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    using EClip = AChuckCharacter::EClip;
    const float T = Clock;
    auto Loop = [&](EClip Clip, float Rate)
    {
        if (C->Base != Clip) C->SetClip(Clip, 0, .25f);
        const UAnimSequence* Seq = C->Clips[static_cast<int32>(Clip)];
        C->BaseTime = FMath::Fmod(C->BaseTime + DeltaSeconds * Rate, Seq ? FMath::Max(Seq->GetPlayLength(), .1f) : 1.f);
    };
    // Flailing, turned and rolled by the current, as the sweep at Phlegethos carried him.
    auto Tossed = [T](float Yaw, float Calm) { return FRotator((-12.f + 8.f * FMath::Sin(T * 2.6f)) * Calm, Yaw + 40.f * Calm * FMath::Sin(T * 1.2f), 14.f * Calm * FMath::Sin(T * 2.1f)); };
    const float Bob = 5.f * FMath::Sin(T * 6.3f);
    if (T < Falls)
    {
        const float S = RiverS(T);
        const FVector Mid = RiverAt(S), Side = RiverSide(S), Fwd = FVector::CrossProduct(Side, FVector::UpVector);
        // Under: down a hand's depth, eased in; out of it at once (the surfacings are sharp).
        float Dunk = 0.f;
        for (const FVector2D& D : Dunks) if (T >= D.X && T < D.Y) Dunk = Ease(Span(T, D.X, D.X + .25f));
        const float Across = 55.f * FMath::Sin(T * .9f) + 25.f * FMath::Sin(T * 2.3f + 1.f);
        const FVector Local = Mid + Side * Across + FVector(0, 0, Float + Bob - 46.f * Dunk);
        C->SetActorLocationAndRotation(FeyWorld(Local), Tossed(Fwd.Rotation().Yaw, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
        Loop(EClip::FallFlail, 1.f);
        return;
    }
    const float Weave = 30.f * FMath::Sin(T * 1.3f);
    FVector L; FRotator R = Tossed(180.f, 1.f);
    if (T < UpperLip) { L = FVector(StreamX0 - (StreamX0 - UpperLipX) * Span(T, Falls, UpperLip), FallY + Weave, UpperZ + Float + Bob); Loop(EClip::FallFlail, 1.f); }
    else if (T < Shelf)
    {
        // Over the upper lip, turning over as he drops onto the shelf.
        const float D = T - UpperLip;
        L = FVector(UpperLipX - (UpperLipX - ShelfX) * D / (Shelf - UpperLip), FallY + Weave, UpperZ + Float - Gravity * D * D);
        R = FRotator(-12.f - 75.f * FMath::Sin(PI * D / (Shelf - UpperLip)), 180.f + 20.f * D, 30.f * FMath::Sin(D * 5.f));
        Loop(EClip::Tumble, 1.f);
    }
    else if (T < LowerLip) { L = FVector(ShelfX - (ShelfX - LowerLipX) * Span(T, Shelf, LowerLip), FallY + Weave, ShelfZ + Float + Bob); Loop(EClip::FallFlail, 1.2f); }
    else if (T < Plunge)
    {
        // The big drop, out in front of the curtain, into the pond.
        const float D = T - LowerLip;
        L = FVector(LowerLipX - (LowerLipX - PlungeX) * D / (Plunge - LowerLip), FallY + Weave, FMath::Max(ShelfZ + Float - Gravity * D * D, WaterZ - 4.f));
        R = FRotator(-12.f - 95.f * FMath::Sin(PI * .5f * D / (Plunge - LowerLip)), 180.f - 25.f * D, 40.f * FMath::Sin(D * 4.f));
        Loop(EClip::Tumble, 1.1f);
    }
    else if (T < Surface)
    {
        // Under, all of him, and up.
        const float U = Span(T, Plunge, Surface);
        L = FVector(FMath::Lerp(PlungeX, SurfaceX, Ease(U)), FallY + Weave, FMath::Lerp(WaterZ + Float, WaterZ - 44.f, FMath::Sin(PI * FMath::Pow(U, .6f))));
        R = Tossed(170.f, 1.f);
        Loop(EClip::FallFlail, .7f);
        DriftFrom = FVector(SurfaceX, FallY + Weave, WaterZ + Float);
    }
    else if (T < Wash)
    {
        // The current calms, and carries him slowly in to the south shore.
        const FVector Shore = FeyLocal(ShoreAt), Dir = (Shore - DriftFrom).GetSafeNormal2D();
        const FVector Edge = Shore - Dir * 70.f;
        const float U = Span(T, Surface, Wash), E = 1.f - FMath::Pow(1.f - U, 2.2f);
        L = FMath::Lerp(DriftFrom, FVector(Edge.X, Edge.Y, WaterZ + Float), E) + FVector(0, 0, Bob * (1.f - U));
        R = Tossed(Dir.Rotation().Yaw, 1.f - .6f * U);
        Loop(EClip::FallFlail, 1.f - .55f * U);
    }
    else
    {
        // Set down on the shore: face down, still; then up onto all fours, and up (CrashLand: sprawled to .95 s, up by 2.2 s).
        const FVector Shore = FeyLocal(ShoreAt) + FVector(0, 0, 36.65f);
        const FVector Dir = (FeyLocal(ShoreAt) - DriftFrom).GetSafeNormal2D();
        const float U = Ease(Span(T, Wash, Wash + .35f));
        L = FMath::Lerp(Shore - Dir * 70.f, Shore, U);
        R = FRotator(0, ShoreYawDeg, 0);
        if (T < Stood)
        {
            if (C->Base != EClip::CrashLand) C->SetClip(EClip::CrashLand, .12f, .3f);
            C->BaseTime = T < Rise ? FMath::Min(.12f + (T - Wash), .95f) : .95f + (T - Rise) * (2.2f - .95f) / (Stood - Rise);
        }
        else Loop(EClip::Idle, 1.f);
    }
    C->SetActorLocationAndRotation(FeyWorld(L), R, false, nullptr, ETeleportType::TeleportPhysics);
}

void AFeyRiverScene::UpdateCamera()
{
    const float T = Clock;
    const FVector Him = ChuckTarget(), Up = FVector::UpVector;
    FVector From, At = Him;
    float Focal = 35.f, Stop = 4.f, Bias = 0.f;
    if (T < Falls)
    {
        const float S = RiverS(T);
        const FVector Side = RiverSide(S), Fwd = FVector::CrossProduct(Side, Up);
        if (T < Score)
        {
            // In the dark, close in front of him: the water and his head, barely.
            From = Him + Fwd * 135.f + Side * 45.f + Up * (12.f + 4.f * FMath::Sin(T * 1.7f));
            At = Him + Up * 4.f;
            Focal = 35.f; Stop = 2.2f; Bias = -1.f;
        }
        else if (T < Out)
        {
            // Behind him: the light ahead.
            From = Him - Fwd * 160.f - Side * 35.f + Up * 48.f;
            At = FeyWorld(RiverAt(RiverMouth)) + Up * 70.f;
            Focal = 28.f; Stop = 5.6f; Bias = -1.5f;
        }
        else if (T < Face)
        {
            // Outside the mouth, low over the water, on the dark opening: he comes out of it into the light; as he nears, the
            // view swings round behind him and follows him on, rising a little to the banks.
            const float Sc = RiverMouth + 650.f;
            const FVector Fixed = FeyWorld(RiverAt(Sc) + RiverSide(Sc) * 70.f + Up * 85.f);
            const FVector Trail = Him - Fwd * 320.f + Side * 70.f + Up * FMath::Lerp(90.f, 150.f, Ease(Span(T, 11.5f, Face)));
            const float Swing = Ease(Span(T, 10.4f, 11.8f));
            From = FMath::Lerp(Fixed, Trail, Swing);
            const FVector Mouth = FeyWorld(RiverAt(RiverMouth)) + Up * 110.f;
            At = FMath::Lerp(FMath::Lerp(Mouth, Him + Up * 12.f, Ease(Span(T, Out + .6f, Out + 1.6f))), Him + Fwd * 200.f + Up * 20.f, Swing);
            Focal = 24.f; Stop = 8.f;
        }
        else if (T < Ahead)
        {
            // Close on him among the banks, the sun on his face.
            From = Him + Fwd * 105.f + Side * 34.f + Up * 20.f;
            At = Him + Up * 16.f + Fwd * 10.f;
            Focal = 50.f; Stop = 2.f;
        }
        else
        {
            // Behind and above him: the river ahead quickening, the low sun.
            From = Him - Fwd * 380.f + Side * 60.f + Up * 210.f;
            At = Him + Fwd * 450.f + Up * 10.f;
            Focal = 24.f; Stop = 11.f;
        }
    }
    else if (T < Shelf)
    {
        // Low behind him on the stream as it runs to the lip, the glade opening below it; then at the lip, down at him.
        // (over the lip he drops below the stream's edge: the view comes up to the lip and looks down after him)
        const float Over = Ease(Span(T, UpperLip - .15f, UpperLip + .35f));
        const float X = FMath::Lerp(FMath::Max(static_cast<float>(FeyLocal(Him).X), UpperLipX) + 170.f, UpperLipX + 60.f, Over);
        From = FeyWorld(FVector(X, FallY - FMath::Lerp(45.f, 70.f, Over), UpperZ + FMath::Lerp(48.f, 170.f, Over)));
        At = FMath::Lerp(Him + FVector(-420.f, 0, -90.f), Him, Over);
        Focal = 24.f; Stop = 8.f;
    }
    else if (T < Drift)
    {
        // From the pond, up at the falls: the lip, him coming over it, the plunge, the water.
        From = FeyWorld(FVector(4300.f, 760.f, WaterZ + 65.f));
        const FVector Lip = FeyWorld(FVector(LowerLipX, FallY, LedgeTop - 40.f));
        At = FMath::Lerp(Lip, Him, Ease(Span(T, LowerLip - .1f, LowerLip + .35f)));
        Focal = 28.f; Stop = 8.f;
    }
    else if (T < Wash)
    {
        // Beside him, low on the water, as he drifts in.
        const FVector Dir = (ShoreAt - FeyWorld(DriftFrom)).GetSafeNormal2D(), Perp(-Dir.Y, Dir.X, 0.f);
        From = Him + Perp * 260.f - Dir * 70.f + Up * 40.f;
        Focal = 35.f; Stop = 4.f;
    }
    else if (T < Last)
    {
        // Low over the water, three-quarters to his side (the grass is behind him): set down on the shore, still, and up.
        const FVector Dir = FRotator(0, ShoreYawDeg, 0).Vector(), Perp(-Dir.Y, Dir.X, 0.f);
        const FVector Spot = ShoreAt + Perp * 170.f - Dir * 175.f;
        From = FVector(Spot.X, Spot.Y, FeyWorld(FVector(0, 0, WaterZ)).Z + 42.f);
        At = ShoreAt + Up * 16.f;
        Focal = 35.f; Stop = 2.8f;
    }
    else
    {
        // Behind him, rising to where the game's camera takes over: the glade ahead of him.
        const FVector Dir = FRotator(0, ShoreYawDeg, 0).Vector();
        const float U = Ease(Span(T, Last, End));
        From = FMath::Lerp(ShoreAt - Dir * 140.f + Up * 45.f, ShoreAt - Dir * 320.f + Up * 140.f, U);
        At = ShoreAt + Dir * 600.f + Up * 60.f;
        Focal = 28.f; Stop = 8.f;
    }
    Aim(From, At, Focal, Stop, Bias);
    if (Glow) { Glow->SetWorldLocation(From + (Him - From) * .3f + Up * 30.f); Glow->SetIntensity(T < Score ? .5f * Ease(Span(T, Light, Light + 2.5f)) : T < Out ? .3f : 0.f); }
}

void AFeyRiverScene::UpdateSound(float DeltaSeconds)
{
    const float T = Clock;
    const bool bUnderNow = IsUnder(T);
    if (bUnderNow != bUnder)
    {
        // Muffled under; at once sharp and loud as he comes up.
        for (UAudioComponent* A : {Rush, Roar}) if (A) { A->SetLowPassFilterEnabled(bUnderNow); A->SetLowPassFilterFrequency(420.f); }
        bUnder = bUnderNow;
    }
    const float RushWant = T < Score ? (bUnderNow ? .5f : 1.f) : T < Falls ? .8f : T < Plunge ? .7f : T < Surface ? .4f : FMath::Lerp(.3f, 0.f, Span(T, Surface + 1.f, Wash + .8f));
    const float RoarWant = T < Score ? (bUnderNow ? .2f : .38f) : T < Ahead ? .3f : T < Falls ? FMath::Lerp(.3f, .85f, Span(T, Ahead, Falls)) : 0.f;
    const float Rate = bUnderNow ? 6.f : 20.f;
    if (Rush) Rush->SetVolumeMultiplier(FMath::FInterpTo(Rush->VolumeMultiplier, RushWant, DeltaSeconds, Rate));
    if (Roar) Roar->SetVolumeMultiplier(FMath::FInterpTo(Roar->VolumeMultiplier, RoarWant, DeltaSeconds, T < Falls ? Rate : 3.f));
}

void AFeyRiverScene::PlaySplash(const FVector& World, float Volume, float Pitch)
{
    if (Splashes.Num()) UGameplayStatics::PlaySound2D(this, Splashes[SplashCount++ % Splashes.Num()], Volume, Pitch);
}

void AFeyRiverScene::Events(float From, float To)
{
    auto Crossed = [&](float At) { return From < At && At <= To; };
    APlayerController* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr;
    APlayerCameraManager* Lens = PC ? PC->PlayerCameraManager : nullptr;
    auto Cut = [Lens]() { if (Lens) Lens->SetGameCameraCutThisFrame(); };
    AFeywildSetting* S = AFeywildSetting::Get(GetWorld());
    if (!bStage) { BuildStage(); Cut(); return; }
    const FVector Him = ChuckTarget();
    if (Crossed(Light) && Lens) Lens->StartCameraFade(1.f, 0.f, 2.6f, FLinearColor::Black, false, false);
    for (int32 I = 0; I < 4; ++I)
        if (Crossed(Dunks[I].Y)) { PlaySplash(Him, FMath::FRandRange(.7f, .95f), FMath::FRandRange(.86f, 1.1f)); ++Surfacings; }
    if (Crossed(Score) && Music) { Music->Play(SongStart); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_SCORE at=%.2f song=%.1f"), To, SongStart); }
    for (float At : {Out, Face, Ahead, Falls, Shelf, Drift, Wash, Last}) if (Crossed(At)) Cut();
    if (Crossed(Falls) && S) { S->ShowRiver(false); S->SetShade(1.f); }
    if (Crossed(Shelf)) PlaySplash(Him, .75f, .95f);
    if (Crossed(Plunge))
    {
        if (S) S->Splash(Him, 1.f);
        PlaySplash(Him, 1.f, .72f); PlaySplash(Him, .8f, .95f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_PLUNGE at=%.2f where=%s"), To, *FeyLocal(Him).ToString());
    }
    if (Crossed(Surface)) { if (S) S->Splash(Him, .35f); PlaySplash(Him, .55f, 1.08f); ++Surfacings; }
    if (Crossed(Wash))
    {
        if (S) S->Splash(Him, .2f);
        PlaySplash(Him, .4f, 1.15f);
        if (LandSound) UGameplayStatics::PlaySound2D(this, LandSound, .35f, .8f);
        bWashed = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_WASHED at=%.2f shore=%s"), To, *FeyLocal(ShoreAt).ToString());
    }
    if (Crossed(Rise)) bRose = true;
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckFeyRiverTest")))
    {
        AChuckCharacter* C = Chuck.Get();
        for (float At : ShotTimes)
            if (Crossed(At))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FeyRiver/T%04.1f_%s.png"), At, ShotName(At)), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_SHOT t=%.1f shot=%s chuck=%s camera=%s"), At, ShotName(At), C ? *FeyLocal(C->GetActorLocation()).ToString() : TEXT("-"), *FeyLocal(Camera->GetActorLocation()).ToString());
            }
        if (Crossed(10.f)) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_MUSIC playing=%d"), IsMusicPlaying());
    }
}

void AFeyRiverScene::Finish()
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_END at=%.2f skipped=%d surfacings=%d washed=%d rose=%d music_playing=%d"), Clock, bSkipping, Surfacings, bWashed, bRose, IsMusicPlaying());
    for (int32 I = 0; I < 3; ++I)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RIVER_FPS part=%s average=%.1f worst_frame_ms=%.1f"), I == 0 ? TEXT("river") : I == 1 ? TEXT("falls") : TEXT("shore"),
            PartTime[I] > 0 ? PartFrames[I] / PartTime[I] : 0.f, PartWorst[I] * 1000.f);
    if (Music) Music->Stop();
    for (UAudioComponent* A : {Rush, Roar}) if (A) A->Stop();
    if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld())) { S->ShowRiver(false); S->SetShade(1.f); }
    AChuckCharacter* C = Chuck.Get();
    if (C) C->EndFeyRiver();
    if (auto* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr)
    {
        // Into the game's own camera behind him; after a skip, up out of the black.
        PC->SetViewTargetWithBlend(C, bSkipping ? 0.f : 1.2f, VTBlend_EaseInOut, 2.f);
        if (bSkipping && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.f, FLinearColor::Black, false, false);
    }
    if (Camera) Camera->SetLifeSpan(2.f);
    Destroy();
}

void AFeyRiverScene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    const float Before = Clock;
    // (The first frame builds the stage under the black: its hitch is not scene time.)
    Clock += bStage ? DeltaSeconds : 0.f;
    Events(Before, Clock);
    if (bSkipping && Clock - SkipAt >= .5f) { Finish(); return; }
    if (Clock >= End) { Finish(); return; }
    // In the tunnel the sky's light is all but gone; at the glimpse some of the day outside shows; out of it, all of it.
    if (AFeywildSetting* S = AFeywildSetting::Get(GetWorld()); S && Clock < Falls) S->SetShade(Clock < Score ? .05f : FMath::Lerp(.3f, 1.f, Ease(Span(Clock, Out, Out + 1.4f))));
    const int32 Part = Clock < Falls ? 0 : Clock < Wash ? 1 : 2;
    if (Clock > .5f) { PartFrames[Part] += 1; PartTime[Part] += DeltaSeconds; PartWorst[Part] = FMath::Max(PartWorst[Part], DeltaSeconds); }
    UpdateChuck(DeltaSeconds);
    UpdateSound(DeltaSeconds);
    if (Camera && Chuck.IsValid()) UpdateCamera();
}
