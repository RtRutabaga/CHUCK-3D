#include "TempleSnake.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<ATempleSnake>> SnakeRegistry;
    constexpr float Spacing = 2.f;        // cm between trail points
    constexpr float Wavelength = 46.f;    // of the crawl's undulation along the track
    constexpr float SnoutU = 180.f;       // the snout's x in the rig (Tools/build_temple_snake.py)
    constexpr float HeadU = 170.5f;       // the head bone's joint

    bool Suppressed()
    {
        const TCHAR* Cmd = FCommandLine::Get();
        if (FParse::Param(Cmd, TEXT("ChuckSnakeTest")) || FParse::Param(Cmd, TEXT("ChuckSnakeCapture"))) return false;
        TArray<FString> Words; FString(Cmd).ParseIntoArrayWS(Words);
        for (const FString& Word : Words)
            if (Word.StartsWith(TEXT("-Chuck")) && (Word.Contains(TEXT("Test")) || Word.Contains(TEXT("Capture")) || Word.Contains(TEXT("Review")))) return true;
        return false;
    }

    FTransform RefComponentSpace(const FReferenceSkeleton& Ref, int32 Index)
    {
        FTransform T = Ref.GetRefBonePose()[Index];
        for (int32 P = Ref.GetParentIndex(Index); P != INDEX_NONE; P = Ref.GetParentIndex(P)) T = T * Ref.GetRefBonePose()[P];
        return T;
    }

    FVector Bezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T)
    {
        const float U = 1.f - T;
        return A * (U * U * U) + B * (3.f * U * U * T) + C * (3.f * U * T * T) + D * (T * T * T);
    }
}

const TArray<TWeakObjectPtr<ATempleSnake>>& ATempleSnake::All() { return SnakeRegistry; }

ATempleSnake::ATempleSnake()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetLightingChannels(false, true, false);   // the interior's torches light channel 1 only
    Body->SetBoundsScale(2.6f);                       // the body runs far from its reference pose
    Body->bCastDynamicShadow = true;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Snake/SK_TempleSnake.SK_TempleSnake"));
    Body->SetSkinnedAssetAndUpdate(Mesh.Object);
}

ATempleSnake* ATempleSnake::SpawnAt(UWorld* World, const FTempleSnakePost& InPost)
{
    if (!World) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; Params.bDeferConstruction = true;
    auto* S = World->SpawnActor<ATempleSnake>(InPost.Feet, FRotator::ZeroRotator, Params);
    if (!S) return nullptr;
    S->Post = InPost;
    S->Random.Initialize(static_cast<int32>(InPost.Feet.X * 3 + InPost.Feet.Y * 11));
    UGameplayStatics::FinishSpawningActor(S, FTransform(FRotator::ZeroRotator, InPost.Feet));
    return S;
}

void SpawnTempleSnakes(UWorld* World)
{
    if (!World || Suppressed()) return;
    for (TActorIterator<ATempleSnake> It(World); It; ++It) return;   // once
    TArray<FTempleSnakePost> Posts; ChultTempleSnakePosts(Posts);
    int32 Spawned = 0, Wander = 0;
    for (const FTempleSnakePost& P : Posts) if (ATempleSnake::SpawnAt(World, P)) { ++Spawned; Wander += P.bWander; }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKES spawned=%d of=%d wander=%d"), Spawned, Posts.Num(), Wander);
}

void ATempleSnake::BeginPlay()
{
    Super::BeginPlay();
    SnakeRegistry.Add(this);
    // The rig, parents first: spine bones by their distance from the snout, the head, jaw and tongue.
    if (const USkeletalMesh* M = Cast<USkeletalMesh>(Body->GetSkinnedAsset()))
    {
        const FReferenceSkeleton& Ref = M->GetRefSkeleton();
        for (int32 I = 0; I < Ref.GetNum(); ++I)
        {
            FBoneRef B; B.Name = Ref.GetBoneName(I); B.Index = I; B.Ref = RefComponentSpace(Ref, I);
            const FString N = B.Name.ToString();
            B.Kind = N.StartsWith(TEXT("s")) && N.Len() == 3 ? 0 : N == TEXT("head") ? 1 : N == TEXT("jaw") ? 2 : N == TEXT("tongue") ? 3 : 4;
            B.Arc = SnoutU - static_cast<float>(B.Ref.GetLocation().X);
            if (B.Kind == 1) RefHead = B.Ref; else if (B.Kind == 2) RefJaw = B.Ref; else if (B.Kind == 3) RefTongue = B.Ref;
            Bones.Add(B);
        }
    }
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(S);
    };
    Load(HissSounds, TEXT("SFX_SnakeHiss"), 3); Load(StrikeSounds, TEXT("SFX_SnakeStrike"), 3); Load(SlitherSounds, TEXT("SFX_SnakeSlither"), 4); Load(DeathSounds, TEXT("SFX_SnakeDeath"), 2);
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 1400.f;
    ResetToPost();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_SPAWN bones=%d spine0_x=%.1f head_x=%.1f sounds=%d mesh=%d"), Bones.Num(),
        Bones.Num() > 1 ? Bones[1].Ref.GetLocation().X : -1.f, RefHead.GetLocation().X, HissSounds.Num() + StrikeSounds.Num() + SlitherSounds.Num() + DeathSounds.Num(), Body->GetSkinnedAsset() != nullptr);
}

