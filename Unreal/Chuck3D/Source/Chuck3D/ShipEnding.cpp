#include "ShipEnding.h"
#include "ShipSetting.h"
#include "ShipStorm.h"
#include "ShipCrew.h"
#include "ShipRope.h"
#include "ShipEndingScene.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "CigarettePickup.h"
#include "DockSubtitles.h"
#include "NPCVoiceData.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// The ship's ending in the storm (Claude, 2026-11-08; ShipEnding.h, docs/regions/SHIP-STORM-ENDING.md).
using namespace Ship;

namespace
{
    constexpr float Stand = 33.5f;   // his capsule's centre over a floor
    // Who speaks: the captain at the helm, the young crewman (one of the fore haulers, port side), the older one (the cheering
    // pirate, hauling at the main), Jeffries at the mainmast.
    constexpr EDockHuman Captain = EDockHuman::PirateCaptain, Crew1 = EDockHuman::PirateFencerA, Crew2 = EDockHuman::PirateCheering, Jeff = EDockHuman::Jeffries;
    const TCHAR* const CaptainNpc = TEXT("PirateCaptain");
    const TCHAR* const Crew1Npc = TEXT("PirateCrewman1");
    const TCHAR* const Crew2Npc = TEXT("PirateCrewman2");

    // The guidance (the plan's table). Jargon first, Repeat after it, Clear once he has been GuideClear s in the same place.
    struct FGuide { const TCHAR* Jargon; const TCHAR* Repeat; const TCHAR* Clear; float First; float Every; };
    enum ESit : int32 { ReefDeck, ReefFore, ReefTop, ReefHigh, ReefYard, ReefOtherL, ReefOtherS, ReefDeckL, ReefDeckS,
        LeapDeck, LeapMissed, LeapLeaf, LeapAgain, LeapJump, ForeTopSit, ForeDropped, SitCount };
    const FGuide Guides[SitCount] = {
        /* ReefDeck    */ {TEXT("CAP_10"), TEXT("CAP_10"), TEXT("CAP_11"), 8.f, 10.f},
        /* ReefFore    */ {TEXT("CAP_12"), TEXT("CAP_12"), TEXT("CAP_13"), 3.f, 11.f},
        /* ReefTop     */ {TEXT("CAP_14"), TEXT("CAP_14"), TEXT("CAP_15"), 6.f, 11.f},
        /* ReefHigh    */ {TEXT("CAP_16"), TEXT("CAP_16"), TEXT("CAP_17"), 4.f, 11.f},
        /* ReefYard    */ {TEXT("CAP_18"), TEXT("CAP_18"), TEXT("CAP_19"), 7.f, 11.f},
        /* ReefOtherL  */ {TEXT("CAP_20L"), TEXT("CAP_20L"), TEXT("CAP_21"), 5.f, 11.f},
        /* ReefOtherS  */ {TEXT("CAP_20S"), TEXT("CAP_20S"), TEXT("CAP_21"), 5.f, 11.f},
        /* ReefDeckL   */ {TEXT("CAP_22L"), TEXT("CAP_22L"), TEXT("CAP_23"), 5.f, 11.f},
        /* ReefDeckS   */ {TEXT("CAP_22S"), TEXT("CAP_22S"), TEXT("CAP_23"), 5.f, 11.f},
        /* LeapDeck    */ {TEXT("CAP_24"), TEXT("CAP_24"), TEXT("CAP_25"), 8.f, 11.f},
        /* LeapMissed  */ {TEXT("CAP_28"), TEXT("CAP_24"), TEXT("CAP_25"), 1.5f, 11.f},
        /* LeapLeaf    */ {TEXT("CAP_26"), TEXT("CAP_26"), TEXT("CAP_26"), 3.f, 12.f},
        /* LeapAgain   */ {TEXT("CAP_29"), TEXT("CAP_26"), TEXT("CAP_26"), 1.5f, 12.f},
        /* LeapJump    */ {TEXT("CAP_06"), TEXT("CAP_06"), TEXT("CAP_27"), 4.f, 6.f},
        /* ForeTopSit  */ {TEXT("CAP_30"), TEXT("CAP_30"), TEXT("CAP_31"), 6.f, 11.f},
        /* ForeDropped */ {TEXT("CAP_32"), TEXT("CAP_33"), TEXT("CAP_25"), 1.f, 11.f}};
    // The background orders, in the gaps (the joke): each once in turn, then round again more rarely.
    struct FBack { EDockHuman Who; const TCHAR* Npc; const TCHAR* Id; };
    const FBack Back[] = {{Captain, CaptainNpc, TEXT("BG_01")}, {Crew2, Crew2Npc, TEXT("BG_06")}, {Captain, CaptainNpc, TEXT("BG_02")},
        {Captain, CaptainNpc, TEXT("BG_04")}, {Crew1, Crew1Npc, TEXT("BG_08")}, {Captain, CaptainNpc, TEXT("BG_03")}, {Captain, CaptainNpc, TEXT("BG_05")},
        {Captain, CaptainNpc, TEXT("BG_07")}, {Captain, CaptainNpc, TEXT("BG_09")}};
    constexpr int32 BackCount = UE_ARRAY_COUNT(Back);
    constexpr float BackQuiet = 14.f, BackQuietLater = 24.f;

