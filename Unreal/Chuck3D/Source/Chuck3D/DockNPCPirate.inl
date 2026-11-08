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

namespace JeffriesRopeData
{
#include "JeffriesRopeBody.inl"
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
        // The ship's hemp (2026-11-08: the CC0 rope scan, as on the storm's lines; was plain canvas).
        UMaterialInterface* Hemp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Rope.M_Rope"));
        if (!Hemp) Hemp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_Canvas.M_Canvas"));
        if (Hemp) Ropes->SetMaterial(0, Hemp);
    }
    bMeasureRope = FParse::Param(FCommandLine::Get(), TEXT("ChuckShipTest"));
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckBakeJeffriesRope"))) BakeRopeBody();
}

void ADockNPC::BakeRopeBody() const
{
    // His mesh's own vertices (the editor's imported model) near where his ropes go: his torso from below the hips to above
    // the chest, and his arms; each kept in the frame of the bone that carries most of it, so that, posed each frame, they
    // follow his body as it moves. One a 1.4 cm cell.
#if WITH_EDITORONLY_DATA
    const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Body->GetSkinnedAsset());
    const FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
    if (!Model || Model->LODModels.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("CHUCK_JEFFRIES_ROPE_BAKE failed: no imported model")); return; }
    const FSkeletalMeshLODModel& LOD = Model->LODModels[0];
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TArray<FTransform> RefComp; RefComp.SetNum(Ref.GetNum());
    for (int32 B = 0; B < Ref.GetNum(); ++B)
    {
        const int32 Parent = Ref.GetParentIndex(B);
        RefComp[B] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[B] : Ref.GetRefBonePose()[B] * RefComp[Parent];
    }
    TMap<int32, int32> Ours;
    for (const int32 E : { Pelvis, Spine1, Spine2, Chest, ClavL, ClavR, UpperL, UpperR, LowerL, LowerR, HandL, HandR, ThighL, ThighR }) Ours.Add(BoneIndex[E], E);
    const double Low = RefComp[BoneIndex[Pelvis]].GetLocation().Z - 12., High = RefComp[BoneIndex[Chest]].GetLocation().Z + 30.;
    TSet<FIntVector> Cells;
    FString Out = TEXT("// Jeffries' body where his ropes go: points on his mesh (his shirt and arms), each in the frame of the bone that carries it\n")
        TEXT("// ({EBone, x, y, z}, cm). Generated: run the ship with -ChuckBakeJeffriesRope in the editor (ADockNPC::BakeRopeBody,\n")
        TEXT("// DockNPCPirate.inl), which rewrites this file; then rebuild. Empty ({-1}): his ropes fall back to plain ovals round him.\n")
        TEXT("const float JeffriesRopeBody[][4] = {\n");
    int32 Count = 0;
    for (const FSkelMeshSection& S : LOD.Sections)
        for (const FSoftSkinVertex& V : S.SoftVertices)
        {
            int32 Best = 0;
            for (int32 I = 1; I < MAX_TOTAL_INFLUENCES; ++I) if (V.InfluenceWeights[I] > V.InfluenceWeights[Best]) Best = I;
            if (!S.BoneMap.IsValidIndex(V.InfluenceBones[Best])) continue;
            const int32 MeshBone = S.BoneMap[V.InfluenceBones[Best]];
            const int32* E = Ours.Find(MeshBone);
            if (!E) continue;
            const FVector P(V.Position);
            const bool bArm = *E >= UpperL && *E <= HandR;
            if (!bArm && (P.Z < Low || P.Z > High)) continue;
            bool bThere = false;
            Cells.Add(FIntVector(FMath::FloorToInt32(P.X / 1.4), FMath::FloorToInt32(P.Y / 1.4), FMath::FloorToInt32(P.Z / 1.4)), &bThere);
            if (bThere) continue;
            const FVector L = RefComp[MeshBone].GetRotation().UnrotateVector(P - RefComp[MeshBone].GetLocation());   // (unscaled: the rig's bones carry a scale)
            Out += FString::Printf(TEXT("    {%d.f, %.2ff, %.2ff, %.2ff},\n"), *E, L.X, L.Y, L.Z);
            ++Count;
        }
    Out += TEXT("};\n");
    const FString File = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir() / TEXT("Chuck3D/JeffriesRopeBody.inl"));
    const bool bSaved = FFileHelper::SaveStringToFile(Out, *File);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_JEFFRIES_ROPE_BAKE points=%d saved=%d file=%s"), Count, bSaved ? 1 : 0, *File);
