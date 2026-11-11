#include "ShipCrew.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ShipSetting.h"
#include "ShipStorm.h"
#include "ProceduralMeshComponent.h"
#include "ShipRope.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "ShipMusicMap.inl"

using namespace Ship;

namespace
{
    double GScoreStart = -1.0;   // audio time the playing track began
    float GScoreOffset = 0.f;
    ShipScore::ETrack GTrack = ShipScore::ETrack::Ship;
    float GLastBeatStep = 0.f;   // how far (in beats) the count moved at the last change of track
    double GBeatShift = 0.0;     // whole beats added at a change of track, so the crew's count runs on (no phrase cut short)
    template <int32 N> float MapThrough(const float (&K)[N][2], float S)
    {
        if (S <= K[0][0]) return K[0][1];
        for (int32 I = 1; I < N; ++I)
            if (S < K[I][0]) return FMath::Lerp(K[I - 1][1], K[I][1], (S - K[I - 1][0]) / FMath::Max(K[I][0] - K[I - 1][0], 1e-3f));
        return K[N - 1][1];
    }
    float Ease01(float X) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f)); }
    // Where the crew stand (the ship frame). The deck's performers amidships by the main hatch's grating, where he comes up
    // the companion: the dancer on the planking just forward of the grating, clear of the capstan (user 2026-11-02: he
    // danced on the grating), the concertina player and the cheering pirate either side of him; Jeffries with his back to the main mast's aft side, between its fife rails (left open for him
    // in milestone 2); the fencers' ground forward of the fore mast, over the fore grating, clear of the rails and the stores.
    const FVector2D DancerAt(215, 92), ConcertinaAt(215, -165), CheeringAt(100, 215);
    constexpr float JeffriesBack = 14.f;              // cm from the mast's face to his heels' line
    // The fencers: their middle stays in this box, each a half-gap either side of it along the line between them.
    constexpr float FenceX0 = 600, FenceX1 = 1150, FenceY = 170, FenceHalf = 75;
    // The seated pirate on the mess table's stool that faces the door from the arrival compartment (ShipSetting.cpp's table
    // at -340, -175: five stools 130 cm round it from 20 degrees, every 72; this is the third), the table 76 high, the stools 41.
    constexpr float TableX = -340, TableY = -175, StoolR = 130, StoolA = 164, StoolTop = 41, TableTop = 76;
    // The chef at the galley's work table (560..800 x -75..75, top 86 high), on its port side facing across it.
    const FVector2D ChefAt(680, -108);
    // The captain (user 2026-11-02: on deck from the start; the 2D game brings him on only at the trigger): on the quarterdeck
    // just forward of the skylight, looking forward over the waist at his crew (the way across the quarterdeck passes before him).
    const FVector2D CaptainAt(-1065, -45);
    constexpr float ChefYaw = 90;
}

void ShipScore::Started(const UWorld* World, ETrack Track, float OffsetSeconds)
{
    const bool bWas = World && IsPlaying();
    const float Before = bWas ? Beat(World) : 0.f;
    GScoreStart = World ? World->GetAudioTimeSeconds() : 0.0;
    GScoreOffset = OffsetSeconds;
    GTrack = Track;
    GBeatShift = 0.0;
    // Taking over from the other track: the count goes on from where it was, only the part of a beat following the music.
    if (bWas) { GBeatShift = FMath::RoundToDouble(Before - Beat(World)); GLastBeatStep = Beat(World) - Before; }
}

float ShipScore::LastBeatStep() { return GLastBeatStep; }

bool ShipScore::IsPlaying() { return GScoreStart >= 0.0; }
ShipScore::ETrack ShipScore::Current() { return GTrack; }
float ShipScore::Duration(ETrack Track) { return Track == ETrack::Ship ? ShipMusicMap::ShipDuration : ShipMusicMap::ConcertinaDuration; }

float ShipScore::Position(const UWorld* World)
{
    if (!World || GScoreStart < 0.0) return 0.f;
    return FMath::Fmod(static_cast<float>(World->GetAudioTimeSeconds() - GScoreStart) + GScoreOffset, Duration(GTrack));
}

float ShipScore::Matching(ETrack From, float Seconds)
{
    const float S = FMath::Clamp(Seconds, 0.f, Duration(From));
    const float To = From == ETrack::Ship ? MapThrough(ShipMusicMap::ShipToConcertina, S) : MapThrough(ShipMusicMap::ConcertinaToShip, S);
    return FMath::Clamp(To, 0.f, Duration(From == ETrack::Ship ? ETrack::Concertina : ETrack::Ship) - .05f);
}

float ShipScore::Beat(const UWorld* World)
{
    const bool bShip = GTrack == ETrack::Ship;
    const float Period = 60.f / (bShip ? ShipMusicMap::ShipBPM : ShipMusicMap::ConcertinaBPM);
    if (!World) return 0.f;
    if (GScoreStart < 0.0) return World->GetTimeSeconds() / Period;
    // Counted on through the track's loops (a whole number of beats a loop), so a phrase isn't cut short where it loops.
    const double Length = Duration(GTrack), Elapsed = World->GetAudioTimeSeconds() - GScoreStart + GScoreOffset;
    const double Loops = FMath::FloorToDouble(Elapsed / Length), In = Elapsed - Loops * Length;
    return static_cast<float>((In - (bShip ? ShipMusicMap::ShipFirstBeat : ShipMusicMap::ConcertinaFirstBeat)) / Period
        + Loops * FMath::RoundToDouble(Length / Period) + GBeatShift);
}

FVector ShipScore::ConcertinaSource() { return ShipWorld(FVector(ConcertinaAt.X, ConcertinaAt.Y, DeckZ + 110.f)); }

AShipCrew::AShipCrew()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;   // before the crew's own ticks: they pose to this frame's cue
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AShipCrew* AShipCrew::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AShipCrew> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipCrew* AShipCrew::Spawn(UWorld* World)
{
    if (!World) return nullptr;
    if (AShipCrew* Existing = Get(World)) return Existing;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Crew = World->SpawnActor<AShipCrew>(ShipOrigin(), FRotator::ZeroRotator, P);
    if (Crew) Crew->PlaceCrew();
    return Crew;
}

ADockNPC* AShipCrew::Find(EDockHuman Kind) const
{
    for (ADockNPC* N : Crew) if (N && N->GetKind() == Kind) return N;
    return nullptr;
}

int32 AShipCrew::GetCount() const
{
    int32 N = 0;
    for (const ADockNPC* C : Crew) N += C != nullptr;
    return N;
}