void ATempleSnake::EndPlay(const EEndPlayReason::Type Reason)
{
    SnakeRegistry.RemoveAll([this](const TWeakObjectPtr<ATempleSnake>& S) { return !S.IsValid() || S.Get() == this; });
    Super::EndPlay(Reason);
}

const TCHAR* ATempleSnake::GetStateName() const
{
    static const TCHAR* Names[] = {TEXT("Rest"), TEXT("Wander"), TEXT("Alert"), TEXT("Chase"), TEXT("Rear"), TEXT("Strike"), TEXT("Recoil"), TEXT("Return"), TEXT("Dead")};
    return Names[static_cast<int32>(State)];
}

void ATempleSnake::SetState(EState To)
{
    State = To; StateTime = 0;
    if (To == EState::Rear) Sound(HissSounds, .8f);
    else if (To == EState::Strike) Sound(StrikeSounds, 1.f);
    else if (To == EState::Dead) Sound(DeathSounds, 1.f);
}

void ATempleSnake::Sound(const TArray<USoundBase*>& Set, float Volume)
{
    if (!Set.Num()) return;
    UGameplayStatics::PlaySoundAtLocation(this, Set[Random.RandRange(0, Set.Num() - 1)], HeadPos + FVector(0, 0, 5), Volume, Random.FRandRange(.92f, 1.08f), 0.f, Attenuation);
    ++SoundsPlayed;
}

bool ATempleSnake::InLeash(const FVector& World, float Margin) const
{
    const FVector F = ChultTempleInteriorFrameOf(World);
    return F.X >= Post.Leash.Min.X - Margin && F.X <= Post.Leash.Max.X + Margin && F.Y >= Post.Leash.Min.Y - Margin && F.Y <= Post.Leash.Max.Y + Margin;
}

// ------------------------------------------------------------------ the track

void ATempleSnake::Coil()
{
    // At rest it lies in a flat coil: an Archimedean spiral 9 cm across its middle, the turns 7.6 cm
    // apart (its body is 6.7 cm thick), the tail inside and the head at the outer end.
    const float Turn = Random.FRand() < .5f ? 1.f : -1.f;
    TArray<FVector> Raw;
    float Arc = 0; FVector Last = FVector::ZeroVector; bool bFirst = true;
    for (float Th = 0; Arc < Length + 30.f; Th += .01f)
    {
        const float R = 4.5f + 7.6f * Th / (2 * PI);
        const FVector P(FMath::Cos(Th * Turn) * R, FMath::Sin(Th * Turn) * R, 0);
        if (!bFirst) Arc += static_cast<float>(FVector::Dist(P, Last));
        Raw.Add(P); Last = P; bFirst = false;
    }
    // Head first, every Spacing, turned to the post's yaw.
    const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(Post.Yaw));
    Trail.Reset();
    const FVector C(Post.Feet.X, Post.Feet.Y, GroundZ);
    FVector Prev = Raw.Last(); float Carry = 0;
    HeadPos = C + Yaw.RotateVector(Prev);
    for (int32 I = Raw.Num() - 2; I >= 0; --I)
    {
        const FVector P = Raw[I];
        float Seg = static_cast<float>(FVector::Dist(P, Prev));
        FVector From = Prev;
        while (Carry + Seg >= Spacing)
        {
            const float Need = Spacing - Carry;
            const FVector At = From + (P - From).GetSafeNormal() * Need;
            Trail.Add(C + Yaw.RotateVector(At));
            Seg -= Need; From = At; Carry = 0;
        }
        Carry += Seg; Prev = P;
    }
    const FVector Out = (HeadPos - Trail[0]).GetSafeNormal2D();
    Heading = Out.Rotation().Yaw;
}