    const NPCVoiceData::FLine* FindLine(const FString& Npc, const FString& Id)
    {
        for (int32 I = 0; I < NPCVoiceData::LineCount; ++I)
            if (Npc == NPCVoiceData::Lines[I].Npc && Id == NPCVoiceData::Lines[I].Id) return &NPCVoiceData::Lines[I];
        return nullptr;
    }
    // The main course yard's swing grips (world) and the fore topsail yard's, from the ship's registered grips.
    bool OnMainCourse(const FVector& L) { return FMath::Abs(L.Z - MainYardZ) < 6.f && FMath::Abs(L.X - MainYardX) < 10.f; }
    int32 NearestMainGrip(const FVector& At, FVector* Grip = nullptr)
    {
        const TArray<FChuckSwingGrip>& All = GetChuckSwingGrips();
        int32 Best = INDEX_NONE; float BestD = 1e9f;
        for (int32 I = 0; I < All.Num(); ++I)
        {
            if (!OnMainCourse(ShipLocal(All[I].Grip))) continue;
            const float D = FVector::Dist(All[I].Grip, At);
            if (D < BestD) { BestD = D; Best = I; }
        }
        if (Grip && Best != INDEX_NONE) *Grip = All[Best].Grip;
        return BestD < 90.f ? Best : INDEX_NONE;
    }
    const FChuckClimbable* FindClimb(bool bFore, bool bLower, float Side)
    {
        for (const FChuckClimbable& C : GetChuckClimbables())
        {
            const FVector Foot = ShipLocal(C.Foot);
            if ((Foot.X > 0.f) != bFore || C.Line.Num() == 0 || FMath::Sign(Foot.Y) != Side) continue;
            if (C.bTopOut == bLower && (bLower ? Foot.Z < DeckZ + 5.f : Foot.Z > DeckZ + 300.f)) return &C;
        }
        return nullptr;
    }
    bool InBox(const FVector& L, float X0, float X1, float Half) { return L.X > X0 - 10 && L.X < X1 + 10 && FMath::Abs(L.Y) < Half + 10; }
}

const TCHAR* AShipEnding::StepName(EStep S)
{
    static const TCHAR* N[] = {TEXT("waiting"), TEXT("reef"), TEXT("trouble"), TEXT("leap"), TEXT("fore_climb"), TEXT("scene"), TEXT("done")};
    return N[FMath::Clamp(int32(S), 0, 6)];
}
const TCHAR* AShipEnding::WhereName(EWhere W)
{
    static const TCHAR* N[] = {TEXT("deck"), TEXT("main_rigging"), TEXT("fore_rigging"), TEXT("main_top"), TEXT("main_yard"), TEXT("main_above"), TEXT("fore_top"),
        TEXT("fore_above"), TEXT("air"), TEXT("other")};
    return N[FMath::Clamp(int32(W), 0, 9)];
}

FVector AShipEnding::LeafSpot() { return ShipWorld(FVector(MainMastX + 110, -104, MainTopZ + 1)); }
FVector AShipEnding::WorkSpot(int32 Side)
{
    // The outermost grip still over the deck (4.4 m out): from there the canvas flogs out to the yardarm.
    for (const FChuckSwingGrip& G : GetChuckSwingGrips())
    {
        const FVector L = ShipLocal(G.Grip);
        if (OnMainCourse(L) && FMath::Abs(FMath::Abs(L.Y) - 440.f) < 15.f && (L.Y > 0) == (Side == 0)) return G.Grip;
    }
    return ShipWorld(FVector(MainYardX, Side == 0 ? 440.f : -440.f, MainYardZ));
}
FVector AShipEnding::ForeTopSpot(float Y) { return ShipWorld(FVector(ForeMastX + 30, Y, ForeTopZ + Stand)); }
FVector AShipEnding::ForeShroudsHead(float Side)
{
    const FChuckClimbable* C = FindClimb(true, false, Side);
    return C ? C->Line.Last() : ShipWorld(FVector(ForeMastX + 31, Side * 49, ForeTopZ + 720));
}

bool ShipEndingSceneLetterboxed(const UWorld* World)
{
    const AShipForeTopsailScene* S = AShipForeTopsailScene::Get(World);
    return S && S->IsLetterboxed();
}

AShipEnding::AShipEnding()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;   // (after his own movement: the leap's launch is set after his takeoff)
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AShipEnding* AShipEnding::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AShipEnding> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipEnding* AShipEnding::Spawn(UWorld* World)
{
    if (AShipEnding* E = Get(World)) return E;
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AShipEnding* E = World->SpawnActor<AShipEnding>(ShipOrigin(), FRotator::ZeroRotator, P);
    if (!E) return nullptr;
    // Shouted: heard all over the deck and up in the rig (the ship is 34 m long, the fore topmast head 20 m up).
    E->Shout = NewObject<USoundAttenuation>(E);
    FSoundAttenuationSettings& A = E->Shout->Attenuation;
    A.bAttenuate = true; A.bSpatialize = true;
    A.AttenuationShape = EAttenuationShape::Sphere;
    A.AttenuationShapeExtents = FVector(1800.f, 0.f, 0.f);
    A.FalloffDistance = 3000.f;
    A.NonSpatializedRadiusStart = 900.f; A.NonSpatializedRadiusEnd = 300.f;
    // The guide lights: the golden leaf's aura, dimmer.
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* Aura = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Pickups/M_GoldenLeafAura.M_GoldenLeafAura"));
    for (int32 I = 0; I < 4; ++I)
    {
        auto* H = NewObject<UStaticMeshComponent>(E, *FString::Printf(TEXT("GuideHalo%d"), I));
        H->SetupAttachment(E->RootComponent); H->SetStaticMesh(Sphere);
        H->SetCollisionEnabled(ECollisionEnabled::NoCollision); H->SetCastShadow(false); H->SetCanEverAffectNavigation(false);
        H->SetUsingAbsoluteLocation(true); H->SetWorldScale3D(FVector(.55f)); H->SetVisibility(false);
        H->RegisterComponent();
        UMaterialInstanceDynamic* M = Aura ? H->CreateDynamicMaterialInstance(0, Aura) : nullptr;
        if (M) M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, .64f, .2f));
        auto* L = NewObject<UPointLightComponent>(E, *FString::Printf(TEXT("GuideLamp%d"), I));
        L->SetupAttachment(E->RootComponent); L->SetUsingAbsoluteLocation(true);
        L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f); L->SetAttenuationRadius(260.f);
        L->SetLightColor(FLinearColor(1.f, .64f, .2f)); L->SetCastShadows(true);   // (shadowed, so it never lights his fur through him) L->SetVisibility(false);
        L->RegisterComponent();
        E->GuideHalo.Add(H); E->GuideLamp.Add(L); E->GuideMat.Add(M);
    }
    UMaterialInterface* Canvas = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Canvas.M_Canvas"));
    UMaterialInterface* Tarred = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/MI_RopeTarred.MI_RopeTarred"));
    E->Flaps = NewObject<UProceduralMeshComponent>(E, TEXT("LooseCanvas"));
    E->Flaps->SetupAttachment(E->RootComponent); E->Flaps->SetCollisionEnabled(ECollisionEnabled::NoCollision); E->Flaps->RegisterComponent();
    E->Flaps->SetWorldLocation(ShipOrigin()); E->Flaps->SetMaterial(0, Canvas); E->Flaps->SetMaterial(1, Canvas);
    E->Strands = NewObject<UProceduralMeshComponent>(E, TEXT("CarriedRatlines"));
    E->Strands->SetupAttachment(E->RootComponent); E->Strands->SetCollisionEnabled(ECollisionEnabled::NoCollision); E->Strands->RegisterComponent();
    E->Strands->SetWorldLocation(ShipOrigin()); E->Strands->SetMaterial(0, Tarred);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_READY canvas=%d tarred=%d aura=%d"), Canvas != nullptr, Tarred != nullptr, Aura != nullptr);
    return E;
}

