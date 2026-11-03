// The pirate ship's crew (Claude, 2026-11-02; docs/regions/SHIP.md, docs/SHIP.md milestone 3), after the 2D game's Phase 7:
// "pirates should have noticeably richer animation than previous NPCs ... rhythmic body movement synchronized with the
// soundtrack". Included at the end of DockNPC.cpp, in its namespace (bones, clips, Pitch/Yaw/Roll, HalfHeight, the tankard's
// numbers). AShipCrew (ShipCrew.cpp) spawns them, gives them their lines and the beat, and runs the fencing between two.
//
// Every performance is laid over a motion-capture idle (the drunk's sway, or a standing one) as on the docks, and keeps to the
// ship score's beat (136.12 a minute):
//   the concertina player squeezes his instrument open and shut over two bars, bobbing on the beat, nodding;
//   the cheering pirate sways, bobbing, his tankard at his side; every eight bars he raises it high and pumps it on the beat,
//     and every other time drinks from it instead;
//   the dancing pirate dances a jig: a hop on every beat, kicks and heel taps on alternate legs, a jump to close the phrase,
//     hands on his hips;
//   the fencers stand side-on in a crouch and fight in phrases of cuts, thrusts, feints, beats and binds (ShipCrew.cpp),
//     each blow landing on the other's blade, the sword moving on smooth curves between its positions, on feet that step
//     and stay put, the free arm thrown back for balance in a lunge;
//   Jeffries, tied to the main mast by the chest and the waist, his wrists bound behind him round it, heaves against the ropes;
//   the seated pirate sways on his stool at the mess table with his tankard (the tavern drinker's pose, DockNPC.cpp);
//   the chef chops at his work table on the beat until he sees the rat, then chases it with his cleaver (TickChef).
//   the captain (a halfling on tall platform boots, on the quarterdeck) stands with his fists on his hips, his cutlass planted
//     point-down in the deck at his right; every so often he pulls it out, holds it up and out to his side a while, and drives
//     it back into the planks (the user's two reference pictures: hands on hips, and the cutlass raised).
// While Chuck talks to one, the performance settles (it doesn't stop).
#include "ProceduralMeshComponent.h"

namespace
{
    constexpr float ScoreBeat = 60.f / 136.12f;   // s a beat (ShipCrew.h ShipScore)
    float Frac(float X) { return X - FMath::FloorToFloat(X); }
    float Ease01(float X) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f)); }
    // Up over [A0, A1], down over [B0, B1] (beats or seconds): a held lift.
    float Held(float U, float A0, float A1, float B0, float B1) { return Ease01((U - A0) / (A1 - A0)) * (1.f - Ease01((U - B0) / (B1 - B0))); }
    // The props (Tools/build_pirate_props.py; manifest blade_cm, face_cm, length_cm): every one has its origin in the fist,
    // +X along the grip toward the blade (the thumb's side), -Z toward the edge (the knuckles' side).
    constexpr float CutlassBase = 6.7f, CutlassTip = 70.7f, ConcertinaFace = 10.f, BellowsLength = 10.f, CleaverTip = 27.5f;
    // The chef: his jog clip's own speed (CHUCK_WALK_LOOP) and the pace he chases at (Chuck runs at 225 cm/s and sprints at
    // 380, ChuckClipData.h: a run gains on him slowly, a sprint quickly).
    constexpr float ChefJogClip = 273.4f, ChefRunSpeed = 205.f, ChefWalkSpeed = 110.f, ChefTurnRate = 320.f;
    constexpr float ChefNoticeRange = 700.f, ChefReach = 95.f, ChefHitRadius = 40.f, ChefGiveUp = 5.f, ChefLostWait = 2.5f;
    constexpr float ChefWindup = .38f, ChefStrike = .14f, ChefRecover = .55f, ChefHitPause = 1.4f;
    constexpr float FencerCrouch = 11.f;        // cm his hips are down in the stance
    constexpr float StepTime = .2f, StepLift = 7.f, StepSlack = 5.f;
    // The captain's round (s): fists on hips, then reach for the planted cutlass, pull it out, raise it, hold it up, bring it
    // back over its place, drive it in, let go, fists on hips again. (2026-11-02: a longer reach, the hand opening on the way.)
    constexpr float CaptainRound = 28.f, CapReach = 15.6f, CapPull = 16.7f, CapRaise = 17.1f, CapHold = 18.f, CapLower = 24.5f,
        CapPlunge = 25.4f, CapLetGo = 25.8f, CapBack = 26.6f;
    constexpr float CaptainPlunge = 4.f;         // cm the planted tip goes into the deck
    // The mouths on their models (SourceAssets/NPCs/Humans/manifest.json mouth_cm): where a drinker's tankard meets his lips.
    const FVector SeatedMouth(15.3f, 0.f, 156.8f), CheeringMouth(16.5f, 0.f, 163.7f);
}

UStaticMeshComponent* ADockNPC::AddPirateProp(const TCHAR* Path, const TCHAR* Name)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
    if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_PIRATE_PROP_MISSING %s %s"), *GetName(), Path); return nullptr; }
    auto* C = NewObject<UStaticMeshComponent>(this, Name);
    C->SetStaticMesh(Mesh);
    C->SetupAttachment(Body);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCanEverAffectNavigation(false);
    C->RegisterComponent();
    PirateProps.Add(C);
    return C;
}

void ADockNPC::PlaceProp(int32 Index, const FTransform& At)
{
    if (PirateProps.IsValidIndex(Index) && PirateProps[Index]) PirateProps[Index]->SetRelativeTransform(At);
}

void ADockNPC::SetPirateLines(const TArray<FString>& First, const TArray<FString>& Repeat)
{
    Lines = First; RepeatLines = Repeat;
}

void ADockNPC::Shout(const FString& Text, float Seconds)
{
    ShoutText = Text; ShoutUntil = Clock + Seconds;
}

bool ADockNPC::IsShouting() const { return ShoutUntil >= 0.f && Clock <= ShoutUntil; }

bool ADockNPC::GetFeet(FVector& Back, FVector& Front) const
{
    if (!bPlanted) return false;
    Back = Plant[0]; Front = Plant[1];
    return true;
}

bool ADockNPC::GetBlade(FVector& Base, FVector& Tip) const
{
    if (Kind != EDockHuman::PirateFencerA && Kind != EDockHuman::PirateFencerB) return false;
    Base = BladeBaseW; Tip = BladeTipW;
    return !Tip.IsZero();
}

const TCHAR* ADockNPC::GetCaptainPhase() const
{
    static const TCHAR* Names[] = { TEXT("Hips"), TEXT("Draw"), TEXT("Display"), TEXT("Return") };
    return Names[FMath::Clamp(CaptainPhase, 0, 3)];
}

const TCHAR* ADockNPC::GetChefStateName() const
{
    static const TCHAR* Names[] = { TEXT("Work"), TEXT("Notice"), TEXT("Chase"), TEXT("Swing"), TEXT("Lost"), TEXT("Return") };
    return Names[static_cast<int32>(Chef)];
}

void ADockNPC::SetupPirate()
{
    if (BoneIndex.Contains(INDEX_NONE)) return;
    // Standing ankles and feet (the dancer's and the fencers' legs are placed from them).
    TArray<FTransform> Standing;
    Solve(Rest, Standing);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        AnkleStand[Side] = Standing[BoneIndex[Of(FootL, Side)]].GetLocation();
        FootRest[Side] = Standing[BoneIndex[Of(FootL, Side)]].GetRotation();
    }
    AnkleRest = static_cast<float>(AnkleStand[0].Z);
    switch (Kind)
    {
    case EDockHuman::PirateConcertina:
        DisplayName = TEXT("Concertina Pirate"); IdleClip = ClipDrunkSway;
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_ConcertinaEnd.SM_ConcertinaEnd"), TEXT("ConcertinaLeft"));
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_ConcertinaEnd.SM_ConcertinaEnd"), TEXT("ConcertinaRight"));
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_ConcertinaBellows.SM_ConcertinaBellows"), TEXT("Bellows"));
        SetGrip(0, .45f); SetGrip(1, .45f);   // fingers on the buttons
        break;
    case EDockHuman::PirateCheering:
        DisplayName = TEXT("Cheering Pirate"); IdleClip = ClipDrunkSway; PirateMouth = CheeringMouth;
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_Tankard.SM_Tankard"), TEXT("CheerTankard"));
        SetGrip(1, 1.f);
        break;
    case EDockHuman::PirateDancer:
        DisplayName = TEXT("Dancing Pirate"); IdleClip = ClipStandHip;
        SetGrip(0, .85f); SetGrip(1, .85f);   // fists on his hips
        break;
    case EDockHuman::PirateFencerA: case EDockHuman::PirateFencerB:
        DisplayName = TEXT("Swordsman"); IdleClip = ClipStandLook;
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_Cutlass.SM_Cutlass"), TEXT("Cutlass"));
        SetGrip(1, 1.f); SetGrip(0, .55f);
        break;
    case EDockHuman::Jeffries:
        DisplayName = TEXT("Jeffries"); IdleClip = ClipStandLook;
        SetGrip(0, .7f); SetGrip(1, .7f);
        break;
    case EDockHuman::PirateChef:
        DisplayName = TEXT("Pirate Chef"); IdleClip = ClipStandLook;
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_Cleaver.SM_Cleaver"), TEXT("Cleaver"));
        SetGrip(1, 1.f); SetGrip(0, .6f);
        break;
    case EDockHuman::PirateSeated:
        DisplayName = TEXT("Seated Pirate"); IdleClip = ClipStandLook; PirateMouth = SeatedMouth;
        break;
    case EDockHuman::PirateCaptain:
    {
        DisplayName = TEXT("Pirate Captain"); IdleClip = ClipStandLook;
        AddPirateProp(TEXT("/Game/Characters/Humans/Props/SM_Cutlass.SM_Cutlass"), TEXT("CaptainCutlass"));
        SetGrip(0, .85f); SetGrip(1, 1.f);
        // Where his cutlass stands planted: before his right foot, clear of his hip, its grip low enough that his arm
        // reaches down and forward to it nearly straight (user 2026-11-02: the grip was by his hip, so his fist on the hip
        // and his fist on the grip were the same place and he never seemed to take it). The blade leans a little forward and
        // out, so the hilt leans back toward him; sized so its tip goes a few cm into the deck (a halfling's cutlass is shorter).
        const float Right = -ArmOut;
        const FVector S = Standing[BoneIndex[UpperR]].GetLocation();
        const float L = static_cast<float>(FVector::Dist(S, Standing[BoneIndex[LowerR]].GetLocation()) + FVector::Dist(Standing[BoneIndex[LowerR]].GetLocation(), Standing[BoneIndex[HandR]].GetLocation()));
        CaptainHilt = S + FVector(.58f * L, Right * .3f * L, -.72f * L);
        CaptainBlade = FVector(.1f, Right * .16f, -1.f).GetSafeNormal();
        CutlassScale = FMath::Clamp(static_cast<float>(CaptainHilt.Z + CaptainPlunge) / (static_cast<float>(-CaptainBlade.Z) * CutlassTip), .7f, 1.1f);
        CaptainTipDepth = -static_cast<float>(CaptainHilt.Z + CaptainBlade.Z * CutlassTip * CutlassScale);
        CaptainT = 4.f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CAPTAIN_READY arm_cm=%.1f hilt=%s cutlass_scale=%.2f tip_depth_cm=%.1f"), L, *CaptainHilt.ToString(), CutlassScale, CaptainTipDepth);
        break;
    }
    default: break;
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PIRATE_READY %s props=%d mocap=%d"), *DisplayName, PirateProps.Num(), HasMocap() ? 1 : 0);
}

