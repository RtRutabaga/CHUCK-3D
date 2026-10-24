#include "TempleBeholder.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "TempleSkeleton.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    constexpr float EyeRadius = 42.f;      // Tools/build_beholder.py: R_EYE
    constexpr float LidClose = 42.f;       // degrees the upper lid travels to meet the lower

    FTransform RefComponentSpace(const FReferenceSkeleton& Ref, int32 Index)
    {
        FTransform X = Ref.GetRefBonePose()[Index];
        for (int32 P = Ref.GetParentIndex(Index); P != INDEX_NONE; P = Ref.GetParentIndex(P)) X = X * Ref.GetRefBonePose()[P];
        return X;
    }

    float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

    /** A turn of at most MaxDeg degrees from the rest direction From toward To (both unit). */
    FQuat Toward(const FVector& From, const FVector& To, float MaxDeg)
    {
        FQuat Q = FQuat::FindBetweenNormals(From, To);
        FVector Axis; float Angle; Q.ToAxisAndAngle(Axis, Angle);
        if (Angle > PI) Angle -= 2.f * PI;
        const float Max = FMath::DegreesToRadians(MaxDeg);
        return FQuat(Axis, FMath::Clamp(Angle, -Max, Max));
    }
}

ATempleBeholder::ATempleBeholder()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetLightingChannels(false, true, false);   // the interior's torches light channel 1 only
    Body->SetBoundsScale(1.6f);                       // the stalks sway out of the reference pose's bounds
    Body->bCastDynamicShadow = true;
    EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
    EyeLight->SetupAttachment(RootComponent);
    EyeLight->SetLightColor(FLinearColor(1.f, .16f, .1f));
    EyeLight->SetIntensity(0.f); EyeLight->SetAttenuationRadius(650.f); EyeLight->SetCastShadows(false);
    EyeLight->SetLightingChannels(false, true, false);
}

void ATempleBeholder::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(20261023);
}

void ATempleBeholder::Setup(USkeletalMesh* Mesh, ATempleSanctumBattle* InBattle, const FVector& InHome)
{
    Battle = InBattle; Home = InHome;
    if (Mesh) Body->SetSkinnedAssetAndUpdate(Mesh);
    const int32 EyeSlot = Body->GetMaterialIndex(TEXT("Eye"));
    if (EyeSlot != INDEX_NONE) EyeMat = Body->CreateDynamicMaterialInstance(EyeSlot);
    ReadRig();
    Yaw = GetActorRotation().Yaw;
    // Which way the rig's rotations about Y turn the face down (FQuat is right-handed; the mesh faces +X).
    CloseSign = FQuat(FVector::YAxisVector, .3f).RotateVector(FVector::XAxisVector).Z < 0.f ? 1.f : -1.f;
    PickGaze();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_SPAWN mesh=%d bones=%d stalks=%d eye_mat=%d at=%s"), Mesh != nullptr, Bones.Num(), Stalks.Num(), EyeMat != nullptr, *GetActorLocation().ToCompactString());
}

