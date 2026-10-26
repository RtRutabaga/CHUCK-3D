#include "TempleHero.h"
#include "HeroClipData.h"
#include "TempleSanctumBattle.h"
#include "TempleSkeleton.h"
#include "TempleSeal.h"
#include "ChultTempleInterior.h"
#include "ChultTempleWind.h"
#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"

namespace
{
    using EBlow = ATempleSkeleton::EBlow;
    namespace FD = HeroClipData::Fighter;

    // The fighter's blows: clip, moment of contact (clip s), hit-stop (s), what it does, the range it is
    // chosen at (centre to centre, cm) and how often.
    struct FCut { const TCHAR* Clip; float Contact, Length, HitStop; EBlow Blow; float MinD, MaxD, Weight; };
    const FCut Cuts[] = {
        // At contact the blade's cutting length spans about 60-120 cm ahead of him (Tools/build_hero_anims.py):
        // a skeleton's spine (16 cm round) inside that is cut.
        {TEXT("CutDown"), FD::CutDownContact, FD::CutDownLength, .085f, EBlow::Cut, 64.f, 118.f, 1.f},
        {TEXT("CutFlat"), FD::CutFlatContact, FD::CutFlatLength, .075f, EBlow::Decapitate, 64.f, 116.f, 1.f},
        {TEXT("Overhead"), FD::OverheadContact, FD::OverheadLength, .11f, EBlow::Cleave, 68.f, 118.f, .6f},
        {TEXT("CutRise"), FD::CutRiseContact, FD::CutRiseLength, .07f, EBlow::Cut, 62.f, 110.f, 0.f},   // for a foe above him (the beholder): not drawn against skeletons
        {TEXT("Bash"), FD::BashContact, FD::BashLength, .06f, EBlow::Bash, 45.f, 95.f, 0.f}};
    const FCut* FindCut(FName Clip)
    {
        for (const FCut& C : Cuts) if (Clip == FName(C.Clip)) return &C;
        return nullptr;
    }

    FTransform RefComponentSpace(const FReferenceSkeleton& Ref, int32 Index)
    {
        FTransform T = Ref.GetRefBonePose()[Index];
        for (int32 P = Ref.GetParentIndex(Index); P != INDEX_NONE; P = Ref.GetParentIndex(P)) T = T * Ref.GetRefBonePose()[P];
        return T;
    }

    float RootAt(const float* Root, int32 N, float T)
    {
        const float F = FMath::Clamp(T * 30.f, 0.f, static_cast<float>(N - 1));
        const int32 I = FMath::Min(static_cast<int32>(F), N - 2);
        return FMath::Lerp(Root[I], Root[I + 1], F - I);
    }
}

ATempleHero::ATempleHero()
{
    PrimaryActorTick.bCanEverTick = true;
    Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
    Capsule->InitCapsuleSize(Radius, HalfHeight);
    Capsule->SetCollisionProfileName(TEXT("Pawn"));
    RootComponent = Capsule;
    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Capsule);
    Mesh->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetAnimInstanceClass(USkeletonAnimInstance::StaticClass());
    Mesh->SetLightingChannels(false, true, false);   // the interior's torches light channel 1 only
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;   // the blade's hits are read from its bones
    Trail = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Trail"));
    Trail->SetupAttachment(Capsule);
    Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trail->SetCastShadow(false);
    Trail->SetLightingChannels(false, true, false);
}

void ATempleHero::Setup(ERole InRole, ATempleSanctumBattle* InBattle, USkeletalMesh* InMesh, const TMap<FName, UAnimSequence*>& InClips)
{
    Role = InRole; Battle = InBattle; Clips = InClips;
    Random.Initialize(17 + static_cast<int32>(Role) * 101);
    Mesh->SetSkinnedAssetAndUpdate(InMesh);
    if (InMesh)
    {
        const FReferenceSkeleton& Ref = InMesh->GetRefSkeleton();
        const int32 BL = Ref.FindBoneIndex(TEXT("ball_l")), BR = Ref.FindBoneIndex(TEXT("ball_r")), FL = Ref.FindBoneIndex(TEXT("foot_l")), FR = Ref.FindBoneIndex(TEXT("foot_r"));
        if (BL != INDEX_NONE && BR != INDEX_NONE && FL != INDEX_NONE && FR != INDEX_NONE)
        {
            FVector F = RefComponentSpace(Ref, BL).GetLocation() + RefComponentSpace(Ref, BR).GetLocation() - RefComponentSpace(Ref, FL).GetLocation() - RefComponentSpace(Ref, FR).GetLocation();
            F.Z = 0; BodyForward = F.GetSafeNormal();
            Mesh->SetRelativeRotation(FRotator(0, -BodyForward.Rotation().Yaw, 0));
        }
    }
    Animator.Init(Mesh, Clips, 200.f, BodyForward);
    if (Battle.IsValid()) Trail->SetMaterial(0, Battle->GetTrailMaterial());
    if (Role == ERole::Wizard && Battle.IsValid())
    {
        // The binding: a sea-green light in the gem, and its hum.
        GemLight = NewObject<UPointLightComponent>(this);
        GemLight->SetupAttachment(Mesh, TEXT("weapon_tip"));
        GemLight->SetLightColor(FLinearColor(.25f, 1.f, .75f));
        GemLight->SetIntensity(900.f); GemLight->SetAttenuationRadius(320.f); GemLight->SetCastShadows(false);
        GemLight->SetLightingChannels(false, true, false);
        GemLight->RegisterComponent();
        if (USoundBase* H = Battle->HumSound())
        {
            Hum = NewObject<UAudioComponent>(this);
            Hum->SetupAttachment(Mesh, TEXT("weapon_tip"));
            Hum->SetSound(H); Hum->SetVolumeMultiplier(.32f); Hum->bAutoActivate = false;
            Hum->AttenuationSettings = Battle->GetAttenuation();
            Hum->RegisterComponent();
            Hum->Play();
        }
    }
    BackToRest();
}

