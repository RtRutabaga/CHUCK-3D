#include "SewerLife.h"
#include "DockSewer.h"
#include "EnemyRat.h"
#include "GrassTuft.h"
#include "DockNPC.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "TimerManager.h"

namespace
{
    int32 RatsPlaced = 0, TuftsPlaced = 0, FirstGroupPlaced = 0;
    TArray<TWeakObjectPtr<ADockNPC>> Zombies;
    TArray<int32> ZombieSamples;
    constexpr int32 FirstRats = 36;       // samples: the first gap is 26..29, so a few metres beyond it
    // Rat groups along the route (sample, count); the wide chamber (around the
    // middle) is left to its zombies.
    const int32 RatGroups[][2] = { {FirstRats, 3}, {120, 2}, {262, 3}, {330, 2} };
}

void UseSewerLighting(AActor* Actor)
{
    if (!Actor) return;
    TArray<UPrimitiveComponent*> Parts;
    Actor->GetComponents(Parts);
    for (UPrimitiveComponent* Part : Parts) Part->SetLightingChannels(false, true, false);
}

int32 GetSewerRatsPlaced() { return RatsPlaced; }
int32 GetSewerTuftsPlaced() { return TuftsPlaced; }
int32 GetSewerFirstRatsSample() { return FirstRats; }
int32 GetSewerFirstGroupPlaced() { return FirstGroupPlaced; }
int32 GetSewerZombieCount() { return Zombies.Num(); }
ADockNPC* GetSewerZombie(int32 Index) { return Zombies.IsValidIndex(Index) ? Zombies[Index].Get() : nullptr; }
int32 GetSewerZombieSample(int32 Index) { return ZombieSamples.IsValidIndex(Index) ? ZombieSamples[Index] : -1; }