void ADockNPC::SitAtTable(const FVector& Hips, float Yaw, float StoolTop, float TableTop, const FVector& LookAt)
{
    if (bSeated || BoneIndex.Contains(INDEX_NONE)) return;
    SetActorLocationAndRotation(Hips + FVector(0.f, 0.f, HalfHeight), FRotator(0.f, Yaw, 0.f));
    HomeYaw = Yaw; bTurning = false; TurnHold = 0.f;
    bSeated = true; SeatTop = StoolTop; SeatBone = WorkerSitBone;
    SetupSeat(LookAt);
    Glance = GlanceCentre;
    IdleClip = ClipStandLook;
    bDrinker = true; DrinkTableTop = TableTop;
    SetGrip(1, 1.f);
    Tankard = NewObject<UStaticMeshComponent>(this, TEXT("Tankard"));
    Tankard->SetStaticMesh(KeeperMeshes[0]);
    Tankard->SetupAttachment(Body);
    Tankard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tankard->SetCanEverAffectNavigation(false);
    Tankard->RegisterComponent();
    DrinkClock = FMath::FRandRange(DrinkDown, DrinkPeriod - 4.f);
}

void ADockNPC::TieToMast(const FVector& MastCentre, float MastRadius)
{
    MastLocal = GetActorTransform().InverseTransformPosition(MastCentre);
    MastLocal.Z = 0.;
    MastR = MastRadius;
    if (!Ropes)
    {
        Ropes = NewObject<UProceduralMeshComponent>(this, TEXT("Ropes"));
        Ropes->SetupAttachment(Body);
        Ropes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Ropes->bUseAsyncCooking = true;
        Ropes->RegisterComponent();
        if (auto* Hemp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Canvas.M_Canvas"))) Ropes->SetMaterial(0, Hemp);
    }
}

void ADockNPC::SetupChef(const FVector& WorkFeet, float WorkYaw, const FBox2D& Galley, float FloorZ)
{
    ChefHome = WorkFeet + FVector(0.f, 0.f, HalfHeight);
    ChefHomeYaw = WorkYaw; ChefBox = Galley; ChefFloor = FloorZ;
    SetActorLocationAndRotation(ChefHome, FRotator(0.f, WorkYaw, 0.f));
    HomeYaw = WorkYaw;
    Chef = EChef::Work; ChefTime = 0.f;
}

// ------------------------------------------------------------------ per frame

void ADockNPC::TickPirate(float DeltaSeconds)
{
    // The first lines until one conversation is talked through, then the repeat ones (the 2D game's *_first / *_repeat).
    if (bTalking) bInConversation = true;
    else if (bInConversation)
    {
        bInConversation = false; ++Conversations;
        if (RepeatLines.Num()) Lines = RepeatLines;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PIRATE_TALKED %s conversations=%d"), *DisplayName, Conversations);
    }
    if (ShoutUntil >= 0.f && Clock > ShoutUntil) ShoutUntil = -1.f;
    switch (Kind)
    {
    case EDockHuman::PirateChef: TickChef(DeltaSeconds); break;
    case EDockHuman::PirateCaptain: if (CaptainHold > 0.f) CaptainHold -= DeltaSeconds; else CaptainT = FMath::Fmod(CaptainT + DeltaSeconds, CaptainRound); break;
    case EDockHuman::PirateFencerA: case EDockHuman::PirateFencerB:
        if (bFenceCue)
            SetActorLocationAndRotation(Fence.Centre + FVector(0.f, 0.f, HalfHeight), FRotator(0.f, static_cast<float>(Fence.Facing.Rotation().Yaw), 0.f));
        break;
    case EDockHuman::Jeffries:
    {
        // Bursts of heaving at the ropes (three heaves each), more often with the rat near him; between them he sags and
        // twists, never still.
        float Near = 0.f;
        if (const APawn* Chuck = UGameplayStatics::GetPlayerPawn(this, 0)) Near = FMath::Clamp(1.f - static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(), GetActorLocation())) / 500.f, 0.f, 1.f);
        StrainT += DeltaSeconds;
        if ((NextStrain -= DeltaSeconds) <= 0.f) { StrainT = 0.f; NextStrain = FMath::Lerp(FMath::FRandRange(3.f, 6.f), FMath::FRandRange(1.6f, 2.6f), Near); }
        const float Heave = StrainT < 1.65f ? FMath::Max(0.f, FMath::Sin(StrainT / .55f * PI)) : 0.f;
        Strain = FMath::Clamp(.18f + .1f * FMath::Sin(Clock * 1.3f) + .75f * Heave, 0.f, 1.f);
        break;
    }
    default: break;
    }
}

bool ADockNPC::ChefStep(const FVector& Direction, float Distance)
{
    // As the zombie: blocked by the galley's walls and furniture (a capsule clear of the floor), only onto floor, and he
    // keeps to the galley.
    if (Distance <= 0.f) return false;
    const FVector From = GetActorLocation(), To = From + Direction.GetSafeNormal2D() * Distance;
    if (!ChefBox.IsInside(FVector2D(To.X, To.Y))) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChefStep), false, this);
    const FCollisionObjectQueryParams Static(ECC_WorldStatic);
    FHitResult Hit;
    if (GetWorld()->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, Static, FCollisionShape::MakeCapsule(24.f, HalfHeight - 18.f), Query)) return false;
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByObjectType(Floor, To - FVector(0, 0, HalfHeight - 30.f), To - FVector(0, 0, HalfHeight + 30.f), Static, Query)) return false;
    SetActorLocation(FVector(To.X, To.Y, Floor.ImpactPoint.Z + HalfHeight));
    return true;
}

