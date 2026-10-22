#include "ChultTempleInterior.h"
#include "ChultTempleTraps.h"
#include "ChultTempleRun.h"
#include "ChultTempleWind.h"
#include "TempleSkeleton.h"
#include "TempleSnake.h"
#include "ChultTemple.h"
#include "ChultCanyon.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ClayJar.h"
#include "ChuckBreakable.h"
#include "DockFire.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"

// The temple interior (Claude, 2026-10-20; docs/CHULT-TEMPLE-INTERIOR.md).
// Laid out in the canyon frame like the exterior (ChultTemple.cpp): S toward
// the temple, T across, Z above the plaza. The spaces are lined from inside:
// their faces look inward, the masonry between them is never seen.
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float PZ(){return ChultTemplePlazaZ();}
    // Frame (S,T, Z above the plaza) to local (relative to FloorOrigin), and back.
    FVector At(float S,float T,float Z){return ChultCanyonFrame(S,T,PZ()+Z);}
    FVector AtV(const FVector& F){return At(F.X,F.Y,F.Z);}
    FVector ToFrame(const FVector& Local){const FVector P=Local-ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z-PZ());}
    FVector WorldFrame(const FVector& World){return ToFrame(World-AChultLanding::FloorOrigin());}
    FVector WorldAt(float S,float T,float Z){return AChultLanding::FloorOrigin()+At(S,T,Z);}
    float YawOf(float DS,float DT){return (Along*DS+Across*DT).Rotation().Yaw;}

    // ---------------------------------------------------------------- layout
    constexpr float Rise=16;
    // The sanctuary's cella (ChultTemple.cpp: TowerFront+60 .. +360, CellHalf 260) and its new back door.
    constexpr float SanctuaryFront=77750;   // the sanctuary's front face (TowerFront)
    constexpr float CellaS0=77810,CellaS1=78110,CellaHalf=260,CellaZ=1500,CellaTop=1800;
    constexpr float BackDoorHalf=100,BackDoorTop=240;
    // The hidden stair: flight A down +S, a landing, flight B back down -S, a landing on the well.
    constexpr float FlightATop=78200,FlightAFoot=78800,FlightAZ=1500,L1Z=1100;
    constexpr float L1S1=79000,L1T0=-100,L1T1=340,L1H=240;
    constexpr float FlightBT0=140,FlightBT1=340,FlightBTop=78800,FlightBFoot=77960,L2Z=540;
    constexpr float L2S0=77760,L2H=250,FlightH=250;
    // The well: 12 m square, a gallery along its -T wall, a stair round its walls, a pit to the Astral.
    constexpr float WellS0=77420,WellS1=78620,WellT0=500,WellT1=1700,WellTop=960,WellRingZ=-1460,WellDeep=-2300;
    constexpr float WellStair=200,WellBottom=WellRingZ-60;
    constexpr float PitS0=WellS0+WellStair,PitS1=WellS1-WellStair,PitT0=WellT0+WellStair,PitT1=WellT1-WellStair;
    // Its door into the Great Hall, centred on the hall's axis.
    constexpr float AxisT=1100,DoorHalf=110,DoorH=300,Floor=-1460;
    // The Great Hall (2D map 1).
    constexpr float HallS0=73400,HallS1=77360,HallT0=300,HallT1=1900,HallTop=-560;
    // The spike corridor (map 2), through 60 cm walls at both ends.
    constexpr float KS0=69460,KS1=73340,KT0=900,KT1=1300,KTop=-1060,PassH=280;
    const float SpikeS[]={72700,72000,71300,70600,69900};
    const float SpikePhase[]={0.f,1.5f,.4f,1.9f,.8f};
    constexpr float SpikeHalfDepth=80;
    // The pier hall (map 3) and its rift.
    // The rift is 5.5 m: too far for a sprint leap or a run along a side wall, so
    // the rings are the only way over (user 2026-10-20; it had a slab bridge too).
    constexpr float PS0=66400,PS1=69400,PT0=-300,PT1=2500,PTop=-660,RiftS0=67450,RiftS1=68000;
    constexpr float RingT=2100,RingZ=Floor+100;
    const float RingS[]={68030,67890,67750,67610,67470};
    // The dart gallery (map 4), turning west (-T), and the choked way on.
    constexpr float DS0=66720,DS1=67080,DT1=-360,DT0=-5560,DTop=-1060,ChokeT=-5160;
    // The snake chamber (2D map 5, October 21): through the gallery's arch (no longer choked), broad, its lanes
    // looping round five piers; its way on (map 6) through an arch in the +S wall, choked.
    constexpr float NS0=65900,NS1=67900,NT0=-8360,NT1=DT0,NTop=-660,ExitT=-7950,ExitHalf=110,ExitDeep=380;
    const FVector2D SnakePiers[]={{66400,-6300},{67400,-6300},{66900,-7050},{66350,-7750},{67450,-7650}};
    constexpr float SnakePierHalf=70;
    constexpr int32 Launchers=8;
    float LauncherT(int32 K){return -1100-540*K;}
    // The cracked slabs (2026-10-20, moved from the rift): three across a pit in the
    // dart gallery's floor before its first launcher.
    constexpr float PitG0=-830,PitG1=-560,SlabW=90;

    // A space: a frame-aligned box, or a flight along S whose floor runs from ZA at S0 to ZB at S1.
    struct FSpace{const TCHAR* Name;float S0,S1,T0,T1,Z0,Z1;ETempleInteriorSpawn Spawn;bool bCuts;bool bFlight;float ZA,ZB,H;};
    const TArray<FSpace>& Spaces()
    {
        static const TArray<FSpace> List={
            {TEXT("Cella"),CellaS0,CellaS1,-CellaHalf,CellaHalf,CellaZ,CellaTop,ETempleInteriorSpawn::Well,false,false,0,0,0},
            {TEXT("BackDoor"),CellaS1,FlightATop,-BackDoorHalf,BackDoorHalf,CellaZ,CellaZ+BackDoorTop,ETempleInteriorSpawn::Well,true,false,0,0,0},
            {TEXT("FlightA"),FlightATop,FlightAFoot,-BackDoorHalf,BackDoorHalf,L1Z,FlightAZ+FlightH,ETempleInteriorSpawn::Well,true,true,FlightAZ,L1Z,FlightH},
            {TEXT("Landing1"),FlightAFoot,L1S1,L1T0,L1T1,L1Z,L1Z+L1H,ETempleInteriorSpawn::Well,true,false,0,0,0},
            {TEXT("FlightB"),FlightBFoot,FlightBTop,FlightBT0,FlightBT1,L2Z,L1Z+FlightH,ETempleInteriorSpawn::Well,true,true,L2Z,L1Z,FlightH},
            {TEXT("Landing2"),L2S0,FlightBFoot,FlightBT0,WellT0,L2Z,L2Z+L2H,ETempleInteriorSpawn::Well,true,false,0,0,0},
            {TEXT("Well"),WellS0,WellS1,WellT0,WellT1,WellDeep-200,WellTop,ETempleInteriorSpawn::Well,true,false,0,0,0},
            {TEXT("WellDoor"),HallS1,WellS0,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH,ETempleInteriorSpawn::Hall,false,false,0,0,0},
            {TEXT("Hall"),HallS0,HallS1,HallT0,HallT1,Floor,HallTop,ETempleInteriorSpawn::Hall,false,false,0,0,0},
            {TEXT("HallDoor"),KS1,HallS0,KT0,KT1,Floor,Floor+PassH,ETempleInteriorSpawn::Spikes,false,false,0,0,0},
            {TEXT("Spikes"),KS0,KS1,KT0,KT1,Floor,KTop,ETempleInteriorSpawn::Spikes,false,false,0,0,0},
            {TEXT("SpikeDoor"),PS1,KS0,KT0,KT1,Floor,Floor+PassH,ETempleInteriorSpawn::Piers,false,false,0,0,0},
            {TEXT("Piers"),PS0,PS1,PT0,PT1,WellDeep-200,PTop,ETempleInteriorSpawn::Piers,false,false,0,0,0},
            {TEXT("PierDoor"),DS0,DS1,DT1,PT0,Floor,Floor+PassH,ETempleInteriorSpawn::Darts,false,false,0,0,0},
            {TEXT("Darts"),DS0,DS1,ChokeT,DT1,WellDeep-200,DTop,ETempleInteriorSpawn::Darts,false,false,0,0,0},
            {TEXT("SnakeArch"),DS0,DS1,DT0,ChokeT,Floor,DTop,ETempleInteriorSpawn::Snakes,false,false,0,0,0},   // the gallery's last 4 m, past its arch: the snake chamber's threshold
            // (The collapse beyond the chamber's +S arch comes first: a death within its margin is the climb's.)
            {TEXT("Collapse"),NS1,TempleRun::CollapseS1,ExitT-ExitHalf,ExitT+ExitHalf,Floor,TempleRun::CollapseTop,ETempleInteriorSpawn::Climb,false,false,0,0,0},
            {TEXT("Snakes"),NS0,NS1,NT0,NT1,Floor,NTop,ETempleInteriorSpawn::Snakes,false,false,0,0,0},
            // The trap run (October 22, ChultTempleRun.h): the chimney, then each section its own respawn.
            {TEXT("Shaft"),TempleRun::BS0,TempleRun::BS1,TempleRun::ShaftT0,TempleRun::ShaftT1,TempleRun::RubbleTop,TempleRun::UZ,ETempleInteriorSpawn::Climb,false,false,0,0,0},
            {TEXT("RunPits"),TempleRun::BS0,TempleRun::BS1,TempleRun::ShaftT1,TempleRun::BT1,TempleRun::UZ-TempleRun::PitDepth-200,TempleRun::Top,ETempleInteriorSpawn::Pits,false,false,0,0,0},
            {TEXT("RunPitAlcove"),TempleRun::BS0-TempleRun::BreakDeep,TempleRun::BS0,TempleRun::BreakT0,TempleRun::BreakT1,TempleRun::UZ-TempleRun::PitDepth-200,TempleRun::Top,ETempleInteriorSpawn::Pits,false,false,0,0,0},
            {TEXT("RunAxes"),TempleRun::CS0,TempleRun::CS1,TempleRun::CT0,TempleRun::CT1,TempleRun::UZ-TempleRun::TrenchDepth-200,TempleRun::CTop,ETempleInteriorSpawn::Axes,false,false,0,0,0},
            {TEXT("RunFire"),TempleRun::DS0,TempleRun::DS1,TempleRun::DT0,TempleRun::DT1,TempleRun::UZ,TempleRun::UZ+400,ETempleInteriorSpawn::Fire,false,false,0,0,0},
            {TEXT("RunSaws"),TempleRun::ES0,TempleRun::ES1,TempleRun::ET0,TempleRun::ET1,TempleRun::UZ,TempleRun::ETop,ETempleInteriorSpawn::Saws,false,false,0,0,0},
            {TEXT("RunGauntlet"),TempleRun::FS0,TempleRun::FS1,TempleRun::FT0,TempleRun::FT1,TempleRun::UZ-TempleRun::PitDepth-200,TempleRun::FTop,ETempleInteriorSpawn::Gauntlet,false,false,0,0,0},
            {TEXT("RunGauntletAxes"),TempleRun::GAxeS0,TempleRun::GAxeS1,TempleRun::GAxeT0,TempleRun::GAxeT1,TempleRun::UZ-TempleRun::TrenchDepth-200,TempleRun::UZ+420,ETempleInteriorSpawn::Gauntlet,false,false,0,0,0},
            {TEXT("RunAnte"),TempleRun::AnteS0,TempleRun::AnteS1,TempleRun::AnteT0,TempleRun::AnteT1,TempleRun::UZ,TempleRun::AnteTop,ETempleInteriorSpawn::Gauntlet,false,false,0,0,0},
            // The Astral wind (October 22, ChultTempleWind.h): the bends first (their corners hold the respawns), then the
            // legs (leg 1 from the antechamber's old choke), leg 3's two alcoves, then the sanctum.
            {TEXT("WindBend1"),TempleWind::Bends[0].S0,TempleWind::Bends[0].S1,TempleWind::Bends[0].T0,TempleWind::Bends[0].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind2,false,false,0,0,0},
            {TEXT("WindBend2"),TempleWind::Bends[1].S0,TempleWind::Bends[1].S1,TempleWind::Bends[1].T0,TempleWind::Bends[1].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind4,false,false,0,0,0},
            {TEXT("WindBend3"),TempleWind::Bends[2].S0,TempleWind::Bends[2].S1,TempleWind::Bends[2].T0,TempleWind::Bends[2].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind6,false,false,0,0,0},
            {TEXT("WindLeg1"),TempleRun::FS0,TempleRun::FS1,TempleRun::AnteT1,TempleWind::Legs[0].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind1,false,false,0,0,0},
            {TEXT("WindLeg2"),TempleWind::Legs[1].S0,TempleWind::Legs[1].S1,TempleWind::Legs[1].T0,TempleWind::Legs[1].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind3,false,false,0,0,0},
            {TEXT("WindLeg3"),TempleWind::Legs[2].S0,TempleWind::Legs[2].S1,TempleWind::Legs[2].T0,TempleWind::Legs[2].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind5,false,false,0,0,0},
            {TEXT("WindLeg3MinusAlcove"),TempleWind::Legs[2].S0-TempleWind::BreakDeep,TempleWind::Legs[2].S0,TempleWind::BreakMinusT0,TempleWind::BreakMinusT1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind5,false,false,0,0,0},
            {TEXT("WindLeg3PlusAlcove"),TempleWind::Legs[2].S1,TempleWind::Legs[2].S1+TempleWind::BreakDeep,TempleWind::BreakPlusT0,TempleWind::BreakPlusT1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind5,false,false,0,0,0},
            {TEXT("WindLeg4"),TempleWind::Legs[3].S0,TempleWind::Legs[3].S1,TempleWind::Legs[3].T0,TempleWind::Legs[3].T1,TempleWind::UZ-400,TempleWind::Top,ETempleInteriorSpawn::Wind7,false,false,0,0,0},
            {TEXT("Sanctum"),TempleWind::HS0,TempleWind::HS1,TempleWind::HT0,TempleWind::HT1,TempleWind::HZ,TempleWind::HTop,ETempleInteriorSpawn::Sanctum,false,false,0,0,0}};
        return List;
    }
    // A flight's floor line at S.
    float FlightLine(const FSpace& Sp,float S){return FMath::Lerp(Sp.ZA,Sp.ZB,FMath::Clamp((S-Sp.S0)/(Sp.S1-Sp.S0),0.f,1.f));}
    bool InSpace(const FSpace& Sp,const FVector& F,float Margin)
    {
        if(F.X<Sp.S0-Margin||F.X>Sp.S1+Margin||F.Y<Sp.T0-Margin||F.Y>Sp.T1+Margin)return false;
        if(!Sp.bFlight)return F.Z>Sp.Z0-Margin&&F.Z<Sp.Z1+Margin;
        const float L=FlightLine(Sp,F.X);return F.Z>L-Rise-Margin&&F.Z<L+Sp.H+Margin;
    }
    const FSpace* SpaceAt(const FVector& F,float Margin)
    {
        for(const FSpace& Sp:Spaces())if(InSpace(Sp,F,Margin))return &Sp;
        return nullptr;
    }

    struct FSpawnSpot{float S,T,Z,DS,DT;const TCHAR* Name;};
    FSpawnSpot SpawnSpot(ETempleInteriorSpawn Spawn)
    {
        switch(Spawn)
        {
        case ETempleInteriorSpawn::Well:return {(L2S0+FlightBFoot)*.5f,WellT0+100,L2Z,1,0,TEXT("Temple Well")};
        case ETempleInteriorSpawn::Hall:return {HallS1-220,AxisT,Floor,-1,0,TEXT("Temple Hall")};
        case ETempleInteriorSpawn::Spikes:return {KS1-150,AxisT,Floor,-1,0,TEXT("Temple Spikes")};
        case ETempleInteriorSpawn::Piers:return {PS1-150,AxisT,Floor,-1,0,TEXT("Temple Piers")};
        case ETempleInteriorSpawn::Darts:return {(DS0+DS1)*.5f,DT1-70,Floor,0,-1,TEXT("Temple Darts")};
        case ETempleInteriorSpawn::Snakes:return {(DS0+DS1)*.5f,ChokeT-130,Floor,0,-1,TEXT("Temple Snakes")};   // between the arch and the chamber, out of the snakes' sense
        // The trap run (October 22): the rubble's foot, then the start of each section.
        case ETempleInteriorSpawn::Climb:return {NS1+52,ExitT,Floor,1,0,TEXT("Temple Climb")};
        case ETempleInteriorSpawn::Pits:return {(TempleRun::BS0+TempleRun::BS1)*.5f,-7650,TempleRun::UZ,0,1,TEXT("Temple Pits")};
        case ETempleInteriorSpawn::Axes:return {68650,(TempleRun::CausewayT0+TempleRun::CausewayT1)*.5f,TempleRun::UZ,1,0,TEXT("Temple Axes")};
        case ETempleInteriorSpawn::Fire:return {(TempleRun::DS0+TempleRun::DS1)*.5f,-4900,TempleRun::UZ,0,-1,TEXT("Temple Fire")};
        case ETempleInteriorSpawn::Saws:return {(TempleRun::DS0+TempleRun::DS1)*.5f,(TempleRun::ET0+TempleRun::ET1)*.5f,TempleRun::UZ,1,0,TEXT("Temple Saws")};
        case ETempleInteriorSpawn::Gauntlet:return {(TempleRun::FS0+TempleRun::FS1)*.5f,-7580,TempleRun::UZ,0,1,TEXT("Temple Gauntlet")};
        // The Astral wind (October 22): each leg's start and each bend's (a corner), then the sanctum's gallery.
        case ETempleInteriorSpawn::Wind1:return {(TempleRun::FS0+TempleRun::FS1)*.5f,-3000,TempleWind::UZ,0,1,TEXT("Astral Wind 1")};
        case ETempleInteriorSpawn::Wind2:return {(TempleRun::FS0+TempleRun::FS1)*.5f,-710,TempleWind::UZ,1,0,TEXT("Astral Wind 2")};
        case ETempleInteriorSpawn::Wind3:return {(TempleWind::Legs[1].S0+TempleWind::Legs[1].S1)*.5f,-1000,TempleWind::UZ,0,-1,TEXT("Astral Wind 3")};
        case ETempleInteriorSpawn::Wind4:return {(TempleWind::Legs[1].S0+TempleWind::Legs[1].S1)*.5f,-3150,TempleWind::UZ,1,0,TEXT("Astral Wind 4")};
        case ETempleInteriorSpawn::Wind5:return {(TempleWind::Legs[2].S0+TempleWind::Legs[2].S1)*.5f,-2860,TempleWind::UZ,0,1,TEXT("Astral Wind 5")};
        case ETempleInteriorSpawn::Wind6:return {(TempleWind::Legs[2].S0+TempleWind::Legs[2].S1)*.5f,-710,TempleWind::UZ,1,0,TEXT("Astral Wind 6")};
        case ETempleInteriorSpawn::Wind7:return {(TempleWind::Legs[3].S0+TempleWind::Legs[3].S1)*.5f,-1000,TempleWind::UZ,0,-1,TEXT("Astral Wind 7")};
        default:return {TempleWind::HallMid,TempleWind::HT1-220,TempleWind::UZ,0,-1,TEXT("Temple Sanctum")};
        }
    }

    // ---------------------------------------------------------------- building
    struct FSurf
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;
        void Add(const FVector& P,const FVector& Normal)
        {
            V.Add(P);N.Add(Normal);C.Add(FColor::White);
            if(FMath::Abs(Normal.Z)>.7f)UV.Add(FVector2D(P.X,P.Y)/200);
            else{const FVector Axis=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();UV.Add(FVector2D(FVector::DotProduct(P,Axis),P.Z)/200);}
        }
        // The outward normal is (B-A)x(C-A); UE draws the reversed winding.
        void Quad(const FVector& A,const FVector& B,const FVector& Cc,const FVector& D)
        {
            const FVector Normal=FVector::CrossProduct(B-A,Cc-A).GetSafeNormal();if(Normal.IsNearlyZero())return;
            const int32 I=V.Num();for(const FVector* P:{&A,&B,&Cc,&D})Add(*P,Normal);
            T.Append({I,I+2,I+1,I,I+3,I+2});
        }
    };
    struct FParts{FSurf Stone,Moss,Plaster,Steps,StepMoss,Ramp,Dark,Iron,Wood,Astral,Block,Veil;};
    // A box in the frame (closed, faces outward).
    void Box(FSurf& M,float S0,float S1,float T0,float T1,float Z0,float Z1)
    {
        if(S1<S0)Swap(S0,S1);if(T1<T0)Swap(T0,T1);if(Z1<Z0)Swap(Z0,Z1);
        M.Quad(At(S0,T0,Z1),At(S1,T0,Z1),At(S1,T1,Z1),At(S0,T1,Z1));
        M.Quad(At(S0,T0,Z0),At(S0,T1,Z0),At(S1,T1,Z0),At(S1,T0,Z0));
        M.Quad(At(S0,T0,Z0),At(S1,T0,Z0),At(S1,T0,Z1),At(S0,T0,Z1));
        M.Quad(At(S1,T0,Z0),At(S1,T1,Z0),At(S1,T1,Z1),At(S1,T0,Z1));
        M.Quad(At(S1,T1,Z0),At(S0,T1,Z0),At(S0,T1,Z1),At(S1,T1,Z1));
        M.Quad(At(S0,T1,Z0),At(S0,T0,Z0),At(S0,T0,Z1),At(S0,T1,Z1));
    }
    // A box turned by Rotation (frame axes) about its centre.
    void TurnedBox(FSurf& M,FVector Center,FVector Half,FRotator Rotation)
    {
        auto C=[&](float X,float Y,float Z){const FVector L=Center+Rotation.RotateVector(FVector(X*Half.X,Y*Half.Y,Z*Half.Z));return At(L.X,L.Y,L.Z);};
        M.Quad(C(-1,-1,1),C(1,-1,1),C(1,1,1),C(-1,1,1));M.Quad(C(-1,-1,-1),C(-1,1,-1),C(1,1,-1),C(1,-1,-1));
        M.Quad(C(-1,-1,-1),C(1,-1,-1),C(1,-1,1),C(-1,-1,1));M.Quad(C(1,-1,-1),C(1,1,-1),C(1,1,1),C(1,-1,1));
        M.Quad(C(1,1,-1),C(-1,1,-1),C(-1,1,1),C(1,1,1));M.Quad(C(-1,1,-1),C(-1,-1,-1),C(-1,-1,1),C(-1,1,1));
    }
    // A quad (frame points) facing Toward.
    void QuadToward(FSurf& M,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& Toward)
    {
        const FVector a=AtV(A),b=AtV(B),c=AtV(C),d=AtV(D),t=AtV(Toward);
        if(FVector::DotProduct(FVector::CrossProduct(b-a,c-a),t-a)>=0)M.Quad(a,b,c,d);else M.Quad(d,c,b,a);
    }
    // A tube along Path (frame), radius R0 to R1, capped.
    void PathTube(FSurf& M,const TArray<FVector>& Path,float R0,float R1,int32 Seg=8)
    {
        TArray<TArray<FVector>> Rings;
        for(int32 I=0;I<Path.Num();++I)
        {
            const FVector D=(Path[FMath::Min(I+1,Path.Num()-1)]-Path[FMath::Max(I-1,0)]).GetSafeNormal();
            const FVector X=FMath::Abs(D.Z)>.9f?FVector::CrossProduct(D,FVector::ForwardVector).GetSafeNormal():FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),Y=FVector::CrossProduct(D,X);
            const float Rr=FMath::Lerp(R0,R1,I/float(FMath::Max(Path.Num()-1,1)));
            TArray<FVector>& Ring=Rings.AddDefaulted_GetRef();
            for(int32 K=0;K<Seg;++K){const float A=K*2*PI/Seg;Ring.Add(Path[I]+(X*FMath::Cos(A)+Y*FMath::Sin(A))*Rr);}
        }
        for(int32 I=0;I+1<Rings.Num();++I)for(int32 K=0;K<Seg;++K)
        {
            const int32 L=(K+1)%Seg;const FVector Mid=(Path[I]+Path[I+1])*.5f;
            QuadToward(M,Rings[I][K],Rings[I+1][K],Rings[I+1][L],Rings[I][L],Mid+(Mid-(Rings[I][K]+Rings[I+1][L])*.5f)*-50);
        }
        for(int32 End:{0,Rings.Num()-1})
        {
            const FVector Out=Path[End]+(End?1:-1)*(Path.Last()-Path[0]).GetSafeNormal()*100;
            for(int32 K=1;K+1<Seg;++K)QuadToward(M,Rings[End][0],Rings[End][K],Rings[End][K+1],Rings[End][0],Out);
        }
    }
    void Drum(FSurf& M,const FVector& A,const FVector& B,float R,int32 Seg=12){PathTube(M,{A,B},R,R,Seg);}

    // Rectangles (U along a wall or S, V across or Z) minus a cut.
    void Subtract(TArray<FBox2D>& Pieces,const FBox2D& Cut)
    {
        TArray<FBox2D> Out;
        for(const FBox2D& P:Pieces)
        {
            if(Cut.Min.X>=P.Max.X||Cut.Max.X<=P.Min.X||Cut.Min.Y>=P.Max.Y||Cut.Max.Y<=P.Min.Y){Out.Add(P);continue;}
            if(Cut.Min.X>P.Min.X)Out.Add(FBox2D(P.Min,FVector2D(Cut.Min.X,P.Max.Y)));
            if(Cut.Max.X<P.Max.X)Out.Add(FBox2D(FVector2D(Cut.Max.X,P.Min.Y),P.Max));
            const float X0=FMath::Max(P.Min.X,Cut.Min.X),X1=FMath::Min(P.Max.X,Cut.Max.X);
            if(Cut.Min.Y>P.Min.Y)Out.Add(FBox2D(FVector2D(X0,P.Min.Y),FVector2D(X1,Cut.Min.Y)));
            if(Cut.Max.Y<P.Max.Y)Out.Add(FBox2D(FVector2D(X0,Cut.Max.Y),FVector2D(X1,P.Max.Y)));
        }
        Pieces.Reset();for(const FBox2D& P:Out)if(P.Max.X-P.Min.X>.5f&&P.Max.Y-P.Min.Y>.5f)Pieces.Add(P);
    }
    // An opening in a lined space's wall: Face 0 the -S wall, 1 the +S wall, 2 the -T wall, 3 the +T wall;
    // U0..U1 along the wall (T for faces 0 and 1, S for 2 and 3), Z0..Z1 above the plaza.
    struct FHole{int32 Face;float U0,U1,Z0,Z1;};
    struct FLining{float Dado=150,Frieze=0;bool bFloor=true,bCeiling=true;FSurf* Low=nullptr;FSurf* High=nullptr;FSurf* Top=nullptr;TArray<FBox2D> FloorHoles;};
    // A space's lining: floor, ceiling and walls facing in, with openings. The
    // walls are Low (stone) to Dado above the floor, High above that, and a
    // frieze band Frieze deep under the ceiling in Top (Low if unset).
    void Line(FParts& P,float S0,float S1,float T0,float T1,float Z0,float Z1,const TArray<FHole>& Holes,const FLining& L)
    {
        const FVector Mid((S0+S1)*.5f,(T0+T1)*.5f,(Z0+Z1)*.5f);
        FSurf& Low=L.Low?*L.Low:P.Stone;FSurf& High=L.High?*L.High:P.Stone;FSurf& TopBand=L.Top?*L.Top:Low;
        if(L.bFloor)
        {
            TArray<FBox2D> Pieces={FBox2D(FVector2D(S0,T0),FVector2D(S1,T1))};for(const FBox2D& H:L.FloorHoles)Subtract(Pieces,H);
            for(const FBox2D& Q:Pieces)QuadToward(P.Stone,{Q.Min.X,Q.Min.Y,Z0},{Q.Max.X,Q.Min.Y,Z0},{Q.Max.X,Q.Max.Y,Z0},{Q.Min.X,Q.Max.Y,Z0},FVector(Q.GetCenter().X,Q.GetCenter().Y,Z0+100));
        }
        if(L.bCeiling)QuadToward(P.Stone,{S0,T0,Z1},{S1,T0,Z1},{S1,T1,Z1},{S0,T1,Z1},FVector(Mid.X,Mid.Y,Z1-100));
        for(int32 F=0;F<4;++F)
        {
            const float U0=F<2?T0:S0,U1=F<2?T1:S1;
            TArray<FBox2D> Pieces={FBox2D(FVector2D(U0,Z0),FVector2D(U1,Z1))};
            for(const FHole& H:Holes)if(H.Face==F)Subtract(Pieces,FBox2D(FVector2D(H.U0,H.Z0),FVector2D(H.U1,H.Z1)));
            auto Pt=[&](float U,float Z){return F==0?FVector(S0,U,Z):F==1?FVector(S1,U,Z):F==2?FVector(U,T0,Z):FVector(U,T1,Z);};
            for(const FBox2D& Q:Pieces)
            {
                // Split at the dado and the frieze.
                const float Cuts[]={Q.Min.Y,FMath::Clamp(Z0+L.Dado,Q.Min.Y,Q.Max.Y),FMath::Clamp(Z1-L.Frieze,Q.Min.Y,Q.Max.Y),Q.Max.Y};
                for(int32 K=0;K<3;++K)
                {
                    if(Cuts[K+1]-Cuts[K]<.5f)continue;
                    FSurf& M=K==1?High:K==2?TopBand:Low;
                    const FVector Toward=Pt((Q.Min.X+Q.Max.X)*.5f,(Cuts[K]+Cuts[K+1])*.5f)+(Mid-Pt((Q.Min.X+Q.Max.X)*.5f,Mid.Z)).GetSafeNormal()*50;
                    QuadToward(M,Pt(Q.Min.X,Cuts[K]),Pt(Q.Max.X,Cuts[K]),Pt(Q.Max.X,Cuts[K+1]),Pt(Q.Min.X,Cuts[K+1]),Toward);
                }
            }
        }
    }
    // A corbel vault over a long space: Courses stepping in Step and up Course
    // from both long walls (Axis 0: the space runs along S, so the walls are at T0
    // and T1), the last course meeting the ceiling at Z1.
    void Corbel(FParts& P,int32 Axis,float S0,float S1,float T0,float T1,float Z1,int32 Courses,float Step,float Course)
    {
        const float Zs=Z1-Courses*Course;
        for(float Side:{-1.f,1.f})for(int32 K=1;K<=Courses;++K)
        {
            const float Za=Zs+(K-1)*Course,Zb=Za+Course;
            auto Pt=[&](float Along_,float In,float Z){const float W=Side<0?(Axis==0?T0:S0)+In:(Axis==0?T1:S1)-In;return Axis==0?FVector(Along_,W,Z):FVector(W,Along_,Z);};
            const float A0=Axis==0?S0:T0,A1=Axis==0?S1:T1;
            // The course's underside and its face.
            QuadToward(P.Stone,Pt(A0,(K-1)*Step,Za),Pt(A1,(K-1)*Step,Za),Pt(A1,K*Step,Za),Pt(A0,K*Step,Za),Pt((A0+A1)*.5f,(K-.5f)*Step,Za-100));
            QuadToward(P.Stone,Pt(A0,K*Step,Za),Pt(A1,K*Step,Za),Pt(A1,K*Step,Zb),Pt(A0,K*Step,Zb),Pt((A0+A1)*.5f,K*Step+100,(Za+Zb)*.5f));
        }
    }

    // A straight flight along S between landings: visible steps (no collision)
    // over a hidden ramp through the treads' middles, its walls and a sloped,
    // corbelled ceiling FlightH over the floor line. The ends are open.
    int32 Flight(FParts& P,float SFoot,float STop,float T0,float T1,float ZFoot,float ZTop,float H,float FootCeiling,float TopCeiling,FRandomStream& R)
    {
        const float Dir=FMath::Sign(STop-SFoot),Len=FMath::Abs(STop-SFoot);
        const int32 N=FMath::RoundToInt((ZTop-ZFoot)/Rise);const float Rr=(ZTop-ZFoot)/N,Tread=Len/N;
        auto S=[&](float U){return SFoot+Dir*U;};
        auto Line_=[&](float U){return ZFoot+U*(ZTop-ZFoot)/Len;};
        for(int32 K=0;K<N;++K)
        {
            const float U0=K*Tread,U1=(K+1)*Tread,Za=ZFoot+K*Rr,Zb=Za+Rr;
            FSurf& M=R.FRand()<.2f?P.StepMoss:P.Steps;
            QuadToward(M,{S(U0),T0,Zb},{S(U1),T0,Zb},{S(U1),T1,Zb},{S(U0),T1,Zb},{S((U0+U1)*.5f),(T0+T1)*.5f,Zb+100});
            QuadToward(M,{S(U0),T0,Za},{S(U0),T1,Za},{S(U0),T1,Zb},{S(U0),T0,Zb},{S(U0-30),(T0+T1)*.5f,(Za+Zb)*.5f});
        }
        // The ramp: from half a tread before the foot to the top, then on 10 cm over the landing.
        {
            const float W0=T0+2,W1=T1-2;
            const float Us[]={-Tread*.5f,Len-Tread*.5f,Len+10};const float Zs[]={ZFoot,ZTop,ZTop};
            for(int32 I=0;I<2;++I)QuadToward(P.Ramp,{S(Us[I]),W0,Zs[I]},{S(Us[I+1]),W0,Zs[I+1]},{S(Us[I+1]),W1,Zs[I+1]},{S(Us[I]),W1,Zs[I]},{S((Us[I]+Us[I+1])*.5f),(W0+W1)*.5f,Zs[I+1]+100});
        }
        // The walls (below the steps' line to the corbels) and the corbelled ceiling, sloping with the floor.
        constexpr int32 Courses=3;constexpr float Step=22,Course=26;
        const float Mt=(T0+T1)*.5f;
        for(float Side:{0.f,1.f})
        {
            const float W=Side?T1:T0,In=Side?-1.f:1.f;
            QuadToward(P.Stone,{S(0),W,ZFoot-24},{S(Len),W,ZTop-24},{S(Len),W,ZTop+H-Courses*Course},{S(0),W,ZFoot+H-Courses*Course},{S(Len*.5f),Mt,Line_(Len*.5f)+H*.5f});
            for(int32 K=1;K<=Courses;++K)
            {
                const float Da=H-(Courses-K+1)*Course,Db=Da+Course,Ia=W+In*(K-1)*Step,Ib=W+In*K*Step;
                QuadToward(P.Stone,{S(0),Ia,ZFoot+Da},{S(Len),Ia,ZTop+Da},{S(Len),Ib,ZTop+Da},{S(0),Ib,ZFoot+Da},{S(Len*.5f),(Ia+Ib)*.5f,Line_(Len*.5f)+Da-100});
                QuadToward(P.Stone,{S(0),Ib,ZFoot+Da},{S(Len),Ib,ZTop+Da},{S(Len),Ib,ZTop+Db},{S(0),Ib,ZFoot+Db},{S(Len*.5f),Mt,Line_(Len*.5f)+(Da+Db)*.5f});
            }
        }
        const float C0=T0+Courses*Step,C1=T1-Courses*Step;
        QuadToward(P.Stone,{S(0),C0,ZFoot+H},{S(Len),C0,ZTop+H},{S(Len),C1,ZTop+H},{S(0),C1,ZFoot+H},{S(Len*.5f),Mt,Line_(Len*.5f)+H-100});
        // A lintel at each end, faced both ways, from where the corbels begin (or the
        // landing's ceiling, if lower) to the flight's ceiling: it closes the corbels'
        // ends and any gap over the landing's lower ceiling.
        auto Cap=[&](float U,float Zl,float Zh)
        {
            if(Zh-Zl<.5f)return;
            for(float Toward:{-1.f,1.f})QuadToward(P.Stone,{S(U),T0,Zl},{S(U),T1,Zl},{S(U),T1,Zh},{S(U),T0,Zh},{S(U+Toward*50),Mt,(Zl+Zh)*.5f});
        };
        Cap(0,FMath::Min(FootCeiling,ZFoot+H-Courses*Course),ZFoot+H);Cap(Len,FMath::Min(TopCeiling,ZTop+H-Courses*Course),ZTop+H);
        return N;
    }
    // A run of the well's stair hugging a wall: Axis 0 runs along S, 1 along T;
    // across from C0 to C1, its open edge (toward the pit) at Open. A solid
    // flight with a soffit 50 cm under it and a fascia on its open side,
    // clipped above Clip (the ring floor under the last run).
    int32 Run(FParts& P,int32 Axis,float UFoot,float UTop,float C0,float C1,float Open,float ZFoot,float ZTop,float Clip,FRandomStream& R)
    {
        const float Dir=FMath::Sign(UTop-UFoot),Len=FMath::Abs(UTop-UFoot);
        const int32 N=FMath::RoundToInt((ZTop-ZFoot)/Rise);const float Rr=(ZTop-ZFoot)/N,Tread=Len/N;
        auto Pt=[&](float U,float C,float Z){const float A=UFoot+Dir*U;return Axis==0?FVector(A,C,Z):FVector(C,A,Z);};
        auto Line_=[&](float U){return ZFoot+U*(ZTop-ZFoot)/Len;};
        auto Soffit=[&](float U){return FMath::Max(Line_(U)-50,Clip);};
        const float Cm=(C0+C1)*.5f,Out=Open==C0?-1.f:1.f;
        for(int32 K=0;K<N;++K)
        {
            const float U0=K*Tread,U1=(K+1)*Tread,Za=ZFoot+K*Rr,Zb=Za+Rr;
            FSurf& M=R.FRand()<.22f?P.StepMoss:P.Steps;
            QuadToward(M,Pt(U0,C0,Zb),Pt(U1,C0,Zb),Pt(U1,C1,Zb),Pt(U0,C1,Zb),Pt((U0+U1)*.5f,Cm,Zb+100));
            QuadToward(M,Pt(U0,C0,Za),Pt(U0,C1,Za),Pt(U0,C1,Zb),Pt(U0,C0,Zb),Pt(U0-30,Cm,(Za+Zb)*.5f));
            // The fascia on the open side, from the soffit up to this tread.
            QuadToward(P.Stone,Pt(U0,Open,Soffit(U0)),Pt(U1,Open,Soffit(U1)),Pt(U1,Open,Zb),Pt(U0,Open,Zb),Pt((U0+U1)*.5f,Open+Out*100,(Za+Zb)*.5f));
        }
        if(Line_(0)-50<Clip||true)
        {
            // The soffit, in pieces so it follows the clip.
            for(int32 I=0;I<8;++I)
            {
                const float U0=Len*I/8.f,U1=Len*(I+1)/8.f;
                if(Line_(U0)-50<=Clip&&Line_(U1)-50<=Clip)continue;
                QuadToward(P.Stone,Pt(U0,C0,Soffit(U0)),Pt(U1,C0,Soffit(U1)),Pt(U1,C1,Soffit(U1)),Pt(U0,C1,Soffit(U0)),Pt((U0+U1)*.5f,Cm,Soffit((U0+U1)*.5f)-100));
            }
        }
        const float W0=C0+2,W1=C1-2;
        const float Us[]={-Tread*.5f,Len-Tread*.5f,Len+10};const float Zs[]={ZFoot,ZTop,ZTop};
        for(int32 I=0;I<2;++I)QuadToward(P.Ramp,Pt(Us[I],W0,Zs[I]),Pt(Us[I+1],W0,Zs[I+1]),Pt(Us[I+1],W1,Zs[I+1]),Pt(Us[I],W1,Zs[I]),Pt((Us[I]+Us[I+1])*.5f,Cm,Zs[I+1]+100));
        return N;
    }

    // ---------------------------------------------------------------- dressing
    // A fire (frame coordinates of its flame's base) and where its light hangs.
    struct FFire{FVector Flame,Light;float Width,Height,Intensity,Radius;};
    struct FJarSpot{FVector At;float Yaw;bool bUrn;int32 Cigarettes;};
    struct FIdol{FVector At;float Yaw;float Radius;};
    struct FSkull{FVector At;FRotator Rotation;};
    // An iron wall torch on the wall plane (Axis 0: the wall is at S=W; 1: at T=W),
    // at U along it, pointing Into (+1/-1 along the axis) with its cup's rim at Z.
    void Torch(FParts& P,TArray<FFire>& Fires,int32 Axis,float W,float Into,float U,float Z,float Intensity=4200,float Radius=650)
    {
        auto L=[&](float O,float A,float Zz){return Axis==0?FVector(W+Into*O,U+A,Zz):FVector(U+A,W+Into*O,Zz);};
        auto B=[&](FSurf& M,float O0,float O1,float A0,float A1,float Za,float Zb){const FVector X=L(O0,A0,Za),Y=L(O1,A1,Zb);Box(M,X.X,Y.X,X.Y,Y.Y,Za,Zb);};
        B(P.Iron,0,4,-6,6,Z-46,Z-14);         // wall plate
        B(P.Iron,0,22,-2.5f,2.5f,Z-26,Z-21); // arm
        Drum(P.Iron,L(24,0,Z-15),L(24,0,Z-1),7.5f,10);
        Drum(P.Iron,L(24,0,Z-24),L(24,0,Z-15),3.5f,8);
        Fires.Add({L(24,0,Z-2),L(36,0,Z+18),18,30,Intensity,Radius});
    }
    // A standing brazier: a stone stem and a wide bowl, its fire at Z+84.
    void Brazier(FParts& P,TArray<FFire>& Fires,float S,float T,float Z)
    {
        Drum(P.Stone,{S,T,Z},{S,T,Z+10},17,12);Drum(P.Stone,{S,T,Z+10},{S,T,Z+66},8,10);
        Drum(P.Stone,{S,T,Z+66},{S,T,Z+82},24,14);Drum(P.Dark,{S,T,Z+82},{S,T,Z+83},20,14);
        Fires.Add({{S,T,Z+80},{S,T,Z+130},30,42,5600,800});
    }
    // A glyph panel on a wall (Axis 0: the wall at S=W; 1: at T=W), standing out
    // Into the room: rows of carved cartouches, each with a number in bars and dots.
    void Glyphs(FParts& P,FRandomStream& R,int32 Axis,float W,float Into,float U0,float U1,float Z0,float Z1)
    {
        auto On=[&](FSurf& M,float A0,float A1,float Za,float Zb,float D0,float D1)
        {const float W0=W+Into*D0,W1=W+Into*D1;if(Axis==0)Box(M,W0,W1,A0,A1,Za,Zb);else Box(M,A0,A1,W0,W1,Za,Zb);};
        On(P.Stone,U0-6,U1+6,Z0-6,Z0-1,0,2.5f);On(P.Stone,U0-6,U1+6,Z1+1,Z1+6,0,2.5f);
        for(float Z=Z0+17;Z<Z1-12;Z+=36)for(float U=U0+15;U<U1-10;U+=31)
        {
            const float H=R.FRandRange(13,16),Wd=R.FRandRange(11,13.5f);const int32 Num=R.RandRange(1,19);
            On(P.Plaster,U-Wd,U+Wd,Z-H,Z+H,0,2.5f);
            float Zm=Z-H+4;
            for(int32 B=0;B<Num/5;++B){On(P.Dark,U-Wd*.75f,U+Wd*.75f,Zm,Zm+2.6f,2.5f,3.2f);Zm+=4.6f;}
            const int32 Dots=Num%5;
            for(int32 K=0;K<Dots;++K){const float Ud=U+(K-(Dots-1)*.5f)*5.2f;On(P.Dark,Ud-1.6f,Ud+1.6f,Zm+.4f,Zm+3.6f,2.5f,3.2f);}
        }
    }
    // A square pier from Z0 to Z1: a mossy base, a stone shaft, a plaster band, a spreading capital.
    void Pier(FParts& P,float S,float T,float Half,float Z0,float Z1)
    {
        Box(P.Stone,S-Half-14,S+Half+14,T-Half-14,T+Half+14,Z0,Z0+16);
        Box(P.Stone,S-Half-8,S+Half+8,T-Half-8,T+Half+8,Z0+16,Z0+40);
        Box(P.Stone,S-Half,S+Half,T-Half,T+Half,Z0+40,Z1-110);
        Box(P.Plaster,S-Half-6,S+Half+6,T-Half-6,T+Half+6,Z1-110,Z1-80);
        Box(P.Stone,S-Half-20,S+Half+20,T-Half-20,T+Half+20,Z1-80,Z1-40);
        Box(P.Stone,S-Half-34,S+Half+34,T-Half-34,T+Half+34,Z1-40,Z1);
    }
    // A carved serpent mouth on a wall (the dart launchers): a square relief, a stepped brow, a dark throat.
    void SerpentMouth(FParts& P,int32 Axis,float W,float Into,float U,float Z)
    {
        auto On=[&](FSurf& M,float A0,float A1,float Za,float Zb,float D0,float D1)
        {const float W0=W+Into*D0,W1=W+Into*D1;if(Axis==0)Box(M,W0,W1,U+A0,U+A1,Za,Zb);else Box(M,U+A0,U+A1,W0,W1,Za,Zb);};
        On(P.Stone,-46,46,Z-36,Z+52,0,6);                                           // the panel
        On(P.Stone,-50,50,Z+52,Z+60,0,9);On(P.Stone,-50,50,Z-44,Z-36,0,9);           // its bands
        On(P.Stone,-38,38,Z+20,Z+44,6,14);                                          // the snout over the mouth
        On(P.Stone,-42,42,Z+40,Z+50,6,18);                                          // the brow
        for(float A:{-26.f,26.f}){On(P.Stone,A-9,A+9,Z+26,Z+40,14,17);On(P.Dark,A-4,A+4,Z+30,Z+36,17,17.4f);}   // eyes
        On(P.Stone,-32,32,Z-30,Z-12,6,15);                                          // lower jaw
        On(P.Dark,-22,22,Z-12,Z+20,6,6.6f);                                          // the open throat
        for(float A:{-17.f,-6.f,6.f,17.f})On(P.Plaster,A-2.5f,A+2.5f,Z+10,Z+20,6,10);// upper teeth
        for(float A:{-12.f,12.f})On(P.Plaster,A-2.5f,A+2.5f,Z-12,Z-4,6,10);          // lower teeth
    }

    UProceduralMeshComponent* Emit(AActor* Owner,const TCHAR* Name,const TArray<TPair<FSurf*,const TCHAR*>>& Sections,bool bSolid,int32& Missing,int32& Triangles,bool bIndoors=true)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        int32 Section=0;
        for(const auto& Pair:Sections)
        {
            const FSurf& S=*Pair.Key;TArray<FProcMeshTangent> Tangents;
            for(const FVector& N:S.N){const FVector Axis=FMath::Abs(N.Z)>.95f?FVector::ForwardVector:FVector::UpVector;Tangents.Emplace(FVector::CrossProduct(Axis,N).GetSafeNormal(),false);}
            Mesh->CreateMeshSection(Section,S.V,S.T,S.N,S.UV,S.C,Tangents,bSolid);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,Pair.Value);Missing+=!Material;Mesh->SetMaterial(Section++,Material);Triangles+=S.T.Num()/3;
        }
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        if(bIndoors)Mesh->SetLightingChannels(false,true,false);
        Mesh->ComponentTags.Add(TEXT("ChultTempleInterior"));
        return Mesh;
    }
    // Dry dungeon stone (Tools/create_temple_interior_materials.py): the ruins' scans without
    // their low-ground moss; the ruins' own stone, mossy this far down, for damp accents.
    const TCHAR* StoneMat=TEXT("/Game/Art/ChultTemple/M_TempleDungeonStone.M_TempleDungeonStone");
    const TCHAR* MossMat=TEXT("/Game/Art/ChultRuins/M_RuinStone.M_RuinStone");
    const TCHAR* PlasterMat=TEXT("/Game/Art/Materials/M_WeatheredPlaster.M_WeatheredPlaster");
    const TCHAR* DarkMat=TEXT("/Game/Art/ChultTemple/M_TempleVoid.M_TempleVoid");   // unlit black
    const TCHAR* IronMat=TEXT("/Game/Art/Materials/M_RustIron.M_RustIron");
    const TCHAR* WoodMat=TEXT("/Game/Art/Materials/M_Wood.M_Wood");
    const TCHAR* AstralMat=TEXT("/Game/Art/Materials/M_AstralDepth.M_AstralDepth");
    const TCHAR* OilMat=TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist");   // the ruptures' veils (the sewer's)
    const TCHAR* IdolMesh=TEXT("/Game/Art/ChultTemple/SM_TempleSerpentIdol.SM_TempleSerpentIdol");
    const TCHAR* SkullMesh=TEXT("/Game/Art/ChultTemple/SM_TempleSkull.SM_TempleSkull");

    // Built once per world (the checks read these).
    struct FBuilt{int32 Steps=0,Fires=0,Jars=0,Urns=0,Idols=0,Grips=0,FirstGrip=-1,Slabs=0,Bands=0,Launchers=0,Axes=0,Jets=0,Saws=0;};
    FBuilt Built;
    #include "ChultTempleRunBuild.inl"
    #include "ChultTempleWindBuild.inl"
}

