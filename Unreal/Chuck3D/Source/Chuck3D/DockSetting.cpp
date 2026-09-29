#include "DockSetting.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
    auto Box=[&](FVector P,FVector Size,const TCHAR* Surface,bool Collision=false,FRotator Rotation=FRotator::ZeroRotator)
    {
        const FString Key=FString(Surface)+(Collision?TEXT("_solid"):TEXT("_detail"));
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
            Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
    };
    auto Beam=[&](FVector A,FVector B,float Width,const TCHAR* Material)
    {
        const FVector Delta=B-A;
        Box((A+B)*.5f,FVector(Delta.Size(),Width,Width),Material,false,Delta.Rotation());
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
        Box(FVector(X,-680,82),FVector(78,80,64),TEXT("WoodLight"),true);
        for(float Z : {56.f,108.f}) Box(FVector(X,-722,Z),FVector(82,4,5),TEXT("Dark"));
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
        Box(FVector(-1100,Y,.4f),FVector(18,100,1),TEXT("Dark")); // recessed-looking drainage strip
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
    Box(FVector(-600,1900,-70),FVector(2400,1600,180),TEXT("Stone"));
    Box(FVector(3800,0,-80),FVector(900,5200,100),TEXT("Stone")); // bank below original distant warehouses
    for(int32 I=0;I<9;++I)
    {
        House(FVector(-2090,-2250+I*520,20),FVector(460,420,520+(I%3)*110),I%2?TEXT("Stone"):TEXT("Plaster"),false);
        House(FVector(-2790,-2470+I*590,40),FVector(580,460,700+(I%4)*80),TEXT("Stone"),false);
    }
    for(int32 I=0;I<4;++I)
        House(FVector(-1390+I*520,1400,20),FVector(440,480,560+(I%2)*130),TEXT("Plaster"),false);
    // The port belongs to a rising city, not a strip of houses on an island.
    // These are scenery beyond the existing closed perimeter, never new traversal.
    Box(FVector(-4500,100,-160),FVector(3800,7800,360),TEXT("Stone"));
    Box(FVector(-4650,250,120),FVector(2300,6800,200),TEXT("Stone"));
    Box(FVector(-5500,300,330),FVector(1300,6100,220),TEXT("Stone"));
    Box(FVector(-1100,3200,-100),FVector(3900,2000,260),TEXT("Stone"));
    for(int32 I=0;I<8;++I)
    {
        const float Y=-2700+I*780.f;
        House(FVector(-3830,Y,220),FVector(520+(I%2)*110,550,720+(I%3)*160),
            I%3?TEXT("Plaster"):TEXT("Stone"),false);
        House(FVector(-5240,Y+160,440),FVector(630,560,780+(I%4)*100),TEXT("Stone"),false);
    }
    for(int32 I=0;I<5;++I)
        House(FVector(-2130+I*680,2830,30),FVector(560,620,660+(I%3)*130),TEXT("Plaster"),false);
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
    // Solid perimeter walls meet the nonplayable city; gate-shaped panels imply streets beyond.
    Box(FVector(-1790,-700,100),FVector(20,3200,200),TEXT("Stone"),true);
    Box(FVector(-800,890,100),FVector(2000,20,200),TEXT("Stone"),true);
    Box(FVector(-1100,875,110),FVector(180,12,220),TEXT("Wood"));
    Label(FVector(-1100,866,250),TEXT("DOCK STREET"),-90);
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
    }
    // Opt-in setting review only; normal play and the traversal tests keep their camera.
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSettingCapture")))
    {
        const FVector Views[]={FVector(2200,-3400,1900),FVector(-1100,-2100,100),
            FVector(-1080,-500,105),FVector(840,-1780,105)};
        const FVector Targets[]={FVector(-600,-600,80),FVector(-1100,-400,160),
            FVector(-430,-750,145),FVector(750,-850,170)};
        auto* Camera=World->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(75);
        for(int32 I=0;I<4;++I)
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
        },22.f,false);
    }
}
