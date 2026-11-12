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
#include "Sound/SoundWave.h"
#include "ChuckClimbable.h"
#include "ShipEnding.h"
#include "UObject/ConstructorHelpers.h"

// The chest, the ending's trigger and the storm (Claude, 2026-11-03; ShipStorm.h, docs/SHIP.md).
using namespace Ship;

namespace
{
    bool GStep[ShipGate::Steps] = {};
    float GWind = 0.f, GRupture = 0.f;
    UStaticMesh* GPlane = nullptr;
    float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }

    // The chest (ship frame): its body runs X -1230..-1140, Y 150..205 against the starboard side, on the cabin floor, 42 cm high
    // (the ship's mesh); its lid is 17 cm more (a 3 cm skirt and a 14 cm barrel top since 2026-11-11), hinged along the back
    // top edge. The front faces the cabin (-Y).
    constexpr float ChestX0 = -1230, ChestX1 = -1140, ChestY0 = 150, ChestY1 = 205, ChestBody = 42, ChestSkirt = 3, ChestDome = 14, ChestLid = ChestSkirt + ChestDome;
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
        if (AShipEnding* E = AShipEnding::Get(World)) E->Reset();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_GATE reset=1"));
    }
}

float ShipStormWind() { return GWind; }
bool ShipRatClearOnDeck(const APawn* Pawn)
{
    // On the weather deck (or the quarterdeck, or up the rig) and at least 1.7 m from the companion hatch's middle.
    if (!Pawn || ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck)) return false;
    const FVector L = ShipLocal(Pawn->GetActorLocation());
    return FVector2D::Distance(FVector2D(L.X, L.Y), FVector2D((CompX0 + CompX1) * .5f, (CompY0 + CompY1) * .5f)) >= 170.f;
}
bool ShipRuptureOpen() { return GRupture > .3f; }
bool ShipRuptureSwallows(const FVector& Feet)
{
    // Into the companion hatch under it (its opening, the ladder's lip on its fore edge), down past the deck's top.
    if (GRupture < .3f || !InShip(Feet)) return false;
    const FVector L = ShipLocal(Feet);
    return L.X > CompX0 && L.X < CompX1 + 4 && L.Y > CompY0 && L.Y < CompY1 && L.Z < DeckZ + 6 && L.Z > LowerZ - 60;
}

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
    // A treasure chest's lid (2026-11-11, user: "make the chest look more like a treasure chest"; it was a flat board box):
    // a barrel top over a short skirt, its staves along X; iron bands over it at the body's straps and round both ends;
    // brass studs along the bands, brass caps on the corners, and the brass hasp hanging over the front to the lock.
    // The dome's section: Y from the front (-D) to the hinge (0), Z = skirt + rise * sin(phi), with phi 0..pi.
    constexpr int32 NPhi = 14;
    auto Arc = [&](float Phi, float Grow) { return FVector2D(-D * .5f - (D * .5f + Grow) * FMath::Cos(Phi), ChestSkirt + (ChestDome + Grow) * FMath::Sin(Phi)); };
    auto ArcNormal = [&](float Phi) { return FVector(0, -FMath::Cos(Phi) / (D * .5f), FMath::Sin(Phi) / ChestDome).GetSafeNormal(); };
    // A strip of the dome's surface between X0 and X1, Grow out from the wood, with its two end faces (Ends) when a band.
    auto Dome = [&](TArray<FVector>& V, TArray<int32>& T, TArray<FVector>& N, TArray<FVector2D>& UV, float X0, float X1, float Grow, bool bEnds)
    {
        float Arclen = 0;
        for (int32 I = 0; I < NPhi; ++I)
        {
            const float P0 = PI * I / NPhi, P1 = PI * (I + 1) / NPhi;
            const FVector2D A = Arc(P0, Grow), B = Arc(P1, Grow);
            const float Step = FVector2D::Distance(A, B);
            const int32 Base = V.Num();
            V.Append({FVector(X0, A.X, A.Y), FVector(X1, A.X, A.Y), FVector(X1, B.X, B.Y), FVector(X0, B.X, B.Y)});
            N.Append({ArcNormal(P0), ArcNormal(P0), ArcNormal(P1), ArcNormal(P1)});
            UV.Append({FVector2D(X0, Arclen) / 60.f, FVector2D(X1, Arclen) / 60.f, FVector2D(X1, Arclen + Step) / 60.f, FVector2D(X0, Arclen + Step) / 60.f});
            T.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});   // (UE draws the reversed winding)
            Arclen += Step;
        }
        // The ends: fans over the half-ellipse (for the wood, the lid's two end boards; for a band, its edges).
        for (const float X : {X0, X1})
        {
            if (!bEnds && FMath::Abs(FMath::Abs(X) - HX) > .1f) continue;
            const FVector Nn(X < (X0 + X1) * .5f ? -1 : 1, 0, 0);
            const FVector Mid(X, -D * .5f, ChestSkirt);
            for (int32 I = 0; I < NPhi; ++I)
            {
                const FVector2D A = Arc(PI * I / NPhi, Grow), B = Arc(PI * (I + 1) / NPhi, Grow);
                const FVector2D Ai = Arc(PI * I / NPhi, bEnds ? 0.f : -D), Bi = Arc(PI * (I + 1) / NPhi, bEnds ? 0.f : -D);
                const int32 Base = V.Num();
                if (bEnds)
                {
                    V.Append({FVector(X, Ai.X, Ai.Y), FVector(X, A.X, A.Y), FVector(X, B.X, B.Y), FVector(X, Bi.X, Bi.Y)});
                    for (int32 K = 0; K < 4; ++K) N.Add(Nn);
                    for (int32 K = 0; K < 4; ++K) UV.Add(FVector2D(V[Base + K].Y, V[Base + K].Z) / 60.f);
                }
                else
                {
                    V.Append({Mid, FVector(X, A.X, A.Y), FVector(X, B.X, B.Y), Mid});
                    for (int32 K = 0; K < 4; ++K) N.Add(Nn);
                    for (int32 K = 0; K < 4; ++K) UV.Add(FVector2D(V[Base + K].Y, V[Base + K].Z) / 60.f);
                }
                if (Nn.X > 0) T.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3}); else T.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
            }
        }
    };
    Section(0, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        Box(V, T, N, UV, FVector(-HX, -D, 0), FVector(HX, 0, ChestSkirt));   // the skirt (its underside the lid's inside)
        Dome(V, T, N, UV, -HX, HX, 0.f, false);
    }, TEXT("/Game/Art/Materials/M_AgedDockTimber.M_AgedDockTimber"));
    Section(1, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        // Bands over the top at the body's straps and round each end; a band along the skirt.
        for (const float X : {-37.f, 37.f}) { Dome(V, T, N, UV, X - 3, X + 3, .8f, true); Box(V, T, N, UV, FVector(X - 3, -D - .8f, -.4f), FVector(X + 3, .8f, ChestSkirt + .2f)); }
        for (const float S : {-1.f, 1.f}) Dome(V, T, N, UV, S > 0 ? HX - 3.5f : -HX - .6f, S > 0 ? HX + .6f : -HX + 3.5f, .8f, true);
        Box(V, T, N, UV, FVector(-HX - .6f, -D - .8f, -.4f), FVector(HX + .6f, -D + .2f, ChestSkirt + .4f));
    }, TEXT("/Game/Art/Materials/M_RustIron.M_RustIron"));
    Section(2, [&](auto& V, auto& T, auto& N, auto& UV)
    {
        // The hasp: a plate down from the dome's front over the lock, its ring.
        Box(V, T, N, UV, FVector(-5, -D - 2.6f, -10), FVector(5, -D - .8f, ChestSkirt + 5));
        Box(V, T, N, UV, FVector(-2, -D - 3.6f, -8), FVector(2, -D - 2.6f, -4));
        // Studs along the bands over the top, and caps on the dome's four corners.
        for (const float X : {-37.f, 37.f, -HX + 1.5f, HX - 1.5f})
            for (int32 I = 1; I < NPhi; I += 2)
            {
                const float Phi = PI * I / NPhi;
                const FVector2D At = Arc(Phi, .8f);
                const FVector C(X, At.X, At.Y), Nn = ArcNormal(Phi) * .9f;
                Box(V, T, N, UV, C + FVector(-1.1f, -1.1f, -1.1f) + Nn, C + FVector(1.1f, 1.1f, 1.1f) + Nn);
            }
        for (const float X : {-HX - 1.f, HX - 6.f}) for (const float Y : {-D - 1.f, -6.f}) Box(V, T, N, UV, FVector(X, Y, -.6f), FVector(X + 7, Y + 7, ChestSkirt + 1.5f));
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
    constexpr int32 MaxDrops = 3400, MaxSplashes = 240, MaxBlobs = 520, MaxPuffs = 180;
    constexpr float IntenseRamp = 25.f;   // s the second part takes to build (2026-11-04)
    constexpr float RuptureForm = 2.4f;   // s the rupture takes to tear open
    constexpr float WaveLife = 4.4f;      // s a sea breaking aboard runs across the deck and drains
    constexpr float RainBox = 1500.f, RainAbove = 1300.f, RainBelow = 500.f;
    // The gale blows from the port beam (the flag streams to starboard), the ship's way adds a little from ahead.
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
    // (2026-11-04: the spume's billows, Unity Labs' CC0 smoke flipbook drawn as white water: Tools/create_ship_storm.py.)
    S->PuffSprites = S->MakeSprites(TEXT("StormSpume"), TEXT("/Game/Art/Ship/M_ShipSpume.M_ShipSpume"), 3);
    TArray<FTransform> Zero;
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxDrops); S->RainSprites->AddInstances(Zero, false, true);
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxSplashes); S->SplashSprites->AddInstances(Zero, false, true);
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxBlobs); S->SpraySprites->AddInstances(Zero, false, true);
    Zero.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxPuffs); S->PuffSprites->AddInstances(Zero, false, true);
    // The rupture over the companion hatch, and the seas washing across the deck (rebuilt as they change; relative to the
    // storm, at the ship's origin: the ship frame).
    auto Proc = [&](const TCHAR* Name) { auto* M = NewObject<UProceduralMeshComponent>(S, Name); M->SetupAttachment(S->RootComponent); M->RegisterComponent();
        M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(false); return M; };
    S->RuptureMesh = Proc(TEXT("StormRupture"));
    S->WashMesh = Proc(TEXT("StormDeckWash"));
    S->RuptureLight = NewObject<UPointLightComponent>(S, TEXT("StormRuptureLight"));
    S->RuptureLight->SetupAttachment(S->RootComponent);
    S->RuptureLight->SetRelativeLocation(FVector((CompX0 + CompX1) * .5f, (CompY0 + CompY1) * .5f, DeckZ + 45));
    S->RuptureLight->SetIntensity(0.f); S->RuptureLight->SetAttenuationRadius(560.f); S->RuptureLight->SetLightColor(FLinearColor(.48f, .035f, 1.f));
    // Shadowed (user 2026-11-09): unshadowed, it lit strand hair through the mainmast and through the head, so Jeffries' hair
    // and the crew's beards blazed purple.
    S->RuptureLight->SetCastShadows(true); S->RuptureLight->RegisterComponent();
    // (Drawn from the start, a sliver out of sight under the deck, so its shaders are ready when it first shows.)
    S->BuildRupture(0.f);
    // The second part comes when the user's storm track ends (its length; -ChuckShipStormIntenseAt= for tests).
    // (User 2026-11-05: "only do normal storm for about half the song then fade into storm-intense song and start storm
    // intense sequence".)
    if (const USoundWave* Track = LoadObject<USoundWave>(nullptr, TEXT("/Game/Art/Audio/SW_ShipStorm.SW_ShipStorm"))) S->IntenseAt = Track->Duration * .5f;
    FParse::Value(FCommandLine::Get(), TEXT("ChuckShipStormIntenseAt="), S->IntenseAt);
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
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_READY loops=%d near_thunder=%d far_thunder=%d slams=%d intense_at=%.1f spume=%d"),
        S->Loops.FilterByPredicate([](UAudioComponent* A) { return A != nullptr; }).Num(), S->NearThunder.Num(), S->FarThunder.Num(), S->Slams.Num(), S->IntenseAt,
        S->PuffSprites && S->PuffSprites->GetMaterial(0) != nullptr);
    return S;
}

