#include "TempleSanctumBattle.h"
#include "ChuckCharacter.h"
#include "DockGameMode.h"
#include "ChultTempleWind.h"
#include "ChultTempleInterior.h"
#include "HeroClipData.h"
#include "TempleHero.h"
#include "TempleBeholder.h"
#include "TempleSeal.h"
#include "TempleSanctumScene.h"
#include "ChultTempleRubble.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

namespace
{
    using namespace TempleWind;
    TWeakObjectPtr<ATempleSanctumBattle> TheBattle;

    constexpr int32 ChipPool = 260, DustPool = 150, FirePool = 140, ArrowPool = 14, CorePool = 3;
    constexpr int32 WantUp = 3;               // skeletons on their feet at once while he is in the hall
    constexpr float RecycleAfter = 14.f;      // the fallen lie this long before going back to the bones

    const TCHAR* HeroName(int32 R) { return R == 0 ? TEXT("Fighter") : R == 1 ? TEXT("Wizard") : TEXT("Ranger"); }
    const TArray<const TCHAR*>& HeroClipNames(int32 R)
    {
        static const TArray<const TCHAR*> F = {TEXT("Guard"), TEXT("CutDown"), TEXT("CutFlat"), TEXT("CutRise"), TEXT("Overhead"), TEXT("Bash"), TEXT("BlockHit"), TEXT("HitLight"), TEXT("HitHeavy"), TEXT("Advance"), TEXT("Retreat")};
        static const TArray<const TCHAR*> W = {TEXT("Idle"), TEXT("Channel"), TEXT("Cast"), TEXT("HitLight"), TEXT("HitHeavy"), TEXT("Seal"), TEXT("SealStrain")};
        static const TArray<const TCHAR*> Ra = {TEXT("Idle"), TEXT("Shoot"), TEXT("HitLight"), TEXT("HitHeavy")};
        return R == 0 ? F : R == 1 ? W : Ra;
    }
    struct FSoundSet { const TCHAR* Stem; int32 Count; };
    const FSoundSet SoundSets[] = {{TEXT("SFX_HeroSwordWhoosh"), 4}, {TEXT("SFX_HeroSwordHitBone"), 4}, {TEXT("SFX_HeroShieldBlock"), 3}, {TEXT("SFX_HeroArmourHit"), 3},
        {TEXT("SFX_HeroArmourStep"), 4}, {TEXT("SFX_HeroShieldBash"), 2}, {TEXT("SFX_HeroBowDraw"), 1}, {TEXT("SFX_HeroBowRelease"), 3}, {TEXT("SFX_HeroArrowFlight"), 2},
        {TEXT("SFX_HeroArrowHit"), 3}, {TEXT("SFX_HeroBoltCast"), 2}, {TEXT("SFX_HeroBoltImpact"), 2}, {TEXT("SFX_HeroSkullBounce"), 3}, {TEXT("SFX_SkeletonHit"), 3},
        {TEXT("SFX_BeholderRayCharge"), 2}, {TEXT("SFX_BeholderRayDisintegrate"), 1}, {TEXT("SFX_BeholderRayEnervate"), 1}, {TEXT("SFX_BeholderRayTelekinetic"), 1},
        {TEXT("SFX_BeholderRayParalyze"), 1}, {TEXT("SFX_BeholderRayFear"), 1}, {TEXT("SFX_BeholderRayImpact"), 3}, {TEXT("SFX_BeholderDisintegrate"), 1},
        {TEXT("SFX_BeholderConeCharge"), 1}, {TEXT("SFX_BeholderConeBlast"), 1}, {TEXT("SFX_BeholderRoar"), 2}, {TEXT("SFX_BeholderSnap"), 2},
        {TEXT("SFX_BeholderGrowl"), 3}, {TEXT("SFX_BeholderHurt"), 2}, {TEXT("SFX_SealSurge"), 2}, {TEXT("SFX_SealBreak"), 1}};
    const TCHAR* const SealWebPath = TEXT("/Game/Art/ChultTemple/BeholderFX/M_TempleSealWeb.M_TempleSealWeb");
    const TCHAR* const BeholderFxPaths[] = {TEXT("/Game/Art/ChultTemple/BeholderFX/M_BeholderBeam.M_BeholderBeam"), TEXT("/Game/Art/ChultTemple/BeholderFX/M_BeholderWave.M_BeholderWave"),
        TEXT("/Game/Art/ChultTemple/BeholderFX/M_BeholderSpark.M_BeholderSpark"), TEXT("/Game/Art/ChultTemple/BeholderFX/M_BeholderFlare.M_BeholderFlare")};

    bool Suppressed()
    {
        const TCHAR* Cmd = FCommandLine::Get();
        if (FParse::Param(Cmd, TEXT("ChuckSanctumTest")) || FParse::Param(Cmd, TEXT("ChuckSanctumCapture"))) return false;
        TArray<FString> Words; FString(Cmd).ParseIntoArrayWS(Words);
        for (const FString& Word : Words)
            if (Word.StartsWith(TEXT("-Chuck")) && (Word.Contains(TEXT("Test")) || Word.Contains(TEXT("Capture")) || Word.Contains(TEXT("Review")))) return true;
        return false;
    }
    FVector W(float S, float T, float Z) { return ChultTempleInteriorWorldOf(FVector(S, T, Z)); }
    float YawToward(const FVector& From, const FVector& To) { return static_cast<float>((To - From).GetSafeNormal2D().Rotation().Yaw); }

    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Zv = Eye - P; if (!Zv.Normalize()) Zv = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Zv.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Zv).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Zv, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        return FTransform(FMatrix(X0 * C + Y0 * S, Y0 * C - X0 * S, Zv, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
    const FTransform Hidden(FQuat::Identity, FVector(0, 0, -1e6), FVector(.001f));
}

ATempleSanctumBattle* ATempleSanctumBattle::Get(const UWorld* World)
{
    ATempleSanctumBattle* B = TheBattle.Get();
    return B && B->GetWorld() == World ? B : nullptr;
}

ATempleSanctumBattle::ATempleSanctumBattle()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // The fight's own assets are streamed in when he nears the sanctum (Tick), not held from startup: held by
    // the class, all 200 MB of them loaded in every run of the game (DefaultGame.ini cooks
    // /Game/Characters/Heroes; the sounds and the skull are in /Game/Art, always cooked). Only the engine
    // shapes and the skeletons' bone material are referenced here (tiny, and the skeletons hold the latter).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneF(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoneF(TEXT("/Game/Characters/Skeleton/MI_SkeletonBone.MI_SkeletonBone"));
    Cube = CubeF.Object; Plane = PlaneF.Object; Sphere = SphereF.Object; BoneMat = BoneF.Object;
    auto MakeFx = [this](const TCHAR* Name, int32 Floats)
    {
        auto* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        C->SetupAttachment(RootComponent);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->SetLightingChannels(false, true, false);
        C->NumCustomDataFloats = Floats;
        C->SetMobility(EComponentMobility::Movable);
        return C;
    };
    ChipFx = MakeFx(TEXT("Chips"), 0); ChipFx->SetCastShadow(true);
    DustFx = MakeFx(TEXT("Dust"), 3);
    ArrowFx = MakeFx(TEXT("Arrows"), 0); ArrowFx->SetCastShadow(true);
    BoltFireFx = MakeFx(TEXT("BoltFire"), 3);
    BoltCoreFx = MakeFx(TEXT("BoltCore"), 0);
    Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
    Flash->SetupAttachment(RootComponent);
    Flash->SetIntensity(0.f); Flash->SetAttenuationRadius(700.f); Flash->SetCastShadows(false);
    Flash->SetLightingChannels(false, true, false);
}

void ATempleSanctumBattle::BeginPlay()
{
    Super::BeginPlay();
    TheBattle = this;
    Random.Initialize(20261022);
    Floor = static_cast<float>(W(HallMid, DaisT1 + 700, HZ).Z);
    Beholder = W(HallMid, DaisT1 - 150, HZ + DaisH + 260);
    // The rest waits for its assets (Tick: StartLoading, then FinishLoading).
}

TArray<FSoftObjectPath> ATempleSanctumBattle::AssetPaths() const
{
    TArray<FSoftObjectPath> P;
    for (int32 R = 0; R < 3; ++R)
    {
        const FString Name = HeroName(R);
        P.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/Heroes/%s/SK_Hero%s.SK_Hero%s"), *Name, *Name, *Name)));
        for (const TCHAR* Clip : HeroClipNames(R))
            P.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/Heroes/%s/Anim/AS_Hero%s_%s.AS_Hero%s_%s"), *Name, *Name, Clip, *Name, Clip)));
        // Their strand hair (SourceAssets/Heroes/manifest.json `grooms`), streamed with the rest so Setup's attach doesn't load it in a hitch.
        static const TCHAR* const Grooms[3][3] = { {TEXT("Beard"), TEXT("BeardJaw"), nullptr}, {TEXT("Hair"), TEXT("Beard"), TEXT("BeardJaw")}, {TEXT("Hair"), nullptr, nullptr} };
        for (const TCHAR* G : Grooms[FMath::Clamp(R, 0, 2)])
            if (G) P.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/Heroes/%s/GR_%s_%s.GR_%s_%s"), *Name, *Name, G, *Name, G)));
    }
    P.Add(FSoftObjectPath(TEXT("/Game/Characters/Beholder/SK_Beholder.SK_Beholder")));
    for (const TCHAR* A : {TEXT("/Game/Characters/Heroes/Props/SM_HeroArrow.SM_HeroArrow"), TEXT("/Game/Art/ChultTemple/SM_TempleSkull.SM_TempleSkull"),
                           TEXT("/Game/Characters/Heroes/FX/M_HeroDust.M_HeroDust"), TEXT("/Game/Characters/Heroes/FX/M_HeroBoltFire.M_HeroBoltFire"),
                           TEXT("/Game/Characters/Heroes/FX/M_HeroGlow.M_HeroGlow"), TEXT("/Game/Characters/Heroes/FX/M_HeroTrail.M_HeroTrail"),
                           TEXT("/Game/Art/Audio/SFX/SFX_HeroChannel_00.SFX_HeroChannel_00")})
        P.Add(FSoftObjectPath(A));
    for (const FSoundSet& Set : SoundSets)
        for (int32 I = 0; I < Set.Count; ++I)
            P.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Set.Stem, I, Set.Stem, I)));
    for (const TCHAR* Fx : BeholderFxPaths) P.Add(FSoftObjectPath(Fx));
    P.Add(FSoftObjectPath(SealWebPath));
    TalkAssetPaths(P);
    return P;
}

void ATempleSanctumBattle::StartLoading()
{
    if (bLoading || bReady) return;
    bLoading = true;
    LoadStart = FPlatformTime::Seconds();
    TWeakObjectPtr<ATempleSanctumBattle> Self(this);
    Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetPaths(), FStreamableDelegate::CreateLambda([Self]() { if (Self.IsValid()) Self->FinishLoading(); }));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_LOADING assets=%d"), AssetPaths().Num());
}

void ATempleSanctumBattle::FinishLoading()
{
    if (bReady) return;
    auto Find = [](const FString& Path) { return FSoftObjectPath(Path).ResolveObject(); };
    for (int32 R = 0; R < 3; ++R)
    {
        const FString Name = HeroName(R);
        HeroMeshes.Add(Cast<USkeletalMesh>(Find(FString::Printf(TEXT("/Game/Characters/Heroes/%s/SK_Hero%s.SK_Hero%s"), *Name, *Name, *Name))));
        TMap<FName, UAnimSequence*> Map;
        for (const TCHAR* Clip : HeroClipNames(R))
        {
            auto* A = Cast<UAnimSequence>(Find(FString::Printf(TEXT("/Game/Characters/Heroes/%s/Anim/AS_Hero%s_%s.AS_Hero%s_%s"), *Name, *Name, Clip, *Name, Clip)));
            Map.Add(Clip, A); HeroClipList.Add(A);
        }
        HeroClips.Add(Map);
    }
    BeholderMesh = Cast<USkeletalMesh>(Find(TEXT("/Game/Characters/Beholder/SK_Beholder.SK_Beholder")));
    ArrowMesh = Cast<UStaticMesh>(Find(TEXT("/Game/Characters/Heroes/Props/SM_HeroArrow.SM_HeroArrow")));
    SkullMesh = Cast<UStaticMesh>(Find(TEXT("/Game/Art/ChultTemple/SM_TempleSkull.SM_TempleSkull")));
    DustMat = Cast<UMaterialInterface>(Find(TEXT("/Game/Characters/Heroes/FX/M_HeroDust.M_HeroDust")));
    BoltFireMat = Cast<UMaterialInterface>(Find(TEXT("/Game/Characters/Heroes/FX/M_HeroBoltFire.M_HeroBoltFire")));
    GlowMat = Cast<UMaterialInterface>(Find(TEXT("/Game/Characters/Heroes/FX/M_HeroGlow.M_HeroGlow")));
    TrailMat = Cast<UMaterialInterface>(Find(TEXT("/Game/Characters/Heroes/FX/M_HeroTrail.M_HeroTrail")));
    Hum = Cast<USoundBase>(Find(TEXT("/Game/Art/Audio/SFX/SFX_HeroChannel_00.SFX_HeroChannel_00")));
    for (const TCHAR* Fx : BeholderFxPaths) BeholderFx.Add(Cast<UMaterialInterface>(Find(Fx)));
    SealWebMat = Cast<UMaterialInterface>(Find(SealWebPath));
    for (const FSoundSet& Set : SoundSets)
    {
        TArray<USoundBase*> List;
        for (int32 I = 0; I < Set.Count; ++I)
            if (auto* S = Cast<USoundBase>(Find(FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Set.Stem, I, Set.Stem, I)))) { List.Add(S); SoundList.Add(S); }
        Sounds.Add(List);
    }
    ChipFx->SetStaticMesh(Cube); if (BoneMat) ChipFx->SetMaterial(0, BoneMat);
    DustFx->SetStaticMesh(Plane); if (DustMat) DustFx->SetMaterial(0, DustMat);
    ArrowFx->SetStaticMesh(ArrowMesh);
    BoltFireFx->SetStaticMesh(Plane); if (BoltFireMat) BoltFireFx->SetMaterial(0, BoltFireMat);
    BoltCoreFx->SetStaticMesh(Sphere); if (GlowMat) BoltCoreFx->SetMaterial(0, GlowMat);
    auto Fill = [](UInstancedStaticMeshComponent* C, int32 N)
    {
        TArray<FTransform> T; T.Init(Hidden, N);
        C->AddInstances(T, false, true);
    };
    Fill(ChipFx, ChipPool); Fill(DustFx, DustPool); Fill(ArrowFx, ArrowPool); Fill(BoltFireFx, FirePool); Fill(BoltCoreFx, CorePool);
    for (int32 K = 0; K < CorePool; ++K)
    {
        auto* L = NewObject<UPointLightComponent>(this);
        L->SetupAttachment(RootComponent); L->SetLightColor(FLinearColor(.3f, 1.f, .8f)); L->SetIntensity(0.f);
        L->SetAttenuationRadius(600.f); L->SetCastShadows(false); L->SetLightingChannels(false, true, false);
        L->RegisterComponent(); BoltLights.Add(L);
    }
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 3600.f;   // heard across the hall (54 m) and from the gallery
    FinishTalkLoading();
    bReady = true; bLoading = false;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_LOADED seconds=%.2f"), FPlatformTime::Seconds() - LoadStart);
    SpawnCast();
}

