#include "ShipEndingScene.h"
#include "ShipEnding.h"
#include "ShipSetting.h"
#include "ShipCrew.h"
#include "ShipStorm.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DockGameMode.h"
#include "AudioDevice.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"

// The fore topsail cutscene (Claude, 2026-11-08; 2026-11-09 the knock-down and the storm shots; ShipEndingScene.h). Shots:
// "Crosstrees" (close, as he takes the fore topsail yard and goes out along it), "SeaLevel" (low over the swell off her lee
// bow, she driving past), "Aerial" (high and far off her weather quarter, lightning beyond her), "Jeffries" (at the mainmast,
// seeing them), "Ruptures" (the Astral ones in the sea round her, sliding aft), "Clewline" (close on him at the yard),
// "Astern" (low, far astern: her stern lifting and dropping, the ruptures glowing round her), "Burning" (over the bow: the
// Phlegethos rupture drifting toward her), "Comber" (from off her lee side: the great sea rising beyond her, breaking over
// her, knocking her over), "Grip" (close: he swings out on the yardarm and loses his grip), "Below" (from above as he falls
// into it). The shots off her hold the horizon level, so she is seen to roll and to go over.
using namespace Ship;

namespace
{
    // Jeffries' lines (the user's takes, 2026-11-08), at their beats.
    struct FBeat { float At; const TCHAR* Id; };
    const FBeat Beats[] = {{12.7f, TEXT("JEF_X01")}, {15.8f, TEXT("JEF_01")}, {19.3f, TEXT("JEF_02")}, {23.4f, TEXT("JEF_03")}, {27.8f, TEXT("JEF_04")},
        {31.3f, TEXT("JEF_05")}, {37.45f, TEXT("JEF_X02")}, {38.5f, TEXT("JEF_06")}};   // (X02: the sea breaking over him)
    constexpr int32 BeatCount = UE_ARRAY_COUNT(Beats);
    float SeaLocalZ(const UWorld* World, const FVector2D& At)
    {
        const AShipSetting* S = AShipSetting::Get(World);
        const FVector W = ShipWorld(FVector(At.X, At.Y, WaterZ));
        return S ? static_cast<float>(S->SeaPlaneZ(W) - ShipOrigin().Z) : WaterZ;
    }
    // The fore topsail yard's grip on Side, K out from the topmast (world), from the ship's registered grips.
    FVector TopsailGrip(float Side, int32 K)
    {
        TArray<FVector> Found;
        for (const FChuckSwingGrip& G : GetChuckSwingGrips())
        {
            const FVector L = ShipLocal(G.Grip);
            if (L.X > 0 && FMath::Abs(L.Z - (ForeTopZ + 760.f)) < 12.f && FMath::Sign(L.Y) == Side) Found.Add(G.Grip);
        }
        Found.Sort([](const FVector& A, const FVector& B) { return FMath::Abs(ShipLocal(A).Y) < FMath::Abs(ShipLocal(B).Y); });
        return Found.Num() ? Found[FMath::Clamp(K, 0, Found.Num() - 1)] : ShipWorld(FVector(ForeMastX + 63, Side * (75 + 90 * K), ForeTopZ + 760));
    }
}

AShipForeTopsailScene::AShipForeTopsailScene()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AShipForeTopsailScene* AShipForeTopsailScene::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AShipForeTopsailScene> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipForeTopsailScene* AShipForeTopsailScene::Play(UWorld* World, float InSide)
{
    AShipForeTopsailScene* S = Get(World);
    if (!S)
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        S = World->SpawnActor<AShipForeTopsailScene>(ShipOrigin(), FRotator::ZeroRotator, P);
    }
    if (S && !S->bPlaying && !S->bFinished) S->Begin(InSide);
    return S;
}

