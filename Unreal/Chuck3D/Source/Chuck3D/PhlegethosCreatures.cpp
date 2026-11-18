// Phlegethos's creatures (Claude, 2026-11-16). See PhlegethosCreatures.h and docs/regions/PHLEGETHOS.md#creatures.
#include "PhlegethosCreatures.h"
#include "TempleSanctumBattle.h"
#include "PhlegethosSetting.h"
#include "ChuckCharacter.h"
#include "ChuckFlames.h"
#include "ChultTempleInterior.h"
#include "TempleSnake.h"
#include "TempleSkeleton.h"
#include "DockNPC.h"
#include "Kismet/KismetMathLibrary.h"
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
    // (the stage's creatures are its own: the temple's skeletons' leashes and sounds are in the temple's frame)
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
    // Lemures (2D: the zombie's and skeleton's lifecycle, slower and more durable): beside the hills' path, the wastes', and
    // before the gate. Some lie in the ash until he is near, then rise. Each keeps to its own patch of ground.
    struct FLemureAt { float X, Y; bool bDormant; };
    const FLemureAt Lemures[] = {
        {-7600, HillsPathY(-7600) - 250, false}, {-6100, HillsPathY(-6100) + 260, false}, {-4500, HillsPathY(-4500) - 260, true},
        {29800, ApproachPathY(29800) - 270, false}, {33000, ApproachPathY(33000) + 280, true}, {36600, ApproachPathY(36600) - 260, false},
        {GateX - 2100, 500, false}, {GateX - 1300, -550, true}};
    int32 LemuresPlaced = 0;
    for (const FLemureAt& A : Lemures)
    {
        const float Z = GroundAt(World, A.X, A.Y);
        if (Z < G - 60) continue;
        FTempleSkeletonPost Post;
        Post.Kind = 1; Post.bDormant = A.bDormant; Post.Space = TEXT("phlegethos");
        Post.Feet = PhlegWorld(FVector(A.X, A.Y, Z));
        const float PathY = A.X < HillsJoinX ? HillsPathY(A.X) : A.X < ForecourtX ? ApproachPathY(A.X) : 0.f;
        Post.Yaw = PathY > A.Y ? 90.f : -90.f;   // facing the path
        Post.Leash = FBox2D(FVector2D(A.X - 480, FMath::Min(A.Y, PathY) - 420), FVector2D(A.X + 480, FMath::Max(A.Y, PathY) + 420));
        LemuresPlaced += ATempleSkeleton::SpawnAt(World, Post) != nullptr;
    }
    // Spined devils: three on columns in the river beside the gorge's shelf, throwing across it at the cliff; three on
    // low columns beside the wastes' path, throwing across it.
    int32 DevilsPlaced = 0;
    auto Devil = [&](const APhlegSpinedDevil::FPerch& P) { if (auto* D = World->SpawnActor<APhlegSpinedDevil>(P.Seat, FRotator(0, P.Yaw, 0), SP)) { D->Init(P); ++DevilsPlaced; } };
    const float ShelfAt[] = {13500, 14450, 15950};
    for (int32 I = 0; I < UE_ARRAY_COUNT(ShelfAt); ++I)
    {
        APhlegSpinedDevil::FPerch P;
        P.Seat = PhlegWorld(FVector(ShelfAt[I], 230, G + 95)); P.Yaw = -90.f;
        P.Aim = PhlegWorld(FVector(ShelfAt[I] - 40, CliffY + 8, (ShelfAt[I] > Break2X0 ? LowShelf : G) + 32));
        P.Delay = I * .7f; P.ColumnDrop = 460.f;
        Devil(P);
    }
    const float WasteAt[] = {31000, 34500, 38200};
    for (int32 I = 0; I < UE_ARRAY_COUNT(WasteAt); ++I)
    {
        const float X = WasteAt[I], Side = I & 1 ? -1.f : 1.f, PathY = ApproachPathY(X);
        APhlegSpinedDevil::FPerch P;
        P.Seat = PhlegWorld(FVector(X, PathY + Side * 330, GroundAt(World, X, PathY + Side * 330) + 70)); P.Yaw = Side > 0 ? -90.f : 90.f;
        P.Aim = PhlegWorld(FVector(X + 30, PathY - Side * 250, GroundAt(World, X + 30, PathY) + 32));
        P.Delay = .3f + I * .8f; P.ColumnDrop = 460.f;
        Devil(P);
    }
    // The Pit Fiend (2D: the colossus, wholly avoidable; its battle with the heroes is not built): its round through the
    // wastes, along the path itself, weaving across it. It notices him from 22 m in front of it.
    {
        const FVector2D Round[] = {{31200, 300}, {33600, -300}, {36000, 300}, {37800, -100}, {35800, -300}, {33400, 300}, {30600, -250}};   // (2026-11-16: along the path itself, weaving across it, where it is seen: 15 m off, it was hidden among the boulders)
        FTempleSkeletonPost Post;
        Post.Kind = 2; Post.Space = TEXT("wastes");
        for (const FVector2D& R : Round)
        {
            // (on the broken ground, never in one of the lava hollows off the path: in toward the path until it is ground)
            float Y = ApproachPathY(R.X) + R.Y, Z = GroundAt(World, R.X, Y);
            for (float K = .85f; (Z < FortressTop - 400.f || Z > FortressTop + 150.f) && K > .1f; K -= .15f) { Y = ApproachPathY(R.X) + R.Y * K; Z = GroundAt(World, R.X, Y); }
            Post.Route.Add(PhlegWorld(FVector(R.X, Y, Z)));
        }
        Post.Feet = Post.Route[0];
        Post.Yaw = (Post.Route[1] - Post.Route[0]).Rotation().Yaw;
        Post.Leash = FBox2D(FVector2D(29200, -3600), FVector2D(39800, 3600));
        ATempleSkeleton::SpawnAt(World, Post);
    }
    // The trio's battle at the fortress (FortressBattle.cpp, 2026-11-16): streamed in as he crosses the wastes.
    ATempleSanctumBattle::SpawnFortress(World);
    // The bus stop's commuter (2026-11-17; PhlegethosBusStop.inl): on the pavement by the kerb, looking up the road for
    // his bus; the city's response to the rat (the 2D game's city.json "businessman"). No umbrella (the user).
    {
        FVector2D C, T, Nn; BusStopFrame(C, T, Nn);
        const FVector2D At = C + T * 70.f + Nn * 585.f;
        const FVector2D Look = (-T * .55f - Nn * .85f).GetSafeNormal();
        if (ADockNPC* Man = ADockNPC::SpawnHuman(World, EDockHuman::Businessman, PhlegWorld(FVector(At.X, At.Y, GroundAt(World, At.X, At.Y))), FMath::RadiansToDegrees(FMath::Atan2(Look.Y, Look.X))))
        {
            Man->DisplayName = TEXT("Businessman");
            Man->Lines = {TEXT("Ah! A rat!")};
            Man->Tags.Add(TEXT("PhlegBusinessman"));
            UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_BUSINESSMAN at=%s"), *PhlegLocal(Man->GetActorLocation()).ToString());
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_CREATURES skulls=%d harmful=%d fire_snakes=%d lemures=%d devils=%d fiend=%d"), PhlegethosFlameskullCount(false),
        PhlegethosFlameskullCount(true), SnakesPlaced, LemuresPlaced, DevilsPlaced, PhlegethosPitFiend() != nullptr);
}

