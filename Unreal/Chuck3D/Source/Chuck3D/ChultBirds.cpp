#include "ChultBirds.h"
#include "ChultBridge.h"
#include "ChultCanyon.h"
#include "ChultLanding.h"
#include "ChultTemple.h"
#include "ChultTempleInterior.h"
#include "ChultUpriver.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Chult's macaws (Claude, 2026-10-19; docs/CHULT-BIRDS.md). Every position here
// is local to Chult's FloorOrigin unless named World; the canyon frame is
// ChultCanyonFrame(S along, T across, Z).
namespace
{
    const TCHAR* MeshDir=TEXT("/Game/Art/ChultBirds");
    // Wing joints from Tools/build_chult_birds.py (Blender +Y left is Unreal -Y).
    const FVector Shoulder(3,4.5f,2.5f);
    constexpr float InnerSpan=21;
    constexpr float BridgeEnd=4560;   // the far lip along the bridge, near enough (posts at -240/4800)

    FVector Along(){static const FVector A=(ChultCanyonFrame(1000,0)-ChultCanyonFrame(0,0)).GetSafeNormal();return A;}
    FVector Across(){static const FVector A=(ChultCanyonFrame(0,1000)-ChultCanyonFrame(0,0)).GetSafeNormal();return A;}
    // Local -> canyon frame (S, T, Z).
    FVector ToFrame(const FVector& Local){const FVector P=Local-ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along()),FVector::DotProduct(P,Across()),Local.Z);}
    FVector At(float S,float T,float Z){return ChultCanyonFrame(S,T,Z);}

    // Automated runs keep the worlds their checks were written against.
    bool Suppressed()
    {
        const TCHAR* Cmd=FCommandLine::Get();
        if(FParse::Param(Cmd,TEXT("ChuckBirdReview")))return false;
        TArray<FString> Words;FString(Cmd).ParseIntoArrayWS(Words);
        for(const FString& Word:Words)
            if(Word.StartsWith(TEXT("-Chuck"))&&(Word.Contains(TEXT("Test"))||Word.Contains(TEXT("Capture"))||Word.Contains(TEXT("Review"))))return true;
        return false;
    }

    // A Catmull-Rom line through the points, as a polyline with lengths.
    void MakePath(const TArray<FVector>& P,TArray<FVector>& Out,TArray<float>& Len)
    {
        Out.Reset();Len.Reset();
        const int32 N=P.Num();
        auto Pt=[&](int32 I){return I<0?P[0]*2-P[1]:I>=N?P[N-1]*2-P[N-2]:P[I];};
        for(int32 I=0;I<N-1;++I)
            for(int32 K=0;K<24;++K)
            {
                const float U=K/24.f,U2=U*U,U3=U2*U;
                const FVector A=Pt(I-1),B=Pt(I),C=Pt(I+1),D=Pt(I+2);
                Out.Add(.5f*((2*B)+(-A+C)*U+(2*A-5*B+4*C-D)*U2+(-A+3*B-3*C+D)*U3));
            }
        Out.Add(P.Last());
        Len.Add(0);for(int32 I=1;I<Out.Num();++I)Len.Add(Len.Last()+FVector::Dist(Out[I-1],Out[I]));
    }
    FVector AlongPath(const TArray<FVector>& P,const TArray<float>& Len,float D)
    {
        D=FMath::Clamp(D,0.f,Len.Last());
        int32 Lo=0,Hi=Len.Num()-1;
        while(Hi-Lo>1){const int32 Mid=(Lo+Hi)/2;(Len[Mid]<=D?Lo:Hi)=Mid;}
        const float Seg=Len[Hi]-Len[Lo];
        return FMath::Lerp(P[Lo],P[Hi],Seg>0?(D-Len[Lo])/Seg:0.f);
    }
    float Smooth(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);}
}

AChultBirds::AChultBirds()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetActorEnableCollision(false);
}

AChultBirds* AChultBirds::Find(UWorld* World)
{
    for(TActorIterator<AChultBirds> It(World);It;++It)return *It;
    return nullptr;
}

void AChultBirds::AddBird(ERole Kind,bool bScarlet,float Scale)
{
    FBird B;B.Role=Kind;B.Scale=Scale;
    const FString Plumage=bScarlet?TEXT("Scarlet"):TEXT("BlueGold");
    auto Mesh=[&](const TCHAR* Part)
    {
        const FString Name=FString::Printf(TEXT("SM_%s_%s"),*Plumage,Part);
        UStaticMesh* Asset=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),MeshDir,*Name,*Name));
        MissingMeshes+=!Asset;return Asset;
    };
    auto Part=[&](USceneComponent* Parent,UStaticMesh* Asset,const FVector& Offset)
    {
        auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(Parent);C->SetStaticMesh(Asset);
        C->SetRelativeLocation(Offset);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);
        C->SetCanEverAffectNavigation(false);C->bReceivesDecals=false;C->SetCastShadow(true);C->SetMobility(EComponentMobility::Movable);
        C->RegisterComponent();return C;
    };
    B.Root=NewObject<USceneComponent>(this);B.Root->SetMobility(EComponentMobility::Movable);B.Root->SetupAttachment(RootComponent);
    B.Root->SetRelativeScale3D(FVector(Scale));B.Root->RegisterComponent();
    B.Body=Part(B.Root,Mesh(TEXT("Body")),FVector::ZeroVector);
    B.In[0]=Part(B.Body,Mesh(TEXT("WingInL")),FVector(Shoulder.X,-Shoulder.Y,Shoulder.Z));
    B.In[1]=Part(B.Body,Mesh(TEXT("WingInR")),FVector(Shoulder.X,Shoulder.Y,Shoulder.Z));
    B.Out[0]=Part(B.In[0],Mesh(TEXT("WingOutL")),FVector(0,-InnerSpan,0));
    B.Out[1]=Part(B.In[1],Mesh(TEXT("WingOutR")),FVector(0,InnerSpan,0));
    B.Root->SetVisibility(false,true);
    Birds.Add(B);
}

