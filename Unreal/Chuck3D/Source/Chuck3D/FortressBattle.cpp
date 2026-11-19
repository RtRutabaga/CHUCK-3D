// The trio's battle before Phlegethos's fortress (Claude, 2026-11-16; the user: "go ahead and do the trio encounter";
// docs/regions/PHLEGETHOS.md#the-trios-battle). The 2D game's Phase 8 fortress climax: "The trio of adventurers are already
// battling a Pit Fiend. The fighter engages lesser devils. The wizard casts powerful spells. The female ranger fires
// directly at the Pit Fiend ... Chuck is not participating; he survives by avoiding hazards." Its entrance dialogue
// (data/dialogue/phlegethos_fortress.json) as the view takes in the battle.
//
// The sanctum's battle (TempleSanctumBattle.cpp) on a second site: its heroes, their arrows, bolts, sounds and the wizard's
// seal, here over the breach in the forecourt (PhlegethosSetting.h). Lemures (the lesser devils) come up out of the
// forecourt's edges at the fighter; the Pit Fiend (FortressFiend.cpp) stands before the breach and the gate.
#include "TempleSanctumBattle.h"
#include "ChuckCharacter.h"
#include "ChultTempleWind.h"
#include "ChuckFlames.h"
#include "DockSubtitles.h"
#include "DockGameMode.h"
#include "NPCVoiceData.h"
#include "PhlegethosSetting.h"
#include "PhlegethosCreatures.h"
#include "PhlegethosBleed.h"
#include "TempleBeholder.h"
#include "TempleHero.h"
#include "TempleSeal.h"
#include "TempleSkeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
    FVector FortW(float X, float Y, float Dz = 0.f) { return PhlegWorld(FVector(X, Y, Phleg::ApproachGround(X, Y) + Dz)); }
    float FortYaw(const FVector& From, const FVector& To) { return static_cast<float>((To - From).GetSafeNormal2D().Rotation().Yaw); }
    // The cast's ground (local X, Y): the fiend before the gate right of the processional; the fighter between it and the
    // breach's side; the wizard at the breach; the ranger back on the right with a clear line at the fiend.
    // (2026-11-17: the whole battle on the bleedthrough's island, Phleg::IslandX0..X1 41433..43167, the user: Chuck shut in
    // on the battle's side; the fiend at 42880, before the front moat and the idols' ledge at the wall, was at 43150)
    constexpr float FiendX = 42880, FiendY = 1250;
    const FVector2D HeroAt[3] = {{42500, 900}, {42180, -950}, {42200, 2000}};
    // The lemures' ground: along the island's sides, off the river, the breach and the cast.
    const FVector2D LemureAt[] = {{42950, 2150}, {42550, 2300}, {42150, 2250}, {42950, -2200}, {42600, -2300}, {42150, -2250}};
    constexpr int32 FortWantUp = 3;   // (2026-11-17: more of them at once, the fight more intense; was 2)
    constexpr int32 BallSlots = 2, BurstSlots = 3;
    const FVector Away(0, 0, -1e6);
    // The trio's lines (2D data/dialogue/phlegethos_fortress.json, "phlegethos_battle_entrance"), one to each of them in the
    // order the sanctum's are (fighter, wizard, ranger), between the fiend's two in Infernal (2026-11-18, the user's
    // ElevenLabs takes; its subtitles are unintelligible Infernal, the user's ask; "have the fiends first line be the first
    // line in the scene and his last line be the last line spoken"). The entrance plays them one after the other (Gap:
    // seconds after the one before ends; the first's from the scene's start). Their words and lengths are NPCVoiceData's.
    struct FFortLine { int32 Who; const TCHAR* Npc; const TCHAR* Id; float Gap; };   // Who: 0 fighter, 1 wizard, 2 ranger, 3 the fiend
    const FFortLine FortLines[] = {
        {3, TEXT("PitFiend"), TEXT("fiend_00"), .7f},          // (Infernal: at them)
        {0, TEXT("HeroFighter"), TEXT("fortress_00"), .45f},   // "The fractures are spreading faster than we can bind them!"
        {1, TEXT("HeroWizard"), TEXT("fortress_00"), .35f},    // "Then we hold this one shut. Keep that thing away from the breach!"
        {2, TEXT("HeroRanger"), TEXT("fortress_00"), .35f},    // "I have the fiend. Finish the repair!"
        {3, TEXT("PitFiend"), TEXT("fiend_01"), .5f}};         // (Infernal: its answer, the last word)
    constexpr int32 FortEntranceLines = UE_ARRAY_COUNT(FortLines);
    const TCHAR* const FortSpeakers[] = {TEXT("Fighter"), TEXT("Wizard"), TEXT("Ranger"), TEXT("Pit Fiend")};
    int32 FortVoice(int32 Line)
    {
        for (int32 I = 0; I < NPCVoiceData::LineCount; ++I)
            if (FCString::Strcmp(NPCVoiceData::Lines[I].Npc, FortLines[Line].Npc) == 0 && FCString::Strcmp(NPCVoiceData::Lines[I].Id, FortLines[Line].Id) == 0) return I;
        return INDEX_NONE;
    }
    const TCHAR* FortText(int32 Line) { const int32 V = FortVoice(Line); return V == INDEX_NONE ? TEXT("") : NPCVoiceData::Lines[V].Text; }
    float FortSeconds(int32 Line) { const int32 V = FortVoice(Line); return V == INDEX_NONE ? 3.f : FMath::Max(1.f, NPCVoiceData::Lines[V].Seconds); }
    /** When each entrance line starts on the scene's clock, and when the scene ends (after the fiend's answer). */
    float FortLineAt(int32 Line)
    {
        float At = FortLines[0].Gap;
        for (int32 K = 1; K <= Line && K < FortEntranceLines; ++K) At += FortSeconds(K - 1) + FortLines[K].Gap;
        return At;
    }
    float FortSceneLength() { return FortLineAt(FortEntranceLines - 1) + FortSeconds(FortEntranceLines - 1) + .5f; }
    float FortLoudness(int32 Line, float T)
    {
        const int32 V = FortVoice(Line);
        if (V == INDEX_NONE || T < 0.f) return 0.f;
        const NPCVoiceData::FLine& L = NPCVoiceData::Lines[V];
        if (L.Frames < 2) return 0.f;
        const float F = T * NPCVoiceData::EnvelopeRate;
        const int32 A = FMath::Clamp(FMath::FloorToInt(F), 0, L.Frames - 1), B = FMath::Min(A + 1, L.Frames - 1);
        return FMath::Lerp(static_cast<float>(L.Envelope[A]), static_cast<float>(L.Envelope[B]), F - FMath::FloorToFloat(F)) / 255.f;
    }
}