#else
    UE_LOG(LogTemp, Error, TEXT("CHUCK_JEFFRIES_ROPE_BAKE failed: needs the editor"));
#endif
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
    if (StormWork != EStormWork::None) { TickStormWork(DeltaSeconds); return; }   // (2026-11-04: at his storm station)
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
    if (StormWork != EStormWork::None) { StormDelta(Delta, BoneDelta, HipsOffset); return; }
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
    if (StormWork != EStormWork::None) { PoseStorm(Space); return; }
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
        // (2026-11-04, user: his hands were "backwards on the concertina": palms out, the fingers curling away from it. In this
        // rig fingers up with the palm in toward the bellows means the thumb's side back toward him.)
        const FVector Thumb = FVector::VectorPlaneProject(FVector(-1.f, 0.f, .2f), Along).GetSafeNormal();
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
            FVector Fist = Hips + FVector(-21.f + 2.f * Strain, OutOf(Side) * (15.f + 3.f * Strain), 10.f);
            // Against the mast, not in it (2026-11-08): his fists either side of it behind him.
            const FVector2D Off(Fist.X - MastLocal.X, Fist.Y - MastLocal.Y);
            if (Off.Size() < MastR + 4.5f) { const FVector2D O = Off.GetSafeNormal() * (MastR + 4.5f); Fist.X = MastLocal.X + O.X; Fist.Y = MastLocal.Y + O.Y; }
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
    // (2026-11-08, the user: "it needs to be around him, not with parts of him passing through it impossibly".) Lashings
    // hauled tight round him and the mast: at each turn's height the rope lies round the outline of his body there (his
    // mesh's points, JeffriesRopeBody.inl, posed with him every frame) and the mast's together, as a rope pulled taut round
    // both does: over his front and arms, straight across from his sides to the mast, round its far side. Two turns over his
    // chest and arms, two round his hips, each lashing climbing to its next turn behind the mast. His wrists: two turns
    // round each forearm above the hand, and the line between them round the mast's far side.
    constexpr float R = 1.25f, Snug = .2f;   // the rope's radius, and how far it presses into the cloth
    TArray<FVector> Pts; TArray<int32> Bones;
    for (const auto& B : JeffriesRopeData::JeffriesRopeBody)
        if (B[0] >= 0.f)
        {
            const int32 E = static_cast<int32>(B[0]);
            Pts.Add(Space[BoneIndex[E]].GetLocation() + Space[BoneIndex[E]].GetRotation().RotateVector(FVector(B[1], B[2], B[3]))); Bones.Add(E);
        }
    const FVector ChestAt = Space[BoneIndex[Chest]].GetLocation(), HipsAt = Space[BoneIndex[Pelvis]].GetLocation();
    const FVector2D Mast(MastLocal.X, MastLocal.Y);
    const FVector2D Back = (Mast - FVector2D(ChestAt.X, ChestAt.Y)).GetSafeNormal();   // from him toward the mast
    const auto Round = [](int32 K, int32 N) { const float A = 2.f * PI * K / N; return FVector2D(FMath::Cos(A), FMath::Sin(A)); };
    if (Pts.IsEmpty())   // (no baked body: ovals round his chest and hips)
        for (const FVector& C : { ChestAt + FVector(0, 0, 5), HipsAt + FVector(0, 0, 14) })
            for (int32 Dz = -6; Dz <= 6; Dz += 3)
                for (int32 K = 0; K < 24; ++K)
                {
                    const FVector2D O = Round(K, 24) * FVector2D(15.f, 24.f);
                    Pts.Add(FVector(C.X - Back.X * O.X - Back.Y * O.Y, C.Y - Back.Y * O.X + Back.X * O.Y, C.Z + Dz)); Bones.Add(Chest);
                }
    // The rope's centre line hauled tight round In (points in a plane), a rope's radius off them less what it presses in.
    const auto Taut = [&](const TArray<FVector2D>& In, TArray<FVector2D>& Out)
    {
        TArray<FVector2D> Hull, Grown;
        ShipRope::Hull2D(In, Hull);
        for (const FVector2D& P : Hull) for (int32 K = 0; K < 12; ++K) Grown.Add(P + Round(K, 12) * (R - Snug));
        ShipRope::Hull2D(Grown, Out);
    };
    // The loop round his body and the mast at height Z, starting behind the mast.
    const auto Loop = [&](float Z, TArray<FVector2D>& Out)
    {
        TArray<FVector2D> In;
        for (const FVector& P : Pts) if (FMath::Abs(P.Z - Z) < 2.f) In.Add(FVector2D(P.X, P.Y));
        for (int32 K = 0; K < 32; ++K) In.Add(Mast + Round(K, 32) * MastR);
        TArray<FVector2D> L;
        Taut(In, L);
        int32 Start = 0;
        for (int32 I = 1; I < L.Num(); ++I) if (FVector2D::DotProduct(L[I] - Mast, Back) > FVector2D::DotProduct(L[Start] - Mast, Back)) Start = I;
        Out.Reset();
        for (int32 I = 0; I < L.Num(); ++I) Out.Add(L[(Start + I) % L.Num()]);
    };
    TArray<TArray<FVector>> RopeLines;
    for (const TArray<float>& Turns : { TArray<float>{ static_cast<float>(ChestAt.Z) + 15.f, static_cast<float>(ChestAt.Z) + 8.f },   // (over his chest, above his bound hands)
                                        TArray<float>{ static_cast<float>(HipsAt.Z) + 2.f, static_cast<float>(HipsAt.Z) - 5.f } })   // (round his hips, under his bound hands)
    {
        TArray<FVector>& Path = RopeLines.AddDefaulted_GetRef();
        for (int32 T = 0; T < Turns.Num(); ++T)
        {
            TArray<FVector2D> L; Loop(Turns[T], L);
            for (int32 I = 0; I < L.Num(); ++I)
            {
                const FVector2D& P = L[I];
                // Climbing to its next turn over the last stretch round the mast's far side (a lashing's turns run on).
                float Z = Turns[T];
                const FVector2D D = (P - Mast).GetSafeNormal();
                const float Phi = FMath::Atan2(static_cast<float>(Back.X * D.Y - Back.Y * D.X), static_cast<float>(Back.X * D.X + Back.Y * D.Y));
                if (T + 1 < Turns.Num() && I > L.Num() / 2 && FMath::Abs(Phi) < 1.3f) Z = FMath::Lerp(Turns[T], Turns[T + 1], 1.f - FMath::Abs(Phi) / 1.3f);
                Path.Add(FVector(P.X, P.Y, Z));
            }
            if (T + 1 == Turns.Num() && L.Num()) Path.Add(FVector(L[0].X, L[0].Y, Turns[T] - 1.f));   // its end tucked under the last turn
        }
    }
    // The wrists: two turns round each forearm above the hand, wound from the side toward the mast.
    TArray<FVector> Wound[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector Hand = Space[BoneIndex[HandL + Side]].GetLocation(), Elbow = Space[BoneIndex[LowerL + Side]].GetLocation();
        const FVector A = (Hand - Elbow).GetSafeNormal(), W = Hand - A * 4.f;
        FVector X = FVector::CrossProduct(A, FVector::UpVector);
        if (X.SizeSquared() < 1e-4) X = FVector::CrossProduct(A, FVector::ForwardVector);
        X.Normalize();
        const FVector Y = FVector::CrossProduct(A, X);
        TArray<FVector2D> In, L;
        for (int32 I = 0; I < Pts.Num(); ++I)
            if ((Bones[I] == LowerL + Side || Bones[I] == HandL + Side) && FMath::Abs(FVector::DotProduct(Pts[I] - W, A)) < 3.5f && FVector::Dist(Pts[I], W) < 10.f)
                In.Add(FVector2D(FVector::DotProduct(Pts[I] - W, X), FVector::DotProduct(Pts[I] - W, Y)));
        if (In.Num() < 3) for (int32 K = 0; K < 12; ++K) In.Add(Round(K, 12) * 3.5f);
        Taut(In, L);
        const FVector ToMast = FVector(Mast.X, Mast.Y, W.Z) - W;
        const FVector2D Dir(FVector::DotProduct(ToMast, X), FVector::DotProduct(ToMast, Y));
        int32 Start = 0;
        for (int32 I = 1; I < L.Num(); ++I) if (FVector2D::DotProduct(L[I], Dir) > FVector2D::DotProduct(L[Start], Dir)) Start = I;
        for (int32 T = 0; T < 2; ++T)
            for (int32 I = 0; I < L.Num(); ++I)
            {
                const FVector2D P = L[(Start + I) % L.Num()];
                Wound[Side].Add(W + A * (-2.6f + 2.6f * (T + I / float(L.Num()))) + X * P.X + Y * P.Y);
            }
        if (Side == 0) Algo::Reverse(Wound[0]);   // (the left ends where the line leaves it, toward the mast)
    }
    // The line between them, round the far side of the mast from him, leaving and joining each wrist along a tangent.
    {
        TArray<FVector>& Path = RopeLines.AddDefaulted_GetRef();
        Path = Wound[0];
        const FVector E0 = Wound[0].Last(), E1 = Wound[1][0];
        const float Rm = MastR + R - Snug;
        TArray<FVector2D> In, H;
        for (int32 K = 0; K < 48; ++K) In.Add(Mast + Round(K, 48) * Rm);
        In.Add(FVector2D(E0.X, E0.Y)); In.Add(FVector2D(E1.X, E1.Y));
        ShipRope::Hull2D(In, H);
        const int32 N = H.Num();
        const auto Nearest = [&](const FVector2D& P) { int32 Best = 0; for (int32 I = 1; I < N; ++I) if (FVector2D::DistSquared(H[I], P) < FVector2D::DistSquared(H[Best], P)) Best = I; return Best; };
        const int32 I0 = Nearest(FVector2D(E0.X, E0.Y)), I1 = Nearest(FVector2D(E1.X, E1.Y)), IB = Nearest(Mast - Back * Rm);
        bool bForwardPassesHim = false;
        for (int32 I = I0; I != I1; I = (I + 1) % N) if (I == IB) bForwardPassesHim = true;
        const int32 Step = bForwardPassesHim ? N - 1 : 1;
        int32 Count = 0;
        for (int32 I = I0; I != I1; I = (I + Step) % N) ++Count;
        int32 K = 0;
        for (int32 I = I0; ; I = (I + Step) % N, ++K)
        {
            Path.Add(FVector(H[I].X, H[I].Y, FMath::Lerp(E0.Z, E1.Z, Count ? K / float(Count) : 0.f)));
            if (I == I1) break;
        }
        Path.Append(Wound[1]);
    }
    ShipRope::FMesh M;
    for (const TArray<FVector>& Path : RopeLines) ShipRope::Tube(M, Path, R, 0.f, 8);
    M.Into(Ropes, 0, Ropes->GetNumSections() > 0);

    // Tests: the furthest his body comes inside a rope (cm; a body point a rope's radius from its centre line touches it).
    if (bMeasureRope && (RopeFrame++ % 15) == 0)
        for (const FVector& P : Pts)
        {
            float Near = 1e9f;
            for (const TArray<FVector>& Path : RopeLines)
                for (int32 I = 1; I < Path.Num(); ++I) Near = FMath::Min(Near, static_cast<float>(FMath::PointDistToSegment(P, Path[I - 1], Path[I])));
            RopeDepth = FMath::Max(RopeDepth, R - Near);
        }
}

