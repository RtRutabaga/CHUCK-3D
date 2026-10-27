#include "ChultTempleTraps.h"
#include "ChultTempleRubble.h"
#include "ChuckCharacter.h"
#include "ChultTempleInterior.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AChultTempleTraps::AChultTempleTraps()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // The fire jets' sprite quad: a hard reference, so the cook keeps this engine mesh.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    JetQuad = Quad.Object;
}

AChultTempleTraps* AChultTempleTraps::Find(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AChultTempleTraps> It(World); It; ++It) return *It;
    return nullptr;
}

void AChultTempleTraps::LoadSounds()
{
    if (Attenuation) return;
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 2200.f;
    auto Load = [this](TArray<USoundBase*>& Set, const TCHAR* Name, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
        {
            const FString Path = FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Name, I, Name, I);
            USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
            if (Sound) { Set.Add(Sound); Held.Add(Sound); } else ++MissingSounds;
        }
    };
    Load(SpikeRattle, TEXT("SFX_TempleSpikeRattle"), 2);
    Load(SpikeThrust, TEXT("SFX_TempleSpikeThrust"), 3);
    Load(DartClick, TEXT("SFX_TempleDartClick"), 2);
    Load(DartShot, TEXT("SFX_TempleDartShot"), 3);
    Load(DartHit, TEXT("SFX_TempleDartHit"), 3);
    Load(SlabCrack, TEXT("SFX_TempleSlabCrack"), 2);
    Load(SlabFall, TEXT("SFX_TempleSlabFall"), 2);
    // The trap run (2026-10-22; Tools/gen_temple_run_sfx.py).
    Load(AxeSwoosh, TEXT("SFX_TempleRunAxeSwoosh"), 3);
    Load(AxeCreak, TEXT("SFX_TempleRunAxeCreak"), 2);
    Load(FireHiss, TEXT("SFX_TempleRunFireHiss"), 2);
    Load(FireRoar, TEXT("SFX_TempleRunFireRoar"), 2);
    Load(SawSpin, TEXT("SFX_TempleRunSawSpin"), 2);
    Load(SawDrop, TEXT("SFX_TempleRunSawDrop"), 2);
    Load(TrapHit, TEXT("SFX_TempleRunHit"), 2);
}

void AChultTempleTraps::Play(const TArray<USoundBase*>& Set, const FVector& At, float Volume)
{
    if (Set.Num() == 0 || !bHearing) return;
    ++SoundsPlayed;
    UGameplayStatics::PlaySoundAtLocation(this, Set[FMath::RandRange(0, Set.Num() - 1)], At, Volume, FMath::FRandRange(.95f, 1.05f), 0.f, Attenuation);
}

void AChultTempleTraps::AddSpikeBand(UProceduralMeshComponent* Mesh, const FVector& Floor, const FVector& Along, float HalfDepth, float HalfWidth, float Phase)
{
    LoadSounds();
    FSpikeBand Band;
    Band.Mesh = Mesh; Band.Floor = Floor; Band.Along = Along.GetSafeNormal2D();
    Band.HalfDepth = HalfDepth; Band.HalfWidth = HalfWidth; Band.Phase = Phase;
    if (Mesh) Band.Base = Mesh->GetRelativeLocation();
    Spikes.Add(Band);
}

float AChultTempleTraps::SpikeTip(int32 Band, double Time) const
{
    // Down, then a rattle (tips just showing), a thrust, held up, drawn back.
    const float U = FMath::Fmod(static_cast<float>(Time) - Spikes[Band].Phase + SpikePeriod * 1000.f, SpikePeriod);
    // At rest their points stand 7 cm proud of the grate, so the band reads as a trap.
    if (U < 1.55f) return SpikeRest;
    if (U < 2.0f) return SpikeRest + 3.f + 1.5f * FMath::Sin(U * 70.f);
    if (U < 2.08f) return FMath::Lerp(SpikeRest + 3.f, SpikeTall, (U - 2.f) / .08f);
    if (U < 2.75f) return SpikeTall;
    return FMath::Lerp(SpikeTall, SpikeRest, FMath::SmoothStep(0.f, 1.f, (U - 2.75f) / .25f));
}