void ATempleHero::BeginPlay()
{
    Super::BeginPlay();
    // The mesh evaluates after this actor's tick: the bones read in a tick then show the clip time set in the
    // last one (BonesClipT), whatever the frame rate (the blade's hit test depends on it).
    Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
    Home = GetActorLocation() - FVector(0, 0, HalfHeight); HomeYaw = GetActorRotation().Yaw;
}

const TCHAR* ATempleHero::GetRoleName() const
{
    return Role == ERole::Fighter ? TEXT("Fighter") : Role == ERole::Wizard ? TEXT("Wizard") : TEXT("Ranger");
}

int32 ATempleHero::GetAnimEvaluations() const
{
    auto* A = Cast<USkeletonAnimInstance>(Mesh->GetAnimInstance());
    return A ? A->GetEvaluations() : 0;
}

FVector ATempleHero::HeadLocation() const { return Mesh->GetBoneLocation(TEXT("head")); }

float ATempleHero::GetContactTime() const
{
    const FCut* C = Act == EAct::Attack ? FindCut(Animator.Playing()) : nullptr;
    return C ? C->Contact : -1.f;
}
FVector ATempleHero::BladeTip() const { return Mesh->GetBoneLocation(TEXT("weapon_tip")); }
FVector ATempleHero::BladeBase() const
{
    const FVector G = Mesh->GetBoneLocation(TEXT("weapon_r")), T = BladeTip();
    return G + (T - G) * .28f;   // past the guard: the cutting length of the blade
}

float ATempleHero::GapTo(const FVector& P) const
{
    const FVector A = Mesh->GetBoneLocation(TEXT("pelvis")), B = Mesh->GetBoneLocation(TEXT("neck_01"));
    float Gap = FMath::Sqrt(static_cast<float>(FMath::PointDistToSegmentSquared(P, A, B))) - (Role == ERole::Fighter ? 27.f : 21.f);
    Gap = FMath::Min(Gap, static_cast<float>(FVector::Dist(P, HeadLocation())) - 13.f);
    if (Role == ERole::Fighter) Gap = FMath::Min(Gap, static_cast<float>(FVector::Dist(P, Mesh->GetBoneLocation(TEXT("shield_l")))) - 30.f);
    return Gap;
}

void ATempleHero::Play(FName Clip, bool bLoop, float Blend)
{
    Animator.Play(Clip, bLoop, 1.f, true, Blend);
    ActTime = 0; bWhoosh = bContact = bFired = bDrawSound = false;
    bStepSound[0] = bStepSound[1] = bStepSound[2] = false;
    StruckThisSwing.Reset();
}

void ATempleHero::BackToRest()
{
    Act = EAct::Idle;
    // The wizard's work, since 2026-10-25, is sealing the sea tear (his Seal loop); before, the binding at the dais.
    const bool bSeal = Role == ERole::Wizard && Battle.IsValid() && Battle->GetSeal() && Clips.Contains(TEXT("Seal")) && Clips[TEXT("Seal")];
    const FName Rest = Role == ERole::Fighter ? FName(TEXT("Guard")) : Role == ERole::Wizard ? FName(bSeal ? TEXT("Seal") : TEXT("Channel")) : FName(TEXT("Idle"));
    if (Animator.Playing() != Rest) { Animator.Play(Rest, true, 1.f, true, .3f); }
    ActTime = 0;
}

void ATempleHero::Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds)
{
    if (Dir.IsNearlyZero()) return;
    SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, Dir.Rotation().Yaw, DegPerSecond * DeltaSeconds), 0));
}

ATempleSkeleton* ATempleHero::NearestFoe(float Within, float Cone) const
{
    if (!Battle.IsValid()) return nullptr;
    ATempleSkeleton* Best = nullptr; float BestD = Within;
    const FVector Here = GetActorLocation(), Fwd = GetActorForwardVector();
    for (const TWeakObjectPtr<ATempleSkeleton>& W : Battle->Foes())
    {
        ATempleSkeleton* S = W.Get();
        if (!S || !S->IsStanding()) continue;
        const FVector To = S->GetActorLocation() - Here;
        const float D = static_cast<float>(To.Size2D());
        if (D >= BestD) continue;
        if (Cone > -1.f && FVector::DotProduct(To.GetSafeNormal2D(), Fwd) < Cone) continue;
        BestD = D; Best = S;
    }
    return Best;
}