// ------------------------------------------------------------------ the storm (2026-11-04)
// (User: "during storm have the NPC's responding to the storm, doing things sailors would in a storm, the captain at the
// helm. no concertina held by the concertina player as he is now manning the ship as with the other crewmen".) At the
// storm's start each of the deck's crew stows what he holds (the concertina, the tankard, the cutlasses) and walks briskly to
// his station (AShipCrew::BeginStorm): two to a line, they haul it hand over hand, leaning back into each heave together;
// the dancer sweats up a halyard, reaching high and throwing his weight down on it; the captain takes the helm, steering
// the wheel against the sea. They work harder and quicker as the storm builds.
namespace
{
    constexpr float StormWalk = 165.f;   // cm/s to his station
    // A hand round a line: its thumb's side along the line (ThumbWay), its palm toward PalmWant; the knuckles' way follows
    // (the rig's hands: palm = (along x thumb), mirrored for the right hand: measured on the concertina, 2026-11-04).
    void GripOn(const FVector& ThumbWay, const FVector& PalmWant, float Out, FVector& Along, FVector& Thumb)
    {
        Thumb = ThumbWay.GetSafeNormal();
        const FVector Palm = FVector::VectorPlaneProject(PalmWant, Thumb).GetSafeNormal();
        Along = FVector::CrossProduct(Thumb, Palm * Out).GetSafeNormal();
    }
}