void AShipStorm::Calm()
{
    bStarted = false; Clock = 0.f; Wind = Dark = Sea = Rain = Flash = Intense = 0.f; GWind = 0.f;
    Pulses.Reset(); PendingThunder.Reset(); Blobs.Reset(); Splashes.Reset(); Puffs.Reset(); Waves.Reset();
    NextStrike = LightningFrom; NextSpray = 6.f; NextWave = 30.f;
    // The rupture closes and the ladder opens again; the crew go back to what they were doing.
    if (RuptureClock >= 0.f) SetChuckClimbableBlocked(ShipWorld(FVector(CompX1, (CompY0 + CompY1) * .5f, DeckZ)), false);
    Rupture = 0.f; GRupture = 0.f; RuptureClock = -1.f; BuildRupture(0.f);
    if (RuptureLight) RuptureLight->SetIntensity(0.f);
    if (WashMesh) WashMesh->ClearAllMeshSections();
    if (PuffSprites) PuffSprites->SetVisibility(false);
    if (AShipCrew* Crew = AShipCrew::Get(GetWorld())) Crew->EndStorm();
    for (UAudioComponent* A : Loops) if (A) A->Stop();
    for (float& L : LoopNow) L = 0.f;
    for (auto* C : {RainSprites, SplashSprites, SpraySprites}) if (C) C->SetVisibility(false);
    if (BoltMid) BoltMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
    if (BoltGlowMid) BoltGlowMid->SetScalarParameterValue(TEXT("Intensity"), 0.f);
}

