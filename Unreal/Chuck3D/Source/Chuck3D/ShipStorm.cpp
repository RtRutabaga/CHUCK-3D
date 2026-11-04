#include "ShipStorm.h"
#include "ShipSetting.h"
#include "ShipCrew.h"
#include "DockNPC.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// The chest, the ending's trigger and the storm (Claude, 2026-11-03; ShipStorm.h, docs/SHIP.md).
using namespace Ship;

namespace
{
    bool GStep[ShipGate::Steps] = {};
    float GWind = 0.f;
    UStaticMesh* GPlane = nullptr;
    float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }

    // The chest (ship frame): its body runs X -1230..-1140, Y 150..205 against the port side, on the cabin floor, 42 cm high
    // (the ship's mesh); its lid is 14 cm more, hinged along the back top edge. The front faces the cabin (-Y).
    constexpr float ChestX0 = -1230, ChestX1 = -1140, ChestY0 = 150, ChestY1 = 205, ChestBody = 42, ChestLid = 14;
    constexpr float LidOpen = 108.f;   // degrees
    constexpr float LidTime = 1.1f;

    // A box (X0..X1, Y0..Y1, Z0..Z1) into a mesh section's arrays.
    void Box(TArray<FVector>& V, TArray<int32>& T, TArray<FVector>& N, TArray<FVector2D>& UV, const FVector& A, const FVector& B)
    {
        const FVector C[8] = {{A.X, A.Y, A.Z}, {B.X, A.Y, A.Z}, {B.X, B.Y, A.Z}, {A.X, B.Y, A.Z}, {A.X, A.Y, B.Z}, {B.X, A.Y, B.Z}, {B.X, B.Y, B.Z}, {A.X, B.Y, B.Z}};
        const int32 F[6][4] = {{4, 5, 6, 7}, {3, 2, 1, 0}, {0, 1, 5, 4}, {2, 3, 7, 6}, {1, 2, 6, 5}, {3, 0, 4, 7}};
        const FVector Nn[6] = {{0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, 1, 0}, {1, 0, 0}, {-1, 0, 0}};
        for (int32 I = 0; I < 6; ++I)
        {
            const int32 Base = V.Num();
            for (int32 K = 0; K < 4; ++K)
            {
                const FVector& P = C[F[I][K]];
                V.Add(P); N.Add(Nn[I]);
                // Planar UVs in 60 cm units across the face.
                const FVector2D Q = FMath::Abs(Nn[I].Z) > .5 ? FVector2D(P.X, P.Y) : FMath::Abs(Nn[I].Y) > .5 ? FVector2D(P.X, P.Z) : FVector2D(P.Y, P.Z);
                UV.Add(Q / 60.f);
            }
            T.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------- the gate
namespace ShipGate
{
    const TCHAR* StepName(EStep Step)
    {
        static const TCHAR* Names[] = {TEXT("chef"), TEXT("seated"), TEXT("concertina"), TEXT("cheering"), TEXT("dancer"), TEXT("jeffries"), TEXT("chest")};
        return Names[FMath::Clamp(int32(Step), 0, Steps - 1)];
    }
    void Mark(EStep Step)
    {
        if (GStep[int32(Step)]) return;
        GStep[int32(Step)] = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_GATE step=%s done=%d/%d open=%d"), StepName(Step), DoneCount(), Steps, IsOpen());
    }
    bool Done(EStep Step) { return GStep[int32(Step)]; }
    int32 DoneCount() { int32 N = 0; for (bool B : GStep) N += B; return N; }
    bool IsOpen() { return DoneCount() == Steps; }
    float OpenedFor(const UWorld* World) { const AShipStorm* S = AShipStorm::Get(World); return S && IsOpen() ? S->GetClock() : -1.f; }
    void OpenAll() { for (int32 I = 0; I < Steps; ++I) Mark(EStep(I)); }
    void Reset(UWorld* World)
    {
        for (bool& B : GStep) B = false;
        if (AShipChest* C = AShipChest::Get(World)) C->Close();
        if (AShipStorm* S = AShipStorm::Get(World)) S->Calm();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_GATE reset=1"));
    }
}

float ShipStormWind() { return GWind; }

// ---------------------------------------------------------------------------------------------------------------- the chest
AShipChest::AShipChest()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    HitRadius = 45.f; HitHeight = ChestBody + ChestLid; bCentred = false;
}

AShipChest* AShipChest::Get(const UWorld* World)
{
    if (World) for (TActorIterator<AShipChest> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipChest* AShipChest::Spawn(UWorld* World)
{
    if (AShipChest* Existing = Get(World)) return Existing;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* C = World->SpawnActor<AShipChest>(ShipWorld(FVector((ChestX0 + ChestX1) * .5f, (ChestY0 + ChestY1) * .5f, LowerZ)), FRotator::ZeroRotator, P);
    if (C) C->BuildLid();
    return C;
}

void AShipChest::BuildLid()
{
    // The hinge along the back top edge; the lid in its frame runs forward (-Y) from it.
    Hinge = NewObject<USceneComponent>(this, TEXT("Hinge"));
    Hinge->SetupAttachment(RootComponent);
    Hinge->SetRelativeLocation(FVector(0, (ChestY1 - ChestY0) * .5f, ChestBody));
    Hinge->RegisterComponent();
    Lid = NewObject<UProceduralMeshComponent>(this, TEXT("Lid"));
    Lid->SetupAttachment(Hinge);
    Lid->RegisterComponent();
    const float HX = (ChestX1 - ChestX0) * .5f, D = ChestY1 - ChestY0;
    auto Section = [&](int32 Index, TFunctionRef<void(TArray<FVector>&, TArray<int32>&, TArray<FVector>&, TArray<FVector2D>&)> Fill, const TCHAR* Material)
    {
        TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV;
        Fill(V, T, N, UV);
        Lid->CreateMeshSection(Index, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), Index == 0);
        if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Material)) Lid->SetMaterial(Index, M);
    };
    // The lid's boards, a little crowned (a raised middle plank), then its iron bands and the brass hasp over the front.
    Section(0, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        Box(V, T, N, UV, FVector(-HX, -D, 0), FVector(HX, 0, ChestLid - 3));
        Box(V, T, N, UV, FVector(-HX + 3, -D + 6, ChestLid - 3), FVector(HX - 3, -6, ChestLid));
    }, TEXT("/Game/Art/Materials/M_AgedDockTimber.M_AgedDockTimber"));
    Section(1, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        for (float X : {-37.f, 37.f}) Box(V, T, N, UV, FVector(X - 3, -D - 1, -.5f), FVector(X + 3, 1, ChestLid + 1));
    }, TEXT("/Game/Art/Materials/M_RustIron.M_RustIron"));
    Section(2, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        Box(V, T, N, UV, FVector(-5, -D - 3, -9), FVector(5, -D - 1, ChestLid - 2));
    }, TEXT("/Game/Art/Materials/M_Metal.M_Metal"));
    Lid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Lid->ComponentTags.Add(TEXT("Ship"));
    // The gold inside: a warm light that comes up as the lid lifts.
    Glow = NewObject<UPointLightComponent>(this, TEXT("Glow"));
    Glow->SetupAttachment(RootComponent);
    Glow->SetRelativeLocation(FVector(0, 0, ChestBody + 6));
    Glow->SetIntensityUnits(ELightUnits::Candelas);
    Glow->SetIntensity(0.f); Glow->SetAttenuationRadius(260.f);
    Glow->SetLightColor(FLinearColor(1.f, .66f, .25f));
    Glow->SetCastShadows(false);
    Glow->RegisterComponent();
    // A session's chest stays open (through deaths and a rebuilt stage).
    if (ShipGate::Done(ShipGate::EStep::Chest)) { bOpen = bDropped = bBroken = true; OpenClock = LidTime; LidAngle = LidOpen; Hinge->SetRelativeRotation(FRotator(0, 0, LidAngle)); }
}

