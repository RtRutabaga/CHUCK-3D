// The heroes' lines in the sanctum (user 2026-10-25: the trio's ElevenLabs takes; their words are the 2D game's
// data/dialogue/temple_sanctum.json). Two groups, as in the 2D game: their calls when Chuck comes into the
// sanctum, and the argument before the wizard's Fireball. Only the speaking for now: the Fireball itself, his
// casting and the cutscenes are not made yet, so after "FIREBALL!!" the fight simply goes on
// (docs/CHULT-TEMPLE-HEROES.md#their-lines).
#include "TempleSanctumBattle.h"
#include "ChuckCharacter.h"
#include "DockSubtitles.h"
#include "NPCVoiceData.h"
#include "TempleHero.h"
#include "TempleSanctumScene.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
    struct FTalkLine { int32 Hero; const TCHAR* Npc; const TCHAR* Id; float Gap; };
    // Gap: seconds of Chuck in the sanctum before the line (after the last one ended). The user asked that he not be
    // rushed: a long wait after he comes in, and room between the lines that belong together.
    const FTalkLine Script[] = {
        {0, TEXT("HeroFighter"), TEXT("entrance_00"), 8.f},    // "Hold the line! It bleeds - I have seen it bleed!"
        {1, TEXT("HeroWizard"), TEXT("entrance_00"), 2.8f},    // "Keep its eye off me! The binding needs another minute!"
        {2, TEXT("HeroRanger"), TEXT("entrance_00"), 2.8f},    // "I'll cover you! We have to keep pushing forward!"
        // The 2D game starts the argument 24 s after Chuck is sealed into the hall; here (no seal yet) 24 s after their calls.
        {2, TEXT("HeroRanger"), TEXT("fireball_00"), 24.f},    // "Wait, I know those sigils, you can't cast that here!"
        {0, TEXT("HeroFighter"), TEXT("fireball_00"), 2.2f},   // "... we're too close to an astral rip, we don't know what will happen"
        {1, TEXT("HeroWizard"), TEXT("fireball_00"), 2.2f},    // "I have to try, we're out of options!"
        {1, TEXT("HeroWizard"), TEXT("fireball_01"), 1.6f},    // "......"
        {1, TEXT("HeroWizard"), TEXT("fireball_02"), 1.2f}};   // "FIREBALL!!" (the blast, later, as it ends)
    constexpr int32 ScriptCount = UE_ARRAY_COUNT(Script);
    constexpr float OutReset = 4.f;     // s out of the sanctum before the scene starts over (the 2D room reloading)
    constexpr float JawOpen = 9.f;      // degrees at the line's loudest
    const TCHAR* const Speakers[] = {TEXT("Fighter"), TEXT("Wizard"), TEXT("Ranger")};

    int32 VoiceIndex(int32 Step)
    {
        for (int32 I = 0; I < NPCVoiceData::LineCount; ++I)
            if (FCString::Strcmp(NPCVoiceData::Lines[I].Npc, Script[Step].Npc) == 0 && FCString::Strcmp(NPCVoiceData::Lines[I].Id, Script[Step].Id) == 0) return I;
        return INDEX_NONE;
    }
    float Loudness(int32 Voice, float T)
    {
        if (Voice == INDEX_NONE || T < 0.f) return 0.f;
        const NPCVoiceData::FLine& L = NPCVoiceData::Lines[Voice];
        if (L.Frames < 2) return 0.f;
        const float F = T * NPCVoiceData::EnvelopeRate;
        const int32 A = FMath::Clamp(FMath::FloorToInt(F), 0, L.Frames - 1), B = FMath::Min(A + 1, L.Frames - 1);
        return FMath::Lerp(static_cast<float>(L.Envelope[A]), static_cast<float>(L.Envelope[B]), F - FMath::FloorToFloat(F)) / 255.f;
    }
}

int32 ATempleSanctumBattle::TalkLineCount() { return ScriptCount; }

void ATempleSanctumBattle::TalkAssetPaths(TArray<FSoftObjectPath>& Paths)
{
    for (int32 S = 0; S < ScriptCount; ++S)
    {
        const int32 V = VoiceIndex(S);
        if (V != INDEX_NONE && *NPCVoiceData::Lines[V].Sound) Paths.Add(FSoftObjectPath(NPCVoiceData::Lines[V].Sound));
    }
}

void ATempleSanctumBattle::FinishTalkLoading()
{
    VoiceSounds.Reset();
    for (int32 S = 0; S < ScriptCount; ++S)
    {
        const int32 V = VoiceIndex(S);
        VoiceSounds.Add(V != INDEX_NONE && *NPCVoiceData::Lines[V].Sound ? Cast<USoundBase>(FSoftObjectPath(NPCVoiceData::Lines[V].Sound).ResolveObject()) : nullptr);
    }
    // Their voices carry over the fight to the gallery and the end of the last leg (the hall is 54 m long).
    VoiceAttenuation = NewObject<USoundAttenuation>(this);
    VoiceAttenuation->Attenuation.bAttenuate = true;
    VoiceAttenuation->Attenuation.bSpatialize = true;
    VoiceAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    VoiceAttenuation->Attenuation.AttenuationShapeExtents = FVector(1500.f, 0.f, 0.f);
    VoiceAttenuation->Attenuation.FalloffDistance = 5000.f;
    VoiceAttenuation->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
    VoiceAttenuation->Attenuation.NonSpatializedRadiusStart = 600.f;   // a hero right beside him is not panned hard to one ear
    VoiceAttenuation->Attenuation.NonSpatializedRadiusEnd = 200.f;
    int32 Found = 0;
    for (USoundBase* S : VoiceSounds) Found += S != nullptr;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_TALK_READY lines=%d sounds=%d"), ScriptCount, Found);
}