void AShipStorm::Begin()
{
    if (bStarted) return;
    bStarted = true;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM start=1 intense_at=%.1f"), IntenseAt);
    // The crew leave their music, dancing and fencing for their storm stations (2026-11-04).
    if (AShipCrew* Crew = AShipCrew::Get(GetWorld())) Crew->BeginStorm();
}

void AShipStorm::SetClock(float Seconds)
{
    if (Seconds > 0.f) Begin();
    Clock = Seconds;
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
    // (User 2026-11-05: the storm and its music begin only once he is up on the weather deck and clear of the companion, so
    // that the rupture closing the way below can never leave him there.)
    if (!bStarted && ShipGate::IsOpen() && ShipRatClearOnDeck(UGameplayStatics::GetPlayerPawn(this, 0))) { Begin(); Clock = 0.f; }
    if (!bStarted) return;
    Clock += Dt;
    const float T = Clock;
    // Gusts (a few seconds) and squalls (most of a minute): the full storm never sits still.
    Gust = .5f + .32f * FMath::Sin(T * 2 * PI / 6.3f) * FMath::Sin(T * 2 * PI / 2.9f + .7f) + .18f * FMath::Sin(T * 2 * PI / 13.7f + 2.f);
    Squall = FMath::Clamp(.5f + .3f * FMath::Sin(T * 2 * PI / 47.f - 1.2f) + .2f * FMath::Sin(T * 2 * PI / 19.f), 0.f, 1.f);
    // The second part (2026-11-04): from the end of the storm track, harder gusts, heavier rain, a bigger sea.
    const float WasIntense = Intense;
    Intense = Smooth(IntenseAt, IntenseAt + IntenseRamp, T);
    if (WasIntense <= 0.f && Intense > 0.f) { NextWave = 4.f; UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_STORM_INTENSE start=1 clock=%.1f"), T); }
    Wind = Smooth(WindIn0, WindIn1, T) * (.82f + .18f * Gust);
    Dark = Smooth(DarkIn0, DarkIn1, T) * (.9f + .1f * Squall);
    Sea = Smooth(SeaIn0, SeaIn1, T);
    Rain = FMath::Min(1.f, Smooth(RainIn0, RainIn1, T) * (.55f + .45f * Squall) + .3f * Intense);
    GWind = Wind * (1.f + .3f * Intense);
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
    TickDeckWaves(Dt, Eye);
    TickPuffs(Dt, Eye);
    TickLightning(Dt);
    TickSound(Dt, bBelow);
    TickRupture(Dt, Pawn);
}

void AShipStorm::TickRain(float Dt, const FVector& Eye, bool bInside)
{
    if (!RainSprites || !SplashSprites) return;
    const int32 Shown = FMath::RoundToInt(MaxDrops * FMath::Clamp(Rain, 0.f, 1.f) * FMath::Lerp(.76f, 1.f, Intense));
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

void AShipStorm::SprayBurst(const FVector& At, float Size, bool bSound)
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
    // (2026-11-04) a few billows of white water over the thrown spray.
    Billow(At + FVector(0, 0, 40), FMath::RoundToInt(2 + 3 * Size), 150.f * Size);
    if (bSound && Slams.Num())
    {
        const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
        const bool bApart = Pawn && ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck);   // below decks: through the hull
        UAudioComponent* A = UGameplayStatics::SpawnSoundAtLocation(this, Slams[FMath::RandRange(0, Slams.Num() - 1)], At, FRotator::ZeroRotator,
            FMath::Clamp(.55f * Size, .3f, .9f) * (bApart ? .6f : 1.f) * FMath::Max(SpeechDuck, .5f), FMath::FRandRange(.9f, 1.05f));
        if (A && bApart) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(700.f); }
    }
}