bool ADockNPC::ChefCanReach(const AChuckCharacter* Chuck) const
{
    // In the galley, no higher than a counter, and nothing over him a man couldn't reach past: under the work table or the
    // counter he's safe (the 2D galley's lanes: an escape, not a fight).
    if (!Chuck || Chuck->IsAstral()) return false;
    const FVector C = Chuck->GetActorLocation();
    if (!ChefBox.IsInside(FVector2D(C.X, C.Y)) || C.Z - ChefFloor > 140.f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChefReach), false, this);
    Query.AddIgnoredActor(Chuck);
    FHitResult Over;
    return !GetWorld()->LineTraceSingleByObjectType(Over, C, C + FVector(0, 0, 110.f), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

void ADockNPC::TickChef(float DeltaSeconds)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    ChefTime += DeltaSeconds;
    const FVector Me = GetActorLocation();
    const bool bInGalley = Chuck && !Chuck->IsAstral() && ChefBox.ExpandBy(10.f).IsInside(FVector2D(Chuck->GetActorLocation().X, Chuck->GetActorLocation().Y))
        && FMath::Abs(Chuck->GetActorLocation().Z - ChefFloor) < 200.f;
    const bool bReach = ChefCanReach(Chuck);
    const float Dist = Chuck ? static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(), Me)) : 1e6f;
    // A death (a respawn) re-arms his line, as the 2D galley's notice re-arms.
    bool bRespawned = false;
    if (Chuck && Chuck->GetRespawns() != ChefRespawnsSeen) { ChefRespawnsSeen = Chuck->GetRespawns(); bChefSaidLine = false; bRespawned = true; }
    // A rat that has just appeared in the galley (a respawn or a checkpoint) is let be until he moves, so arriving there
    // never costs Sanity before the player can act.
    if (Chuck)
    {
        const FVector Rat = Chuck->GetActorLocation();
        if (bRespawned || FVector::Dist(Rat, ChefRatLast) > 150.f) { ChefRatArrival = Rat; bChefRatStill = true; }
        else if (bChefRatStill && FVector::Dist2D(Rat, ChefRatArrival) > 50.f) bChefRatStill = false;
        ChefRatLast = Rat;
    }
    const auto Sees = [&]()
    {
        if (!bInGalley || Dist > ChefNoticeRange || bChefRatStill) return false;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ChefSees), false, this);
        Query.AddIgnoredActor(Chuck);
        FHitResult Hit;
        return !GetWorld()->LineTraceSingleByObjectType(Hit, Me + FVector(0, 0, 70.f), Chuck->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
    };
    const auto Move = [&](const FVector& Toward, float Speed)
    {
        // Straight at it, or round whatever's in the way (the first of these turns that's free).
        FVector Dir = (Toward - GetActorLocation()).GetSafeNormal2D();
        ChefSpeed = 0.f;
        if (Dir.IsNearlyZero()) return;
        for (const float Turn : { 0.f, 35.f, -35.f, 70.f, -70.f, 105.f, -105.f })
        {
            const FVector Try = Dir.RotateAngleAxis(Turn, FVector::UpVector);
            if (ChefStep(Try, Speed * DeltaSeconds)) { ChefSpeed = Speed; TurnZombie(GetActorLocation() + Try * 100.f, DeltaSeconds, ChefTurnRate); return; }
        }
        TurnZombie(Toward, DeltaSeconds, ChefTurnRate);
    };
    switch (Chef)
    {
    case EChef::Work:
        ChefSpeed = 0.f;
        if (Sees()) { Chef = EChef::Notice; ChefTime = 0.f; ++ChefNotices;
            if (!bChefSaidLine) { Shout(TEXT("Damn! Another rat got in, come er' you little bugger!"), 3.6f); bChefSaidLine = true; }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_CHEF_NOTICE at_cm=%.0f"), Dist); }
        break;
    case EChef::Notice:
        ChefSpeed = 0.f;
        if (Chuck) TurnZombie(Chuck->GetActorLocation(), DeltaSeconds, ChefTurnRate * .6f);
        if (ChefTime > 1.f) { Chef = EChef::Chase; ChefTime = 0.f; UnreachableFor = 0.f; }
        break;
    case EChef::Chase:
        if (!bInGalley) { Chef = EChef::Lost; ChefTime = 0.f; break; }
        UnreachableFor = bReach ? 0.f : UnreachableFor + DeltaSeconds;
        if (UnreachableFor > ChefGiveUp) { Chef = EChef::Lost; ChefTime = 0.f; break; }
        if (bReach && Dist < ChefReach) { Chef = EChef::Swing; ChefTime = 0.f; bChefStruck = false; ++ChefSwings; ChefAim = Chuck->GetActorLocation(); break; }
        // Up to the rat (or as near as the furniture lets him, bending to look under it).
        Move(Chuck->GetActorLocation(), FMath::Min(ChefRunSpeed, ChefWalkSpeed + 400.f * ChefTime));
        if (Dist < ChefReach * .7f) ChefSpeed = 0.f;
        break;
    case EChef::Swing:
    {
        ChefSpeed = 0.f;
        if (ChefTime < ChefWindup && Chuck) { TurnZombie(Chuck->GetActorLocation(), DeltaSeconds, ChefTurnRate); ChefAim = Chuck->GetActorLocation(); }
        if (!bChefStruck && ChefTime >= ChefWindup + ChefStrike)
        {
            bChefStruck = true;
            // The cleaver comes down where he aimed: the rat, if he's still there and still reachable.
            const FVector Edge = Me + GetActorForwardVector() * 70.f;
            if (Chuck && bReach && FVector::Dist2D(Chuck->GetActorLocation(), ChefAim) < ChefHitRadius + 20.f && FVector::Dist2D(Chuck->GetActorLocation(), Edge) < ChefReach)
                if (Chuck->TakeBite(Me, 1)) { ++ChefHits; bChefHitPause = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_CHEF_HIT sanity=%d"), Chuck->GetSanity()); }
        }
        // (After a blow that lands he stands over it a moment longer: a running rat gets away.)
        if (ChefTime >= ChefWindup + ChefStrike + ChefRecover + (bChefHitPause ? ChefHitPause : 0.f)) { Chef = EChef::Chase; ChefTime = 0.f; bChefHitPause = false; }
        break;
    }
    case EChef::Lost:
        // At the edge of his galley, shaking the cleaver after the rat; then back to work.
        ChefSpeed = 0.f;
        if (bInGalley && bReach && Sees()) { Chef = EChef::Chase; ChefTime = 0.f; UnreachableFor = 0.f; break; }
        if (ChefTime > ChefLostWait) { Chef = EChef::Return; ChefTime = 0.f; }
        break;
    case EChef::Return:
        if (Sees() && bReach) { Chef = EChef::Chase; ChefTime = 0.f; UnreachableFor = 0.f; break; }
        if (FVector::Dist2D(Me, ChefHome) < 12.f)
        {
            ChefSpeed = 0.f;
            const float Current = static_cast<float>(GetActorRotation().Yaw);
            const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, ChefHomeYaw), -ChefTurnRate * DeltaSeconds, ChefTurnRate * DeltaSeconds);
            SetActorRotation(FRotator(0.f, Current + Step, 0.f));
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(Current, ChefHomeYaw)) < 3.f) { Chef = EChef::Work; ChefTime = 0.f; }
        }
        else Move(ChefHome, ChefWalkSpeed);
        break;
    }
    // The jog's stride paced to his speed; blended in as he gets going.
    JogBlend = FMath::FInterpTo(JogBlend, ChefSpeed > 30.f ? 1.f : 0.f, DeltaSeconds, 6.f);
    JogTime += DeltaSeconds * ChefSpeed / FMath::Max(ChefJogClip * HipScale, 1.f);   // the clip covers its own speed at rate 1
    ChopClock += DeltaSeconds;
}

// ------------------------------------------------------------------ the pose

void ADockNPC::PirateDelta(TArray<FQuat>& Delta, TArray<FQuat>& BoneDelta, FVector& HipsOffset)
{
    const float B = Beat, F = Frac(B);
    const float Dip = .5f + .5f * FMath::Cos(2.f * PI * F);   // 1 on the beat
    const float Perf = 1.f - .7f * TalkBlend;                 // settling while the rat talks to him
    const auto Damp = [&](float W)
    {
        for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, W);
        HipsOffset *= W;
    };
    PirateDip = 0.f;
    switch (Kind)
    {
    case EDockHuman::PirateConcertina:
    {
        Damp(.45f);
        PirateDip = 2.5f * Dip * Perf; HipsOffset.Z -= PirateDip;
        const float Squeeze = FMath::Sin(2.f * PI * B / 8.f);
        Delta[Spine2] = Roll(2.5f * Squeeze * Perf);
        Delta[Chest] = Roll(3.5f * Squeeze * Perf) * Yaw(2.f * FMath::Sin(2.f * PI * B / 4.f) * Perf) * Pitch(3.f);
        Delta[Head] = Pitch(4.f * Dip * Perf) * Roll(3.f * FMath::Sin(2.f * PI * B / 8.f + .6f) * Perf) * Delta[Head];
        break;
    }
    case EDockHuman::PirateCheering:
        Damp(.85f);
        PirateDip = 2.f * Dip * Perf; HipsOffset.Z -= PirateDip;
        Delta[Chest] = Pitch(-5.f * ToastLift) * Delta[Chest];
        Delta[Head] = Pitch(4.f * Dip * Perf - 14.f * ToastLift) * Delta[Head];
        break;
    case EDockHuman::PirateDancer:
    {
        Damp(.25f);
        // The jig's hop: up and down on every beat, higher for the jump that closes the phrase (PosePirate places the legs).
        const int32 K = FMath::FloorToInt(B) & 7;
        const float Hop = (K == 7 ? 11.f : 6.f) * FMath::Sin(PI * F) * Perf;
        HipsOffset = FVector(0.f, 0.f, Hop);
        const float Lead = (K == 0 || K == 1 || K == 4 || K == 5) ? 1.f : -1.f;   // which leg is out: his right, or his left
        Delta[Pelvis] = Yaw(-5.f * Lead * Perf);
        Delta[Spine2] = Yaw(4.f * Lead * Perf);
        Delta[Chest] = Yaw(5.f * Lead * Perf) * Pitch(-2.f + 2.f * Dip);
        Delta[Head] = Pitch(3.f * Dip * Perf) * Delta[Head];
        break;
    }
    case EDockHuman::PirateFencerA: case EDockHuman::PirateFencerB:
    {
        Damp(.3f);
        // Down in his stance, bouncing lightly on the beat while on guard; lower in a lunge or a low parry; the hips carried
        // forward over the front knee in a lunge, back when he leans away from a blade.
        const float Guarded = 1.f - FMath::Clamp(Fence.Lunge + Fence.Crouch, 0.f, 1.f);
        HipsOffset.Z -= FencerCrouch + 8.f * Fence.Lunge + 9.f * Fence.Crouch + 1.8f * Dip * Guarded;
        HipsOffset.X += 13.f * Fence.Lunge - 10.f * Fence.Lean;
        // Side-on, sword shoulder forward; the shoulders wound back for a cut and driven through it; leaning into a lunge or
        // away from a blade; the head turned back to his man throughout.
        const float Tw = Fence.Twist;
        Delta[Pelvis] = Yaw(-22.f + 6.f * Tw);
        Delta[Spine1] = Pitch(3.f + 7.f * Fence.Lunge - 9.f * Fence.Lean + 3.f * Fence.Crouch) * Yaw(3.f * Tw);
        Delta[Spine2] = Yaw(-8.f + 9.f * Tw) * Pitch(3.f + 6.f * Fence.Lunge - 7.f * Fence.Lean);
        Delta[Chest] = Yaw(-6.f + 11.f * Tw) * Roll(-3.f * Fence.Lean);
        Delta[Neck] = Yaw(14.f - 5.f * Tw) * Pitch(-3.f * Fence.Lunge + 4.f * Fence.Lean) * Delta[Neck];
        Delta[Head] = Yaw(18.f - 12.f * Tw) * Pitch(-4.f * Fence.Lunge) * Delta[Head];
        break;
    }
    case EDockHuman::Jeffries:
    {
        Damp(.3f);
        const float Twist = FMath::Sin(Clock * 2.3f) * (.4f + .6f * Strain);
        const float Thrash = FMath::Sin(Clock * 7.1f) * FMath::Max(0.f, Strain - .3f);
        Delta[Spine2] = Pitch(5.f * Strain) * Yaw(7.f * Twist);
        Delta[Chest] = Pitch(7.f * Strain) * Roll(5.f * Twist);
        Delta[Neck] = Pitch(8.f * Strain) * Delta[Neck];
        Delta[Head] = Yaw(16.f * Thrash) * Pitch(-10.f * Strain) * Delta[Head];
        break;
    }
    case EDockHuman::PirateSeated:
        // Swaying on his stool with the beat, side to side over two beats, nodding.
        Delta[Spine1] = Roll(3.f * FMath::Sin(PI * B) * Perf) * Delta[Spine1];
        Delta[Chest] = Roll(2.f * FMath::Sin(PI * B - .5f) * Perf) * Delta[Chest];
        Delta[Head] = Pitch(3.f * Dip * Perf) * Delta[Head];
        break;
    case EDockHuman::PirateChef:
    {
        // Bent over the work, chopping; leaning into the run; up and over into the blow at the rat.
        const float Work = (Chef == EChef::Work || (Chef == EChef::Return && JogBlend < .5f)) ? 1.f : 0.f;
        float Wind = 0.f, Down = 0.f;
        if (Chef == EChef::Swing)
        {
            Wind = Ease01(ChefTime / ChefWindup) * (1.f - Ease01((ChefTime - ChefWindup) / ChefStrike));
            Down = Ease01((ChefTime - ChefWindup) / ChefStrike) * (1.f - Ease01((ChefTime - ChefWindup - ChefStrike - .15f) / (ChefRecover - .15f)));
        }
        SwingDrive = Down;
        Delta[Spine1] = Pitch(4.f * JogBlend + 12.f * Down);
        Delta[Spine2] = Pitch(8.f * Work + 4.f * JogBlend - 8.f * Wind + 16.f * Down) * Yaw(6.f * Wind);
        Delta[Chest] = Pitch(4.f * Work - 4.f * Wind + 10.f * Down);
        HipsOffset.Z -= 14.f * Down;
        PirateDip = 14.f * Down;
        break;
    }
    default: break;
    }
}

