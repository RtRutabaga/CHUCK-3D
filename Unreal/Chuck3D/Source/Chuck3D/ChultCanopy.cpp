#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    // Trunk axes and crotch heights. Each tree's walking limbs point straight at
    // its neighbours; tree 0 has a third limb under the ruin's landing slab.
    struct FTreeDef { FVector2D At; float Top; };
    const FTreeDef Defs[]={{{5560,1230},650},{{7250,760},650},{{8350,1800},760},{{6750,2600},650}};
    const FVector2D SlabAt(5530,820);
    constexpr float GripOver=98;      // grip above the walking limb: a standing hop (18 cm) catches it; hanging feet clear the bark by 34
    constexpr float FirstGrip=300;    // first/last grip of a chain, out from the trunk axis
    constexpr float LimbLength=430;
    constexpr float SlabLimbLength=470;
    constexpr float GripSpacing=135;
    constexpr float HalfGap=90;       // 180 cm between the two boughs' end grips; no wood spans it
    const FVector GroundLane[]={{4200,500,0},{4900,500,0},{5300,900,0},{5600,900,0},{6200,900,0},{6500,1600,0},{7400,2200,0},{7800,3200,0},{6800,3300,0},{6000,2900,0},{5100,1800,0},{4600,1350,0}};
    bool RootClear(FVector At,float Margin=115)
    {
        At.Z=0;
        for(int32 I=1;I<UE_ARRAY_COUNT(GroundLane);++I)
            if(FVector::Dist2D(At,FMath::ClosestPointOnSegment(At,GroundLane[I-1],GroundLane[I]))<Margin) return false;
        return true;
    }
    FVector Flat(FVector2D P,float Z=0) {return FVector(P.X,P.Y,Z);}
    FVector Across(const FVector& D) {return FVector(-D.Y,D.X,0);}
    int32 BoughSide(int32 Tree,int32 Limb) {return ((Tree+Limb)%2)?1:-1;}

    float LimbLen(int32 Tree,int32 Limb) {return Tree==0 && Limb==2?SlabLimbLength:LimbLength;}
    float LimbRadius(float S,float Len)
    {
        if(S<70) return 62;
        if(S<300) return FMath::Lerp(62.f,34.f,(S-70)/230);
        const float TipStart=Len-45;
        if(S<TipStart) return FMath::Lerp(34.f,25.f,(S-300)/FMath::Max(1.f,TipStart-300));
        const float U=FMath::Clamp((S-TipStart)/45,0.f,1.f);
        return FMath::Max(2.f,25*FMath::Sqrt(1-U*U));
    }

    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        void Vertex(FVector P,FVector Normal,FVector2D Tex)
        { V.Add(P); N.Add(Normal); UV.Add(Tex); C.Add(FColor(190,180,0,0)); }
        void Tri(int32 A,int32 B,int32 D) { T.Append({A,B,D}); }
    };
    // A continuous swept surface with parallel-transported frames (no twisting
    // where a branch turns upright), an elliptical section (RS across, RU up)
    // and an optional radial modulation for fluting and lumps.
    void Sweep(FSurface& S,const TArray<FVector>& Path,const TArray<float>& RS,const TArray<float>& RU,int32 Sides,
        TFunctionRef<float(int32,float)> Mult,bool bCap=true,const FVector* CapAt=nullptr,bool bStartCap=false)
    {
        const int32 Base=S.V.Num(),Count=Path.Num();
        ON_SCOPE_EXIT
        {
            if(!bStartCap) return;
            const int32 Cap=S.V.Num();
            S.Vertex(Path[0],(Path[0]-Path[1]).GetSafeNormal(),FVector2D(.5f,0));
            for(int32 K=0;K<Sides;++K) S.Tri(Cap,Base+K+1,Base+K);
        };
        FVector Dir=(Path[1]-Path[0]).GetSafeNormal();
        FVector Side=FVector::CrossProduct(Dir,FMath::Abs(Dir.Z)>.95f?FVector::ForwardVector:FVector::UpVector).GetSafeNormal();
        const float Tiles=FMath::Max(1.f,FMath::RoundToFloat(PI*(RS[0]+RU[0])/150));
        float Length=0;
        for(int32 J=0;J<Count;++J)
        {
            const FVector D=(Path[FMath::Min(J+1,Count-1)]-Path[FMath::Max(J-1,0)]).GetSafeNormal();
            if(J) {Side=FQuat::FindBetweenNormals(Dir,D).RotateVector(Side);Length+=FVector::Dist(Path[J-1],Path[J]);}
            Dir=D;Side=(Side-D*FVector::DotProduct(Side,D)).GetSafeNormal();
            const FVector Up=FVector::CrossProduct(Side,D);
            for(int32 K=0;K<=Sides;++K)
            {
                const float A=2*PI*K/Sides,Co=FMath::Cos(A),Si=FMath::Sin(A),M=Mult(J,A);
                const FVector Normal=(Side*Co/FMath::Max(.5f,RS[J])+Up*Si/FMath::Max(.5f,RU[J])).GetSafeNormal();
                S.Vertex(Path[J]+(Side*Co*RS[J]+Up*Si*RU[J])*M,Normal,FVector2D(K/static_cast<float>(Sides)*Tiles,Length/200));
                if(J && K<Sides)
                { const int32 I=Base+J*(Sides+1)+K; S.Tri(I-Sides-1,I-Sides,I);S.Tri(I-Sides,I+1,I); }
            }
        }
        if(!bCap) return;
        const int32 Cap=S.V.Num(),Last=Base+(Count-1)*(Sides+1);
        S.Vertex(CapAt?*CapAt:Path.Last(),CapAt?FVector::UpVector:Dir,FVector2D(.5f,Length/200));
        for(int32 K=0;K<Sides;++K) S.Tri(Cap,Last+K,Last+K+1);
    }
    void Round(FSurface& S,const TArray<FVector>& Path,const TArray<float>& Radii,int32 Sides,int32 Seed=0,float Lumps=.04f)
    {
        Sweep(S,Path,Radii,Radii,Sides,[Seed,Lumps](int32 J,float A){return 1+Lumps*FMath::Sin(3*A+J*.7f+Seed)+Lumps*.5f*FMath::Sin(5*A-J*.4f+Seed*2);});
    }
    FVector Bezier(FVector A,FVector B,FVector C,FVector D,float U)
    {
        const float V=1-U; return A*V*V*V+B*3*V*V*U+C*3*V*U*U+D*U*U*U;
    }
    FVector CatmullRom(FVector P0,FVector P1,FVector P2,FVector P3,float U)
    {
        const float U2=U*U,U3=U2*U;
        return ((P1*2)+(-P0+P2)*U+(P0*2-P1*5+P2*4-P3)*U2+(-P0+P1*3-P2*3+P3)*U3)*.5f;
    }
    // A tapering branch along a cubic, radii from R0 to R1.
    void Branch(FSurface& S,FVector A,FVector B,FVector C,FVector D,float R0,float R1,int32 Steps,int32 Sides,int32 Seed)
    {
        TArray<FVector> P;TArray<float> R;
        for(int32 J=0;J<=Steps;++J){const float U=J/static_cast<float>(Steps);P.Add(Bezier(A,B,C,D,U));R.Add(FMath::Lerp(R0,R1,FMath::Pow(U,.8f)));}
        Round(S,P,R,Sides,Seed,.03f);
    }

    struct FCanopy
    {
        TArray<FChultCanopyTree> Trees;
        TArray<FChultCanopyChain> Chains;
        TArray<TArray<float>> LimbLeaders;   // per tree: leader yaw (radians)
    };
    FVector LimbTopAt(const FChultCanopyTree& T,int32 Tree,int32 Limb,float S)
    {
        const FVector D=T.Limbs[Limb];
        const float Len=LimbLen(Tree,Limb);
        const bool Slab=Tree==0 && Limb==2;
        // Level to the first grip, then a slight droop and a gentle sideways wander toward the tip.
        const float Wave=Slab?0:3*FMath::Sin(S/60+Tree)*FMath::SmoothStep(100.f,250.f,S);
        const float Droop=FMath::Square(FMath::Max(0.f,S-310))/1100;
        const float Wander=Slab?0:10*FMath::Sin(S/45+Tree*2+Limb)*FMath::SmoothStep(330.f,Len,S);
        return T.Trunk+D*S+Across(D)*Wander+FVector(0,0,(Slab?T.Top-5:T.Top)+Wave-Droop);
    }
    const FCanopy& Canopy()
    {
        static const FCanopy Data=[]()
        {
            FCanopy R;
            for(int32 I=0;I<4;++I)
            {
                FChultCanopyTree T;T.Trunk=Flat(Defs[I].At);T.Top=Defs[I].Top;
                T.Limbs.Add(Flat(Defs[(I+1)%4].At-Defs[I].At).GetSafeNormal2D());
                T.Limbs.Add(Flat(Defs[(I+3)%4].At-Defs[I].At).GetSafeNormal2D());
                if(I==0) T.Limbs.Add(Flat(SlabAt-Defs[I].At).GetSafeNormal2D());
                FVector Bisect=(T.Limbs[0]+T.Limbs[1]).GetSafeNormal2D();
                if(Bisect.IsNearlyZero()) Bisect=Across(T.Limbs[0]);
                T.Fork=T.Trunk+Bisect*45+FVector(0,0,T.Top);
                R.Trees.Add(T);
            }
            // Leaders rise from the back of the crotch, in the widest gap between limbs.
            for(int32 I=0;I<4;++I)
            {
                TArray<float> Yaws;
                for(const FVector& L:R.Trees[I].Limbs) Yaws.Add(FMath::Atan2(L.Y,L.X));
                Yaws.Sort();
                float Best=-1,Mid=0;
                for(int32 K=0;K<Yaws.Num();++K)
                {
                    const float A=Yaws[K],B=K+1<Yaws.Num()?Yaws[K+1]:Yaws[0]+2*PI;
                    if(B-A>Best){Best=B-A;Mid=(A+B)*.5f;}
                }
                const float Spread=FMath::Min(FMath::DegreesToRadians(38.f),Best*.22f);
                R.LimbLeaders.Add({Mid-Spread,Mid+Spread});
            }
            // Lianas: one per tree, off the outgoing limb, on the side away from its hand bough.
            for(int32 I=0;I<4;++I)
            {
                FChultCanopyTree& T=R.Trees[I];
                const FVector D=T.Limbs[0],Out=Across(D)*-BoughSide(I,0);
                const float S=210,Rad=LimbRadius(S,LimbLength);
                const FVector Axis=LimbTopAt(T,I,0,S)-FVector(0,0,Rad);
                T.LianaOut=Out;
                T.LianaFoot=FVector(Axis.X,Axis.Y,0)+Out*(Rad+21);
                T.LianaLip=Axis+Out*(Rad*.55f)+FVector(0,0,Rad*.83f);
            }
            for(int32 E=0;E<4;++E)
            {
                const int32 A=E,B=(E+1)%4;
                FChultCanopyChain Chain;Chain.Name=FName(*FString::Printf(TEXT("ChultCanopyChain%d"),E));Chain.From=A;Chain.To=B;
                const FVector GA=LimbTopAt(R.Trees[A],A,0,FirstGrip)+FVector(0,0,GripOver);
                const FVector GB=LimbTopAt(R.Trees[B],B,1,FirstGrip)+FVector(0,0,GripOver);
                const FVector Side=Across((GB-GA).GetSafeNormal2D())*(E%2?1.f:-1.f);
                const float Span=FVector::Dist2D(GA,GB),Gap=HalfGap/Span;
                // A gentle arch and sideways bow, with the small irregular wander of a real
                // branch (none at the end grips, which sit exactly over the walking limbs).
                auto At=[&](float U)
                {
                    const float Bump=FMath::Sin(PI*U),Fine=FMath::Sin(PI*FMath::Min(1.f,4*FMath::Min(U,1-U)));
                    return FMath::Lerp(GA,GB,U)+FVector(0,0,28*Bump+5*Fine*FMath::Sin(U*23+E))+Side*(24*Bump+9*Fine*FMath::Sin(U*17+E*3));
                };
                for(int32 Half=0;Half<2;++Half)
                {
                    if(Half) Chain.BranchBreak=Chain.Paws.Num();
                    const float Start=Half?.5f+Gap:0,End=Half?1:.5f-Gap;
                    const int32 Steps=FMath::CeilToInt(Span*(End-Start)/GripSpacing);
                    for(int32 K=0;K<=Steps;++K) Chain.Paws.Add(At(FMath::Lerp(Start,End,K/static_cast<float>(Steps))));
                }
                R.Chains.Add(MoveTemp(Chain));
            }
            return R;
        }();
        return Data;
    }
    // Leader K of tree I: a cubic from inside the crotch up into the crown.
    void LeaderCurve(const FCanopy& Data,int32 I,int32 K,FVector Out4[4])
    {
        const auto& T=Data.Trees[I];const float Yaw=Data.LimbLeaders[I][K];
        const FVector Out(FMath::Cos(Yaw),FMath::Sin(Yaw),0),Side=Across(Out);
        Out4[0]=T.Trunk+Out*34+FVector(0,0,T.Top-120);Out4[1]=T.Trunk+Out*60+FVector(0,0,T.Top+250);
        Out4[2]=T.Trunk+Out*170+Side*(K?30:-30)+FVector(0,0,T.Top+620);Out4[3]=T.Trunk+Out*300+Side*(K?70:-70)+FVector(0,0,T.Top+900);
    }
    int32 GripIndex(const FVector& World)
    {
        const auto& Grips=GetChuckSwingGrips();
        for(int32 I=0;I<Grips.Num();++I) if(FVector::Dist(Grips[I].Grip,World)<1) return I;
        return -1;
    }
}