// ---------------------------------------------------------------- public helpers
FVector ChultTempleInteriorSpawn(ETempleInteriorSpawn Spawn)
{
    const FSpawnSpot P=SpawnSpot(Spawn);return WorldAt(P.S,P.T,P.Z+34.65f);
}
float ChultTempleInteriorSpawnYaw(ETempleInteriorSpawn Spawn){const FSpawnSpot P=SpawnSpot(Spawn);return YawOf(P.DS,P.DT);}
const TCHAR* ChultTempleInteriorSpawnName(ETempleInteriorSpawn Spawn){return SpawnSpot(Spawn).Name;}
bool ChultTempleInteriorRespawn(const FVector& DeathWorld,FVector& OutWorld)
{
    const FSpace* Sp=SpaceAt(WorldFrame(DeathWorld),60);
    if(!Sp)return false;
    OutWorld=ChultTempleInteriorSpawn(Sp->Spawn);return true;
}
bool IsChultTempleInteriorSpawn(const FVector& World,float& OutYaw)
{
    for(int32 I=0;I<TempleInteriorSpawnCount;++I)
        if(FVector::Dist(World,ChultTempleInteriorSpawn(ETempleInteriorSpawn(I)))<1){OutYaw=ChultTempleInteriorSpawnYaw(ETempleInteriorSpawn(I));return true;}
    return false;
}
bool InChultTempleInterior(const FVector& World,float Margin){return SpaceAt(WorldFrame(World),Margin)!=nullptr;}
bool ChultTempleInteriorSwallows(const FVector& FeetWorld)
{
    const FVector F=WorldFrame(FeetWorld);
    if(F.X>PitS0&&F.X<PitS1&&F.Y>PitT0&&F.Y<PitT1&&F.Z<WellRingZ-60&&F.Z>WellDeep-600)return true;
    if(F.X>DS0&&F.X<DS1&&F.Y>PitG0&&F.Y<PitG1&&F.Z<Floor-60&&F.Z>WellDeep-600)return true;
    // The trap run's spike pits and trenches (October 22): below 80 cm under the floor he is on the spikes.
    {
        using namespace TempleRun;
        auto In=[&](float S0,float S1,float T0,float T1,float Depth){return F.X>S0&&F.X<S1&&F.Y>T0&&F.Y<T1&&F.Z<UZ-80&&F.Z>UZ-Depth-300;};
        for(const FPit& Pt:Pits)if(In(BS0,BS1,Pt.T0,Pt.T1,PitDepth))return true;
        if(In(BS0-BreakDeep,BS0,BreakT0,BreakT1,PitDepth))return true;
        if(In(CausewayS0,CausewayS1,CT0,CausewayT0,TrenchDepth)||In(CausewayS0,CausewayS1,CausewayT1,CT1,TrenchDepth))return true;
        for(const FPit& Pt:GPits)if(In(FS0,FS1,Pt.T0,Pt.T1,PitDepth))return true;
        const float Mid=(FS0+FS1)*.5f;
        if(In(GAxeS0,Mid-80,GAxeT0+50,GAxeT1-50,TrenchDepth)||In(Mid+80,GAxeS1,GAxeT0+50,GAxeT1-50,TrenchDepth))return true;
    }
    // The Astral wind (October 22): its spike pits and leg 3's alcoves as the run's, its ruptures 40 cm down.
    {
        using namespace TempleWind;
        auto In=[&](float S0,float S1,float T0,float T1){return F.X>S0&&F.X<S1&&F.Y>FMath::Min(T0,T1)&&F.Y<FMath::Max(T0,T1)&&F.Z<UZ-80&&F.Z>UZ-PitDepth-300;};
        for(int32 L=0;L<4;++L)if(In(Legs[L].S0,Legs[L].S1,Pits[L].From,Pits[L].To))return true;
        if(In(Legs[2].S0-BreakDeep,Legs[2].S0,BreakMinusT0,BreakMinusT1)||In(Legs[2].S1,Legs[2].S1+BreakDeep,BreakPlusT0,BreakPlusT1))return true;
        if(F.Z<UZ-40&&F.Z>UZ-500&&F.X>TempleRun::FS0-100&&F.X<Bends[2].S1+100&&F.Y>Legs[3].T0-100&&F.Y<Bends[0].T1+100&&ChultTempleWindTearAt(FVector2D(F.X,F.Y)))return true;
    }
    return F.X>RiftS0&&F.X<RiftS1&&F.Y>PT0&&F.Y<PT1&&F.Z<Floor-60&&F.Z>WellDeep-600;
}
void ChultTempleInteriorCutsAt(float Z,TArray<FBox2D>& Out)
{
    for(const FSpace& Sp:Spaces())
    {
        if(!Sp.bCuts)continue;
        if(!Sp.bFlight){if(Z>=Sp.Z0-2&&Z<=Sp.Z1+2)Out.Add(FBox2D(FVector2D(Sp.S0,Sp.T0),FVector2D(Sp.S1,Sp.T1)));continue;}
        // A flight: where its floor (a step below the line) to its ceiling crosses Z.
        float Lo=1e9f,Hi=-1e9f;
        for(float S=Sp.S0;S<=Sp.S1+.01f;S+=2){const float L=FlightLine(Sp,S);if(Z>=L-Rise-26&&Z<=L+Sp.H+2){Lo=FMath::Min(Lo,S);Hi=FMath::Max(Hi,S);}}
        if(Hi>=Lo)Out.Add(FBox2D(FVector2D(FMath::Max(Sp.S0,Lo-2),Sp.T0),FVector2D(FMath::Min(Sp.S1,Hi+2),Sp.T1)));
    }
}
bool ChultTempleInteriorCutsGround(float S,float T){return S>WellS0-120&&S<WellS1+120&&T>WellT0-120&&T<WellT1+120;}