void ATempleSnake::Advance(const FVector& Delta)
{
    HeadPos += FVector(Delta.X, Delta.Y, 0);
    while (FVector::Dist2D(HeadPos, Trail[0]) >= Spacing)
        Trail.Insert(Trail[0] + (HeadPos - Trail[0]).GetSafeNormal2D() * Spacing, 0);
    const int32 Keep = FMath::CeilToInt((Length + 30.f) / Spacing);
    if (Trail.Num() > Keep) Trail.SetNum(Keep);
}

FVector ATempleSnake::TrailPoint(float Arc) const
{
    if (!Trail.Num()) return HeadPos;
    const float D0 = static_cast<float>(FVector::Dist2D(HeadPos, Trail[0]));
    if (Arc <= D0) return D0 > 1e-3f ? FMath::Lerp(HeadPos, Trail[0], Arc / D0) : HeadPos;
    const float R = (Arc - D0) / Spacing;
    const int32 I = FMath::FloorToInt(R);
    if (I + 1 >= Trail.Num()) return Trail.Last() + (Trail.Last() - Trail[FMath::Max(0, Trail.Num() - 2)]).GetSafeNormal() * (Arc - D0 - (Trail.Num() - 1) * Spacing);
    return FMath::Lerp(Trail[I], Trail[I + 1], R - I);
}

FVector ATempleSnake::BodyPoint(float Arc) const
{
    FVector P = TrailPoint(Arc);
    if (RearW > .001f && Arc < Front && Curve.Num() > 1)
    {
        const float R = FMath::Clamp(Arc, 0.f, static_cast<float>(Curve.Num() - 1));
        const int32 I = FMath::Min(FMath::FloorToInt(R), Curve.Num() - 2);
        P = FMath::Lerp(P, FMath::Lerp(Curve[I], Curve[I + 1], R - I), RearW);
    }
    if (DeathT >= 0.f)
    {
        // The death throes: waves running down it, dying away over two seconds.
        const FVector T = (TrailPoint(Arc - 2.f) - TrailPoint(Arc + 2.f)).GetSafeNormal2D();
        const FVector Side(-T.Y, T.X, 0);
        const float Amp = 9.f * FMath::Exp(-1.6f * DeathT) * FMath::Sin(2.f * PI * (Arc / 55.f - 1.7f * DeathT));
        P += Side * Amp * FMath::Clamp(Arc / 30.f, .3f, 1.f);
    }
    return P;
}

void ATempleSnake::PostureCurve(TArray<FVector>& Out) const
{
    // The front third drawn up into an S from where it leaves the floor (B) to the snout (H):
    // a cubic curve as long as that part of the body, its handles swung out to the side.
    const FVector B = TrailPoint(Front);
    const FVector Tb = (TrailPoint(Front - 2.f) - TrailPoint(Front + 2.f)).GetSafeNormal2D();
    const FVector Fwd = Aim.GetSafeNormal2D();
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd) * (Post.Yaw > 180.f ? 1.f : -1.f);
    const FVector H = B + Fwd * (Ready + StrikeReach * Ext) + FVector(0, 0, 16.f - 7.f * Ext);
    auto Make = [&](float K, FVector& C1, FVector& C2) { C1 = B + Tb * K * .55f + Side * K * .55f + FVector(0, 0, K * .12f); C2 = H - Fwd * K * .5f - Side * K * .3f; };
    auto CurveLength = [&](float K) { FVector C1, C2; Make(K, C1, C2); float Len = 0; FVector Prev = B; for (int32 I = 1; I <= 32; ++I) { const FVector P = Bezier(B, C1, C2, H, I / 32.f); Len += static_cast<float>(FVector::Dist(P, Prev)); Prev = P; } return Len; };
    float Lo = 0, Hi = 90;
    if (CurveLength(0) < Front) for (int32 It = 0; It < 18; ++It) { const float Mid = (Lo + Hi) * .5f; (CurveLength(Mid) < Front ? Lo : Hi) = Mid; }
    FVector C1, C2; Make(Lo, C1, C2);
    // Samples from the snout back, then 1 cm apart along it.
    TArray<FVector> S; TArray<float> Cum; float Len = 0;
    for (int32 I = 0; I <= 64; ++I) { const FVector P = Bezier(B, C1, C2, H, 1.f - I / 64.f); if (I) Len += static_cast<float>(FVector::Dist(P, S.Last())); S.Add(P); Cum.Add(Len); }
    Out.Reset();
    int32 J = 0;
    for (int32 A = 0; A <= static_cast<int32>(Front); ++A)
    {
        const float Want = A * Len / Front;   // spread over the curve (stretched a little if it is short of the body's length)
        while (J + 1 < Cum.Num() - 1 && Cum[J + 1] < Want) ++J;
        const float Span = FMath::Max(1e-3f, Cum[J + 1] - Cum[J]);
        Out.Add(FMath::Lerp(S[J], S[J + 1], FMath::Clamp((Want - Cum[J]) / Span, 0.f, 1.f)));
    }
}

