#include "FallCutscene.h"
#include "ChultLanding.h"
#include "AstralSummon.h"
#include "ChuckAnimInstance.h"
#include "ChuckCharacter.h"
#include "ChuckClipData.h"
#include "DockGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"

namespace
{
    float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
    float Span(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }
    // Bar lines of the score, for the test's report.
    const TCHAR* ShotName(float T)
    {
        return T < AFallCutscene::Switch ? TEXT("Pantry") : T < AFallCutscene::Reveal ? TEXT("Tumble") : T < AFallCutscene::CloseUp ? TEXT("OpenSky") : T < AFallCutscene::Down ? TEXT("Face")
            : T < AFallCutscene::Below ? TEXT("CloudDeck") : T < AFallCutscene::Canopy ? TEXT("UnderCloud") : T < AFallCutscene::Crash ? TEXT("FromCanopy")
            : T < AFallCutscene::Impact ? TEXT("Crash") : T < AFallCutscene::Look ? TEXT("Floor") : T < AFallCutscene::Drag ? TEXT("Look")
            : T < AFallCutscene::PullBack ? TEXT("Drag") : TEXT("PullBack");
    }
    // Test captures (-ChuckFallCutsceneTest), song time.
    const float ShotTimes[] = {-4.75f, -2.6f, -.6f, 3.f, 6.5f, 10.f, 15.f, 16.9f, 18.6f, 20.3f, 22.6f, 24.6f, 25.4f, 26.38f, 26.6f, 27.4f, 29.f, 29.5f, 30.4f, 33.1f, 35.f, 37.2f, 41.f, 44.f, 46.2f};
    const FLinearColor SkyWhite(.80f, .92f, .95f);
}

// 25 km from the docks, under a part of the (world-anchored) cloud deck he has to pass through.
FVector AFallCutscene::LandingSpot() { return FVector(400000, 2500000, 0); }

AFallCutscene::AFallCutscene()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AFallCutscene* AFallCutscene::Start(AChuckCharacter* Chuck)
{
    UWorld* World = Chuck ? Chuck->GetWorld() : nullptr;
    if (!World) return nullptr;
    auto* Scene = World->SpawnActor<AFallCutscene>(FVector::ZeroVector, FRotator::ZeroRotator);
    Scene->Chuck = Chuck;
    Scene->PantryStart = Chuck->GetActorLocation();
    Scene->PantryYaw = Chuck->GetActorRotation().Yaw;
    Chuck->AddTickPrerequisiteActor(Scene);
    // The cellar's view stays at the hole and watches him go.
    Scene->Camera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator);
    Scene->Camera->GetCameraComponent()->bConstrainAspectRatio = false;
    if (auto* PC = Cast<APlayerController>(Chuck->GetController())) PC->SetViewTarget(Scene->Camera);
    if (auto* Mode = World->GetAuthGameMode<ADockGameMode>()) Mode->FadeOutMusic(.9f);
    if (auto* Song = LoadObject<USoundWave>(nullptr, TEXT("/Game/Art/Audio/SW_FallToChult.SW_FallToChult")))
    {
        Scene->Music = UGameplayStatics::CreateSound2D(World, Song, .6f, 1.f, 0.f, nullptr, false, false);
    }
    if (auto* Air = LoadObject<USoundWave>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_FallWind_00.SFX_FallWind_00")))
        Scene->Wind = UGameplayStatics::CreateSound2D(World, Air, 0.f, 1.f, 0.f, nullptr, false, false);
    for (int32 I = 0; I < 3; ++I)
        if (auto* Leafy = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_LeafCrash_%02d.SFX_LeafCrash_%02d"), I, I)))
            Scene->LeafCrashes.Add(Leafy);
    Scene->LandSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_Land_02.SFX_Land_02"));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_START at=%s music=%d wind=%d crashes=%d"), *Scene->PantryStart.ToString(), Scene->Music != nullptr, Scene->Wind != nullptr, Scene->LeafCrashes.Num());
    return Scene;
}

bool AFallCutscene::IsMusicPlaying() const { return Music && Music->IsPlaying(); }

void AFallCutscene::Skip()
{
    if (bSkipping || bFinished) return;
    SkipAt = Clock; bSkipping = true;
    if (auto* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr)
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .45f, FLinearColor::Black, false, true);
    if (Music) Music->FadeOut(.5f, 0.f);
    if (Wind) Wind->FadeOut(.3f, 0.f);
    if (Flap) Flap->FadeOut(.3f, 0.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_SKIP at=%.2f"), Clock);
}