void AShipStorm::TickSpray(float Dt, const FVector& Eye)
{
    if (!SpraySprites) return;
    // Seas over the bow as she pitches into them, and over the weather (port) rail as she rolls.
    NextSpray -= Dt;
    if (NextSpray <= 0.f && Sea > .35f)
    {
        NextSpray = FMath::FRandRange(1.2f, 3.6f) / FMath::Max(Sea, .4f) / (1.f + 1.3f * Intense);
        const bool bBow = FMath::FRand() < .45f;
        const FVector At = bBow ? FVector(FMath::FRandRange(1350.f, 1650.f), FMath::FRandRange(-180.f, 60.f), RailZ + 30)
                                : FVector(FMath::FRandRange(-700.f, 1200.f), 0, RailZ + 10);
        FVector L = At;
        if (!bBow) L.Y = -ShipOuterHalf(At.X, RailZ) - 20.f;
        SprayBurst(ShipWorld(L), FMath::FRandRange(.7f, 1.25f) * Sea * (1.f + .35f * Intense));
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
    const float Volume = (bNear ? 1.f : FMath::Lerp(.85f, .45f, FMath::Clamp((Distance - 120000.f) / 500000.f, 0.f, 1.f))) * (bBelow ? .7f : 1.f) * FMath::Max(SpeechDuck, .5f);
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
        NextStrike = FMath::Lerp(16.f, 5.5f, Dark) * FMath::FRandRange(.6f, 1.4f) * FMath::Lerp(1.f, .55f, Intense);
        const bool bClose = Dark > .8f && FMath::FRand() < .2f + .2f * Intense;
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
        (bBelow ? Wind * .12f : Wind * (.75f + .25f * Gust) * (1.f + .35f * Up)) * (1.f + .25f * Intense),
        bBelow ? Wind * .85f : Wind * .15f,
        bBelow ? 0.f : Wind * .45f,
        (bBelow ? Sea * .3f : Sea * .7f * (1.f - .4f * Up)) * (1.f + .35f * Intense) };
    // Under the crew's shouted lines (user 2026-11-09: hard to hear over the storm) the rain, the gale, the canvas and the
    // sea sink about 8 dB, quickly, and come back over a second or so after the line; once the storm is intense, about 13 dB
    // (2026-11-10: the intense storm's lines still too quiet). Its sea slams and thunder under a line are halved.
    bool bSpeech = false;
    for (const TWeakObjectPtr<ADockNPC>& N : ADockNPC::All()) bSpeech |= N.IsValid() && N->IsSpeaking() && InShip(N->GetActorLocation());
    SpeechQuiet = bSpeech ? 0.f : SpeechQuiet + Dt;
    const float DuckTo = SpeechQuiet < .4f ? .4f - .18f * Intense : 1.f;
    SpeechDuck = FMath::FInterpConstantTo(SpeechDuck, DuckTo, Dt, DuckTo < SpeechDuck ? 3.f : .7f);
    for (int32 I = 0; I < 6 && I < Loops.Num(); ++I)
    {
        LoopNow[I] = FMath::FInterpTo(LoopNow[I], Want[I], Dt, 1.6f);
        if (!Loops[I]) continue;
        if (LoopNow[I] > .002f && !Loops[I]->IsPlaying()) Loops[I]->Play(FMath::FRandRange(0.f, 20.f));
        Loops[I]->SetVolumeMultiplier(FMath::Max(.001f, LoopNow[I] * SpeechDuck));
    }
}

