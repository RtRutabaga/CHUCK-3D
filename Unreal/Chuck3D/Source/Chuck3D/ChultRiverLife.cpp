#include "ChultLife.h"
#include "ChultLanding.h"
#include "ChultCanyon.h"
#include "ChultRiver.h"
#include "ChultPath.h"
#include "ChuckCharacter.h"
#include "DockNPC.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Components/CapsuleComponent.h"
#include "Components/LocalFogVolumeComponent.h"

namespace
{
    struct FRiverHome { FVector Feet; bool NearRoad; TWeakObjectPtr<ADockNPC> Zombie; bool Path=false,Opposite=false; };
    TArray<FRiverHome> Homes;
    void ProtectRiverCamera(ADockNPC* Zombie)
    {
        // Only the river encounters: keep a pursuing torso out of the orbit.
        // Separate camera-only geometry leaves pawn/traversal queries unchanged.
        auto* Guard=NewObject<UCapsuleComponent>(Zombie,TEXT("RiverZombieCamera"));
        Guard->InitCapsuleSize(43,98);
        Guard->SetupAttachment(Zombie->GetRootComponent());
        Guard->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Guard->SetCollisionObjectType(ECC_WorldDynamic);
        Guard->SetCollisionResponseToAllChannels(ECR_Ignore);
        Guard->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);
        Guard->ComponentTags.Add(TEXT("RiverZombieCamera"));
        Guard->SetCanEverAffectNavigation(false);Guard->RegisterComponent();
    }
    FVector Frame(FVector P)
    {
        P-=ChultCanyonFrame(0,0);return FVector(P.X*.8+P.Y*.6,-P.X*.6+P.Y*.8,P.Z);
    }
    FVector RoadAt(float S)
    {
        const auto& Road=ChultFarRoad();
        for(int32 I=1;I<Road.Num();++I)
        {
            const float A=Frame(Road[I-1]).X,B=Frame(Road[I]).X;
            if(A<=S&&B>=S)
            {
                FVector P=FMath::Lerp(Road[I-1],Road[I],(S-A)/FMath::Max(1.f,B-A));
                P.Z=ChultCanyonGroundZ(P);return P;
            }
        }
        return FVector::ZeroVector;
    }
    bool OpenFloor(UWorld* World,FVector P,FVector& Feet)
    {
        const FVector G=AChultLanding::FloorOrigin();const float Z=ChultCanyonGroundZ(P);
        FCollisionQueryParams Q(SCENE_QUERY_STAT(RiverZombieFloor),false);
        const FCollisionObjectQueryParams Static(ECC_WorldStatic);FHitResult Hit;
        if(!World->LineTraceSingleByObjectType(Hit,G+FVector(P.X,P.Y,Z+200),G+FVector(P.X,P.Y,Z-50),Static,Q)
            ||Hit.ImpactNormal.Z<.85||FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)>12)return false;
        Feet=Hit.ImpactPoint;
        return !World->OverlapAnyTestByObjectType(Feet+FVector(0,0,100),FQuat::Identity,Static,FCollisionShape::MakeCapsule(35,60),Q);
    }
}

void BuildChultRiverEncounterMist(AActor* Owner)
{
    for(auto* C:Owner->GetComponents())if(C->ComponentHasTag(TEXT("RiverEncounterMist")))return;
    const FVector G=AChultLanding::FloorOrigin();
    for(int32 I=0;I<8;++I)for(int32 Bank=0;Bank<2;++Bank)
    {
        const float S=18900+1900*I;
        FVector P=Bank ? ChultCanyonFrame(S,FMath::Min(6100.f,ChultRiverCenter(S)+ChultRiverWidth(S)+650)) : RoadAt(S)+FVector(.6,-.8,0)*350;
        P.Z=ChultCanyonGroundZ(P)-430;
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner,*FString::Printf(TEXT("RiverEncounterMist%d_%d"),I,Bank));
        Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+P);
        Fog->SetWorldScale3D(FVector(1.6f));Fog->ComponentTags.Add(TEXT("RiverEncounterMist"));
        Fog->RadialFogExtinction=.85f;Fog->HeightFogExtinction=Bank?.3f:.2f;
        Fog->HeightFogFalloff=2600;Fog->HeightFogOffset=.61f;
        Fog->FogAlbedo=FLinearColor(.64,.74,.75);Fog->FogPhaseG=.1f;
        Fog->RegisterComponent();Fog->SetFogEmissive(FLinearColor(.018,.027,.029));
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ENCOUNTER_MIST pools=16 banks=2 collision=0"));
}