bool AShipChest::CanInteract(const AActor* Chuck) const
{
    if (!Chuck || bOpen) return false;
    const FVector To = GetActorLocation() - Chuck->GetActorLocation();
    return To.Size2D() < 95.f && FMath::Abs(To.Z) < 80.f && FVector::DotProduct(To.GetSafeNormal2D(), Chuck->GetActorForwardVector().GetSafeNormal2D()) > .3f;
}

void AShipChest::Break(const FVector& Swing) { Open(); }

void AShipChest::Open()
{
    if (bOpen) return;
    bOpen = true; bBroken = true; OpenClock = 0.f;
    ShipGate::Mark(ShipGate::EStep::Chest);
    // The old lid's hinges complain.
    if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_ShipCreak_02.SFX_ShipCreak_02")))
        UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() + FVector(0, 0, 50), .55f, .8f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CHEST open=1"));
}

void AShipChest::Close()
{
    bOpen = bDropped = bBroken = false; OpenClock = 0.f; LidAngle = 0.f;
    if (Hinge) Hinge->SetRelativeRotation(FRotator::ZeroRotator);
    if (Glow) Glow->SetIntensity(0.f);
    if (Carton.IsValid()) Carton->Destroy();
    Carton.Reset();
}

void AShipChest::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bOpen || OpenClock >= LidTime + 3.f) return;
    OpenClock += Dt;
    // The lid swings back past upright against its stay and settles (ease out with a little overshoot).
    const float U = FMath::Clamp(OpenClock / LidTime, 0.f, 1.f);
    const float Ease = 1.f + 2.2f * FMath::Pow(U - 1.f, 3) + 1.2f * FMath::Pow(U - 1.f, 2);
    LidAngle = LidOpen * Ease;
    if (Hinge) Hinge->SetRelativeRotation(FRotator(0, 0, LidAngle));
    if (Glow) Glow->SetIntensity(FMath::Lerp(0.f, 22.f, FMath::Clamp(OpenClock / .7f, 0.f, 1.f)) * (1.f - .45f * FMath::Clamp((OpenClock - 1.5f) / 2.f, 0.f, 1.f)));
    // The carton pops out over the front and lands before it.
    if (!bDropped && OpenClock > .55f)
    {
        bDropped = true;
        const FVector From = ShipWorld(FVector((ChestX0 + ChestX1) * .5f, ChestY0 + 18, LowerZ + ChestBody + 10));
        Carton = ACigarettePickup::SpawnCarton(GetWorld(), From, FVector(FMath::FRandRange(-15.f, 15.f), -125.f, 230.f), ShipWorld(FVector(0, 0, LowerZ)).Z);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CARTON dropped=%d"), Carton.IsValid());
    }
    if (OpenClock >= LidTime && OpenClock - Dt < LidTime)
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_DeckStamp_01.SFX_DeckStamp_01")))
            UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() + FVector(0, 30, 60), .3f, 1.15f);
}