// ---------------------------------------------------------------------------------------------------------------- the rupture
// (2026-11-04, user: "when the storm starts and player is above deck, have an astral rupture form at ladder locking player
// to staying above deck".) Once he is up on deck in the storm, clear of the hatch, the Astral tears open in the companion
// hatch: a slit first, along the hatch, that widens until the whole opening is the Astral's glow, with the oil-film veils
// standing up round its edge as the sewer's and the temple's ruptures have, and its purple light. The ladder below can't be
// taken hold of; stepping into it is an Astral death, and every death aboard now returns him to the deck.
void AShipStorm::TickRupture(float Dt, const APawn* Pawn)
{
    const FVector2D Mid((CompX0 + CompX1) * .5f, (CompY0 + CompY1) * .5f);
    if (RuptureClock < 0.f)
    {
        if (!ShipRatClearOnDeck(Pawn)) return;   // (not under his feet as he steps off the ladder)
        const FVector L = ShipLocal(Pawn->GetActorLocation());
        RuptureClock = 0.f;
        SetChuckClimbableBlocked(ShipWorld(FVector(CompX1, Mid.Y, DeckZ)), true);
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_AstralSummon_00.SFX_AstralSummon_00")))
            UGameplayStatics::PlaySoundAtLocation(this, S, ShipWorld(FVector(Mid.X, Mid.Y, DeckZ + 40)), 1.f, .82f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RUPTURE forming=1 storm_s=%.1f rat_from_hatch_cm=%.0f"), Clock, FVector2D::Distance(FVector2D(L.X, L.Y), Mid));
    }
    RuptureClock += Dt;
    const float Was = Rupture;
    Rupture = FMath::Clamp(RuptureClock / RuptureForm, 0.f, 1.f);
    GRupture = Rupture;
    const int32 Key = FMath::RoundToInt(Rupture * 80.f);
    if (Key != RuptureBuilt) BuildRupture(Rupture);
    if (Was < 1.f && Rupture >= 1.f) UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_RUPTURE open=1"));
    // Its light: a flare as it tears, then a slow uneven throb.
    if (RuptureLight)
    {
        const float Flare = FMath::Exp(-FMath::Square((RuptureClock - 1.3f) / .7f));
        RuptureLight->SetIntensity(Rupture * (2600.f + 500.f * FMath::Sin(RuptureClock * 2.1f) * FMath::Sin(RuptureClock * .73f + 1.f)) + 7000.f * Flare);
    }
}

void AShipStorm::BuildRupture(float Open)
{
    if (!RuptureMesh) return;
    RuptureBuilt = FMath::RoundToInt(Open * 80.f);
    // The tear: a ragged oval round the hatch's middle, a slit along it at first, clipped to the opening (so at full it is
    // the whole hatch, the ladder's top under the Astral's glow 8 cm down).
    const float X0 = CompX0 + 1, X1 = CompX1 - 1, Y0 = CompY0 + 1, Y1 = CompY1 - 1;
    const FVector2D C((X0 + X1) * .5f, (Y0 + Y1) * .5f);
    const float E = FMath::SmoothStep(0.f, 1.f, Open);
    const float Rx = Open > 0.f ? 115.f * (.3f + .7f * E) : .5f, Ry = Open > 0.f ? FMath::Max(.5f, 115.f * FMath::Pow(E, 1.4f)) : .5f;
    constexpr int32 N = 44;
    TArray<FVector2D> Edge;
    for (int32 I = 0; I < N; ++I)
    {
        const float A = 2.f * PI * I / N;
        const float Rag = 1.f + .14f * FMath::Sin(3.f * A + 1.f) + .08f * FMath::Sin(7.f * A + 2.f) + .05f * FMath::Sin(13.f * A + .4f);
        // (Pointed at its two ends while it is still a slit.)
        const float Point = FMath::Pow(FMath::Abs(FMath::Sin(A)), FMath::Lerp(.6f, 1.f, E));
        FVector2D P = C + FVector2D(Rx * FMath::Cos(A) * Rag, Ry * FMath::Sin(A) * Rag * FMath::Lerp(Point, 1.f, E));
        P.X = FMath::Clamp(P.X, X0, X1); P.Y = FMath::Clamp(P.Y, Y0, Y1);
        Edge.Add(P);
    }
    const float CapZ = Open > 0.f ? DeckZ - 8 : LowerZ - 40;   // (closed: a sliver hidden under the deck)
    // Section 0: the Astral's glow filling it (a fan, both sides).
    {
        TArray<FVector> V, Nn; TArray<int32> T; TArray<FVector2D> UV;
        V.Add(FVector(C.X, C.Y, CapZ)); Nn.Add(FVector::UpVector); UV.Add(FVector2D(.5f, .5f));
        for (const FVector2D& P : Edge) { V.Add(FVector(P.X, P.Y, CapZ)); Nn.Add(FVector::UpVector); UV.Add(FVector2D((P.X - X0) / 120.f, (P.Y - Y0) / 120.f)); }
        for (int32 I = 0; I < N; ++I) { const int32 A = 1 + I, B = 1 + (I + 1) % N; T.Append({0, B, A, 0, A, B}); }
        RuptureMesh->CreateMeshSection(0, V, T, Nn, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    }
    // Section 1: the veils, standing up from its edge and leaning in a little, taller as it opens (V up: the film fades with it).
    {
        TArray<FVector> V, Nn; TArray<int32> T; TArray<FVector2D> UV;
        constexpr int32 Levels = 8;
        const float H = Open > 0.f ? 25.f + 95.f * FMath::Pow(E, .7f) : 1.f;
        for (int32 I = 0; I <= N; ++I)
        {
            // (Round the hatch rather than along its square edge: a ragged oval just outside its coaming, as it opens.)
            const float A = 2.f * PI * (I % N) / N;
            const float Rag = 1.f + .1f * FMath::Sin(3.f * A + 2.f) + .07f * FMath::Sin(8.f * A + .3f) + .04f * FMath::Sin(17.f * A);
            const FVector2D P = Open > 0.f ? C + FVector2D(FMath::Cos(A) * (40.f + 46.f * E), FMath::Sin(A) * (12.f + 74.f * FMath::Pow(E, 1.2f))) * Rag : Edge[I % N];
            const FVector2D In = (C - P).GetSafeNormal();
            for (int32 Lv = 0; Lv <= Levels; ++Lv)
            {
                const float W = Lv / float(Levels), U = I / float(N);
                const float Hh = H * (1.f + .18f * FMath::Sin(U * 19.f) + .1f * FMath::Sin(U * 41.f + 1.f));
                const FVector2D Q = P + In * (W * W * 14.f * FMath::Sin(U * 13.f));
                V.Add(FVector(Q.X, Q.Y, (Open > 0.f ? DeckZ + 4 : LowerZ - 40) + W * Hh));
                Nn.Add(FVector(-In.X, -In.Y, 0));
                UV.Add(FVector2D(U * 3.f, W));
            }
        }
        for (int32 I = 0; I < N; ++I)
            for (int32 Lv = 0; Lv < Levels; ++Lv)
            {
                const int32 A = I * (Levels + 1) + Lv, B = (I + 1) * (Levels + 1) + Lv, Cc = B + 1, D = A + 1;
                T.Append({A, B, Cc, A, Cc, D, A, Cc, B, A, D, Cc});   // (both windings)
            }
        RuptureMesh->CreateMeshSection(1, V, T, Nn, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    }
    static UMaterialInterface* Depth = nullptr; static UMaterialInterface* Oil = nullptr;
    if (!Depth) Depth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_AstralDepth.M_AstralDepth"));
    if (!Oil) Oil = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist"));
    RuptureMesh->SetMaterial(0, Depth); RuptureMesh->SetMaterial(1, Oil);
}

// ---------------------------------------------------------------------------------------------------------------- seas aboard
// (2026-11-04, user: "when the storm intensity increases have more wave influence on ship, more wind, waves crashing on deck
// at times, make it look intense and cool".) In the second part, every 7-13 s a sea breaks over the weather (port) rail
// into the waist: white water bursting up along the rail and billowing over, then a sheet of green water with a foaming front
// running across the deck to the lee rail and draining away. Its front shoves him across the deck if it catches him there.
// (Never over the companion hatch: the rupture is there.)
void AShipStorm::DeckWaveNow(float X, float Size, float From)
{
    const float Half = (230.f + 120.f * Size);
    const float Fs = From >= 0.f ? 1.f : -1.f;   // (the weather rail is -Y; a sea from starboard mirrors it)
    FDeckWave W; W.Size = Size; W.X0 = X - Half; W.X1 = X + Half; W.From = Fs;
    Waves.Add(W); ++DeckWaves;
    // The burst along the rail, and billows of spume over it.
    for (int32 K = 0; K < 5; ++K)
    {
        const float Xk = FMath::Lerp(W.X0 + 40.f, W.X1 - 40.f, K / 4.f) + FMath::FRandRange(-30.f, 30.f);
        const FVector At = ShipWorld(FVector(Xk, -Fs * (-ShipOuterHalf(Xk, RailZ) - 15.f), RailZ + 20));
        SprayBurst(At, 1.5f * Size, false);
        Billow(At + FVector(0, -Fs * 50, 70), 3, 190.f * Size);
    }
    // Its sound: the sea's slam, heavy, and a second as it comes over.
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bApart = Pawn && ShipSpaceAt(Pawn->GetActorLocation()) != int32(EShipSpawn::Deck);
    const FVector Mid = ShipWorld(FVector(X, Fs * ShipOuterHalf(X, RailZ), RailZ));
    for (int32 K = 0; K < 2 && Slams.Num(); ++K)
        if (UAudioComponent* A = UGameplayStatics::SpawnSoundAtLocation(this, Slams[FMath::RandRange(0, Slams.Num() - 1)], Mid, FRotator::ZeroRotator,
            (K == 0 ? 1.f : .7f) * (bApart ? .6f : 1.f) * FMath::Max(SpeechDuck, .5f), K == 0 ? .82f : .95f, K * .25f))
            if (bApart) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(700.f); }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_DECK_WAVE n=%d x=%.0f size=%.2f"), DeckWaves, X, Size);
}

void AShipStorm::TickDeckWaves(float Dt, const FVector& Eye)
{
    NextWave -= Dt;
    if (Intense > .3f && Sea > .9f && NextWave <= 0.f)
    {
        NextWave = FMath::FRandRange(7.f, 13.f);
        // Aft of the main mast, or forward of the companion (well clear of the rupture).
        const float X = FMath::FRand() < .3f ? FMath::FRandRange(-720.f, -560.f) : FMath::FRandRange(180.f, 1050.f);
        DeckWaveNow(X, FMath::FRandRange(.85f, 1.3f) * Intense);
    }
    if (!WashMesh) return;
    if (!Waves.Num()) { if (WashMesh->GetNumSections()) WashMesh->ClearAllMeshSections(); return; }
    TArray<FVector> V, Nn; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> Col;
    ACharacter* Chuck = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    constexpr int32 NX = 26, NY = 22;
    for (int32 Wi = Waves.Num() - 1; Wi >= 0; --Wi)
    {
        FDeckWave& W = Waves[Wi];
        W.Age += Dt;
        if (W.Age > WaveLife) { Waves.RemoveAt(Wi); continue; }
        // The front runs from the weather rail across to the lee one in about two seconds (fast, then slowing), the water
        // behind it thinning; then it all drains away.
        const float Run = FMath::Clamp(W.Age / 2.f, 0.f, 1.f);
        const float Go = 1.f - FMath::Square(1.f - Run);
        const float Body = W.Size * (1.f - FMath::SmoothStep(1.9f, WaveLife, W.Age)) * FMath::SmoothStep(0.f, .2f, W.Age);
        const int32 Base = V.Num();
        for (int32 I = 0; I <= NX; ++I)
        {
            const float U = I / float(NX), X = FMath::Lerp(W.X0, W.X1, U);
            const float Half = ShipInnerHalf(X, DeckZ + 10) - 4.f;
            const float Front = FMath::Lerp(-Half, Half, Go) + 25.f * FMath::Sin(U * 9.f + W.X0) * (1.f - Run * .5f);
            const float Ends = FMath::Sin(PI * U);   // (lower toward its ends along the deck)
            for (int32 J = 0; J <= NY; ++J)
            {
                const float Y = FMath::Lerp(-Half, FMath::Min(Front, Half), J / float(NY));
                const float D = FMath::Max(0.f, Front - Y);   // behind the front
                const float H = Body * Ends * (6.f + 20.f * FMath::Exp(-D / 70.f) + 6.f * FMath::Exp(-(Y + Half) / 60.f) * (1.f - Run));
                V.Add(FVector(X, -W.From * Y, DeckZ + .6f + FMath::Max(0.f, H)));
                Nn.Add(FVector(0, W.From * .25f * FMath::Exp(-D / 70.f), 1).GetSafeNormal());
                UV.Add(FVector2D(X / 300.f, Y / 300.f));
                // R: foam (the front, and the water still churning near the rail); G: how much water (its opacity).
                const float Foam = FMath::Clamp(FMath::Exp(-D / 32.f) + .25f * (1.f - Run) * FMath::Exp(-(Y + Half) / 70.f), 0.f, 1.f);
                const float Amount = FMath::Clamp(Body * Ends * 1.6f, 0.f, 1.f) * FMath::SmoothStep(0.f, 30.f, Front - Y + 4.f);
                Col.Add(FColor(uint8(Foam * 255), uint8(Amount * 255), 0, 255));
            }
        }
        for (int32 I = 0; I < NX; ++I)
            for (int32 J = 0; J < NY; ++J)
            {
                const int32 A = Base + I * (NY + 1) + J, B = A + NY + 1;
                if (W.From < 0.f) T.Append({A, A + 1, B, A + 1, B + 1, B});
                else T.Append({A, B, A + 1, A + 1, B, B + 1});
            }
        // Its front catches him on the deck: a shove toward the lee rail.
        if (Chuck && !W.bShoved && W.Age < 2.2f)
        {
            FVector L = ShipLocal(Chuck->GetActorLocation()) - FVector(0, 0, 34.65f);
            L.Y *= -W.From;   // (into the wave's own frame: from -Y)
            const float Half = ShipInnerHalf(static_cast<float>(L.X), DeckZ + 10) - 4.f;
            const float Front = FMath::Lerp(-Half, Half, Go);
            if (L.X > W.X0 + 20 && L.X < W.X1 - 20 && L.Z > DeckZ - 5 && L.Z < DeckZ + 45 && Front > L.Y && Front - L.Y < 80.f)
            {
                W.bShoved = true; ++WaveShoves;
                Chuck->LaunchCharacter(FVector(-60.f, -W.From * 380.f * W.Size, 150.f), true, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_WAVE_SHOVE n=%d at=%s"), WaveShoves, *L.ToString());
            }
        }
    }
    if (V.Num())
    {
        WashMesh->CreateMeshSection(0, V, T, Nn, UV, Col, TArray<FProcMeshTangent>(), false);
        static UMaterialInterface* Wash = nullptr;
        if (!Wash) Wash = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Ship/M_ShipDeckWash.M_ShipDeckWash"));
        WashMesh->SetMaterial(0, Wash);
    }
    else WashMesh->ClearAllMeshSections();
}

// ---------------------------------------------------------------------------------------------------------------- spume
void AShipStorm::Billow(const FVector& At, int32 Count, float Size)
{
    for (int32 I = 0; I < Count && Puffs.Num() < MaxPuffs; ++I)
    {
        FPuff P;
        P.P = At + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-20.f, 40.f)) * (Size / 200.f);
        P.V = FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(60.f, 260.f), FMath::FRandRange(220.f, 520.f));
        P.Age = -FMath::FRandRange(0.f, .15f); P.Life = FMath::FRandRange(1.3f, 2.3f); P.Size = Size * FMath::FRandRange(.7f, 1.3f);
        P.Frame0 = FMath::FRandRange(0.f, 10.f);
        Puffs.Add(P);
    }
}