void AShipCrew::PlaceCrew()
{
    UWorld* World = GetWorld();
    const auto Deck = [](const FVector2D& At) { return ShipWorld(FVector(At.X, At.Y, DeckZ)); };
    const auto Face = [](const FVector2D& From, const FVector2D& To) { return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X))); };
    const auto Add = [&](EDockHuman Kind, const FVector& Feet, float Yaw)
    {
        ADockNPC* N = ADockNPC::SpawnHuman(World, Kind, Feet, Yaw);
        if (N) { Crew.Add(N); N->AddTickPrerequisiteActor(this); }   // they pose to this frame's beat and cue
        return N;
    };
    // The deck's performers, each turned toward the dancer in the middle (the dancer toward the companion he comes up).
    if (ADockNPC* N = Add(EDockHuman::PirateDancer, Deck(DancerAt), Face(DancerAt, FVector2D(-250, 130))))
        N->SetPirateVoice(TEXT("PirateDancer"), { TEXT("DAN_01") }, { TEXT("DAN_02") }, { TEXT("DAN_ARR") });
    if (ADockNPC* N = Add(EDockHuman::PirateConcertina, Deck(ConcertinaAt), Face(ConcertinaAt, DancerAt)))
        N->SetPirateVoice(TEXT("PirateConcertina"), { TEXT("CON_01") }, { TEXT("CON_02") }, { TEXT("CON_ARR") });
    if (ADockNPC* N = Add(EDockHuman::PirateCheering, Deck(CheeringAt), Face(CheeringAt, DancerAt)))
        N->SetPirateVoice(TEXT("PirateCheering"), { TEXT("CHE_01") }, { TEXT("CHE_02"), TEXT("CHE_03") }, { TEXT("CHE_ARR") });
    // Jeffries, his back to the main mast, facing aft.
    if (ADockNPC* N = Add(EDockHuman::Jeffries, Deck(FVector2D(MainMastX - MastR - JeffriesBack, 0)), 180.f))
    {
        N->SetPirateVoice(TEXT("Jeffries"), { TEXT("JEF_P01"), TEXT("JEF_P02") }, { TEXT("JEF_P03"), TEXT("JEF_P04") });   // (no "Arrgh!": he goes on)
        N->TieToMast(ShipWorld(FVector(MainMastX, 0, DeckZ)), MastR);
    }
    // The captain (the 2D game gives him no lines before the trigger; the user's pre-storm takes, 2026-11-11).
    ADockNPC* Captain = Add(EDockHuman::PirateCaptain, ShipWorld(FVector(CaptainAt.X, CaptainAt.Y, QuarterZ)), 0.f);
    if (Captain) Captain->SetPirateVoice(TEXT("PirateCaptain"), { TEXT("CAP_P00") }, { TEXT("CAP_P01") }, { TEXT("CAP_P02") });
    // The fencers (no lines: busy).
    Pair = PairStart = FVector2D((FenceX0 + FenceX1) * .5f, 0);
    AxisYaw = AxisStart = 0.f;
    ADockNPC* FencerA = Add(EDockHuman::PirateFencerA, Deck(Pair - FVector2D(FenceHalf, 0)), 0.f);
    // The halfling's fist, sized to his hand against a grown man's (the fencers' cutlass sits right in theirs): 2026-11-02.
    if (Captain && FencerA && FencerA->GetHandLength() > 1.f)
    {
        Captain->SetFistScale(Captain->GetHandLength() / FencerA->GetHandLength());
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CAPTAIN_FIST hand_cm=%.1f man_hand_cm=%.1f fist_scale=%.2f"), Captain->GetHandLength(), FencerA->GetHandLength(), Captain->GetFistScale());
    }
    Add(EDockHuman::PirateFencerB, Deck(Pair + FVector2D(FenceHalf, 0)), 180.f);
    // Below decks: the seated pirate at the mess table, the chef at his work table.
    {
        const float A = FMath::DegreesToRadians(StoolA);
        const FVector2D Stool(TableX + StoolR * FMath::Cos(A), TableY + StoolR * FMath::Sin(A));
        if (ADockNPC* N = Add(EDockHuman::PirateSeated, ShipWorld(FVector(Stool.X, Stool.Y, LowerZ)), 0.f))
        {
            N->SitAtTable(ShipWorld(FVector(Stool.X, Stool.Y, LowerZ)), Face(Stool, FVector2D(TableX, TableY)), StoolTop, TableTop, ShipWorld(FVector(ArrivalAft, 0, LowerZ + 150)));
            N->SetPirateVoice(TEXT("PirateSeated"), { TEXT("SEA_01") }, { TEXT("SEA_02") }, { TEXT("SEA_ARR") });
        }
    }
    if (ADockNPC* N = Add(EDockHuman::PirateChef, ShipWorld(FVector(ChefAt.X, ChefAt.Y, LowerZ)), ChefYaw))
    {
        const float Half = ShipInnerHalf(800, LowerZ + 60) - 30.f;
        const FVector2D O(ShipOrigin().X, ShipOrigin().Y);
        N->SetupChef(ShipWorld(FVector(ChefAt.X, ChefAt.Y, LowerZ)), ChefYaw, FBox2D(O + FVector2D(ArrivalFore + 30, -Half), O + FVector2D(GalleyFore - 30, Half)), ShipOrigin().Z + LowerZ);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CREW placed=%d"), GetCount());
}

void AShipCrew::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Beat = ShipScore::Beat(GetWorld());
    for (ADockNPC* N : Crew) if (N) N->SetBeat(Beat);
    // In the storm (2026-11-04) nobody fences; the crew work as hard as the storm blows.
    if (bStorm)
    {
        const AShipStorm* Storm = AShipStorm::Get(GetWorld());
        const float Force = Storm ? FMath::Max(Storm->GetWind(), .3f) + Storm->GetIntense() : 1.f;
        for (ADockNPC* N : Crew) if (N) N->SetStormForce(Force);
        UpdateStormLines(DeltaSeconds);
    }
    else TickFencing(DeltaSeconds);
    // Nobody through the hull: each body (a 24 cm capsule) inside the planking at his hips' height.
    for (const ADockNPC* N : Crew)
        if (N)
        {
            const FVector L = ShipLocal(N->GetActorLocation());
            const float Half = ShipInnerHalf(static_cast<float>(L.X), static_cast<float>(L.Z) - 10.f);
            if (Half > 0.f) WorstHullOut = FMath::Max(WorstHullOut, FMath::Abs(static_cast<float>(L.Y)) + 24.f - Half);
        }
}

FVector AShipCrew::GetFencePair() const { return ShipWorld(FVector(Pair.X, Pair.Y, DeckZ)); }