float AFallCutscene::Altitude(float T)
{
    // Monotone cubic (Fritsch-Carlson) through the descent's keys: slow and
    // vast above the clouds, fast through and under them, then the canopy
    // coming up at him. Cloud deck 2.0-3.1 km.
    static const float Ts[] = {Switch, Reveal, CloseUp, Down, 16.5f, 19.3f, Canopy, Crash};
    static const float Hs[] = {585000.f, 548000.f, 455000.f, 385000.f, 318000.f, 200000.f, 26000.f, 4300.f};
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

void AFallCutscene::BuildStage()
{
    UWorld* World = GetWorld();
    const FVector G = LandingSpot();
    auto Attach = [this](USceneComponent* C, const FVector& At, const FRotator& Rot = FRotator::ZeroRotator)
    {
        SkyParts.Add(C);   // the open sky's environment, handed over to Chult's at the crash
        C->SetupAttachment(RootComponent);
        C->SetWorldLocationAndRotation(At, Rot);
        C->RegisterComponent();
    };
    // The open sky: a slightly teal atmosphere (the pantry's sky, and the
    // 2D game's) with a real cloud deck 2.0-3.1 km over the jungle.
    auto* Atmosphere = NewObject<USkyAtmosphereComponent>(this);
    Atmosphere->SetRayleighScattering(FLinearColor(.13f, .43f, .66f));
    Atmosphere->SetMieScatteringScale(.006f);
    Atmosphere->SetHeightFogContribution(1.f);
    Attach(Atmosphere, G);
    auto* Clouds = NewObject<UVolumetricCloudComponent>(this);
    Clouds->SetLayerBottomAltitude(2.0f);
    Clouds->SetLayerHeight(1.1f);
    if (auto* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Cutscenes/FallToChult/MI_FallClouds.MI_FallClouds")))
    {
        // A fuller, denser deck than the engine default (coverage -0.2, density
        // 0.008, which only reads as cloud kilometres away), so he passes through it.
        auto* Deck = UMaterialInstanceDynamic::Create(CloudMaterial, this);
        Deck->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), CloudCoverage);
        Deck->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), CloudDensity);
        Clouds->SetMaterial(Deck);
    }
    Attach(Clouds, G);
    // Late-afternoon sun, low enough to slant shafts through the canopy and
    // backlight him from across the clearing.
    Sun = NewObject<UDirectionalLightComponent>(this);
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetIntensity(8.f);
    Sun->SetLightColor(FLinearColor(1.f, .9f, .78f));
    Sun->SetAtmosphereSunLight(true);
    Sun->bCastCloudShadows = true;
    Sun->DynamicShadowDistanceMovableLight = 9000.f;
    Sun->DynamicShadowCascades = 3;
    Sun->SetVolumetricScatteringIntensity(8.f);
    Attach(Sun, G, FRotator(-30.f, 20.f, 0.f));
    SkyLight = NewObject<USkyLightComponent>(this);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->bRealTimeCapture = true;
    SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
    SkyLight->SetIntensity(1.4f);
    Attach(SkyLight, G);
    Fog = NewObject<UExponentialHeightFogComponent>(this);
    // Thick only in the jungle's lowest few hundred metres: clear air above.
    Fog->SetFogDensity(.05f);
    Fog->SetFogHeightFalloff(20.f);
    Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogScatteringDistribution(.75f);
    Fog->SetVolumetricFogExtinctionScale(2.f);
    Fog->SetVolumetricFogDistance(7000.f);
    Fog->SetFogInscatteringColor(FLinearColor(.12f, .2f, .16f));
    Attach(Fog, G);
    auto* Post = NewObject<UPostProcessComponent>(this);
    Post->bUnbound = true;
    Post->Priority = 100.f;
    FPostProcessSettings& P = Post->Settings;
    P.bOverride_AutoExposureMethod = true; P.AutoExposureMethod = AEM_Histogram;
    P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = -2.f;
    P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = 10.f;
    P.bOverride_AutoExposureSpeedUp = true; P.AutoExposureSpeedUp = 1.5f;
    P.bOverride_AutoExposureSpeedDown = true; P.AutoExposureSpeedDown = 1.f;
    P.bOverride_AutoExposureBias = true; P.AutoExposureBias = 0.f;
    P.bOverride_BloomIntensity = true; P.BloomIntensity = .8f;
    P.bOverride_LensFlareIntensity = true; P.LensFlareIntensity = .35f;
    P.bOverride_VignetteIntensity = true; P.VignetteIntensity = .45f;
    P.bOverride_FilmGrainIntensity = true; P.FilmGrainIntensity = .05f;
    P.bOverride_SceneFringeIntensity = true; P.SceneFringeIntensity = .4f;
    P.bOverride_MotionBlurAmount = true; P.MotionBlurAmount = .35f;
    P.bOverride_AmbientOcclusionIntensity = true; P.AmbientOcclusionIntensity = .6f;
    P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.08f);
    Attach(Post, G);

    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto Mesh = [](const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"), Name, Name)); };
    auto Material = [](const TCHAR* Name) { return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"), Name, Name)); };
    auto Set = [this](UStaticMesh* M, UMaterialInterface* Override = nullptr, bool bShadow = true)
    {
        auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        H->SetStaticMesh(M);
        if (Override) H->SetMaterial(0, Override);
        H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        H->SetCastShadow(bShadow);
        H->SetupAttachment(RootComponent);
        H->RegisterComponent();
        Jungle.Add(H);
        return H;
    };
    // The floor round the landing, and the jungle running away to the
    // horizon: plain components (a single kilometres-wide instance was culled).
    auto Plain = [this](UStaticMesh* M, UMaterialInterface* With, const FTransform& At)
    {
        auto* C = NewObject<UStaticMeshComponent>(this);
        C->SetStaticMesh(M);
        C->SetMaterial(0, With);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->SetupAttachment(RootComponent);
        C->SetWorldTransform(At);
        C->RegisterComponent();
    };
    Plain(Cube, Material(TEXT("M_FallJungleFar")), FTransform(FRotator::ZeroRotator, G - FVector(0, 0, 80), FVector(400000, 400000, 1)));
    FRandomStream R(1994);
    // Through the deck: soft puffs of mist packed round his column, rushing
    // up past him as the 2D game's clouds scrolled past.
    {
        Puffs = Set(Sphere, Material(TEXT("M_FallCloudPuff")), false);
        for (int32 I = 0; I < 150; ++I)
        {
            const float Z = R.FRandRange(196000.f, 312000.f), A = R.FRandRange(0, 2 * PI), D = FMath::Sqrt(R.FRand()) * 5500.f + 350.f;
            const float Size = R.FRandRange(10.f, 34.f);
            Puffs->AddInstance(FTransform(FRotator(0, R.FRandRange(0, 360), 0), G + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Z), FVector(Size, Size * R.FRandRange(.7f, 1.3f), Size * R.FRandRange(.45f, .7f))), true);
        }
    }
    // The canopy seen from the air beyond the landing's own 90 m of forest:
    // crowns, on trunks out to 150 m, thinning into the far material at 600 m.
    auto* Crowns = Set(Mesh(TEXT("SM_FallCrown")));
    auto* Trunks = Set(Cylinder, Material(TEXT("M_FallBark")));
    for (float Y = -60000; Y <= 60000; Y += 750)
        for (float X = -60000; X <= 60000; X += 750)
        {
            const FVector2D At(X + R.FRandRange(-300, 300), Y + R.FRandRange(-300, 300));
            const float D = At.Size();
            if (D < 9200 || D > 60000 || R.FRand() < FMath::Clamp((D - 30000.f) / 30000.f, 0.f, .9f)) continue;
            const float Scale = (D > 15000 ? 1.35f : 1.f) * R.FRandRange(.8f, 1.25f), Top = R.FRandRange(3000, 4100);
            Crowns->AddInstance(FTransform(FRotator(0, R.FRandRange(0, 360), 0), G + FVector(At.X, At.Y, Top), FVector(Scale)), true);
            if (D < 15000)
            {
                const float Girth = FMath::Pow(R.FRandRange(.35f, 1.f), 2.f) * 2.f + .5f;
                Trunks->AddInstance(FTransform(FRotator(R.FRandRange(-2, 2), R.FRandRange(0, 360), R.FRandRange(-2, 2)), G + FVector(At.X, At.Y, Top * .5f - 100), FVector(Girth, Girth * R.FRandRange(.85f, 1.15f), Top / 100.f)), true);
            }
        }
    // The clearing he lands in is playable Chult itself (Codex's ChultLanding:
    // the same trees, understory, wet dressing and floor), so the end of the
    // cutscene and the start of play are one place. Its lighting comes in as
    // he crashes into the canopy.
    Landing = GetWorld()->SpawnActor<AChultLanding>();
    Landing->BuildSet();
    // Leaves shaken loose by his fall.
    FallingLeaves = NewObject<UInstancedStaticMeshComponent>(this);
    FallingLeaves->SetStaticMesh(Mesh(TEXT("SM_FallLeafCard")));
    FallingLeaves->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallingLeaves->SetCastShadow(false);
    FallingLeaves->SetupAttachment(RootComponent);
    FallingLeaves->RegisterComponent();
    int32 Instances = 0;
    for (auto* H : Jungle) Instances += H->GetInstanceCount();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_STAGE sets=%d instances=%d crowns=%d trunks=%d landing=%d"), Jungle.Num(), Instances, Crowns->GetInstanceCount(), Trunks->GetInstanceCount(), Landing != nullptr);
}