AChultBirds* AChultBirds::Spawn(UWorld* World)
{
    if(!World||Suppressed())return nullptr;
    if(AChultBirds* Existing=Find(World))return Existing;
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Flock=World->SpawnActor<AChultBirds>(AChultLanding::FloorOrigin(),FRotator::ZeroRotator,Params);
    if(!Flock)return nullptr;
    FRandomStream R(20261019);
    const FVector Spire=ToFrame(ChultTempleSpireTop());
    // The gyre over the spire: eight pairs, wheeling the same way at their own
    // radius and height, each pair wingtip to wingtip as macaws fly.
    for(int32 Pair=0;Pair<8;++Pair)
    {
        const int32 First=Flock->Birds.Num();
        for(int32 K=0;K<2;++K)
        {
            Flock->AddBird(ERole::Gyre,(Pair+K)%3!=0,1.5f);
            FBird& B=Flock->Birds.Last();
            B.Seed=R.FRandRange(0,10);B.FlapHz=R.FRandRange(3.8f,4.4f);B.FlapPhase=R.FRandRange(0,2*PI);
            B.Speed=R.FRandRange(820,980);B.Radius=2700+Pair*330+R.FRandRange(-150,150);
            B.Height=Spire.Z-1000+((Pair*5)%8)*300;B.Phase=Pair*2*PI/8*2.3f;B.Dir=1;
            B.Centre=FVector(Spire.X,Spire.Y,0);
            if(K==1){B.Partner=First;B.Lag=R.FRandRange(.25f,.5f);B.Beside=FVector(0,R.FRandRange(120,170)*(Pair%2?1:-1),R.FRandRange(-40,60));B.Speed=Flock->Birds[First].Speed;}
        }
    }
    // A lower wheel of six pairs over the avenue and the plaza's front, turning
    // the other way: they sweep over his head as he walks up to the temple.
    for(int32 Pair=0;Pair<6;++Pair)
    {
        const int32 First=Flock->Birds.Num();
        for(int32 K=0;K<2;++K)
        {
            Flock->AddBird(ERole::Gyre,(Pair+K)%2==0,1.45f);
            FBird& B=Flock->Birds.Last();
            B.Seed=R.FRandRange(0,10);B.FlapHz=R.FRandRange(3.9f,4.5f);B.FlapPhase=R.FRandRange(0,2*PI);
            B.Speed=R.FRandRange(860,1000);B.Radius=2100+Pair*300+R.FRandRange(-120,120);
            B.Height=ChultTemplePlazaZ()+2200+((Pair*3)%6)*190;B.Phase=Pair*2*PI/6*1.7f+.5f;B.Dir=-1;
            B.Centre=FVector(Spire.X-6800,Spire.Y,0);B.bSwooper=K==0;
            if(K==1){B.Partner=First;B.Lag=R.FRandRange(.25f,.5f);B.Beside=FVector(0,R.FRandRange(120,170)*(Pair%2?-1:1),R.FRandRange(-40,60));B.Speed=Flock->Birds[First].Speed;}
        }
    }
    // Two pairs on a figure of eight round the two pyramids, lower, over the stairs' heads.
    for(int32 Pair=0;Pair<2;++Pair)
    {
        const int32 First=Flock->Birds.Num();
        for(int32 K=0;K<2;++K)
        {
            Flock->AddBird(ERole::Eight,K==0,1.4f);
            FBird& B=Flock->Birds.Last();
            B.Seed=R.FRandRange(0,10);B.FlapHz=R.FRandRange(4.f,4.6f);B.FlapPhase=R.FRandRange(0,2*PI);
            B.Speed=1000;B.Radius=6900;B.Height=Spire.Z-900+Pair*500;B.Phase=Pair*PI;B.Dir=Pair?-1:1;
            B.Centre=FVector(Spire.X-2300,Spire.Y,0);
            if(K==1){B.Partner=First;B.Lag=.35f;B.Beside=FVector(0,140,30);}
        }
    }
    // The reveal's flock, waiting in the trees ahead on the road.
    for(int32 K=0;K<7;++K)
    {
        Flock->AddBird(ERole::Reveal,K%3!=1,1.5f);
        FBird& B=Flock->Birds.Last();
        B.Seed=R.FRandRange(0,10);B.FlapHz=R.FRandRange(4.2f,4.8f);B.FlapPhase=R.FRandRange(0,2*PI);
        B.Speed=R.FRandRange(1100,1250);B.Radius=3100+K*280;B.Height=Spire.Z-700+(K%4)*420;B.Dir=1;B.Centre=FVector(Spire.X,Spire.Y,0);
    }
    // The bridge: a flock to cross the gorge ahead of him, and a pair to overtake him.
    for(int32 K=0;K<7;++K)
    {
        Flock->AddBird(ERole::BridgeCross,K%3!=2,1.3f);
        FBird& B=Flock->Birds.Last();B.Seed=R.FRandRange(0,10);B.FlapHz=R.FRandRange(4.4f,5.2f);B.FlapPhase=R.FRandRange(0,2*PI);B.Speed=R.FRandRange(1180,1320);
    }
    for(int32 K=0;K<2;++K)
    {
        Flock->AddBird(ERole::BridgePass,K==0,1.3f);
        FBird& B=Flock->Birds.Last();B.Seed=R.FRandRange(0,10);B.FlapHz=5.f;B.FlapPhase=K*1.3f;B.Speed=1400;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS spawned=%d gyre=%d eight=%d reveal=%d bridge_cross=%d bridge_pass=%d missing_meshes=%d"),
        Flock->Birds.Num(),Flock->Count(ERole::Gyre),Flock->Count(ERole::Eight),Flock->Count(ERole::Reveal),Flock->Count(ERole::BridgeCross),Flock->Count(ERole::BridgePass),Flock->MissingMeshes);
    return Flock;
}

int32 AChultBirds::Count(ERole Kind) const
{
    int32 N=0;for(const FBird& B:Birds)N+=B.Role==Kind;return N;
}

FVector AChultBirds::OrbitAt(const FBird& B,double Time) const
{
    const float T=static_cast<float>(Time);
    if(B.Role==ERole::Eight)
    {
        // A lemniscate of Gerono across the precinct: out over each pyramid, crossing in front of the spire.
        const float W=B.Speed/(B.Radius*3.0f);
        const float Th=B.Phase+B.Dir*W*T;
        const float S=B.Centre.X+1900*FMath::Sin(2*Th);
        const float Tt=B.Centre.Y+B.Radius*FMath::Sin(Th);
        return At(S,Tt,B.Height+260*FMath::Sin(Th*2+B.Seed));
    }
    // The gyre: a wheeling circle that breathes in and out and rises and sinks,
    // its centre drifting slowly over the precinct.
    const float R=B.Radius*(1+.1f*FMath::Sin(.07f*T+B.Seed));
    const float Th=B.Phase+B.Dir*B.Speed/B.Radius*T;
    const float CS=B.Centre.X+700*FMath::Sin(.021f*T),CT=B.Centre.Y+1300*FMath::Sin(.017f*T+1);
    return At(CS+R*FMath::Cos(Th),CT+R*FMath::Sin(Th),B.Height+320*FMath::Sin(.13f*T+B.Seed*2));
}

