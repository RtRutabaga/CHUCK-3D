#include "ShipCrew.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ShipSetting.h"

using namespace Ship;

namespace
{
    double GScoreStart = -1.0;   // audio time the ship track began
    float GScoreOffset = 0.f;
    float Ease01(float X) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f)); }
    float Held(float U, float A0, float A1, float B0, float B1) { return Ease01((U - A0) / (A1 - A0)) * (1.f - Ease01((U - B0) / (B1 - B0))); }
    // Where the crew stand (the ship frame). The deck's performers in a ring amidships, round the main hatch's grating, where
    // he comes up the companion; Jeffries with his back to the main mast's aft side, between its fife rails (left open for him
    // in milestone 2); the fencers' ground forward of the fore mast, over the fore grating, clear of the rails and the stores.
    const FVector2D DancerAt(70, -20), ConcertinaAt(215, -165), CheeringAt(150, 170);
    constexpr float JeffriesBack = 14.f;              // cm from the mast's face to his heels' line
    // The fencers: their middle stays in this box, each a half-gap either side of it along the line between them.
    constexpr float FenceX0 = 600, FenceX1 = 1150, FenceY = 170, FenceHalf = 85, FenceStep = 32, FenceTurn = 22;
    // The seated pirate on the mess table's stool that faces the door from the arrival compartment (ShipSetting.cpp's table
    // at -340, -175: five stools 130 cm round it from 20 degrees, every 72; this is the third), the table 76 high, the stools 41.
    constexpr float TableX = -340, TableY = -175, StoolR = 130, StoolA = 164, StoolTop = 41, TableTop = 76;
    // The chef at the galley's work table (560..800 x -75..75, top 86 high), on its starboard side facing across it.
    const FVector2D ChefAt(680, -108);
    // The captain (user 2026-11-02: on deck from the start; the 2D game brings him on only at the trigger): on the quarterdeck
    // just forward of the skylight, looking forward over the waist at his crew (the way across the quarterdeck passes before him).
    const FVector2D CaptainAt(-1065, -45);
    constexpr float ChefYaw = 90;
}

void ShipScore::Started(const UWorld* World, float OffsetSeconds)
{
    GScoreStart = World ? World->GetAudioTimeSeconds() : 0.0;
    GScoreOffset = OffsetSeconds;
}

bool ShipScore::IsPlaying() { return GScoreStart >= 0.0; }