int32 PhlegethosLemureCount()
{
    int32 N = 0;
    for (const auto& S : ATempleSkeleton::All()) if (S.IsValid() && S->IsLemure() && FCString::Strcmp(S->GetPost().Space, TEXT("fortress")) != 0) ++N;   // (the region's own, not the trio's battle's)
    return N;
}

ATempleSkeleton* PhlegethosPitFiend()
{
    for (const auto& S : ATempleSkeleton::All()) if (S.IsValid() && S->IsFiend() && !S->IsFortressFiend()) return S.Get();   // (the wastes' own, not the fortress's)
    return nullptr;
}

int32 PhlegethosFlameskullCount(bool bHarmfulOnly)
{
    int32 N = 0;
    for (const auto& S : GSkulls) if (S.IsValid() && (!bHarmfulOnly || S->GetHaunt().bHarmful)) ++N;
    return N;
}

// ---------------------------------------------------------------------------------------------------------------------
// The spined devils (2026-11-16).

namespace
{
    TArray<TWeakObjectPtr<APhlegSpinedDevil>> GDevils;
    constexpr float DevilSize = 1.25f;                 // (the model is 45 cm crouched; a little more, to read beside him)
    const FVector HeadPivot(4, 0, 33), WingPivotL(-5, 5, 31), WingPivotR(-5, -5, 31), TailPivot(-8, 0, 9);
    const FVector TailTipLocal(-28, -1, 23);           // the burning tuft, in the tail's frame
    constexpr int32 SpinePool = 3;
    UStaticMesh* CreatureMesh(const TCHAR* Name)
    {
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Art/Phlegethos/Creatures/%s.%s"), Name, Name));
    }
}