FVector AChultBirds::FlightAt(const FBird& B,double Time,bool& bOut) const
{
    bOut=false;
    if(B.Start<0||B.Path.Num()<2)return FVector::ZeroVector;
    const float D=static_cast<float>((Time-B.Start-B.Delay)*B.Speed);
    if(D<0){if(B.bOrbitBefore){bOut=true;return OrbitAt(B,Time);}return B.Path[0];}
    if(D>B.Len.Last())
    {
        if(B.bJoinOrbit){bOut=true;return OrbitAt(B,Time);}
        return B.Path.Last();
    }
    bOut=true;return AlongPath(B.Path,B.Len,D);
}

bool AChultBirds::IsOut(int32 Index,double Time) const
{
    const FBird& B=Birds[Index];
    if(B.Role==ERole::Gyre||B.Role==ERole::Eight)return true;
    bool bOut;FlightAt(B,Time,bOut);return bOut;
}

FVector AChultBirds::Position(int32 Index,double Time) const
{
    const FBird& B=Birds[Index];
    if(B.Partner>=0)
    {
        // Beside its partner a moment behind, offset across its heading.
        const double T=Time-B.Lag;
        const FVector P=Position(B.Partner,T),V=Position(B.Partner,T+.1)-Position(B.Partner,T-.1);
        const FVector Right=FVector(-V.Y,V.X,0).GetSafeNormal();
        return P+Right*B.Beside.Y+FVector(0,0,B.Beside.Z);
    }
    if((B.Role==ERole::Gyre||B.Role==ERole::Eight)&&B.Start<0)return OrbitAt(B,Time);
    bool bOut;return FlightAt(B,Time,bOut);
}

void AChultBirds::LaunchBridge(bool bPass,float S,float Z,float Dir,float Side,double Now)
{
    const ERole Kind=bPass?ERole::BridgePass:ERole::BridgeCross;
    int32 K=0;
    for(FBird& B:Birds)
    {
        if(B.Role!=Kind)continue;
        TArray<FVector> P;
        // Clear of both walls, which lean in below the lips.
        auto Clip=[](float X){return FMath::Clamp(X,600.f,BridgeEnd-650);};
        if(!bPass)
        {
            // Up out of the gorge from ahead of him on one side, across the bridge a
            // few metres in front of him (the leader near his own height), and away
            // past him up the other reach: in his camera's view the whole way.
            // His camera looks down on him into the gorge, so most cross at or under the deck.
            static const float Off[]={0,240,-160,480,650,-300,820},Up[]={60,-160,-380,220,-560,340,-60},Delay[]={0,.22f,.45f,.65f,.85f,1.1f,1.4f};
            const float Sx=S+Dir*Off[K],Zx=Z+Up[K];
            P={At(Clip(Sx+Dir*2600),-Side*4600,Zx-900),At(Clip(Sx+Dir*1300),-Side*2000,Zx-250),At(Clip(Sx),0,Zx),
               At(Clip(Sx-Dir*500),Side*2300,Zx+350),At(Clip(Sx-Dir*900),Side*6500,Zx+1400)};
            B.Delay=Delay[K];
        }
        else
        {
            // From behind him, low over his head and away ahead, banking off down the gorge.
            const float Lat=K?-Side*95:Side*25,Rise=K?55:0;
            P={At(S-Dir*1600,Side*480+Lat,Z+650+Rise),At(S-Dir*700,Side*230+Lat,Z+320+Rise),At(S+Dir*150,Lat,Z+190+Rise),
               At(S+Dir*900,Side*1300+Lat,Z-120+Rise),At(S+Dir*1400,Side*3700,Z-850),At(S+Dir*1600,Side*6200,Z-650)};
            for(int32 I=0;I<P.Num();++I){const FVector F=ToFrame(P[I]);P[I]=At(FMath::Clamp(static_cast<float>(F.X),I?600.f:100.f,BridgeEnd-(I?650.f:150.f)),F.Y,F.Z);}
            B.Delay=K*.32f;
        }
        MakePath(P,B.Path,B.Len);B.Start=Now;B.bJoinOrbit=false;B.Flap=1;++K;
    }
    ++(bPass?BridgePasses:BridgeCrossings);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_LAUNCH bridge_%s s=%.0f z=%.0f dir=%.0f side=%.0f"),bPass?TEXT("pass"):TEXT("cross"),S,Z,Dir,Side);
}

void AChultBirds::LaunchReveal(double Now)
{
    const float Reveal=ChultTempleRevealS();
    int32 K=0;
    for(FBird& B:Birds)
    {
        if(B.Role!=ERole::Reveal)continue;
        // They burst from the undergrowth either side of the road just ahead of
        // him, into the road's opening, and away up it ahead of him toward the
        // temple, climbing (his camera follows them up the road), then round
        // into the gyre over the spire.
        const float S=Reveal+500+K*120,Side=K%2?1.f:-1.f;
        auto Road=[](float Sr,float Off,float H){const FVector P=At(Sr,ChultUpriverRoadT(Sr)+Off,0);return At(Sr,ChultUpriverRoadT(Sr)+Off,ChultCanyonGroundZ(P)+H);};
        const FVector Off=Road(S,Side*(650+(K%3)*150),150+(K%3)*60);
        B.Delay=K*.25f;B.Start=Now;B.bJoinOrbit=true;B.bOrbitBefore=false;B.Flap=1;
        const float W=B.Speed/B.Radius;
        float Arrive=Now+B.Delay+14;
        for(int32 Pass=0;Pass<3;++Pass)
        {
            // Meet the circle on its side toward the road (theta near pi).
            B.Phase=PI+(K-3)*.12f-B.Dir*W*static_cast<float>(Arrive);
            const FVector End=OrbitAt(B,Arrive),Before=OrbitAt(B,Arrive-1.6);
            // Through the strip of sky over the road, above the undergrowth that
            // hides anything lower from his camera, for the first 30-50 m.
            const FVector Lift=Road(S+900,Side*(160+(K%3)*60),340+(K%2)*60);
            const FVector Cross=Road(S+2800,-Side*(80+(K%3)*70),540+(K%3)*50);
            const FVector Climb=Road(S+5600,Side*(K%3-1)*180,950+K*50);
            const FVector Mid=FMath::Lerp(Climb,Before,.5f)+FVector(0,0,600);
            MakePath({Off,Lift,Cross,Climb,Mid,Before,End},B.Path,B.Len);
            Arrive=Now+B.Delay+B.Len.Last()/B.Speed;
        }
        B.Arrive=Arrive;++K;
    }
    ++RevealFlights;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_LAUNCH reveal birds=%d"),K);
}