// ---------------------------------------------------------------- build
FVector ChultTempleInteriorFrameOf(const FVector& World){return WorldFrame(World);}
FVector ChultTempleInteriorWorldOf(const FVector& Frame){return WorldAt(Frame.X,Frame.Y,Frame.Z);}
float ChultTempleInteriorFloorZ(){return static_cast<float>(WorldAt(HallS0,AxisT,Floor).Z);}

void ChultTempleSkeletonPosts(TArray<FTempleSkeletonPost>& Out)
{
    Out.Reset();
    auto Post=[&](float S,float T,float DS,float DT,bool bDormant,const FBox2D& Leash,const TCHAR* Space)
    {
        FTempleSkeletonPost P;P.Feet=WorldAt(S,T,Floor);P.Yaw=YawOf(DS,DT);P.bDormant=bDormant;P.Leash=Leash;P.Space=Space;Out.Add(P);
    };
    // The spike corridor: one in each gap after the first band (the entry gap holds the respawn),
    // lying along a wall, the last on its feet by the door into the pier hall. Kept 30 cm off the
    // bands' edges and the walls (the leash holds the capsule's centre: its body is 34 cm round).
    const float In=64;
    for(int32 B=0;B+1<UE_ARRAY_COUNT(SpikeS);++B)
    {
        const float S0=SpikeS[B+1]+SpikeHalfDepth,S1=SpikeS[B]-SpikeHalfDepth;
        Post((S0+S1)*.5f+(B%2?60:-60),B%2?KT0+70:KT1-70,B%2?1:-1,0,true,FBox2D(FVector2D(S0+In,KT0+In),FVector2D(S1-In,KT1-In)),TEXT("Spikes"));
    }
    {
        const float S0=KS0,S1=SpikeS[UE_ARRAY_COUNT(SpikeS)-1]-SpikeHalfDepth;
        Post((S0+S1)*.5f+40,AxisT+90,1,0,false,FBox2D(FVector2D(S0+In,KT0+In),FVector2D(S1-In,KT1-In)),TEXT("Spikes"));
    }
    // The pier hall: seven before the rift (from 5 m of the respawn by its door), five beyond it.
    // Each side's leash stops 60 cm short of the rift's lip.
    const FBox2D Near(FVector2D(RiftS1+60,PT0+40),FVector2D(PS1-40,PT1-40)),Far(FVector2D(PS0+40,PT0+40),FVector2D(RiftS0-60,PT1-40));
    Post(68700,2250,-1,-1,true,Near,TEXT("Piers"));     // by the wall the rings hang along
    Post(68250,2350,-1,-.3f,false,Near,TEXT("Piers"));  // before the first ring, watching them
    Post(68250,1150,1,0,false,Near,TEXT("Piers"));      // on the axis, before the lip
    Post(68650,1100,0,1,true,Near,TEXT("Piers"));
    Post(68700,0,-1,1,true,Near,TEXT("Piers"));
    Post(68200,-120,1,.5f,false,Near,TEXT("Piers"));
    Post(69050,-150,0,1,true,Near,TEXT("Piers"));
    Post(66900,2250,1,-1,true,Far,TEXT("Piers"));       // where he steps down off the last ring
    Post(67000,1100,1,.3f,false,Far,TEXT("Piers"));
    Post(66550,1150,0,-1,true,Far,TEXT("Piers"));
    Post(66900,60,1,-.6f,false,Far,TEXT("Piers"));      // before the way on
    Post(67250,-120,-1,1,true,Far,TEXT("Piers"));
}

void ChultTempleSnakeObstacles(TArray<FBox2D>& Out)
{
    Out.Reset();
    for(const FVector2D& Pr:SnakePiers)Out.Add(FBox2D(Pr-FVector2D(SnakePierHalf+14,SnakePierHalf+14),Pr+FVector2D(SnakePierHalf+14,SnakePierHalf+14)));
    for(float Sg:{-1.f,1.f})Out.Add(FBox2D(FVector2D(66900+Sg*300-52,NT0),FVector2D(66900+Sg*300+52,NT0+142)));
    Out.Add(FBox2D(FVector2D(66900-24,NT0+106),FVector2D(66900+24,NT0+154)));   // the brazier
}

void ChultTempleSnakePosts(TArray<FTempleSnakePost>& Out)
{
    // Twenty, as in the 2D map: spread through the lanes, clear of the piers, the idols, the way in (its
    // first 4 m: the respawn) and the way on, at least 1.8 m apart. Seeded, so always the same.
    Out.Reset();
    TArray<FBox2D> Obstacles;ChultTempleSnakeObstacles(Obstacles);
    const FBox2D Leash(FVector2D(NS0+60,NT0+60),FVector2D(NS1-60,NT1-60));
    FRandomStream R(2026102103);
    TArray<FVector2D> Placed;
    for(int32 Try=0;Try<4000&&Placed.Num()<20;++Try)
    {
        const FVector2D C(R.FRandRange(NS0+110,NS1-110),R.FRandRange(NT0+130,NT1-400));
        bool Ok=FVector2D::Distance(C,FVector2D(NS1,ExitT))>260&&FVector2D::Distance(C,FVector2D((DS0+DS1)*.5f,NT1))>400;
        for(const FBox2D& B:Obstacles)Ok&=B.ComputeSquaredDistanceToPoint(C)>110*110;
        for(const FVector2D& O:Placed)Ok&=FVector2D::Distance(C,O)>180;
        if(!Ok)continue;
        Placed.Add(C);
        FTempleSnakePost P;P.Feet=WorldAt(C.X,C.Y,Floor);P.Yaw=R.FRandRange(0,360);P.bWander=Placed.Num()%3==0;P.Leash=Leash;Out.Add(P);
    }
}