int32 APhlegSpinedDevil::Thrown = 0;
int32 APhlegSpinedDevil::Hits = 0;
int32 APhlegSpinedDevil::Scratched = 0;
int32 APhlegSpinedDevil::Aimed = 0;
const TArray<TWeakObjectPtr<APhlegSpinedDevil>>& APhlegSpinedDevil::All() { return GDevils; }

APhlegSpinedDevil::APhlegSpinedDevil()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // Its column (collision: he can climb onto it, and the spines' flight starts clear of it).
    Column = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Column"));
    Column->SetupAttachment(RootComponent);
    auto* Devil = CreateDefaultSubobject<USceneComponent>(TEXT("Devil"));
    Devil->SetupAttachment(RootComponent);
    Devil->SetRelativeScale3D(FVector(DevilSize));
    auto Part = [&](const TCHAR* Name, USceneComponent* Parent, const FVector& At)
    {
        auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        C->SetupAttachment(Parent);
        C->SetRelativeLocation(At);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return C;
    };
    Body = Part(TEXT("Body"), Devil, FVector::ZeroVector);
    Head = Part(TEXT("Head"), Devil, HeadPivot);
    WingL = Part(TEXT("WingL"), Devil, WingPivotL);
    WingR = Part(TEXT("WingR"), Devil, WingPivotR);
    Tail = Part(TEXT("Tail"), Devil, TailPivot);
    for (int32 I = 0; I < SpinePool; ++I)
    {
        auto* S = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Spine%d"), I));
        S->SetupAttachment(RootComponent);
        S->SetUsingAbsoluteLocation(true); S->SetUsingAbsoluteRotation(true); S->SetUsingAbsoluteScale(true);
        S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        S->SetCastShadow(false);
        S->SetRelativeScale3D(FVector(1.3f));
        S->SetHiddenInGame(true);
        SpineMeshes.Add(S);
    }
    // The smoulder of its spines on it.
    Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
    Light->SetupAttachment(Tail);
    Light->SetRelativeLocation(TailTipLocal);
    Light->SetLightColor(FLinearColor(1.f, .45f, .12f));
    Light->SetIntensity(160.f);
    Light->SetAttenuationRadius(220.f);
    Light->SetCastShadows(false);
}

void APhlegSpinedDevil::BeginPlay()
{
    Super::BeginPlay();
    GDevils.Add(this);
    // (Loaded here, not held by the constructor: the import script replaces these.)
    Column->SetStaticMesh(CreatureMesh(TEXT("SM_DevilPerch")));
    Body->SetStaticMesh(CreatureMesh(TEXT("SM_SpinedDevil_Body")));
    Head->SetStaticMesh(CreatureMesh(TEXT("SM_SpinedDevil_Head")));
    WingL->SetStaticMesh(CreatureMesh(TEXT("SM_SpinedDevil_WingL")));
    WingR->SetStaticMesh(CreatureMesh(TEXT("SM_SpinedDevil_WingR")));
    Tail->SetStaticMesh(CreatureMesh(TEXT("SM_SpinedDevil_Tail")));
    UStaticMesh* SpineMesh = CreatureMesh(TEXT("SM_DevilSpine"));
    for (UStaticMeshComponent* S : SpineMeshes) S->SetStaticMesh(SpineMesh);
    auto Sound = [](const TCHAR* Name) { return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name)); };
    ThrowSound = Sound(TEXT("SFX_HeroArrowFlight_00")); StoneSound = Sound(TEXT("SFX_HeroArrowHit_01"));
    BurnSound = Sound(TEXT("SFX_TempleRunFireHiss_00")); HissSound = Sound(TEXT("SFX_RaptorHiss_00"));
}