bool ATempleHero::TakeSkeletonBlow(ATempleSkeleton* From, const FVector& At, bool bHeavy)
{
    if (!From || !Battle.IsValid()) return false;
    const FVector ToFoe = (From->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    const float Facing = FVector::DotProduct(ToFoe, GetActorForwardVector());
    // Mid-blow, the shield stays up through his wind-up: a blow caught on it then jolts him (the spring layer)
    // but does not stop his cut. Past the moment of contact he is open.
    const FCut* Cut = Act == EAct::Attack ? FindCut(Animator.Playing()) : nullptr;
    const bool bWindUp = Cut && Animator.Time() < Cut->Contact;
    const bool bGuarded = Role == ERole::Fighter && Facing > .3f &&
        (Act == EAct::Idle || Act == EAct::Step || (Act == EAct::React && Animator.Playing() == FName(TEXT("BlockHit"))) || bWindUp);
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (bGuarded && bWindUp)
    {
        ++Blocks;
        Animator.Flinch(5.f, FMath::Clamp(static_cast<float>(GetActorTransform().InverseTransformVectorNoScale(ToFoe).Y) * 6.f, -6.f, 6.f));
        Battle->OnBlocked(At, -ToFoe);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_BLOCK hero=%s mid_swing=1"), GetRoleName());
        return true;
    }
    if (bGuarded)
    {
        ++Blocks;
        Act = EAct::React; Play(TEXT("BlockHit"), false, .06f);
        HitStopUntil = W + .06f;
        Battle->OnBlocked(At, -ToFoe);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_BLOCK hero=%s"), GetRoleName());
        return true;
    }
    ++HitsTaken;
    // Turned by the blow: from his right (+Y local) it twists him left (+ twist).
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(ToFoe);
    Animator.Flinch(bHeavy ? 16.f : 9.f, static_cast<float>(Local.Y) * (bHeavy ? 14.f : 8.f));
    Act = EAct::React; Play(bHeavy ? TEXT("HitHeavy") : TEXT("HitLight"), false, .06f);
    HitStopUntil = W + .05f;
    Battle->Sound(Role == ERole::Fighter ? ATempleSanctumBattle::ESound::ArmourHit : ATempleSanctumBattle::ESound::BodyHit, At, Role == ERole::Fighter ? 1.f : .8f);
    UpdateTrail(false);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_HIT hero=%s heavy=%d"), GetRoleName(), bHeavy ? 1 : 0);
    return false;
}

void ATempleHero::Script(FName Clip, bool bLoop, float Blend, const FVector& Toward)
{
    bScripted = true; ScriptFace = Toward; ParalysedUntil = 0.f; HitStopUntil = 0.f;
    if (!Clip.IsNone()) Play(Clip, bLoop, Blend);
}

void ATempleHero::EndScript()
{
    if (!bScripted) return;
    bScripted = false; ScriptFace = FVector::ZeroVector;
    BackToRest();
}

void ATempleHero::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    Animator.SetTimeScale(W < HitStopUntil || W < ParalysedUntil ? .03f : 1.f);
    BonesClipT = Animator.Time();   // what the bones show now (evaluated after the last tick)
    BonesClip = Animator.Playing();
    Animator.Tick(DeltaSeconds, 0.f);
    if (W >= HitStopUntil) ActTime += DeltaSeconds;
    if (!Battle.IsValid()) return;
    TickShove(DeltaSeconds);
    // In a cutscene's hold: the clip it set, turning to face what it asked.
    if (bScripted) { if (!ScriptFace.IsZero()) Face(ScriptFace - GetActorLocation(), 120.f, DeltaSeconds); UpdateTrail(false); return; }
    if (W < ParalysedUntil) { UpdateTrail(false); return; }   // held rigid by the beholder's ray
    if (bHeld) { UpdateTrail(false); return; }
    switch (Role)
    {
    case ERole::Fighter: TickFighter(DeltaSeconds); break;
    case ERole::Wizard: TickWizard(DeltaSeconds); break;
    case ERole::Ranger: TickRanger(DeltaSeconds); break;
    }
}

void ATempleHero::TickFighter(float DeltaSeconds)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Here = GetActorLocation();
    ATempleSkeleton* Foe = NearestFoe(340.f, -1.f);
    // He watches the nearest of them; with none, the dais.
    {
        const FVector LookAt = Foe ? Foe->GetMesh()->GetBoneLocation(TEXT("head")) : Battle->BeholderSpot();
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(LookAt - HeadLocation());
        Animator.SetLook(FMath::Clamp(-FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -50.f, 50.f),
                         FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))), -30.f, 25.f));
    }
    switch (Act)
    {
    case EAct::Attack:
    {
        const FCut* C = FindCut(Animator.Playing());
        if (!C) { BackToRest(); break; }
        const float T = Animator.Time();
        // Until the blow commits he keeps it on the target.
        if (Target.IsValid() && T < C->Contact - .16f) Face(Target->GetActorLocation() - Here, 220.f, DeltaSeconds);
        if (!bWhoosh && T >= C->Contact - .18f && C->Blow != EBlow::Bash) { bWhoosh = true; Battle->Sound(ATempleSanctumBattle::ESound::Whoosh, BladeTip(), .9f, Random.FRandRange(.94f, 1.06f)); }
        if (!bStepSound[0] && T >= C->Contact - .04f) { bStepSound[0] = true; Battle->Sound(ATempleSanctumBattle::ESound::ArmourStep, Here - FVector(0, 0, HalfHeight - 5), .55f); }
        FighterContact(DeltaSeconds);
        BeholderContact();
        UpdateTrail(C->Blow != EBlow::Bash && T > C->Contact - .2f && T < C->Contact + .12f);
        if (T >= C->Length - .12f) { BackToRest(); CounterOn = nullptr; NextAt = W + Random.FRandRange(.25f, .7f); }
        break;
    }
    case EAct::React:
        UpdateTrail(false);
        if (Animator.Finished() || Animator.Time() >= Animator.Length() - .1f || (CounterOn.IsValid() && Animator.Time() > .22f))
        {
            BackToRest();
            // A bite taken on the shield is answered at once: a rising cut at its face.
            if (CounterOn.IsValid() && W - CounterAt > .9f) CounterOn = nullptr;   // too late: it is gone
            if (CounterOn.IsValid())
            {
                Act = EAct::Attack; Play(TEXT("CutRise"), false, .08f); LastCut = TEXT("CutRise"); ++Swings; Target = nullptr; bCounterHit = false;
                PrevBase = BladeBase(); PrevTip = BladeTip();
            }
        }
        break;
    case EAct::Step:
    {
        const bool bAdvance = Animator.Playing() == FName(TEXT("Advance"));
        const float* Root = bAdvance ? FD::AdvanceRoot : FD::RetreatRoot;
        const int32 N = bAdvance ? UE_ARRAY_COUNT(FD::AdvanceRoot) : UE_ARRAY_COUNT(FD::RetreatRoot);
        const float T = Animator.Time();
        const float Now = RootAt(Root, N, T);
        FHitResult Hit;
        // (Never a step onto the sanctum's broken floor, 2026-10-24.)
        const FVector Next = ChultTempleInteriorFrameOf(GetActorLocation() + GetActorForwardVector() * (Now - StepStart));
        if (!ChultTempleSanctumHoleAt(FVector2D(Next.X, Next.Y), 45.f)) AddActorWorldOffset(GetActorForwardVector() * (Now - StepStart), true, &Hit);
        StepStart = Now;
        for (int32 K = 0; K < 2; ++K)
            if (!bStepSound[K] && T >= (K ? .6f : .32f)) { bStepSound[K] = true; Battle->Sound(ATempleSanctumBattle::ESound::ArmourStep, Here - FVector(0, 0, HalfHeight - 5), .6f); }
        if (T >= FD::AdvanceLength - .05f) BackToRest();
        break;
    }
    case EAct::Idle:
    default:
    {
        UpdateTrail(false);
        if (!Foe)
        {
            // Back to his ground, facing the dais.
            const FVector ToHome = Home + FVector(0, 0, HalfHeight) - Here;
            if (ToHome.Size2D() > 40.f && W >= NextAt)
            {
                const bool bAhead = FVector::DotProduct(ToHome.GetSafeNormal2D(), GetActorForwardVector()) > 0;
                Act = EAct::Step; StepStart = 0; Play(bAhead ? TEXT("Advance") : TEXT("Retreat"), false, .15f);
                NextAt = W + 1.f;
                break;
            }
            Face(FRotator(0, HomeYaw, 0).Vector(), 60.f, DeltaSeconds);
            break;
        }
        const FVector To = Foe->GetActorLocation() - Here;
        const float D = static_cast<float>(To.Size2D());
        Face(To, 160.f, DeltaSeconds);
        const float Facing = FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector());
        if (W < NextAt || Facing < .9f) break;
        // It is about to strike, close: the shield in its face.
        FName Pick = Forced;
        if (Pick.IsNone() && Foe->StrikeIn() < .45f && D < 108.f) Pick = TEXT("Bash");
        if (Pick.IsNone())
        {
            float Sum = 0;
            for (const FCut& C : Cuts) if (D >= C.MinD && D <= C.MaxD) Sum += C.Weight;
            if (Sum > 0)
            {
                float R = Random.FRand() * Sum;
                for (const FCut& C : Cuts) if (D >= C.MinD && D <= C.MaxD && (R -= C.Weight) <= 0) { Pick = C.Clip; break; }
            }
            else if (D < 92.f && D > 50.f) Pick = TEXT("Bash");
            else if (D > 150.f && D < 260.f && FVector::Dist2D(Here, Home) < 90.f && Foe->GetVelocity().Size2D() < 20.f)
            {
                Act = EAct::Step; StepStart = 0; Play(TEXT("Advance"), false, .15f); NextAt = W + .4f; break;
            }
        }
        if (Pick.IsNone()) break;
        Forced = NAME_None;
        Target = Foe; LastCut = Pick; ++Swings;
        Act = EAct::Attack; Play(Pick, false, .12f);
        PrevBase = BladeBase(); PrevTip = BladeTip();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_SWING clip=%s dist=%.0f"), *Pick.ToString(), D);
        break;
    }
    }
}