void BuildChultTempleInterior(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();UWorld* World=Owner->GetWorld();
    FParts P;FRandomStream R(2026102001);TArray<FFire> Fires;TArray<FJarSpot> Jars;TArray<FIdol> Idols;TArray<FSkull> Skulls;Built=FBuilt();
    auto Jar=[&](float S,float T,float Z,bool Urn){Jars.Add({FVector(S,T,Z),float(R.FRandRange(0,360)),Urn,Urn?R.RandRange(2,4):R.RandRange(1,2)});};
    FLining Plain;FLining Hall_;Hall_.Dado=160;Hall_.Frieze=110;Hall_.Top=&P.Plaster;

    // ---- The cella: lined inside the sanctuary's masonry, its back wall opened.
    {
        FLining L;L.Dado=55;L.High=&P.Plaster;L.bFloor=true;
        const float In=1;   // a centimetre in from the masonry
        Line(P,CellaS0+In,CellaS1-In,-CellaHalf+In,CellaHalf-In,CellaZ+.6f,CellaTop-In,
            {{0,-120,120,CellaZ,CellaZ+260},{1,-BackDoorHalf,BackDoorHalf,CellaZ,CellaZ+BackDoorTop}},L);
        for(float Sg:{-1.f,1.f})Torch(P,Fires,0,CellaS1-In,-1,Sg*175,CellaZ+175,3600,560);
        Glyphs(P,R,0,CellaS1-In,-1,-95,95,CellaZ+BackDoorTop+8,CellaTop-12);
        Jar(CellaS1-40,-CellaHalf+38,CellaZ+.6f,false);Jar(CellaS1-40,CellaHalf-38,CellaZ+.6f,true);
    }
    // ---- The back door through the sanctuary's back wall, then the hidden stair.
    Line(P,CellaS1,FlightATop,-BackDoorHalf,BackDoorHalf,CellaZ,CellaZ+BackDoorTop,{{0,-BackDoorHalf,BackDoorHalf,CellaZ,CellaZ+BackDoorTop},{1,-BackDoorHalf,BackDoorHalf,CellaZ,CellaZ+BackDoorTop}},Plain);
    Built.Steps+=Flight(P,FlightAFoot,FlightATop,-BackDoorHalf,BackDoorHalf,L1Z,FlightAZ,FlightH,L1Z+L1H,CellaZ+BackDoorTop,R);
    Line(P,FlightAFoot,L1S1,L1T0,L1T1,L1Z,L1Z+L1H,{{0,-BackDoorHalf,BackDoorHalf,L1Z,L1Z+L1H},{0,FlightBT0,FlightBT1,L1Z,L1Z+L1H}},Plain);
    Torch(P,Fires,0,L1S1,-1,120,L1Z+170,3800,600);
    Built.Steps+=Flight(P,FlightBFoot,FlightBTop,FlightBT0,FlightBT1,L2Z,L1Z,FlightH,L2Z+L2H,L1Z+L1H,R);
    // Landing 2 opens straight onto the well's gallery.
    Line(P,L2S0,FlightBFoot,FlightBT0,WellT0,L2Z,L2Z+L2H,{{1,FlightBT0,FlightBT1,L2Z,L2Z+L2H},{3,L2S0,FlightBFoot,L2Z,L2Z+L2H}},Plain);
    Torch(P,Fires,1,FlightBT0,1,L2S0+70,L2Z+170,3600,560);

    // ---- The well.
    {
        FLining L;L.bFloor=false;L.bCeiling=false;L.Dado=0;
        Line(P,WellS0,WellS1,WellT0,WellT1,WellBottom,WellTop-4*40,{{2,L2S0,FlightBFoot,L2Z,L2Z+L2H},{0,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH}},L);
        // A square corbel vault closing the top.
        for(int32 K=1;K<=4;++K)
        {
            const float I0=(K-1)*40,I1=K*40,Za=WellTop-(5-K)*40,Zb=Za+40;
            auto Ring_=[&](float I,float Z,FSurf& M,bool Under)
            {
                const FVector Mid((WellS0+WellS1)*.5f,(WellT0+WellT1)*.5f,Z+(Under?-200:0));
                const float S0=WellS0+I,S1=WellS1-I,T0=WellT0+I,T1=WellT1-I;
                if(Under)
                {
                    // The course's underside: a frame between I0 and I1.
                    const float A0=WellS0+I0,A1=WellS1-I0,B0=WellT0+I0,B1=WellT1-I0;
                    QuadToward(M,{A0,B0,Z},{A1,B0,Z},{A1,T0,Z},{A0,T0,Z},Mid);QuadToward(M,{A0,T1,Z},{A1,T1,Z},{A1,B1,Z},{A0,B1,Z},Mid);
                    QuadToward(M,{A0,T0,Z},{S0,T0,Z},{S0,T1,Z},{A0,T1,Z},Mid);QuadToward(M,{S1,T0,Z},{A1,T0,Z},{A1,T1,Z},{S1,T1,Z},Mid);
                    return;
                }
                const FVector C((WellS0+WellS1)*.5f,(WellT0+WellT1)*.5f,Z+20);
                QuadToward(M,{S0,T0,Z},{S1,T0,Z},{S1,T0,Z+40},{S0,T0,Z+40},C);QuadToward(M,{S0,T1,Z},{S1,T1,Z},{S1,T1,Z+40},{S0,T1,Z+40},C);
                QuadToward(M,{S0,T0,Z},{S0,T1,Z},{S0,T1,Z+40},{S0,T0,Z+40},C);QuadToward(M,{S1,T0,Z},{S1,T1,Z},{S1,T1,Z+40},{S1,T0,Z+40},C);
            };
            Ring_(I1,Za,P.Stone,true);Ring_(I1,Za,P.Stone,false);(void)I0;(void)Zb;
        }
        QuadToward(P.Stone,{WellS0+160,WellT0+160,WellTop},{WellS1-160,WellT0+160,WellTop},{WellS1-160,WellT1-160,WellTop},{WellS0+160,WellT1-160,WellTop},{(WellS0+WellS1)*.5f,(WellT0+WellT1)*.5f,WellTop-200});
        // The gallery along the -T wall, then five runs down round the walls with landings at the corners.
        Box(P.Stone,WellS0,WellS1,WellT0,WellT0+WellStair,L2Z-50,L2Z);
        const float SA=WellS1-WellStair,TA=WellT1-WellStair;
        Built.Steps+=Run(P,1,PitT1,PitT0,SA,WellS1,SA,140,L2Z,-1e9f,R);                    // R1 along the +S wall, +T
        Box(P.Stone,SA,WellS1,TA,WellT1,90,140);
        Built.Steps+=Run(P,0,PitS0,PitS1,TA,WellT1,TA,-260,140,-1e9f,R);                  // R2 along the +T wall, -S
        Box(P.Stone,WellS0,PitS0,TA,WellT1,-310,-260);
        Built.Steps+=Run(P,1,PitT0,PitT1,WellS0,PitS0,PitS0,-660,-260,-1e9f,R);            // R3 along the -S wall, -T
        Box(P.Stone,WellS0,PitS0,WellT0,PitT0,-710,-660);
        Built.Steps+=Run(P,0,PitS1,PitS0,WellT0,PitT0,PitT0,-1060,-660,-1e9f,R);           // R4 along the -T wall, +S
        Box(P.Stone,SA,WellS1,WellT0,PitT0,-1110,-1060);
        Built.Steps+=Run(P,1,PitT1,PitT0,SA,WellS1,SA,WellRingZ,-1060,WellRingZ,R);        // R5 along the +S wall, +T
        // The ring floor round the pit, and the pit's shaft down to the Astral.
        Box(P.Stone,WellS0,WellS1,WellT0,PitT0,WellBottom,WellRingZ);Box(P.Stone,WellS0,WellS1,PitT1,WellT1,WellBottom,WellRingZ);
        Box(P.Stone,WellS0,PitS0,PitT0,PitT1,WellBottom,WellRingZ);Box(P.Stone,PitS1,WellS1,PitT0,PitT1,WellBottom,WellRingZ);
        FLining Shaft;Shaft.bFloor=false;Shaft.bCeiling=false;Shaft.Dado=0;
        Line(P,PitS0,PitS1,PitT0,PitT1,WellDeep,WellBottom,{},Shaft);
        QuadToward(P.Astral,{PitS0,PitT0,WellDeep},{PitS1,PitT0,WellDeep},{PitS1,PitT1,WellDeep},{PitS0,PitT1,WellDeep},{(PitS0+PitS1)*.5f,(PitT0+PitT1)*.5f,WellDeep+100});
        // Torches: over the gallery, above each corner and halfway down each run, round the ring.
        Torch(P,Fires,1,WellT0,1,WellS0+330,L2Z+175);Torch(P,Fires,1,WellT0,1,WellS1-330,L2Z+175);
        Torch(P,Fires,0,WellS1,-1,(PitT0+PitT1)*.5f,(L2Z+140)*.5f+175);
        Torch(P,Fires,1,WellT1,-1,WellS1-100,140+175);
        Torch(P,Fires,1,WellT1,-1,(PitS0+PitS1)*.5f,(140-260)*.5f+175);
        Torch(P,Fires,0,WellS0,1,WellT1-100,-260+175);
        Torch(P,Fires,0,WellS0,1,(PitT0+PitT1)*.5f+200,(-260-660)*.5f+175);
        Torch(P,Fires,1,WellT0,1,WellS0+100,-660+175);
        Torch(P,Fires,1,WellT0,1,(PitS0+PitS1)*.5f,(-660-1060)*.5f+175);
        Torch(P,Fires,0,WellS1,-1,WellT0+100,-1060+175);
        Torch(P,Fires,0,WellS1,-1,(PitT0+PitT1)*.5f,(-1060+WellRingZ)*.5f+175);
        Torch(P,Fires,0,WellS0,1,AxisT-DoorHalf-90,WellRingZ+175);Torch(P,Fires,0,WellS0,1,AxisT+DoorHalf+90,WellRingZ+175);
        Torch(P,Fires,1,WellT1,-1,(WellS0+WellS1)*.5f,WellRingZ+175);
        Jar(WellS0+40,WellT0+40,WellRingZ,false);Jar(WellS0+40,WellT1-40,WellRingZ,true);Jar(WellS0+90,WellT1-45,WellRingZ,false);
    }

    // ---- The Great Hall (2D map 1): two rows of six piers, a processional way, idols, stelae.
    {
        Line(P,WellS0-60,WellS0,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH,{{0,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH},{1,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH}},Plain);
        Line(P,HallS0,HallS1,HallT0,HallT1,Floor,HallTop,{{1,AxisT-DoorHalf,AxisT+DoorHalf,Floor,Floor+DoorH},{0,KT0,KT1,Floor,Floor+PassH}},Hall_);
        const float Rows[]={AxisT-500,AxisT+500};
        for(int32 I=0;I<6;++I)for(float T:Rows)
        {
            const float S=HallS1-600-I*600;
            Pier(P,S,T,80,Floor,HallTop);
            // A torch on each pier's aisle face.
            Torch(P,Fires,1,T<AxisT?T+80:T-80,T<AxisT?1:-1,S,Floor+210);
        }
        // Beams over the rows and across at each pier.
        for(float T:Rows)Box(P.Stone,HallS0,HallS1,T-70,T+70,HallTop-55,HallTop);
        for(int32 I=0;I<6;++I){const float S=HallS1-600-I*600;Box(P.Stone,S-60,S+60,HallT0,HallT1,HallTop-45,HallTop);}
        // The processional way: a pale inlay between mossy kerbs.
        QuadToward(P.StepMoss,{HallS0+20,AxisT-200,Floor+.6f},{HallS1-20,AxisT-200,Floor+.6f},{HallS1-20,AxisT-170,Floor+.6f},{HallS0+20,AxisT-170,Floor+.6f},{HallS0,AxisT-185,Floor+100});
        QuadToward(P.StepMoss,{HallS0+20,AxisT+170,Floor+.6f},{HallS1-20,AxisT+170,Floor+.6f},{HallS1-20,AxisT+200,Floor+.6f},{HallS0+20,AxisT+200,Floor+.6f},{HallS0,AxisT+185,Floor+100});
        for(float S=HallS0+40;S<HallS1-60;S+=120)QuadToward(P.Steps,{S,AxisT-160,Floor+.4f},{S+110,AxisT-160,Floor+.4f},{S+110,AxisT+160,Floor+.4f},{S,AxisT+160,Floor+.4f},{S,AxisT,Floor+100});
        // Wall torches between the piers, glyph panels and tall stelae against the side walls.
        for(int32 I=0;I<5;++I)
        {
            const float S=HallS1-900-I*600;
            Torch(P,Fires,1,HallT0,1,S,Floor+230);Torch(P,Fires,1,HallT1,-1,S,Floor+230);
            Glyphs(P,R,1,HallT0,1,S-230,S-90,Floor+330,Floor+560);Glyphs(P,R,1,HallT1,-1,S+90,S+230,Floor+330,Floor+560);
        }
        for(int32 I=0;I<2;++I)for(float Sg:{-1.f,1.f})
        {
            const float S=I?74660:75860,T=Sg<0?HallT0+45:HallT1-45;
            Box(P.Stone,S-70,S+70,T-28,T+28,Floor,Floor+330);Box(P.Moss,S-76,S+76,T-32,T+32,Floor+330,Floor+350);
            Glyphs(P,R,1,T-Sg*28,-Sg,S-58,S+58,Floor+40,Floor+300);
        }
        // Serpent idols on plinths facing the way, braziers at both ends.
        for(float S:{76460.f,74060.f})for(float Sg:{-1.f,1.f})
        {
            const float T=AxisT+Sg*330;
            Box(P.Stone,S-45,S+45,T-45,T+45,Floor,Floor+85);Box(P.Stone,S-52,S+52,T-52,T+52,Floor+85,Floor+93);
            Idols.Add({FVector(S,T,Floor+93),YawOf(0,-Sg),20});
        }
        for(float S:{HallS1-320,HallS0+320})for(float Sg:{-1.f,1.f})Brazier(P,Fires,S,AxisT+Sg*260,Floor);
        // The end walls: torches either side of each door, a frieze over the far one.
        for(float Sg:{-1.f,1.f}){Torch(P,Fires,0,HallS1,-1,AxisT+Sg*230,Floor+220);Torch(P,Fires,0,HallS0,1,AxisT+Sg*320,Floor+220);}
        Glyphs(P,R,0,HallS0,1,KT0-60,KT1+60,Floor+PassH+40,Floor+PassH+230);
        // A fallen column drum or two, and pots along the walls.
        Drum(P.Stone,{75300,HallT0+150,Floor+30},{75380,HallT0+230,Floor+30},30,12);
        Drum(P.Stone,{75150,HallT0+120,Floor},{75150,HallT0+120,Floor+56},30,12);
        for(int32 I=0;I<7;++I)
        {
            const float S=HallS1-450-I*560;
            Jar(S+R.FRandRange(-60,60),HallT0+40,Floor,I%3==1);Jar(S+R.FRandRange(-60,60),HallT1-40,Floor,I%3==2);
        }
    }

    // ---- The spike corridor (2D map 2): five spike bands across a corbel-vaulted corridor.
    TArray<TPair<FSurf,FVector>> BandMeshes;   // spike geometry relative to each band's floor centre
    {
        Line(P,KS1,HallS0,KT0,KT1,Floor,Floor+PassH,{{0,KT0,KT1,Floor,Floor+PassH},{1,KT0,KT1,Floor,Floor+PassH}},Plain);
        FLining L;L.Dado=0;L.bCeiling=true;
        Line(P,KS0,KS1,KT0,KT1,Floor,KTop,{{1,KT0,KT1,Floor,Floor+PassH},{0,KT0,KT1,Floor,Floor+PassH}},L);
        Corbel(P,0,KS0,KS1,KT0,KT1,KTop,4,25,30);
        for(float S=KS1-300;S>KS0+100;S-=420)
        {
            const bool Left=FMath::RoundToInt((KS1-S)/420)%2==0;
            Torch(P,Fires,1,Left?KT0:KT1,Left?1:-1,S,Floor+200,3800,620);
        }
        for(int32 B=0;B<UE_ARRAY_COUNT(SpikeS);++B)
        {
            const float S0=SpikeS[B]-SpikeHalfDepth,S1=SpikeS[B]+SpikeHalfDepth;
            // The grate: an iron plate with a dark hole at every spike.
            Box(P.Iron,S0,S1,KT0,KT1,Floor,Floor+.8f);
            FSurf Spk;const FVector Centre=At(SpikeS[B],AxisT,Floor);
            for(float S=S0+13;S<S1;S+=26.6f)for(float T=KT0+12.5f;T<KT1;T+=25)
            {
                Box(P.Dark,S-5.5f,S+5.5f,T-5.5f,T+5.5f,Floor+.8f,Floor+1.1f);
                // A four-sided spike, tip SpikeTall above the floor at rest (the traps lower it).
                const FVector Tip=At(S,T,Floor+AChultTempleTraps::SpikeTall)-Centre;
                const FVector C[4]={At(S-5,T-5,Floor-2)-Centre,At(S+5,T-5,Floor-2)-Centre,At(S+5,T+5,Floor-2)-Centre,At(S-5,T+5,Floor-2)-Centre};
                for(int32 K=0;K<4;++K){const FVector A=C[K],Bv=C[(K+1)%4];const FVector Mid=(A+Bv)*.5f;
                    if(FVector::DotProduct(FVector::CrossProduct(Bv-A,Tip-A),Mid-(C[0]+C[2])*.5f)>=0)Spk.Quad(A,Bv,Tip,Tip);else Spk.Quad(Bv,A,Tip,Tip);}
            }
            BandMeshes.Add({Spk,G+Centre});
        }
        // Skulls just short of three bands, from their own stream (the rest keeps its draws).
        FRandomStream Bones(2026102002);
        for(int32 B:{0,2,4})
        {
            const float Sg=B==2?-1.f:1.f;
            Skulls.Add({FVector(SpikeS[B]+Sg*(SpikeHalfDepth+Bones.FRandRange(12,30)),Bones.FRandRange(KT0+40,KT1-40),Floor+6),FRotator(Bones.FRandRange(-15,25),Bones.FRandRange(0,360),Bones.FRandRange(-80,80))});
        }
    }

    // ---- The pier hall (2D map 3): eight piers and an Astral rift crossed by slabs or rings.
    TArray<FSurf> SlabMeshes;
    {
        Line(P,PS1,KS0,KT0,KT1,Floor,Floor+PassH,{{0,KT0,KT1,Floor,Floor+PassH},{1,KT0,KT1,Floor,Floor+PassH}},Plain);
        FLining L=Hall_;L.FloorHoles={FBox2D(FVector2D(RiftS0,PT0),FVector2D(RiftS1,PT1))};
        Line(P,PS0,PS1,PT0,PT1,Floor,PTop,{{1,KT0,KT1,Floor,Floor+PassH},{2,DS0,DS1,Floor,Floor+PassH}},L);
        // The rift: its walls down to the Astral, which glows up out of it.
        FLining Rift;Rift.bFloor=false;Rift.bCeiling=false;Rift.Dado=0;
        Line(P,RiftS0,RiftS1,PT0,PT1,WellDeep,Floor,{},Rift);
        QuadToward(P.Astral,{RiftS0,PT0,WellDeep},{RiftS1,PT0,WellDeep},{RiftS1,PT1,WellDeep},{RiftS0,PT1,WellDeep},{(RiftS0+RiftS1)*.5f,AxisT,WellDeep+100});
        // Broken edges: a few chipped blocks along both lips.
        for(int32 I=0;I<14;++I)
        {
            const float T=R.FRandRange(PT0+60,PT1-60);if(FMath::Abs(T-RingT)<120)continue;
            const bool Near=I%2==0;const float S=Near?RiftS1+R.FRandRange(32,60):RiftS0-R.FRandRange(32,60);   // clear of the lip: nothing to stand on over the rift
            TurnedBox(R.FRand()<.4f?P.Moss:P.Stone,FVector(S,T,Floor+R.FRandRange(6,12)),FVector(R.FRandRange(14,26),R.FRandRange(18,40),R.FRandRange(8,14)),FRotator(R.FRandRange(-8,8),R.FRandRange(0,360),R.FRandRange(-6,6)));
        }
        // The piers, clear of the rift and the ways across.
        for(float S:{68950.f,68450.f,67150.f,66700.f})for(float T:{500.f,1700.f})
        {
            Pier(P,S,T,75,Floor,PTop);
            Torch(P,Fires,0,S+(S>RiftS1?-75:75),S>RiftS1?-1:1,T,Floor+210,3800,620);
        }
        for(float T:{500.f,1700.f})Box(P.Stone,PS0,PS1,T-65,T+65,PTop-50,PTop);
        // Wall torches.
        for(float S:{69050.f,68350.f,67250.f,66650.f}){Torch(P,Fires,1,PT1,-1,S,Floor+230);if(S>RiftS1||S<DS0-200||S>DS1+200)Torch(P,Fires,1,PT0,1,S,Floor+230);}
        Torch(P,Fires,0,PS0,1,AxisT,Floor+230);Torch(P,Fires,0,PS1,-1,AxisT+420,Floor+220);Torch(P,Fires,0,PS1,-1,AxisT-420,Floor+220);
        Glyphs(P,R,0,PS0,1,AxisT-260,AxisT+260,Floor+320,Floor+600);
        // Iron rings hung on chains from the vault over the rift, by the +T wall (brachiation).
        for(float S:RingS)
        {
            const FVector Grip(S,RingT,RingZ);
            // A ring 9 cm in radius across the line of swing, its bottom bar the grip.
            TArray<FVector> Loop;for(int32 K=0;K<=12;++K){const float A=K*2*PI/12;Loop.Add(FVector(S,RingT+FMath::Sin(A)*9,RingZ+9-FMath::Cos(A)*9));}
            PathTube(P.Iron,Loop,1.6f,1.6f,6);
            // The chain: links as short alternating bars up to a ceiling plate.
            for(float Z=RingZ+18;Z<PTop-10;Z+=11){const bool Turn=FMath::RoundToInt((Z-RingZ)/11)%2==0;Box(P.Iron,S-(Turn?1:3),S+(Turn?1:3),RingT-(Turn?3:1),RingT+(Turn?3:1),Z,Z+9);}
            Box(P.Iron,S-14,S+14,RingT-14,RingT+14,PTop-6,PTop);
            FChuckSwingGrip Gr;Gr.Grip=G+At(Grip.X,Grip.Y,Grip.Z);Gr.Along=-Along;
            if(Built.FirstGrip<0)Built.FirstGrip=GetChuckSwingGrips().Num();
            AddChuckSwingGrip(Gr);++Built.Grips;
        }
        // Rift light (purple), pots in the far corners.
        Jar(PS1-40,PT1-40,Floor,true);Jar(PS1-40,PT0+40,Floor,false);Jar(PS0+45,PT1-45,Floor,false);Jar(PS0+45,PT1-95,Floor,true);Jar(PS0+40,PT0+260,Floor,false);
    }

    // ---- The dart gallery (2D map 4): eight serpent mouths, turning west; the choked way on.
    {
        Line(P,DS0,DS1,DT1,PT0,Floor,Floor+PassH,{{2,DS0,DS1,Floor,Floor+PassH},{3,DS0,DS1,Floor,Floor+PassH}},Plain);
        FLining L;L.Dado=0;L.FloorHoles={FBox2D(FVector2D(DS0,PitG0),FVector2D(DS1,PitG1))};
        Line(P,DS0,DS1,DT0,DT1,Floor,DTop,{{3,DS0,DS1,Floor,Floor+PassH},{2,DS0,DS1,Floor,DTop}},L);   // its far end open onto the snake chamber
        // The slab pit: its walls down to the Astral, and three cracked slabs across it.
        FLining PitL;PitL.bFloor=false;PitL.bCeiling=false;PitL.Dado=0;
        Line(P,DS0,DS1,PitG0,PitG1,WellDeep,Floor,{},PitL);
        QuadToward(P.Astral,{DS0,PitG0,WellDeep},{DS1,PitG0,WellDeep},{DS1,PitG1,WellDeep},{DS0,PitG1,WellDeep},{(DS0+DS1)*.5f,(PitG0+PitG1)*.5f,WellDeep+100});
        for(int32 K=0;K<3;++K)
        {
            const float T1=PitG1-K*SlabW-1,T0=T1-SlabW+2,S0=DS0+1,S1=DS1-1;
            FSurf Slab;const FVector Home=At((S0+S1)*.5f,(T0+T1)*.5f,Floor);
            auto B=[&](FSurf& M,float a0,float a1,float b0,float b1,float z0,float z1){const int32 Before=M.V.Num();Box(M,a0,a1,b0,b1,z0,z1);for(int32 I=Before;I<M.V.Num();++I)M.V[I]-=Home;};
            B(Slab,S0,S1,T0,T1,Floor-34,Floor);
            SlabMeshes.Add(Slab);
            // Cracks drawn in black across its top (part of the slab, so they fall with it):
            // wandering lines in from either end.
            FSurf Cracks;
            for(int32 C=0;C<3;++C)
            {
                FVector2D A(C%2?S0+2:S1-2,FMath::Lerp(T0,T1,R.FRandRange(.15f,.85f)));
                const float Dir=C%2?1.f:-1.f;
                for(int32 J=0;J<5;++J)
                {
                    const FVector2D Bp(FMath::Clamp(A.X+Dir*R.FRandRange(25,50),S0+2,S1-2),FMath::Clamp(A.Y+R.FRandRange(-14,14),T0+3,T1-3));
                    if(FVector2D::Distance(A,Bp)<2)break;
                    const FVector2D D=(Bp-A).GetSafeNormal(),N(-D.Y*.9f,D.X*.9f);
                    const FVector Q[4]={At(A.X-N.X,A.Y-N.Y,Floor+.3f),At(Bp.X-N.X,Bp.Y-N.Y,Floor+.3f),At(Bp.X+N.X,Bp.Y+N.Y,Floor+.3f),At(A.X+N.X,A.Y+N.Y,Floor+.3f)};
                    const FVector Up=At(A.X,A.Y,Floor+100);
                    if(FVector::DotProduct(FVector::CrossProduct(Q[1]-Q[0],Q[2]-Q[0]),Up-Q[0])>=0)Cracks.Quad(Q[0]-Home,Q[1]-Home,Q[2]-Home,Q[3]-Home);
                    else Cracks.Quad(Q[3]-Home,Q[2]-Home,Q[1]-Home,Q[0]-Home);
                    A=Bp;
                }
            }
            SlabMeshes.Add(Cracks);
        }
        Corbel(P,1,DS0,DS1,DT0,DT1,DTop,4,22,30);
        for(int32 K=0;K<Launchers;++K)
        {
            const bool PlusWall=K%2==0;
            SerpentMouth(P,0,PlusWall?DS1:DS0,PlusWall?-1:1,LauncherT(K),Floor+36);
        }
        for(float T=DT1-300;T>ChokeT+100;T-=600)
        {
            const bool Plus=FMath::RoundToInt((DT1-300-T)/600)%2==0;
            Torch(P,Fires,0,Plus?DS1:DS0,Plus?-1:1,T-280,Floor+215,3800,620);
        }
        // The arch (October 21: the collapse behind it cleared, the way on to the snake chamber).
        for(float Sg:{-1.f,1.f})Box(P.Stone,Sg<0?DS0:DS1-40,Sg<0?DS0+40:DS1,ChokeT-30,ChokeT+30,Floor,Floor+PassH);
        Box(P.Stone,DS0,DS1,ChokeT-30,ChokeT+30,Floor+PassH,DTop);
        Box(P.Plaster,DS0+40,DS1-40,ChokeT+24,ChokeT+30,Floor+PassH-20,Floor+PassH);
        Jar(DS0+40,DT1-60,Floor,false);
    }

    // ---- The snake chamber (2D map 5, October 21): broad, torch-lit, its lanes looping round five piers;
    //      two serpent idols and a brazier under a glyph panel at the far end; the way on choked.
    {
        Line(P,NS0,NS1,NT0,NT1,Floor,NTop,{{3,DS0,DS1,Floor,DTop},{1,ExitT-ExitHalf,ExitT+ExitHalf,Floor,Floor+PassH}},Hall_);
        // The way on: an arch in the +S wall. Since October 22 the rubble heaped behind it is
        // climbed, to a chimney up to the trap run (TempleRunBuild::Collapse).
        for(float Sg:{-1.f,1.f})Box(P.Stone,NS1-30,NS1+30,Sg<0?ExitT-ExitHalf:ExitT+ExitHalf-36,Sg<0?ExitT-ExitHalf+36:ExitT+ExitHalf,Floor,Floor+PassH);
        Box(P.Plaster,NS1-36,NS1-30,ExitT-ExitHalf+36,ExitT+ExitHalf-36,Floor+PassH-20,Floor+PassH);
        // Beams across the vault.
        for(float T:{-6150.f,-7150.f,-8000.f})Box(P.Stone,NS0,NS1,T-60,T+60,NTop-50,NTop);
        // The piers, a torch on each toward the chamber's middle.
        for(const FVector2D& Pr:SnakePiers)
        {
            Pier(P,Pr.X,Pr.Y,SnakePierHalf,Floor,NTop);
            const bool Inward=Pr.X<66900;
            if(FMath::Abs(Pr.X-66900)>100)Torch(P,Fires,0,Pr.X+(Inward?SnakePierHalf:-SnakePierHalf),Inward?1:-1,Pr.Y,Floor+210,3800,620);
            else for(float Sg:{-1.f,1.f})Torch(P,Fires,1,Pr.Y+Sg*SnakePierHalf,Sg,Pr.X,Floor+210,3800,620);
        }
        // Serpent idols on plinths at the far wall, facing the way in; a brazier between them, a glyph panel over.
        for(float Sg:{-1.f,1.f})
        {
            const float S=66900+Sg*300,T=NT0+90;
            Box(P.Stone,S-45,S+45,T-45,T+45,Floor,Floor+85);Box(P.Stone,S-52,S+52,T-52,T+52,Floor+85,Floor+93);
            Idols.Add({FVector(S,T,Floor+93),YawOf(0,1),20});
        }
        Brazier(P,Fires,66900,NT0+130,Floor);
        Glyphs(P,R,1,NT0,1,66500,67300,Floor+330,Floor+620);
        // Wall torches: both long walls, the far wall's corners, either side of the way in and the way on.
        for(float T=-5900;T>NT0+150;T-=550)
        {
            Torch(P,Fires,0,NS0,1,T,Floor+230);
            if(FMath::Abs(T-ExitT)>220)Torch(P,Fires,0,NS1,-1,T,Floor+230);
        }
        for(float S:{66150.f,67650.f})Torch(P,Fires,1,NT0,1,S,Floor+230);
        for(float S:{DS0-260,DS1+260})Torch(P,Fires,1,NT1,-1,S,Floor+230);
        for(float Sg:{-1.f,1.f})Torch(P,Fires,0,NS1,-1,ExitT+Sg*200,Floor+230);
        // Fallen column drums, pots along the walls, old skulls.
        Drum(P.Stone,{66080,-6900,Floor+30},{66140,-7010,Floor+30},30,12);
        Drum(P.Stone,{67720,-6650,Floor},{67720,-6650,Floor+56},30,12);
        Jar(NS0+45,NT1-80,Floor,false);Jar(NS0+45,NT0+60,Floor,true);Jar(NS1-45,NT0+60,Floor,false);Jar(NS1-45,-6250,Floor,true);
        Jar(NS0+45,-7300,Floor,false);Jar(NS1-45,NT1-90,Floor,true);Jar(66550,NT0+45,Floor,false);
        FRandomStream Bones(2026102102);
        for(int32 I=0;I<5;++I)Skulls.Add({FVector(Bones.FRandRange(NS0+80,NS1-80),Bones.FRandRange(NT0+300,NT1-400),Floor+6),FRotator(Bones.FRandRange(-15,25),Bones.FRandRange(0,360),Bones.FRandRange(-80,80))});
    }

    // ---- The trap run (October 22; ChultTempleRunBuild.inl).
    FRunMovers Movers;
    TempleRunBuild::BuildRun(P,Fires,Jars,Skulls,Idols,Movers);
    // ---- The Astral wind and the sanctum (October 22; ChultTempleWindBuild.inl).
    TArray<FGlow> Glows;
    Built.Steps+=TempleWindBuild::BuildWind(P,Fires,Jars,Skulls,Idols,Glows);

    // ---- Emit.
    int32 Missing=0,Triangles=0;
    Emit(Owner,TEXT("ChultTempleInteriorMasonry"),{{&P.Stone,StoneMat},{&P.Moss,MossMat},{&P.Plaster,PlasterMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleInteriorSteps"),{{&P.Steps,StoneMat},{&P.StepMoss,StoneMat}},false,Missing,Triangles);
    auto* Ramp=Emit(Owner,TEXT("ChultTempleInteriorRamp"),{{&P.Ramp,StoneMat}},true,Missing,Triangles);Ramp->SetHiddenInGame(true);Ramp->SetCastShadow(false);
    Emit(Owner,TEXT("ChultTempleInteriorDark"),{{&P.Dark,DarkMat}},false,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleInteriorIron"),{{&P.Iron,IronMat}},false,Missing,Triangles);
    auto* Astral=Emit(Owner,TEXT("ChultTempleInteriorAstral"),{{&P.Astral,AstralMat}},false,Missing,Triangles,false);Astral->SetCastShadow(false);
    auto* Veils=Emit(Owner,TEXT("ChultTempleInteriorVeils"),{{&P.Veil,OilMat}},false,Missing,Triangles);Veils->SetCastShadow(false);
    auto* Block=Emit(Owner,TEXT("ChultTempleInteriorBlock"),{{&P.Block,StoneMat}},true,Missing,Triangles);Block->SetHiddenInGame(true);Block->SetCastShadow(false);
    // The traps' moving parts.
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Traps=World->SpawnActor<AChultTempleTraps>(G,FRotator::ZeroRotator,Params);
    if(Traps)
    {
        auto* Iron=LoadObject<UMaterialInterface>(nullptr,IronMat);auto* Wood=LoadObject<UMaterialInterface>(nullptr,WoodMat);auto* Stone=LoadObject<UMaterialInterface>(nullptr,StoneMat);
        auto* DarkM=LoadObject<UMaterialInterface>(nullptr,DarkMat);
        Missing+=!Iron+!Wood+!Stone+!DarkM;
        auto MakeMesh=[&](const FString& Name,const FSurf& S,UMaterialInterface* M,const FVector& Location,bool bSolid,const FSurf* Second=nullptr,UMaterialInterface* M2=nullptr)
        {
            auto* Mesh=NewObject<UProceduralMeshComponent>(Traps,*Name);Mesh->SetupAttachment(Traps->GetRootComponent());Mesh->bUseComplexAsSimpleCollision=true;
            Mesh->RegisterComponent();Mesh->SetWorldLocation(Location);
            Mesh->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,TArray<FProcMeshTangent>(),bSolid);Mesh->SetMaterial(0,M);Triangles+=S.T.Num()/3;
            if(Second){Mesh->CreateMeshSection(1,Second->V,Second->T,Second->N,Second->UV,Second->C,TArray<FProcMeshTangent>(),false);Mesh->SetMaterial(1,M2);Triangles+=Second->T.Num()/3;}
            Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            Mesh->SetLightingChannels(false,true,false);Mesh->ComponentTags.Add(TEXT("ChultTempleInterior"));
            return Mesh;
        };
        for(int32 B=0;B<BandMeshes.Num();++B)
        {
            auto* Mesh=MakeMesh(FString::Printf(TEXT("ChultTempleSpikes%d"),B),BandMeshes[B].Key,Iron,BandMeshes[B].Value,false);
            Mesh->SetCastShadow(false);
            Traps->AddSpikeBand(Mesh,BandMeshes[B].Value,-Along,SpikeHalfDepth,(KT1-KT0)*.5f,SpikePhase[B]);++Built.Bands;
        }
        for(int32 K=0;K<3;++K)
        {
            const FVector Home=G+At((DS0+DS1)*.5f,PitG1-K*SlabW-SlabW*.5f,Floor);
            auto* Mesh=MakeMesh(FString::Printf(TEXT("ChultTempleSlab%d"),K),SlabMeshes[K*2],Stone,Home,true,&SlabMeshes[K*2+1],DarkM);
            Traps->AddSlab(Mesh);++Built.Slabs;
        }
        Traps->RiftCentre=G+At((DS0+DS1)*.5f,(PitG0+PitG1)*.5f,Floor);Traps->RiftAxisA=Along;Traps->RiftAxisB=Across;
        Traps->RiftHalfA=(DS1-DS0)*.5f+20;Traps->RiftHalfB=(PitG1-PitG0)*.5f+20;
        for(int32 K=0;K<Launchers;++K)
        {
            const bool PlusWall=K%2==0;const float W=PlusWall?DS1:DS0;
            const FVector Mouth=G+At(W+(PlusWall?-6:6),LauncherT(K),Floor+36),Far=G+At(PlusWall?DS0:DS1,LauncherT(K),Floor+36);
            // The dart's head ends a few centimetres into the far wall.
            Traps->AddLauncher(Mouth,(Far-Mouth).GetSafeNormal(),FVector::Dist(Mouth,Far)-2,3.2f,FMath::Fmod(K*1.15f,3.2f));++Built.Launchers;
        }
        // The darts: a shaft, an iron head, two crossed fletchings (along +X).
        FSurf Shaft,Head;
        {
            // Faces turned away from Inside.
            auto Out=[](FSurf& M,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& Inside)
            {if(FVector::DotProduct(FVector::CrossProduct(B-A,C-A),A-Inside)>=0)M.Quad(A,B,C,D);else M.Quad(D,C,B,A);};
            const int32 Seg=6;
            for(int32 K=0;K<Seg;++K)
            {
                const float A=K*2*PI/Seg,B=(K+1)*2*PI/Seg;
                const FVector a(0,FMath::Cos(A)*.7f,FMath::Sin(A)*.7f),b(0,FMath::Cos(B)*.7f,FMath::Sin(B)*.7f);
                Out(Shaft,a+FVector(-24,0,0),a,b,b+FVector(-24,0,0),FVector(-12,0,0));
                Out(Head,a*1.5f,b*1.5f,FVector(5,0,0),FVector(5,0,0),FVector(1,0,0));
            }
            for(int32 K=0;K<2;++K)
            {
                // Two crossed fletchings, each faced both ways.
                const FVector Up=K?FVector(0,0,2.2f):FVector(0,2.2f,0),Side=K?FVector(0,1,0):FVector(0,0,1);
                for(float Sg:{-1.f,1.f})for(float Face:{-1.f,1.f})
                    Out(Shaft,FVector(-23,0,0),FVector(-17,0,0),FVector(-18,0,0)+Up*Sg,FVector(-24,0,0)+Up*Sg,FVector(-20,0,0)+Up*Sg*.5f-Side*Face);
            }
        }
        for(int32 I=0;I<20;++I)
        {
            auto* Mesh=MakeMesh(FString::Printf(TEXT("ChultTempleDart%d"),I),Shaft,Wood,G,false,&Head,Iron);
            Mesh->SetCastShadow(false);Traps->AddDart(Mesh);
        }
        // The trap run's moving parts (October 22; ChultTempleRunTraps.cpp): axes, fire jets, saws.
        // Blades and discs in dark forged steel on their own UVs (Tools/create_temple_run_materials.py: it
        // catches the torchlight and doesn't slide over a moving blade); hangers in rusty iron.
        auto* BladeIron=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultTemple/M_TempleBlade.M_TempleBlade"));Missing+=!BladeIron;
        auto Dir=[&](const FVector& F){return (Along*F.X+Across*F.Y+FVector(0,0,F.Z)).GetSafeNormal();};
        for(int32 I=0;I<Movers.Axes.Num();++I)
        {
            const FRunAxe& A=Movers.Axes[I];FSurf Blade,Haft;TempleRunBuild::AxeMesh(Blade,Haft,A.Arm);
            for(FSurf* Sf:{&Blade,&Haft})for(FVector2D& Uv:Sf->UV)Uv*=8.f;   // the iron's rust at a blade's scale
            auto* Mesh=MakeMesh(FString::Printf(TEXT("ChultTempleAxe%d"),I),Blade,BladeIron,G+AtV(A.Pivot),false,&Haft,Wood);
            Traps->AddAxe(Mesh,G+AtV(A.Pivot),Dir(A.Route),A.Arm,A.Amplitude,A.Period,A.Phase);++Built.Axes;
        }
        auto* FlameMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_TorchFlame.M_TorchFlame"));Missing+=!FlameMat;
        // A jet of flame along its local X, from the torches' flame: a few tongues streaming out along
        // the jet, and a row of flames rising off it, taller toward its end (burning gas climbs).
        auto FlameMesh=[&](const FString& Name,const FVector& Mouth,const FVector& D,float Reach,bool bEmber)
        {
            auto* Mesh=NewObject<UProceduralMeshComponent>(Traps,*Name);Mesh->SetupAttachment(Traps->GetRootComponent());
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->RegisterComponent();
            Mesh->SetWorldLocationAndRotation(Mouth,D.Rotation());
            TArray<FVector> V,N;TArray<int32> Tri;TArray<FVector2D> UV;
            FRandomStream FR(GetTypeHash(Name));
            // A flame: three crossed quads round Axis, base at Base, Length along Axis.
            auto Tongue=[&](const FVector& Base,const FVector& Axis,float Length,float Width,float Turn)
            {
                const FVector Ax=Axis.GetSafeNormal(),X0=FVector::CrossProduct(Ax,FMath::Abs(Ax.Z)>.9f?FVector::ForwardVector:FVector::UpVector).GetSafeNormal(),Y0=FVector::CrossProduct(Ax,X0);
                for(int32 K=0;K<3;++K)
                {
                    const float A=K*PI/3+Turn;const FVector Side=X0*FMath::Cos(A)+Y0*FMath::Sin(A);const int32 B=V.Num();
                    V.Append({Base-Side*Width*.5f,Base+Side*Width*.5f,Base+Ax*Length+Side*Width*.5f,Base+Ax*Length-Side*Width*.5f});
                    UV.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)});
                    const FVector Nn=FVector::CrossProduct(Ax,Side);for(int32 J=0;J<4;++J)N.Add(Nn);
                    Tri.Append({B,B+1,B+2,B,B+2,B+3});
                }
            };
            if(bEmber){Tongue(FVector(4,0,-10),FVector(.25f,0,1),24,30,0);Tongue(FVector(-2,0,0),FVector(1,0,.15f),26,28,.4f);}
            else
            {
                for(int32 K=0;K<4;++K)Tongue(FVector(-6+K*Reach*.18f,0,FR.FRandRange(-4,4)),FVector(1,0,FR.FRandRange(.02f,.12f)),Reach*(.45f+.1f*K),46+18*K,FR.FRandRange(0,PI));
                // Broad, overlapping flames rising off the jet (the torch flame fills about half its quad's width).
                const int32 Rising=FMath::Max(6,FMath::RoundToInt(Reach/13));
                for(int32 K=0;K<Rising;++K)
                {
                    const float U=(K+FR.FRandRange(.2f,.8f))/Rising;
                    Tongue(FVector(Reach*(.04f+.94f*U),FR.FRandRange(-12,12),-26+FR.FRandRange(-4,4)),FVector(FR.FRandRange(.15f,.45f),FR.FRandRange(-.2f,.2f),1),32+30*U+FR.FRandRange(-6,8),62+34*U+FR.FRandRange(-8,8),FR.FRandRange(0,PI));
                }
            }
            Mesh->CreateMeshSection_LinearColor(0,V,Tri,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
            Mesh->SetMaterial(0,FlameMat);Mesh->ComponentTags.Add(TEXT("ChultTempleRunFlame"));
            return Mesh;
        };
        for(int32 I=0;I<Movers.Jets.Num();++I)
        {
            const FRunJet& J=Movers.Jets[I];const FVector Mouth=G+AtV(J.Mouth),D=Dir(J.Dir);
            auto* Light=NewObject<UPointLightComponent>(Traps);Light->SetupAttachment(Traps->GetRootComponent());Light->SetMobility(EComponentMobility::Movable);
            Light->SetWorldLocation(Mouth+D*J.Reach*.4f+FVector(0,0,30));Light->SetAttenuationRadius(650);Light->SetLightColor(FLinearColor(1.f,.5f,.2f));
            Light->SetCastShadows(false);Light->SetLightingChannels(false,true,false);Light->SetIntensity(0);Light->RegisterComponent();
            Traps->AddJet(FlameMesh(FString::Printf(TEXT("ChultTempleJet%d"),I),Mouth,D,J.Reach,false),FlameMesh(FString::Printf(TEXT("ChultTempleJetEmber%d"),I),Mouth,D,J.Reach,true),
                Light,Mouth,D,Dir(J.Route),J.Reach,J.Period,J.Phase,J.Warn,J.Burn);
            ++Built.Jets;
        }
        for(int32 I=0;I<Movers.Saws.Num();++I)
        {
            const FRunSaw& S=Movers.Saws[I];FSurf DiscS,ArmS;
            TempleRunBuild::SawMesh(DiscS,S.Radius);TempleRunBuild::SawHanger(ArmS,S.Ceiling-S.Axle.Z+S.Lift+6);
            for(FSurf* Sf:{&DiscS,&ArmS})for(FVector2D& Uv:Sf->UV)Uv*=8.f;
            const FVector Axle=G+AtV(S.Axle),Route=Dir(S.Route);
            auto* Disc=MakeMesh(FString::Printf(TEXT("ChultTempleSaw%d"),I),DiscS,BladeIron,Axle,false);Disc->SetWorldRotation(Route.Rotation());
            auto* ArmM=MakeMesh(FString::Printf(TEXT("ChultTempleSawArm%d"),I),ArmS,Iron,Axle,false);ArmM->SetWorldRotation(Route.Rotation());
            Traps->AddSaw(Disc,ArmM,Axle,Route,S.Radius,S.bDrops,S.Lift,S.Period,S.Phase);++Built.Saws;
        }
    }
    // Fires: a flame and a flickering light on the interior's channel.
    for(const FFire& F:Fires)
    {
        AddDockFlame(Owner,G+AtV(F.Flame),F.Width,F.Height);
        auto* L=NewObject<UPointLightComponent>(Owner);L->SetupAttachment(Owner->GetRootComponent());L->SetMobility(EComponentMobility::Movable);
        L->SetWorldLocation(G+AtV(F.Light));L->SetIntensity(F.Intensity);L->SetAttenuationRadius(F.Radius);
        L->SetLightColor(FLinearColor(1.f,.56f,.27f));L->SetCastShadows(false);L->SetLightingChannels(false,true,false);L->RegisterComponent();
        L->ComponentTags.Add(TEXT("ChultTempleInteriorTorch"));
        if(Traps)Traps->AddTorch(L,F.Intensity);
    }
    Built.Fires=Fires.Num();
    // The Astral's glow up the well's pit and the rift.
    auto Glow=[&](const FVector& F,float Intensity,float Radius)
    {
        auto* L=NewObject<UPointLightComponent>(Owner);L->SetupAttachment(Owner->GetRootComponent());L->SetMobility(EComponentMobility::Movable);
        L->SetWorldLocation(G+AtV(F));L->SetIntensity(Intensity);L->SetAttenuationRadius(Radius);L->SetLightColor(FLinearColor(.48f,.035f,1));
        L->SetCastShadows(false);L->SetLightingChannels(false,true,false);L->RegisterComponent();L->ComponentTags.Add(TEXT("ChultTempleInteriorTorch"));
    };
    Glow(FVector((PitS0+PitS1)*.5f,(PitT0+PitT1)*.5f,WellRingZ-380),12000,1500);
    for(float T:{300.f,1100.f,2000.f})Glow(FVector((RiftS0+RiftS1)*.5f,T,Floor-420),9000,1100);
    Glow(FVector((DS0+DS1)*.5f,(PitG0+PitG1)*.5f,Floor-380),6000,800);
    for(const FGlow& Gl:Glows)Glow(Gl.At,Gl.Intensity,Gl.Radius);   // the Astral wind's ruptures
    // Idols and pots.
    for(const FIdol& I:Idols)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Owner);Mesh->SetupAttachment(Owner->GetRootComponent());
        UStaticMesh* Asset=LoadObject<UStaticMesh>(nullptr,IdolMesh);Missing+=!Asset;Mesh->SetStaticMesh(Asset);
        Mesh->SetWorldLocationAndRotation(G+AtV(I.At),FRotator(0,I.Yaw,0));Mesh->SetWorldScale3D(FVector(I.Radius/25));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->SetLightingChannels(false,true,false);
        Mesh->ComponentTags.Add(TEXT("ChultTempleInterior"));Mesh->RegisterComponent();++Built.Idols;
    }
    for(const FSkull& K:Skulls)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Owner);Mesh->SetupAttachment(Owner->GetRootComponent());
        UStaticMesh* Asset=LoadObject<UStaticMesh>(nullptr,SkullMesh);Missing+=!Asset;Mesh->SetStaticMesh(Asset);
        Mesh->SetWorldLocationAndRotation(G+AtV(K.At),K.Rotation);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetLightingChannels(false,true,false);Mesh->ComponentTags.Add(TEXT("ChultTempleInterior"));Mesh->RegisterComponent();
    }
    for(const FJarSpot& J:Jars)
        if(AClayJar* Pot=AClayJar::PlaceKind(World,J.bUrn?AClayUrn::StaticClass():ATempleJar::StaticClass(),G+AtV(J.At+FVector(0,0,1)),J.Yaw,J.Cigarettes))
        {Pot->SetIndoors();Pot->Tags.Add(TEXT("ChultTempleInteriorJar"));(J.bUrn?Built.Urns:Built.Jars)++;}
    // The skeletons (TempleSkeleton.h), once the floors' collision can be stood on.
    {FTimerHandle SkeletonTimer;TWeakObjectPtr<UWorld> W=World;World->GetTimerManager().SetTimer(SkeletonTimer,[W](){if(W.IsValid()){SpawnTempleSkeletons(W.Get());SpawnTempleSnakes(W.Get());}},.5f,false);}
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_INTERIOR spaces=%d steps=%d torches=%d spike_bands=%d launchers=%d slabs=%d grips=%d axes=%d jets=%d saws=%d idols=%d jars=%d urns=%d wind_tears=%d wind_glows=%d missing_assets=%d missing_sounds=%d triangles=%d build_ms=%.1f"),
        Spaces().Num(),Built.Steps,Built.Fires,Built.Bands,Built.Launchers,Built.Slabs,Built.Grips,Built.Axes,Built.Jets,Built.Saws,Built.Idols,Built.Jars,Built.Urns,ChultTempleWindTears().Num(),Glows.Num(),Missing,Traps?Traps->MissingSounds:-1,Triangles,(FPlatformTime::Seconds()-Started)*1000);
}