void ATempleSanctumBattle::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TheBattle.Get() == this) TheBattle = nullptr;
    Super::EndPlay(Reason);
}

void ATempleSanctumBattle::SpawnCast()
{
    UWorld* World = GetWorld();
    // The heroes on the floor before the dais, facing where the beholder will hang: the fighter in front,
    // the wizard behind on his left and the ranger behind on his right (the 2D tableau's arc).
    struct FPlace { float S, T; };
    // (Relative to the dais' front since the hall was enlarged, 2026-10-23.)
    const FPlace Places[] = {{HallMid, DaisT1 + 550}, {HallMid - 380, DaisT1 + 970}, {HallMid + 390, DaisT1 + 950}};
    for (int32 R = 0; R < 3; ++R)
    {
        const FVector Feet = W(Places[R].S, Places[R].T, HZ);
        const float Yaw = YawToward(Feet, Beholder);
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* H = World->SpawnActor<ATempleHero>(Feet + FVector(0, 0, ATempleHero::HalfHeight + 1), FRotator(0, Yaw, 0), P);
        if (!H) continue;
        H->Setup(static_cast<ATempleHero::ERole>(R), this, HeroMeshes[R], HeroClips[R]);
        H->SetHome(Feet, Yaw);
        Heroes.Add(H);
    }
    // The wizard's seal over the sea tear to his left (2026-10-25): he works it, facing the tear, when nothing comes at him.
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto* Seal = World->SpawnActor<ATempleSeal>(FVector::ZeroVector, FRotator::ZeroRotator, P))
        {
            Seal->Setup(this, SealWebMat, BeholderFx[0], BeholderFx[2], Plane);
            TheSeal = Seal;
            if (ATempleHero* Wz = GetHero(1)) { const FVector Feet = W(Places[1].S, Places[1].T, HZ); Wz->SetHome(Feet, YawToward(Feet, Seal->GetCentre())); Wz->SetActorRotation(FRotator(0, YawToward(Feet, Seal->GetCentre()), 0)); Wz->Rest(); }
        }
    }
    // The beholder over the front of the dais, facing them (2026-10-23).
    if (BeholderMesh)
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const float Yaw = Heroes.Num() && Heroes[0].IsValid() ? (Heroes[0]->GetActorLocation() - Beholder).Rotation().Yaw : 0.f;
        if (auto* Bh = World->SpawnActor<ATempleBeholder>(Beholder, FRotator(0, Yaw, 0), P))
        {
            Bh->Setup(BeholderMesh, this, Beholder); TheBeholder = Bh;
            Bh->SetFx(BeholderFx[0], BeholderFx[1], BeholderFx[2], BeholderFx[3], Plane);
        }
    }
    // The cutscenes (2026-10-26): the entrance and the Fireball.
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto* Scene = World->SpawnActor<ATempleSanctumScene>(Beholder, FRotator::ZeroRotator, P)) { Scene->Setup(this); TheScene = Scene; }
    }
    // The reserve: skeletons lying among the bones round the dais end of the hall (clear of the pillars,
    // idols, braziers and the heroes' ground).
    // In the enlarged hall (2026-10-23): in the nave before the dais and in the aisles, clear of the piers,
    // the great fire bowls and the heroes' ground.
    const FPlace Posts[] = {{HallMid - 250, DaisT1 + 200}, {HallMid + 250, DaisT1 + 200}, {HallMid - 650, DaisT1 + 560}, {HallMid + 650, DaisT1 + 650},
                            {HallMid - 200, DaisT1 + 1550}, {HallMid + 200, DaisT1 + 1550}, {HallMid - 1180, DaisT1 + 330}, {HallMid + 1180, DaisT1 + 330},
                            {HallMid - 1180, DaisT1 + 1150}, {HallMid + 1180, DaisT1 + 1150}, {HallMid - 650, DaisT1 + 1500}, {HallMid + 650, DaisT1 + 1500}};
    // (Since 2026-10-24 short of the pool before the gallery: it was to GalleryT - 300.)
    const FBox2D Leash(FVector2D(HS0 + 60, DaisT1 + 100), FVector2D(HS1 - 60, PoolT - PoolJag - 300));
    for (const FPlace& Pl : Posts)
    {
        FTempleSkeletonPost Post;
        Post.Feet = W(Pl.S, Pl.T, HZ);
        Post.Yaw = YawToward(Post.Feet, W(HallMid, DaisT1 + 700, HZ)) + Random.FRandRange(-40.f, 40.f);
        Post.bDormant = true; Post.Leash = Leash; Post.Space = TEXT("sanctum");
        Reserve.Add(Post);
        if (ATempleSkeleton* S = ATempleSkeleton::SpawnAt(World, Post))
        {
            S->SetSanctum(this);
            S->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Pool.Add(S);
        }
    }
    // Every hero's ground and every post stands a metre clear of the broken floor (2026-10-24).
    int32 NearHoles = 0;
    for (const FPlace& Pl : Places) NearHoles += ChultTempleSanctumHoleAt(FVector2D(Pl.S, Pl.T), 100) != 0;
    for (const FPlace& Pl : Posts) NearHoles += ChultTempleSanctumHoleAt(FVector2D(Pl.S, Pl.T), 100) != 0;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_BATTLE heroes=%d beholder=%d skeletons=%d meshes=%d clips=%d sounds=%d arrow=%d skull=%d fx=%d near_holes=%d beholder_fx=%d seal=%d"),
        Heroes.Num(), TheBeholder.IsValid(), Pool.Num(), HeroMeshes.FilterByPredicate([](USkeletalMesh* M) { return M != nullptr; }).Num(),
        HeroClipList.FilterByPredicate([](UAnimSequence* A) { return A != nullptr; }).Num(), SoundList.Num(), ArrowMesh != nullptr, SkullMesh != nullptr,
        (DustMat != nullptr) + (BoltFireMat != nullptr) + (GlowMat != nullptr) + (TrailMat != nullptr) + (BoneMat != nullptr), NearHoles,
        BeholderFx.FilterByPredicate([](UMaterialInterface* M) { return M != nullptr; }).Num(), TheSeal.IsValid() && SealWebMat ? 1 : 0);
}

int32 ATempleSanctumBattle::GetUp() const
{
    int32 N = 0;
    for (const TWeakObjectPtr<ATempleSkeleton>& S : Pool) N += S.IsValid() && S->IsUp();
    return N;
}

ATempleSkeleton* ATempleSanctumBattle::WakeOne(ATempleHero* For, const FVector* At)
{
    TArray<ATempleSkeleton*> Lying;
    for (const TWeakObjectPtr<ATempleSkeleton>& S : Pool) if (S.IsValid() && S->GetState() == ATempleSkeleton::EState::Dormant) Lying.Add(S.Get());
    if (!Lying.Num()) return nullptr;
    ATempleSkeleton* Pick = Lying[Random.RandRange(0, Lying.Num() - 1)];
    if (At)
    {
        float Best = 1e9f;
        for (ATempleSkeleton* S : Lying) { const float D = static_cast<float>(FVector::Dist2D(S->GetActorLocation(), *At)); if (D < Best) { Best = D; Pick = S; } }
    }
    if (!For)
    {
        // Who it goes for: mostly the fighter (two at him at most), else the ranger, else the wizard.
        int32 On[3] = {0, 0, 0};
        for (const TWeakObjectPtr<ATempleSkeleton>& S : Pool)
            if (S.IsValid() && S->IsUp() && S->GetFoe()) for (int32 R = 0; R < 3; ++R) On[R] += S->GetFoe() == GetHero(R);
        const int32 R = On[0] < 2 && Random.FRand() < .8f ? 0 : On[2] < 1 && Random.FRand() < .7f ? 2 : On[1] < 1 ? 1 : 0;
        For = GetHero(R);
    }
    Pick->SetFoe(For);
    Pick->Wake();
    ++WakeCount;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_WAKE foe=%s up=%d"), For ? For->GetRoleName() : TEXT("none"), GetUp());
    return Pick;
}

void ATempleSanctumBattle::Recycle()
{
    // The fallen go back to the bones where Chuck is not looking, on a free reserve post.
    for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Pool)
    {
        ATempleSkeleton* S = Wk.Get();
        if (!S || S->GetDeadTime() < RecycleAfter || S->WasRecentlyRendered(.5f)) continue;
        TArray<int32> Free;
        for (int32 I = 0; I < Reserve.Num(); ++I)
        {
            bool bTaken = false;
            for (const TWeakObjectPtr<ATempleSkeleton>& O : Pool)
                if (O.IsValid() && O.Get() != S && FVector::Dist2D(O->GetActorLocation(), Reserve[I].Feet) < 90.f) { bTaken = true; break; }
            if (!bTaken) Free.Add(I);
        }
        if (!Free.Num()) continue;
        for (int32 K = Skulls.Num() - 1; K >= 0; --K) if (Skulls[K].Owner == Wk) Skulls.RemoveAt(K);
        S->ResetToReserve(Reserve[Free[Random.RandRange(0, Free.Num() - 1)]]);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_RECYCLE"));
    }
}

void ATempleSanctumBattle::Sound(ESound Kind, const FVector& At, float Volume, float Pitch)
{
    if (bHushed) return;   // (from the rubble map the hall is not heard)
    const int32 K = static_cast<int32>(Kind);
    if (!Sounds.IsValidIndex(K) || !Sounds[K].Num()) return;
    UGameplayStatics::PlaySoundAtLocation(this, Sounds[K][Random.RandRange(0, Sounds[K].Num() - 1)], At, Volume, Pitch * Random.FRandRange(.95f, 1.05f), 0.f, Attenuation);
    ++SoundsPlayed;
}

void ATempleSanctumBattle::Splinters(const FVector& At, const FVector& Dir, int32 Count, float Speed)
{
    const FVector D = Dir.GetSafeNormal();
    for (int32 I = 0; I < Count; ++I)
    {
        if (Chips.Num() >= ChipPool) Chips.RemoveAt(0);
        FChip C;
        C.Pos = At + FMath::VRand() * 4.f;
        C.Vel = (D * Random.FRandRange(.3f, 1.f) + FMath::VRand() * .8f + FVector(0, 0, .45f)).GetSafeNormal() * Speed * Random.FRandRange(.35f, 1.f);
        C.Rot = FQuat(FMath::VRand(), Random.FRandRange(0.f, 6.28f));
        C.Spin = FMath::VRand() * Random.FRandRange(8.f, 25.f);
        C.Size = Random.FRandRange(.6f, 2.4f) * (Random.FRand() < .12f ? 2.2f : 1.f);
        C.Age = 0; C.Life = Random.FRandRange(5.f, 9.f); C.bRest = false;
        Chips.Add(C);
    }
}

void ATempleSanctumBattle::Dust(const FVector& At, const FVector& Dir, int32 Count, float Size)
{
    for (int32 I = 0; I < Count; ++I)
    {
        if (Puffs.Num() >= DustPool + FirePool) break;
        FPuff P;
        P.Pos = At + FMath::VRand() * 6.f;
        P.Vel = Dir.GetSafeNormal() * Random.FRandRange(40.f, 140.f) + FMath::VRand() * 40.f;
        P.Size = Size * Random.FRandRange(.6f, 1.2f); P.Grow = Random.FRandRange(25.f, 60.f);
        P.Age = 0; P.Life = Random.FRandRange(1.2f, 2.4f); P.Frame = Random.FRand() * 64.f; P.Rate = Random.FRandRange(12.f, 20.f);
        P.Spin = Random.FRandRange(0.f, 6.28f); P.Alpha = Random.FRandRange(.25f, .45f); P.bFire = false;
        Puffs.Add(P);
    }
}

