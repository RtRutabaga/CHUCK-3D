#include "DockForge.h"
#include "DockFire.h"
#include "DockReturn.h"
#include "SewerSlide.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void BuildDockForge(AActor* Owner)
{
    UWorld* World=Owner->GetWorld();
    const FVector F(-780,-3724,0); // Retains Claude's smith/forge sound alignment.
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto Material=[](const TCHAR* Name)
    {return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Name,Name));};
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Mat,bool Solid=false,UStaticMesh* Asset=nullptr,FRotator R=FRotator::ZeroRotator)
    {
        if(!Asset) Asset=Cube;
        const FString Key=FString(Mat)+Asset->GetName()+(Solid?TEXT("solid"):TEXT("detail"));
        auto*& B=Batches.FindOrAdd(Key);
        if(!B)
        {
            B=NewObject<UInstancedStaticMeshComponent>(Owner); B->SetupAttachment(Owner->GetRootComponent());
            B->SetStaticMesh(Asset); B->SetMaterial(0,Material(Mat));
            B->ComponentTags.Add(TEXT("ForgeDressing"));
            B->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision")); B->RegisterComponent();
        }
        B->AddInstance(FTransform(R,P,Size/100.f));
    };
    auto Beam=[&](FVector A,FVector B,float Width,const TCHAR* Mat)
    { const FVector D=B-A; Shape((A+B)*.5f,FVector(D.Size(),Width,Width),Mat,false,nullptr,D.Rotation()); };
    // Broad raised hearth, stone footing, coal tray and open arched furnace.
    Shape(F+FVector(0,0,8),FVector(198,116,16),TEXT("Stone"),true);
    Shape(F+FVector(0,0,41),FVector(180,100,66),TEXT("ForgeBrick"),true);
    Shape(F+FVector(0,0,76),FVector(176,102,7),TEXT("ForgeIron"),true);
    Shape(F+FVector(0,0,82),FVector(127,76,8),TEXT("ForgeAsh"));
    for(float X : {-81.f,81.f}) Shape(F+FVector(X,0,131),FVector(26,100,104),TEXT("ForgeBrick"),true);
    Shape(F+FVector(0,-30,152),FVector(180,46,146),TEXT("ForgeBrick"),true);

    // Extruded voussoirs retain an actual opening, rather than painting one on a box.
    TArray<FVector> V,N; TArray<int32> Tri; TArray<FVector2D> UV;
    auto Quad=[&](FVector A,FVector B,FVector C,FVector D)
    {
        const int32 I=V.Num(); V.Append({A,B,C,D}); Tri.Append({I,I+1,I+2,I,I+2,I+3});
        const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        for(int32 J=0;J<4;++J) N.Add(Normal);
        UV.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)});
    };
    auto Surface=[&](const TCHAR* Mat,bool Solid,FVector At)
    {
        auto* M=NewObject<UProceduralMeshComponent>(Owner); M->SetupAttachment(Owner->GetRootComponent());
        M->ComponentTags.Add(TEXT("ForgeDressing")); M->RegisterComponent(); M->SetWorldLocation(At);
        M->CreateMeshSection_LinearColor(0,V,Tri,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),Solid);
        M->SetMaterial(0,Material(Mat)); M->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
        V.Reset(); N.Reset(); Tri.Reset(); UV.Reset(); return M;
    };
    for(int32 I=0;I<11;++I)
    {
        const float A=PI*I/11+.008f, B=PI*(I+1)/11-.008f;
        const FVector IA(68*FMath::Cos(A),0,131+68*FMath::Sin(A)), IB(68*FMath::Cos(B),0,131+68*FMath::Sin(B));
        const FVector OA(94*FMath::Cos(A),0,131+94*FMath::Sin(A)), OB(94*FMath::Cos(B),0,131+94*FMath::Sin(B));
        const FVector Front(0,50,0),Back(0,-50,0);
        Quad(IA+Front,IB+Front,OB+Front,OA+Front); Quad(IB+Back,IA+Back,OA+Back,OB+Back);
        Quad(IA+Back,IB+Back,IB+Front,IA+Front); Quad(OA+Front,OB+Front,OB+Back,OA+Back);
        Quad(IA+Front,OA+Front,OA+Back,IA+Back); Quad(IB+Back,OB+Back,OB+Front,IB+Front);
    }
    auto* Arch=Surface(TEXT("ForgeBrick"),true,F);
    // Riveted sheet-iron hood tapers into a tall working chimney, above the roof.
    const FVector Low[]={FVector(-96,-54,225),FVector(96,-54,225),FVector(96,54,225),FVector(-96,54,225)};
    const FVector High[]={FVector(-36,-30,307),FVector(36,-30,307),FVector(36,30,307),FVector(-36,30,307)};
    for(int32 I=0;I<4;++I) Quad(Low[I],Low[(I+1)%4],High[(I+1)%4],High[I]);
    Surface(TEXT("ForgeIron"),true,F);
    Shape(F+FVector(0,0,514),FVector(72,60,414),TEXT("ForgeBrick"),true);
    for(float Z : {319.f,420.f,610.f})
    {
        Shape(F+FVector(0,0,Z),FVector(79,67,6),TEXT("ForgeIron"));
        for(float X : {-28.f,28.f}) Shape(F+FVector(X,35,Z),FVector(4,3,4),TEXT("ForgeIron"),false,Sphere);
    }
    for(float X : {-42.f,42.f}) Shape(F+FVector(X,0,730),FVector(12,83,18),TEXT("Stone"),true);
    for(float Y : {-36.f,36.f}) Shape(F+FVector(0,Y,730),FVector(72,12,18),TEXT("Stone"),true);
    Shape(F+FVector(0,0,721),FVector(68,56,2),TEXT("ForgeAsh"));
    // Strap seams and rivets give the hood a handmade construction read.
    for(float X : {-65.f,0.f,65.f})
    {
        Beam(F+FVector(X,55,226),F+FVector(X*.375f,31,303),3,TEXT("ForgeIron"));
        for(int32 I=0;I<3;++I) Shape(F+FVector(X*(1-I*.20f),57-I*8,234+I*25),FVector(3,3,3),TEXT("Metal"),false,Sphere);
    }
    // Coal lies in the firepot; the mouth remains below the arch, not on its facade.
    for(int32 I=0;I<90;++I)
    {
        const float X=-56+(I%10)*12, Y=5+(I/10)*4;
        Shape(F+FVector(X,Y,89+(I%3)*2),FVector(7+(I%3)*2,7+(I%2)*2,5+(I%4)),I%11==0?TEXT("FireEmber"):TEXT("ForgeAsh"),false,Cube,FRotator(I*17,I*37,I*11));
    }
    for(int32 I=0;I<4;++I) AddDockFlame(Owner,F+FVector(-27+I*18,6,91),19,31+(I%2)*9);
    auto* Glow=NewObject<UPointLightComponent>(Owner); Glow->SetupAttachment(Owner->GetRootComponent());
    Glow->SetRelativeLocation(F+FVector(0,28,112)); Glow->SetIntensity(1000); Glow->SetAttenuationRadius(330);
    Glow->SetLightColor(FLinearColor(1,.32f,.07f)); Glow->SetCastShadows(false); Glow->RegisterComponent();
    // A restrained drifting plume, through the chimney rather than a room fog.
    for(int32 Sheet=0;Sheet<2;++Sheet)
    {
        const FVector Side=Sheet==0?FVector(1,0,0):FVector(0,1,0);
        for(int32 I=0;I<6;++I)
        {
            const float H=I*48.f, Width=35+I*10.f;
            const FVector A(I*9.f,0,H), B((I+1)*9.f,0,H+48);
            const int32 Offset=UV.Num();
            Quad(A-Side*Width*.5f,A+Side*Width*.5f,B+Side*(Width+10)*.5f,B-Side*(Width+10)*.5f);
            UV[Offset]=FVector2D(0,I/6.f); UV[Offset+1]=FVector2D(1,I/6.f);
            UV[Offset+2]=FVector2D(1,(I+1)/6.f); UV[Offset+3]=FVector2D(0,(I+1)/6.f);
        }
    }
    auto* Smoke=Surface(TEXT("ForgeSmoke"),false,F+FVector(0,0,734)); Smoke->SetCastShadow(false);

    // Side-fed bellows: timber cheeks, leather folds, nozzle entering the firepot.
    const FVector Bellows=F+FVector(117,-10,96);
    Beam(F+FVector(76,-10,86),Bellows,12,TEXT("ForgeIron"));
    for(float Z : {-16.f,16.f}) Shape(Bellows+FVector(17,0,Z),FVector(71,46,6),TEXT("AgedDockTimber"),false,nullptr,FRotator(0,0,Z*.35f));
    for(int32 I=0;I<6;++I) Shape(Bellows+FVector(18,-19+I*7.5f,0),FVector(60,6,26),TEXT("Leather"),false,nullptr,FRotator(0,0,(I%2?1:-1)*8));
    for(float Y : {-30.f,30.f}) Shape(Bellows+FVector(25,Y,-56),FVector(10,10,76),TEXT("AgedDockTimber"),true);
    Beam(Bellows+FVector(48,0,22),Bellows+FVector(70,0,28),8,TEXT("AgedDockTimber"));

    // Worn workbench beside the existing dry fuel stack, outside the smith's reach.
    const FVector Bench(-1150,-3695,0);
    for(float X : {-66.f,66.f}) for(float Y : {-30.f,30.f})
        Shape(Bench+FVector(X,Y,43),FVector(12,12,86),TEXT("AgedDockTimber"),true);
    Shape(Bench+FVector(0,0,87),FVector(154,83,10),TEXT("AgedDockTimber"),true);
    for(float Y : {-28.f,0.f,28.f}) Shape(Bench+FVector(0,Y,94),FVector(151,25,4),TEXT("WoodLight"));
    Shape(Bench+FVector(0,0,24),FVector(139,68,8),TEXT("AgedDockTimber"));
    for(int32 I=0;I<6;++I) Shape(Bench+FVector(-50+I*18,0,33),FVector(10,59,10),TEXT("ForgeAsh"));
    // Small vise, stock rods and a hanging rack of tongs/pokers/unfinished ironwork.
    Shape(Bench+FVector(53,17,108),FVector(23,27,26),TEXT("ForgeIron"));
    for(float X : {44.f,62.f}) Shape(Bench+FVector(X,17,124),FVector(8,33,8),TEXT("Metal"));
    Beam(Bench+FVector(53,-4,105),Bench+FVector(53,-31,105),4,TEXT("ForgeIron"));
    for(int32 I=0;I<5;++I) Shape(Bench+FVector(-44+I*12,0,100),FVector(5,51+I*3,5),TEXT("ForgeIron"),false,nullptr,FRotator(0,I*3,0));
    Beam(FVector(-1230,-3737,214),FVector(-1060,-3737,214),8,TEXT("AgedDockTimber"));
    for(int32 I=0;I<5;++I)
    {
        const FVector Top(-1207+I*31,-3724,204),Bottom=Top-FVector(0,0,55+(I%3)*13);
        Beam(Top,Bottom,3,TEXT("ForgeIron"));
        Beam(Top,Top+FVector(0,0,9),5,TEXT("ForgeIron"));
        Beam(Bottom,Bottom+FVector(8,0,-9),3,TEXT("ForgeIron"));
        if(I%2==0) Beam(Top+FVector(7,0,0),Bottom+FVector(7,0,-8),3,TEXT("ForgeIron"));
    }
    // Stave quenching tub: hollow rim, visible dark water, iron hoops and real body collision.
    const FVector Tub(-1110,-3520,0);
    Shape(Tub+FVector(0,0,22),FVector(60,60,44),TEXT("AgedDockTimber"),true,Cylinder);
    for(int32 I=0;I<18;++I)
    {
        const float A=I*2*PI/18;
        Shape(Tub+FVector(30*FMath::Cos(A),30*FMath::Sin(A),25),FVector(11,5,50),TEXT("AgedDockTimber"),false,nullptr,FRotator(0,FMath::RadiansToDegrees(A)+90,0));
        for(float Z : {9.f,39.f}) Shape(Tub+FVector(31*FMath::Cos(A),31*FMath::Sin(A),Z),FVector(11,3,4),TEXT("ForgeIron"),false,nullptr,FRotator(0,FMath::RadiansToDegrees(A)+90,0));
    }
    Shape(Tub+FVector(0,0,45),FVector(53,53,2),TEXT("FountainWater"),false,Cylinder);
    Beam(Tub+FVector(-42,8,52),Tub+FVector(42,8,52),8,TEXT("AgedDockTimber"));
    // Stock offcuts, clinker and a coal basket read as work, rather than rubbish piles.
    for(int32 I=0;I<8;++I) Shape(F+FVector(112+I*7,50,7+(I%3)*5),FVector(8,40+I*3,7),TEXT("ForgeIron"),false,nullptr,FRotator(0,I*7,0));
    for(int32 I=0;I<10;++I) Shape(F+FVector(-63+I*12,66+(I%3)*8,3),FVector(8,6,3),TEXT("ForgeAsh"),false,Sphere);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failures=0;
        for(const TCHAR* Name : {TEXT("ForgeBrick"),TEXT("ForgeIron"),TEXT("ForgeAsh"),TEXT("ForgeSmoke")}) if(!Material(Name)) ++Failures;
        for(const FVector P : {F+FVector(0,0,75),Bench+FVector(0,0,90),Tub+FVector(0,0,40),F+FVector(0,0,710)})
        { FHitResult H; if(!World->LineTraceSingleByChannel(H,P+FVector(0,0,40),P-FVector(0,0,80),ECC_Visibility) || !H.GetComponent() || !H.GetComponent()->ComponentHasTag(TEXT("ForgeDressing"))) ++Failures; }
        // Front walking line and the smith/anvil gap must remain free for Chuck.
        for(const FVector P : {FVector(-950,-3490,35),FVector(-900,-3660,35),FVector(-1030,-3600,35)})
        { FHitResult H; if(World->SweepSingleByChannel(H,P,P+FVector(0,10,0),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failures; }
        if(!Arch->GetMaterial(0) || Smoke->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_FORGE_DRESSING failures=%d materials=4 solid_samples=4 clear_routes=3"),Failures);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckForgeCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(62);
        const FVector Views[]={FVector(-1290,-3290,240),FVector(-620,-3450,160),FVector(-800,-3540,98),FVector(-1290,-3290,240),FVector(-700,-3470,280),FVector(-1160,-3520,570)};
        const FVector Targets[]={FVector(-935,-3690,190),FVector(-790,-3700,170),FVector(-780,-3724,130),FVector(-935,-3690,190),FVector(-780,-3724,170),FVector(-780,-3740,600)};
        for(int32 I=0;I<6;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,I,P=Views[I],T=Targets[I]]()
            { if(I==3) MarkDockSewerExited(); Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); World->GetFirstPlayerController()->SetViewTarget(Camera); },4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I]()
            {const FString Folder=FPaths::ScreenShotDir()/TEXT("Forge"); IFileManager::Get().MakeDirectory(*Folder,true); FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit; World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},30.f,false);
    }
}