bool AChultTempleTraps::InSpikeBand(int32 Band, const FVector& Location, float Margin) const
{
    const FSpikeBand& B = Spikes[Band];
    const FVector D = Location - B.Floor;
    const FVector Side(-B.Along.Y, B.Along.X, 0);
    return FMath::Abs(FVector::DotProduct(D, B.Along)) < B.HalfDepth + Margin && FMath::Abs(FVector::DotProduct(D, Side)) < B.HalfWidth;
}

void AChultTempleTraps::AddLauncher(const FVector& Mouth, const FVector& Dir, float Span, float Period, float Phase)
{
    LoadSounds();
    FLauncher L; L.Mouth = Mouth; L.Dir = Dir.GetSafeNormal(); L.Span = Span; L.Period = Period; L.Phase = Phase;
    Launchers.Add(L);
}

void AChultTempleTraps::AddDart(UProceduralMeshComponent* Mesh)
{
    FDart D; D.Mesh = Mesh;
    if (Mesh) Mesh->SetVisibility(false);
    Darts.Add(D);
}

bool AChultTempleTraps::DartLineDanger(int32 L, double Time, float Lead) const
{
    // A cycle: the click at 0, darts leave at DartClickLead and +DartGap and cross in Span/DartSpeed.
    const FLauncher& La = Launchers[L];
    const float Cross = La.Span / DartSpeed;
    for (float Ahead = 0; Ahead <= Lead + .001f; Ahead += .05f)
    {
        const float U = FMath::Fmod(static_cast<float>(Time + Ahead) - La.Phase + La.Period * 1000.f, La.Period);
        if (U > DartClickLead - .08f && U < DartClickLead + DartGap + Cross + .1f) return true;
    }
    return false;
}

void AChultTempleTraps::AddSlab(UProceduralMeshComponent* Mesh)
{
    LoadSounds();
    FSlab S; S.Mesh = Mesh;
    if (Mesh) { S.Home = Mesh->GetComponentLocation(); S.HomeRotation = Mesh->GetComponentRotation(); }
    Slabs.Add(S);
}

void AChultTempleTraps::AddTorch(UPointLightComponent* Light, float Intensity)
{
    if (!Light) return;
    const FVector P = Light->GetComponentLocation();
    Torches.Add({Light, Intensity, static_cast<float>(P.X * .071 + P.Y * .043)});
}

void AChultTempleTraps::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* World = GetWorld();
    const double Now = World->GetTimeSeconds();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    // The traps are heard only from inside the temple (2026-10-20: the spike corridor
    // runs 14 m under the avenue, and its thrusts carried up to the road).
    // (Not from the rubble map, 2026-10-26: the trap run's last sections are only metres away through the rock.)
    bHearing = Chuck && InChultTempleInterior(Chuck->GetActorLocation(), 100.f) && !InChultTempleRubble(Chuck->GetActorLocation());
    TickSpikes(Chuck, Now);
    TickDarts(Chuck, Now, DeltaSeconds);
    TickSlabs(Chuck, Now, DeltaSeconds);
    TickAxes(Chuck, Now);
    TickJets(Chuck, Now, DeltaSeconds);
    TickSaws(Chuck, Now, DeltaSeconds);
    if (Now >= NextFlicker)
    {
        NextFlicker = Now + .05;
        const float T = static_cast<float>(Now);
        for (const FTorch& L : Torches)
            if (L.Light.IsValid()) L.Light->SetIntensity(L.Intensity * (.93f + .06f * FMath::Sin(T * 6.3f + L.Phase) + .04f * FMath::Sin(T * 10.7f + L.Phase * 1.7f)));
    }
}