void ATempleHero::FighterContact(float DeltaSeconds)
{
    // The bones are those of the last tick's clip time: test with that time, not the clock's.
    const FCut* C = BonesClip == Animator.Playing() ? FindCut(Animator.Playing()) : nullptr;
    const FVector Base = BladeBase(), Tip = BladeTip();
    const float T = BonesClipT;
    if (!C || bContact) { PrevBase = Base; PrevTip = Tip; PrevClipT = T; return; }
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (T < C->Contact - .1f || T > C->Contact + .12f)
    {
        PrevBase = Base; PrevTip = Tip; PrevClipT = T;
        if (T > C->Contact + .12f)
        {
            bContact = true;
            if (!StruckThisSwing.Num() && Target.IsValid())
            {
                // A miss: where the blade and the bones were, in his own frame (cm: +X ahead, +Y right, +Z from the floor).
                const FTransform X(GetActorRotation(), GetActorLocation() - FVector(0, 0, HalfHeight));
                const FVector P = X.InverseTransformPosition(Target->GetMesh()->GetBoneLocation(TEXT("pelvis")));
                const FVector N = X.InverseTransformPosition(Target->GetMesh()->GetBoneLocation(TEXT("neck_01")));
                const FVector Bl = X.InverseTransformPosition(ContactBase), Tl = X.InverseTransformPosition(ContactTip);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_MISS clip=%s target=%s pelvis=(%.0f,%.0f,%.0f) neck=(%.0f,%.0f,%.0f) blade_at_contact=(%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f) standing=%d"),
                    *Animator.Playing().ToString(), Target->GetStateName(), P.X, P.Y, P.Z, N.X, N.Y, N.Z, Bl.X, Bl.Y, Bl.Z, Tl.X, Tl.Y, Tl.Z, Target->IsStanding() ? 1 : 0);
            }
        }
        return;
    }
    if (FMath::Abs(T - C->Contact) < .02f) { ContactBase = Base; ContactTip = Tip; }
    if (W < HitStopUntil) return;   // frozen on a blow: the blade is where it was
    // Its speed over the clip's own time (frame hitches and hit-stop do not change how fast it swings).
    const FVector Vel = (Tip - PrevTip) / FMath::Max(1e-3f, T - PrevClipT);
    bool bAny = false;
    for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Battle->Foes())
    {
        ATempleSkeleton* S = Wk.Get();
        if (!S || !S->IsStanding() || StruckThisSwing.Contains(Wk)) continue;
        if (FVector::Dist2D(S->GetActorLocation(), GetActorLocation()) > 260.f) continue;
        USkeletalMeshComponent* M = S->GetMesh();
        const FVector Pelvis = M->GetBoneLocation(TEXT("pelvis")), Neck = M->GetBoneLocation(TEXT("neck_01")), Head = M->GetBoneLocation(TEXT("head")) + FVector(0, 0, 7);
        const FVector Throat = (Neck + M->GetBoneLocation(TEXT("head"))) * .5f;
        float BodyGap = 1e9f, HeadGap = 1e9f, NeckGap = 1e9f; FVector At = Tip;
        if (C->Blow == EBlow::Bash)
        {
            const FVector Shield = Mesh->GetBoneLocation(TEXT("shield_l"));
            BodyGap = FMath::Sqrt(static_cast<float>(FMath::PointDistToSegmentSquared(Shield, Pelvis, Neck))) - 52.f;
            At = Shield;
        }
        else
        {
            // The blade swept from where it was a frame ago to where it is: segments across it.
            for (int32 K = 0; K <= 4; ++K)
            {
                const float U = K / 4.f;
                const FVector A = FMath::Lerp(PrevBase, Base, U), B = FMath::Lerp(PrevTip, Tip, U);
                FVector P1, P2;
                FMath::SegmentDistToSegmentSafe(A, B, Pelvis, Neck, P1, P2);
                const float G = static_cast<float>(FVector::Dist(P1, P2)) - 16.f;
                if (G < BodyGap) { BodyGap = G; At = (P1 + P2) * .5f; }
                const FVector Q = FMath::ClosestPointOnSegment(Head, A, B);
                const float H = static_cast<float>(FVector::Dist(Q, Head)) - 14.f;
                if (H < HeadGap) { HeadGap = H; if (H < BodyGap) At = Q; }
                const FVector Qn = FMath::ClosestPointOnSegment(Throat, A, B);
                const float N = static_cast<float>(FVector::Dist(Qn, Throat)) - 9.f;
                if (N < NeckGap) { NeckGap = N; if (N < BodyGap && N < HeadGap) At = Qn; }
            }
        }
        if (FMath::Min(BodyGap, FMath::Min(HeadGap, NeckGap)) > 0.f) continue;
        EBlow Kind = C->Blow;
        if (Kind == EBlow::Decapitate && NeckGap > 3.f && HeadGap > 0.f) Kind = EBlow::Cut;   // the level cut took the chest, not the neck
        StruckThisSwing.Add(Wk);
        bAny = true; ++CleanHits;
        LastHitStop = C->HitStop; LastBladeSpeed = static_cast<float>(Vel.Size()) / 100.f;
        HitStopUntil = W + C->HitStop;
        S->HitStop(C->HitStop);
        const bool bFell = S->TakeHeroBlow(Kind, GetActorLocation(), At);
        Battle->OnBladeHit(S, Kind, At, Kind == EBlow::Bash ? GetActorForwardVector() * 500.f : Vel);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_CUT clip=%s blow=%d fell=%d body_gap=%.1f neck_gap=%.1f head_gap=%.1f blade_m_s=%.1f clip_t=%.3f at_z=%.0f"),
            *Animator.Playing().ToString(), static_cast<int32>(Kind), bFell ? 1 : 0, BodyGap, NeckGap, HeadGap, LastBladeSpeed, T, At.Z - (GetActorLocation().Z - HalfHeight));
    }
    if (bAny && C->Blow == EBlow::Bash) bContact = true;
    PrevBase = Base; PrevTip = Tip; PrevClipT = T;
}