void ATempleBeholder::ReadRig()
{
    Bones.Reset(); Stalks.Reset();
    const USkeletalMesh* M = Cast<USkeletalMesh>(Body->GetSkinnedAsset());
    if (!M) return;
    const FReferenceSkeleton& Ref = M->GetRefSkeleton();
    TMap<FName, int32> ByName;
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        FBone B; B.Name = Ref.GetBoneName(I); B.Parent = Ref.GetParentIndex(I);
        const FTransform CS = RefComponentSpace(Ref, I);
        B.RefPos = CS.GetLocation(); B.RefRot = CS.GetRotation(); B.RefScale = CS.GetScale3D();
        const FString N = B.Name.ToString();
        if (N == TEXT("root")) B.Kind = EKind::Root;
        else if (N == TEXT("body")) B.Kind = EKind::Body;
        else if (N == TEXT("jaw")) B.Kind = EKind::Jaw;
        else if (N.StartsWith(TEXT("tongue"))) B.Kind = EKind::Tongue;
        else if (N == TEXT("lid_upper")) B.Kind = EKind::LidUpper;
        else if (N == TEXT("lid_lower")) B.Kind = EKind::LidLower;
        else if (N == TEXT("eye")) { B.Kind = EKind::Eye; EyeRest = B.RefPos; }
        else if (N.StartsWith(TEXT("stalk")))
        {
            FString Num, Rest; N.Mid(5).Split(TEXT("_"), &Num, &Rest);
            B.Stalk = FCString::Atoi(*Num);
            if (Rest == TEXT("eye")) B.Kind = EKind::StalkEye; else { B.Kind = EKind::Stalk; B.Seg = FCString::Atoi(*Rest); }
            if (Stalks.Num() <= B.Stalk) Stalks.SetNum(B.Stalk + 1);
        }
        ByName.Add(B.Name, I);
        Bones.Add(B);
    }
    // Each stalk segment's direction (to the next joint) and the two axes it writhes about.
    for (FBone& B : Bones)
    {
        if (B.Kind != EKind::Stalk && B.Kind != EKind::StalkEye) continue;
        const FName Next = B.Kind == EKind::StalkEye ? NAME_None
            : B.Seg < 5 ? FName(*FString::Printf(TEXT("stalk%d_%d"), B.Stalk, B.Seg + 1)) : FName(*FString::Printf(TEXT("stalk%d_eye"), B.Stalk));
        if (B.Kind == EKind::StalkEye)
        {
            const FBone& P = Bones[B.Parent];
            B.Dir = (B.RefPos - P.RefPos).GetSafeNormal();
            Stalks[B.Stalk].Eye = ByName[B.Name]; Stalks[B.Stalk].RestEye = B.RefPos;
        }
        else if (const int32* NI = ByName.Find(Next)) B.Dir = (Bones[*NI].RefPos - B.RefPos).GetSafeNormal();
        B.Axis1 = FVector::CrossProduct(B.Dir, FVector::UpVector).GetSafeNormal();
        if (B.Axis1.IsNearlyZero()) B.Axis1 = FVector::RightVector;
        B.Axis2 = FVector::CrossProduct(B.Dir, B.Axis1).GetSafeNormal();
    }
    for (int32 K = 0; K < Stalks.Num(); ++K)
    {
        FStalk& S = Stalks[K];
        S.Phase1 = Random.FRandRange(0.f, 2.f * PI); S.Phase2 = Random.FRandRange(0.f, 2.f * PI);
        S.Rate1 = Random.FRandRange(.28f, .46f); S.Rate2 = Random.FRandRange(.33f, .55f);
        S.NextLook = Random.FRandRange(0.f, 2.f);
    }
    Delta.Init(FQuat::Identity, Bones.Num()); Pos.Init(FVector::ZeroVector, Bones.Num());
    for (const FBone& B : Bones)
        if (B.Parent == INDEX_NONE || B.Kind == EKind::Eye || B.Kind == EKind::LidUpper || B.Name == FName(TEXT("stalk0_eye")))
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_BONE %s pos=%s rot=%s scale=%s"), *B.Name.ToString(), *B.RefPos.ToCompactString(), *B.RefRot.Rotator().ToCompactString(), *B.RefScale.ToCompactString());
}

FVector ATempleBeholder::GetEyeLocation() const
{
    for (int32 I = 0; I < Bones.Num(); ++I)
        if (Bones[I].Kind == EKind::Eye) return Body->GetComponentTransform().TransformPosition(Pos[I] + Delta[I].RotateVector(FVector(EyeRadius, 0, 0)));
    return GetActorLocation();
}