void AChultBirds::Watch(double Now)
{
    auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!C)return;
    const FVector Local=C->GetActorLocation()-AChultLanding::FloorOrigin(),F=ToFrame(Local);
    const float S=F.X;
    if(LastSAt>=0&&Now-LastSAt<.5){const float V=(S-LastS)/FMath::Max(static_cast<float>(Now-LastSAt),.001f);AlongSpeed=FMath::Lerp(AlongSpeed,V,.08f);}
    // A teleport (a respawn, a checkpoint) is not arriving: only a frame's move counts.
    const float PrevS=LastS;const bool bFresh=LastSAt>=0&&Now-LastSAt<.5&&FMath::Abs(S-PrevS)<400;
    LastS=S;LastSAt=Now;
    // The bridge: once each per crossing, re-armed off the bridge.
    const bool bOverGorge=S>-100&&S<BridgeEnd+150&&FMath::Abs(F.Y)<700;
    if(!bOverGorge){bCrossArmed=bPassArmed=true;}
    else if(C->IsSwinging()||C->IsSwingLeaping())
    {
        const float Dir=FMath::Abs(AlongSpeed)>25?FMath::Sign(AlongSpeed):(S<BridgeEnd*.5f?1.f:-1.f);
        const float Progress=Dir>0?S/BridgeEnd:(BridgeEnd-S)/BridgeEnd;
        // Where he'll be when they get there (the flock takes ~3.7 s to the bridge, the pair ~1.7 s).
        const float Ahead=FMath::Clamp(AlongSpeed*Dir,0.f,320.f);
        const float Side=FMath::FRand()<.5f?-1.f:1.f;
        if(bCrossArmed&&Progress>.14f&&Progress<.8f){bCrossArmed=false;LaunchBridge(false,S+Dir*(320+Ahead*3.7f),Local.Z,Dir,Side,Now);}
        else if(bPassArmed&&!bCrossArmed&&Progress>.4f&&Progress<.56f){bPassArmed=false;LaunchBridge(true,S+Dir*Ahead*1.7f,Local.Z,Dir,Side,Now);}
    }
    // In the precinct, every 15-25 s a pair from the low wheel swoops across in front of him.
    // Not underground in the temple interior (2026-10-20).
    if(S>64000&&S<82000&&FMath::Abs(F.Y)<10500&&!InChultTempleInterior(C->GetActorLocation(),200))
    {
        if(NextSwoop<0)NextSwoop=Now+6;
        else if(Now>=NextSwoop)
        {
            const APlayerController* PC=Cast<APlayerController>(C->GetController());
            FVector View=PC&&PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraRotation().Vector():C->GetActorForwardVector();
            View.Z=0;View=View.GetSafeNormal();
            NextSwoop=Now+(TrySwoop(Now,Local,View)?FMath::FRandRange(15.f,24.f):3.f);
        }
    }
    else NextSwoop=-1;
    // The temple: as it comes into view running up the road.
    const float Reveal=ChultTempleRevealS();
    if(S<Reveal-8000&&!bRevealArmed)
    {
        bRevealArmed=true;
        for(FBird& B:Birds)if(B.Role==ERole::Reveal)B.Start=-1;
    }
    if(bRevealArmed&&bFresh&&PrevS<Reveal-250&&S>=Reveal-250&&FMath::Abs(F.Y-ChultUpriverRoadT(S))<3000)
    {
        bRevealArmed=false;LaunchReveal(Now);
    }
}

bool AChultBirds::IsSwooping(double Now) const
{
    for(const FBird& B:Birds)if(B.bSwooper&&B.Start>=0&&Now<B.Arrive)return true;
    return false;
}

bool AChultBirds::TrySwoop(double Now,const FVector& ChuckLocal,const FVector& ViewForward)
{
    UWorld* World=GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    const FVector Right(-ViewForward.Y,ViewForward.X,0);
    auto Clear=[&](const TArray<FVector>& Path)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultBirdSwoop),true);FHitResult Hit;
        for(int32 I=1;I<Path.Num();++I)
            for(int32 Try=0;Try<4&&World->LineTraceSingleByObjectType(Hit,G+Path[I-1],G+Path[I],Objects,Q);++Try)
            {
                if(Hit.GetComponent()&&(!Hit.GetComponent()->IsVisible()||Hit.GetComponent()->bHiddenInGame)){Q.AddIgnoredComponent(Hit.GetComponent());continue;}
                return false;
            }
        return true;
    };
    const bool bOvertake=SwoopTurn%2==0;const float Side=(SwoopTurn/2)%2?1.f:-1.f;++SwoopTurn;
    for(const FVector2D Try:{FVector2D(1300,320),FVector2D(1600,420),FVector2D(1100,260),FVector2D(1900,520)})
    {
        // His follow camera looks down on him: only low and close is in its view.
        // Alternately: down out of the wheel over his shoulder and away ahead of
        // him low up the avenue (in view as they go); or in from one side,
        // skimming 2.5-5 m over the ground 11-19 m ahead, and out the other.
        auto OnGround=[](FVector P,float H){P.Z=ChultCanyonGroundZ(P)+H;return P;};
        FVector Low=OnGround(ChuckLocal+ViewForward*Try.X,Try.Y),In,Out;
        TArray<FVector> Way;
        if(bOvertake)
        {
            In=ChuckLocal-ViewForward*500+Right*Side*350+FVector(0,0,900);
            Way={In,OnGround(ChuckLocal+ViewForward*700+Right*Side*110,Try.Y*.85f),OnGround(ChuckLocal+ViewForward*(Try.X+900)-Right*Side*220,Try.Y*1.2f)};
            Out=OnGround(ChuckLocal+ViewForward*(Try.X+2800)+Right*Side*700,Try.Y+900);
        }
        else
        {
            In=Low-Right*Side*1900+ViewForward*300+FVector(0,0,1000);Out=Low+Right*Side*2100+ViewForward*700+FVector(0,0,1200);
            Way={In,Low};
        }
        // The free leader whose wheel brings it nearest the way in.
        int32 Pick=-1;float Best=1e12f;
        for(int32 I=0;I<Birds.Num();++I)
        {
            const FBird& B=Birds[I];if(!B.bSwooper||(B.Start>=0&&Now<B.Arrive+2))continue;
            const float D=FVector::DistSquared(OrbitAt(B,Now),In);if(D<Best){Best=D;Pick=I;}
        }
        if(Pick<0)return false;
        FBird& B=Birds[Pick];
        TArray<FVector> Path;TArray<float> Len;
        double Arrive=Now+(FMath::Sqrt(Best)+FVector::Dist(In,Out)+3000)/B.Speed;
        for(int32 Pass=0;Pass<3;++Pass)
        {
            TArray<FVector> Points={OrbitAt(B,Now),OrbitAt(B,Now+1.2)};Points.Append(Way);Points.Append({Out,OrbitAt(B,Arrive-1.6),OrbitAt(B,Arrive)});
            MakePath(Points,Path,Len);
            Arrive=Now+Len.Last()/B.Speed;
        }
        if(!Clear(Path))continue;
        B.Path=Path;B.Len=Len;B.Start=Now;B.Delay=0;B.bJoinOrbit=B.bOrbitBefore=true;B.Arrive=Arrive;
        ++Swoops;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_SWOOP bird=%d kind=%s ahead=%.0f height=%.0f seconds=%.1f"),Pick,bOvertake?TEXT("overtake"):TEXT("across"),Try.X,Try.Y,Arrive-Now);
        return true;
    }
    return false;
}