void APhlegSpinedDevil::EndPlay(const EEndPlayReason::Type Reason)
{
    GDevils.RemoveAll([this](const TWeakObjectPtr<APhlegSpinedDevil>& P) { return !P.IsValid() || P.Get() == this; });
    Super::EndPlay(Reason);
}

void APhlegSpinedDevil::Init(const FPerch& InPerch)
{
    Perch = InPerch;
    SetActorLocationAndRotation(Perch.Seat, FRotator(0, Perch.Yaw, 0));
    Column->SetRelativeLocation(FVector(0, 0, -Perch.ColumnDrop));
    Column->SetRelativeRotation(FRotator(0, FMath::Frac(Perch.Seat.X * .013f) * 360.f, 0));
    Clock = 0; NextThrow = Perch.Delay + Interval; FlickT = -1; AimAt = Perch.Aim;
    Flying.SetNum(SpinePool);
    if (AChuckFlames* Fire = AChuckFlames::Get(GetWorld()))
        for (FSpine& S : Flying) S.Flame = Fire->AddMoving(this, Perch.Seat - FVector(0, 0, 8000), 8.f, 12.f);
}

FVector APhlegSpinedDevil::GetCentre() const { return GetActorTransform().TransformPosition(FVector(2, 0, 18) * DevilSize); }
FVector APhlegSpinedDevil::TailTip() const { return Tail->GetComponentTransform().TransformPosition(TailTipLocal); }

void APhlegSpinedDevil::Throw()
{
    // A spine from the tuft at its tail's end, through the aim point across the lane and on until it meets stone.
    int32 Free = INDEX_NONE;
    for (int32 I = 0; I < Flying.Num(); ++I) if (Flying[I].Life < 0 && Flying[I].Stuck < 0) { Free = I; break; }
    if (Free == INDEX_NONE) return;
    FSpine& S = Flying[Free];
    S.Pos = TailTip();
    S.Vel = (AimAt - S.Pos).GetSafeNormal() * SpineSpeed;
    S.Life = 3.f; S.Stuck = -1.f;
    SpineMeshes[Free]->SetHiddenInGame(false);
    ++Thrown;
    if (ThrowSound) UGameplayStatics::PlaySoundAtLocation(this, ThrowSound, S.Pos, .2f, 1.35f);   // (2026-11-17: quieter, the user; was .5)
}

void APhlegSpinedDevil::TakeAim(AChuckCharacter* Chuck)
{
    // (2026-11-17, the user: "intentionally target chuck (poorly)".) As its tail winds back: at him if he is in front of it, in
    // sight and in range, where he is now (it never leads him: on the move he is past it), and usually wide of him.
    AimAt = Perch.Aim;
    if (!Chuck || Chuck->IsAstral()) return;
    const FVector Chest = Chuck->GetActorLocation() - FVector(0, 0, Stand - 34.f), From = GetCentre();
    const FVector To = Chest - From;
    if (To.Size() > AimRange || FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()) < .1f) return;
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(DevilAim), false, this); Q.AddIgnoredActor(Chuck);
    if (GetWorld()->LineTraceSingleByChannel(Hit, From, Chest, ECC_Visibility, Q)) return;
    const bool bWide = FMath::FRand() < MissChance;
    const float Miss = bWide ? FMath::FRandRange(35.f, 110.f) : FMath::FRandRange(0.f, 12.f), A = FMath::FRandRange(0.f, 2.f * PI);
    const FVector Side = FVector::CrossProduct(To.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
    AimAt = Chest + Side * FMath::Cos(A) * Miss + FVector::UpVector * FMath::Sin(A) * Miss * .5f;
    ++Aimed;
}