void ADockNPC::KeepFeetOn(TArray<FTransform>& Space, const FVector Ankles[2], const FQuat Feet[2], const FVector& KneeAhead)
{
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Count = Space.Num();
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const int32 Thigh = BoneIndex[Of(ThighL, Side)], Calf = BoneIndex[Of(CalfL, Side)], Foot = BoneIndex[Of(FootL, Side)];
        const float Out = Side == 0 ? ArmOut : -ArmOut;
        TwoBone(Space, Thigh, Calf, Foot, Ankles[Side], KneeAhead + FVector(0.f, Out * .25f, 0.f));
        TArray<FTransform> Local; Local.SetNum(Count);
        for (int32 Bn = Foot + 1; Bn < Count; ++Bn) Local[Bn] = Space[Bn].GetRelativeTransform(Space[Ref.GetParentIndex(Bn)]);
        Space[Foot].SetRotation(Feet[Side]);
        for (int32 Bn = Foot + 1; Bn < Count; ++Bn)
            for (int32 P = Ref.GetParentIndex(Bn); P != INDEX_NONE; P = Ref.GetParentIndex(P))
                if (P == Foot) { Space[Bn] = Local[Bn] * Space[Ref.GetParentIndex(Bn)]; break; }
        if (Clock > 6.f) WorstFootSink = FMath::Max(WorstFootSink, AnkleRest - 1.5f - static_cast<float>(Space[Foot].GetLocation().Z));
    }
}

void ADockNPC::HoldTankard(TArray<FTransform>& Space, int32 Side, const FVector& Handle, const FVector& Along, const FVector& Up, const FVector& Pole)
{
    // The fist on the handle, knuckles toward the tankard's body, thumb along its up (as PoseDrink).
    const FVector A = FVector::VectorPlaneProject(Along, Up).GetSafeNormal();
    PlaceHand(Space, Side, Handle, A, Up, Pole);
    FVector F, FA, FT;
    HandFrame(Space, Side, F, FA, FT);
    if (Clock > 6.f) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(F, Handle)));
    PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(A, Up).ToQuat(), F));
}