// ---------------------------------------------------------------- checks
namespace
{
    // The hidden ramp's height at U along a flight or run (the treads' middles), as Flight()/Run() lay it.
    float RampAt(float UFoot,float UTop,float ZFoot,float ZTop,float U)
    {
        const float Len=FMath::Abs(UTop-UFoot);const int32 N=FMath::RoundToInt((ZTop-ZFoot)/Rise);const float Rr=(ZTop-ZFoot)/N,Tread=Len/N;
        const float Uf=(U-UFoot)*FMath::Sign(UTop-UFoot);
        return FMath::Clamp(ZFoot+Rr*.5f+Uf*Rr/Tread,ZFoot,ZTop);
    }
    // The Astral wind: at Along on a leg (Axis 1, along T) or a bend (Axis 0, along S) centred at Centre
    // across, the walkable point nearest its middle, 28 cm clear of every rupture. False if none.
    bool WindLane(float Centre,float At_,int32 Axis,float& Out)
    {
        const float Half=(Axis==1?TempleWind::Narrow:TempleWind::Wide)*.5f-28;
        for(float D=0;D<=Half;D+=10)for(float Sg:{1.f,-1.f})
        {
            const float C=Centre+Sg*D;
            if(!ChultTempleWindTearAt(Axis==1?FVector2D(C,At_):FVector2D(At_,C),28)){Out=C;return true;}
        }
        return false;
    }
    // The way through, from the cella to the sanctum's dais: frame points on the walking surface.
    struct FRoutePt{FVector F;bool bSlope;bool bGapAfter=false;};
    TArray<FRoutePt> Route()
    {
        TArray<FRoutePt> R;
        auto Add=[&](float S,float T,float Z,bool Sl=false){R.Add({FVector(S,T,Z),Sl});};
        auto AlongS=[&](float From,float To,float T,float UFoot,float UTop,float ZFoot,float ZTop)
        {const float D=FMath::Sign(To-From);for(float S=From;D>0?S<=To+.1f:S>=To-.1f;S+=D*40)Add(S,T,RampAt(UFoot,UTop,ZFoot,ZTop,S),true);};
        auto AlongT=[&](float From,float To,float S,float UFoot,float UTop,float ZFoot,float ZTop)
        {const float D=FMath::Sign(To-From);for(float T=From;D>0?T<=To+.1f:T>=To-.1f;T+=D*40)Add(S,T,RampAt(UFoot,UTop,ZFoot,ZTop,T),true);};
        const float SC=WellS1-WellStair*.5f,TC=WellT1-WellStair*.5f,SW=WellS0+WellStair*.5f,TW=WellT0+WellStair*.5f;
        Add(77960,0,CellaZ+.6f);Add(78150,0,CellaZ);
        AlongS(FlightATop,FlightAFoot,0,FlightAFoot,FlightATop,L1Z,FlightAZ);
        Add(78900,0,L1Z);Add(78900,240,L1Z);
        AlongS(FlightBTop,FlightBFoot,240,FlightBFoot,FlightBTop,L2Z,L1Z);
        Add(77860,240,L2Z);Add(77860,TW,L2Z);Add(SC,TW,L2Z);
        AlongT(PitT0,PitT1,SC,PitT1,PitT0,140,L2Z);Add(SC,TC,140);
        AlongS(PitS1,PitS0,TC,PitS0,PitS1,-260,140);Add(SW,TC,-260);
        AlongT(PitT1,PitT0,SW,PitT0,PitT1,-660,-260);Add(SW,TW,-660);
        AlongS(PitS0,PitS1,TW,PitS1,PitS0,-1060,-660);Add(SC,TW,-1060);
        AlongT(PitT0,PitT1,SC,PitT1,PitT0,WellRingZ,-1060);
        Add(SC,TC,Floor);Add(SW,TC,Floor);Add(SW,AxisT,Floor);Add(WellS0-30,AxisT,Floor);Add(HallS1-160,AxisT,Floor);
        for(float S=HallS1-500;S>=HallS0+200;S-=400)Add(S,AxisT,Floor);
        Add(HallS0-30,AxisT,Floor);
        for(float S=KS1-140;S>=KS0+100;S-=300)Add(S,AxisT,Floor);
        Add(KS0-30,AxisT,Floor);Add(PS1-100,AxisT,Floor);Add(68600,AxisT,Floor);Add(RiftS1+100,AxisT,Floor);
        R.Last().bGapAfter=true;   // over the rift only by the rings
        Add(RiftS0-100,AxisT,Floor);Add((DS0+DS1)*.5f,AxisT,Floor);Add((DS0+DS1)*.5f,PT0+100,Floor);Add((DS0+DS1)*.5f,PT0-30,Floor);
        for(float T=DT1-125;T>=DT0-100;T-=150)Add((DS0+DS1)*.5f,T,Floor);
        // On through the snake chamber, round its middle pier, to the choked way on.
        Add(66900,-6000,Floor);Add(67100,-6650,Floor);Add(67650,-7050,Floor);Add(67700,-7600,Floor);Add(67700,ExitT,Floor);Add(NS1-60,ExitT,Floor);
        // The trap run (October 22): the rubble's foot (climbed), then along each section's floor; over the pits only on the walls.
        {
            using namespace TempleRun;
            Add(NS1+52,ExitT,Floor);R.Last().bGapAfter=true;
            const float BM=(BS0+BS1)*.5f;
            Add(BM,-7850,UZ);
            for(int32 K=0;K<3;++K)
            {
                Add(BM,Pits[K].T0-60,UZ);R.Last().bGapAfter=true;
                Add(BM,Pits[K].T1+60,UZ);
            }
            Add(BM,-4800,UZ);Add(BM,-4600,UZ);
            const float CM=(CausewayT0+CausewayT1)*.5f;
            for(float S=68700;S<=71000;S+=160)Add(S,CM,UZ);
            const float DM=(TempleRun::DS0+TempleRun::DS1)*.5f;   // (the dart gallery has a DS0 of its own)
            Add(DM,-4600,UZ);
            for(float T=-4800;T>=-7600;T-=150)Add(DM,T,UZ);
            const float EM=(ET0+ET1)*.5f;
            for(float S=DM;S<=73640;S+=150)Add(S,EM,UZ);
            const float FM=(FS0+FS1)*.5f;
            Add(FM,EM,UZ);Add(FM,GPits[0].T0-60,UZ);R.Last().bGapAfter=true;
            for(float T=GPits[0].T1+60;T<GPits[1].T0-50;T+=150)Add(FM,T,UZ);
            Add(FM,GPits[1].T0-60,UZ);R.Last().bGapAfter=true;
            Add(FM,GPits[1].T1+60,UZ);Add(FM,AnteT0+100,UZ);Add(FM,AnteT1-60,UZ);
        }
        // The Astral wind (October 22): through the old choke, along each leg and bend round its ruptures
        // (WindLane), over each spike pit only on the walls, onto the sanctum's gallery, down its -S stair, to the dais.
        {
            using namespace TempleWind;
            for(int32 L=0;L<4;++L)
            {
                const FSeg& G=Legs[L];const float Mid=(G.S0+G.S1)*.5f,Start=L==0?TempleRun::AnteT1+20:(G.Dir>0?G.T0-150:G.T1+150),End=G.Dir>0?G.T1+150:G.T0-150;
                const float Near=Pits[L].From-G.Dir*60,Far=Pits[L].To+G.Dir*60;
                bool Gapped=false;
                for(float T=Start;G.Dir>0?T<=End:T>=End;T+=G.Dir*40)
                {
                    if((T-Near)*G.Dir>0&&(T-Far)*G.Dir<0)
                    {
                        if(!Gapped&&R.Num()){Add(Mid,Near,UZ);R.Last().bGapAfter=true;Add(Mid,Far,UZ);Gapped=true;}
                        continue;
                    }
                    float S;if(!WindLane(Mid,T,1,S))continue;
                    Add(S,T,UZ);
                }
                if(L<3)
                {
                    const FSeg& B=Bends[L];const float BM=(B.T0+B.T1)*.5f;
                    for(float S=B.S0+90;S<=B.S1-90;S+=40){float T;if(WindLane(BM,S,0,T))Add(S,T,UZ);}
                }
            }
            Add(HallMid,HT1-60,UZ);Add(HS0+StairW+40,GalleryT+60,UZ);Add(HS0+80,GalleryT+60,UZ);
            AlongT(GalleryT-20,StairFoot+20,HS0+80,StairFoot,GalleryT,HZ,UZ);
            Add(HS0+80,StairFoot-80,HZ);Add(77900,StairFoot-80,HZ);Add(77900,StairFoot+65,HZ);Add(HallMid,StairFoot+65,HZ);
            for(float T=StairFoot-300;T>=DaisT1+300;T-=400)Add(HallMid,T,HZ);
            Add(HallMid,DaisT1+200,HZ);Add(HallMid,DaisT1+80,HZ+49,true);Add(HallMid,DaisT1+10,HZ+90,true);Add(HallMid,DaisT1-120,HZ+DaisH);
        }
        return R;
    }
}