void APhlegSpinedDevil::TickSpines(float Dt, AChuckCharacter* Chuck)
{
    AChuckFlames* Fire = AChuckFlames::Get(GetWorld());
    for (int32 I = 0; I < Flying.Num(); ++I)
    {
        FSpine& S = Flying[I];
        UStaticMeshComponent* M = SpineMeshes[I];
        if (S.Stuck >= 0)
        {
            // Burning out where it struck.
            S.Stuck -= Dt;
            if (Fire) Fire->SetBase(S.Flame, S.Stuck > .4f ? S.Pos : Perch.Seat - FVector(0, 0, 8000));
            if (S.Stuck < 0) M->SetHiddenInGame(true);
            continue;
        }
        if (S.Life < 0) continue;
        S.Life -= Dt;
        const FVector From = S.Pos, To = S.Pos + S.Vel * Dt;
        bool bEnd = S.Life < 0;
        // Him: the spine's step against his body (feet to shoulders).
        if (Chuck && !Chuck->IsAstral() && !APhlegFlameskull::bCalm)
        {
            const FVector C = Chuck->GetActorLocation(), Feet = C - FVector(0, 0, Stand);
            FVector PA, PB;
            FMath::SegmentDistToSegmentSafe(From, To, Feet + FVector(0, 0, 10), Feet + FVector(0, 0, 58), PA, PB);
            if (FVector::Dist(PA, PB) < 16.f)
            {
                // Knocked on along its flight (bZombieHit: a shove, not a launch).
                if (Chuck->TakeBite(C - S.Vel.GetSafeNormal2D() * 100.f, SpineBurn, true, false))
                {
                    ++Hits;
                    if (BurnSound) UGameplayStatics::PlaySoundAtLocation(this, BurnSound, PA, .35f, 1.2f);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_SPINE_HIT at=%s sanity=%d"), *PhlegLocal(C).ToString(), Chuck->GetSanity());
                }
                S.Life = -1.f; M->SetHiddenInGame(true);
                if (Fire) Fire->SetBase(S.Flame, Perch.Seat - FVector(0, 0, 8000));
                continue;
            }
        }
        // Stone: it sticks and burns out.
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(DevilSpine), false, this);
        if (Chuck) Q.AddIgnoredActor(Chuck);
        if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
        {
            S.Pos = Hit.ImpactPoint - S.Vel.GetSafeNormal() * 6.f; S.Life = -1.f; S.Stuck = 1.4f;
            if (StoneSound) UGameplayStatics::PlaySoundAtLocation(this, StoneSound, S.Pos, .14f, 1.3f);
        }
        else S.Pos = To;
        if (bEnd && S.Stuck < 0) { S.Life = -1.f; M->SetHiddenInGame(true); if (Fire) Fire->SetBase(S.Flame, Perch.Seat - FVector(0, 0, 8000)); continue; }
        M->SetWorldLocationAndRotation(S.Pos, S.Vel.Rotation());
        if (Fire) Fire->SetBase(S.Flame, S.Pos - S.Vel.GetSafeNormal() * 10.f);
    }
}