void ATempleHero::UpdateTrail(bool bLive)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (bLive && W >= HitStopUntil) TrailSamples.Add({BladeBase(), BladeTip(), W});
    constexpr float Life = .11f;
    TrailSamples.RemoveAll([W, Life](const FTrailSample& S) { return W - S.Time > Life; });
    if (TrailSamples.Num() < 2) { if (Trail->GetNumSections()) Trail->ClearAllMeshSections(); return; }
    // A faint smear of the blade's passage: brightest at the point, gone toward the hilt and with age.
    TArray<FVector> V; TArray<int32> I; TArray<FLinearColor> C; TArray<FVector> N; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
    const FTransform& X = Trail->GetComponentTransform();
    for (const FTrailSample& S : TrailSamples)
    {
        const float A = FMath::Square(1.f - (W - S.Time) / Life);
        V.Add(X.InverseTransformPosition(S.Base)); V.Add(X.InverseTransformPosition(S.Tip));
        C.Add(FLinearColor(1, 1, 1, 0)); C.Add(FLinearColor(1, 1, 1, A));
        N.Add(FVector::UpVector); N.Add(FVector::UpVector);
        UV.Add(FVector2D(0, 0)); UV.Add(FVector2D(1, 0));
    }
    for (int32 K = 0; K + 1 < TrailSamples.Num(); ++K)
    {
        const int32 A = K * 2, B = A + 1, Cc = A + 2, D = A + 3;
        I.Append({A, B, D, A, D, Cc});
    }
    Trail->CreateMeshSection_LinearColor(0, V, I, N, UV, C, Tan, false);
}