void AChultTempleTraps::TickSpikes(AChuckCharacter* Chuck, double Now)
{
    const FVector At = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
    for (int32 I = 0; I < Spikes.Num(); ++I)
    {
        FSpikeBand& B = Spikes[I];
        const float Tip = SpikeTip(I, Now);
        if (B.Mesh.IsValid()) B.Mesh->SetRelativeLocation(B.Base + FVector(0, 0, Tip - SpikeTall));
        const float U = FMath::Fmod(static_cast<float>(Now) - B.Phase + SpikePeriod * 1000.f, SpikePeriod);
        const int32 Stage = U < 1.55f ? 0 : U < 2.f ? 1 : U < 2.75f ? 2 : 3;
        // Only within earshot: the sounds of a band he can't hear aren't started.
        const bool Near = Chuck && FVector::Dist(At, B.Floor) < 2600.f;
        if (Stage != B.Stage)
        {
            if (Stage == 1 && Near) Play(SpikeRattle, B.Floor, .55f);
            if (Stage == 2) { ++Thrusts; if (Near) Play(SpikeThrust, B.Floor, .9f); }
            B.Stage = Stage;
        }
        if (!Chuck || Tip <= SpikeDangerTip || Chuck->IsAstral()) continue;
        const float Feet = At.Z - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (Feet > B.Floor.Z + Tip - 4.f || !InSpikeBand(I, At)) continue;
        // Knocked back out of the band the way he is nearer to.
        const float Along = FVector::DotProduct(At - B.Floor, B.Along);
        const FVector From = At - B.Along * (Along >= 0 ? 60.f : -60.f);
        if (Chuck->TakeBite(From, 1, false, true))
        {
            ++SpikeHits;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SPIKE_HIT band=%d sanity=%d tip=%.0f"), I, Chuck->GetSanity(), Tip);
        }
    }
}

void AChultTempleTraps::Fire(int32 L)
{
    const FLauncher& La = Launchers[L];
    for (FDart& D : Darts)
    {
        if (D.bFlying || D.StuckUntil > 0 || !D.Mesh.IsValid()) continue;
        D.Launcher = L; D.Pos = La.Mouth; D.Dir = La.Dir; D.Left = La.Span; D.bFlying = true; D.bDodged = false;
        D.Mesh->SetWorldLocationAndRotation(D.Pos, D.Dir.Rotation());
        D.Mesh->SetVisibility(true);
        ++DartsFired;
        return;
    }
}

void AChultTempleTraps::TickDarts(AChuckCharacter* Chuck, double Now, float Dt)
{
    const FVector At = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
    for (int32 L = 0; L < Launchers.Num(); ++L)
    {
        FLauncher& La = Launchers[L];
        const float U = FMath::Fmod(static_cast<float>(Now) - La.Phase + La.Period * 1000.f, La.Period);
        const bool Near = Chuck && FVector::Dist(At, La.Mouth) < 2400.f;
        auto Crossed = [&](float Mark) { return La.LastU >= 0 && ((La.LastU < Mark && U >= Mark) || (La.LastU > U && U >= Mark)); };
        if (Crossed(0.f) || (La.LastU > U && La.LastU >= 0)) { if (Near) Play(DartClick, La.Mouth, .6f); }
        if (Crossed(DartClickLead)) { Fire(L); if (Near) Play(DartShot, La.Mouth, .8f); }
        if (Crossed(DartClickLead + DartGap)) { Fire(L); if (Near) Play(DartShot, La.Mouth, .8f); }
        La.LastU = U;
    }
    const float Half = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
    const float Radius = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    for (FDart& D : Darts)
    {
        if (!D.Mesh.IsValid()) continue;
        if (D.StuckUntil > 0)
        {
            if (Now >= D.StuckUntil) { D.StuckUntil = -1; D.Mesh->SetVisibility(false); }
            continue;
        }
        if (!D.bFlying) continue;
        const float Step = FMath::Min(DartSpeed * Dt, D.Left);
        const FVector Prev = D.Pos;
        D.Pos += D.Dir * Step; D.Left -= Step;
        D.Mesh->SetWorldLocation(D.Pos);
        if (Chuck && !Chuck->IsAstral())
        {
            // The dart's path this frame against his capsule's axis.
            FVector OnDart, OnBody;
            FMath::SegmentDistToSegmentSafe(Prev, D.Pos, At - FVector(0, 0, Half - Radius), At + FVector(0, 0, Half - Radius), OnDart, OnBody);
            if (FVector::Dist(OnDart, OnBody) < Radius + 1.5f)
            {
                if (Chuck->TakeBite(Prev, 1, false))
                {
                    ++DartHits; D.bFlying = false; D.Mesh->SetVisibility(false);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_DART_HIT launcher=%d sanity=%d"), D.Launcher, Chuck->GetSanity());
                    continue;
                }
                if (!D.bDodged) { D.bDodged = true; ++DartDodges; }
            }
        }
        if (D.Left <= 0)
        {
            // Into the far wall: it sticks a moment.
            D.bFlying = false; D.StuckUntil = Now + 1.2;
            if (Chuck && FVector::Dist(At, D.Pos) < 2000.f) Play(DartHit, D.Pos, .5f);
        }
    }
}