void ATempleSanctumBattle::RestartTalk(float GapScale)
{
    if (VoiceAudio) VoiceAudio->Stop();
    TalkStep = 0; TalkPlaying = -1; TalkWait = 0.f; TalkElapsed = 0.f; TalkOut = 0.f; TalkSpoken = 0;
    TalkGapScale = GapScale;
    for (const TWeakObjectPtr<ATempleHero>& H : Heroes) if (H.IsValid()) H->SetSpeech(0.f);
}

void ATempleSanctumBattle::TickTalk(float DeltaSeconds, bool bIn, const AChuckCharacter* Chuck)
{
    // Out of the sanctum a while, or dead and back at a checkpoint: the scene starts over when he comes in again.
    TalkOut = bIn ? 0.f : TalkOut + DeltaSeconds;
    const int32 Respawns = Chuck ? Chuck->GetRespawns() : 0;
    if ((TalkOut > OutReset && (TalkStep > 0 || TalkWait > 0.f)) || Respawns != TalkRespawns)
    {
        TalkRespawns = Respawns;
        RestartTalk(TalkGapScale);
    }
    if (TalkPlaying >= 0)
    {
        TalkElapsed += DeltaSeconds;
        const int32 V = VoiceIndex(TalkPlaying);
        ATempleHero* H = Heroes.IsValidIndex(Script[TalkPlaying].Hero) ? Heroes[Script[TalkPlaying].Hero].Get() : nullptr;
        if (H) H->SetSpeech(Loudness(V, TalkElapsed) * JawOpen);
        if (V == INDEX_NONE || TalkElapsed > NPCVoiceData::Lines[V].Seconds + .1f)
        {
            if (H) H->SetSpeech(0.f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_LINE_END step=%d npc=%s id=%s"), TalkPlaying, Script[TalkPlaying].Npc, Script[TalkPlaying].Id);
            TalkPlaying = -1; TalkWait = 0.f;
            if (TalkStep >= ScriptCount) UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_TALK_DONE lines=%d"), TalkSpoken);
        }
        return;
    }
    if (!bIn || TalkStep >= ScriptCount) return;
    // After the fighter's call (heard as he comes in, in the entrance), the rest wait for him to come down onto the hall's
    // floor (user 2026-10-26: "wait until chuck is on the main floor before picking up the next lines"): the waits only
    // run while he is there, so the argument's 24 s are 24 s of the fight on the floor, however long the stairs took.
    if (TalkStep > 0 && !bOnFloor) return;
    TalkWait += DeltaSeconds;
    if (TalkWait < Script[TalkStep].Gap * TalkGapScale) return;
    // The argument is the Fireball's cutscene (TempleSanctumScene.h, 2026-10-26): it takes the view first, once the
    // beholder is between attacks (and the entrance is over).
    if (TalkStep == FireballFirstLine && bFireballScene && TheScene.IsValid() && !TheScene->IsFireball() && (TheScene->IsPlaying() || !TheScene->StartFireball())) return;
    // His line: from his head, the subtitles running with it.
    const int32 Step = TalkStep++;
    ATempleHero* H = Heroes.IsValidIndex(Script[Step].Hero) ? Heroes[Script[Step].Hero].Get() : nullptr;
    USoundBase* Sound = VoiceSounds.IsValidIndex(Step) ? VoiceSounds[Step] : nullptr;
    if (VoiceAudio) { VoiceAudio->Stop(); VoiceAudio = nullptr; }
    if (H && Sound)
        VoiceAudio = UGameplayStatics::SpawnSoundAttached(Sound, H->GetMesh(), TEXT("head"), FVector(0.f, 0.f, 5.f), EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, VoiceAttenuation);
    TalkPlaying = Step; TalkElapsed = 0.f; ++TalkSpoken;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_HERO_LINE step=%d npc=%s id=%s sound=%d playing=%d"), Step, Script[Step].Npc, Script[Step].Id, Sound != nullptr, VoiceAudio && VoiceAudio->IsPlaying() ? 1 : 0);
}

bool ATempleSanctumBattle::IsHeroSpeaking() const { return TalkPlaying >= 0; }

bool ATempleSanctumBattle::IsScenePlaying() const { return TheScene.IsValid() && TheScene->IsLetterboxed(); }

bool ATempleSanctumBattle::IsTalkSoundPlaying() const { return TalkPlaying >= 0 && VoiceAudio && VoiceAudio->IsPlaying(); }

bool ATempleSanctumBattle::GetHeroSubtitle(FString& Speaker, FString& Text) const
{
    if (TalkPlaying < 0) return false;
    const int32 V = VoiceIndex(TalkPlaying);
    if (V == INDEX_NONE) return false;
    const NPCVoiceData::FLine& L = NPCVoiceData::Lines[V];
    Speaker = Speakers[Script[TalkPlaying].Hero];
    Text = DockSubtitles::At(L.Text, L.Seconds, TalkElapsed);
    return true;
}