void ADockNPC::PosePirate(TArray<FTransform>& Space, float DeltaSeconds)
{
    const float B = Beat, F = Frac(B);
    const float Dip = .5f + .5f * FMath::Cos(2.f * PI * F);
    const float Perf = 1.f - .7f * TalkBlend;
    const auto OutOf = [this](int32 Side) { return Side == 0 ? ArmOut : -ArmOut; };   // +Y is out on this side
    const float Right = -ArmOut;   // which way is his right (+/-Y)
    const auto Settle = [&](float Dipped)
    {
        // The feet where the clip had them before the hips dipped: the knees take the dip.
        FVector Ankles[2]; FQuat Feet[2];
        for (int32 Side = 0; Side < 2; ++Side)
        {
            Ankles[Side] = Space[BoneIndex[Of(FootL, Side)]].GetLocation() + FVector(0.f, 0.f, Dipped);
            Feet[Side] = Space[BoneIndex[Of(FootL, Side)]].GetRotation();
        }
        if (Dipped > .05f) KeepFeetOn(Space, Ankles, Feet, FVector(1.f, 0.f, 0.f));
    };
    const FVector Shoulder[2] = { Space[BoneIndex[UpperL]].GetLocation(), Space[BoneIndex[UpperR]].GetLocation() };
    switch (Kind)
    {
    case EDockHuman::PirateConcertina:
    {
        Settle(PirateDip);
        // The concertina across his front at the chest, squeezed open and shut over two bars (with a small push on each beat),
        // tipping as it opens.
        const float Squeeze = FMath::Sin(2.f * PI * B / 8.f);
        BellowsGap = (34.f + 12.f * Squeeze - 2.f * Dip) * FMath::Lerp(.75f, 1.f, Perf);
        const float Tip = FMath::DegreesToRadians(9.f * FMath::Cos(2.f * PI * B / 8.f) * Perf);
        const FVector Axis = FVector(0.f, FMath::Cos(Tip), FMath::Sin(Tip)) * Right;   // from his left hand to his right
        const FVector Centre((Shoulder[0] + Shoulder[1]) * .5f + FVector(31.f, 0.f, -24.f));
        const FVector Fist[2] = { Centre - Axis * (BellowsGap * .5f + ConcertinaFace), Centre + Axis * (BellowsGap * .5f + ConcertinaFace) };
        const FVector Along = FVector::VectorPlaneProject(FVector(.25f, 0.f, 1.f), Axis).GetSafeNormal();   // knuckles up, over the buttons
        const FVector Thumb = FVector::VectorPlaneProject(FVector(1.f, 0.f, -.2f), Along).GetSafeNormal();
        for (int32 Side = 0; Side < 2; ++Side)
            PlaceHand(Space, Side, Fist[Side], Along, Thumb, FVector(-.3f, OutOf(Side), -.7f));
        FVector P[2], A, T;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            HandFrame(Space, Side, P[Side], A, T);
            if (Clock > 6.f) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(P[Side], Fist[Side])));
        }
        // Each end on its fist, facing the other; the bellows between their faces.
        const FVector Across = (P[1] - P[0]).GetSafeNormal();
        const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Across).GetSafeNormal();
        PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(Across, Up).ToQuat(), P[0]));
        PlaceProp(1, FTransform(FRotationMatrix::MakeFromXZ(-Across, Up).ToQuat(), P[1]));
        const float Gap = FMath::Max(2.f, static_cast<float>(FVector::Dist(P[0], P[1])) - 2.f * ConcertinaFace);
        PlaceProp(2, FTransform(FRotationMatrix::MakeFromXZ(Across, Up).ToQuat(), P[0] + Across * ConcertinaFace, FVector(Gap / BellowsLength, 1.f, 1.f)));
        break;
    }
    case EDockHuman::PirateCheering:
    {
        Settle(PirateDip);
        // Every eight bars: a toast (raised high, pumped on the beat), or every other time a long drink.
        const int32 Cycle = FMath::FloorToInt(B / 32.f);
        const float U = (B / 32.f - Cycle) * 32.f;   // beats into the cycle
        const bool bDrink = (Cycle & 1) != 0;
        const float Lift = (bDrink ? Held(U, 0.f, 2.5f, 6.f, 8.f) : Held(U, 0.f, 2.f, 7.f, 9.f)) * Perf;
        if (Cycle != ToastCycle && U > 1.f) ToastCycle = Cycle;
        ToastLift = bDrink ? 0.f : Lift;
        const FVector RestGrip = Shoulder[1] + FVector(22.f, Right * 6.f, -50.f);
        const FVector RestAlong(1.f, Right * .3f, 0.f);
        if (!bDrink)
        {
            const FVector HeadAt = Space[BoneIndex[Head]].GetLocation();
            const FVector High = FVector(Shoulder[1].X + 12.f, Shoulder[1].Y + Right * 14.f, HeadAt.Z + 17.f + 5.f * Dip * Lift);
            const FVector Raised = FMath::Lerp(RestGrip, High, Lift);
            const FVector Up = FMath::Lerp(FVector::UpVector, FVector(.25f, Right * .1f, 1.f), Lift).GetSafeNormal();
            HoldTankard(Space, 1, Raised, FMath::Lerp(RestAlong, FVector(.4f, Right * .2f, 0.f), Lift), Up, FVector(-.3f, Right, FMath::Lerp(-.7f, .2f, Lift)));
        }
        else
        {
            // To his lips and tipped (the tavern drinker's numbers, standing).
            const FVector Lips = Space[BoneIndex[Head]].TransformPosition(HeadRef.InverseTransformPosition(PirateMouth));
            const float TipDeg = FMath::Lerp(DrinkTipStart, DrinkTipEnd, Ease01((U - 2.5f) / 3.5f));
            const float Tip = FMath::DegreesToRadians(TipDeg);
            const FVector DrinkUp(-FMath::Sin(Tip), 0.f, FMath::Cos(Tip));
            const FVector DrinkAlong(0.f, -Right, 0.f);
            const FVector RimLow = FVector::VectorPlaneProject(-FVector::UpVector, DrinkUp).GetSafeNormal();
            const FVector Contact = Lips + FVector(.6f, 0.f, -.8f);
            const FVector DrinkGrip = Contact - DrinkAlong * TankardBody - DrinkUp * TankardRimHeight - RimLow * TankardRim;
            const FVector Up = FMath::Lerp(FVector::UpVector, DrinkUp, Lift).GetSafeNormal();
            HoldTankard(Space, 1, FMath::Lerp(RestGrip, DrinkGrip, Lift), FMath::Lerp(RestAlong, DrinkAlong, Lift), Up, FVector(-.3f, Right, FMath::Lerp(-.7f, -.15f, Lift)));
        }
        break;
    }
    case EDockHuman::PirateDancer:
    {
        // The jig over a phrase of eight beats: kick out on his right and back, his left and back; a heel tapped out on his
        // right and back, his left and back; then both feet off the deck in a jump. Every beat he hops on the foot he stands on.
        const int32 K = FMath::FloorToInt(B) & 7;
        const float Hop = (K == 7 ? 11.f : 6.f) * FMath::Sin(PI * F) * Perf;
        const int32 Free = (K == 0 || K == 1 || K == 4 || K == 5) ? 1 : 0;   // the leg doing the step (1: his right)
        const bool bHeel = K >= 4 && K <= 6;
        const float T = ((K & 1) + F) * .5f;   // through the two beats of this step: out by .4, held, home by .95
        const float Out = Held(T, 0.f, .4f, .55f, .95f) * Perf;
        FVector Ankles[2]; FQuat Feet[2];
        for (int32 Side = 0; Side < 2; ++Side)
        {
            FVector A = AnkleStand[Side];
            float Toe = 0.f;
            if (K == 7) { A.Z += Hop * .8f; A.Y = FMath::Lerp(A.Y, A.Y * .45f, FMath::Sin(PI * F) * Perf); Toe = 10.f * FMath::Sin(PI * F); }   // the jump: heels clicked
            else if (Side == Free)
            {
                const FVector Kick = AnkleStand[Side] + (bHeel ? FVector(24.f, OutOf(Side) * 9.f, 2.f) : FVector(30.f, 0.f, 20.f));
                A = FMath::Lerp(AnkleStand[Side], Kick, Out) + FVector(0.f, 0.f, Hop * (1.f - Out) * .3f);
                Toe = bHeel ? -32.f * Out : 38.f * Out;   // heel down, toes up; or toes pointed
            }
            else { A.Z += Hop * .55f; Toe = 12.f * FMath::Sin(PI * F) * Perf; }   // the hop leaves the deck from the ball of his foot
            Ankles[Side] = A; Feet[Side] = Pitch(Toe) * FootRest[Side];
        }
        KeepFeetOn(Space, Ankles, Feet, FVector(1.f, 0.f, 0.f));
        DanceFeet = FVector2D(Ankles[0].Z - AnkleRest, Ankles[1].Z - AnkleRest);
        // Heard on the planking (2026-11-03): each beat his hop lands, the jump's landing both feet hard, the heel-taps light.
        {
            const int32 Whole = FMath::FloorToInt(B);
            if (Whole != DanceBeatHeard && Perf > .5f)
            {
                const bool bFirst = DanceBeatHeard == MIN_int32;
                DanceBeatHeard = Whole;
                const FVector Deck = Body->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, AnkleRest));
                if (!bFirst)
                {
                    if ((Whole & 7) == 0) ShipSound(this, TEXT("SFX_DeckStamp"), 4, Deck, .8f);
                    else ShipSound(this, TEXT("SFX_DeckStep"), 9, Deck, (Whole & 7) >= 4 ? .4f : .55f);
                }
            }
        }
        // Fists on his hips, elbows out.
        const FVector Hips = Space[BoneIndex[Pelvis]].GetLocation();
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector Fist = Hips + FVector(-3.f, OutOf(Side) * 20.f, 9.f);
            const FVector Along = FVector(.15f, -OutOf(Side) * .55f, -.8f).GetSafeNormal();
            const FVector Thumb = FVector::VectorPlaneProject(FVector(1.f, 0.f, .3f), Along).GetSafeNormal();
            PlaceHand(Space, Side, Fist, Along, Thumb, FVector(-.5f, OutOf(Side), .3f));
        }
        break;
    }
    case EDockHuman::PirateFencerA: case EDockHuman::PirateFencerB:
    {
        if (!bFenceCue) break;
        // The feet: his right ahead toward his man, his left behind and turned out; each planted on the deck and stepped
        // (one at a time, quickly) when the stance has moved off it. A lunge throws the front foot well forward.
        const FTransform ToWorld = Body->GetComponentTransform();
        const FVector Want[2] = {
            FVector(-24.f - 7.f * Fence.Lunge, ArmOut * 12.f, AnkleRest),                                  // his left, behind
            FVector(26.f + Fence.Stride + 34.f * Fence.Lunge, -ArmOut * 8.f, AnkleRest) };                  // his right, ahead
        constexpr float FenceStepTime = .15f;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector W = ToWorld.TransformPosition(Want[Side]);
            if (!bPlanted) { Plant[Side] = W; StepT[Side] = -1.f; continue; }
            if (StepT[Side] < 0.f && StepT[1 - Side] < 0.f && FVector::Dist2D(Plant[Side], W) > StepSlack) { StepFrom[Side] = Plant[Side]; StepT[Side] = 0.f; }
            if (StepT[Side] >= 0.f)
            {
                StepT[Side] = FMath::Min(1.f, StepT[Side] + DeltaSeconds / FenceStepTime);
                StepTo[Side] = W;
                const float E = Ease01(StepT[Side]);
                Plant[Side] = FMath::Lerp(StepFrom[Side], StepTo[Side], E) + FVector(0.f, 0.f, StepLift * .7f * FMath::Sin(PI * StepT[Side]));
                if (StepT[Side] >= 1.f)
                {
                    Plant[Side] = StepTo[Side]; StepT[Side] = -1.f;
                    // His boot down on the planking (2026-11-03): a step heard by how far it went (a shuffle of a few cm is
                    // silent); a lunge's front foot lands hard.
                    const float Went = static_cast<float>(FVector::Dist2D(StepFrom[Side], Plant[Side]));
                    if (Side == 1 && Fence.Lunge > .6f && Went > 20.f) ShipSound(this, TEXT("SFX_DeckStamp"), 4, Plant[Side], .55f);
                    else if (Went > 16.f) ShipSound(this, TEXT("SFX_DeckStep"), 9, Plant[Side], FMath::GetMappedRangeValueClamped(FVector2f(16.f, 45.f), FVector2f(.16f, .34f), Went));
                }
            }
        }
        bPlanted = true;
        FVector Ankles[2]; FQuat Feet[2];
        for (int32 Side = 0; Side < 2; ++Side)
        {
            Ankles[Side] = ToWorld.InverseTransformPosition(Plant[Side]);
            Feet[Side] = Side == 0 ? Yaw(ArmOut * 70.f) * FootRest[0] : FootRest[1];
        }
        KeepFeetOn(Space, Ankles, Feet, FVector(1.f, 0.f, .2f));
        // The sword (2026-11-03): each key of his phrase is a fist, a blade, the edge's way and where the elbow points, all in
        // his own frame (X toward his man, Z up); between keys the four run on a cubic, stopping where he stops (a parry held,
        // a blow landing) and sweeping on through a wind-up. Blows are aimed onto his man's blade where it is, so steel meets
        // steel; parries are where the blow will come.
        const FVector S = Shoulder[1];
        const float ArmReach = static_cast<float>(FVector::Dist(S, Space[BoneIndex[LowerR]].GetLocation()) + FVector::Dist(Space[BoneIndex[LowerR]].GetLocation(), Space[BoneIndex[HandR]].GetLocation()));
        const float SwordReach = ArmReach * .97f;
        const FVector Target = ToWorld.InverseTransformPosition(Fence.Target);
        const FVector HeadAt = ToWorld.InverseTransformPosition(Fence.Head);
        const bool bPartner = !Fence.PartnerTip.IsZero();
        const FVector PB = bPartner ? ToWorld.InverseTransformPosition(Fence.PartnerBase) : Target;
        const FVector PT = bPartner ? ToWorld.InverseTransformPosition(Fence.PartnerTip) : Target;
        const float R = Right;
        const auto Dir = [](float X, float Y, float Z) { return FVector(X, Y, Z).GetSafeNormal(); };
        // The edge's way: Hint square to the blade (the way the edge faces: down a blow's path, toward a blow it parries).
        const auto EdgeOf = [](const FVector& Hint, const FVector& Blade)
        {
            const FVector E = FVector::VectorPlaneProject(Hint, Blade);
            return E.SizeSquared() > 1e-4f ? E.GetSafeNormal() : FVector::VectorPlaneProject(FVector(0.f, 0.f, -1.f), Blade).GetSafeNormal();
        };
        // A blow along Way that lands on his man's blade: the point where they cross is as near his man's hilt as his arm
        // allows, on his own blade's last third; a hair to his own side of it, so the steel touches rather than passes.
        const auto OnBlade = [&](const FVector& Way, const FVector& Aim)
        {
            const FVector W = Way.GetSafeNormal();
            if (bPartner)
                for (float Fr = .14f; Fr <= .9f; Fr += .06f)
                {
                    FVector C = PB + (PT - PB) * Fr;
                    FVector N = FVector::CrossProduct(W, (PT - PB).GetSafeNormal());
                    if (N.SizeSquared() > 1e-4f) { N.Normalize(); if (FVector::DotProduct(N, S - C) < 0.f) N = -N; C += N * 1.2f; }
                    for (const float G : { .7f, .6f, .8f, .5f })
                    {
                        const FVector Fist = C - W * (CutlassBase + G * (CutlassTip - CutlassBase));
                        if (FVector::Dist(S, Fist) > SwordReach) continue;
                        // (clear of his man's head: the blade beyond the crossing doesn't run on through it)
                        const FVector Closest = FMath::ClosestPointOnSegment(HeadAt, Fist + W * CutlassBase, Fist + W * CutlassTip);
                        if (FVector::Dist(Closest, HeadAt) < 16.f) continue;
                        return Fist;
                    }
                }
            // Out of reach: the crossing that needs the least more reach, the blow's line kept and drawn back toward him by
            // what it lacks (it falls a little short of the steel rather than swinging off line at his man's head).
            FVector Best = S + W * SwordReach;
            float Lack = 1e9f;
            if (bPartner)
                for (float Fr = .22f; Fr <= .9f; Fr += .06f)
                    for (const float G : { .7f, .5f, .35f })
                    {
                        const FVector Fist = PB + (PT - PB) * Fr - W * (CutlassBase + G * (CutlassTip - CutlassBase));
                        const float L = static_cast<float>(FVector::Dist(S, Fist)) - SwordReach;
                        if (L < Lack) { Lack = L; Best = Fist; }
                    }
            else Best = Aim - W * 45.f;
            return S + (Best - S).GetClampedToMaxSize(SwordReach);
        };
        struct FSword { FVector Fist, Blade, Edge, Pole; };
        const float Search = Clock * 2.3f;   // the guard's point, never quite still, feeling for an opening
        const auto PoseOf = [&](EFencePose Pose) -> FSword
        {
            FSword K;
            switch (Pose)
            {
            case EFencePose::ChamberHigh:   // the hand up by his right ear, the blade back over his shoulder
                K.Fist = S + FVector(4.f, R * 10.f, 22.f); K.Blade = Dir(-.55f, R * .25f, .8f); K.Edge = EdgeOf(FVector(.8f, 0.f, .6f), K.Blade); K.Pole = Dir(-.1f, R, -.15f); break;
            case EFencePose::SwingHigh:     // coming over: the blade upright, the hand rising forward
                K.Fist = S + FVector(22.f, R * 6.f, 22.f); K.Blade = Dir(.1f, R * .12f, 1.f); K.Edge = EdgeOf(FVector(1.f, 0.f, 0.f), K.Blade); K.Pole = Dir(-.2f, R * .8f, -.5f); break;
            case EFencePose::CutHead:       // down at his head
                K.Blade = (((HeadAt - S).GetSafeNormal() + FVector(0.f, 0.f, -.3f))).GetSafeNormal(); K.Fist = OnBlade(K.Blade, HeadAt);
                K.Edge = EdgeOf(FVector(0.f, 0.f, -1.f), K.Blade); K.Pole = Dir(-.3f, R * .5f, -1.f); break;
            case EFencePose::ChamberFlank:  // the hand out to his right, the blade back and up behind it
                K.Fist = S + FVector(6.f, R * 24.f, -4.f); K.Blade = Dir(-.45f, R * .75f, .45f); K.Edge = EdgeOf(FVector(.6f, -R, 0.f), K.Blade); K.Pole = Dir(-.6f, R * .5f, -.6f); break;
            case EFencePose::CutFlank:      // across from his right at his man's left side
                K.Blade = ((Target - S).GetSafeNormal() + FVector(0.f, R * .5f, -.1f)).GetSafeNormal(); K.Fist = OnBlade(K.Blade, Target);
                K.Edge = EdgeOf(FVector(0.f, -R, 0.f), K.Blade); K.Pole = Dir(-.25f, R * .4f, -1.f); break;
            case EFencePose::ChamberLow:    // the hand low across to his left, the blade back behind it
                K.Fist = S + FVector(14.f, -R * 14.f, -26.f); K.Blade = Dir(-.4f, -R * .8f, -.3f); K.Edge = EdgeOf(FVector(.6f, R, .2f), K.Blade); K.Pole = Dir(-.3f, R * .8f, -.6f); break;
            case EFencePose::CutLow:        // backhand, low at his man's right leg
                K.Blade = ((Target + FVector(0.f, 0.f, -30.f) - S).GetSafeNormal() + FVector(0.f, -R * .5f, -.2f)).GetSafeNormal(); K.Fist = OnBlade(K.Blade, Target);
                K.Edge = EdgeOf(FVector(0.f, R, .3f), K.Blade); K.Pole = Dir(-.2f, R * .6f, -1.f); break;
            case EFencePose::Thrust:        // the point at his chest, met by his parry
                K.Blade = (Target + FVector(0.f, 0.f, 12.f) - S).GetSafeNormal(); K.Fist = OnBlade(K.Blade, Target);   // level at his upper chest
                K.Edge = EdgeOf(FVector(0.f, R * .3f, -1.f), K.Blade); K.Pole = Dir(-.25f, R * .6f, -1.f); break;
            case EFencePose::Lunge:         // the point at his chest, nothing in the way, as far as the arm goes
                K.Blade = (Target - S).GetSafeNormal(); K.Fist = S + K.Blade * SwordReach;
                K.Edge = EdgeOf(FVector(0.f, R * .3f, -1.f), K.Blade); K.Pole = Dir(-.25f, R * .6f, -1.f); break;
            case EFencePose::Feint:         // a half thrust at his inside
                K.Blade = (Target - S + FVector(0.f, R * 10.f, 0.f)).GetSafeNormal(); K.Fist = S + K.Blade * SwordReach * .62f;
                K.Edge = EdgeOf(FVector(0.f, 0.f, -1.f), K.Blade); K.Pole = Dir(-.25f, R * .6f, -1.f); break;
            case EFencePose::Disengage:     // the point dipped under his man's blade, to come up on its other side
                K.Fist = S + FVector(32.f, -R * 2.f, -30.f); K.Blade = Dir(1.f, R * .2f, -.2f); K.Edge = EdgeOf(FVector(0.f, 0.f, -1.f), K.Blade); K.Pole = Dir(-.25f, R * .6f, -1.f); break;
            case EFencePose::ParryHigh:     // the blade across over his head, edge up
                K.Fist = S + FVector(30.f, R * 12.f, 18.f); K.Blade = Dir(.35f, -R, .2f); K.Edge = EdgeOf(FVector(0.f, 0.f, 1.f), K.Blade); K.Pole = Dir(0.f, R, -.3f); break;
            case EFencePose::ParryIn:       // the blade upright before his left side
                K.Fist = S + FVector(32.f, -R * 14.f, -16.f); K.Blade = Dir(.6f, -R * .2f, .8f); K.Edge = EdgeOf(FVector(.3f, -R, 0.f), K.Blade); K.Pole = Dir(-.3f, R * .5f, -1.f); break;
            case EFencePose::ParryOut:      // the blade upright before his right side
                K.Fist = S + FVector(34.f, R * 14.f, -18.f); K.Blade = Dir(.65f, R * .25f, .75f); K.Edge = EdgeOf(FVector(.3f, R, 0.f), K.Blade); K.Pole = Dir(-.3f, R * .8f, -.8f); break;
            case EFencePose::ParryLow:      // the hand high, the point down before his right leg
                K.Fist = S + FVector(28.f, R * 12.f, -24.f); K.Blade = Dir(.45f, R * .35f, -1.f); K.Edge = EdgeOf(FVector(.2f, R, 0.f), K.Blade); K.Pole = Dir(0.f, R, .4f); break;
            case EFencePose::Knocked:       // his blade beaten out to his right
                K.Fist = S + FVector(28.f, R * 24.f, -12.f); K.Blade = Dir(.75f, R * .6f, .25f); K.Edge = EdgeOf(FVector(0.f, 0.f, -1.f), K.Blade); K.Pole = Dir(-.3f, R * .8f, -.8f); break;
            case EFencePose::BeatAside:     // a sharp blow on his man's blade, sweeping it to his own left
                K.Blade = Dir(.8f, -R * .55f, .05f); K.Fist = OnBlade(K.Blade, Target); K.Edge = EdgeOf(FVector(0.f, -R, 0.f), K.Blade); K.Pole = Dir(-.3f, R * .6f, -1.f); break;
            case EFencePose::BindHold:      // blades crossed high near the hilts
                K.Fist = S + FVector(36.f, -R * 4.f, 4.f); K.Blade = Dir(.45f, -R * .3f, .85f); K.Edge = EdgeOf(FVector(0.f, -R, 0.f), K.Blade); K.Pole = Dir(-.3f, R * .7f, -1.f); break;
            case EFencePose::BindPress:     // bearing down on his man's crossed blade
                K.Blade = Dir(.45f, -R * .3f, .85f); K.Fist = OnBlade(K.Blade, HeadAt); K.Edge = EdgeOf(FVector(0.f, -R, -.3f), K.Blade); K.Pole = Dir(-.3f, R * .7f, -1.f); break;
            default:                        // on guard: the fist forward, the point up at his man's eyes, moving a little
                K.Fist = S + FVector(34.f, -R * 6.f, -20.f) + FVector(0.f, R * 1.5f * FMath::Sin(Search), 1.5f * FMath::Sin(Search * .73f + 1.f));
                K.Blade = (Target + FVector(0.f, R * 6.f * FMath::Sin(Search * 1.3f), 28.f + 5.f * FMath::Cos(Search)) - K.Fist).GetSafeNormal();
                K.Edge = EdgeOf(FVector(.25f, 0.f, -1.f), K.Blade); K.Pole = Dir(-.25f, R * .6f, -1.f); break;
            }
            return K;
        };
        FSword Key[4];
        FVector Fists[4], Blades[4], Edges[4], Poles[4];
        for (int32 J = 0; J < 4; ++J)
        {
            Key[J] = PoseOf(Fence.Key[J].Pose);
            Fists[J] = Key[J].Fist; Blades[J] = Key[J].Blade; Edges[J] = Key[J].Edge; Poles[J] = Key[J].Pole;
        }
        FVector Fist = FenceCurve(Fence.Key, Fists, Fence.Now);
        Fist = S + (Fist - S).GetClampedToMaxSize(SwordReach);
        const FVector Blade = FenceCurve(Fence.Key, Blades, Fence.Now).GetSafeNormal();
        const FVector Along = EdgeOf(FenceCurve(Fence.Key, Edges, Fence.Now), Blade);
        PlaceHand(Space, 1, Fist, Along, Blade, FenceCurve(Fence.Key, Poles, Fence.Now).GetSafeNormal());
        FVector P, A, T;
        HandFrame(Space, 1, P, A, T);
        if (Clock > 6.f) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(P, Fist)));
        PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(T, -FVector::VectorPlaneProject(A, T).GetSafeNormal()).ToQuat(), P));
        BladeBaseW = ToWorld.TransformPosition(P + T * CutlassBase);
        BladeTipW = ToWorld.TransformPosition(P + T * CutlassTip);
        // The other hand: a fist on his hip on guard; thrown back and down behind him for balance as he lunges or cuts.
        {
            const FVector HipsAt = Space[BoneIndex[Pelvis]].GetLocation();
            const float Back = Ease01(FMath::Clamp(Fence.Lunge * 1.4f + FMath::Max(0.f, Fence.Twist) * .3f, 0.f, 1.f));
            const FVector HipFist = HipsAt + FVector(-3.f, ArmOut * 20.f, 9.f);
            const FVector BackFist = Shoulder[0] + FVector(-36.f, ArmOut * 16.f, -24.f);
            const FVector HipAlong = FVector(.15f, -ArmOut * .55f, -.8f).GetSafeNormal(), BackAlong = FVector(-.75f, ArmOut * .25f, -.6f).GetSafeNormal();
            const FVector OffFist = FMath::Lerp(HipFist, BackFist, Back);
            const FVector OffAlong = FMath::Lerp(HipAlong, BackAlong, Back).GetSafeNormal();
            const FVector OffThumb = FVector::VectorPlaneProject(FMath::Lerp(FVector(1.f, 0.f, .3f), FVector(0.f, 0.f, 1.f), Back), OffAlong).GetSafeNormal();
            PlaceHand(Space, 0, OffFist, OffAlong, OffThumb, FMath::Lerp(FVector(-.5f, ArmOut, .3f), FVector(.2f, ArmOut, -.6f), Back).GetSafeNormal());
            SetGrip(0, FMath::Lerp(.55f, .3f, Back));
        }
        break;
    }
    case EDockHuman::PirateCaptain:
    {
        // Fists on his hips (just outside his hip joints, over the coat), elbows out; his right goes to the cutlass and back.
        const FVector HipsAt = Space[BoneIndex[Pelvis]].GetLocation();
        const float HipW = static_cast<float>(FMath::Abs(Space[BoneIndex[ThighL]].GetLocation().Y - Space[BoneIndex[ThighR]].GetLocation().Y)) * .5f;
        struct FCap { FVector Fist, Along, Blade, Pole; };
        const auto OnHip = [&](int32 Side)
        {
            const FVector Along = FVector(.15f, -OutOf(Side) * .55f, -.8f).GetSafeNormal();
            return FCap{ HipsAt + FVector(-2.f, OutOf(Side) * (HipW + 5.f), 9.f), Along, FVector::VectorPlaneProject(FVector(1.f, 0.f, .3f), Along).GetSafeNormal(), FVector(-.5f, OutOf(Side), .3f) };
        };
        const auto Mix = [](const FCap& A, const FCap& B, float T)
        {
            T = Ease01(T);
            return FCap{ FMath::Lerp(A.Fist, B.Fist, T), FMath::Lerp(A.Along, B.Along, T).GetSafeNormal(), FMath::Lerp(A.Blade, B.Blade, T).GetSafeNormal(), FMath::Lerp(A.Pole, B.Pole, T).GetSafeNormal() };
        };
        const FCap LeftHip = OnHip(0);
        PlaceHand(Space, 0, LeftHip.Fist, LeftHip.Along, LeftHip.Blade, LeftHip.Pole);
        const float L = static_cast<float>(FVector::Dist(Shoulder[1], Space[BoneIndex[LowerR]].GetLocation()) + FVector::Dist(Space[BoneIndex[LowerR]].GetLocation(), Space[BoneIndex[HandR]].GetLocation()));
        // Taking the planted grip: his fist over it, the thumb down its length toward the blade (so, drawn, the blade comes up
        // out of the thumb's side of his fist), his knuckles forward and a little in, the wrist square to the grip, the elbow
        // out to his side and back.
        const FVector StuckAlong = FVector::VectorPlaneProject(FVector(1.f, 0.f, 0.f), CaptainBlade).GetSafeNormal();
        const FVector GripAlong = FVector::VectorPlaneProject(FVector(1.f, -Right * .15f, -.2f), CaptainBlade).GetSafeNormal();
        const FCap Planted{ CaptainHilt, GripAlong, CaptainBlade, FVector(-.35f, Right * .85f, .15f).GetSafeNormal() };
        const FCap Pulled{ CaptainHilt - CaptainBlade * 16.f, StuckAlong, CaptainBlade, Planted.Pole };
        // Held up and out to his right, the blade rising past his shoulder (the reference), turning slowly as he shows it.
        const float Sway = FMath::Sin(CaptainT * .9f) * .12f;
        // (His arm nearly straight, out and a little down from the shoulder; the blade angled up and out past his head.)
        const FVector UpBlade = FVector(.2f + Sway, Right * .72f, .66f).GetSafeNormal();
        const FCap Raised{ Shoulder[1] + FVector(.28f * L, Right * .8f * L, -.45f * L), FVector::VectorPlaneProject(FVector(.7f, Right * .2f, .3f), UpBlade).GetSafeNormal(), UpBlade,
            FVector(-.5f, Right * .3f, -1.f).GetSafeNormal() };
        const FCap Hip = OnHip(1);
        const float C = CaptainT;
        FCap Hand = Hip; bool bHeld = false;
        if (C < CapReach || C >= CapBack) CaptainPhase = 0;
        else if (C < CapPull) { CaptainPhase = 1; Hand = Mix(Hip, Planted, (C - CapReach) / (CapPull - CapReach)); }
        else if (C < CapRaise) { CaptainPhase = 1; Hand = Mix(Planted, Pulled, (C - CapPull) / (CapRaise - CapPull)); bHeld = true; }
        else if (C < CapHold) { CaptainPhase = 1; Hand = Mix(Pulled, Raised, (C - CapRaise) / (CapHold - CapRaise)); bHeld = true; }
        else if (C < CapLower) { CaptainPhase = 2; Hand = Raised; bHeld = true; }
        else if (C < CapPlunge) { CaptainPhase = 3; Hand = Mix(Raised, Pulled, (C - CapLower) / (CapPlunge - CapLower)); bHeld = true; }
        else if (C < CapLetGo) { CaptainPhase = 3; Hand = Mix(Pulled, Planted, (C - CapPlunge) / (CapLetGo - CapPlunge)); bHeld = true; }
        else { CaptainPhase = 3; Hand = Mix(Planted, Hip, (C - CapLetGo) / (CapBack - CapLetGo)); }
        PlaceHand(Space, 1, Hand.Fist, Hand.Along, Hand.Blade, Hand.Pole);
        // His right hand's fingers: a fist on his hip; opening as he reaches, closing round the grip as he gets there; shut
        // while he holds it; opening as he lets go and curling back into a fist on the way to his hip.
        {
            float G = .85f;
            if (C >= CapReach && C < CapPull)
            {
                const float T = (C - CapReach) / (CapPull - CapReach);
                G = T < .55f ? FMath::Lerp(.85f, .2f, Ease01(T / .55f)) : FMath::Lerp(.2f, 1.f, Ease01((T - .55f) / .45f));
            }
            else if (bHeld) G = 1.f;
            else if (C >= CapLetGo && C < CapBack)
            {
                const float T = (C - CapLetGo) / (CapBack - CapLetGo);
                G = T < .35f ? FMath::Lerp(1.f, .3f, Ease01(T / .35f)) : FMath::Lerp(.3f, .85f, Ease01((T - .35f) / .65f));
            }
            Grip[1] = G;
        }
        const FVector Size(CutlassScale);
        if (bHeld)
        {
            FVector P, A, T;
            HandFrame(Space, 1, P, A, T);
            if (Clock > 6.f) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(P, Hand.Fist)));
            PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(T, -FVector::VectorPlaneProject(A, T).GetSafeNormal()).ToQuat(), P, Size));
        }
        else PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(CaptainBlade, -StuckAlong).ToQuat(), CaptainHilt, Size));
        break;
    }
    case EDockHuman::Jeffries:
    {
        // His wrists bound behind his back round the mast; they pull as he heaves.
        const FVector Hips = Space[BoneIndex[Pelvis]].GetLocation();
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector Fist = Hips + FVector(-21.f + 2.f * Strain, OutOf(Side) * (8.f + 3.f * Strain), 10.f);
            const FVector Along = FVector(-.3f, 0.f, -.95f).GetSafeNormal();
            const FVector Thumb = FVector::VectorPlaneProject(FVector(0.f, -OutOf(Side), 0.f), Along).GetSafeNormal();
            PlaceHand(Space, Side, Fist, Along, Thumb, FVector(-1.f, OutOf(Side) * .5f, 0.f));
        }
        UpdateRopes(Space);
        break;
    }
    case EDockHuman::PirateChef:
    {
        if (PirateDip > .05f) Settle(PirateDip);
        // The cleaver: chopping on the board on every other beat at his work, carried high as he runs, raised and brought
        // down at the rat. His left hand holds the work steady on the board.
        const FVector Board(46.f, Right * 8.f, ChefTableTop + 3.f);
        FVector Fist; FVector Along;
        if (Chef == EChef::Work || (Chef == EChef::Return && JogBlend < .5f && FVector::Dist2D(GetActorLocation(), ChefHome) < 12.f))
        {
            const float U = Frac(B * .5f + .12f);   // the blade comes down on the beat
            const float H = U < .12f ? 26.f * (1.f - Ease01(U / .12f)) : 26.f * Ease01((U - .12f) / .7f);
            Fist = Board + FVector(-6.f + 4.f * H / 26.f, 0.f, H);
            Along = FMath::Lerp(FVector(.55f, 0.f, -.8f), FVector(.6f, 0.f, .5f), H / 26.f).GetSafeNormal();
            const FVector Hold(46.f, -Right * 14.f, ChefTableTop + 5.f);
            PlaceHand(Space, 0, Hold, FVector(.7f, Right * .2f, -.7f).GetSafeNormal(), FVector(.1f, Right, 0.f).GetSafeNormal(), FVector(-.4f, -Right, -.5f));
        }
        else
        {
            const float Wind = Chef == EChef::Swing ? Ease01(ChefTime / ChefWindup) * (1.f - SwingDrive) : 0.f;
            const FVector Carry = Shoulder[1] + FVector(22.f, Right * 10.f, -10.f);
            const FVector Raised = Shoulder[1] + FVector(4.f, Right * 8.f, 34.f);
            const FVector Struck = FVector(78.f, Right * 6.f, 32.f);   // at the rat on the floor before him
            Fist = FMath::Lerp(FMath::Lerp(Carry, Raised, Wind), Struck, SwingDrive);
            Along = FMath::Lerp(FMath::Lerp(FVector(.5f, 0.f, .85f), FVector(-.2f, 0.f, 1.f), Wind), FVector(.5f, 0.f, -.85f), SwingDrive).GetSafeNormal();
            if (Chef == EChef::Lost) Fist += FVector(0.f, 0.f, 8.f * FMath::Sin(Clock * 14.f));   // shaking it after him
        }
        const FVector Thumb = FVector::VectorPlaneProject(FVector(.35f, -Right * .9f, .2f), Along).GetSafeNormal();
        PlaceHand(Space, 1, Fist, Along, Thumb, FVector(-.4f, Right, -.6f));
        FVector P, A, T;
        HandFrame(Space, 1, P, A, T);
        if (Clock > 6.f && Chef == EChef::Work) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(P, Fist)));
        PlaceProp(0, FTransform(FRotationMatrix::MakeFromXZ(T, -FVector::VectorPlaneProject(A, T).GetSafeNormal()).ToQuat(), P));
        break;
    }
    default: break;
    }
}

