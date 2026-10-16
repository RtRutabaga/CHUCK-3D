#include "ChultTemple.h"
#include "ChultCanyon.h"
#include "ChultRiver.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

// The temple exterior (Claude, 2026-10-15; docs/CHULT-TEMPLE.md). Everything
// is laid out in the canyon frame on a level plaza at PlazaZ(): S runs from
// the jungle toward the pyramid, T across, Z up from the plaza.
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}

    // The plaza: level from the deep jungle's edge to behind the pyramid.
    constexpr float PlazaFrom=64500,PlazaTo=91500,PlazaHalf=5600,PlazaBlend=1500;
    // The pyramid: four terraces 5 m high, each 4 m in from the one below,
    // on a 44 m square; its stair (16 cm risers, 24 cm treads, 6 m wide)
    // climbs the -S face to the summit platform 20 m up at 39 degrees (walkable: 45).
    constexpr float Sc=78200,Base=2200,Inset=400,Tier=500,Talud=350,TaludIn=127,Band=112;
    constexpr int32 Tiers=4;
    constexpr float Rise=16,Tread=20,StairHalf=300,BalusterOut=420;
    constexpr float Top=Tiers*Tier;
    constexpr float StairTop=Sc-(Base-(Tiers-1)*Inset-Band),StairFoot=StairTop-Top/Rise*Tread,BalusterFoot=StairFoot-60;
    // The summit house: a dark doorway in its front wall, a roof comb behind.
    constexpr float HouseFront=Sc-450,HouseBack=Sc+650,HouseHalf=650,DoorHalf=120,DoorTop=260,WallTop=420,Vestibule=300;
    // The avenue: skull stakes either side, then the plaza.
    constexpr float SpawnS=65200,StakeT=520;
    const float StakeS[]={66500,68300,70100,71900};
    const float StelaS[]={67400,71000};

    float PlazaZ(){static const float Z=ChultCanyonBaseZ(ChultCanyonFrame(69000,0));return Z;}
    float BoxDistance(float S,float T)
    {
        const float DS=FMath::Max(FMath::Max(PlazaFrom-S,S-PlazaTo),0.f),DT=FMath::Max(FMath::Abs(T)-PlazaHalf,0.f);
        return FMath::Sqrt(DS*DS+DT*DT);
    }
    FVector At(float S,float T,float Z){return ChultCanyonFrame(S,T,PlazaZ()+Z);}
    // Steps' line through the middle of each tread: the walking surface (a hidden ramp).
    float RampZ(float S){return FMath::Clamp(Rise*.5f+(S-StairFoot)*Rise/Tread,0.f,Top);}

    struct FSurf
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;
        void Add(const FVector& P,const FVector& Normal)
        {
            V.Add(P);N.Add(Normal);C.Add(FColor::White);
            if(FMath::Abs(Normal.Z)>.7f)UV.Add(FVector2D(P.X,P.Y)/200);
            else{const FVector Axis=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();UV.Add(FVector2D(FVector::DotProduct(P,Axis),P.Z)/200);}
        }
        // The outward normal is (B-A)x(C-A); UE draws the reversed winding.
        void Quad(const FVector& A,const FVector& B,const FVector& Cc,const FVector& D)
        {
            const FVector Normal=FVector::CrossProduct(B-A,Cc-A).GetSafeNormal();if(Normal.IsNearlyZero())return;
            const int32 I=V.Num();for(const FVector* P:{&A,&B,&Cc,&D})Add(*P,Normal);
            T.Append({I,I+2,I+1,I,I+3,I+2});
        }
        void Tri(const FVector& A,const FVector& B,const FVector& Cc)
        {
            const FVector Normal=FVector::CrossProduct(B-A,Cc-A).GetSafeNormal();if(Normal.IsNearlyZero())return;
            const int32 I=V.Num();for(const FVector* P:{&A,&B,&Cc})Add(*P,Normal);
            T.Append({I,I+2,I+1});
        }
    };
    // A box in the frame (S0<S1, T0<T1, Z above the plaza). Inward reverses every face.
    void Box(FSurf& M,float S0,float S1,float T0,float T1,float Z0,float Z1,bool Inward=false,bool Bottom=false)
    {
        auto Q=[&](FVector A,FVector B,FVector Cc,FVector D){if(Inward)M.Quad(A,D,Cc,B);else M.Quad(A,B,Cc,D);};
        Q(At(S0,T0,Z1),At(S1,T0,Z1),At(S1,T1,Z1),At(S0,T1,Z1));
        if(Bottom||Inward)Q(At(S0,T0,Z0),At(S0,T1,Z0),At(S1,T1,Z0),At(S1,T0,Z0));
        Q(At(S0,T0,Z0),At(S1,T0,Z0),At(S1,T0,Z1),At(S0,T0,Z1));
        Q(At(S1,T0,Z0),At(S1,T1,Z0),At(S1,T1,Z1),At(S1,T0,Z1));
        Q(At(S1,T1,Z0),At(S0,T1,Z0),At(S0,T1,Z1),At(S1,T1,Z1));
        Q(At(S0,T1,Z0),At(S0,T0,Z0),At(S0,T0,Z1),At(S0,T1,Z1));
    }
    // A box turned by Rotation (in frame axes S,T,Z) about its centre.
    void TurnedBox(FSurf& M,FVector Center,FVector Half,FRotator Rotation)
    {
        auto C=[&](float X,float Y,float Z){const FVector L=Center+Rotation.RotateVector(FVector(X*Half.X,Y*Half.Y,Z*Half.Z));return At(L.X,L.Y,L.Z);};
        M.Quad(C(-1,-1,1),C(1,-1,1),C(1,1,1),C(-1,1,1));M.Quad(C(-1,-1,-1),C(-1,1,-1),C(1,1,-1),C(1,-1,-1));
        M.Quad(C(-1,-1,-1),C(1,-1,-1),C(1,-1,1),C(-1,-1,1));M.Quad(C(1,-1,-1),C(1,1,-1),C(1,1,1),C(1,-1,1));
        M.Quad(C(1,1,-1),C(-1,1,-1),C(-1,1,1),C(1,1,1));M.Quad(C(-1,1,-1),C(-1,-1,-1),C(-1,-1,1),C(-1,1,1));
    }
    // An ellipsoid with axes Fwd/Side/Up (frame directions) and radii R.
    void Ellipsoid(FSurf& M,FVector Center,FVector Fwd,FVector R,int32 Seg=10)
    {
        Fwd=Fwd.GetSafeNormal();const FVector Up(0,0,1),Side=FVector::CrossProduct(Up,Fwd);
        auto P=[&](int32 I,int32 J){const float A=I*2*PI/Seg,H=-PI*.5f+J*PI/(Seg/2);
            const FVector L=Center+Fwd*FMath::Cos(A)*FMath::Cos(H)*R.X+Side*FMath::Sin(A)*FMath::Cos(H)*R.Y+Up*FMath::Sin(H)*R.Z;return At(L.X,L.Y,L.Z);};
        for(int32 I=0;I<Seg;++I)for(int32 J=0;J<Seg/2;++J)M.Quad(P(I,J),P(I+1,J),P(I+1,J+1),P(I,J+1));
    }
    UProceduralMeshComponent* Emit(AActor* Owner,const TCHAR* Name,const TArray<TPair<FSurf*,const TCHAR*>>& Sections,bool bSolid,int32& Missing,int32& Triangles)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        int32 Section=0;
        for(const auto& Pair:Sections)
        {
            const FSurf& S=*Pair.Key;TArray<FProcMeshTangent> Tangents;
            for(const FVector& N:S.N){const FVector Axis=FMath::Abs(N.Z)>.95f?FVector::ForwardVector:FVector::UpVector;Tangents.Emplace(FVector::CrossProduct(Axis,N).GetSafeNormal(),false);}
            Mesh->CreateMeshSection(Section,S.V,S.T,S.N,S.UV,S.C,Tangents,bSolid);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,Pair.Value);Missing+=!Material;Mesh->SetMaterial(Section++,Material);Triangles+=S.T.Num()/3;
        }
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->ComponentTags.Add(TEXT("ChultTemple"));
        return Mesh;
    }
    const TCHAR* StoneMat=TEXT("/Game/Art/ChultRuins/M_RuinStone.M_RuinStone");
    const TCHAR* MossMat=TEXT("/Game/Art/ChultRuins/M_RuinMoss.M_RuinMoss");
    const TCHAR* PlasterMat=TEXT("/Game/Art/Materials/M_WeatheredPlaster.M_WeatheredPlaster");
    const TCHAR* BoneMat=TEXT("/Game/Art/Materials/M_Plaster.M_Plaster");
    const TCHAR* DarkMat=TEXT("/Game/Art/Materials/M_Dark.M_Dark");
    const TCHAR* BarkMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_FallBark.M_FallBark");
    const TCHAR* VineMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_ChultHangingMoss.M_ChultHangingMoss");
    float TierHalf(int32 I){return Base-I*Inset;}
}