void AChultBirds::Pose(FBird& B,int32 Index,double Now,float Dt)
{
    const bool bOut=IsOut(Index,Now);
    if(bOut!=B.bVisible){B.bVisible=bOut;B.Root->SetVisibility(bOut,true);}
    if(!bOut)return;
    const FVector P=Position(Index,Now),P0=Position(Index,Now-.12),P1=Position(Index,Now+.12);
    const FVector V=(P1-P0)/.24f,A=(P1-2*P+P0)/(.12f*.12f);
    const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(V.Y,V.X));
    const float Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(V.Z,V.Size2D()))*.8f,-35.f,35.f);
    const FVector Right(-FMath::Sin(FMath::DegreesToRadians(Yaw)),FMath::Cos(FMath::DegreesToRadians(Yaw)),0);
    const float Roll=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(A,Right),980.f)),-55.f,55.f);
    // Flapping: flights mostly flap, the gyre mostly glides in long arcs with
    // flapping bouts; anything climbing flaps.
    const float T=static_cast<float>(Now),Climb=V.Z/FMath::Max(B.Speed,1.f);
    float Want;
    if(B.Role==ERole::Gyre||B.Role==ERole::Eight||(B.Role==ERole::Reveal&&Now>B.Arrive))
        Want=FMath::Sin(.31f*T+B.Seed*7)>.5f?1.f:.0f;
    else
    {
        const float Since=static_cast<float>(Now-B.Start-B.Delay);
        Want=Since<2.5f?1.f:(FMath::Sin(.9f*T+B.Seed)>.75f?.15f:.9f);
    }
    Want=FMath::Max(Want,Smooth((Climb-.05f)/.12f));
    B.Flap=FMath::FInterpTo(B.Flap,Want,Dt,2.2f);
    B.FlapPhase=FMath::Fmod(B.FlapPhase+2*PI*B.FlapHz*Dt,2*PI);
    const float W=B.Flap,Sn=FMath::Sin(B.FlapPhase),Cs=FMath::Cos(B.FlapPhase);
    const float Inner=(1-W)*7+W*(12+46*Sn),Outer=(1-W)*-5+W*(6+30*FMath::Sin(B.FlapPhase-.7f));
    const float Sweep=W*12*FMath::Max(0.f,-Cs);
    B.Root->SetWorldLocationAndRotation(AChultLanding::FloorOrigin()+P,FRotator(Pitch,Yaw,Roll));
    B.Body->SetRelativeLocationAndRotation(FVector(0,0,-W*2.4f*Sn),FRotator(W*3*Cs,0,0));
    B.In[0]->SetRelativeRotation(FRotator(0,0,Inner));B.In[1]->SetRelativeRotation(FRotator(0,0,-Inner));
    B.Out[0]->SetRelativeRotation(FRotator(0,-Sweep,Outer));B.Out[1]->SetRelativeRotation(FRotator(0,Sweep,-Outer));
    B.LastPos=P;
}

void AChultBirds::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now=GetWorld()->GetTimeSeconds();
    Watch(Now);
    // Only what can be seen: the temple's birds within ~350 m, the bridge's while out.
    const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    const FVector Eye=Pawn?Pawn->GetActorLocation()-AChultLanding::FloorOrigin():FVector::ZeroVector;
    const bool bTemple=FVector::Dist2D(Eye,ChultTempleSpireTop())<35000;
    for(int32 I=0;I<Birds.Num();++I)
    {
        FBird& B=Birds[I];
        const bool bTempleBird=B.Role==ERole::Gyre||B.Role==ERole::Eight||B.Role==ERole::Reveal;
        if(bTempleBird&&!bTemple){if(B.bVisible){B.bVisible=false;B.Root->SetVisibility(false,true);}continue;}
        Pose(B,I,Now,DeltaSeconds);
    }
}

int32 AChultBirds::CheckClearance(float Seconds,TArray<FString>& Hits)
{
    UWorld* World=GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultBirds),true);
    int32 Count=0;
    auto Trace=[&](const FVector& A,const FVector& B,const TCHAR* What,int32 Index)
    {
        FHitResult Hit;FCollisionQueryParams Each=Q;
        // Invisible blockers (the jungle wall behind the temple) are not seen to be flown through.
        for(int32 Try=0;Try<6&&World->LineTraceSingleByObjectType(Hit,G+A,G+B,Objects,Each);++Try)
        {
            if(Hit.GetComponent()&&(!Hit.GetComponent()->IsVisible()||Hit.GetComponent()->bHiddenInGame)){Each.AddIgnoredComponent(Hit.GetComponent());continue;}
            ++Count;if(Hits.Num()<12)Hits.Add(FString::Printf(TEXT("%s#%d@%s:%s"),What,Index,*ToFrame(Hit.ImpactPoint-G).ToString(),*GetNameSafe(Hit.GetComponent())));
            break;
        }
    };
    // Every orbiting bird (and each pair's follower) for Seconds of flight.
    for(int32 I=0;I<Birds.Num();++I)
    {
        const FBird& B=Birds[I];
        if(B.Role!=ERole::Gyre&&B.Role!=ERole::Eight)continue;
        for(float T=0;T<Seconds;T+=.25f)Trace(Position(I,T),Position(I,T+.25f),B.Role==ERole::Gyre?TEXT("gyre"):TEXT("eight"),I);
    }
    // Sample bridge flights at three places, both ways, from both sides; then the reveal.
    const int32 SavedCross=BridgeCrossings,SavedPass=BridgePasses,SavedReveal=RevealFlights;
    for(float Where:{.15f,.4f,.56f,.75f})for(float Dir:{1.f,-1.f})for(float Side:{1.f,-1.f})for(bool bPass:{false,true})
    {
        // Where each fires: the flock from 15% to 80% of the way over, the pair from 40% to 56%.
        if(bPass&&(Where<.4f||Where>.56f))continue;
        const float S=Dir>0?Where*BridgeEnd:BridgeEnd*(1-Where);
        LaunchBridge(bPass,bPass?S:S+Dir*320,ChultBridgeDeckZ(S)-70,Dir,Side,0);
        for(int32 I=0;I<Birds.Num();++I)
        {
            const FBird& B=Birds[I];if(B.Role!=(bPass?ERole::BridgePass:ERole::BridgeCross))continue;
            for(float D=0;D+50<B.Len.Last();D+=50)Trace(AlongPath(B.Path,B.Len,D),AlongPath(B.Path,B.Len,D+50),bPass?TEXT("pass"):TEXT("cross"),I);
        }
    }
    LaunchReveal(0);
    for(int32 I=0;I<Birds.Num();++I)
    {
        const FBird& B=Birds[I];if(B.Role!=ERole::Reveal)continue;
        // From 3 m out of the canopy (it takes off from among the leaves) on into the gyre.
        for(float D=300;D+50<B.Len.Last();D+=50)Trace(AlongPath(B.Path,B.Len,D),AlongPath(B.Path,B.Len,D+50),TEXT("reveal"),I);
        for(float T=0;T<Seconds;T+=.25f)Trace(OrbitAt(B,B.Arrive+T),OrbitAt(B,B.Arrive+T+.25f),TEXT("reveal_gyre"),I);
    }
    for(FBird& B:Birds)if(B.Role==ERole::Reveal||B.Role==ERole::BridgeCross||B.Role==ERole::BridgePass)B.Start=-1;
    BridgeCrossings=SavedCross;BridgePasses=SavedPass;RevealFlights=SavedReveal;
    return Count;
}

