// The sanctum's cutscenes: the entrance and the Fireball (Claude, 2026-10-26; TempleSanctumScene.h,
// docs/CHULT-TEMPLE-BEHOLDER.md#the-cutscenes).
#include "TempleSanctumScene.h"
#include "ChuckCharacter.h"
#include "ChultTempleInterior.h"
#include "ChultTempleWind.h"
#include "DockGameMode.h"
#include "HeroClipData.h"
#include "TempleBeholder.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    using namespace TempleWind;
    constexpr int32 FlarePool = 240, SparkPool = 420;
    const FTransform Hidden(FQuat::Identity, FVector(0, 0, -1e6), FVector(.001f));
    const FLinearColor White(1.f, .97f, .9f);
    FVector W(float S, float T, float Z) { return ChultTempleInteriorWorldOf(FVector(S, T, Z)); }
    float Ease(float U) { U = FMath::Clamp(U, 0.f, 1.f); return U * U * (3.f - 2.f * U); }

    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Zv = Eye - P; if (!Zv.Normalize()) Zv = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Zv.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Zv).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Zv, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        return FTransform(FMatrix(X0 * C + Y0 * S, Y0 * C - X0 * S, Zv, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
    // Fire's colour as it cools: white-yellow at the heart to deep orange.
    FLinearColor FireCol(float Heat) { return FMath::Lerp(FLinearColor(.9f, .26f, .04f), FLinearColor(1.f, .7f, .32f), FMath::Clamp(Heat, 0.f, 1.f)); }
}

ATempleSanctumScene::ATempleSanctumScene()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto MakeFx = [this](const TCHAR* Name, int32 Floats)
    {
        auto* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        C->SetupAttachment(RootComponent);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->NumCustomDataFloats = Floats;
        C->SetMobility(EComponentMobility::Movable);
        return C;
    };
    FlareFx = MakeFx(TEXT("Fire"), 5);
    SparkFx = MakeFx(TEXT("Sparks"), 4);
    auto MakeLight = [this](const TCHAR* Name, const FLinearColor& Col)
    {
        auto* L = CreateDefaultSubobject<UPointLightComponent>(Name);
        L->SetupAttachment(RootComponent); L->SetIntensity(0.f); L->SetCastShadows(false); L->SetLightColor(Col);
        L->SetLightingChannels(true, true, false);   // (some of the sanctum's floor is on the default channel)
        return L;
    };
    BlastLight = MakeLight(TEXT("BlastLight"), FLinearColor(1.f, .55f, .22f));
    FrontLight = MakeLight(TEXT("FrontLight"), FLinearColor(1.f, .5f, .18f));
    GemLight = MakeLight(TEXT("GemLight"), FLinearColor(1.f, .5f, .15f));
    AstralLight = MakeLight(TEXT("AstralLight"), FLinearColor(.55f, .1f, 1.f));
}