void ADockNPC::SetStormWork(EStormWork Work, const TArray<FVector>& Way, float Yaw, const FVector& RopeA, const FVector& RopeB, float InPhase)
{
    if (Work == EStormWork::None)
    {
        if (StormWork == EStormWork::None) return;
        // Back to what he was doing, where he was (a calm comes only from the checkpoint menu's fresh start).
        StormWork = EStormWork::None; StormArrived = false; StormBlend = 0.f; StormSpeed = 0.f; JogBlend = 0.f; HelmAngle = 0.f;
        SetActorLocationAndRotation(StormHome.GetLocation(), StormHome.Rotator());
        HomeYaw = StormHomeYaw;
        for (UStaticMeshComponent* P : PirateProps) if (P) P->SetVisibility(true);
        bPlanted = false;
        return;
    }
    if (StormWork == EStormWork::None) { StormHome = GetActorTransform(); StormHomeYaw = HomeYaw; }
    StormWork = Work; StormWay = Way; StormLeg = 0; StormArrived = false; StormYaw = Yaw;
    StormRopeA = RopeA; StormRopeB = RopeB; StormPhase = InPhase; StormClock = 0.f; StormBlend = 0.f;
    // The concertina, the tankard and the cutlasses put away; the fencing over.
    for (UStaticMeshComponent* P : PirateProps) if (P) P->SetVisibility(false);
    bFenceCue = false; bPlanted = false; bTurning = false; TurnHold = 0.f;
    Grip[0] = Grip[1] = .5f;
    StormFistW[0] = StormFistW[1] = StormWantW[0] = StormWantW[1] = FVector::ZeroVector; StormHoldW[0] = StormHoldW[1] = 0.f;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_PIRATE_STORM_WORK %s work=%d legs=%d"), *DisplayName, int32(Work), Way.Num());
}