// ---------------------------------------------------------------------------------------------------------------- the storm
namespace
{
    // How the storm comes in (seconds after the gate opens) and how it then keeps on.
    constexpr float WindIn0 = 0, WindIn1 = 45, DarkIn0 = 4, DarkIn1 = 55, SeaIn0 = 8, SeaIn1 = 70, RainIn0 = 22, RainIn1 = 65, LightningFrom = 18;
    constexpr int32 MaxDrops = 2600, MaxSplashes = 240, MaxBlobs = 420;
    constexpr float RainBox = 1500.f, RainAbove = 1300.f, RainBelow = 500.f;
    // The gale blows from the starboard beam (the flag streams to port), the ship's way adds a little from ahead.
    FVector WindVelocity(float Wind) { return FVector(-180.f, 620.f, 0.f) * Wind; }
    // The surface a falling drop or a blob stops at (ship frame): the deck it is over, or the sea.
    float SurfaceZ(const FVector& L)
    {
        if (L.X > SternX && L.X < BowX && FMath::Abs(L.Y) < ShipOuterHalf(L.X, DeckZ)) return L.X < QuarterFore ? QuarterZ : DeckZ;
        return WaterZ;
    }
}

AShipStorm::AShipStorm()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved this frame
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    GPlane = Quad.Object;
}

AShipStorm* AShipStorm::Get(const UWorld* World)
{
    if (World) for (TActorIterator<AShipStorm> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

UInstancedStaticMeshComponent* AShipStorm::MakeSprites(const TCHAR* Name, const TCHAR* Material, int32 CustomFloats)
{
    auto* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
    C->SetStaticMesh(GPlane);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCastShadow(false);
    C->SetMobility(EComponentMobility::Movable);
    C->NumCustomDataFloats = CustomFloats;
    C->SetupAttachment(RootComponent);
    C->RegisterComponent();
    C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
    C->SetWorldTransform(FTransform::Identity);
    if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Material)) C->SetMaterial(0, M);
    else UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_STORM_MISSING %s"), Material);
    C->SetVisibility(false);
    return C;
}

AShipStorm* AShipStorm::Spawn(UWorld* World)
{
    if (AShipStorm* Existing = Get(World)) return Existing;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* S = World->SpawnActor<AShipStorm>(ShipOrigin(), FRotator::ZeroRotator, P);
    if (!S) return nullptr;
    // Materials: Tools/create_ship_storm.py.
    S->RainSprites = S->MakeSprites(TEXT("StormRain"), TEXT("/Game/Art/Ship/M_ShipRain.M_ShipRain"), 1);
    S->SplashSprites = S->MakeSprites(TEXT("StormSplash"), TEXT("/Game/Art/Ship/M_ShipSpray.M_ShipSpray"), 1);
    S->SpraySprites = S->MakeSprites(TEXT("StormSpray"), TEXT("/Game/Art/Ship/M_ShipSpray.M_ShipSpray"), 1);
    TArray<FTransform> Zero;
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxDrops); S->RainSprites->AddInstances(Zero, false, true);
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxSplashes); S->SplashSprites->AddInstances(Zero, false, true);
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxBlobs); S->SpraySprites->AddInstances(Zero, false, true);
    S->Bolt = NewObject<UProceduralMeshComponent>(S, TEXT("StormBolt"));
    S->Bolt->SetupAttachment(S->RootComponent);
    S->Bolt->RegisterComponent();
    S->Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision); S->Bolt->SetCastShadow(false);
    if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Ship/M_ShipBolt.M_ShipBolt")))
    {
        S->BoltMid = UMaterialInstanceDynamic::Create(M, S);
        S->BoltGlowMid = UMaterialInstanceDynamic::Create(M, S);
        S->BoltGlowMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
    }
    if (S->RainSprites->GetMaterial(0)) S->RainMid = S->RainSprites->CreateDynamicMaterialInstance(0);
    if (S->BoltMid) S->Bolt->SetMaterial(0, S->BoltMid);
    if (S->BoltMid) S->BoltMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);   // (a dark bolt far off from the start: its shader compiled before the first strike)
    S->BuildBolt(ShipWorld(FVector(0, 0, WaterZ)) + FVector(300000.f, 0, 0), ShipWorld(FVector(0, 0, DeckZ + 150)));
    // The drops start spread through their column (placed round the view when the rain first shows).
    S->Drops.SetNum(MaxDrops);
    for (FDrop& D : S->Drops) { D.P = FVector(FMath::FRandRange(-RainBox, RainBox), FMath::FRandRange(-RainBox, RainBox), FMath::FRandRange(-RainBelow, RainAbove)); D.Speed = FMath::FRandRange(820.f, 1050.f); D.Len = FMath::FRandRange(26.f, 48.f); }
    // The storm's sounds (Tools/make_storm_sounds.py): loops, mixed by where he is in TickSound; thunder and the sea's slams.
    auto Loop = [&](const TCHAR* Name, bool bMuffled) -> UAudioComponent*
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name));
        if (!Sound) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_SOUND_MISSING %s"), Name); return nullptr; }
        auto* A = NewObject<UAudioComponent>(S); A->SetSound(Sound); A->bAutoActivate = false; A->bAllowSpatialization = false;
        A->SetupAttachment(S->RootComponent); A->RegisterComponent();
        if (bMuffled) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(900.f); }
        A->SetVolumeMultiplier(.001f);
        return A;
    };
    S->Loops = {Loop(TEXT("SFX_StormRain_00"), false), Loop(TEXT("SFX_StormRainAbove_00"), true), Loop(TEXT("SFX_StormWind_00"), false),
        Loop(TEXT("SFX_StormWindBelow_00"), false), Loop(TEXT("SFX_StormFlap_00"), false), Loop(TEXT("SFX_StormSea_00"), false)};
    auto Set = [&](const TCHAR* Stem, TArray<USoundBase*>& Out)
    {
        for (int32 I = 0; I < 12; ++I)
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Out.Add(Sound);
            else break;
        if (!Out.Num()) UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_SOUND_MISSING %s"), Stem);
    };
    Set(TEXT("SFX_StormThunderNear"), S->NearThunder);
    Set(TEXT("SFX_StormThunderFar"), S->FarThunder);
    Set(TEXT("SFX_StormSlam"), S->Slams);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_READY loops=%d near_thunder=%d far_thunder=%d slams=%d"),
        S->Loops.FilterByPredicate([](UAudioComponent* A) { return A != nullptr; }).Num(), S->NearThunder.Num(), S->FarThunder.Num(), S->Slams.Num());
    return S;
}