void ATempleSanctumBattle::FirePuffs(const FVector& At, const FVector& Vel, int32 Count, float Size)
{
    for (int32 I = 0; I < Count; ++I)
    {
        FPuff P;
        P.Pos = At + FMath::VRand() * 3.f;
        P.Vel = Vel + FMath::VRand() * 60.f;
        P.Size = Size * Random.FRandRange(.7f, 1.2f); P.Grow = Random.FRandRange(10.f, 40.f);
        P.Age = 0; P.Life = Random.FRandRange(.18f, .35f); P.Frame = Random.FRand() * 64.f; P.Rate = Random.FRandRange(30.f, 50.f);
        P.Spin = Random.FRandRange(0.f, 6.28f); P.Alpha = 1.f; P.bFire = true;
        Puffs.Add(P);
    }
}

void ATempleSanctumBattle::OnBladeHit(ATempleSkeleton* Victim, EBlow Kind, const FVector& At, const FVector& Vel)
{
    const FVector Dir = Vel.GetSafeNormal();
    // The sound of the blow first (it has to be on the frame of contact), then what it does to the bone.
    if (Kind == EBlow::Bash) Sound(ESound::ShieldBash, At, 1.f);
    else Sound(ESound::BladeBone, At, Kind == EBlow::Cleave ? 1.f : .95f, Kind == EBlow::Cleave ? .88f : 1.f);
    Splinters(At, Dir, Kind == EBlow::Cleave ? 34 : Kind == EBlow::Bash ? 10 : 22, Kind == EBlow::Cleave ? 520.f : 420.f);
    Dust(At, Dir, Kind == EBlow::Bash ? 3 : 5, 22.f);
    if (Kind == EBlow::Decapitate && Victim && SkullMesh)
    {
        ++Decapitations;
        // The skull off its spine, the blade's way and up, tumbling.
        FSkullFlight F;
        F.Owner = Victim;
        const FTransform T = Victim->SkullTransform();
        F.Pos = T.GetLocation() + FVector(0, 0, 6);
        F.Vel = Dir.GetSafeNormal2D() * Random.FRandRange(260.f, 360.f) + FVector(0, 0, Random.FRandRange(260.f, 340.f));
        F.Rot = T.GetRotation(); F.Spin = (FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal() * Random.FRandRange(9.f, 14.f) + FMath::VRand() * 3.f);
        F.Age = 0; F.bRest = false; F.Bounces = 0;
        Skulls.Add(F);
    }
    if (Kind != EBlow::Bash) ++Fallen;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_BLADE_HIT kind=%d chips=%d skulls=%d"), static_cast<int32>(Kind), Chips.Num(), Skulls.Num());
}

void ATempleSanctumBattle::OnBlocked(const FVector& At, const FVector& Normal)
{
    Sound(ESound::ShieldBlock, At, 1.f);
    Dust(At, Normal, 2, 14.f);
    Splinters(At, Normal, 4, 160.f);
}

void ATempleSanctumBattle::FireArrow(const FVector& From, ATempleSkeleton* At, const FVector& Fallback)
{
    FArrowFlight A;
    A.Pos = From; A.Target = At; A.Age = 0; A.bSpent = false;
    FVector Aim = Fallback;
    if (At)
    {
        // At the chest, led by where it is going in the arrow's time of flight (about 50 m/s).
        const FVector Chest = At->GetMesh()->GetBoneLocation(TEXT("spine_03"));
        const float Tf = static_cast<float>(FVector::Dist(From, Chest)) / 5000.f;
        Aim = Chest + At->GetVelocity() * Tf + FVector(0, 0, .5f * 980.f * Tf * Tf);
    }
    A.Vel = (Aim - From).GetSafeNormal() * 5000.f;
    if (Arrows.Num() >= ArrowPool) Arrows.RemoveAt(0);
    Arrows.Add(A);
    Sound(ESound::ArrowFlight, From + A.Vel * .08f, .6f);
}

void ATempleSanctumBattle::FireBolt(const FVector& From, ATempleSkeleton* At)
{
    if (Bolts.Num() >= CorePool) Bolts.RemoveAt(0);
    FBoltFlight B;
    B.Pos = From; B.Target = At; B.Age = 0;
    // At a skeleton, or (none given) at the beholder itself.
    const FVector Aim = At ? At->GetMesh()->GetBoneLocation(TEXT("spine_03")) : TheBeholder.IsValid() ? TheBeholder->GetCentre() : Beholder;
    B.Vel = (Aim - From).GetSafeNormal() * 1500.f + FVector(0, 0, 150.f);
    Bolts.Add(B);
    Flash->SetWorldLocation(From); Flash->SetLightColor(FLinearColor(.35f, 1.f, .8f)); FlashTime = .18f;
}

void ATempleSanctumBattle::ClearFlying()
{
    Chips.Reset(); Puffs.Reset(); Arrows.Reset(); Bolts.Reset(); Skulls.Reset();
}