FVector ATempleSanctumBattle::BigFoeCentre() const
{
    if (const ATempleBeholder* Bh = TheBeholder.Get()) return Bh->GetCentre();
    if (const ATempleSkeleton* Fd = TheFiend.Get()) return Fd->GetMesh()->GetBoneLocation(TEXT("spine_03"));
    return Beholder;
}

bool ATempleSanctumBattle::IsBigFoeFighting() const
{
    if (const ATempleBeholder* Bh = TheBeholder.Get()) return Bh->IsFighting();
    return TheFiend.IsValid() && bActive && !IsFortressScenePlaying();
}

bool ATempleSanctumBattle::HoleAt(const FVector& World, float Margin) const
{
    if (IsFortress())
    {
        const FVector L = PhlegLocal(World);
        if (const APhlegBleed* Bl = APhlegBleed::Get(GetWorld()); Bl && Bl->OpenAt(static_cast<float>(L.X), static_cast<float>(L.Y))) return true;   // (a fallen slab, 2026-11-17)
        return Phleg::BreachInside(static_cast<float>(L.X), static_cast<float>(L.Y)) > -Margin;
    }
    const FVector F = ChultTempleInteriorFrameOf(World);
    return ChultTempleSanctumHoleAt(FVector2D(F.X, F.Y), Margin) != 0;
}