void APhlegSpinedDevil::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, .05f);
    UWorld* World = GetWorld();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    const APhlegethosSetting* Stage = APhlegethosSetting::Get(World);
    const bool bFar = !Chuck || !Stage || Stage->IsHidden() || FVector::Dist(Chuck->GetActorLocation(), Perch.Seat) > 7000.f;
    if (bFar != bAsleep) { bAsleep = bFar; SetActorHiddenInGame(bFar); }
    if (bFar) return;
    Clock += Dt;
    const FVector C = Chuck->GetActorLocation();

    // The beat: the tail winds back for a moment, then whips over its shoulder, and the spine goes at the whip.
    const bool bThrowing = !APhlegFlameskull::bCalm && Stage->IsLive() && FVector::Dist(C, Perch.Seat) < 4500.f;
    if (bThrowing && FlickT < 0 && Clock >= NextThrow - .5f) { FlickT = 0; bReleased = false; TakeAim(Chuck); }
    float TailPitch = 4.f * FMath::Sin(Clock * 1.3f + Perch.Delay), TailYaw = 6.f * FMath::Sin(Clock * .8f + Perch.Delay * 2.f);
    if (FlickT >= 0)
    {
        FlickT += Dt;
        // wind back (0 - .5 s), whip (.5 - .68), settle (to 1.3)
        if (FlickT < .5f) TailPitch = 18.f * FMath::SmoothStep(0.f, 1.f, FlickT / .5f);
        else if (FlickT < .68f) TailPitch = FMath::Lerp(18.f, -78.f, FMath::SmoothStep(0.f, 1.f, (FlickT - .5f) / .18f));
        else TailPitch = FMath::Lerp(-78.f, 0.f, FMath::SmoothStep(0.f, 1.f, (FlickT - .68f) / .62f));
        if (!bReleased && FlickT >= .6f)
        {
            bReleased = true; Throw(); Flap = 1.f;
            if (HissSound && FMath::FRand() < .4f) UGameplayStatics::PlaySoundAtLocation(this, HissSound, GetCentre(), .1f, 1.6f);
        }
        if (FlickT > 1.3f) { FlickT = -1; NextThrow += Interval; while (NextThrow < Clock) NextThrow += Interval; }
    }
    else if (!bThrowing) NextThrow = Clock + Perch.Delay * .5f + .6f;
    Tail->SetRelativeRotation(FRotator(TailPitch, TailYaw, 0));

    // The wings stir; at a throw they beat open once.
    Flap = FMath::Max(0.f, Flap - Dt * 1.8f);
    const float Wing = 5.f * FMath::Sin(Clock * 2.1f + Perch.Delay) + 38.f * FMath::Sin(FMath::Min(1.f, (1.f - Flap) * 1.f) * PI) * (Flap > 0);
    WingL->SetRelativeRotation(FRotator(0, -Wing * .3f, Wing));
    WingR->SetRelativeRotation(FRotator(0, Wing * .3f, -Wing));
    // It breathes; its head follows him.
    Body->SetRelativeScale3D(FVector(1.f, 1.f, 1.f + .02f * FMath::Sin(Clock * 2.4f + Perch.Delay)));
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(C - Head->GetComponentLocation());
    const bool bWatch = Local.Size() < 1500.f;
    const float WantYaw = bWatch ? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -75.f, 75.f) : 20.f * FMath::Sin(Clock * .5f);
    const float WantPitch = bWatch ? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Z), static_cast<float>(Local.Size2D()))), -45.f, 25.f) : -10.f;
    LookYaw = FMath::FInterpTo(LookYaw, WantYaw, Dt, 4.f); LookPitch = FMath::FInterpTo(LookPitch, WantPitch, Dt, 4.f);
    Head->SetRelativeRotation(FRotator(LookPitch, LookYaw, 0));
    Light->SetIntensity(160.f * (.75f + .25f * FMath::Sin(Clock * 13.f + Perch.Delay)) + 500.f * Flap);

    TickSpines(Dt, Chuck);

    // Brushing against it hurts (2D: contact merely hurts; the scratch is what kills).
    TouchWait -= Dt;
    if (!APhlegFlameskull::bCalm && TouchWait <= 0 && FVector::Dist(C, GetCentre()) < 42.f && Chuck->TakeBite(GetCentre(), 1, true, false))
    {
        TouchWait = 1.f; ++Hits;
        if (BurnSound) UGameplayStatics::PlaySoundAtLocation(this, BurnSound, C, .28f, 1.4f);
    }
}

bool APhlegSpinedDevil::TryScratch(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach)
{
    if (!Chuck || bAsleep) return false;
    const FVector Cn = GetCentre();
    const FVector To = Cn - Location;
    const float Feet = static_cast<float>(Location.Z) - Stand;
    // (on a low column it sits above his head: he reaches up to it; its body is about 26 cm round its middle)
    if (static_cast<float>(To.Size2D()) - 26.f > Reach || Cn.Z < Feet - 10.f || Cn.Z > Feet + 105.f) return false;
    if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) < .3f && To.Size2D() > 20.f) return false;
    // (2D: scratching one immediately defeats Chuck.) Its spines through him: all of his sanity, through a dodge.
    ++Scratched;
    if (HissSound) UGameplayStatics::PlaySoundAtLocation(this, HissSound, Cn, .45f, 1.2f);
    if (BurnSound) UGameplayStatics::PlaySoundAtLocation(this, BurnSound, Location, .5f, 1.f);
    Flap = 1.f;
    Chuck->TakeBite(Cn, Chuck->GetSanity(), false, true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_DEVIL_SCRATCHED sanity=%d"), Chuck->GetSanity());
    return true;
}

bool PhlegethosScratchDevils(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach)
{
    bool bAny = false;
    for (const auto& D : GDevils) if (D.IsValid() && D->TryScratch(Chuck, Location, Ahead, Reach)) { bAny = true; break; }
    return bAny;
}