void ADockNPC::UpdateRopes(const TArray<FTransform>& Space)
{
    if (!Ropes) return;
    // Two bands round his chest (over his arms) and his waist and round the mast behind him, and a lashing round his wrists
    // tied off to the mast: hemp tubes rebuilt every frame from where his body is (component space).
    TArray<TArray<FVector>> Paths;
    const FVector Mast(MastLocal.X, MastLocal.Y, 0.f);
    const auto Band = [&](const FVector& Centre, float Front, float Half, float Z)
    {
        TArray<FVector> P;
        const FVector Back = (Mast - FVector(Centre.X, Centre.Y, 0.f)).GetSafeNormal2D();   // toward the mast, behind him
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Back);
        for (int32 K = 0; K <= 12; ++K)   // round his front, from one side to the other
        {
            const float A = PI * (K / 12.f) - PI * .5f;
            P.Add(FVector(Centre.X, Centre.Y, Z) - Back * (Front * FMath::Cos(A)) + Side * (Half * FMath::Sin(A)));
        }
        for (int32 K = 0; K <= 14; ++K)   // round the far side of the mast and back
        {
            const float A = PI * .5f - PI * (K / 14.f);
            P.Add(FVector(Mast.X, Mast.Y, Z) + Back * ((MastR + 1.2f) * FMath::Cos(A)) + Side * ((MastR + 1.2f) * FMath::Sin(A)));
        }
        const FVector First = P[0];   // (a copy: adding an element of the array to itself)
        P.Add(First);
        Paths.Add(P);
    };
    const FVector ChestAt = Space[BoneIndex[Chest]].GetLocation(), HipsAt = Space[BoneIndex[Pelvis]].GetLocation();
    Band(ChestAt, 15.f, 24.f, static_cast<float>(ChestAt.Z) + 10.f);
    Band(ChestAt, 15.f, 24.f, static_cast<float>(ChestAt.Z) + 1.f);
    Band(HipsAt, 15.f, 20.f, static_cast<float>(HipsAt.Z) + 14.f);
    // The wrists: three turns round both, then a line back to the mast.
    const FVector Wrists = (Space[BoneIndex[HandL]].GetLocation() + Space[BoneIndex[HandR]].GetLocation()) * .5f;
    {
        TArray<FVector> P;
        for (int32 K = 0; K <= 36; ++K)
        {
            const float A = 2.f * PI * K / 12.f;
            P.Add(Wrists + FVector(4.5f * FMath::Cos(A), (K / 36.f - .5f) * 6.f, 4.f * FMath::Sin(A)));
        }
        P.Add(FVector(Mast.X + (Wrists.X - Mast.X) * .35f, Mast.Y, Wrists.Z - 2.f));
        P.Add(FVector(Mast.X, Mast.Y, Wrists.Z - 2.f) + (FVector(Wrists.X, Wrists.Y, 0.f) - FVector(Mast.X, Mast.Y, 0.f)).GetSafeNormal2D() * (MastR + 1.f));
        Paths.Add(P);
    }
    TArray<FVector> V, N; TArray<int32> Tris; TArray<FVector2D> UV;
    constexpr int32 Sides = 6; constexpr float R = 1.25f;
    for (const TArray<FVector>& P : Paths)
    {
        const int32 Base = V.Num();
        for (int32 I = 0; I < P.Num(); ++I)
        {
            const FVector D = (P[FMath::Min(I + 1, P.Num() - 1)] - P[FMath::Max(I - 1, 0)]).GetSafeNormal();
            FVector X = FVector::CrossProduct(D, FVector::UpVector);
            if (X.SizeSquared() < 1e-4) X = FVector::CrossProduct(D, FVector::ForwardVector);
            X.Normalize();
            const FVector Y = FVector::CrossProduct(X, D);
            for (int32 S = 0; S < Sides; ++S)
            {
                const float A = 2.f * PI * S / Sides;
                const FVector O = X * FMath::Cos(A) + Y * FMath::Sin(A);
                V.Add(P[I] + O * R); N.Add(O); UV.Add(FVector2D(S / float(Sides), I * .3f));
            }
        }
        for (int32 I = 0; I + 1 < P.Num(); ++I)
            for (int32 S = 0; S < Sides; ++S)
            {
                const int32 A = Base + I * Sides + S, Bq = Base + I * Sides + (S + 1) % Sides, C = A + Sides, D = Bq + Sides;
                Tris.Append({ A, Bq, C, Bq, D, C });   // (the ship's own winding: outward faces clockwise)
            }
    }
    if (Ropes->GetNumSections() == 0 || Ropes->GetProcMeshSection(0)->ProcVertexBuffer.Num() != V.Num())
        Ropes->CreateMeshSection(0, V, Tris, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    else Ropes->UpdateMeshSection(0, V, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>());
}
