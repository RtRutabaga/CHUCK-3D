#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyRat.generated.h"

class UPoseableMeshComponent;
class USoundBase;
class USoundAttenuation;

/**
 * An ordinary big dock rat, about 31 cm plus its tail (user 2026-09-30: small enemies, "regular largish
 * rats"; Tools/build_enemy_rat.py). Readable rather than hard, in keeping with
 * the game's "feels difficult but isn't": it sniffs about its home, notices
 * Chuck nearby on its level, scurries in, and before every bite gives a clear
 * tell (a crouch and a hiss) so a roll, side jump or step away beats it. It
 * backs off after each lunge. Two slashes: the first knocks it back, the
 * second kills it, and it drops a cigarette or two.
 *
 * No authored clips: the bones are posed procedurally every frame
 * (component-space rotations on the reference pose) - trot, sniff, crouch,
 * lunge, flinch, the death roll.
 */
UCLASS()
class CHUCK3D_API AEnemyRat : public ACharacter
{
    GENERATED_BODY()
public:
    AEnemyRat();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Roam, Chase, Windup, Lunge, Recover, Hurt, Dead };
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    bool IsDead() const { return State == EState::Dead; }
    int32 GetHitsTaken() const { return HitsTaken; }
    /** Struck by Chuck's slash sweeping along Swing: flinch back, or die. */
    void TakeSlash(const FVector& Swing);
    /** Reach footprint for the slash (cm): about the rat's half length. */
    static constexpr float HitRadius = 16.f;
    static constexpr float HitHeight = 18.f;
    // Tuning (see the class comment).
    static constexpr float NoticeRange = 350.f;
    static constexpr float LoseRange = 700.f;
    static constexpr float ChaseSpeed = 170.f;
    static constexpr float RoamSpeed = 60.f;
    static constexpr float StrikeRange = 50.f;     // centre to centre, where it winds up: inside the slash's reach, so the tell is a chance to strike
    static constexpr float WindupTime = .45f;      // the tell
    static constexpr float LungeTime = .25f;
    static constexpr float LungeSpeed = 280.f;
    static constexpr float BiteRange = 45.f;       // centre to centre during the lunge
    static constexpr float RecoverTime = .7f;
    static constexpr int32 Health = 2;
    /** Every live rat, for Chuck's slash (tests too). */
    static const TArray<TWeakObjectPtr<AEnemyRat>>& All();
    /** The docks' rats, away from the start: the cargo wharf, the timber yard, the garden. */
    static void SpawnDockRats(UWorld* World);
    static AEnemyRat* Place(UWorld* World, const FVector2D& At, float Yaw);
    int32 Cigarettes = 1;
    float GetLastWindupSeconds() const { return LastWindupSeconds; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UPoseableMeshComponent* Body;
    UPROPERTY() USoundAttenuation* Attenuation;
    UPROPERTY() TArray<USoundBase*> ChitterSounds;
    UPROPERTY() TArray<USoundBase*> HissSounds;
    UPROPERTY() TArray<USoundBase*> BiteSounds;
    UPROPERTY() TArray<USoundBase*> HurtSounds;
    UPROPERTY() TArray<USoundBase*> DeathSounds;
    EState State = EState::Roam;
    float StateTime = 0;
    FVector Home = FVector::ZeroVector;
    FVector RoamTarget = FVector::ZeroVector;
    float RoamPause = 0;
    float NextChitter = 0;
    bool bBit = false;
    int32 HitsTaken = 0;
    float LastWindupSeconds = 0;
    FVector LungeDirection = FVector::ForwardVector;
    // Procedural pose.
    float GaitPhase = 0;
    float Crouch = 0;          // 0..1
    float Stretch = 0;         // lunge reach 0..1
    float Flinch = 0;          // hurt recoil 0..1
    float Sniff = 0;
    float DeathRoll = 0;
    TArray<int32> BoneIndex;   // by EBone
    void SetState(EState NewState);
    void Play(const TArray<USoundBase*>& Set, float Volume);
    void UpdatePose(float DeltaSeconds);
    void FaceToward(const FVector& Target, float DeltaSeconds, float Rate);
};