void AFallCutscene::HideDocks()
{
    // Nothing of Waterdeep follows him down: its geometry, lights, fog,
    // post-process and sounds go dark (the demo ends after this).
    int32 Hidden = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (A == this || A == Camera || A == Chuck.Get() || A->GetAttachParentActor() == Chuck.Get() || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>() || A->IsA<AInfo>() || A->IsA<AAstralSummon>()) continue;
        if (auto* Volume = Cast<APostProcessVolume>(A)) Volume->bEnabled = false;
        A->SetActorHiddenInGame(true);
        TInlineComponentArray<UActorComponent*> Components(A);
        for (UActorComponent* C : Components)
        {
            if (auto* Sound = Cast<UAudioComponent>(C)) Sound->Stop();
            else if (C->IsA<ULightComponentBase>() || C->IsA<UExponentialHeightFogComponent>() || C->IsA<USkyAtmosphereComponent>() || C->IsA<UVolumetricCloudComponent>())
                Cast<USceneComponent>(C)->SetVisibility(false);
        }
        ++Hidden;
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_DOCKS_HIDDEN actors=%d"), Hidden);
}

void AFallCutscene::Burst(const FVector& At, int32 Count, float Speed)
{
    for (int32 I = 0; I < Count; ++I)
    {
        FLeaf L;
        L.Position = At + FMath::VRand() * FMath::FRandRange(10.f, 90.f);
        L.Velocity = FMath::VRand() * FMath::FRandRange(.3f, 1.f) * Speed + FVector(0, 0, Speed * .25f);
        L.Rotation = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
        L.Spin = FRotator(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f));
        L.Age = 0; L.Life = FMath::FRandRange(5.f, 9.f);
        Leaves.Add(L);
    }
}