void AShipStorm::TickPuffs(float Dt, const FVector& Eye)
{
    if (!PuffSprites) return;
    const FVector Blow = WindVelocity(Wind);
    const FVector O = ShipOrigin();
    TArray<FTransform> X; X.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxPuffs);
    int32 N = 0;
    for (int32 I = Puffs.Num() - 1; I >= 0; --I)
    {
        FPuff& P = Puffs[I];
        P.Age += Dt;
        if (P.Age < 0.f) continue;
        if (P.Age > P.Life) { Puffs.RemoveAtSwap(I); continue; }
        // Thrown up, slowed by the air, carried off down the wind, settling.
        P.V.X += (Blow.X - P.V.X) * 1.1f * Dt; P.V.Y += (Blow.Y - P.V.Y) * 1.1f * Dt; P.V.Z -= 330.f * Dt; P.V.Z *= 1.f - .6f * Dt;
        P.P += P.V * Dt;
        const FVector L = P.P - O;
        if (L.Z < SurfaceZ(L) + P.Size * .25f) { P.P.Z = O.Z + SurfaceZ(L) + P.Size * .25f; P.V.Z = FMath::Max(0.f, static_cast<float>(P.V.Z)); }
        if (N >= MaxPuffs) continue;
        const float U = P.Age / P.Life;
        FVector Z = Eye - P.P; const float Dist = Z.Size(); if (!Z.Normalize()) Z = FVector::UpVector;
        if (Dist < 40.f) continue;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal(), Y0 = FVector::CrossProduct(Z, X0);
        const float Spin = I * 2.3f + P.Age * .4f;
        const FVector Xa = X0 * FMath::Cos(Spin) + Y0 * FMath::Sin(Spin), Ya = Y0 * FMath::Cos(Spin) - X0 * FMath::Sin(Spin);
        X[N] = FTransform(FMatrix(Xa, Ya, Z, FVector::ZeroVector).ToQuat(), P.P, FVector(P.Size * (.55f + .85f * U) / 100.f));
        PuffSprites->SetCustomDataValue(N, 0, P.Frame0 + U * 46.f, false);
        PuffSprites->SetCustomDataValue(N, 1, 0.f, false);
        // (Thinner close to the view: a billow must never fill the screen.)
        PuffSprites->SetCustomDataValue(N, 2, .6f * FMath::SmoothStep(0.f, .1f, U) * FMath::Pow(1.f - U, 1.4f) * FMath::SmoothStep(60.f, 420.f, Dist), false);
        ++N;
    }
    PuffsShown = N;
    PuffSprites->SetVisibility(N > 0);
    PuffSprites->BatchUpdateInstancesTransforms(0, X, true, true, true);
}