void ATempleSanctumBattle::TickFlying(float Dt, const FVector& Eye)
{
    const float G = 980.f;
    // Splinters: thrown, tumbling, bouncing once or twice on the flags, then lying there.
    for (FChip& C : Chips)
    {
        C.Age += Dt;
        if (C.bRest) continue;
        C.Vel.Z -= G * Dt; C.Pos += C.Vel * Dt;
        C.Rot = FQuat(C.Spin.GetSafeNormal(), static_cast<float>(C.Spin.Size()) * Dt) * C.Rot;
        if (C.Pos.Z < Floor + C.Size * .3f)
        {
            C.Pos.Z = Floor + C.Size * .3f;
            if (FMath::Abs(C.Vel.Z) < 60.f) { C.bRest = true; C.Rot = FQuat(FVector::UpVector, Random.FRandRange(0.f, 6.28f)); continue; }
            C.Vel.Z *= -.28f; C.Vel.X *= .5f; C.Vel.Y *= .5f; C.Spin *= .5f;
        }
    }
    Chips.RemoveAll([](const FChip& C) { return C.Age > C.Life; });
    for (FPuff& P : Puffs)
    {
        P.Age += Dt; P.Frame += P.Rate * Dt; P.Size += P.Grow * Dt;
        P.Pos += P.Vel * Dt; P.Vel *= FMath::Exp(-(P.bFire ? 5.f : 2.6f) * Dt);
        if (!P.bFire) P.Vel.Z += 12.f * Dt;   // dust drifts, settling slowly
    }
    Puffs.RemoveAll([](const FPuff& P) { return P.Age > P.Life; });
    // Arrows: ballistic; one that meets a skeleton's bones stands in them.
    for (FArrowFlight& A : Arrows)
    {
        A.Age += Dt;
        if (A.bSpent) continue;
        const FVector From = A.Pos;
        A.Vel.Z -= G * Dt; A.Pos += A.Vel * Dt;
        const FVector Tip0 = From + A.Vel.GetSafeNormal() * 76.f, Tip1 = A.Pos + A.Vel.GetSafeNormal() * 76.f;
        if (ATempleBeholder* Bh = TheBeholder.Get())
        {
            // Its blast stops arrows in the air (they drop); its hide takes those that reach it.
            if (Bh->IsConeBlasting() && Bh->InCone(A.Pos)) { A.Vel *= .08f; }
            const FVector Q = FMath::ClosestPointOnSegment(Bh->GetCentre(), Tip0, Tip1);
            if (FVector::Dist(Q, Bh->GetCentre()) < ATempleBeholder::BodyRadius)
            {
                A.bSpent = true; ++BeholderArrows;
                const FVector Hide = Bh->GetCentre() + (Q - Bh->GetCentre()).GetSafeNormal() * ATempleBeholder::BodyRadius * .9f;
                Sound(ESound::BodyHit, Hide, .9f, .75f);
                Bh->StickArrowAt(Hide, A.Vel);
                Bh->TakeHit(Hide, A.Vel, .7f);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_ARROW_BEHOLDER"));
                continue;
            }
        }
        for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Pool)
        {
            ATempleSkeleton* S = Wk.Get();
            if (!S || !S->IsUp()) continue;
            USkeletalMeshComponent* M = S->GetMesh();
            FVector P1, P2;
            FMath::SegmentDistToSegmentSafe(Tip0, Tip1, M->GetBoneLocation(TEXT("pelvis")), M->GetBoneLocation(TEXT("head")), P1, P2);
            if (FVector::Dist(P1, P2) > 17.f) continue;
            A.bSpent = true;
            Sound(ESound::ArrowHit, P1, .95f);
            Splinters(P1, A.Vel, 8, 240.f);
            S->HitStop(.05f);
            S->StickArrow(ArrowMesh, P1, A.Vel);
            const bool bFell = S->TakeHeroBlow(EBlow::Arrow, From, P1);
            if (bFell) ++Fallen;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_ARROW_HIT fell=%d arrows_in=%d"), bFell ? 1 : 0, S->GetArrowsIn());
            break;
        }
        if (!A.bSpent && A.Pos.Z < Floor) { A.bSpent = true; A.Age = 1e3f; }
    }
    Arrows.RemoveAll([](const FArrowFlight& A) { return A.bSpent || A.Age > 2.f; });
    // Bolts: a sea-green fire, turning onto its mark; a burst where it strikes.
    for (int32 I = Bolts.Num() - 1; I >= 0; --I)
    {
        FBoltFlight& B = Bolts[I];
        B.Age += Dt;
        ATempleSkeleton* S = B.Target.Get();
        ATempleBeholder* Bh = TheBeholder.Get();
        const bool bAtBeholder = !B.Target.IsValid() && Bh;
        const FVector Aim = S && S->IsUp() ? S->GetMesh()->GetBoneLocation(TEXT("spine_03")) : bAtBeholder ? Bh->GetCentre() : B.Pos + B.Vel;
        const FVector Want = (Aim - B.Pos).GetSafeNormal() * 1500.f;
        B.Vel = FMath::Lerp(B.Vel, Want, FMath::Min(1.f, 5.f * Dt));
        B.Pos += B.Vel * Dt;
        FirePuffs(B.Pos, -B.Vel * .15f, 3, 18.f);
        if (Bh && Bh->IsConeBlasting() && Bh->InCone(B.Pos))
        {
            // The antimagic cone: the spell is simply gone, a gutter of sparks where it was.
            ++Fizzled;
            FirePuffs(B.Pos, FVector::ZeroVector, 6, 14.f);
            Splinters(B.Pos, B.Vel, 6, 120.f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_BOLT_FIZZLED"));
            Bolts.RemoveAt(I);
            continue;
        }
        if (bAtBeholder && FVector::Dist(B.Pos, Bh->GetCentre()) < ATempleBeholder::BodyRadius + 15.f)
        {
            ++BeholderBolts;
            Sound(ESound::BoltImpact, B.Pos, 1.f);
            FirePuffs(B.Pos, FVector::ZeroVector, 26, 46.f);
            for (int32 K = 0; K < 18; ++K) FirePuffs(B.Pos, FMath::VRand() * 420.f, 1, 22.f);
            Flash->SetWorldLocation(B.Pos); Flash->SetLightColor(FLinearColor(.35f, 1.f, .8f)); FlashTime = .35f;
            Bh->TakeHit(B.Pos, B.Vel, 1.5f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_BOLT_BEHOLDER"));
            Bolts.RemoveAt(I);
            continue;
        }
        if ((S && S->IsUp() && FVector::Dist(B.Pos, Aim) < 28.f) || B.Age > 2.5f || B.Pos.Z < Floor)
        {
            Sound(ESound::BoltImpact, B.Pos, 1.f);
            FirePuffs(B.Pos, FVector::ZeroVector, 24, 40.f);
            for (int32 K = 0; K < 18; ++K) FirePuffs(B.Pos, FMath::VRand() * 420.f, 1, 22.f);
            Splinters(B.Pos, B.Vel, 26, 600.f);
            Dust(B.Pos, FVector::UpVector, 6, 30.f);
            Flash->SetWorldLocation(B.Pos); Flash->SetLightColor(FLinearColor(.35f, 1.f, .8f)); FlashTime = .35f;
            if (S && S->IsUp() && FVector::Dist(B.Pos, Aim) < 40.f) { if (S->TakeHeroBlow(EBlow::Bolt, B.Pos - B.Vel * .1f, B.Pos)) ++Fallen; }
            Bolts.RemoveAt(I);
        }
    }
    // Struck-off skulls: thrown, tumbling, a hard bounce or two, a roll.
    for (FSkullFlight& F : Skulls)
    {
        F.Age += Dt;
        if (F.bRest) continue;
        F.Vel.Z -= G * Dt; F.Pos += F.Vel * Dt;
        F.Rot = FQuat(F.Spin.GetSafeNormal(), static_cast<float>(F.Spin.Size()) * Dt) * F.Rot;
        if (F.Pos.Z < Floor + 9.f)
        {
            F.Pos.Z = Floor + 9.f;
            if (FMath::Abs(F.Vel.Z) > 120.f) { Sound(ESound::SkullBounce, F.Pos, FMath::Clamp(static_cast<float>(FMath::Abs(F.Vel.Z)) / 500.f, .3f, 1.f)); ++F.Bounces; }
            if (FMath::Abs(F.Vel.Z) < 70.f && F.Vel.Size2D() < 30.f) { F.bRest = true; continue; }
            F.Vel.Z *= -.35f; F.Vel.X *= .7f; F.Vel.Y *= .7f; F.Spin *= .7f;
            F.Vel *= FMath::Exp(-1.5f * Dt);
        }
    }
    if (FlashTime > 0.f) { FlashTime -= Dt; Flash->SetIntensity(FMath::Max(0.f, FlashTime) * 60000.f); }
    else Flash->SetIntensity(0.f);
}

void ATempleSanctumBattle::Draw(const FVector& Eye)
{
    {
        TArray<FTransform> T; T.Init(Hidden, ChipPool);
        for (int32 I = 0; I < Chips.Num() && I < ChipPool; ++I)
        {
            const FChip& C = Chips[I];
            const float Fade = FMath::Clamp((C.Life - C.Age) / .6f, 0.f, 1.f);
            T[I] = FTransform(C.Rot, C.Pos, FVector(C.Size, C.Size * .45f, C.Size * .28f) * (Fade / 100.f));
        }
        ChipFx->BatchUpdateInstancesTransforms(0, T, true, true);
    }
    {
        TArray<FTransform> Td; Td.Init(Hidden, DustPool);
        TArray<FTransform> Tf; Tf.Init(Hidden, FirePool);
        int32 Nd = 0, Nf = 0;
        for (const FPuff& P : Puffs)
        {
            const float U = P.Age / P.Life;
            const float A = FMath::Clamp(U / .12f, 0.f, 1.f) * FMath::Clamp((1.f - U) / .5f, 0.f, 1.f) * P.Alpha;
            const float Data[3] = {P.Frame, P.bFire ? 1.f - U : 0.f, A};
            if (P.bFire && Nf < FirePool) { Tf[Nf] = Billboard(P.Pos, Eye, P.Spin, P.Size); BoltFireFx->SetCustomData(Nf, MakeArrayView(Data, 3), false); ++Nf; }
            else if (!P.bFire && Nd < DustPool) { Td[Nd] = Billboard(P.Pos, Eye, P.Spin, P.Size); DustFx->SetCustomData(Nd, MakeArrayView(Data, 3), false); ++Nd; }
        }
        for (int32 I = Nd; I < DustPool; ++I) { const float Z0[3] = {0, 0, 0}; DustFx->SetCustomData(I, MakeArrayView(Z0, 3), false); }
        for (int32 I = Nf; I < FirePool; ++I) { const float Z0[3] = {0, 0, 0}; BoltFireFx->SetCustomData(I, MakeArrayView(Z0, 3), false); }
        DustFx->BatchUpdateInstancesTransforms(0, Td, true, true);
        BoltFireFx->BatchUpdateInstancesTransforms(0, Tf, true, true);
    }
    {
        TArray<FTransform> T; T.Init(Hidden, ArrowPool);
        for (int32 I = 0; I < Arrows.Num() && I < ArrowPool; ++I) T[I] = FTransform(Arrows[I].Vel.Rotation(), Arrows[I].Pos, FVector::OneVector);
        ArrowFx->BatchUpdateInstancesTransforms(0, T, true, true);
    }
    {
        TArray<FTransform> T; T.Init(Hidden, CorePool);
        for (int32 I = 0; I < CorePool; ++I)
        {
            if (I < Bolts.Num())
            {
                T[I] = FTransform(FQuat::Identity, Bolts[I].Pos, FVector(.13f + .02f * FMath::Sin(Bolts[I].Age * 40.f)));
                BoltLights[I]->SetWorldLocation(Bolts[I].Pos); BoltLights[I]->SetIntensity(9000.f);
            }
            else BoltLights[I]->SetIntensity(0.f);
        }
        BoltCoreFx->BatchUpdateInstancesTransforms(0, T, true, true);
    }
    // Skulls: one component each, made as they are needed.
    while (SkullComps.Num() < Skulls.Num())
    {
        auto* C = NewObject<UStaticMeshComponent>(this);
        C->SetStaticMesh(SkullMesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetLightingChannels(false, true, false);
        C->SetMobility(EComponentMobility::Movable);
        C->RegisterComponent();
        if (SkullMesh) { const float L = static_cast<float>(SkullMesh->GetBounds().BoxExtent.GetMax()) * 2.f; C->SetWorldScale3D(FVector(L > 1.f ? 21.f / L : 1.f)); }
        SkullComps.Add(C);
    }
    for (int32 I = 0; I < SkullComps.Num(); ++I)
    {
        UStaticMeshComponent* C = SkullComps[I];
        if (I < Skulls.Num()) { C->SetVisibility(true); C->SetWorldLocationAndRotation(Skulls[I].Pos, Skulls[I].Rot); }
        else C->SetVisibility(false);
    }
}

void ATempleSanctumBattle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!bReady)
    {
        // Streamed in while he is still on his way: within 150 m of the hall (the Astral wind, the trap run).
        if (Chuck && !bLoading)
        {
            const FVector F = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
            const float Dx = FMath::Max(0.f, FMath::Max(HS0 - static_cast<float>(F.X), static_cast<float>(F.X) - HS1));
            const float Dy = FMath::Max(0.f, FMath::Max(HT0 - static_cast<float>(F.Y), static_cast<float>(F.Y) - HT1));
            if (bForced || FMath::Sqrt(Dx * Dx + Dy * Dy) < 15000.f) StartLoading();
        }
        else if (!Chuck && bForced) StartLoading();
        return;
    }
    FVector Eye = FVector::ZeroVector;
    if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0)) Eye = Cam->GetCameraLocation();
    // Only while he is in the sanctum (the hall, its gallery, the end of the last leg) does it go on.
    bool bIn = false;
    if (Chuck)
    {
        const FVector F = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        bIn = F.X > HS0 - 200 && F.X < HS1 + 200 && F.Y > HT0 - 200 && F.Y < HT1 + 900 && F.Z > HZ - 300 && F.Z < HTop + 200;
    }
    const bool bWas = bActive;
    bActive = bIn || bForced;
    // Until the entrance has played, the beholder keeps its opening (the scene counts on it) and only hovers.
    bEntrancePending = bEntranceScene && !bForced && TheScene.IsValid() && TheScene->GetEntrances() == EntranceBase;
    if (ATempleBeholder* Bh = TheBeholder.Get()) Bh->SetFighting(bActive && bBeholderFights && !bEntrancePending);
    if (bActive != bWas || !bTickModeSet)
    {
        // Bones refreshed every frame only while the fight is on (its hit tests read them); otherwise only when seen.
        bTickModeSet = true;
        const EVisibilityBasedAnimTickOption Mode = bActive ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        for (const TWeakObjectPtr<ATempleSkeleton>& S : Pool) if (S.IsValid()) S->GetMesh()->VisibilityBasedAnimTickOption = Mode;
        for (const TWeakObjectPtr<ATempleHero>& H : Heroes) if (H.IsValid()) H->GetMesh()->VisibilityBasedAnimTickOption = Mode;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_ACTIVE %d"), bActive ? 1 : 0);
    }
    const float Wt = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (bActive && bWaves && GetUp() < WantUp && Wt >= NextWake) { WakeOne(nullptr); NextWake = Wt + Random.FRandRange(1.8f, 3.6f); }
    if (bWaves) Recycle();
    // Down on the hall's floor: below the stairs' last metre, inside the hall (the islands in the pool count; the gallery does not).
    bOnFloor = false; bHushed = false;
    if (Chuck)
    {
        const FVector F = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        bOnFloor = bIn && F.Z < HZ + 200 && F.Y < GalleryT - 100;
        bHushed = InChultTempleRubble(Chuck->GetActorLocation());
    }
    // His first coming out onto the gallery this session: the entrance (TempleSanctumScene.h). Not at the hall's edge
    // (user 2026-10-27): the last leg's wall run throws him in quickly, and the old trigger at the end of the leg could
    // fire over its spike pit (during a fall death, or with the tunnels' score still up). It waits until he has been
    // on his feet in the hall itself (the gallery, or wherever he has dropped to) and alive for EntranceSettle seconds
    // (time in the air pauses it), and the sanctum's score has taken over.
    if (bEntrancePending && Chuck)
    {
        const FVector F = ChultTempleInteriorFrameOf(Chuck->GetActorLocation());
        const bool bInHall = F.X > HS0 && F.X < HS1 && F.Y < HT1 - 30 && F.Y > HT0 && F.Z > HZ - 100 && F.Z < HTop;
        const UCharacterMovementComponent* Move = Chuck->GetCharacterMovement();
        const bool bGrounded = Move && Move->IsMovingOnGround();
        if (!bInHall || Chuck->IsAstral()) EntranceDwell = 0;
        else if (bGrounded) EntranceDwell += DeltaSeconds;
        const ADockGameMode* Mode = GetWorld()->GetAuthGameMode<ADockGameMode>();
        const bool bScore = !Mode || Mode->GetMusicRegion() == 8;
        if (EntranceDwell >= EntranceSettle && bGrounded && bScore && TheScene->StartEntrance()) { bEntrancePending = false; EntranceDwell = 0; }
    }
    else EntranceDwell = 0;
    bWasIn = bIn;
    TickTalk(DeltaSeconds, bIn, Chuck);
    TickFlying(DeltaSeconds, Eye);
    Draw(Eye);
}

void ATempleSanctumBattle::ReArmEntrance()
{
    EntranceBase = TheScene.IsValid() ? TheScene->GetEntrances() : 0;
    EntranceDwell = 0;
}

void SpawnTempleSanctumBattle(UWorld* World)
{
    if (!World || Suppressed()) return;
    for (TActorIterator<ATempleSanctumBattle> It(World); It; ++It) return;   // once
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    World->SpawnActor<ATempleSanctumBattle>(FVector::ZeroVector, FRotator::ZeroRotator, P);
}

