// -ChuckSoundAudit[=<checkpoint>] (Claude, 2026-11-13; docs/AUTOMATION-DEFECT-CATALOG.md, category 6).
// Starts at a dev checkpoint (Waterdeep when none is given), waits for the region to stand, then looks for sounds heard
// through walls, floors and the ground: the sewer zombies heard in the street, the temple's insides outside, the rats in
// the hold heard all over the ship.
//
// The sources are the region's audio components, its creatures and people, and whatever is playing at the start. Round
// each, places a rat could stand (7 and 15 m off, on any floor within 9 m up or down) with solid between them and the
// source are found. Chuck is set at each in turn, for 1.2 s, and the audio engine's own list of the sounds it is playing
// is read with the gain each reaches the listener at (its volume times distance and occlusion falloff; the game's own gating included). A
// spatialized sound over 0.05 (-26 dB) whose straight line to the listener goes through solid, with no clear way round
// (4 or 8 m over the middle, or 4 m to either side: a tree or a rock in the open is not a leak), is a leak
// (CHUCK_SOUND_AUDIT_LEAK). Each source (to 4 m) is counted once, at its loudest. One-shots play when they play, so a
// run can miss a source that is quiet for the 1.2 s at its places; the counts vary a little between runs.
//
// A planted probe (any loaded sound, 3 m under the floor at each place) must be heard as a leak, so the audit cannot pass
// deaf. Intended cases go in Content/Data/Audit/Allow.txt as "sound <wildcard on '<sound> <component>'>".
// Writes Saved/Audit/<Region>-sound.csv. Moves Chuck about (collision and movement off); quits.
#include "CoreMinimal.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "ChuckCharacter.h"
#include "ChultPropAudit.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

bool ChuckAuditAllowed(const TCHAR* Check, const FString& Key);

namespace
{
    constexpr float Heard = .05f;
    constexpr int32 MaxPlaces = 80;

    struct FPlaying { FString Sound, Comp; FVector At; float Volume; };

    struct FLeak { FString Sound, Comp; FVector At, Listener; float Volume; int32 Place; };

    struct FRun
    {
        UWorld* World = nullptr;
        FString Region;
        TWeakObjectPtr<AChuckCharacter> Chuck;
        TArray<FVector> Places;
        int32 Next = 0;
        TWeakObjectPtr<UAudioComponent> Probe;
        int32 ProbeHeard = 0;
        TMap<FString, FLeak> Leaks;   // by source (to 4 m), at its loudest
        int32 Allowed = 0, HeardTotal = 0, Round = 0;
        double Start = 0;
        FCriticalSection Lock;
        TArray<FPlaying> Playing;
        bool bReady = false;
        int32 Active = 0, Spatial = 0, Queries = 0;
        float Loudest = 0;
    };

    // The audio engine's own list of what is playing and how loud it reaches the listener (read on the audio thread).
    void Query(TSharedRef<FRun> R)
    {
        { FScopeLock L(&R->Lock); R->bReady = false; R->Playing.Reset(); }
        FAudioDevice* AD = R->World->GetAudioDeviceRaw();
        if (!AD) { FScopeLock L(&R->Lock); R->bReady = true; return; }
        FAudioThread::RunCommandOnAudioThread([AD, R]()
        {
            TArray<FPlaying> Out;
            int32 Active = 0;
            for (const FActiveSound* A : AD->GetActiveSounds())
            {
                ++Active;
                if (!A || !A->GetSound() || A->bIsUISound || !A->bAllowSpatialization || !A->bHasAttenuationSettings) continue;
                float V = 0;
                for (const auto& W : A->GetWaveInstances()) if (W.Value) V = FMath::Max(V, W.Value->GetVolumeWithDistanceAndOcclusionAttenuation());   // not GetActualVolume: that includes the primary volume, 0 while the window is in the background
                Out.Add({A->GetSound()->GetName(), A->GetAudioComponentName(), A->Transform.GetLocation(), V});
            }
            FScopeLock L(&R->Lock);
            R->Active += Active; R->Spatial += Out.Num(); ++R->Queries;
            for (const FPlaying& P : Out) R->Loudest = FMath::Max(R->Loudest, P.Volume);
            R->Playing = MoveTemp(Out);
            R->bReady = true;
        });
    }

