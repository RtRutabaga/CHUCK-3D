#include "DockVista.h"
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

namespace
{
float Ground(float X,float Y)
{
    // Three overlapping land masses form one mainland around the north of
    // the harbor. Negative heights let the coastline emerge from the sea.
    const float West=-X-5800.f+700.f*FMath::Sin(Y*.00019f);
    const float North=Y-6100.f+550.f*FMath::Sin(X*.00024f);
    const float East=X-(6500.f+FMath::Max(0.f,-Y)*.65f)+650.f*FMath::Sin(Y*.00028f);
    const float Inland=FMath::Max3(West,North,East);
    const float Rise=FMath::Clamp(Inland/6500.f,0.f,1.f);
    const float Rolling=650.f+380.f*FMath::Sin(X*.00023f+Y*.0001f)+280.f*FMath::Cos(Y*.00031f-X*.00013f);
    const float Ridge=(3000.f+1200.f*FMath::Sin(Y*.00021f)+500.f*FMath::Sin(Y*.00049f))*FMath::Exp(-FMath::Square((X+24000.f)/10000.f))
        +(3300.f+1300.f*FMath::Sin(X*.00017f)+650.f*FMath::Cos(X*.00041f))*FMath::Exp(-FMath::Square((Y-30000.f)/11500.f));
    // A prominent rocky coastal summit above the western city, with a shoulder
    // and a lower inland ridge. Uneven spurs keep the outline from a smooth cone.
    auto Peak=[](float DX,float DY,float RX,float RY,float Height,float Phase)
    {
        const float D=FMath::Sqrt(FMath::Square(DX/RX)+FMath::Square(DY/RY));
        const float A=FMath::Atan2(DY,DX);
        const float Spur=1+.12f*FMath::Sin(A*5+Phase)+.055f*FMath::Sin(A*9-Phase);
        const float T=FMath::Clamp(1-D/Spur,0.f,1.f);
        const float Gullies=1+.07f*FMath::Sin(A*13+Phase+D*8)*T*(1-T);
        return Height*FMath::Pow(T,1.45f)*Gullies;
    };
    const float Mountain=FMath::Max3(Peak(X+32000,Y-6500,15500,14500,12000,.7f),
        Peak(X+36000,Y-15500,12500,10000,8000,2.1f),Peak(X+27000,Y+4300,11000,9500,6500,1.6f));
    return -180.f+FMath::Clamp(Inland*.18f,0.f,380.f)+Rise*(Rolling+Ridge+Mountain);
}
}

