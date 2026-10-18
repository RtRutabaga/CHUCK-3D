#include "ChultWreck.h"
#include "ChultCanyon.h"
#include "ChultLanding.h"
#include "ChultBridge.h"
#include "ChultPath.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// The jungle wreck (Claude, 2026-10-18; docs/CHULT-WRECK.md). The 2D game's
// Phase 5 cog stranded in the jungle, without its sailor: a clinker-built cog
// with a flat bottom, straight raked stem and sternpost, through-beams, a
// stern castle with a cabin under it, a forecastle and a stern-hung rudder.
// It came down recently: the timber is sound, the canvas is still whole
// enough to drape, the jungle has barely started on it. It lies heeled over
// toward the road just past the bridge landing, with its mast broken over
// the low side and Astral ruptures open in the ground all round it.
//
// The hull is built in the ship's own frame (X to the bow, Y to the high
// side, Z up from the keel) and the whole is one turned component, so the
// timber's grain runs along the ship. Everything on the ground is in the
// floor frame (FloorOrigin-local).
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}
    float GroundAt(const FVector& P){return ChultCanyonGroundZ(P);}
    FVector OnGround(FVector P,float Up=0){P.Z=GroundAt(P)+Up;return P;}
    FVector2D XY(const FVector& P){return FVector2D(P.X,P.Y);}

    // Where it lies: the keel's midpoint, the bow toward the bridge swung a
    // little toward the road, heeled over onto the road side, bow down, sunk in.
    constexpr float KeelS=8300,KeelT=-950,SwingDeg=8,HeelDeg=18,PitchDeg=2,Sink=55;
    // The hull: 23 m of keel, 7.6 m beam, 4 m deep amidships, the sheer rising to both ends.
    constexpr float KeelHalf=1150,BowRake=.55f,SternRake=.22f,HalfBeam=380,FlatHalf=230,FlatT=.22f,Bulwark=95,Shell=6,Post=14;
    constexpr int32 Strakes=10,Stations=56;
    constexpr float FloorZ=58;                                   // the hold's floor (its top)
    constexpr float HatchAft=-420,HatchFore=-80,HatchHalf=120;   // the main hatch
    constexpr float RampFoot=-750;                               // the hold ramp rises from here to the hatch's fore edge
    constexpr float CabinFore=-760,CabinAft=-1080,CabinDoor=45,CabinDoorTop=170;
    constexpr float ForeFrom=820,ForeZ=480;                      // the forecastle platform
    constexpr float StumpX=40,StumpR=32;
    constexpr float BreakFrom=-25,BreakTo=400;                   // the low side's bulwark, gone where the mast went over
    constexpr float PanelFrom=0,PanelTo=100,PanelRun=300;        // the torn-off bulwark, lying as a ramp to the deck
    constexpr float BreachFrom=-470,BreachTo=-120;               // the hole in the low side, at the ground
    const float BeamX[]={-880,-620,250,480,720,950};

    float SheerZ(float S){return 400+(S>0?125.f:155.f)*S*S;}
    float BottomZ(float S){const float A=FMath::Max(0.f,(FMath::Abs(S)-.6f)/.4f);return 45*A*A;}
    float DeckZ(float S){return SheerZ(S)-Bulwark;}
    float EndX(float S,float Z){return S>=0?KeelHalf+BowRake*Z:KeelHalf+SternRake*Z;}
    float Plan(float S){return FMath::Pow(FMath::Max(0.f,1-FMath::Pow(FMath::Abs(S),2.4f)),.6f);}
    float PlanB(float S){return FMath::Pow(FMath::Max(0.f,1-FMath::Pow(FMath::Abs(S),1.8f)),.8f);}
    float GirthK(float T){return (T-FlatT)/(1-FlatT);}
    float GirthT(float K){return FlatT+K*(1-FlatT);}
    // Half-breadth of the outer planking at girth T (0 keel, 1 sheer) and its height.
    void Section(float S,float T,float& Y,float& Z)
    {
        const float Zb=BottomZ(S),Zs=SheerZ(S);
        if(T<FlatT){Z=Zb;Y=FMath::Max(FlatHalf*PlanB(S)*(T/FlatT),Post*T/FlatT);return;}
        const float K=GirthK(T);Z=Zb+(Zs-Zb)*K;
        Y=FMath::Max(Post,FMath::Lerp(FlatHalf*PlanB(S),HalfBeam*Plan(S),1-FMath::Pow(1-K,2.2f)));
    }
    // A point on the planking. G in [-1,1]: negative on the low side. Out moves
    // it out of the planking (the clinker laps) or, negative, to its inner face.
    FVector Hull(float S,float G,float Out=0)
    {
        const float T=FMath::Abs(G),Side=G<0?-1.f:1.f;float Y,Z;Section(S,T,Y,Z);
        if(T<FlatT&&Out<0)Z-=Out;else Y+=Out*FMath::Min(1.f,Y/60.f);
        return FVector(S*EndX(S,Z),Side*Y,Z);
    }
    FVector HullNormal(float S,float G)
    {
        constexpr float E=.004f;
        const FVector DS=Hull(FMath::Min(S+E,1.f),G)-Hull(FMath::Max(S-E,-1.f),G);
        const FVector DG=Hull(S,FMath::Clamp(G+E,-1.f,1.f))-Hull(S,FMath::Clamp(G-E,-1.f,1.f));
        FVector N=FVector::CrossProduct(DS,DG).GetSafeNormal();
        const FVector P=Hull(S,G);
        if(FVector::DotProduct(N,P-FVector(P.X,0,SheerZ(S)*.6f))<0)N=-N;
        return N.IsNearlyZero()?FVector(0,G<0?-1:1,0):N;
    }
    // Half-breadth inside the planking at height Z, and outside it.
    float InnerHalf(float S,float Z)
    {
        const float Zb=BottomZ(S),Zs=SheerZ(S);if(Z<=Zb)return 0;
        const float K=FMath::Clamp((Z-Zb)/FMath::Max(1.f,Zs-Zb),0.f,1.f);
        return FMath::Max(0.f,FMath::Lerp(FlatHalf*PlanB(S),HalfBeam*Plan(S),1-FMath::Pow(1-K,2.2f))-Shell);
    }
    float OuterHalf(float S,float Z){return InnerHalf(S,Z)+Shell;}
    float StationAt(float X,float Z){return FMath::Clamp(X/EndX(X,Z),-1.f,1.f);}
    // The station at X on the deck (whose height depends on the station).
    float DeckStation(float X){float S=StationAt(X,400);for(int32 I=0;I<3;++I)S=StationAt(X,DeckZ(S));return S;}
    float DeckAt(float X){return DeckZ(DeckStation(X));}

    FQuat ShipQuat()
    {
        const float B=FMath::DegreesToRadians(SwingDeg);
        const FVector Fwd=-Along*FMath::Cos(B)+Across*FMath::Sin(B);
        // Yaw, then the bow down (about Y), then heeled: +X roll lowers the -Y (low) side.
        return FQuat(FRotator(0,Fwd.Rotation().Yaw,0))*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(PitchDeg))*FQuat(FVector::XAxisVector,FMath::DegreesToRadians(HeelDeg));
    }
    const FTransform& Ship()
    {
        static const FTransform X=[]{FVector O=ChultCanyonFrame(KeelS,KeelT);O.Z=GroundAt(O)-Sink;return FTransform(ShipQuat(),O);}();
        return X;
    }
    FVector ToFloor(const FVector& L){return Ship().TransformPosition(L);}
    FVector ToShip(const FVector& F){return Ship().InverseTransformPosition(F);}
    // The ship's heading on the ground: to the bow, and to the high side.
    FVector YawFwd(){return Ship().GetRotation().RotateVector(FVector::XAxisVector).GetSafeNormal2D();}
    FVector YawHigh(){const FVector F=YawFwd();return FVector(-F.Y,F.X,0);}

    bool Inside(const TArray<FVector2D>& Poly,const FVector2D& P)
    {
        bool In=false;
        for(int32 I=0,J=Poly.Num()-1;I<Poly.Num();J=I++)
        {
            const FVector2D& A=Poly[I];const FVector2D& B=Poly[J];
            if(((A.Y>P.Y)!=(B.Y>P.Y))&&(P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X))In=!In;
        }
        return In;
    }

    // The ruptures: S, T, length, greatest half-width, turn from the S axis (degrees).
    const float RiftData[][5]={
        {7000,-150,260,48,-20},{9150,-350,210,40,10},{9600,-350,340,58,25},{9950,-1350,240,44,-35},
        {9050,-1600,380,62,5},{7900,-1650,260,46,-15},{6950,-1250,190,38,60},{8250,-2300,150,30,30},{9300,450,130,28,-50}};
    constexpr int32 RiftSide=18;
    struct FRift{FVector2D C,Dir,Perp;float Len=0,Wid=0,LipZ=0,Radius=0;TArray<FVector2D> Edge;FBox2D Box;};
    struct FLayout{TArray<FVector2D> HullCut;FBox2D HullBox;TArray<FRift> Rifts;FBox2D All;};
    const FLayout& Layout()
    {
        static const FLayout L=[]
        {
            FLayout R;
            // Where the planking goes into the ground, station by station, both sides.
            TArray<FVector2D> Low,High;
            for(int32 I=0;I<=80;++I)
            {
                const float S=-1+2.f*I/80;FVector2D Cross[2];bool Ok[2]={false,false};
                for(int32 Side=0;Side<2;++Side)for(float T=0;T<=1.0001f;T+=.005f)
                {
                    const FVector P=ToFloor(Hull(S,(Side==0?-1:1)*T));
                    if(P.Z>=GroundAt(P)){Ok[Side]=T>0;Cross[Side]=XY(P);break;}
                }
                if(Ok[0]&&Ok[1]){Low.Add(Cross[0]);High.Add(Cross[1]);}
            }
            TArray<FVector2D> Poly=Low;for(int32 I=High.Num()-1;I>=0;--I)Poly.Add(High[I]);
            // Grown 8 cm, so a cut cell's step never shows ground inside the planking.
            double Area=0;for(int32 I=0;I<Poly.Num();++I){const FVector2D& A=Poly[I];const FVector2D& B=Poly[(I+1)%Poly.Num()];Area+=A.X*B.Y-B.X*A.Y;}
            R.HullBox.Init();
            for(int32 I=0;I<Poly.Num();++I)
            {
                const FVector2D D=(Poly[(I+1)%Poly.Num()]-Poly[(I+Poly.Num()-1)%Poly.Num()]).GetSafeNormal();
                const FVector2D Out=Area>0?FVector2D(D.Y,-D.X):FVector2D(-D.Y,D.X);
                R.HullCut.Add(Poly[I]+Out*8);R.HullBox+=R.HullCut.Last();
            }
            R.HullBox=R.HullBox.ExpandBy(15);R.All=R.HullBox;
            for(int32 K=0;K<UE_ARRAY_COUNT(RiftData);++K)
            {
                FRift F;const float* D=RiftData[K];const FVector C=ChultCanyonFrame(D[0],D[1]);
                const float A=FMath::DegreesToRadians(D[4]);
                const FVector Dir=Along*FMath::Cos(A)+Across*FMath::Sin(A);
                F.C=XY(C);F.Dir=XY(Dir);F.Perp=FVector2D(-Dir.Y,Dir.X);F.Len=D[2];F.Wid=D[3];
                // The sewer's rupture edge: a pointed tear, ragged and a little bent.
                for(int32 Side=0;Side<2;++Side)for(int32 I=0;I<=RiftSide;++I)
                {
                    if(Side==1&&(I==0||I==RiftSide))continue;
                    const float U=Side==0?I/float(RiftSide):1-I/float(RiftSide),Sg=Side==0?-1.f:1.f;
                    const float Env=FMath::Pow(FMath::Max(0.f,FMath::Sin(PI*U)),.65f);
                    const float W=5+Env*F.Wid*(1+.15f*FMath::Sin(U*13+Sg*2.3f+K)+.1f*FMath::Sin(U*29+Sg+K*1.7f));
                    const float Bend=F.Len*.08f*FMath::Sin(PI*U)*FMath::Sin(K*2.1f+.6f);
                    F.Edge.Add(F.C+F.Dir*((U-.5f)*F.Len)+F.Perp*(Bend+Sg*W));
                }
                F.Box.Init();F.LipZ=1.e9f;
                for(const FVector2D& P:F.Edge){F.Box+=P;F.Radius=FMath::Max(F.Radius,float(FVector2D::Distance(P,F.C)));F.LipZ=FMath::Min(F.LipZ,GroundAt(FVector(P.X,P.Y,0)));}
                F.Box=F.Box.ExpandBy(15);R.All+=F.Box;R.Rifts.Add(MoveTemp(F));
            }
            R.All=R.All.ExpandBy(300);
            return R;
        }();
        return L;
    }
    bool CutAt(const FVector2D& P)
    {
        const FLayout& L=Layout();
        if(L.HullBox.IsInside(P)&&Inside(L.HullCut,P))return true;
        for(const FRift& R:L.Rifts)if(R.Box.IsInside(P)&&Inside(R.Edge,P))return true;
        return false;
    }
    FBox2D CellBox(float S,float T){FBox2D B(ForceInit);for(float A:{S,S+100})for(float C:{T,T+100})B+=XY(ChultCanyonFrame(A,C));return B;}

    struct FSurf
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FProcMeshTangent> Tan;
        void Vert(const FVector& P,const FVector& Nn)
        {
            V.Add(P);N.Add(Nn);UV.Add(FVector2D(P.X+P.Y,P.Z)/120);
            const FVector Ax=FMath::Abs(Nn.Z)>.95f?FVector::ForwardVector:FVector::UpVector;
            Tan.Add(FProcMeshTangent(FVector::CrossProduct(Ax,Nn).GetSafeNormal(),false));
        }
        // Its front faces the side the normals point to (UE draws clockwise fronts).
        void Quad(const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& NA,const FVector& NB,const FVector& NC,const FVector& ND)
        {
            const FVector G=FVector::CrossProduct(B-A,C-A)+FVector::CrossProduct(C-A,D-A);
            const int32 I=V.Num();Vert(A,NA);Vert(B,NB);Vert(C,NC);Vert(D,ND);
            if(FVector::DotProduct(G,NA+NB+NC+ND)>=0)T.Append({I,I+2,I+1,I,I+3,I+2});else T.Append({I,I+1,I+2,I,I+2,I+3});
        }
        void Flat(const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& Out){Quad(A,B,C,D,Out,Out,Out,Out);}
        void Tri(const FVector& A,const FVector& B,const FVector& C,const FVector& Out)
        {
            const int32 I=V.Num();Vert(A,Out);Vert(B,Out);Vert(C,Out);
            if(FVector::DotProduct(FVector::CrossProduct(B-A,C-A),Out)>=0)T.Append({I,I+2,I+1});else T.Append({I,I+1,I+2});
        }
        // Both faces (canvas, veils).
        void TwoSided(const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& NA,const FVector& NB,const FVector& NC,const FVector& ND)
        {Quad(A,B,C,D,NA,NB,NC,ND);Quad(A,B,C,D,-NA,-NB,-NC,-ND);}
        // An oriented box: centre and three half-axes.
        void Box(const FVector& C,const FVector& X,const FVector& Y,const FVector& Z)
        {
            auto P=[&](float A,float B,float Cc){return C+X*A+Y*B+Z*Cc;};
            const FVector Xn=X.GetSafeNormal(),Yn=Y.GetSafeNormal(),Zn=Z.GetSafeNormal();
            Flat(P(1,-1,-1),P(1,1,-1),P(1,1,1),P(1,-1,1),Xn);Flat(P(-1,-1,-1),P(-1,1,-1),P(-1,1,1),P(-1,-1,1),-Xn);
            Flat(P(-1,1,-1),P(1,1,-1),P(1,1,1),P(-1,1,1),Yn);Flat(P(-1,-1,-1),P(1,-1,-1),P(1,-1,1),P(-1,-1,1),-Yn);
            Flat(P(-1,-1,1),P(1,-1,1),P(1,1,1),P(-1,1,1),Zn);Flat(P(-1,-1,-1),P(1,-1,-1),P(1,1,-1),P(-1,1,-1),-Zn);
        }
        void Box(const FVector& C,const FVector& H){Box(C,FVector(H.X,0,0),FVector(0,H.Y,0),FVector(0,0,H.Z));}
        // A box turned by yaw (degrees) about its centre's vertical.
        void TurnedBox(const FVector& C,const FVector& H,float Yaw,float Tilt=0)
        {
            const FQuat Q=FQuat(FRotator(0,Yaw,Tilt));
            Box(C,Q.RotateVector(FVector(H.X,0,0)),Q.RotateVector(FVector(0,H.Y,0)),Q.RotateVector(FVector(0,0,H.Z)));
        }
        // A tapered spar from A to B, its faces round the axis, one of them on top.
        void Spar(const FVector& A,const FVector& B,float RA,float RB,int32 Sides=8,bool bCapA=true,bool bCapB=true)
        {
            const FVector D=(B-A).GetSafeNormal();FVector X=FVector::CrossProduct(FVector::UpVector,D).GetSafeNormal();
            if(X.IsNearlyZero())X=FVector::RightVector;
            const FVector Y=FVector::CrossProduct(D,X);
            auto Dir=[&](float K){const float An=(K+.5f)*2*PI/Sides;return X*FMath::Cos(An)+Y*FMath::Sin(An);};
            for(int32 I=0;I<Sides;++I)
            {
                const FVector N0=Dir(I),N1=Dir(I+1),Mid=(N0+N1).GetSafeNormal();
                Quad(A+N0*RA,A+N1*RA,B+N1*RB,B+N0*RB,Mid,Mid,Mid,Mid);
                if(bCapA)Tri(A,A+N0*RA,A+N1*RA,-D);
                if(bCapB)Tri(B,B+N0*RB,B+N1*RB,D);
            }
        }
        // A rope or creeper hanging between two points, sagging, kept above Floor when given.
        void Tube(const TArray<FVector>& Path,float R0,float R1,int32 Sides=6)
        {
            for(int32 I=1;I<Path.Num();++I)
            {
                const float U0=(I-1)/float(Path.Num()-1),U1=I/float(Path.Num()-1);
                Spar(Path[I-1],Path[I],FMath::Lerp(R0,R1,U0),FMath::Lerp(R0,R1,U1),Sides,I==1,I==Path.Num()-1);
            }
        }
        // A plank along a centre line of its top (one point per step), width W, thickness per point.
        void Strip(const TArray<FVector>& Top,const FVector& Side,float W,const TArray<float>& Thick)
        {
            const FVector Sd=Side.GetSafeNormal()*W*.5f;
            for(int32 I=1;I<Top.Num();++I)
            {
                const FVector A=Top[I-1],B=Top[I],Dn0=FVector(0,0,Thick[I-1]),Dn1=FVector(0,0,Thick[I]);
                const FVector Up=FVector::CrossProduct(B-A,Sd).GetSafeNormal()*(FVector::CrossProduct(B-A,Sd).Z>=0?1:-1);
                Flat(A-Sd,B-Sd,B+Sd,A+Sd,Up);
                Flat(A-Sd-Dn0,B-Sd-Dn1,B+Sd-Dn1,A+Sd-Dn0,-Up);
                Flat(A+Sd,B+Sd,B+Sd-Dn1,A+Sd-Dn0,Sd.GetSafeNormal());
                Flat(A-Sd,B-Sd,B-Sd-Dn1,A-Sd-Dn0,-Sd.GetSafeNormal());
            }
            const FVector E0=(Top[0]-Top[1]).GetSafeNormal(),E1=(Top.Last()-Top[Top.Num()-2]).GetSafeNormal();
            Flat(Top[0]-Sd,Top[0]+Sd,Top[0]+Sd-FVector(0,0,Thick[0]),Top[0]-Sd-FVector(0,0,Thick[0]),E0);
            Flat(Top.Last()-Sd,Top.Last()+Sd,Top.Last()+Sd-FVector(0,0,Thick.Last()),Top.Last()-Sd-FVector(0,0,Thick.Last()),E1);
        }
    };
    // The far bank's ground cells, laid exactly as ChultCanyon lays them.
    struct FGround
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tan;
        void Tri(const FVector& A,const FVector& B,const FVector& D)
        {
            const FVector Nn=FVector::CrossProduct(B-A,D-A).GetSafeNormal();const int32 S=V.Num();
            for(const FVector& P:{A,B,D}){V.Add(P);N.Add(Nn);UV.Add(FVector2D(P.X/260,P.Y/260));C.Add(FColor::White);Tan.Add(FProcMeshTangent(FVector(1,0,-Nn.X/Nn.Z).GetSafeNormal(),false));}
            T.Append({S,S+2,S+1});
        }
    };

    const TCHAR* WoodMat=TEXT("/Game/Art/Materials/M_Wood.M_Wood");
    const TCHAR* DeckMat=TEXT("/Game/Art/Materials/M_WoodLight.M_WoodLight");
    const TCHAR* CanvasMat=TEXT("/Game/Art/Materials/M_Canvas.M_Canvas");
    const TCHAR* IronMat=TEXT("/Game/Art/Materials/M_RustIron.M_RustIron");
    const TCHAR* GroundMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_FallGround.M_FallGround");
    const TCHAR* VineMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_ChultHangingMoss.M_ChultHangingMoss");
    const TCHAR* AstralMat=TEXT("/Game/Art/Materials/M_AstralDepth.M_AstralDepth");
    const TCHAR* OilMat=TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist");

    UProceduralMeshComponent* Emit(AActor* Owner,const TCHAR* Name,const FTransform& FloorXf,const TArray<TPair<FSurf*,const TCHAR*>>& Sections,bool bSolid,bool bHidden,int32& Missing,int32& Triangles)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldTransform(FTransform(FloorXf.GetRotation(),AChultLanding::FloorOrigin()+FloorXf.GetLocation()));
        Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        int32 Section=0;
        for(const auto& Pair:Sections)
        {
            FSurf& S=*Pair.Key;if(S.T.IsEmpty())continue;
            Mesh->CreateMeshSection(Section,S.V,S.T,S.N,S.UV,TArray<FColor>(),S.Tan,bSolid);
            if(Pair.Value){auto* M=LoadObject<UMaterialInterface>(nullptr,Pair.Value);Missing+=!M;Mesh->SetMaterial(Section,M);}
            ++Section;Triangles+=S.T.Num()/3;
        }
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        if(bHidden){Mesh->SetHiddenInGame(true);Mesh->SetCastShadow(false);}
        Mesh->ComponentTags.Add(TEXT("ChultWreck"));
        return Mesh;
    }

    // Holes in the planking: the breach at the ground on the low side, the
    // bulwark the mast tore away, two sprung planks high on the other side.
    bool Broken(float S,float G)
    {
        const float T=FMath::Abs(G);if(T<FlatT)return false;
        const float K=GirthK(T);
        if(G<0)
        {
            const float A=StationAt(BreachFrom,200)+.03f*FMath::Sin(K*29),B=StationAt(BreachTo,200)+.035f*FMath::Sin(K*23+1);
            const float Lo=.36f+.03f*FMath::Sin(S*47),Hi=.73f+.04f*FMath::Sin(S*31+2);
            if(S>A&&S<B&&K>Lo&&K<Hi)return true;
            if(S>StationAt(BreakFrom,300)+.02f*FMath::Sin(K*40)&&S<StationAt(BreakTo,300)+.02f*FMath::Sin(K*33)&&K>.755f)return true;
            return false;
        }
        return (S>.44f&&S<.53f&&K>.55f&&K<.66f)||(S>-.62f&&S<-.56f&&K>.70f&&K<.76f);
    }

    // The fallen mast: its broken butt on the deck beside the stump, over the
    // gap in the low side's bulwark, its head on the ground toward the road.
    struct FMast{FVector Butt,Head;float RButt=32,RHead=22;FVector H;};
    const FMast& Mast()
    {
        static const FMast M=[]
        {
            FMast R;const float X=190;
            R.Butt=ToFloor(FVector(X,-110,DeckAt(X)+R.RButt));
            const float Fan=FMath::DegreesToRadians(25);
            R.H=(-YawHigh()*FMath::Cos(Fan)+YawFwd()*FMath::Sin(Fan)).GetSafeNormal2D();
            R.Head=R.Butt+R.H*1350;R.Head.Z=GroundAt(R.Head)+R.RHead;
            return R;
        }();
        return M;
    }
    // The deck edge where the torn bulwark lies (floor frame), at X along the ship.
    FVector PanelTop(float X){const float S=DeckStation(X),Z=DeckZ(S);return ToFloor(FVector(X,-(InnerHalf(S,Z)-10),Z));}
    FVector PanelFoot(float X){FVector P=PanelTop(X)-YawHigh()*PanelRun;P.Z=GroundAt(P)-1;return P;}
    // Inside the hull on the floor, on the deck: standing points (floor frame, ground under the feet).
    FVector HoldPoint(float X,float Y){return ToFloor(FVector(X,Y,FloorZ));}
    FVector DeckPoint(float X,float Y){return ToFloor(FVector(X,Y,DeckAt(X)));}
    // Outside the breach on the ground, 2.5 m out.
    FVector BreachApproach(){const float X=(BreachFrom+BreachTo)*.5f;const FVector P=ToFloor(Hull(StationAt(X,250),-GirthT(.55f)));return OnGround(P-YawHigh()*250);}

    // Ground clear of the jungle's plants (and trees, farther) round the wreck.
    bool InClearing(const FVector& P,float Margin)
    {
        const FLayout& L=Layout();
        if(!L.All.ExpandBy(Margin).IsInside(XY(P)))return false;
        const FVector D=P-Ship().GetLocation();
        const float X=FVector::DotProduct(D,YawFwd()),Y=FVector::DotProduct(D,YawHigh());
        if(X>-1450-Margin&&X<1700+Margin&&Y>-640-Margin&&Y<380+Margin)return true;
        for(const FRift& R:L.Rifts)if(FVector2D::Distance(R.C,XY(P))<R.Radius+70+Margin)return true;
        const FMast& M=Mast();
        if(FMath::PointDistToSegment(FVector(P.X,P.Y,0),FVector(M.Butt.X,M.Butt.Y,0),FVector(M.Head.X,M.Head.Y,0))<150+Margin)return true;
        // The yard and sail beside the mast's head; the torn bulwark ramp.
        const FVector Sail=M.Head-M.H*480;
        if(FVector::Dist2D(P,Sail)<720+Margin)return true;
        for(float X2:{PanelFrom,PanelTo})if(FVector::Dist2D(P,PanelFoot(X2))<220+Margin)return true;
        return false;
    }
    int32 ClearVegetation(AActor* Owner)
    {
        int32 Removed=0;const FVector G=AChultLanding::FloorOrigin();
        TArray<UInstancedStaticMeshComponent*> Sets;Owner->GetComponents(Sets);
        for(UInstancedStaticMeshComponent* Set:Sets)
        {
            if(!Set||Set->ComponentHasTag(TEXT("ChultWreck"))||!Set->GetStaticMesh())continue;
            const FString Name=Set->GetStaticMesh()->GetName();
            // Trees stand back farther: their crowns would come through the hull.
            const float Margin=Name.Contains(TEXT("Tree"))||Name.Contains(TEXT("Trunk"))?420.f:Name.Contains(TEXT("Rock"))?60.f:20.f;
            TArray<int32> Out;
            for(int32 I=0;I<Set->GetInstanceCount();++I)
            {
                FTransform X;Set->GetInstanceTransform(I,X,true);
                if(InClearing(X.GetLocation()-G,Margin))Out.Add(I);
            }
            if(Out.Num()){Set->RemoveInstances(Out);Removed+=Out.Num();}
        }
        return Removed;
    }
}