void AFallCutscene::UpdateLeaves(float DeltaSeconds)
{
    if (!FallingLeaves) return;
    const float Floor = LandingSpot().Z + 2.f;
    FallingLeaves->ClearInstances();
    for (int32 I = Leaves.Num() - 1; I >= 0; --I)
    {
        FLeaf& L = Leaves[I];
        L.Age += DeltaSeconds;
        if (L.Age > L.Life) { Leaves.RemoveAtSwap(I); continue; }
        // Thrown out, then caught by the air: a slow fluttering sink.
        L.Velocity *= FMath::Max(0.f, 1.f - 2.5f * DeltaSeconds);
        L.Velocity.Z = FMath::Max(L.Velocity.Z - 300.f * DeltaSeconds, -60.f);
        L.Position += (L.Velocity + FVector(FMath::Sin(L.Age * 3.f + I) * 30.f, FMath::Cos(L.Age * 2.3f + I) * 30.f, 0)) * DeltaSeconds;
        if (L.Position.Z <= Floor) { L.Position.Z = Floor; L.Velocity = FVector::ZeroVector; L.Spin = FRotator::ZeroRotator; L.Rotation.Pitch = 0; L.Rotation.Roll = 0; }
        L.Rotation += L.Spin * DeltaSeconds;
    }
    // None right at the lens, where a leaf is only a blur.
    const FVector Lens = Camera ? Camera->GetActorLocation() : FVector::ZeroVector;
    for (const FLeaf& L : Leaves) if (FVector::DistSquared(L.Position, Lens) > FMath::Square(160.f)) FallingLeaves->AddInstance(FTransform(L.Rotation, L.Position, FVector(FMath::Min(1.f, L.Age * 6.f) * 1.4f)), true);
}

FVector AFallCutscene::ChuckTarget() const
{
    const AChuckCharacter* C = Chuck.Get();
    return C ? C->GetMesh()->GetComponentLocation() + C->GetMesh()->GetComponentQuat().RotateVector(FVector(0, 0, 32.f)) : FVector::ZeroVector;
}

void AFallCutscene::Aim(const FVector& From, const FVector& At, float FocalMM, float FStop)
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

