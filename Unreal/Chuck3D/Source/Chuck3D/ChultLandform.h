#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "ChultLanding.h"
#include "Materials/MaterialInterface.h"

// Setting-only, closed rock forms. World-aligned material; no hidden blocker.
struct FChultLandform
{
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;
    TArray<FColor> C;TArray<FProcMeshTangent> Tangents;
    void Tri(FVector A,FVector B,FVector D)
    {
        const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
        FVector Side=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();if(Side.IsNearlyZero())Side=FVector::ForwardVector;
        const int32 Start=V.Num();
        for(FVector P:{A,B,D}){V.Add(P);N.Add(Normal);UV.Add(FVector2D(FVector::DotProduct(P,Side)/300,P.Z/300));C.Add(FColor::White);Tangents.Add(FProcMeshTangent(Side,false));}
        T.Append({Start,Start+2,Start+1});
    }
    void Quad(FVector A,FVector B,FVector D,FVector E){Tri(A,B,D);Tri(A,D,E);}
    void Rock(FVector Center,FVector Size,float Yaw,float Seed)
    {
        const FRotator Rotation(0,Yaw,0);
        auto Point=[&](int32 I,int32 J)
        {
            const float A=I*2*PI/14.f,H=-PI*.5f+J*PI/9.f;
            const float R=1+.12f*FMath::Sin(A*3+Seed)+.07f*FMath::Cos(A*7+H*4+Seed)+.06f*FMath::Sin(H*11+Seed);
            FVector P(FMath::Cos(A)*FMath::Cos(H)*Size.X*R,FMath::Sin(A)*FMath::Cos(H)*Size.Y*R,FMath::Sin(H)*Size.Z);
            P.X+=FMath::Sin(H*5+Seed)*Size.X*.12f*FMath::Cos(H);
            return Center+Rotation.RotateVector(P);
        };
        for(int32 I=0;I<14;++I)for(int32 J=0;J<9;++J)Quad(Point(I,J),Point(I+1,J),Point(I+1,J+1),Point(I,J+1));
    }
    UProceduralMeshComponent* Finish(AActor* Owner,const TCHAR* Name,const TCHAR* Path,bool Solid=true)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->RegisterComponent();
        Mesh->bUseComplexAsSimpleCollision=true;Mesh->CreateMeshSection(0,V,T,N,UV,C,Tangents,Solid);
        Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Path));Mesh->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));return Mesh;
    }
};