namespace
{
    // The fencers' blows (user 2026-11-03: "more intense and complex while being smooth realistic good looking movements").
    // Each phrase is a run of blows, one fencer attacking and the other parrying, the parry turning into the riposte:
    //   Head: a cut down at his head from over the shoulder, parried overhead;
    //   Flank: a cut from his right at the other's flank, parried with the blade upright on that side;
    //   Low: a backhand cut low at the other's leg, parried with the point down;
    //   Lunge: a lunging thrust at the chest, parried to the outside;
    //   Feint: a half thrust to the inside the other goes to parry, the point dipped under his blade and thrust home on the
    //     outside, parried at the last moment;
    //   BeatEvade: a sharp beat that knocks the other's blade aside and a lunge into the opening, which he escapes by leaping back;
    //   Bind: blades crossed hilt to hilt, the two close and shoving, then springing apart.
    enum class EAttack : uint8 { Head, Flank, Low, Lunge, Feint, BeatEvade, Bind, Count };
    // Beats from the attacker's first move to the blow, from the blow until both are back on guard, and how often each comes.
    constexpr float AttackLead[] = { .85f, .7f, .7f, .6f, 1.1f, .95f, .7f };
    constexpr float AttackTail[] = { .8f, .75f, .8f, 1.f, 1.f, 1.1f, 2.4f };
    constexpr float AttackWeight[] = { 3.f, 3.f, 2.f, 3.f, 2.f, 1.5f, 1.2f };
    // Ground the attacker gains at each blow (cm), and the last beat a blow's recovery may run to (the phrase ends on guard).
    constexpr float AttackGain[] = { 10.f, 10.f, 10.f, 14.f, 14.f, 20.f, 0.f };
    constexpr float PhraseEnd = 7.6f;
    // How a sword position turns his shoulders (+1 the sword shoulder driven forward, -1 drawn back) and bends his knees.
    float PoseTwist(EFencePose P)
    {
        switch (P)
        {
        case EFencePose::ChamberHigh: return -.5f;  case EFencePose::SwingHigh: return -.1f;  case EFencePose::CutHead: return .7f;
        case EFencePose::ChamberFlank: return -.8f; case EFencePose::CutFlank: return .8f;    case EFencePose::ChamberLow: return -.3f;
        case EFencePose::CutLow: return .5f;        case EFencePose::Thrust: case EFencePose::Lunge: return .9f;
        case EFencePose::Feint: return .4f;         case EFencePose::Disengage: return .3f;   case EFencePose::ParryHigh: return .1f;
        case EFencePose::ParryIn: return .45f;      case EFencePose::ParryOut: return -.3f;   case EFencePose::ParryLow: return -.2f;
        case EFencePose::Knocked: return -.6f;      case EFencePose::BeatAside: return .5f;   case EFencePose::BindHold: return .3f;
        case EFencePose::BindPress: return .4f;     default: return 0.f;
        }
    }
    float PoseCrouch(EFencePose P)
    {
        switch (P)
        {
        case EFencePose::CutHead: return .1f;   case EFencePose::ChamberFlank: return .1f; case EFencePose::CutFlank: return .15f;
        case EFencePose::ChamberLow: return .3f; case EFencePose::CutLow: return .55f;     case EFencePose::Disengage: return .1f;
        case EFencePose::ParryHigh: return .3f; case EFencePose::ParryIn: return .1f;      case EFencePose::ParryOut: return .1f;
        case EFencePose::ParryLow: return .6f;  case EFencePose::BindHold: case EFencePose::BindPress: return .1f;
        default: return 0.f;
        }
    }
    FFenceKey FenceKey(float Beat, EFencePose Pose, bool bFlow = false, float Lunge = 0.f, float Off = 0.f, float Lean = 0.f)
    {
        FFenceKey K; K.Beat = Beat; K.Pose = Pose; K.bFlow = bFlow; K.Lunge = Lunge; K.Off = Off; K.Lean = Lean;
        return K;
    }
    // The keys round beat Now: Out[1] the last at or before it, Out[2] the next, Out[0] and Out[3] their neighbours.
    void KeysAround(const TArray<FFenceKey>& K, float Now, FFenceKey Out[4])
    {
        int32 I = 0;
        while (I + 2 < K.Num() && K[I + 1].Beat <= Now) ++I;
        Out[0] = K[FMath::Max(I - 1, 0)]; Out[1] = K[I]; Out[2] = K[FMath::Min(I + 1, K.Num() - 1)]; Out[3] = K[FMath::Min(I + 2, K.Num() - 1)];
    }
    struct FFenceBody { float Lunge, Off, Lean, Twist, Crouch; };
    FFenceBody BodyAt(const TArray<FFenceKey>& K, float Now)
    {
        FFenceKey A[4]; KeysAround(K, Now, A);
        float V[4];
        const auto Eval = [&](auto Get) { for (int32 J = 0; J < 4; ++J) V[J] = Get(A[J]); return FenceCurve(A, V, Now); };
        return { FMath::Clamp(Eval([](const FFenceKey& X) { return X.Lunge; }), 0.f, 1.f), Eval([](const FFenceKey& X) { return X.Off; }),
            FMath::Clamp(Eval([](const FFenceKey& X) { return X.Lean; }), 0.f, 1.f), Eval([](const FFenceKey& X) { return PoseTwist(X.Pose); }),
            FMath::Clamp(Eval([](const FFenceKey& X) { return PoseCrouch(X.Pose); }), 0.f, 1.f) };
    }
    FVector2D AxisOf(float Yaw) { return FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw))); }
    // How far a fencer standing at P would be outside the ground (0 inside).
    float OutAt(const FVector2D& P)
    {
        const float Half = FMath::Min(FenceY, ShipInnerHalf(P.X, DeckZ + 40) - 90.f);
        return FMath::Max(0.f, FMath::Max(FMath::Max(FenceX0 - static_cast<float>(P.X), static_cast<float>(P.X) - FenceX1), FMath::Abs(static_cast<float>(P.Y)) - Half));
    }
    // ... and either of a pair with this middle and line, Pad cm further apart than their stance.
    float Outside(const FVector2D& Centre, float Yaw, float Pad = 0.f)
    {
        const FVector2D Ax = AxisOf(Yaw) * (FenceHalf + Pad);
        return FMath::Max(OutAt(Centre - Ax), OutAt(Centre + Ax));
    }
}

FString AShipCrew::GetFenceTally() const
{
    return FString::Printf(TEXT("head=%d flank=%d low=%d lunge=%d feint=%d beat=%d bind=%d"), Tally[0], Tally[1], Tally[2], Tally[3], Tally[4], Tally[5], Tally[6]);
}

