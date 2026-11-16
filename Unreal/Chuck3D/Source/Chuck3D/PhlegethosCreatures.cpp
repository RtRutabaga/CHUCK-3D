// Phlegethos's creatures (Claude, 2026-11-16). See PhlegethosCreatures.h and docs/regions/PHLEGETHOS.md#creatures.
#include "PhlegethosCreatures.h"
#include "PhlegethosSetting.h"
#include "ChuckCharacter.h"
#include "ChuckFlames.h"
#include "ChultTempleInterior.h"
#include "TempleSnake.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

using namespace Phleg;

namespace
{
    TArray<TWeakObjectPtr<APhlegFlameskull>> GSkulls;
    constexpr float Stand = 34.65f;   // his capsule's half height: his centre over his feet
}

bool APhlegFlameskull::bCalm = false;
int32 APhlegFlameskull::Burns = 0;
const TArray<TWeakObjectPtr<APhlegFlameskull>>& APhlegFlameskull::All() { return GSkulls; }

APhlegFlameskull::APhlegFlameskull()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SkullMesh(TEXT("/Game/Art/ChultTemple/SM_TempleSkull.SM_TempleSkull"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Pass(TEXT("/Game/Art/Audio/SFX/SFX_TempleRunFireRoar_01.SFX_TempleRunFireRoar_01"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Hiss(TEXT("/Game/Art/Audio/SFX/SFX_TempleRunFireHiss_01.SFX_TempleRunFireHiss_01"));
    PassSound = Pass.Object; BurnSound = Hiss.Object;
    // The skull's origin is under its jaw, its face +X, 19 cm long: its middle at the actor.
    Skull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Skull"));
    Skull->SetupAttachment(RootComponent);
    Skull->SetStaticMesh(SkullMesh.Object);
    Skull->SetRelativeLocation(FVector(-1.2f, 0, -9.6f));
    Skull->SetRelativeScale3D(FVector(1.2f));   // (a big man's skull: real size read small beside him)
    Skull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Skull->SetCastShadow(false);
    // The eyes: embers deep in the sockets.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        auto* Eye = CreateDefaultSubobject<UStaticMeshComponent>(Side ? TEXT("EyeR") : TEXT("EyeL"));
        Eye->SetupAttachment(Skull);
        Eye->SetStaticMesh(Sphere.Object);
        Eye->SetRelativeLocation(FVector(6.f, Side ? 2.9f : -2.9f, 9.2f));
        Eye->SetRelativeScale3D(FVector(.022f, .026f, .02f));
        Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Eye->SetCastShadow(false);
        (Side ? EyeR : EyeL) = Eye;
    }
    // Its fire's light on the ground and the lava under it.
    Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
    Light->SetupAttachment(RootComponent);
    Light->SetRelativeLocation(FVector(16.f, 0, 6.f));   // (before its face: its own fire lights the bone, else it was a black ball)
    Light->SetLightColor(FLinearColor(1.f, .42f, .11f));
    Light->SetIntensity(700.f);
    Light->SetAttenuationRadius(420.f);
    Light->SetCastShadows(false);
    Light->SetSourceRadius(8.f);
}

void APhlegFlameskull::BeginPlay()
{
    Super::BeginPlay();
    GSkulls.Add(this);
    // (Loaded here, not held by the constructor: a constructor's finder roots it, and the material script cannot then
    // rebuild it.)
    if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Phlegethos/M_PhlegMolten.M_PhlegMolten"))) { EyeL->SetMaterial(0, Glow); EyeR->SetMaterial(0, Glow); }
}
void APhlegFlameskull::EndPlay(const EEndPlayReason::Type Reason)
{
    GSkulls.RemoveAll([this](const TWeakObjectPtr<APhlegFlameskull>& P) { return !P.IsValid() || P.Get() == this; });
    Super::EndPlay(Reason);
}

void APhlegFlameskull::Init(const FHaunt& InHaunt, float InPhase)
{
    Haunt = InHaunt;
    Haunt.Axis = Haunt.Axis.GetSafeNormal2D();
    Phase = InPhase;
    Seed = FMath::Frac(InPhase * 7.31f) * 100.f;
    Clock = 0;
    Last = Place(0);
    SetActorLocation(Last);
    if (AChuckFlames* Fire = AChuckFlames::Get(GetWorld())) Flame = Fire->AddMoving(this, Last - FVector(0, 0, 3), 26.f, 32.f);
}

FVector APhlegFlameskull::Place(float T) const
{
    // 2D: along = sin(t * travel) * range, across = sin(t * travel * 3.1) * weave; here a bob as well, and the beats a
    // little apart so it never settles into a figure.
    const float A = T * Haunt.Rate + Phase;
    const FVector Side(-Haunt.Axis.Y, Haunt.Axis.X, 0);
    return Haunt.Home + Haunt.Axis * (FMath::Sin(A) * Haunt.Range) + Side * (FMath::Sin(A * 3.1f + Phase * 2.f) * Haunt.Weave)
        + FVector(0, 0, FMath::Sin(A * 2.3f + 1.f + Phase) * Haunt.Bob);
}

void APhlegFlameskull::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, .05f);
    UWorld* World = GetWorld();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    const APhlegethosSetting* Stage = APhlegethosSetting::Get(World);
    // Asleep (hidden, still) while he is far off or the stage is not standing.
    const bool bFar = !Chuck || !Stage || Stage->IsHidden() || FVector::Dist(Chuck->GetActorLocation(), Haunt.Home) > 9000.f;
    if (bFar != bAsleep) { bAsleep = bFar; SetActorHiddenInGame(bFar); }
    if (bFar) return;

    Clock += Dt;
    const FVector P = Place(Clock);
    const FVector V = (P - Last) / FMath::Max(Dt, 1e-3f);
    Last = P;
    // It looks where it goes, banking into the weave, nodding a little.
    FRotator Want = Facing;
    if (V.SizeSquared2D() > 400.f)
    {
        const FVector Side(-Haunt.Axis.Y, Haunt.Axis.X, 0);
        Want = FRotator(FMath::Clamp(-V.Z * .06f, -20.f, 20.f), V.Rotation().Yaw, FMath::Clamp(FVector::DotProduct(V, Side) * .05f, -25.f, 25.f));
    }
    Facing = FMath::RInterpTo(Facing, Want, Dt, 9.f);
    SetActorLocationAndRotation(P, Facing);
    if (AChuckFlames* Fire = AChuckFlames::Get(World)) Fire->SetBase(Flame, P - FVector(0, 0, 3) - V.GetSafeNormal() * 4.f);
    Light->SetIntensity(700.f * (.8f + .15f * FMath::Sin(Clock * 17.f + Seed) + .08f * FMath::Sin(Clock * 41.f + Seed * 2.f)));

    // A touch: its burning skull against his body (feet to shoulders).
    const FVector C = Chuck->GetActorLocation();
    const FVector Feet = C - FVector(0, 0, Stand);
    const FVector Near = FMath::ClosestPointOnSegment(P, Feet + FVector(0, 0, 12), Feet + FVector(0, 0, 56));
    const float D = FVector::Dist(P, Near);
    if (Haunt.bHarmful && !bCalm && Stage->IsLive() && D < Radius + 18.f && !Chuck->IsAstral())
    {
        // Thrown back along the way (-X: the route runs +X), never off the spine's side.
        if (Chuck->TakeBite(C + FVector(100, 0, 0), Burn, true, false))
        {
            ++Burns;
            if (BurnSound) UGameplayStatics::PlaySoundAtLocation(this, BurnSound, P, .45f, 1.1f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_SKULL_BURN at=%s sanity=%d"), *PhlegLocal(C).ToString(), Chuck->GetSanity());
        }
    }
    // Its rush of fire going past him, soft.
    PassWait -= Dt;
    const bool bNear = D < 220.f;
    if (bNear && !bWasNear && PassWait <= 0 && PassSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, PassSound, P, .16f, FMath::FRandRange(1.25f, 1.45f));
        PassWait = 1.2f;
    }
    bWasNear = bNear;
}