void AFallCutscene::UpdateChuck(float DeltaSeconds)
{
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    using EClip = AChuckCharacter::EClip;
    const float T = Clock;
    const FVector G = LandingSpot();
    const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (T < Switch)
    {
        // Down the cellar's sky well, tipping forward as he goes.
        const float U = T + Lead;   // seconds since he dropped through the hole
        C->SetActorLocationAndRotation(PantryStart - FVector(0, 0, 250.f * U + 250.f * U * U), FRotator(-55.f * Ease(U / (Switch + Lead)), PantryYaw + 20.f * U, 0));
        C->BaseTime += DeltaSeconds * .6f;
        return;
    }
    if (T < Crash)
    {
        // The long fall: belly down, spread, turning slowly in the wind,
        // drifting a little until the canopy.
        const float Drift = 1.f - Ease(Span(T, 19.f, Crash));
        const FVector Off(FMath::Sin(T * .41f) * 420.f * Drift, FMath::Cos(T * .33f) * 380.f * Drift, Altitude(T) + Half);
        // Out of the hole he tumbles, then spreads and steadies before the score.
        const float Wild = 1.f - Ease(Span(T, Switch + .6f, Reveal - .4f));
        const FRotator Body(-80.f + 8.f * FMath::Sin(T * 1.1f) + Wild * 150.f * FMath::Sin(T * 2.3f), 40.f + 11.f * T + Wild * 60.f * T, 9.f * FMath::Sin(T * .8f) + Wild * 100.f * FMath::Sin(T * 1.7f + 1.f));
        C->SetActorLocationAndRotation(G + Off, Body);
        C->BaseTime += DeltaSeconds * .5f;
        return;
    }
    if (T < Impact)
    {
        // Through the canopy: three branches knock him about, each blow
        // stalling him before he drops on.
        static const float Keys[] = {Crash, 25.35f, 25.80f, Impact};
        static const float Heights[] = {4300.f, 3000.f, 1750.f, 0.f};
        int32 K = 0;
        while (T > Keys[K + 1]) ++K;
        const float U = Span(T, Keys[K], Keys[K + 1]);
        const float Fall = K == 2 ? U * U : 1.f - FMath::Square(1.f - U) * .4f - .6f * (1.f - U);
        const float Z = FMath::Lerp(Heights[K], Heights[K + 1], FMath::Clamp(Fall, 0.f, 1.f));
        const FVector Jolt(FMath::Sin(K * 2.1f + U * 2.f) * 45.f * (1.f - U), FMath::Cos(K * 1.7f) * 35.f * (1.f - U), 0);
        const float Spin = Span(T, Crash, Impact);
        // Out of the last branch face down, spread, for the ground.
        const FRotator Body = FMath::Lerp(FRotator(-80.f + 220.f * Spin, 40.f + 11.f * Crash + 160.f * Spin, 40.f * FMath::Sin(Spin * 9.f)), FRotator(-80.f, ProneYaw, 25.f), Ease(Span(T, 25.9f, Impact)));
        const FVector Touchdown = FRotator(0, ProneYaw, 0).Vector() * -38.f * Span(T, 25.9f, Impact);   // where the skid starts
        C->SetActorLocationAndRotation(G + Touchdown + Jolt * (1.f - Ease(Span(T, 25.9f, Impact))) + FVector(0, 0, Z + FMath::Lerp(Half, ProneHeight, Span(T, 25.9f, Impact))), Body);
        C->BaseTime += DeltaSeconds * 1.2f;
        return;
    }
    // On the jungle floor.
    const float Clip = C->Clips[static_cast<int32>(EClip::Summon)]->GetPlayLength();
    const float Rise = Summon + AAstralSummon::SummonPeak;
    if (T < Rise - SwapLead)
    {
        // The impact from that height: flat on his front, a hard bounce and a
        // short skid, a smaller second bounce, then nothing. He lies where he
        // fell, spread as he fell, until the light comes for him.
        const float Since = T - Impact;
        const float Lift = Since < .3f ? 24.f * FMath::Sin(PI * Since / .3f) : Since < .46f ? 6.f * FMath::Sin(PI * (Since - .3f) / .16f) : 0.f;
        // (He skids to a stop on the spot he will rise from.)
        const FVector Skid = FRotator(0, ProneYaw, 0).Vector() * -38.f * FMath::Exp(-Since * 7.f);
        // Sprawled: the hanging pose (arms overhead, legs straight) laid face down.
        if (C->Base != EClip::Hang) C->SetClip(EClip::Hang, 0, .06f);
        C->BaseTime = 0;
        const float Settle = FMath::Exp(-Since * 4.5f);
        const FRotator Body(-90.f + 10.f * Settle * FMath::Sin(Since * 22.f), ProneYaw + 12.f * (1.f - Settle), 14.f + 20.f * Settle * FMath::Cos(Since * 17.f));
        C->SetActorLocationAndRotation(G + Skid + FVector(0, 0, ProneHeight + Lift), Body);
        return;   // the spread pose is held: no breath, no twitch
    }
    float Yaw = 0;
    const float TurnLength = C->Clips[static_cast<int32>(EClip::TurnLeft90)]->GetPlayLength();
    auto Turn = [&](float From, float Start, float Delta)
    {
        const float Frame = FMath::Clamp((T - Start) * 30.f, 0.f, static_cast<float>(ChuckClipData::TurnYawFrames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), ChuckClipData::TurnYawFrames - 1);
        return From + FMath::Lerp(ChuckClipData::TurnYaw[I], ChuckClipData::TurnYaw[I + 1], Frame - I) / 90.f * Delta;
    };
    // He looks off to his left, holds, and comes back to the clearing.
    constexpr float LookLeft = Look + .2f, LookBack = Look + 1.9f;
    if (T >= LookBack) Yaw = Turn(-70.f, LookBack, 70.f);
    else if (T >= LookLeft) Yaw = Turn(0.f, LookLeft, -70.f);
    C->SetActorLocationAndRotation(G + FVector(0, 0, Half), FRotator(0, Yaw, 0));
    // At the light's peak he is curled where he lay, and rises out of it.
    if (T < Rise) { if (C->Base != EClip::Summon) C->SetClip(EClip::Summon, 0, 0.f); C->BaseTime = 0; }
    else if (T < Rise + Clip) { C->BaseTime = T - Rise; }
    else if (T >= LookLeft && T < LookLeft + TurnLength) { if (C->Base != EClip::TurnLeft90) C->SetClip(EClip::TurnLeft90, 0, .15f); C->BaseTime = T - LookLeft; }
    else if (T >= LookBack && T < LookBack + TurnLength) { if (C->Base != EClip::TurnRight90) C->SetClip(EClip::TurnRight90, 0, .15f); C->BaseTime = T - LookBack; }
    else { if (C->Base != EClip::Idle) C->SetClip(EClip::Idle, 0, .3f); else C->BaseTime += DeltaSeconds; }
}

