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

namespace
{
    struct FRiverHome { FVector Feet; bool NearRoad; TWeakObjectPtr<ADockNPC> Zombie; };
    TArray<FRiverHome> Homes;
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
                Homes.Add({Feet,Near,Z});
                UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE station=%d road=%d s=%.0f road_distance=%.0f feet=%s"),Station,Near,F.X,D,*(Feet-G).ToString());
            }
            break;
        }
    }
    int32 Near=0;for(const auto& H:Homes)Near+=H.NearRoad;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIES total=%d road=%d flanks=%d"),Homes.Num(),Near,Homes.Num()-Near);
}

void StartChultRiverZombieReview(UWorld* World)
{
    auto* Chuck=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World,0));
    if(!Chuck||ChultFarRoad().IsEmpty()) { FPlatformMisc::RequestExitWithStatus(false,1);return; }
    struct FRun { int32 Failures=0,Trial=0;float Start=-1,Progress=0;bool Shot=false;FTimerHandle Timer; };
    auto Run=MakeShared<FRun>();
    int32 Near=0,Flanks=0,Clear=0,Dry=0,Supported=0;
    const FVector G=AChultLanding::FloorOrigin();
    for(const auto& H:Homes)
    {
        Near+=H.NearRoad;Flanks+=!H.NearRoad;
        const FVector P=H.Feet-G,F=Frame(P);FVector Feet;
        Clear+=ChultPathDistance(P)>(H.NearRoad?700:ADockNPC::ZombieLeash+180);
        Dry+=F.X>18500&&F.X<32800&&F.Y<ChultRiverCenter(F.X)-ChultRiverWidth(F.X)-200;
        Supported+=OpenFloor(World,P,Feet)&&FVector::Dist(Feet,H.Feet)<2;
    }
    Run->Failures+=(Near!=8)+(Flanks!=24)+(Clear!=32)+(Dry!=32)+(Supported!=32);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_LAYOUT failures=%d road=%d flanks=%d clear=%d dry=%d supported=%d"),Run->Failures,Near,Flanks,Clear,Dry,Supported);
    Chuck->DisableInput(Cast<APlayerController>(Chuck->GetController()));Chuck->SetLookLocked(true);
    // Walk three actual encounter stretches with the complete population live.
    // No sanity refill during a trial: at most one ordinary hit is acceptable.
    World->GetTimerManager().SetTimer(Run->Timer,[World,Chuck,Run,G]()
    {
        const float S=Run->Trial==0?18200:Run->Trial==1?23900:29600;
        auto* Move=Chuck->GetCharacterMovement();const float Now=World->GetTimeSeconds();
        if(Run->Start<0)
        {
            const FVector P=RoadAt(S);Move->StopMovementImmediately();
            Chuck->SetActorLocation(G+P+FVector(0,0,40),false,nullptr,ETeleportType::TeleportPhysics);
            Move->SetMovementMode(MOVE_Walking);Chuck->SetSanity(AChuckCharacter::MaxSanity);
            Chuck->SetActorRotation(FVector(.8,.6,0).Rotation());Chuck->Recenter();Chuck->SetOrbitPitch(-12);
            Run->Start=Now;Run->Progress=0;Run->Shot=false;
        }
        const float Elapsed=Now-Run->Start;
        const FVector Goal=G+RoadAt(S+1500);
        const float Distance=FVector::Dist2D(Chuck->GetActorLocation(),Goal);
        if(Elapsed>3&&!Run->Shot)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/FString::Printf(TEXT("Chult/RiverZombies%d.png"),Run->Trial),true,false);
            Run->Shot=true;
        }
        if(Distance>35&&Elapsed<30&&!Chuck->IsAstral())
        {
            const FVector Target=G+RoadAt(S+FMath::Min(1500.f,Run->Progress+80));
            if(FVector::Dist2D(Chuck->GetActorLocation(),Target)<40)Run->Progress+=40;
            const FVector D=(Target-Chuck->GetActorLocation()).GetSafeNormal2D();
            Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;
        }
        Chuck->SetTestStick(FVector2D::ZeroVector);Move->StopMovementImmediately();
        int32 FlanksActive=0;
        for(const auto& H:Homes)if(!H.NearRoad&&H.Zombie.IsValid())FlanksActive+=FCString::Strcmp(H.Zombie->GetZombieStateName(),TEXT("Idle"))!=0;
        const bool Pass=Distance<=35&&Move->IsMovingOnGround()&&Chuck->GetSanity()>=3&&!Chuck->IsAstral()&&FlanksActive==0;
        Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_WALK trial=%d pass=%d distance=%.1f sanity=%d flank_active=%d seconds=%.1f"),Run->Trial,Pass,Distance,Chuck->GetSanity(),FlanksActive,Elapsed);
        ++Run->Trial;Run->Start=-1;
        if(Run->Trial==3)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RIVER_ZOMBIE_TEST failures=%d trials=3"),Run->Failures);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
            // Leave this delegate alive until process exit (no freed captures).
            World->GetTimerManager().PauseTimer(Run->Timer);
        }
    },.016f,true);
}