void AShipForeTopsailScene::Begin(float InSide)
{
    Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    Side = InSide >= 0.f ? 1.f : -1.f;
    bPlaying = true; Clock = 0.f; Next = 0; bCut = true;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Camera = GetWorld()->SpawnActor<ACameraActor>(C->GetActorLocation(), FRotator::ZeroRotator, P);
    if (Camera) Camera->GetCameraComponent()->bConstrainAspectRatio = false;
    if (auto* PC = Cast<APlayerController>(C->GetController())) { C->DisableInput(PC); C->SetTestStick(FVector2D::ZeroVector); C->SetRunHeld(false); }
    // The ruptures: Astral ones round her (Seed: their ragged shapes), the burning one far ahead, coming down on her bow.
    const float Off = -Side;   // (most on the far side from him, some on his)
    Rifts = {{{2600, Off * 1500}, 260, 1.f, false}, {{1700, Side * 1700}, 200, 2.f, false}, {{900, Off * 2200}, 320, 3.f, false},
        {{-300, Side * 1500}, 230, 4.f, false}, {{-1200, Off * 1300}, 280, 5.f, false}, {{-2300, Side * 2100}, 340, 6.f, false},
        {{3800, Side * 900}, 240, 7.f, false}, {{-3200, Off * 2000}, 300, 8.f, false}, {{0, 0}, 330, 9.f, true}};
    // The knock-down heels her toward his side (the sign of a roll that takes the sea's up away from him).
    KnockRoll = Side * KnockDeg * (FQuat(FRotator(0, 0, 10)).RotateVector(FVector::UpVector).Y < 0 ? 1.f : -1.f);
    MaxHeel = 0.f; bHeld = bHitDone = false; ReleasedAt = -1.f; Phi = PhiRate = 0.f; StrikeNext = 0;
    Facing = FQuat(FRotator(0, Side > 0 ? 90.f : -90.f, 0));
    // Where he will fall: hanging from the yardarm's grip with her knocked over, swung out to the true vertical.
    {
        const FVector Grip = ShipLocal(TopsailGrip(Side, 3));
        const FQuat Swung(-Facing.GetRightVector(), FMath::DegreesToRadians(KnockDeg));
        LandAt = SlipAt + .84f + PredictFall(Grip + Swung.RotateVector(Facing.RotateVector(FVector(-5.5f, 0, -31.f))), FVector::ZeroVector, Land);
    }
    Comber = NewObject<UProceduralMeshComponent>(this, TEXT("Comber"));
    Comber->SetupAttachment(RootComponent); Comber->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comber->RegisterComponent(); Comber->SetWorldLocation(ShipOrigin());
    Comber->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Ship/M_ShipComber.M_ShipComber")));
    Ruptures = NewObject<UProceduralMeshComponent>(this, TEXT("SeaRuptures"));
    Ruptures->SetupAttachment(RootComponent); Ruptures->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ruptures->SetCastShadow(false);
    Ruptures->RegisterComponent(); Ruptures->SetWorldLocation(ShipOrigin());
    Ruptures->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_SeaRuptureAstral.M_SeaRuptureAstral")));
    Ruptures->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist")));
    Ruptures->SetMaterial(2, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_SeaRupturePhlegethos.M_SeaRupturePhlegethos")));
    Ruptures->SetMaterial(3, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_SeaRuptureFoam.M_SeaRuptureFoam")));
    bShots = FParse::Param(FCommandLine::Get(), TEXT("ChuckShipSceneShots"));
    for (int32 I = 0; I < 4; ++I)
    {
        auto* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("RiftLight%d"), I));
        L->SetupAttachment(RootComponent); L->SetUsingAbsoluteLocation(true); L->SetCastShadows(true);   // (shadowed: unshadowed, they lit the crew's strand hair through the ship)
        L->SetAttenuationRadius(I == 3 ? 2400.f : 1600.f); L->SetIntensity(0.f);
        L->SetLightColor(I == 3 ? FLinearColor(1.f, .32f, .08f) : FLinearColor(.62f, .3f, 1.f));
        L->RegisterComponent(); Lights.Add(L);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_START side=%.0f at=%s"), Side, *ShipLocal(C->GetActorLocation()).ToString());
}

void AShipForeTopsailScene::Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov, bool bLevel)
{
    if (!Camera) return;
    if (FCString::Strcmp(Name, Shot) != 0)
    {
        bCut = true; Shot = Name;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_SHOT shot=%s clock=%.2f"), Name, Clock);
        if (AChuckCharacter* C = Chuck.Get()) if (auto* PC = Cast<APlayerController>(C->GetController())) PC->SetViewTarget(Camera);
    }
    const float Dt = GetWorld()->GetDeltaSeconds();
    Eye = bCut ? InEye : FMath::VInterpTo(Eye, InEye, Dt, 3.f);
    Aim = bCut ? InAim : FMath::VInterpTo(Aim, InAim, Dt, 4.f);
    bCut = false;
    FRotator Look = (Aim - Eye).Rotation();
    if (bLevel)
        if (const AShipSetting* S = AShipSetting::Get(GetWorld())) Look = FRotationMatrix::MakeFromXZ((Aim - Eye).GetSafeNormal(), S->GetSeaUp()).Rotator();
    // The sea's blow: a shake, dying away.
    const float Shake = 1.f - FMath::SmoothStep(WaveHit, WaveHit + 1.4f, Clock);
    if (Clock > WaveHit && Shake > 0.f)
    {
        const float A = Shake * (FCString::Strcmp(Name, TEXT("Grip")) == 0 ? 5.f : 9.f);
        Look.Roll += 1.6f * Shake * FMath::Sin(Clock * 37.f);
        Camera->SetActorLocationAndRotation(Eye + FVector(FMath::Sin(Clock * 53.f), FMath::Sin(Clock * 47.f + 1.f), FMath::Sin(Clock * 61.f + 2.f)) * A, Look);
    }
    else Camera->SetActorLocationAndRotation(Eye, Look);
    Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

FVector2D AShipForeTopsailScene::RiftAt(const FRupture& R) const
{
    // Sliding aft past her on the current; the burning one timed to be under him as he reaches the sea.
    if (R.bBurning) return FVector2D(Land.X + (LandAt - Clock) * Drift, Land.Y);
    return R.At - FVector2D(Clock * Drift, 0.f);
}

void AShipForeTopsailScene::BuildRuptures()
{
    // Each rupture is a ragged disc on the sea whose material traces the view down a well under it (the sea pouring over
    // its lip and down its sides, the plane it opens onto at the bottom: Tools/create_sea_rupture_materials.py), with the
    // white water drawn round into it on the sea about its lip. Sections: 0 Astral holes, 1 the Astral's oil film standing
    // at their lips, 2 the Phlegethos hole, 3 the foam rings.
    if (!Ruptures) return;
    // (Each vertex on the sea's plane where it is: she may be knocked far over.)
    auto SeaZ = [&](double X, double Y) { return SeaLocalZ(GetWorld(), FVector2D(X, Y)) + 28.f; };
    TArray<FVector> V[4], N[4]; TArray<int32> T[4]; TArray<FVector2D> UV[4], UV1[4], UV2[4];
    constexpr int32 Edge = 48;
    auto Lip = [](float A, float Seed, float Rag)
    {
        return 1.03f + Rag * (.08f * FMath::Sin(3.f * A + Seed) + .05f * FMath::Sin(7.f * A + 2.f * Seed) + .03f * FMath::Sin(13.f * A + Seed * 3.f));
    };
    auto Hole = [&](int32 S, const FVector2D& C, float Z, float R, float Seed, float Rag, float Open)
    {
        const int32 Base = V[S].Num();
        V[S].Add(FVector(C.X, C.Y, SeaZ(C.X, C.Y))); UV[S].Add(FVector2D::ZeroVector);
        for (int32 I = 0; I < Edge; ++I)
        {
            const float A = 2.f * PI * I / Edge, K = Lip(A, Seed, Rag);
            const double Px = C.X + FMath::Cos(A) * R * K * 1.25f, Py = C.Y + FMath::Sin(A) * R * K * .85f;
            V[S].Add(FVector(Px, Py, SeaZ(Px, Py)));
            UV[S].Add(FVector2D(FMath::Cos(A) * K, FMath::Sin(A) * K));   // (the well's wall is the unit circle)
        }
        for (int32 I = 0; I <= Edge; ++I) { N[S].Add(FVector::UpVector); UV1[S].Add(FVector2D(R, Open)); UV2[S].Add(FVector2D(Seed, 0.f)); }
        for (int32 I = 0; I < Edge; ++I) T[S].Append({Base, Base + 1 + (I + 1) % Edge, Base + 1 + I});
    };
    auto Foam = [&](const FVector2D& C, float Z, float R, float Seed, float Rag, float Kind, float Open)
    {
        const int32 Base = V[3].Num();
        for (int32 I = 0; I <= Edge; ++I)
        {
            const float A = 2.f * PI * I / Edge, K = Lip(A, Seed, Rag);
            const FVector2D D(FMath::Cos(A) * 1.25f, FMath::Sin(A) * .85f);
            const FVector2D In = C + D * R * K * .99f, Out = C + D * (R * K * 1.55f + 60.f);
            V[3].Add(FVector(In.X, In.Y, SeaZ(In.X, In.Y) + 1.f)); V[3].Add(FVector(Out.X, Out.Y, SeaZ(Out.X, Out.Y) - 6.f));
            UV[3].Add(FVector2D(I / float(Edge), 0.f)); UV[3].Add(FVector2D(I / float(Edge), 1.f));
            for (int32 J = 0; J < 2; ++J) { N[3].Add(FVector::UpVector); UV1[3].Add(FVector2D(Kind, Open)); UV2[3].Add(FVector2D::ZeroVector); }
        }
        for (int32 I = 0; I < Edge; ++I) { const int32 A = Base + I * 2; T[3].Append({A, A + 2, A + 3, A, A + 3, A + 1}); }
    };
    int32 Near = 0;
    for (const FRupture& R : Rifts)
    {
        const FVector2D C = RiftAt(R);
        const float Open = R.bBurning ? FMath::SmoothStep(BurningAt - 5.f, BurningAt - 1.f, Clock) : FMath::SmoothStep(5.f + R.Seed * .7f, 9.f + R.Seed * .7f, Clock);
        if (Open <= .01f) continue;
        const float Z = SeaLocalZ(GetWorld(), C) + 28.f, Rr = R.R * (.25f + .75f * Open);
        if (!R.bBurning)
        {
            Hole(0, C, Z, Rr, R.Seed, 1.f, Open);
            Foam(C, Z, Rr, R.Seed, 1.f, 0.f, Open);
            // A low veil of the Astral's oily film standing up round its lip.
            const int32 Base = V[1].Num(); constexpr int32 Ring = 30;
            for (int32 I = 0; I <= Ring; ++I)
            {
                const float A = 2.f * PI * (I % Ring) / Ring, K = Lip(A, R.Seed, 1.f);
                const FVector2D P = C + FVector2D(FMath::Cos(A) * Rr * K * 1.25f, FMath::Sin(A) * Rr * K * .85f);
                const float H = 25.f + 45.f * Open * (1.f + .3f * FMath::Sin(A * 5.f + Clock));
                const float Zp = SeaZ(P.X, P.Y);
                V[1].Add(FVector(P.X, P.Y, Zp - 4)); V[1].Add(FVector(P.X, P.Y, Zp + H));
                N[1].Add(FVector(FMath::Cos(A), FMath::Sin(A), 0)); N[1].Add(FVector(FMath::Cos(A), FMath::Sin(A), 0));
                UV[1].Add(FVector2D(I / float(Ring) * 3.f, 0.f)); UV[1].Add(FVector2D(I / float(Ring) * 3.f, 1.f));
                UV1[1].Add(FVector2D::ZeroVector); UV1[1].Add(FVector2D::ZeroVector); UV2[1].Add(FVector2D::ZeroVector); UV2[1].Add(FVector2D::ZeroVector);
            }
            for (int32 I = 0; I < Ring; ++I) { const int32 A = Base + I * 2; T[1].Append({A, A + 2, A + 3, A, A + 3, A + 1, A, A + 3, A + 2, A, A + 1, A + 3}); }
            // Light from the nearest three, welling up out of them.
            if (Near < 3 && FVector2D::Distance(C, FVector2D(0, 0)) < 3200.f)
            {
                Lights[Near]->SetWorldLocation(ShipWorld(FVector(C.X, C.Y, Z + 160.f)));
                Lights[Near]->SetIntensity(Open * (6000.f + 1200.f * FMath::Sin(Clock * 2.3f + R.Seed)));
                ++Near;
            }
        }
        else
        {
            Hole(2, C, Z, Rr, R.Seed, .7f, Open);
            Foam(C, Z, Rr, R.Seed, .7f, 1.f, Open);
            Lights[3]->SetWorldLocation(ShipWorld(FVector(C.X, C.Y, Z + 450.f)));
            Lights[3]->SetIntensity(Open * (5200.f + 1400.f * FMath::Sin(Clock * 3.1f) * FMath::Sin(Clock * 1.7f + 1.f)));
        }
    }
    for (int32 I = Near; I < 3; ++I) Lights[I]->SetIntensity(0.f);
    for (int32 S = 0; S < 4; ++S)
    {
        if (T[S].Num()) Ruptures->CreateMeshSection(S, V[S], T[S], N[S], UV[S], UV1[S], UV2[S], TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        else Ruptures->ClearMeshSection(S);
    }
}

void AShipForeTopsailScene::DriveChuck(float Dt)
{
    AChuckCharacter* C = Chuck.Get();
    if (!C) return;
    if (bHeld) { HoldChuck(Dt); return; }
    auto Push = [&](const FVector& D) { C->SetTestStickWorld(D); C->AddMovementInput(D, 1.f, true); };
    if (!bCaught)
    {
        if (C->IsSwinging()) { bCaught = true; LeapWait = 0.f; UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_CAUGHT clock=%.2f at=%s"), Clock, *ShipLocal(C->GetActorLocation()).ToString()); }
        // At the shrouds' head he takes the fore topsail yard's first grip (behind the cut to the first shot).
        else if (!bPlaced)
        {
            bPlaced = true;
            const FVector G = TopsailGrip(Side, 0);
            const TArray<FChuckSwingGrip>& All = GetChuckSwingGrips();
            C->SetActorRotation(FRotator(0, Side > 0 ? 90.f : -90.f, 0));   // (facing out along the yard)
            for (int32 I = 0; I < All.Num(); ++I) if (All[I].Grip.Equals(G, 1.f)) C->CatchSwingGrip(I);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_PLACED clock=%.2f side=%.0f swinging=%d"), Clock, Side, C->IsSwinging());
        }
        return;
    }
    // Out along the yard to the yardarm, where the clewline's quarter block is: three grips (the last; none beyond to catch him).
    if (YardLeaps < 3 && Clock < ClewAt)
    {
        Push(FVector(0, Side, 0));
        LeapWait += Dt;
        if (C->IsSwinging() && LeapWait > .8f) { C->JumpPressed(); ++YardLeaps; LeapWait = 0.f; }
        return;
    }
    C->SetTestStick(FVector2D::ZeroVector);
    if (Clock >= SlipAt && !C->IsSwingLeaping())
    {
        // The yard jolts as she goes over: from here the scene holds him (SwingSlip) about the grip he has.
        const int32 Gi = C->GetSwingGrip();
        SlipGrip = GetChuckSwingGrips().IsValidIndex(Gi) ? GetChuckSwingGrips()[Gi].Grip : TopsailGrip(Side, 3);
        C->BeginSceneHold();
        bHeld = true; Phi = 0.f; PhiRate = 0.f;
        PrevCentre = SlipGrip + Facing.RotateVector(FVector(-5.5f, 0, -31.f));
        const FQuat Swung(-Facing.GetRightVector(), FMath::DegreesToRadians(KnockDeg));
        LandAt = SlipAt + .84f + PredictFall(ShipLocal(SlipGrip) + Swung.RotateVector(Facing.RotateVector(FVector(-5.5f, 0, -31.f))), FVector::ZeroVector, Land);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_SLIP clock=%.2f grip=%s land=%s"), Clock, *ShipLocal(SlipGrip).ToString(), *Land.ToString());
        HoldChuck(Dt);
    }
}

void AShipForeTopsailScene::HoldChuck(float Dt)
{
    AChuckCharacter* C = Chuck.Get();
    if (!C || bPlunged) return;
    const AShipSetting* Ship = AShipSetting::Get(GetWorld());
    const FVector Up = Ship ? Ship->GetSeaUp() : FVector::UpVector;
    const float T = Clock - SlipAt, Slip = C->GetSceneClipLength(0);
    if (!bLetGo)
    {
        // Hanging from the yardarm, he swings out after the true vertical as she goes over (a pendulum about the grip,
        // kicked by the yard's jolt), his grip giving way (the clip) until it lets go.
        const FVector Out = Facing.GetForwardVector();
        const float HeelOut = FMath::Atan2(static_cast<float>(-FVector::DotProduct(Up, Out)), static_cast<float>(Up.Z));   // + with the true vertical outboard
        if (T > .08f && T < .18f) PhiRate += 7.f * Dt;
        PhiRate += (-(Phi - HeelOut) * 16.f - PhiRate * 1.6f) * Dt;
        Phi += PhiRate * Dt;
        const FQuat R(-Facing.GetRightVector(), Phi);
        const FVector Centre = SlipGrip + R.RotateVector(Facing.RotateVector(FVector(-5.5f, 0, -31.f)));
        C->SetScenePose(Centre, R * Facing);
        C->SetSceneClip(0, FMath::Min(T, Slip), .12f);
        if (T >= Slip * .7f)
        {
            // Off: from where he is, as he was moving.
            bLetGo = bThrown = true; ReleasedAt = Clock;
            FallP = Centre; FallV = Dt > 0.f ? (Centre - PrevCentre) / Dt : FVector::ZeroVector; FallRot = R * Facing;
            LandAt = Clock + PredictFall(ShipLocal(FallP), FallV, Land);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_THROWN clock=%.2f from=%s fall_s=%.2f land=%s swing_deg=%.1f heel_deg=%.1f lost_grip=1"),
                Clock, *ShipLocal(FallP).ToString(), LandAt - Clock, *Land.ToString(), FMath::RadiansToDegrees(Phi), FMath::RadiansToDegrees(HeelOut));
        }
        PrevCentre = Centre;
        return;
    }
    // Falling: straight down, the sea's way, turning upright to it, clawing after the yard.
    const float G = -static_cast<float>(C->GetCharacterMovement()->GetGravityZ());
    FallV += -Up * G * Dt;
    FallP += FallV * Dt;
    FallRot = FQuat::Slerp(FallRot, FQuat::FindBetweenNormals(FVector::UpVector, Up) * Facing, FMath::Min(1.f, Dt * 2.5f));
    C->SetScenePose(FallP, FallRot);
    if (T < Slip) C->SetSceneClip(0, T, 0.f);
    else C->SetSceneClip(1, T - Slip, .2f);
}

float AShipForeTopsailScene::PredictFall(const FVector& P0, const FVector& V0, FVector& Out) const
{
    // (Ship frame; the sea knocked over to KnockRoll; its plane through the waterline at her middle.)
    const FVector Up = FQuat(FRotator(0, 0, KnockRoll)).RotateVector(FVector::UpVector);
    const AChuckCharacter* C = Chuck.Get();
    const float G = C ? -static_cast<float>(C->GetCharacterMovement()->GetGravityZ()) : 980.f;
    FVector P = P0, V = V0;
    float T = 0.f;
    constexpr float Step = 1.f / 120.f;
    while (T < 6.f && FVector::DotProduct(P - FVector(0, 0, WaterZ), Up) > 70.f) { V += -Up * G * Step; P += V * Step; T += Step; }
    Out = P;
    return T;
}

void AShipForeTopsailScene::Knock()
{
    // The great sea strikes her at WaveHit: she goes over in about a second, a little past and back, and stays down.
    AShipSetting* Ship = AShipSetting::Get(GetWorld());
    if (!Ship) return;
    const float W = FMath::SmoothStep(WaveHit - .1f, WaveHit + .95f, Clock);
    const float Over = 1.f + .1f * FMath::Exp(-FMath::Square((Clock - WaveHit - 1.15f) / .45f));
    Ship->SetKnock(KnockRoll * Over, W);
    const FVector Up = Ship->GetSeaUp();
    MaxHeel = FMath::Max(MaxHeel, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Up.Z), -1.f, 1.f))));
    // Her side takes it: the white water bursting up over the weather rail and across the deck.
    if (!bHitDone && Clock >= WaveHit)
    {
        bHitDone = true;
        if (AShipStorm* Storm = AShipStorm::Get(GetWorld()))
        {
            const float From = -Side;
            for (int32 K = 0; K < 9; ++K)
            {
                const float X = FMath::Lerp(-1300.f, 1400.f, K / 8.f) + FMath::FRandRange(-80.f, 80.f);
                const FVector Rail = ShipWorld(FVector(X, From * (ShipOuterHalf(X, RailZ) + 30.f), RailZ + 40.f));
                Storm->SprayNow(Rail, 2.2f);
                Storm->BillowNow(Rail + FVector(0, 0, 120.f + 90.f * (K % 3)), 4, 380.f);
                Storm->BillowNow(Rail + FVector(0, -From * 120.f, 420.f + 60.f * (K % 2)), 2, 520.f);
            }
            Storm->DeckWaveNow(-450.f, 1.7f, From);
            Storm->DeckWaveNow(650.f, 1.6f, From);
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_WAVE clock=%.2f from=%.0f knock_deg=%.1f"), Clock, -Side, KnockRoll);
    }
}