void ATempleSanctumBattle::SpawnFortressCast()
{
    UWorld* World = GetWorld();
    const FVector FiendFeet = FortW(FiendX, FiendY);
    for (int32 R = 0; R < 3; ++R)
    {
        const FVector Feet = FortW(HeroAt[R].X, HeroAt[R].Y);
        const float Yaw = R == 1 ? FortYaw(Feet, FortW(Phleg::BreachX, Phleg::BreachY)) : FortYaw(Feet, FiendFeet);
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* H = World->SpawnActor<ATempleHero>(Feet + FVector(0, 0, ATempleHero::HalfHeight + 1), FRotator(0, Yaw, 0), P);
        if (!H) continue;
        H->Setup(static_cast<ATempleHero::ERole>(R), this, HeroMeshes[R], HeroClips[R]);
        H->SetHome(Feet, Yaw);
        Heroes.Add(H);
    }
    // The wizard's binding over the breach.
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto* Seal = World->SpawnActor<ATempleSeal>(FVector::ZeroVector, FRotator::ZeroRotator, P))
        {
            Seal->Setup(this, SealWebMat, BeholderFx.IsValidIndex(0) ? BeholderFx[0] : nullptr, BeholderFx.IsValidIndex(2) ? BeholderFx[2] : nullptr, Plane);
            Seal->PlaceAtBreach();
            TheSeal = Seal;
            if (ATempleHero* Wz = GetHero(1)) Wz->Rest();
        }
    }
    // The Pit Fiend before the gate, facing the trio.
    {
        FTempleSkeletonPost Post;
        Post.Kind = 2; Post.Space = TEXT("fortress");
        Post.Feet = FiendFeet;
        Post.Yaw = FortYaw(FiendFeet, FortW(HeroAt[0].X, HeroAt[0].Y));
        Post.Leash = FBox2D(FVector2D(FiendX - 1100, FiendY - 1500), FVector2D(Phleg::IslandX1 - 160, FiendY + 1100));
        if (ATempleSkeleton* F = ATempleSkeleton::SpawnAt(World, Post))
        {
            F->SetSanctum(this);
            F->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            TheFiend = F;
        }
    }
    // The lemures lying along the forecourt's edges.
    const FBox2D Leash(FVector2D(Phleg::RiverX1 + 60, Phleg::IslandY0 + 80), FVector2D(Phleg::IslandX1 - 80, Phleg::IslandY1 - 80));
    for (const FVector2D& L : LemureAt)
    {
        FTempleSkeletonPost Post;
        Post.Kind = 1; Post.bDormant = true; Post.Space = TEXT("fortress");
        Post.Feet = FortW(L.X, L.Y);
        Post.Yaw = FortYaw(Post.Feet, FortW(HeroAt[0].X, HeroAt[0].Y)) + Random.FRandRange(-30.f, 30.f);
        Post.Leash = Leash;
        Reserve.Add(Post);
        if (ATempleSkeleton* S = ATempleSkeleton::SpawnAt(World, Post))
        {
            S->SetSanctum(this);
            Pool.Add(S);
        }
    }
    // The fire's flames (ChuckFlames: the flipbook fire every brazier uses): the balls in flight and the bursts, parked away
    // until wanted.
    if (AChuckFlames* Fire = AChuckFlames::Get(World))
    {
        for (int32 K = 0; K < BallSlots; ++K) BallFlames.Add(Fire->AddMoving(this, Away, 46.f, 70.f));
        for (int32 K = 0; K < BurstSlots; ++K) BurstFlames.Add(Fire->AddMoving(this, Away, 150.f, 230.f));
    }
    FireLight = NewObject<UPointLightComponent>(this);
    FireLight->SetupAttachment(RootComponent); FireLight->SetLightColor(FLinearColor(1.f, .45f, .15f)); FireLight->SetIntensity(0.f);
    FireLight->SetAttenuationRadius(1400.f); FireLight->SetCastShadows(false); FireLight->RegisterComponent();
    // Lit by Phlegethos's lights (lighting channel 0), not the temple interior's torches (1): the heroes, their hair and
    // gear, the battle's flying things and flashes, the seal.
    {
        TArray<AActor*> Lit = {this};
        for (const TWeakObjectPtr<ATempleHero>& H : Heroes) if (H.IsValid()) Lit.Add(H.Get());
        if (TheSeal.IsValid()) Lit.Add(TheSeal.Get());
        for (AActor* A : Lit)
        {
            TArray<UPrimitiveComponent*> Prims; A->GetComponents(Prims);
            for (UPrimitiveComponent* P : Prims) P->SetLightingChannels(true, false, false);
            TArray<ULightComponent*> Lights; A->GetComponents(Lights);
            for (ULightComponent* L : Lights) L->SetLightingChannels(true, false, false);
        }
    }
    // The forecourt's own camera for the entrance.
    FortCam = World->SpawnActor<ACameraActor>();
    // (2026-11-18) A warm light on the fiend's face for its two shots in the entrance (it stands against the fires behind).
    FaceLight = NewObject<UPointLightComponent>(this);
    FaceLight->SetupAttachment(RootComponent); FaceLight->RegisterComponent();
    FaceLight->SetIntensityUnits(ELightUnits::Candelas); FaceLight->SetIntensity(0.f);
    FaceLight->SetLightColor(FLinearColor(1.f, .72f, .52f)); FaceLight->SetAttenuationRadius(900.f); FaceLight->SetCastShadows(false);
    FaceLight->SetSourceRadius(80.f); FaceLight->SetLightingChannels(true, false, false);
    if (FortCam) { FortCam->GetCameraComponent()->SetFieldOfView(50.f); FortCam->GetCameraComponent()->bConstrainAspectRatio = false; }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_BATTLE heroes=%d fiend=%d lemures=%d seal=%d flames=%d"), Heroes.Num(), TheFiend.IsValid() ? 1 : 0, Pool.Num(),
        TheSeal.IsValid() ? 1 : 0, BallFlames.Num() + BurstFlames.Num());
}

void ATempleSanctumBattle::TickFortress(float Dt, AChuckCharacter* Chuck, const FVector& Eye)
{
    // On while he is on the forecourt or the last of the path to it (in the wastes it is heard, not fought).
    bool bIn = false;
    if (Chuck && InPhlegethos(Chuck->GetActorLocation()))
    {
        const FVector L = PhlegLocal(Chuck->GetActorLocation());
        bIn = L.X > Phleg::ForecourtX - 1800 && FMath::Abs(L.Y) < 4500 && L.Z > Phleg::FortressTop - 500 && L.Z < Phleg::FortressTop + 1500;
    }
    const bool bWas = bActive;
    bActive = (bIn || bForced) && !APhlegFlameskull::bCalm;   // (Phlegethos's trials of the moves: held, as its other creatures)
    if (bActive != bWas || !bTickModeSet)
    {
        bTickModeSet = true;
        const EVisibilityBasedAnimTickOption Mode = bActive ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        for (const TWeakObjectPtr<ATempleSkeleton>& S : Pool) if (S.IsValid()) S->GetMesh()->VisibilityBasedAnimTickOption = Mode;
        for (const TWeakObjectPtr<ATempleHero>& H : Heroes) if (H.IsValid()) H->GetMesh()->VisibilityBasedAnimTickOption = Mode;
        if (TheFiend.IsValid()) TheFiend->GetMesh()->VisibilityBasedAnimTickOption = Mode;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_ACTIVE %d"), bActive ? 1 : 0);
    }
    const float Wt = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (bActive && bWaves && GetUp() < FortWantUp && Wt >= NextWake) { WakeOne(nullptr); NextWake = Wt + Random.FRandRange(3.f, 6.f); }
    if (bWaves) Recycle();
    bOnFloor = bIn;
    // The heroes' dodges, at their moment.
    for (int32 K = Dodges.Num() - 1; K >= 0; --K)
    {
        FDodge& D = Dodges[K];
        if (Wt < D.When) continue;
        if (ATempleHero* H = D.Hero.Get())
        {
            if (D.bSmash) { H->EvadeFiendSmash(D.Past); ++SmashDodges; }
            else { H->EvadeFire(D.From, D.Past); ++FireDodges; }
        }
        Dodges.RemoveAt(K);
    }
    TickFortressScene(Dt, Chuck);
    TickFortressLines(Dt);
    TickFortressFire(Dt, Chuck);
    TickFlying(Dt, Eye);
    Draw(Eye);
}