void AChultTempleTraps::TickSlabs(AChuckCharacter* Chuck, double Now, float Dt)
{
    UPrimitiveComponent* Floor = nullptr;
    if (Chuck && Chuck->GetCharacterMovement()->IsMovingOnGround()) Floor = Chuck->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent();
    bool Clear = !Chuck || Chuck->IsAstral();
    if (!Clear)
    {
        const FVector D = Chuck->GetActorLocation() - RiftCentre;
        Clear = FMath::Abs(FVector::DotProduct(D, RiftAxisA)) > RiftHalfA || FMath::Abs(FVector::DotProduct(D, RiftAxisB)) > RiftHalfB;
    }
    for (FSlab& S : Slabs)
    {
        UProceduralMeshComponent* M = S.Mesh.Get();
        if (!M) continue;
        switch (S.State)
        {
        case ESlab::Whole:
            if (Floor == M)
            {
                S.State = ESlab::Cracking; S.At = Now;
                Play(SlabCrack, S.Home, .8f);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SLAB_CRACK at=%s"), *S.Home.ToString());
            }
            break;
        case ESlab::Cracking:
        {
            const float Age = static_cast<float>(Now - S.At);
            // A shudder growing toward the fall.
            const float J = 1.2f * Age / SlabDelay;
            M->SetWorldLocationAndRotation(S.Home + FVector(FMath::FRandRange(-J, J), FMath::FRandRange(-J, J), -Age * 3.f), S.HomeRotation + FRotator(FMath::FRandRange(-J, J) * .4f, 0, FMath::FRandRange(-J, J) * .4f));
            if (Age >= SlabDelay)
            {
                S.State = ESlab::Falling; S.At = Now; ++SlabsFallen;
                S.Velocity = FVector(0, 0, -60.f);
                S.Spin = FRotator(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-60.f, 60.f));
                M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Play(SlabFall, S.Home, 1.f);
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SLAB_FALL at=%s"), *S.Home.ToString());
            }
            break;
        }
        case ESlab::Falling:
            S.Velocity.Z -= 980.f * Dt;
            M->SetWorldLocationAndRotation(M->GetComponentLocation() + S.Velocity * Dt, M->GetComponentRotation() + S.Spin * Dt);
            if (Now - S.At > 2.0) { S.State = ESlab::Gone; M->SetVisibility(false); }
            break;
        case ESlab::Gone:
            if (Now - S.At > SlabReturn && Clear)
            {
                M->SetWorldLocationAndRotation(S.Home, S.HomeRotation);
                M->SetVisibility(true);
                M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                S.State = ESlab::Whole; ++SlabsRestored;
            }
            break;
        }
    }
}