FVector ATempleBeholder::Watchable(int32 Pick) const
{
    // 0-2 the heroes' heads, 3 a standing skeleton, 4 Chuck (when near and before it), else a hero.
    const ATempleSanctumBattle* B = Battle.Get();
    if (Pick == 4)
        if (const APawn* Chuck = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
        {
            const FVector To = Chuck->GetActorLocation() - GetActorLocation();
            if (To.Size() < 2200.f && FVector::DotProduct(To.GetSafeNormal(), GetActorForwardVector()) > -.2f && !Chuck->IsHidden())
                return Chuck->GetActorLocation() + FVector(0, 0, 20);
        }
    if (Pick == 3 && B)
    {
        TArray<FVector> Up;
        for (const TWeakObjectPtr<ATempleSkeleton>& S : B->Foes()) if (S.IsValid() && S->IsUp()) Up.Add(S->GetActorLocation() + FVector(0, 0, 60));
        if (Up.Num()) return Up[Random.RandRange(0, Up.Num() - 1)];
    }
    const int32 R = Pick >= 0 && Pick < 3 ? Pick : 0;
    if (B) if (const ATempleHero* H = B->GetHero(R)) return H->HeadLocation();
    return GetActorLocation() + GetActorForwardVector() * 600.f - FVector(0, 0, 200);
}

void ATempleBeholder::PickGaze()
{
    if (!HeldGaze.IsZero()) { GazeAt = HeldGaze; return; }
    const float R = Random.FRand();
    // Mostly the fighter in front of it; the others, a skeleton, and Chuck when he is close.
    const int32 Pick = R < .38f ? 0 : R < .53f ? 1 : R < .68f ? 2 : R < .8f ? 3 : 4;
    GazeAt = Watchable(Pick);
    NextGaze = T + Random.FRandRange(1.1f, 3.4f);
    ++Saccades;
}

void ATempleBeholder::Snarl()
{
    SnarlStart = T; SnarlLen = Random.FRandRange(1.f, 1.6f); SnarlOpen = Random.FRandRange(16.f, 22.f); ++Snarls;
}

void ATempleBeholder::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Bones.Num()) return;
    Dt = FMath::Min(Dt, .1f);
    T += Dt;
    if (T >= NextGaze || !HeldGaze.IsZero()) { if (!HeldGaze.IsZero()) GazeAt = HeldGaze; else PickGaze(); }
    if (T >= NextBlink)
    {
        BlinkStart = T; ++Blinks;
        NextBlink = T + (Random.FRand() < .15f ? .42f : Random.FRandRange(2.4f, 6.5f));
    }
    if (T >= NextSnarl) { Snarl(); NextSnarl = T + Random.FRandRange(7.f, 13.f); }
    // It turns to face the fight (the fighter, whom the others stand behind), slowly, swaying a little.
    FVector Focus = GetActorLocation() + GetActorForwardVector() * 500.f;
    if (const ATempleSanctumBattle* B = Battle.Get()) if (const ATempleHero* F = B->GetHero(0)) Focus = F->GetActorLocation();
    const float Want = (Focus - GetActorLocation()).Rotation().Yaw + 9.f * FMath::Sin(T * .21f);
    Yaw = FMath::FixedTurn(Yaw, Want, 10.f * Dt);
    SetActorLocationAndRotation(Home + FVector(22.f * FMath::Sin(T * .17f), 18.f * FMath::Sin(T * .13f + 1.f), 0), FRotator(0, Yaw, 0));
    Pose(Dt);
}