float ShipScore::Beat(const UWorld* World)
{
    const float Period = 60.f / BeatsPerMinute;
    if (!World) return 0.f;
    if (GScoreStart < 0.0) return World->GetTimeSeconds() / Period;
    const float T = FMath::Fmod(static_cast<float>(World->GetAudioTimeSeconds() - GScoreStart) + GScoreOffset, Duration);
    return (T - FirstBeat) / Period;
}

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
        N->SetPirateLines({ TEXT("Deck's moving. Saves me half the work.") }, { TEXT("Other half's mine.") });
    if (ADockNPC* N = Add(EDockHuman::PirateConcertina, Deck(ConcertinaAt), Face(ConcertinaAt, DancerAt)))
        N->SetPirateLines({ TEXT("Requests are considered in the order they are shouted.") }, { TEXT("Still considering it.") });
    if (ADockNPC* N = Add(EDockHuman::PirateCheering, Deck(CheeringAt), Face(CheeringAt, DancerAt)))
        N->SetPirateLines({ TEXT("Don't mind Jeffries over there tied to the mast. He's gone mad. Too much sun, I think.") },
            { TEXT("You're not smoking the captain's cigarettes, are you!?"), TEXT("Don't let the captain catch you sniffing around in his quarters!") });
    // Jeffries, his back to the main mast, facing aft.
    if (ADockNPC* N = Add(EDockHuman::Jeffries, Deck(FVector2D(MainMastX - MastR - JeffriesBack, 0)), 180.f))
    {
        N->SetPirateLines({ TEXT("The sea is wrong, the land is wrong. Open your eyes!"), TEXT("Half the crew is gone... lost to the purple. They don't even remember them.") },
            { TEXT("Don't go into the purple! DON'T GO INTO THE PURPLE!"), TEXT("Why don't they see!?") });
        N->TieToMast(ShipWorld(FVector(MainMastX, 0, DeckZ)), MastR);
    }
    // The captain (no lines yet: the 2D game gives him none before the trigger).
    Add(EDockHuman::PirateCaptain, ShipWorld(FVector(CaptainAt.X, CaptainAt.Y, QuarterZ)), 0.f);
    // The fencers (no lines: busy).
    Pair = PairStart = FVector2D((FenceX0 + FenceX1) * .5f, 0);
    AxisYaw = AxisStart = 0.f;
    Add(EDockHuman::PirateFencerA, Deck(Pair - FVector2D(FenceHalf, 0)), 0.f);
    Add(EDockHuman::PirateFencerB, Deck(Pair + FVector2D(FenceHalf, 0)), 180.f);
    // Below decks: the seated pirate at the mess table, the chef at his work table.
    {
        const float A = FMath::DegreesToRadians(StoolA);
        const FVector2D Stool(TableX + StoolR * FMath::Cos(A), TableY + StoolR * FMath::Sin(A));
        if (ADockNPC* N = Add(EDockHuman::PirateSeated, ShipWorld(FVector(Stool.X, Stool.Y, LowerZ)), 0.f))
        {
            N->SitAtTable(ShipWorld(FVector(Stool.X, Stool.Y, LowerZ)), Face(Stool, FVector2D(TableX, TableY)), StoolTop, TableTop, ShipWorld(FVector(ArrivalAft, 0, LowerZ + 150)));
            N->SetPirateLines({ TEXT("Well I'll be. Never seen a rat wearin' a coat before.") }, { TEXT("You seem like a good listener, I bet all of the crew would love to bend your ear") });
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
    TickFencing(DeltaSeconds);
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

void AShipCrew::TickFencing(float DeltaSeconds)
{
    ADockNPC* F[2] = { Find(EDockHuman::PirateFencerA), Find(EDockHuman::PirateFencerB) };
    if (!F[0] || !F[1]) return;
    const auto Axis = [](float Yaw) { return FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw))); };
    const auto Outside = [](const FVector2D& Centre, float Yaw, float Pad = 0.f)
    {
        // How far either fencer would stand outside the ground (0 inside).
        float Out = 0.f;
        for (const float S : { -1.f, 1.f })
        {
            const FVector2D P = Centre + FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw))) * (S * (FenceHalf + Pad));
            const float Half = FMath::Min(FenceY, ShipInnerHalf(P.X, DeckZ + 40) - 90.f);
            Out = FMath::Max(Out, FMath::Max(FMath::Max(FenceX0 - static_cast<float>(P.X), static_cast<float>(P.X) - FenceX1), FMath::Abs(static_cast<float>(P.Y)) - Half));
        }
        return FMath::Max(0.f, Out);
    };
    // Eight-beat phrases: the leader advances and lunges (the other retreats and parries), then either the other answers
    // (they end where they began) or the leader presses again (they move a step and a half along the deck); the pair turns
    // a little during the recoveries. Whoever leads, and which way they turn, keeps them on their ground.
    const int32 P = FMath::FloorToInt(Beat / 8.f);
    if (P != Phrase)
    {
        Phrase = P; ++Phrases;
        LastParryGap = PhraseParryGap; PhraseParryGap = 1e3f;
        PairStart = Pair; AxisStart = AxisYaw;
        Leader = Phrases & 1;
        bPress = Phrases % 3 == 2;
        const auto Shift = [&](int32 L, bool bTwice) { return Axis(AxisStart) * (FenceStep * (L == 0 ? 1.f : -1.f) * (bTwice ? 2.f : 0.f)); };
        if (bPress && Outside(PairStart + Shift(Leader, true), AxisStart, 30.f) > 0.f)
        {
            Leader = 1 - Leader;
            if (Outside(PairStart + Shift(Leader, true), AxisStart, 30.f) > 0.f) bPress = false;
        }
        const FVector2D End = PairStart + Shift(Leader, bPress);
        AxisTurn = 0.f;
        for (const float T : { FenceTurn * (Phrases % 2 ? 1.f : -1.f), -FenceTurn * (Phrases % 2 ? 1.f : -1.f) })
            if (Outside(End, AxisStart + T, 20.f) <= 0.f) { AxisTurn = T; break; }
    }
    const float U = Beat - P * 8.f;
    const int32 Second = bPress ? Leader : 1 - Leader;
    const float Dir[2] = { 1.f, -1.f };   // fencer A advances along the axis, B against it
    const FVector2D A0 = Axis(AxisStart);
    Pair = PairStart + A0 * (FenceStep * (Ease01(U / 2.f) * Dir[Leader] + Ease01((U - 4.f) / 2.f) * Dir[Second]));
    AxisYaw = AxisStart + AxisTurn * (.5f * Ease01(U - 3.f) + .5f * Ease01(U - 7.f));
    const FVector2D Ax = Axis(AxisYaw);
    WorstFenceOut = FMath::Max(WorstFenceOut, Outside(Pair, AxisYaw));
    for (int32 I = 0; I < 2; ++I)
    {
        FFenceCue Cue;
        const FVector2D C = Pair + Ax * (I == 0 ? -FenceHalf : FenceHalf);
        Cue.Centre = ShipWorld(FVector(C.X, C.Y, DeckZ));
        Cue.Facing = FVector(Ax.X, Ax.Y, 0.f) * (I == 0 ? 1.f : -1.f);
        Cue.Target = F[1 - I]->GetActorLocation() + FVector(0, 0, 38.f);
        const bool bFirst = I == Leader, bSecond = I == Second;
        const float Attack = (bFirst ? Held(U, 2.f, 2.45f, 2.8f, 3.6f) : 0.f) + (bSecond ? Held(U, 6.f, 6.45f, 6.8f, 7.6f) : 0.f);
        const float Defend = (!bFirst ? Held(U, 1.95f, 2.3f, 2.9f, 3.5f) : 0.f) + (!bSecond ? Held(U, 5.95f, 6.3f, 6.9f, 7.5f) : 0.f);
        Cue.Thrust = FMath::Clamp(Attack, 0.f, 1.f); Cue.Lunge = Cue.Thrust; Cue.Parry = FMath::Clamp(Defend, 0.f, 1.f);
        // On an advance his front foot leads; on a retreat it follows.
        const float Adv1 = Held(U, 0.f, .6f, 1.f, 1.8f), Adv2 = Held(U - 4.f, 0.f, .6f, 1.f, 1.8f);
        Cue.Stride = (bFirst ? 14.f : -10.f) * Adv1 + (bSecond ? 14.f : -10.f) * Adv2;
        F[I]->SetFenceCue(Cue);
    }
    // How close the thrust's tip came to the parrying blade (they should meet).
    {
        FVector B0, T0, B1, T1;
        if (F[0]->GetBlade(B0, T0) && F[1]->GetBlade(B1, T1) && ((U > 2.3f && U < 3.f) || (U > 6.3f && U < 7.f)))
        {
            FVector Pa, Pb;
            FMath::SegmentDistToSegmentSafe(B0, T0, B1, T1, Pa, Pb);
            PhraseParryGap = FMath::Min(PhraseParryGap, static_cast<float>(FVector::Dist(Pa, Pb)));
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