ADockNPC* AShipEnding::Body(EDockHuman Who) const
{
    const AShipCrew* Crew = AShipCrew::Get(GetWorld());
    return Crew ? Crew->Find(Who) : nullptr;
}

bool AShipEnding::Say(EDockHuman Who, const TCHAR* Npc, const TCHAR* Id, bool bForce)
{
    if (IsSpeaking() && !bForce) return false;
    const NPCVoiceData::FLine* Line = FindLine(Npc, Id);
    if (!Line) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_LINE_MISSING %s %s"), Npc, Id); return false; }
    if (IsSpeaking()) if (ADockNPC* Was = Body(Speaking.Who)) Was->StopSpeaking(.15f);
    ADockNPC* N = Body(Who);
    const float Seconds = N ? N->SpeakLine(Npc, Id, Shout) : -1.f;
    Speaking.Who = Who; Speaking.Npc = Npc; Speaking.Id = Id; Speaking.Text = Line->Text;
    Speaking.Seconds = Seconds > 0.f ? Seconds : Line->Seconds; Speaking.Elapsed = 0.f;
    LastLine = Id; ++LinesSpoken; Said.FindOrAdd(Id)++;
    QuietFor = 0.f;
    const AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_LINE id=%s npc=%s body=%d seconds=%.2f step=%s where=%s at=%s"), Id, Npc, N != nullptr, Speaking.Seconds, StepName(Step), WhereName(Where),
        Chuck ? *ShipLocal(Chuck->GetActorLocation()).ToString() : TEXT("-"));
    return true;
}

void AShipEnding::Queue(EDockHuman Who, const TCHAR* Npc, const TCHAR* Id, float Gap)
{
    Pending.Add({Who, Npc, Id, Gap});
}

bool AShipEnding::GetSubtitle(FString& Speaker, FString& Text) const
{
    if (!IsSpeaking() || Speaking.Id.StartsWith(TEXT("JEF_X"))) return false;   // (Jeffries' strain and his choking: sounds only)
    const AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck || (!InShip(Chuck->GetActorLocation()) && !ShipEndingSceneLetterboxed(GetWorld()))) return false;
    Speaker = Speaking.Who == Captain ? TEXT("Captain") : Speaking.Who == Jeff ? TEXT("Jeffries") : TEXT("Crewman");
    Text = DockSubtitles::At(Speaking.Text, Speaking.Seconds, Speaking.Elapsed);
    return true;
}

void AShipEnding::Reset()
{
    Step = EStep::Waiting; bSideDone[0] = bSideDone[1] = false; WorkTime[0] = WorkTime[1] = 0.f;
    bCarried = bStartLines = bTroubleQueued = bCaptainCalled = bJumpCalled = bMissed = bForeDropped = bLeafAgain = bLeafSaid = bLanded = bWasOnTop = bLeaping = false;
    BothDoneAt = -1.f; Background = 0; Situation = -1; Pending.Reset(); FlapRoll[0] = FlapRoll[1] = 0.f;
    if (IsSpeaking()) if (ADockNPC* N = Body(Speaking.Who)) N->StopSpeaking(.2f);
    Speaking = FSpoken();
    if (Leaf.IsValid()) Leaf->Destroy();
    Leaf.Reset();
    // The fore ratlines whole again.
    for (TActorIterator<AShipSetting> It(GetWorld()); It; ++It)
        for (UActorComponent* C : It->GetComponents())
            if (C->GetFName() == TEXT("ShipForeRatlines")) Cast<USceneComponent>(C)->SetVisibility(true);
    for (float Side : {1.f, -1.f}) if (const FChuckClimbable* C = FindClimb(true, true, Side)) SetChuckClimbableBlocked(C->Lip, false);
    StrandList.Reset(); if (Strands) Strands->ClearAllMeshSections();
    SetGuides({});
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_RESET"));
}

void AShipEnding::SkipTo(EStep To)
{
    Reset();
    bStartLines = true;
    if (To >= EStep::Trouble) { bSideDone[0] = bSideDone[1] = true; FlapRoll[0] = FlapRoll[1] = 1.f; }
    if (To >= EStep::Leap) { CarryAway(); bTroubleQueued = bCaptainCalled = true; }
    if (To >= EStep::ForeClimb) bJumpCalled = true;
    Step = To == EStep::Trouble ? EStep::Leap : To;   // (the trouble's lines are the carry-away: skipping to it is the leap)
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_SKIP to=%s"), StepName(Step));
}