void ATempleSanctumScene::Setup(ATempleSanctumBattle* InBattle)
{
    Battle = InBattle;
    Random.Initialize(20261026);
    UStaticMesh* Plane = InBattle ? InBattle->GetPlaneMesh() : nullptr;
    UMaterialInterface* SparkMat = InBattle ? InBattle->GetBeholderFxMaterial(2) : nullptr;
    UMaterialInterface* FlareMat = InBattle ? InBattle->GetBeholderFxMaterial(3) : nullptr;
    if (Plane && SparkMat && FlareMat)
    {
        FlareFx->SetStaticMesh(Plane); FlareFx->SetMaterial(0, FlareMat);
        SparkFx->SetStaticMesh(Plane); SparkFx->SetMaterial(0, SparkMat);
        TArray<FTransform> X; X.Init(Hidden, FlarePool); FlareFx->AddInstances(X, false, true);
        X.Init(Hidden, SparkPool); SparkFx->AddInstances(X, false, true);
        bReady = true;
    }
    Camera = GetWorld()->SpawnActor<ACameraActor>(GetActorLocation(), FRotator::ZeroRotator);
    if (Camera) { Camera->GetCameraComponent()->bConstrainAspectRatio = false; Camera->GetCameraComponent()->SetFieldOfView(50.f); }
    BlastLight->SetAttenuationRadius(7000.f); FrontLight->SetAttenuationRadius(2600.f); GemLight->SetAttenuationRadius(700.f); AstralLight->SetAttenuationRadius(4000.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_SCENE_READY fx=%d camera=%d"), bReady ? 1 : 0, Camera != nullptr);
}

ATempleHero* ATempleSanctumScene::Hero(int32 I) const { return Battle.IsValid() ? Battle->GetHero(I) : nullptr; }

void ATempleSanctumScene::Begin(EKind InKind)
{
    Kind = InKind; Clock = 0; ShotClock = 0; Shot = TEXT(""); bCut = true; bReleased = false; bThrown = false; bSkipping = false;
    DetonateAt = CastAt = BeadAt = SkipAt = -1; bCastPlayed = bGatherSound = false; WaitClock = 0;
    Flares.Reset(); Motes.Reset();
    AChuckCharacter* C = Chuck.Get();
    if (auto* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr)
    {
        C->DisableInput(PC); C->SetTestStick(FVector2D::ZeroVector); C->SetRunHeld(false);
        if (Camera) PC->SetViewTarget(Camera);
    }
    Respawns0 = C ? C->GetRespawns() : 0;
}

void ATempleSanctumScene::Release()
{
    if (bReleased) return;
    bReleased = true;
    AChuckCharacter* C = Chuck.Get();
    if (auto* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr)
    {
        PC->SetViewTargetWithBlend(C, Kind == EKind::Entrance && !bSkipping ? .8f : 0.f, VTBlend_EaseInOut, 2.f);
        C->EnableInput(PC);
    }
}

void ATempleSanctumScene::Finish()
{
    Release();
    if (ATempleSanctumBattle* B = Battle.Get())
    {
        B->SetBeholderFights(true); B->SetWaves(true);
        if (ATempleBeholder* Bh = B->GetBeholder()) { Bh->HoldGaze(FVector::ZeroVector); Bh->OrderHover(FVector::ZeroVector); }
    }
    for (int32 I = 0; I < 3; ++I) if (ATempleHero* H = Hero(I)) H->EndScript();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_SCENE_END kind=%d clock=%.2f skipped=%d"), static_cast<int32>(Kind), Clock, bSkipping ? 1 : 0);
    Kind = EKind::None; Shot = TEXT("");
    Flares.Reset(); Motes.Reset();
    TickFx(0.f);
    for (UPointLightComponent* L : {BlastLight, FrontLight, GemLight, AstralLight}) L->SetIntensity(0.f);
}

bool ATempleSanctumScene::StartEntrance()
{
    Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (IsPlaying() || !Chuck.IsValid() || !Camera || !Hero(0)) return false;
    Begin(EKind::Entrance);
    ++Entrances;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_ENTRANCE_START n=%d at=%s"), Entrances, *ChultTempleInteriorFrameOf(Chuck->GetActorLocation()).ToString());
    return true;
}

bool ATempleSanctumScene::StartFireball()
{
    Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    ATempleSanctumBattle* B = Battle.Get();
    ATempleBeholder* Bh = B ? B->GetBeholder() : nullptr;
    if (IsPlaying() || !Chuck.IsValid() || !Camera || !B || !Bh || !Hero(1)) return false;
    // Not mid-attack (a cone flinging him while the view is elsewhere), and not while he is in the air.
    if (Bh->IsBusy() || Chuck->IsAstral()) return false;   // (asked again next frame: its moves end within seconds)
    Begin(EKind::Fireball);
    ++Fireballs;
    B->SetBeholderFights(false); B->SetWaves(false);
    Bh->OrderHover(B->BeholderSpot());
    // The wizard turns from the sea tear to the beholder and draws the sigils.
    if (ATempleHero* Wz = Hero(1)) { Wz->Script(TEXT("Channel"), true, .4f, Bh->GetCentre()); Bh->HoldGaze(Wz->HeadLocation()); }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_FIREBALL_START n=%d chuck=%s sanity=%d"), Fireballs, *ChultTempleInteriorFrameOf(Chuck->GetActorLocation()).ToString(), Chuck->GetSanity());
    return true;
}

void ATempleSanctumScene::Skip()
{
    if (!IsPlaying() || bSkipping || bReleased) return;
    bSkipping = true; ++Skips; SkipAt = Clock;
    APlayerController* PC = Chuck.IsValid() ? Cast<APlayerController>(Chuck->GetController()) : nullptr;
    if (Kind == EKind::Entrance)
    {
        if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .25f, FLinearColor::Black, false, true);
    }
    else if (DetonateAt < 0 || Clock - DetonateAt < BlastWhite)
    {
        // Straight to the white: the blast is taken as having happened.
        if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .3f, White, false, true);
        if (DetonateAt < 0)
        {
            if (ATempleBeholder* Bh = Battle.IsValid() ? Battle->GetBeholder() : nullptr) BlastAt = Bh->GetCentre();
            if (Chuck.IsValid()) ChuckAt = Chuck->GetActorLocation();
        }
        if (DetonateAt < 0 && Battle.IsValid())
            if (ADockGameMode* Mode = GetWorld()->GetAuthGameMode<ADockGameMode>()) { Mode->FadeOutMusic(.3f); Mode->HoldMusic(9.f); }
        DetonateAt = Clock - BlastWhite + .3f;
        Flares.Reset(); Motes.Reset();
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_SCENE_SKIP kind=%d at=%.2f"), static_cast<int32>(Kind), Clock);
}