int32 CheckChultTempleInterior(AActor* Owner,AChuckCharacter* Chuck)
{
    UWorld* World=Owner->GetWorld();
    int32 Failures=0,RoutePts=0,Sweeps=0,Probes=0,Intrusions=0,Above=0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultTempleInterior),false,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    auto Fail=[&](const TCHAR* What,const FVector& F){++Failures;if(Failures<30)UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_INTERIOR_FAIL %s at=%s"),What,*F.ToString());};
    // 1. The way through: a floor under every point where the build says, and the capsule clear between them.
    {
        const TArray<FRoutePt> R=Route();FVector Prev=FVector::ZeroVector;
        for(int32 I=0;I<R.Num();++I)
        {
            const FVector F=R[I].F;FHitResult Hit;++RoutePts;
            const FVector W=WorldAt(F.X,F.Y,F.Z);
            const bool Found=World->LineTraceSingleByChannel(Hit,W+FVector(0,0,120),W-FVector(0,0,60),ECC_Visibility,Q);
            if(!Found||FMath::Abs(Hit.ImpactPoint.Z-W.Z)>12||Hit.ImpactNormal.Z<.7f)
            {Fail(TEXT("route_floor"),FVector(F.X,F.Y,Found?Hit.ImpactPoint.Z-W.Z:-999.f));if(Found)UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_INTERIOR_FLOOR_HIT %s"),*GetNameSafe(Hit.GetComponent()));}
            const FVector Body=(Found?FVector(W.X,W.Y,Hit.ImpactPoint.Z):W)+FVector(0,0,Half+(R[I].bSlope?18:4));
            if(I>0&&!R[I-1].bGapAfter&&World->SweepSingleByChannel(Hit,Prev,Body,FQuat::Identity,ECC_Pawn,Capsule,Q))
            {Fail(TEXT("route_blocked"),F);UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_INTERIOR_BLOCKED_BY %s %s"),*GetNameSafe(Hit.GetComponent()),*GetNameSafe(Hit.GetActor()));}
            Prev=Body;++Sweeps;
        }
        // The snake chamber's way on is climbed, not walked (October 22: the rubble's first block stops the capsule),
        // (the trap run's antechamber opens on the Astral wind since October 22).
        FHitResult Hit;++Sweeps;
        if(!World->SweepSingleByChannel(Hit,WorldAt(NS1-60,ExitT,Floor+Half+4),WorldAt(TempleRun::CollapseS1-30,ExitT,Floor+Half+4),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("rubble_open"),FVector(NS1,ExitT,0));
        // (The antechamber's way on was choked until the Astral wind, October 22: the route goes on through it now.)
    }
    // 2. Nothing of the exterior (masonry, floor, trunks) inside any space: spheres on a grid.
    {
        FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        const auto Probe=FCollisionShape::MakeSphere(45);
        auto Test=[&](const FVector& F)
        {
            ++Probes;TArray<FOverlapResult> Hits;
            if(!World->OverlapMultiByObjectType(Hits,WorldAt(F.X,F.Y,F.Z),FQuat::Identity,Objects,Probe,Q))return;
            for(const FOverlapResult& H:Hits)
            {
                UPrimitiveComponent* C=H.GetComponent();if(!C||C->ComponentHasTag(TEXT("ChultTempleInterior")))continue;
                if(H.GetActor()&&(H.GetActor()->IsA<AChuckBreakable>()||H.GetActor()->IsA<APawn>()))continue;
                if(C->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
                ++Intrusions;if(Intrusions<15)UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_INTERIOR_INTRUSION %s (%s) at=%s"),*GetNameSafe(C),*GetNameSafe(H.GetActor()),*F.ToString());
                return;
            }
        };
        for(const FSpace& Sp:Spaces())
        {
            const bool Deep=FCString::Strcmp(Sp.Name,TEXT("Well"))==0||FCString::Strcmp(Sp.Name,TEXT("Piers"))==0||FCString::Strcmp(Sp.Name,TEXT("Darts"))==0;
            const float Z0=Deep?WellBottom+50:Sp.Z0+50,Z1=Sp.Z1-50;
            for(float S=Sp.S0+50;S<=Sp.S1-50;S+=80)for(float T=Sp.T0+50;T<=Sp.T1-50;T+=80)
            {
                if(Sp.bFlight){const float L=FlightLine(Sp,S);for(float Z=L+50;Z<=L+Sp.H-50;Z+=80)Test(FVector(S,T,Z));continue;}
                for(float Z=Z0;Z<=Z1;Z+=80)Test(FVector(S,T,Z));
            }
        }
        if(Intrusions)Fail(TEXT("exterior_inside"),FVector(Intrusions,0,0));
    }
    // 3. Under the plaza the ground stays well above every ceiling (the trees' trunks go about 1.3 m down).
    for(const FSpace& Sp:Spaces())
    {
        if(Sp.Z1>-300)continue;
        for(float S=Sp.S0;S<=Sp.S1;S+=200)for(float T=Sp.T0;T<=Sp.T1;T+=200)
        {
            ++Above;const float Ground=ChultCanyonGroundZ(ChultCanyonFrame(S,T))-PZ();
            if(Ground<Sp.Z1+400)Fail(TEXT("ground_too_close"),FVector(S,T,Ground-Sp.Z1));
        }
    }
    // 4. The spawns: a floor, room to stand, Chult's, and each returns a death there to itself.
    for(int32 I=0;I<TempleInteriorSpawnCount;++I)
    {
        const FVector P=ChultTempleInteriorSpawn(ETempleInteriorSpawn(I));FHitResult Hit;FVector Back;
        const bool OnFloor=World->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,Half+40),ECC_Visibility,Q);
        if(!OnFloor||FMath::Abs(Hit.ImpactPoint.Z-(P.Z-34.65f))>6||World->OverlapBlockingTestByChannel(P+FVector(0,0,2),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("spawn"),WorldFrame(P));
        if(!AChultLanding::Contains(P)||!ChultTempleInteriorRespawn(P,Back)||FVector::Dist(Back,P)>1)Fail(TEXT("spawn_return"),WorldFrame(P));
        float Yaw;if(!IsChultTempleInteriorSpawn(P,Yaw))Fail(TEXT("spawn_yaw"),WorldFrame(P));
    }
    {
        // Deaths in the pit, the hall, the corridor, the rift and the gallery.
        struct FMap{FVector F;ETempleInteriorSpawn Want;};
        const FMap Maps[]={{FVector(78000,1100,-1800),ETempleInteriorSpawn::Well},{FVector(78300,240,900),ETempleInteriorSpawn::Well},{FVector(75000,AxisT,Floor+40),ETempleInteriorSpawn::Hall},
            {FVector(71300,AxisT,Floor+40),ETempleInteriorSpawn::Spikes},{FVector(67800,AxisT,Floor-500),ETempleInteriorSpawn::Piers},{FVector(66900,-3000,Floor+40),ETempleInteriorSpawn::Darts},{FVector(66900,-7000,Floor+40),ETempleInteriorSpawn::Snakes},{FVector(66900,-5400,Floor+40),ETempleInteriorSpawn::Snakes},
            {FVector(68100,ExitT,Floor+150),ETempleInteriorSpawn::Climb},{FVector(68400,-6300,TempleRun::UZ-100),ETempleInteriorSpawn::Pits},{FVector(70000,-4460,TempleRun::UZ+40),ETempleInteriorSpawn::Axes},
            {FVector(70000,-4650,TempleRun::UZ-100),ETempleInteriorSpawn::Axes},{FVector(71230,-6000,TempleRun::UZ+40),ETempleInteriorSpawn::Fire},{FVector(72500,-7820,TempleRun::UZ+40),ETempleInteriorSpawn::Saws},
            {FVector(73650,-5000,TempleRun::UZ+40),ETempleInteriorSpawn::Gauntlet},{FVector(73650,-4300,TempleRun::UZ-100),ETempleInteriorSpawn::Gauntlet},{FVector(73650,-3500,TempleRun::UZ+40),ETempleInteriorSpawn::Gauntlet}};
        for(const FMap& M:Maps){FVector Back;if(!ChultTempleInteriorRespawn(WorldAt(M.F.X,M.F.Y,M.F.Z),Back)||FVector::Dist(Back,ChultTempleInteriorSpawn(M.Want))>1)Fail(TEXT("death_map"),M.F);}
        // Outside it a death is the far bank's as before.
        FVector Back;if(ChultTempleInteriorRespawn(WorldAt(70000,4000,40),Back)||ChultTempleInteriorRespawn(WorldAt(77300,0,1500+40),Back))Fail(TEXT("death_map_outside"),FVector::ZeroVector);
        // The pit and the rift swallow; their lips and the slabs don't.
        for(const FVector& F:{FVector(78000,1100,-1700),FVector(67800,AxisT,Floor-200),FVector(67800,300,Floor-120),FVector((DS0+DS1)*.5f,(PitG0+PitG1)*.5f,Floor-150),
            FVector(68400,-6300,TempleRun::UZ-100),FVector(68280,-5400,TempleRun::UZ-100),FVector(70000,-4650,TempleRun::UZ-100),FVector(73650,-4300,TempleRun::UZ-100),FVector(73480,-6450,TempleRun::UZ-100)})
            if(!ChultTempleInteriorSwallows(WorldAt(F.X,F.Y,F.Z)))Fail(TEXT("no_swallow"),F);
        for(const FVector& F:{FVector(WellS0+100,1100,WellRingZ+1),FVector(67800,AxisT,Floor+1),FVector(68100,AxisT,Floor+1),FVector(78000,1100,500),
            FVector(68400,-6300,TempleRun::UZ-60),FVector(68400,-6800,TempleRun::UZ+1),FVector(70000,-4460,TempleRun::UZ+1),FVector(68400,-7950,TempleRun::RubbleTop+1)})
            if(ChultTempleInteriorSwallows(WorldAt(F.X,F.Y,F.Z)))Fail(TEXT("swallow_safe"),F);
    }
    // The rift has no way across but the rings: no floor anywhere over it.
    for(float S=RiftS0+20;S<RiftS1-10;S+=50)for(float T=PT0+20;T<PT1-10;T+=100)
    {
        FHitResult Hit;const FVector W=WorldAt(S,T,Floor+60);++Sweeps;
        if(World->SweepSingleByChannel(Hit,W,W-FVector(0,0,180),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(14),Q))Fail(TEXT("rift_floor"),FVector(S,T,Hit.ImpactPoint.Z));
    }
    // 4b. The Astral wind (October 22): every rupture open, a way round them all, each pit's run-up clear, deaths.
    int32 WindLanes=0,WindCells=0;
    {
        using namespace TempleWind;
        const TArray<FTempleWindTear>& Tears=ChultTempleWindTears();
        for(const FTempleWindTear& Tr:Tears)
        {
            // Open: nothing to stand on in it down to 2 m (its Astral surface has no collision); its rim is floor.
            FHitResult Hit;const FVector W=WorldAt(Tr.Mid.X,Tr.Mid.Y,UZ+60);++Sweeps;
            if(World->SweepSingleByChannel(Hit,W,W-FVector(0,0,260),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(6),Q))Fail(TEXT("tear_floor"),FVector(Tr.Mid.X,Tr.Mid.Y,Hit.ImpactPoint.Z-WorldAt(0,0,UZ).Z));
            if(!ChultTempleInteriorSwallows(WorldAt(Tr.Mid.X,Tr.Mid.Y,UZ-80)))Fail(TEXT("tear_no_swallow"),FVector(Tr.Mid.X,Tr.Mid.Y,0));
            const FSeg& Sg=Tr.Seg<4?Legs[Tr.Seg]:Bends[Tr.Seg-4];const FBox2D On(FVector2D(Sg.S0+8,Sg.T0+8),FVector2D(Sg.S1-8,Sg.T1-8));
            FVector2D Rim=Tr.Edge[0]+(Tr.Edge[0]-Tr.Mid).GetSafeNormal()*30;
            for(const int32 E:{0,18,9,27})if(Tr.Edge.IsValidIndex(E)){const FVector2D Cand=Tr.Edge[E]+(Tr.Edge[E]-Tr.Mid).GetSafeNormal()*30;if(On.IsInside(Cand)&&!ChultTempleWindTearAt(Cand,4)){Rim=Cand;break;}}
            if(ChultTempleInteriorSwallows(WorldAt(Rim.X,Rim.Y,UZ+1)))Fail(TEXT("tear_rim_swallows"),FVector(Rim.X,Rim.Y,0));
            const FVector R0=WorldAt(Rim.X,Rim.Y,UZ+40);
            if(!World->LineTraceSingleByChannel(Hit,R0,R0-FVector(0,0,80),ECC_Visibility,Q)||FMath::Abs(Hit.ImpactPoint.Z-WorldAt(0,0,UZ).Z)>4)Fail(TEXT("tear_rim"),FVector(Rim.X,Rim.Y,0));
        }
        // A way round: a 10 cm grid of the tunnels' floors where his capsule (18 cm round) stands clear of every
        // rupture and pit, flooded from each start (the wind's respawn, each pit's far side) to the next goal.
        const FBox2D Area(FVector2D(TempleRun::FS0-20,FMath::Min(Bends[1].T0,Legs[3].T0)-20),FVector2D(Bends[2].S1+20,Bends[0].T1+20));
        const int32 NX=FMath::CeilToInt((Area.Max.X-Area.Min.X)/10),NY=FMath::CeilToInt((Area.Max.Y-Area.Min.Y)/10);
        TArray<FBox2D> Floors={FBox2D(FVector2D(TempleRun::FS0,TempleRun::AnteT1),FVector2D(TempleRun::FS1,ExitT1))};
        for(const FSeg& G:Legs)Floors.Add(FBox2D(FVector2D(G.S0,G.T0),FVector2D(G.S1,G.T1)));
        for(const FSeg& G:Bends)Floors.Add(FBox2D(FVector2D(G.S0,G.T0),FVector2D(G.S1,G.T1)));
        TArray<FBox2D> PitBoxes;for(int32 L=0;L<4;++L)PitBoxes.Add(FBox2D(FVector2D(Legs[L].S0,FMath::Min(Pits[L].From,Pits[L].To)),FVector2D(Legs[L].S1,FMath::Max(Pits[L].From,Pits[L].To))));
        auto Stand=[&](const FVector2D& C)
        {
            for(int32 K=0;K<9;++K)
            {
                const FVector2D P=K==8?C:C+FVector2D(FMath::Cos(K*PI/4),FMath::Sin(K*PI/4))*18;
                bool On=false;for(const FBox2D& B:Floors)On|=B.IsInside(P);
                if(!On)return false;
                for(const FBox2D& B:PitBoxes)if(B.IsInside(P))return false;
            }
            return !ChultTempleWindTearAt(C,18);
        };
        TArray<uint8> Free;Free.SetNumZeroed(NX*NY);
        for(int32 X=0;X<NX;++X)for(int32 Y=0;Y<NY;++Y){Free[X*NY+Y]=Stand(Area.Min+FVector2D(X*10+5,Y*10+5));WindCells+=Free[X*NY+Y];}
        auto Cell=[&](float S,float T){return FIntPoint(FMath::FloorToInt((S-Area.Min.X)/10),FMath::FloorToInt((T-Area.Min.Y)/10));};
        auto Reaches=[&](const FVector2D& From,const FVector2D& To)
        {
            const FIntPoint A=Cell(From.X,From.Y),B=Cell(To.X,To.Y);
            if(!Free[A.X*NY+A.Y]||!Free[B.X*NY+B.Y])return false;
            TArray<uint8> Seen;Seen.SetNumZeroed(NX*NY);TArray<FIntPoint> Open={A};Seen[A.X*NY+A.Y]=1;
            for(int32 I=0;I<Open.Num();++I)
            {
                const FIntPoint C=Open[I];if(C==B)return true;
                for(const FIntPoint D:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
                {
                    const FIntPoint N=C+D;if(N.X<0||N.Y<0||N.X>=NX||N.Y>=NY)continue;
                    const int32 Id=N.X*NY+N.Y;if(!Free[Id]||Seen[Id])continue;Seen[Id]=1;Open.Add(N);
                }
            }
            return false;
        };
        const FVector2D Wind1(TempleRun::FS0+90,-3000);
        FVector2D From=Wind1;
        for(int32 L=0;L<=4;++L)
        {
            const FVector2D Goal=L<4?FVector2D((Legs[L].S0+Legs[L].S1)*.5f,Pits[L].From-Legs[L].Dir*50):FVector2D((Legs[3].S0+Legs[3].S1)*.5f,Legs[3].T0+40);
            if(Reaches(From,Goal))++WindLanes;else Fail(TEXT("wind_lane"),FVector(From.X,From.Y,L));
            if(L<4)From=FVector2D((Legs[L].S0+Legs[L].S1)*.5f,Pits[L].To+Legs[L].Dir*60);
        }
        // Each bend from its respawn to the next leg's start.
        for(int32 B=0;B<3;++B)
        {
            const FVector2D A((Legs[B].S0+Legs[B].S1)*.5f,(Bends[B].T0+Bends[B].T1)*.5f),C((Legs[B+1].S0+Legs[B+1].S1)*.5f,(Bends[B].T0+Bends[B].T1)*.5f);
            if(Reaches(A,C))++WindLanes;else Fail(TEXT("wind_bend_lane"),FVector(B,0,0));
        }
        // Each pit's run-up: 2.5 m along a wall (32 cm off it) to its lip, clear of ruptures; leg 3's on its +S wall.
        for(int32 L=0;L<4;++L)
        {
            int32 ClearSides=0;
            for(int32 Sg:{0,1})
            {
                if(L==2&&Sg==0)continue;
                const float S=Sg?Legs[L].S1-32:Legs[L].S0+32;bool Ok=true;
                for(float U=0;U<=250;U+=10)Ok&=!ChultTempleWindTearAt(FVector2D(S,Pits[L].From-Legs[L].Dir*U),20);
                ClearSides+=Ok;
            }
            if(!ClearSides)Fail(TEXT("wind_runup"),FVector(L,0,0));
        }
        // Deaths in each part map to its own respawn; the pits and alcoves swallow, their lips do not.
        struct FMap{FVector F;ETempleInteriorSpawn Want;};
        const FMap Maps[]={{FVector(TempleRun::FS0+90,-2500,UZ+40),ETempleInteriorSpawn::Wind1},{FVector(74600,-710,UZ+40),ETempleInteriorSpawn::Wind2},
            {FVector(75510,-2000,UZ-100),ETempleInteriorSpawn::Wind3},{FVector(76300,-3150,UZ+40),ETempleInteriorSpawn::Wind4},{FVector(77210,-2000,UZ-100),ETempleInteriorSpawn::Wind5},
            {FVector(77120-60,-2300,UZ-100),ETempleInteriorSpawn::Wind5},{FVector(78000,-710,UZ+40),ETempleInteriorSpawn::Wind6},{FVector(78710,-2000,UZ-100),ETempleInteriorSpawn::Wind7},
            {FVector(HallMid,-5400,HZ+40),ETempleInteriorSpawn::Sanctum},{FVector(HallMid,GalleryT+200,UZ+40),ETempleInteriorSpawn::Sanctum}};
        for(const FMap& M:Maps){FVector Back;if(!ChultTempleInteriorRespawn(WorldAt(M.F.X,M.F.Y,M.F.Z),Back)||FVector::Dist(Back,ChultTempleInteriorSpawn(M.Want))>1)Fail(TEXT("wind_death_map"),M.F);}
        for(int32 L=0;L<4;++L)
        {
            const float Mid=(Legs[L].S0+Legs[L].S1)*.5f,T=(Pits[L].From+Pits[L].To)*.5f;
            if(!ChultTempleInteriorSwallows(WorldAt(Mid,T,UZ-100)))Fail(TEXT("wind_pit_swallow"),FVector(Mid,T,0));
            if(ChultTempleInteriorSwallows(WorldAt(Mid,Pits[L].From-Legs[L].Dir*40,UZ+1)))Fail(TEXT("wind_lip_swallows"),FVector(Mid,T,0));
        }
        if(!ChultTempleInteriorSwallows(WorldAt(Legs[2].S0-60,(BreakMinusT0+BreakMinusT1)*.5f,UZ-100))||!ChultTempleInteriorSwallows(WorldAt(Legs[2].S1+60,(BreakPlusT0+BreakPlusT1)*.5f,UZ-100)))Fail(TEXT("wind_alcove_swallow"),FVector::ZeroVector);
        if(ChultTempleInteriorSwallows(WorldAt(HallMid,-5400,HZ+1))||ChultTempleInteriorSwallows(WorldAt(HallMid,GalleryT+200,UZ+1)))Fail(TEXT("sanctum_swallows"),FVector::ZeroVector);
        // The sanctum's gallery holds him; nothing blocks the hall's floor along the processional.
        {
            FHitResult Hit;const FVector A=WorldAt(HallMid,GalleryT+100,UZ+40);
            if(!World->LineTraceSingleByChannel(Hit,A,A-FVector(0,0,80),ECC_Visibility,Q)||FMath::Abs(Hit.ImpactPoint.Z-WorldAt(0,0,UZ).Z)>4)Fail(TEXT("gallery_floor"),FVector::ZeroVector);
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_WIND_CHECK lanes=%d cells=%d tears=%d"),WindLanes,WindCells,ChultTempleWindTears().Num());
    // 5. The rings: where they hang, the first caught from a standing jump, the body clear between them.
    int32 Grips=0;
    if(Built.FirstGrip>=0&&GetChuckSwingGrips().Num()>=Built.FirstGrip+int32(UE_ARRAY_COUNT(RingS)))
    {
        for(int32 K=0;K<int32(UE_ARRAY_COUNT(RingS));++K)
        {
            const FVector Want=WorldAt(RingS[K],RingT,RingZ),Got=GetChuckSwingGrips()[Built.FirstGrip+K].Grip;
            if(FVector::Dist(Want,Got)>1)Fail(TEXT("ring_grip"),FVector(K,0,0));else ++Grips;
            if(K>0)
            {
                FHitResult Hit;const FVector A=GetChuckSwingGrips()[Built.FirstGrip+K-1].Grip-FVector(0,0,31.5f),B=Got-FVector(0,0,31.5f);++Sweeps;
                if(World->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("ring_path"),FVector(K,0,0));
            }
        }
        // His paws at a standing jump's top: SwingGripZ 63.5 above his feet, an 18 cm jump.
        const float Reach=RingZ-(Floor+63.5f+18.f);
        if(Reach>30||Reach<-45)Fail(TEXT("ring_reach"),FVector(Reach,0,0));
    }
    else Fail(TEXT("rings_missing"),FVector::ZeroVector);
    // 6. The traps, the light.
    int32 Bands=0,Launch=0,Slabs=0,Torches=0;
    if(AChultTempleTraps* Traps=AChultTempleTraps::Find(World))
    {
        Bands=Traps->Spikes.Num();Launch=Traps->Launchers.Num();Slabs=Traps->Slabs.Num();Torches=Traps->TorchCount();
        if(Bands!=5||Launch!=Launchers||Slabs!=3||Traps->Darts.Num()<16)Fail(TEXT("traps"),FVector(Bands,Launch,Slabs));
        // The trap run: 4 + 2 axes, 9 + 2 fire jets, 6 + 2 saw rows of two discs.
        if(Traps->Axes.Num()!=6||Traps->Jets.Num()!=11||Traps->Saws.Num()!=16)Fail(TEXT("run_traps"),FVector(Traps->Axes.Num(),Traps->Jets.Num(),Traps->Saws.Num()));
        // Each band is dangerous for about a second of its three.
        for(int32 B=0;B<Bands;++B)
        {
            int32 Danger=0;for(int32 K=0;K<300;++K)Danger+=Traps->SpikeDanger(B,K*.01);
            if(Danger<80||Danger>110)Fail(TEXT("spike_cycle"),FVector(B,Danger,0));
        }
    }
    else Fail(TEXT("traps_missing"),FVector::ZeroVector);
    {
        TArray<UProceduralMeshComponent*> Meshes;Owner->GetComponents(Meshes);
        for(auto* M:Meshes)if(M->ComponentHasTag(TEXT("ChultTempleInterior"))&&M->GetName()!=TEXT("ChultTempleInteriorAstral")&&(M->LightingChannels.bChannel0||!M->LightingChannels.bChannel1))Fail(TEXT("light_channel"),FVector::ZeroVector);
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_INTERIOR_CHECK failures=%d route_points=%d sweeps=%d probes=%d intrusions=%d ground_probes=%d spawns=%d grips=%d bands=%d launchers=%d slabs=%d torches=%d"),
        Failures,RoutePts,Sweeps,Probes,Intrusions,Above,TempleInteriorSpawnCount,Grips,Bands,Launch,Slabs,Torches);
    return Failures;
}

// ---------------------------------------------------------------- review
void ReviewChultTempleInterior(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    UWorld* World=Owner->GetWorld();
    struct FView{const TCHAR* Name;FVector Eye,Aim;};
    const float SC=WellS1-WellStair*.5f,TC=WellT1-WellStair*.5f,SW=WellS0+WellStair*.5f,TW=WellT0+WellStair*.5f;
    const TArray<FView> Views={
        {TEXT("InteriorDoorway"),FVector(77450,-60,CellaZ+120),FVector(78110,0,CellaZ+110)},
        {TEXT("InteriorCella"),FVector(77830,-170,CellaZ+150),FVector(78110,20,CellaZ+100)},
        {TEXT("InteriorFlightA"),FVector(78150,-40,CellaZ+120),FVector(78800,0,L1Z+40)},
        {TEXT("InteriorFlightB"),FVector(78960,250,L1Z+110),FVector(77960,240,L2Z+60)},
        {TEXT("InteriorWellGallery"),FVector(77560,600,L2Z+150),FVector(78560,1500,60)},
        {TEXT("InteriorWellDown"),FVector(78000,820,L2Z+60),FVector(78010,1110,WellRingZ-200)},
        {TEXT("InteriorWellUp"),FVector(SW+40,TC,WellRingZ+90),FVector(78200,900,700)},
        {TEXT("InteriorWellRing"),FVector(SC,TC-40,WellRingZ+120),FVector(SW,AxisT,WellRingZ+60)},
        {TEXT("InteriorHall"),FVector(HallS1-140,AxisT+60,Floor+160),FVector(HallS0,AxisT,Floor+170)},
        {TEXT("InteriorHallSide"),FVector(74900,HallT0+160,Floor+120),FVector(76300,AxisT+300,Floor+330)},
        {TEXT("InteriorSpikes"),FVector(KS1-80,KT0+70,Floor+110),FVector(KS0,AxisT+60,Floor+40)},
        {TEXT("InteriorSpikesClose"),FVector(SpikeS[0]+260,KT1-60,Floor+70),FVector(SpikeS[0],AxisT,Floor+10)},
        {TEXT("InteriorPierHall"),FVector(PS1-120,2380,Floor+260),FVector(66800,300,Floor+60)},
        {TEXT("InteriorRift"),FVector(RiftS1+330,1460,Floor+190),FVector(RiftS0-100,AxisT-60,Floor-80)},
        {TEXT("InteriorSlabPit"),FVector((DS0+DS1)*.5f+60,DT1-40,Floor+100),FVector((DS0+DS1)*.5f,PitG0-80,Floor-40)},
        {TEXT("InteriorRings"),FVector(RiftS1+420,RingT-330,Floor+150),FVector(RiftS0-60,RingT+60,RingZ+40)},
        {TEXT("InteriorDarts"),FVector((DS0+DS1)*.5f,DT1-60,Floor+110),FVector((DS0+DS1)*.5f,-3000,Floor+40)},
        {TEXT("InteriorSnakeArch"),FVector((DS0+DS1)*.5f,ChokeT+420,Floor+110),FVector((DS0+DS1)*.5f,-6600,Floor+120)},
        {TEXT("InteriorSnakes"),FVector(NS0+120,NT1-120,Floor+260),FVector(67300,-7700,Floor+40)},
        {TEXT("InteriorSnakeIdols"),FVector(66420,-7350,Floor+110),FVector(66900,NT0,Floor+170)}};
    auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(74);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController())){PC->SetViewTarget(Camera);PC->ConsoleCommand(TEXT("r.VSync 0"));PC->ConsoleCommand(TEXT("t.MaxFPS 0"));}
    // Under the camera while it looks: the light and fog follow the camera (ChultTemple.cpp), Chuck stays out of shot.
    Chuck->ResetAtLocation(ChultTempleInteriorSpawn(ETempleInteriorSpawn::Hall));

    enum class ET:uint8{Descent,Spikes,SlabRun,SlabFall,Rings,Darts,WellFall,Hearing};
    const ET Trials[]={ET::Descent,ET::Spikes,ET::SlabRun,ET::SlabFall,ET::Rings,ET::Darts,ET::WellFall,ET::Hearing};
    const TCHAR* TrialNames[]={TEXT("descent"),TEXT("spikes"),TEXT("slab_run"),TEXT("slab_fall"),TEXT("rings"),TEXT("darts"),TEXT("well_fall"),TEXT("hearing")};
    struct FRun{int32 Phase=0,Trial=-1,Leg=0,Sub=0,Band=0,Failures=0,Jumps=0,Deaths=0,Hits=0,DartHits=0,Fallen=0,Restored=0,Leaps=0,Timed=0;bool bHit=false;
        int32 TraceSub=-1,TraceBand=-1;double Next=0,Start=0,LastJump=-100,Mark=-1,ViewClock=0,StallAt=-1;FVector StallFrom=FVector::ZeroVector;uint64 ViewFrame=0;bool bDone=false,bStarted=false,bPass=false;TArray<int32> Results;TWeakObjectPtr<ACameraActor> Camera;FTimerHandle Timer;};
    auto Run=MakeShared<FRun>();Run->Failures=Failures;Run->Camera=Camera;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Views,Trials,TrialNames,SC,TC,SW,TW]()
    {
        if(Run->bDone)return;
        UWorld* World=Owner->GetWorld();const double Now=World->GetTimeSeconds();
        const int32 I=Run->Phase/2;
        if(I<Views.Num())
        {
            if(Now<Run->Next)return;Run->Next=Now+2.5;
            if(Run->Phase%2==0)
            {
                if(Run->Camera.IsValid())Run->Camera->SetActorLocationAndRotation(WorldAt(Views[I].Eye.X,Views[I].Eye.Y,Views[I].Eye.Z),(AtV(Views[I].Aim)-AtV(Views[I].Eye)).Rotation());
                Run->ViewClock=-1;   // the frame clock starts a second later, once the view has settled
            }
            else
            {
                // Two stills wait (up to 4 s) for their moment: the first band's spikes up, a dart crossing.
                if(Run->ViewClock<0){Run->ViewClock=FPlatformTime::Seconds();Run->ViewFrame=GFrameCounter;Run->Next=Now+1.2;return;}
                AChultTempleTraps* T=AChultTempleTraps::Find(World);const FString Name=Views[I].Name;
                bool Ready=true;
                if(T&&Name==TEXT("InteriorSpikesClose"))Ready=T->SpikeTip(0,Now)>=AChultTempleTraps::SpikeTall-2;
                if(T&&Name==TEXT("InteriorDarts")){Ready=false;for(const auto& D:T->Darts)Ready|=D.bFlying&&D.Launcher<2&&D.Left>T->Launchers[D.Launcher].Span*.3f&&D.Left<T->Launchers[D.Launcher].Span*.7f;}
                if(!Ready&&Run->Mark<0)Run->Mark=Now;
                if(!Ready&&Now-Run->Mark<4){Run->Next=Now;return;}
                Run->Mark=-1;
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Name+TEXT(".png")),true,false);const double Frames=FMath::Max<double>(1,double(GFrameCounter-Run->ViewFrame));
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_INTERIOR_VIEW name=%s ready=%d avg_ms=%.1f"),*Name,Ready,(FPlatformTime::Seconds()-Run->ViewClock)*1000/Frames);
            }
            ++Run->Phase;return;
        }
        // -ChuckInteriorViewsOnly: the stills only (look iterations).
        if(FParse::Param(FCommandLine::Get(),TEXT("ChuckInteriorViewsOnly"))){Run->bDone=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_INTERIOR_TEST views_only=1"));FPlatformMisc::RequestExitWithStatus(false,0);return;}
        AChultTempleTraps* Traps=AChultTempleTraps::Find(World);
        auto* M=Chuck->GetCharacterMovement();
        const FVector P=WorldFrame(Chuck->GetActorLocation());
        auto Steer=[&](float S,float T)->bool
        {
            if(FVector2D::Distance(FVector2D(P.X,P.Y),FVector2D(S,T))<35){Run->StallAt=-1;return true;}
            // Not getting anywhere for 3 s: say where and what is ahead (once per stall).
            if(Run->StallAt<0||FVector::Dist2D(P,Run->StallFrom)>30){Run->StallAt=Now;Run->StallFrom=P;}
            else if(Now-Run->StallAt>3)
            {
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(InteriorStall),false,Chuck);
                const FVector Ahead=(WorldAt(S,T,P.Z)-Chuck->GetActorLocation()).GetSafeNormal2D()*40;
                World->SweepSingleByChannel(Hit,Chuck->GetActorLocation(),Chuck->GetActorLocation()+Ahead,FQuat::Identity,ECC_Pawn,Chuck->GetCapsuleComponent()->GetCollisionShape(),Q);
                UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_INTERIOR_STALL at=%s to=(%.0f,%.0f) gait=%s ground=%d blocked_by=%s %s"),*P.ToString(),S,T,Chuck->GetGaitName(),Chuck->GetCharacterMovement()->IsMovingOnGround(),*GetNameSafe(Hit.GetComponent()),*GetNameSafe(Hit.GetActor()));
                Run->StallAt=Now;Run->StallFrom=P;
            }
            const FVector D=(WorldAt(S,T,P.Z)-Chuck->GetActorLocation()).GetSafeNormal2D();
            Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return false;
        };
        auto Stop=[&](){Chuck->SetTestStick(FVector2D::ZeroVector);};
        auto Reset=[&](const FVector& At_,float Yaw){Chuck->ResetAtLocation(At_);Chuck->SetActorRotation(FRotator(0,Yaw,0));Chuck->SetRunHeld(false);Stop();};
        auto Spawn=[&](ETempleInteriorSpawn S){Reset(ChultTempleInteriorSpawn(S),ChultTempleInteriorSpawnYaw(S));};
        // Next trial.
        if(!Run->bStarted)
        {
            if(Now<Run->Next)return;
            ++Run->Trial;Run->Sub=Run->Leg=Run->Band=Run->Jumps=Run->Timed=0;Run->Mark=-1;Run->bPass=Run->bHit=false;Run->LastJump=-100;
            FString Only;
            if(FParse::Value(FCommandLine::Get(),TEXT("ChuckInteriorTrials="),Only))
                while(Run->Trial<UE_ARRAY_COUNT(Trials)&&!Only.Contains(TrialNames[Run->Trial])){Run->Results.Add(1);++Run->Trial;}
            if(Run->Trial>=UE_ARRAY_COUNT(Trials))
            {
                int32 Fails=Run->Failures;FString Line;
                for(int32 K=0;K<Run->Results.Num();++K){Fails+=!Run->Results[K];Line+=FString::Printf(TEXT(" %s=%d"),TrialNames[K],Run->Results[K]);}
                Run->bDone=true;Stop();
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_INTERIOR_TEST failures=%d%s"),Fails,*Line);
                FPlatformMisc::RequestExitWithStatus(false,Fails?1:0);
                return;
            }
            if(Run->Camera.IsValid())Run->Camera->Destroy();
            // The stills ran uncapped to time them; the trials' bot steers once per 16 ms
            // timer tick, so above 60 fps it would push the stick only every other frame.
            if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->ConsoleCommand(TEXT("t.MaxFPS 60"));
            Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);
            Run->Deaths=Chuck->GetFallDeaths();Run->Hits=Traps?Traps->SpikeHits:0;Run->DartHits=Traps?Traps->DartHits:0;
            Run->Fallen=Traps?Traps->SlabsFallen:0;Run->Restored=Traps?Traps->SlabsRestored:0;Run->Leaps=Chuck->GetSwingLeaps();
            switch(Trials[Run->Trial])
            {
            case ET::Descent:Reset(WorldAt(SanctuaryFront-200,0,CellaZ+34.65f),YawOf(1,0));Chuck->SetRunHeld(true);break;
            case ET::Spikes:Spawn(ETempleInteriorSpawn::Spikes);break;
            case ET::SlabRun:case ET::SlabFall:Spawn(ETempleInteriorSpawn::Darts);break;
            case ET::Rings:Reset(WorldAt(RingS[0]+15,RingT,Floor+34.65f),YawOf(-1,0));break;
            case ET::Darts:Spawn(ETempleInteriorSpawn::Darts);break;
            case ET::WellFall:Spawn(ETempleInteriorSpawn::Well);break;
            // On the avenue right over the spike corridor (the plaza, 14.6 m above it).
            case ET::Hearing:Reset(WorldAt(SpikeS[1],AxisT,34.65f),YawOf(1,0));Run->Hits=Traps?Traps->SoundsPlayed:0;break;
            }
            Chuck->SetOrbitPitch(-12);Chuck->Recenter();
            Run->Start=Now;Run->bStarted=true;return;
        }
        const float Time=Now-Run->Start;const bool Ground=M->IsMovingOnGround();
        if(Run->Sub!=Run->TraceSub||Run->Band!=Run->TraceBand)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_STEP trial=%s band=%d sub=%d t=%.2f at=%s gait=%s speed=%.0f tip0=%.0f"),TrialNames[Run->Trial],Run->Band,Run->Sub,Time,*P.ToString(),Chuck->GetGaitName(),Chuck->GetVelocity().Size2D(),Traps&&Run->Band<Traps->Spikes.Num()?Traps->SpikeTip(Run->Band,Now):-1.f);
            Run->TraceSub=Run->Sub;Run->TraceBand=Run->Band;
        }
        const int32 NewDeaths=Chuck->GetFallDeaths()-Run->Deaths;
        bool Done=false;float Limit=60;
        switch(Trials[Run->Trial])
        {
        case ET::Descent:
        {
            // In through the sanctuary's doorway, down the hidden stair, round the well and into the Great Hall.
            const FVector2D Way[]={{77960,0},{78150,0},{FlightAFoot+80,0},{FlightAFoot+100,240},{FlightBFoot-100,240},{FlightBFoot-100,TW},{SC,TW},{SC,TC},{SW,TC},{SW,TW},{SC,TW},{SC,TC},{SW,TC},{SW,AxisT},{HallS1-150,AxisT},{HallS0+300,AxisT}};
            Limit=240;
            if(Run->Leg<UE_ARRAY_COUNT(Way))
            {
                if(Steer(Way[Run->Leg].X,Way[Run->Leg].Y)){UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_WALK leg=%d at=%s seconds=%.1f ground=%d"),Run->Leg,*P.ToString(),Time,Ground);++Run->Leg;}
            }
            else{Stop();Done=true;Run->bPass=Ground&&NewDeaths==0&&FMath::Abs(P.Z-34.65f-Floor)<15;}
            if(NewDeaths)Done=true;
            break;
        }
        case ET::Spikes:
        {
            // Read each band: wait short of it until it has just drawn back, then run through.
            Limit=90;
            const int32 Bands=UE_ARRAY_COUNT(SpikeS);
            if(!Traps){Done=true;break;}
            if(Run->Band<Bands)
            {
                const float Wait=SpikeS[Run->Band]+SpikeHalfDepth+70,Exit=SpikeS[Run->Band]-SpikeHalfDepth-45;
                if(Run->Sub==0){Chuck->SetRunHeld(false);if(Steer(Wait,AxisT)){Stop();Run->Sub=1;}}
                else if(Run->Sub==1)
                {
                    // Go as the band draws back: clear for the next 1.8 s (of its 2.05).
                    bool Safe=true;for(float A=0;A<1.8f;A+=.05f)Safe&=!Traps->SpikeDanger(Run->Band,Now+A);
                    if(Safe){Run->Sub=2;Chuck->SetRunHeld(true);Run->Mark=Traps->SpikeHits;}
                }
                else if(Traps->SpikeHits>Run->Mark){Run->Sub=0;}   // caught: back to the wait, read it again
                else if(Steer(Exit,AxisT)){Run->Sub=0;++Run->Band;}
            }
            else if(Run->Sub<3)
            {
                // Then stand in the last band on purpose: it must hurt, once, and throw him out.
                Run->Timed=Traps->SpikeHits-Run->Hits;Run->Hits=Traps->SpikeHits;Run->Sub=3;Chuck->SetRunHeld(false);Run->Mark=-1;
            }
            else if(Run->Sub==3){if(Steer(SpikeS[Bands-1],AxisT)){Stop();Run->Sub=4;Run->LastJump=Now;}}
            else
            {
                const bool Hit=Traps->SpikeHits>Run->Hits;
                if(Hit||Now-Run->LastJump>4)
                {
                    Done=true;
                    Run->bPass=Hit&&Traps->SpikeHits==Run->Hits+1&&Run->Timed==0&&Chuck->GetSanity()==AChuckCharacter::MaxSanity-1;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_SPIKES bands=%d timed_hits=%d deliberate_hit=%d sanity=%d"),Run->Band,Run->Timed,Hit,Chuck->GetSanity());
                }
            }
            if(NewDeaths)Done=true;
            break;
        }
        case ET::SlabRun:
        {
            // Run straight across the cracked slabs: they fall behind him, he reaches the far side; then they come back.
            Limit=40;
            if(!Traps){Done=true;break;}
            const float SM=(DS0+DS1)*.5f;
            if(Run->Sub==0){Chuck->SetRunHeld(true);Run->Sub=1;}
            else if(Run->Sub==1){if(Steer(SM,PitG0-90)){Stop();Run->Sub=2;Run->Mark=Now;}}
            else if(Run->Sub==2){Chuck->SetRunHeld(false);Run->Sub=3;}
            else
            {
                const int32 Fell=Traps->SlabsFallen-Run->Fallen,Back=Traps->SlabsRestored-Run->Restored;
                if((Fell>=3&&Back>=Fell)||Time>Limit-1)
                {
                    Done=true;Run->bPass=Fell>=3&&Back>=Fell&&NewDeaths==0&&Ground;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_SLABS crossed=1 fell=%d restored=%d seconds=%.1f"),Fell,Back,Time);
                }
            }
            if(NewDeaths)Done=true;
            break;
        }
        case ET::SlabFall:
        {
            // Walk onto the first slab and stop: it gives way, he falls into the rift (an Astral
            // death) and comes back at the pier hall's door.
            Limit=25;
            // (On an end slab he can catch the lip as it drops and hang there; the middle one is out of reach.)
            if(Run->Sub==0){Chuck->SetRunHeld(true);Run->Sub=1;}
            else if(Run->Sub==1){if(Steer((DS0+DS1)*.5f,(PitG0+PitG1)*.5f+25)){Stop();Chuck->SetRunHeld(false);Run->Sub=2;}}
            else if(Run->Sub==2){Stop();if(NewDeaths>0){Run->Sub=3;Run->Mark=Now;}}
            else if(Now-Run->Mark>1)
            {
                const bool Home=FVector::Dist(Chuck->GetActorLocation(),ChultTempleInteriorSpawn(ETempleInteriorSpawn::Darts))<40&&Ground&&!Chuck->IsAstral();
                if(Home||Now-Run->Mark>10)
                {
                    Done=true;Run->bPass=Home&&NewDeaths==1;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_SLAB_FALL deaths=%d home=%d seconds=%.1f"),NewDeaths,Home,Time);
                }
            }
            break;
        }
        case ET::Rings:
        {
            // Jump to the first ring, swing ring to ring across the rift, step down beyond it.
            Limit=25;
            auto Press=[&](){Chuck->JumpPressed();Run->LastJump=Now;++Run->Jumps;};
            const int32 Grip=Chuck->GetSwingGrip(),Last=Built.FirstGrip+int32(UE_ARRAY_COUNT(RingS))-1;Stop();
            if(Run->Sub==0){if((Run->Jumps==0&&Time>.6f&&Ground)||(Run->Jumps>0&&Run->Jumps<4&&Ground&&Now-Run->LastJump>1.f))Press();if(Chuck->IsSwinging())Run->Sub=1;}
            else if(Run->Sub==1)
            {
                if(Chuck->IsSwinging()&&Grip==Last&&Now-Run->LastJump>.5f){Press();Run->Sub=2;}
                else if(Chuck->IsSwinging()&&Grip!=Last&&Now-Run->LastJump>.45f)Press();
            }
            else if(Ground&&Now-Run->LastJump>.6f)
            {
                Done=true;Run->bPass=P.X<RiftS0-5&&NewDeaths==0&&Chuck->GetSwingLeaps()-Run->Leaps==int32(UE_ARRAY_COUNT(RingS))-1;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_RINGS leaps=%d at=%s seconds=%.1f"),Chuck->GetSwingLeaps()-Run->Leaps,*P.ToString(),Time);
            }
            if(NewDeaths){Done=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_RINGS fell grip=%d sub=%d"),Grip,Run->Sub);}
            break;
        }
        case ET::Darts:
        {
            // Read each serpent mouth: wait short of its line until the next volley is far off, then run across.
            Limit=120;
            if(!Traps){Done=true;break;}
            const float SM=(DS0+DS1)*.5f;
            if(Run->Band<Launchers)
            {
                const float Line=LauncherT(Run->Band);
                // The first leg crosses the slab pit: run it.
                if(Run->Sub==0){Chuck->SetRunHeld(Run->Band==0);if(Steer(SM,Line+80)){Stop();Run->Sub=1;}}
                else if(Run->Sub==1){if(!Traps->DartLineDanger(Run->Band,Now,1.f)){Run->Sub=2;Chuck->SetRunHeld(true);}}
                else if(Steer(SM,Line-80)){Run->Sub=0;++Run->Band;}
            }
            else if(Run->Sub<3){Run->Timed=Traps->DartHits-Run->DartHits;Run->DartHits=Traps->DartHits;Chuck->SetRunHeld(false);Run->Sub=3;}
            else if(Run->Sub==3){if(Steer(SM,LauncherT(Launchers-1))){Stop();Run->Sub=4;Run->LastJump=Now;}}
            else if(Run->Sub==4)
            {
                // Onto the line itself (the arrival tolerance can leave him beside it).
                const float Off=P.Y-LauncherT(Launchers-1);
                if(FMath::Abs(Off)>5){const FVector D=(Off>0?-Across:Across);Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,.3f,true);}else Stop();
                const bool Hit=Traps->DartHits>Run->DartHits;
                if(Hit||Now-Run->LastJump>6){Run->Sub=5;Run->LastJump=Now;Run->bHit=Hit;}
            }
            else if(Run->Sub==5)
            {
                // On through the arch into the snake chamber (it was choked until October 21).
                Chuck->SetRunHeld(true);Steer(SM,DT0-300);
                if(P.Y<DT0-150||Now-Run->LastJump>10)
                {
                    Done=true;Stop();
                    Run->bPass=Run->Timed==0&&Run->bHit&&P.Y<DT0-150&&NewDeaths==0;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_DARTS lines=%d timed_hits=%d deliberate_hit=%d fired=%d furthest_t=%.0f sanity=%d"),Run->Band,Run->Timed,Run->bHit,Traps->DartsFired,P.Y,Chuck->GetSanity());
                }
            }
            break;
        }
        case ET::Hearing:
        {
            // Over the corridor no trap is heard; down in it the spikes are (2026-10-20: their
            // thrusts carried up through the ground to the avenue).
            Limit=20;
            if(!Traps){Done=true;break;}
            if(Run->Sub==0&&Time>7){Run->Timed=Traps->SoundsPlayed-Run->Hits;Spawn(ETempleInteriorSpawn::Spikes);Run->Hits=Traps->SoundsPlayed;Run->Sub=1;}
            else if(Run->Sub==1&&Time>14)
            {
                const int32 Below=Traps->SoundsPlayed-Run->Hits;Done=true;Run->bPass=Run->Timed==0&&Below>0;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_HEARING above=%d below=%d"),Run->Timed,Below);
            }
            break;
        }
        case ET::WellFall:
        {
            // Run off the gallery's edge into the pit: an Astral death, back on the gallery.
            Limit=20;
            if(Run->Sub==0){Chuck->SetRunHeld(true);Steer(78000,AxisT);if(NewDeaths>0){Stop();Run->Sub=1;Run->Mark=Now;}}
            else if(Now-Run->Mark>1)
            {
                const bool Home=FVector::Dist(Chuck->GetActorLocation(),ChultTempleInteriorSpawn(ETempleInteriorSpawn::Well))<40&&Ground&&!Chuck->IsAstral();
                if(Home||Now-Run->Mark>10){Done=true;Run->bPass=Home&&NewDeaths==1;UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_WELL_FALL deaths=%d home=%d"),NewDeaths,Home);}
            }
            break;
        }
        }
        if(!Done&&Time<Limit)return;
        Stop();Chuck->SetRunHeld(false);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_TEMPLE_INTERIOR_TRIAL name=%s pass=%d seconds=%.1f leg=%d sub=%d band=%d deaths=%d sanity=%d gait=%s at=%s"),
            TrialNames[Run->Trial],Run->bPass,Time,Run->Leg,Run->Sub,Run->Band,NewDeaths,Chuck->GetSanity(),Chuck->GetGaitName(),*P.ToString());
        Run->Results.Add(Run->bPass?1:0);Run->bStarted=false;Run->Next=Now+1.5;
    },.016f,true);
}