void ATempleHero::TickWizard(float DeltaSeconds)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Here = GetActorLocation();
    const float Snuffed = IsSuppressed() ? .04f + .12f * FMath::Abs(FMath::Sin(W * 23.f)) : 1.f;
    if (Hum) Hum->SetVolumeMultiplier(.32f * (IsSuppressed() ? .05f : 1.f));
    if (GemLight) GemLight->SetIntensity(Snuffed * (700.f + 250.f * FMath::Sin(W * 7.3f) * FMath::Sin(W * 3.1f + 1.f) + (Act == EAct::Attack ? 4000.f * FMath::Max(0.f, 1.f - FMath::Abs(Animator.Time() - HeroClipData::Wizard::CastFire) * 5.f) : 0.f)));
    const ATempleBeholder* Bh = Battle->GetBeholder();
    ATempleSeal* Seal = Battle->GetSeal();
    FVector LookAt = Act == EAct::Attack && Target.IsValid() ? Target->GetMesh()->GetBoneLocation(TEXT("head")) : Bh ? Bh->GetCentre() : Battle->BeholderSpot();
    if (Seal && Act != EAct::Attack) LookAt = Seal->GetCentre() + (GetActorLocation() - Seal->GetCentre()).GetSafeNormal2D() * 40.f;   // the knitting edge nearest him
    if (Seal && IsSealing())
    {
        // His gem and his open hand feed the threads.
        Seal->Work(Mesh->GetBoneLocation(TEXT("weapon_tip")), Mesh->GetBoneLocation(TEXT("hand_l")) + GetActorForwardVector() * 12.f);
    }
    {
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(LookAt - HeadLocation());
        Animator.SetLook(FMath::Clamp(-FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -45.f, 45.f) * .6f,
                         FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))), -30.f, 25.f) * .5f);
    }
    switch (Act)
    {
    case EAct::Attack:
        if (Target.IsValid()) Face(Target->GetActorLocation() - Here, 90.f, DeltaSeconds);
        else if (Bh) Face(Bh->GetCentre() - Here, 90.f, DeltaSeconds);
        if (!bFired && Animator.Time() >= HeroClipData::Wizard::CastFire)
        {
            bFired = true; ++Shots;
            const FVector From = Mesh->GetBoneLocation(TEXT("hand_l")) + GetActorForwardVector() * 14.f;
            Battle->FireBolt(From, Target.Get());
        }
        if (Animator.Time() >= HeroClipData::Wizard::CastLength - .1f) { BackToRest(); NextAt = W + Random.FRandRange(7.f, 12.f); }
        break;
    case EAct::React:
        if (Seal && Animator.Playing() == FName(TEXT("SealStrain"))) Face(Seal->GetCentre() - Here, 40.f, DeltaSeconds);
        if (Animator.Time() >= Animator.Length() - .1f) BackToRest();
        break;
    default:
    {
        Face((Seal ? Seal->GetCentre() : Bh ? Bh->GetCentre() : Battle->BeholderSpot()) - Here, 50.f, DeltaSeconds);
        if (W < NextAt || IsSuppressed()) break;
        // The binding has his mind: he turns a bolt only on one that comes for him or the ranger.
        ATempleSkeleton* Foe = nullptr; float BestD = 1100.f;
        for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Battle->Foes())
        {
            ATempleSkeleton* S = Wk.Get();
            if (!S || !S->IsStanding() || !S->GetFoe() || S->GetFoe() == Battle->GetHero(0)) continue;
            const float D = static_cast<float>(FVector::Dist2D(S->GetActorLocation(), Here));
            if (D < BestD) { BestD = D; Foe = S; }
        }
        if (!Foe)
        {
            // With none coming, now and then he turns a bolt on the beholder itself.
            if (Bh && Bh->IsFighting() && W >= NextBeholderShot)
            {
                NextBeholderShot = W + Random.FRandRange(Seal ? 13.f : 8.f, Seal ? 19.f : 13.f);   // the seal has most of him
                Target = nullptr; Act = EAct::Attack; Play(TEXT("Cast"), false, .18f);
                Battle->Sound(ATempleSanctumBattle::ESound::BoltCast, Mesh->GetBoneLocation(TEXT("hand_l")), .9f);
                break;
            }
            NextAt = W + 1.f; break;
        }
        Target = Foe; Act = EAct::Attack; Play(TEXT("Cast"), false, .18f);
        Battle->Sound(ATempleSanctumBattle::ESound::BoltCast, Mesh->GetBoneLocation(TEXT("hand_l")), .9f);
        break;
    }
    }
}