void AShipForeTopsailScene::BuildComber()
{
    // The great sea (M_ShipComber): a long ridge of water running down on her from the far side, rising as it comes, its
    // crest curling over and plunging onto her side at WaveHit, then falling away into white water under her.
    if (!Comber) return;
    const float Show = FMath::SmoothStep(BurningAt, BurningAt + 1.5f, Clock) * (1.f - FMath::SmoothStep(WaveHit + .3f, WaveHit + 1.1f, Clock));
    if (Show <= .001f) { if (Comber->GetNumSections()) Comber->ClearAllMeshSections(); return; }
    // Its crest's distance from her middle (cm, toward the far side), its height over the sea, and its curl.
    const float D = FMath::Max(560.f + 1500.f * (WaveHit - Clock), 520.f);   // (against her side, where it breaks)
    const float H = Show * (250.f + 1050.f * FMath::SmoothStep(BurningAt, WaveHit - .7f, Clock)) * (1.f - .85f * FMath::SmoothStep(WaveHit - .05f, WaveHit + .7f, Clock));
    const float Curl = FMath::SmoothStep(WaveHit - 1.6f, WaveHit - .1f, Clock);
    const float Broken = FMath::SmoothStep(WaveHit - .2f, WaveHit + .6f, Clock);
    // Its profile: cm across from the crest toward her (at 9 m high), height as a fraction of H, foam, thin water.
    struct FPt { float D, Hf, Foam, Thin; };
    const FPt Prof[] = {
        {-3000.f, 0.f, 0.f, 0.f}, {-1800.f, .12f, 0.f, .05f}, {-1000.f, .38f, .02f, .1f}, {-500.f, .7f, .05f, .25f}, {-200.f, .9f, .15f, .45f},
        {0.f, 1.f, .45f, .7f},
        {120.f + 200.f * Curl, .97f - .05f * Curl, .8f, .85f}, {190.f + 330.f * Curl, .86f - .18f * Curl, 1.f, .8f}, {210.f + 360.f * Curl, .7f - .28f * Curl, 1.f, .6f},
        {150.f + 250.f * Curl, .6f - .25f * Curl, .7f, .3f}, {120.f + 170.f * Curl, .48f - .2f * Curl, .35f, .2f},
        {100.f + 60.f * Curl, .38f - .1f * Curl, .22f, .15f}, {200.f + 100.f * Curl, .18f - .05f * Curl, .3f, .1f}, {380.f + 150.f * Curl, .06f, .7f, .05f},
        {560.f + 200.f * Curl, 0.f, .85f, 0.f}};
    constexpr int32 NP = UE_ARRAY_COUNT(Prof), NX = 64;
    const float S = FMath::Max(H, 1.f) / 900.f;
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> Col;
    V.Reserve((NX + 1) * NP);
    for (int32 I = 0; I <= NX; ++I)
    {
        const float X = FMath::Lerp(-3600.f, 3800.f, I / float(NX));
        const float Ends = FMath::SmoothStep(-3600.f, -1800.f, X) * (1.f - FMath::SmoothStep(2000.f, 3800.f, X));
        const float Lag = .00012f * FMath::Square(X - 100.f);   // (its ends behind its middle)
        // A lumpy, uneven crest along its length, the lip running ahead in places, the whole of it heaving as it comes.
        const float Lump = 1.f + .13f * FMath::Sin(X / 730.f + 1.3f + Clock * .6f) + .07f * FMath::Sin(X / 260.f + 4.1f - Clock * 1.1f) + .04f * FMath::Sin(X / 97.f + Clock * 2.3f);
        const float Reach = 1.f + .25f * FMath::Sin(X / 540.f + 2.2f) + .12f * FMath::Sin(X / 180.f - Clock * 1.7f);
        for (int32 J = 0; J < NP; ++J)
        {
            const FPt& Pt = Prof[J];
            const float Dd = Pt.D * (Pt.D < 0.f ? 1.f : S * Reach);
            const float Y = -Side * (D + Lag - Dd);
            const float Z = SeaLocalZ(GetWorld(), FVector2D(X, Y)) + Pt.Hf * H * Ends * Lump - 30.f * (1.f - Ends);
            V.Add(FVector(X, Y, Z));
            UV.Add(FVector2D(X / 400.f, J / float(NP - 1)));
            const float Foam = FMath::Clamp(Pt.Foam * (.5f + .5f * Ends) * (1.f + .5f * Broken) + .25f * Broken * Pt.Hf, 0.f, 1.f);
            Col.Add(FColor(uint8(Foam * 255), uint8(Pt.Thin * Ends * (1.f - Broken) * 255), 0, 255));
        }
    }
    N.SetNum(V.Num());
    for (int32 I = 0; I <= NX; ++I)
        for (int32 J = 0; J < NP; ++J)
        {
            const FVector Dx = V[FMath::Min(I + 1, NX) * NP + J] - V[FMath::Max(I - 1, 0) * NP + J];
            const FVector Dp = V[I * NP + FMath::Min(J + 1, NP - 1)] - V[I * NP + FMath::Max(J - 1, 0)];
            N[I * NP + J] = (Side * FVector::CrossProduct(Dx, Dp)).GetSafeNormal();   // (up, out of the water)
        }
    for (int32 I = 0; I < NX; ++I)
        for (int32 J = 0; J + 1 < NP; ++J)
        {
            const int32 A = I * NP + J, B = A + NP;
            if (Side > 0.f) T.Append({A, A + 1, B, A + 1, B + 1, B});
            else T.Append({A, B, A + 1, A + 1, B, B + 1});
        }
    Comber->CreateMeshSection(0, V, T, N, UV, Col, TArray<FProcMeshTangent>(), false);
}