void SpawnSewerLife(UWorld* World)
{
    RatsPlaced = TuftsPlaced = FirstGroupPlaced = 0;
    const int32 Count = DockSewerSamples();
    if (Count < 20) return;
    const bool bWalkThrough = FParse::Param(FCommandLine::Get(), TEXT("ChuckSewerTest")) || FParse::Param(FCommandLine::Get(), TEXT("ChuckStreamTest"));
    // Rats: on the banks either side of the stream, spread along a few metres.
    if (!bWalkThrough)
        for (const auto& Group : RatGroups)
            for (int32 K = 0; K < Group[1]; ++K)
            {
                const int32 I = FMath::Clamp(Group[0] + K * 3, 2, Count - 6);
                if (DockSewerIsGap(I) || DockSewerIsChamber(I)) continue;
                const float Side = (K % 2) ? -1.f : 1.f;
                const FVector Ground = DockSewerPoint(I) + DockSewerSide(I) * (Side * (60.f + 25.f * K));
                if (AEnemyRat* Rat = AEnemyRat::PlaceAt(World, Ground, FMath::FRandRange(0.f, 360.f)))
                {
                    Rat->Cigarettes = 1 + (RatsPlaced % 2);
                    UseSewerLighting(Rat);
                    ++RatsPlaced;
                    if (&Group == &RatGroups[0]) ++FirstGroupPlaced;
                }
            }
    // Zombies (user 2026-10-04): one in the tunnel before the wide chamber,
    // three in the chamber, one at the end by the stream chute. None near the
    // post-chamber wall-run break. (sample, fraction of the half floor width
    // out from the centre line, facing back down the route or across it)
    Zombies.Reset(); ZombieSamples.Reset();
    if (!bWalkThrough)
    {
        const int32 Middle = Count / 2;
        const struct { int32 Sample; float Out; float Turn; } Places[] = {
            { 100, .4f, 0.f },                 // the tunnel before the chamber
            { Middle - 12, .5f, 30.f },        // the chamber, spread over its length and both sides
            { Middle + 2, -.5f, -60.f },
            { Middle + 8, .45f, 100.f },
            { Count - 9, -.35f, 0.f } };       // by the chute at the end
        for (const auto& Place : Places)
        {
            // Step back off any opening (and its edge) to solid floor.
            int32 I = FMath::Clamp(Place.Sample, 8, Count - 6);
            while (I > 8 && (DockSewerIsGap(I) || DockSewerIsGap(I - 1) || DockSewerIsGap(I + 1))) --I;
            const FVector Back = DockSewerPoint(I - 4) - DockSewerPoint(I);
            const FVector Feet = DockSewerPoint(I) + DockSewerSide(I) * (Place.Out * DockSewerHalfWidth(I));
            if (ADockNPC* Dead = ADockNPC::SpawnZombie(World, Feet, static_cast<float>(Back.Rotation().Yaw) + Place.Turn))
            {
                UseSewerLighting(Dead);
                Zombies.Add(Dead); ZombieSamples.Add(I);
            }
        }
    }
    // Moss clumps where the floor meets the wall, alternating sides every few
    // metres, a third of them holding a cigarette (as the docks' grass).
    FRandomStream Random(20261003);
    for (int32 I = 8; I < Count - 8; I += 9)
    {
        if (I>=DockSewerWallRiftStart()-8 && I<=DockSewerWallRiftEnd()+8) continue;
        if (DockSewerIsGap(I) || DockSewerIsGap(I + 3) || DockSewerIsGap(I - 3)) continue;
        const float Side = ((I / 9) % 2) ? -1.f : 1.f;
        const int32 Clump = 2 + Random.RandRange(0, 2);
        for (int32 K = 0; K < Clump; ++K)
        {
            const int32 J = FMath::Clamp(I + K, 2, Count - 6);
            const float Out = DockSewerHalfWidth(J) * Random.FRandRange(.55f, .72f);
            const FVector Ground = DockSewerPoint(J) + DockSewerSide(J) * (Side * Out);
            if (AGrassTuft* Tuft = AGrassTuft::PlantAt(World, Ground, Random.RandRange(0, 2), Random.FRandRange(0.f, 360.f), Random.FRandRange(.8f, 1.1f)))
            {
                Tuft->SetMoss();
                Tuft->Cigarettes = Random.FRand() < .33f ? 1 : 0;
                UseSewerLighting(Tuft);
                ++TuftsPlaced;
            }
        }
    }
    FString Samples;
    for (const int32 I : ZombieSamples) Samples += FString::Printf(TEXT("%s%d"), Samples.IsEmpty() ? TEXT("") : TEXT(","), I);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SEWER_LIFE rats=%d moss_tufts=%d first_rats_sample=%d zombies=%d zombie_samples=%s samples=%d walkthrough=%d"), RatsPlaced, TuftsPlaced, FirstRats, Zombies.Num(), *Samples, Count, bWalkThrough ? 1 : 0);
    // -ChuckSewerLifeCapture: the first rat group past the gap, and a moss clump, then quit.
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckSewerLifeCapture")))
    {
        auto* Camera = World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(70);
        const int32 From[] = { FirstRats - 6, 62, DockSewerWallRiftStart()-3 }, To[] = { FirstRats + 3, 64, DockSewerWallRiftStart()+1 };
        const float Up[] = { 70.f, 45.f, 80.f }, Out[] = { 0.f, -.4f, -.25f };
        for (int32 V = 0; V < 3; ++V)
        {
            const FVector P = DockSewerPoint(From[V]) + DockSewerSide(From[V]) * (Out[V] * DockSewerHalfWidth(From[V])) + FVector(0, 0, Up[V]);
            const FVector T = DockSewerPoint(To[V]) + DockSewerSide(To[V]) * (V ? .4f * DockSewerHalfWidth(To[V]) : 0.f) + FVector(0, 0, V==2 ? 85.f : 15.f);
            FTimerHandle View, Shot;
            World->GetTimerManager().SetTimer(View, [World, Camera, P, T]() { Camera->SetActorLocationAndRotation(P, (T - P).Rotation()); World->GetFirstPlayerController()->SetViewTarget(Camera); }, 3.f + V * 3.f, false);
            World->GetTimerManager().SetTimer(Shot, [V]() {
                const FString Folder = FPaths::ScreenShotDir() / TEXT("SewerLife"); IFileManager::Get().MakeDirectory(*Folder, true);
                FScreenshotRequest::RequestScreenshot(Folder / FString::Printf(TEXT("View%d.png"), V), false, false); }, 5.f + V * 3.f, false);
        }
        FTimerHandle Quit; World->GetTimerManager().SetTimer(Quit, [World]() { World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit")); }, 13.f, false);
    }
}
