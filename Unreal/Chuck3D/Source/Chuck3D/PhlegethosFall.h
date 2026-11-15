#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhlegethosFall.generated.h"

class AChuckCharacter;
class ACameraActor;
class UAudioComponent;
class USoundBase;
class UVolumetricCloudComponent;

/**
 * Phase 7's end into Phase 8 (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md): out of the burning rupture under the
 * ship's bow, Chuck falls through the Hells' heated air and an ash deck, the volcano climbing into view as he drops
 * (the 2D scene's beat), and hits the basalt plain. He dies his quiet death there, is summoned back, looks round and
 * takes a drag; the camera cranes up over Phlegethos as the volcano erupts, and fades. After the 2D game's
 * hell_falling_cutscene_scene.py, which echoes the fall to Chult and reuses its music.
 *
 * The score is the user's fall theme (SW_FallToChult), one phrase further than the fall to Chult (user 2026-11-14:
 * "a bit more of the song creating a slightly longer cut scene"): bars 0..33 (to 62.6 s), faded over the last two
 * bars. Bar lines at 0.55 + 1.879 k s. No control: Esc skips to the end. Both hand over to playable Phlegethos.
 */
UCLASS()
class CHUCK3D_API APhlegethosFall : public AActor
{
    GENERATED_BODY()
public:
    APhlegethosFall();
    virtual void Tick(float DeltaSeconds) override;
    static APhlegethosFall* Start(AChuckCharacter* Chuck);
    void Skip();
    float GetClock() const { return Clock; }
    bool IsMusicPlaying() const;
    static float Bar(int32 K) { return .55f + 1.879f * K; }
    // Song-time beats (bars).
    static constexpr float Lead = 5.f;            // s of falling with only the air before the score (the clock starts at -Lead)
    static constexpr float FadeUp = -4.2f;        // out of the dark
    static constexpr float Reveal = .55f;         // bar 0: the wide sky
    static constexpr float CloseUp = 8.07f;       // bar 4: his face
    static constexpr float Deck = 13.70f;         // bar 7: down past him at the ash deck
    static constexpr float Volcano = 17.46f;      // bar 9: under it, the volcano climbing into view
    static constexpr float Rivers = 21.22f;       // bar 11: the lava rivers below him
    static constexpr float FromGround = 24.98f;   // bar 13 (the breakdown): from the plain, up at him
    static constexpr float Rush = 28.73f;         // bar 15: the ground rushing up
    static constexpr float Impact = 30.61f;       // bar 16
    static constexpr float Summon = 34.37f;       // bar 18
    static constexpr float SwapLead = .11f;
    static constexpr float Look = 38.13f;         // bar 20
    static constexpr float Drag = 41.88f;         // bar 22
    static constexpr float Crane = 45.64f;        // bar 24: up over Phlegethos
    static constexpr float Eruption = 49.40f;     // bar 26
    static constexpr float MusicFade = 58.80f;    // bar 31
    static constexpr float Black = 60.68f;        // bar 32
    static constexpr float End = 62.56f;          // bar 33
    static constexpr float ProneYaw = 70.f;
    static constexpr float ProneHeight = 14.f;
    static constexpr float ShakeTime = .6f;
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    UPROPERTY() UAudioComponent* Music = nullptr;
    UPROPERTY() UAudioComponent* Wind = nullptr;
    UPROPERTY() UAudioComponent* Flap = nullptr;
    UPROPERTY() UAudioComponent* Whistle = nullptr;
    UPROPERTY() USoundBase* ThudSound = nullptr;
    UPROPERTY() USoundBase* LandSound = nullptr;
    UPROPERTY() UVolumetricCloudComponent* AshDeck = nullptr;
    float Clock = -Lead, SkipAt = 0, ShakeUntil = -1e9f;
    bool bSkipping = false, bStage = false, bLanded = false, bVanished = false, bSummoned = false, bFinished = false, bErupted = false;
    int32 Exhales = 0;
    void BuildStage();
    void UpdateChuck(float DeltaSeconds);
    void UpdateCamera();
    void UpdateAir();
    void Events(float From, float To);
    void Finish();
    static float Altitude(float T);
    FVector ChuckTarget() const;
    void Aim(const FVector& From, const FVector& At, float FocalMM, float FStop);
};