// ------------------------------------------------------------------ the pose

void ATempleSnake::Pose()
{
    if (RearW > .001f) PostureCurve(Curve); else Curve.Reset();
    Points.Reset();
    for (float A = 0; A <= Length; A += 6.f) Points.Add(BodyPoint(A));
    const FVector Mid = BodyPoint(Length * .5f);
    SetActorLocation(Mid);
    const FVector Comp = GetActorLocation();
    auto Frame = [&](const FVector& T, float Arc)
    {
        FQuat Q = FRotationMatrix::MakeFromXZ(T.IsNearlyZero() ? FVector::ForwardVector : T, FVector::UpVector).ToQuat();
        if (DeathT >= 0.f)   // dead, it rolls half over along its middle
            Q = Q * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(150.f * FMath::Sin(PI * FMath::Clamp(Arc / Length, 0.f, 1.f)) * FMath::Clamp(DeathT / 1.2f, 0.f, 1.f)));
        return Q;
    };
    const FVector HeadJoint = BodyPoint(SnoutU - HeadU), Tip = BodyPoint(0);
    const FQuat Qh = Frame((Tip - HeadJoint).GetSafeNormal(), 0.f);
    const FTransform HeadCS(Qh * RefHead.GetRotation(), HeadJoint - Comp + Qh.RotateVector(FVector(0, 0, RefHead.GetLocation().Z)));
    FTransform JawCS = RefJaw.GetRelativeTransform(RefHead) * HeadCS;
    {
        // The gape: the jaw swings down about the head's left-right axis (whichever sign takes the chin down).
        const FVector Lateral = Qh.RotateVector(FVector::RightVector);
        const float Angle = FMath::DegreesToRadians(58.f * Gape);
        FQuat G(Lateral, Angle);
        if (G.RotateVector(Qh.RotateVector(FVector::ForwardVector)).Z > Qh.RotateVector(FVector::ForwardVector).Z) G = FQuat(Lateral, -Angle);
        JawCS.SetRotation(G * JawCS.GetRotation());
    }
    FTransform TongueCS = RefTongue.GetRelativeTransform(RefJaw) * JawCS;
    if (Flick > .01f)
    {
        TongueCS.AddToTranslation(Qh.RotateVector(FVector::ForwardVector) * 4.6f * Flick);
        const float Wiggle = FMath::DegreesToRadians(14.f * Flick * FMath::Sin(FlickClock * 2 * PI * 9.f));
        TongueCS.SetRotation(FQuat(Qh.RotateVector(FVector::RightVector), Wiggle) * TongueCS.GetRotation());
    }
    Snout = Tip + Qh.RotateVector(FVector(0, 0, 1.2f));
    for (const FBoneRef& B : Bones)
    {
        FTransform CS;
        switch (B.Kind)
        {
        case 0:
        {
            const FVector P = BodyPoint(B.Arc);
            const FQuat Q = Frame((BodyPoint(B.Arc - 1.5f) - BodyPoint(B.Arc + 1.5f)).GetSafeNormal(), B.Arc);
            CS = FTransform(Q * B.Ref.GetRotation(), P - Comp + Q.RotateVector(FVector(0, 0, B.Ref.GetLocation().Z)), B.Ref.GetScale3D());
            break;
        }
        case 1: CS = HeadCS; break;
        case 2: CS = JawCS; break;
        case 3: CS = TongueCS; break;
        default: CS = FTransform(B.Ref.GetRotation(), BodyPoint(Length) - Comp, B.Ref.GetScale3D()); break;
        }
        Body->SetBoneTransformByName(B.Name, CS, EBoneSpaces::ComponentSpace);
    }
    Body->MarkRefreshTransformDirty();
    bPosed = true;
}