// ---------------------------------------------------------------------------- the fiend's blows on the heroes

void ATempleSanctumBattle::FiendSmashComing(ATempleHero* At, const FVector& Ground, float Lead, float Chance)
{
    if (!At) return;
    // Mostly he sees it come and is out from under it as it falls (0.25 s before it lands); now and then he is caught
    // bracing, and the blast throws him. (Lead: seconds from now he moves; the leap and the sweep, 2026-11-17.)
    if (Random.FRand() < Chance)
    {
        FDodge D; D.Hero = At; D.When = static_cast<float>(GetWorld()->GetTimeSeconds()) + Lead; D.Past = Ground; D.bSmash = true;
        Dodges.Add(D);
    }
}

void ATempleSanctumBattle::FiendBlast(const FVector& At, float Strength)
{
    if (Strength < .5f) return;
    for (const TWeakObjectPtr<ATempleHero>& Wk : Heroes)
    {
        ATempleHero* H = Wk.Get();
        if (!H) continue;
        const float D = static_cast<float>(FVector::Dist2D(H->GetActorLocation(), At));
        const float R = 160.f + 260.f * Strength;
        if (D < R) H->TakeFiendBlast(At, Strength * (1.f - .6f * D / R));
    }
    // The lemures near it are thrown down too (it does not care).
    for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Pool)
        if (ATempleSkeleton* S = Wk.Get(); S && S->IsStanding() && FVector::Dist2D(S->GetActorLocation(), At) < 140.f + 120.f * Strength)
            if (S->TakeHeroBlow(EBlow::Bolt, At, S->GetActorLocation())) ++Fallen;
}

void ATempleSanctumBattle::FireballGather(ATempleSkeleton* Fiend)
{
    for (int32 Slot = 0; Slot < BallSlots; ++Slot)
    {
        if (Fireballs.ContainsByPredicate([Slot](const FFireball& F) { return F.Slot == Slot; })) continue;
        FFireball F; F.Owner = Fiend; F.Slot = Slot; F.bLoose = false;
        F.Pos = Fiend->GetMesh()->GetBoneLocation(TEXT("hand_l"));
        Fireballs.Add(F);
        Sound(ESound::BoltCast, F.Pos, 1.f, .55f);
        return;
    }
}

void ATempleSanctumBattle::FireballLoose(ATempleSkeleton* Fiend, ATempleHero* At)
{
    for (FFireball& F : Fireballs)
    {
        if (F.Owner.Get() != Fiend || F.bLoose) continue;
        F.bLoose = true; F.Age = 0; F.Target = At;
        FVector Aim = At ? At->GetActorLocation() + FVector(0, 0, 20) : F.Pos + Fiend->GetActorForwardVector() * 1500.f - FVector(0, 0, 300);
        const float Speed = 1700.f, Tf = static_cast<float>(FVector::Dist(F.Pos, Aim)) / Speed;
        Aim.Z += .5f * 300.f * Tf * Tf;   // (a little drop over the flight)
        F.Vel = (Aim - F.Pos).GetSafeNormal() * Speed;
        Sound(ESound::ArrowFlight, F.Pos, 1.f, .5f);
        // The ranger and the wizard see it coming (most of the time) and throw themselves aside as it arrives.
        if (At && Random.FRand() < .55f)
        {
            FDodge D; D.Hero = At; D.When = static_cast<float>(GetWorld()->GetTimeSeconds()) + FMath::Max(0.f, Tf - .55f); D.From = F.Pos; D.Past = Aim; D.bSmash = false;
            Dodges.Add(D);
        }
        return;
    }
}