void ADockNPC::TickStormWork(float DeltaSeconds)
{
    StormClock += DeltaSeconds;
    if (!StormArrived)
    {
        const FVector Me = GetActorLocation();
        if (StormLeg < StormWay.Num())
        {
            // Along his way, each leg straight (kept on his own deck's level), turning as he goes.
            const FVector To(StormWay[StormLeg].X, StormWay[StormLeg].Y, Me.Z);
            const float Dist = static_cast<float>(FVector::Dist2D(To, Me));
            const bool bLast = StormLeg == StormWay.Num() - 1;
            if (Dist < 5.f) { ++StormLeg; }
            else
            {
                StormSpeed = StormWalk * (bLast ? FMath::Clamp(Dist / 70.f, .35f, 1.f) : 1.f);
                SetActorLocation(Me + (To - Me).GetSafeNormal2D() * FMath::Min(Dist, StormSpeed * DeltaSeconds));
                TurnZombie(To, DeltaSeconds, 420.f);
            }
        }
        else
        {
            // There: turn to his work.
            StormSpeed = 0.f;
            const float Current = static_cast<float>(GetActorRotation().Yaw);
            const float Left = FMath::FindDeltaAngleDegrees(Current, StormYaw);
            const float Step = FMath::Clamp(Left, -260.f * DeltaSeconds, 260.f * DeltaSeconds);
            SetActorRotation(FRotator(0.f, Current + Step, 0.f));
            if (FMath::Abs(Left) < 2.f)
            {
                SetActorRotation(FRotator(0.f, StormYaw, 0.f)); HomeYaw = StormYaw; StormArrived = true;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_PIRATE_STORM_STATION %s after_s=%.1f"), *DisplayName, StormClock);
            }
        }
    }
    JogBlend = FMath::FInterpTo(JogBlend, StormSpeed > 30.f ? 1.f : 0.f, DeltaSeconds, 6.f);
    JogTime += DeltaSeconds * StormSpeed / FMath::Max(ChefJogClip * HipScale, 1.f);
    StormBlend = FMath::FInterpConstantTo(StormBlend, StormArrived ? 1.f : 0.f, DeltaSeconds, 1.3f);
    if (StormWork == EStormWork::Helm)
    {
        // Steering: the wheel worked to and fro against the sea (a long sweep, quicker corrections over it), wider in the
        // full gale and wider still once it has intensified.
        const float T = GetWorld()->GetTimeSeconds();
        const float Sweep = 16.f + 10.f * FMath::Clamp(StormForce - 1.f, 0.f, 1.f);
        HelmAngle = StormBlend * (Sweep * FMath::Sin(T * 2.f * PI / 6.8f) + .35f * Sweep * FMath::Sin(T * 2.f * PI / 2.6f + 1.3f));
    }
    // Jeffries' storm is AShipCrew's (he stays tied); the others' work has no talk.
    TalkBlend = 0.f;
}