namespace
{
    // The road spine's centre (local Y) at X.
    float SpineY(float X)
    {
        for (const FSeg& S : Spine) if (X >= S.X0 - 60 && X <= S.X1 + 60) return FMath::Lerp(S.Y0, S.Y1, FMath::SmoothStep(0.f, 1.f, (X - S.X0) / (S.X1 - S.X0)));
        return 0.f;
    }
    // The walkable ground under a local point (a trace down onto the built stage), or G.
    float GroundAt(UWorld* World, float X, float Y)
    {
        FHitResult Hit;
        const FVector Top = PhlegWorld(FVector(X, Y, G + 1500));
        if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 2500), ECC_Visibility)) return static_cast<float>(PhlegLocal(Hit.ImpactPoint).Z);
        return G;
    }
}

void SpawnPhlegethosCreatures(APhlegethosSetting* Stage)
{
    if (!Stage || GSkulls.Num()) return;
    UWorld* World = Stage->GetWorld();
    FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Skull = [&](const APhlegFlameskull::FHaunt& H, float Phase)
    {
        if (auto* S = World->SpawnActor<APhlegFlameskull>(H.Home, FRotator::ZeroRotator, SP)) S->Init(H, Phase);
    };
    // The lava road, where the geysers stood (the user, 2026-11-16: "later we'll add flame skull hostile npcs here as the
    // hazard"): four weaving across the spine and out over the lava either side, chest high on him. Each crosses the spine
    // in under half a second, then is out over the lava for two: watch it, go behind it.
    const float RoadAt[] = {3350, 4250, 5300, 6450};
    for (int32 I = 0; I < UE_ARRAY_COUNT(RoadAt); ++I)
    {
        APhlegFlameskull::FHaunt H;
        H.Home = PhlegWorld(FVector(RoadAt[I], SpineY(RoadAt[I]), G + 46));
        H.Axis = FVector(.12f * (I & 1 ? 1 : -1), 1, 0);
        H.Range = 260; H.Weave = 45; H.Bob = 14; H.Rate = 1.25f + .08f * I;
        Skull(H, I * 1.9f);
    }
    // The ascent's ramp, where the bombs fell: three across it.
    const float RampAt[] = {21050, 21600, 22120};
    for (int32 I = 0; I < UE_ARRAY_COUNT(RampAt); ++I)
    {
        APhlegFlameskull::FHaunt H;
        H.Home = PhlegWorld(FVector(RampAt[I], 0, GroundAt(World, RampAt[I], 0) + 46));
        H.Axis = FVector(.15f * (I & 1 ? -1 : 1), 1, 0);
        H.Range = RampHalf + 80; H.Weave = 50; H.Bob = 14; H.Rate = 1.2f + .1f * I;
        Skull(H, 1.f + I * 2.3f);
    }
    // Out over the lava as the setting's life: off the lake's columns and the gorge's river, well out of his reach.
    const FVector Far[] = {FVector(8900, 900, G + 160), FVector(10900, -950, G + 220), FVector(11800, 1100, G + 120), FVector(14400, 900, G + 260), FVector(-1500, -3300, G + 200)};
    for (int32 I = 0; I < UE_ARRAY_COUNT(Far); ++I)
    {
        APhlegFlameskull::FHaunt H;
        H.Home = PhlegWorld(Far[I]);
        H.Axis = FVector(FMath::Cos(I * 1.3f), FMath::Sin(I * 1.3f), 0);
        H.Range = 450; H.Weave = 90; H.Bob = 40; H.Rate = .9f + .1f * I; H.bHarmful = false;
        Skull(H, I * 2.7f);
    }
    // Fire snakes (2D: the temple snake with only its look changed; it keeps to the shores and the yard): the temple's
    // pit viper, one scratch kills it, but burning. In the hills beside the path, in the wastes, before the gate. Each
    // keeps to its own patch of ground (Leash, the stage frame) and goes back to its place when he is gone.
    struct FSnakeAt { float X, Y, HalfX, HalfY, Yaw; };
    const FSnakeAt Snakes[] = {
        {-8300, HillsPathY(-8300) + 260, 380, 300, 200}, {-6900, HillsPathY(-6900) - 280, 400, 320, 30}, {-5200, HillsPathY(-5200) + 300, 420, 320, 250}, {-3700, HillsPathY(-3700) - 240, 380, 300, 120},
        {28400, ApproachPathY(28400) + 280, 420, 320, 90}, {31800, ApproachPathY(31800) - 300, 420, 320, 300}, {35600, ApproachPathY(35600) + 260, 400, 320, 180}, {39000, ApproachPathY(39000) - 260, 420, 320, 20},
        {GateX - 2500, -700, 450, 350, 160}, {GateX - 1700, 650, 450, 350, 340}, {GateX - 900, -250, 400, 300, 60}};
    int32 SnakesPlaced = 0;
    for (const FSnakeAt& A : Snakes)
    {
        const float Z = GroundAt(World, A.X, A.Y);
        if (Z < G - 60) continue;   // (never out on the lava)
        FTempleSnakePost Post;
        Post.Feet = PhlegWorld(FVector(A.X, A.Y, Z));
        Post.Yaw = A.Yaw; Post.bWander = true; Post.bFire = true;
        Post.Leash = FBox2D(FVector2D(A.X - A.HalfX, A.Y - A.HalfY), FVector2D(A.X + A.HalfX, A.Y + A.HalfY));
        SnakesPlaced += ATempleSnake::SpawnAt(World, Post) != nullptr;
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_CREATURES skulls=%d harmful=%d fire_snakes=%d"), PhlegethosFlameskullCount(false), PhlegethosFlameskullCount(true), SnakesPlaced);
}

int32 PhlegethosFlameskullCount(bool bHarmfulOnly)
{
    int32 N = 0;
    for (const auto& S : GSkulls) if (S.IsValid() && (!bHarmfulOnly || S->GetHaunt().bHarmful)) ++N;
    return N;
}