void AShipCrew::PlanPhrase()
{
    using P = EFencePose;
    FRandomStream R(20261103 + Phrases * 7919);
    for (TArray<FFenceKey>& L : Keys) { L.Reset(); L.Add(FenceKey(0.f, P::Guard)); }
    PairKeys.Reset(); PairKeys.Add(FenceKey(0.f, P::Guard));
    Blows.Reset();
    const float PlanYaw = AxisStart + AxisTurn;
    // Every fifth phrase they circle (the pair turning on the deck), watching each other, before they engage again.
    const bool bBreath = Phrases % 5 == 0;
    float Cur = .35f;
    if (bBreath)
    {
        for (int32 I = 0; I < 2; ++I)
        {
            const float S = I == Leader ? 1.f : -1.f;
            Keys[I].Add(FenceKey(.6f, P::Guard, true, 0.f, -14.f * S));
            Keys[I].Add(FenceKey(1.3f, P::Guard, true, 0.f, 12.f * S));
            Keys[I].Add(FenceKey(1.9f, P::Guard, false, 0.f, 0.f));
        }
        Cur = 2.2f;
    }
    const auto Merge = [](TArray<FFenceKey>& L, const TArray<FFenceKey>& Add)
    {
        // What he was going to do after this moves first is dropped (a return to guard the next blow cuts short).
        while (L.Num() > 1 && L.Last().Beat > Add[0].Beat - .2f) L.Pop();
        L.Append(Add);
    };
    float TotalWeight = 0.f;
    for (const float W : AttackWeight) TotalWeight += W;
    int32 Att = Leader, Fails = 0;
    EAttack Last = EAttack::Count;
    bool bBound = false;
    float Gained = 0.f;
    while (Fails < 12)
    {
        float Pick = R.FRand() * TotalWeight;
        int32 A = 0;
        for (; A < int32(EAttack::Count) - 1; ++A) { if (Pick < AttackWeight[A]) break; Pick -= AttackWeight[A]; }
        const EAttack Kind = EAttack(A);
        if ((Kind == EAttack::Bind && (bBound || bBreath)) || (Kind == Last && R.FRand() < .75f)) { ++Fails; continue; }
        const float S = Cur + AttackLead[A];
        if (S + AttackTail[A] > PhraseEnd) { ++Fails; continue; }
        TArray<FFenceKey> Ak, Dk;
        float Contact = S;
        bool bContact = true;
        switch (Kind)
        {
        case EAttack::Head:
            Ak = { FenceKey(S - .85f, P::ChamberHigh, true, .1f, 4.f), FenceKey(S - .38f, P::SwingHigh, true, .3f, 8.f), FenceKey(S, P::CutHead, false, .55f, 12.f),
                FenceKey(S + .18f, P::CutHead, false, .55f, 12.f), FenceKey(S + .8f, P::Guard) };
            Dk = { FenceKey(S - .32f, P::ParryHigh, false, 0.f, -6.f, .1f), FenceKey(S + .2f, P::ParryHigh, false, 0.f, -8.f, .1f), FenceKey(S + .8f, P::Guard) };
            break;
        case EAttack::Flank:
            Ak = { FenceKey(S - .7f, P::ChamberFlank, true, .1f, 4.f), FenceKey(S, P::CutFlank, false, .5f, 12.f), FenceKey(S + .18f, P::CutFlank, false, .5f, 12.f), FenceKey(S + .75f, P::Guard) };
            Dk = { FenceKey(S - .28f, P::ParryIn, false, 0.f, -6.f), FenceKey(S + .2f, P::ParryIn, false, 0.f, -6.f), FenceKey(S + .75f, P::Guard) };
            break;
        case EAttack::Low:
            Ak = { FenceKey(S - .7f, P::ChamberLow, true, .15f, 4.f), FenceKey(S, P::CutLow, false, .75f, 12.f), FenceKey(S + .18f, P::CutLow, false, .75f, 12.f), FenceKey(S + .8f, P::Guard) };
            Dk = { FenceKey(S - .28f, P::ParryLow, false, 0.f, -6.f), FenceKey(S + .2f, P::ParryLow, false, 0.f, -6.f), FenceKey(S + .8f, P::Guard) };
            break;
        case EAttack::Lunge:
            Ak = { FenceKey(S - .6f, P::Guard, true, 0.f, 10.f), FenceKey(S, P::Thrust, false, 1.f, 12.f), FenceKey(S + .2f, P::Thrust, false, 1.f, 12.f), FenceKey(S + 1.f, P::Guard) };
            Dk = { FenceKey(S - .25f, P::ParryOut, false, 0.f, -10.f), FenceKey(S + .22f, P::ParryOut, false, 0.f, -10.f), FenceKey(S + .9f, P::Guard) };
            break;
        case EAttack::Feint:
            Ak = { FenceKey(S - 1.1f, P::Guard, true, 0.f, 6.f), FenceKey(S - .75f, P::Feint, false, .3f, 10.f), FenceKey(S - .4f, P::Disengage, true, .35f, 10.f),
                FenceKey(S, P::Thrust, false, 1.f, 12.f), FenceKey(S + .2f, P::Thrust, false, 1.f, 12.f), FenceKey(S + 1.f, P::Guard) };
            Dk = { FenceKey(S - .55f, P::ParryIn, false, 0.f, -4.f), FenceKey(S - .1f, P::ParryOut, false, 0.f, -10.f), FenceKey(S + .22f, P::ParryOut, false, 0.f, -10.f), FenceKey(S + .9f, P::Guard) };
            break;
        case EAttack::BeatEvade:
            Ak = { FenceKey(S - .95f, P::Guard, true, 0.f, 4.f), FenceKey(S - .6f, P::BeatAside, false, .2f, 10.f), FenceKey(S, P::Lunge, false, 1.f, 14.f),
                FenceKey(S + .22f, P::Lunge, false, 1.f, 14.f), FenceKey(S + 1.1f, P::Guard) };
            Dk = { FenceKey(S - .5f, P::Knocked, false, 0.f, -2.f), FenceKey(S - .05f, P::Guard, false, 0.f, -34.f, .75f), FenceKey(S + .3f, P::Guard, false, 0.f, -34.f, .6f), FenceKey(S + 1.f, P::Guard) };
            Contact = S - .6f;   // the beat meets his blade; the lunge meets nothing
            break;
        default:   // the bind
            Ak = { FenceKey(S - .7f, P::SwingHigh, true, .1f, 12.f), FenceKey(S, P::BindPress, false, .25f, 22.f), FenceKey(S + .7f, P::BindPress, false, .2f, 28.f),
                FenceKey(S + 1.4f, P::BindPress, false, .15f, 22.f), FenceKey(S + 1.9f, P::Guard, true, 0.f, -14.f, .2f), FenceKey(S + 2.4f, P::Guard) };
            Dk = { FenceKey(S - .25f, P::BindHold, false, 0.f, 18.f), FenceKey(S + .7f, P::BindHold, false, 0.f, 12.f, .15f), FenceKey(S + 1.4f, P::BindHold, false, 0.f, 18.f),
                FenceKey(S + 1.9f, P::Guard, true, 0.f, -14.f, .2f), FenceKey(S + 2.4f, P::Guard) };
            break;
        }
        // The attacker steps in to land his blow on the other's blade near its hilt; the other gives less ground for it.
        for (FFenceKey& X : Ak) if (X.Pose == P::CutHead || X.Pose == P::CutFlank || X.Pose == P::CutLow || X.Pose == P::Thrust) X.Off += X.Pose == P::CutHead ? 14.f : 8.f;
        for (FFenceKey& X : Dk) if (X.Pose == P::ParryHigh || X.Pose == P::ParryIn || X.Pose == P::ParryOut || X.Pose == P::ParryLow) X.Off += 4.f;
        Merge(Keys[Att], Ak);
        Merge(Keys[1 - Att], Dk);
        FBlow Blow; Blow.Strike = Contact; Blow.bContact = bContact; Blow.Kind = uint8(A);
        // The blade heard cutting the air on its way in: a cut's swing, a thrust's lunge (the beat's lunge goes past him).
        Blow.Swish = Kind == EAttack::BeatEvade ? S - .2f : S - .3f;
        Blows.Add(Blow);
        // The attacker gains ground (fencer A along the line, B against it), if the pair stays on theirs.
        float Gain = AttackGain[A] * (Att == 0 ? 1.f : -1.f);
        if (Outside(PairStart + AxisOf(PlanYaw) * (Gained + Gain), PlanYaw, 40.f) > 0.f) Gain = 0.f;
        if (Gain != 0.f)
        {
            PairKeys.Add(FenceKey(S - .4f, P::Guard, false, 0.f, Gained));
            Gained += Gain;
            PairKeys.Add(FenceKey(S + .3f, P::Guard, false, 0.f, Gained));
        }
        ++Tally[A];
        Last = Kind; bBound |= Kind == EAttack::Bind;
        Cur = S + (Kind == EAttack::Bind ? AttackTail[A] : .45f + (R.FRand() < .4f ? .35f : 0.f));
        // Mostly the one who parried ripostes; now and then the attacker renews his attack.
        if (R.FRand() < .72f) Att = 1 - Att;
    }
    for (TArray<FFenceKey>& L : Keys) if (L.Last().Beat < 7.95f) L.Add(FenceKey(8.f, P::Guard));
    PairKeys.Add(FenceKey(8.f, P::Guard, false, 0.f, Gained));
}