bool ChultWreckCutsCell(float S,float T)
{
    const FLayout& L=Layout();const FBox2D B=CellBox(S,T);
    if(!B.Intersect(L.All))return false;
    if(B.Intersect(L.HullBox))return true;
    for(const FRift& R:L.Rifts)if(B.Intersect(R.Box))return true;
    return false;
}

bool ChultWreckRuptureSwallows(const FVector& FeetWorld)
{
    const FVector P=FeetWorld-AChultLanding::FloorOrigin();const FLayout& L=Layout();
    if(!L.All.IsInside(XY(P)))return false;
    for(const FRift& R:L.Rifts)
        if(FVector2D::DistSquared(R.C,XY(P))<FMath::Square(R.Radius+5)&&P.Z<R.LipZ-40&&Inside(R.Edge,XY(P)))return true;
    return false;
}

void BuildChultWreck(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FLayout& L=Layout();
    int32 Missing=0,Triangles=0;
    FSurf Hull_,Deck,Iron,Vines,Hidden,SpiltHidden;

    // ---- The hull planking: flush-laid bottom, clinker sides (each strake laps
    // over the one below), inner face, plank thickness at every broken edge.
    TArray<FVector2f> Rows;
    for(float T:{0.f,FlatT/3,FlatT*2/3,FlatT})Rows.Add({T,0});
    for(int32 J=0;J<Strakes;++J)
    {
        const float K0=J/float(Strakes),K1=(J+1)/float(Strakes);
        Rows.Add({GirthT(K0),4});Rows.Add({GirthT((K0+K1)*.5f),2});Rows.Add({GirthT(K1),0});
    }
    TArray<FVector2f> Girth;
    for(int32 R=Rows.Num()-1;R>=1;--R)Girth.Add({-Rows[R].X,Rows[R].Y});
    for(int32 R=0;R<Rows.Num();++R)Girth.Add(Rows[R]);
    TArray<float> SS;for(int32 I=0;I<=Stations;++I)SS.Add(-1+2.f*I/Stations);
    auto Removed=[&](int32 I,int32 R)
    {
        if(I<0||I>=Stations||R<0||R>=Girth.Num()-1)return true;
        return Broken((SS[I]+SS[I+1])*.5f,(Girth[R].X+Girth[R+1].X)*.5f);
    };
    auto Out=[&](int32 I,int32 R){return Hull(SS[I],Girth[R].X,Girth[R].Y);};
    auto In=[&](int32 I,int32 R){return Hull(SS[I],Girth[R].X,-Shell);};
    auto Nrm=[&](int32 I,int32 R){return HullNormal(SS[I],Girth[R].X);};
    int32 Breached=0;
    for(int32 I=0;I<Stations;++I)for(int32 R=0;R<Girth.Num()-1;++R)
    {
        if(Removed(I,R)){++Breached;continue;}
        Hull_.Quad(Out(I,R),Out(I+1,R),Out(I+1,R+1),Out(I,R+1),Nrm(I,R),Nrm(I+1,R),Nrm(I+1,R+1),Nrm(I,R+1));
        Hull_.Quad(In(I,R),In(I+1,R),In(I+1,R+1),In(I,R+1),-Nrm(I,R),-Nrm(I+1,R),-Nrm(I+1,R+1),-Nrm(I,R+1));
        // The plank's own thickness wherever the planking stops.
        const FVector Mid=(Out(I,R)+Out(I+1,R+1))*.5f;
        auto Edge=[&](int32 IA,int32 RA,int32 IB,int32 RB)
        {
            const FVector A=Out(IA,RA),B=Out(IB,RB),Ai=In(IA,RA),Bi=In(IB,RB);
            const FVector Away=((A+B)*.5f-Mid).GetSafeNormal();
            Hull_.Flat(A,B,Bi,Ai,Away);
        };
        if(Removed(I-1,R))Edge(I,R,I,R+1);
        if(Removed(I+1,R))Edge(I+1,R,I+1,R+1);
        if(Removed(I,R-1))Edge(I,R,I+1,R);
        if(Removed(I,R+1))Edge(I,R+1,I+1,R+1);
    }
    // Stem and sternpost: the two sides closed across their faces.
    for(int32 End=0;End<2;++End)
    {
        const float S=End==0?-1.f:1.f;
        for(int32 R=0;R<Rows.Num()-1;++R)
        {
            const FVector A=Hull(S,Rows[R].X),B=Hull(S,Rows[R+1].X),C=Hull(S,-Rows[R+1].X),D=Hull(S,-Rows[R].X);
            Hull_.Flat(A,B,C,D,FVector(S,0,0));
        }
        // The post itself stands proud of the planking, keel to head.
        const FVector Foot=Hull(S,0)+FVector(S*6,0,-8);
        const FVector PostHead(S*EndX(S,SheerZ(S)+45),0,SheerZ(S)+45);
        Hull_.Spar(Foot,PostHead,15,13,6);
    }
    // The keel along the bottom.
    Hull_.Box(FVector(0,0,-9),FVector(KeelHalf+20,14,10));

    // ---- Inside: the hold's floor, the frames up the sides, the deck and its beams.
    auto Lanes=[&](float Half,float Width){TArray<float> Y;for(float C=-Half+Width*.5f;C<=Half-Width*.5f;C+=Width+2)Y.Add(C);return Y;};
    {
        // Floor planks, straight, as far as the hull's width lets each lane run.
        for(float Yc:Lanes(InnerHalf(0,FloorZ)-2,22))
        {
            float X0=1.e9f,X1=-1.e9f;
            for(float X=-1150;X<=1150;X+=20)if(InnerHalf(StationAt(X,FloorZ),FloorZ)>=FMath::Abs(Yc)+13){X0=FMath::Min(X0,X);X1=FMath::Max(X1,X);}
            if(X1>X0)Deck.Box(FVector((X0+X1)*.5f,Yc,FloorZ-3),FVector((X1-X0)*.5f,11,3));
        }
        // Frames every 75 cm up both sides, floor to deck.
        for(float X=-1000;X<=1060;X+=75)for(float Sg:{-1.f,1.f})
        {
            TArray<FVector> Inner,Face;
            for(int32 J=0;J<=10;++J)
            {
                const float Z=FMath::Lerp(FloorZ,DeckAt(X)-8,J/10.f),S=StationAt(X,Z),Y=InnerHalf(S,Z);
                if(Y<20){Inner.Reset();break;}
                Inner.Add(FVector(X,Sg*Y,Z));
            }
            for(int32 J=1;J<Inner.Num();++J)
            {
                const FVector A=Inner[J-1],B=Inner[J],Inw(0,-Sg,0),Side(1,0,0);
                Hull_.Flat(A+Inw*7-Side*6,B+Inw*7-Side*6,B+Inw*7+Side*6,A+Inw*7+Side*6,Inw);
                Hull_.Flat(A-Side*6,B-Side*6,B+Inw*7-Side*6,A+Inw*7-Side*6,-Side);
                Hull_.Flat(A+Side*6,B+Side*6,B+Inw*7+Side*6,A+Inw*7+Side*6,Side);
            }
        }
        // Deck planks follow the sheer; the hatch, the mast and two smashed planks are left open.
        for(float Yc:Lanes(InnerHalf(0,DeckZ(0))-2,22))
        {
            TArray<FVector> Run;TArray<float> Thick;
            auto Flush=[&]{if(Run.Num()>1)Deck.Strip(Run,FVector(0,1,0),22,Thick);Run.Reset();Thick.Reset();};
            for(float X=-1080;X<=1100;X+=20)
            {
                const float S=DeckStation(X),Z=DeckZ(S);
                const bool Open=InnerHalf(S,Z)-3<FMath::Abs(Yc)+11
                    ||(X>HatchAft&&X<HatchFore&&FMath::Abs(Yc)<HatchHalf+11)
                    ||(FMath::Abs(X-StumpX)<StumpR+6&&FMath::Abs(Yc)<StumpR+11)
                    ||(X>460&&X<580&&Yc>-310&&Yc<-250);
                if(Open){Flush();continue;}
                Run.Add(FVector(X,Yc,Z));Thick.Add(6);
            }
            Flush();
        }
        // Through-beams: under the deck, out through both sides (the cog's mark).
        for(float X:BeamX)
        {
            const float S=DeckStation(X),Zc=DeckZ(S)-6-11;
            Hull_.Box(FVector(X,0,Zc),FVector(11,OuterHalf(S,Zc)+35,11));
        }
        // The hatch's coaming on three sides (its fore edge, where the ramp comes up, is gone).
        {
            const float Z=DeckAt((HatchAft+HatchFore)*.5f);
            for(float Sg:{-1.f,1.f})Hull_.Box(FVector((HatchAft+HatchFore)*.5f,Sg*(HatchHalf+6),Z+10),FVector((HatchFore-HatchAft)*.5f+6,6,10));
            Hull_.Box(FVector(HatchAft-6,0,Z+10),FVector(6,HatchHalf+12,10));
        }
        // The hold ramp: three planks from the floor aft up through the hatch to its
        // fore edge, flush with the deck there; the foot is cut thin onto the floor.
        const float TopZ=DeckAt(HatchFore);
        for(float Yc:{-34.f,0.f,34.f})
        {
            TArray<FVector> Run;TArray<float> Thick;
            for(int32 J=0;J<=24;++J)
            {
                const float U=J/24.f,X=FMath::Lerp(RampFoot,HatchFore,U);
                Run.Add(FVector(X,Yc,FMath::Lerp(FloorZ+.5f,TopZ,U)));Thick.Add(J==0?.5f:J==1?4.f:7.f);
            }
            Deck.Strip(Run,FVector(0,1,0),32,Thick);
        }
        // Cleats across the ramp, as on a gangway.
        for(int32 J=3;J<24;J+=3)
        {
            const float U=J/24.f,X=FMath::Lerp(RampFoot,HatchFore,U);
            Hull_.Box(FVector(X,0,FMath::Lerp(FloorZ,TopZ,U)+1.5f),FVector(3,48,1.5f));
        }
    }

    // ---- The mast stump, broken off above the deck, and its iron bands.
    {
        const float Top=DeckAt(StumpX)+230;
        Hull_.Spar(FVector(StumpX,0,FloorZ),FVector(StumpX,0,Top),StumpR,StumpR-2,8,true,false);
        FRandomStream R(20261018);
        for(int32 I=0;I<8;++I)
        {
            // Splinters standing up from the break.
            const float A=I*2*PI/8+R.FRandRange(-.3f,.3f),H=R.FRandRange(12,70),Lean=R.FRandRange(3,22);
            const FVector At(StumpX+FMath::Cos(A)*(StumpR-R.FRandRange(6,16)),FMath::Sin(A)*(StumpR-R.FRandRange(6,16)),Top-R.FRandRange(0,10));
            Hull_.Spar(At,At+FVector(FMath::Cos(A+R.FRandRange(-.5f,.5f))*Lean,FMath::Sin(A)*Lean,H),R.FRandRange(4,10),R.FRandRange(.8f,2.5f),4);
        }
        Hull_.Box(FVector(StumpX,0,Top-6),FVector(StumpR-6,StumpR-6,6));
        for(float Z:{DeckAt(StumpX)+40,DeckAt(StumpX)+150})Iron.Spar(FVector(StumpX,0,Z),FVector(StumpX,0,Z+7),StumpR+1.5f,StumpR+1.5f,8);
        // Mast partners round its foot on the deck.
        for(float Sg:{-1.f,1.f})Hull_.Box(FVector(StumpX,Sg*(StumpR+9),DeckAt(StumpX)+5),FVector(StumpR+18,8,5));
    }

    // ---- The stern castle: a cabin over the deck with a door forward, its roof a railed platform.
    {
        const float Roof=DeckAt(CabinFore)+230;
        auto WallY=[&](float X){const float S=StationAt(X,SheerZ(DeckStation(X)));return OuterHalf(S,SheerZ(S))-2;};
        for(float Sg:{-1.f,1.f})
        {
            TArray<FVector> Line;
            for(float X=CabinAft;X<=CabinFore+.1f;X+=40)Line.Add(FVector(X,Sg*WallY(X),0));
            for(int32 J=1;J<Line.Num();++J)
            {
                const FVector A=Line[J-1],B=Line[J];
                const float Za=SheerZ(DeckStation(A.X))-4,Zb=SheerZ(DeckStation(B.X))-4;
                const FVector OutN=FVector::CrossProduct(B-A,FVector::UpVector).GetSafeNormal()*(Sg>0?-1:1);
                const FVector I8=-OutN*8;
                Hull_.Flat(FVector(A.X,A.Y,Za),FVector(B.X,B.Y,Zb),FVector(B.X,B.Y,Roof+70),FVector(A.X,A.Y,Roof+70),OutN);
                Hull_.Flat(FVector(A.X,A.Y,Za)+I8,FVector(B.X,B.Y,Zb)+I8,FVector(B.X,B.Y,Roof)+I8,FVector(A.X,A.Y,Roof)+I8,-OutN);
                Hull_.Flat(FVector(A.X,A.Y,Roof+70),FVector(B.X,B.Y,Roof+70),FVector(B.X,B.Y,Roof+70)+I8,FVector(A.X,A.Y,Roof+70)+I8,FVector::UpVector);
                // The rail's inner face above the roof.
                Hull_.Flat(FVector(A.X,A.Y,Roof+6)+I8,FVector(B.X,B.Y,Roof+6)+I8,FVector(B.X,B.Y,Roof+70)+I8,FVector(A.X,A.Y,Roof+70)+I8,-OutN);
            }
        }
        // Fore wall with its door; aft wall.
        {
            const float W=WallY(CabinFore),Z0=DeckAt(CabinFore)-2,Top=Roof+70;
            const float Door=Z0+CabinDoorTop;
            for(float Sg:{-1.f,1.f})Hull_.Box(FVector(CabinFore-4,Sg*(CabinDoor+W)*.5f,(Z0+Roof)*.5f),FVector(4,(W-CabinDoor)*.5f,(Roof-Z0)*.5f));
            Hull_.Box(FVector(CabinFore-4,0,(Door+Roof)*.5f),FVector(4,CabinDoor,(Roof-Door)*.5f));
            // The roof's rail across the front.
            Hull_.Box(FVector(CabinFore-4,0,(Roof+Top)*.5f+3),FVector(4,W,(Top-Roof)*.5f-3));
            const float Wa=WallY(CabinAft),Za=DeckAt(CabinAft)-2;
            Hull_.Box(FVector(CabinAft+4,0,(Za+Top)*.5f),FVector(4,Wa,(Top-Za)*.5f));
        }
        for(float Yc:Lanes(WallY(CabinFore)-8,22))
        {
            float X1=CabinFore;float X0=CabinFore;
            for(float X=CabinFore;X>=CabinAft;X-=20)if(WallY(X)-8>=FMath::Abs(Yc)+11)X0=X;
            if(X0<X1)Deck.Box(FVector((X0+X1)*.5f,Yc,Roof+3),FVector((X1-X0)*.5f,11,3));
        }
        // Roof beams across, seen from inside the cabin.
        for(float X=CabinFore-60;X>CabinAft+30;X-=90)Hull_.Box(FVector(X,0,Roof-8),FVector(8,WallY(X)-8,8));
    }

    // ---- The forecastle: a platform in the bow on two posts, railed round.
    {
        for(float Yc:Lanes(InnerHalf(StationAt(ForeFrom,ForeZ),ForeZ)-4,22))
        {
            float X1=ForeFrom;
            for(float X=ForeFrom;X<=1500;X+=20)if(InnerHalf(StationAt(X,ForeZ),ForeZ)-4>=FMath::Abs(Yc)+11)X1=X;
            if(X1>ForeFrom)Deck.Box(FVector((ForeFrom+X1)*.5f,Yc,ForeZ-3),FVector((X1-ForeFrom)*.5f,11,3));
        }
        const float W=InnerHalf(StationAt(ForeFrom,ForeZ),ForeZ);
        Hull_.Box(FVector(ForeFrom+8,0,ForeZ-14),FVector(9,W,8));
        for(float Sg:{-1.f,1.f})
        {
            const float Z0=DeckAt(ForeFrom);
            Hull_.Box(FVector(ForeFrom+12,Sg*(W-30),(Z0+ForeZ-6)*.5f),FVector(10,10,(ForeZ-6-Z0)*.5f));
            TArray<FVector> Line;
            for(float X=ForeFrom;X<=1400;X+=40){const float S=StationAt(X,ForeZ+30);if(OuterHalf(S,ForeZ+30)<20)break;Line.Add(FVector(X,Sg*(OuterHalf(S,ForeZ+30)-2),0));}
            for(int32 J=1;J<Line.Num();++J)
            {
                const FVector A=Line[J-1],B=Line[J];
                const float Za=SheerZ(StationAt(A.X,ForeZ))-4,Zb=SheerZ(StationAt(B.X,ForeZ))-4;
                const FVector OutN=FVector::CrossProduct(B-A,FVector::UpVector).GetSafeNormal()*(Sg>0?-1:1),I8=-OutN*7;
                Hull_.Flat(FVector(A.X,A.Y,Za),FVector(B.X,B.Y,Zb),FVector(B.X,B.Y,ForeZ+60),FVector(A.X,A.Y,ForeZ+60),OutN);
                Hull_.Flat(FVector(A.X,A.Y,Za)+I8,FVector(B.X,B.Y,Zb)+I8,FVector(B.X,B.Y,ForeZ+60)+I8,FVector(A.X,A.Y,ForeZ+60)+I8,-OutN);
                Hull_.Flat(FVector(A.X,A.Y,ForeZ+60),FVector(B.X,B.Y,ForeZ+60),FVector(B.X,B.Y,ForeZ+60)+I8,FVector(A.X,A.Y,ForeZ+60)+I8,FVector::UpVector);
            }
        }
        // The bowsprit.
        const FVector Root(EndX(1,470)-60,0,470);
        Hull_.Spar(Root,Root+FVector(1,0,.32f).GetSafeNormal()*520,16,9,8);
        Iron.Spar(Root+FVector(80,0,25),Root+FVector(88,0,28),17.5f,17.5f,8);
    }

    // ---- The rudder hanging off the sternpost, swung hard over, and its iron.
    {
        const float Z0=40,Z1=440,PostX=-EndX(-1,240)-12;
        const float Turn=FMath::DegreesToRadians(28);
        const FVector Aft(-FMath::Cos(Turn),FMath::Sin(Turn),0);
        const FVector C=FVector(PostX,0,(Z0+Z1)*.5f)+Aft*62;
        Hull_.Box(C,Aft*55,FVector(-Aft.Y,Aft.X,0)*7,FVector(0,0,(Z1-Z0)*.5f));
        for(float Z:{110.f,250.f,390.f})Iron.Box(FVector(PostX,0,Z)+Aft*30,Aft*36,FVector(-Aft.Y,Aft.X,0)*9,FVector(0,0,4));
    }

    // ---- Cargo in the hold, slid down against the low side; crates are standing ground.
    auto* CrateMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    auto* CoilMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_RopeCoil.SM_RopeCoil"));
    Missing+=!CrateMesh+!BarrelMesh+!CoilMesh;
    // Cargo props are plain mesh components (their materials aren't all made for instancing).
    struct FProps
    {
        AActor* Owner;UStaticMesh* Mesh;FTransform Parent;FString Name;int32 N=0;
        void AddInstance(const FTransform& Local)
        {
            auto* Prop=NewObject<UStaticMeshComponent>(Owner,*FString::Printf(TEXT("%s%d"),*Name,N++));Prop->SetupAttachment(Owner->GetRootComponent());
            const FTransform X=Local*Parent;
            Prop->SetWorldTransform(FTransform(X.GetRotation(),AChultLanding::FloorOrigin()+X.GetLocation(),X.GetScale3D()));
            Prop->SetStaticMesh(Mesh);Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);Prop->ComponentTags.Add(TEXT("ChultWreck"));Prop->RegisterComponent();
        }
    };
    auto MakeSet=[&](UStaticMesh* Mesh,const TCHAR* Name,const FTransform& FloorXf){return FProps{Owner,Mesh,FloorXf,Name};};
    auto HoldCrates=MakeSet(CrateMesh,TEXT("ChultWreckHoldCrates"),Ship());
    auto HoldBarrels=MakeSet(BarrelMesh,TEXT("ChultWreckHoldBarrels"),Ship());
    int32 Cargo=0;
    {
        struct FCrate{float X,Y,Z,Yaw;};
        const FCrate Crates[]={{-300,-270,0,8},{-180,-235,0,-12},{420,-250,0,4},{480,-190,0,-9},{545,-260,0,15},{450,-245,60,-4},{760,-200,0,30},{-560,-150,0,-25}};
        for(const FCrate& C:Crates)
        {
            const FVector At(C.X,C.Y,FloorZ+30+C.Z);
            HoldCrates.AddInstance(FTransform(FRotator(0,C.Yaw,0),At));
            Hidden.TurnedBox(At,FVector(30,32.5f,30),C.Yaw);++Cargo;
        }
        struct FBarrel{float X,Y,Yaw;bool bLying;};
        const FBarrel Barrels[]={{-650,-240,0,true},{-700,-175,0,false},{-640,-165,0,false},{150,-245,80,true},{960,-150,10,false}};
        for(const FBarrel& B:Barrels)
        {
            const FVector At(B.X,B.Y,FloorZ+(B.bLying?31:45));
            const FRotator Rot(B.bLying?90:0,B.Yaw,0);
            HoldBarrels.AddInstance(FTransform(Rot,At));
            if(B.bLying)Hidden.TurnedBox(At,FVector(45,29,29),B.Yaw);else Hidden.Box(At,FVector(28,28,45));
            ++Cargo;
        }
    }

    // ---- A little of the jungle has found it: creepers over the high gunwale.
    {
        FRandomStream R(1018);
        for(float X:{-520.f,150.f,610.f,880.f})
        {
            TArray<FVector> Path;const float Drop=R.FRandRange(.18f,.36f);
            for(int32 J=0;J<=8;++J)
            {
                const float T=1-Drop*J/8.f,S=StationAt(X+FMath::Sin(J*1.3f)*14,SheerZ(0));
                Path.Add(Hull(S,T,7+2*FMath::Sin(J*2.1f))+FVector(0,0,J==0?6:0));
            }
            Path.Insert(FVector(X-4,OuterHalf(StationAt(X,DeckAt(X)),DeckAt(X))-10,SheerZ(StationAt(X,400))+5),0);
            Vines.Tube(Path,4.5f,1.5f,5);
        }
        // One creeper up the high side's bilge from the ground.
        TArray<FVector> Path;
        for(int32 J=0;J<=10;++J)Path.Add(Hull(.28f+.01f*FMath::Sin(J*1.7f),.05f+.5f*J/10.f,5));
        Vines.Tube(Path,3.5f,1.2f,5);
    }

    Emit(Owner,TEXT("ChultWreckHull"),Ship(),{{&Hull_,WoodMat},{&Deck,DeckMat},{&Iron,IronMat}},true,false,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckCreepers"),Ship(),{{&Vines,VineMat}},false,false,Missing,Triangles);

    // ---- On the ground: the fallen mast, its yard and sail, rigging, the torn bulwark.
    FSurf Spars,Canvas,Rope,Panel,Ramp,Soil,Collar,Depth,Veils;
    const FMast& M=Mast();
    const FVector Side(-M.H.Y,M.H.X,0),BowSide=Side*FMath::Sign(FVector::DotProduct(Side,YawFwd()));
    {
        Spars.Spar(M.Butt,M.Head,M.RButt,M.RHead,8);
        // Its broken butt: splinters.
        FRandomStream R(77);const FVector Back=(M.Butt-M.Head).GetSafeNormal();
        for(int32 I=0;I<7;++I)
        {
            const float A=I*2*PI/7+R.FRandRange(-.3f,.3f);
            FVector X=FVector::CrossProduct(FVector::UpVector,Back).GetSafeNormal();const FVector Y=FVector::CrossProduct(Back,X);
            const FVector At=M.Butt+(X*FMath::Cos(A)+Y*FMath::Sin(A))*(M.RButt-10);
            Spars.Spar(At,At+Back*R.FRandRange(20,55),8,1.5f,4);
        }
        // The masthead's cap and a sheave block.
        const FVector D=(M.Head-M.Butt).GetSafeNormal();
        Spars.Spar(M.Head-D*40,M.Head+D*10,M.RHead+8,M.RHead+8,8);
        // How high the fallen mast holds anything laid over it at P, falling away to its sides.
        auto MastTop=[&](const FVector& P,float Slope)
        {
            const FVector Closest=FMath::ClosestPointOnSegment(FVector(P.X,P.Y,0),FVector(M.Butt.X,M.Butt.Y,0),FVector(M.Head.X,M.Head.Y,0));
            const float U=FVector::Dist2D(Closest,M.Butt)/FMath::Max(1.f,float(FVector::Dist2D(M.Head,M.Butt)));
            const float Z=FMath::Lerp(float(M.Butt.Z),float(M.Head.Z),U),Rad=FMath::Lerp(M.RButt,M.RHead,U);
            return Z+Rad-Slope*FMath::Max(0.f,float(FVector::Dist2D(Closest,P))-Rad*.5f);
        };
        // The yard came down across the mast near its head and broke in two over it:
        // both halves run from the mast down to the ground.
        const FVector YardMid=M.Head-M.H*170;
        const FVector YardTop=FVector(YardMid.X,YardMid.Y,MastTop(YardMid,0)+14);
        const FVector Y0=OnGround(YardMid-Side*560+M.H*40,11),Y1=OnGround(YardMid+Side*600-M.H*30,11);
        Spars.Spar(YardTop-Side*14,Y0,15,9,8);Spars.Spar(YardTop+Side*14+FVector(0,0,-4),Y1,15,9,8);
        for(float Sg:{-1.f,1.f})for(int32 I=0;I<4;++I)
        {
            const FVector At=YardTop+Side*(Sg*14)+FVector(0,0,(I-1.5f)*6);
            Spars.Spar(At,At+Side*(Sg*(14+I*7))+M.H*((I-1.5f)*5),5,1.2f,4);
        }
        // The sail, still bent to the yard, spread back over the mast toward the hull
        // and down on the ground in long folds; ragged at its foot, a rip and a hole.
        constexpr int32 NA=40,NB=26;TArray<FVector> Grid;int32 SailTornForRift=0;
        FRandomStream Jit(4242);
        auto YardAt=[&](float U){return U<.5f?FMath::Lerp(Y0,YardTop,U*2):FMath::Lerp(YardTop,Y1,U*2-1);};
        for(int32 B=0;B<=NB;++B)for(int32 A=0;A<=NA;++A)
        {
            const float U=A/float(NA),V=B/float(NB);
            const FVector Head=YardAt(U);
            FVector P=Head-M.H*(V*640)+Side*(V*(U-.5f)*120)+Side*(18*FMath::Sin(V*6+U*3));
            if(B>0&&A>0&&A<NA)P+=FVector(Jit.FRandRange(-7,7),Jit.FRandRange(-7,7),0);
            const float Ground=GroundAt(P);
            float Z=Ground+3+9*FMath::Square(FMath::Sin(U*11+V*2.5f))+5*FMath::Sin(V*13-U*5);
            Z=FMath::Max(Z,Ground+2);
            Z=FMath::Max(Z,MastTop(P,.7f)+3);
            Z=FMath::Max(Z,float(Head.Z)+4-.75f*V*640);
            P.Z=Z;Grid.Add(P);
        }
        auto Torn=[&](float U,float V)
        {
            if(V>.8f+.11f*FMath::Sin(U*17)+.06f*FMath::Sin(U*41+1))return true;     // the ragged foot
            if(U<.03f+.04f*FMath::Max(0.f,FMath::Sin(V*9))&&V>.25f)return true;     // a torn leech
            const float Rip=U-FMath::Lerp(.63f,.74f,V)-.012f*FMath::Sin(V*57);
            if(V>.28f&&FMath::Abs(Rip)<.012f+.01f*V)return true;                     // a long rip
            return FVector2D::Distance(FVector2D(U*1.6f,V),FVector2D(.3f*1.6f,.6f))<.06f+.02f*FMath::Sin(U*90+V*70); // a hole
        };
        for(int32 B=0;B<NB;++B)for(int32 A=0;A<NA;++A)
        {
            const int32 I=B*(NA+1)+A;
            const FVector& P0=Grid[I];const FVector& P1=Grid[I+1];const FVector& P2=Grid[I+NA+2];const FVector& P3=Grid[I+NA+1];
            const float U=(A+.5f)/NA,V=(B+.5f)/NB,Du=.25f/NA,Dv=.25f/NB;
            // Each square's two triangles go separately, so the tears run on the diagonals too.
            for(int32 Half=0;Half<2;++Half)
            {
                const float Cu=U+(Half?-Du:Du),Cv=V+(Half?Dv:-Dv);if(Torn(Cu,Cv))continue;
                const FVector& Q1=Half?P2:P1;const FVector& Q2=Half?P3:P2;
                // Never over a rupture: the canvas lies torn back round any it reaches.
                const FVector2D Mid=XY((P0+Q1+Q2)/3);bool bOver=false;
                for(const FRift& Rf:L.Rifts)bOver|=FVector2D::Distance(Mid,Rf.C)<Rf.Radius+45;
                if(bOver){++SailTornForRift;continue;}
                FVector N=FVector::CrossProduct(Q1-P0,Q2-P0).GetSafeNormal();if(N.Z<0)N=-N;
                Canvas.Tri(P0,Q1,Q2,N);Canvas.Tri(P0,Q1,Q2,-N);
            }
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_SAIL triangles=%d torn_back_from_ruptures=%d"),Canvas.T.Num()/3,SailTornForRift);
        // Rigging: shrouds from the mast to the high side's rail, slack; loose lines to the ground.
        auto Hang=[&](const FVector& A,const FVector& B,float Sag)
        {
            TArray<FVector> Path;
            for(int32 J=0;J<=12;++J)
            {
                const float U=J/12.f;FVector P=FMath::Lerp(A,B,U)-FVector(0,0,Sag*4*U*(1-U));
                P.Z=FMath::Max(float(P.Z),GroundAt(P)+2.5f);Path.Add(P);
            }
            Rope.Tube(Path,2.2f,2.2f,6);
        };
        const FVector MastAt=M.Butt+D*300+FVector(0,0,M.RButt-6);
        for(float X:{-40.f,200.f,430.f}){const float S=StationAt(X,SheerZ(0));Hang(MastAt,ToFloor(FVector(X,OuterHalf(S,SheerZ(S))+4,SheerZ(S)+2)),60);}
        Hang(ToFloor(FVector(StumpX,0,DeckAt(StumpX)+200)),ToFloor(FVector(-130,-OuterHalf(StationAt(-130,400),300)-3,300)),45);
        Hang(M.Head+FVector(0,0,M.RHead),OnGround(M.Head+BowSide*420+M.H*150,2),40);
        Hang(M.Head-D*120+FVector(0,0,M.RHead),Y1+FVector(0,0,12),30);
        Hang(ToFloor(FVector(EndX(1,470)-60,0,470)+FVector(1,0,.32f).GetSafeNormal()*500),OnGround(ToFloor(FVector(1900,-150,0)),2),70);
        Hang(MastAt+D*400,OnGround(M.Head-M.H*700-BowSide*350,2),50);
    }
    {
        // The torn bulwark: a panel of strakes on two frames lying from the deck edge to
        // the ground, a ramp up onto the deck (its walking face is a hidden slab).
        const FVector T0=PanelTop(PanelFrom),T1=PanelTop(PanelTo),F0=PanelFoot(PanelFrom),F1=PanelFoot(PanelTo);
        const FVector Dn=-FVector(0,0,8);
        Ramp.Flat(T0,T1,F1,F0,FVector::UpVector);Ramp.Flat(T0+Dn,T1+Dn,F1+Dn,F0+Dn,-FVector::UpVector);
        Ramp.Flat(T0,F0,F0+Dn,T0+Dn,-YawFwd());Ramp.Flat(T1,F1,F1+Dn,T1+Dn,YawFwd());
        const FVector Up=FVector::CrossProduct(T1-T0,F0-T0).GetSafeNormal()*(FVector::CrossProduct(T1-T0,F0-T0).Z>0?1:-1);
        for(int32 J=0;J<6;++J)
        {
            const float U0=(J+.08f)/6.f,U1=(J+.92f)/6.f;
            const FVector A=FMath::Lerp(T0,F0,U0)-YawFwd()*12,B=FMath::Lerp(T1,F1,U0)+YawFwd()*12,C=FMath::Lerp(T1,F1,U1)+YawFwd()*12,Dd=FMath::Lerp(T0,F0,U1)-YawFwd()*12;
            const FVector Lift=Up*(1.2f+.6f*J/6.f);
            Panel.Box((A+B+C+Dd)*.25f+Lift-Up*2.5f,(B-A)*.5f,(Dd-A)*.5f,Up*2.5f);
        }
        for(float U:{.15f,.85f})
        {
            const FVector A=FMath::Lerp(T0,T1,U),B=FMath::Lerp(F0,F1,U);
            Panel.Box((A+B)*.5f-Up*9,(A-B)*.5f,YawFwd()*6,Up*6);
        }
    }

    // ---- The Astral ruptures: the sewer's natural tears in the ground. A soil lip
    // over the cut, a well of the Astral glowing below it, the oil-film veils
    // standing at its long edges, a purple light.
    int32 Lights=0;
    for(int32 K=0;K<L.Rifts.Num();++K)
    {
        const FRift& R=L.Rifts[K];const int32 N=R.Edge.Num();
        TArray<FVector> E,Nout;
        for(int32 I=0;I<N;++I)
        {
            const FVector2D D=(R.Edge[(I+1)%N]-R.Edge[(I+N-1)%N]).GetSafeNormal();
            FVector2D O(D.Y,-D.X);if(FVector2D::DotProduct(O,R.Edge[I]-R.C)<0)O=-O;
            E.Add(OnGround(FVector(R.Edge[I].X,R.Edge[I].Y,0)));Nout.Add(FVector(O.X,O.Y,0));
        }
        const FVector Centre(R.C.X,R.C.Y,R.LipZ);
        for(int32 I=0;I<N;++I)
        {
            const int32 J=(I+1)%N;
            // The soil collar laps over the cut ground, then breaks down inside.
            const FVector OA=OnGround(E[I]+Nout[I]*14,2.5f),OB=OnGround(E[J]+Nout[J]*14,2.5f);
            const FVector PA=E[I]+FVector(0,0,.8f),PB=E[J]+FVector(0,0,.8f);
            const FVector IA=E[I]-Nout[I]*5-FVector(0,0,26+6*FMath::Sin(I*2.7f)),IB=E[J]-Nout[J]*5-FVector(0,0,26+6*FMath::Sin(J*2.7f));
            Collar.Quad(OA,OB,PB,PA,FVector::UpVector,FVector::UpVector,FVector::UpVector,FVector::UpVector);
            Collar.Quad(PA,PB,IB,IA,-Nout[I],-Nout[J],-Nout[J],-Nout[I]);
            // The well below, and the deep surface he sees.
            const FVector WA=E[I]-FVector(0,0,18),WB=E[J]-FVector(0,0,18),BA=E[I]-FVector(0,0,300),BB=E[J]-FVector(0,0,300);
            Depth.Quad(WA,WB,BB,BA,-Nout[I],-Nout[J],-Nout[J],-Nout[I]);
            Depth.Tri(Centre-FVector(0,0,18),WA,WB,FVector::UpVector);
            Depth.Tri(Centre-FVector(0,0,300),BA,BB,FVector::UpVector);
        }
        // Two upright, curved veils along the long edges, fading upward (the sewer's).
        const float Sc=FMath::Clamp(R.Wid/58.f,.55f,1.1f);constexpr int32 Levels=8;
        for(int32 SideI=0;SideI<2;++SideI)
        {
            TArray<int32> Idx;
            if(SideI==0)for(int32 I=0;I<=RiftSide;++I)Idx.Add(I);
            else{Idx.Add(RiftSide);for(int32 I=RiftSide+1;I<N;++I)Idx.Add(I);Idx.Add(0);}
            for(int32 Row=1;Row<Idx.Num();++Row)for(int32 Lv=0;Lv<Levels;++Lv)
            {
                auto P=[&](int32 Rw,int32 Lev)
                {
                    const float U=Rw/float(Idx.Num()-1),H=Lev/float(Levels);
                    const FVector Ed=E[Idx[Rw]],InD=(Centre-Ed).GetSafeNormal2D();
                    return Ed+InD*(H*H*12*Sc*FMath::Sin(U*13+SideI))+FVector(0,0,3+H*Sc*(72+10*FMath::Sin(U*11+SideI)));
                };
                const FVector A=P(Row-1,Lv),B=P(Row,Lv),C=P(Row,Lv+1),D=P(Row-1,Lv+1);
                const FVector Nn=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
                const int32 First=Veils.V.Num();
                Veils.TwoSided(A,B,C,D,Nn,Nn,Nn,Nn);
                // Veil UVs: U along the edge, V up (the oil film's shader fades with V).
                const float U0=(Row-1)/float(Idx.Num()-1),U1=Row/float(Idx.Num()-1),V0=Lv/float(Levels),V1=(Lv+1)/float(Levels);
                for(int32 Rep=0;Rep<2;++Rep){const int32 B0=First+Rep*4;Veils.UV[B0]=FVector2D(U0,V0);Veils.UV[B0+1]=FVector2D(U1,V0);Veils.UV[B0+2]=FVector2D(U1,V1);Veils.UV[B0+3]=FVector2D(U0,V1);}
            }
        }
        auto* Lamp=NewObject<UPointLightComponent>(Owner,*FString::Printf(TEXT("ChultWreckRiftLight%d"),K));Lamp->SetupAttachment(Owner->GetRootComponent());
        Lamp->SetWorldLocation(AChultLanding::FloorOrigin()+Centre+FVector(0,0,30));
        Lamp->SetIntensity(R.Wid>45?3200:2000);Lamp->SetAttenuationRadius(R.Wid>45?560:380);Lamp->SetLightColor(FLinearColor(.48f,.035f,1));
        Lamp->SetCastShadows(false);Lamp->ComponentTags.Add(TEXT("ChultWreck"));Lamp->RegisterComponent();++Lights;
    }

    // ---- The ground again where it was cut: each cut cell laid as the canyon lays
    // it, in 10 cm squares where the cut crosses it, without the cut.
    int32 CutCells=0;
    {
        FGround Ground;
        for(float S=4500;S<11500;S+=100)for(float T=-4500;T<4500;T+=100)
        {
            if(!ChultWreckCutsCell(S,T))continue;++CutCells;
            FVector A=ChultCanyonFrame(S,T),B=ChultCanyonFrame(S+100,T),D=ChultCanyonFrame(S+100,T+100),E=ChultCanyonFrame(S,T+100);
            const float ZA=GroundAt(A),ZB=GroundAt(B),ZD=GroundAt(D),ZE=GroundAt(E);
            auto Pt=[&](float U,float V){FVector P=ChultCanyonFrame(S+U*100,T+V*100);P.Z=U>=V?ZA+U*(ZB-ZA)+V*(ZD-ZB):ZA+V*(ZE-ZA)+U*(ZD-ZE);return P;};
            int32 Cut=0;
            for(int32 I=0;I<10;++I)for(int32 J=0;J<10;++J)Cut+=CutAt(XY(Pt((I+.5f)/10,(J+.5f)/10)));
            if(Cut==100)continue;
            if(Cut==0){A.Z=ZA;B.Z=ZB;D.Z=ZD;E.Z=ZE;Ground.Tri(A,B,D);Ground.Tri(A,D,E);continue;}
            for(int32 I=0;I<10;++I)for(int32 J=0;J<10;++J)
            {
                const float U0=I/10.f,U1=(I+1)/10.f,V0=J/10.f,V1=(J+1)/10.f;
                if(CutAt(XY(Pt((U0+U1)*.5f,(V0+V1)*.5f))))continue;
                const FVector a=Pt(U0,V0),b=Pt(U1,V0),d=Pt(U1,V1),e=Pt(U0,V1);
                Ground.Tri(a,b,d);Ground.Tri(a,d,e);
            }
        }
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,TEXT("ChultWreckGround"));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        Mesh->CreateMeshSection(0,Ground.V,Ground.T,Ground.N,Ground.UV,Ground.C,Ground.Tan,true);
        auto* Mat=LoadObject<UMaterialInterface>(nullptr,GroundMat);Missing+=!Mat;Mesh->SetMaterial(0,Mat);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->ComponentTags.Add(TEXT("ChultWreck"));Triangles+=Ground.T.Num()/3;
    }

    // ---- Cargo spilled out of the breach onto the ground.
    {
        auto Crates=MakeSet(CrateMesh,TEXT("ChultWreckSpiltCrates"),FTransform::Identity);
        auto Barrels=MakeSet(BarrelMesh,TEXT("ChultWreckSpiltBarrels"),FTransform::Identity);
        auto Coils=MakeSet(CoilMesh,TEXT("ChultWreckSpiltCoils"),FTransform::Identity);
        const FVector Outward=-YawHigh(),Fwd=YawFwd(),Base=ToFloor(FVector(BreachFrom-90,0,0));
        auto AtGround=[&](float Aft,float Off){FVector P=Base-Fwd*Aft;P=FVector(P.X,P.Y,0)+Outward*Off;return OnGround(P);};
        const float Yaw=Fwd.Rotation().Yaw;
        struct FPiece{float Aft,Off,Turn,Tilt;};
        for(const FPiece& P:{FPiece{0,520,14,0},FPiece{50,610,-21,9},FPiece{-40,470,40,-14}})
        {
            const FVector At=AtGround(P.Aft,P.Off)+FVector(0,0,28);
            Crates.AddInstance(FTransform(FRotator(0,Yaw+P.Turn,P.Tilt),At));SpiltHidden.TurnedBox(At,FVector(30,32.5f,30),Yaw+P.Turn,P.Tilt);++Cargo;
        }
        const FVector BarrelAt=AtGround(80,560)+FVector(0,0,29);
        Barrels.AddInstance(FTransform(FRotator(90,Yaw+70,0),BarrelAt));SpiltHidden.TurnedBox(BarrelAt,FVector(45,28,28),Yaw+70);++Cargo;
        Coils.AddInstance(FTransform(FRotator(0,Yaw+30,0),AtGround(30,700)+FVector(0,0,2)));++Cargo;
    }

    Emit(Owner,TEXT("ChultWreckSpars"),FTransform::Identity,{{&Spars,WoodMat},{&Panel,WoodMat}},true,false,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckCanvas"),FTransform::Identity,{{&Canvas,CanvasMat},{&Rope,CanvasMat}},false,false,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckWalks"),FTransform::Identity,{{&Ramp,nullptr}},true,true,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckCargoCollision"),Ship(),{{&Hidden,nullptr}},true,true,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckSpiltCollision"),FTransform::Identity,{{&SpiltHidden,nullptr}},true,true,Missing,Triangles);
    Emit(Owner,TEXT("ChultWreckRiftLips"),FTransform::Identity,{{&Collar,GroundMat}},false,false,Missing,Triangles);
    auto* DepthMesh=Emit(Owner,TEXT("ChultWreckRiftDepth"),FTransform::Identity,{{&Depth,AstralMat}},false,false,Missing,Triangles);DepthMesh->SetCastShadow(false);
    auto* VeilMesh=Emit(Owner,TEXT("ChultWreckRiftVeils"),FTransform::Identity,{{&Veils,OilMat}},false,false,Missing,Triangles);VeilMesh->SetCastShadow(false);

    const int32 Cleared=ClearVegetation(Owner);
    // The road and the bridge exit's jungle are laid later (ChultPath, at 0.6 s): clear again after.
    FTimerHandle Later;
    Owner->GetWorldTimerManager().SetTimer(Later,FTimerDelegate::CreateWeakLambda(Owner,[Owner]()
    {
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_CLEARED instances=%d pass=2"),ClearVegetation(Owner));
    }),1.2f,false);

    const FVector Bow=ToFloor(Hull(1,0)),Stern=ToFloor(Hull(-1,0));
    const float LowEdge=ToFloor(FVector(0,-InnerHalf(0,DeckZ(0)),DeckZ(0))).Z-GroundAt(ToFloor(FVector(0,-InnerHalf(0,DeckZ(0)),DeckZ(0))));
    // How far the mast clears the deck edge it lies over (in the ship's frame).
    float OverEdge=-1;
    for(int32 J=0;J<=200;++J)
    {
        const FVector Ls=ToShip(FMath::Lerp(M.Butt,M.Head,J/200.f));const float S=DeckStation(Ls.X);
        if(Ls.Y<-OuterHalf(S,DeckZ(S))){OverEdge=Ls.Z-FMath::Lerp(M.RButt,M.RHead,J/200.f)-DeckZ(S);break;}
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK cog=1 crew=0 length_cm=%.0f heel=%.0f ruptures=%d rupture_lights=%d cut_cells=%d breached_quads=%d cargo=%d cleared=%d triangles=%d missing_assets=%d low_deck_edge_cm=%.0f bow_s=%.0f stern_s=%.0f ms=%.0f"),
        FVector::Dist2D(Bow,Stern),HeelDeg,L.Rifts.Num(),Lights,CutCells,Breached,Cargo,Cleared,Triangles,Missing,LowEdge,Frame(Bow).X,Frame(Stern).X,(FPlatformTime::Seconds()-Started)*1000);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_MAST length_cm=%.0f head_s=%.0f head_t=%.0f over_edge_cm=%.0f"),FVector::Dist(M.Butt,M.Head),Frame(M.Head).X,Frame(M.Head).Y,OverEdge);
}