// ---------------------------------------------------------------------------
// -ChuckSanctumTest / -ChuckSanctumCapture: each of the fighter's blows on a skeleton placed in his measure
// (contact, hit-stop, the fall, the skull), a skeleton's blow on his shield and on his back, the ranger's
// arrows and the wizard's bolt, then the battle let run for a while; stills of each.
void StartTempleSanctumReview(UWorld* World)
{
    ATempleSanctumBattle* B = ATempleSanctumBattle::Get(World);
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    static int32 Waits = 0;
    if (B && !B->IsReady() && Waits++ < 60)
    {
        // Its assets stream in first.
        B->SetForcedActive(true);
        FTimerHandle Again; World->GetTimerManager().SetTimer(Again, [World]() { StartTempleSanctumReview(World); }, .5f, false);
        return;
    }
    if (!B || !Chuck || !B->GetHero(0) || !B->GetHero(1) || !B->GetHero(2))
    {
        UE_LOG(LogTemp, Error, TEXT("CHUCK_SANCTUM_TEST_COMPLETE failures=1 battle=%d"), B != nullptr);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ChuckSanctumCapture"));
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    Chuck->DisableInput(PC);
    Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum));
    Chuck->SetActorHiddenInGame(true);
    auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(60);
    if (PC) { PC->SetViewTarget(Camera); PC->ConsoleCommand(TEXT("t.MaxFPS 60")); }
    B->SetForcedActive(true); B->SetWaves(false);
    // The cutscenes only in their own trials (2026-10-26).
    B->SetFireballScene(false); B->SetEntranceScene(false);
    ATempleHero* F = B->GetHero(0); ATempleHero* Wz = B->GetHero(1); ATempleHero* Rg = B->GetHero(2);
    Wz->SetHeld(true); Rg->SetHeld(true);
    // Kind 0 a cut, 1 a block, 2 a blow on his back, 3 arrows, 4 bolt, 5 the battle, 6 portraits, 7 the beholder; the beholder's fight
    // (2026-10-25): 8 its five rays, 9 a ray through Chuck, 10 the bite, 11 the cone (Chuck in it, out of it, behind a pier), 12 it fighting by itself;
    // 13 the wizard sealing the sea tear (it knits, the tear surges and he strains, the beholder's cone shatters it);
    // 15 the cone at Chuck by the rupture's edge (user 2026-10-25: flung onto a pier's island and trapped): thrown short of it;
    // 14 the heroes' lines (TempleHeroTalk.cpp), the waits shortened: in order, each heard, subtitled, the speaker's jaw moving, the music ducked.
    struct FTrial { const TCHAR* Name; FName Clip; int32 Kind; };
    static const FTrial Trials[] = {{TEXT("cut_down"), TEXT("CutDown"), 0}, {TEXT("cut_flat"), TEXT("CutFlat"), 0}, {TEXT("overhead"), TEXT("Overhead"), 0},
        {TEXT("bash"), TEXT("Bash"), 0}, {TEXT("block"), NAME_None, 1}, {TEXT("hit_behind"), NAME_None, 2},
        {TEXT("arrows"), NAME_None, 3}, {TEXT("bolt"), NAME_None, 4}, {TEXT("portraits"), NAME_None, 6}, {TEXT("beholder"), NAME_None, 7},
        {TEXT("beholder_rays"), NAME_None, 8}, {TEXT("beholder_chuck"), NAME_None, 9}, {TEXT("beholder_bite"), NAME_None, 10}, {TEXT("beholder_cone"), NAME_None, 11}, {TEXT("cone_edge"), NAME_None, 15},
        {TEXT("beholder_fight"), NAME_None, 12}, {TEXT("wizard_seal"), NAME_None, 13}, {TEXT("hero_talk"), NAME_None, 14},
        // The cutscenes (2026-10-26, TempleSanctumScene.h): 16 the entrance; 17 the Fireball from the argument to the rubble; 18 the same skipped with Esc.
        {TEXT("entrance"), NAME_None, 16}, {TEXT("entrance_wait"), NAME_None, 20}, {TEXT("fireball"), NAME_None, 17}, {TEXT("fireball_skip"), NAME_None, 18},
        // 19: the lines after the first wait for him on the hall's floor (user 2026-10-26).
        {TEXT("talk_floor"), NAME_None, 19}, {TEXT("battle"), NAME_None, 5}};
    struct FRun
    {
        int32 Trial = 0, Failures = 0, Phase = 0, Shots = 0, Beat = 0, Swing = -1, Swings0 = 0, Hits0 = 0, Blocks0 = 0, Taken0 = 0, Fallen0 = 0, Decap0 = 0, Shots0 = 0, Up0 = 0;
        float Start = -1, NextShot = 0, DeadAt = -1;
        // The beholder's trials: counts at the start, the cone trial's three parts.
        float SealMax = 0; int32 Strains0 = 0, Surges0 = 0, Breaks0 = 0;
        int32 Rays0 = 0, Dust0 = 0, HeroHits0 = 0, Chuck0 = 0, Bites0 = 0, Cones0 = 0, Volleys0 = 0, Struck0 = 0, Shelter0 = 0, Sub = -1, Seen = 0, Hits[3] = {-1, -1, -1};
        float EventAt = -1;
        float JawMax[3] = {0, 0, 0}, DuckMin = 1;
        int32 TalkLast = -1, TalkOrdered = 1, TalkSounds = 0, TalkSubs = 0;
        float EdgeMoved[3] = {0, 0, 0}; int32 EdgeSafe[3] = {0, 0, 0}, EdgeResp0 = 0; FVector EdgeFrom = FVector::ZeroVector;
        int32 Scene0[4] = {0, 0, 0, 0}, SceneShots = 0; FString SceneShot; bool bSkipSent = false;
        float HallAt = -1, SceneAt = -1; FVector SceneFrom = FVector::ZeroVector; bool bFoughtEarly = false, bTalkEarly = false; int32 Spoken0 = 0;
        TWeakObjectPtr<ATempleSkeleton> Subject;
        FTimerHandle Timer;
    };
    auto Run = MakeShared<FRun>();
    const FVector FHome = F->GetActorLocation();
    World->GetTimerManager().SetTimer(Run->Timer, [World, B, F, Wz, Rg, Run, Camera, bCapture, FHome]()
    {
        constexpr int32 Count = UE_ARRAY_COUNT(Trials);
        if (Run->Trial >= Count) return;
        // -ChuckSanctumOnly=<trial>[+<trial>...]: just those.
        FString OnlyTrial;
        if (FParse::Value(FCommandLine::Get(), TEXT("ChuckSanctumOnly="), OnlyTrial) && Run->Start < 0)
        {
            TArray<FString> Names; OnlyTrial.ParseIntoArray(Names, TEXT("+"));
            while (Run->Trial < Count && !Names.Contains(Trials[Run->Trial].Name)) ++Run->Trial;
        }
        if (Run->Trial >= Count) { UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_TEST_COMPLETE failures=%d"), Run->Failures); FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0); return; }
        const FTrial& T = Trials[Run->Trial];
        const float Now = static_cast<float>(World->GetTimeSeconds());
        auto Shot = [&](const FString& Name)
        {
            if (!bCapture || Run->Shots >= 140 || FScreenshotRequest::IsScreenshotRequested()) return;
            ++Run->Shots;
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Sanctum") / (Name + TEXT(".png")), false, false);
        };
        auto View = [&](const FVector& Eye, const FVector& Aim) { Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation()); };
        const FVector Fw = F->GetActorForwardVector(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
        const FVector FAt = F->GetActorLocation();
        if (Run->Start < 0)
        {
            Run->Start = Now; Run->Phase = 0;
            Run->Swings0 = F->GetSwings(); Run->Hits0 = F->GetCleanHits(); Run->Blocks0 = F->GetBlocks(); Run->Taken0 = F->GetHitsTaken();
            Run->Fallen0 = B->GetFallen(); Run->Decap0 = B->GetDecapitations(); Run->Shots0 = Rg->GetShots() + Wz->GetShots(); Run->Up0 = B->GetUp();
            // A fresh skeleton for the trial: the nearest in reserve, woken and set before the fighter (or behind him).
            // For a blow (2026-10-23), those left standing from the last trial go back too: its forced cut is the fighter's
            // next swing, and it must meet only the trial's own skeleton (one still chasing him took it out of range).
            for (const TWeakObjectPtr<ATempleSkeleton>& S : B->Foes())
                if (S.IsValid() && (S->IsDead() || T.Kind <= 2)) { FTempleSkeletonPost P = S->GetPost(); S->ResetToReserve(P); }
            B->ClearFlying();
            F->SetActorLocation(FHome); F->SetActorRotation(FRotator(0, (B->BeholderSpot() - FHome).Rotation().Yaw, 0)); F->Rest();
            F->SetHeld(T.Kind == 3 || T.Kind == 4);
            B->SetBeholderFights(T.Kind == 12 || T.Kind == 5);
            if (ATempleBeholder* Bh0 = B->GetBeholder())
            {
                Run->Rays0 = Bh0->GetRaysFired(); Run->Dust0 = Bh0->GetDisintegrated(); Run->HeroHits0 = Bh0->GetHeroHits(); Run->Chuck0 = Bh0->GetChuckHits();
                Run->Bites0 = Bh0->GetBites(); Run->Cones0 = Bh0->GetCones(); Run->Volleys0 = Bh0->GetVolleys(); Run->Struck0 = Bh0->GetHitsTaken(); Run->Shelter0 = Bh0->GetSheltered();
                Run->Sub = -1; Run->Seen = Bh0->GetRaysFired(); Run->EventAt = -1; Run->Hits[0] = Run->Hits[1] = Run->Hits[2] = -1;
            }
            Wz->SetHeld(T.Kind != 4 && T.Kind != 5); Rg->SetHeld(T.Kind != 3 && T.Kind != 5);
            if (T.Kind == 13)
            {
                Wz->SetActorLocation(Wz->GetActorLocation()); Wz->Rest(); Wz->SetHeld(false);
                if (ATempleSeal* Sl = B->GetSeal()) { Run->Strains0 = Wz->GetStrains(); Run->Surges0 = Sl->GetSurges(); Run->Breaks0 = Sl->GetBreaks(); Run->SealMax = 0; }
            }
            if (T.Kind == 19)
            {
                if (AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))) Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum));
                B->RestartTalk(.25f); Run->Sub = 0; Run->TalkLast = -1;
            }
            if (T.Kind == 14)
            {
                // Down on the hall's floor (hidden): the lines after the first wait for him there (2026-10-26).
                if (AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))) Ch->ResetAtLocation(W(HallMid + 650, DaisT1 + 1900, HZ + 34.65f));
                B->RestartTalk(.25f);
                Run->JawMax[0] = Run->JawMax[1] = Run->JawMax[2] = 0; Run->DuckMin = 1; Run->TalkLast = -1; Run->TalkOrdered = 1; Run->TalkSounds = 0; Run->TalkSubs = 0;
            }
            if (T.Kind == 20)
            {
                // Walked in from the last leg's end (past its pit), the battle as in play and not forced: the entrance must
                // wait until he is on his feet in the hall a while, and the beholder hold its opening until it plays.
                for (ATempleHero* H : {F, Wz, Rg}) { H->EndScript(); H->Rest(); H->SetHeld(false); }
                B->SetWaves(true); B->SetBeholderFights(true); B->RestartTalk(1.f);
                B->SetForcedActive(false); B->SetEntranceScene(true); B->ReArmEntrance();
                if (ATempleSanctumScene* Sc = B->GetScene()) Run->Scene0[0] = Sc->GetEntrances();
                Run->Spoken0 = B->GetTalkSpoken(); Run->HallAt = Run->SceneAt = -1; Run->bFoughtEarly = Run->bTalkEarly = false; Run->SceneShots = 0; Run->SceneShot.Reset();
                if (AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
                {
                    Ch->ResetAtLocation(W(78710, HT1 + 450, UZ + 34.65f));
                    Ch->SetActorRotation(FRotator(0, YawToward(Ch->GetActorLocation(), W(78710, HT0, UZ)), 0)); Ch->Recenter();
                    Ch->SetActorHiddenInGame(false); Ch->SetSanity(AChuckCharacter::MaxSanity);
                    if (auto* PCc = Cast<APlayerController>(Ch->GetController())) PCc->SetViewTarget(Ch);
                }
            }
            if (T.Kind >= 16 && T.Kind <= 18)
            {
                // The cutscenes: the battle as in play, Chuck there and seen (on the gallery for the entrance, on the hall's floor for the Fireball).
                for (ATempleHero* H : {F, Wz, Rg}) { H->EndScript(); H->Rest(); H->SetHeld(false); }
                B->SetWaves(true); B->SetBeholderFights(true);
                if (ATempleSanctumScene* Sc = B->GetScene()) { Run->Scene0[0] = Sc->GetEntrances(); Run->Scene0[1] = Sc->GetFireballs(); Run->Scene0[2] = Sc->GetThrows(); Run->Scene0[3] = Sc->GetSkips(); }
                Run->SceneShots = 0; Run->SceneShot.Reset(); Run->bSkipSent = false;
                AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
                if (Ch)
                {
                    Ch->ResetAtLocation(T.Kind == 16 ? ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum) : W(HallMid + 650, DaisT1 + 1900, HZ + 34.65f));
                    Ch->SetActorRotation(FRotator(0, YawToward(Ch->GetActorLocation(), B->BeholderSpot()), 0)); Ch->Recenter();
                    Ch->SetActorHiddenInGame(false); Ch->SetSanity(AChuckCharacter::MaxSanity);
                    if (auto* PCc = Cast<APlayerController>(Ch->GetController())) PCc->SetViewTarget(Ch);
                }
                if (T.Kind == 16) { if (ATempleSanctumScene* Sc = B->GetScene()) Sc->StartEntrance(); }
                else { B->SetFireballScene(true); B->RestartTalk(.12f); }
            }
            else if (T.Kind == 5 || T.Kind == 12) { B->SetWaves(true); Wz->SetHeld(false); Rg->SetHeld(false); F->SetHeld(false); }
            else if (T.Kind >= 6 && T.Kind != 13)
            {
                // Each hero still, in its rest clip, the fallen cleared away.
                for (const TWeakObjectPtr<ATempleSkeleton>& K : B->Foes()) if (K.IsValid() && !K->IsDead()) { FTempleSkeletonPost P = K->GetPost(); K->ResetToReserve(P); }
                for (ATempleHero* H : {F, Wz, Rg}) { H->Rest(); H->SetHeld(false); }
            }
            else
            {
                ATempleSkeleton* S = B->WakeOne(T.Kind == 3 ? Rg : T.Kind == 4 ? Wz : F, &FAt);
                Run->Subject = S;
                if (S)
                {
                    const FVector Place = T.Kind == 2 ? FAt - Fw * 100.f : T.Kind >= 3 ? FAt + Fw * 420.f + Rt * 160.f : FAt + Fw * (T.Clip == FName(TEXT("Bash")) ? 80.f : 100.f);
                    S->SetActorLocation(FVector(Place.X, Place.Y, S->GetActorLocation().Z));
                    S->SetActorRotation(FRotator(0, (FAt - Place).Rotation().Yaw, 0));
                    S->SetFoe(T.Kind == 3 ? Rg : T.Kind == 4 ? Wz : F);
                    if (T.Kind == 0) F->ForceClip(T.Clip);
                }
            }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_TRIAL_START trial=%s"), T.Name);
        }
        const float Age = Now - Run->Start;
        // The camera: side-on to the fighter and his foe, the blow across the frame.
        if (T.Kind <= 2)
        {
            const ATempleSkeleton* Sub = Run->Subject.Get();
            const FVector Mid = Sub ? (FAt + Sub->GetActorLocation()) * .5f : FAt + Fw * 60.f;
            const FVector Across = Sub ? FVector::CrossProduct(FVector::UpVector, (Sub->GetActorLocation() - FAt).GetSafeNormal2D()) : Rt;
            View(Mid + Across * 360.f + FVector(0, 0, 55), Mid + FVector(0, 0, 20));
        }
        else if (T.Kind == 3) View(Rg->GetActorLocation() - Rg->GetActorForwardVector() * 240.f + FVector(0, 0, 90) + FVector::CrossProduct(FVector::UpVector, Rg->GetActorForwardVector()) * 120.f, Rg->GetActorLocation() + Rg->GetActorForwardVector() * 300.f);
        else if (T.Kind == 6)
        {
            // Nine views, three of each: full figure three-quarter, the head, the back of the head (hair, 2026-10-29).
            const int32 V = FMath::Clamp(static_cast<int32>(Age / 1.5f), 0, 8);
            ATempleHero* H = V < 3 ? F : V < 6 ? Wz : Rg;
            const FVector Hf = H->GetActorForwardVector(), Hr = FVector::CrossProduct(FVector::UpVector, Hf), Ha = H->GetActorLocation();
            if (V % 3 == 0) View(Ha + Hf * 260.f + Hr * 150.f + FVector(0, 0, 30), Ha + FVector(0, 0, 5));
            else if (V % 3 == 2) View(H->HeadLocation() - Hf * 70.f + Hr * 30.f + FVector(0, 0, 8), H->HeadLocation() - FVector(0, 0, 8));
            else View(H->HeadLocation() + Hf * 75.f + Hr * 35.f + FVector(0, 0, 4), H->HeadLocation() - FVector(0, 0, 6));
            if (bCapture && Age > 1.f + V * 1.5f && Run->Phase == V && !FScreenshotRequest::IsScreenshotRequested())
            {
                Shot(FString::Printf(TEXT("Sanctum_portrait_%s_%s"), H->GetRoleName(), V % 3 == 1 ? TEXT("head") : V % 3 == 2 ? TEXT("back") : TEXT("figure")));
                ++Run->Phase;
            }
        }
        else if (T.Kind == 7)
        {
            // The beholder: from the fighter's place, close before it, its side, the great eye, a snarl, and the hall from behind the heroes.
            if (ATempleBeholder* Bh = B->GetBeholder())
            {
                const int32 V = FMath::Clamp(static_cast<int32>(Age / 2.f), 0, 5);
                const FVector Bc = Bh->GetActorLocation(), Bf = Bh->GetActorForwardVector(), Br = FVector::CrossProduct(FVector::UpVector, Bf);
                if (V == 4 && Run->Phase == 4 && Run->Beat == 0) { Bh->Snarl(); Run->Beat = 1; }
                Bh->HoldGaze(V == 3 || V == 4 ? Camera->GetActorLocation() : FVector::ZeroVector);
                switch (V)
                {
                case 0: View(FAt + FVector(0, 0, 150) - Fw * 120.f + Rt * 60.f, Bc); break;
                case 1: View(Bc + Bf * 430.f + Br * 160.f - FVector(0, 0, 60), Bc + FVector(0, 0, 40)); break;
                case 2: View(Bc + Br * 520.f + FVector(0, 0, 40), Bc + FVector(0, 0, 50)); break;
                case 3: View(Bh->GetEyeLocation() + Bf * 170.f + Br * 30.f - FVector(0, 0, 15), Bh->GetEyeLocation()); break;
                case 4: View(Bc + Bf * 240.f - Br * 70.f - FVector(0, 0, 90), Bc - FVector(0, 0, 55)); break;
                default: View(FAt - Fw * 900.f + Rt * 250.f + FVector(0, 0, 330), Bc - FVector(0, 0, 100)); break;
                }
                if (bCapture && Age > 1.2f + V * 2.f && Run->Phase == V && !FScreenshotRequest::IsScreenshotRequested())
                {
                    static const TCHAR* Names[] = {TEXT("from_fighter"), TEXT("front"), TEXT("side"), TEXT("eye"), TEXT("snarl"), TEXT("hall")};
                    Shot(FString::Printf(TEXT("Sanctum_beholder_%s"), Names[V]));
                    ++Run->Phase;
                }
            }
        }
        else if (((T.Kind >= 8 && T.Kind <= 12) || T.Kind == 15) && B->GetBeholder())
        {
            ATempleBeholder* Bh = B->GetBeholder();
            const FVector Bc = Bh->GetActorLocation();
            const FVector In = (FAt - Bc).GetSafeNormal2D(), Ac = FVector::CrossProduct(FVector::UpVector, In);
            AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (T.Kind == 8) View(FAt + In * 900.f - Ac * 650.f + FVector(0, 0, 160), (Bc + FAt) * .5f - FVector(0, 0, 40));
            else if (T.Kind == 9 && Ch) View(Ch->GetActorLocation() + (Ch->GetActorLocation() - Bc).GetSafeNormal2D() * 260.f + Ac * 220.f + FVector(0, 0, 60), FMath::Lerp(Ch->GetActorLocation(), Bc, .35f));
            else if (T.Kind == 10) { const FVector Mid = (Bc + FAt) * .5f; View(Mid - Ac * 760.f + In * 150.f + FVector(0, 0, 70), Mid + FVector(0, 0, 30)); }
            else if ((T.Kind == 11 || T.Kind == 15) && Ch)
            {
                const FVector Cp = Ch->GetActorLocation();
                const FVector Out = (Cp - Bc).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Out);
                View(Cp + Out * 520.f + Side * 380.f + FVector(0, 0, 260), FMath::Lerp(Bc, Cp, .55f) - FVector(0, 0, 80));
            }
            else if (T.Kind == 12)
            {
                // Four views in turn: behind the heroes, the aisle, the floor where Chuck would be, over the fighter's shoulder.
                switch (static_cast<int32>(Age / 6.f) % 4)
                {
                case 0: View(FAt + In * 950.f + Ac * 260.f + FVector(0, 0, 330), Bc - FVector(0, 0, 120)); break;
                case 1: View((Bc + FAt) * .5f + Ac * 1100.f + FVector(0, 0, 60), (Bc + FAt) * .5f + FVector(0, 0, 40)); break;
                case 2: View(FAt + In * 1700.f - Ac * 300.f + FVector(0, 0, 60), Bc - FVector(0, 0, 60)); break;
                default: View(FAt + In * 170.f - Ac * 90.f + FVector(0, 0, 165), Bc); break;
                }
            }
            if (bCapture && !FScreenshotRequest::IsScreenshotRequested())
            {
                // On its events: each ray just after it fires, the bite's beats, the cone's charge, blast and front; and every 2.5 s in the free fight.
                if (Bh->GetRaysFired() != Run->Seen) { Run->Seen = Bh->GetRaysFired(); Run->EventAt = Now + .07f; }
                if (Run->EventAt > 0 && Now >= Run->EventAt) { Run->EventAt = -1; Shot(FString::Printf(TEXT("Sanctum_%s_ray%02d"), T.Name, Run->Seen - Run->Rays0)); }
                const float Mt = Bh->GetMoveTime();
                static const float BiteBeats[] = {.3f, .62f, .8f, .93f, 1.1f, 1.45f, 2.f};
                static const float ConeBeats[] = {.6f, 1.6f, 2.3f, 2.47f, 2.62f, 2.8f, 3.2f};
                const float* Beats = Bh->GetMove() == EBeholderMove::Bite ? BiteBeats : Bh->GetMove() == EBeholderMove::Cone ? ConeBeats : nullptr;
                const int32 MoveId = Bh->GetBites() * 100 + Bh->GetCones();
                if (MoveId != Run->Swing) { Run->Swing = MoveId; Run->Beat = 0; }
                if (Beats && Run->Beat < 7 && Mt >= Beats[Run->Beat] && !FScreenshotRequest::IsScreenshotRequested())
                {
                    Shot(FString::Printf(TEXT("Sanctum_%s_%s%d_%d"), T.Name, Bh->GetMove() == EBeholderMove::Bite ? TEXT("bite") : TEXT("cone"), Bh->GetMove() == EBeholderMove::Bite ? Bh->GetBites() - Run->Bites0 : Bh->GetCones() - Run->Cones0, Run->Beat));
                    ++Run->Beat;
                }
                if (T.Kind == 12 && Now >= Run->NextShot && !FScreenshotRequest::IsScreenshotRequested()) { Run->NextShot = Now + 2.5f; Shot(FString::Printf(TEXT("Sanctum_%s_t%02d"), T.Name, static_cast<int32>(Age))); }
            }
        }
        else if (T.Kind == 14)
        {
            // The one speaking, from before him at head height; between lines, the three together.
            const int32 Pl = B->GetTalkPlaying();
            ATempleHero* Who = Pl >= 0 ? B->GetHero(Pl == 0 || Pl == 4 ? 0 : Pl == 2 || Pl == 3 ? 2 : 1) : nullptr;
            if (Who) View(Who->HeadLocation() + Who->GetActorForwardVector() * 140.f + FVector::CrossProduct(FVector::UpVector, Who->GetActorForwardVector()) * 40.f + FVector(0, 0, 5), Who->HeadLocation() - FVector(0, 0, 8));
            else View(FAt - Fw * 300.f + Rt * 120.f + FVector(0, 0, 180), Wz->GetActorLocation() + FVector(0, 0, 60));
            if (bCapture && Pl >= 0 && Now >= Run->NextShot && !FScreenshotRequest::IsScreenshotRequested()) { Run->NextShot = Now + .5f; Shot(FString::Printf(TEXT("Sanctum_hero_talk_%d_%02d"), Pl, static_cast<int32>(Age * 2.f))); }
        }
        else if (T.Kind == 13 && B->GetSeal())
        {
            // The wizard and the tear: from his left side, low; over his shoulder at the web; from across the tear at him; high above.
            const FVector Sc = B->GetSeal()->GetCentre(), Wl = Wz->GetActorLocation();
            const FVector To = (Sc - Wl).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, To);
            switch (FMath::Clamp(static_cast<int32>(Age / 4.5f), 0, 3))
            {
            case 0: View(FMath::Lerp(Wl, Sc, .45f) - Side * 460.f - To * 120.f + FVector(0, 0, 150), FMath::Lerp(Wl, Sc, .5f) - FVector(0, 0, 30)); break;
            case 1: View(Wl - To * 170.f + Side * 70.f + FVector(0, 0, 110), Sc); break;
            case 2: View(Sc + To * 180.f - Side * 260.f + FVector(0, 0, 120), FMath::Lerp(Sc, Wl, .6f) + FVector(0, 0, 20)); break;
            default: View(FMath::Lerp(Wl, Sc, .5f) + Side * 250.f - To * 150.f + FVector(0, 0, 520), FMath::Lerp(Wl, Sc, .55f) - FVector(0, 0, 60)); break;
            }
            if (bCapture && Now >= Run->NextShot && !FScreenshotRequest::IsScreenshotRequested()) { Run->NextShot = Now + .75f; Shot(FString::Printf(TEXT("Sanctum_wizard_seal_%02d"), static_cast<int32>(Age * 4.f))); }
        }
        else if (T.Kind == 4) View(Wz->GetActorLocation() + FVector(0, 0, 160) - Wz->GetActorForwardVector() * 200.f + FVector::CrossProduct(FVector::UpVector, Wz->GetActorForwardVector()) * -180.f, Wz->GetActorLocation() + Wz->GetActorForwardVector() * 400.f);
        else View(FAt - Fw * 650.f + Rt * 350.f + FVector(0, 0, 260), FAt + Fw * 150.f);
        // Stills through each blow: before, at and after its moment of contact.
        if (bCapture && T.Kind <= 2 && F->GetContactTime() > 0.f)
        {
            static const float Beats[] = {-.24f, -.12f, -.05f, 0.f, .06f, .16f, .4f, .8f, 1.2f};
            if (F->GetSwings() != Run->Swing) { Run->Swing = F->GetSwings(); Run->Beat = 0; }
            if (Run->Beat < UE_ARRAY_COUNT(Beats) && F->GetClipTime() >= F->GetContactTime() + Beats[Run->Beat] && !FScreenshotRequest::IsScreenshotRequested())
            {
                Shot(FString::Printf(TEXT("Sanctum_%s_s%d_%02d"), T.Name, Run->Swing - Run->Swings0, Run->Beat));
                ++Run->Beat;
            }
        }
        if (bCapture && T.Kind > 0 && T.Kind < 6 && Age > 3.f && Now >= Run->NextShot && Run->Phase < 6)
        {
            Run->NextShot = Now + (T.Kind == 5 ? 2.f : .25f);
            ++Run->Phase;
            Shot(FString::Printf(TEXT("Sanctum_%s_%d"), T.Name, Run->Phase));
        }
        ATempleSkeleton* S = Run->Subject.Get();
        bool bDone = false, bPass = false; FString Note;
        switch (T.Kind)
        {
        case 0:
            if (S && S->IsDead() && Run->DeadAt < 0) Run->DeadAt = Now;
            if (Run->DeadAt > 0) { bDone = Now - Run->DeadAt > 2.6f; bPass = F->GetCleanHits() > Run->Hits0; }
            if (T.Clip == FName(TEXT("Bash")) && F->GetCleanHits() > Run->Hits0 && Age > 2.5f) { bDone = true; bPass = true; }
            if (Age > 9.f) bDone = true;
            Note = FString::Printf(TEXT("swings=%d hits=%d fell=%d decap=%d hitstop=%.3f blade_m_s=%.1f cut=%s"), F->GetSwings() - Run->Swings0, F->GetCleanHits() - Run->Hits0,
                B->GetFallen() - Run->Fallen0, B->GetDecapitations() - Run->Decap0, F->GetLastHitStop(), F->GetLastBladeSpeed(), *F->GetLastCut().ToString());
            if (bDone && T.Clip == FName(TEXT("CutFlat")) && B->GetDecapitations() == Run->Decap0) Note += TEXT(" (no decapitation)");
            break;
        case 1:
            F->SetHeld(true);   // he stands in guard and takes it
            if (F->GetBlocks() > Run->Blocks0 && Age > 2.5f) { bDone = true; bPass = true; }
            if (Age > 12.f) bDone = true;
            Note = FString::Printf(TEXT("blocks=%d taken=%d"), F->GetBlocks() - Run->Blocks0, F->GetHitsTaken() - Run->Taken0);
            break;
        case 2:
            F->SetHeld(true);
            if (F->GetHitsTaken() > Run->Taken0 && Age > 2.5f) { bDone = true; bPass = true; }
            if (Age > 12.f) bDone = true;
            Note = FString::Printf(TEXT("blocks=%d taken=%d"), F->GetBlocks() - Run->Blocks0, F->GetHitsTaken() - Run->Taken0);
            break;
        case 3:
            if (S) { S->SetActorLocation(FVector(S->GetActorLocation().X, S->GetActorLocation().Y, S->GetActorLocation().Z)); }
            if (S && S->IsDead() && Age > 2.f) { bDone = Age > 6.f; bPass = S->GetArrowsIn() >= 3; }
            if (Age > 22.f) bDone = true;
            Note = FString::Printf(TEXT("shots=%d arrows_in=%d dead=%d"), Rg->GetShots() + Wz->GetShots() - Run->Shots0, S ? S->GetArrowsIn() : -1, S && S->IsDead());
            break;
        case 4:
            if (S && S->IsDead() && Age > 2.f) { bDone = Age > 5.f; bPass = true; }
            if (Age > 22.f) bDone = true;
            Note = FString::Printf(TEXT("casts=%d dead=%d"), Wz->GetShots() + Rg->GetShots() - Run->Shots0, S && S->IsDead());
            break;
        case 6:
            bDone = Age > 14.5f; bPass = true;
            Note = TEXT("stills");
            break;
        case 7:
        {
            // It is there, posed every frame, its stalks moving, its eye and lids and jaw working.
            ATempleBeholder* Bh = B->GetBeholder();
            bDone = Age > 12.5f || !Bh;
            bPass = Bh && Bh->GetPoses() > 200 && Bh->GetStalkTravel() > 8.f && Bh->GetBlinks() >= 1 && Bh->GetSnarls() >= 1 && Bh->GetSaccades() >= 3;
            Note = Bh ? FString::Printf(TEXT("poses=%d stalk_travel_cm=%.1f blinks=%d snarls=%d saccades=%d jaw=%.1f"), Bh->GetPoses(), Bh->GetStalkTravel(), Bh->GetBlinks(), Bh->GetSnarls(), Bh->GetSaccades(), Bh->GetJawAngle()) : FString(TEXT("no beholder"));
            if (bDone && Bh) Bh->HoldGaze(FVector::ZeroVector);
            break;
        }
        case 8:
        {
            // Its five rays, one of each: dust a skeleton set before the fighter, then one at each hero (the last to be evaded).
            ATempleBeholder* Bh = B->GetBeholder();
            if (!Bh) { bDone = true; break; }
            if (Run->Phase == 0 && Age > .2f)
            {
                Run->Phase = 1;
                const FVector Place = FAt + (B->BeholderSpot() - FAt).GetSafeNormal2D() * 260.f + Rt * 60.f;
                ATempleSkeleton* K = B->WakeOne(F, &Place);
                if (K) { K->SetActorLocation(FVector(Place.X, Place.Y, K->GetActorLocation().Z)); K->SetActorRotation(FRotator(0, (FAt - Place).Rotation().Yaw, 0)); }
                Run->Subject = K;
                TArray<FBeholderRayOrder> O;
                auto Add = [&O](EBeholderRay Kind, AActor* At, bool bStrike, float Delay) { FBeholderRayOrder R; R.Kind = Kind; R.Target = At; R.bStrike = bStrike; R.Delay = Delay; O.Add(R); };
                Add(EBeholderRay::Disintegrate, K, true, .8f);
                Add(EBeholderRay::Enervate, Wz, true, 1.9f);
                Add(EBeholderRay::Telekinetic, Rg, true, 3.f);
                Add(EBeholderRay::Paralyze, F, true, 4.1f);
                Add(EBeholderRay::Fear, Wz, false, 5.2f);
                Bh->OrderRays(O);
            }
            bDone = Age > 8.5f;
            bPass = Bh->GetRaysFired() - Run->Rays0 == 5 && Bh->GetDisintegrated() > Run->Dust0 && Bh->GetHeroHits() - Run->HeroHits0 >= 2;
            Note = FString::Printf(TEXT("rays=%d dust=%d hero_hits=%d"), Bh->GetRaysFired() - Run->Rays0, Bh->GetDisintegrated() - Run->Dust0, Bh->GetHeroHits() - Run->HeroHits0);
            break;
        }
        case 9:
        {
            // A ray loosed through Chuck where he stands on the floor: a point of Sanity.
            ATempleBeholder* Bh = B->GetBeholder();
            auto* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (!Bh || !Ch) { bDone = true; break; }
            if (Run->Phase == 0)
            {
                Run->Phase = 1;
                Ch->SetActorHiddenInGame(false); Ch->SetSanity(AChuckCharacter::MaxSanity);
                Ch->ResetAtLocation(FVector(FAt.X, FAt.Y, B->FloorZ() + 60.f) + Rt * 380.f - Fw * 250.f);
            }
            if (Run->Phase == 1 && Age > .8f)
            {
                Run->Phase = 2;
                FBeholderRayOrder R; R.Kind = EBeholderRay::Fear; R.Target = Ch; R.bStrike = true;
                Bh->OrderRays({R});
            }
            bDone = Age > 3.5f;
            bPass = Ch->GetSanity() == AChuckCharacter::MaxSanity - 1 && Bh->GetChuckHits() - Run->Chuck0 == 1;
            Note = FString::Printf(TEXT("sanity=%d chuck_hits=%d"), Ch->GetSanity(), Bh->GetChuckHits() - Run->Chuck0);
            if (bDone) { Ch->SetSanity(AChuckCharacter::MaxSanity); Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum)); Ch->SetActorHiddenInGame(true); }
            break;
        }
        case 10:
        {
            // The bite on the fighter: the lunge reaches him, the jaws close on his shield, it goes back up to its place.
            ATempleBeholder* Bh = B->GetBeholder();
            if (!Bh) { bDone = true; break; }
            if (Run->Phase == 0 && Age > .6f) { Run->Phase = 1; Bh->OrderBite(F); }
            bDone = Age > 5.f;
            bPass = Bh->GetBites() - Run->Bites0 == 1 && Bh->GetClosestBite() < 60.f && F->GetBlocks() > Run->Blocks0 && Bh->GetHomeGap() < 90.f && Bh->GetMove() == EBeholderMove::Hover;
            Note = FString::Printf(TEXT("bites=%d closest_cm=%.0f blocks=%d counter_cuts=%d home_gap_cm=%.0f"), Bh->GetBites() - Run->Bites0, Bh->GetClosestBite(), F->GetBlocks() - Run->Blocks0, F->GetBeholderCuts(), Bh->GetHomeGap());
            break;
        }
        case 11:
        {
            // The cone three times: Chuck in its path beyond the fighter (hit), well off its line (missed), and behind a pier (sheltered).
            ATempleBeholder* Bh = B->GetBeholder();
            auto* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (!Bh || !Ch) { bDone = true; break; }
            const int32 Part = FMath::Min(2, static_cast<int32>(Age / 6.f));
            if (Part != Run->Sub)
            {
                if (Run->Sub >= 0) Run->Hits[Run->Sub] = AChuckCharacter::MaxSanity - Ch->GetSanity();
                Run->Sub = Part; Run->Phase = 0;
                Ch->SetActorHiddenInGame(false); Ch->SetSanity(AChuckCharacter::MaxSanity);
                const FVector Bc = Bh->GetActorLocation();
                const FVector Dir = (FAt - Bc).GetSafeNormal2D();
                FVector Spot, Toward;
                if (Part == 0) { Toward = FAt - FVector(0, 0, ATempleHero::HalfHeight) + Dir * 500.f; Spot = Bc + Dir * 1750.f; }
                else if (Part == 1) { Toward = FAt - FVector(0, 0, ATempleHero::HalfHeight) + Dir * 500.f; Spot = Bc + Dir.RotateAngleAxis(52.f, FVector::UpVector) * 1300.f; }
                else
                {
                    const FVector Pier = W(HallMid - NaveHalf, PierT[3], HZ);
                    Toward = Pier + FVector(0, 0, 60);
                    Spot = Pier + (Pier - Bc).GetSafeNormal2D() * (PierHalf + 75.f);
                }
                Ch->ResetAtLocation(FVector(Spot.X, Spot.Y, B->FloorZ() + 60.f));
                // The cone is ordered a moment later, once he has settled on the floor.
                FTimerHandle Later;
                TWeakObjectPtr<ATempleBeholder> WBh(Bh);
                World->GetTimerManager().SetTimer(Later, [WBh, Toward]() { if (WBh.IsValid()) WBh->OrderCone(Toward); }, .5f, false);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_TRIAL part=%d chuck=%s"), Part, *Spot.ToCompactString());
            }
            bDone = Age > 17.5f;
            if (bDone) Run->Hits[2] = AChuckCharacter::MaxSanity - Ch->GetSanity();
            bPass = Run->Hits[0] == 2 && Run->Hits[1] == 0 && Run->Hits[2] == 0 && Bh->GetSheltered() > Run->Shelter0 && Bh->GetCones() - Run->Cones0 == 3;
            Note = FString::Printf(TEXT("in_path_lost=%d off_line_lost=%d behind_pier_lost=%d sheltered=%d cones=%d"), Run->Hits[0], Run->Hits[1], Run->Hits[2], Bh->GetSheltered() - Run->Shelter0, Bh->GetCones() - Run->Cones0);
            if (bDone) { Ch->SetSanity(AChuckCharacter::MaxSanity); Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum)); Ch->SetActorHiddenInGame(true); }
            break;
        }
        case 15:
        {
            // The cone at Chuck 2.25 m before the rupture's edge, in line with a colonnade pier's island, then before open rupture
            // (a full throw would land him on the island or in the Astral), then well back from it (the full throw kept).
            ATempleBeholder* Bh = B->GetBeholder();
            auto* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (!Bh || !Ch) { bDone = true; break; }
            auto Judge = [&](int32 K)
            {
                Run->Hits[K] = AChuckCharacter::MaxSanity - Ch->GetSanity();
                const FVector Fr = ChultTempleInteriorFrameOf(Ch->GetActorLocation());
                const FVector2D F2(Fr.X, Fr.Y);
                Run->EdgeSafe[K] = ChultTempleSanctumHoleAt(F2) == 0 && !ChultTempleSanctumOnIsland(F2) && Fr.Y < PoolT - PoolJag && Ch->GetRespawns() == Run->EdgeResp0;
                Run->EdgeMoved[K] = static_cast<float>(FVector::Dist2D(Ch->GetActorLocation(), Run->EdgeFrom));
                UE_LOG(LogTemp, Display, TEXT("CHUCK_CONE_EDGE part=%d lost=%d safe=%d moved=%.0f frame=%s"), K, Run->Hits[K], Run->EdgeSafe[K], Run->EdgeMoved[K], *Fr.ToCompactString());
            };
            const int32 Part = FMath::Min(2, static_cast<int32>(Age / 8.f));
            // Beside the second colonnade pier on its inner side (a diagonal throw carries over the gap onto the first pier's island),
            // before open rupture, and well back from it.
            static const FVector2D Spots[] = {{HallMid - NaveHalf + 220, PoolT - 200}, {HallMid, PoolT - 225}, {HallMid + 700, PoolT - 925}};
            const FVector Spot = W(Spots[Part].X, Spots[Part].Y, HZ);
            if (Part != Run->Sub)
            {
                if (Run->Sub >= 0) Judge(Run->Sub);
                Run->Sub = Part; Run->Phase = 0;
                Ch->SetActorHiddenInGame(false); Ch->SetSanity(AChuckCharacter::MaxSanity);
                Ch->ResetAtLocation(FVector(Spot.X, Spot.Y, B->FloorZ() + 60.f));
                Run->EdgeFrom = Ch->GetActorLocation(); Run->EdgeResp0 = Ch->GetRespawns();
                // It hangs over the dais, the edge at the cone's limit: brought 16 m off him, as the cutscenes can.
                FVector Near = W(HallMid, Spots[Part].Y - 1600, HZ); Near.Z = Bh->GetActorLocation().Z;
                Bh->OrderHover(Near);
            }
            // Ordered once he has settled and the beholder is free (a cone from the trial before may still be ending).
            if (Run->Phase == 0 && Age - Part * 8.f > 2.5f && Bh->OrderCone(Spot + FVector(0, 0, 40))) Run->Phase = 1;
            bDone = Age > 23.5f;
            if (bDone) { Judge(2); Bh->OrderHover(FVector::ZeroVector); }
            bPass = Run->Hits[0] == 2 && Run->Hits[1] == 2 && Run->Hits[2] == 2 && Run->EdgeSafe[0] && Run->EdgeSafe[1] && Run->EdgeSafe[2] && Run->EdgeMoved[2] > 250.f;
            Note = FString::Printf(TEXT("lost=%d/%d/%d safe=%d/%d/%d moved=%.0f/%.0f/%.0f"), Run->Hits[0], Run->Hits[1], Run->Hits[2], Run->EdgeSafe[0], Run->EdgeSafe[1], Run->EdgeSafe[2],
                Run->EdgeMoved[0], Run->EdgeMoved[1], Run->EdgeMoved[2]);
            if (bDone) { Ch->SetSanity(AChuckCharacter::MaxSanity); Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum)); Ch->SetActorHiddenInGame(true); }
            break;
        }
        case 12:
        {
            // It fights by itself with the skeletons coming: every kind of move within the minute, the heroes striking back.
            ATempleBeholder* Bh = B->GetBeholder();
            if (!Bh) { bDone = true; break; }
            bDone = Age > 52.f;
            bPass = Bh->GetRaysFired() - Run->Rays0 >= 10 && Bh->GetVolleys() - Run->Volleys0 >= 2 && Bh->GetBites() - Run->Bites0 >= 1 && Bh->GetCones() - Run->Cones0 >= 1 && Bh->GetHitsTaken() > Run->Struck0;
            Note = FString::Printf(TEXT("rays=%d volleys=%d bites=%d cones=%d struck=%d hero_hits=%d dust=%d arrows_in=%d bolts_in=%d fizzled=%d"), Bh->GetRaysFired() - Run->Rays0, Bh->GetVolleys() - Run->Volleys0,
                Bh->GetBites() - Run->Bites0, Bh->GetCones() - Run->Cones0, Bh->GetHitsTaken() - Run->Struck0, Bh->GetHeroHits() - Run->HeroHits0, Bh->GetDisintegrated() - Run->Dust0,
                B->GetBeholderArrows(), B->GetBeholderBolts(), B->GetFizzled());
            break;
        }
        case 13:
        {
            // He works the seal (it knits), the tear surges and he strains against it, then the beholder's cone shatters it.
            ATempleSeal* Sl = B->GetSeal();
            ATempleBeholder* Bh = B->GetBeholder();
            if (!Sl) { bDone = true; break; }
            Run->SealMax = FMath::Max(Run->SealMax, Sl->GetCoverage());
            if (Run->Phase == 0 && Age > 10.f) { Run->Phase = 1; Sl->ForceSurge(); }
            if (Run->Phase == 1 && Age > 13.f && Bh) { Run->Phase = 2; Bh->OrderCone(Wz->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight) + (Wz->GetActorLocation() - Bh->GetActorLocation()).GetSafeNormal2D() * 300.f); }
            bDone = Age > 18.5f;
            bPass = Run->SealMax > .3f && Sl->GetSurges() > Run->Surges0 && Wz->GetStrains() > Run->Strains0 && Sl->GetBreaks() > Run->Breaks0 && Sl->GetCoverage() < .1f;
            Note = FString::Printf(TEXT("max_coverage=%.2f surges=%d strains=%d breaks=%d coverage_now=%.2f clip=%s"), Run->SealMax, Sl->GetSurges() - Run->Surges0,
                Wz->GetStrains() - Run->Strains0, Sl->GetBreaks() - Run->Breaks0, Sl->GetCoverage(), *Wz->GetClip().ToString());
            break;
        }
        case 14:
        {
            const int32 Pl = B->GetTalkPlaying();
            if (Pl >= 0 && Pl != Run->TalkLast)
            {
                Run->TalkOrdered &= Pl == Run->TalkLast + 1;
                Run->TalkLast = Pl;
                Run->TalkSounds += B->IsTalkSoundPlaying();
                FString Who, Cue;
                Run->TalkSubs += B->GetHeroSubtitle(Who, Cue) && !Cue.IsEmpty();
            }
            for (int32 H = 0; H < 3; ++H) Run->JawMax[H] = FMath::Max(Run->JawMax[H], B->GetHero(H)->GetJaw());
            if (Pl >= 0 && B->IsTalkSoundPlaying())
                if (const auto* Gm = Cast<ADockGameMode>(World->GetAuthGameMode())) Run->DuckMin = FMath::Min(Run->DuckMin, Gm->GetMusicDuck());
            const int32 N = ATempleSanctumBattle::TalkLineCount();
            bDone = (B->GetTalkStep() >= N && Pl < 0) || Age > 75.f;
            bPass = B->GetTalkSpoken() == N && Run->TalkLast == N - 1 && Run->TalkOrdered && Run->TalkSounds == N && Run->TalkSubs == N
                && Run->JawMax[0] > 3.f && Run->JawMax[1] > 3.f && Run->JawMax[2] > 3.f && Run->DuckMin < .5f;
            Note = FString::Printf(TEXT("spoken=%d/%d ordered=%d sounds=%d subtitles=%d jaw=%.1f/%.1f/%.1f duck_min=%.2f seconds=%.1f"), B->GetTalkSpoken(), N, Run->TalkOrdered,
                Run->TalkSounds, Run->TalkSubs, Run->JawMax[0], Run->JawMax[1], Run->JawMax[2], Run->DuckMin, Age);
            break;
        }
        case 19:
        {
            // On the gallery: the fighter's call (2 s at this scale), then nothing for 12 s. Then on the floor: the rest come.
            AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (Run->Sub == 0 && Age > 12.f) { Run->Sub = 1; Run->TalkLast = B->GetTalkSpoken(); if (Ch) Ch->ResetAtLocation(W(HallMid + 650, DaisT1 + 1900, HZ + 34.65f)); }
            bDone = (Run->Sub == 1 && B->GetTalkSpoken() >= 3) || Age > 30.f;
            bPass = Run->Sub == 1 && Run->TalkLast == 1 && B->GetTalkSpoken() >= 3;
            Note = FString::Printf(TEXT("spoken_on_gallery=%d spoken_after=%d on_floor=%d seconds=%.1f"), Run->TalkLast, B->GetTalkSpoken(), B->IsChuckOnFloor(), Age);
            if (bDone && Ch) Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum));
            break;
        }
        case 20:
        {
            ATempleSanctumScene* Sc = B->GetScene();
            AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (!Sc || !Ch) { bDone = true; Note = TEXT("no_scene"); break; }
            const FVector Fr = ChultTempleInteriorFrameOf(Ch->GetActorLocation());
            const bool bStarted = Sc->GetEntrances() > Run->Scene0[0];
            if (!bStarted)
            {
                // On toward the hall's far end (the balustrade stops him on the gallery).
                const FVector D = (W(78710, HT0, UZ) - W(78710, HT1, UZ)).GetSafeNormal2D();
                Ch->SetTestStickWorld(D); Ch->AddMovementInput(D, 1.f, true);
                if (Run->HallAt < 0 && Fr.Y < HT1 - 30) Run->HallAt = Now;
                if (const ATempleBeholder* Bh = B->GetBeholder()) Run->bFoughtEarly |= Age > .3f && Bh->IsFighting();   // (the first frames still show the last trial's forced fight)
                Run->bTalkEarly |= B->GetTalkSpoken() > Run->Spoken0;
            }
            else if (Run->SceneAt < 0) { Run->SceneAt = Now; Run->SceneFrom = Fr; }
            const FString ShotNow = Sc->GetShotName();
            if (!ShotNow.IsEmpty() && ShotNow != Run->SceneShot) { Run->SceneShot = ShotNow; ++Run->SceneShots; }
            bDone = (Run->SceneAt > 0 && !Sc->IsPlaying()) || Age > 40.f;
            const bool bBack = Ch->InputEnabled() && !Sc->IsPlaying();
            const float Wait = Run->SceneAt > 0 && Run->HallAt > 0 ? Run->SceneAt - Run->HallAt : -1.f;
            bPass = Run->SceneAt > 0 && Wait >= ATempleSanctumBattle::EntranceSettle - .05f && Run->SceneFrom.Y < HT1 - 100 && !Run->bFoughtEarly && !Run->bTalkEarly && Run->SceneShots >= 3 && bBack;
            Note = FString::Printf(TEXT("entrances=%d wait=%.2f depth=%.0f fought_early=%d talk_early=%d shots=%d control_back=%d seconds=%.1f"),
                Sc->GetEntrances() - Run->Scene0[0], Wait, HT1 - Run->SceneFrom.Y, Run->bFoughtEarly, Run->bTalkEarly, Run->SceneShots, bBack, Age);
            if (bDone)
            {
                B->SetForcedActive(true); B->SetEntranceScene(false);
                Ch->SetTestStick(FVector2D::ZeroVector);
                Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum)); Ch->SetActorHiddenInGame(true);
                if (auto* PCc = Cast<APlayerController>(Ch->GetController())) { PCc->SetViewTarget(Camera); Ch->DisableInput(PCc); }
            }
            break;
        }
        case 16: case 17: case 18:
        {
            ATempleSanctumScene* Sc = B->GetScene();
            AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (!Sc || !Ch) { bDone = true; Note = TEXT("no_scene"); break; }
            const FString ShotNow = Sc->GetShotName();
            if (!ShotNow.IsEmpty() && ShotNow != Run->SceneShot)
            {
                Run->SceneShot = ShotNow; ++Run->SceneShots;
                Run->NextShot = Now + (T.Kind == 16 ? 1.2f : .5f);   // a still a moment into each shot
                Run->Phase = 1;
            }
            if (bCapture && Run->Phase == 1 && Now >= Run->NextShot && !FScreenshotRequest::IsScreenshotRequested()) { Run->Phase = 0; Shot(FString::Printf(TEXT("Sanctum_%s_%s"), T.Name, *Run->SceneShot)); }
            if (bCapture && T.Kind == 17 && Sc->GetDetonations() > 0 && Sc->GetClock() - Sc->GetDetonateClock() > .35f && Run->EventAt < 0 && !FScreenshotRequest::IsScreenshotRequested()) { Run->EventAt = Now; Shot(TEXT("Sanctum_fireball_bloom")); }
            if (bCapture && Sc->GetThrows() > Run->Scene0[2] && Run->DeadAt < 0 && Sc->IsPlaying() && Sc->GetClock() - Sc->GetDetonateClock() > ATempleSanctumScene::BlastFadeIn + 1.6f && !FScreenshotRequest::IsScreenshotRequested()) { Run->DeadAt = Now; Shot(FString::Printf(TEXT("Sanctum_%s_rubble"), T.Name)); }
            if (T.Kind == 18 && !Run->bSkipSent && Sc->IsFireball() && Sc->GetClock() > 2.f) { Run->bSkipSent = true; Sc->Skip(); }
            bDone = (Age > 2.f && !Sc->IsPlaying() && (T.Kind == 16 || Sc->GetFireballs() > Run->Scene0[1])) || Age > 100.f;
            const bool bBack = Ch->InputEnabled() && !Sc->IsPlaying();
            if (T.Kind == 16)
            {
                bPass = Sc->GetEntrances() == Run->Scene0[0] + 1 && Run->SceneShots >= 3 && bBack;
                Note = FString::Printf(TEXT("entrances=%d shots=%d control_back=%d seconds=%.1f"), Sc->GetEntrances() - Run->Scene0[0], Run->SceneShots, bBack, Age);
            }
            else
            {
                const bool bRubble = InChultTempleRubble(Ch->GetActorLocation());
                bPass = Sc->GetFireballs() == Run->Scene0[1] + 1 && Sc->GetThrows() == Run->Scene0[2] + 1 && Sc->GetSanityAfter() == AChuckCharacter::MaxSanity / 2
                    && Ch->GetSanity() == AChuckCharacter::MaxSanity / 2 && bRubble && bBack
                    && (T.Kind == 18 ? Sc->GetSkips() == Run->Scene0[3] + 1 : Sc->GetDetonations() > 0 && Run->SceneShots >= 6 && Sc->GetMostFlares() > 100);
                Note = FString::Printf(TEXT("fireballs=%d detonations=%d throws=%d skips=%d sanity_after=%d sanity=%d in_rubble=%d control_back=%d shots=%d most_flares=%d detonate_clock=%.2f seconds=%.1f"),
                    Sc->GetFireballs() - Run->Scene0[1], Sc->GetDetonations(), Sc->GetThrows() - Run->Scene0[2], Sc->GetSkips() - Run->Scene0[3], Sc->GetSanityAfter(), Ch->GetSanity(), bRubble, bBack,
                    Run->SceneShots, Sc->GetMostFlares(), Sc->GetDetonateClock(), Age);
            }
            if (bDone)
            {
                // Back to the trials' own view and a hidden Chuck at the gallery.
                B->SetFireballScene(false);
                Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum)); Ch->SetActorHiddenInGame(true);
                if (auto* PCc = Cast<APlayerController>(Ch->GetController())) { PCc->SetViewTarget(Camera); Ch->DisableInput(PCc); }
                Run->DeadAt = -1; Run->EventAt = -1;
            }
            break;
        }
        default:
            bDone = Age > 45.f;
            bPass = B->GetFallen() - Run->Fallen0 >= 3 && F->GetCleanHits() > Run->Hits0;
            Note = FString::Printf(TEXT("fallen=%d decap=%d fighter_hits=%d blocks=%d taken=%d up=%d sounds=%d"), B->GetFallen() - Run->Fallen0, B->GetDecapitations() - Run->Decap0,
                F->GetCleanHits() - Run->Hits0, F->GetBlocks() - Run->Blocks0, F->GetHitsTaken() - Run->Taken0, B->GetUp(), B->GetSoundsPlayed());
            break;
        }
        if (!bDone) return;
        if (!bPass) ++Run->Failures;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_TRIAL trial=%s pass=%d %s"), T.Name, bPass ? 1 : 0, *Note);
        ++Run->Trial; Run->Start = -1; Run->Phase = 0; Run->DeadAt = -1;
        F->SetHeld(false); F->ForceClip(NAME_None);
        if (T.Kind == 14) if (AChuckCharacter* Ch = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))) Ch->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Sanctum));
        if (Run->Trial >= Count)
        {
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_TEST_COMPLETE failures=%d evaluations=%d/%d/%d"), Run->Failures, F->GetAnimEvaluations(), Wz->GetAnimEvaluations(), Rg->GetAnimEvaluations());
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }, 1.f / 60.f, true);
}
