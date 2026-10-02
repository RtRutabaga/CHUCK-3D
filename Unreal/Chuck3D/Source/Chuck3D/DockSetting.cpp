#include "DockSetting.h"
#include "DockVista.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/OutputDeviceNull.h"

void BuildDockSetting(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>();
    Owner->Tags.Add(TEXT("DockSetting"));
    auto* Root=NewObject<USceneComponent>(Owner);
    Owner->SetRootComponent(Root);
    Root->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Box=[&](FVector P,FVector Size,const TCHAR* Surface,bool Collision=false,FRotator Rotation=FRotator::ZeroRotator,bool Visible=true)
    {
        const FString Key=FString(Surface)+(Collision?TEXT("_solid"):TEXT("_detail"))+(Visible?TEXT(""):TEXT("_hidden"));
        auto*& Batch=Batches.FindOrAdd(Key);
        if(!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(Owner);
            Batch->SetupAttachment(Root);
            Batch->SetStaticMesh(Cube);
            auto* Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Surface,Surface));
            if(!Mat) Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Surface,Surface));
            Batch->SetMaterial(0,Mat);
            Batch->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
            Batch->SetVisibility(Visible);
            Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
    };
    auto Beam=[&](FVector A,FVector B,float Width,const TCHAR* Material)
    {
        const FVector Delta=B-A;
        Box((A+B)*.5f,FVector(Delta.Size(),Width,Width),Material,false,Delta.Rotation());
    };
    auto Prop=[&](UStaticMesh* Mesh,FVector P,FVector Scale=FVector::OneVector,float Yaw=0.f)
    {
        if(!Mesh) return;
        auto* Component=NewObject<UStaticMeshComponent>(Owner);
        Component->SetupAttachment(Root);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeTransform(FTransform(FRotator(0,Yaw,0),P,Scale));
        Component->SetCollisionProfileName(TEXT("NoCollision"));
        Component->RegisterComponent();
    };
    auto* CrateMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* RopeMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_RopeCoil.SM_RopeCoil"));
    auto Label=[&](FVector P,const TCHAR* Words,float Yaw=0.f)
    {
        auto* Text=NewObject<UTextRenderComponent>(Owner);
        Text->SetupAttachment(Root);
        Text->SetRelativeLocation(P);
        Text->SetRelativeRotation(FRotator(0,Yaw,0));
        Text->SetWorldSize(16);
        Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetTextRenderColor(FColor(205,191,154));
        Text->SetText(FText::FromString(Words));
        Text->RegisterComponent();
    };
    // Continuous ground, not isolated platforms. All new route surfaces meet z=0.
    Box(FVector(-1150,-700,-45),FVector(1300,3200,90),TEXT("Stone"),true); // west dock street
    Box(FVector(-150,750,-45),FVector(700,700,90),TEXT("Stone"),true); // tavern court
    Box(FVector(600,-1400,-45),FVector(600,800,90),TEXT("Stone"),true); // market service quay
    // Full depth to the two old frontage shells; leave their test-facing walls intact.
    Box(FVector(-60,530,155),FVector(560,270,310),TEXT("Plaster"),true);
    Box(FVector(-600,50,160),FVector(200,600,320),TEXT("Stone"),true);

    auto House=[&](FVector P,FVector Size,const TCHAR* Surface,bool Solid=true)
    {
        Box(P+FVector(0,0,Size.Z*.5f),Size,Surface,Solid);
        Box(P+FVector(0,0,24),FVector(Size.X+8,Size.Y+8,48),TEXT("Stone"),false);
        for(float X : {-Size.X*.5f,Size.X*.5f}) for(float Y : {-Size.Y*.5f,Size.Y*.5f})
            Box(P+FVector(X,Y,Size.Z*.5f),FVector(12,12,Size.Z),TEXT("Wood"));
        for(float Z=210; Z<Size.Z; Z+=230)
            Box(P+FVector(0,0,Z),FVector(Size.X+12,Size.Y+12,10),TEXT("Wood"));
        // Two pitched roof slopes with boarded gable ends, no floating flat lids.
        const float Rise=Size.X*.288675f;
        for(float Side : {-1.f,1.f})
            Box(P+FVector(Side*Size.X*.25f,0,Size.Z+Rise*.5f),
                FVector(Size.X*.57735f+24,Size.Y+40,12),TEXT("Roof"),Solid,FRotator(-Side*30,0,0));
        for(int32 Plank=0;Plank<12;++Plank)
        {
            const float X=-Size.X*.5f+(Plank+.5f)*Size.X/12;
            const float H=FMath::Max(4.f,Rise-FMath::Abs(X)*.57735f);
            for(float Side : {-1.f,1.f})
                Box(P+FVector(X,Side*Size.Y*.5f,Size.Z+H*.5f),FVector(Size.X/12,8,H),TEXT("Wood"));
        }
        Box(P+FVector(0,0,Size.Z+Rise),FVector(14,Size.Y+48,14),TEXT("Dark"));
        Box(P+FVector(-Size.X*.25f,Size.Y*.2f,Size.Z+Rise*.65f),FVector(45,48,150),TEXT("Stone"));
        // Closed doors and shuttered windows on both street-facing sides.
        for(float Side : {-1.f,1.f})
        {
            Box(P+FVector(Side*(Size.X*.5f+2),0,103),FVector(5,95,206),TEXT("Wood"));
            for(float Z=155; Z<Size.Z-25; Z+=220) for(float Y : {-Size.Y*.3f,Size.Y*.3f})
            {
                Box(P+FVector(Side*(Size.X*.5f+3),Y,Z),FVector(6,66,90),TEXT("Wood"));
                Box(P+FVector(Side*(Size.X*.5f+7),Y,Z),FVector(3,42,68),TEXT("Dark"));
                Box(P+FVector(Side*(Size.X*.5f+10),Y,Z-46),FVector(22,80,8),TEXT("Stone"));
                Box(P+FVector(Side*(Size.X*.5f+9),Y,Z),FVector(3,4,70),TEXT("WoodLight"));
            }
            // Gable facades need human-scale openings too, including the quay store.
            Box(P+FVector(0,Side*(Size.Y*.5f+3),104),FVector(104,6,208),TEXT("Wood"));
            for(float X : {-Size.X*.28f,Size.X*.28f}) for(float Z=155;Z<Size.Z-25;Z+=220)
            {
                Box(P+FVector(X,Side*(Size.Y*.5f+3),Z),FVector(66,6,90),TEXT("Wood"));
                Box(P+FVector(X,Side*(Size.Y*.5f+7),Z),FVector(42,3,68),TEXT("Dark"));
                Box(P+FVector(X,Side*(Size.Y*.5f+9),Z-46),FVector(80,22,8),TEXT("Stone"));
            }
        }
    };
    House(FVector(-1550,-1800,0),FVector(360,440,520),TEXT("Plaster"));
    House(FVector(-1530,-1080,0),FVector(400,470,450),TEXT("Stone"));
    House(FVector(-1510,-360,0),FVector(420,450,560),TEXT("Plaster"));
    House(FVector(-1490,430,0),FVector(440,480,490),TEXT("Stone"));
    House(FVector(-780,-1880,0),FVector(340,350,370),TEXT("Plaster"));
    House(FVector(-780,720,0),FVector(340,340,500),TEXT("Plaster"));
    House(FVector(600,-1550,0),FVector(340,320,400),TEXT("Stone"));
    // Tavern gets its back wall/roof, but the familiar front and starting route stay.
    for(float Side : {-1.f,1.f})
        Box(FVector(-60,530+Side*70,346),FVector(590,167,12),TEXT("Roof"),false,FRotator(0,0,Side*25));

    // Loading-door surrounds, cargo battens and hoists: dress existing obstacles
    // without adding collision or changing their ledges, roof gaps or chimney widths.
    Box(FVector(-180,-903,105),FVector(112,6,210),TEXT("Wood"));
    for(float X : {-228.f,-132.f}) Box(FVector(X,-908,105),FVector(8,8,210),TEXT("Dark"));
    Beam(FVector(-180,-835,280),FVector(-180,-945,280),10,TEXT("Wood"));
    Beam(FVector(-180,-940,278),FVector(-180,-940,155),2,TEXT("Dark"));
    Label(FVector(-180,-912,220),TEXT("BONDED STORES"),-90);
    // Row houses are low workshops with working flat roofs; do not cap the jump route.
    for(int32 I=0;I<3;++I)
    {
        const float Y=-1150-I*250.f;
        Box(FVector(-254,Y,136),FVector(6,130,26),TEXT("Wood"));
        Label(FVector(-249,Y,136),I==0?TEXT("CHANDLER"):I==1?TEXT("SAIL REPAIR"):TEXT("COOPER"));
        for(float Offset : {-60.f,60.f})
            Box(FVector(-256,Y+Offset,55),FVector(4,6,110),TEXT("Dark"));
    }
    // Visually bind the timber chimney stacks as lumber bundles. No top clutter.
    for(const FVector Stack : {FVector(510,-490,160),FVector(510,-650,200),FVector(510,-830,90)})
        for(float X : {400.f,620.f}) for(float Side : {-1.f,1.f})
            Box(FVector(X,Stack.Y+Side*30.7f,Stack.Z*.5f),FVector(6,1,Stack.Z),TEXT("Dark"));
    Beam(FVector(744,-679,10),FVector(856,-679,235),7,TEXT("WoodLight"));
    Beam(FVector(856,-561,10),FVector(744,-561,235),7,TEXT("WoodLight"));
    Beam(FVector(800,-620,190),FVector(1080,-620,258),8,TEXT("WoodLight"));
    // Cargo handling court west of the crate steps; clear apron to their existing start.
    Box(FVector(-770,-670,25),FVector(230,150,50),TEXT("Wood"),true);
    for(float X : {-840.f,-740.f})
    {
        Box(FVector(X,-680,82),FVector(78,80,64),TEXT("WoodLight"),true,FRotator::ZeroRotator,!CrateMesh);
        Prop(CrateMesh,FVector(X,-680,82),FVector(78.f/60,80.f/65,64.f/60));
    }
    for(float X : {-885.f,-655.f}) Box(FVector(X,-700,200),FVector(14,14,400),TEXT("Wood"),true);
    Box(FVector(-770,-700,400),FVector(280,18,18),TEXT("Wood"));
    Beam(FVector(-880,-700,320),FVector(-795,-700,396),10,TEXT("Wood"));
    Box(FVector(-770,-700,356),FVector(200,10,36),TEXT("Wood"));
    for(float X : {-850.f,-690.f}) Beam(FVector(X,-700,398),FVector(X,-700,372),2,TEXT("Dark"));
    Label(FVector(-770,-707,356),TEXT("CARGO COURT"),-90);
    Label(FVector(-770,-693,356),TEXT("CARGO COURT"),90);
    // A continuous walking loop down Dock Street, around the cooperage and back.
    for(float Y=-2050;Y<=650;Y+=150)
    {
        Box(FVector(-1100,Y,.2f),FVector(18,150,.4f),TEXT("Dark"));
        for(float X : {-1111.f,-1089.f}) Box(FVector(X,Y,.5f),FVector(4,150,1),TEXT("Stone"));
        for(float Offset=-65;Offset<=65;Offset+=13)
            Box(FVector(-1100,Y+Offset,.6f),FVector(18,2,1),TEXT("Wood"));
    }
    for(const FVector P : {FVector(-1220,-1530,0),FVector(-1220,-760,0),FVector(-1220,40,0),FVector(395,-1280,0)})
    {
        Box(P+FVector(0,0,145),FVector(10,10,290),TEXT("Wood"),true);
        Beam(P+FVector(0,0,275),P+FVector(45,0,275),6,TEXT("Dark"));
        Box(P+FVector(45,0,251),FVector(24,24,34),TEXT("Dark"));
        Box(P+FVector(45,0,251),FVector(26,16,22),TEXT("Amber"));
    }
    // Retaining quay faces descend into water, rather than exposing wafer-thin slabs.
    Box(FVector(-1150,-2310,-65),FVector(1320,30,190),TEXT("Stone"),true);
    Box(FVector(915,-1400,-65),FVector(30,800,190),TEXT("Stone"),true);
    for(float Y=-1770;Y<=-1030;Y+=160)
        Box(FVector(882,Y,20),FVector(24,24,40),TEXT("Dark"),true);
    // Grounded city mass beyond closed perimeter blocks. No isolated skyline boxes.
    Box(FVector(-3100,-400,-70),FVector(2600,6200,180),TEXT("Stone"));
    Box(FVector(-450,2550,-45),FVector(2700,3300,90),TEXT("Stone"),true);
    Box(FVector(3800,0,-80),FVector(900,5200,100),TEXT("Stone")); // bank below original distant warehouses
    for(int32 I=0;I<9;++I)
    {
        House(FVector(-2090,-2250+I*520,20),FVector(460,420,520+(I%3)*110),I%2?TEXT("Stone"):TEXT("Plaster"),false);
        House(FVector(-2790,-2470+I*590,40),FVector(580,460,700+(I%4)*80),TEXT("Stone"),false);
    }
    for(int32 I=0;I<4;++I)
        House(FVector(-1450+I*600,1400,0),FVector(440,480,560+(I%2)*130),TEXT("Plaster"),true);
    // The port belongs to a rising city, not a strip of houses on an island.
    // These are scenery beyond the existing closed perimeter, never new traversal.
    Box(FVector(-4500,100,-160),FVector(3800,7800,360),TEXT("Stone"));
    Box(FVector(-4650,250,120),FVector(2300,6800,200),TEXT("Stone"));
    Box(FVector(-5500,300,330),FVector(1300,6100,220),TEXT("Stone"));
    Box(FVector(-2400,3200,-100),FVector(1300,2000,260),TEXT("Stone"));
    for(int32 I=0;I<8;++I)
    {
        const float Y=-2700+I*780.f;
        House(FVector(-3830,Y,220),FVector(520+(I%2)*110,550,720+(I%3)*160),
            I%3?TEXT("Plaster"):TEXT("Stone"),false);
        House(FVector(-5240,Y+160,440),FVector(630,560,780+(I%4)*100),TEXT("Stone"),false);
    }
    for(int32 I=0;I<5;++I)
        House(FVector(-2130+I*680,2830,I==0?30:0),FVector(560,620,660+(I%3)*130),TEXT("Plaster"),I>0);
    // Retaining wall, buttresses and a distant civic roof provide a varied silhouette.
    Box(FVector(-3500,250,160),FVector(45,6800,320),TEXT("Stone"));
    for(float Y=-2920;Y<=3450;Y+=430)
        Box(FVector(-3460,Y,150),FVector(95,65,300),TEXT("Stone"));
    House(FVector(-5480,700,440),FVector(860,1160,1150),TEXT("Stone"),false);
    for(float Y : {130.f,1270.f})
    {
        Box(FVector(-5480,Y,1200),FVector(270,270,1520),TEXT("Stone"));
        Box(FVector(-5480,Y,1970),FVector(310,310,32),TEXT("Dark"));
        for(float X : {-5590.f,-5480.f,-5370.f})
            for(float Offset : {-110.f,110.f})
                Box(FVector(X,Y+Offset,2020),FVector(48,48,75),TEXT("Stone"));
    }
    // Across the basin, connect the old distant frontage to a substantial bank.
    Box(FVector(4660,100,-90),FVector(1650,6500,120),TEXT("Stone"));
    for(int32 I=0;I<9;++I)
        House(FVector(4410,-2600+I*610,-30),FVector(560,510,650+(I%4)*110),
            I%2?TEXT("Plaster"):TEXT("Stone"),false);
    Box(FVector(3400,100,-50),FVector(55,6300,170),TEXT("Stone"));
    // Opposite working waterfront: give the bank depth, loading faces and a
    // sheltered harbor mouth. All scenery stays beyond the playable basin.
    auto* BoatMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_HarborBoat.SM_HarborBoat"));
    Box(FVector(3370,100,42),FVector(115,6320,20),TEXT("Stone"));
    for(float Y=-2870;Y<=3100;Y+=310)
    {
        Box(FVector(3350,Y,-68),FVector(100,75,220),TEXT("Stone"));
        Box(FVector(3292,Y,-54),FVector(22,24,180),TEXT("Wood"));
        for(float Z : {-100.f,-15.f})
            Box(FVector(3278,Y,Z),FVector(8,34,12),TEXT("Dark"));
    }
    for(int32 I=0;I<3;++I)
    {
        const float Y=-2100+I*1700.f;
        // Short timber landing fingers, below rather than across the skyline.
        Box(FVector(3160,Y,-4),FVector(420,180,24),TEXT("Wood"));
        for(float X : {2970.f,3160.f,3350.f}) for(float Side : {-1.f,1.f})
        {
            Box(FVector(X,Y+Side*78,-70),FVector(24,24,230),TEXT("Wood"));
            Box(FVector(X,Y+Side*78,51),FVector(30,30,12),TEXT("Dark"));
        }
        for(float X=2960;X<=3340;X+=24)
            Box(FVector(X,Y,9),FVector(22,174,2),TEXT("WoodLight"));
        Beam(FVector(2965,Y-80,-130),FVector(3345,Y-80,-18),12,TEXT("Wood"));
        Prop(BoatMesh,FVector(2990,Y+240,-60),FVector(.85f),90);
        Prop(RopeMesh,FVector(3100,Y,12),FVector(.8f));
        // Dockside derrick and hanging tackle remain empty, not a floating load.
        Box(FVector(3330,Y,250),FVector(28,28,500),TEXT("Wood"));
        Beam(FVector(3330,Y,430),FVector(3100,Y,510),20,TEXT("Wood"));
        Beam(FVector(3330,Y,240),FVector(3130,Y,495),14,TEXT("WoodLight"));
        Beam(FVector(3100,Y,506),FVector(3100,Y,230),3,TEXT("Dark"));
        Box(FVector(3100,Y,222),FVector(18,12,22),TEXT("Dark"));
    }
    // Articulate existing far warehouses rather than placing new buildings in
    // front of them. These details sit on the west faces of the second row.
    for(int32 I=0;I<9;++I)
    {
        const FVector P(4125,-2600+I*610,0);
        Box(P+FVector(0,0,100),FVector(8,150,220),TEXT("Wood"));
        for(float Y : {-80.f,80.f}) Box(P+FVector(-5,Y,108),FVector(14,12,236),TEXT("Stone"));
        Box(P+FVector(-5,0,228),FVector(16,178,18),TEXT("Stone"));
        for(float Z : {32.f,173.f}) Box(P+FVector(-9,0,Z),FVector(5,146,8),TEXT("Dark"));
        Beam(P+FVector(-11,-65,15),P+FVector(-11,65,210),7,TEXT("WoodLight"));
        Box(P+FVector(-70,0,260),FVector(150,210,12),TEXT("Roof"),false,FRotator(8,0,0));
        for(float Y : {-90.f,90.f}) Beam(P+FVector(-2,Y,160),P+FVector(-125,Y,245),9,TEXT("Wood"));
    }
    BuildCoastalVista(World);
    // Distant banks continue around the inlet. Leave a broad visible shipping
    // channel between the two breakwater heads; no bridge across open water.
    Box(FVector(4680,4650,-140),FVector(2700,3000,240),TEXT("Stone"));
    Box(FVector(720,4620,-140),FVector(1650,1700,240),TEXT("Stone"));
    Box(FVector(1170,3840,-36),FVector(1300,130,190),TEXT("Stone"));
    Box(FVector(3060,3840,-36),FVector(650,130,190),TEXT("Stone"));
    for(float X : {1770.f,2785.f})
    {
        Box(FVector(X,3840,155),FVector(180,180,340),TEXT("Stone"));
        Box(FVector(X,3840,327),FVector(215,215,24),TEXT("Dark"));
        for(float Side : {-1.f,1.f}) for(float Other : {-1.f,1.f})
            Box(FVector(X+Side*70,3840+Other*70,357),FVector(38,38,48),TEXT("Stone"));
    }
    for(int32 I=0;I<4;++I)
        House(FVector(3900+I*420,4350+(I%2)*620,-20),FVector(360,440,540+I*80),
            I%2?TEXT("Stone"):TEXT("Plaster"),false);
    // Street-facing brackets, shutters and canopies distinguish the near buildings.
    const FVector StreetFronts[]={FVector(-1369,-1800,0),FVector(-1329,-1080,0),
        FVector(-1299,-360,0),FVector(-1269,430,0)};
    for(int32 I=0;I<4;++I)
    {
        const FVector P=StreetFronts[I];
        Box(P+FVector(45,0,230),FVector(100,145,8),I%2?TEXT("Wood"):TEXT("Roof"),false,FRotator(-8,0,0));
        for(float Y : {-60.f,60.f})
            Beam(P+FVector(0,Y,160),P+FVector(85,Y,225),6,TEXT("Wood"));
        for(float Y : {-135.f,135.f})
        {
            Box(P+FVector(14,Y,350),FVector(8,28,85),TEXT("WoodLight"),false,FRotator(0,Y>0?25:-25,0));
            Beam(P+FVector(8,Y-20,275),P+FVector(8,Y+20,320),5,TEXT("Wood"));
        }
        Beam(P+FVector(0,115,260),P+FVector(76,115,260),6,TEXT("Dark"));
        Box(P+FVector(65,115,231),FVector(8,64,42),TEXT("Wood"));
        Label(P+FVector(70,115,232),I==0?TEXT("NETS"):I==1?TEXT("STORES"):I==2?TEXT("ROPE"):TEXT("SAILS"));
    }
    // Flat workshop roofs stay usable: only dress faces below existing ledges.
    for(int32 I=0;I<3;++I)
    {
        const float Y=-1150-I*250.f;
        Box(FVector(-245,Y,121),FVector(30,146,6),TEXT("Wood"));
        for(float Offset : {-62.f,62.f})
            Beam(FVector(-259,Y+Offset,84),FVector(-232,Y+Offset,119),4,TEXT("Wood"));
        Box(FVector(-252,Y,48),FVector(10,90,7),TEXT("WoodLight"));
    }
    // Near-field detail: sit on existing faces, with no new collision or roof clutter.
    Prop(RopeMesh,FVector(-735,-680,114),FVector(.65f),15);
    for(float X=-870;X<=-670;X+=25)
        Box(FVector(X,-670,50.4f),FVector(23,146,.8f),TEXT("WoodLight"));
    // Framed doors, boards, hinges and diagonal braces on the bonded warehouse.
    for(float X=-226;X<=-134;X+=13)
        Box(FVector(X,-907,104),FVector(11,2,200),TEXT("WoodLight"));
    Beam(FVector(-226,-911,20),FVector(-184,-911,195),5,TEXT("Wood"));
    Beam(FVector(-176,-911,195),FVector(-134,-911,20),5,TEXT("Wood"));
    for(float Z : {42.f,168.f}) for(float X : {-215.f,-145.f})
        Box(FVector(X,-914,Z),FVector(29,3,5),TEXT("Dark"));
    for(float X : {-187.f,-173.f}) Box(FVector(X,-916,100),FVector(3,4,14),TEXT("Dark"));
    // Warehouse street face: timber bays and high barred windows.
    for(float Y=-230;Y<=330;Y+=140)
    {
        Box(FVector(-702,Y,158),FVector(7,11,312),TEXT("Wood"));
        if(Y<300)
        {
            Box(FVector(-706,Y+65,208),FVector(7,75,90),TEXT("Wood"));
            Box(FVector(-711,Y+65,208),FVector(4,61,74),TEXT("Dark"));
            for(float Offset : {-21.f,0.f,21.f})
                Box(FVector(-715,Y+65+Offset,208),FVector(3,3,76),TEXT("WoodLight"));
            Box(FVector(-716,Y+65,160),FVector(22,87,8),TEXT("Stone"));
        }
    }
    // Nets hung against the solid storehouse wall, not across a walking route.
    for(float Y : {-160.f,100.f}) Box(FVector(-724,Y,99),FVector(7,7,198),TEXT("Wood"));
    Beam(FVector(-724,-160,195),FVector(-724,100,195),7,TEXT("Wood"));
    for(int32 Row=0;Row<10;++Row) for(int32 Col=0;Col<16;++Col)
    {
        const float Y=-153+Col*15.f;
        const float Z=38+Row*15.f;
        const float Sag=8*FMath::Sin(PI*(Col+.5f)/16);
        Beam(FVector(-725,Y,Z-Sag),FVector(-725,Y+15,Z+15-Sag),.65f,TEXT("WoodLight"));
        Beam(FVector(-725,Y,Z+15-Sag),FVector(-725,Y+15,Z-Sag),.65f,TEXT("WoodLight"));
    }
    // The back of the tavern is a building, not an unarticulated wall.
    for(float X : {-290.f,-70.f,150.f})
    {
        Box(FVector(X,668,174),FVector(90,7,114),TEXT("Wood"));
        Box(FVector(X,673,174),FVector(72,4,96),TEXT("Dark"));
        Box(FVector(X,677,174),FVector(5,4,96),TEXT("WoodLight"));
        Box(FVector(X,677,174),FVector(72,4,5),TEXT("WoodLight"));
        Box(FVector(X,680,112),FVector(105,25,8),TEXT("Stone"));
    }
    for(float X : {-330.f,-180.f,-30.f,120.f,210.f})
        Box(FVector(X,668,150),FVector(11,9,300),TEXT("Wood"));
    // Dock Street entrance: the town beyond this labeled wall is now a real
    // playable neighborhood. Keep the western/far city boundary closed.
    // Match the plaza's masonry thickness and battlements while retaining the
    // existing boundary heights and entrance. Length runs along the local X axis.
    auto CityWall=[&](FVector P,float Length,float Height,float Yaw,FVector Inward)
    {
        const FRotator R(0,Yaw,0);
        Box(P+FVector(0,0,Height*.5f),FVector(Length,70,Height),TEXT("Stone"),true,R);
        Box(P+FVector(0,0,Height-8),FVector(Length+8,88,16),TEXT("Stone"),false,R);
        for(float Along=-Length*.5f+50;Along<Length*.5f;Along+=140)
            Box(P+R.RotateVector(FVector(Along,0,Height+25)),FVector(78,88,65),TEXT("Stone"),true,R);
        for(float Along=-Length*.5f+240;Along<Length*.5f-100;Along+=780)
        {
            const FVector T=P+R.RotateVector(FVector(Along,0,0))+Inward*65+FVector(0,0,FMath::Min(250.f,Height-45));
            Beam(T-Inward*25-FVector(0,0,65),T,9,TEXT("Wood"));
            Box(T+FVector(0,0,6),FVector(24,24,19),TEXT("Dark"));
            Box(T+FVector(0,0,26),FVector(12,12,32),TEXT("TorchFlame"));
            Box(T+FVector(3,0,46),FVector(6,7,16),TEXT("TorchFlame"));
            auto* Light=NewObject<UPointLightComponent>(Owner);
            Light->SetupAttachment(Root);
            Light->SetRelativeLocation(T+FVector(0,0,35));
            Light->SetIntensity(1500); Light->SetAttenuationRadius(390);
            Light->SetLightColor(FLinearColor(1,.43f,.12f));
            Light->SetCastShadows(false); Light->RegisterComponent();
        }
    };
    CityWall(FVector(-1790,-700,0),3200,200,90,FVector(1,0,0));
    // Connected floor behind Dock Street, with enough depth for a street loop.
    Box(FVector(-1100,930,-45),FVector(360,100,90),TEXT("Stone"),true);
    // Wall segments leave a 320 cm entrance at the sign, while the next city
    // wall remains solid and non-traversable.
    Box(FVector(-1530,890,100),FVector(540,20,200),TEXT("Stone"),true);
    Box(FVector(-370,890,100),FVector(1140,20,200),TEXT("Stone"),true);
    for(float X : {-1270.f,-930.f})
    {
        Box(FVector(X,890,110),FVector(18,30,220),TEXT("Wood"));
        Box(FVector(X,890,240),FVector(34,34,18),TEXT("Dark"));
        Box(FVector(X,890,330),FVector(44,44,22),TEXT("Stone"));
    }
    Box(FVector(-1100,890,260),FVector(380,20,44),TEXT("Wood"));
    Label(FVector(-1100,875,260),TEXT("DOCK STREET"),-90);
    // A small street-and-court loop makes the newly opened side useful rather
    // than a bare collision slab. Its buildings are solid at human scale.
    Box(FVector(-1160,1260,1),FVector(18,680,2),TEXT("Dark"));
    for(float Y=940;Y<=1600;Y+=110)
        Box(FVector(-1160,Y,2),FVector(18,2,2),TEXT("Wood"));
    Box(FVector(-1150,1940,1),FVector(1320,18,2),TEXT("Dark"));
    for(float X=-1720;X<=-520;X+=120)
        Box(FVector(X,1940,2),FVector(2,18,2),TEXT("Wood"));
    // Close every scenery boundary of the newly playable district.
    CityWall(FVector(-1790,2550,0),3300,900,90,FVector(1,0,0));
    CityWall(FVector(890,2550,0),3300,600,90,FVector(-1,0,0));
    CityWall(FVector(-450,4190,0),2700,900,0,FVector(0,-1,0));
    // Modest side gate in the city-facing (west) wall of the far Dock Street
    // court. Keep the wall solid; this is the future sewer entrance location.
    const FVector Gate(-1748,3650,0);
    Box(Gate+FVector(0,0,130),FVector(8,190,260),TEXT("Dark"));
    for(float Y=-85;Y<=85;Y+=17)
        Box(Gate+FVector(6,Y,128),FVector(7,15,252),TEXT("Wood"));
    for(float Z : {46.f,202.f})
        Box(Gate+FVector(12,0,Z),FVector(5,178,9),TEXT("Dark"));
    for(float Y : {-110.f,110.f})
        Box(Gate+FVector(10,Y,140),FVector(35,30,280),TEXT("Stone"));
    Box(Gate+FVector(10,0,281),FVector(42,250,26),TEXT("Stone"));
    Box(Gate+FVector(15,0,298),FVector(45,38,26),TEXT("Stone"));
    Box(Gate+FVector(17,-25,112),FVector(6,10,24),TEXT("Dark"));
    for(float Y : {-72.f,72.f}) for(float Z : {46.f,202.f})
        Box(Gate+FVector(16,Y,Z),FVector(4,6,6),TEXT("Metal"));
    // Walkable closed iron grate set flush into the paving before the gate.
    // The original ground is retained underneath; no hole or transition yet.
    const FVector Grate(-1580,3650,0);
    Box(Grate+FVector(0,0,.5f),FVector(150,170,1),TEXT("Dark"));
    for(float X : {-81.f,81.f}) Box(Grate+FVector(X,0,1),FVector(12,194,2),TEXT("Stone"));
    for(float Y : {-91.f,91.f}) Box(Grate+FVector(0,Y,1),FVector(150,12,2),TEXT("Stone"));
    for(float Y=-74;Y<=74;Y+=18.5f) Box(Grate+FVector(0,Y,1.4f),FVector(146,5,1.2f),TEXT("Metal"));
    for(float X : {-52.f,52.f}) Box(Grate+FVector(X,0,1.3f),FVector(7,165,1),TEXT("Dark"));
    for(float Y : {-63.f,63.f}) Box(Grate+FVector(-74,Y,1.5f),FVector(12,17,1),TEXT("Dark"));
    Box(FVector(540,1100,160),FVector(700,20,320),TEXT("Stone"),true);
    for(float Y : {1900.f,3500.f})
    {
        // Resting places and cargo tuck into the edge, leaving the court open.
        Box(FVector(-1640,Y,35),FVector(65,180,12),TEXT("Wood"),true);
        for(float Side : {-1.f,1.f}) Box(FVector(-1640,Y+Side*65,16),FVector(50,14,32),TEXT("Wood"),true);
        Prop(CrateMesh,FVector(820,Y,30));
        Box(FVector(820,Y,30),FVector(60,65,60),TEXT("Wood"),true,FRotator::ZeroRotator,!CrateMesh);
        Prop(RopeMesh,FVector(820,Y,61),FVector(.7f));
    }
    for(int32 I=0;I<4;++I)
    {
        const float X=-1450+I*600.f;
        Box(FVector(X,1646,255),FVector(160,10,35),TEXT("Wood"));
        Label(FVector(X,1654,255),I==0?TEXT("ROPEWORKS"):I==1?TEXT("NET MENDER"):I==2?TEXT("STORES"):TEXT("SAILMAKER"),90);
        Box(FVector(X,1680,226),FVector(190,90,8),TEXT("Roof"),false,FRotator(0,0,-8));
        for(float Side : {-1.f,1.f}) Beam(FVector(X+Side*75,1645,150),FVector(X+Side*75,1715,218),6,TEXT("Wood"));
        Box(FVector(X+150,1652,260),FVector(24,24,36),TEXT("Dark"));
        Box(FVector(X+150,1665,260),FVector(18,4,24),TEXT("Amber"));
        auto* Lamp=NewObject<UPointLightComponent>(Owner);
        Lamp->SetupAttachment(Root);
        Lamp->SetRelativeLocation(FVector(X+150,1690,260));
        Lamp->SetLightColor(FLinearColor(1,.55f,.23f));
        Lamp->SetIntensity(1600);
        Lamp->SetAttenuationRadius(360);
        Lamp->SetCastShadows(false);
        Lamp->RegisterComponent();
    }
    // The larger wall beyond the court stays closed.
    Box(FVector(-500,1000,-45),FVector(20,200,90),TEXT("Stone"),true);
    Box(FVector(-150,1090,70),FVector(700,20,140),TEXT("Stone"),true);
    Box(FVector(190,750,70),FVector(20,700,140),TEXT("Stone"),true);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failed=0;
        const FVector Route[]={FVector(-550,-430,0),FVector(-1100,-430,0),FVector(-1100,-1450,0),
            FVector(-1100,-2150,0),FVector(-540,-2150,0),FVector(-540,-1730,0),
            FVector(350,-1100,0),FVector(350,-1730,0),FVector(-400,750,0)};
        for(const FVector P : Route)
        {
            FHitResult Hit;
            const bool Ground=World->LineTraceSingleByChannel(Hit,P+FVector(0,0,60),P-FVector(0,0,100),ECC_Visibility);
            if(!Ground || FMath::Abs(Hit.ImpactPoint.Z)>2) ++Failed;
        }
        // Capsule clearance along the new street and the connection to the old wharf.
        for(const TPair<FVector,FVector>& Leg : {TPair<FVector,FVector>(Route[0],Route[1]),
            TPair<FVector,FVector>(Route[1],Route[2]),TPair<FVector,FVector>(Route[2],Route[3]),
            TPair<FVector,FVector>(Route[3],Route[4]),TPair<FVector,FVector>(Route[4],Route[5]),
            TPair<FVector,FVector>(Route[6],Route[7])})
        {
            FHitResult Hit;
            if(World->SweepSingleByChannel(Hit,Leg.Key+FVector(0,0,35),Leg.Value+FVector(0,0,35),
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failed;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_WORLD_CHECK_COMPLETE failures=%d floor_samples=9 capsule_routes=6"),Failed);
        int32 StreetFailures=0;
        const FVector Street[]={FVector(-1100,800,0),FVector(-1100,1050,0),FVector(-1150,1050,0),
            FVector(-1150,1800,0),FVector(750,1800,0),FVector(750,2300,0),
            FVector(-1110,2300,0),FVector(-1110,3300,0),FVector(750,3300,0),FVector(750,3900,0)};
        for(int32 I=0;I<UE_ARRAY_COUNT(Street);++I)
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,Street[I]+FVector(0,0,50),Street[I]-FVector(0,0,100),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.Z)>2) ++StreetFailures;
            if(I>0 && World->SweepSingleByChannel(Hit,Street[I-1]+FVector(0,0,35),Street[I]+FVector(0,0,35),
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++StreetFailures;
        }
        for(int32 I=0;I<4;++I)
        {
            FHitResult Hit;
            const float X=-1450+I*600.f;
            if(!World->LineTraceSingleByChannel(Hit,FVector(X,1800,100),FVector(X,1400,100),ECC_Visibility)) ++StreetFailures;
            if(!World->LineTraceSingleByChannel(Hit,FVector(X,1400,950),FVector(X,1400,450),ECC_Visibility)) ++StreetFailures;
        }
        FHitResult Boundary;
        if(!World->LineTraceSingleByChannel(Boundary,FVector(-1700,2100,100),FVector(-1900,2100,100),ECC_Visibility)) ++StreetFailures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_DOCKSTREET_CHECK failures=%d floors=10 routes=9 buildings=8 boundary=1"),StreetFailures);
        FHitResult GateHit,GrateHit,ApproachHit;
        const bool GateClosed=World->LineTraceSingleByChannel(GateHit,FVector(-1600,3650,100),FVector(-1900,3650,100),ECC_Visibility);
        const bool GrateGround=World->LineTraceSingleByChannel(GrateHit,FVector(-1580,3650,50),FVector(-1580,3650,-100),ECC_Visibility)
            && FMath::Abs(GrateHit.ImpactPoint.Z)<3;
        const bool ApproachClear=!World->SweepSingleByChannel(ApproachHit,FVector(-1100,3650,35),FVector(-1580,3650,35),
            FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f));
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SIDEGATE_CHECK failures=%d gate_closed=%d grate_ground=%d approach_clear=%d"),
            (!GateClosed)+(!GrateGround)+(!ApproachClear),GateClosed,GrateGround,ApproachClear);
    }
    // Opt-in setting review only; normal play and the traversal tests keep their camera.
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSettingCapture")))
    {
        const FVector Views[]={FVector(2200,-3400,1900),FVector(-1100,-2100,100),
            FVector(-1080,-500,105),FVector(840,-1780,105),FVector(1000,-1000,100),FVector(900,800,180),FVector(-1100,650,100),FVector(650,2150,170),FVector(800,3900,1800),FVector(-1150,3430,120),FVector(-1400,3650,560)};
        const FVector Targets[]={FVector(-600,-600,80),FVector(-1100,-400,160),
            FVector(-430,-750,145),FVector(750,-850,170),FVector(3380,0,180),FVector(2200,3840,180),FVector(-1150,1500,120),FVector(-900,1600,230),FVector(-450,2300,0),FVector(-1720,3650,115),FVector(-1580,3650,0)};
        auto* Camera=World->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(75);
        for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)
        {
            FTimerHandle Handle;
            World->GetTimerManager().SetTimer(Handle,[World,Camera,I,P=Views[I],T=Targets[I]]()
            {
                Camera->SetActorLocationAndRotation(P,(T-P).Rotation());
                if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
            },4.f+I*4.f,false);
            FTimerHandle CaptureHandle;
            World->GetTimerManager().SetTimer(CaptureHandle,[I]()
            {
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Setting");
                IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle ExitHandle;
        World->GetTimerManager().SetTimer(ExitHandle,[World]()
        {
            if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
        },50.f,false);
    }
}