int32 CheckChultWreck(AActor* Owner,AChuckCharacter* Chuck)
{
    auto* W=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();const FLayout& L=Layout();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultWreck),true,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    int32 Failures=0,Open=0,Rims=0,Floors=0,Road=0;bool bBreach=true,bRamp=true;
    // Every rupture is open: nothing to stand on down to 4 m. Its rim is ground.
    for(const FRift& R:L.Rifts)
    {
        FHitResult Hit;const FVector C(R.C.X,R.C.Y,R.LipZ);
        if(W->LineTraceSingleByChannel(Hit,G+C+FVector(0,0,150),G+C-FVector(0,0,400),ECC_Pawn,Q))
        {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_RIFT_BLOCKED at=%s component=%s"),*C.ToString(),*GetNameSafe(Hit.GetComponent()));}
        else ++Open;
        for(int32 I=0;I<R.Edge.Num();I+=R.Edge.Num()/6)
        {
            const FVector2D At=R.Edge[I]+(R.Edge[I]-R.C).GetSafeNormal()*45;const FVector P=OnGround(FVector(At.X,At.Y,0));
            if(!W->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,150),G+P-FVector(0,0,80),ECC_Pawn,Q)){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_RIM_HOLE at=%s"),*P.ToString());continue;}
            const FString Name=GetNameSafe(Hit.GetComponent());
            if(!Name.Contains(TEXT("Ground"))&&!Name.Contains(TEXT("Banks")))continue;
            ++Rims;if(FMath::Abs(Hit.ImpactPoint.Z-G.Z-P.Z)>8){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_RIM_HEIGHT at=%s hit=%.1f"),*P.ToString(),Hit.ImpactPoint.Z-G.Z);}
        }
    }
    // Standing ground inside: the hold's floor, the deck, the cabin, the ramps.
    const float RampU=.45f;
    const FVector RampMid=ToFloor(FVector(FMath::Lerp(RampFoot,HatchFore,RampU),0,FMath::Lerp(FloorZ,DeckAt(HatchFore),RampU)));
    for(const FVector& P:{HoldPoint(-300,-120),HoldPoint(-850,-110),DeckPoint(-500,0),DeckPoint(600,100),DeckPoint(-920,0),DeckPoint(50,-300),RampMid,FMath::Lerp(PanelTop(50),PanelFoot(50),.5f)})
    {
        FHitResult Hit;++Floors;
        const bool Found=W->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,60),G+P-FVector(0,0,40),ECC_Pawn,Q);
        if(!Found||FMath::Abs(Hit.ImpactPoint.Z-G.Z-P.Z)>10||Hit.ImpactNormal.Z<.7f)
        {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_FLOOR_FAIL at=%s found=%d hit=%.1f normal=%.2f component=%s"),*P.ToString(),Found,Found?Hit.ImpactPoint.Z-G.Z:0.,Found?Hit.ImpactNormal.Z:0.,*GetNameSafe(Hit.GetComponent()));}
    }
    // In through the breach at the ground: nothing in the way until he's over the hold.
    {
        const FVector From=BreachApproach(),Goal=HoldPoint(-300,-120),Dir=(FVector(Goal.X,Goal.Y,0)-FVector(From.X,From.Y,0)).GetSafeNormal();
        FVector Edge=From;for(int32 I=0;I<400&&!CutAt(XY(Edge));++I)Edge+=Dir*2;
        const FVector A=From+FVector(0,0,Half+6),B=FVector(Edge.X,Edge.Y,A.Z)+Dir*30;FHitResult Hit;
        if(W->SweepSingleByChannel(Hit,G+A,G+B,FQuat::Identity,ECC_Pawn,Capsule,Q)){bBreach=false;++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_BREACH_BLOCKED component=%s at=%s"),*GetNameSafe(Hit.GetComponent()),*(Hit.ImpactPoint-G).ToString());}
    }
    // Up the torn bulwark onto the deck.
    for(int32 I=0;I<10;++I)
    {
        const FVector A=FMath::Lerp(PanelFoot(50),PanelTop(50),I/10.f)+FVector(0,0,Half+5),B=FMath::Lerp(PanelFoot(50),PanelTop(50),(I+1)/10.f)+FVector(0,0,Half+5);FHitResult Hit;
        if(W->SweepSingleByChannel(Hit,G+A,G+B,FQuat::Identity,ECC_Pawn,Capsule,Q)){bRamp=false;++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_PANEL_BLOCKED step=%d component=%s"),I,*GetNameSafe(Hit.GetComponent()));break;}
    }
    // The road past it stays open.
    const TArray<FVector>& Way=ChultFarRoad();
    for(int32 I=1;I<Way.Num();++I)
    {
        const float S=Frame(Way[I]).X;if(S<5000||S>10500)continue;
        const FVector A=OnGround(Way[I-1],Half+5),B=OnGround(Way[I],Half+5);FHitResult Hit;++Road;
        if(W->SweepSingleByChannel(Hit,G+A,G+B,FQuat::Identity,ECC_Pawn,Capsule,Q)&&!GetNameSafe(Hit.GetComponent()).Contains(TEXT("Banks"))&&!GetNameSafe(Hit.GetComponent()).Contains(TEXT("Ground"))){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_WRECK_ROAD_BLOCKED s=%.0f component=%s"),S,*GetNameSafe(Hit.GetComponent()));}
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_CHECK failures=%d rifts_open=%d/%d rims=%d floors=%d breach_clear=%d panel_clear=%d road_sweeps=%d"),Failures,Open,L.Rifts.Num(),Rims,Floors,bBreach,bRamp,Road);
    return Failures;
}

void ReviewChultWreck(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    const FVector G=AChultLanding::FloorOrigin();const FMast& M=Mast();const FLayout& L=Layout();
    const FVector Keel=Ship().GetLocation();
    const FVector Side(-M.H.Y,M.H.X,0),BowSide=Side*FMath::Sign(FVector::DotProduct(Side,YawFwd()));
    TArray<FVector> Eyes,Aims;TArray<FString> Names;
    auto View=[&](const TCHAR* Name,const FVector& Eye,const FVector& Aim){Names.Add(Name);Eyes.Add(Eye);Aims.Add(Aim);};
    const FRift& R0=L.Rifts[0];const FVector2D Off=R0.Perp*(FVector2D::DotProduct(R0.Perp,XY(Across))>0?1.f:-1.f);
    View(TEXT("WreckFromBridge"),OnGround(ChultCanyonFrame(4950,0),70),Keel+FVector(0,0,250));
    View(TEXT("WreckRoad"),OnGround(ChultCanyonFrame(7000,900),160),Keel+FVector(0,0,200));
    View(TEXT("WreckBreach"),BreachApproach()+FVector(0,0,90)-YawHigh()*150+YawFwd()*120,ToFloor(Hull(StationAt((BreachFrom+BreachTo)*.5f,250),-GirthT(.55f))));
    View(TEXT("WreckRupture"),OnGround(FVector(R0.C.X+Off.X*260,R0.C.Y+Off.Y*260,0),75),FVector(R0.C.X,R0.C.Y,R0.LipZ-20));
    View(TEXT("WreckDeck"),ToFloor(FVector(-1000,0,DeckAt(CabinFore)+230+200)),ToFloor(FVector(600,0,DeckAt(600))));
    View(TEXT("WreckHold"),ToFloor(FVector(-980,60,FloorZ+130)),ToFloor(FVector(300,-120,FloorZ+70)));
    View(TEXT("WreckOverview"),OnGround(ChultCanyonFrame(7000,1700),1100),Keel+FVector(0,0,150));
    View(TEXT("WreckHighSide"),OnGround(ChultCanyonFrame(7900,-3000),180),Keel+FVector(0,0,220));
    View(TEXT("WreckCabin"),ToFloor(FVector(-790,70,DeckAt(-790)+120)),ToFloor(FVector(-1060,-40,DeckAt(-1000)+70)));
    View(TEXT("WreckSail"),OnGround(M.Head+M.H*450+BowSide*350,170),OnGround(M.Head-M.H*450+BowSide*80,40));
    View(TEXT("WreckStern"),OnGround(ChultCanyonFrame(10600,-500),150),Keel+FVector(0,0,250));

    struct FLeg{FString Name;FVector Start;float Yaw=0;TArray<FVector> Way;bool bRift=false;};
    TArray<FLeg> Legs;const float Lift=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2;
    {
        FLeg Leg;Leg.Name=TEXT("road");Leg.Start=ChultBridgeSpawn(true);
        for(const FVector& P:ChultFarRoad()){const float S=Frame(P).X;if(S>5300&&S<9800&&(Leg.Way.IsEmpty()||FVector::Dist2D(P,Leg.Way.Last())>250))Leg.Way.Add(OnGround(P));}
        if(Leg.Way.Num())Leg.Yaw=(Leg.Way[0]-(Leg.Start-G)).Rotation().Yaw;
        Legs.Add(Leg);
    }
    {
        FLeg Leg;Leg.Name=TEXT("hold_loop");Leg.Start=G+BreachApproach()+FVector(0,0,Lift);
        Leg.Way={HoldPoint(-300,-120),HoldPoint(-850,-110),HoldPoint(-850,0),DeckPoint(-60,0),DeckPoint(50,-300),PanelTop(50),OnGround(PanelFoot(50)-YawHigh()*90)};
        Leg.Yaw=(Leg.Way[0]-BreachApproach()).Rotation().Yaw;Legs.Add(Leg);
    }
    {
        FLeg Leg;Leg.Name=TEXT("cabin");Leg.Start=G+DeckPoint(-500,0)+FVector(0,0,Lift+3);
        Leg.Way={DeckPoint(-700,0),DeckPoint(-920,0)};Leg.Yaw=(-YawFwd()).Rotation().Yaw;Legs.Add(Leg);
    }
    {
        FLeg Leg;Leg.Name=TEXT("rupture");Leg.bRift=true;
        const FVector From=OnGround(FVector(R0.C.X+Off.X*(R0.Wid+150),R0.C.Y+Off.Y*(R0.Wid+150),0));
        Leg.Start=G+From+FVector(0,0,Lift);Leg.Way={FVector(R0.C.X,R0.C.Y,R0.LipZ)};Leg.Yaw=(Leg.Way[0]-From).Rotation().Yaw;Legs.Add(Leg);
    }

    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    struct FRun{int32 Phase=0,Leg=0,Way=0,Failures=0,Deaths=0;double Next=0,Start=-1,DiedAt=-1;bool bDone=false;TArray<int32> Passed;TWeakObjectPtr<ACameraActor> Camera;FTimerHandle Timer;};
    auto Run=MakeShared<FRun>();Run->Failures=Failures;Run->Camera=Camera;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names,Legs,G,Lift]()
    {
        if(Run->bDone)return;
        const double Now=Owner->GetWorld()->GetTimeSeconds();const int32 I=Run->Phase/2;
        if(I<Eyes.Num())
        {
            if(Now<Run->Next)return;Run->Next=Now+2.5;
            if(Run->Phase%2==0){if(Run->Camera.IsValid())Run->Camera->SetActorLocationAndRotation(G+Eyes[I],(Aims[I]-Eyes[I]).Rotation());}
            else{FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_VIEW name=%s"),*Names[I]);}
            ++Run->Phase;return;
        }
        auto* Mv=Chuck->GetCharacterMovement();
        if(Run->Leg>=Legs.Num())
        {
            Run->bDone=true;Chuck->SetTestStick(FVector2D::ZeroVector);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_TEST failures=%d legs=%d road=%d hold_loop=%d cabin=%d rupture=%d"),Run->Failures,Legs.Num(),
                Run->Passed.IsValidIndex(0)&&Run->Passed[0],Run->Passed.IsValidIndex(1)&&Run->Passed[1],Run->Passed.IsValidIndex(2)&&Run->Passed[2],Run->Passed.IsValidIndex(3)&&Run->Passed[3]);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);return;
        }
        const FLeg& Leg=Legs[Run->Leg];
        if(Run->Start<0)
        {
            if(Now<Run->Next)return;
            if(Run->Camera.IsValid())Run->Camera->Destroy();
            Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);
            Chuck->ResetAtLocation(Leg.Start);Chuck->SetActorRotation(FRotator(0,Leg.Yaw,0));Chuck->SetOrbitPitch(-12);Chuck->SetRunHeld(Leg.Name==TEXT("road"));
            Run->Start=Now;Run->Way=0;Run->Deaths=Chuck->GetFallDeaths();Run->DiedAt=-1;return;
        }
        const float Time=Now-Run->Start;const FVector P=Chuck->GetActorLocation()-G;
        auto Steer=[&](const FVector& To){const FVector D=(To-P).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);};
        bool bEnd=false,bPass=false;
        if(Leg.bRift)
        {
            if(Run->DiedAt<0)
            {
                if(Chuck->GetFallDeaths()>Run->Deaths){Run->DiedAt=Now;Chuck->SetTestStick(FVector2D::ZeroVector);UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_RIFT_DEATH seconds=%.2f at=%s"),Time,*P.ToString());return;}
                if(Time<15){Steer(Leg.Way[0]);return;}
                bEnd=true;
            }
            else
            {
                if(IsChultBridgeRespawn(Chuck->GetActorLocation())&&Mv->IsMovingOnGround()&&!Chuck->IsAstral()){bEnd=true;bPass=true;}
                else if(Now-Run->DiedAt>15)bEnd=true;
                else return;
            }
        }
        else if(Run->Way<Leg.Way.Num())
        {
            const FVector To=Leg.Way[Run->Way];
            if(FVector::Dist2D(P,To)<40)
            {
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_WALK leg=%s way=%d seconds=%.1f z_over=%.1f ground=%d"),*Leg.Name,Run->Way,Time,P.Z-Lift+2-To.Z,Mv->IsMovingOnGround());
                if(Leg.Name==TEXT("hold_loop")&&(Run->Way==1||Run->Way==4))
                    FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Run->Way==1?TEXT("WreckWalkHold.png"):TEXT("WreckWalkDeck.png")),true,false);
                ++Run->Way;return;
            }
            if(Time<60&&Chuck->GetFallDeaths()==Run->Deaths){Chuck->SetRunHeld(Leg.Name==TEXT("road"));Steer(To);return;}
            bEnd=true;
        }
        else
        {
            bEnd=true;
            bPass=Mv->IsMovingOnGround()&&Chuck->GetFallDeaths()==Run->Deaths&&FMath::Abs(P.Z-Lift+2-Leg.Way.Last().Z)<30;
        }
        if(!bEnd)return;
        Chuck->SetTestStick(FVector2D::ZeroVector);Mv->StopMovementImmediately();
        Run->Failures+=!bPass;Run->Passed.Add(bPass);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WRECK_LEG name=%s pass=%d ways=%d/%d seconds=%.1f deaths=%d at=%s"),*Leg.Name,bPass,Run->Way,Leg.Way.Num(),Time,Chuck->GetFallDeaths()-Run->Deaths,*P.ToString());
        ++Run->Leg;Run->Start=-1;Run->Next=Now+.6;
    },.016f,true);
}
