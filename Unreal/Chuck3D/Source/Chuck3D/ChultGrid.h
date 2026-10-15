#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "ChultLanding.h"
#include "Materials/MaterialInterface.h"

// A shared-vertex grid surface (Claude, 2026-10-15). Normals come from the
// grid itself, so slopes shade smoothly; vertex colours carry the rock/jungle
// weights to M_ChultGorge; it is emitted in chunks so distant pieces cull and
// only reachable ones cook collision. Vertices are relative to FloorOrigin.
struct FChultGrid
{
    int32 Cols=0,Rows=0;
    TArray<FVector> P;TArray<FColor> C;TArray<uint8> Hidden;
    void Init(int32 InCols,int32 InRows){Cols=InCols;Rows=InRows;P.SetNumZeroed(Cols*Rows);C.Init(FColor::White,Cols*Rows);Hidden.Init(0,Cols*Rows);}
    int32 Id(int32 I,int32 J)const{return J*Cols+I;}
    FVector Normal(int32 I,int32 J,const FVector& Out)const
    {
        const FVector A=P[Id(FMath::Min(I+1,Cols-1),J)]-P[Id(FMath::Max(I-1,0),J)];
        const FVector B=P[Id(I,FMath::Min(J+1,Rows-1))]-P[Id(I,FMath::Max(J-1,0))];
        FVector N=FVector::CrossProduct(A,B).GetSafeNormal();
        if(N.IsNearlyZero())N=Out;else if(FVector::DotProduct(N,Out)<0)N=-N;
        return N;
    }
    // Columns [I0,I1] x rows [J0,J1] as one component; null when every cell is hidden.
    UProceduralMeshComponent* Emit(AActor* Owner,const FString& Name,UMaterialInterface* Material,int32 I0,int32 I1,int32 J0,int32 J1,
        TFunctionRef<FVector(int32,int32)> OutDir,bool bSolid,bool bShadow,int32& Triangles) const
    {
        const int32 W=I1-I0+1;TArray<FVector> V,N;TArray<FVector2D> UV;TArray<FColor> Col;TArray<FProcMeshTangent> Tan;TArray<int32> T;
        for(int32 J=J0;J<=J1;++J)for(int32 I=I0;I<=I1;++I)
        {
            const FVector Q=P[Id(I,J)],Nn=Normal(I,J,OutDir(I,J));
            V.Add(Q);N.Add(Nn);UV.Add(FVector2D((Q.X+Q.Y)/400,Q.Z/400));Col.Add(C[Id(I,J)]);
            FVector Side=FVector::CrossProduct(FVector::UpVector,Nn).GetSafeNormal();if(Side.IsNearlyZero())Side=FVector::ForwardVector;
            Tan.Add(FProcMeshTangent(Side,false));
        }
        auto L=[&](int32 I,int32 J){return (J-J0)*W+(I-I0);};
        for(int32 J=J0;J<J1;++J)for(int32 I=I0;I<I1;++I)
        {
            if(Hidden[Id(I,J)]&&Hidden[Id(I+1,J)]&&Hidden[Id(I,J+1)]&&Hidden[Id(I+1,J+1)])continue;
            const int32 A=L(I,J),B=L(I+1,J),Cc=L(I+1,J+1),D=L(I,J+1);
            for(const FIntVector& Tri:{FIntVector(A,B,Cc),FIntVector(A,Cc,D)})
            {
                // UE draws clockwise fronts: wind each triangle against its shading normal.
                const FVector Face=FVector::CrossProduct(V[Tri.Y]-V[Tri.X],V[Tri.Z]-V[Tri.X]);
                const FVector Want=N[Tri.X]+N[Tri.Y]+N[Tri.Z];
                if(FVector::DotProduct(Face,Want)<0)T.Append({Tri.X,Tri.Y,Tri.Z});else T.Append({Tri.X,Tri.Z,Tri.Y});
            }
        }
        if(T.Num()==0)return nullptr;
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(*Name));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        Mesh->CreateMeshSection(0,V,T,N,UV,Col,Tan,bSolid);Mesh->SetMaterial(0,Material);
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCastShadow(bShadow);Triangles+=T.Num()/3;
        return Mesh;
    }
};