void AFallCutscene::UpdateCamera()
{
    const float T = Clock;
    const FVector G = LandingSpot();
    const FVector C = ChuckTarget();
    FVector From, At = C;
    float Focal = 35.f, Stop = 4.f;
    auto Orbit = [](float YawDeg, float PitchDeg, float Dist) { return FRotator(PitchDeg, YawDeg, 0).Vector() * Dist; };
    if (T < Switch)
    {
        const FVector2D Hole = FVector2D(PantryStart.X, PantryStart.Y);
        From = FVector(Hole.X - 40.f, Hole.Y - 70.f, PantryStart.Z + 170.f);
        Focal = 28.f; Stop = 8.f;
    }
    else if (T < Reveal)
    {
        // Out of the white: tumbling, close, before the score; the camera
        // buffeted with him.
        const float U = Span(T, Switch, Reveal);
        From = C + Orbit(200.f + 25.f * U, 14.f, FMath::Lerp(240.f, 300.f, U)) + FVector(FMath::Sin(T * 13.f), FMath::Sin(T * 11.f + 1.f), FMath::Sin(T * 17.f + 2.f)) * 4.f;
        Focal = 32.f; Stop = 4.f;
    }
    else if (T < CloseUp)
    {
        // The score's first hit: small in a vast sky over the cloud deck, the sun on him.
        const float U = Ease(Span(T, Reveal, CloseUp));
        From = C + Orbit(192.f + 16.f * U, 4.f, FMath::Lerp(620.f, 430.f, U));
        At = C + FVector(0, 0, 40.f);
        Focal = 24.f; Stop = 11.f;
    }
    else if (T < Down)
    {
        // His face, from under him, turning with him.
        const FVector Face = Chuck->GetActorForwardVector();
        const FVector Side = Chuck->GetActorRightVector();
        const float U = Span(T, CloseUp, Down);
        From = C + Face * FMath::Lerp(120.f, 95.f, U) + Side * 25.f + FVector(0, 0, -10.f);
        At = C + Face * 20.f;
        Focal = 50.f; Stop = 2.2f;
    }
    else if (T < Below)
    {
        // Over his shoulder, down at the cloud deck coming up.
        From = C + FVector(-70.f, 50.f, 240.f);
        At = C + FVector(30.f, -20.f, -500.f);
        Focal = 24.f; Stop = 11.f;
    }
    else if (T < Canopy)
    {
        // Out under the deck, the jungle spread below.
        const float U = Span(T, Below, Canopy);
        From = C + Orbit(110.f + 25.f * U, 55.f, 330.f);
        At = C + FVector(0, 0, -120.f);
        Focal = 32.f; Stop = 8.f;
    }
    else if (T < Crash)
    {
        // From the top of the canopy, up at him coming down out of the clouds.
        From = G + FVector(520.f, -360.f, 4450.f);
        Focal = 28.f; Stop = 11.f;
    }
    else if (T < Impact)
    {
        // Down through the leaves with him.
        const float U = Span(T, Crash, Impact);
        From = C + FVector(150.f, -130.f, FMath::Lerp(160.f, 70.f, U));
        Focal = 24.f; Stop = 5.6f;
    }
    else if (T < Look)
    {
        // The floor at his height: the light, the giants behind him.
        const float U = Ease(Span(T, Impact + .3f, Look));
        const FVector Settle = FMath::Lerp(C + FVector(150.f, -130.f, 70.f), G + FVector(300.f, -110.f, 32.f), Ease(Span(T, Impact, Impact + .35f)));
        From = FMath::Lerp(Settle, G + FVector(200.f, -75.f, 30.f), U);
        At = G + FVector(0, 0, 26.f);
        Focal = 24.f; Stop = 4.f;
    }
    else if (T < Drag)
    {
        const float U = Ease(Span(T, Look, Drag));
        From = G + FVector(FMath::Lerp(170.f, 140.f, U), FMath::Lerp(-95.f, -70.f, U), 44.f);
        At = C + FVector(0, 0, 10.f);
        Focal = 35.f; Stop = 2.8f;
    }
    else if (T < PullBack)
    {
        // Close on his face for the drag.
        const FVector Head = Chuck->GetMesh()->GetSocketLocation(TEXT("socket_cigarette"));
        const float U = Span(T, Drag, PullBack);
        // Three-quarter, so the breath crosses the frame instead of the lens.
        From = Head + FVector(FMath::Lerp(60.f, 52.f, U), FMath::Lerp(-75.f, -66.f, U), 6.f);
        At = Head + FVector(4.f, 0, 1.f);
        Focal = 50.f; Stop = 2.f;
    }
    else
    {
        // Back and low: a small rat at the foot of the giants.
        const float U = Ease(Span(T, PullBack, End));
        From = FMath::Lerp(G + FVector(170.f, -50.f, 55.f), G + FVector(1150.f, -310.f, 250.f), U);
        At = FMath::Lerp(G + FVector(0, 0, 35.f), G + FVector(-400.f, 80.f, 400.f), U);
        Focal = FMath::Lerp(35.f, 24.f, U); Stop = FMath::Lerp(4.f, 11.f, U);
    }
    if (T < ShakeUntil)
    {
        const float K = FMath::Square((ShakeUntil - T) / ShakeTime);
        From += FVector(FMath::Sin(T * 91.f), FMath::Sin(T * 77.f + 1.f), FMath::Sin(T * 113.f + 2.f)) * 14.f * K;
    }
    Aim(From, At, Focal, Stop);
}