void AShipCrew::TickFencing(float DeltaSeconds)
{
    ADockNPC* F[2] = { Find(EDockHuman::PirateFencerA), Find(EDockHuman::PirateFencerB) };
    if (!F[0] || !F[1]) return;
    const int32 P = FMath::FloorToInt(Beat / 8.f);
    if (P != Phrase || PairKeys.Num() == 0)   // (2026-11-08: a beat before the score's first, -1 like the unset phrase, left them unplanned)
    {
        Phrase = P; ++Phrases;
        PairStart = Pair; AxisStart = AxisYaw;
        // They take turns to open; whoever's advance keeps them on their ground.
        Leader = Phrases & 1;
        if (Outside(PairStart + AxisOf(AxisStart) * (Leader == 0 ? 30.f : -30.f), AxisStart, 40.f) > 0.f) Leader = 1 - Leader;
        AxisTurn = 0.f;
        if (Phrases % 5 == 0)   // circling: the pair turns on the deck
            for (const float T : { 30.f, -30.f, 15.f, -15.f })
                if (Outside(PairStart, AxisStart + T * (Phrases % 8 ? 1.f : -1.f), 45.f) <= 0.f) { AxisTurn = T * (Phrases % 8 ? 1.f : -1.f); break; }
        PlanPhrase();
    }
    const float U = Beat - P * 8.f;
    AxisYaw = AxisStart + AxisTurn * Ease01((U - .3f) / 1.7f);
    const FVector2D Ax = AxisOf(AxisYaw);
    {
        FFenceKey K[4]; KeysAround(PairKeys, U, K);
        float V[4]; for (int32 J = 0; J < 4; ++J) V[J] = K[J].Off;
        Pair = PairStart + Ax * FenceCurve(K, V, U);
    }
    for (int32 I = 0; I < 2; ++I)
    {
        const FFenceBody Bd = BodyAt(Keys[I], U);
        const float Side = I == 0 ? -1.f : 1.f;
        const FVector2D C = Pair + Ax * (Side * (FenceHalf - Bd.Off));
        WorstFenceOut = FMath::Max(WorstFenceOut, OutAt(C));
        // His feet lead the way he is going: the front foot first going forward, the back foot first going back.
        const FVector2D Facing = Ax * -Side;
        const float Speed = DeltaSeconds > 0.f && !FencerAt[I].IsZero() ? static_cast<float>(FVector2D::DotProduct(C - FencerAt[I], Facing)) / DeltaSeconds : 0.f;
        FencerAt[I] = C;
        StrideNow[I] = FMath::FInterpTo(StrideNow[I], FMath::Clamp(Speed * .18f, -10.f, 14.f), DeltaSeconds, 12.f);
        FFenceCue Cue;
        Cue.Centre = ShipWorld(FVector(C.X, C.Y, DeckZ));
        Cue.Facing = FVector(Facing.X, Facing.Y, 0.f);
        Cue.Target = F[1 - I]->GetActorLocation() + FVector(0, 0, 38.f);
        Cue.Head = F[1 - I]->GetActorLocation() + FVector(0, 0, 72.f);
        if (!F[1 - I]->GetBlade(Cue.PartnerBase, Cue.PartnerTip)) Cue.PartnerBase = Cue.PartnerTip = FVector::ZeroVector;
        KeysAround(Keys[I], U, Cue.Key);
        Cue.Now = U;
        Cue.Lunge = Bd.Lunge; Cue.Lean = Bd.Lean; Cue.Twist = Bd.Twist; Cue.Crouch = Bd.Crouch;
        Cue.Stride = StrideNow[I];
        F[I]->SetFenceCue(Cue);
    }
    // The fight heard (2026-11-03): each blow's blade through the air, the steel meeting where the blades cross, and in a
    // bind the hilts locking and the blades scraping as they shove. Recorded (Tools/make_ship_sounds.py); spatial, carrying
    // across the deck; quieter below (ShipSound).
    {
        FVector B0(0.f), T0(0.f), B1(0.f), T1(0.f);
        const bool bBlades = F[0]->GetBlade(B0, T0) && F[1]->GetBlade(B1, T1);
        const FVector Mid = (F[0]->GetActorLocation() + F[1]->GetActorLocation()) * .5f + FVector(0, 0, 60.f);
        auto Crossing = [&]()
        {
            if (!bBlades) return Mid;
            FVector Pa, Pb; FMath::SegmentDistToSegmentSafe(B0, T0, B1, T1, Pa, Pb);
            return (Pa + Pb) * .5f;
        };
        static constexpr float ClashVolume[] = { 1.f, .9f, .8f, .7f, .7f, 1.f, .9f };   // head, flank, low, lunge, feint, beat, bind
        for (FBlow& Bl : Blows)
        {
            auto Due = [&](uint8 Bit, float At) { if ((Bl.Heard & Bit) || U < At || U > At + .5f) return false; Bl.Heard |= Bit; return true; };
            const bool bBind = Bl.Kind == uint8(EAttack::Bind);
            const bool bThrust = Bl.Kind == uint8(EAttack::Lunge) || Bl.Kind == uint8(EAttack::Feint) || Bl.Kind == uint8(EAttack::BeatEvade);
            if (Due(1, Bl.Swish)) ShipSound(this, TEXT("SFX_SwordSwish"), 1, Mid, bThrust ? .3f : .45f);
            if (Bl.bContact && Due(2, Bl.Strike))
            {
                ShipSound(this, bBind ? TEXT("SFX_SwordBind") : TEXT("SFX_SwordClash"), bBind ? 2 : 10, Crossing(), ClashVolume[FMath::Min<int32>(Bl.Kind, 6)], true);
                ++Clashes;
            }
            if (bBind && Due(4, Bl.Strike + .25f)) ShipSound(this, TEXT("SFX_SwordScrape"), 1, Crossing(), .55f, true);
            if (bBind && Due(8, Bl.Strike + 1.5f)) ShipSound(this, TEXT("SFX_SwordScrape"), 1, Crossing(), .45f, true);   // springing apart
        }
    }
    // How close their blades came at each blow that should meet the other's blade (they should touch).
    {
        FVector B0(0.f), T0(0.f), B1(0.f), T1(0.f);
        const bool bBlades = F[0]->GetBlade(B0, T0) && F[1]->GetBlade(B1, T1);
        for (FBlow& Bl : Blows)
        {
            if (Bl.bDone || !Bl.bContact) continue;
            if (U > Bl.Strike + .2f)
            {
                Bl.bDone = true;
                if (Bl.Gap > 999.f) continue;   // never seen at a steady frame rate
                LastParryGap = Bl.Gap;
                static const TCHAR* KindNames[] = { TEXT("head"), TEXT("flank"), TEXT("low"), TEXT("lunge"), TEXT("feint"), TEXT("beat"), TEXT("bind") };
                UE_LOG(LogTemp, Display, TEXT("CHUCK_FENCE_BLOW kind=%s gap=%.1f beat=%.2f"), KindNames[FMath::Min<int32>(Bl.Kind, 6)], Bl.Gap, Beat);
                if (Phrases > 2) { WorstContactGap = FMath::Max(WorstContactGap, Bl.Gap); ++Contacts; Touching += Bl.Gap < 4.f; }
            }
            else if (bBlades && DeltaSeconds < .05f && U >= Bl.Strike - .03f)
            {
                FVector Pa, Pb;
                FMath::SegmentDistToSegmentSafe(B0, T0, B1, T1, Pa, Pb);
                Bl.Gap = FMath::Min(Bl.Gap, static_cast<float>(FVector::Dist(Pa, Pb)));
            }
        }
    }
    // Their blades (over his head as they fence, but low in a lunge), and their feet, cost Chuck Sanity where they meet him
    // (the 2D swordsmen: 15 of 100; one of his five).
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || Chuck->IsAstral()) return;
    const UCapsuleComponent* Cap = Chuck->GetCapsuleComponent();
    const float R = Cap->GetScaledCapsuleRadius(), H = Cap->GetScaledCapsuleHalfHeight();
    const FVector C = Chuck->GetActorLocation();
    for (ADockNPC* N : F)
    {
        FVector Base, Tip, Pa, Pb;
        bool bHit = false;
        if (N->GetBlade(Base, Tip))
        {
            FMath::SegmentDistToSegmentSafe(Base, Tip, C - FVector(0, 0, H - R), C + FVector(0, 0, H - R), Pa, Pb);
            bHit = FVector::Dist(Pa, Pb) < R + 2.f;
        }
        // His feet and legs: the rat in his stance, or in the way of an advance, is trodden on.
        FVector Back, Front;
        if (N->GetFeet(Back, Front))
        {
            FMath::SegmentDistToSegmentSafe(Back, Front, C - FVector(0, 0, H - R), C + FVector(0, 0, H - R), Pa, Pb);
            bHit |= FVector::Dist2D(Pa, Pb) < R + 10.f && FMath::Abs(Pa.Z - C.Z) < H + 20.f;
        }
        const FVector Body = N->GetActorLocation();
        bHit |= FVector::Dist2D(Body, C) < R + 27.f && FMath::Abs(Body.Z - C.Z) < 110.f;   // against his body
        if (bHit && Chuck->TakeBite(bHit ? Tip : Body, 1)) { ++FencerHits; UE_LOG(LogTemp, Display, TEXT("CHUCK_FENCER_HIT sanity=%d"), Chuck->GetSanity()); }
    }
}