// -ChuckChultTest -ChuckBirdReview: checks, stills of the temple's birds, then
// the real character crosses the bridge (both flights must come, the flock
// in his camera's view, the pair close over him) and runs up the road to the
// reveal (the flock must go up and reach the gyre); then quits.
void ReviewChultBirds(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    UWorld* World=Owner->GetWorld();AChultBirds* Flock=AChultBirds::Find(World);
    const FVector G=AChultLanding::FloorOrigin();
    using ERole=AChultBirds::ERole;
    TArray<FString> Hits;const int32 Clear=Flock?Flock->CheckClearance(240,Hits):-1;
    const int32 Static=!Flock||Flock->GetMissingMeshes()>0||Flock->Count(ERole::Gyre)!=28||Flock->Count(ERole::Eight)!=4||Flock->Count(ERole::Reveal)!=7
        ||Flock->Count(ERole::BridgeCross)!=7||Flock->Count(ERole::BridgePass)!=2||Clear!=0;
    // The precinct's tallest planting (foliage doesn't collide): how high the crowns reach near the gyre.
    float Crowns=0;int32 InCrowns=0,Samples=0,InCrownsLow=0;
    if(Flock)
    {
        TArray<FBox> Boxes;const FVector SpireWorld=G+ChultTempleSpireTop();
        for(TObjectIterator<UInstancedStaticMeshComponent> It;It;++It)
        {
            if(It->GetWorld()!=World||!It->GetStaticMesh())continue;
            const FBox Local=It->GetStaticMesh()->GetBoundingBox();
            for(int32 I=0;I<It->GetInstanceCount();++I)
            {
                FTransform X;It->GetInstanceTransform(I,X,true);
                if(FVector::Dist2D(X.GetLocation(),SpireWorld)>9500)continue;
                const FBox Box=Local.TransformBy(X);Boxes.Add(Box);Crowns=FMath::Max(Crowns,static_cast<float>(Box.Max.Z-SpireWorld.Z));
            }
        }
        for(int32 I=0;I<Flock->GetBirds().Num();++I)
        {
            if(Flock->GetBirds()[I].Role!=ERole::Gyre&&Flock->GetBirds()[I].Role!=ERole::Eight)continue;
            const bool bLow=Flock->GetBirds()[I].Centre.X<ToFrame(ChultTempleSpireTop()).X-1000;
            for(float T=0;T<240;T+=1){const FVector P=G+Flock->Position(I,T);++Samples;for(const FBox& Box:Boxes)if(Box.IsInside(P)){++InCrowns;InCrownsLow+=bLow;break;}}
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_CHECK failures=%d birds=%d missing_meshes=%d clearance_hits=%d crowns_top_above_spire=%.0f in_crown_bounds=%d/%d low_wheel=%d %s"),Static,Flock?Flock->GetBirds().Num():0,Flock?Flock->GetMissingMeshes():-1,Clear,Crowns,InCrowns,Samples,InCrownsLow,*FString::Join(Hits,TEXT(" ")));
    if(!Flock){FPlatformMisc::RequestExitWithStatus(false,1);return;}
    auto* PC=Cast<APlayerController>(Chuck->GetController());
    // No stray mouse or pad input moves him or his camera (as the bridge test).
    Chuck->DisableInput(PC);Chuck->SetLookLocked(true);
    auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(70);
    const FString Folder=FPaths::ScreenShotDir()/TEXT("Chult");IFileManager::Get().MakeDirectory(*Folder,true);
    auto Shot=[Folder](const FString& Name){FScreenshotRequest::RequestScreenshot(Folder/(Name+TEXT(".png")),true,false);};
    // Stills: temple views (Eye, Aim local), then a chase view on a scarlet and a blue-and-gold bird.
    const FVector Spire=ChultTempleSpireTop();
    const FVector SF=ToFrame(Spire);
    struct FView{FString Name;FVector Eye,Aim;int32 Chase=-1;};
    TArray<FView> Views;
    auto Ground=[](float S,float T,float H){const FVector P=At(S,T,0);return At(S,T,ChultCanyonGroundZ(P)+H);};
    Views.Add({TEXT("BirdsTemplePlaza"),Ground(SF.X-9000,-600,170),At(SF.X,0,SF.Z+1400)});
    Views.Add({TEXT("BirdsAvenueUp"),Ground(SF.X-11500,350,170),At(SF.X-6800,0,ChultTemplePlazaZ()+2900)});
    Views.Add({TEXT("BirdsTempleStair"),Ground(SF.X-5200,700,1600),At(SF.X+600,0,SF.Z+2200)});
    Views.Add({TEXT("BirdsTempleAvenue"),Ground(ChultTempleRevealS()+1600,ChultUpriverRoadT(ChultTempleRevealS()+1600),160),At(SF.X,0,SF.Z+300)});
    Views.Add({TEXT("BirdsTempleWide"),Ground(SF.X-16000,-3500,5200),At(SF.X,0,SF.Z)});
    int32 Scarlet=-1,BlueGold=-1;
    for(int32 I=0;I<Flock->GetBirds().Num();++I)
    {
        const auto& B=Flock->GetBirds()[I];if(B.Role!=ERole::Gyre)continue;
        const bool bScarlet=B.Body->GetStaticMesh()&&B.Body->GetStaticMesh()->GetName().Contains(TEXT("Scarlet"));
        if(bScarlet&&Scarlet<0)Scarlet=I;if(!bScarlet&&BlueGold<0)BlueGold=I;
    }
    Views.Add({TEXT("BirdsCloseScarlet"),FVector::ZeroVector,FVector::ZeroVector,Scarlet});
    Views.Add({TEXT("BirdsCloseBlueGold"),FVector::ZeroVector,FVector::ZeroVector,BlueGold});
    Views.Add({TEXT("BirdsCloseUnder"),FVector::ZeroVector,FVector::ZeroVector,Scarlet>=0?Scarlet+1:-1});

    struct FRun
    {
        int32 Phase=0,View=0,Failures=0,Jumps=0,Shots=0,CrossSeen=0,PassSeen=0,RevealSeen=0,SwoopSeen=0,SwoopsAt=0;double Next=0,Start=-1,LastJump=-1,CrossAt=-1,PassAt=-1,RevealAt=-1;
        float PassNearest=1e9f,CrossNearest=1e9f,CrossTime=0,PassTime=0,RevealTime=0,SwoopTime=0;double Last=-1;TSet<int32> Seen;bool bDone=false;FTimerHandle Timer;
    };
    auto Run=MakeShared<FRun>();Run->Failures=Failures+Static;
    TWeakObjectPtr<ACameraActor> Cam=Camera;TWeakObjectPtr<AChultBirds> WeakFlock=Flock;
    World->GetTimerManager().SetTimer(Run->Timer,[World,Chuck,PC,Run,Views,Cam,WeakFlock,G,Shot]()
    {
        if(Run->bDone||!WeakFlock.IsValid())return;
        AChultBirds* Flock=WeakFlock.Get();
        const double Now=World->GetTimeSeconds();const float Dt=Run->Last<0?0.f:static_cast<float>(Now-Run->Last);Run->Last=Now;
        // 1: stills, one every 2.5 s; a chase view follows its bird.
        if(Run->Phase==0)
        {
            if(Run->View>=Views.Num()){Run->Phase=1;if(Cam.IsValid())Cam->Destroy();PC->SetViewTarget(Chuck);Chuck->SetActorHiddenInGame(false);Run->Start=-1;return;}
            const FView& V=Views[Run->View];
            // He waits hidden at the temple (its birds fly only near it).
            if(Run->Start<0){Run->Start=Now;if(Run->View==0)Chuck->ResetAtLocation(ChultTempleSpawn());Chuck->SetActorHiddenInGame(true);if(Cam.IsValid())PC->SetViewTarget(Cam.Get());}
            if(Cam.IsValid())
            {
                if(V.Chase>=0)
                {
                    const FVector P=Flock->Position(V.Chase,Now),D=(Flock->Position(V.Chase,Now+.1)-P).GetSafeNormal();
                    const FVector Right=FVector(-D.Y,D.X,0).GetSafeNormal();
                    const bool bUnder=V.Name.Contains(TEXT("Under"));
                    const FVector Eye=P-D*(bUnder?180:260)+Right*(bUnder?60:150)+FVector(0,0,bUnder?-160:55);
                    Cam->SetActorLocationAndRotation(G+Eye,(P+D*40-Eye).Rotation());
                }
                else Cam->SetActorLocationAndRotation(G+V.Eye,(V.Aim-V.Eye).Rotation());
            }
            if(Now-Run->Start>2.5){Shot(V.Name);UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_VIEW name=%s"),*V.Name);++Run->View;Run->Start=-1;}
            return;
        }
        // In view of his own camera, within 25 m: what a player would see.
        auto Seen=[&](ERole Role,float& Nearest)
        {
            const FVector CamAt=PC->PlayerCameraManager->GetCameraLocation(),CamFwd=PC->PlayerCameraManager->GetCameraRotation().Vector();
            int32 N=0;
            for(int32 I=0;I<Flock->GetBirds().Num();++I)
            {
                if(Flock->GetBirds()[I].Role!=Role||!Flock->IsOut(I,Now))continue;
                const FVector P=G+Flock->Position(I,Now);
                Nearest=FMath::Min(Nearest,static_cast<float>(FVector::Dist(P,Chuck->GetActorLocation())));
                // On his screen (projected into the viewport) and within 40 m.
                FVector2D Screen;int32 W=0,H=0;PC->GetViewportSize(W,H);
                if(FVector::Dist(P,CamAt)<4000&&FVector::DotProduct(P-CamAt,CamFwd)>0&&PC->ProjectWorldLocationToScreen(P,Screen)
                   &&Screen.X>=0&&Screen.Y>=0&&Screen.X<W&&Screen.Y<H){Run->Seen.Add(I);++N;}
            }
            return N;
        };
        // 2: the bridge, crossed plank to plank by the real character.
        if(Run->Phase==1)
        {
            const auto& Planks=ChultBridgePlanks();const int32 Last=Planks.Num()-1;
            const FVector LastGrip=G+Planks[Last].Grip;
            const int32 LastIndex=GetChuckSwingGrips().IndexOfByPredicate([&](const FChuckSwingGrip& S){return FVector::Dist(S.Grip,LastGrip)<1;});
            if(Run->Start<0)
            {
                Run->Start=Now;Run->Jumps=0;
                Chuck->ResetAtLocation(G+ChultBridgeNearStand()+FVector(0,0,34.65f));Chuck->SetActorRotation(FRotator(0,ChultBridgeRespawnYaw(),0));Chuck->Recenter();
                Chuck->SetOrbitPitch(-8);PC->SetViewTarget(Chuck);return;
            }
            const float Time=Now-Run->Start;
            Chuck->SetTestStick(FVector2D::ZeroVector);
            if(Run->Jumps==0&&Time>.8&&Chuck->GetCharacterMovement()->IsMovingOnGround()){Chuck->JumpPressed();Run->LastJump=Time;++Run->Jumps;}
            else if(Chuck->IsSwinging()&&Chuck->GetSwingGrip()!=LastIndex&&Time-Run->LastJump>.45){Chuck->JumpPressed();Run->LastJump=Time;++Run->Jumps;}
            if(Run->CrossAt<0&&Flock->GetBridgeFlights(false)>0)Run->CrossAt=Now;
            if(Run->PassAt<0&&Flock->GetBridgeFlights(true)>0)Run->PassAt=Now;
            const int32 CrossNow=Seen(ERole::BridgeCross,Run->CrossNearest),PassNow=Seen(ERole::BridgePass,Run->PassNearest);
            Run->CrossSeen=FMath::Max(Run->CrossSeen,CrossNow);Run->PassSeen=FMath::Max(Run->PassSeen,PassNow);
            Run->CrossTime+=CrossNow?Dt:0;Run->PassTime+=PassNow?Dt:0;
            const bool bFlying=(Run->CrossAt>0&&Now-Run->CrossAt<9)||(Run->PassAt>0&&Now-Run->PassAt<5);
            if(bFlying&&Now>=Run->Next){Run->Next=Now+.3;Shot(FString::Printf(TEXT("BirdsBridge_%02d"),Run->Shots++));}
            const bool bEnd=Chuck->IsSwinging()&&Chuck->GetSwingGrip()==LastIndex;
            if((bEnd&&Run->PassAt>0&&Now-Run->PassAt>5)||Time>60||Chuck->GetFallDeaths()>0)
            {
                const bool Pass=bEnd&&Run->CrossAt>0&&Run->PassAt>0&&Run->CrossSeen>=3&&Run->CrossTime>=1.5f&&Run->PassSeen>=1&&Run->PassTime>=.5f&&Run->PassNearest<400&&Run->PassNearest>60;
                Run->Failures+=!Pass;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_BRIDGE failures=%d crossed=%d cross_at=%.1f pass_at=%.1f cross_in_view=%d cross_on_screen_s=%.1f pass_in_view=%d pass_on_screen_s=%.1f cross_nearest=%.0f pass_nearest=%.0f seconds=%.1f"),
                    !Pass,bEnd,Run->CrossAt>0?Run->CrossAt-Run->Start:-1.,Run->PassAt>0?Run->PassAt-Run->Start:-1.,Run->CrossSeen,Run->CrossTime,Run->PassSeen,Run->PassTime,Run->CrossNearest,Run->PassNearest,Time);
                Run->Phase=2;Run->Start=-1;Run->Shots=0;Run->Seen.Reset();
            }
            return;
        }
        // 3: up the road to the reveal; the flock goes up and joins the gyre.
        if(Run->Phase==2)
        {
            const float From=ChultTempleRevealS()-2600;
            if(Run->Start<0)
            {
                const FVector Foot=At(From,ChultUpriverRoadT(From),0);
                Chuck->ResetAtLocation(G+FVector(Foot.X,Foot.Y,ChultCanyonGroundZ(Foot)+40));
                Chuck->SetActorRotation(FRotator(0,(At(From+500,ChultUpriverRoadT(From+500),0)-Foot).Rotation().Yaw,0));Chuck->Recenter();
                Chuck->SetOrbitPitch(-8);Chuck->SetRunHeld(true);Run->Start=Now;Run->Next=Now;return;
            }
            const float Time=Now-Run->Start;const FVector F=ToFrame(Chuck->GetActorLocation()-G);
            if(Run->RevealAt<0&&Flock->GetRevealFlights()>0)Run->RevealAt=Now;
            float Nearest=1e9f;const int32 RevealNow=Seen(ERole::Reveal,Nearest);Run->RevealSeen=FMath::Max(Run->RevealSeen,RevealNow);Run->RevealTime+=RevealNow?Dt:0;
            if(Run->RevealAt>0&&Now-Run->RevealAt<12&&Now>=Run->Next){Run->Next=Now+.4;Shot(FString::Printf(TEXT("BirdsReveal_%02d"),Run->Shots++));}
            if(F.X<ChultTempleRevealS()+1400&&Time<40)
            {
                const FVector Ahead=G+At(F.X+500,ChultUpriverRoadT(F.X+500),0);
                const FVector D=(Ahead-Chuck->GetActorLocation()).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;
            }
            Chuck->SetTestStick(FVector2D::ZeroVector);Chuck->SetRunHeld(false);
            // Wait for the last to reach the gyre, then check each is on its circle.
            double Arrive=0;for(const auto& B:Flock->GetBirds())if(B.Role==ERole::Reveal)Arrive=FMath::Max(Arrive,B.Arrive);
            if(Run->RevealAt>0&&Now<Arrive+3&&Time<80)return;
            int32 OnCircle=0;
            for(int32 I=0;I<Flock->GetBirds().Num();++I)
            {
                const auto& B=Flock->GetBirds()[I];if(B.Role!=ERole::Reveal||!Flock->IsOut(I,Now))continue;
                const FVector P=ToFrame(Flock->Position(I,Now));
                OnCircle+=FVector::Dist2D(P,FVector(B.Centre.X,B.Centre.Y,0))<B.Radius*1.6f+2000&&P.Z>B.Height-600;
            }
            const bool Pass=Run->RevealAt>0&&Run->RevealSeen>=3&&Run->RevealTime>=3&&OnCircle==Flock->Count(ERole::Reveal);
            Run->Failures+=!Pass;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_REVEAL failures=%d in_view=%d on_screen_s=%.1f in_gyre=%d seconds=%.1f"),!Pass,Run->RevealSeen,Run->RevealTime,OnCircle,Time);
            Run->Phase=3;Run->Start=-1;Run->Shots=0;Run->Seen.Reset();return;
        }
        // 4: standing on the avenue facing the temple: the low wheel's pairs swoop across his view.
        if(Run->Phase==3)
        {
            if(Run->Start<0)
            {
                Chuck->ResetAtLocation(ChultTempleSpawn());Chuck->SetActorRotation(FRotator(0,ChultTempleSpawnYaw(),0));Chuck->Recenter();
                Chuck->SetOrbitPitch(-8);Run->Start=Now;Run->SwoopsAt=Flock->GetSwoops();return;
            }
            const float Time=Now-Run->Start;
            // Low-wheel birds are Gyre birds; count those within 40 m in his view while a swoop is on.
            if(Flock->IsSwooping(Now))
            {
                float Nearest=1e9f;Run->SwoopTime+=Seen(ERole::Gyre,Nearest)?Dt:0;
                Run->SwoopSeen=FMath::Max(Run->SwoopSeen,Run->Seen.Num());
                if(Now>=Run->Next&&Run->Shots<70){Run->Next=Now+.4;Shot(FString::Printf(TEXT("BirdsSwoop_%02d"),Run->Shots++));}
            }
            if(Time<50)return;
            const int32 Swoops=Flock->GetSwoops()-Run->SwoopsAt;
            const bool Pass=Swoops>=2&&Run->SwoopSeen>=2&&Run->SwoopTime>=4;
            Run->Failures+=!Pass;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_SWOOPS failures=%d swoops=%d birds_in_view=%d on_screen_s=%.1f seconds=%.1f"),!Pass,Swoops,Run->SwoopSeen,Run->SwoopTime,Time);
            Run->bDone=true;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BIRDS_TEST failures=%d"),Run->Failures);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