void SpawnChultRiverZombies(UWorld* World)
{
    if(!World)return;
    for(TActorIterator<ADockNPC> It(World);It;++It)if(It->ActorHasTag(TEXT("ChultRiverZombie")))return;
    Homes.Reset();FRandomStream R(20261016);
    const FVector G=AChultLanding::FloorOrigin(),Inland(.6,-.8,0);
    // Eight separated singles by the road, three deeper jungle sentries at
    // each station. No new enemies in the bridge arrival or the giants' grove.
    for(int32 Station=0;Station<8;++Station)for(int32 Band=0;Band<4;++Band)
    {
        const bool Near=Band==0;
        for(int32 Try=0;Try<160;++Try)
        {
            const float S=19000+1900*Station+R.FRandRange(-160,160);
            const FVector Road=RoadAt(S);
            const float Offset=Near ? 850.f : 1450.f+(Band-1)*950.f+(Try/20)*150.f;
            const FVector P=Road+Inland*(Offset+R.FRandRange(-90,90))+FVector(.8,.6,0)*R.FRandRange(-160,160);
            const FVector F=Frame(P);const float D=ChultPathDistance(P);
            if(!InChultRiverExtension(P)||F.X<18500||F.X>32800||F.Y<-6100||F.Y>ChultRiverCenter(F.X)-ChultRiverWidth(F.X)-210
                ||D<(Near?700.f:1300.f)||D>(Near?950.f:3700.f))continue;
            bool Spaced=true;
            for(const auto& H:Homes)if(FVector::Dist2D(H.Feet-G,P)<(Near&&H.NearRoad?1450:650))Spaced=false;
            if(!Spaced)continue;
            FVector Feet;if(!OpenFloor(World,P,Feet))continue;
            const float Yaw=Near ? (RoadAt(S+100)-RoadAt(S-100)).Rotation().Yaw : (Road-P).Rotation().Yaw+R.FRandRange(-25,25);
            if(auto* Z=ADockNPC::SpawnZombie(World,Feet,Yaw))
            {
                Z->Tags.Add(TEXT("ChultRiverZombie"));
                Z->Tags.Add(Near?TEXT("ChultRiverRoadZombie"):TEXT("ChultRiverFlankZombie"));
                ProtectRiverCamera(Z);
                Homes.Add({Feet,Near,Z});
                UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE station=%d road=%d s=%.0f road_distance=%.0f feet=%s"),Station,Near,F.X,D,*(Feet-G).ToString());
            }
            break;
        }
    }
    auto Add=[&](FVector P,bool Opposite)
    {
        const FVector F=Frame(P);const float D=ChultPathDistance(P);
        if(!InChultRiverExtension(P)||F.X<18200||F.X>32900||F.Y>6400||F.Y<-6100)return false;
        const float Bank=ChultRiverCenter(F.X),Width=ChultRiverWidth(F.X);
        if(Opposite ? F.Y<Bank+Width+210||D<1300 : F.Y>Bank-Width-210||D<80||D>270)return false;
        for(const auto& H:Homes)if(FVector::Dist2D(H.Feet-G,P)<(Opposite?600:440))return false;
        FVector Feet;if(!OpenFloor(World,P,Feet))return false;
        const FVector Facing=Opposite ? -FVector(.8,.6,0) : RoadAt(F.X-500)-P;
        if(auto* Z=ADockNPC::SpawnZombie(World,Feet,Facing.Rotation().Yaw))
        {
            Z->Tags.Add(TEXT("ChultRiverZombie"));
            Z->Tags.Add(Opposite?TEXT("ChultRiverOppositeZombie"):TEXT("ChultRiverPathAttacker"));
            ProtectRiverCamera(Z);
            Homes.Add({Feet,false,Z,!Opposite,Opposite});
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ADDED opposite=%d s=%.0f road_distance=%.0f feet=%s"),Opposite,F.X,D,*(Feet-G).ToString());
            return true;
        }
        return false;
    };
    // A staggered attacker every nine metres, facing the incoming player.
    // Alternate shoulders; leave the paving physically passable for a dodge.
    for(int32 I=0;I<16;++I)for(int32 Try=0;Try<240;++Try)
    {
        const float S=18800+900*I+R.FRandRange(-100,100);
        const FVector Road=RoadAt(S),Along=(RoadAt(S+80)-RoadAt(S-80)).GetSafeNormal2D();
        const FVector Side(-Along.Y,Along.X,0);
        if(Add(Road+Side*((I%2?1.f:-1.f)*R.FRandRange(110,240)),false))break;
    }
    // The opposite bank is real terrain, not a decorative crowd in the water.
    // Three sentries per station, spread along and back from its shoreline.
    for(int32 Station=0;Station<8;++Station)for(int32 I=0;I<3;++I)for(int32 Try=0;Try<600;++Try)
    {
        const float S=19000+1900*Station+R.FRandRange(-500,500);
        const float Shore=ChultRiverCenter(S)+ChultRiverWidth(S);
        const float Outer=FMath::Min(2300.f,6380-Shore);
        if(Outer<300)continue;
        if(Add(ChultCanyonFrame(S,Shore+R.FRandRange(280,Outer)),true))break;
    }
    int32 Milling=0;
    for(int32 I=0;I<Homes.Num();++I)
    {
        const auto& H=Homes[I];if(!H.NearRoad&&!H.Path)continue;
        const FVector P=H.Feet-G;const float S=Frame(P).X;
        const FVector Along=(RoadAt(S+100)-RoadAt(S-100)).GetSafeNormal2D();
        for(float Radius:{90.f,65.f,40.f,25.f})
        {
          for(int32 Orientation=0;Orientation<9;++Orientation)
          {
            const FVector D=Orientation==0?Along:Along.RotateAngleAxis((Orientation-1)*45.f,FVector::UpVector),Side(-D.Y,D.X,0);
            TArray<FVector> Points;bool Clear=true;
            for(FVector Offset:{FVector::ZeroVector,D*Radius+Side*(Radius*.25f),
                (Orientation==0?-D:D*.5f)*Radius+Side*(Radius*(Orientation==0?.25f:-.25f))})
            {
                const FVector At=P+Offset,F=Frame(At);FVector Feet;
                if(F.Y>ChultRiverCenter(F.X)-ChultRiverWidth(F.X)-180||!OpenFloor(World,At,Feet)){Clear=false;break;}
                Points.Add(Feet);
            }
            FCollisionQueryParams Q(SCENE_QUERY_STAT(RiverZombieMill),false);
            for(int32 J=0;J<Points.Num()&&Clear;++J)
            {
                const FVector A=Points[J],B=Points[(J+1)%Points.Num()];FHitResult Hit;
                Clear=!World->SweepSingleByObjectType(Hit,A+FVector(0,0,100),B+FVector(0,0,100),FQuat::Identity,
                    FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(35,60),Q);
                for(int32 K=1;K<5&&Clear;++K){FVector Feet;Clear=OpenFloor(World,FMath::Lerp(A,B,K/5.f)-G,Feet);}
            }
            if(Clear&&H.Zombie.IsValid())
            {
                H.Zombie->SetZombieMilling(Points,20261018+I*71);++Milling;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_MILL_ROUTE home=%d radius=%.0f orientation=%d"),I,Radius,Orientation);break;
            }
          }
          if(H.Zombie.IsValid()&&H.Zombie->HasZombieMilling())break;
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_MILLING configured=%d route_radius_cm=100 travel_bound_cm=115 speed_fraction=0.35"),Milling);
    int32 Near=0,Flanks=0,Path=0,Opposite=0;
    for(const auto& H:Homes){Near+=H.NearRoad;Path+=H.Path;Opposite+=H.Opposite;Flanks+=!H.NearRoad&&!H.Path&&!H.Opposite;}
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIES total=%d road=%d flanks=%d path_attackers=%d opposite=%d"),Homes.Num(),Near,Flanks,Path,Opposite);
}

void StartChultRiverZombieReview(UWorld* World)
{
    auto* Chuck=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World,0));
    if(!Chuck||ChultFarRoad().IsEmpty()) { FPlatformMisc::RequestExitWithStatus(false,1);return; }
    struct FRun
    {
        int32 Failures=0,Trial=-1,RollsBefore=0,LungesBefore=0,Attempt=0;
        float Start=-1,LastDodge=-10,WindupAt=0;bool Shot=false;
        TWeakObjectPtr<ADockNPC> Windup,Target;FTimerHandle Timer;
    };
    auto Run=MakeShared<FRun>();
    int32 FogPools=0;
    for(TActorIterator<AChultLanding> It(World);It;++It)for(auto* C:It->GetComponents())
        if(C->ComponentHasTag(TEXT("RiverEncounterMist")))++FogPools;
    Run->Failures+=FogPools!=16;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_MIST_CHECK pools=%d pass=%d"),FogPools,FogPools==16);
    int32 Near=0,Flanks=0,Path=0,Opposite=0,Clear=0,Dry=0,Supported=0,CameraGuards=0;
    const FVector G=AChultLanding::FloorOrigin();
    for(const auto& H:Homes)
    {
        Near+=H.NearRoad;Path+=H.Path;Opposite+=H.Opposite;Flanks+=!H.NearRoad&&!H.Path&&!H.Opposite;
        const FVector P=H.Feet-G,F=Frame(P);FVector Feet;
        const float D=ChultPathDistance(P);
        Clear+=H.Path ? D>=80&&D<=270 : D>(H.NearRoad?700:ADockNPC::ZombieLeash+180);
        Dry+=F.X>18200&&F.X<32900&&(H.Opposite ? F.Y>ChultRiverCenter(F.X)+ChultRiverWidth(F.X)+200 : F.Y<ChultRiverCenter(F.X)-ChultRiverWidth(F.X)-200);
        Supported+=OpenFloor(World,P,Feet)&&FVector::Dist(Feet,H.Feet)<2;
        if(H.Zombie.IsValid())for(auto* C:H.Zombie->GetComponents())
            if(auto* Guard=Cast<UCapsuleComponent>(C);Guard&&Guard->ComponentHasTag(TEXT("RiverZombieCamera")))
                CameraGuards+=Guard->GetCollisionResponseToChannel(ECC_Camera)==ECR_Block&&Guard->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore
                    &&Guard->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Ignore;
    }
    Run->Failures+=(Near!=8)+(Flanks!=24)+(Path!=16)+(Opposite!=24)+(Clear!=72)+(Dry!=72)+(Supported!=72)+(CameraGuards!=72);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_LAYOUT failures=%d road=%d flanks=%d path_attackers=%d opposite=%d clear=%d dry=%d supported=%d camera_guards=%d"),Run->Failures,Near,Flanks,Path,Opposite,Clear,Dry,Supported,CameraGuards);
    Chuck->DisableInput(Cast<APlayerController>(Chuck->GetController()));Chuck->SetLookLocked(true);
    // Three live dodge routes, an unguarded control and an opposite-bank approach.
    // No sanity refill during a trial; no combat, enemy freezing or stat changes.
    World->GetTimerManager().SetTimer(Run->Timer,[World,Chuck,Run,G]()
    {
        const float S=Run->Trial==0?18200:Run->Trial==1?23900:29600;
        auto* Move=Chuck->GetCharacterMovement();const float Now=World->GetTimeSeconds();
        if(Run->Trial==-1)
        {
            if(Run->Start<0)Run->Start=Now;
            if(Now-Run->Start<12)return;
            int32 Configured=0,Moved=0,Bounded=0,Stationary=0;
            for(const auto& H:Homes)if(H.Zombie.IsValid())
            {
                auto* Z=H.Zombie.Get();
                if(H.Path||H.NearRoad)
                {
                    Configured+=Z->HasZombieMilling();Moved+=Z->GetZombieMillingTravel()>10;
                    Bounded+=FVector::Dist2D(Z->GetActorLocation(),H.Feet)<125;
                }
                else Stationary+=FVector::Dist2D(Z->GetActorLocation(),H.Feet)<2;
            }
            const bool Pass=Configured==24&&Moved==24&&Bounded==24&&Stationary==48;
            Run->Failures+=!Pass;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_MILL_TEST pass=%d configured=%d moved=%d bounded=%d stationary=%d"),Pass,Configured,Moved,Bounded,Stationary);
            Run->Trial=0;Run->Start=-1;return;
        }
        auto Lunges=[](){int32 N=0;for(const auto& H:Homes)if(H.Path&&H.Zombie.IsValid())N+=H.Zombie->GetLunges();return N;};
        if(Run->Trial>=3)
        {
            const bool OppositeTrial=Run->Trial==4;
            if(Run->Start<0)
            {
                Run->Target.Reset();
                for(const auto& H:Homes)if((OppositeTrial?H.Opposite:H.Path)&&H.Zombie.IsValid())
                {
                    FVector Feet;const FVector P=H.Feet-G+FVector(.8,.6,0)*(OppositeTrial?280.f:-90.f);
                    if(!OpenFloor(World,P,Feet)||FMath::Abs(Feet.Z-H.Feet.Z)>40)continue;
                    if(OppositeTrial&&Frame(P).Y<ChultRiverCenter(Frame(P).X)+ChultRiverWidth(Frame(P).X)+150)continue;
                    Chuck->ResetAtLocation(Feet+FVector(0,0,40));Chuck->SetSanity(5);
                    Chuck->SetActorRotation((H.Feet-Feet).GetSafeNormal2D().Rotation());Chuck->Recenter();
                    Chuck->SetTestStick(FVector2D::ZeroVector);Run->Target=H.Zombie;
                    Run->LungesBefore=H.Zombie->GetLunges();break;
                }
                Run->Start=Now;
            }
            auto* Z=Run->Target.Get();
            const bool Responded=Z&&(OppositeTrial ? FCString::Strcmp(Z->GetZombieStateName(),TEXT("Idle"))!=0 : Chuck->GetSanity()<5&&Z->GetLunges()>Run->LungesBefore);
            if(!Responded&&Now-Run->Start<6)return;
            Run->Failures+=!Responded;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_CONTROL opposite=%d pass=%d sanity=%d state=%s"),OppositeTrial,Responded,Chuck->GetSanity(),Z?Z->GetZombieStateName():TEXT("missing"));
            ++Run->Trial;Run->Start=-1;
            if(Run->Trial==5)
            {
                UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_TEST failures=%d trials=5"),Run->Failures);
                FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);World->GetTimerManager().PauseTimer(Run->Timer);
            }
            return;
        }
        if(Run->Start<0)
        {
            const FVector P=RoadAt(S);Move->StopMovementImmediately();
            Chuck->ResetAtLocation(G+P+FVector(0,0,40));
            Move->SetMovementMode(MOVE_Walking);Chuck->SetSanity(AChuckCharacter::MaxSanity);
            Chuck->SetActorRotation(FVector(.8,.6,0).Rotation());Chuck->Recenter();Chuck->SetOrbitPitch(-12);
            Run->Start=Now;Run->Shot=false;Run->RollsBefore=Chuck->GetSfxCount(AChuckCharacter::ESfx::Roll);Run->LungesBefore=Lunges();Run->Windup.Reset();
        }
        const float Elapsed=Now-Run->Start;
        const FVector Goal=G+RoadAt(S+1800);
        const float Distance=FVector::Dist2D(Chuck->GetActorLocation(),Goal);
        if(Elapsed>12&&!Run->Shot)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/FString::Printf(TEXT("Chult/RiverZombies%d.png"),Run->Trial),true,false);
            Run->Shot=true;
        }
        if(Distance>35&&Elapsed<40&&!Chuck->IsAstral())
        {
            const float At=Frame(Chuck->GetActorLocation()-G).X;
            const FVector Target=G+RoadAt(FMath::Clamp(At+100,S,S+1800));
            const FVector D=(Target-Chuck->GetActorLocation()).GetSafeNormal2D();
            ADockNPC* Threat=nullptr;
            for(const auto& H:Homes)if(H.Path&&H.Zombie.IsValid()&&FVector::Dist2D(H.Zombie->GetActorLocation(),Chuck->GetActorLocation())<230
                &&FCString::Strcmp(H.Zombie->GetZombieStateName(),TEXT("Windup"))==0){Threat=H.Zombie.Get();break;}
            if(Threat!=Run->Windup.Get()){Run->Windup=Threat;Run->WindupAt=Now;}
            // Roll .34 s into the .5 s windup: the bite comes .22 s into the lunge (.72 s), and a roll with the
            // stick held carries out into a stride at .56 s, so the bite lands mid-roll with ~.18 s either side.
            // (Rolling at .18 s ended the roll's immunity .02 s after the bite: a frame's hitch failed the trial,
            // 2026-10-24.)
            if(Threat&&Now-Run->WindupAt>.34f&&Now-Run->LastDodge>.85f&&FCString::Strcmp(Chuck->GetGaitName(),TEXT("Roll"))!=0)
            {
                Chuck->SetActorRotation(D.Rotation());Chuck->Recenter();Chuck->DodgeToward(FVector2D(0,1));Run->LastDodge=Now;
            }
            Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;
        }
        Chuck->SetTestStick(FVector2D::ZeroVector);Move->StopMovementImmediately();
        int32 FlanksActive=0;
        for(const auto& H:Homes)if(!H.NearRoad&&!H.Path&&!H.Opposite&&H.Zombie.IsValid())FlanksActive+=FCString::Strcmp(H.Zombie->GetZombieStateName(),TEXT("Idle"))!=0;
        const int32 Rolls=Chuck->GetSfxCount(AChuckCharacter::ESfx::Roll)-Run->RollsBefore,Attacks=Lunges()-Run->LungesBefore;
        const bool Pass=Distance<=35&&Move->IsMovingOnGround()&&Chuck->GetSanity()>=3&&!Chuck->IsAstral()&&FlanksActive==0&&Rolls>=1&&Attacks>=1;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_DODGE trial=%d pass=%d distance=%.1f sanity=%d flank_active=%d rolls=%d attacks=%d seconds=%.1f attempt=%d"),Run->Trial,Pass,Distance,Chuck->GetSanity(),FlanksActive,Rolls,Attacks,Elapsed,Run->Attempt+1);
        // A bite costs two Sanity. Tests turn grabs off (ADockNPC::GetZombieGrabChance), so the
        // outcome depends only on frame timing; the route must pass at the same bar within three tries.
        if(!Pass&&++Run->Attempt<3){Run->Start=-1;return;}
        Run->Failures+=!Pass;Run->Attempt=0;
        ++Run->Trial;Run->Start=-1;
    },.016f,true);
}