// ---------------------------------------------------------------------------------------------------------------- the storm
// (2026-11-04, user: "during storm have the NPC's responding to the storm, doing things sailors would in a storm, the captain
// at the helm. no concertina held by the concertina player as he is now manning the ship as with the other crewmen".)
// The lines they work (ship frame): each hauling part comes down from its block aloft to a lead block at the deck, round its
// sheave and up into the haulers' hands; behind the last hand it hangs slack to the deck and runs aft along it into a coil.
// The dancer sweats up a halyard: down from its block aloft through his hands to its belaying pin in the main mast's starboard
// fife rail, its tail hanging off the pin to a coil at his feet. (2026-11-07: rebuilt each frame through the hands holding
// them, ShipRope.h, where they had been straight sticks with sharp corners.)
namespace
{
    constexpr float LineR = 1.5f;   // a 3 cm line
    struct FHaulLine { FVector Aloft, Block, Hands0, Hands1, Tail; FVector Side; };
    const FHaulLine MainHaul{ {-478, -42, DeckZ + 940}, {-430, -230, DeckZ + 16}, {-395, -230, DeckZ + 98}, {-150, -230, DeckZ + 92}, {-70, -230, DeckZ}, {0, 1, 0} };
    const FHaulLine ForeHaul{ {442, 40, DeckZ + 880}, {335, 235, DeckZ + 16}, {370, 235, DeckZ + 98}, {650, 235, DeckZ + 92}, {730, 235, DeckZ}, {0, -1, 0} };
    // (On the fife rail's pin at x -511, its head 94 cm up; the tail led off aft along the rail's outboard side and flaked
    // down aft of the rail, clear of the dancer's feet.)
    const FVector HalyardPin(-511, 75, DeckZ + 94), HalyardAloft(-511, 72, DeckZ + 1000);
    const FVector HalyardWay = FVector(-1, .15f, 0).GetSafeNormal();
    constexpr float SheaveR = 5.5f, FistBend = 3.f, PinBend = 2.5f;
    // The lead block's sheave: its centre 30 cm off the deck, turning in the plane of the line aloft and the hauling part.
    FVector SheaveAt(const FHaulLine& L) { return L.Block + FVector(0, 0, 14); }
    FVector SheaveAxle(const FHaulLine& L) { const FVector C = SheaveAt(L); return FVector::CrossProduct(L.Aloft - C, L.Hands0 - C).GetSafeNormal(); }
    // Which of the crew are on which line: the main's and the fore's hauling parts, then the halyard.
    const EDockHuman LineMen[3][2] = { { EDockHuman::PirateConcertina, EDockHuman::PirateCheering },
        { EDockHuman::PirateFencerA, EDockHuman::PirateFencerB }, { EDockHuman::PirateDancer, EDockHuman::Count } };
}

