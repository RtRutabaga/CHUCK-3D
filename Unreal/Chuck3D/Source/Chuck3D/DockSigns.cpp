#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "DockSigns.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

void AddDockTradeSign(UWorld* World,FVector Center,const TCHAR* Words,float Yaw,float Width)
{
    auto* Sign=World->SpawnActor<AActor>(); Sign->Tags.Add(TEXT("DockTradeSign"));
    auto* Root=NewObject<USceneComponent>(Sign); Sign->SetRootComponent(Root); Root->RegisterComponent();
    Sign->SetActorLocationAndRotation(Center,FRotator(0,Yaw,0));
    const float W=Width>0?Width:FMath::Max(80.f,FCString::Strlen(Words)*9.f+26.f);
    const float H=36.f;
    // Shallow hand-hewn slab: chipped silhouette, unequal ends and a bevel.
    // Local +X is the readable face, Y follows the timber's long grain.
    const FVector2D Outline[]={FVector2D(-W*.5f+3,-H*.5f+2),FVector2D(-W*.19f,-H*.5f),
        FVector2D(W*.24f,-H*.5f+1),FVector2D(W*.5f-5,-H*.5f+1),
        FVector2D(W*.5f,-H*.5f+6),FVector2D(W*.5f-1,H*.5f-4),
        FVector2D(W*.5f-7,H*.5f-1),FVector2D(W*.12f,H*.5f),
        FVector2D(-W*.28f,H*.5f-1),FVector2D(-W*.5f+2,H*.5f-3),
        FVector2D(-W*.5f,H*.1f),FVector2D(-W*.5f+1,-H*.3f)};
    TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV;
    auto Tri=[&](FVector A,FVector B,FVector C)
    {
        const int32 I=V.Num(); const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        for(const FVector P : {A,B,C}) { V.Add(P); N.Add(Normal); UV.Add(FVector2D(P.Y/W+.5f,P.Z/H+.5f)); }
        T.Append({I,I+2,I+1}); // Unreal front faces use clockwise winding.
    };
    for(int32 I=0;I<UE_ARRAY_COUNT(Outline);++I)
    {
        const FVector2D A=Outline[I],B=Outline[(I+1)%UE_ARRAY_COUNT(Outline)];
        const FVector FA(0,A.X*.975f,A.Y*.86f),FB(0,B.X*.975f,B.Y*.86f);
        const FVector EA(-1,A.X,A.Y),EB(-1,B.X,B.Y),BA(-4.5f,A.X,A.Y),BB(-4.5f,B.X,B.Y);
        Tri(FVector::ZeroVector,FA,FB); Tri(FA,EA,EB); Tri(FA,EB,FB);
        Tri(EA,BA,BB); Tri(EA,BB,EB); Tri(FVector(-4.5f,0,0),BB,BA);
    }
    auto* Board=NewObject<UProceduralMeshComponent>(Sign); Board->SetupAttachment(Root);
    Board->CreateMeshSection(0,V,T,N,UV,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
    Board->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_WoodLight.M_WoodLight")));
    Board->SetCollisionProfileName(TEXT("NoCollision")); Board->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Detail=[&](FVector P,FVector S,const TCHAR* Mat,FRotator R=FRotator::ZeroRotator)
    {
        auto* C=NewObject<UStaticMeshComponent>(Sign); C->SetupAttachment(Root); C->SetStaticMesh(Cube);
        C->SetRelativeTransform(FTransform(R,P,S/100.f));
        C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Mat,Mat)));
        C->SetCollisionProfileName(TEXT("NoCollision")); C->RegisterComponent();
    };
    // Weather checking stays near the rim, outside the lettering. Low relief,
    // dark cracks and rusted peg heads avoid shiny decorative hardware.
    for(float Side : {-1.f,1.f})
    {
        Detail(FVector(.12f,Side*(W*.5f-14),Side*12),FVector(.2f,19,.55f),TEXT("Dark"),FRotator(Side*2,0,0));
        Detail(FVector(.25f,Side*(W*.5f-9),0),FVector(.7f,1.6f,1.6f),TEXT("RustIron"));
        Detail(FVector(-1.5f,Side*(W*.5f-3),-12),FVector(2,6,2),TEXT("Wood"));
    }
    for(float Side : {-1.f,1.f})
        Detail(FVector(-6,Side*W*.30f,0),FVector(5,4,28),TEXT("Wood"));
    const float Size=FMath::Min(16.f,(W-30.f)/FMath::Max(1.f,FCString::Strlen(Words)*.59f));
    auto Letter=[&](float Scale,float Offset,FColor Color)
    {
        auto* Text=NewObject<UTextRenderComponent>(Sign); Text->SetupAttachment(Root);
        Text->SetRelativeLocation(FVector(Offset,0,0));
        Text->SetHorizontalAlignment(EHTA_Center); Text->SetVerticalAlignment(EVRTA_TextCenter);
        Text->SetWorldSize(Size*Scale); Text->SetText(FText::FromString(Words));
        Text->SetTextRenderColor(Color); Text->SetCollisionProfileName(TEXT("NoCollision")); Text->RegisterComponent();
    };
    // Dark scorched rim behind the charcoal core, flush to the timber face.
    // Separate planes prevent the competing coplanar surfaces seen on old crates.
    Letter(1.012f,.12f,FColor(39,20,8)); Letter(1.f,.18f,FColor(20,12,8));
}

void ReviewDockTradeSigns(UWorld* World)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckSignCapture"))) return;
    auto* Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(48);
    int32 I=0;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        if(!It->ActorHasTag(TEXT("DockTradeSign"))) continue;
        const FVector Target=It->GetActorLocation(),P=Target+It->GetActorForwardVector()*250+FVector(0,0,12);
        FTimerHandle View,Shot;
        World->GetTimerManager().SetTimer(View,[World,Camera,P,Target](){
            Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());
            if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
        },4.f+I*3.f,false);
        World->GetTimerManager().SetTimer(Shot,[I](){
            const FString Folder=FPaths::ScreenShotDir()/TEXT("TradeSigns"); IFileManager::Get().MakeDirectory(*Folder,true);
            FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%02d.png"),I),false,false);
        },6.f+I*3.f,false);
        ++I;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SIGN_REVIEW signs=%d"),I);
    FTimerHandle Exit; World->GetTimerManager().SetTimer(Exit,[World](){
        if(auto* PC=World->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
    },8.f+I*3.f,false);
}