void AFallCutscene::UpdateLight()
{
    if (!Sun || Clock >= Crash) return;
    // Toward evening as he nears Chult (the 2D sky darkens to dusk).
    const float Dusk = Ease(Span(Clock, 17.f, Crash));
    Sun->SetLightColor(FMath::Lerp(FLinearColor(1.f, .9f, .78f), FLinearColor(1.f, .76f, .52f), Dusk));
    // Under the canopy the light is what gets through it.
    Sun->SetIntensity(FMath::Lerp(FMath::Lerp(11.f, 6.5f, Dusk), 4.5f, Ease(Span(Clock, Crash, Impact))));
    if (Wind && Clock < Crash)
    {
        // The rush of air rises with his speed, under the score.
        const float Rush = Clock < Reveal ? .95f : Clock < CloseUp ? .25f : Clock < Down ? .4f : Clock < Below ? .7f : Clock < Canopy ? .9f : FMath::Lerp(.35f, .8f, Span(Clock, Canopy, Crash));
        Wind->SetVolumeMultiplier(FMath::FInterpTo(Wind->VolumeMultiplier, Rush, GetWorld()->GetDeltaSeconds(), 2.f));
        if (Flap) Flap->SetVolumeMultiplier(FMath::FInterpTo(Flap->VolumeMultiplier, Clock < Reveal ? .8f : Clock < CloseUp ? .2f : Clock < Down ? .35f : .55f, GetWorld()->GetDeltaSeconds(), 2.f));
    }
}

