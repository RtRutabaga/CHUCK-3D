#include "DockSigns.h"
#include "DockPlaza.h"
#include "DockFire.h"
#include "DockForge.h"
#include "SewerSlide.h"
#include "DockReturn.h"
#include "DockNPC.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

void BuildDockPlaza(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>();
    Owner->Tags.Add(TEXT("WaterdeepPlaza"));
    auto* Root=NewObject<USceneComponent>(Owner);
    Owner->SetRootComponent(Root); Root->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Material,bool Solid=false,UStaticMesh* Mesh=nullptr,FRotator Rot=FRotator::ZeroRotator)
    {
        if(Solid && FCString::Strcmp(Material,TEXT("Plaster"))==0) Material=TEXT("WeatheredPlaster");
        if(!Mesh) Mesh=Cube;
        const FString Key=FString(Material)+Mesh->GetName()+(Solid?TEXT("solid"):TEXT("detail"));
        auto*& Batch=Batches.FindOrAdd(Key);
        if(!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(Owner);
            Batch->SetupAttachment(Root); Batch->SetStaticMesh(Mesh);
            Batch->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Material,Material)));
            Batch->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
            Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rot,P,Size/100.f));
    };
    auto Beam=[&](FVector A,FVector B,float Width,const TCHAR* Mat)
    { const FVector D=B-A; Shape((A+B)*.5f,FVector(D.Size(),Width,Width),Mat,false,nullptr,D.Rotation()); };
    auto Sign=[&](FVector P,const TCHAR* Words,float Yaw=90.f)
    { AddDockTradeSign(World,P,Words,Yaw,190.f); };
    TArray<TWeakObjectPtr<UPointLightComponent>> Lights;
    auto Glow=[&](FVector P,float Intensity,float Radius)
    {
        auto* L=NewObject<UPointLightComponent>(Owner); L->SetupAttachment(Root);
        L->SetRelativeLocation(P); L->SetMobility(EComponentMobility::Movable);
        L->SetIntensity(Intensity); L->SetAttenuationRadius(Radius);
        L->SetLightColor(FLinearColor(1.f,.43f,.12f)); L->SetCastShadows(false); L->RegisterComponent();
        Lights.Add(L);
    };
    auto Lamp=[&](FVector P)
    {
        Shape(P+FVector(0,0,145),FVector(12,12,290),TEXT("Dark"),true);
        Shape(P+FVector(0,0,286),FVector(43,43,8),TEXT("Dark"));
        Shape(P+FVector(0,0,292),FVector(9,9,4),TEXT("Dark"),false,Cylinder);
        AddDockFlame(Owner,P+FVector(0,0,293),20,35);
        for(float X : {-17.f,17.f}) for(float Y : {-17.f,17.f})
            Shape(P+FVector(X,Y,310),FVector(4,4,42),TEXT("Dark"));
        Shape(P+FVector(0,0,335),FVector(48,48,10),TEXT("Dark"));
        Glow(P+FVector(0,0,309),1800,430);
    };
    auto Torch=[&](FVector P)
    {
        Beam(P-FVector(0,18,65),P,9,TEXT("Wood"));
        Shape(P+FVector(0,0,6),FVector(24,24,19),TEXT("Dark"),false,Cylinder);
        AddDockFlame(Owner,P+FVector(0,0,15),26,43);
        Glow(P+FVector(0,0,35),1500,390);
    };

    // Connected plaza apron replaces the obsolete railed narrow walkway and
    // fills the tiny enclosed water pocket beside the former boat placement.
    Shape(FVector(200,-3325,-55),FVector(3200,2150,110),TEXT("Stone"),true);
    Shape(FVector(200,-2075,-55),FVector(1400,550,110),TEXT("Stone"),true);
    // Retain the kerb only along the actual waterside edge.
    Shape(FVector(894,-2070,22),FVector(12,530,44),TEXT("Stone"),true);
    // Keep the plaza approach level: the former crosswise 48 cm lip is removed.
    // Broad, ground-level ring around the fountain; visual courses never alter navigation.
    const FVector F(260,-3320,0);
    Shape(F+FVector(0,0,.7f),FVector(930,930,1),TEXT("WoodLight"),false,Cylinder);
    Shape(F+FVector(0,0,1.4f),FVector(870,870,1),TEXT("Stone"),false,Cylinder);
    Shape(F+FVector(0,0,9),FVector(450,450,18),TEXT("Stone"),true,Cylinder);
    for(int32 I=0;I<32;++I)
    {
        const float A=I*2*PI/32;
        Shape(F+FVector(209*FMath::Cos(A),209*FMath::Sin(A),43),FVector(43,25,64),TEXT("Stone"),true,nullptr,FRotator(0,FMath::RadiansToDegrees(A)+90,0));
    }
    // Single surface disks avoid double refraction through the old closed cylinders.
    auto Basin=[&](float Radius,float Z)
    {
        auto* Surface=NewObject<UProceduralMeshComponent>(Owner);
        Surface->SetupAttachment(Root); Surface->RegisterComponent();
        TArray<FVector> V,N; TArray<int32> Tri; TArray<FVector2D> UV;
        TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
        V.Add(F+FVector(0,0,Z));
        for(int32 I=0;I<=64;++I)
        {
            const float A=I*2*PI/64;
            V.Add(F+FVector(Radius*FMath::Cos(A),Radius*FMath::Sin(A),Z));
            if(I<64) Tri.Append({0,I+1,I+2});
        }
        for(const FVector& P : V)
        { N.Add(FVector::UpVector); UV.Add(FVector2D(P.X,P.Y)/100.f); Tangents.Add(FProcMeshTangent(1,0,0)); }
        Surface->CreateMeshSection_LinearColor(0,V,Tri,N,UV,Colors,Tangents,false);
        Surface->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_FountainBasinWater.M_FountainBasinWater")));
        Surface->SetCollisionProfileName(TEXT("NoCollision")); Surface->SetCastShadow(false);
        return Surface;
    };
    auto* LowerBasin=Basin(196,53);
    Shape(F+FVector(0,0,87),FVector(54,54,160),TEXT("Stone"),true,Cylinder);
    Shape(F+FVector(0,0,167),FVector(153,153,18),TEXT("Stone"),true,Cylinder);
    auto* UpperBasin=Basin(68.5f,178);
    // Four small falling jets, deliberately restrained rather than a magical effect.
    for(int32 I=0;I<4;++I)
    {
        const float A=I*PI*.5f;
        FVector Previous=F+FVector(62*FMath::Cos(A),62*FMath::Sin(A),179);
        for(int32 S=1;S<=12;++S)
        {
            const float T=S/12.f;
            const float R=62+92*T;
            const FVector P=F+FVector(R*FMath::Cos(A),R*FMath::Sin(A),179+75*T-200*T*T);
            Beam(Previous,P,1.4f,TEXT("FountainJetWater")); Previous=P;
        }
    }
    // Small moving beads make the jets read as falling water instead of solid rails.
    auto* Drops=NewObject<UInstancedStaticMeshComponent>(Owner);
    Drops->SetupAttachment(Root); Drops->SetStaticMesh(Sphere);
    Drops->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_FountainJetWater.M_FountainJetWater")));
    Drops->SetCollisionProfileName(TEXT("NoCollision")); Drops->SetCastShadow(false); Drops->RegisterComponent();
    // Four impacts, twelve staggered droplets each; tiny ballistic splashes, no fog cloud.
    for(int32 I=0;I<72;++I) Drops->AddInstance(FTransform(FQuat::Identity,F+FVector(0,0,170),FVector(.027f,.027f,.045f)));
    FTimerHandle WaterMotion;
    World->GetTimerManager().SetTimer(WaterMotion,[Weak=TWeakObjectPtr<UInstancedStaticMeshComponent>(Drops),World,F]()
    {
        if(!Weak.IsValid()) return;
        for(int32 I=0;I<24;++I)
        {
            const float T=FMath::Frac(World->GetTimeSeconds()*.72f+(I%6)/6.f);
            const float A=(I/6)*PI*.5f, R=62+92*T;
            const FVector P=F+FVector(R*FMath::Cos(A),R*FMath::Sin(A),179+75*T-200*T*T);
            Weak->UpdateInstanceTransform(I,FTransform(FQuat::Identity,P,FVector(.027f,.027f,.045f)),false,false,true);
        }
        for(int32 I=24;I<72;++I)
        {
            const int32 N=I-24;
            const float T=FMath::Frac(World->GetTimeSeconds()*1.8f+(N%12)/12.f);
            const float A=(N/12)*PI*.5f, Spread=N*2.399963f;
            const FVector Landing=F+FVector(154*FMath::Cos(A),154*FMath::Sin(A),54);
            const float Reach=11+(N%5)*2, Height=10+(N%4)*3;
            const FVector P=Landing+FVector(Reach*T*FMath::Cos(Spread),Reach*T*FMath::Sin(Spread),4*Height*T*(1-T));
            const float Size=.008f+.004f*(1-T);
            Weak->UpdateInstanceTransform(I,FTransform(FQuat::Identity,P,FVector(Size,Size,Size*1.5f)),false,I==71,true);
        }
    },1.f/30.f,true);
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failures=0;
        for(const TCHAR* Name : {TEXT("FountainBasinWater"),TEXT("FountainJetWater"),TEXT("HarborWater")})
            if(!LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Name,Name))) ++Failures;
        for(const FString Key : {TEXT("FountainJetWaterCubedetail")})
        {
            auto* B=Batches.FindRef(Key);
            if(!B || !B->GetMaterial(0) || B->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++Failures;
        }
        for(auto* B : {LowerBasin,UpperBasin})
            if(!B->GetMaterial(0) || B->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++Failures;
        if(Drops->GetInstanceCount()!=72 || Drops->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SURFACE_WATER_CHECK failures=%d materials=3 jet_drops=24 splash_drops=48"),Failures);
    }
    // Coping and plinth courses retain the existing basin collision.
    for(int32 I=0;I<32;++I)
    {
        const float A=I*2*PI/32;
        for(float Z : {17.f,77.f}) Shape(F+FVector(209*FMath::Cos(A),209*FMath::Sin(A),Z),FVector(43,32,6),TEXT("Stone"),false,nullptr,FRotator(0,FMath::RadiansToDegrees(A)+90,0));
    }
    // Enclosing walls and a closed district gate, loosely echoing the 2D plaza.
    Shape(FVector(200,-4380,230),FVector(3200,70,460),TEXT("Stone"),true);
    for(float X : {-1380.f,1780.f}) Shape(FVector(X,-3330,230),FVector(70,2100,460),TEXT("Stone"),true);
    for(float X=-1360;X<=1780;X+=140) Shape(FVector(X,-4380,485),FVector(78,88,65),TEXT("Stone"),true);
    for(float Y=-4300;Y<=-2300;Y+=140) for(float X : {-1380.f,1780.f})
        Shape(FVector(X,Y,485),FVector(88,78,65),TEXT("Stone"),true);
    // Join the older district boundary to the plaza's western corner.
    // This short return sits outside both ground-level plaza approaches.
    Shape(FVector(-1585,-2300,230),FVector(480,70,460),TEXT("Stone"),true);
    Shape(FVector(-1585,-2300,452),FVector(488,88,16),TEXT("Stone"));
    for(float X=-1770;X<=-1370;X+=140)
        Shape(FVector(X,-2300,485),FVector(78,88,65),TEXT("Stone"),true);
    Torch(FVector(-1585,-2355,250));
    for(float X : {-60.f,580.f})
    {
        Shape(FVector(X,-4270,330),FVector(240,260,660),TEXT("Stone"),true);
        Shape(FVector(X,-4270,670),FVector(270,290,35),TEXT("Dark"));
        for(float DX : {-90.f,90.f}) for(float DY : {-100.f,100.f})
            Shape(FVector(X+DX,-4270+DY,720),FVector(70,70,80),TEXT("Stone"),true);
    }
    Shape(FVector(260,-4336,205),FVector(395,16,410),TEXT("Dark"),true);
    for(float X=84;X<=440;X+=28) Shape(FVector(X,-4317,205),FVector(11,12,410),TEXT("Wood"));
    for(float Z : {60.f,160.f,270.f,375.f}) Shape(FVector(260,-4309,Z),FVector(385,14,10),TEXT("Dark"));
    // Iron gate straps and worn hanging standards break up the large tower faces.
    for(float X : {-60.f,580.f})
    {
        Shape(FVector(X,-4134,453),FVector(100,6,184),TEXT("Dark"));
        for(float DX : {-48.f,48.f}) Shape(FVector(X+DX,-4129,453),FVector(5,4,184),TEXT("WoodLight"));
        Shape(FVector(X,-4127,480),FVector(25,4,5),TEXT("WoodLight"));
        Shape(FVector(X,-4127,468),FVector(5,4,46),TEXT("WoodLight"));
        Shape(FVector(X,-4127,498),FVector(17,17,3),TEXT("WoodLight"),false,Cylinder,FRotator(0,0,90));
        Shape(FVector(X,-4124,498),FVector(9,9,3),TEXT("Dark"),false,Cylinder,FRotator(0,0,90));
        for(float Side : {-1.f,1.f})
        {
            Beam(FVector(X,-4127,445),FVector(X+Side*22,-4127,459),5,TEXT("WoodLight"));
            Beam(FVector(X+Side*22,-4127,459),FVector(X+Side*22,-4127,468),5,TEXT("WoodLight"));
        }
        Beam(FVector(X-63,-4130,551),FVector(X+63,-4130,551),7,TEXT("Dark"));
    }
    for(float X : {100.f,420.f}) for(float Z : {60.f,160.f,270.f,375.f})
        Shape(FVector(X,-4298,Z),FVector(7,5,7),TEXT("WoodLight"),false,Sphere);
    // Small barred sewer arch set into the east wall: solid backing, no transition/interaction.
    Shape(FVector(1737,-3450,122),FVector(24,252,244),TEXT("Dark"),true);
    for(float Y : {-3596.f,-3304.f}) Shape(FVector(1720,Y,118),FVector(125,52,236),TEXT("Stone"),true);
    for(int32 I=0;I<=10;++I)
    {
        const float A=I*PI/10;
        Shape(FVector(1720,-3450+142*FMath::Cos(A),202+110*FMath::Sin(A)),FVector(125,50,50),TEXT("Stone"),true,nullptr,FRotator(0,0,FMath::RadiansToDegrees(A)));
    }
    for(float Y=-3555;Y<=-3345;Y+=30) Shape(FVector(1715,Y,112),FVector(12,10,224),TEXT("Dark"));
    for(float Z : {65.f,145.f,220.f}) Shape(FVector(1708,-3450,Z),FVector(14,225,9),TEXT("Wood"));
    Shape(FVector(1682,-3450,5),FVector(125,260,10),TEXT("Stone"),true);
    // Closed-gate latch and runoff grating: visual only, no prompt or unlock state.
    Shape(FVector(1695,-3450,116),FVector(13,56,13),TEXT("Dark"));
    Shape(FVector(1686,-3450,101),FVector(10,20,27),TEXT("Dark"));
    Shape(FVector(1650,-3450,10.2f),FVector(80,170,.4f),TEXT("Dark"));
    for(float Y=-3520;Y<=-3380;Y+=14) Shape(FVector(1650,Y,11),FVector(78,3,2),TEXT("WoodLight"));
    // Shops remain exterior-only: smithy west, alchemist east of the closed gate.
    auto Shop=[&](FVector P,const TCHAR* Name)
    {
        Shape(P+FVector(0,0,235),FVector(590,360,470),TEXT("Plaster"),true);
        for(float X : {-286.f,0.f,286.f}) Shape(P+FVector(X,184,230),FVector(14,12,460),TEXT("Wood"));
        Shape(P+FVector(0,187,112),FVector(115,12,224),TEXT("Wood"));
        for(float X : {-180.f,180.f})
        {
            Shape(P+FVector(X,187,193),FVector(95,10,115),TEXT("Dark"));
            Shape(P+FVector(X,194,193),FVector(5,5,115),TEXT("WoodLight"));
            Shape(P+FVector(X,194,193),FVector(95,5,5),TEXT("WoodLight"));
            Shape(P+FVector(X,195,133),FVector(113,35,12),TEXT("Stone"));
        }
        for(float S : {-1.f,1.f}) Shape(P+FVector(0,S*90,520),FVector(640,221,16),TEXT("Roof"),true,nullptr,FRotator(0,0,S*29));
        for(int32 I=0;I<12;++I)
        {
            const float Y=-165+I*30.f, H=FMath::Max(5.f,100-FMath::Abs(Y)*.555f);
            for(float X : {-294.f,294.f}) Shape(P+FVector(X,Y,470+H*.5f),FVector(8,30,H),TEXT("Wood"));
        }
        Shape(P+FVector(0,0,574),FVector(648,16,12),TEXT("Dark"));
        for(float X : {-180.f,180.f})
        {
            Shape(P+FVector(X,213,261),FVector(111,61,8),TEXT("Roof"),false,nullptr,FRotator(0,0,-8));
            for(float DX : {-45.f,45.f}) Beam(P+FVector(X+DX,186,225),P+FVector(X+DX,237,259),5,TEXT("Wood"));
        }
        for(float X=-45;X<=45;X+=15) Shape(P+FVector(X,195,111),FVector(13,3,211),TEXT("WoodLight"));
        for(float Z : {40.f,180.f}) Shape(P+FVector(0,199,Z),FVector(104,5,8),TEXT("Dark"));
        Shape(P+FVector(39,201,102),FVector(6,8,16),TEXT("Dark"));
        // Old but tended shopfronts: repaired corners, shutters and timber knees.
        for(float X : {-270.f,270.f})
        {
            for(int32 I=0;I<5;++I)
                Shape(P+FVector(X+(X>0?-1:1)*(I%2)*12,187,28+I*25),FVector(43-I*5,4,23),TEXT("Stone"));
            Beam(P+FVector(X,191,310),P+FVector(X+(X>0?-80:80),191,400),9,TEXT("Wood"));
        }
        for(float X : {-180.f,180.f}) for(float S : {-1.f,1.f})
        {
            Shape(P+FVector(X+S*60,199,193),FVector(27,7,112),TEXT("Wood"),false,nullptr,FRotator(0,0,S*3));
            for(float Z : {158.f,226.f}) Shape(P+FVector(X+S*60,204,Z),FVector(27,4,6),TEXT("Dark"));
        }
        for(int32 I=0;I<4;++I)
            Shape(P+FVector(-245+I*38,185,17),FVector(33,11,30),TEXT("Stone"));
        Shape(P+FVector(-215,-65,545),FVector(62,62,180),TEXT("Stone"));
        Sign(P+FVector(0,204,301),Name);
    };
    Shop(FVector(-865,-3940,0),TEXT("SMITHY"));
    Shop(FVector(1280,-3940,0),TEXT("ALCHEMIST"));
    // Dry fuel stacked against the smithy, and herb bundles beneath shop eaves.
    for(int32 Row=0;Row<3;++Row) for(int32 Col=0;Col<5;++Col)
        Shape(FVector(-1140+Col*18,-3742,10+Row*17),FVector(16,54,15),((Row+Col)%3)?TEXT("Wood"):TEXT("WoodLight"),false,Cylinder,FRotator(90,0,0));
    for(int32 I=0;I<5;++I)
    {
        const FVector P(1070+I*45,-3725,255);
        Beam(P+FVector(0,0,34),P,1,TEXT("Wood"));
        for(float S : {-1.f,0.f,1.f}) Beam(P+FVector(S*5,0,0),P+FVector(S*11,3,-28),3,TEXT("WoodLight"));
    }
    // Working forge dressing preserves Claude's smith, anvil and sound placement.
    BuildDockForge(Owner);
    for(int32 I=0;I<7;++I)
        Shape(FVector(1100+I*40,-3729,147),FVector(15,15,26+(I%3)*9),I%2?TEXT("FountainWater"):TEXT("Amber"),false,Cylinder);
    // Market stands and benches around the clear central loop.
    for(const FVector P : {FVector(-950,-2700,0),FVector(1280,-2670,0)})
    {
        Shape(P+FVector(0,0,82),FVector(230,105,12),TEXT("WoodLight"),true);
        for(float X : {-108.f,108.f}) for(float Y : {-45.f,45.f})
            Shape(P+FVector(X,Y,140),FVector(10,10,280),TEXT("Wood"),true);
        for(int32 I=0;I<10;++I) Shape(P+FVector(-112+I*25,0,288),FVector(25,155,10),I%2?TEXT("Roof"):TEXT("Plaster"),false,nullptr,FRotator(10,0,0));
        // Stitched canvas patches and older replacement counter boards.
        for(float X : {-55.f,65.f})
        {
            Shape(P+FVector(X,15,295),FVector(39,46,1),TEXT("WoodLight"),false,nullptr,FRotator(10,0,0));
            for(float DX : {-17.f,17.f}) for(float Y=-3;Y<=31;Y+=9)
                Shape(P+FVector(X+DX,Y,295+Y*.176f),FVector(3,1,1),TEXT("Dark"));
        }
        for(int32 I=0;I<8;++I) Shape(P+FVector(-105+I*30,0,89),FVector(27,103,1),I==2?TEXT("Wood"):TEXT("WoodLight"));
        for(float X : {-70.f,0.f,70.f}) Shape(P+FVector(X,0,105),FVector(54,60,35),TEXT("Wood"),true);
    }
    // The east bench; the west one moved to the bay side, turned to the fountain, with the old elf on it (ADockNPC::ElfBench, user 2026-10-06).
    for(const FVector B : {FVector(960,-3350,0),FVector(ADockNPC::ElfBench.X,ADockNPC::ElfBench.Y,0)})
    {
        const FRotator R(0,B.X==960 ? 0.f : ADockNPC::ElfYaw+90.f,0);   // the bench's length runs across the way she faces
        Shape(B+R.RotateVector(FVector(0,0,44)),FVector(185,50,10),TEXT("WoodLight"),true,nullptr,R);
        for(float DX : {-65.f,65.f}) Shape(B+R.RotateVector(FVector(DX,0,20)),FVector(14,42,40),TEXT("Dark"),true,nullptr,R);
    }
    // Ruin fragment off the main loop recalls the docks' old broken enclosure.
    Shape(FVector(-1080,-3180,42),FVector(180,34,84),TEXT("Stone"),true);
    Shape(FVector(-1160,-3080,75),FVector(35,220,150),TEXT("Stone"),true);
    for(int32 I=0;I<4;++I) Shape(FVector(-1090+I*42,-3070+(I%2)*44,15),FVector(38,34,30),TEXT("Stone"),true,nullptr,FRotator(0,I*19,0));
    for(const FVector P : {FVector(-330,-2780,0),FVector(830,-2780,0),FVector(-330,-3780,0),FVector(830,-3780,0),FVector(530,-2230,0)}) Lamp(P);
    for(float X : {-1180.f,-420.f,900.f,1530.f}) Torch(FVector(X,-4320,250));
    for(float Y : {-2700.f,-3150.f,-3650.f}) Torch(FVector(-1325,Y,250));
    Torch(FVector(1675,-3255,235)); Torch(FVector(1675,-3645,235));
    // Light the older dock-street lanterns too; their geometry stays unchanged.
    for(const FVector P : {FVector(-1175,-1530,251),FVector(-1175,-760,251),FVector(-1175,40,251),FVector(440,-1280,251)})
    { AddDockFlame(Owner,P-FVector(0,0,11),15,25); Glow(P,1400,360); }
    Torch(FVector(-460,-230,225)); Torch(FVector(170,308,235));
    FTimerHandle Flicker;
    World->GetTimerManager().SetTimer(Flicker,[Lights,World]()
    {
        const float T=World->GetTimeSeconds();
        for(int32 I=0;I<Lights.Num();++I) if(Lights[I].IsValid())
            Lights[I]->SetLightColor(FLinearColor(1.f,.42f+.025f*FMath::Sin(T*3.7f+I),.12f));
    },.12f,true);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failures=0;
        const FVector Route[]={FVector(180,-1730,0),FVector(180,-2440,0),FVector(600,-1730,0),
            FVector(600,-2440,0),FVector(600,-2780,0),FVector(260,-2780,0),FVector(-180,-2980,0),
            FVector(-180,-3650,0),FVector(630,-3650,0),FVector(630,-2980,0),FVector(1480,-3450,0),
            FVector(-350,-2050,0),FVector(0,-2050,0),FVector(780,-2050,0)};
        for(const FVector P : Route)
        {
            FHitResult H;
            if(!World->LineTraceSingleByChannel(H,P+FVector(0,0,60),P-FVector(0,0,80),ECC_Visibility) || FMath::Abs(H.ImpactPoint.Z)>3) ++Failures;
        }
        for(const FIntPoint Leg : {FIntPoint(0,1),FIntPoint(2,3),FIntPoint(3,4),FIntPoint(4,5),FIntPoint(5,6),FIntPoint(6,7),FIntPoint(7,8),FIntPoint(8,9),FIntPoint(11,12),FIntPoint(12,13),FIntPoint(1,12)})
        {
            FHitResult H;
            if(World->SweepSingleByChannel(H,Route[Leg.X]+FVector(0,0,35),Route[Leg.Y]+FVector(0,0,35),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failures;
        }
        FHitResult Gate;
        const bool Closed=World->LineTraceSingleByChannel(Gate,FVector(1500,-3450,80),FVector(1900,-3450,80),ECC_Visibility);
        if(!Closed) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PLAZA_CHECK failures=%d floor_samples=14 capsule_routes=11 sewer_closed=%d"),Failures,Closed);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckWaterCapture")))
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("ChuckWaterNight")))
        {
            // Sewer construction resets its state later in BeginPlay; defer the
            // review-only evening transition until all scene builders finish.
            FTimerHandle Night,Check;
            World->GetTimerManager().SetTimer(Night,[](){MarkDockSewerExited();},2.f,false);
            World->GetTimerManager().SetTimer(Check,[World](){CheckDockReturn(World,true);},3.f,false);
        }
        // Each repeated view is four seconds apart, exposing actual wave/splash motion.
        const FVector Views[]={FVector(-80,-3030,130),FVector(-80,-3030,130),FVector(260,-3290,440),FVector(260,-3290,440),FVector(640,-160,125),FVector(640,-160,125),FVector(750,-1250,860),FVector(750,-1250,860)};
        const FVector Targets[]={FVector(260,-3320,85),FVector(260,-3320,85),FVector(260,-3320,40),FVector(260,-3320,40),FVector(1400,650,-60),FVector(1400,650,-60),FVector(2700,2000,-60),FVector(2700,2000,-60)};
        auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(65);
        for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)
        {
            FTimerHandle H;
            World->GetTimerManager().SetTimer(H,[World,Camera,P=Views[I],T=Targets[I]]()
            { Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera); },4.f+I*4,false);
            FTimerHandle S;
            World->GetTimerManager().SetTimer(S,[I]()
            { const FString D=FPaths::ScreenShotDir()/TEXT("SurfaceWater"); IFileManager::Get().MakeDirectory(*D,true); FScreenshotRequest::RequestScreenshot(D/FString::Printf(TEXT("View%d.png"),I),false,false); },6.f+I*4,false);
        }
        FTimerHandle Quit;
        World->GetTimerManager().SetTimer(Quit,[World]() { if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); },38.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPlazaCapture")))
    {
        const FVector Views[]={FVector(250,-2550,1900),FVector(-170,-2800,100),FVector(1130,-3450,95),FVector(300,-3730,100),FVector(-240,-180,110),FVector(-850,-1700,650),FVector(300,-3800,900)};
        const FVector Targets[]={FVector(260,-3350,40),FVector(260,-3320,150),FVector(1740,-3450,175),FVector(260,-4340,290),FVector(90,200,135),FVector(-1450,-2350,180),FVector(260,-6200,700)};
        auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(75);
        for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)
        {
            FTimerHandle H;
            World->GetTimerManager().SetTimer(H,[World,Camera,P=Views[I],T=Targets[I]]()
            { Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera); },4.f+I*4,false);
            FTimerHandle S;
            World->GetTimerManager().SetTimer(S,[I]()
            { const FString D=FPaths::ScreenShotDir()/TEXT("Plaza"); IFileManager::Get().MakeDirectory(*D,true); FScreenshotRequest::RequestScreenshot(D/FString::Printf(TEXT("View%d.png"),I),false,false); },6.f+I*4,false);
        }
        FTimerHandle Quit;
        World->GetTimerManager().SetTimer(Quit,[World]() { if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); },34.f,false);
    }
}
