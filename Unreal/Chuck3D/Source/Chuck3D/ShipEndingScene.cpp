#include "ShipEndingScene.h"
#include "ShipEnding.h"
#include "ShipSetting.h"
#include "ShipCrew.h"
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

// The fore topsail cutscene (Claude, 2026-11-08; ShipEndingScene.h). Shots: "Crosstrees" (close, as he takes the fore topsail
// yard and goes out along it), "Wide" (the brig in the storm from off her side), "Jeffries" (at the mainmast, seeing them),
// "Ruptures" (the Astral ones in the sea round her, sliding aft), "Clewline" (close on him at the yard), "Burning" (over the
// bow: the Phlegethos rupture drifting toward her), "Gust" (thrown off), "Below" (from above as he falls into it).
using namespace Ship;

namespace
{
    // Jeffries' lines (the user's takes, 2026-11-08), at their beats.
    struct FBeat { float At; const TCHAR* Id; };
    const FBeat Beats[] = {{10.7f, TEXT("JEF_X01")}, {15.6f, TEXT("JEF_01")}, {19.3f, TEXT("JEF_02")}, {23.5f, TEXT("JEF_03")}, {27.3f, TEXT("JEF_04")},
        {30.6f, TEXT("JEF_05")}, {35.95f, TEXT("JEF_06")}};
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
    LandAt = GustAt + 2.45f;
    Land = ShipLocal(TopsailGrip(Side, 2)) + FVector(120.f * 2.45f, Side * 430.f * 2.45f, 0.f);
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

void AShipForeTopsailScene::Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov)
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
    Camera->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
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
    TArray<FVector> V[4], N[4]; TArray<int32> T[4]; TArray<FVector2D> UV[4], UV1[4], UV2[4];
    constexpr int32 Edge = 48;
    auto Lip = [](float A, float Seed, float Rag)
    {
        return 1.03f + Rag * (.08f * FMath::Sin(3.f * A + Seed) + .05f * FMath::Sin(7.f * A + 2.f * Seed) + .03f * FMath::Sin(13.f * A + Seed * 3.f));
    };
    auto Hole = [&](int32 S, const FVector2D& C, float Z, float R, float Seed, float Rag, float Open)
    {
        const int32 Base = V[S].Num();
        V[S].Add(FVector(C.X, C.Y, Z)); UV[S].Add(FVector2D::ZeroVector);
        for (int32 I = 0; I < Edge; ++I)
        {
            const float A = 2.f * PI * I / Edge, K = Lip(A, Seed, Rag);
            V[S].Add(FVector(C.X + FMath::Cos(A) * R * K * 1.25f, C.Y + FMath::Sin(A) * R * K * .85f, Z));
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
            V[3].Add(FVector(In.X, In.Y, Z + 1.f)); V[3].Add(FVector(Out.X, Out.Y, Z - 6.f));
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
                V[1].Add(FVector(P.X, P.Y, Z - 4)); V[1].Add(FVector(P.X, P.Y, Z + H));
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
    if (!C || bThrown) return;
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
    if (Clock >= GustAt)
    {
        // The gust: the yard jerks, the canvas flogs, and he is thrown off it, out clear of the side.
        // (Let go first; once he is off the yard, thrown.)
        if (C->IsSwingLeaping()) return;   // (between grips: off the next one he reaches)
        if (C->IsSwinging() || C->IsHanging()) C->DodgeToward(FVector2D::ZeroVector);
        bLetGo = true;
        const FVector V(120.f, Side * 430.f, 200.f);
        C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        C->GetCharacterMovement()->Velocity = V;
        C->LaunchCharacter(V, true, true);
        bThrown = true;
        const FVector P = ShipLocal(C->GetActorLocation());
        const float G = -static_cast<float>(C->GetCharacterMovement()->GetGravityZ());
        const float Drop = static_cast<float>(P.Z) - (WaterZ + 60.f);
        const float Tf = (V.Z + FMath::Sqrt(V.Z * V.Z + 2.f * G * FMath::Max(Drop, 1.f))) / FMath::Max(G, 1.f);
        Land = P + FVector(V.X * Tf, V.Y * Tf, 0.f); LandAt = Clock + Tf;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SCENE_THROWN clock=%.2f from=%s fall_s=%.2f land=%s"), Clock, *P.ToString(), Tf, *Land.ToString());
    }
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
    if (bShots)
    {
        static const float ShotAt[] = {20.5f, 23.5f, 29.5f, 32.f, 34.5f, 37.2f};
        if (ShotNext < UE_ARRAY_COUNT(ShotAt) && Clock >= ShotAt[ShotNext])
        {
            const FString Folder = FPaths::ScreenShotDir() / TEXT("ShipRuptures"); IFileManager::Get().MakeDirectory(*Folder, true);
            FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("Rupture_%d_%s.png"), ShotNext, Shot), false, false);
            ++ShotNext;
        }
    }
    const FVector P = C->GetActorLocation(), L = ShipLocal(P);
    const AShipCrew* Crew = AShipCrew::Get(GetWorld());
    const ADockNPC* J = Crew ? Crew->Find(EDockHuman::Jeffries) : nullptr;
    if (Clock < WideAt) Frame(P + FVector(-230.f, Side * 250.f, 60.f), P + FVector(70.f, 0, 15.f), TEXT("Crosstrees"), 50.f);
    else if (Clock < JeffriesAt)
    {
        const float U = (Clock - WideAt) / (JeffriesAt - WideAt);
        Frame(ShipWorld(FVector(2600.f - 500.f * U, Side * 3300.f, 1500.f - 200.f * U)), ShipWorld(FVector(150.f, 0, 1050.f)), TEXT("Wide"), 44.f);
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
    else if (Clock < BurningAt) Frame(P + FVector(-150.f, Side * 120.f, -35.f), P + FVector(0, 0, 25.f), TEXT("Clewline"), 46.f);
    else if (Clock < GustAt)
    {
        const FVector2D R = RiftAt(Rifts.Last());
        Frame(ShipWorld(FVector(300.f, Side * 1500.f, 1750.f)), ShipWorld(FVector(R.X, R.Y, WaterZ)), TEXT("Burning"), 58.f);   // (abaft it and outboard of the headsails)
    }
    else if (Clock < BelowAt) Frame(P + FVector(-330.f, -Side * 60.f, 140.f), P, TEXT("Gust"), 56.f);
    else
    {
        const FVector2D R = RiftAt(Rifts.Last());
        // (From outboard of the fall, so the fore topsail is not between.)
        Frame(ShipWorld(FVector(Land.X - 150.f, Land.Y + Side * 380.f, 2700.f)), FMath::Lerp(P, ShipWorld(FVector(R.X, R.Y, WaterZ)), .4f), TEXT("Below"), 58.f);
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