void AShipStorm::Calm()
{
    bStarted = false; Clock = 0.f; Wind = Dark = Sea = Rain = Flash = 0.f; GWind = 0.f;
    Pulses.Reset(); PendingThunder.Reset(); Blobs.Reset(); Splashes.Reset();
    NextStrike = LightningFrom; NextSpray = 6.f;
    for (UAudioComponent* A : Loops) if (A) A->Stop();
    for (float& L : LoopNow) L = 0.f;
    for (auto* C : {RainSprites, SplashSprites, SpraySprites}) if (C) C->SetVisibility(false);
    if (BoltMid) BoltMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
    if (BoltGlowMid) BoltGlowMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
}

void AShipStorm::Levels(float Dt)
{
    // The gate (the 2D game's seven conditions): the crew's are read here; the chest marks its own.
    if (!ShipGate::IsOpen())
        if (const AShipCrew* Crew = AShipCrew::Get(GetWorld()))
        {
            using ShipGate::EStep;
            if (const ADockNPC* Chef = Crew->Find(EDockHuman::PirateChef))
            {
                const FString State = Chef->GetChefStateName();
                if (State == TEXT("Notice") || State == TEXT("Chase") || State == TEXT("Swing")) ShipGate::Mark(EStep::Chef);
            }
            const TPair<EDockHuman, EStep> Talks[] = {{EDockHuman::PirateSeated, EStep::Seated}, {EDockHuman::PirateConcertina, EStep::Concertina},
                {EDockHuman::PirateCheering, EStep::Cheering}, {EDockHuman::PirateDancer, EStep::Dancer}, {EDockHuman::Jeffries, EStep::Jeffries}};
            for (const auto& T : Talks) if (const ADockNPC* N = Crew->Find(T.Key)) if (N->HasTalkedOnce()) ShipGate::Mark(T.Value);
        }
    if (!bStarted && ShipGate::IsOpen())
    {
        bStarted = true; Clock = 0.f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM start=1"));
    }
    if (!bStarted) return;
    Clock += Dt;
    const float T = Clock;
    // Gusts (a few seconds) and squalls (most of a minute): the full storm never sits still.
    Gust = .5f + .32f * FMath::Sin(T * 2 * PI / 6.3f) * FMath::Sin(T * 2 * PI / 2.9f + .7f) + .18f * FMath::Sin(T * 2 * PI / 13.7f + 2.f);
    Squall = FMath::Clamp(.5f + .3f * FMath::Sin(T * 2 * PI / 47.f - 1.2f) + .2f * FMath::Sin(T * 2 * PI / 19.f), 0.f, 1.f);
    Wind = Smooth(WindIn0, WindIn1, T) * (.82f + .18f * Gust);
    Dark = Smooth(DarkIn0, DarkIn1, T) * (.9f + .1f * Squall);
    Sea = Smooth(SeaIn0, SeaIn1, T);
    Rain = Smooth(RainIn0, RainIn1, T) * (.55f + .45f * Squall);
    GWind = Wind;
}

