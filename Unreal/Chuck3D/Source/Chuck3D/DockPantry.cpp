#include "DockPantry.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ClayJar.h"
#include "SewerSlide.h"
#include "DockFire.h"
#include "DockNPC.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"

namespace
{
    // The cellar (user 2026-10-03: "a bit bigger" than Codex's 5.8 x 5.25 m
    // foundation): 8 x 7 m under the tavern, floor -320, ceiling -100.
    constexpr float X0 = -450.f, X1 = 350.f, Y0 = 400.f, Y1 = 1100.f, Floor = -320.f;
    // Its floor is broken, as in the 2D game's pantry: Astral ruptures here and
    // there, and in the middle a wide hole onto open sky with one island of
    // floor in it, a crate, and on the crate the cheese. The sky ring is wider
    // than Chuck's longest jump (a long side jump: about 1.26 m, ~1.56 m edge
    // to edge with his body) by enough that he arrives too low even to catch
    // the island's edge: it looks worth a try and isn't.
    const FVector2D SkyCentre(-170.f, 690.f);
    constexpr float SkyRadius = 265.f, IslandRadius = 52.f;
    struct FRift { float X, Y, RX, RY; };
    const FRift Rifts[] = { {240.f, 520.f, 34.f, 26.f}, {190.f, 780.f, 28.f, 40.f}, {-60.f, 1035.f, 42.f, 28.f}, {-400.f, 1060.f, 30.f, 26.f} };
    // 0 floor, 1 Astral rupture, 2 sky.
    int32 HoleAt(float X, float Y)
    {
        const FVector2D D = FVector2D(X, Y) - SkyCentre;
        const float R = static_cast<float>(D.Size()), A = FMath::Atan2(static_cast<float>(D.Y), static_cast<float>(D.X));
        if (R < SkyRadius + 10.f * FMath::Sin(5.f * A) + 6.f * FMath::Sin(11.f * A + 1.f) && R > IslandRadius + 4.f * FMath::Sin(7.f * A)) return 2;
        for (const FRift& Rift : Rifts)
        {
            const float DX = (X - Rift.X) / Rift.RX, DY = (Y - Rift.Y) / Rift.RY;
            const float Jag = 1.f + .14f * FMath::Sin(6.f * FMath::Atan2(DY, DX) + Rift.X * .1f);
            if (FMath::Sqrt(DX * DX + DY * DY) * Jag < 1.f) return 1;
        }
        return 0;
    }
}

bool IsWithinDockPantry(const FVector& P)
{
    // Bounded permission for this cellar/shaft, not a blanket below-world
    // bypass; down to 380 cm under the floor so a fall through its holes is
    // seen before it counts as off the map.
    return (P.X > X0 && P.X < X1 && P.Y > Y0 && P.Y < Y1 && P.Z > Floor - 380.f && P.Z < -105)
        || (P.X > 20 && P.X < 130 && P.Y > 875 && P.Y < 955 && P.Z >= -105 && P.Z < 70);
}

FVector DockPantryStartLocation() { return FVector(55, 835, Floor + 34.65f); }

int32 DockPantryHoleAt(const FVector2D& At) { return HoleAt(static_cast<float>(At.X), static_cast<float>(At.Y)); }
FVector2D DockPantrySkyCentre() { return SkyCentre; }
float DockPantrySkyRadius() { return SkyRadius; }
float DockPantryIslandRadius() { return IslandRadius; }
bool IsDockPantrySkyRim(const FVector& Edge)
{
    // Whatever reaches the outer rim from inside the hole (a jump back off the
    // cheese's island) falls: the cheese is a one-way trip.
    const float R = static_cast<float>(FVector2D::Distance(FVector2D(Edge.X, Edge.Y), SkyCentre));
    return FMath::Abs(Edge.Z - Floor) < 25.f && R > SkyRadius - 35.f && R < SkyRadius + 35.f;
}