void AShipEnding::SpawnLeaf()
{
    if (Leaf.IsValid()) Leaf->Destroy();
    Leaf = ACigarettePickup::SpawnGolden(GetWorld(), LeafSpot());
    if (Leaf.IsValid()) Leaf->SetSoftAura();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_LEAF spawned=%d again=%d at=%s"), Leaf.IsValid(), bLeafAgain, *ShipLocal(LeafSpot()).ToString());
}

void AShipEnding::CarryAway()
{
    if (bCarried) return;
    bCarried = true; CarriedClock = 0.f;
    // The fore lower shrouds' ratlines go (their rungs), the shrouds stand; their nets close.
    for (TActorIterator<AShipSetting> It(GetWorld()); It; ++It)
        for (UActorComponent* C : It->GetComponents())
            if (C->GetFName() == TEXT("ShipForeRatlines")) Cast<USceneComponent>(C)->SetVisibility(false);
    int32 Blocked = 0;
    StrandList.Reset();
    for (float Side : {1.f, -1.f})
    {
        if (const FChuckClimbable* C = FindClimb(true, true, Side)) Blocked += SetChuckClimbableBlocked(C->Lip, true);
        // The broken rungs' ends left on the shrouds, streaming to leeward (port) and down: along the net's line, every 38 cm.
        const FChuckClimbable* C = FindClimb(true, true, Side);
        if (!C || C->Line.Num() < 4) continue;
        const FVector A = ShipLocal(C->Line[2]), B = ShipLocal(C->Line.Last());
        FRandomStream R(int32(Side * 7 + 11));
        for (float T = 0.f; T <= 1.f; T += .085f)
        {
            const FVector P = FMath::Lerp(A, B, T) - FVector(0, Side * 18, 0);
            for (float Dx : {-55.f, 30.f})
                if (R.FRand() < .8f) StrandList.Add({P + FVector(Dx, 0, 0), FVector(R.FRandRange(-.3f, .2f), 1.f, -.5f).GetSafeNormal(), static_cast<float>(R.FRandRange(18.f, 48.f)), static_cast<float>(R.FRandRange(0.f, 6.f))});
        }
    }
    if (AShipStorm* Storm = AShipStorm::Get(GetWorld())) Storm->DeckWaveNow(ForeMastX - 40.f, 1.25f);
    if (USoundBase* Crack = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_ShipCreak_02.SFX_ShipCreak_02")))
        UGameplayStatics::PlaySoundAtLocation(this, Crack, ShipWorld(FVector(ForeMastX, 0, DeckZ + 400)), 1.f, .7f);
    SpawnLeaf();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_FORE_CARRIED blocked=%d strands=%d"), Blocked, StrandList.Num());
}

void AShipEnding::TickWhere(const AChuckCharacter* Chuck)
{
    const FVector L = ShipLocal(Chuck->GetActorLocation());
    const auto* M = Chuck->GetCharacterMovement();
    EWhere W = EWhere::Other;
    if (Chuck->IsOnLadder()) W = L.X < 0 ? (L.Z < MainTopZ - 5 ? EWhere::MainRigging : EWhere::MainAbove) : (L.Z < ForeTopZ - 5 ? EWhere::ForeRigging : EWhere::ForeAbove);
    else if (Chuck->IsSwinging() || Chuck->IsSwingLeaping() || Chuck->IsHanging())
        W = L.X < 0 ? (L.Z < MainTopZ ? EWhere::MainYard : EWhere::MainAbove) : (L.Z < ForeTopZ ? EWhere::ForeRigging : EWhere::ForeAbove);
    else if (M->IsMovingOnGround())
    {
        if (FMath::Abs(L.Z - (MainTopZ + Stand)) < 25 && InBox(L, TopX0, TopX1, TopHalf)) W = EWhere::MainTop;
        else if (FMath::Abs(L.Z - (ForeTopZ + Stand)) < 25 && InBox(L, ForeMastX - 70, ForeMastX + 130, 120)) W = EWhere::ForeTop;
        else if (L.Z < DeckZ - 40) W = EWhere::Other;
        else if (L.Z < DeckZ + 300) W = EWhere::Deck;
        else W = L.X < 0 ? EWhere::MainAbove : EWhere::ForeAbove;
    }
    else W = EWhere::Air;
    if (W != Where) UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_WHERE %s -> %s step=%s at=%s"), WhereName(Where), WhereName(W), StepName(Step), *L.ToString());
    Where = W;
    if (W != EWhere::Air) LastGround = W;
}

void AShipEnding::TickSpeech(float Dt)
{
    if (IsSpeaking())
    {
        Speaking.Elapsed += Dt;
        if (Speaking.Elapsed > Speaking.Seconds + .1f) { Speaking.Seconds = 0.f; Speaking.Elapsed = 0.f; }
        return;
    }
    QuietFor += Dt;
    if (Pending.Num() == 0) return;
    // The second crewman's call that the fore ratlines have carried away waits until he is off the fore lower shrouds.
    if (Pending[0].Id == TEXT("CR2_03") && Where == EWhere::ForeRigging) return;
    if ((PendingWait += Dt) < Pending[0].Gap) return;
    const FQueued Q = Pending[0];
    Pending.RemoveAt(0); PendingWait = 0.f;
    Say(Q.Who, *Q.Npc, *Q.Id);
    if (Q.Id == TEXT("CR2_03")) CarryAway();
}

void AShipEnding::TickSteps(float Dt, AChuckCharacter* Chuck)
{
    AShipStorm* Storm = AShipStorm::Get(GetWorld());
    const FVector L = ShipLocal(Chuck->GetActorLocation());
    switch (Step)
    {
    case EStep::Waiting:
        if (!Storm || !Storm->HasStarted()) return;
        // The storm upon them: the captain's two lines (the user's, 2026-11-05).
        if (!bStartLines && Storm->GetClock() > 3.f) { Queue(Captain, CaptainNpc, TEXT("CAP_S00"), 0.f); Queue(Captain, CaptainNpc, TEXT("CAP_S01"), 1.5f); bStartLines = true; }
        // The orders begin with the storm's second part.
        if (Storm->GetIntense() > .05f && bStartLines)
        {
            Step = EStep::Reef; Progress();
            Queue(Captain, CaptainNpc, TEXT("CAP_01"), 1.f); Queue(Crew1, Crew1Npc, TEXT("CR1_01"), .7f);
            Queue(Crew2, Crew2Npc, TEXT("CR2_01"), .5f); Queue(Captain, CaptainNpc, TEXT("CAP_02"), .7f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_STEP reef storm_s=%.1f"), Storm->GetClock());
        }
        return;
    case EStep::Reef:
    {
        // Hanging still at a work spot on the main yard: that side's points tied.
        FVector Grip;
        const int32 G = Chuck->IsSwinging() ? NearestMainGrip(Chuck->GetActorLocation(), &Grip) : INDEX_NONE;
        const FVector GL = ShipLocal(Grip);
        for (int32 S = 0; S < 2; ++S)
        {
            const bool bHere = G != INDEX_NONE && FMath::Abs(GL.Y) >= WorkReach && (GL.Y > 0) == (S == 0) && !bSideDone[S];
            WorkTime[S] = bHere ? WorkTime[S] + Dt : 0.f;
            if (WorkTime[S] < WorkHold) continue;
            bSideDone[S] = true; Progress();
            if (USoundBase* Tug = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_ShipCreak_01.SFX_ShipCreak_01")))
                UGameplayStatics::PlaySoundAtLocation(this, Tug, Grip, .8f, 1.3f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_REEF side=%s grip_y=%.0f"), S == 0 ? TEXT("larboard") : TEXT("starboard"), GL.Y);
            Queue(Crew1, Crew1Npc, S == 0 ? TEXT("CR1_02L") : TEXT("CR1_02S"), .4f);
            if (bSideDone[0] && bSideDone[1])
            {
                Queue(Crew2, Crew2Npc, TEXT("CR2_02"), .8f);
                Step = EStep::Trouble; BothDoneAt = Clock;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_STEP trouble"));
            }
        }
        return;
    }
    case EStep::Trouble:
        if (!bTroubleQueued && Clock - BothDoneAt > 3.f && Pending.Num() == 0 && !IsSpeaking() && Where != EWhere::Air)
        {
            Queue(Crew1, Crew1Npc, TEXT("CR1_03"), 2.f); Queue(Crew1, Crew1Npc, TEXT("CR1_04"), .8f); Queue(Crew2, Crew2Npc, TEXT("CR2_03"), .8f);
            bTroubleQueued = true;
        }
        if (bCarried && !bCaptainCalled && Pending.Num() == 0 && !IsSpeaking())
        {
            bCaptainCalled = true; Progress();
            const bool bMain = Where == EWhere::MainTop || Where == EWhere::MainYard || Where == EWhere::MainAbove || (Where == EWhere::MainRigging && L.Z > MainTopZ - 400);
            const bool bFore = Where == EWhere::ForeTop || Where == EWhere::ForeAbove || (Where == EWhere::Air && L.X > 0 && L.Z > ForeTopZ - 100);
            Queue(Captain, CaptainNpc, bFore ? TEXT("CAP_05") : bMain ? TEXT("CAP_03") : TEXT("CAP_04"), 1.f);
            Step = bFore ? EStep::ForeClimb : EStep::Leap;
            if (bFore) bJumpCalled = true;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_STEP %s"), StepName(Step));
        }
        return;
    case EStep::Leap:
        // On the main top: the order to jump (once; the guidance repeats it).
        if (!bJumpCalled && Where == EWhere::MainTop && Pending.Num() == 0) { Queue(Captain, CaptainNpc, TEXT("CAP_06"), .8f); bJumpCalled = true; Progress(); }
        // The leaf: smoked and burnt out before he made the jump, another by the jar.
        if (Leaf.IsValid() && !Leaf->IsGoldenAvailable() && !Chuck->IsStaminaLocked())
        {
            if ((LeafGone += Dt) > 2.f) { bLeafAgain = true; bLeafSaid = false; LeafGone = -1.f; SpawnLeaf(); }
        }
        else LeafGone = FMath::Min(LeafGone, 0.f);
        if (!Leaf.IsValid() && bCarried) SpawnLeaf();
        if (Where == EWhere::ForeTop || Where == EWhere::ForeAbove)
        {
            ++ForeLandings; Step = EStep::ForeClimb; Progress(); bForeDropped = bMissed = false;
            Queue(Crew2, Crew2Npc, TEXT("CR2_04"), .3f); Queue(Captain, CaptainNpc, TEXT("CAP_07"), .8f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_FORE_LANDED n=%d at=%s leaps=%d"), ForeLandings, *L.ToString(), Leaps);
        }
        return;
    case EStep::ForeClimb:
    {
        if (Where == EWhere::Deck) { bForeDropped = true; Step = EStep::Leap; UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_FORE_DROPPED at=%s"), *L.ToString()); return; }
        if (Where == EWhere::MainTop || Where == EWhere::MainRigging) { Step = EStep::Leap; return; }
        // At the head of the fore topmast shrouds: the cutscene.
        const float HeadZ = static_cast<float>(ShipLocal(ForeShroudsHead(1.f)).Z);
        if ((Chuck->IsOnLadder() || Chuck->IsSwinging() || Chuck->IsSwingLeaping()) && L.X > 0 && L.Z > HeadZ - 25.f && Pending.Num() == 0)
        {
            if (IsSpeaking()) if (ADockNPC* N = Body(Speaking.Who)) N->StopSpeaking(.4f);
            Speaking = FSpoken();
            Step = EStep::Scene; SetGuides({});
            AShipForeTopsailScene::Play(GetWorld(), FMath::Sign(L.Y) == 0 ? 1.f : FMath::Sign(static_cast<float>(L.Y)));
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENDING_STEP scene at=%s"), *L.ToString());
        }
        return;
    }
    case EStep::Scene:
        if (const AShipForeTopsailScene* S = AShipForeTopsailScene::Get(GetWorld())) if (S->IsFinished()) Step = EStep::Done;
        return;
    default: return;
    }
}

void AShipEnding::TickLeap(float Dt, AChuckCharacter* Chuck)
{
    auto* M = Chuck->GetCharacterMovement();
    const FVector L = ShipLocal(Chuck->GetActorLocation());
    const bool bGround = M->IsMovingOnGround();
    // Off the main top's fore rim on the golden leaf, running toward the fore: the long leap.
    // Off the main top's fore edge on the golden leaf, heading forward: the long leap. Any way he leaves it (a running jump, a
    // sprint leap, or the run's vault over the rim, which reaches the air at its top), once he is in free flight just past it.
    const bool bFree = !bGround && FCString::Strcmp(Chuck->GetGaitName(), TEXT("Air")) == 0;
    if (bWasOnTop && bFree && !bLeaping && L.Z > MainTopZ && L.X > TopX1 - 70.f && L.X < TopX1 + 160.f)
    {
        const FVector H(M->Velocity.X, M->Velocity.Y, 0.f);
        const bool bGo = bCarried && Step == EStep::Leap && Chuck->IsStaminaLocked() && M->Velocity.Z > -150.f && H.Size() > 120.f && H.GetSafeNormal().X > .6f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_LEAP_TAKEOFF go=%d carried=%d step=%s locked=%d at=%s v=%s"), bGo, bCarried, StepName(Step), Chuck->IsStaminaLocked(), *L.ToString(), *M->Velocity.ToString());
        if (bGo)
        {
            const FVector Dir = H.GetSafeNormal();
            M->Velocity = Dir * LeapForward + FVector(0, 0, LeapUp);
            bLeaping = true; ++Leaps; LeapFrom = L; Progress();
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_LEAP n=%d from=%s run_cm_s=%.0f dir=%s sprint=%d"), Leaps, *L.ToString(), H.Size(), *Dir.ToString(), Chuck->IsSprintLeaping() ? 1 : 0);
        }
        bWasOnTop = false;   // (one try a departure)
    }
    if (bGround || Chuck->IsOnLadder() || Chuck->IsSwinging() || Chuck->IsHanging()) bWasOnTop = bGround && Where == EWhere::MainTop;
    if (!bLeaping) return;
    if (bGround || Chuck->IsOnLadder() || Chuck->IsSwinging() || Chuck->IsHanging() || Chuck->IsAstral())
    {
        bLeaping = false; LeapTo = L;
        if (Where == EWhere::Deck) bMissed = true;
        // Onto the fore top (2 m deep): he lands and stops there, not sliding on off its far side at the leap's speed.
        if (Where == EWhere::ForeTop || (bGround && L.X > ForeMastX - 90.f && L.Z > ForeTopZ)) M->Velocity = FVector(M->Velocity.X, M->Velocity.Y, 0.f).GetClampedToMaxSize(40.f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_LEAP_END to=%s where=%s out_cm=%.0f"), *L.ToString(), WhereName(Where), FVector::Dist2D(LeapFrom, L));
        return;
    }
    // Drawn gently onto the fore top when aimed near it: the horizontal speed eased toward what lands there.
    if (L.X > ForeMastX - 80.f) return;
    const float G = -static_cast<float>(M->GetGravityZ());
    const FVector Target(ForeMastX + 30.f, FMath::Clamp(static_cast<float>(L.Y), -95.f, 95.f), ForeTopZ + Stand);
    const float Vz = static_cast<float>(M->Velocity.Z), Drop = static_cast<float>(L.Z - Target.Z);
    const float Disc = Vz * Vz + 2.f * G * Drop;
    if (G <= 0.f || Disc < 0.f) return;
    const float T = (Vz + FMath::Sqrt(Disc)) / G;
    if (T < .08f) return;
    FVector Want = FVector(Target.X - L.X, Target.Y - L.Y, 0.f) / T;
    if (FMath::Abs(Target.Y) < 50.f) Want.Y = (FMath::Sign(L.Y + .01f) * 55.f - L.Y) / T;   // (clear of the mast's hole)
    const FVector H(M->Velocity.X, M->Velocity.Y, 0.f);
    const float Cos = static_cast<float>(FVector::DotProduct(H.GetSafeNormal(), Want.GetSafeNormal()));
    if (Cos < FMath::Cos(FMath::DegreesToRadians(22.f)) || Want.Size() > H.Size() * 1.5f || Want.Size() < H.Size() * .55f) return;
    const FVector NewH = FMath::Lerp(H, Want, FMath::Min(1.f, 5.f * Dt));
    M->Velocity = FVector(NewH.X, NewH.Y, M->Velocity.Z);
}

void AShipEnding::TickGuidance(float Dt, const AChuckCharacter* Chuck)
{
    int32 Sit = -1;
    const bool bOne = bSideDone[0] != bSideDone[1];
    const int32 Left = bSideDone[0] ? 1 : 0;   // the side still loose
    switch (Step)
    {
    case EStep::Reef:
        switch (Where)
        {
        case EWhere::Deck: Sit = bOne ? (Left == 0 ? ReefDeckL : ReefDeckS) : ReefDeck; break;
        case EWhere::ForeRigging: case EWhere::ForeTop: case EWhere::ForeAbove: Sit = ReefFore; break;
        case EWhere::MainTop: Sit = bOne ? (Left == 0 ? ReefOtherL : ReefOtherS) : ReefTop; break;
        case EWhere::MainAbove: Sit = ReefHigh; break;
        case EWhere::MainYard:
        {
            FVector Grip;
            const bool bAt = NearestMainGrip(Chuck->GetActorLocation(), &Grip) != INDEX_NONE;
            const FVector GL = ShipLocal(Grip);
            const int32 SideHere = GL.Y > 0 ? 0 : 1;
            if (bAt && bSideDone[SideHere]) Sit = Left == 0 ? ReefOtherL : ReefOtherS;
            else if (!(bAt && FMath::Abs(GL.Y) >= WorkReach - 100.f)) Sit = ReefYard;
            break;
        }
        default: break;
        }
        break;
    case EStep::Leap:
        if (!bCaptainCalled) break;
        switch (Where)
        {
        case EWhere::Deck: Sit = bForeDropped ? ForeDropped : bMissed ? LeapMissed : LeapDeck; break;
        case EWhere::MainTop: Sit = Chuck->IsStaminaLocked() ? LeapJump : (bLeafAgain && !bLeafSaid) ? LeapAgain : LeapLeaf; break;
        default: break;
        }
        break;
    case EStep::ForeClimb:
        if (Where == EWhere::ForeTop) Sit = ForeTopSit;
        break;
    default: break;
    }
    if (Sit != Situation) { Situation = Sit; SituationTime = 0.f; GuideSaid = 0; NextGuideIn = Sit >= 0 ? Guides[Sit].First : -1.f; }
    if (Sit < 0) return;
    // (The time there counts once the scripted lines have been said; the clear line only after the jargon has been heard.)
    if (Pending.Num()) return;
    SituationTime += Dt;
    if (IsSpeaking()) return;
    if ((NextGuideIn -= Dt) > 0.f) return;
    const FGuide& G = Guides[Sit];
    const TCHAR* Id = SituationTime >= GuideClear && GuideSaid > 0 ? G.Clear : GuideSaid > 0 ? G.Repeat : G.Jargon;
    if (!Say(Captain, CaptainNpc, Id)) return;
    ++GuideSaid; ++GuidesShown; NextGuideIn = G.Every;
    // The one-off calls: after the miss and the new leaf they are said, the usual lines follow.
    if (Sit == LeapMissed) bMissed = false;
    if (Sit == LeapAgain) bLeafSaid = true;
    if (Sit == ForeDropped && GuideSaid > 2) bForeDropped = false;
}

void AShipEnding::SetGuides(const TArray<FVector>& At)
{
    GuideAt = At;
    for (int32 I = 0; I < GuideHalo.Num(); ++I)
    {
        const bool bOn = At.IsValidIndex(I);
        GuideHalo[I]->SetVisibility(bOn); GuideLamp[I]->SetVisibility(bOn);
        if (bOn) { GuideHalo[I]->SetWorldLocation(At[I]); GuideLamp[I]->SetWorldLocation(At[I] + FVector(0, 0, 20)); }
    }
}

void AShipEnding::TickGuideLights(float Dt)
{
    // Only the next target or targets glow; the crew never mention it.
    TArray<FVector> At;
    auto MainFeet = [&]() { for (float Side : {1.f, -1.f}) if (const FChuckClimbable* C = FindClimb(false, true, Side)) At.Add(C->Foot + FVector(0, 0, 30)); };
    const FVector MainTop = ShipWorld(FVector((TopX0 + TopX1) * .5f + 40.f, 70.f, MainTopZ + 25.f));
    switch (Step)
    {
    case EStep::Reef:
        if (Where == EWhere::Deck || Where == EWhere::ForeRigging || Where == EWhere::ForeTop || Where == EWhere::ForeAbove) MainFeet();
        else if (Where == EWhere::MainRigging) At.Add(MainTop);
        else for (int32 S = 0; S < 2; ++S) if (!bSideDone[S]) At.Add(WorkSpot(S) - FVector(0, 0, 25));
        break;
    case EStep::Leap:
        if (!bCaptainCalled) break;
        if (Where == EWhere::Deck || Where == EWhere::Other) MainFeet();
        else if (Where == EWhere::MainRigging) At.Add(MainTop);
        else { At.Add(ForeTopSpot(70.f) - FVector(0, 0, 8)); At.Add(ForeTopSpot(-70.f) - FVector(0, 0, 8)); }
        break;
    case EStep::ForeClimb:
        if (Where == EWhere::ForeTop) for (float Side : {1.f, -1.f}) { if (const FChuckClimbable* C = FindClimb(true, false, Side)) At.Add(C->Line[1]); }
        else for (float Side : {1.f, -1.f}) At.Add(ForeShroudsHead(Side) + FVector(0, 0, 25));
        break;
    default: break;
    }
    if (At.Num() != GuideAt.Num() || (At.Num() && !At[0].Equals(GuideAt[0], 1.f))) { SetGuides(At); ++GuidesShown; }
    GuidePhase += Dt;
    // Soft (user 2026-11-09): a gentle gold glow that breathes a little, slowly, and bobs only slightly.
    const float Breath = .5f + .5f * FMath::Sin(GuidePhase * 1.4f);
    for (int32 I = 0; I < GuideAt.Num() && I < GuideHalo.Num(); ++I)
    {
        if (GuideMat[I]) GuideMat[I]->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(.2f, .26f, Breath));
        GuideLamp[I]->SetIntensity(FMath::Lerp(.45f, .55f, Breath));
        GuideHalo[I]->SetWorldLocation(GuideAt[I] + FVector(0, 0, 2.f * FMath::Sin(GuidePhase * 1.1f + I)));
    }
}

void AShipEnding::TickFlaps(float Dt)
{
    // The main course's loose canvas at each yardarm: out from the work spot to the yardarm, bellying forward and flogging in
    // the gale; tied, it rolls up onto the yard and is gone. Seen only in the storm's second part.
    if (!Flaps) return;
    const AShipStorm* Storm = AShipStorm::Get(GetWorld());
    const float Show = Storm ? FMath::Clamp(Storm->GetIntense() * 3.f, 0.f, 1.f) : 0.f;
    FlapClock += Dt * (1.f + 1.5f * ShipStormWind());
    for (int32 S = 0; S < 2; ++S)
    {
        FlapRoll[S] = bSideDone[S] ? FMath::Min(1.f, FlapRoll[S] + Dt / 1.4f) : 0.f;
        const float Drop = 135.f * Show * (1.f - FMath::SmoothStep(0.f, 1.f, FlapRoll[S]));
        if (Drop < 2.f) { Flaps->ClearMeshSection(S); continue; }
        const float Side = S == 0 ? 1.f : -1.f;
        constexpr int32 NU = 10, NV = 7;
        TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV;
        for (int32 I = 0; I <= NU; ++I)
            for (int32 J = 0; J <= NV; ++J)
            {
                const float U = I / float(NU), W = J / float(NV);
                const float Y = Side * FMath::Lerp(330.f, 600.f, U);
                // Bellied forward, wider out toward the yardarm; flogging waves running out along it.
                const float Belly = (35.f + 30.f * U) * FMath::Sin(PI * .8f * W) * (.6f + .4f * U);
                const float Flog = (10.f + 26.f * U) * W * FMath::Sin(FlapClock * 9.f - U * 6.f + W * 3.f + S * 1.7f) + 8.f * W * FMath::Sin(FlapClock * 17.f + U * 11.f);
                V.Add(FVector(MainYardX + 22.f + Belly + Flog, Y + Side * W * 25.f * U, MainYardZ + 6.f - W * Drop * (.75f + .25f * U)));
                N.Add(FVector(-1, 0, 0)); UV.Add(FVector2D(U * 2.4f, W * 1.2f));
            }
        for (int32 I = 0; I < NU; ++I)
            for (int32 J = 0; J < NV; ++J)
            {
                const int32 A = I * (NV + 1) + J, B = A + NV + 1;
                T.Append({A, B, B + 1, A, B + 1, A + 1, A, B + 1, B, A, A + 1, B + 1});   // (both faces)
            }
        Flaps->CreateMeshSection(S, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    }
}

void AShipEnding::TickCarried(float Dt)
{
    if (!bCarried || !Strands || StrandList.Num() == 0) return;
    CarriedClock += Dt;
    // The broken rung ends flog out to leeward, faster in the gusts.
    const float Wind = FMath::Max(.6f, ShipStormWind());
    ShipRope::FMesh Mesh;
    for (const FStrand& S : StrandList)
    {
        TArray<FVector> Path;
        for (int32 K = 0; K <= 5; ++K)
        {
            const float U = K / 5.f;
            const FVector Wave = FVector(0, 0, 1) * (9.f * U * FMath::Sin(CarriedClock * 13.f * Wind + S.Phase + U * 4.f)) + FVector(1, 0, 0) * (5.f * U * FMath::Sin(CarriedClock * 9.f + S.Phase * 2.f));
            Path.Add(S.Anchor + S.Lee * (S.Len * U) + Wave);
        }
        ShipRope::Tube(Mesh, Path, .9f, 0.f, 6);
    }
    Mesh.Into(Strands, 0, false);
}

void AShipEnding::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    TickFlaps(Dt);
    TickCarried(Dt);
    if (!Chuck || !ShipGate::IsOpen()) { TickSpeech(Dt); return; }
    if (Step == EStep::Scene || Step == EStep::Done) { TickSpeech(Dt); TickSteps(Dt, Chuck); return; }
    if (!InShip(Chuck->GetActorLocation())) { TickSpeech(Dt); return; }
    TickWhere(Chuck);
    TickLeap(Dt, Chuck);
    TickSteps(Dt, Chuck);
    TickSpeech(Dt);
    TickGuidance(Dt, Chuck);
    TickGuideLights(Dt);
    // The background orders, in the gaps (from the storm's start lines on).
    const bool bLive = bStartLines && !IsSpeaking() && Pending.Num() == 0 && (Step == EStep::Waiting || Step == EStep::Reef || Step == EStep::Leap || Step == EStep::ForeClimb);
    if (bLive && QuietFor > (Background < BackCount ? BackQuiet : BackQuietLater) && (Situation < 0 || NextGuideIn > 5.f) && Said.Contains(TEXT("CAP_S01")))
    {
        const FBack& B = Back[Background % BackCount];
        if (Say(B.Who, B.Npc, B.Id)) ++Background;
    }
    // Jeffries: a sea breaking aboard catches him at the mast, now and then; and he strains at his ropes as the storm builds.
    if (const AShipStorm* Storm = AShipStorm::Get(GetWorld()))
    {
        if (Storm->GetDeckWaves() < WavesSeen) WavesSeen = Storm->GetDeckWaves();
        if (Storm->GetDeckWaves() > WavesSeen)
        {
            WavesSeen = Storm->GetDeckWaves();
            if (!IsSpeaking() && Pending.Num() == 0 && Clock - StormSeen > 40.f) { Queue(Jeff, TEXT("Jeffries"), TEXT("JEF_X02"), .6f); StormSeen = Clock; }
        }
        if (Storm->GetIntense() > .02f && !Said.Contains(TEXT("JEF_X01")) && !IsSpeaking() && Pending.Num() == 0 && Step == EStep::Waiting)
            Say(Jeff, TEXT("Jeffries"), TEXT("JEF_X01"));
    }
    if (Chuck->GetRespawns() != Respawns) { Respawns = Chuck->GetRespawns(); bLeaping = false; bWasOnTop = false; }
}