void AShipStorm::Tick(float Dt)
{
    Super::Tick(Dt);
    Dt = FMath::Min(Dt, .1f);
    Levels(Dt);
    const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    const FVector Eye = Cam ? Cam->GetCameraLocation() : ShipWorld(FVector(0, 0, DeckZ + 150));
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bBelow = Pawn && ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck) && InShip(Pawn->GetActorLocation());
    if (!bStarted)
    {
        for (auto* C : {RainSprites, SplashSprites, SpraySprites}) if (C && C->IsVisible()) C->SetVisibility(false);
        return;
    }
    TickRain(Dt, Eye, bBelow);
    TickSpray(Dt, Eye);
    TickLightning(Dt);
    TickSound(Dt, bBelow);
}

void AShipStorm::TickRain(float Dt, const FVector& Eye, bool bInside)
{
    if (!RainSprites || !SplashSprites) return;
    const int32 Shown = FMath::RoundToInt(MaxDrops * FMath::Clamp(Rain, 0.f, 1.f));
    if (Shown == 0 && !RainSprites->IsVisible()) return;
    RainSprites->SetVisibility(Shown > 0);
    const FVector Blow = WindVelocity(Wind);
    const FVector O = ShipOrigin();
    TArray<FTransform> X; X.SetNumUninitialized(MaxDrops);
    DropsShown = 0;
    for (int32 I = 0; I < MaxDrops; ++I)
    {
        FDrop& D = Drops[I];
        // Held in a box round the view (wrapping as he moves); recycled at the top when they reach the deck or the sea.
        const FVector V(Blow.X, Blow.Y, -D.Speed);
        D.P += V * Dt;
        FVector R = D.P - Eye;
        if (R.X > RainBox) D.P.X -= 2 * RainBox; else if (R.X < -RainBox) D.P.X += 2 * RainBox;
        if (R.Y > RainBox) D.P.Y -= 2 * RainBox; else if (R.Y < -RainBox) D.P.Y += 2 * RainBox;
        if (R.Z > RainAbove + 200) D.P.Z -= RainAbove + RainBelow;
        const FVector L = D.P - O;
        const float Floor = SurfaceZ(L);
        if (L.Z < Floor || R.Z < -RainBelow)
        {
            // Landing on the deck near him: a splash.
            if (L.Z < Floor && Floor > WaterZ + 1 && I < Shown && Splashes.Num() < MaxSplashes && FVector::DistSquared(D.P, Eye) < 900.f * 900.f && FMath::FRand() < .55f)
            {
                Splashes.Add({FVector(D.P.X, D.P.Y, O.Z + Floor + .5f), 0.f, FMath::FRandRange(5.f, 10.f)});
                ++SplashesMade;
            }
            D.P = Eye + FVector(FMath::FRandRange(-RainBox, RainBox), FMath::FRandRange(-RainBox, RainBox), RainAbove - FMath::FRandRange(0.f, 300.f));
            D.Speed = FMath::FRandRange(820.f, 1050.f);
        }
        if (I >= Shown) { X[I] = FTransform(FQuat::Identity, D.P, FVector::ZeroVector); continue; }
        // A streak along its fall, turned to face the view; thicker far off so it still reads, never in his face.
        const FVector Long = V.GetSafeNormal();
        const FVector ToEye = Eye - D.P;
        const float Dist = ToEye.Size();
        FVector Wide = FVector::CrossProduct(Long, ToEye / FMath::Max(Dist, 1.f));
        if (!Wide.Normalize() || Dist < 35.f) { X[I] = FTransform(FQuat::Identity, D.P, FVector::ZeroVector); continue; }
        const FVector Nn = FVector::CrossProduct(Long, Wide);
        const float Width = .8f + Dist * .0013f;
        X[I] = FTransform(FMatrix(Long, Wide, Nn, FVector::ZeroVector).ToQuat(), D.P, FVector(D.Len * (V.Size() / 1000.f) / 100.f, Width / 100.f, 1.f));
        ++DropsShown;
    }
    RainSprites->BatchUpdateInstancesTransforms(0, X, true, true, true);
    if (RainMid) RainMid->SetScalarParameterValue(TEXT("Flash"), Flash);
    // Splashes: a small burst of droplets that rises and goes in a fifth of a second.
    TArray<FTransform> S; S.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxSplashes);
    int32 N = 0;
    for (int32 I = Splashes.Num() - 1; I >= 0; --I)
    {
        FSplash& P = Splashes[I];
        P.Age += Dt;
        if (P.Age > .2f) { Splashes.RemoveAtSwap(I); continue; }
        const float U = P.Age / .2f;
        FVector Z = Eye - P.P; Z.Z = 0; if (!Z.Normalize()) Z = FVector::ForwardVector;
        const FVector Y = -FVector::UpVector, Xa = FVector::CrossProduct(Y, Z);
        const float W = P.Size * (.5f + U), H = P.Size * (.35f + .5f * U);
        S[N] = FTransform(FMatrix(Xa, Y, Z, FVector::ZeroVector).ToQuat(), P.P + FVector(0, 0, H * .5f), FVector(W / 100.f, H / 100.f, 1.f));
        SplashSprites->SetCustomDataValue(N, 0, .55f * (1.f - U), false);
        ++N;
    }
    SplashSprites->SetVisibility(N > 0);
    SplashSprites->BatchUpdateInstancesTransforms(0, S, true, true, true);
}