    bool Shown(const AActor* A) { return A && !A->IsHidden() && !A->IsPendingKillPending(); }

    // Places a rat could stand round a source with solid between them and it.
    void FindPlaces(TSharedRef<FRun> R, const TArray<FVector>& Sources)
    {
        UWorld* World = R->World;
        AChuckCharacter* Chuck = R->Chuck.Get();
        FCollisionQueryParams Q(SCENE_QUERY_STAT(SoundAuditPlaces), false, Chuck);
        const float Half = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 40.f;
        const float Radius = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() : 20.f;
        const int32 PerSource = FMath::Max(1, MaxPlaces / FMath::Max(1, Sources.Num()));
        for (const FVector& S : Sources)
        {
            int32 Found = 0;
            for (const float Dist : {700.f, 1500.f})
            for (int32 K = 0; K < 8 && Found < PerSource && R->Places.Num() < MaxPlaces; ++K)
            {
                const FVector Col = S + FVector(1, 0, 0).RotateAngleAxis(45.f * K + Dist * .01f, FVector::UpVector) * Dist;
                float Top = S.Z + 900;
                for (int32 Layer = 0; Layer < 6 && Found < PerSource; ++Layer)
                {
                    FHitResult Floor;
                    if (!World->LineTraceSingleByChannel(Floor, FVector(Col.X, Col.Y, Top), FVector(Col.X, Col.Y, S.Z - 900), ECC_Visibility, Q)) break;
                    Top = Floor.ImpactPoint.Z - 10;
                    if (Floor.ImpactNormal.Z < .7f) continue;
                    const FVector Stand = Floor.ImpactPoint + FVector(0, 0, Half + 3);
                    if (World->OverlapBlockingTestByChannel(Stand, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Q)) continue;
                    const FVector Ear = Stand + FVector(0, 0, Half);
                    FHitResult Wall;
                    const FVector To = S - (S - Ear).GetSafeNormal() * 60;
                    if (!World->LineTraceSingleByChannel(Wall, Ear, To, ECC_Visibility, Q)) continue;   // in the open: not a leak test
                    bool bNear = false;
                    for (const FVector& P : R->Places) bNear |= FVector::DistSquared(P, Stand) < 300 * 300;
                    if (bNear) continue;
                    R->Places.Add(Stand); ++Found;
                }
            }
        }
    }