void BuildCoastalVista(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>(); Owner->Tags.Add(TEXT("CoastalVista"));
    auto* Root=NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
    // About 25k vertices for the whole distant countryside; no physics cooking.
    auto* Terrain=NewObject<UProceduralMeshComponent>(Owner); Terrain->SetupAttachment(Root);
    Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision); Terrain->SetCastShadow(false);
    Terrain->RegisterComponent();
    TArray<FVector> V,N; TArray<int32> Indices; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
    constexpr int32 NX=154, NY=151; constexpr float Step=600.f;
    for(int32 Y=0;Y<NY;++Y) for(int32 X=0;X<NX;++X)
    {
        const float WX=-52000+X*Step,WY=-40000+Y*Step,H=Ground(WX,WY);
        V.Add(FVector(WX,WY,H)); UV.Add(FVector2D(WX/3000,WY/3000));
        const FVector Normal=FVector(Ground(WX-100,WY)-Ground(WX+100,WY),Ground(WX,WY-100)-Ground(WX,WY+100),200).GetSafeNormal();
        N.Add(Normal);
        const float Variation=.5f+.5f*FMath::Sin(WX*.00065f)*FMath::Cos(WY*.00048f);
        FLinearColor C=FMath::Lerp(FLinearColor(.105f,.15f,.085f),FLinearColor(.24f,.25f,.13f),Variation);
        if(H<130) C=FMath::Lerp(FLinearColor(.27f,.24f,.19f),C,FMath::Clamp((H+40)/170,0.f,1.f));
        if(Normal.Z<.90f) C=FLinearColor(.24f,.25f,.25f);
        const float Rock=FMath::Max(1-Normal.Z,FMath::Clamp((H-4500)/6500,0.f,1.f));
        const float Strata=.5f+.5f*FMath::Sin(H*.002f+WX*.0004f+FMath::Sin(WY*.0007f)*2);
        const FLinearColor RockColor=FMath::Lerp(FLinearColor(.20f,.215f,.21f),FLinearColor(.39f,.37f,.31f),Variation*.5f+Strata*.5f);
        C=FMath::Lerp(C,RockColor,FMath::Clamp(Rock*1.7f,0.f,1.f));
        Colors.Add(C);
    }
    for(int32 Y=0;Y<NY-1;++Y) for(int32 X=0;X<NX-1;++X)
    {
        const int32 A=Y*NX+X,B=A+1,C=A+NX,D=C+1;
        Indices.Append({A,B,C,B,D,C});
    }
    Terrain->CreateMeshSection_LinearColor(0,V,Indices,N,UV,Colors,TArray<FProcMeshTangent>(),false);
    Terrain->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_VistaTerrain.M_VistaTerrain")));

    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Mat,FRotator Rot=FRotator::ZeroRotator,bool Spire=false)
    {
        const FString Key=FString(Mat)+(Spire?TEXT("cone"):TEXT("cube"));
        auto*& B=Batches.FindOrAdd(Key);
        if(!B) { B=NewObject<UInstancedStaticMeshComponent>(Owner); B->SetupAttachment(Root); B->SetStaticMesh(Spire?Cone:Cube);
            B->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Mat,Mat)));
            B->SetCollisionEnabled(ECollisionEnabled::NoCollision); B->SetCastShadow(false); B->RegisterComponent(); }
        B->AddInstance(FTransform(Rot,P,Size/100.f));
    };
    auto House=[&](float X,float Y,float Z,float W,float D,float H,float Angle,int32 Style)
    {
        const FRotator R(0,Angle,0); const FVector P(X,Y,Z);
        Shape(P+FVector(0,0,H*.5f),FVector(W,D,H),Style%3?TEXT("Plaster"):TEXT("Stone"),R);
        for(float S : {-1.f,1.f})
            Shape(P+R.RotateVector(FVector(S*W*.25f,0,H+W*.1443f)),FVector(W*.57735f+25,D+30,15),TEXT("Roof"),FRotator(-S*30,Angle,0));
        // Boarded gable closes the space under both slopes.
        for(int32 J=0;J<8;++J)
        {
            const float LX=-W*.5f+(J+.5f)*W/8,GH=FMath::Max(2.f,W*.288675f-FMath::Abs(LX)*.57735f);
            for(float Side : {-1.f,1.f}) Shape(P+R.RotateVector(FVector(LX,Side*D*.5f,H+GH*.5f)),FVector(W/8,8,GH),TEXT("Wood"),R);
        }
        for(float Z=180;Z<H-25;Z+=230)
        {
            Shape(P+FVector(0,0,Z-65),FVector(W+8,D+8,8),TEXT("Wood"),R);
            for(float S : {-1.f,1.f}) for(float Offset : {-.27f,.27f})
            {
                Shape(P+R.RotateVector(FVector(S*(W*.5f+3),D*Offset,Z)),FVector(5,42,66),TEXT("Dark"),R);
                Shape(P+R.RotateVector(FVector(W*Offset,S*(D*.5f+3),Z)),FVector(42,5,66),TEXT("Dark"),R);
            }
        }
        if(Style%3==0) Shape(P+R.RotateVector(FVector(W*.2f,D*.2f,H+W*.2f)),FVector(40,45,140),TEXT("Stone"),R);
    };
    // An unbroken strip behind the existing north walls joins both banks.
    Shape(FVector(1900,6340,-125),FVector(10500,1050,250),TEXT("Stone"));
    Shape(FVector(1200,5860,22),FVector(4600,65,95),TEXT("Stone"));
    // The old opposite waterfront is backed by continuous land rather than
    // unsupported houses. Its inland edge blends into the terrain shore.
    Shape(FVector(6500,1800,-130),FVector(3200,9000,260),TEXT("Stone"));
    FRandomStream Rng(31871);
    // Inland streets beyond the plaza gate: supported scenery connects west
    // into the existing city, while the harbor mouth remains open to the east.
    Shape(FVector(-1700,-7600,-130),FVector(6200,6500,260),TEXT("Stone"));
    for(int32 Row=0;Row<4;++Row) for(int32 Col=0;Col<7;++Col)
    {
        const float X=-3850+Col*730.f, Y=-5000-Row*830.f;
        // Preserve a suggestion of a street continuing through the closed gate.
        if(Col==5) continue;
        House(X,Y,0,470+(Col%2)*60,520,620+((Row+Col)%4)*170,(Col%3-1)*7,Row+Col);
    }
    House(-2700,-6800,0,850,750,1300,0,0);
    // Terminate the visible lane in architecture rather than a platform edge.
    for(int32 Col=0;Col<7;++Col)
        House(-3850+Col*730.f,-8450,0,620,650,1250+(Col%3)*130,0,Col);
    for(int32 Col=0;Col<6;++Col)
        House(-3650+Col*820.f,-9550,0,680,720,1500+(Col%2)*180,4,Col+1);
    for(int32 Row=0;Row<6;++Row) for(int32 Col=0;Col<18;++Col)
    {
        const float X=-7300+Col*850.f+Rng.FRandRange(-160,160), Y=7000+Row*860.f+Rng.FRandRange(-140,140);
        const float Z=FMath::Max(0.f,Ground(X,Y)-25);
        House(X,Y,Z,Rng.FRandRange(350,630),Rng.FRandRange(390,630),Rng.FRandRange(420,1050),Rng.FRandRange(-12,12),Col+Row);
    }
    for(int32 Row=0;Row<4;++Row) for(int32 Col=0;Col<12;++Col)
    {
        const float X=6500+Row*820.f+Rng.FRandRange(-100,100),Y=-2800+Col*700.f;
        House(X,Y,FMath::Max(0.f,Ground(X,Y)-20),430,490,Rng.FRandRange(430,950),Rng.FRandRange(-15,15),Col+Row);
    }
    for(int32 Row=0;Row<5;++Row) for(int32 Col=0;Col<15;++Col)
    {
        const float X=-7100-Row*900.f,Y=-5200+Col*800.f;
        House(X,Y,FMath::Max(0.f,Ground(X,Y)-25),480,550,Rng.FRandRange(550,1050),Rng.FRandRange(-12,12),Col+Row);
    }
    // A few taller civic silhouettes break the roof rows; no new playable sites.
    for(FVector P : {FVector(-9200,2300,0),FVector(-4200,10700,0),FVector(4600,12000,0),FVector(9100,4100,0)})
    {
        P.Z=Ground(P.X,P.Y);
        Shape(P+FVector(0,0,900),FVector(380,380,1800),TEXT("Stone"));
        Shape(P+FVector(0,0,1990),FVector(560,560,520),TEXT("Roof"),FRotator::ZeroRotator,true);
        House(P.X+600,P.Y,P.Z,900,650,1100,0,0);
    }
    // Sparse woodland silhouettes in the country beyond the built roof rows.
    for(int32 I=0;I<380;++I)
    {
        const float X=Rng.FRandRange(-30000,28000),Y=Rng.FRandRange(-14000,35000),H=Ground(X,Y);
        if(H<200 || H>5500 || (X>-12500 && X<11000 && Y>-6000 && Y<13000)) continue;
        const float Height=Rng.FRandRange(350,750);
        Shape(FVector(X,Y,H+Height*.3f),FVector(Height*.55f,Height*.55f,Height),TEXT("VistaFoliage"),FRotator::ZeroRotator,true);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckVistaCapture")))
    {
        // Ear-height above the highest current Dock Street roof (~10.5 m).
        const FVector Roof(-770,2830,1150);
        const FVector Targets[]={FVector(-10000,2830,1400),FVector(0,16000,1200),FVector(12000,1500,900),FVector(2000,-16000,0),FVector(1700,6100,200),FVector(-32000,6500,6500)};
        auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(85);
        for(int32 I=0;I<UE_ARRAY_COUNT(Targets);++I)
        {
            FTimerHandle H,S;
            const FVector P=I==5?FVector(2260,3080,100):Roof;
            World->GetTimerManager().SetTimer(H,[World,Camera,T=Targets[I],P]()
            { Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera); },4.f+I*4,false);
            World->GetTimerManager().SetTimer(S,[I]()
            { const FString D=FPaths::ScreenShotDir()/TEXT("Vista"); IFileManager::Get().MakeDirectory(*D,true); FScreenshotRequest::RequestScreenshot(D/FString::Printf(TEXT("View%d.png"),I),false,false); },6.f+I*4,false);
        }
        FTimerHandle Q; World->GetTimerManager().SetTimer(Q,[World]() { if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); },30.f,false);
    }
}