void ATempleSanctumScene::Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov)
{
    if (!Camera) return;
    if (FCString::Strcmp(Name, Shot) != 0) { bCut = true; Shot = Name; ShotClock = 0; UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_SCENE_SHOT kind=%d shot=%s clock=%.2f"), static_cast<int32>(Kind), Name, Clock); }
    const float Dt = GetWorld()->GetDeltaSeconds();
    // Never inside the beholder (it lunges and drifts): out of a sphere round it.
    FVector Want = InEye;
    if (const ATempleBeholder* Bh = Battle.IsValid() ? Battle->GetBeholder() : nullptr)
    {
        const FVector Off = Want - Bh->GetCentre();
        if (Off.Size() < 300.f) Want = Bh->GetCentre() + (Off.IsNearlyZero() ? FVector::UpVector : Off.GetSafeNormal()) * 300.f;
    }
    Eye = bCut ? Want : FMath::VInterpTo(Eye, Want, Dt, 7.f);
    Aim = bCut ? InAim : FMath::VInterpTo(Aim, InAim, Dt, 7.f);
    bCut = false;
    Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void ATempleSanctumScene::Tick(float Dt)
{
    Super::Tick(Dt);
    if (Kind == EKind::None) return;
    Clock += Dt; ShotClock += Dt;
    AChuckCharacter* C = Chuck.Get();
    if (!C || !Battle.IsValid()) { Finish(); return; }
    if (APlayerController* PC = Cast<APlayerController>(C->GetController()))
        if (!bReleased && PC->WasInputKeyJustPressed(EKeys::Escape)) Skip();
    if (Kind == EKind::Entrance) TickEntrance(Dt);
    else TickFireball(Dt);
    TickFx(Dt);
}

// ---------------------------------------------------------------- the entrance
void ATempleSanctumScene::TickEntrance(float Dt)
{
    AChuckCharacter* C = Chuck.Get();
    ATempleSanctumBattle* B = Battle.Get();
    ATempleBeholder* Bh = B->GetBeholder();
    ATempleHero* F = Hero(0);
    if (bSkipping)
    {
        if (Clock - SkipAt > .28f)
        {
            if (APlayerController* PC = Cast<APlayerController>(C->GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, .45f, FLinearColor::Black, false, false);
            Finish();
        }
        return;
    }
    const FVector Centre = Bh ? Bh->GetCentre() : B->BeholderSpot();
    const FVector Me = C->GetActorLocation();
    const FVector In = (W(0, -1, 0) - W(0, 0, 0)).GetSafeNormal();   // -T: from the gallery into the hall
    if (Clock < EntranceHall)
    {
        // Over his shoulder on the gallery, rising to the balustrade, down into the hall at the beholder.
        const float U = Ease(Clock / EntranceHall);
        const FVector F0 = ChultTempleInteriorFrameOf(Me);
        const FVector E0 = Me - In * 200.f + FVector(0, 0, 175.f);   // (over the balustrade from the start)
        const FVector E1 = W(F0.X, GalleryT + 70, UZ + 230);
        Frame(FMath::Lerp(E0, E1, U), FMath::Lerp(Me + In * 500.f - FVector(0, 0, 120), Centre, U), TEXT("EntranceHall"), FMath::Lerp(58.f, 48.f, U));
    }
    else if (Clock < EntranceFloor)
    {
        // Low on the floor behind the heroes, up at it over the dais, drifting across.
        const float U = Ease((Clock - EntranceHall) / (EntranceFloor - EntranceHall));
        Frame(FMath::Lerp(W(HallMid + 680, DaisT1 + 1750, HZ + 60), W(HallMid + 420, DaisT1 + 1520, HZ + 75), U), Centre - FVector(0, 0, 60), TEXT("EntranceFloor"), 55.f);
    }
    else if (Clock < EntranceEnd && F)
    {
        // The fighter, from before him and a little low, as he calls out to the others.
        const FVector Head = F->HeadLocation(), Fw = F->GetActorForwardVector().GetSafeNormal2D(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
        const float U = Ease((Clock - EntranceFloor) / (EntranceEnd - EntranceFloor));
        Frame(Head + Rt * FMath::Lerp(260.f, 230.f, U) + Fw * 110.f - FVector(0, 0, 15), Head + Fw * 30.f - FVector(0, 0, 8), TEXT("EntranceFighter"), 44.f);
    }
    else Finish();
}

// ---------------------------------------------------------------- the Fireball
void ATempleSanctumScene::Sigils(const FVector& Feet, const FVector& Tip, float Strength)
{
    // Drawn as sparks each frame (not particles): two rings on the flags round him with runes between them, turning
    // slowly; a small ring turning fast round his staff's head.
    if (Strength <= .01f) return;
    const float T = Clock;
    const FLinearColor Col = FLinearColor(1.f, .32f, .05f) * (.55f * Strength);
    auto Pt = [&](const FVector& P, float Size) { if (SigilPts.Num() < 300) SigilPts.Add({P, FVector::ZeroVector, Col, Size * .6f, 0.f, 1.f, 0.f}); };
    for (int32 Ring = 0; Ring < 2; ++Ring)
    {
        const float R = Ring ? 150.f : 112.f, Turn = (Ring ? -.25f : .35f) * T;
        const int32 N = Ring ? 96 : 76;
        for (int32 K = 0; K < N; ++K) { const float A = Turn + K * UE_TWO_PI / N; Pt(Feet + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 2.f), 3.2f); }
    }
    for (int32 G = 0; G < 12; ++G)
    {
        // A rune: a stroke across the band and a hooked stroke beside it, its own shape by its index.
        const float A = .1f * T + G * UE_TWO_PI / 12;
        const FVector Out(FMath::Cos(A), FMath::Sin(A), 0), Side(-Out.Y, Out.X, 0);
        const FVector Mid = Feet + Out * 131.f + FVector(0, 0, 2.f);
        for (int32 S = -2; S <= 2; ++S) Pt(Mid + Out * (S * 4.f) + Side * ((G % 3) - 1) * 3.f, 2.6f);
        for (int32 S = 0; S < 3; ++S) Pt(Mid + Side * (6.f + S * 3.f) * ((G % 2) ? 1.f : -1.f) + Out * (S == 2 ? 4.f : 0.f), 2.4f);
    }
    for (int32 K = 0; K < 16; ++K)
    {
        const float A = 2.6f * T + K * UE_TWO_PI / 16;
        Pt(Tip + FVector(FMath::Cos(A) * 20.f, FMath::Sin(A) * 20.f, FMath::Sin(A * 2.f) * 5.f), 1.8f);
    }
}

void ATempleSanctumScene::TickFireball(float Dt)
{
    AChuckCharacter* C = Chuck.Get();
    ATempleSanctumBattle* B = Battle.Get();
    ATempleBeholder* Bh = B->GetBeholder();
    ATempleHero* F = Hero(0); ATempleHero* Wz = Hero(1); ATempleHero* Rg = Hero(2);
    if (!Bh || !Wz) { Finish(); return; }
    // Dead and back at a checkpoint before the blast (it cannot happen with his control off, but): give up the scene.
    if (C->GetRespawns() != Respawns0 && !bThrown) { Finish(); return; }
    const FVector Centre = Bh->GetCentre();
    const int32 Playing = B->GetTalkPlaying(), Step = B->GetTalkStep();
    const FVector WzFeet = Wz->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight - 2.f);
    const FVector Tip = Wz->GetMesh()->GetBoneLocation(TEXT("weapon_tip"));
    const float Since = DetonateAt >= 0 ? Clock - DetonateAt : -1.f;

    if (DetonateAt < 0)
    {
        // Before the blast: the argument, shot by who speaks; the sigils grow with it.
        const int32 Line = Playing >= 0 ? Playing : Step - 1;   // between lines, the last speaker's shot holds
        Sigils(WzFeet, Tip, FMath::Clamp(Clock / 2.5f, 0.f, 1.f) * (Line >= 6 ? 1.6f : 1.f));
        const FVector ToBh = (Centre - Wz->GetActorLocation()).GetSafeNormal2D(), Rt = FVector::CrossProduct(FVector::UpVector, ToBh);
        const float Gather = FCString::Strcmp(Shot, TEXT("FireballGather")) == 0 ? ShotClock : 0.f;   // (the shot's own time: 0 on its first frame)
        GemLight->SetWorldLocation(Tip); GemLight->SetIntensity(1500.f + (Line >= 6 ? 9000.f * FMath::Clamp(Gather / 2.f, 0.f, 1.f) : 0.f) + 400.f * FMath::Sin(Clock * 11.f));
        if (Line <= 3 && Rg)
        {
            // The ranger, over the wizard's shoulder: she knows those sigils.
            const FVector Away = (Wz->GetActorLocation() - Rg->GetActorLocation()).GetSafeNormal2D();
            Frame(Wz->HeadLocation() + Away * 150.f + FVector::CrossProduct(FVector::UpVector, Away) * 55.f + FVector(0, 0, 18), Rg->HeadLocation() - FVector(0, 0, 10), TEXT("FireballRanger"), 40.f);
        }
        else if (Line == 4 && F)
        {
            // The fighter, shield up toward it, looking back over his shoulder at the rift.
            const FVector Head = F->HeadLocation(), Fw = F->GetActorForwardVector().GetSafeNormal2D(), FR = FVector::CrossProduct(FVector::UpVector, Fw);
            Frame(Head - FR * 240.f + Fw * 60.f + FVector(0, 0, 10), Head + Fw * 25.f - FVector(0, 0, 6), TEXT("FireballFighter"), 42.f);
        }
        else if (Line == 5)
        {
            // The wizard, close, from the beholder's side: he has to try.
            Frame(Wz->HeadLocation() + ToBh * 175.f + Rt * 45.f - FVector(0, 0, 22), Wz->HeadLocation() - FVector(0, 0, 6), TEXT("FireballWizard"), 38.f);
        }
        else if (Line == 6)
        {
            // "......": a mote of fire swells at his staff's head, the sigils blaze; the view closes in on it.
            const float U = Ease(Gather / 2.2f);
            Frame(Tip + Rt * FMath::Lerp(190.f, 95.f, U) + ToBh * FMath::Lerp(60.f, 30.f, U) - FVector(0, 0, 20), Tip, TEXT("FireballGather"), 40.f);
            if (!bGatherSound) { bGatherSound = true; B->Sound(ATempleSanctumBattle::ESound::BeholderConeCharge, Tip, .8f, .7f); }
            if (Random.FRand() < .7f) Flare(Tip + FMath::VRand() * 3.f, FVector(0, 0, 10), FireCol(.35f), 8.f + 34.f * U, 12.f, .3f, 4.f);
            for (int32 K = 0; K < 3; ++K) { const FVector From = Tip + FMath::VRand() * Random.FRandRange(40.f, 90.f); Mote(From, (Tip - From) * 3.f, FireCol(.2f) * .6f, 1.6f, .3f, 0.f); }
        }
        else
        {
            // "FIREBALL!!": over his shoulder at the beholder as he throws it.
            Frame(Wz->HeadLocation() - ToBh * 175.f + Rt * 75.f + FVector(0, 0, 25), FMath::Lerp(Wz->HeadLocation() + ToBh * 300.f, Centre, .75f), TEXT("FireballCast"), 46.f);
            if (Playing == 7 && CastAt < 0) CastAt = Clock + .3f;
            if (CastAt >= 0 && !bCastPlayed && Clock >= CastAt) { bCastPlayed = true; Wz->Script(TEXT("Cast"), false, .08f, Centre); }
            if (bCastPlayed && BeadAt < 0 && Clock >= CastAt + HeroClipData::Wizard::CastFire)
            {
                BeadAt = Clock; BeadPos = Wz->GetMesh()->GetBoneLocation(TEXT("hand_l")) + Wz->GetActorForwardVector() * 14.f;
                B->Sound(ATempleSanctumBattle::ESound::BoltCast, BeadPos, 1.f, .7f);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_FIREBALL_THROWN clock=%.2f"), Clock);
            }
            if (BeadAt >= 0)
            {
                const FVector To = Centre - BeadPos;
                const float Dist = static_cast<float>(To.Size());
                BeadPos += To.GetSafeNormal() * FMath::Min(Dist, BeadSpeed * Dt);
                Flare(BeadPos, -To.GetSafeNormal() * 200.f, FireCol(1.f), 34.f, 30.f, .22f, 3.f);
                for (int32 K = 0; K < 3; ++K) Mote(BeadPos, -To.GetSafeNormal() * 300.f + FMath::VRand() * 120.f, FireCol(.8f), 3.f, .4f, 2.f);
                GemLight->SetWorldLocation(BeadPos); GemLight->SetIntensity(14000.f);
                if (Dist < ATempleBeholder::BodyRadius + 10.f) Detonate();
            }
        }
        // However the lines went, it is thrown in the end.
        if (DetonateAt < 0 && Clock > 45.f) Detonate();
        return;
    }

    // ---- After the blast.
    GemLight->SetIntensity(0.f);
    if (!bSkipping)
    {
        const float Light = Since < .12f ? Since / .12f : FMath::Exp(-(Since - .12f) * 1.6f);
        BlastLight->SetWorldLocation(BlastAt); BlastLight->SetIntensity(320000.f * Light);
        AstralLight->SetIntensity(Since > .3f ? 120000.f * FMath::Exp(-(Since - .3f) * 1.2f) : 0.f);
        // The fireball: its shell racing out round the beholder through the first second.
        if (Since < 1.f)
        {
            const float R = 1150.f * (1.f - FMath::Exp(-3.2f * Since));
            for (int32 K = 0; K < 5; ++K)
            {
                const FVector D = FMath::VRand(); const FVector P = BlastAt + D * R * Random.FRandRange(.8f, 1.f);
                Flare(P, D * 500.f, FireCol(Random.FRandRange(.1f, .6f)), Random.FRandRange(180.f, 360.f), 260.f, Random.FRandRange(.9f, 1.5f), 1.6f, 140.f);
            }
        }
        // The Astral under the floor erupts with it: purple fire up out of the pool and the ruptures.
        if (Since > .3f && Since < .9f)
            for (int32 K = 0; K < 2; ++K)
            {
                const FVector P = W(Random.FRandRange(PoolS0 + 100, PoolS1 - 100), Random.FRandRange(PoolT + 50, HT1 - 300), HZ - 40);
                Flare(P, FVector(Random.FRandRange(-80.f, 80.f), Random.FRandRange(-80.f, 80.f), Random.FRandRange(700.f, 1300.f)), FLinearColor(.6f, .16f, 1.f), Random.FRandRange(120.f, 220.f), 220.f, Random.FRandRange(1.f, 1.6f), 1.2f);
            }
        // The blast rolls across the hall at him.
        if (Since > BlastWide - .25f && Since < BlastWhite + .1f)
        {
            const float U = Ease((Since - (BlastWide - .25f)) / (BlastWhite - BlastWide + .25f));
            const FVector Dir = (ChuckAt - BlastAt).GetSafeNormal();
            const FVector Front = FMath::Lerp(BlastAt, ChuckAt - Dir * 60.f, U);
            const FVector S1 = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal(), S2 = FVector::CrossProduct(S1, Dir);
            for (int32 K = 0; K < 7; ++K)
            {
                const float A = Random.FRandRange(0.f, UE_TWO_PI), Rr = 480.f * FMath::Sqrt(Random.FRand());
                Flare(Front + (S1 * FMath::Cos(A) + S2 * FMath::Sin(A)) * Rr, Dir * 700.f, FireCol(Random.FRandRange(.2f, .8f)), Random.FRandRange(260.f, 420.f), 300.f, Random.FRandRange(.5f, .8f), 1.f);
            }
            FrontLight->SetWorldLocation(Front); FrontLight->SetIntensity(120000.f);
        }
        else FrontLight->SetIntensity(0.f);
    }
    if (Since < BlastWide)
    {
        // Wide, from the hall's side: it blooms round the beholder, the heroes small against it.
        Frame(W(HallMid + 560, DaisT1 + 2250, HZ + 200), FMath::Lerp(Centre, W(HallMid, DaisT1 + 600, HZ + 160), .3f), TEXT("FireballBlast"), 64.f);
    }
    else if (Since < BlastThrow)
    {
        // At him: behind him and low, the fire coming at the lens past him.
        const FVector Dir = (ChuckAt - BlastAt).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Dir);
        Frame(ChuckAt + Dir * 190.f + Side * 45.f + FVector(0, 0, 30), ChuckAt - Dir * 500.f + FVector(0, 0, 60), TEXT("FireballChuck"), 58.f);
        if (!bSkipping && Since - Dt < BlastWide)
        {
            B->Sound(ATempleSanctumBattle::ESound::BeholderConeBlast, ChuckAt - Dir * 400.f, 1.f, .45f);
            BeholderShake(GetWorld(), ChuckAt, 1.6f, 3000.f, 1);
        }
    }
    if (Since >= BlastWhite && Since - Dt < BlastWhite && !bSkipping)
        if (APlayerController* PC = Cast<APlayerController>(C->GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .22f, White, false, true);
    if (Since >= BlastThrow && !bThrown) Throw();
    if (bThrown && Since >= BlastFadeIn && Since - Dt < BlastFadeIn)
        if (APlayerController* PC = Cast<APlayerController>(C->GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.6f, White, false, false);
    if (bThrown && Since >= BlastControl) Release();
    if (Since >= BlastEnd) Finish();
}

void ATempleSanctumScene::Detonate()
{
    ATempleSanctumBattle* B = Battle.Get();
    ATempleBeholder* Bh = B ? B->GetBeholder() : nullptr;
    AChuckCharacter* C = Chuck.Get();
    if (!B || !Bh || !C || DetonateAt >= 0) return;
    DetonateAt = Clock; ++Detonations;
    BlastAt = Bh->GetCentre();
    ChuckAt = C->GetActorLocation();
    // The heart of it: a ball of fire round the beholder, swelling and rolling up.
    for (int32 K = 0; K < 70; ++K)
    {
        const FVector D = FMath::VRand();
        Flare(BlastAt + D * Random.FRandRange(0.f, 80.f), D * Random.FRandRange(300.f, 1500.f), FireCol(Random.FRandRange(.4f, 1.f)), Random.FRandRange(120.f, 260.f), Random.FRandRange(500.f, 900.f), Random.FRandRange(1.4f, 2.4f), 2.2f, 60.f);
    }
    for (int32 K = 0; K < 120; ++K) Mote(BlastAt, FMath::VRand() * Random.FRandRange(600.f, 2200.f), FireCol(Random.FRandRange(.5f, 1.f)), Random.FRandRange(3.f, 7.f), Random.FRandRange(.6f, 1.6f), 1.2f);
    B->Sound(ATempleSanctumBattle::ESound::BeholderConeBlast, BlastAt, 1.f, .62f);
    B->Sound(ATempleSanctumBattle::ESound::BoltImpact, BlastAt, 1.f, .5f);
    B->Sound(ATempleSanctumBattle::ESound::BeholderHurt, BlastAt, 1.f, .85f);
    Bh->TakeHit(BlastAt, (BlastAt - Hero(1)->GetActorLocation()).GetSafeNormal(), 4.f);
    BeholderShake(GetWorld(), BlastAt, 2.f, 6000.f, 1);
    if (ADockGameMode* Mode = GetWorld()->GetAuthGameMode<ADockGameMode>()) { Mode->FadeOutMusic(.25f); Mode->HoldMusic(10.f); }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_FIREBALL_BLAST clock=%.2f at=%s chuck=%s distance=%.0f"), Clock, *ChultTempleInteriorFrameOf(BlastAt).ToString(), *ChultTempleInteriorFrameOf(ChuckAt).ToString(), FVector::Dist(BlastAt, ChuckAt));
}

void ATempleSanctumScene::Throw()
{
    bThrown = true; ++Throws;
    AChuckCharacter* C = Chuck.Get();
    APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
    // The blast's toll, as the 2D game: at most half his Sanity (it never heals him). Then into the rubble, where he
    // comes to as the white lifts.
    const int32 Kept = C ? FMath::Min(C->GetSanity(), AChuckCharacter::MaxSanity / 2) : 0;
    if (C)
    {
        C->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Rubble));
        C->SetActorRotation(FRotator(0, ChultTempleInteriorSpawnYaw(ETempleInteriorSpawn::Rubble), 0));
        C->Recenter();
        C->SetSanity(Kept);
        SanityAfter = C->GetSanity();
    }
    if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(1.f, White, false);   // (the reset lifts it)
    if (PC && C) PC->SetViewTarget(C);
    bReleased = false;   // still his view, but no control until the white lifts
    Flares.Reset(); Motes.Reset();
    for (UPointLightComponent* L : {BlastLight, FrontLight, GemLight, AstralLight}) L->SetIntensity(0.f);
    // The hall goes back to its fight, as it was when he came in, for whoever comes back to it.
    if (ATempleSanctumBattle* B = Battle.Get())
    {
        B->SetBeholderFights(true); B->SetWaves(true); B->RestartTalk();
        if (ATempleBeholder* Bh = B->GetBeholder()) { Bh->HoldGaze(FVector::ZeroVector); Bh->OrderHover(FVector::ZeroVector); }
    }
    for (int32 I = 0; I < 3; ++I) if (ATempleHero* H = Hero(I)) H->EndScript();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_FIREBALL_THROW sanity=%d at=%s skipped=%d"), SanityAfter, C ? *ChultTempleInteriorFrameOf(C->GetActorLocation()).ToString() : TEXT("none"), bSkipping ? 1 : 0);
}

// ---------------------------------------------------------------- fire and sparks
void ATempleSanctumScene::Flare(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Grow, float Life, float Drag, float Rise)
{
    if (Flares.Num() >= FlarePool) Flares.RemoveAt(0, 1, EAllowShrinking::No);
    Flares.Add({At, Vel, Col, Size, Grow, 0.f, Life, static_cast<float>(Random.FRandRange(0.f, 8.f)), 64.f / Life, static_cast<float>(Random.FRandRange(0.f, 6.28f)), Drag, Rise});
    MostFlares = FMath::Max(MostFlares, Flares.Num());
}

void ATempleSanctumScene::Mote(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Drag)
{
    if (Motes.Num() >= SparkPool) Motes.RemoveAt(0, 1, EAllowShrinking::No);
    Motes.Add({At, Vel, Col, Size, 0.f, Life, Drag});
}

void ATempleSanctumScene::TickFx(float Dt)
{
    if (!bReady) return;
    for (FFlareP& P : Flares) { P.Age += Dt; P.Vel *= FMath::Exp(-P.Drag * Dt); P.Vel.Z += P.Rise * Dt; P.Pos += P.Vel * Dt; P.Size += P.Grow * Dt; P.Frame += P.Rate * Dt; }
    Flares.RemoveAll([](const FFlareP& P) { return P.Age > P.Life; });
    for (FMoteP& M : Motes) { M.Age += Dt; M.Vel *= FMath::Exp(-M.Drag * Dt); M.Pos += M.Vel * Dt; }
    Motes.RemoveAll([](const FMoteP& M) { return M.Age > M.Life; });
    FVector CamEye = Eye;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) if (PC->PlayerCameraManager) CamEye = PC->PlayerCameraManager->GetCameraLocation();
    {
        TArray<FTransform> X; X.Init(Hidden, FlarePool);
        for (int32 I = 0; I < FlarePool; ++I)
        {
            float D[5] = {0, 0, 0, 0, 0};
            if (I < Flares.Num())
            {
                const FFlareP& P = Flares[I];
                const float U = P.Age / P.Life;
                X[I] = Billboard(P.Pos, CamEye, P.Spin, P.Size);
                D[0] = P.Frame; D[1] = P.Col.R; D[2] = P.Col.G; D[3] = P.Col.B; D[4] = FMath::Clamp(U / .08f, 0.f, 1.f) * (1.f - U);
            }
            FlareFx->SetCustomData(I, MakeArrayView(D, 5), false);
        }
        FlareFx->BatchUpdateInstancesTransforms(0, X, true, true);
    }
    {
        TArray<FTransform> X; X.Init(Hidden, SparkPool);
        for (int32 I = 0; I < SparkPool; ++I)
        {
            float D[4] = {0, 0, 0, 0};
            if (I < SigilPts.Num())
            {
                const FMoteP& M = SigilPts[I];
                X[I] = Billboard(M.Pos, CamEye, 0.f, M.Size);
                D[0] = M.Col.R; D[1] = M.Col.G; D[2] = M.Col.B; D[3] = .85f + .15f * FMath::Sin(Clock * 17.f + I);
            }
            else if (I - SigilPts.Num() < Motes.Num())
            {
                const FMoteP& M = Motes[I - SigilPts.Num()];
                const float U = M.Age / M.Life;
                X[I] = Billboard(M.Pos, CamEye, 0.f, M.Size);
                D[0] = M.Col.R; D[1] = M.Col.G; D[2] = M.Col.B; D[3] = FMath::Clamp(U / .08f, 0.f, 1.f) * (1.f - U);
            }
            SparkFx->SetCustomData(I, MakeArrayView(D, 4), false);
        }
        SparkFx->BatchUpdateInstancesTransforms(0, X, true, true);
    }
    SigilPts.Reset();
}