namespace
{
    // Where in his heave (or his pull on the halyard) he is: shared by the men on one line (world time, their phase).
    float StormCycle(const UWorld* World, float Phase, float Force, float Slow, float Quick)
    {
        const float Period = FMath::Lerp(Slow, Quick, FMath::Clamp((Force - .5f) / 1.5f, 0.f, 1.f));
        return Frac(static_cast<float>(World->GetTimeSeconds()) / Period + Phase);
    }
}

void ADockNPC::StormDelta(TArray<FQuat>& Delta, TArray<FQuat>& BoneDelta, FVector& HipsOffset)
{
    const float Work = StormBlend;
    for (FQuat& Q : BoneDelta) Q = FQuat::Slerp(FQuat::Identity, Q, FMath::Lerp(1.f, .35f, Work));
    HipsOffset *= FMath::Lerp(1.f, .35f, Work);
    PirateDip = 0.f;
    if (Work <= 0.f) return;
    switch (StormWork)
    {
    case EStormWork::Haul:
    {
        // Reaching forward along the line, then heaving back on it, his weight thrown back, knees bent.
        const float U = StormCycle(GetWorld(), StormPhase, StormForce, 2.8f, 2.05f);
        const float Pull = U < .5f ? Ease01(U / .5f) : 1.f - Ease01((U - .5f) / .5f);
        const float Lean = Work * (5.f - 19.f * Pull);   // + forward, - back
        Delta[Spine1] = Pitch(.35f * Lean) * Delta[Spine1];
        Delta[Spine2] = Pitch(.35f * Lean) * Delta[Spine2];
        Delta[Chest] = Pitch(.3f * Lean) * Delta[Chest];
        Delta[Head] = Pitch(-.5f * Lean + 6.f * Work) * Delta[Head];
        HipsOffset.X -= Work * (3.f + 13.f * Pull);
        HipsOffset.Z -= Work * (8.f + 6.f * Pull);
        break;
    }
    case EStormWork::Sweat:
    {
        // Up on his toes reaching for the line high, then dropping his weight down onto it.
        const float U = StormCycle(GetWorld(), StormPhase, StormForce, 2.6f, 1.9f);
        const float Up = U < .3f ? Ease01(U / .3f) : U < .4f ? 1.f : 1.f - Ease01((U - .4f) / .45f);
        const float Down = U < .4f ? 0.f : U < .85f ? Ease01((U - .4f) / .45f) : 1.f - Ease01((U - .85f) / .15f);
        Delta[Spine1] = Pitch(Work * (-3.f * Up + 9.f * Down)) * Delta[Spine1];
        Delta[Spine2] = Pitch(Work * (-3.f * Up + 7.f * Down)) * Delta[Spine2];
        Delta[Head] = Pitch(Work * (-22.f * Up + 6.f * Down)) * Delta[Head];
        HipsOffset.Z += Work * (3.f * Up - 15.f * Down);
        break;
    }
    case EStormWork::Helm:
    {
        // Braced behind the wheel, leaning a little into it, his body going with it as he heaves it round.
        Delta[Spine1] = Roll(-.12f * HelmAngle) * Pitch(3.f * Work) * Delta[Spine1];
        Delta[Chest] = Roll(-.1f * HelmAngle) * Pitch(3.f * Work) * Delta[Chest];
        Delta[Head] = Pitch(-5.f * Work) * Roll(.08f * HelmAngle) * Delta[Head];
        HipsOffset.Z -= 5.f * Work;
        break;
    }
    default: break;
    }
}

