#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyRiverScene.generated.h"

class AChuckCharacter;
class ACameraActor;
class UAudioComponent;
class USoundBase;
class UPointLightComponent;

// Scene time of the fall theme's bar K, the score entering at 7 s on 48.5 s of song time (bar lines at 0.55 + 1.879 k s).
namespace FeyRiver { constexpr float Bar(int32 K) { return .55f + 1.879f * K - 41.5f; } }

/**
 * The river cutscene (Claude, 2026-11-19; docs/regions/FEYWILD.md#the-river-cutscene, build stage 2). The Feywild river
 * that swept Chuck into the tear at Phlegethos's fortress (PhlegethosBleed.cpp) carries him into the Feywild:
 *  1. sound only, about 7 s: near darkness round his head in a root tunnel, the water roaring, muffled each time he goes
 *     under, sharp and loud as he surfaces, at no steady rhythm;
 *  2. the score enters with the first light: the user's fall theme (SW_FallToChult) from 48.5 s, its last section, played
 *     through to its own end;
 *  3. out of the tunnel's mouth the Feywild opens round the river: bare rock and root, then moss, fungi, flowers, old oaks,
 *     the low gold sun ahead;
 *  4. on the score's lift (bar 33) the river is the slice's stream above the two-step falls: over both, a plunge on bar 36,
 *     and the pond's still water;
 *  5. he washes onto its south shore, lies still a beat, pushes himself up, and the score ends as control returns.
 * The clock is scene time from the sweep's end; the score starts at Score, so song time is Clock + SongOffset and the bars
 * fall at Bar(k). No control: Esc skips to the end. The river's set is its own (AFeywildSetting::BuildRiver).
 */
UCLASS()
class CHUCK3D_API AFeyRiverScene : public AActor
{
    GENERATED_BODY()
public:
    AFeyRiverScene();
    virtual void Tick(float DeltaSeconds) override;
    static AFeyRiverScene* Start(AChuckCharacter* Chuck);
    void Skip();
    float GetClock() const { return Clock; }
    bool IsMusicPlaying() const;
    static constexpr float SongStart = 48.5f;
    static constexpr float Score = 7.f;
    static constexpr float SongOffset = SongStart - Score;
    static_assert(SongOffset == 41.5f, "FeyRiver::Bar assumes the score enters at 7 s on 48.5 s");
    // The beats.
    static constexpr float Light = 1.2f;                                // out of the black into near darkness
    static constexpr float Out = FeyRiver::Bar(26);                              // 7.90: outside the tunnel's mouth
    static constexpr float Face = FeyRiver::Bar(29);                             // 13.53: close on him among the banks
    static constexpr float Ahead = FeyRiver::Bar(31);                            // 17.29: behind him, the river ahead, the sun
    static constexpr float Falls = FeyRiver::Bar(33);                            // 21.05: the score lifts: the stream above the falls
    static constexpr float UpperLip = 22.92f;
    static constexpr float Shelf = 23.85f;
    static constexpr float LowerLip = 25.67f;
    static constexpr float Plunge = FeyRiver::Bar(36);                           // 26.69
    static constexpr float Surface = 27.45f;
    static constexpr float Drift = FeyRiver::Bar(37);                            // 28.57
    static constexpr float Wash = FeyRiver::Bar(38);                             // 30.45: onto the shore
    static constexpr float Rise = 32.85f;
    static constexpr float Stood = 34.10f;
    static constexpr float Last = FeyRiver::Bar(40);                             // 34.21: behind him, the glade
    static constexpr float End = 78.f - SongOffset;                    // 36.50: the song's own end
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    UPROPERTY() UAudioComponent* Music = nullptr;
    UPROPERTY() UAudioComponent* Rush = nullptr;    // the river's wash
    UPROPERTY() UAudioComponent* Roar = nullptr;    // its deeper roar, rising toward the falls
    UPROPERTY() TArray<USoundBase*> Splashes;
    UPROPERTY() USoundBase* LandSound = nullptr;
    UPROPERTY() UPointLightComponent* Glow = nullptr;   // the faint cold light round his head in the dark
    float Clock = 0, SkipAt = 0;
    bool bSkipping = false, bStage = false, bFinished = false, bUnder = false, bWashed = false, bRose = false;
    int32 Surfacings = 0, SplashCount = 0;
    FVector ShoreAt = FVector::ZeroVector, DriftFrom = FVector::ZeroVector;
    float ShoreYawDeg = 0;
    int32 PartFrames[3] = {0, 0, 0};
    float PartTime[3] = {0, 0, 0}, PartWorst[3] = {0, 0, 0};
    void BuildStage();
    /** Distance along the river (cm) at scene time T, before the falls. */
    static float RiverS(float T);
    /** Head under the water (the first seconds' dunks, the plunge). */
    bool IsUnder(float T) const;
    FVector ChuckTarget() const;
    void UpdateChuck(float DeltaSeconds);
    void UpdateCamera();
    void UpdateSound(float DeltaSeconds);
    void Events(float From, float To);
    void PlaySplash(const FVector& World, float Volume, float Pitch);
    void Finish();
    void Aim(const FVector& From, const FVector& At, float FocalMM, float FStop, float Bias = 0.f);
};