void AShipForeTopsailScene::Lines()
{
    AShipEnding* E = AShipEnding::Get(GetWorld());
    while (Next < BeatCount && Clock >= Beats[Next].At)
    {
        if (E) E->Say(EDockHuman::Jeffries, TEXT("Jeffries"), Beats[Next].Id, true);
        ++Next;
    }
}

void AShipForeTopsailScene::Skip()
{
    if (!bPlaying || bSkipping) return;
    bSkipping = true; EndAt = Clock + .6f;
    if (AChuckCharacter* C = Chuck.Get())
        if (auto* PC = Cast<APlayerController>(C->GetController()))
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, .5f, FLinearColor::Black, false, true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_SKIP clock=%.2f"), Clock);
}

void AShipForeTopsailScene::Finish()
{
    if (bFinished) return;
    bFinished = true; bPlaying = false;
    if (FAudioDevice* AD = GetWorld()->GetAudioDeviceRaw()) AD->SetTransientPrimaryVolume(1.f);
    if (AShipSetting* Ship = AShipSetting::Get(GetWorld())) Ship->SetKnock(0.f, 0.f);   // (behind the end card)
    if (Comber) Comber->ClearAllMeshSections();
    if (AChuckCharacter* C = Chuck.Get())
    {
        if (auto* PC = Cast<APlayerController>(C->GetController())) PC->SetViewTarget(C);
        C->EndDemo();
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_END clock=%.2f skipped=%d thrown=%d plunged=%d miss_cm=%.0f leaps=%d"), Clock, bSkipping, bThrown, bPlunged, PlungeMiss, YardLeaps);
}

void AShipForeTopsailScene::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bPlaying) return;
    AChuckCharacter* C = Chuck.Get();
    if (!C) { Finish(); return; }
    Clock += Dt;
    if (auto* PC = Cast<APlayerController>(C->GetController()))
        if (PC->WasInputKeyJustPressed(EKeys::Escape)) Skip();
    // To black and silence at the end.
    if (EndAt >= 0.f)
    {
        const float U = FMath::Clamp(1.f - (Clock - (EndAt - 1.2f)) / 1.2f, 0.f, 1.f);
        VolumeNow = FMath::Min(VolumeNow, bSkipping ? 0.f : U);
        if (FAudioDevice* AD = GetWorld()->GetAudioDeviceRaw()) AD->SetTransientPrimaryVolume(VolumeNow);
        if (Clock >= EndAt) { Finish(); return; }
    }
    if (bSkipping) return;
    DriveChuck(Dt);
    Lines();
    BuildRuptures();
    Knock();
    BuildComber();
    // Lightning: behind her in the aerial shot, ahead astern of her, and beyond the great sea as it comes.
    {
        struct FStrike { float At, Distance, Bearing; };
        const FStrike Strikes[] = {{10.2f, 90000.f, Side * 39.f}, {28.7f, 70000.f, Side * 15.f}, {35.3f, 60000.f, -Side * 70.f}};
        if (StrikeNext < UE_ARRAY_COUNT(Strikes) && Clock >= Strikes[StrikeNext].At)
        {
            if (AShipStorm* Storm = AShipStorm::Get(GetWorld())) Storm->StrikeNow(Strikes[StrikeNext].Distance, Strikes[StrikeNext].Bearing);
            ++StrikeNext;
        }
    }
    if (bShots)
    {
        static const float ShotAt[] = {7.5f, 9.8f, 29.f, 36.2f, 36.9f, 37.55f, 38.1f, 38.7f, 39.7f};
        if (ShotNext < UE_ARRAY_COUNT(ShotAt) && Clock >= ShotAt[ShotNext])
        {
            const FString Folder = FPaths::ScreenShotDir() / TEXT("ShipKnock"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("Knock_%d_%s.png"), ShotNext, Shot), false, false);
            ++ShotNext;
        }
    }
    const FVector P = C->GetActorLocation(), L = ShipLocal(P);
    const AShipCrew* Crew = AShipCrew::Get(GetWorld());
    const ADockNPC* J = Crew ? Crew->Find(EDockHuman::Jeffries) : nullptr;
    const AShipSetting* Ship = AShipSetting::Get(GetWorld());
    const FVector SeaUp = Ship ? Ship->GetSeaUp() : FVector::UpVector;
    // A camera off her rides the sea: placed in the sea's frame (turned with it round her waterline), so she is seen to roll
    // and to go over while the horizon holds level.
    const FQuat Turn = Ship ? Ship->GetSeaTurn() : FQuat::Identity;
    auto AtSea = [&](const FVector& Local) { return ShipWorld(FVector(0, 0, WaterZ) + Turn.RotateVector(Local - FVector(0, 0, WaterZ))); };
    if (Clock < WideAt) Frame(P + FVector(-230.f, Side * 250.f, 60.f), P + FVector(70.f, 0, 15.f), TEXT("Crosstrees"), 50.f);
    else if (Clock < AerialAt)
    {
        // Low over the swell off her lee bow, drifting aft as she drives on, her masts towering over the lens.
        const float U = (Clock - WideAt) / (AerialAt - WideAt);
        Frame(AtSea(FVector(2700.f - 900.f * U, Side * 2300.f, WaterZ + 380.f)), ShipWorld(FVector(400.f - 500.f * U, 0, 1150.f)), TEXT("SeaLevel"), 62.f, true);
    }
    else if (Clock < JeffriesAt)
    {
        // High and far off her weather quarter, closing a little: the whole brig rolling in it, lightning beyond her.
        const float U = (Clock - AerialAt) / (JeffriesAt - AerialAt);
        Frame(AtSea(FVector(-5200.f + 800.f * U, -Side * 4200.f, 4300.f - 600.f * U)), ShipWorld(FVector(300.f, 0, 800.f)), TEXT("Aerial"), 42.f, true);
    }
    else if (Clock < RupturesAt)
    {
        // In front of his face, a little to one side, easing in as he sees them.
        const FVector Head = J ? J->GetHeadLocation() : ShipWorld(FVector(MainMastX - 60, 0, DeckZ + 150));
        const FVector Fwd = J ? J->GetActorForwardVector().GetSafeNormal2D() : FVector(-1, 0, 0);
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
        const float U = (Clock - JeffriesAt) / (RupturesAt - JeffriesAt);
        Frame(Head + Fwd * (150.f - 45.f * U) + Right * 45.f + FVector(0, 0, 6.f), Head - FVector(0, 0, 6.f), TEXT("Jeffries"), 40.f);
    }
    else if (Clock < ClewAt)
    {
        const float U = (Clock - RupturesAt) / (ClewAt - RupturesAt);
        Frame(ShipWorld(FVector(250.f - 300.f * U, Side * 520.f, 1150.f)), ShipWorld(FVector(-900.f - 400.f * U, Side * 1700.f, 320.f)), TEXT("Ruptures"), 52.f);
    }
    else if (Clock < AsternAt) Frame(P + FVector(-150.f, Side * 120.f, -35.f), P + FVector(0, 0, 25.f), TEXT("Clewline"), 46.f);
    else if (Clock < BurningAt)
    {
        // Low and far astern, to weather: her stern lifting and falling away, the ruptures' glow round her.
        const float U = (Clock - AsternAt) / (BurningAt - AsternAt);
        Frame(AtSea(FVector(-5600.f + 400.f * U, -Side * 1500.f, WaterZ + 650.f)), ShipWorld(FVector(200.f, Side * 300.f, 1000.f)), TEXT("Astern"), 40.f, true);
    }
    else if (Clock < ComberAt)
    {
        const FVector2D R = RiftAt(Rifts.Last());
        Frame(AtSea(FVector(300.f, Side * 1500.f, 1750.f)), ShipWorld(FVector(R.X, R.Y, WaterZ)), TEXT("Burning"), 58.f, true);   // (abaft it and outboard of the headsails)
    }
    else if (Clock < GripAt)
    {
        // From ahead of her, a little to lee, low: the great sea coming down on her beam, its crest curling over in profile
        // against the sky; it breaks on her and she goes over, her masts swinging down toward the lens's side.
        const float U = (Clock - ComberAt) / (GripAt - ComberAt);
        Frame(AtSea(FVector(5600.f - 400.f * U, Side * 1100.f, WaterZ + 650.f)), ShipWorld(FVector(-100.f, -Side * 1100.f, 900.f)), TEXT("Comber"), 60.f, true);
    }
    else if (Clock < BelowAt)
    {
        // Close on him at the yardarm, in her frame, from abaft and a little above, along the yard: him hanging under it,
        // the sea far below past him swinging up as she goes over, the burning rupture's glow on it.
        const FVector G = bHeld ? SlipGrip : P;
        Frame(G + FVector(-240.f, -Side * 40.f, 50.f), G + FVector(0.f, Side * 30.f, -55.f), TEXT("Grip"), 55.f);
    }
    else
    {
        const FVector2D R = RiftAt(Rifts.Last());
        // From above him (the sea's up), outboard of the fall, so the fore topsail is not between.
        const FVector Over = ShipWorld(FVector(Land.X - 150.f, Land.Y + Side * 380.f, Land.Z)) + SeaUp * 2400.f;
        Frame(Over, FMath::Lerp(P, ShipWorld(FVector(R.X, R.Y, Land.Z)), .4f), TEXT("Below"), 58.f, true);
    }
    // Into the sea through the burning rupture: a flare, then black.
    if (bThrown && !bPlunged && L.Z < SeaLocalZ(GetWorld(), FVector2D(L.X, L.Y)) + 70.f)
    {
        bPlunged = true;
        const FVector2D R = RiftAt(Rifts.Last());
        PlungeMiss = FVector2D::Distance(R, FVector2D(L.X, L.Y));
        C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->DisableMovement();
        C->SetActorHiddenInGame(true);
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_AstralVanish_00.SFX_AstralVanish_00"))) UGameplayStatics::PlaySoundAtLocation(this, S, P, 1.f, .7f);
        if (auto* PC = Cast<APlayerController>(C->GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 1.1f, FLinearColor(.04f, 0.f, 0.f), false, true);
        Lights[3]->SetIntensity(30000.f);
        EndAt = Clock + 1.6f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_PLUNGE clock=%.2f at=%s miss_cm=%.0f"), Clock, *L.ToString(), PlungeMiss);
    }
    // Never falling past it (a fall that somehow misses the sea's plane).
    if (bThrown && !bPlunged && Clock > LandAt + 3.f) { bPlunged = true; PlungeMiss = 1e4f; EndAt = Clock + .8f; UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_SCENE_NO_PLUNGE at=%s"), *L.ToString()); }
}