void AShipStorm::SprayBurst(const FVector& At, float Size)
{
    // Water thrown up where a sea meets the bow or the weather side: up, out and blown across the deck.
    const int32 Count = FMath::RoundToInt(38 * Size);
    for (int32 I = 0; I < Count && Blobs.Num() < MaxBlobs; ++I)
    {
        FBlob B;
        B.P = At + FVector(FMath::FRandRange(-120.f, 120.f) * Size, FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-30.f, 30.f));
        B.V = FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-80.f, 120.f), FMath::FRandRange(260.f, 640.f) * FMath::Sqrt(Size));
        B.Age = -FMath::FRandRange(0.f, .18f); B.Life = FMath::FRandRange(1.1f, 2.1f); B.Size = FMath::FRandRange(45.f, 110.f) * Size;
        Blobs.Add(B);
    }
    ++Sprays;
    if (Slams.Num())
    {
        const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
        const bool bApart = Pawn && ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck);   // below decks: through the hull
        UAudioComponent* A = UGameplayStatics::SpawnSoundAtLocation(this, Slams[FMath::RandRange(0, Slams.Num() - 1)], At, FRotator::ZeroRotator,
            FMath::Clamp(.55f * Size, .3f, .9f) * (bApart ? .6f : 1.f), FMath::FRandRange(.9f, 1.05f));
        if (A && bApart) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(700.f); }
    }
}

void AShipStorm::TickSpray(float Dt, const FVector& Eye)
{
    if (!SpraySprites) return;
    // Seas over the bow as she pitches into them, and over the weather (starboard) rail as she rolls.
    NextSpray -= Dt;
    if (NextSpray <= 0.f && Sea > .35f)
    {
        NextSpray = FMath::FRandRange(1.2f, 3.6f) / FMath::Max(Sea, .4f);
        const bool bBow = FMath::FRand() < .45f;
        const FVector At = bBow ? FVector(FMath::FRandRange(1350.f, 1650.f), FMath::FRandRange(-180.f, 60.f), RailZ + 30)
                                : FVector(FMath::FRandRange(-700.f, 1200.f), 0, RailZ + 10);
        FVector L = At;
        if (!bBow) L.Y = -ShipOuterHalf(At.X, RailZ) - 20.f;
        SprayBurst(ShipWorld(L), FMath::FRandRange(.7f, 1.25f) * Sea);
    }
    const FVector Blow = WindVelocity(Wind);
    TArray<FTransform> X; X.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxBlobs);
    int32 N = 0;
    const FVector O = ShipOrigin();
    for (int32 I = Blobs.Num() - 1; I >= 0; --I)
    {
        FBlob& B = Blobs[I];
        B.Age += Dt;
        if (B.Age < 0.f) continue;
        if (B.Age > B.Life) { Blobs.RemoveAtSwap(I); continue; }
        B.V.X += (Blow.X - B.V.X) * 1.4f * Dt; B.V.Y += (Blow.Y - B.V.Y) * 1.4f * Dt; B.V.Z -= 620.f * Dt;
        B.P += B.V * Dt;
        const FVector L = B.P - O;
        if (L.Z < SurfaceZ(L)) { B.P.Z = O.Z + SurfaceZ(L); B.V *= .2f; B.Life = FMath::Min(B.Life, B.Age + .25f); }
        if (N >= MaxBlobs) continue;
        const float U = B.Age / B.Life;
        FVector Z = Eye - B.P; if (!Z.Normalize()) Z = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal(), Y0 = FVector::CrossProduct(Z, X0);
        const float Spin = I * 1.7f + B.Age * .8f;
        const FVector Xa = X0 * FMath::Cos(Spin) + Y0 * FMath::Sin(Spin), Ya = Y0 * FMath::Cos(Spin) - X0 * FMath::Sin(Spin);
        X[N] = FTransform(FMatrix(Xa, Ya, Z, FVector::ZeroVector).ToQuat(), B.P, FVector(B.Size * (1.f + 1.6f * U) / 100.f));
        SpraySprites->SetCustomDataValue(N, 0, .6f * FMath::SmoothStep(0.f, .08f, U) * FMath::Pow(1.f - U, 1.4f), false);
        ++N;
    }
    SpraySprites->SetVisibility(N > 0);
    SpraySprites->BatchUpdateInstancesTransforms(0, X, true, true, true);
}