void ATempleBeholder::Pose(float Dt)
{
    const FTransform Comp = Body->GetComponentTransform();
    auto DownAboutY = [this](float Deg) { return FQuat(FVector::YAxisVector, CloseSign * FMath::DegreesToRadians(Deg)); };
    const FVector GazeC = Comp.InverseTransformPosition(GazeAt);
    // The snarl's envelope and the blink's.
    const float Sa = T - SnarlStart;
    const float Snarling = Sa < 0.f ? 0.f : Sa < .3f ? Ease(Sa / .3f) : Sa < .3f + SnarlLen ? 1.f : 1.f - Ease((Sa - .3f - SnarlLen) / .5f);
    const float Ba = T - BlinkStart;
    const float Blink = Ba < 0.f ? 0.f : Ba < .08f ? Ease(Ba / .08f) : Ba < .13f ? 1.f : 1.f - Ease((Ba - .13f) / .19f);
    JawNow = 2.f + 1.5f * FMath::Sin(T * .9f) + SnarlOpen * Snarling;
    const float Bob = 7.f * FMath::Sin(T * 1.7f) + 2.5f * FMath::Sin(T * .63f + 1.f);
    const float Elev = FMath::RadiansToDegrees(FMath::Atan2(GazeC.Z, FVector2D(GazeC.X, GazeC.Y).Size()));
    float EyeDown = 0.f;
    for (int32 I = 0; I < Bones.Num(); ++I)
    {
        const FBone& B = Bones[I];
        const int32 P = B.Parent;
        const FQuat DP = P == INDEX_NONE ? FQuat::Identity : Delta[P];
        FQuat Own = FQuat::Identity;
        FVector Offset = FVector::ZeroVector;
        switch (B.Kind)
        {
        case EKind::Body:
            Own = DownAboutY(FMath::Clamp(-Elev * .3f, -8.f, 12.f) + 1.5f * FMath::Sin(T * .5f)) * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(2.5f * FMath::Sin(T * .6f)));
            Offset = FVector(0, 0, Bob);
            break;
        case EKind::Jaw: Own = DownAboutY(JawNow); break;
        case EKind::Tongue: Own = DownAboutY((B.Name == FName(TEXT("tongue_0")) ? -1.f : 3.f) * FMath::Sin(T * 1.3f + 0.7f) - 4.f * Snarling); break;
        case EKind::Eye:
        {
            const FVector Dir = DP.Inverse().RotateVector((GazeC - (Pos[P] + DP.RotateVector(B.RefPos - Bones[P].RefPos))).GetSafeNormal());
            const FQuat Want = Toward(FVector::XAxisVector, Dir, 30.f);
            EyeAim = FQuat::Slerp(EyeAim, Want, 1.f - FMath::Exp(-Dt * 22.f));   // saccades are quick
            Own = EyeAim;
            EyeDown = -FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(EyeAim.RotateVector(FVector::XAxisVector).Z, -1.f, 1.f)));
            break;
        }
        case EKind::LidUpper: Own = DownAboutY(Blink * LidClose + FMath::Clamp(EyeDown * .6f, -8.f, 14.f) + 5.f * Snarling); break;
        case EKind::LidLower: Own = DownAboutY(-(Blink * 6.f + 4.f * Snarling) + FMath::Clamp(EyeDown * .3f, -4.f, 6.f)); break;
        case EKind::Stalk:
        {
            const FStalk& S = Stalks[B.Stalk];
            const float A = 3.f + 1.7f * B.Seg;
            const float A1 = A * (FMath::Sin(T * S.Rate1 * 2.f * PI + S.Phase1 + B.Seg * .85f) + .35f * FMath::Sin(T * .8f + S.Phase2));
            const float A2 = .8f * A * FMath::Sin(T * S.Rate2 * 2.f * PI + S.Phase2 + B.Seg * 1.1f);
            Own = FQuat(B.Axis1, FMath::DegreesToRadians(A1)) * FQuat(B.Axis2, FMath::DegreesToRadians(A2));
            break;
        }
        case EKind::StalkEye:
        {
            FStalk& S = Stalks[B.Stalk];
            if (T >= S.NextLook || S.Look.IsZero())
            {
                const float R = Random.FRand();
                S.Look = Watchable(R < .3f ? 0 : R < .45f ? 1 : R < .6f ? 2 : R < .85f ? 3 : 4) + FVector(Random.FRandRange(-60, 60), Random.FRandRange(-60, 60), 0);
                S.NextLook = T + Random.FRandRange(1.5f, 4.5f);
            }
            const FVector Here = Pos[P] + DP.RotateVector(B.RefPos - Bones[P].RefPos);
            const FVector Dir = DP.Inverse().RotateVector((Comp.InverseTransformPosition(S.Look) - Here).GetSafeNormal());
            S.Aim = FQuat::Slerp(S.Aim, Toward(B.Dir, Dir, 60.f), 1.f - FMath::Exp(-Dt * 7.f));
            Own = S.Aim;
            break;
        }
        default: break;
        }
        Delta[I] = DP * Own;
        Pos[I] = (P == INDEX_NONE ? B.RefPos : Pos[P] + DP.RotateVector(B.RefPos - Bones[P].RefPos)) + Offset;
        Body->SetBoneTransformByName(B.Name, FTransform(Delta[I] * B.RefRot, Pos[I], B.RefScale), EBoneSpaces::ComponentSpace);
    }
    Body->MarkRefreshTransformDirty();
    for (const FStalk& S : Stalks) if (S.Eye != INDEX_NONE) StalkTravel = FMath::Max(StalkTravel, static_cast<float>(FVector::Dist(Pos[S.Eye], S.RestEye)));
    // The iris smoulders, and flares in a snarl; a red light off it on the lids and the floor before it.
    const float Glow = .5f + .2f * FMath::Sin(T * 1.3f) + 1.2f * Snarling;
    if (EyeMat) EyeMat->SetScalarParameterValue(TEXT("Glow"), Glow);
    if (EyeLight)
    {
        EyeLight->SetWorldLocation(GetEyeLocation() + GetActorForwardVector() * 25.f);
        EyeLight->SetIntensity((1.f - .8f * Blink) * (150.f + 450.f * Snarling + 40.f * FMath::Sin(T * 1.3f)));
    }
    ++Poses;
}