void ADockNPC::PoseStorm(TArray<FTransform>& Space)
{
    const float Work = StormBlend;
    StormHoldW[0] = StormHoldW[1] = 0.f;
    if (Work <= 0.f) return;   // (walking there: the jog alone, arms free)
    const FTransform ToWorld = Body->GetComponentTransform();
    const auto OutOf = [this](int32 Side) { return Side == 0 ? ArmOut : -ArmOut; };
    // The feet: a long stance on the line (his left forward), square and wide at the halyard and the wheel.
    {
        FVector Ankles[2]; FQuat Feet[2];
        for (int32 Side = 0; Side < 2; ++Side)
        {
            FVector Off(0.f, OutOf(Side) * 6.f, 0.f);
            if (StormWork == EStormWork::Haul) Off = Side == 0 ? FVector(22.f, OutOf(Side) * 3.f, 0.f) : FVector(-28.f, OutOf(Side) * 7.f, 0.f);
            else if (StormWork == EStormWork::Helm) Off = FVector(0.f, OutOf(Side) * 7.f, 0.f);
            Ankles[Side] = AnkleStand[Side] + Off * Work;
            Feet[Side] = (StormWork == EStormWork::Haul && Side == 1 ? Yaw(OutOf(Side) * 25.f * Work) : FQuat::Identity) * FootRest[Side];
        }
        KeepFeetOn(Space, Ankles, Feet, FVector(1.f, 0.f, 0.f));
    }
    FVector Want[2], ThumbWay[2], PalmWay[2], Pole[2];
    float G[2] = { 1.f, 1.f };
    const FVector ChestAt = Space[BoneIndex[Chest]].GetLocation();
    const FVector Shoulder[2] = { Space[BoneIndex[UpperL]].GetLocation(), Space[BoneIndex[UpperR]].GetLocation() };
    // Every grip within his arm's reach: on its line (From, Dir), the point nearest the one wanted (S along it) that his
    // shoulder reaches (the halfling's short arms at the wheel; a long reach forward along a line).
    float ArmReach[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector Up = Space[BoneIndex[Of(UpperL, Side)]].GetLocation(), Lo = Space[BoneIndex[Of(LowerL, Side)]].GetLocation(), Ha = Space[BoneIndex[Of(HandL, Side)]].GetLocation();
        ArmReach[Side] = static_cast<float>(FVector::Dist(Up, Lo) + FVector::Dist(Lo, Ha)) * .88f + 4.f * FistScale;
    }
    const auto WithinReach = [&](int32 Side, const FVector& From, const FVector& Dir, float S)
    {
        const FVector D = From - Shoulder[Side];
        const float B = static_cast<float>(FVector::DotProduct(Dir, D)), C = static_cast<float>(D.SizeSquared()) - ArmReach[Side] * ArmReach[Side];
        const float Disc = B * B - C;
        if (Disc < 0.f) return -B;   // (out of reach all along it: its nearest point)
        const float R = FMath::Sqrt(Disc);
        return FMath::Clamp(S, -B - R, -B + R);
    };
    switch (StormWork)
    {
    case EStormWork::Haul:
    {
        // Both hands on the line before him, the forward one his left: in the heave both draw back toward him; then the
        // forward hand lets go and reaches on along it, and the other follows up behind it.
        const FVector A = ToWorld.InverseTransformPosition(StormRopeA), B = ToWorld.InverseTransformPosition(StormRopeB);
        const FVector Dir = (A - B).GetSafeNormal();
        const float S0 = static_cast<float>(FVector::DotProduct(ChestAt - B, Dir));
        const float U = StormCycle(GetWorld(), StormPhase, StormForce, 2.8f, 2.05f);
        float SF = 62.f, SB = 36.f, LiftF = 0.f, LiftB = 0.f;
        if (U < .5f) { const float E = Ease01(U / .5f); SF = FMath::Lerp(62.f, 36.f, E); SB = FMath::Lerp(36.f, 10.f, E); }
        else
        {
            const float V = (U - .5f) / .5f;
            if (V < .5f) { const float E = Ease01(V / .5f); SF = FMath::Lerp(36.f, 62.f, E); SB = 10.f; LiftF = 6.f * FMath::Sin(PI * V / .5f); G[0] = FMath::Lerp(1.f, .35f, FMath::Sin(PI * V / .5f)); }
            else { const float E = Ease01((V - .5f) / .5f); SF = 62.f; SB = FMath::Lerp(10.f, 36.f, E); LiftB = 6.f * FMath::Sin(PI * (V - .5f) / .5f); G[1] = FMath::Lerp(1.f, .35f, FMath::Sin(PI * (V - .5f) / .5f)); }
        }
        const float F = WithinReach(0, B, Dir, S0 + SF);
        const float Bk = WithinReach(1, B, Dir, FMath::Min(S0 + SB, F - 13.f));
        Want[0] = B + Dir * F + FVector(0.f, 0.f, LiftF);
        Want[1] = B + Dir * Bk + FVector(0.f, 0.f, LiftB);
        for (int32 Side = 0; Side < 2; ++Side) { ThumbWay[Side] = Dir; PalmWay[Side] = FVector(0.f, 0.f, -1.f); Pole[Side] = FVector(-.3f, OutOf(Side), -1.f); }
        break;
    }
    case EStormWork::Sweat:
    {
        // The halyard straight up before him: both hands high on it, then hauled down to his waist (his left above his right).
        const FVector A = ToWorld.InverseTransformPosition(StormRopeA), B = ToWorld.InverseTransformPosition(StormRopeB);
        const FVector Dir = (B - A).GetSafeNormal();
        const float U = StormCycle(GetWorld(), StormPhase, StormForce, 2.6f, 1.9f);
        const float High = static_cast<float>(FMath::Max(Shoulder[0].Z, Shoulder[1].Z)) + 40.f;
        const float Low = static_cast<float>(Space[BoneIndex[Pelvis]].GetLocation().Z) + 22.f;
        float H = Low;
        if (U < .3f) { H = FMath::Lerp(Low, High, Ease01(U / .3f)); G[0] = G[1] = .3f; }
        else if (U < .4f) { H = High; G[0] = G[1] = FMath::Lerp(.3f, 1.f, (U - .3f) / .1f); }
        else if (U < .85f) H = FMath::Lerp(High, Low, Ease01((U - .4f) / .45f));
        const float Per = 1.f / FMath::Max(static_cast<float>(Dir.Z), .2f);   // along the line a cm of height
        const float S0 = WithinReach(0, A, Dir, (H + 7.f - static_cast<float>(A.Z)) * Per);
        const float S1 = WithinReach(1, A, Dir, FMath::Min((H - 7.f - static_cast<float>(A.Z)) * Per, S0 - 13.f * Per));
        Want[0] = A + Dir * S0; Want[1] = A + Dir * S1;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            ThumbWay[Side] = Dir;
            PalmWay[Side] = FVector::VectorPlaneProject(Want[Side] - Shoulder[Side], FVector::UpVector).GetSafeNormal();
            Pole[Side] = FVector(-.2f, OutOf(Side), -.6f);
        }
        break;
    }
    case EStormWork::Helm:
    {
        // On the two spokes either side just below the hub (the eighth and the second below level), round as the wheel turns.
        const FVector Hub = ToWorld.InverseTransformPosition(StormRopeA);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            // (Spoke 9 points to the ship's port side and spoke 5 to starboard, both a little below the hub, when the wheel is square.)
            const int32 K = (OutOf(Side) * static_cast<float>(ToWorld.InverseTransformVector(FVector(0, 1, 0)).Y)) > 0.f ? 9 : 5;
            const FVector Spoke = ToWorld.InverseTransformVectorNoScale(Ship::HelmSpoke(K, HelmAngle)).GetSafeNormal();
            Want[Side] = Hub + Spoke * FMath::Clamp(WithinReach(Side, Hub, Spoke, 32.f), 16.f, 66.f);
            ThumbWay[Side] = Spoke;
            PalmWay[Side] = FVector(1.f, 0.f, 0.f);
            Pole[Side] = FVector(-.4f, OutOf(Side), -.7f);
        }
        break;
    }
    default: return;
    }
    for (int32 Side = 0; Side < 2; ++Side)
    {
        FVector Along, Thumb, Now, NowAlong, NowThumb;
        GripOn(ThumbWay[Side], PalmWay[Side], OutOf(Side), Along, Thumb);
        // (A line can be gripped either way round: the way that keeps his knuckles going on from his forearm, not bent
        // back toward his shoulder, as the hand reaching across his body would be.)
        if (FVector::DotProduct(Along, Want[Side] - Shoulder[Side]) < 0.f) GripOn(-ThumbWay[Side], PalmWay[Side], OutOf(Side), Along, Thumb);
        // (Eased in from where his arms hung as he walked up.)
        HandFrame(Space, Side, Now, NowAlong, NowThumb);
        const FVector Fist = FMath::Lerp(Now, Want[Side], Work);
        PlaceHand(Space, Side, Fist, FMath::Lerp(NowAlong, Along, Work).GetSafeNormal(), FMath::Lerp(NowThumb, Thumb, Work).GetSafeNormal(), Pole[Side].GetSafeNormal());
        Grip[Side] = FMath::Lerp(.5f, G[Side], Work);
        StormHoldW[Side] = Work * FMath::Clamp((G[Side] - .35f) / .65f, 0.f, 1.f);
        FVector P, A2, T2;
        HandFrame(Space, Side, P, A2, T2);
        StormFistW[Side] = ToWorld.TransformPosition(P);
        StormWantW[Side] = ToWorld.TransformPosition(Want[Side]);
        if (Work >= 1.f && Clock > 6.f) WorstPropGrip = FMath::Max(WorstPropGrip, static_cast<float>(FVector::Dist(P, Want[Side])));
    }
}