void AShipStorm::StrikeNow(float Distance, float Bearing)
{
    const FVector Dir = FRotator(0, Bearing, 0).Vector();
    const bool bNear = Distance < 120000.f;
    // Two to four strokes down the same channel, the first the brightest.
    Pulses.Reset();
    const int32 Strokes = FMath::RandRange(2, 4);
    float At = 0.f;
    for (int32 K = 0; K < Strokes; ++K) { Pulses.Add(At); At += FMath::FRandRange(.05f, .14f); }
    StrikeAge = 0.f;
    StrikeBright = bNear ? FMath::Lerp(4.f, 2.2f, Distance / 120000.f) : FMath::Lerp(1.6f, .6f, FMath::Clamp((Distance - 120000.f) / 500000.f, 0.f, 1.f));
    FlashFrom = (Dir + FVector(0, 0, .6f)).GetSafeNormal();
    const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    BuildBolt(ShipWorld(FVector(0, 0, WaterZ)) + Dir * Distance, Cam ? Cam->GetCameraLocation() : ShipWorld(FVector(0, 0, DeckZ + 150)));
    // Its thunder, after the light (sound at about a third of real speed's delay: still well behind the flash).
    PendingThunder.Add({Clock + Distance / 34300.f * .35f + .05f, Distance, bNear});
    ++Strikes;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STRIKE n=%d distance_m=%.0f bearing=%.0f near=%d bright=%.2f"), Strikes, Distance / 100.f, Bearing, bNear, StrikeBright);
}

void AShipStorm::BuildBolt(const FVector& Base, const FVector& Eye)
{
    if (!Bolt) return;
    // A jagged channel from the cloud base down to the sea, by halving and kicking each segment (finer kinks at each
    // halving), with forks off its upper part that wander down and out, some forking again.
    const float CloudZ = 70000.f;
    TArray<TArray<FVector>> Paths;
    TArray<FVector> Main = {Base + FVector(FMath::FRandRange(-6000.f, 6000.f), FMath::FRandRange(-6000.f, 6000.f), CloudZ), Base};
    float Kick = CloudZ * .17f;
    for (int32 Level = 0; Level < 8; ++Level)
    {
        TArray<FVector> Next;
        for (int32 I = 0; I + 1 < Main.Num(); ++I)
        {
            Next.Add(Main[I]);
            const FVector Mid = (Main[I] + Main[I + 1]) * .5f + FVector(FMath::FRandRange(-Kick, Kick), FMath::FRandRange(-Kick, Kick), FMath::FRandRange(-Kick, Kick) * .25f);
            Next.Add(Mid);
        }
        Next.Add(Main.Last());
        Main = MoveTemp(Next); Kick *= .6f;
    }
    Paths.Add(Main);
    auto Fork = [&](const FVector& From, FVector D, int32 Steps, float Step)
    {
        TArray<FVector> F = {From};
        for (int32 K = 0; K < Steps; ++K)
        {
            D = (D + FMath::VRand() * .55f).GetSafeNormal(); D.Z = FMath::Min(D.Z, -.25f); D.Normalize();
            F.Add(F.Last() + D * Step * FMath::FRandRange(.6f, 1.3f));
        }
        Paths.Add(F);
        return F;
    };
    const int32 Forks = FMath::RandRange(5, 8);
    for (int32 K = 0; K < Forks; ++K)
    {
        const int32 From = FMath::RandRange(Main.Num() / 10, Main.Num() * 3 / 5);
        const FVector D = (Main[FMath::Min(From + 6, Main.Num() - 1)] - Main[From]).GetSafeNormal() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0);
        const TArray<FVector> F = Fork(Main[From], D, FMath::RandRange(8, 20), CloudZ * .03f);
        if (FMath::FRand() < .5f && F.Num() > 6)
        {
            const int32 At = FMath::RandRange(2, F.Num() - 3);
            Fork(F[At], (F[At + 1] - F[At]).GetSafeNormal() + FMath::VRand() * .8f, FMath::RandRange(4, 9), CloudZ * .02f);
        }
    }
    // Ribbons turned to the view: a thin white core (the main channel wider than its forks, the forks thinning to their
    // ends), and a wide faint glow round each (section 1, its own dimmer material).
    const float Far = FVector::Dist(Eye, Base);
    const FVector O = Bolt->GetComponentLocation();   // (its vertices relative to the storm, at the ship's origin)
    for (int32 Section = 0; Section < 2; ++Section)
    {
        TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV;
        for (int32 PIdx = 0; PIdx < Paths.Num(); ++PIdx)
        {
            const TArray<FVector>& P = Paths[PIdx];
            const float W0 = (PIdx == 0 ? 1.f : .45f) * FMath::Max(80.f, Far * .002f) * (Section == 0 ? 1.f : 9.f);
            for (int32 I = 0; I < P.Num(); ++I)
            {
                const FVector Along = (P[FMath::Min(I + 1, P.Num() - 1)] - P[FMath::Max(I - 1, 0)]).GetSafeNormal();
                const FVector ToEye = (Eye - P[I]).GetSafeNormal();
                FVector Side = FVector::CrossProduct(Along, ToEye);
                if (!Side.Normalize()) Side = FVector::RightVector;
                const float W = W0 * (PIdx == 0 ? 1.f : 1.f - .8f * I / float(P.Num()));
                const int32 Base0 = V.Num();
                V.Add(P[I] - Side * W - O); V.Add(P[I] + Side * W - O);
                N.Add(ToEye); N.Add(ToEye);
                UV.Add(FVector2D(0, I)); UV.Add(FVector2D(1, I));
                // (Both windings: the channel's turns flip which way a one-sided strip would face.)
                if (I > 0) T.Append({Base0 - 2, Base0, Base0 - 1, Base0 - 1, Base0, Base0 + 1, Base0 - 2, Base0 - 1, Base0, Base0 - 1, Base0 + 1, Base0});
            }
        }
        Bolt->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    }
    if (BoltMid) Bolt->SetMaterial(0, BoltMid);
    if (BoltGlowMid) Bolt->SetMaterial(1, BoltGlowMid);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_BOLT paths=%d points=%d distance_m=%.0f"), Paths.Num(), Main.Num(), Far / 100.f);
}