void AShipCrew::BeginStorm()
{
    if (bStorm) return;
    bStorm = true;
    const auto W = [](float X, float Y, float Z) { return ShipWorld(FVector(X, Y, Z)); };
    const auto Way = [&](std::initializer_list<FVector2D> Points, float Z) { TArray<FVector> Out; for (const FVector2D& P : Points) Out.Add(W(P.X, P.Y, Z)); return Out; };
    int32 Sent = 0;
    const auto Send = [&](EDockHuman Kind, EStormWork Work, const TArray<FVector>& Points, float Yaw, const FVector& A, const FVector& B, float Phase)
    {
        if (ADockNPC* N = Find(Kind)) { N->SetStormWork(Work, Points, Yaw, A, B, Phase); ++Sent; }
    };
    // The concertina player and the cheering pirate on the main's line to port, one either side of it, facing its block.
    Send(EDockHuman::PirateConcertina, EStormWork::Haul, Way({ {-330, -214} }, DeckZ), 180.f, ShipWorld(MainHaul.Hands0), ShipWorld(MainHaul.Hands1), 0.f);
    Send(EDockHuman::PirateCheering, EStormWork::Haul, Way({ {185, 90}, {185, -90}, {-60, -246}, {-230, -246} }, DeckZ), 180.f, ShipWorld(MainHaul.Hands0), ShipWorld(MainHaul.Hands1), .04f);
    // The fencers, their cutlasses put up, on the fore's line to starboard.
    Send(EDockHuman::PirateFencerA, EStormWork::Haul, Way({ {430, 251} }, DeckZ), 180.f, ShipWorld(ForeHaul.Hands0), ShipWorld(ForeHaul.Hands1), .5f);
    Send(EDockHuman::PirateFencerB, EStormWork::Haul, Way({ {530, 219} }, DeckZ), 180.f, ShipWorld(ForeHaul.Hands0), ShipWorld(ForeHaul.Hands1), .54f);
    // The dancer round the companion (clear of it) to the main mast's starboard fife rail, sweating up its halyard.
    Send(EDockHuman::PirateDancer, EStormWork::Sweat, Way({ {-150, 285}, {-400, 255}, {-505, 112} }, DeckZ), -90.f, ShipWorld(HalyardPin), ShipWorld(HalyardAloft), .25f);
    // The captain round the skylight and the wheel to the helm, behind the wheel facing forward.
    Send(EDockHuman::PirateCaptain, EStormWork::Helm, Way({ {-1165, -110}, {-1500, -110}, {HelmHub().X - 22.f, 0} }, QuarterZ), 0.f, ShipWorld(HelmHub()), ShipWorld(HelmHub() - FVector(1, 0, 0)), 0.f);
    // The lines they work, rigged: their blocks (wood, and the lead blocks' strops) once; the ropes each frame.
    if (!StormLines)
    {
        StormLines = NewObject<UProceduralMeshComponent>(this, TEXT("StormLines"));
        StormLines->SetupAttachment(RootComponent);
        StormLines->RegisterComponent();
        StormLines->SetWorldLocation(ShipOrigin());
        StormLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StormLines->bUseAsyncCooking = true;
        // Hemp: the CC0 rope scan (M_Rope); failing that the deck's rope coils' own material.
        UMaterialInterface* Hemp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Rope.M_Rope"));
        if (!Hemp) { const UStaticMesh* CoilMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/SM_RopeCoil.SM_RopeCoil")); Hemp = CoilMesh ? CoilMesh->GetMaterial(0) : nullptr; }
        if (!Hemp) Hemp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Canvas.M_Canvas"));
        StormLines->SetMaterial(0, Hemp);
        StormLines->SetMaterial(2, Hemp);
        if (UMaterialInterface* Wood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Wood.M_Wood"))) StormLines->SetMaterial(1, Wood);
        ShipRope::FMesh Blocks, Strops;
        for (const FHaulLine* L : { &MainHaul, &ForeHaul })
        {
            // The lead block round its sheave (where the rope bends with its hauling part at rest), stropped to a ringbolt
            // in the deck under it; the block aloft hanging from the mast, its pendant up to it.
            const FVector C = SheaveAt(*L), A = (L->Aloft - C).GetSafeNormal(), B = (L->Hands0 - C).GetSafeNormal();
            const FVector Long = (A + B).GetSafeNormal(), Cheek = SheaveAxle(*L);
            ShipRope::Shell(Blocks, C, Long, Cheek, FVector(11.f, 8.f, 5.f));
            const FVector Foot = C - Long * 11.f;
            ShipRope::Tube(Strops, { Foot + Long * 3.f, Foot, FVector(Foot.X, Foot.Y, DeckZ + 1.f) }, 1.3f);
            ShipRope::Tube(Strops, { FVector(Foot.X - 4, Foot.Y, DeckZ + 1.2f), FVector(Foot.X + 4, Foot.Y, DeckZ + 1.2f) }, 1.4f);
            ShipRope::Shell(Blocks, L->Aloft + A * 2.f, A, FVector::CrossProduct(A, FVector::UpVector).GetSafeNormal(), FVector(11.f, 8.f, 5.f));
            ShipRope::Tube(Strops, { L->Aloft + A * 10.f, L->Aloft + A * 40.f }, 1.4f);
        }
        ShipRope::Shell(Blocks, HalyardAloft + FVector(0, 0, 2), FVector::UpVector, FVector(1, 0, 0), FVector(11.f, 8.f, 5.f));
        ShipRope::Tube(Strops, { HalyardAloft + FVector(0, 0, 10), HalyardAloft + FVector(0, 0, 40) }, 1.4f);
        Blocks.Into(StormLines, 1, false);
        Strops.Into(StormLines, 2, false);
    }
    for (int32 L = 0; L < 3; ++L) { LineHauled[L] = 0.f; for (bool& B : LineFistSeen[L]) B = false; }
    UpdateStormLines(0.f);
    StormLines->SetVisibility(true);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CREW_STORM begin=1 sent=%d"), Sent);
}

void AShipCrew::UpdateStormLines(float DeltaSeconds)
{
    if (!StormLines) return;
    ShipRope::FMesh Ropes;
    float WorstGap = 0.f, WorstTurn = 0.f;
    FVector WorstAt = FVector::ZeroVector;
    for (int32 Line = 0; Line < 3; ++Line)
    {
        const bool bHalyard = Line == 2;
        const FHaulLine& H = Line == 0 ? MainHaul : ForeHaul;
        // The line the hands are put on (A toward the block or the pin, B on along it) and the way the rope runs through
        // them as it is hauled (toward its tail).
        const FVector A = bHalyard ? HalyardPin : H.Hands0, B = bHalyard ? HalyardAloft : H.Hands1;
        const FVector Dir = (B - A).GetSafeNormal(), TailWard = bHalyard ? -Dir : Dir;
        const FVector Top = bHalyard ? HalyardAloft : SheaveAt(H);
        // Each hand on it: where it holds the rope. Settling in, the rope comes up off the deck into his hands (the halyard
        // hangs taut from aloft anyway); a hand let go to reach on along it no longer bends it.
        struct FHold { FVector P; float S; FVector Fist; float Hold; };
        TArray<FHold> Holds;
        float Pulled = 0.f; int32 Pulling = 0;
        for (int32 Man = 0; Man < 2; ++Man)
        {
            const ADockNPC* N = LineMen[Line][Man] == EDockHuman::Count ? nullptr : Find(LineMen[Line][Man]);
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const int32 Slot = Man * 2 + Side;
                const float Blend = N && N->GetStormWork() != EStormWork::None ? N->GetStormBlend() : 0.f;
                const FVector FistW = N ? N->GetStormFist(Side) : FVector::ZeroVector;
                if (Blend < .02f || FistW.IsNearlyZero()) { LineFistSeen[Line][Slot] = false; continue; }
                const FVector F = ShipLocal(FistW);
                const float Hold = N->GetStormHold(Side) / FMath::Max(Blend, 1e-3f);   // (its grip alone)
                const FVector OnLine = A + Dir * FVector::DotProduct(F - A, Dir);
                const FVector Rest = bHalyard ? OnLine : FVector(OnLine.X, OnLine.Y, DeckZ + LineR);
                const FVector P = FMath::Lerp(Rest, FMath::Lerp(OnLine, F, Hold), Blend);
                Holds.Add({ P, static_cast<float>(FVector::DotProduct(P - Top, bHalyard ? -Dir : Dir)), F, Hold * Blend });
                // The rope hauled through the hands that grip it, by how far they draw it toward its tail.
                if (LineFistSeen[Line][Slot] && Hold * Blend > .9f)
                {
                    Pulled += static_cast<float>(FVector::DotProduct(F - LineFistPrev[Line][Slot], TailWard)); ++Pulling;
                }
                LineFistPrev[Line][Slot] = F; LineFistSeen[Line][Slot] = true;
            }
        }
        // (Only ever in: under the yard's weight the line does not run back when they ease and reach on; a holding hand
        // carried forward by his body slips along it.)
        if (Pulling) LineHauled[Line] += FMath::Clamp(Pulled / Pulling, 0.f, 90.f * DeltaSeconds);
        Holds.Sort([](const FHold& X, const FHold& Y) { return X.S < Y.S; });
        // Down from aloft, round the lead block's sheave, through the hands in order along it.
        TArray<ShipRope::FKnot> Knots;
        Knots.Add({ bHalyard ? HalyardAloft : H.Aloft, 0.f });
        for (const FHold& Hd : Holds) Knots.Add({ Hd.P, FistBend, 1.f });
        FVector Way, Coil0, CoilSide; float Outer, Inner;
        if (bHalyard)
        {
            // Down to its pin, over the pin's head and off it aft, outboard of the rail.
            Knots.Add({ HalyardPin + FVector(0, 0, 4), PinBend });
            Knots.Add({ HalyardPin + FVector(-8, 6, 1), 0.f });
            Way = HalyardWay; CoilSide = FVector(-Way.Y, Way.X, 0) * -1.f; Outer = 17.f; Inner = 6.f;
            Coil0 = FVector(HalyardPin.X, HalyardPin.Y + 10.f, DeckZ + LineR) + Way * 90.f;
        }
        else
        {
            if (Holds.Num() == 0) Knots.Add({ FVector(Top.X, Top.Y, DeckZ + LineR) + Dir.GetSafeNormal2D() * 40.f, 12.f });   // (nobody on it: along the deck)
            Way = FVector(Dir.X, Dir.Y, 0).GetSafeNormal(); CoilSide = H.Side; Outer = 23.f; Inner = 8.f;
            Coil0 = FVector(H.Tail.X, H.Tail.Y, DeckZ + LineR);
        }
        TArray<FVector> Path;
        if (bHalyard) ShipRope::Bent(Knots, Path);
        else
        {
            // Round the sheave from aloft toward the first hand, then on from where it leaves the sheave.
            Path.Add(H.Aloft);
            ShipRope::Sheave(H.Aloft, SheaveAt(H), SheaveAxle(H), SheaveR, Knots[1].P, Path);
            Knots[0].P = Path.Last();
            TArray<FVector> On;
            ShipRope::Bent(Knots, On);
            for (int32 I = 1; I < On.Num(); ++I) Path.Add(On[I]);
        }
        // Behind the last hand (or off the pin) slack to the deck, along it, and flaked down in its coil.
        const FVector Last = Knots.Last().P, Before = Knots[Knots.Num() - 2].P;
        FVector Touch;
        ShipRope::Droop(Last, (Last - Before).GetSafeNormal(), Way, DeckZ + LineR, Path, Touch);
        // (Along the deck in a gentle S from where it touched down to the coil's way in, however far aside the hand was.)
        const float Run = FMath::Max(static_cast<float>(FVector::DotProduct(Coil0 - Touch, Way)), 2.f);
        const FVector C0 = Touch + Way * Run + FVector::VectorPlaneProject(Coil0 - Touch, Way) * FMath::Clamp(Run / 20.f, 0.f, 1.f);
        for (int32 K = 1; K <= 8; ++K)
        {
            const float T = K / 8.f, T2 = T * T, T3 = T2 * T;
            Path.Add(Touch * (2 * T3 - 3 * T2 + 1) + Way * Run * (T3 - 2 * T2 + T) + C0 * (-2 * T3 + 3 * T2) + Way * Run * (T3 - T2));
        }
        ShipRope::Coil(Path.Last(), Way, CoilSide, Outer, Inner, Path);
        ShipRope::Tube(Ropes, Path, LineR, -FMath::Fmod(LineHauled[Line], ShipRope::LayRepeat));
        // How true it is: every gripping fist on the rope; no sharp turn anywhere along it.
        for (const FHold& Hd : Holds)
            if (Hd.Hold > .9f)
            {
                float Near = 1e9f;
                for (int32 I = 1; I < Path.Num(); ++I) Near = FMath::Min(Near, static_cast<float>(FMath::PointDistToSegment(Hd.Fist, Path[I - 1], Path[I])));
                WorstGap = FMath::Max(WorstGap, Near);
            }
        for (int32 I = 1; I + 1 < Path.Num(); ++I)
        {
            const FVector U = Path[I] - Path[I - 1], V = Path[I + 1] - Path[I];
            if (U.Size() > .2f && V.Size() > .2f)
            {
                const float Turn = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(U.GetSafeNormal(), V.GetSafeNormal())), -1.f, 1.f)));
                if (Turn > WorstTurn) { WorstTurn = Turn; WorstAt = Path[I]; }
            }
        }
    }
    Ropes.Into(StormLines, 0, true);
    LineFistGap = WorstGap; LineWorstTurn = WorstTurn; LineWorstTurnAt = WorstAt;
}

void AShipCrew::EndStorm()
{
    if (!bStorm) return;
    bStorm = false;
    for (ADockNPC* N : Crew) if (N && N->GetStormWork() != EStormWork::None) N->SetStormWork(EStormWork::None, {}, 0.f, FVector::ZeroVector, FVector::ZeroVector, 0.f);
    if (StormLines) StormLines->SetVisibility(false);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CREW_STORM end=1"));
}

int32 AShipCrew::GetAtStations(int32* Of) const
{
    int32 At = 0, Total = 0;
    for (const ADockNPC* N : Crew)
        if (N && N->GetStormWork() != EStormWork::None) { ++Total; At += N->IsAtStormStation(); }
    if (Of) *Of = Total;
    return At;
}