bool InChultCanopyReserve(const FVector& P,float Margin)
{ return P.X>5000-Margin && P.X<8800+Margin && P.Y>250-Margin && P.Y<3000+Margin; }

const TArray<FChultCanopyChain>& ChultCanopyChains() {return Canopy().Chains;}
const TArray<FChultCanopyTree>& ChultCanopyTrees() {return Canopy().Trees;}
FVector ChultCanopyLimbTop(int32 Tree,int32 Limb,float S) {return LimbTopAt(Canopy().Trees[Tree],Tree,Limb,S);}

void BuildChultCanopy(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();
    const FCanopy& Data=Canopy();
    auto* Bark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/M_FallBark.M_FallBark"));
    auto* Leaf=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallLeafCard.SM_FallLeafCard"));
    auto Finish=[&](const TCHAR* Name,FSurface& S,bool Solid)
    {
        auto* P=NewObject<UProceduralMeshComponent>(Owner,FName(Name));
        P->SetupAttachment(Owner->GetRootComponent());P->SetWorldLocation(G);P->RegisterComponent();P->bUseComplexAsSimpleCollision=true;
        TArray<FVector> Normals;TArray<FProcMeshTangent> Tangents;
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(S.V,S.T,S.UV,Normals,Tangents);
        P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,Tangents,Solid);
        P->SetMaterial(0,Bark);P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid)P->SetCollisionProfileName(TEXT("BlockAll"));P->ComponentTags.Add(TEXT("ChultCanopy"));
    };
    // Leaves stay out of the swing lanes, off the walking limbs and clear of the crotches.
    auto LeafClear=[&](const FVector& P)
    {
        for(const auto& Chain:Data.Chains)
            for(int32 I=1;I<Chain.Paws.Num();++I)
            {
                const FVector Q=FMath::ClosestPointOnSegment(P,Chain.Paws[I-1],Chain.Paws[I]);
                if(FVector::Dist2D(P,Q)<75 && P.Z>Q.Z-130 && P.Z<Q.Z+45) return false;
            }
        for(int32 I=0;I<4;++I)
        {
            const auto& T=Data.Trees[I];
            if(FVector::Dist2D(P,T.Fork)<170 && P.Z<T.Top+230) return false;
            for(int32 L=0;L<T.Limbs.Num();++L)
            {
                const FVector Q=FMath::ClosestPointOnSegment(P,T.Trunk+FVector(0,0,T.Top),LimbTopAt(T,I,L,LimbLen(I,L)));
                if(FVector::Dist2D(P,Q)<70 && P.Z<T.Top+160 && P.Z>T.Top-80) return false;
            }
        }
        return true;
    };
    FSurface Solid,Branches,Vines;int32 Fins=0,Roots=0,LeafCount=0,Rejected=0;
    for(int32 I=0;I<4;++I)
    {
        const FChultCanopyTree& T=Data.Trees[I];
        const float Seed=I*1.7f;
        auto* Crown=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyCrown%d"),I)));
        Crown->SetupAttachment(Owner->GetRootComponent());Crown->SetWorldLocation(G);Crown->SetStaticMesh(Leaf);
        Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);Crown->ComponentTags.Add(TEXT("ChultCanopyFoliage"));Crown->RegisterComponent();
        FRandomStream Rng(911+I*131);
        auto Cluster=[&](FVector At,float Radius,int32 Count,float Squash=.6f)
        {
            for(int32 K=0;K<Count;++K)
            {
                const FVector Offset(Rng.FRandRange(-1,1),Rng.FRandRange(-1,1),Rng.FRandRange(-.7f,1));
                if(Offset.SizeSquared()>1) continue;
                const FVector P=At+FVector(Offset.X*Radius,Offset.Y*Radius,Offset.Z*Radius*Squash);
                if(!LeafClear(P)) {++Rejected;continue;}
                const float Scale=Rng.FRandRange(1.4f,2.5f);
                Crown->AddInstance(FTransform(FRotator(Rng.FRandRange(-60,60),Rng.FRandRange(0,360),Rng.FRandRange(-30,30)),P,FVector(Scale)),false);++LeafCount;
            }
        };
        // Trunk: a broad flared, fluted base tapering to the crotch, with a slight
        // lean that straightens by the top; the top is the crotch's standing bowl.
        {
            TArray<FVector> P;TArray<float> R;
            constexpr int32 Rings=30;
            for(int32 J=0;J<=Rings;++J)
            {
                const float Z=FMath::Lerp(-40.f,T.Top-6,J/static_cast<float>(Rings)),Fade=1-FMath::Clamp(Z/T.Top,0.f,1.f);
                P.Add(T.Trunk+FVector(14*FMath::Sin(Z/260+Seed)*Fade,11*FMath::Cos(Z/310+Seed*2)*Fade,Z));
                R.Add(84+16*Fade+75*FMath::Exp(-FMath::Max(Z,0.f)/55));
            }
            const FVector Cap=T.Trunk+FVector(0,0,T.Top+3);
            Sweep(Solid,P,R,R,40,[&](int32 J,float A)
            {
                const float Z=P[J].Z-T.Trunk.Z,Flute=.11f*FMath::Exp(-FMath::Max(Z,0.f)/140)+.022f;
                return 1+Flute*FMath::Sin(7*A+Seed)+.018f*FMath::Sin(3*A+Z/90+Seed)+.012f*FMath::Sin(11*A-Z/40);
            },true,&Cap);
        }
        // Walking limbs: thick at the crotch, level out past the first grip, then
        // a slight droop. Their tops are the bark he walks on.
        for(int32 L=0;L<T.Limbs.Num();++L)
        {
            const float Len=LimbLen(I,L);
            const bool Slab=I==0 && L==2;
            TArray<FVector> P;TArray<float> R;
            for(float S=0;S<=Len+.1f;S+=Len/36)
            {
                // The slab's limb ends under the stone; the others carry on as a thinner branch.
                const float Rad=Slab?LimbRadius(S,Len):LimbRadius(FMath::Min(S,Len-46),Len);
                P.Add(LimbTopAt(T,I,L,S)-FVector(0,0,Rad));R.Add(Rad);
            }
            if(!Slab)
            {
                // Out past the walking part it bends away from under the grips, dips and
                // rises to a leafy end, rather than stopping in a blunt stub.
                const FVector D=T.Limbs[L],Away=Across(D)*-BoughSide(I,L),E=P.Last(),Dir=(P.Last()-P[P.Num()-2]).GetSafeNormal();
                const float Curl=FMath::Lerp(25.f,70.f,FMath::Frac((I*3+L)*.37f)),Spread=FMath::Lerp(140.f,200.f,FMath::Frac((I*5+L*3)*.29f));
                const FVector C1=E+Dir*60+Away*25,C2=E+D*120+Away*Spread*.5f+FVector(0,0,Curl*.3f),End=E+D*160+Away*Spread+FVector(0,0,Curl);
                const float R0=R.Last();
                for(int32 J=1;J<=12;++J){const float U=J/12.f;P.Add(Bezier(E,C1,C2,End,U));R.Add(FMath::Lerp(R0,2.5f,FMath::Pow(U,.6f)));}
                Cluster(End+FVector(0,0,30),130,90,.5f);
                Cluster(Bezier(E,C1,C2,End,.7f)+Away*30+FVector(0,0,70),70,30,.6f);
            }
            Sweep(Solid,P,R,R,28,[&](int32 J,float A){return 1+.035f*FMath::Sin(4*A+J*.5f+Seed)+.02f*FMath::Sin(7*A-J*.3f);},true,nullptr,true);
        }
        // The crotch's saddle: a broad, low swelling where the limbs leave the trunk,
        // so the bark between them is one surface rather than a notch.
        {
            const FVector Front=(T.Fork-FVector(T.Trunk.X,T.Trunk.Y,T.Fork.Z)).GetSafeNormal2D();
            TArray<FVector> P;TArray<float> RS,RU;
            for(int32 J=0;J<=10;++J)
            {
                // Its outer end rounds off under the bark rather than ending in a flat face.
                const float U=J/10.f,Round=J<8?1.f:FMath::Sqrt(FMath::Max(0.f,1-FMath::Square((U-.7f)/.3f)));
                P.Add(T.Trunk+Front*FMath::Lerp(0.f,125.f,U)+FVector(0,0,T.Top-34-14*U*U));
                RS.Add(FMath::Max(3.f,FMath::Lerp(75.f,40.f,U*U)*Round));RU.Add(FMath::Max(2.f,FMath::Lerp(36.f,18.f,U*U)*Round));
            }
            Sweep(Solid,P,RS,RU,24,[&](int32 J,float A){return 1+.03f*FMath::Sin(3*A+J+Seed);},true,nullptr,true);
        }
        // Leaders: two from the back of the crotch, rising and spreading into the crown.
        for(int32 K=0;K<2;++K)
        {
            const float Yaw=Data.LimbLeaders[I][K];
            FVector Curve[4];LeaderCurve(Data,I,K,Curve);
            const FVector A=Curve[0],B=Curve[1],C=Curve[2],D=Curve[3];
            TArray<FVector> P;TArray<float> R;
            for(int32 J=0;J<=22;++J){const float U=J/22.f;P.Add(Bezier(A,B,C,D,U));R.Add(FMath::Lerp(60.f,9.f,FMath::Pow(U,.9f)));}
            Round(Solid,P,R,24,I*2+K);
            Cluster(D+FVector(0,0,50),230,320,.45f);
            // Spreading boughs off each leader, leaves gathered at their ends.
            for(int32 Bough=0;Bough<5;++Bough)
            {
                const float U=.36f+Bough*.13f;
                const FVector From=Bezier(A,B,C,D,U);
                const float Turn=(Bough%2?1:-1)*FMath::DegreesToRadians(Rng.FRandRange(45,85))+Yaw;
                const FVector Dir(FMath::Cos(Turn),FMath::Sin(Turn),0);
                const float Reach=Rng.FRandRange(220,360),Rise=Rng.FRandRange(50,150);
                const FVector End=From+Dir*Reach+FVector(0,0,Rise);
                Branch(Branches,From,From+Dir*Reach*.35f+FVector(0,0,Rise*.75f),From+Dir*Reach*.7f+FVector(0,0,Rise*1.05f),End,FMath::Lerp(17.f,10.f,U),2.5f,10,10,Bough+K*4);
                Cluster(End,150,150,.5f);
                Cluster(From+Dir*Reach*.6f+FVector(0,0,Rise*.9f+25),90,55,.6f);
            }
        }
        // Plank buttresses: tall thin fins that run out and sink into the ground,
        // with a concave top edge; none crosses a ground lane or the liana's foot.
        const float LianaYaw=FMath::Atan2(T.LianaFoot.Y-T.Trunk.Y,T.LianaFoot.X-T.Trunk.X);
        for(int32 F=0;F<7;++F)
        {
            const float Yaw=F*2*PI/7+Seed+Rng.FRandRange(-.18f,.18f);
            if(FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,LianaYaw))<FMath::DegreesToRadians(32.f)) continue;
            const FVector Out(FMath::Cos(Yaw),FMath::Sin(Yaw),0),Side=Across(Out);
            const float Reach=Rng.FRandRange(270,390),Height=Rng.FRandRange(115,150),Wobble=Rng.FRandRange(-22,22);
            bool Clear=true;
            for(int32 K=2;K<=10;++K) Clear&=RootClear(T.Trunk+Out*(Reach*K/10.f)+Side*Wobble*FMath::Sin(PI*K/10.f));
            if(!Clear) continue;
            TArray<FVector> P;TArray<float> RS,RU;
            for(int32 K=0;K<=16;++K)
            {
                const float U=K/16.f,H=Height*FMath::Pow(1-U,1.8f)+4;
                P.Add(T.Trunk+Out*(45+(Reach-45)*U)+Side*Wobble*FMath::Sin(PI*U)+FVector(0,0,H-12));
                RS.Add(FMath::Lerp(18.f,5.f,U));RU.Add(H);
            }
            Sweep(Solid,P,RS,RU,16,[&](int32 J,float A){return 1+.05f*FMath::Sin(2*A+J*.6f+F);});
            ++Fins;
        }
        // Low surface roots snake between the fins.
        for(int32 K=0;K<4;++K)
        {
            const float Yaw=(K+.5f)*2*PI/4+Seed*1.3f;
            if(FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,LianaYaw))<FMath::DegreesToRadians(30.f)) continue;
            const FVector Out(FMath::Cos(Yaw),FMath::Sin(Yaw),0),Side=Across(Out);
            const float Reach=Rng.FRandRange(230,340);
            bool Clear=true;
            for(int32 J=2;J<=10;++J) Clear&=RootClear(T.Trunk+Out*(Reach*J/10.f));
            if(!Clear) continue;
            TArray<FVector> P;TArray<float> RS,RU;
            for(int32 J=0;J<=14;++J)
            {
                const float U=J/14.f;
                P.Add(T.Trunk+Out*(60+(Reach-60)*U)+Side*28*FMath::Sin(U*5+K)+FVector(0,0,10*(1-U)-4));
                RS.Add(16*(1-U)+2);RU.Add(9*(1-U)+1.5f);
            }
            Sweep(Solid,P,RS,RU,12,[](int32,float){return 1.f;});
            ++Roots;
        }
        // The liana: over the limb's side and down to a loose coil on the ground.
        {
            const FVector Rope=T.LianaFoot-T.LianaOut*18;
            const float LimbR=LimbRadius(210,LimbLength);
            const FVector Axis=ChultCanopyLimbTop(I,0,210)-FVector(0,0,LimbR);
            TArray<FVector> P;TArray<float> R;
            // Over the top of the limb and round its flank, then hanging free to the ground.
            for(int32 J=0;J<=7;++J)
            {
                const float A=FMath::DegreesToRadians(FMath::Lerp(115.f,-5.f,J/7.f));
                P.Add(Axis+(T.LianaOut*FMath::Cos(A)+FVector(0,0,FMath::Sin(A)))*(LimbR*1.035f+3.8f));R.Add(3.6f);
            }
            const FVector Hang=P.Last();
            for(int32 J=1;J<=18;++J)
            {
                const float U=J/18.f,Ease=FMath::SmoothStep(0.f,.25f,U);
                const FVector Line=FVector(FMath::Lerp(Hang.X,Rope.X,Ease),FMath::Lerp(Hang.Y,Rope.Y,Ease),FMath::Lerp(Hang.Z,6.f,U));
                P.Add(Line+(Across(T.LianaOut)*5*FMath::Sin(U*9+I)+T.LianaOut*3*FMath::Sin(U*6))*Ease);
                R.Add(3.6f+.5f*FMath::Sin(U*23));
            }
            P.Add(Rope+FVector(0,0,-2)+Across(T.LianaOut)*9+T.LianaOut*4);R.Add(3.2f);
            P.Add(Rope+FVector(0,0,-9)+Across(T.LianaOut)*16+T.LianaOut*6);R.Add(2.6f);
            Round(Vines,P,R,8,I,.08f);
            FChuckClimbable Liana;
            Liana.Foot=G+T.LianaFoot;Liana.TopZ=G.Z+T.LianaLip.Z;Liana.Out=T.LianaOut;Liana.HalfWidth=18;Liana.Lip=G+T.LianaLip;
            AddChuckClimbable(Liana);
            // A few hanging strands from the crown read as jungle without being climbable.
            for(int32 K=0;K<3;++K)
            {
                const float Yaw=Data.LimbLeaders[I][K%2]+Rng.FRandRange(-.6f,.6f);
                const FVector From=T.Trunk+FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),0)*Rng.FRandRange(180,300)+FVector(0,0,T.Top+Rng.FRandRange(420,560));
                if(!LeafClear(From-FVector(0,0,300))) continue;
                const float Drop=Rng.FRandRange(320,520);
                TArray<FVector> S;TArray<float> SR;
                for(int32 J=0;J<=10;++J){const float U=J/10.f;S.Add(From+FVector(8*FMath::Sin(U*5+K),6*FMath::Cos(U*4+K),-Drop*U));SR.Add(FMath::Lerp(2.6f,1.2f,U));}
                Round(Vines,S,SR,6,K,.05f);
            }
        }
        auto* Fork=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyFork%d"),I)));
        Fork->SetupAttachment(Owner->GetRootComponent());Fork->SetWorldLocation(G+T.Fork);Fork->ComponentTags.Add(TEXT("ChultCanopyStanding"));Fork->RegisterComponent();
    }
    // Hand boughs: from the side of each walking limb, up over it and out along
    // the chain, thinning toward the gap; the grips hang under them.
    int32 Failures=0,Registered=0;float MaxStep=0,MaxRise=0,MinStep=1e6f;
    for(int32 E=0;E<Data.Chains.Num();++E)
    {
        const auto& Chain=Data.Chains[E];const auto& Paws=Chain.Paws;
        for(int32 Half=0;Half<2;++Half)
        {
            const int32 Tree=Half?Chain.To:Chain.From,Limb=Half?1:0;
            const auto& T=Data.Trees[Tree];
            const FVector D=T.Limbs[Limb],Side=Across(D)*BoughSide(Tree,Limb);
            TArray<FVector> Grips;
            if(Half) for(int32 K=Paws.Num()-1;K>=Chain.BranchBreak;--K) Grips.Add(Paws[K]);
            else for(int32 K=0;K<Chain.BranchBreak;++K) Grips.Add(Paws[K]);
            TArray<float> GR;TArray<FVector> Centres;
            for(int32 K=0;K<Grips.Num();++K)
            {
                GR.Add(FMath::Lerp(12.f,6.f,K/static_cast<float>(FMath::Max(1,Grips.Num()-1))));
                Centres.Add(Grips[K]+FVector(0,0,GR[K]-2.5f));
            }
            TArray<FVector> P;TArray<float> R;
            // It grows from the nearer leader above the crotch, arcs out over the
            // walking limb well above his head and settles to the first grip.
            int32 Near=0;float NearDot=-2;
            for(int32 K=0;K<2;++K)
            {
                const float Yaw=Data.LimbLeaders[Tree][K],Dot=FVector::DotProduct(FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),0),D);
                if(Dot>NearDot){NearDot=Dot;Near=K;}
            }
            FVector Lead[4];LeaderCurve(Data,Tree,Near,Lead);
            const FVector Root=Bezier(Lead[0],Lead[1],Lead[2],Lead[3],.24f)-D*20;
            const FVector C1=T.Trunk+D*70+Side*25+FVector(0,0,T.Top+205),C2=T.Trunk+D*205+Side*12+FVector(0,0,Centres[0].Z+30);
            for(int32 J=0;J<14;++J)
            {
                const float U=J/14.f;
                P.Add(Bezier(Root,C1,C2,Centres[0],U));
                R.Add(FMath::Lerp(19.f,GR[0],FMath::SmoothStep(0.f,1.f,U)));
            }
            const FVector Before=Centres[0]-(Centres.Num()>1?Centres[1]-Centres[0]:D*GripSpacing);
            for(int32 K=0;K+1<Centres.Num();++K)
            {
                const FVector P0=K?Centres[K-1]:Before,P3=K+2<Centres.Num()?Centres[K+2]:Centres[K+1]*2-Centres[K];
                for(int32 J=0;J<5;++J){P.Add(CatmullRom(P0,Centres[K],Centres[K+1],P3,J/5.f));R.Add(FMath::Lerp(GR[K],GR[K+1],J/5.f));}
            }
            const FVector Tangent=(Centres.Last()-(Centres.Num()>1?Centres[Centres.Num()-2]:Root)).GetSafeNormal();
            P.Add(Centres.Last());R.Add(GR.Last());
            for(int32 J=1;J<=4;++J){const float U=J/4.f;P.Add(Centres.Last()+Tangent*36*U+FVector(0,0,14*U*U)+Across(D)*6*U*U);R.Add(FMath::Lerp(GR.Last(),1.f,U));}
            Round(Branches,P,R,14,E*2+Half,.03f);
            // Leafy twigs rise off the bough, above and beside the hands' line.
            FRandomStream Rng(400+E*17+Half);
            for(int32 K=1;K<Centres.Num();++K)
            {
                const float Sgn=K%2?1.f:-1.f;
                const FVector Dir=(Across(D)*Sgn*.9f+FVector(0,0,.8f)+D*.45f).GetSafeNormal();
                const FVector From=(Centres[K-1]+Centres[K])*.5f+FVector(0,0,GR[K]*.5f)+Across(D)*Sgn*GR[K]*.5f,End=From+Dir*Rng.FRandRange(K%2?75.f:45.f,K%2?105.f:65.f);
                Branch(Branches,From,From+Dir*25,End-FVector(0,0,8)+Across(D)*Sgn*10,End,GR[K]*.4f,.8f,6,6,K);
                if(K%2==0) continue;
                auto* Crown=FindObjectFast<UHierarchicalInstancedStaticMeshComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyCrown%d"),Tree)));
                for(int32 L=0;L<26 && Crown;++L)
                {
                    const FVector Off(Rng.FRandRange(-45,45),Rng.FRandRange(-45,45),Rng.FRandRange(-15,35));
                    const FVector At=End+Off;
                    if(!LeafClear(At)) {++Rejected;continue;}
                    const float Scale=Rng.FRandRange(1.2f,2.f);
                    Crown->AddInstance(FTransform(FRotator(Rng.FRandRange(-50,50),Rng.FRandRange(0,360),Rng.FRandRange(-25,25)),At,FVector(Scale)),false);++LeafCount;
                }
            }
        }
        for(int32 I=0;I<Paws.Num();++I)
        {
            const FVector Along=(Paws[FMath::Min(I+1,Paws.Num()-1)]-Paws[FMath::Max(I-1,0)]).GetSafeNormal2D();
            auto* Marker=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("%s_Paw%02d"),*Chain.Name.ToString(),I)));
            Marker->SetupAttachment(Owner->GetRootComponent());Marker->SetWorldLocationAndRotation(G+Paws[I],Along.Rotation());
            Marker->ComponentTags.Add(TEXT("ChultCanopyGrip"));if(I==Chain.BranchBreak)Marker->ComponentTags.Add(TEXT("ChultCanopyBranchJump"));Marker->RegisterComponent();
            FChuckSwingGrip Grip;Grip.Grip=G+Paws[I];Grip.Along=Along;AddChuckSwingGrip(Grip);
            Registered+=GripIndex(G+Paws[I])>=0;
            if(I)
            {
                const float Step=FVector::Dist2D(Paws[I-1],Paws[I]),Rise=FMath::Abs(Paws[I].Z-Paws[I-1].Z);
                MaxStep=FMath::Max(MaxStep,Step);MinStep=FMath::Min(MinStep,Step);MaxRise=FMath::Max(MaxRise,Rise);
                if(Step<40 || Step>AChuckCharacter::SwingLeapRange || Rise>70)++Failures;
            }
        }
        // End grips hang GripOver above their walking limbs.
        Failures+=!FMath::IsNearlyEqual(Paws[0].Z-ChultCanopyLimbTop(Chain.From,0,FirstGrip).Z,GripOver,1.f);
        Failures+=!FMath::IsNearlyEqual(Paws.Last().Z-ChultCanopyLimbTop(Chain.To,1,FirstGrip).Z,GripOver,1.f);
    }
    Finish(TEXT("ChultCanopySolidBark"),Solid,true);Finish(TEXT("ChultCanopyHandBranches"),Branches,false);Finish(TEXT("ChultCanopyVines"),Vines,false);
    int32 Grips=0;for(const auto& Chain:Data.Chains) Grips+=Chain.Paws.Num();
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY trees=4 chains=%d grips=%d registered_grips=%d lianas=4 triangles=%d"),Data.Chains.Num(),Grips,Registered,(Solid.T.Num()+Branches.T.Num()+Vines.T.Num())/3);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_LAYOUT failures=%d branch_jumps=4 hand_gap=%.0f grip_over=%.0f min_step=%.1f max_step=%.1f max_rise=%.1f fins=%d roots=%d leaves=%d leaves_cleared=%d missing_assets=%d"),
        Failures,2*HalfGap,GripOver,MinStep,MaxStep,MaxRise,Fins,Roots,LeafCount,Rejected,(!Bark)+(!Leaf));
}