void ATempleSanctumBattle::FireBurst(const FVector& At, ATempleHero* Struck)
{
    ++FireBursts;
    Sound(ESound::BoltImpact, At, 1.f, .55f);
    Sound(ESound::SealBreak, At, .7f, .6f);
    if (APhlegethosSetting* Stage = APhlegethosSetting::Get(GetWorld())) Stage->GroundBlow(FVector(At.X, At.Y, Floor), .8f, true);
    BeholderShake(GetWorld(), At, .4f, 2600.f, 0);
    FireFlash = 1.f; FireLight->SetWorldLocation(At + FVector(0, 0, 120));
    // A burst of flame where it struck, a moment.
    for (int32 Slot = 0; Slot < BurstSlots; ++Slot)
        if (!Bursts.ContainsByPredicate([Slot](const FBurst& B) { return B.Slot == Slot; }))
        {
            FBurst B; B.At = FVector(At.X, At.Y, Floor + 4.f); B.Slot = Slot; Bursts.Add(B);
            break;
        }
    // The heroes it reaches: the one it struck, thrown; any beside, staggered.
    for (const TWeakObjectPtr<ATempleHero>& Wk : Heroes)
    {
        ATempleHero* H = Wk.Get();
        if (!H) continue;
        const float D = static_cast<float>(FVector::Dist2D(H->GetActorLocation(), At));
        if (H == Struck || D < 110.f) { H->TakeFiendBlast(At, H == Struck ? .9f : .6f); ++FireHeroHits; }
    }
    // Him: burned in the middle of it; thrown off his feet near it.
    if (auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FVector C = Chuck->GetActorLocation();
        const float D = static_cast<float>(FVector::Dist2D(C, At));
        if (D < 340.f && FMath::Abs(static_cast<float>(C.Z - At.Z)) < 220.f)
        {
            if (D < 150.f) Chuck->TakeBite(At, 1, false, true);
            const float K = D / 340.f;
            Chuck->Blasted(At, FMath::Lerp(560.f, 300.f, K), FMath::Lerp(380.f, 240.f, K));
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIRE_BURST bursts=%d struck=%s chuck_dist=%.0f"), FireBursts, Struck ? Struck->GetRoleName() : TEXT("-"), D);
    }
}

void ATempleSanctumBattle::TickFortressFire(float Dt, AChuckCharacter* Chuck)
{
    AChuckFlames* Fire = AChuckFlames::Get(GetWorld());
    for (int32 I = Fireballs.Num() - 1; I >= 0; --I)
    {
        FFireball& F = Fireballs[I];
        ATempleSkeleton* Fd = F.Owner.Get();
        if (!F.bLoose)
        {
            // In its left hand, growing hot as the arm draws back.
            if (!Fd || Fd->GetState() != ATempleSkeleton::EState::Attack) { if (Fire && BallFlames.IsValidIndex(F.Slot)) Fire->SetBase(BallFlames[F.Slot], Away); Fireballs.RemoveAt(I); continue; }
            F.Pos = Fd->GetMesh()->GetBoneLocation(TEXT("hand_l")) + FVector(0, 0, 10);
        }
        else
        {
            F.Age += Dt;
            const FVector From = F.Pos;
            F.Vel.Z -= 300.f * Dt;
            F.Pos += F.Vel * Dt;
            if (FMath::Fmod(F.Age, .05f) < Dt) FirePuffs(F.Pos, -F.Vel * .1f, 1, 22.f);
            // Into a hero (by the bones, not their place), the ground, or after 3 s.
            ATempleHero* Hit = nullptr;
            for (const TWeakObjectPtr<ATempleHero>& Wk : Heroes)
            {
                ATempleHero* H = Wk.Get();
                if (!H) continue;
                const FVector Q = FMath::ClosestPointOnSegment(H->GetMesh()->GetBoneLocation(TEXT("spine_03")), From, F.Pos);
                if (H->GapTo(Q) < 18.f) { Hit = H; break; }
            }
            FHitResult Gh;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(FortressFireball), false, this);
            if (Fd) Q.AddIgnoredActor(Fd);
            if (Chuck) Q.AddIgnoredActor(Chuck);
            for (const TWeakObjectPtr<ATempleHero>& Wk : Heroes) if (Wk.IsValid()) Q.AddIgnoredActor(Wk.Get());
            for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Pool) if (Wk.IsValid()) Q.AddIgnoredActor(Wk.Get());
            const bool bGround = GetWorld()->LineTraceSingleByChannel(Gh, From, F.Pos, ECC_Visibility, Q);
            // (it passes over him close enough to burn him)
            if (Chuck && FMath::PointDistToSegment(Chuck->GetActorLocation(), From, F.Pos) < 60.f) { FireBurst(Chuck->GetActorLocation(), nullptr); }
            else if (Hit || bGround || F.Age > 3.f || F.Pos.Z < Floor)
                FireBurst(Hit ? FMath::ClosestPointOnSegment(Hit->GetMesh()->GetBoneLocation(TEXT("spine_03")), From, F.Pos) : bGround ? FVector(Gh.ImpactPoint) : F.Pos, Hit);
            else { if (Fire && BallFlames.IsValidIndex(F.Slot)) Fire->SetBase(BallFlames[F.Slot], F.Pos - FVector(0, 0, 25)); continue; }
            if (Fire && BallFlames.IsValidIndex(F.Slot)) Fire->SetBase(BallFlames[F.Slot], Away);
            Fireballs.RemoveAt(I);
            continue;
        }
        if (Fire && BallFlames.IsValidIndex(F.Slot)) Fire->SetBase(BallFlames[F.Slot], F.Pos - FVector(0, 0, 25));
    }
    for (int32 I = Bursts.Num() - 1; I >= 0; --I)
    {
        FBurst& B = Bursts[I];
        B.Age += Dt;
        // (sunk into the ground as it dies down)
        const FVector P = B.At - FVector(0, 0, 230.f * FMath::Clamp((B.Age - .35f) / .6f, 0.f, 1.f));
        if (Fire && BurstFlames.IsValidIndex(B.Slot)) Fire->SetBase(BurstFlames[B.Slot], B.Age < .95f ? P : Away);
        if (B.Age > .95f) Bursts.RemoveAt(I);
    }
    FireFlash = FMath::Max(0.f, FireFlash - Dt * 1.6f);
    if (FireLight) FireLight->SetIntensity(90000.f * FireFlash * FireFlash);
    // The fireball's own glow.
    if (FireLight && FireFlash <= 0.f)
        for (const FFireball& F : Fireballs) { FireLight->SetWorldLocation(F.Pos); FireLight->SetIntensity(F.bLoose ? 9000.f : 4000.f); break; }
}