void ATempleHero::TickRanger(float DeltaSeconds)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Here = GetActorLocation();
    ATempleSkeleton* T = Target.Get();
    if (T && (!T->IsStanding() || FVector::Dist2D(T->GetActorLocation(), Here) > 1700.f)) { Target = nullptr; T = nullptr; }
    if (!T && Act != EAct::React)
    {
        // The fighter holds the line: she shoots what comes for her or the wizard, and what comes for
        // him only while it is still well short of him. The nearest such.
        ATempleSkeleton* Best = nullptr; float BestD = 1e9f;
        const ATempleHero* Fighter = Battle->GetHero(0);
        for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Battle->Foes())
        {
            ATempleSkeleton* S = Wk.Get();
            if (!S || !S->IsStanding()) continue;
            const float D = static_cast<float>(FVector::Dist2D(S->GetActorLocation(), Here));
            if (D < 280.f || D > 1600.f) continue;
            const bool bOurs = S->GetFoe() && S->GetFoe() != Fighter;
            const bool bFar = !Fighter || FVector::Dist2D(S->GetActorLocation(), Fighter->GetActorLocation()) > 320.f;
            if (!bOurs && !bFar) continue;
            if (D < BestD) { BestD = D; Best = S; }
        }
        Target = Best; T = Best;
    }
    const ATempleBeholder* Bh = Battle->GetBeholder();
    const FVector LookAt = T ? T->GetMesh()->GetBoneLocation(TEXT("spine_03")) : Bh ? Bh->GetCentre() : Battle->BeholderSpot();
    {
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(LookAt - HeadLocation());
        Animator.SetLook(FMath::Clamp(-FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -30.f, 30.f) * .4f,
                         FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))), -25.f, 25.f) * .6f);
    }
    switch (Act)
    {
    case EAct::Attack:
    {
        const float Tt = Animator.Time();
        if (T) Face(T->GetActorLocation() - Here, 150.f, DeltaSeconds);
        else if (Bh) Face(Bh->GetCentre() - Here, 150.f, DeltaSeconds);
        if (!bDrawSound && Tt >= .3f) { bDrawSound = true; Battle->Sound(ATempleSanctumBattle::ESound::BowDraw, Mesh->GetBoneLocation(TEXT("weapon_l")), .55f); }
        if (!bFired && Tt >= HeroClipData::Ranger::ShootFire)
        {
            bFired = true; ++Shots;
            const FVector From = Mesh->GetBoneLocation(TEXT("arrow_r"));
            Battle->Sound(ATempleSanctumBattle::ESound::BowRelease, Mesh->GetBoneLocation(TEXT("weapon_l")), .9f);
            FVector Aim = From + GetActorForwardVector() * 1500.f;
            if (!T && Bh)
            {
                // At the beholder's face, raised for the arrow's drop over the distance (about 50 m/s).
                const float Tf = static_cast<float>(FVector::Dist(From, Bh->GetCentre())) / 5000.f;
                Aim = Bh->GetCentre() + FVector(0, 0, .5f * 980.f * Tf * Tf);
            }
            Battle->FireArrow(From, T, Aim);
        }
        if (Tt >= HeroClipData::Ranger::ShootLength - .05f) { BackToRest(); NextAt = W + Random.FRandRange(1.2f, 3.f); }   // ready, the next arrow nocked: she picks her moment
        break;
    }
    case EAct::React:
        if (Animator.Time() >= Animator.Length() - .1f) BackToRest();
        break;
    default:
        if (T)
        {
            Face(T->GetActorLocation() - Here, 150.f, DeltaSeconds);
            if (W >= NextAt) { Act = EAct::Attack; Play(TEXT("Shoot"), false, .25f); }
        }
        else
        {
            Face((Bh ? Bh->GetCentre() : Battle->BeholderSpot()) - Here, 60.f, DeltaSeconds);
            // Nothing coming for them: she looses at the beholder.
            const FVector ToB = Bh ? (Bh->GetCentre() - Here).GetSafeNormal2D() : FVector::ZeroVector;
            if (Bh && Bh->IsFighting() && W >= NextAt && W >= NextBeholderShot && FVector::DotProduct(ToB, GetActorForwardVector()) > .9f)
            {
                NextBeholderShot = W + Random.FRandRange(2.5f, 5.f);
                Act = EAct::Attack; Play(TEXT("Shoot"), false, .25f);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// The beholder's attacks on the heroes (docs/CHULT-TEMPLE-BEHOLDER.md#the-fight).

bool ATempleHero::IsSuppressed() const { return GetWorld() && GetWorld()->GetTimeSeconds() < SuppressUntil; }
bool ATempleHero::IsParalysed() const { return GetWorld() && GetWorld()->GetTimeSeconds() < ParalysedUntil; }

void ATempleHero::Shove(const FVector& Velocity, float Seconds)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    ShoveVel = FVector(Velocity.X, Velocity.Y, 0); ShoveStart = W; ShoveUntil = W + Seconds;
}

void ATempleHero::TickShove(float Dt)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Here = GetActorLocation();
    FVector Step = FVector::ZeroVector;
    if (W < ShoveUntil)
    {
        // Thrown: fast at first, dragging to a stop on the flags.
        const float U = (W - ShoveStart) / FMath::Max(.01f, ShoveUntil - ShoveStart);
        Step = ShoveVel * (2.f * (1.f - U)) * Dt;
    }
    else if (Role != ERole::Fighter && Act == EAct::Idle && W > ShoveUntil + 1.5f)
    {
        // The wizard and the ranger edge back to their ground (the fighter steps back to his).
        const FVector ToHome = Home + FVector(0, 0, HalfHeight) - Here;
        if (ToHome.Size2D() > 15.f) Step = ToHome.GetSafeNormal2D() * FMath::Min(25.f * Dt, static_cast<float>(ToHome.Size2D()));
    }
    if (Step.IsNearlyZero()) return;
    // Never onto the broken floor, never far from his ground.
    const FVector Next = Here + Step;
    const FVector F = ChultTempleInteriorFrameOf(Next);
    if (ChultTempleSanctumHoleAt(FVector2D(F.X, F.Y), 45.f)) { ShoveUntil = 0; return; }
    const FVector Feet = Home + FVector(0, 0, HalfHeight);
    if (FVector::Dist2D(Next, Feet) > (Role == ERole::Fighter ? 220.f : 130.f) && FVector::Dist2D(Next, Feet) > FVector::Dist2D(Here, Feet)) { ShoveUntil = 0; return; }
    FHitResult Hit;
    AddActorWorldOffset(Step, true, &Hit);
}

void ATempleHero::EvadeBeholderRay(const FVector& From, const FVector& Past)
{
    if (IsParalysed()) return;
    // Out of its way: a lunge to the side it passes on, a twist away from it.
    const FVector Side = (Past - GetActorLocation()).GetSafeNormal2D();
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(-Side);
    Animator.Flinch(7.f, static_cast<float>(Local.Y) * 14.f);
    Shove(-Side * 260.f, .3f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_EVADE hero=%s"), GetRoleName());
}

bool ATempleHero::TakeBeholderRay(EBeholderRay Kind, const FVector& From, FVector& InOutAt)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector ToSrc = (From - GetActorLocation()).GetSafeNormal2D();
    const float Facing = static_cast<float>(FVector::DotProduct(ToSrc, GetActorForwardVector()));
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(-ToSrc);
    // The fighter, in guard and turned to it, can take a ray on his shield (the telekinetic one still drives him back).
    const bool bGuard = Role == ERole::Fighter && Facing > .3f && !IsParalysed() && (Act == EAct::Idle || Act == EAct::Step || (Act == EAct::React && Animator.Playing() == FName(TEXT("BlockHit"))));
    if (bGuard && Random.FRand() < .6f)
    {
        ++Blocks;
        Act = EAct::React; Play(TEXT("BlockHit"), false, .05f);
        HitStopUntil = W + .07f;
        InOutAt = Mesh->GetBoneLocation(TEXT("shield_l")) + ToSrc * 18.f;
        Battle->OnBlocked(InOutAt, ToSrc);
        if (Kind == EBeholderRay::Telekinetic) Shove(-ToSrc * 300.f, .4f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_RAY_BLOCK hero=%s kind=%s"), GetRoleName(), ATempleBeholder::RayName(Kind));
        return false;
    }
    ++HitsTaken;
    UpdateTrail(false);
    CounterOn = nullptr;
    switch (Kind)
    {
    case EBeholderRay::Telekinetic:   // seized and hurled back
        Animator.Flinch(22.f, static_cast<float>(Local.Y) * 16.f);
        Act = EAct::React; Play(TEXT("HitHeavy"), false, .05f);
        Shove(-ToSrc * 420.f, .5f);
        break;
    case EBeholderRay::Paralyze:      // held rigid where he stands, a little over two seconds
        Animator.Flinch(6.f, static_cast<float>(Local.Y) * 5.f);
        Act = EAct::React; Play(TEXT("HitLight"), false, .05f);
        ParalysedUntil = W + 2.1f;
        break;
    case EBeholderRay::Fear:          // driven back, turned half away
        Animator.Flinch(11.f, static_cast<float>(Local.Y) * 20.f);
        Act = EAct::React; Play(TEXT("HitLight"), false, .05f);
        Shove(-ToSrc * 200.f, .55f);
        break;
    default:                          // enervation (and disintegration, if it ever reached one): it withers him
        Animator.Flinch(17.f, static_cast<float>(Local.Y) * 12.f);
        Act = EAct::React; Play(TEXT("HitHeavy"), false, .05f);
        HitStopUntil = W + .1f;
        break;
    }
    Battle->Sound(Role == ERole::Fighter ? ATempleSanctumBattle::ESound::ArmourHit : ATempleSanctumBattle::ESound::BodyHit, InOutAt, .8f, .9f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_RAY_HIT hero=%s kind=%s"), GetRoleName(), ATempleBeholder::RayName(Kind));
    return true;
}

bool ATempleHero::TakeBeholderBite(const FVector& From, const FVector& At)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector ToSrc = (From - GetActorLocation()).GetSafeNormal2D();
    const float Facing = static_cast<float>(FVector::DotProduct(ToSrc, GetActorForwardVector()));
    if (Role == ERole::Fighter && Facing > .1f && !IsParalysed())
    {
        // Braced behind the shield: the jaws close on it and drive him back a stride.
        ++Blocks;
        Act = EAct::React; Play(TEXT("BlockHit"), false, .04f);
        Animator.Flinch(14.f, 0.f);
        HitStopUntil = W + .09f;
        Shove(-ToSrc * 120.f, .3f);   // a half step: he holds his ground, to answer it
        Battle->OnBlocked(Mesh->GetBoneLocation(TEXT("shield_l")), ToSrc);
        Battle->Sound(ATempleSanctumBattle::ESound::ShieldBash, At, 1.f, .8f);
        return true;
    }
    ++HitsTaken;
    Animator.Flinch(24.f, 0.f);
    Act = EAct::React; Play(TEXT("HitHeavy"), false, .04f);
    HitStopUntil = W + .08f;
    Shove(-ToSrc * 340.f, .4f);
    Battle->Sound(Role == ERole::Fighter ? ATempleSanctumBattle::ESound::ArmourHit : ATempleSanctumBattle::ESound::BodyHit, At, 1.f, .85f);
    return false;
}

void ATempleHero::TakeBeholderCone(const FVector& Apex, const FVector& Axis)
{
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Away = Axis.GetSafeNormal2D();
    const float Facing = static_cast<float>(FVector::DotProduct(-Away, GetActorForwardVector()));
    CounterOn = nullptr;
    ParalysedUntil = 0;   // the antimagic undoes the paralysis too
    if (Role == ERole::Wizard)
    {
        SuppressUntil = W + 4.f;   // his binding snuffed: the gem dark, the hum gone
        if (ATempleSeal* Seal = Battle->GetSeal()) Seal->Break(Apex);   // and the seal shattered
    }
    if (Role == ERole::Fighter && Facing > .3f)
    {
        ++Blocks;
        Act = EAct::React; Play(TEXT("BlockHit"), false, .04f);
        Animator.Flinch(16.f, 0.f);
        Shove(Away * 330.f, .45f);
        Battle->OnBlocked(Mesh->GetBoneLocation(TEXT("shield_l")), -Away);
    }
    else
    {
        ++HitsTaken;
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Away);
        Animator.Flinch(24.f, static_cast<float>(-Local.Y) * 18.f);
        Act = EAct::React; Play(TEXT("HitHeavy"), false, .04f);
        Shove(Away * 420.f, .5f);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_CONE hero=%s suppressed=%d"), GetRoleName(), IsSuppressed() ? 1 : 0);
}

void ATempleHero::CounterBeholder(ATempleBeholder* Beholder)
{
    if (Role != ERole::Fighter || !Beholder) return;
    CounterOn = Beholder; bCounterHit = false; CounterAt = static_cast<float>(GetWorld()->GetTimeSeconds());
}

void ATempleHero::BeholderContact()
{
    // The counter: the rising cut meets its face if it is still in his reach at the moment of contact.
    ATempleBeholder* Bh = CounterOn.Get();
    const FCut* C = FindCut(Animator.Playing());
    if (!Bh || !C || bCounterHit || Animator.Playing() != FName(TEXT("CutRise"))) return;
    const float T = Animator.Time();
    if (T > C->Contact + .1f)
    {
        // Missed it (it was already drawing back): say how far, once.
        bCounterHit = true;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_BEHOLDER_CUT_MISS gap=%.0f"), FVector::Dist(FMath::ClosestPointOnSegment(Bh->GetCentre(), BladeBase(), BladeTip()), Bh->GetCentre()));
        return;
    }
    if (T < C->Contact - .05f) return;
    const FVector Tip = BladeTip(), Base = BladeBase();
    const FVector Q = FMath::ClosestPointOnSegment(Bh->GetCentre(), Base, Tip);
    if (FVector::Dist(Q, Bh->GetCentre()) > ATempleBeholder::BodyRadius + 45.f) return;
    bCounterHit = true; ++BeholderCuts;
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    HitStopUntil = W + .08f;
    Bh->TakeHit(Q, (FVector(0, 0, 1) + GetActorForwardVector()).GetSafeNormal(), 1.6f);
    Battle->Sound(ATempleSanctumBattle::ESound::BodyHit, Q, 1.f, .7f);
    Battle->Sound(ATempleSanctumBattle::ESound::Whoosh, Q, .6f, .9f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_BEHOLDER_CUT gap=%.0f"), FVector::Dist(Q, Bh->GetCentre()));
}

bool ATempleHero::IsSealing() const
{
    if (Role != ERole::Wizard || IsSuppressed() || IsParalysed()) return false;
    const FName C = Animator.Playing();
    return (Act == EAct::Idle && C == FName(TEXT("Seal"))) || C == FName(TEXT("SealStrain"));
}

void ATempleHero::SealSurge()
{
    if (!IsSealing() || Act != EAct::Idle) return;
    ++Strains;
    Act = EAct::React; Play(TEXT("SealStrain"), false, .06f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_SEAL_STRAIN strains=%d"), Strains);
}