// ------------------------------------------------------------------ moving

bool ATempleSnake::Blocked(const FVector& From, const FVector& To) const
{
    if (!InLeash(To)) return true;
    static TArray<FBox2D> Obstacles;
    if (!Obstacles.Num()) ChultTempleSnakeObstacles(Obstacles);
    const FVector F = ChultTempleInteriorFrameOf(To);
    for (const FBox2D& Box : Obstacles) if (Box.ExpandBy(14.f).IsInside(FVector2D(F.X, F.Y))) return true;
    return false;
}

void ATempleSnake::Crawl(const FVector& Goal, float Speed, float DeltaSeconds)
{
    const FVector To = (Goal - HeadPos).GetSafeNormal2D();
    if (!To.IsNearlyZero()) Heading = FMath::FixedTurn(Heading, To.Rotation().Yaw, 150.f * DeltaSeconds);
    // Round a pier: the first clear heading near the wanted one.
    for (const float Turn : {0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f, 160.f})
    {
        const FVector Try = FRotator(0, Heading + Turn, 0).Vector();
        if (!Blocked(HeadPos, HeadPos + Try * 24.f)) { Heading = FMath::FixedTurn(Heading, Heading + Turn, 300.f * DeltaSeconds); break; }
    }
    // The snout weaves about its heading; the body follows its track.
    const float Dist = Speed * DeltaSeconds;
    Phase += Dist / Wavelength * 2 * PI;
    const FVector Dir = FRotator(0, Heading + 34.f * FMath::Sin(Phase), 0).Vector();
    const FVector Next = HeadPos + Dir * Dist;
    if (!Blocked(HeadPos, Next)) Advance(Next - HeadPos);
    TopSpeed = FMath::Max(TopSpeed, Speed);
    SlitherDistance += Dist;
    if (SlitherDistance > 70.f) { SlitherDistance = 0; Sound(SlitherSounds, .35f); }
}

// ------------------------------------------------------------------ behaviour

void ATempleSnake::ResetToPost()
{
    GroundZ = static_cast<float>(Post.Feet.Z);
    State = EState::Rest; StateTime = Random.FRandRange(0.f, 3.f); DeathT = -1; RearW = 0; Ext = 0; Gape = 0; Flick = 0; Cooldown = 0; Lost = 0;
    Strikes = 0; Bites = 0; LastStrikeGap = 1e9f; WorstLeash = 0; TopSpeed = 0; SlipSum = 0; SlipSamples = 0;
    Coil();
    Pose();
    LastMid = BodyPoint(Length * .5f);
}

bool ATempleSnake::Scratch(const FVector& Location, const FVector& Ahead, float InReach, const FVector& Swing, bool bKill)
{
    if (State == EState::Dead || !Points.Num()) return false;
    const float Feet = static_cast<float>(Location.Z) - 32.5f;
    for (const FVector& P : Points)
    {
        const FVector To = P - Location;
        const float D = static_cast<float>(To.Size2D()) - 3.5f;
        if (D > InReach || FMath::Abs(static_cast<float>(P.Z) - Feet) > 45.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) < .25f && D > 6.f) continue;
        if (bKill)
        {
            Swung = Swing; DeathT = 0; Gape = .45f;
            SetState(EState::Dead);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_DEAD at=%s"), *ChultTempleInteriorFrameOf(HeadPos).ToString());
        }
        return true;
    }
    return false;
}