    void Finish(TSharedRef<FRun> R)
    {
        if (UAudioComponent* P = R->Probe.Get()) P->Stop();
        TArray<FString> Rows{TEXT("kind,sound,component,x,y,z,listener_x,listener_y,listener_z,volume,place")};
        TMap<FString, int32> BySound;
        for (const auto& KV : R->Leaks)
        {
            const FLeak& L = KV.Value;
            Rows.Add(FString::Printf(TEXT("leak,%s,%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.3f,%d"), *L.Sound, *L.Comp.Replace(TEXT(","), TEXT(";")), L.At.X, L.At.Y, L.At.Z,
                L.Listener.X, L.Listener.Y, L.Listener.Z, L.Volume, L.Place));
            if (++BySound.FindOrAdd(L.Sound) <= 3)
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SOUND_AUDIT_LEAK sound=%s component=%s at=%s listener=%s volume=%.3f"), *L.Sound, *L.Comp, *L.At.ToString(), *L.Listener.ToString(), L.Volume);
        }
        for (const auto& KV : BySound) UE_LOG(LogTemp, Display, TEXT("CHUCK_SOUND_AUDIT_GROUP count=%d %s"), KV.Value, *KV.Key);
        FFileHelper::SaveStringArrayToFile(Rows, *(FPaths::ProjectSavedDir() / TEXT("Audit") / R->Region + TEXT("-sound.csv")));
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SOUND_AUDIT region=%s places=%d heard=%d heard_round_obstacles=%d leaks=%d leaks_allowed=%d probe_heard=%d/%d audio=%d active_per_place=%.1f spatial_per_place=%.1f loudest=%.3f ms=%.0f"),
            *R->Region, R->Places.Num(), R->HeardTotal, R->Round, R->Leaks.Num(), R->Allowed, R->ProbeHeard, R->Places.Num(), R->World->GetAudioDeviceRaw() ? 1 : 0,
            R->Active / FMath::Max(1.f, float(R->Queries)), R->Spatial / FMath::Max(1.f, float(R->Queries)), R->Loudest,
            (FPlatformTime::Seconds() - R->Start) * 1000);
        FTimerHandle Quit;
        R->World->GetTimerManager().SetTimer(Quit, [](){ FPlatformMisc::RequestExit(false); }, .5f, false);
    }

    void Step(TSharedRef<FRun> R);

    // What is heard at the place just visited.
    void Judge(TSharedRef<FRun> R)
    {
        bool bReady; { FScopeLock L(&R->Lock); bReady = R->bReady; }
        if (!bReady)
        {
            FTimerHandle Again;
            R->World->GetTimerManager().SetTimer(Again, [R]() { Judge(R); }, .1f, false);
            return;
        }
        TArray<FPlaying> Playing; { FScopeLock L(&R->Lock); Playing = R->Playing; }
        FVector Listener, Front, Right;
        APlayerController* PC = UGameplayStatics::GetPlayerController(R->World, 0);
        if (PC) PC->GetAudioListenerPosition(Listener, Front, Right); else Listener = R->Places[R->Next - 1];
        FCollisionQueryParams Q(SCENE_QUERY_STAT(SoundAuditLine), false, R->Chuck.Get());
        for (const FPlaying& P : Playing)
        {
            if (P.Volume < Heard) continue;
            ++R->HeardTotal;
            const FVector To = P.At - (P.At - Listener).GetSafeNormal() * 60;
            if (FVector::Dist(P.At, Listener) < 120 || !R->World->LineTraceTestByChannel(Listener, To, ECC_Visibility, Q)) continue;
            // In the open with a tree or a rock in the way is not a leak: a way round it (4 or 8 m over, or 4 m to
            // either side, at the middle) that is clear both ways lets the sound by. Walls, floors and the ground don't.
            {
                const FVector Mid = (Listener + To) * .5f;
                const FVector Side = FVector::CrossProduct(To - Listener, FVector::UpVector).GetSafeNormal() * 400;
                bool bRound = false;
                for (const FVector& Off : {FVector(0, 0, 400), FVector(0, 0, 800), Side, -Side})
                    bRound |= !R->World->LineTraceTestByChannel(Listener, Mid + Off, ECC_Visibility, Q) && !R->World->LineTraceTestByChannel(Mid + Off, To, ECC_Visibility, Q);
                if (bRound) { ++R->Round; continue; }
            }
            if (P.Comp.Contains(TEXT("AuditProbeSound"))) { ++R->ProbeHeard; continue; }
            if (ChuckAuditAllowed(TEXT("sound"), P.Sound + TEXT(" ") + P.Comp)) { ++R->Allowed; continue; }
            // One finding per source (to 4 m): the anvil's strikes and taps are one anvil.
            const FString Key = FString::Printf(TEXT("%d,%d,%d"), FMath::RoundToInt(P.At.X / 400), FMath::RoundToInt(P.At.Y / 400), FMath::RoundToInt(P.At.Z / 400));
            FLeak* L = R->Leaks.Find(Key);
            if (!L || L->Volume < P.Volume) R->Leaks.Add(Key, {P.Sound, P.Comp, P.At, Listener, P.Volume, R->Next - 1});
        }
        Step(R);
    }

    // Chuck to the next place; the probe under its floor; listen after 1.2 s.
    void Step(TSharedRef<FRun> R)
    {
        AChuckCharacter* Chuck = R->Chuck.Get();
        if (!Chuck || R->Next >= R->Places.Num()) { Finish(R); return; }
        const FVector At = R->Places[R->Next++];
        Chuck->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
        if (UAudioComponent* P = R->Probe.Get())
        {
            P->SetWorldLocation(At - FVector(0, 0, 300));
            P->Play();
        }
        FTimerHandle Listen;
        R->World->GetTimerManager().SetTimer(Listen, [R]() { Query(R); Judge(R); }, 1.2f, false);
    }

    void Begin(UWorld* World, const FString& Region)
    {
        TSharedRef<FRun> R = MakeShared<FRun>();
        R->World = World; R->Region = Region; R->Start = FPlatformTime::Seconds();
        AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
        R->Chuck = Chuck;
        // The sources: audio components, creatures and people, and what is playing now.
        TArray<FVector> Sources;
        auto Add = [&](const FVector& P) { for (const FVector& S : Sources) if (FVector::DistSquared(S, P) < 400 * 400) return; Sources.Add(P); };
        for (TObjectIterator<UAudioComponent> It; It; ++It)
            if (It->GetWorld() == World && It->IsRegistered() && It->Sound && Shown(It->GetOwner()) && It->GetOwner() != Chuck) Add(It->GetComponentLocation());
        for (TActorIterator<APawn> It(World); It; ++It) if (*It != Chuck && Shown(*It)) Add(It->GetActorLocation());
        FindPlaces(R, Sources);
        // The probe: any loaded sound of a second or more, heard 25 m off.
        USoundWave* Wave = nullptr;
        for (TObjectIterator<USoundWave> It; It && !Wave; ++It)
            if (It->Duration > 1.f && It->Duration < 600.f && !It->IsTemplate() && It->GetPathName().StartsWith(TEXT("/Game/"))) Wave = *It;
        if (Wave && Chuck)
        {
            AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Chuck->GetActorLocation()));
            UAudioComponent* P = NewObject<UAudioComponent>(Holder, TEXT("AuditProbeSound"));
            P->SetSound(Wave);
            P->bAutoActivate = false;
            P->bAllowSpatialization = true;
            P->bOverrideAttenuation = true;
            P->AttenuationOverrides.bAttenuate = true;
            P->AttenuationOverrides.bSpatialize = true;
            P->AttenuationOverrides.AttenuationShapeExtents = FVector(400, 0, 0);
            P->AttenuationOverrides.FalloffDistance = 2500;
            P->RegisterComponent();
            Holder->SetRootComponent(P);
            R->Probe = P;
        }
        if (Chuck)
        {
            // No triggers, no falling, no damage while he is set about.
            Chuck->SetActorEnableCollision(false);
            if (UCharacterMovementComponent* M = Chuck->GetCharacterMovement()) M->DisableMovement();
            Chuck->SetCanBeDamaged(false);
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SOUND_AUDIT_BEGIN region=%s sources=%d places=%d probe=%s"), *Region, Sources.Num(), R->Places.Num(), Wave ? *Wave->GetName() : TEXT("none"));
        Step(R);
    }

    void WaitThenRun(UWorld* World, FString Region, double Since)
    {
        const bool bChult = Region.StartsWith(TEXT("Chult"));
        if ((bChult && !IsChultGroundingDone()) || World->GetTimeSeconds() - Since < 8.0)
        {
            FTimerHandle Again;
            World->GetTimerManager().SetTimer(Again, [World, Region, Since]() { WaitThenRun(World, Region, Since); }, 1.f, false);
            return;
        }
        Begin(World, Region);
    }
}

// Called by the game mode with the checkpoint already started (or Waterdeep).
void StartSoundAudit(UWorld* World, const FString& Region)
{
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SOUND_AUDIT_START region=%s"), *Region);
    WaitThenRun(World, Region, World->GetTimeSeconds());
}