void BuildDockPantry(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>();Owner->Tags.Add(TEXT("DockPantry"));
    auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Root->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Surface,bool Solid=false,int32 Form=0,FRotator Rotation=FRotator::ZeroRotator)
    {
        // The sewer's Astral materials aren't made for instancing: those go on plain mesh components.
        if(FCString::Strncmp(Surface,TEXT("Astral"),6)==0)
        {
            auto* Part=NewObject<UStaticMeshComponent>(Owner);Part->SetupAttachment(Root);Part->SetStaticMesh(Form==1?Cylinder:Form==2?Sphere:Cube);
            Part->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Surface,Surface)));
            Part->SetCollisionProfileName(TEXT("NoCollision"));Part->SetCastShadow(false);
            Part->SetRelativeTransform(FTransform(Rotation,P,Size/100.f));Part->RegisterComponent();
            return;
        }
        const FString Key=FString::Printf(TEXT("%s_%d_%d"),Surface,Solid,Form);
        auto*& Batch=Batches.FindOrAdd(Key);
        if(!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(Owner);Batch->SetupAttachment(Root);
            Batch->SetStaticMesh(Form==1?Cylinder:Form==2?Sphere:Cube);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Surface,Surface));
            if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Surface,Surface));
            Batch->SetMaterial(0,Material);Batch->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
            if(Solid && FCString::Strcmp(Surface,TEXT("Cheese"))==0) Batch->ComponentTags.Add(TEXT("PantryCheese"));
            if(!Solid) Batch->SetCastShadow(FCString::Strcmp(Surface,TEXT("PantrySky"))!=0 && FCString::Strcmp(Surface,TEXT("PantryCloud"))!=0 && FCString::Strcmp(Surface,TEXT("AstralDepth"))!=0);
            Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
    };
    const FVector Mid((X0+X1)*.5f,(Y0+Y1)*.5f,0), Span(X1-X0,Y1-Y0,0);
    // The floor: 10 cm strips, merged along each row, left out where it's broken through.
    constexpr float Cell=10.f;
    for(float Y=Y0+Cell*.5f;Y<Y1;Y+=Cell)
    {
        float Start=-1e9f;
        for(float X=X0+Cell*.5f;X<X1+Cell;X+=Cell)
        {
            const bool bSolid=X<X1 && HoleAt(X,Y)==0;
            if(bSolid && Start<-1e8f) Start=X-Cell*.5f;
            if(!bSolid && Start>-1e8f)
            {
                const float End=X-Cell*.5f;
                Shape(FVector((Start+End)*.5f,Y,Floor-15),FVector(End-Start,Cell,30),TEXT("Stone"),true);
                Start=-1e9f;
            }
        }
    }
    // Walls to the ceiling, the ceiling round the shaft (x20..130, y875..955).
    for(float X : {X0-12.f,X1+12.f}) Shape(FVector(X,Mid.Y,-205),FVector(24,Span.Y+48,230),TEXT("Stone"),true);
    for(float Y : {Y0-12.f,Y1+12.f}) Shape(FVector(Mid.X,Y,-205),FVector(Span.X,24,230),TEXT("Stone"),true);
    Shape(FVector((X0+20)*.5f,Mid.Y,-100),FVector(20-X0,Span.Y,20),TEXT("Stone"),true);
    Shape(FVector((130+X1)*.5f,Mid.Y,-100),FVector(X1-130,Span.Y,20),TEXT("Stone"),true);
    Shape(FVector(75,(Y0+875)*.5f,-100),FVector(110,875-Y0,20),TEXT("Stone"),true);
    Shape(FVector(75,(955+Y1)*.5f,-100),FVector(110,Y1-955,20),TEXT("Stone"),true);
    // Shaft lining through the ground layers (Codex's), hatch surround and upright lid.
    for(float X : {14.f,136.f}) Shape(FVector(X,915,-50),FVector(12,80,100),TEXT("Stone"),true);
    for(float Y : {869.f,961.f}) Shape(FVector(75,Y,-50),FVector(122,12,100),TEXT("Stone"),true);
    for(float X : {14.f,136.f}) Shape(FVector(X,915,1),FVector(12,104,2),TEXT("WoodLight"));
    for(float Y : {869.f,961.f}) Shape(FVector(75,Y,1),FVector(110,12,2),TEXT("WoodLight"));
    Shape(FVector(14,915,48),FVector(5,96,92),TEXT("Wood"));
    for(float Y : {884.f,946.f}) Shape(FVector(17,Y,48),FVector(2,6,80),TEXT("Dark"));
    // The ladder (Codex's rails and rungs): Chuck climbs it (ChuckClimbable, the Ladder gait).
    for(float Y : {891.f,939.f}) Shape(FVector(124,Y,-150),FVector(6,6,330),TEXT("WoodLight"));
    for(float Z=-300;Z<=0;Z+=28) Shape(FVector(124,915,Z),FVector(7,54,5),TEXT("Wood"));
    {
        FChuckClimbable Ladder;
        Ladder.Foot=FVector(103.5f,915,Floor); Ladder.TopZ=0; Ladder.Out=FVector(-1,0,0); Ladder.HalfWidth=22; Ladder.Lip=FVector(130,915,0);
        AddChuckClimbable(Ladder);
    }
    for(float X : {-300.f,-120.f,230.f}) Shape(FVector(X,Mid.Y,-119),FVector(15,Span.Y-10,18),TEXT("Wood"),true);
    // Slightly varied masonry courses over the walls.
    for(int32 Row=0;Row<5;++Row)
    {
        const float Z=-297+Row*39.f;
        for(int32 I=0;I*53.f<Span.Y-40;++I) for(float X : {X0+5.f,X1-5.f})
            Shape(FVector(X,Y0+32+I*53.f+(Row%2)*8,Z),FVector(5,49,34+(I%3)),I%5?TEXT("Stone"):TEXT("Dark"));
        for(int32 I=0;I*55.f<Span.X-40;++I) for(float Y : {Y0+5.f,Y1-5.f})
            Shape(FVector(X0+30+I*55.f+(Row%2)*9,Y,Z-1),FVector(51,5,35),I%6?TEXT("Stone"):TEXT("Dark"));
    }
    // Broken flagstone chips round each hole's edge.
    FRandomStream Chips(20261003);
    auto Chip=[&](float X,float Y){ Shape(FVector(X,Y,Floor+1.5f),FVector(Chips.FRandRange(6,14),Chips.FRandRange(5,11),Chips.FRandRange(2,4)),Chips.FRand()<.3f?TEXT("Dark"):TEXT("Stone"),false,0,FRotator(Chips.FRandRange(-8,8),Chips.FRandRange(0,180),Chips.FRandRange(-8,8))); };
    // The Astral ruptures: the sewer's depth below each, its oil haze over it, its purple light.
    auto* Oil=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist"));
    int32 OilRims=0;
    TArray<UProceduralMeshComponent*> OilMeshes;
    auto OilRim=[&](const TArray<FVector>& Edges,float Height,const FVector2D& Centre,bool Inner)
    {
        TArray<FVector> V,N;TArray<FVector2D> UV;TArray<int32> T;
        constexpr int32 Levels=8;
        for(int32 I=0;I<=Edges.Num();++I) for(int32 J=0;J<=Levels;++J)
        {
            const float U=I/float(Edges.Num()),H=J/float(Levels);
            const FVector Edge=Edges[I%Edges.Num()];
            const FVector In=(FVector(Centre.X,Centre.Y,Floor)-Edge).GetSafeNormal2D()*(Inner?-1.f:1.f);
            V.Add(Edge+In*(H*H*4*FMath::Sin(U*UE_TWO_PI*3))+FVector(0,0,3+H*Height*(.9f+.1f*FMath::Sin(U*UE_TWO_PI*5))));
            N.Add(In);UV.Add(FVector2D(U,H));
        }
        for(int32 I=0;I<Edges.Num();++I) for(int32 J=0;J<Levels;++J)
        {const int32 A=I*(Levels+1)+J;T.Append({A,A+1,A+Levels+1,A+1,A+Levels+2,A+Levels+1});}
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner);Mesh->SetupAttachment(Root);
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));Mesh->SetCastShadow(false);Mesh->RegisterComponent();
        Mesh->CreateMeshSection_LinearColor(0,V,T,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
        Mesh->SetMaterial(0,Oil);OilMeshes.Add(Mesh);++OilRims;
    };
    for(const FRift& Rift : Rifts)
    {
        const float W=Rift.RX*2+30, D=Rift.RY*2+30, Z0=Floor-30, Z1=Floor-330;
        Shape(FVector(Rift.X,Rift.Y,Z1),FVector(W,D,4),TEXT("AstralDepth"));
        for(float S : {-1.f,1.f})
        {
            Shape(FVector(Rift.X+S*W*.5f,Rift.Y,(Z0+Z1)*.5f),FVector(4,D,Z0-Z1),TEXT("AstralDepth"));
            Shape(FVector(Rift.X,Rift.Y+S*D*.5f,(Z0+Z1)*.5f),FVector(W,4,Z0-Z1),TEXT("AstralDepth"));
        }
        TArray<FVector> Rim;
        for(int32 I=0;I<48;++I)
        {
            const float A=I*UE_TWO_PI/48.f,Jag=1.f+.14f*FMath::Sin(6.f*A+Rift.X*.1f);
            Rim.Add(FVector(Rift.X+Rift.RX*FMath::Cos(A)/Jag,Rift.Y+Rift.RY*FMath::Sin(A)/Jag,Floor));
        }
        OilRim(Rim,70,FVector2D(Rift.X,Rift.Y),false);
        auto* Glow=NewObject<UPointLightComponent>(Owner);Glow->SetupAttachment(Root);Glow->SetRelativeLocation(FVector(Rift.X,Rift.Y,Floor-25));
        Glow->SetIntensity(1800);Glow->SetAttenuationRadius(420);Glow->SetLightColor(FLinearColor(.48f,.035f,1));Glow->SetCastShadows(false);Glow->RegisterComponent();
        for(int32 I=0;I<7;++I){ const float A=I*.9f+Rift.X; Chip(Rift.X+FMath::Cos(A)*(Rift.RX+9),Rift.Y+FMath::Sin(A)*(Rift.RY+9)); }
    }
    // The sky: a well of open sky under the middle of the floor, clouds drifting
    // in it, its daylight coming up through the hole. A broken floor fragment,
    // its foundation and a little earth hang impossibly above the clouds.
    {
        const float S=SkyRadius*2+40, Z0=Floor-30, Z1=-1150;
        for(bool Inner : {false,true})
        {
            TArray<FVector> Rim;
            for(int32 I=0;I<128;++I)
            {
                const float A=I*UE_TWO_PI/128.f;
                const float R=Inner ? IslandRadius+4.f*FMath::Sin(7.f*A) : SkyRadius+10.f*FMath::Sin(5.f*A)+6.f*FMath::Sin(11.f*A+1.f);
                const FVector P(SkyCentre.X+R*FMath::Cos(A),SkyCentre.Y+R*FMath::Sin(A),Floor);
                Rim.Add(P);
            }
            OilRim(Rim,Inner?50.f:100.f,SkyCentre,Inner);
        }
        Shape(FVector(SkyCentre.X,SkyCentre.Y,Z1),FVector(S,S,4),TEXT("PantrySky"));
        for(float D : {-1.f,1.f})
        {
            Shape(FVector(SkyCentre.X+D*S*.5f,SkyCentre.Y,(Z0+Z1)*.5f),FVector(4,S,Z0-Z1),TEXT("PantrySky"));
            Shape(FVector(SkyCentre.X,SkyCentre.Y+D*S*.5f,(Z0+Z1)*.5f),FVector(S,4,Z0-Z1),TEXT("PantrySky"));
        }
        auto IslandLayer=[&](float Top,float Bottom,float TopRadius,float BottomRadius,bool Earth)
        {
            constexpr int32 Points=24;
            TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FLinearColor> Colors;
            for(int32 L=0;L<2;++L) for(int32 I=0;I<Points;++I)
            {
                const float A=I*UE_TWO_PI/Points,Jag=1.f+.08f*FMath::Sin(5*A)+.04f*FMath::Cos(9*A);
                const float R=(L?BottomRadius:TopRadius)*Jag;
                V.Add(FVector(SkyCentre.X+R*FMath::Cos(A),SkyCentre.Y+R*FMath::Sin(A),(L?Bottom:Top)+(L?4.f:2.f)*FMath::Sin(7*A)));
                N.Add(FVector(FMath::Cos(A),FMath::Sin(A),-.2f).GetSafeNormal());UV.Add(FVector2D(I*.24f,L*.7f));
                Colors.Add(Earth?FLinearColor(.16f,.085f,.038f)*( .9f+.1f*FMath::Sin(11*A)):FLinearColor::White);
            }
            V.Add(FVector(SkyCentre.X,SkyCentre.Y,Top));V.Add(FVector(SkyCentre.X,SkyCentre.Y,Bottom-3));
            N.Add(FVector::UpVector);N.Add(-FVector::UpVector);UV.Add(FVector2D(.5f,.5f));UV.Add(FVector2D(.5f,.5f));
            Colors.Add(Earth?FLinearColor(.16f,.085f,.038f):FLinearColor::White);Colors.Add(Earth?FLinearColor(.12f,.065f,.028f):FLinearColor::White);
            for(int32 I=0;I<Points;++I)
            {
                const int32 Next=(I+1)%Points;
                T.Append({I,I+Points,Next,Next,I+Points,Next+Points,2*Points,I,Next,2*Points+1,Next+Points,I+Points});
            }
            auto* Mesh=NewObject<UProceduralMeshComponent>(Owner);Mesh->SetupAttachment(Root);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();
            Mesh->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),false);
            Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Earth?TEXT("/Game/Art/Materials/M_VistaTerrain.M_VistaTerrain"):TEXT("/Game/Art/Materials/M_Stone.M_Stone")));
        };
        IslandLayer(Floor-27,Floor-56,50,43,false);
        IslandLayer(Floor-54,Floor-82,43,27,true);
        const FVector Puffs[]={FVector(-260,620,-520),FVector(-90,770,-610),FVector(-210,815,-470),FVector(-115,575,-760),FVector(-300,740,-830),FVector(-40,660,-900)};
        for(int32 I=0;I<UE_ARRAY_COUNT(Puffs);++I)
            for(int32 K=0;K<3;++K)
                Shape(Puffs[I]+FVector(K*38-38,(K%2)*22,K==1?14:0),FVector(95+I*9,70+K*8,46+K*6),TEXT("PantryCloud"),false,2);
        auto* Day=NewObject<UPointLightComponent>(Owner);Day->SetupAttachment(Root);Day->SetRelativeLocation(FVector(SkyCentre.X,SkyCentre.Y,Floor-170));
        Day->SetIntensity(5200);Day->SetAttenuationRadius(760);Day->SetLightColor(FLinearColor(.72f,.86f,1));Day->SetSourceRadius(160);Day->SetCastShadows(false);Day->RegisterComponent();
        for(int32 I=0;I<26;++I){ const float A=I*.2417f*UE_TWO_PI; Chip(SkyCentre.X+FMath::Cos(A)*(SkyRadius+12),SkyCentre.Y+FMath::Sin(A)*(SkyRadius+12)); }
        // On the island, a crate, and on the crate the cheese.
        const FVector Crate(SkyCentre.X,SkyCentre.Y,Floor+28);
        Shape(Crate,FVector(56,56,56),TEXT("Wood"),true);
        for(float Z : {-18.f,18.f}) for(float D : {-1.f,1.f})
        {
            Shape(Crate+FVector(D*29,0,Z),FVector(2,58,7),TEXT("WoodLight"));
            Shape(Crate+FVector(0,D*29,Z),FVector(58,2,7),TEXT("WoodLight"));
        }
        Shape(Crate+FVector(0,0,36),FVector(34,34,15),TEXT("Cheese"),true,1);
        Shape(Crate+FVector(4,-3,44),FVector(16,4,2),TEXT("Cheese"),true,0,FRotator(0,35,0));
    }
    // Stocked racks against the walls (Codex's), their lowest shelf's jars
    // breakable (as the 2D pantry's shelf jars): each spills cigarettes.
    struct FRack { FVector P; bool bAlongX; };
    const FRack Racks[]={{FVector(295,600,Floor),false},{FVector(-250,1068,Floor),true},{FVector(230,1068,Floor),true}};
    int32 JarsPlaced=0;
    for(const FRack& Rack : Racks)
    {
        const FVector Along=Rack.bAlongX?FVector(1,0,0):FVector(0,1,0), Across=Rack.bAlongX?FVector(0,1,0):FVector(1,0,0);
        const FVector Into=Rack.bAlongX?FVector(0,-1,0):FVector(-1,0,0);   // toward the room
        const auto Sized=[&](float A,float B,float H){ return Along*A+Across*B+FVector(0,0,H); };
        for(float L : {-72.f,72.f}) for(float W : {-22.f,22.f}) Shape(Rack.P+Along*L+Across*W+FVector(0,0,87),Sized(7,7,174),TEXT("Wood"),true);
        for(float Z : {20.f,78.f,136.f})
        {
            Shape(Rack.P+FVector(0,0,Z),Sized(159,57,6),TEXT("WoodLight"),true);
            for(int32 I=0;I<4;++I)
            {
                const FVector Jar=Rack.P+Along*(-57+I*38)+FVector(0,0,Z+17);
                if(Z<30.f && I%2==0)
                {
                    if(AClayJar* Real=AClayJar::PlaceAt(World,Jar+Into*14.f-FVector(0,0,14),Chips.FRandRange(0,360),1+(JarsPlaced%2))) ++JarsPlaced;
                    continue;
                }
                if(I%2)
                {
                    Shape(Jar,FVector(27,29,25),TEXT("Plaster"),false,2);
                    Shape(Jar+FVector(0,0,14),FVector(12,12,6),TEXT("Wood"),false,1);
                }
                else
                {
                    Shape(Jar,FVector(21,21,27),TEXT("Amber"),false,1);
                    Shape(Jar+FVector(0,0,15),FVector(23,23,3),TEXT("Wood"),false,1);
                }
            }
        }
    }
    // Clay jars standing on the floor, as the 2D pantry's floor jars.
    for(const FVector2D J : {FVector2D(160,640),FVector2D(120,470),FVector2D(-130,975),FVector2D(250,990)})
        if(AClayJar::PlaceAt(World,FVector(J.X,J.Y,Floor),Chips.FRandRange(0,360),1+(JarsPlaced%3))) ++JarsPlaced;
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    for(const FVector P : {FVector(310,885,-275),FVector(305,955,-275),FVector(-300,990,-275)})
    {
        Shape(P,FVector(62,62,90),TEXT("Wood"),true,1);
        if(BarrelMesh)
        {
            auto* Art=NewObject<UInstancedStaticMeshComponent>(Owner);Art->SetupAttachment(Root);Art->SetStaticMesh(BarrelMesh);
            Art->SetCollisionProfileName(TEXT("NoCollision"));Art->RegisterComponent();Art->AddInstance(FTransform(P));
        }
        for(float Z : {-30.f,30.f}) Shape(P+FVector(0,0,Z),FVector(65,65,6),TEXT("Dark"),false,1);
    }
    // Grain sacks along the south wall, a work crate by the east rack.
    for(int32 I=0;I<4;++I)
    {
        const FVector P(130+I*52,435,-299);
        Shape(P,FVector(47,50,42),TEXT("Plaster"),true,2);
        Shape(P+FVector(0,0,22),FVector(14,14,8),TEXT("Wood"),false,1);
    }
    Shape(FVector(305,750,-290),FVector(70,86,60),TEXT("Wood"),true);
    for(float X : {275.f,335.f}) Shape(FVector(X,750,-262),FVector(6,92,5),TEXT("WoodLight"));
    for(const FVector P : {FVector(X0+14,860,-162),FVector(X1-14,790,-158)})
    {
        Shape(P+FVector(P.X<0?-8:8,0,0),FVector(3,24,35),TEXT("Dark"));
        for(float Y : {-10.f,10.f}) Shape(P+FVector(0,Y,0),FVector(3,3,32),TEXT("Dark"));
        AddDockFlame(Owner,P+FVector(0,0,-9),12,24);
        for(float Z : {-18.f,18.f}) Shape(P+FVector(0,0,Z),FVector(25,27,4),TEXT("Dark"));
        auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Root);Lamp->SetRelativeLocation(P+FVector(P.X<0?20:-20,0,0));
        Lamp->SetIntensity(650);Lamp->SetAttenuationRadius(480);Lamp->SetLightColor(FLinearColor(1,.69f,.4f));Lamp->SetSourceRadius(12);Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_BUILT size=%.0fx%.0f rifts=%d sky_ring_cm=%.0f jars=%d ladders=%d"),Span.X,Span.Y,UE_ARRAY_COUNT(Rifts),SkyRadius-IslandRadius,JarsPlaced,GetChuckClimbables().Num());
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryAdjustmentsTest")))
    {
        FTimerHandle Test;
        World->GetTimerManager().SetTimer(Test,[World,Oil,OilRims,OilMeshes](){
            int32 Failures=!Oil || OilRims!=6;
            for(auto* Mesh : OilMeshes) if(Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++Failures;
            FHitResult Hit;const FVector Cheese(SkyCentre.X,SkyCentre.Y,Floor+64);
            // Keep the sweep above the supporting crate so it tests the cheese.
            const bool Solid=World->SweepSingleByChannel(Hit,Cheese-FVector(50,0,0),Cheese+FVector(50,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(3)) &&
                Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("PantryCheese"));
            if(!Solid) UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_CHEESE_COLLISION_MISS component=%s point=%s"),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());
            Failures+=!Solid;
            const FVector FloorEdge(SkyCentre.X+40,SkyCentre.Y,Floor);
            const bool IslandFloor=World->LineTraceSingleByChannel(Hit,FloorEdge+FVector(0,0,20),FloorEdge-FVector(0,0,100),ECC_Visibility);
            const FVector Under(SkyCentre.X+20,SkyCentre.Y,Floor-130);
            const bool UnderClear=!World->LineTraceSingleByChannel(Hit,Under,Under-FVector(0,0,570),ECC_Visibility);
            Failures+=!IslandFloor || !UnderClear;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_ISLAND_CHECK failures=%d floor=%d underfloor_clear=%d"),!IslandFloor || !UnderClear,IslandFloor,UnderClear);
            auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            ADockNPC* Keeper=nullptr;for(TActorIterator<ADockNPC> NPC(World);NPC;++NPC) if(NPC->IsKeeper()) Keeper=*NPC;
            bool CellarSilent=true,SurfaceTalk=false;
            if(Chuck && Keeper)
            {
                for(const FVector P : {DockPantryStartLocation(),FVector(Keeper->GetActorLocation().X,Keeper->GetActorLocation().Y,Floor+34.65f)})
                {
                    Chuck->ResetAtLocation(P);Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                    CellarSilent&=Chuck->GetTalkPrompt()==nullptr;Chuck->Interact();CellarSilent&=!Chuck->IsTalking();
                }
                Chuck->ResetAtLocation(Keeper->GetActorLocation()+FVector(35,0,34.65f));
                Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                SurfaceTalk=Chuck->GetTalkPrompt()==Keeper;Chuck->Interact();SurfaceTalk&=Chuck->GetTalkingTo()==Keeper;
                Chuck->ResetToDock();
            }
            else ++Failures;
            Failures+=!CellarSilent || !SurfaceTalk;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_ADJUSTMENTS_CHECK failures=%d oil_rims=%d noncolliding=1 cheese_blocks=%d cellar_talk=%d surface_talk=%d"),Failures,OilRims,Solid,!CellarSilent,SurfaceTalk);
            World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
        },3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckDemoEndTest")))
    {
        auto Failures=MakeShared<int32>(0);
        FTimerHandle FloorTest,RiftTest,SkyTest,Check,Shot,Escape,Watchdog;
        const auto Player=[World](){return Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());};
        World->GetTimerManager().SetTimer(FloorTest,[Player](){Player()->ResetAtLocation(DockPantryStartLocation());},2.f,false);
        World->GetTimerManager().SetTimer(RiftTest,[Player,Failures](){
            *Failures+=Player()->HasDemoEnded();
            Player()->ResetAtLocation(FVector(240,520,Floor+35));
            Player()->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        },3.f,false);
        World->GetTimerManager().SetTimer(SkyTest,[Player,Failures](){
            *Failures+=Player()->HasDemoEnded();
            Player()->ResetAtLocation(FVector(SkyCentre.X+180,SkyCentre.Y,Floor+35));
            Player()->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        },4.f,false);
        World->GetTimerManager().SetTimer(Check,[Player,Failures](){
            auto* Chuck=Player();const bool Ended=Chuck->HasDemoEnded();
            *Failures+=!Ended || Chuck->GetCharacterMovement()->MovementMode!=MOVE_None;
            const FVector At=Chuck->GetActorLocation();Chuck->ResetToDock();
            *Failures+=!Chuck->GetActorLocation().Equals(At,1.f);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_DEMO_END_TEST failures=%d sky_ends=%d floor_and_astral_continue=1 reset_blocked=1"),*Failures,Ended);
        },6.f,false);
        World->GetTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Demo/End.png"),true,false);},6.5f,false);
        World->GetTimerManager().SetTimer(Escape,[World](){
            World->GetFirstPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape,IE_Pressed,1));
        },8.f,false);
        World->GetTimerManager().SetTimer(Watchdog,[World](){
            UE_LOG(LogTemp,Error,TEXT("CHUCK_DEMO_EXIT_TEST failures=1 escape_did_not_exit=1"));
            World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
        },10.f,false);
    }
    // A walking circuit round the room that stays on whole floor.
    const TArray<FVector> Walk={FVector(75,915,Floor),FVector(75,800,Floor),FVector(230,640,Floor),FVector(10,1000,Floor),FVector(75,915,Floor)};
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")) || FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryTest")))
    {
        int32 Failures=0;FHitResult Hit;
        for(const FVector P : Walk) if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,20),P-FVector(0,0,30),ECC_Visibility) || FMath::Abs(Hit.ImpactPoint.Z-Floor)>1) ++Failures;
        for(int32 I=1;I<Walk.Num();++I)
            if(World->SweepSingleByChannel(Hit,Walk[I-1]+FVector(0,0,35),Walk[I]+FVector(0,0,35),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failures;
        if(World->SweepSingleByChannel(Hit,FVector(75,915,35),FVector(75,915,-280),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f)))
        {++Failures;UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_SHAFT_BLOCK actor=%s component=%s point=%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());}
        for(const FVector P : {FVector(X0-10,750,-230),FVector(X1+10,750,-230),FVector(-50,Y0-10,-230),FVector(-50,Y1+10,-230)})
            if(!World->LineTraceSingleByChannel(Hit,FVector(-50,750,-230),P,ECC_Visibility)) ++Failures;
        // The holes are real: nothing under them to stand on.
        int32 Open=0;
        for(const FVector2D H : {FVector2D(Rifts[0].X,Rifts[0].Y),FVector2D(Rifts[1].X,Rifts[1].Y),FVector2D(SkyCentre.X+SkyRadius*.6f,SkyCentre.Y),FVector2D(SkyCentre.X,SkyCentre.Y-SkyRadius*.6f)})
            if(!World->LineTraceSingleByChannel(Hit,FVector(H.X,H.Y,Floor+20),FVector(H.X,H.Y,Floor-1200),ECC_Visibility)) ++Open;
        if(Open<4) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_CHECK failures=%d floors=5 routes=4 shaft=1 walls=4 open_holes=%d"),Failures,Open);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(85);
        const FVector Views[]={FVector(205,940,115),FVector(75,915,-65),FVector(300,1060,-125),FVector(110,560,-205),FVector(-60,760,-160)};
        const FVector Targets[]={FVector(75,915,-15),FVector(75,840,-305),FVector(-170,690,-320),FVector(-170,690,-290),FVector(-180,680,-750)};
        constexpr int32 ViewCount=UE_ARRAY_COUNT(Views);
        for(int32 I=0;I<ViewCount;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Views[I],T=Targets[I]](){Camera->SetActorLocationAndRotation(P,(T-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){const FString Folder=FPaths::ScreenShotDir()/TEXT("Pantry");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},5.f+ViewCount*4.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryTest")))
    {
        struct FRun {int32 Target=1,Pass=0;float Time=0;bool Landed=false;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
        FTimerHandle Start;World->GetTimerManager().SetTimer(Start,[World,Run,Walk](){
            MarkDockSewerExited();auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());Chuck->ResetToDock();
            Chuck->SetActorLocation(FVector(75,915,40));Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Run,Walk,Chuck](){
                Run->Time+=.02f;
                if(FMath::Fmod(Run->Time,5.f)<.02f) UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_POSITION %s grounded=%d"),*Chuck->GetActorLocation().ToString(),Chuck->GetCharacterMovement()->IsMovingOnGround());
                if(!Run->Landed && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetActorLocation().Z<-250)
                {Run->Landed=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_LANDED z=%.2f"),Chuck->GetActorLocation().Z);}
                if(Run->Landed)
                {
                    const FVector Goal=Walk[Run->Target]+FVector(0,0,34.65f);
                    if(FVector::Dist2D(Chuck->GetActorLocation(),Goal)<18) ++Run->Target;
                    else Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                    if(Run->Target>=Walk.Num()){++Run->Pass;Run->Target=1;Chuck->ToggleCamera();}
                }
                if(Run->Pass>=2 || Run->Time>50)
                {
                    const bool Passed=Run->Pass>=2 && IsWithinDockPantry(Chuck->GetActorLocation()) && Chuck->GetRespawns()==0;
                    Chuck->ResetToDock();
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_TEST_COMPLETE failures=%d fell=%d circuits=%d surface_reset=%d elapsed=%.2f"),Passed?0:1,Run->Landed,Run->Pass,Chuck->GetActorLocation().Z>0,Run->Time);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
}