void AShipStorm::Thunder(float Distance, bool bNear)
{
    TArray<USoundBase*>& Set = bNear && NearThunder.Num() ? NearThunder : FarThunder;
    if (!Set.Num()) return;
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bBelow = Pawn && ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck);
    const float Volume = (bNear ? 1.f : FMath::Lerp(.85f, .45f, FMath::Clamp((Distance - 120000.f) / 500000.f, 0.f, 1.f))) * (bBelow ? .7f : 1.f);
    if (UAudioComponent* A = UGameplayStatics::SpawnSound2D(this, Set[FMath::RandRange(0, Set.Num() - 1)], Volume, FMath::FRandRange(.92f, 1.04f)))
        if (bBelow) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(650.f); }
    ++Thunders;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_THUNDER n=%d near=%d below=%d volume=%.2f"), Thunders, bNear, bBelow, Volume);
}

void AShipStorm::TickLightning(float Dt)
{
    // The next strike: none in the first seconds; then far off, then closer and more often as the storm comes on.
    NextStrike -= Dt;
    if (Clock > LightningFrom && NextStrike <= 0.f && Dark > .25f)
    {
        NextStrike = FMath::Lerp(16.f, 5.5f, Dark) * FMath::FRandRange(.6f, 1.4f);
        const bool bClose = Dark > .8f && FMath::FRand() < .2f;
        const float Distance = bClose ? FMath::FRandRange(40000.f, 110000.f) : FMath::FRandRange(160000.f, 650000.f) * FMath::Lerp(1.4f, 1.f, Dark);
        StrikeNow(Distance, FMath::FRandRange(0.f, 360.f));
    }
    StrikeAge += Dt;
    float F = 0.f;
    for (int32 K = 0; K < Pulses.Num(); ++K)
    {
        const float Since = StrikeAge - Pulses[K];
        // (A 25 ms rise: an instant jump of the whole scene's light leaves speckles along every edge in the
        // temporal anti-aliasing's history.)
        if (Since >= 0.f) F += (K == 0 ? 1.f : .7f) * (1.f - FMath::Exp(-Since / .025f)) * FMath::Exp(-Since / .09f);
    }
    // A flicker over it, as the channel re-strikes.
    Flash = StrikeBright * F * (.85f + .15f * FMath::Sin(StrikeAge * 90.f));
    // (The bolt stays drawn, dark between strikes, so its shader is ready when it is first seen.)
    const float Lit = StrikeAge < 1.f && Flash > .12f ? Flash : 0.f;
    if (BoltMid) BoltMid->SetScalarParameterValue(TEXT("Intensity"), Lit);
    if (BoltGlowMid) BoltGlowMid->SetScalarParameterValue(TEXT("Intensity"), Lit * .045f);
    for (int32 I = PendingThunder.Num() - 1; I >= 0; --I)
        if (Clock >= PendingThunder[I].At) { Thunder(PendingThunder[I].Distance, PendingThunder[I].bNear); PendingThunder.RemoveAt(I); }
}

void AShipStorm::TickSound(float Dt, bool bBelow)
{
    // On deck: the rain hissing on the planking and the sea, the gale (more aloft), the canvas flogging. Below: the rain
    // drumming overhead, the wind heard through the hull and the sea against it. The ship's own ambience and creaks are AShipSetting's.
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const float Up = Pawn && !bBelow ? FMath::Clamp(static_cast<float>(ShipLocal(Pawn->GetActorLocation()).Z - DeckZ - 150.f) / 900.f, 0.f, 1.f) : 0.f;
    const float Want[6] = {
        bBelow ? 0.f : Rain * .95f,
        bBelow ? Rain * .9f : 0.f,
        bBelow ? Wind * .12f : Wind * (.75f + .25f * Gust) * (1.f + .35f * Up),
        bBelow ? Wind * .85f : Wind * .15f,
        bBelow ? 0.f : Wind * .45f,
        bBelow ? Sea * .3f : Sea * .7f * (1.f - .4f * Up) };
    for (int32 I = 0; I < 6 && I < Loops.Num(); ++I)
    {
        LoopNow[I] = FMath::FInterpTo(LoopNow[I], Want[I], Dt, 1.6f);
        if (!Loops[I]) continue;
        if (LoopNow[I] > .002f && !Loops[I]->IsPlaying()) Loops[I]->Play(FMath::FRandRange(0.f, 20.f));
        Loops[I]->SetVolumeMultiplier(FMath::Max(.001f, LoopNow[I]));
    }
}