void AFallCutscene::Events(float From, float To)
{
    auto Crossed = [&](float At) { return From < At && At <= To; };
    APlayerController* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr;
    APlayerCameraManager* Lens = PC ? PC->PlayerCameraManager : nullptr;
    auto Cut = [Lens]() { if (Lens) Lens->SetGameCameraCutThisFrame(); };
    if (From <= -Lead && Lens) { Lens->StartCameraFade(0.f, 0.f, .01f, SkyWhite, false, false); Cut(); }
    if (Crossed(-Lead + .55f) && Lens) Lens->StartCameraFade(0.f, 1.f, .6f, SkyWhite, false, true);
    // A few seconds of falling with only the air; then the score.
    if (Crossed(0.f) && Music) { Music->Play(); UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_SCORE at=%.2f"), To); }
    if (Crossed(Switch))
    {
        HideDocks();
        BuildStage();
        bStage = true;
        if (Wind) Wind->FadeIn(.6f, .95f);
        if (Flap) Flap->FadeIn(.6f, .8f);
        Cut();
    }
    if (Crossed(Switch + .15f) && Lens) Lens->StartCameraFade(1.f, 0.f, 1.1f, SkyWhite, false, false);
    for (float At : {Reveal, CloseUp, Down, Below, Canopy, Crash, Impact, Look, Drag, PullBack}) if (Crossed(At)) Cut();
    // The mist was for the passage through the deck; from below it would be dots.
    if (Crossed(Canopy) && Puffs) Puffs->SetVisibility(false);
    const FVector G = LandingSpot();
    if (Crossed(Crash) || Crossed(25.35f) || Crossed(25.80f))
    {
        const float Z = Crossed(Crash) ? 4300.f : Crossed(25.35f) ? 3000.f : 1750.f;
        Burst(G + FVector(0, 0, Z), 70, 500.f);
        if (LeafCrashes.Num()) UGameplayStatics::PlaySound2D(this, LeafCrashes[Crashes++ % LeafCrashes.Num()], .7f, FMath::FRandRange(.92f, 1.05f));
        if (Crossed(Crash))
        {
            if (Wind) Wind->FadeOut(.4f, 0.f);
            if (Flap) Flap->FadeOut(.3f, 0.f);
            // Into the canopy and under Chult's own light, fog and exposure.
            for (USceneComponent* Part : SkyParts) if (Part) Part->SetVisibility(false);
            if (Landing) Landing->BuildEnvironment();
        }
    }
    if (Crossed(Impact))
    {
        // A body hitting the ground from the sky: litter thrown up, a heavy
        // thud, the camera jarred.
        Burst(G + FVector(0, 0, 20), 70, 380.f);
        if (LandSound) UGameplayStatics::PlaySound2D(this, LandSound, 1.f, .62f);
        if (LeafCrashes.Num()) UGameplayStatics::PlaySound2D(this, LeafCrashes[0], .5f, .7f);
        ShakeUntil = Impact + ShakeTime;
        bLanded = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_IMPACT at=%.2f"), To);
    }
    AChuckCharacter* C = Chuck.Get();
    if (Crossed(Impact + .3f) && LandSound) UGameplayStatics::PlaySound2D(this, LandSound, .55f, .75f);   // the bounce coming down
    // The light gathers round him where he lies, not where he'd stand.
    const FVector Body = ChuckTarget();
    const FVector Feet(Body.X, Body.Y, G.Z + 1.f);
    // His quiet death on the jungle floor, and his return out of the light.
    // At the column's peak the light takes him (the quiet death) and gives him back curled.
    if (C && Crossed(Summon + AAstralSummon::SummonPeak - SwapLead)) { C->SetAstralHidden(true); bVanished = true; }
    if (Crossed(Summon)) { AAstralSummon::Start(GetWorld(), Feet, true); bSummoned = true; }
    if (C && Crossed(Summon + AAstralSummon::SummonPeak)) C->SetAstralHidden(false);
    if (C && (Crossed(Drag + .55f) || Crossed(Drag + 3.0f))) { C->Exhale(); ++Exhales; }
    if (Crossed(MusicFade) && Music) Music->FadeOut(End - MusicFade, 0.f);
    if (Crossed(Black) && Lens) Lens->StartCameraFade(0.f, 1.f, End - Black, FLinearColor::Black, false, true);
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckFallCutsceneTest")))
    {
        for (float At : ShotTimes)
            if (Crossed(At))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("FallCutscene/T%04.1f_%s.png"), At, ShotName(At)), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_SHOT t=%.1f shot=%s chuck=%s camera=%s"), At, ShotName(At), C ? *C->GetActorLocation().ToString() : TEXT("-"), *Camera->GetActorLocation().ToString());
            }
        if (Crossed(10.f)) UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_MUSIC playing=%d"), IsMusicPlaying());
    }
}

void AFallCutscene::Finish()
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_END at=%.2f skipped=%d landed=%d vanished=%d summoned=%d exhales=%d music_playing=%d"),
        Clock, bSkipping, bLanded, bVanished, bSummoned, Exhales, IsMusicPlaying());
    for (int32 I = 0; I < 3; ++I)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_CUTSCENE_FPS part=%s average=%.1f worst_frame_ms=%.1f"), I == 0 ? TEXT("sky") : I == 1 ? TEXT("canopy") : TEXT("floor"),
            PartTime[I] > 0 ? PartFrames[I] / PartTime[I] : 0.f, PartWorst[I] * 1000.f);
    if (Music) Music->Stop();
    if (Wind) Wind->Stop();
    if (Flap) Flap->Stop();
    if (AChuckCharacter* C = Chuck.Get()) C->EndFallCutscene();
    if (Camera) Camera->Destroy();
    Destroy();
}

void AFallCutscene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    // The clock keeps pace with the score: a hitch (the stage being built,
    // under the white) is not clamped away, so the cuts stay on the bars.
    const float Before = Clock;
    Clock += DeltaSeconds;
    Events(Before, Clock);
    if (bSkipping && Clock - SkipAt >= .5f) { Finish(); return; }
    if (Clock >= End) { Finish(); return; }
    // Frame rate per part, for the test report (sky, canopy, floor).
    const int32 Part = Clock < Canopy ? 0 : Clock < Impact ? 1 : 2;
    if (Clock > Switch + .5f) { PartFrames[Part] += 1; PartTime[Part] += DeltaSeconds; PartWorst[Part] = FMath::Max(PartWorst[Part], DeltaSeconds); }
    UpdateChuck(DeltaSeconds);
    UpdateLeaves(DeltaSeconds);
    UpdateLight();
    if (Camera && Chuck.IsValid()) UpdateCamera();
}
