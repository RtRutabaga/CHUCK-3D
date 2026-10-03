#include "DockWeathering.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
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

void BuildDockWeathering(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>(); Owner->Tags.Add(TEXT("DockWeathering"));
    auto* Root=NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
    auto* Stems=NewObject<UInstancedStaticMeshComponent>(Owner); Stems->SetupAttachment(Root);
    Stems->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Stems->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Wood.M_Wood")));
    Stems->SetCollisionEnabled(ECollisionEnabled::NoCollision); Stems->RegisterComponent();
    auto* RenderEdges=NewObject<UInstancedStaticMeshComponent>(Owner); RenderEdges->SetupAttachment(Root);
    RenderEdges->SetStaticMesh(Stems->GetStaticMesh());
    RenderEdges->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_WeatheredPlaster.M_WeatheredPlaster")));
    RenderEdges->SetCollisionEnabled(ECollisionEnabled::NoCollision); RenderEdges->RegisterComponent();
    TArray<FVector> Leaves,Normals,Chips,ChipNormals; TArray<int32> Faces,ChipFaces;
    TArray<FVector2D> UV,ChipUV; TArray<FLinearColor> Colors,ChipColors;
    FRandomStream Rng(48391);
    auto Stem=[&](FVector A,FVector B,float Width)
    { const FVector D=B-A; Stems->AddInstance(FTransform(D.Rotation(),(A+B)*.5f,FVector(D.Size(),Width,Width)/100)); };
    auto Leaf=[&](FVector P,FVector N,FVector Along,float Size)
    {
        const FVector Up=FVector::UpVector;
        const float Angle=Rng.FRandRange(-70,70);
        const FVector U=Along.RotateAngleAxis(Angle,N), V=Up.RotateAngleAxis(Angle,N);
        const int32 Start=Leaves.Num();
        // Lobed, pointed leaf silhouette, with a slight raised central vein.
        const FVector2D Outline[]={FVector2D(0,.65f),FVector2D(.28f,.12f),FVector2D(.62f,.18f),FVector2D(.4f,-.2f),FVector2D(.42f,-.43f),FVector2D(0,-.3f),FVector2D(-.42f,-.43f),FVector2D(-.4f,-.2f),FVector2D(-.62f,.18f),FVector2D(-.28f,.12f)};
        const FLinearColor C=FMath::Lerp(FLinearColor(.028f,.065f,.018f),FLinearColor(.09f,.16f,.035f),Rng.FRand());
        Leaves.Add(P+N*1.2f); Normals.Add(N); UV.Add(FVector2D(.5f,.5f)); Colors.Add(C);
        for(auto O : Outline) { Leaves.Add(P+(U*O.X+V*O.Y)*Size); Normals.Add(N); UV.Add(FVector2D(O.X*.7f+.5f,O.Y*.8f+.4f)); Colors.Add(C); }
        for(int32 I=0;I<10;++I) Faces.Append({Start,Start+1+I,Start+1+(I+1)%10});
    };
    auto Ivy=[&](FVector Base,FVector N,float Width,float Height)
    {
        const FVector Along=FVector::CrossProduct(FVector::UpVector,N);
        for(int32 Branch=0;Branch<8;++Branch)
        {
            FVector Prev=Base+Along*Rng.FRandRange(-Width*.18f,Width*.18f);
            const float Reach=Height*Rng.FRandRange(.55f,1);
            const float Drift=Rng.FRandRange(-Width*.5f,Width*.5f);
            for(int32 Step=1;Step<=12;++Step)
            {
                const float T=Step/12.f;
                const FVector Next=Base+Along*(Drift*T+FMath::Sin(T*9+Branch)*12)+FVector(0,0,Reach*T)+N*3;
                Stem(Prev,Next,Branch<2?1.6f:.8f);
                for(int32 I=0;I<4;++I)
                {
                    const FVector P=FMath::Lerp(Prev,Next,Rng.FRand())+Along*Rng.FRandRange(-15,15)+N*Rng.FRandRange(2,6);
                    Leaf(P,N,Along,Rng.FRandRange(9,19));
                }
                Prev=Next;
            }
        }
    };
    // Selected shaded stone corners; doors, torch flames and roof routes stay clear.
    Ivy(FVector(-1317,-1260,22),FVector(1,0,0),110,310);
    Ivy(FVector(-1257,615,20),FVector(1,0,0),105,350);
    Ivy(FVector(-722,300,18),FVector(-1,0,0),85,260);
    Ivy(FVector(-1560,1649,20),FVector(0,1,0),70,280);
    Ivy(FVector(884,2990,20),FVector(1,0,0),80,360);
    Ivy(FVector(-1280,-4320,20),FVector(0,1,0),100,300);
    // Irregular exposed masonry patches where old lime render has fallen away.
    auto Spall=[&](FVector P,FVector N,float W,float H)
    {
        const FVector U=FVector::CrossProduct(FVector::UpVector,N), V=FVector::UpVector;
        const int32 Start=Chips.Num();
        Chips.Add(P+N*.6f); ChipNormals.Add(N); ChipUV.Add(FVector2D(.5f,.5f)); ChipColors.Add(FLinearColor::White);
        for(int32 I=0;I<16;++I)
        {
            const float A=I*2*PI/16,R=Rng.FRandRange(.65f,1);
            const FVector Q=P+U*(FMath::Cos(A)*W*.5f*R)+V*(FMath::Sin(A)*H*.5f*R);
            Chips.Add(Q); ChipNormals.Add(N); ChipUV.Add(FVector2D(FMath::Cos(A)*.5f+.5f,FMath::Sin(A)*.5f+.5f)); ChipColors.Add(FLinearColor::White);
            // Thin, chipped render rim instead of rectangular masonry stickers.
            const float Edge=Rng.FRandRange(2,8);
            RenderEdges->AddInstance(FTransform(FRotator::ZeroRotator,Q+N*.7f+V*(Edge*.5f),FVector(1.3f,1.3f,Edge)/100));
        }
        for(int32 I=0;I<16;++I)
        {
            const int32 A=Start+1+I,B=Start+1+(I+1)%16;
            ChipFaces.Append({Start,A,B,Start,B,A}); // Stone material is single-sided.
        }
    };
    Spall(FVector(-1367,-1950,95),FVector(1,0,0),100,140);
    Spall(FVector(-1297,-185,205),FVector(1,0,0),65,95);
    Spall(FVector(-607,-1800,110),FVector(1,0,0),80,155);
    Spall(FVector(-410,1649,80),FVector(0,1,0),95,125);
    Spall(FVector(884,2680,190),FVector(1,0,0),60,95);
    Spall(FVector(1500,-3752,100),FVector(0,1,0),80,140);
    auto Mesh=[&](const TCHAR* Material,const TArray<FVector>& V,const TArray<int32>& I,const TArray<FVector>& N,const TArray<FVector2D>& Tex,const TArray<FLinearColor>& C)
    {
        auto* M=NewObject<UProceduralMeshComponent>(Owner); M->SetupAttachment(Root); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->RegisterComponent();
        M->CreateMeshSection_LinearColor(0,V,I,N,Tex,C,TArray<FProcMeshTangent>(),false);
        M->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Material,Material)));
    };
    Mesh(TEXT("DockIvy"),Leaves,Faces,Normals,UV,Colors);
    Mesh(TEXT("Stone"),Chips,ChipFaces,ChipNormals,ChipUV,ChipColors);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_WEATHERING_READY leaves=%d spalls=6 collision=0"),Leaves.Num()/11);
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckWeatheringCapture")))
    {
        const FVector Views[]={FVector(-1050,-1960,110),FVector(-1090,-1420,140),FVector(-1100,1880,130),FVector(1530,-3450,135)};
        const FVector Targets[]={FVector(-1365,-1870,175),FVector(-1320,-1230,160),FVector(-1510,1645,180),FVector(1420,-3750,165)};
        auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(75);
        for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)
        {
            FTimerHandle H,S;
            World->GetTimerManager().SetTimer(H,[World,Camera,P=Views[I],T=Targets[I]]()
            { Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera); },4.f+I*4,false);
            World->GetTimerManager().SetTimer(S,[I]()
            { const FString D=FPaths::ScreenShotDir()/TEXT("Weathering"); IFileManager::Get().MakeDirectory(*D,true); FScreenshotRequest::RequestScreenshot(D/FString::Printf(TEXT("View%d.png"),I),false,false); },6.f+I*4,false);
        }
        FTimerHandle Quit;
        World->GetTimerManager().SetTimer(Quit,[World]() { if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); },22.f,false);
    }
}