int32 CheckChultCanopy(AActor* Owner,AChuckCharacter* C)
{
    UWorld* World=Owner->GetWorld();
    const FVector G=AChultLanding::FloorOrigin();
    const FCanopy& Data=Canopy();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultCanopyCheck),false,C);
    const float Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FCollisionShape Body=FCollisionShape::MakeCapsule(Radius-1,Half-1);
    const float Drop=63.5f-Half;   // ChuckClipData::SwingGripZ: paws above his centre
    int32 Failures=0,Grips=0,Leaps=0,LimbSamples=0;
    auto Fail=[&](const TCHAR* What,const FVector& At){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_CANOPY_FAIL %s at=%s"),What,*At.ToString());};
    FHitResult Hit;
    for(const auto& Chain:Data.Chains)
    {
        for(int32 I=0;I<Chain.Paws.Num();++I)
        {
            const FVector P=G+Chain.Paws[I];++Grips;
            if(GripIndex(P)<0) Fail(TEXT("unregistered_grip"),Chain.Paws[I]);
            // Hanging at rest, then the leap's own clearance checks (FindSwingTarget's).
            if(World->OverlapBlockingTestByChannel(P-FVector(0,0,Drop),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("hang_blocked"),Chain.Paws[I]);
            if(I)
            {
                const FVector A=G+Chain.Paws[I-1];++Leaps;
                if(World->LineTraceSingleByChannel(Hit,A,P,ECC_Visibility,Q)) Fail(TEXT("leap_line"),Chain.Paws[I]);
                if(World->SweepSingleByChannel(Hit,A-FVector(0,0,Drop),P-FVector(0,0,Drop),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("leap_sweep"),Chain.Paws[I]);
            }
        }
        // Both end grips hang over bark to stand on.
        for(const FVector& End:{Chain.Paws[0],Chain.Paws.Last()})
        {
            const bool Floor=World->LineTraceSingleByChannel(Hit,G+End,G+End-FVector(0,0,160),ECC_Visibility,Q);
            if(!Floor || FMath::Abs(End.Z-GripOver-(Hit.ImpactPoint.Z-G.Z))>12 || Hit.ImpactNormal.Z<.8f) Fail(TEXT("end_grip_floor"),End);
        }
    }
    for(int32 I=0;I<4;++I)
    {
        const auto& T=Data.Trees[I];
        for(int32 L=0;L<2;++L)
            for(float S=90;S<=330;S+=30)
            {
                const FVector Top=G+ChultCanopyLimbTop(I,L,S);++LimbSamples;
                const bool Floor=World->LineTraceSingleByChannel(Hit,Top+FVector(0,0,60),Top-FVector(0,0,40),ECC_Visibility,Q);
                if(!Floor || FMath::Abs(Hit.ImpactPoint.Z-Top.Z)>8 || Hit.ImpactNormal.Z<.85f) Fail(TEXT("limb_floor"),Top-G);
                else if(World->OverlapBlockingTestByChannel(Hit.ImpactPoint+FVector(0,0,Half+3),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("limb_headroom"),Top-G);
            }
        const bool Fork=World->LineTraceSingleByChannel(Hit,G+T.Fork+FVector(0,0,60),G+T.Fork-FVector(0,0,40),ECC_Visibility,Q);
        if(!Fork || FMath::Abs(Hit.ImpactPoint.Z-G.Z-T.Top)>18 || Hit.ImpactNormal.Z<.7f
            || World->OverlapBlockingTestByChannel(Hit.ImpactPoint+FVector(0,0,Half+3),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("fork_stand"),T.Fork);
        // The liana: open ground at its foot, room to climb, bark over the lip.
        const bool Ground=World->LineTraceSingleByChannel(Hit,G+T.LianaFoot+FVector(0,0,60),G+T.LianaFoot-FVector(0,0,20),ECC_Visibility,Q);
        if(!Ground || FMath::Abs(Hit.ImpactPoint.Z-G.Z)>4) Fail(TEXT("liana_ground"),T.LianaFoot);
        if(World->SweepSingleByChannel(Hit,G+T.LianaFoot+FVector(0,0,Half+4),G+FVector(T.LianaFoot.X,T.LianaFoot.Y,T.LianaLip.Z-22),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("liana_climb"),T.LianaFoot);
        const FVector In=G+T.LianaLip-T.LianaOut*18.5f;
        if(!World->LineTraceSingleByChannel(Hit,In+FVector(0,0,50),In-FVector(0,0,30),ECC_Visibility,Q) || Hit.ImpactNormal.Z<.7f) Fail(TEXT("liana_lip"),T.LianaLip);
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_CHECK failures=%d grips=%d leaps=%d limb_samples=%d forks=4 lianas=4"),Failures,Grips,Leaps,LimbSamples);
    return Failures;
}

void StartChultCanopyReview(AActor* Owner,AChuckCharacter* C)
{
    UWorld* World=Owner->GetWorld();auto* PC=Cast<APlayerController>(C->GetController());
    C->DisableInput(PC);C->SetLookLocked(true);PC->SetViewTarget(C);
    const bool bCapture=FParse::Param(FCommandLine::Get(),TEXT("ChuckCanopyCapture"));
    const int32 StaticFailures=CheckChultCanopy(Owner,C);
    // Trials in order; each starts where the last left him except where noted.
    enum EKind {KLianaUp,KCatch,KChain,KDismount,KReverse,KLetGo,KLianaDown,KSlabCatch};
    struct FTrial {EKind Kind;int32 Chain;const TCHAR* Name;};
    static const FTrial Trials[]={
        {KLianaUp,0,TEXT("liana_up")},{KCatch,0,TEXT("catch_0")},{KChain,0,TEXT("chain_0")},{KDismount,0,TEXT("dismount_0")},
        {KCatch,1,TEXT("catch_1")},{KChain,1,TEXT("chain_1")},{KDismount,1,TEXT("dismount_1")},
        {KCatch,2,TEXT("catch_2")},{KChain,2,TEXT("chain_2")},{KDismount,2,TEXT("dismount_2")},
        {KCatch,3,TEXT("catch_3")},{KChain,3,TEXT("chain_3")},{KDismount,3,TEXT("dismount_3")},
        {KCatch,0,TEXT("catch_again_0")},{KReverse,0,TEXT("out_and_back_0")},{KLetGo,0,TEXT("let_go_0")},{KLianaDown,0,TEXT("liana_down")},
        {KSlabCatch,0,TEXT("ruin_slab_to_catch_0")}};
    static constexpr int32 TrialCount=UE_ARRAY_COUNT(Trials);
    struct FRun
    {
        int32 Shot=0;ACameraActor* Camera=nullptr;
        int32 Trial=0,Failures=0,Static=0,Deaths=0,PullUps=0,Catches=0,Leaps=0,Ladders=0,Jumps=0,Waypoint=0,Phase=0,ShotIndex=0;
        float Start=-1,LastJump=-1,NextShot=0,Tick=-1;
        TArray<FVector> Path;TArray<int32> Grips;
        FTimerHandle Timer;
    };
    auto Run=MakeShared<FRun>();Run->Failures=Run->Static=StaticFailures;
    const FVector G=AChultLanding::FloorOrigin();
    World->GetTimerManager().SetTimer(Run->Timer,[World,C,PC,Run,G,bCapture]()
    {
        if(Run->Trial>=TrialCount) return;
        const float Now=World->GetTimeSeconds();if(Now==Run->Tick) return;Run->Tick=Now;
        const FCanopy& Shots=Canopy();
        if(bCapture && !Run->Camera) {Run->Camera=World->SpawnActor<ACameraActor>();Run->Camera->GetCameraComponent()->SetFieldOfView(70);}
        // Review stills from fixed cameras before the trials: overview, a tree in profile,
        // a crotch, buttresses and the liana, the branch gap.
        if(bCapture && Run->Shot<12)
        {
            const auto& T0=Shots.Trees[0];const auto& T1=Shots.Trees[1];const auto& Ch=Shots.Chains[0];
            const FVector Gap=(Ch.Paws[Ch.BranchBreak-1]+Ch.Paws[Ch.BranchBreak])*.5f;
            const FVector Front=(T0.Limbs[0]+T0.Limbs[1]).GetSafeNormal2D();
            const FVector Views[][2]={{{4800,-700,2000},{6900,1700,700}},{{7250,-700,700},T1.Trunk+FVector(0,0,700)},
                {T0.Fork+Front*350+FVector(0,0,280),T0.Fork+FVector(0,0,30)},{T0.LianaFoot+T0.LianaOut*420+FVector(0,0,250),T0.Trunk+FVector(0,0,160)},
                {Gap+Across((Ch.Paws.Last()-Ch.Paws[0]).GetSafeNormal2D())*900+FVector(0,0,150),Gap},{{9400,1800,950},Shots.Trees[2].Trunk+FVector(0,0,800)}};
            const int32 View=Run->Shot/2;
            if(Run->Start<0)
            {
                Run->Start=Now;
                C->ResetAtLocation(G+T0.Fork+FVector(0,0,40));C->GetCharacterMovement()->StopMovementImmediately();
                Run->Camera->SetActorLocationAndRotation(G+Views[View][0],(Views[View][1]-Views[View][0]).Rotation());PC->SetViewTarget(Run->Camera);
                if(View==5)
                {
                    // What the player sees: on the first limb, looking out along chain 0.
                    C->ResetAtLocation(G+ChultCanopyLimbTop(0,0,190)+FVector(0,0,36));C->SetActorRotation(T0.Limbs[0].Rotation());
                    C->Recenter();C->SetOrbitPitch(-8);PC->SetViewTarget(C);
                }
            }
            else if(Run->Shot%2==0 && Now-Run->Start>1.5f) ++Run->Shot;
            else if(Run->Shot%2==1)
            {
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Canopy");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("Review_%d.png"),View),false,false);
                ++Run->Shot;Run->Start=-1;
                if(Run->Shot==12) PC->SetViewTarget(C);
            }
            return;
        }
        const FTrial& Trial=Trials[Run->Trial];
        const FCanopy& Data=Canopy();
        const auto& Chain=Data.Chains[Trial.Chain];
        auto* Movement=C->GetCharacterMovement();
        auto Grip=[&](int32 K){return GripIndex(G+Chain.Paws[K]);};
        if(Run->Start<0)
        {
            Run->Start=Now;Run->LastJump=-1;Run->Jumps=0;Run->Waypoint=0;Run->Phase=0;Run->Path.Reset();
            Run->Deaths=C->GetFallDeaths();Run->PullUps=C->GetPullUps();Run->Catches=C->GetSwingCatches();Run->Leaps=C->GetSwingLeaps();Run->Ladders=C->GetLadderMounts();
            C->SetTestStick(FVector2D::ZeroVector);C->SetRunHeld(false);
            const FChultCanopyTree& T0=Data.Trees[0];
            if(Trial.Kind==KLianaUp)
            {
                C->ResetAtLocation(G+T0.LianaFoot+T0.LianaOut*25+FVector(0,0,34.65f));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Walking);
                C->SetActorRotation((-T0.LianaOut).Rotation());C->Recenter();
            }
            else if(Trial.Kind==KLianaDown)
            {
                C->ResetAtLocation(G+T0.LianaLip-T0.LianaOut*28+FVector(0,0,40));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);
                C->SetActorRotation(T0.LianaOut.Rotation());C->Recenter();
            }
            else if(Trial.Kind==KSlabCatch)
            {
                // From the ruin's landing slab (Codex's causeway end): down onto the
                // slab limb, through the crotch and out to the first grip.
                C->ResetAtLocation(G+FVector(5560,940,670+35));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Walking);
                C->SetActorRotation(FRotator(0,90,0));C->Recenter();
                Run->Path={ChultCanopyLimbTop(0,2,190),T0.Fork,ChultCanopyLimbTop(0,0,180),ChultCanopyLimbTop(0,0,FirstGrip)};
            }
            else if(Trial.Kind==KCatch)
            {
                // From wherever he stands on this tree: into the crotch, out along the next limb, under the first grip.
                const int32 Tree=Chain.From;const auto& T=Data.Trees[Tree];
                const FVector P=C->GetActorLocation()-G;
                if(FVector::Dist2D(P,T.Trunk)<160 || Run->Trial==1) {}
                else {Run->Path.Add(ChultCanopyLimbTop(Tree,1,150));}
                if(Run->Trial!=1) Run->Path.Add(T.Fork);
                Run->Path.Add(ChultCanopyLimbTop(Tree,0,180));Run->Path.Add(ChultCanopyLimbTop(Tree,0,FirstGrip));
            }
        }
        const float Time=Now-Run->Start;const FVector P=C->GetActorLocation()-G;
        const bool bGround=Movement->IsMovingOnGround() && !C->IsOnLadder() && FCString::Strcmp(C->GetGaitName(),TEXT("Climb"))!=0;
        auto Press=[&](){C->JumpPressed();Run->LastJump=Time;++Run->Jumps;};
        auto Move=[&](FVector Direction){Direction=Direction.GetSafeNormal2D();C->SetTestStickWorld(Direction);C->AddMovementInput(Direction,1.f,true);};
        bool Pass=false,Done=false;float Limit=25;
        switch(Trial.Kind)
        {
        case KLianaUp:
        {
            const auto& T=Data.Trees[0];
            Move(-T.LianaOut);
            Pass=bGround && P.Z>T.Top-20 && C->GetPullUps()>Run->PullUps && C->GetLadderMounts()>Run->Ladders;
            Done=Pass && Time-Run->LastJump>.3f;
            if(Pass) C->SetTestStick(FVector2D::ZeroVector);
            break;
        }
        case KCatch:
        case KSlabCatch:
        {
            if(Run->Waypoint<Run->Path.Num())
            {
                const FVector To=Run->Path[Run->Waypoint];
                const bool Last=Run->Waypoint==Run->Path.Num()-1;
                if(FVector::Dist2D(P,To)<(Last?14.f:24.f)) ++Run->Waypoint;
                else Move(To-P);
            }
            if(Run->Waypoint>=Run->Path.Num())
            {
                C->SetTestStick(FVector2D::ZeroVector);
                if(bGround && Run->Jumps==0 && Movement->Velocity.Size2D()<30) Press();
            }
            Pass=C->IsSwinging() && C->GetSwingGrip()==Grip(0);
            Done=Pass && Time-Run->LastJump>.5f;
            Limit=30;
            break;
        }
        case KChain:
        {
            const int32 Goal=Grip(Chain.Paws.Num()-1);
            C->SetTestStick(FVector2D::ZeroVector);
            if(C->IsSwinging() && C->GetSwingGrip()!=Goal && Time-Run->LastJump>.45f) Press();
            Pass=C->IsSwinging() && C->GetSwingGrip()==Goal && C->GetSwingLeaps()-Run->Leaps==Chain.Paws.Num()-1;
            Done=(Pass && Time-Run->LastJump>.7f) || (!C->IsSwinging() && !C->IsSwingLeaping());
            break;
        }
        case KDismount:
        {
            // Space with the stick let go at the chain's end: down onto the limb below, not back along the chain.
            const auto& T=Data.Trees[Chain.To];
            C->SetTestStick(FVector2D::ZeroVector);
            if(Run->Jumps==0 && C->IsSwinging() && Time>.2f) Press();
            Pass=Run->Jumps>0 && bGround && P.Z>T.Top-30 && FVector::Dist2D(P,T.Trunk)<450 && C->GetSwingLeaps()==Run->Leaps;
            Done=(Pass && Time-Run->LastJump>.6f) || (Run->Jumps>0 && C->IsSwinging() && Time-Run->LastJump>.8f);
            break;
        }
        case KReverse:
        {
            // Out three grips with the stick let go, then back three with it held back toward the tree.
            const FVector Home=(Data.Trees[Chain.From].Trunk-Chain.Paws[0]).GetSafeNormal2D();
            const int32 Out=Grip(3),Back=Grip(0);
            if(Run->Phase==0 && C->GetSwingGrip()==Out) Run->Phase=1;
            if(Run->Phase==1) C->SetTestStickWorld(Home); else C->SetTestStick(FVector2D::ZeroVector);
            const int32 Goal=Run->Phase?Back:Out;
            if(C->IsSwinging() && C->GetSwingGrip()!=Goal && Time-Run->LastJump>.45f) Press();
            Pass=Run->Phase==1 && C->IsSwinging() && C->GetSwingGrip()==Back && C->GetSwingLeaps()-Run->Leaps==6;
            Done=(Pass && Time-Run->LastJump>.7f) || (!C->IsSwinging() && !C->IsSwingLeaping());
            break;
        }
        case KLetGo:
        {
            // Out over open ground, then let go (dodge): a long fall, rolled out of, no death.
            const int32 Over=Grip(Chain.BranchBreak-1);
            if(Run->Phase==0) C->SetTestStickWorld(Data.Trees[Chain.From].Limbs[0]); else C->SetTestStick(FVector2D::ZeroVector);
            if(Run->Phase==0 && C->IsSwinging() && C->GetSwingGrip()!=Over && Time-Run->LastJump>.45f) Press();
            if(Run->Phase==0 && C->IsSwinging() && C->GetSwingGrip()==Over && Time-Run->LastJump>.5f) {C->SetTestStick(FVector2D::ZeroVector);C->DodgeToward(FVector2D::ZeroVector);Run->Phase=1;}
            Pass=Run->Phase==1 && bGround && P.Z<60;
            Done=Pass && Time-Run->LastJump>1.f;
            break;
        }
        case KLianaDown:
        {
            const auto& T=Data.Trees[0];
            if(Time>.4f) Move(T.LianaOut);
            Pass=C->GetLadderMounts()>Run->Ladders && bGround && P.Z<40;
            Done=Pass;
            break;
        }
        }
        if(bCapture && Time>=Run->NextShot)
        {
            if(Trial.Kind==KChain || Trial.Kind==KReverse)
            {
                const FVector Mid=G+(Chain.Paws[0]+Chain.Paws.Last())*.5f,Side=Across((Chain.Paws.Last()-Chain.Paws[0]).GetSafeNormal2D());
                const FVector Eye=Mid+Side*900+FVector(0,0,40);
                Run->Camera->SetActorLocationAndRotation(Eye,(C->GetActorLocation()-Eye).Rotation());PC->SetViewTarget(Run->Camera);
            }
            else PC->SetViewTarget(C);
            const FString Folder=FPaths::ScreenShotDir()/TEXT("Canopy");IFileManager::Get().MakeDirectory(*Folder,true);
            FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("T%02d_%s_%03d.png"),Run->Trial,Trial.Name,Run->ShotIndex++),false,false);
            Run->NextShot+=Trial.Kind==KChain?.5f:1.f;
        }
        const bool Fell=C->GetFallDeaths()>Run->Deaths || C->IsAstral();
        if(!Done && !Fell && Time<Limit) return;
        Pass=Pass && !Fell;Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CANOPY_TRIAL trial=%d name=%s pass=%d time=%.2f jumps=%d gait=%s grip=%d catches=%d leaps=%d ladders=%d pullups=%d at=%s"),
            Run->Trial,Trial.Name,Pass,Time,Run->Jumps,C->GetGaitName(),C->GetSwingGrip(),C->GetSwingCatches()-Run->Catches,C->GetSwingLeaps()-Run->Leaps,
            C->GetLadderMounts()-Run->Ladders,C->GetPullUps()-Run->PullUps,*P.ToString());
        C->SetTestStick(FVector2D::ZeroVector);
        // A failure leaves nothing to carry on from: stop there.
        if(!Pass) {Run->Failures+=TrialCount-1-Run->Trial;Run->Trial=TrialCount;}
        else ++Run->Trial;
        Run->Start=-1;Run->ShotIndex=0;Run->NextShot=0;
        if(Run->Trial>=TrialCount)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CANOPY_TEST_COMPLETE failures=%d trials=%d static_failures=%d loop=1 reverse=1 let_go=1 lianas=2 slab=1"),Run->Failures,TrialCount,Run->Static);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