float ChultTempleGroundZ(const FVector& P,float Base_)
{
    const FVector F=Frame(P);if(F.X<PlazaFrom-PlazaBlend-10)return Base_;
    return FMath::Lerp(PlazaZ(),Base_,Ease(BoxDistance(F.X,F.Y)/PlazaBlend));
}
bool InChultTempleReserve(const FVector& P,float Margin)
{
    // The avenue, plaza and pyramid, kept clear of the jungle's trees and plants.
    const FVector F=Frame(P);return F.X>PlazaFrom-Margin&&F.X<Sc+Base+900+Margin&&FMath::Abs(F.Y)<4300+Margin;
}
float ChultTempleRoadEndS(){return StairFoot-150;}
FVector ChultTempleSpawn(){return AChultLanding::FloorOrigin()+At(SpawnS,0,34.65f);}
float ChultTempleSpawnYaw(){return Along.Rotation().Yaw;}

void BuildChultTemple(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    FSurf Stone,Moss,Plaster,Steps,StepMoss,Ramp,Dark,Bone,Bark,Vines;FRandomStream R(2026101591);
    // The terraces: a sloping talud, then a framed tablero (a plastered panel
    // between two stone bands), its top the moss-grown ledge above.
    for(int32 I=0;I<Tiers;++I)
    {
        const float H=TierHalf(I),h=H-TaludIn,Zb=I*Tier,Zt=Zb+Talud;
        auto Cn=[&](float SX,float SY,float Half,float Z){return At(Sc+SX*Half,SY*Half,Z);};
        Stone.Quad(Cn(-1,1,H,Zb),Cn(-1,-1,H,Zb),Cn(-1,-1,h,Zt),Cn(-1,1,h,Zt));
        Stone.Quad(Cn(1,-1,H,Zb),Cn(1,1,H,Zb),Cn(1,1,h,Zt),Cn(1,-1,h,Zt));
        Stone.Quad(Cn(-1,-1,H,Zb),Cn(1,-1,H,Zb),Cn(1,-1,h,Zt),Cn(-1,-1,h,Zt));
        Stone.Quad(Cn(1,1,H,Zb),Cn(-1,1,H,Zb),Cn(-1,1,h,Zt),Cn(1,1,h,Zt));
        const float B=H-Band;
        Box(Stone,Sc-B,Sc+B,-B,B,Zt,Zt+25);
        Box(Plaster,Sc-h,Sc+h,-h,h,Zt+25,Zb+Tier-30);
        Box(Moss,Sc-B,Sc+B,-B,B,Zb+Tier-30,Zb+Tier);
        // Stone ribs divide each panel into framed bays, as the 2D tablero's repeated pattern.
        for(int32 Face=0;Face<4;++Face)for(float U=-h+75;U<h-40;U+=150)
        {
            if(Face==0&&FMath::Abs(U)<BalusterOut+40)continue;
            const float Z0=Zt+25,Z1=Zb+Tier-30;
            if(Face==0)Box(Stone,Sc-h-8,Sc-h+2,U-14,U+14,Z0,Z1);else if(Face==1)Box(Stone,Sc+h-2,Sc+h+8,U-14,U+14,Z0,Z1);
            else if(Face==2)Box(Stone,Sc+U-14,Sc+U+14,-h-8,-h+2,Z0,Z1);else Box(Stone,Sc+U-14,Sc+U+14,h-2,h+8,Z0,Z1);
        }
    }
    // The stair: 125 visible steps (no collision) over a hidden ramp through
    // their treads' middles, so he walks up it; a few treads are mossed or chipped.
    const int32 StepCount=FMath::RoundToInt(Top/Rise);
    for(int32 K=0;K<StepCount;++K)
    {
        const float S0=StairFoot+K*Tread,S1=S0+Tread,Z0=K*Rise,Z1=(K+1)*Rise;
        FSurf& M=R.FRand()<.28f?StepMoss:Steps;
        const float Chip=R.FRand()<.15f?R.FRandRange(-120,120):0;
        M.Quad(At(S0,-StairHalf,Z1),At(S1,-StairHalf,Z1),At(S1,StairHalf,Z1),At(S0,StairHalf,Z1));
        M.Quad(At(S0,StairHalf,Z0),At(S0,-StairHalf,Z0),At(S0,-StairHalf,Z1),At(S0,StairHalf,Z1));
        if(Chip!=0){const float C=Chip>0?StairHalf-Chip:-StairHalf-Chip;Box(Steps,S0-3,S0+10,FMath::Min(C,C+40),FMath::Max(C,C+40),Z1-6,Z1+2);}
    }
    {
        // Ramp: from just before the foot (meeting the plaza flush) to just past the summit edge.
        const float A0=StairFoot-Rise*.5f/Rise*Tread,A1=StairTop-Rise*.5f/Rise*Tread,E=StairTop+80,W=StairHalf-5;
        auto Top_=[&](float S){return S<A1?FMath::Max(0.f,Rise*.5f+(S-StairFoot)*Rise/Tread):Top;};
        TArray<float> Ss={A0,A1,E};
        for(int32 I=0;I+1<Ss.Num();++I)
        {
            const float S0=Ss[I],S1=Ss[I+1],Z0=Top_(S0),Z1=Top_(S1);
            Ramp.Quad(At(S0,-W,Z0),At(S1,-W,Z1),At(S1,W,Z1),At(S0,W,Z0));
            Ramp.Quad(At(S0,-W,Z0-40),At(S1,-W,Z1-40),At(S1,-W,Z1),At(S0,-W,Z0));
            Ramp.Quad(At(S1,W,Z1-40),At(S0,W,Z0-40),At(S0,W,Z0),At(S1,W,Z1));
            Ramp.Quad(At(S0,-W,Z0-40),At(S0,W,Z0-40),At(S1,W,Z1-40),At(S1,-W,Z1-40));
        }
    }
    // Balustrades walling in the stair, 1 m above its noses, each ending at the
    // plaza in a serpent head looking down the avenue.
    for(float Sg:{-1.f,1.f})
    {
        const float T0=Sg<0?-BalusterOut:StairHalf,T1=Sg<0?-StairHalf:BalusterOut;
        const float ZA=Rise+100+(BalusterFoot-StairFoot)*Rise/Tread,ZC=Top+100;
        const FVector A(BalusterFoot,0,0),Bp(StairTop,0,0),Cp(StairTop,0,ZC),D(BalusterFoot,0,ZA);
        auto P=[&](const FVector& Q,float T){return At(Q.X,T,Q.Z);};
        Stone.Quad(P(A,T0),P(Bp,T0),P(Cp,T0),P(D,T0));
        Stone.Quad(P(Bp,T1),P(A,T1),P(D,T1),P(Cp,T1));
        Moss.Quad(P(D,T0),P(Cp,T0),P(Cp,T1),P(D,T1));
        Stone.Quad(P(A,T1),P(A,T0),P(D,T0),P(D,T1));
        // The serpent: blocky Mesoamerican head on the plaza, jaws open.
        const float Tc=Sg*360,Sa=BalusterFoot;
        Box(Stone,Sa-150,Sa+10,Tc-65,Tc+65,45,125);          // head
        Box(Stone,Sa-195,Sa-140,Tc-52,Tc+52,70,122);         // snout
        Box(Stone,Sa-185,Sa-20,Tc-56,Tc+56,0,35);            // lower jaw
        Box(Dark,Sa-178,Sa-30,Tc-46,Tc+46,35,70);            // the throat
        for(float F:{-1.f,1.f})
        {
            Box(Plaster,Sa-192,Sa-180,Tc+F*30-6,Tc+F*30+6,45,70);   // fangs
            Box(Plaster,Sa-182,Sa-172,Tc+F*42-5,Tc+F*42+5,35,52);
            Box(Stone,Sa-125,Sa-80,Tc+F*52-10,Tc+F*52+12,108,135);  // brow
            Box(Dark,Sa-118,Sa-92,Tc+F*62-4,Tc+F*62+6,96,116);      // eye
        }
        Box(Stone,Sa-192,Sa-160,Tc-26,Tc+26,120,146);         // nose curl
        Box(Moss,Sa-80,Sa+20,Tc-74,Tc+74,125,160);           // feathered crest
        for(int32 K=0;K<4;++K)Box(Moss,Sa-60+K*22,Sa-48+K*22,Tc-80,Tc+80,150,176+K*6);
    }
    // The summit house: a dark doorway into a shallow vestibule (the way in
    // will be the temple interior), a corniced roof and a pierced roof comb.
    {
        const float Z=Top;
        Box(Stone,Sc-90,HouseBack,-HouseHalf,HouseHalf,Z,Z+WallTop);
        for(float Sg:{-1.f,1.f})
        {
            const float A=Sg<0?-HouseHalf:Vestibule,B=Sg<0?-Vestibule:HouseHalf;
            Box(Stone,HouseFront,Sc-90,A,B,Z,Z+WallTop);
            const float A2=Sg<0?-Vestibule:DoorHalf,B2=Sg<0?-DoorHalf:Vestibule;
            Box(Stone,HouseFront,HouseFront+60,A2,B2,Z,Z+WallTop);
        }
        Box(Stone,HouseFront,HouseFront+60,-DoorHalf,DoorHalf,Z+DoorTop,Z+WallTop);
        Box(Stone,HouseFront+60,Sc-90,-Vestibule,Vestibule,Z+300,Z+WallTop);
        Box(Dark,HouseFront+60.5f,Sc-90.5f,-Vestibule+.5f,Vestibule-.5f,Z+.5f,Z+299.5f,true);
        Box(Dark,HouseFront-1,HouseFront+61,-DoorHalf+.5f,DoorHalf-.5f,Z+.5f,Z+DoorTop-.5f,true);
        Box(Moss,HouseFront-60,HouseBack+60,-HouseHalf-60,HouseHalf+60,Z+WallTop,Z+WallTop+80);
        Box(Plaster,HouseFront-40,HouseBack+40,-HouseHalf-40,HouseHalf+40,Z+WallTop+80,Z+WallTop+140);
        Box(Moss,HouseFront-50,HouseBack+50,-HouseHalf-50,HouseHalf+50,Z+WallTop+140,Z+WallTop+160);
        // The roof comb: posts and bars with openings between, over the back of the house.
        const float C0=Sc+250,C1=Sc+340,Zc=Z+WallTop+160;
        for(float T:{-520.f,-260.f,0.f,260.f,520.f})Box(Stone,C0,C1,T-45,T+45,Zc,Zc+420);
        for(float Zb:{0.f,190.f,380.f})Box(Stone,C0,C1,-565,565,Zc+Zb,Zc+Zb+50);
        Box(Moss,C0-10,C1+10,-585,585,Zc+430,Zc+460);
        Box(Plaster,C0-6,C0,-500,500,Zc+60,Zc+180);
        // Lintel glyphs over the door.
        for(int32 K=-3;K<=3;++K)Box(Plaster,HouseFront-6,HouseFront,K*34-12,K*34+12,Z+DoorTop+40,Z+DoorTop+40+R.FRandRange(40,90));
    }
    // Stelae beside the avenue, carved toward it, each with a low altar.
    int32 Stelae=0;
    for(float S:StelaS)for(float Sg:{-1.f,1.f})
    {
        const float T=Sg*1500;
        Box(Stone,S-28,S+28,T-65,T+65,0,290);Box(Moss,S-32,S+32,T-70,T+70,290,312);
        const float Face=T-Sg*65;
        for(int32 Rw=0;Rw<5;++Rw)for(int32 Cl=-1;Cl<=1;++Cl)
        {
            const float Zc=60+Rw*44,Sw=Cl*34,H=R.FRandRange(12,18),W=R.FRandRange(11,16);
            if(Sg<0)Box(Plaster,S+Sw-W,S+Sw+W,Face,Face+4,Zc-H,Zc+H);else Box(Plaster,S+Sw-W,S+Sw+W,Face-4,Face,Zc-H,Zc+H);
        }
        Box(Moss,S-40,S+40,T-Sg*170-40,T-Sg*170+40,0,45);++Stelae;
    }
    // Fallen carved blocks on the plaza, clear of the avenue and the stair.
    int32 Fragments=0;
    for(int32 I=0;I<40&&Fragments<14;++I)
    {
        const float S=R.FRandRange(66000,76800),T=R.FRandRange(1000,4000)*(R.FRand()<.5f?-1:1);
        if(S>BalusterFoot-400&&FMath::Abs(T)<1100)continue;bool Clear=true;
        for(float St:StelaS)Clear&=FVector2D(S-St,FMath::Abs(T)-1500).Size()>400;if(!Clear)continue;
        const FVector Half(R.FRandRange(30,70),R.FRandRange(30,60),R.FRandRange(20,45));
        TurnedBox(R.FRand()<.4f?Moss:Stone,FVector(S,T,Half.Z*.7f),Half,FRotator(R.FRandRange(-12,12),R.FRandRange(0,360),R.FRandRange(-10,10)));
        if(R.FRand()<.5f)Box(Plaster,S-Half.X*.5f,S+Half.X*.5f,T-Half.Y-2,T-Half.Y+1,Half.Z*.4f,Half.Z*1.2f);
        ++Fragments;
    }
    // Skull stakes: eight, in pairs down the avenue, the skulls looking across it.
    int32 Stakes=0;
    for(float S:StakeS)for(float Sg:{-1.f,1.f})
    {
        const float T=Sg*StakeT,Lean=R.FRandRange(-4,4);const FVector Foot(S,T,-10),Head(S+Lean,T+R.FRandRange(-3,3),178);
        for(int32 K=0;K<8;++K)
        {
            const float A=K*PI/4,B=(K+1)*PI/4;
            const FVector Oa(FMath::Cos(A),FMath::Sin(A),0),Ob(FMath::Cos(B),FMath::Sin(B),0);
            auto P=[&](const FVector& C,const FVector& O,float Rr){const FVector L=C+O*Rr;return At(L.X,L.Y,L.Z);};
            Bark.Quad(P(Foot,Oa,6),P(Foot,Ob,6),P(Head,Ob,4),P(Head,Oa,4));
        }
        const FVector Fwd(0,-Sg,0),Side(FVector::CrossProduct(FVector::UpVector,Fwd)),Skull=Head+FVector(0,0,10);
        Ellipsoid(Bone,Skull,Fwd,FVector(11,8.5f,9.5f));
        Ellipsoid(Bone,Skull+Fwd*5-FVector(0,0,8),Fwd,FVector(6,6.5f,3.5f),8);
        for(float E:{-1.f,1.f})Ellipsoid(Dark,Skull+Fwd*9.2f+Side*E*3.6f+FVector(0,0,1.2f),Fwd,FVector(1.8f,2.4f,2.4f),8);
        Ellipsoid(Dark,Skull+Fwd*10.2f-FVector(0,0,3.2f),Fwd,FVector(1.2f,1.2f,1.6f),6);
        ++Stakes;
    }
    // Hanging moss over the terraces' tablero edges.
    int32 Strands=0;
    for(int32 I=0;I<70;++I)
    {
        const int32 Tr=R.RandRange(0,Tiers-1);const float H=TierHalf(Tr)-Band+8,Face=R.RandRange(0,3),U=R.FRandRange(-.9f,.9f)*H;
        const FVector Out=Face==0?FVector(-1,0,0):Face==1?FVector(1,0,0):Face==2?FVector(0,-1,0):FVector(0,1,0);
        FVector Start=FVector(Sc,0,Tr*Tier+Tier)+Out*H+FVector(Out.Y,-Out.X,0)*U;
        if(Face==0&&FMath::Abs(U)<BalusterOut+60)continue;
        const float Length=R.FRandRange(120,Tier+150);FVector Prev=Start;
        for(int32 J=1;J<=10;++J)
        {
            const float Uj=J/10.f;FVector Next=Start+FVector(0,0,-Length*Uj)+Out*(Uj<.4f?14*Uj:TaludIn*(Uj-.4f)*.9f+6);
            const FVector D=(Next-Prev).GetSafeNormal(),X=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),Y=FVector::CrossProduct(D,X);
            const float Ra=FMath::Lerp(4.f,1.5f,Uj);
            for(int32 K=0;K<5;++K)
            {
                const float A=K*2*PI/5,B=(K+1)*2*PI/5;
                auto P=[&](const FVector& C,float Ang,float Rr){const FVector L=C+(X*FMath::Cos(Ang)+Y*FMath::Sin(Ang))*Rr;return At(L.X,L.Y,L.Z);};
                Vines.Quad(P(Prev,A,Ra),P(Prev,B,Ra),P(Next,B,Ra),P(Next,A,Ra));
            }
            Prev=Next;
        }
        ++Strands;
    }
    int32 Missing=0,Triangles=0;
    Emit(Owner,TEXT("ChultTempleMasonry"),{{&Stone,StoneMat},{&Moss,MossMat},{&Plaster,PlasterMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleSteps"),{{&Steps,StoneMat},{&StepMoss,MossMat}},false,Missing,Triangles);
    auto* RampMesh=Emit(Owner,TEXT("ChultTempleStairRamp"),{{&Ramp,StoneMat}},true,Missing,Triangles);RampMesh->SetHiddenInGame(true);RampMesh->SetCastShadow(false);
    Emit(Owner,TEXT("ChultTempleDark"),{{&Dark,DarkMat}},false,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleSkulls"),{{&Bone,BoneMat}},false,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleStakes"),{{&Bark,BarkMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleVines"),{{&Vines,VineMat}},false,Missing,Triangles)->SetCastShadow(false);

    // Growth: ferns along every ledge, a few small trees clinging to the upper
    // terraces' corners, and plants at the plaza's edges.
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Plants=0,Trees=0;
    auto Place=[&](FString Name,FVector Local,float Scale,float Yaw)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultTemple_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->SetCollisionEnabled(ECollisionEnabled::NoCollision);H->SetCullDistances(9000,16000);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),G+At(Local.X,Local.Y,Local.Z),FVector(Scale)),true);
    };
    for(int32 I=0;I<520;++I)
    {
        const int32 Tr=R.RandRange(0,Tiers-1),Face=R.RandRange(0,3);
        const float H=TierHalf(Tr)-Band,Mid=(H+(Tr+1<Tiers?TierHalf(Tr+1):0))*.5f,U=R.FRandRange(-.95f,.95f)*H;
        const float Depth=Tr+1<Tiers?R.FRandRange(TierHalf(Tr+1)+30,H-25):R.FRandRange(0,H-25);
        if(Tr==Tiers-1&&R.FRand()<.7f)continue;                       // the summit stays mostly bare
        FVector L=Face==0?FVector(Sc-Depth,U,0):Face==1?FVector(Sc+Depth,U,0):Face==2?FVector(Sc+U,-Depth,0):FVector(Sc+U,Depth,0);
        if(Face==0&&FMath::Abs(U)<BalusterOut+80)continue;
        if(Tr==Tiers-1&&L.X>HouseFront-150&&L.X<HouseBack+150&&FMath::Abs(L.Y)<HouseHalf+150)continue;
        L.Z=Tr*Tier+Tier-2;(void)Mid;const int32 Type=I%6;
        Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type==4?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),L,R.FRandRange(1.2f,2.2f)*(Type<4?1:.6f),R.FRandRange(0,360));++Plants;
    }
    for(int32 I=0;I<4;++I)
    {
        const int32 Tr=1+I%2;const float H=TierHalf(Tr)-Band-120;const float SX=I<2?1.f:-1.f,SY=I%2?1.f:-1.f;
        if(SX<0&&I==2)continue;                                        // leave the front corners clearer
        Place(TEXT("SM_ChultIslandTree"),FVector(Sc+SX*H,SY*H,Tr*Tier+Tier-4),R.FRandRange(.8f,1.1f),R.FRandRange(0,360));++Trees;
    }
    for(int32 I=0;I<1100;++I)
    {
        // Thick toward the jungle, sparse nearer the avenue.
        const float S=R.FRandRange(PlazaFrom,Sc+Base+800),T=FMath::Lerp(800.f,5400.f,FMath::Sqrt(R.FRand()))*(R.FRand()<.5f?-1:1);
        if(FMath::Abs(T)<StakeT+250||(S>BalusterFoot-500&&FMath::Abs(T)<BalusterOut+400))continue;
        if(S>Sc-Base-200&&FMath::Abs(T)<Base+200)continue;
        const int32 Type=I%5;
        Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):TEXT("SM_FallElephantEar"),FVector(S,T,-3),R.FRandRange(1.4f,2.6f)*(Type<4?1:.65f),R.FRandRange(0,360));++Plants;
    }
    // A thin mist pooled round the pyramid's foot, and darkness inside the door.
    for(const FVector& L:{At(Sc-Base-600,-2400,150),At(Sc-Base-600,2400,150),At(Sc+Base+500,0,150)})
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+L);Fog->SetWorldScale3D(FVector(8));
        Fog->RadialFogExtinction=.05f;Fog->HeightFogExtinction=.05f;Fog->HeightFogFalloff=700;Fog->HeightFogOffset=.25f;Fog->FogAlbedo=FLinearColor(.6,.7,.66);Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE tiers=%d height_cm=%.0f steps=%d stakes=%d serpents=2 stelae=%d fragments=%d vines=%d plants=%d trees=%d missing_assets=%d triangles=%d build_ms=%.1f"),
        Tiers,Top+WallTop+620,StepCount,Stakes,Stelae,Fragments,Strands,Plants,Trees,Missing,Triangles,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultTemple(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Stair=0,Plaza=0;auto* World=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultTemple),false,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    auto Fail=[&](const TCHAR* What,const FVector& L){++Failures;if(Failures<20)UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_FAIL %s at=%s"),What,*L.ToString());};
    // The plaza is level where the ground function says.
    for(float S=PlazaFrom+200;S<Sc-Base-200;S+=500)for(float T=-4800;T<=4800;T+=1200)
    {
        const FVector P=ChultCanyonFrame(S,T);++Plaza;
        if(FMath::Abs(ChultCanyonGroundZ(P)-PlazaZ())>.5f)Fail(TEXT("plaza_not_level"),P);
    }
    // The stair's walking surface follows its treads and is open to climb.
    FVector Previous=FVector::ZeroVector;
    for(float S=StairFoot-100;S<=StairTop+150;S+=40)
    {
        FHitResult Hit;const FVector P=At(S,0,0);const float Want=S<StairFoot?0.f:FMath::Min(RampZ(S),Top);
        const bool Found=World->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,Want+150),G+P+FVector(0,0,Want-60),ECC_Visibility,Q);
        if(!Found||FMath::Abs(Hit.ImpactPoint.Z-G.Z-PlazaZ()-Want)>10||Hit.ImpactNormal.Z<.75f)
            Fail(TEXT("stair_floor"),FVector(S,Want,Found?Hit.ImpactPoint.Z-G.Z-PlazaZ():-1.f));
        // On the 39 degree slope the capsule's round foot needs ~10 cm more than on the flat.
        const FVector Body=G+P+FVector(0,0,(Found?Hit.ImpactPoint.Z-G.Z-P.Z:Want)+Half+16);
        if(Stair&&World->SweepSingleByChannel(Hit,Previous,Body,FQuat::Identity,ECC_Pawn,Capsule,Q))
        {Fail(TEXT("stair_blocked"),FVector(S,Hit.ImpactPoint.Z-G.Z-PlazaZ(),Body.Z-G.Z-PlazaZ()));UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_BLOCKED_BY %s"),*GetNameSafe(Hit.GetComponent()));}
        Previous=Body;++Stair;
    }
    // Into the doorway: room to stand in the vestibule; and the avenue is open.
    if(World->OverlapBlockingTestByChannel(G+At(HouseFront+150,0,Top+Half+3),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("vestibule_blocked"),At(HouseFront+150,0,Top));
    FHitResult Hit;
    if(World->SweepSingleByChannel(Hit,G+At(SpawnS,0,Half+3),G+At(StairFoot-120,0,Half+3),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("avenue_blocked"),Hit.ImpactPoint-G);
    if(World->SweepSingleByChannel(Hit,G+At(StairTop+60,0,Top+Half+4),G+At(HouseFront+150,0,Top+Half+4),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("door_blocked"),Hit.ImpactPoint-G);
    // The spawn: on the floor with room to stand.
    {
        const FVector P=ChultTempleSpawn();
        const bool Floor=World->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,Half+40),ECC_Visibility,Q);
        if(!Floor||FMath::Abs(Hit.ImpactPoint.Z-(P.Z-34.65f))>6||World->OverlapBlockingTestByChannel(P+FVector(0,0,2),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("spawn"),P-G);
        Failures+=!AChultLanding::Contains(P);
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_CHECK failures=%d plaza_probes=%d stair_probes=%d vestibule=1 avenue=1 spawn=1"),Failures,Plaza,Stair);
    return Failures;
}

void ReviewChultTemple(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    const TArray<FVector> Eyes={At(SpawnS-300,0,180),At(70500,-900,140),At(BalusterFoot-650,-700,90),At(StairTop+200,-500,Top+170),At(Sc-Base-3500,-4200,2600),At(69300,-150,95)};
    const TArray<FVector> Aims={At(Sc,0,1200),At(Sc,0,1500),At(BalusterFoot-100,-360,80),At(HouseFront,0,Top+200),At(Sc,0,1300),At(68300,-StakeT,180)};
    const TArray<FString> Names={TEXT("TempleAvenue"),TEXT("TemplePlaza"),TEXT("TempleSerpent"),TEXT("TempleDoorway"),TEXT("TempleOverview"),TEXT("TempleSkullStake")};
    struct FRun{int32 Phase=0,Walk=0,Failures=0;double Next=0,Start=-1;bool bDone=false;TWeakObjectPtr<ACameraActor> Camera;FTimerHandle Timer;};
    auto Run=MakeShared<FRun>();Run->Failures=Failures;Run->Camera=Camera;
    // One timer throughout: views every 2.5 s, then the real character's climb.
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names]()
    {
        if(Run->bDone)return;
        const FVector G=AChultLanding::FloorOrigin();const double Now=Owner->GetWorld()->GetTimeSeconds();
        const int32 I=Run->Phase/2;
        if(I<Eyes.Num())
        {
            if(Now<Run->Next)return;Run->Next=Now+2.5;
            if(Run->Phase%2==0){if(Run->Camera.IsValid())Run->Camera->SetActorLocationAndRotation(G+Eyes[I],(Aims[I]-Eyes[I]).Rotation());}
            else{FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_VIEW name=%s"),*Names[I]);}
            ++Run->Phase;return;
        }
        auto* M=Chuck->GetCharacterMovement();
        if(Run->Start<0)
        {
            if(Now<Run->Next)return;
            // The real character: from the spawn down the avenue, up the stair, into the doorway.
            Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);
            if(Run->Camera.IsValid())Run->Camera->Destroy();
            // From the plaza, running (the avenue itself is checked by sweeps above).
            Chuck->ResetAtLocation(G+At(72600,0,34.65f));Chuck->SetActorRotation(FRotator(0,ChultTempleSpawnYaw(),0));Chuck->SetRunHeld(true);
            Run->Start=Now;return;
        }
        const TArray<FVector> Way={At(StairFoot-200,0,0),At(StairTop+100,0,Top),At(HouseFront+170,0,Top)};
        const float Time=Now-Run->Start;const FVector P=Chuck->GetActorLocation()-G;
        if(Run->Walk<Way.Num())
        {
            const FVector To=Way[Run->Walk];
            if(FVector::Dist2D(P,To)<35){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_WALK leg=%d z=%.1f seconds=%.1f ground=%d"),Run->Walk,P.Z-PlazaZ(),Time,M->IsMovingOnGround());++Run->Walk;if(Run->Walk<Way.Num())return;}
            else if(Time<150){const FVector D=(To-P).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;}
        }
        Chuck->SetTestStick(FVector2D::ZeroVector);Chuck->SetRunHeld(false);
        const bool Pass=Run->Walk==Way.Num()&&M->IsMovingOnGround()&&FMath::Abs(P.Z-34.65f-PlazaZ()-Top)<20&&Chuck->GetFallDeaths()==0;
        Run->bDone=true;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_TEST failures=%d climb=%d legs=%d seconds=%.1f"),Run->Failures+!Pass,Pass,Run->Walk,Time);
        FPlatformMisc::RequestExitWithStatus(false,(Run->Failures||!Pass)?1:0);
    },.016f,true);
}