// ---------------------------------------------------------------------------- the entrance and the lines

void ATempleSanctumBattle::TickFortressScene(float Dt, AChuckCharacter* Chuck)
{
    const float Wt = static_cast<float>(GetWorld()->GetTimeSeconds());
    // Once a session: as he first comes out onto the forecourt on his feet (a moment there, as in the sanctum).
    if (FortClock < 0.f)
    {
        if (!bFortScene || !bActive || !Chuck || !FortCam || FortEntrances > 0) return;
        const FVector L = PhlegLocal(Chuck->GetActorLocation());
        const UCharacterMovementComponent* Move = Chuck->GetCharacterMovement();
        const bool bOn = InPhlegethos(Chuck->GetActorLocation()) && L.X > Phleg::ForecourtX + 60 && FMath::Abs(L.Y) < 3000 && Move && Move->IsMovingOnGround() && !Chuck->IsAstral();
        FortDwell = bOn ? FortDwell + Dt : 0.f;
        if (FortDwell < .6f) return;
        FortClock = 0.f; bFortReleased = false; bFortSkipping = false; ++FortEntrances; FortLine = -1; FortSpoken = 0;
        if (auto* PC = Cast<APlayerController>(Chuck->GetController()))
        {
            Chuck->DisableInput(PC); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false);
            PC->SetViewTarget(FortCam);
        }
        // (the fiend speaks first; it brings its mace down on the fighter as he calls out, then throws at the ranger as she
        // answers: FortSay)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_ENTRANCE start"));
    }
    if (FortClock < 0.f) return;
    FortClock += Dt;
    if (!bFortReleased && Chuck)
        if (auto* PC = Cast<APlayerController>(Chuck->GetController()); PC && PC->WasInputKeyJustPressed(EKeys::Escape)) SkipFortressScene();
    // The lines, each on its shot, one after the other (TickFortressLines plays them).
    {
        const int32 K = FMath::Max(FortLine + 1, 0);
        if (FortLine < FortEntranceLines - 1 && K < FortEntranceLines && FortClock >= FortLineAt(K) && !bFortSkipping) FortSay(K);
    }
    if (bFortReleased) return;   // (the last line may still be finishing)
    // The shots, one on each speaker's line.
    ATempleHero* F = GetHero(0); ATempleHero* Wz = GetHero(1); ATempleHero* Rg = GetHero(2);
    ATempleSkeleton* Fd = TheFiend.Get();
    if (!F || !Wz || !Rg || !Fd) { FortressRelease(Chuck); return; }
    const FVector Chest = BigFoeCentre();
    FVector Eye, Aim;
    const float SceneEnd = FortSceneLength();
    int32 Shot = 0;
    while (Shot + 1 < FortEntranceLines && FortClock >= FortLineAt(Shot + 1) - .2f) ++Shot;
    const float ShotFrom = Shot == 0 ? 0.f : FortLineAt(Shot) - .2f, ShotTo = Shot + 1 < FortEntranceLines ? FortLineAt(Shot + 1) - .2f : SceneEnd;
    const float U = FMath::Clamp((FortClock - ShotFrom) / FMath::Max(.5f, ShotTo - ShotFrom), 0.f, 1.f);
    const int32 Who = FortLines[Shot].Who;
    if (Who == 3)
    {
        // The fiend: low before it, from the trio's side, looking up at its head as it speaks, slowly in (its first line
        // wider, from further off and higher, the battle round it; its last close).
        const bool bFirst = Shot == 0;
        const FVector Head = Fd->GetMesh()->GetBoneLocation(TEXT("head"));
        const FVector Fwd = Fd->GetActorForwardVector().GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Fwd);
        if (bFirst)
        {
            Eye = Fd->GetActorLocation() + Fwd * (1500.f - 220.f * U) + Side * (-340.f + 60.f * U) + FVector(0, 0, 190.f + 30.f * U);
            Aim = FMath::Lerp(Chest, Head, .55f);
        }
        else
        {
            // (its face close, at its head's height, three-quarters from the side away from the mace it holds up)
            const FVector HeadUp = Head - Fd->GetActorLocation();
            const FVector Mace = Fd->GetMesh()->GetBoneLocation(TEXT("hand_r")) - Fd->GetActorLocation();
            const float OffMace = FVector::DotProduct(Mace, Side) > 0.f ? -1.f : 1.f;
            Eye = Fd->GetActorLocation() + Fwd * (600.f - 100.f * U) + Side * OffMace * (580.f - 40.f * U) + FVector(0, 0, HeadUp.Z * .95f);
            Aim = Head + FVector(0, 0, -25.f);
        }
    }
    else if (Who == 0)
    {
        // Wide from behind the fighter over the battle to the fiend (from the open ground right of the processional).
        const FVector Back = (F->GetActorLocation() - Fd->GetActorLocation()).GetSafeNormal2D();
        Eye = F->GetActorLocation() + Back * (900.f - 160.f * U) + FVector(0, 1, 0) * (420.f - 80.f * U) + FVector(0, 0, 330.f - 50.f * U);
        Aim = FMath::Lerp(F->GetActorLocation(), Chest, .62f) + FVector(0, 0, 40.f);
    }
    else if (Who == 1)
    {
        // The wizard at the breach, the tear at our feet.
        const FVector Tear = PhlegWorld(FVector(Phleg::BreachX, Phleg::BreachY, Phleg::FortressTop));
        const FVector Back = (Wz->GetActorLocation() - Tear).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Back);
        Eye = Wz->GetActorLocation() + Back * (300.f - 50.f * U) + Side * (-120.f + 30.f * U) + FVector(0, 0, 150.f + 20.f * U);
        Aim = FMath::Lerp(Wz->HeadLocation(), Tear, .6f);
    }
    else
    {
        // Over the ranger's shoulder at the fiend.
        const FVector Back = (Rg->GetActorLocation() - Fd->GetActorLocation()).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Back);
        Eye = Rg->GetActorLocation() + Back * (230.f + 60.f * U) + Side * 70.f + FVector(0, 0, 95.f);
        Aim = Chest - FVector(0, 0, 60.f);
    }
    {
        // (lifted until nothing stands between it and what it looks at)
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FortressCam), false, this);
        Q.AddIgnoredActor(Fd); for (int32 R = 0; R < 3; ++R) if (ATempleHero* H = GetHero(R)) Q.AddIgnoredActor(H);
        if (Chuck) Q.AddIgnoredActor(Chuck);
        for (const TWeakObjectPtr<ATempleSkeleton>& Wk : Pool) if (Wk.IsValid()) Q.AddIgnoredActor(Wk.Get());
        FHitResult Hit;
        for (int32 K = 0; K < 6 && GetWorld()->LineTraceSingleByChannel(Hit, Aim, Eye, ECC_Visibility, Q); ++K) Eye.Z += 120.f;
    }
    FortCam->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
    if (FaceLight)
    {
        // (on the fiend's face from the camera's side while it speaks in the entrance)
        const bool bOnFace = FortLines[Shot].Who == 3;
        const FVector FaceAt = Fd->GetMesh()->GetBoneLocation(TEXT("head"));
        FaceLight->SetWorldLocation(FaceAt + (Eye - FaceAt).GetSafeNormal() * 260.f + FVector(0, 0, 60.f));
        FaceLight->SetIntensity(bOnFace ? 35.f : 0.f);
    }
    if (FortClock >= SceneEnd) FortressRelease(Chuck);
}