void ATempleSnake::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Trail.Num()) return;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector ChuckAt = Chuck ? Chuck->GetActorLocation() : HeadPos + FVector(1e6, 0, 0);
    const float Dist = static_cast<float>(FVector::Dist2D(Snout, ChuckAt));
    // Far off it keeps its last pose.
    if (Dist > 3500.f && State != EState::Dead) return;
    StateTime += DeltaSeconds;
    auto Ease = [DeltaSeconds](float& V, float Target, float Rate) { V = FMath::FInterpConstantTo(V, Target, DeltaSeconds, Rate); };
    // The tongue: a flick now and then (out, quivering, back), more often when it is hunting.
    FlickClock += DeltaSeconds;
    if (State != EState::Dead && State != EState::Strike && FlickClock > NextFlick) { FlickClock = 0; NextFlick = State == EState::Rest ? Random.FRandRange(1.8f, 4.5f) : Random.FRandRange(.7f, 1.6f); }
    Flick = State == EState::Dead || State == EState::Strike ? 0.f : FMath::Clamp(FMath::Sin(PI * FMath::Clamp(FlickClock / .38f, 0.f, 1.f)) * 1.25f, 0.f, 1.f);
    if (State == EState::Dead)
    {
        DeathT += DeltaSeconds;
        Ease(RearW, 0.f, 3.f); Ease(Ext, 0.f, 4.f);
        if (DeathT < 4.f) Pose();
        return;
    }
    if (bHeld) { Pose(); return; }
    const bool bThere = Chuck && !Chuck->IsAstral() && InLeash(ChuckAt, 150.f);
    const bool bReach = bThere && !Chuck->IsSwinging() && FMath::Abs(static_cast<float>(ChuckAt.Z) - 34.65f - GroundZ) < 40.f;
    Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
    auto Sees = [&]()
    {
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(SnakeSight), false, this); Q.AddIgnoredActor(Chuck);
        return !GetWorld()->LineTraceSingleByChannel(Hit, Snout + FVector(0, 0, 10), ChuckAt, ECC_Visibility, Q);
    };
    const FVector MidBefore = BodyPoint(Length * .5f);
    switch (State)
    {
    case EState::Rest:
        Ease(RearW, 0.f, 2.f); Ease(Gape, 0.f, 4.f);
        if (bReach && FVector::Dist2D(HeadPos, ChuckAt) < NoticeRange && Sees()) { Aim = (ChuckAt - HeadPos).GetSafeNormal2D(); SetState(EState::Alert); break; }
        if (Post.bWander && StateTime > 5.f)
        {
            for (int32 Try = 0; Try < 12; ++Try)
            {
                const FVector G = Post.Feet + FRotator(0, Random.FRandRange(0, 360), 0).Vector() * Random.FRandRange(80, 260);
                if (!Blocked(G, G)) { WanderGoal = FVector(G.X, G.Y, GroundZ); SetState(EState::Wander); break; }
            }
        }
        break;
    case EState::Wander:
        Ease(RearW, 0.f, 2.f);
        if (bReach && FVector::Dist2D(HeadPos, ChuckAt) < NoticeRange && Sees()) { Aim = (ChuckAt - HeadPos).GetSafeNormal2D(); SetState(EState::Alert); break; }
        Crawl(WanderGoal, WanderSpeed, DeltaSeconds);
        if (FVector::Dist2D(HeadPos, WanderGoal) < 25.f || StateTime > 20.f) SetState(EState::Rest);
        break;
    case EState::Alert:
        // Head up a little, turned on him, tongue going; then after him.
        Aim = (ChuckAt - TrailPoint(Front)).GetSafeNormal2D();
        Ease(RearW, .45f, 2.f);
        if (StateTime > .45f) SetState(EState::Chase);
        break;
    case EState::Chase:
    {
        Ease(RearW, 0.f, 3.f); Ease(Gape, 0.f, 4.f);
        Lost = bReach ? 0.f : Lost + DeltaSeconds;
        if (Lost > 3.f) { SetState(EState::Return); break; }
        if (!bReach) break;
        const FVector F = ChultTempleInteriorFrameOf(ChuckAt);
        const FVector Goal = ChultTempleInteriorWorldOf(FVector(FMath::Clamp(F.X, Post.Leash.Min.X, Post.Leash.Max.X), FMath::Clamp(F.Y, Post.Leash.Min.Y, Post.Leash.Max.Y), F.Z));
        // Close enough to strike from where its front would leave the floor: it rears. Else on, until its snout is on him.
        if ((FVector::Dist2D(TrailPoint(Front), ChuckAt) < StrikeStart || Dist < 35.f) && Cooldown <= 0.f) {   // (its snout on him after a winding approach: rear anyway)
            Aim = (ChuckAt - TrailPoint(Front)).GetSafeNormal2D(); SetState(EState::Rear); break; }
        if (Dist > 14.f) Crawl(FVector(Goal.X, Goal.Y, GroundZ), ChaseSpeed, DeltaSeconds);
        break;
    }
    case EState::Rear:
        // The tell: the S drawn up, the head following him, a hiss through a parted mouth.
        Ease(RearW, 1.f, 1.f / TellTime * 1.15f); Ease(Gape, .18f, 1.f);
        Aim = FMath::Lerp(Aim, (ChuckAt - TrailPoint(Front)).GetSafeNormal2D(), FMath::Min(1.f, DeltaSeconds * 8.f)).GetSafeNormal2D();
        if (!bReach || FVector::Dist2D(TrailPoint(Front), ChuckAt) > StrikeStart + 40.f) { SetState(EState::Chase); break; }
        if (StateTime >= TellTime && Cooldown <= 0.f && RearW > .9f)
        {
            // As far as he is (to his middle, short of a few cm), no further than it can.
            StrikeReach = FMath::Clamp(static_cast<float>(FVector::Dist2D(TrailPoint(Front), ChuckAt)) - Ready - 6.f, 15.f, Reach);
            LastTell = StateTime; bBit = false; LastStrikeGap = 1e9f; ++Strikes; SetState(EState::Strike);
        }
        break;
    case EState::Strike:
    {
        // Committed: the S throws the head out along the aim, jaws wide.
        Ease(Ext, 1.f, 1.f / StrikeOut); Ease(Gape, 1.f, 1.f / StrikeOut);
        if (Chuck && Ext > .5f)
        {
            const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), R = Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const float Gap = FMath::Sqrt(static_cast<float>(FMath::PointDistToSegmentSquared(Snout, ChuckAt - FVector(0, 0, Half - R), ChuckAt + FVector(0, 0, Half - R)))) - R;
            LastStrikeGap = FMath::Min(LastStrikeGap, Gap);
            if (!bBit && Gap < BiteSlack && bReach)
            {
                bBit = true;
                if (Chuck->TakeBite(Snout, BiteDamage)) ++Bites;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_STRIKE bit=%d gap=%.1f sanity=%d"), Bites, Gap, Chuck->GetSanity());
            }
        }
        if (StateTime >= StrikeOut + StrikeHold) SetState(EState::Recoil);
        break;
    }
    case EState::Recoil:
        Ease(Ext, 0.f, 1.f / RecoilTime); Ease(Gape, .15f, 3.f);
        if (StateTime >= RecoilTime)
        {
            if (!bBit) UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SNAKE_STRIKE bit=0 gap=%.1f miss=1"), LastStrikeGap);
            Cooldown = Random.FRandRange(.7f, 1.3f);
            SetState(bReach && FVector::Dist2D(TrailPoint(Front), ChuckAt) < StrikeStart + 20.f ? EState::Rear : EState::Chase);
        }
        break;
    case EState::Return:
        Ease(RearW, 0.f, 3.f); Ease(Gape, 0.f, 4.f);
        if (bReach && FVector::Dist2D(HeadPos, ChuckAt) < NoticeRange * .7f) { SetState(EState::Alert); break; }
        Crawl(FVector(Post.Feet.X, Post.Feet.Y, GroundZ), WanderSpeed * 1.6f, DeltaSeconds);
        if (FVector::Dist2D(HeadPos, Post.Feet) < 40.f) SetState(EState::Rest);
        break;
    default: break;
    }
    {
        // How far its snout has ever been outside the chamber's rectangle (cm).
        const FVector F = ChultTempleInteriorFrameOf(HeadPos);
        const double Out = FMath::Max3(0.0, Post.Leash.Min.X - F.X, F.X - Post.Leash.Max.X) + FMath::Max3(0.0, Post.Leash.Min.Y - F.Y, F.Y - Post.Leash.Max.Y);
        WorstLeash = FMath::Max(WorstLeash, static_cast<float>(Out));
    }
    Pose();
    // How well the body keeps to its track: the mid-body's sideways motion to its own length.
    const FVector MidNow = BodyPoint(Length * .5f);
    const FVector V = (MidNow - MidBefore) / FMath::Max(DeltaSeconds, 1e-3f);
    if ((State == EState::Chase || State == EState::Wander || State == EState::Return) && V.Size2D() > 20.f)
    {
        const FVector T = (BodyPoint(Length * .5f - 2.f) - BodyPoint(Length * .5f + 2.f)).GetSafeNormal2D();
        const FVector V2(V.X, V.Y, 0);
        SlipSum += static_cast<float>((V2 - T * FVector::DotProduct(V2, T)).Size() / V2.Size());
        ++SlipSamples;
    }
}
