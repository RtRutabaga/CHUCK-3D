#include "DockSetting.h"
#include "DockVista.h"
#include "DockWeathering.h"
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
        if(Collision && FCString::Strcmp(Surface,TEXT("Plaster"))==0) Surface=TEXT("WeatheredPlaster");
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
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    auto SolidBarrel=[&](FVector P,float Scale=1.f)
    {
        Prop(BarrelMesh,P,FVector(Scale));
        // Match the original dock barrel's cylindrical 62 x 62 x 90 cm proxy.
        auto* Body=NewObject<UStaticMeshComponent>(Owner); Body->SetupAttachment(Root);
        Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Body->SetRelativeLocation(P); Body->SetRelativeScale3D(FVector(62,62,90)*Scale/100);
        Body->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Wood.M_Wood")));
        Body->SetCollisionProfileName(TEXT("BlockAll")); Body->SetVisibility(!BarrelMesh); Body->RegisterComponent();
    };
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
    // Roofless stone store: solid lower remnant, genuinely stepped/broken upper
    // courses rather than decorative damage over an invisible full-height box.
    Box(FVector(-600,50,64),FVector(200,600,128),TEXT("Stone"),true);
    for(int32 I=0;I<10;++I)
    {
        const float Y=-220+I*60.f;
        const float H=I<3?192.f:I<6?112.f:160.f;
        Box(FVector(-635,Y,128+H*.5f),FVector(130,58,H),TEXT("Stone"),true);
        const float FrontH=I<3?120.f:I<6?48.f:96.f;
        Box(FVector(-470,Y,200+FrontH*.5f),FVector(60,58,FrontH),TEXT("Stone"),true);
        for(int32 J=0;J<2;++J)
            Box(FVector(-620+J*65,Y,128+H+8+(I%3)*5),FVector(48,42,16),TEXT("Stone"),true,FRotator(0,I*13,0));
    }
    // Fallen masonry clustered against the ruin, outside the street route.
    for(int32 I=0;I<18;++I)
        Box(FVector(-735-(I%3)*15,-210+(I/3)*88,8+(I%3)*6),
            FVector(25+(I%4)*6,24+(I%3)*7,16+(I%3)*12),TEXT("Stone"),true,FRotator(0,I*37,0));
    // Jagged foundation stones retain the central climb/shimmy surfaces of
    // the L remnant but visibly lose their continuous coping and straight ends.
    for(int32 I=0;I<8;++I)
        if(I<2 || I>5) // surviving central landing must clear Chuck's capsule
        Box(FVector(-115+I*22,-374,119+(I%3)*4),FVector(18,20,8+(I%3)*8),TEXT("Stone"),true,FRotator(0,I*11,0));
    for(int32 I=0;I<4;++I)
        Box(FVector(90,-324+I*20,119+(I%2)*7),FVector(18,17,12+(I%2)*14),TEXT("Stone"),true);
    for(int32 I=0;I<9;++I)
        Box(FVector(-132+(I%3)*28,-398-(I/3)*20,6+(I%2)*5),FVector(21,18,12+(I%2)*10),TEXT("Stone"),true,FRotator(0,I*29,0));

    TArray<FVector> SolidChimneys;
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
        Box(P+FVector(-Size.X*.25f,Size.Y*.2f,Size.Z+Rise*.65f),FVector(45,48,150),TEXT("Stone"),Solid);
        if(Solid) SolidChimneys.Add(P+FVector(-Size.X*.25f,Size.Y*.2f,Size.Z+Rise*.65f));
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
        if(Solid)
        {
            // Repairs follow the existing building faces, never the roof route.
            const int32 Age=FMath::Abs(FMath::RoundToInt(P.X+P.Y))/10;
            for(float Side : {-1.f,1.f})
            {
                const float Face=Side*(Size.X*.5f+12);
                // Individual lower masonry courses and replaced door boards.
                for(int32 I=0;I<7;++I)
                {
                    const float Y=-Size.Y*.44f+I*Size.Y*.145f;
                    Box(P+FVector(Face,Y,17+(I%2)*4),FVector(5,Size.Y*.13f,26),TEXT("Stone"));
                }
                for(float Y=-36;Y<=36;Y+=18)
                    Box(P+FVector(Face,Y,100),FVector(3,15,194),((Age+int32(Y))%3)?TEXT("Wood"):TEXT("WoodLight"));
                for(float Z : {39.f,169.f})
                {
                    Box(P+FVector(Face+Side*3,0,Z),FVector(4,87,6),TEXT("Dark"));
                    for(float Y : {-31.f,31.f}) Box(P+FVector(Face+Side*6,Y,Z),FVector(3,4,4),TEXT("Metal"));
                }
                Box(P+FVector(Face+Side*6,31,97),FVector(7,6,16),TEXT("Dark"));
                // Repaired plaster corners: exposed stone, with timber stitch braces.
                const float End=Size.Y*.5f-45;
                for(int32 I=0;I<4;++I)
                    Box(P+FVector(Face,End-(I%2)*13,48+I*25),FVector(4,55-I*8,23),TEXT("Stone"));
                Beam(P+FVector(Face,-End,265),P+FVector(Face,-End+95,355),8,TEXT("Wood"));
                for(float Y : {-Size.Y*.3f,Size.Y*.3f})
                {
                    // Working wooden shutters, some closed and some pulled back.
                    const float Opening=(Age%3)*9.f;
                    for(float S : {-1.f,1.f})
                    {
                        Box(P+FVector(Face+Side*6,Y+S*(37+Opening),155),FVector(5,27,86),TEXT("Wood"),false,FRotator(0,S*Opening,0));
                        for(float Z : {129.f,180.f}) Box(P+FVector(Face+Side*9,Y+S*(37+Opening),Z),FVector(3,28,4),TEXT("Dark"));
                    }
                    Box(P+FVector(Face,Y,208),FVector(27,92,8),TEXT("Wood"));
                }
            }
            // A few mismatched roof repairs lie flush, leaving collision untouched.
            for(int32 I=0;I<3;++I)
            {
                const float X=Size.X*(.16f+.055f*I),Y=Size.Y*(-.3f+.2f*I);
                const float Z=Size.Z+Rise-X*.57735f+9;
                Box(P+FVector(X,Y,Z),FVector(43,54,2),I==1?TEXT("Wood"):TEXT("Roof"),false,FRotator(-30,0,0));
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
    // Narrow, uneven board faces and repaired lower boards on the five flat
    // roofed dock workshops. Decorative thickness stays outside traversal tests.
    const FVector Workshops[]={FVector(-180,-790,230),FVector(170,-770,260),
        FVector(-370,-1150,180),FVector(-370,-1400,230),FVector(-370,-1650,280)};
    for(int32 W=0;W<5;++W)
    {
        const FVector P=Workshops[W];
        const FVector Size=W==0?FVector(300,220,P.Z):W==1?FVector(200,260,P.Z):FVector(220,180,P.Z);
        for(float Side : {-1.f,1.f})
        {
            for(int32 I=0;I<FMath::FloorToInt(Size.X/18);++I)
                Box(FVector(P.X-Size.X*.5f+9+I*18,P.Y+Side*(Size.Y*.5f+.7f),P.Z*.5f),
                    FVector(17,1.4f,P.Z-4),TEXT("AgedDockTimber"));
            for(int32 I=0;I<FMath::FloorToInt(Size.Y/18);++I)
                Box(FVector(P.X+Side*(Size.X*.5f+.7f),P.Y-Size.Y*.5f+9+I*18,P.Z*.5f),
                    FVector(1.4f,17,P.Z-4),TEXT("AgedDockTimber"));
            Box(FVector(P.X+Side*(Size.X*.5f+1.8f),P.Y+Size.Y*.3f,28),
                FVector(2,47,36),TEXT("Wood"));
            for(float Z : {18.f,static_cast<float>(P.Z)-14.f})
                Box(FVector(P.X,P.Y+Side*(Size.Y*.5f+1.8f),Z),FVector(Size.X,3,5),TEXT("Wood"));
        }
    }
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
    // Continuous court paving split around the actual open sewer mouth.
    Box(FVector(-450,2347.5f,-45),FVector(2700,2895,90),TEXT("Stone"),true);
    Box(FVector(-450,4102.5f,-45),FVector(2700,195,90),TEXT("Stone"),true);
    Box(FVector(-1747.5f,3900,-45),FVector(105,210,90),TEXT("Stone"),true);
    Box(FVector(-282.5f,3900,-45),FVector(2365,210,90),TEXT("Stone"),true);
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
    // Inland boundaries stay closed; the eastern waterside now opens to a dock.
    CityWall(FVector(-1790,2550,0),3300,900,90,FVector(1,0,0));
    CityWall(FVector(-450,4190,0),2700,900,0,FVector(0,-1,0));
    // Return the northern boundary along the shore, then out to the water at
    // the breakwater end. Close the vista shortcut beside the new landing.
    CityWall(FVector(890,3965,0),450,900,90,FVector(-1,0,0));
    CityWall(FVector(1350,3740,0),990,900,0,FVector(0,-1,0));
    // Modest working landing, two finger piers, flush with the street paving.
    // Solid continuous deck proxies prevent cracks between decorative boards.
    auto Deck=[&](FVector P,FVector Size)
    {
        Box(P-FVector(0,0,20),FVector(Size.X,Size.Y,40),TEXT("Wood"),true);
        const int32 Boards=FMath::CeilToInt(Size.Y/25.f);
        const float Pitch=Size.Y/Boards;
        for(int32 I=0;I<Boards;++I)
        {
            const float Y=P.Y-Size.Y*.5f+(I+.5f)*Pitch;
            Box(FVector(P.X,Y,.5f),FVector(Size.X-2,Pitch-1,1),I%7==0?TEXT("WoodLight"):TEXT("Wood"));
            // Replacement boards, nails and transverse joins rather than a smooth slab.
            for(float X : {P.X-Size.X*.35f,P.X+Size.X*.35f})
                Box(FVector(X,Y,1.1f),FVector(3,3,1),TEXT("Dark"));
            Box(FVector(P.X+(I%3-1)*Size.X*.18f,Y,1.05f),FVector(1,Pitch-1,.2f),TEXT("Dark"));
        }
        for(float S : {-1.f,1.f})
            Box(P+FVector(0,S*(Size.Y*.5f-12),-45),FVector(Size.X+30,22,35),TEXT("Wood"));
    };
    Deck(FVector(1190,3200,0),FVector(620,1000,0)); // x880..1500: overlaps shore by 20 cm
    Deck(FVector(1910,3080,0),FVector(840,300,0));
    Deck(FVector(1820,3550,0),FVector(660,260,0));
    auto Pile=[&](FVector P)
    {
        Box(P+FVector(0,0,-55),FVector(30,30,240),TEXT("Wood"),true);
        Box(P+FVector(0,0,68),FVector(37,37,9),TEXT("WoodLight"));
        for(float Z : {-80.f,35.f}) Box(P+FVector(0,0,Z),FVector(33,33,5),TEXT("Dark"));
        Prop(RopeMesh,P+FVector(0,0,47),FVector(.42f));
    };
    for(float X : {980.f,1450.f}) for(float Y : {2715.f,3685.f}) Pile(FVector(X,Y,0));
    for(float X : {1750.f,2280.f}) for(float Y : {2945.f,3215.f}) Pile(FVector(X,Y,0));
    for(float X : {1770.f,2100.f}) for(float Y : {3435.f,3665.f}) Pile(FVector(X,Y,0));
    for(float Y : {2740.f,3660.f})
        Beam(FVector(970,Y,-120),FVector(1450,Y,-42),15,TEXT("Wood"));
    // Tied working supplies remain off the landing's central walking line.
    SolidBarrel(FVector(1060,2780,45));
    Prop(CrateMesh,FVector(1370,3610,30));
    Box(FVector(1370,3610,30),FVector(60,65,60),TEXT("Wood"),true,FRotator::ZeroRotator,!CrateMesh);
    Prop(RopeMesh,FVector(1370,3610,61),FVector(.65f));
    for(float X : {1140.f,1230.f}) Box(FVector(X,2730,36),FVector(75,50,72),TEXT("Wood"),true);
    Box(FVector(1185,2730,77),FVector(180,60,10),TEXT("WoodLight"));
    // A dock lamp is outside the entrance and pier walking lines.
    Box(FVector(940,3650,145),FVector(10,10,290),TEXT("Wood"),true);
    Box(FVector(940,3650,298),FVector(35,35,38),TEXT("Dark"));
    Box(FVector(940,3650,298),FVector(27,27,28),TEXT("TorchFlame"));
    auto* DockLamp=NewObject<UPointLightComponent>(Owner);
    DockLamp->SetupAttachment(Root); DockLamp->SetRelativeLocation(FVector(940,3650,298));
    DockLamp->SetIntensity(1600); DockLamp->SetAttenuationRadius(400);
    DockLamp->SetLightColor(FLinearColor(1,.43f,.12f)); DockLamp->SetCastShadows(false); DockLamp->RegisterComponent();
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
    // Wider iron hatch, propped open on its west hinge. This is a dark drop,
    // not a sewer level: entering it uses the existing below-quay spawn reset.
    const FVector Grate(-1580,3900,0);
    for(float X : {-121.f,121.f}) Box(Grate+FVector(X,0,2),FVector(12,234,4),TEXT("Stone"),true);
    for(float Y : {-111.f,111.f}) Box(Grate+FVector(0,Y,2),FVector(230,12,4),TEXT("Stone"),true);
    for(float X : {-111.f,111.f}) Box(Grate+FVector(X,0,-105),FVector(8,210,210),TEXT("Stone"),true);
    for(float Y : {-101.f,101.f}) Box(Grate+FVector(0,Y,-105),FVector(214,8,210),TEXT("Stone"),true);
    // Unlit darkness occludes the global harbor plane without blocking descent.
    Box(Grate+FVector(0,0,-18),FVector(222,202,1),TEXT("SewerVoid"));
    const FVector Hinge=Grate+FVector(-120,0,7);
    const FRotator Open(76,0,0);
    auto Hatch=[&](FVector P,FVector Size){Box(Hinge+Open.RotateVector(P),Size,TEXT("RustIron"),true,Open);};
    for(float Y : {-102.f,102.f}) Hatch(FVector(115,Y,0),FVector(230,10,8));
    for(float X : {5.f,225.f}) Hatch(FVector(X,0,0),FVector(10,204,8));
    for(float X=22;X<220;X+=18) Hatch(FVector(X,0,0),FVector(6,200,7));
    for(float Y : {-58.f,58.f}) Hatch(FVector(115,Y,4),FVector(218,7,5));
    for(float X : {22.f,76.f,130.f,184.f}) for(float Y : {-58.f,58.f})
        Hatch(FVector(X,Y,8),FVector(10,10,4));
    for(float Y : {-78.f,78.f})
    {
        Box(Hinge+FVector(0,Y,0),FVector(18,32,18),TEXT("RustIron"),true);
        Box(Hinge+FVector(-12,Y,0),FVector(24,42,7),TEXT("RustIron"),true);
        Beam(Hinge+FVector(30,Y,0),Hinge+Open.RotateVector(FVector(150,Y,0)),5,TEXT("RustIron"));
    }
    Hatch(FVector(214,0,12),FVector(8,35,7));
    Box(FVector(540,1100,160),FVector(700,20,320),TEXT("Stone"),true);
    for(float Y : {1900.f,3500.f})
    {
        // Resting places and cargo tuck into the edge, leaving the court open.
        const float BenchY=Y==3500.f?3300.f:Y; // Keep the new gate/grate apron clear.
        Box(FVector(-1640,BenchY,35),FVector(65,180,12),TEXT("Wood"),true);
        for(float Side : {-1.f,1.f}) Box(FVector(-1640,BenchY+Side*65,16),FVector(50,14,32),TEXT("Wood"),true);
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
    // Small working possessions stay tight to facades, outside the clear lanes.
    for(const FVector P : {FVector(-1260,-1700,0),FVector(-1190,470,0),FVector(-1720,2100,0),FVector(820,3260,0)})
    {
        SolidBarrel(P+FVector(0,0,31),.68f);
        Prop(RopeMesh,P+FVector(0,0,62),FVector(.48f),25);
        for(int32 I=0;I<5;++I)
            // Foot on ground at y=80; boards meet barrel rim at y=16,z=62.
            Box(P+FVector(-12+I*6,48,31),FVector(5,88,5),I%2?TEXT("Wood"):TEXT("WoodLight"),true,FRotator(0,0,44));
        Box(P+FVector(0,48,34),FVector(33,5,5),TEXT("Dark"),false,FRotator(0,0,44));
    }
    // A ladder kept against the cooperage, below the eaves rather than across a route.
    for(float Y : {-1130.f,-1090.f})
        Beam(FVector(-1320,Y,8),FVector(-1330,Y,265),6,TEXT("WoodLight"));
    for(float Z=30;Z<255;Z+=27) Box(FVector(-1320-Z*.038f,-1110,Z),FVector(6,47,5),TEXT("Wood"));
    // Laundry between upper facades: muted cloth, repaired hems and visible pegs.
    auto Laundry=[&](FVector A,FVector B)
    {
        const FVector D=B-A;
        FVector Prev=A;
        for(int32 I=1;I<=12;++I)
        {
            const float T=I/12.f;
            const FVector Next=A+D*T-FVector(0,0,18*FMath::Sin(PI*T));
            Beam(Prev,Next,1,TEXT("Wood")); Prev=Next;
        }
        const float Yaw=D.Rotation().Yaw;
        for(int32 I=0;I<3;++I)
        {
            const float T=.25f+I*.24f;
            const FVector Top=A+D*T-FVector(0,0,18*FMath::Sin(PI*T));
            const float H=65+I*12;
            for(int32 Fold=0;Fold<5;++Fold)
                Box(Top+FRotator(0,Yaw,0).RotateVector(FVector((Fold-2)*13,(Fold%2)*3,-H*.5f)),FVector(13,2,H),I==1?TEXT("Roof"):TEXT("Plaster"),false,FRotator(0,Yaw,0));
            Box(Top+FVector(0,0,-H+4),FVector(61,3,6),TEXT("WoodLight"),false,FRotator(0,Yaw,0));
            for(float Offset : {-22.f,22.f}) Box(Top+FRotator(0,Yaw,0).RotateVector(FVector(Offset,0,2)),FVector(3,6,9),TEXT("Wood"),false,FRotator(0,Yaw,0));
        }
    };
    Laundry(FVector(-1390,-584,340),FVector(-1390,-843,340));
    Laundry(FVector(-1220,1648,370),FVector(-740,1648,370));
    // Repaired benches: narrow replacement slats and iron end plates.
    for(float Y : {1900.f,3300.f})
    {
        for(int32 I=0;I<5;++I) Box(FVector(-1666+I*13,Y,41.8f),FVector(11,177,1.5f),I==1?TEXT("WoodLight"):TEXT("Wood"));
        for(float DY : {-65.f,65.f}) Box(FVector(-1640,Y+DY,43),FVector(63,5,2),TEXT("Dark"));
    }
    Box(FVector(-500,1000,-45),FVector(20,200,90),TEXT("Stone"),true);
    // The courtyard now joins Dock Street northward; remove its obsolete
    // cross-street divider. Keep only a low kerb at the real eastern quay edge.
    Box(FVector(190,750,20),FVector(20,700,40),TEXT("Stone"),true);
    BuildDockWeathering(World);

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
        const bool GrateOpen=!World->LineTraceSingleByChannel(GrateHit,FVector(-1580,3900,50),FVector(-1580,3900,-100),ECC_Visibility);
        const bool ApproachClear=!World->SweepSingleByChannel(ApproachHit,FVector(-1100,3900,35),FVector(-1390,3900,35),
            FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f));
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SIDEGATE_CHECK failures=%d gate_closed=%d grate_open=%d approach_clear=%d"),
            (!GateClosed)+(!GrateOpen)+(!ApproachClear),GateClosed,GrateOpen,ApproachClear);
        int32 HatchFailures=0;
        for(float X : {-1640.f,-1580.f,-1520.f})
        {
            FHitResult Hit;
            if(World->LineTraceSingleByChannel(Hit,FVector(X,3900,40),FVector(X,3900,-150),ECC_Visibility)) ++HatchFailures;
        }
        for(const FVector P : {FVector(-1400,3900,0),FVector(-1580,3760,0),FVector(-1580,4040,0)})
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,40),P-FVector(0,0,80),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.Z)>2) ++HatchFailures;
        }
        FHitResult RaisedHatch;
        if(!World->LineTraceSingleByChannel(RaisedHatch,FVector(-1600,4002,100),FVector(-1750,4002,100),ECC_Visibility)) ++HatchFailures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_OPENHATCH_CHECK failures=%d shaft=3 surrounds=3 raised_lid=1"),HatchFailures);
        int32 PierFailures=0;
        const FVector PierRoute[]={FVector(750,3080,0),FVector(950,3080,0),FVector(1200,3080,0),FVector(1480,3080,0),
            FVector(1800,3080,0),FVector(2260,3080,0),FVector(1200,3350,0),FVector(1200,3550,0),FVector(1480,3550,0),FVector(2080,3550,0)};
        for(const FVector P : PierRoute)
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,50),P-FVector(0,0,100),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.Z)>2) ++PierFailures;
        }
        for(const FIntPoint Leg : {FIntPoint(0,1),FIntPoint(1,2),FIntPoint(2,3),FIntPoint(3,4),FIntPoint(4,5),FIntPoint(2,6),FIntPoint(6,7),FIntPoint(7,8),FIntPoint(8,9)})
        {
            FHitResult Hit;
            if(World->SweepSingleByChannel(Hit,PierRoute[Leg.X]+FVector(0,0,35),PierRoute[Leg.Y]+FVector(0,0,35),
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++PierFailures;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_COURTPIER_CHECK failures=%d floors=10 routes=9"),PierFailures);
        int32 PropFailures=0;
        for(const FVector P : {FVector(-1260,-1700,31),FVector(-1190,470,31),FVector(-1720,2100,31),FVector(820,3260,31),FVector(1060,2780,45)})
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(75,0,0),P-FVector(75,0,0),ECC_Visibility)
                || FVector::Dist(Hit.ImpactPoint,P)>35)
            { ++PropFailures; UE_LOG(LogTemp,Warning,TEXT("CHUCK_DOCKPROPS barrel failed p=%s hit=%s"),*P.ToString(),*Hit.ImpactPoint.ToString()); }
        }
        for(const FVector P : {FVector(-1260,-1652,31),FVector(-1190,518,31),FVector(-1720,2148,31),FVector(820,3308,31)})
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,40,0),P-FVector(0,40,0),ECC_Visibility)
                || FVector::Dist(Hit.ImpactPoint,P)>22)
            { ++PropFailures; UE_LOG(LogTemp,Warning,TEXT("CHUCK_DOCKPROPS planks failed p=%s hit=%s"),*P.ToString(),*Hit.ImpactPoint.ToString()); }
        }
        for(const FVector P : {FVector(1100,3740,100),FVector(1770,3740,100)})
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P-FVector(0,100,0),P+FVector(0,100,0),ECC_Visibility)) ++PropFailures;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_DOCKPROPS_CHECK failures=%d barrels=5 planks=4 boundaries=2"),PropFailures);
        int32 ChimneyFailures=0;
        for(const FVector P : SolidChimneys)
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(100,0,65),P-FVector(100,0,-65),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.X-(P.X+22.5f))>2) ++ChimneyFailures;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHIMNEY_COLLISION failures=%d checked=%d"),ChimneyFailures,SolidChimneys.Num());
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckWorkshopCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(65);
        const FVector Positions[]={FVector(580,-1180,230),FVector(-1280,4100,350),FVector(-1330,3900,120)};
        const FVector Targets[]={FVector(-220,-1190,125),FVector(-1580,3900,60),FVector(-1580,3900,30)};
        for(int32 I=0;I<3;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Positions[I],T=Targets[I]](){
                Camera->SetActorLocationAndRotation(P,(T-P).Rotation());
                if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
            },4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Workshops");
                IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle Exit;
        World->GetTimerManager().SetTimer(Exit,[World](){if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));},18.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckRuinsCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(65);
        const FVector Positions[]={FVector(180,-560,390),FVector(-930,-350,330),FVector(-1090,630,100)};
        const FVector Targets[]={FVector(-340,-140,100),FVector(-575,40,160),FVector(-1190,510,40)};
        for(int32 I=0;I<3;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Positions[I],T=Targets[I]](){
                Camera->SetActorLocationAndRotation(P,(T-P).Rotation());
                if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
            },4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Ruins");
                IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle Exit;
        World->GetTimerManager().SetTimer(Exit,[World](){if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));},18.f,false);
    }
    // Opt-in setting review only; normal play and the traversal tests keep their camera.
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSettingCapture")))
    {
        const FVector Views[]={FVector(2200,-3400,1900),FVector(-1100,-2100,100),
            FVector(-1080,-500,105),FVector(840,-1780,105),FVector(1000,-1000,100),FVector(900,800,180),FVector(-1100,650,100),FVector(650,2150,170),FVector(800,3900,1800),FVector(-1100,3650,190),FVector(-1400,3900,560),FVector(2550,2400,1500),FVector(1000,3080,95)};
        const FVector Targets[]={FVector(-600,-600,80),FVector(-1100,-400,160),
            FVector(-430,-750,145),FVector(750,-850,170),FVector(3380,0,180),FVector(2200,3840,180),FVector(-1150,1500,120),FVector(-900,1600,230),FVector(-450,2300,0),FVector(-1720,3650,115),FVector(-1580,3900,0),FVector(1400,3300,0),FVector(2250,3080,90)};
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
        },58.f,false);
    }
}