void ATempleSanctumBattle::FortressRelease(AChuckCharacter* Chuck)
{
    if (bFortReleased) return;
    bFortReleased = true;
    if (auto* PC = Chuck ? Cast<APlayerController>(Chuck->GetController()) : nullptr)
    {
        PC->SetViewTargetWithBlend(Chuck, bFortSkipping ? 0.f : .8f, VTBlend_EaseInOut, 2.f);
        Chuck->EnableInput(PC);
    }
    if (FaceLight) FaceLight->SetIntensity(0.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_ENTRANCE end clock=%.1f skipped=%d lines=%d"), FortClock, bFortSkipping ? 1 : 0, FortSpoken);
}

void ATempleSanctumBattle::SkipFortressScene()
{
    if (FortClock < 0.f || bFortReleased) return;
    bFortSkipping = true;
    for (int32 R = 0; R < 3; ++R) if (ATempleHero* H = GetHero(R)) H->SetSpeech(0.f);
    if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->SetFortSpeech(-1.f);
    if (FortVoiceAudio) { FortVoiceAudio->Stop(); FortVoiceAudio = nullptr; }
    FortLine = 99;
    FortressRelease(Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

bool ATempleSanctumBattle::GetFortressSubtitle(FString& Speaker, FString& Text) const
{
    if (FortLine < 0 || FortLine >= UE_ARRAY_COUNT(FortLines)) return false;
    const float Len = FortSeconds(FortLine);
    if (FortTalkT > Len) return false;
    Speaker = FortSpeakers[FortLines[FortLine].Who];
    Text = DockSubtitles::At(FortText(FortLine), Len, FortTalkT);
    return true;
}

bool ATempleSanctumBattle::IsFiendSpeaking() const
{
    return IsFortress() && FortLine >= 0 && FortLine < UE_ARRAY_COUNT(FortLines) && FortLines[FortLine].Who == 3 && FortTalkT <= FortSeconds(FortLine);
}

void ATempleSanctumBattle::FortSay(int32 Line)
{
    if (Line < 0 || Line >= UE_ARRAY_COUNT(FortLines)) return;
    // The last one's speaker quiet; this one from its speaker's head, the subtitles running with it.
    if (FortLine >= 0 && FortLine < UE_ARRAY_COUNT(FortLines))
    {
        if (FortLines[FortLine].Who < 3) { if (ATempleHero* H = GetHero(FortLines[FortLine].Who)) H->SetSpeech(0.f); }
        else if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->SetFortSpeech(-1.f);
    }
    FortLine = Line; FortTalkT = 0.f; ++FortSpoken;
    if (FortVoiceAudio) { FortVoiceAudio->Stop(); FortVoiceAudio = nullptr; }
    if (!VoiceAttenuation)
    {
        // Their voices carry across the forecourt (the island is 50 m long), one beside him not panned hard to one ear.
        VoiceAttenuation = NewObject<USoundAttenuation>(this);
        VoiceAttenuation->Attenuation.bAttenuate = true;
        VoiceAttenuation->Attenuation.bSpatialize = true;
        VoiceAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        VoiceAttenuation->Attenuation.AttenuationShapeExtents = FVector(2000.f, 0.f, 0.f);
        VoiceAttenuation->Attenuation.FalloffDistance = 6000.f;
        VoiceAttenuation->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
        VoiceAttenuation->Attenuation.NonSpatializedRadiusStart = 600.f;
        VoiceAttenuation->Attenuation.NonSpatializedRadiusEnd = 200.f;
    }
    const int32 V = FortVoice(Line);
    if (FortSounds.Num() < UE_ARRAY_COUNT(FortLines)) FortSounds.SetNum(UE_ARRAY_COUNT(FortLines));
    if (!FortSounds[Line] && V != INDEX_NONE && *NPCVoiceData::Lines[V].Sound) FortSounds[Line] = LoadObject<USoundBase>(nullptr, NPCVoiceData::Lines[V].Sound);
    USoundBase* Sound = FortSounds[Line];
    USkeletalMeshComponent* Mesh = nullptr;
    if (FortLines[Line].Who < 3) { if (ATempleHero* H = GetHero(FortLines[Line].Who)) Mesh = H->GetMesh(); }
    else if (ATempleSkeleton* Fd = TheFiend.Get()) Mesh = Fd->GetMesh();
    if (Mesh && Sound)
        FortVoiceAudio = UGameplayStatics::SpawnSoundAttached(Sound, Mesh, TEXT("head"), FVector(0.f, 0.f, 5.f), EAttachLocation::KeepRelativeOffset, true,
            FortLines[Line].Who == 3 ? 1.25f : 1.f, 1.f, 0.f, VoiceAttenuation);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_LINE %d npc=%s id=%s sound=%d playing=%d seconds=%.2f"), Line, FortLines[Line].Npc, FortLines[Line].Id, Sound != nullptr ? 1 : 0,
        FortVoiceAudio && FortVoiceAudio->IsPlaying() ? 1 : 0, FortSeconds(Line));
    // (the fiend's mace down on the fighter as he calls out; it throws at the ranger as she answers)
    if (FortLines[Line].Who == 0) if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->ForceFortress(0);
    if (FortLines[Line].Who == 2) if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->ForceFortress(2);
}

void ATempleSanctumBattle::TickFortressLines(float Dt)
{
    // The line playing: its speaker's jaw with its loudness; done after its length.
    if (FortLine < 0 || FortLine >= UE_ARRAY_COUNT(FortLines)) return;
    FortTalkT += Dt;
    const float Len = FortSeconds(FortLine);
    const int32 Who = FortLines[FortLine].Who;
    const float Loud = FortLoudness(FortLine, FortTalkT);
    if (Who < 3) { if (ATempleHero* H = GetHero(Who)) H->SetSpeech(FortTalkT < Len ? Loud * 9.f : 0.f); }
    else if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->SetFortSpeech(FortTalkT < Len ? Loud * 26.f : -1.f);
    if (FortTalkT > Len + .1f && FortTalkT < 1e5f)
    {
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_LINE_END %d"), FortLine);
        if (Who < 3) { if (ATempleHero* H = GetHero(Who)) H->SetSpeech(0.f); }
        else if (ATempleSkeleton* Fd = TheFiend.Get()) Fd->SetFortSpeech(-1.f);
        // (between the entrance's lines it waits for the next; after its last, or a skip, it is done)
        if (FortLine >= FortEntranceLines - 1 || bFortSkipping || bFortReleased) FortLine = 99;
        FortVoiceAudio = nullptr; FortTalkT = 1e6f;   // (done: no subtitle, waiting for the next)
    }
}

double ATempleSanctumBattle::GetEncounterTime() const
{
    // The encounter's track's place while it plays (its events are timed to it); else the entrance's own clock.
    double M = 0;
    if (ADockGameMode::GetEncounterMusicTime(GetWorld(), M)) return M;
    return FortClock;
}

bool PhlegethosBattleMusic(const UWorld* World, const FVector& At, bool bWas)
{
    // From the moment the trio's entrance starts, while he is at the forecourt (a little further back before it leaves him).
    const ATempleSanctumBattle* B = ATempleSanctumBattle::GetFortress(World);
    if (!B || !(B->IsFortressScenePlaying() || B->HasFortressSceneRun()) || !InPhlegethos(At)) return false;
    return PhlegLocal(At).X > Phleg::ForecourtX - (bWas ? 2600.f : 1500.f);
}
