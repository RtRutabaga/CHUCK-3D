#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ProceduralMeshComponent.h"
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
#include "Algo/Reverse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    // Trunk axes, crotch heights and girth. Each tree's first two walking limbs
    // point straight at its neighbours; tree 0 has a third under the ruin's
    // landing slab; spurs fill the other directions (CanopyData below).
    struct FTreeDef { FVector2D At; float Top; float Scale; float LeaderHeight; };
    // 0-3: the loop by the ruins. 4-7 (user 2026-10-14): a staggered line leading
    // away from tree 2 north-east into the open ground beyond the forest ring.
    const FTreeDef Defs[]={{{5560,1230},650,1.f,900},{{7250,760},650,1.08f,1010},{{8350,1800},760,.93f,840},{{6750,2600},650,1.04f,960},
        {{9050,3250},700,.97f,920},{{10350,4200},760,1.06f,1000},{{10650,5750},690,.95f,880},{{11700,6600},740,1.02f,950}};
    constexpr int32 TreeCount=UE_ARRAY_COUNT(Defs);
    // Each tree's neighbours, in limb order (tree 0 adds the ruin-slab limb after them).
    const TArray<TArray<int32>> Neighbours={{1,3},{2,0},{3,1,4},{0,2},{5,2},{6,4},{7,5},{6}};
    struct FChainDef {int32 A,B;};
    const FChainDef ChainDefs[]={{0,1},{1,2},{2,3},{3,0},{2,4},{4,5},{5,6},{6,7}};
    // The line's corridor (part of the canopy reserve).
    const FVector LinePath[]={{8350,1800,0},{9050,3250,0},{10350,4200,0},{10650,5750,0},{11700,6600,0}};
    const FVector2D SlabAt(5530,820);
    constexpr float GripOver=98;      // grip above the walking limb: a standing hop (18 cm) catches it; hanging feet clear the bark by 34
    constexpr float FirstGrip=300;    // first/last grip of a chain, out from the trunk axis
    constexpr float SlabLimbLength=470;
    constexpr float GripSpacing=135;
    constexpr float HalfGap=90;
    constexpr float LianaS=240;       // where each liana hangs over its tree's outgoing limb       // 180 cm between the two boughs' end grips; no wood spans it
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
    FVector YawDir(float Yaw) {return FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),0);}
    int32 BoughSide(int32 Tree,int32 Limb) {return ((Tree+Limb)%2)?1:-1;}

    float LimbRadius(float S,float Len)
    {
        if(S<70) return 62;
        if(S<300) return FMath::Lerp(62.f,34.f,(S-70)/230);
        const float TipStart=Len-45;
        if(S<TipStart) return FMath::Lerp(34.f,25.f,(S-300)/FMath::Max(1.f,TipStart-300));
        const float U=FMath::Clamp((S-TipStart)/45,0.f,1.f);
        return FMath::Max(2.f,25*FMath::Sqrt(1-U*U));
    }
    // The trunk's radius at height Z (before fluting): a broad flare into the ground.
    float TrunkRadius(const FChultCanopyTree& T,float Z)
    {
        const float Fade=1-FMath::Clamp(Z/T.Top,0.f,1.f);
        return (84+16*Fade+75*FMath::Exp(-FMath::Max(Z,0.f)/55))*T.TrunkScale;
    }

    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        void Vertex(FVector P,FVector Normal,FVector2D Tex)
        { V.Add(P); N.Add(Normal); UV.Add(Tex); C.Add(FColor(190,180,0,0)); }
        void Tri(int32 A,int32 B,int32 D) { T.Append({A,B,D}); }
    };
    // UKismetProceduralMeshLibrary::CalculateTangentsForMesh's tangents in linear
    // time. Its all-pairs vertex search (36 s of a white screen at the fall,
    // 2026-10-14) only smooths normals, which the canopy supplies itself; the
    // per-face UV gradients, summed over each vertex's own faces, are the same.
    void CalculateTangents(const FSurface& S,TArray<FProcMeshTangent>& Out)
    {
        const int32 NumVerts=S.V.Num();
        TArray<FVector> SumX,SumY,SumZ;
        SumX.SetNumZeroed(NumVerts);SumY.SetNumZeroed(NumVerts);SumZ.SetNumZeroed(NumVerts);
        TMap<FVector,FVector> NormalAt;   // face normals per position, across UV seams
        TArray<FVector> FaceZ;FaceZ.SetNumUninitialized(S.T.Num()/3);
        for(int32 F=0;F+2<S.T.Num();F+=3)
        {
            const int32 A=S.T[F],B=S.T[F+1],C=S.T[F+2];
            const FVector E1=S.V[B]-S.V[A],E2=S.V[C]-S.V[A];
            const FVector Z=FVector::CrossProduct(S.V[B]-S.V[C],S.V[A]-S.V[C]).GetSafeNormal();
            const FVector2D T1=S.UV[B]-S.UV[A],T2=S.UV[C]-S.UV[A];
            const double Det=T1.X*T2.Y-T2.X*T1.Y;
            FVector X=FVector::ZeroVector,Y=FVector::ZeroVector;
            if(FMath::Abs(Det)>UE_SMALL_NUMBER)
            { X=((E1*T2.Y-E2*T1.Y)/Det).GetSafeNormal(); Y=((E2*T1.X-E1*T2.X)/Det).GetSafeNormal(); }
            FaceZ[F/3]=Z;
            for(const int32 I:{A,B,C}) { SumX[I]+=X; SumY[I]+=Y; NormalAt.FindOrAdd(S.V[I])+=Z; }
        }
        Out.SetNumUninitialized(NumVerts);
        for(int32 I=0;I<NumVerts;++I)
        {
            const FVector Z=NormalAt.FindRef(S.V[I]).GetSafeNormal();
            FVector X=SumX[I].GetSafeNormal();
            X=(X-Z*FVector::DotProduct(Z,X)).GetSafeNormal();
            Out[I]=FProcMeshTangent(X,FVector::DotProduct(FVector::CrossProduct(Z,X),SumY[I])<0);
        }
    }
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
    // Buttresses (user 2026-10-13: no sharp corners where roots meet the trunk)
    // are lobes of the trunk's own surface: broad and filleted where they join
    // it, a thin rounded plank further out, fading into the bark at the top.
    // The outer edge leaves the bark tangentially and swings out to Reach at
    // the ground (as build_fall_jungle.py's buttressed_trunk).
    struct FButtress {float Yaw,Reach,Height,Width;};
    float ButtressExtent(const FChultCanopyTree& T,const FButtress& B,float Z)
    {
        if(Z>=B.Height) return 0;
        const float U=1-FMath::Pow(FMath::Max(Z,0.f)/B.Height,1/2.2f);
        return (B.Reach-TrunkRadius(T,Z))*FMath::Pow(U,1.6f);
    }
    // The buttress's outer edge at U (0 at its top, 1 at the ground).
    FVector ButtressEdge(const FChultCanopyTree& T,const FButtress& B,float U)
    {
        const float Z=B.Height*FMath::Pow(1-U,2.2f);
        return T.Trunk+YawDir(B.Yaw)*(TrunkRadius(T,Z)+ButtressExtent(T,B,Z))+FVector(0,0,Z);
    }
    void ButtressedTrunk(FSurface& S,const FChultCanopyTree& T,const TArray<FButtress>& Buttresses,float Seed)
    {
        // Angles: sparse round the trunk, dense across each lobe; clockwise, as Sweep winds.
        TArray<float> Angles;
        for(int32 K=0;K<40;++K) Angles.Add(2*PI*K/40);
        float TopZ=0;
        for(const FButtress& B:Buttresses)
        {
            const float W=B.Width*1.25f/(TrunkRadius(T,0)+.5f*ButtressExtent(T,B,0));
            for(float O:{-4.f,-2.6f,-1.7f,-1.1f,-.65f,-.3f,0.f,.3f,.65f,1.1f,1.7f,2.6f,4.f}) Angles.Add(FMath::Fmod(B.Yaw+O*W+4*PI,2*PI));
            TopZ=FMath::Max(TopZ,B.Height);
        }
        Angles.Sort();
        TArray<float> Keep={Angles[0]};
        for(float A:Angles) if(A-Keep.Last()>.004f) Keep.Add(A);
        if(Keep.Last()-Keep[0]>2*PI-.004f) Keep.Pop();
        Algo::Reverse(Keep);
        const int32 N=Keep.Num();
        TArray<float> Zs;
        TopZ=FMath::Min(TopZ+40,T.Top-60);
        for(int32 I=0;I<18;++I) Zs.Add(-40+(TopZ+40)*FMath::Pow(I/17.f,1.5f));
        for(int32 I=1;I<=12;++I) Zs.Add(FMath::Lerp(TopZ,T.Top-6,I/12.f));
        TArray<FVector> P;TArray<float> V;
        for(float Z:Zs)
        {
            const float Fade=1-FMath::Clamp(Z/T.Top,0.f,1.f),R=TrunkRadius(T,Z);
            const FVector C=T.Trunk+FVector(14*FMath::Sin(Z/260+Seed)*Fade,11*FMath::Cos(Z/310+Seed*2)*Fade,Z);
            const float Flute=.11f*FMath::Exp(-FMath::Max(Z,0.f)/140)+.022f;
            for(int32 Q=0;Q<=N;++Q)
            {
                const float A=Q<N?Keep[Q]:Keep[0]-2*PI;
                float Rad=R*(1+Flute*FMath::Sin(7*A+Seed)+.018f*FMath::Sin(3*A+Z/90+Seed)+.012f*FMath::Sin(11*A-Z/40));
                for(const FButtress& B:Buttresses)
                {
                    const float Ex=ButtressExtent(T,B,Z);
                    if(Ex<=0) continue;
                    const float W=B.Width*1.25f/(R+.5f*Ex),D=FMath::FindDeltaAngleRadians(B.Yaw,A);
                    Rad+=Ex*FMath::Exp(-FMath::Square(D/W));
                }
                P.Add(C+YawDir(A)*Rad);V.Add((Z+.6f*(Rad-R))/220);
            }
        }
        const int32 Base=S.V.Num(),Rows=Zs.Num(),Ring=N+1;
        for(int32 J=0;J<Rows;++J)
            for(int32 Q=0;Q<=N;++Q)
            {
                // Normals from the grid: across the ring and up the trunk, turned outward.
                const FVector& At=P[J*Ring+Q];
                const FVector Around=P[J*Ring+(Q<N?Q+1:1)]-P[J*Ring+(Q>0?Q-1:N-1)];
                const FVector Up=P[FMath::Min(J+1,Rows-1)*Ring+Q]-P[FMath::Max(J-1,0)*Ring+Q];
                FVector Normal=FVector::CrossProduct(Around,Up).GetSafeNormal();
                const FVector Out=At-T.Trunk;
                if(FVector::DotProduct(Normal,FVector(Out.X,Out.Y,0))<0) Normal=-Normal;
                S.Vertex(At,Normal,FVector2D((N-Q)/static_cast<float>(N)*4,V[J*Ring+Q]));
                if(J && Q<N)
                { const int32 I=Base+J*Ring+Q; S.Tri(I-Ring,I-N,I);S.Tri(I-N,I+1,I); }
            }
        const int32 Cap=S.V.Num(),Last=Base+(Rows-1)*Ring;
        S.Vertex(T.Trunk+FVector(0,0,T.Top+3),FVector::UpVector,FVector2D(.5f,.5f));
        for(int32 K=0;K<N;++K) S.Tri(Cap,Last+K,Last+K+1);
    }

    struct FCanopy
    {
        TArray<FChultCanopyTree> Trees;
        TArray<FChultCanopyChain> Chains;
        TArray<FChultCanopySpur> Spurs;
        TArray<TArray<float>> LimbLeaders;   // per tree: main leader yaw, second leader yaw (radians)
    };
    FVector LimbTopAt(const FChultCanopyTree& T,int32 Tree,int32 Limb,float S)
    {
        const FVector D=T.Limbs[Limb];
        const float Len=T.LimbLength[Limb];
        const bool Slab=Tree==0 && Limb==2;
        // Level (or a gentle rise or fall, on spurs) to the first grip, then a
        // slight droop and a sideways wander toward the tip.
        const float Wave=Slab?0:3*FMath::Sin(S/60+Tree+Limb)*FMath::SmoothStep(100.f,250.f,S);
        const float Droop=FMath::Square(FMath::Max(0.f,S-310))/1100;
        const float Wander=Slab?0:10*FMath::Sin(S/45+Tree*2+Limb)*FMath::SmoothStep(330.f,Len,S);
        return T.Trunk+D*S+Across(D)*Wander+FVector(0,0,(Slab?T.Top-5:T.Top)+Wave-Droop+T.LimbRise[Limb]*S);
    }
    float LimbRadiusOf(const FChultCanopyTree& T,int32 Limb,float S) {return LimbRadius(S,T.LimbLength[Limb])*T.LimbScale[Limb];}
    // Leader K of tree I. The main one (0) rises from the back of the crotch;
    // the second (1) branches off it above head height, toward its own side.
    void LeaderCurve(const FCanopy& Data,int32 I,int32 K,FVector Out4[4])
    {
        const auto& T=Data.Trees[I];const float H=Defs[I].LeaderHeight;
        const FVector Out=YawDir(Data.LimbLeaders[I][0]),Side=Across(Out);
        Out4[0]=T.Trunk+Out*20+FVector(0,0,T.Top-120);Out4[1]=T.Trunk+Out*42+FVector(0,0,T.Top+260);
        Out4[2]=T.Trunk+Out*150+Side*25+FVector(0,0,T.Top+620);Out4[3]=T.Trunk+Out*250+Side*55+FVector(0,0,T.Top+H);
        if(K==0) return;
        const FVector From=Bezier(Out4[0],Out4[1],Out4[2],Out4[3],.34f),Out2=YawDir(Data.LimbLeaders[I][1]);
        Out4[0]=From-Out2*10-FVector(0,0,30);Out4[1]=From+Out2*70+FVector(0,0,130);
        Out4[2]=From+Out2*200+FVector(0,0,330);Out4[3]=From+Out2*310+FVector(0,0,H*.62f);
    }
    float LeaderRadius(int32 K,float U) {return K?FMath::Lerp(40.f,8.f,FMath::Pow(U,.85f)):FMath::Lerp(56.f,9.f,FMath::Pow(U,.9f));}
    const FCanopy& Canopy()
    {
        static const FCanopy Data=[]()
        {
            FCanopy R;
            for(int32 I=0;I<TreeCount;++I)
            {
                FChultCanopyTree T;T.Trunk=Flat(Defs[I].At);T.Top=Defs[I].Top;T.TrunkScale=Defs[I].Scale;
                FRandomStream Rng(3301+I*71);
                auto AddLimb=[&](FVector D,float Len,float Rise,float Scale,float Away)
                {T.Limbs.Add(D);T.LimbLength.Add(Len);T.LimbRise.Add(Rise);T.LimbScale.Add(Scale);T.LimbAway.Add(Away);};
                for(int32 N:Neighbours[I]) AddLimb(Flat(Defs[N].At-Defs[I].At).GetSafeNormal2D(),Rng.FRandRange(405,460),0,1,-BoughSide(I,T.Limbs.Num()));
                if(I==0) AddLimb(Flat(SlabAt-Defs[I].At).GetSafeNormal2D(),SlabLimbLength,0,1,0);
                // (One chain limb, at the end of the line: stand at its base, away from the leader behind.)
                FVector Bisect=T.Limbs.Num()>1?(T.Limbs[0]+T.Limbs[1]).GetSafeNormal2D():T.Limbs[0];
                if(Bisect.IsNearlyZero()) Bisect=Across(T.Limbs[0]);
                T.Fork=T.Trunk+Bisect*45+FVector(0,0,T.Top);
                R.Trees.Add(T);
            }
            // The main leader rises from the back of the crotch, in the widest gap
            // between the fixed limbs; the second branches off it higher up.
            for(int32 I=0;I<TreeCount;++I)
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
                FRandomStream Rng(77+I*13);
                R.LimbLeaders.Add({Mid,static_cast<float>(Mid+PI+FMath::DegreesToRadians(Rng.FRandRange(-45.f,45.f)))});
            }
            // Lianas: one per tree, off the outgoing limb, on the side away from its hand bough.
            for(int32 I=0;I<TreeCount;++I)
            {
                FChultCanopyTree& T=R.Trees[I];
                const FVector D=T.Limbs[0],Out=Across(D)*-BoughSide(I,0);
                const float S=LianaS,Rad=LimbRadiusOf(T,0,S);
                const FVector Axis=LimbTopAt(T,I,0,S)-FVector(0,0,Rad);
                T.LianaOut=Out;
                T.LianaFoot=FVector(Axis.X,Axis.Y,0)+Out*(Rad+21);
                T.LianaLip=Axis+Out*(Rad*.55f)+FVector(0,0,Rad*.83f);
            }
            for(int32 E=0;E<UE_ARRAY_COUNT(ChainDefs);++E)
            {
                const int32 A=ChainDefs[E].A,B=ChainDefs[E].B;
                FChultCanopyChain Chain;Chain.Name=FName(*FString::Printf(TEXT("ChultCanopyChain%d"),E));Chain.From=A;Chain.To=B;
                Chain.LimbA=Neighbours[A].IndexOfByKey(B);Chain.LimbB=Neighbours[B].IndexOfByKey(A);
                const FVector GA=LimbTopAt(R.Trees[A],A,Chain.LimbA,FirstGrip)+FVector(0,0,GripOver);
                const FVector GB=LimbTopAt(R.Trees[B],B,Chain.LimbB,FirstGrip)+FVector(0,0,GripOver);
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
            // Spurs: side limbs in the remaining directions, as a tree grows them -
            // each its own length, girth, pitch and bend. A direction needs room: at
            // least 52 degrees from every other limb (their first grips are then
            // more than 2.4 m apart, beyond a leap, so no line steals another's
            // grips) and 50 from the main leader; and its grips must stay clear of
            // the chains, the other trees and the ruins.
            for(int32 I=0;I<TreeCount;++I)
            {
                FChultCanopyTree& T=R.Trees[I];
                FRandomStream Rng(9001+I*53);
                TArray<float> Banned;
                // (The line's last tree has only one chain limb: it grows more spurs.)
                for(int32 Try=0;Try<(T.Limbs.Num()==1?4:3);++Try)
                {
                    float BestYaw=0,BestScore=-1;
                    for(int32 Deg=0;Deg<360;Deg+=2)
                    {
                        const float Yaw=FMath::DegreesToRadians(static_cast<float>(Deg));
                        float Score=1e6f;
                        for(const FVector& L:T.Limbs) Score=FMath::Min(Score,FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,FMath::Atan2(L.Y,L.X))));
                        const float Lead=FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,R.LimbLeaders[I][0]));
                        bool Skip=Lead<FMath::DegreesToRadians(54.f);
                        for(float B:Banned) Skip|=FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,B))<FMath::DegreesToRadians(12.f);
                        if(!Skip && Score>BestScore) {BestScore=Score;BestYaw=Yaw;}
                    }
                    if(BestScore<FMath::DegreesToRadians(52.f)) break;
                    const float Yaw=BestYaw+FMath::DegreesToRadians(Rng.FRandRange(-4,4));
                    const FVector D=YawDir(Yaw);
                    const float Len=Rng.FRandRange(330,420),Rise=Rng.FRandRange(-.05f,.08f),Scale=Rng.FRandRange(.78f,.95f);
                    const float Bend=Rng.FRandRange(.00012f,.0003f)*(Rng.FRand()<.5f?-1:1),Droop=Rng.FRandRange(0.f,.00012f),RiseB=Rng.FRandRange(-.02f,.05f);
                    const float First=FMath::Min(Rng.FRandRange(270,312),Len-30);
                    const int32 Count=Rng.RandRange(3,5);
                    T.Limbs.Add(D);T.LimbLength.Add(Len);T.LimbRise.Add(Rise);T.LimbScale.Add(Scale);T.LimbAway.Add(-FMath::Sign(Bend));
                    const int32 Limb=T.Limbs.Num()-1;
                    FChultCanopySpur Spur;Spur.Tree=I;Spur.Limb=Limb;
                    const FVector Base=LimbTopAt(T,I,Limb,First)+FVector(0,0,GripOver);
                    float S=First;
                    for(int32 K=0;K<Count;++K)
                    {
                        if(K) S+=Rng.FRandRange(118,138);
                        const float Dd=S-First;
                        Spur.Paws.Add(FVector(T.Trunk.X,T.Trunk.Y,Base.Z)+D*S+Across(D)*Bend*Dd*Dd+FVector(0,0,RiseB*Dd-Droop*Dd*Dd+(K?4*FMath::Sin(K*2.3f+I):0)));
                    }
                    Spur.Paws[0]=Base;
                    bool Clear=true;
                    for(const FVector& P:Spur.Paws)
                    {
                        Clear&=InChultCanopyReserve(P,250) && !(P.X>5290 && P.X<5770 && P.Y>580 && P.Y<1060) && P.Size2D()<13200;
                        for(int32 J=0;J<TreeCount;++J) if(J!=I) Clear&=FVector::Dist2D(P,R.Trees[J].Trunk)>330;
                        for(const auto& Chain:R.Chains) for(const FVector& Q:Chain.Paws) Clear&=FVector::Dist2D(P,Q)>250;
                        for(const auto& Other:R.Spurs) for(const FVector& Q:Other.Paws) Clear&=FVector::Dist2D(P,Q)>250;
                    }
                    if(!Clear)
                    {
                        T.Limbs.Pop();T.LimbLength.Pop();T.LimbRise.Pop();T.LimbScale.Pop();T.LimbAway.Pop();
                        Banned.Add(BestYaw);continue;
                    }
                    Spur.Name=FName(*FString::Printf(TEXT("ChultCanopySpur%d_%d"),I,Limb));
                    R.Spurs.Add(MoveTemp(Spur));
                }
            }
            return R;
        }();
        return Data;
    }
    int32 GripIndex(const FVector& World)
    {
        const auto& Grips=GetChuckSwingGrips();
        for(int32 I=0;I<Grips.Num();++I) if(FVector::Dist(Grips[I].Grip,World)<1) return I;
        return -1;
    }
}

namespace
{
    // On Codex's landing slab (top Z=670, a checked standing top), by its north
    // edge where it steps down onto tree 0's limb.
    const FVector RespawnTop(5600,885,670);
}
bool InChultCanopyDeathArea(const FVector& World) {return InChultCanopyReserve(World-AChultLanding::FloorOrigin(),500);}
FVector ChultCanopyRespawn() {return AChultLanding::FloorOrigin()+RespawnTop+FVector(0,0,34.65f);}
float ChultCanopyRespawnYaw() {return (Canopy().Trees[0].Trunk-RespawnTop).GetSafeNormal2D().Rotation().Yaw;}
bool InChultCanopyReserve(const FVector& P,float Margin)
{
    if(P.X>5000-Margin && P.X<8800+Margin && P.Y>250-Margin && P.Y<3000+Margin) return true;
    for(int32 I=1;I<UE_ARRAY_COUNT(LinePath);++I)
        if(FVector::Dist2D(P,FMath::ClosestPointOnSegment(FVector(P.X,P.Y,0),LinePath[I-1],LinePath[I]))<1000+Margin) return true;
    return false;
}

const TArray<FChultCanopyChain>& ChultCanopyChains() {return Canopy().Chains;}
const TArray<FChultCanopySpur>& ChultCanopySpurs() {return Canopy().Spurs;}
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
        TArray<FProcMeshTangent> Tangents;
        const double Start=FPlatformTime::Seconds();
        CalculateTangents(S,Tangents);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_TANGENTS mesh=%s vertices=%d ms=%.1f"),Name,S.V.Num(),(FPlatformTime::Seconds()-Start)*1000);
        P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,Tangents,Solid);
        P->SetMaterial(0,Bark);P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid)P->SetCollisionProfileName(TEXT("BlockAll"));P->ComponentTags.Add(TEXT("ChultCanopy"));
    };
    // Every line of grips: the chains and the spurs.
    TArray<const TArray<FVector>*> Lines;
    for(const auto& Chain:Data.Chains) Lines.Add(&Chain.Paws);
    for(const auto& Spur:Data.Spurs) Lines.Add(&Spur.Paws);
    // Leaves and side branches stay out of the swing lanes, off the walking limbs and clear of the crotches.
    auto InLane=[&](const FVector& P,float Wide)
    {
        for(const auto* Line:Lines)
            for(int32 I=1;I<Line->Num();++I)
            {
                const FVector Q=FMath::ClosestPointOnSegment(P,(*Line)[I-1],(*Line)[I]);
                if(FVector::Dist2D(P,Q)<Wide && P.Z>Q.Z-130 && P.Z<Q.Z+45) return true;
            }
        return false;
    };
    auto LeafClear=[&](const FVector& P)
    {
        if(InLane(P,75)) return false;
        for(int32 I=0;I<Data.Trees.Num();++I)
        {
            const auto& T=Data.Trees[I];
            if(FVector::Dist2D(P,T.Fork)<170 && P.Z<T.Top+230) return false;
            for(int32 L=0;L<T.Limbs.Num();++L)
            {
                const FVector Q=FMath::ClosestPointOnSegment(P,T.Trunk+FVector(0,0,T.Top),LimbTopAt(T,I,L,T.LimbLength[L]));
                if(FVector::Dist2D(P,Q)<70 && P.Z<Q.Z+160 && P.Z>Q.Z-80) return false;
            }
        }
        return true;
    };
    FSurface Solid,Branches,Vines;int32 Fins=0,Roots=0,LeafCount=0,Rejected=0,CrownBoughs=0;
    for(int32 I=0;I<Data.Trees.Num();++I)
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
        // Trunk and plank buttresses, one surface (ButtressedTrunk): each buttress
        // its own height, reach and thickness; none crosses a ground lane or the
        // liana's foot.
        {
            const float LianaYaw=FMath::Atan2(T.LianaFoot.Y-T.Trunk.Y,T.LianaFoot.X-T.Trunk.X);
            TArray<FButtress> Buttresses;
            const int32 Count=Rng.RandRange(5,8);
            for(int32 F=0;F<Count;++F)
            {
                FButtress B{static_cast<float>(F*2*PI/Count+Seed+Rng.FRandRange(-.25f,.25f)),Rng.FRandRange(250,420)*T.TrunkScale,Rng.FRandRange(170,320),Rng.FRandRange(11,18)};
                if(FMath::Abs(FMath::FindDeltaAngleRadians(B.Yaw,LianaYaw))<FMath::DegreesToRadians(30.f)) continue;
                bool Clear=true;
                for(int32 K=1;K<=20;++K)
                {
                    const FVector At=ButtressEdge(T,B,K/20.f);
                    Clear&=RootClear(At) && FVector::Dist2D(At,T.LianaFoot)>B.Width*2.5f+60;
                }
                if(Clear) Buttresses.Add(B);
            }
            ButtressedTrunk(Solid,T,Buttresses,Seed);
            Fins+=Buttresses.Num();
        }
        // Walking limbs: thick at the crotch, level out (or rise or fall a little)
        // past the first grip, then a slight droop. Their tops are the bark he walks on.
        for(int32 L=0;L<T.Limbs.Num();++L)
        {
            const float Len=T.LimbLength[L];
            const bool Slab=I==0 && L==2;
            TArray<FVector> P;TArray<float> R;
            for(float S=0;S<=Len+.1f;S+=Len/36)
            {
                // The slab's limb ends under the stone; the others carry on as a thinner branch.
                const float Rad=(Slab?LimbRadius(S,Len):LimbRadius(FMath::Min(S,Len-46),Len))*T.LimbScale[L];
                P.Add(LimbTopAt(T,I,L,S)-FVector(0,0,Rad));R.Add(Rad);
            }
            if(!Slab)
            {
                // Out past the walking part it bends away from under the grips, dips and
                // rises to a leafy end, rather than stopping in a blunt stub.
                const FVector D=T.Limbs[L],Away=Across(D)*T.LimbAway[L],E=P.Last(),Dir=(P.Last()-P[P.Num()-2]).GetSafeNormal();
                const float Curl=FMath::Lerp(25.f,70.f,FMath::Frac((I*3+L)*.37f)),Spread=FMath::Lerp(140.f,200.f,FMath::Frac((I*5+L*3)*.29f));
                const FVector C1=E+Dir*60+Away*25,C2=E+D*120+Away*Spread*.5f+FVector(0,0,Curl*.3f),End=E+D*160+Away*Spread+FVector(0,0,Curl);
                const float R0=R.Last();
                for(int32 J=1;J<=12;++J){const float U=J/12.f;P.Add(Bezier(E,C1,C2,End,U));R.Add(FMath::Lerp(R0,2.5f,FMath::Pow(U,.6f)));}
                Cluster(End+FVector(0,0,30),FMath::Lerp(100.f,150.f,FMath::Frac(L*.43f+I*.17f)),90,.5f);
                Cluster(Bezier(E,C1,C2,End,.7f)+Away*30+FVector(0,0,70),70,30,.6f);
            }
            Sweep(Solid,P,R,R,28,[&](int32 J,float A){return 1+.035f*FMath::Sin(4*A+J*.5f+Seed+L)+.02f*FMath::Sin(7*A-J*.3f);},true,nullptr,true);
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
        // Leaders, and boughs off them spreading all round (golden-angle turns,
        // each its own reach and rise) with leaves gathered at their ends.
        for(int32 K=0;K<2;++K)
        {
            FVector Curve[4];LeaderCurve(Data,I,K,Curve);
            const FVector A=Curve[0],B=Curve[1],C=Curve[2],D=Curve[3];
            TArray<FVector> P;TArray<float> R;
            for(int32 J=0;J<=22;++J){const float U=J/22.f;P.Add(Bezier(A,B,C,D,U));R.Add(LeaderRadius(K,U));}
            Round(Solid,P,R,24,I*2+K);
            Cluster(D+FVector(0,0,50),K?190:240,K?220:320,.45f);
            const int32 Count=K?4:7;
            float Turn=Rng.FRandRange(0,2*PI);
            for(int32 Bough=0;Bough<Count;++Bough)
            {
                Turn+=2.39996f+Rng.FRandRange(-.3f,.3f);
                const float U=FMath::Lerp(K?.25f:.3f,.88f,(Bough+Rng.FRandRange(.1f,.9f))/Count);
                const FVector From=Bezier(A,B,C,D,U);
                const FVector Dir=YawDir(Turn);
                const float Reach=Rng.FRandRange(200,400)*(1.1f-.3f*U),Rise=Rng.FRandRange(40,170);
                const FVector End=From+Dir*Reach+FVector(0,0,Rise);
                bool Clear=true;
                for(int32 J=1;J<=6;++J) Clear&=!InLane(Bezier(From,From+Dir*Reach*.35f+FVector(0,0,Rise*.75f),From+Dir*Reach*.7f+FVector(0,0,Rise*1.05f),End,J/6.f),90);
                if(!Clear) continue;
                Branch(Branches,From,From+Dir*Reach*.35f+FVector(0,0,Rise*.75f),From+Dir*Reach*.7f+FVector(0,0,Rise*1.05f),End,LeaderRadius(K,U)*.55f,2.5f,10,10,Bough+K*7);
                ++CrownBoughs;
                Cluster(End,Rng.FRandRange(110,170),Rng.RandRange(110,170),.5f);
                Cluster(From+Dir*Reach*.6f+FVector(0,0,Rise*.9f+25),Rng.FRandRange(70,100),Rng.RandRange(35,65),.6f);
            }
        }
        const float LianaYaw=FMath::Atan2(T.LianaFoot.Y-T.Trunk.Y,T.LianaFoot.X-T.Trunk.X);
        // Low surface roots snake between the fins.
        for(int32 K=0;K<4;++K)
        {
            const float Yaw=(K+.5f)*2*PI/4+Seed*1.3f;
            if(FMath::Abs(FMath::FindDeltaAngleRadians(Yaw,LianaYaw))<FMath::DegreesToRadians(30.f)) continue;
            const FVector Out=YawDir(Yaw),Side=Across(Out);
            const float Reach=Rng.FRandRange(230,340);
            bool Clear=true;
            for(int32 J=1;J<=14;++J)
            {
                const float U=J/14.f;const FVector At=T.Trunk+Out*(60+(Reach-60)*U)+Side*28*FMath::Sin(U*5+K);
                Clear&=RootClear(At) && FVector::Dist2D(At,T.LianaFoot)>60;
            }
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
        // The liana: over the limb's side and down to the ground.
        {
            const FVector Rope=T.LianaFoot-T.LianaOut*18;
            const float LimbR=LimbRadiusOf(T,0,LianaS);
            const FVector Axis=ChultCanopyLimbTop(I,0,LianaS)-FVector(0,0,LimbR);
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
                const FVector From=T.Trunk+YawDir(Yaw)*Rng.FRandRange(180,300)+FVector(0,0,T.Top+Rng.FRandRange(420,560));
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
    // Hand boughs: from a leader, up over the walking limb and out along the line
    // of grips, thinning toward its end; the grips hang under them.
    auto HandBough=[&](int32 Tree,int32 Limb,const TArray<FVector>& Grips,int32 Seed,float Thick)
    {
        const auto& T=Data.Trees[Tree];
        const FVector D=T.Limbs[Limb],Side=Across(D)*-T.LimbAway[Limb];
        TArray<float> GR;TArray<FVector> Centres;
        for(int32 K=0;K<Grips.Num();++K)
        {
            GR.Add(FMath::Lerp(12.f,6.f,K/static_cast<float>(FMath::Max(1,Grips.Num()-1)))*Thick);
            Centres.Add(Grips[K]+FVector(0,0,GR[K]-2.5f));
        }
        // The root: the point on either leader nearest above the limb's base.
        const FVector Want=T.Trunk+D*50+FVector(0,0,T.Top+215);
        FVector Root=Want;float RootDist=1e9f;
        for(int32 K=0;K<2;++K)
        {
            FVector Lead[4];LeaderCurve(Data,Tree,K,Lead);
            for(int32 J=0;J<=12;++J)
            {
                const float U=FMath::Lerp(K?0.f:.15f,.5f,J/12.f);
                const FVector P=Bezier(Lead[0],Lead[1],Lead[2],Lead[3],U);
                if(P.Z>T.Top+170 && FVector::Dist(P,Want)<RootDist){RootDist=FVector::Dist(P,Want);Root=P;}
            }
        }
        TArray<FVector> P;TArray<float> R;
        const FVector C1=T.Trunk+D*85+Side*25+FVector(0,0,T.Top+200),C2=T.Trunk+D*215+Side*12+FVector(0,0,Centres[0].Z+30);
        for(int32 J=0;J<14;++J)
        {
            const float U=J/14.f;
            P.Add(Bezier(Root,C1,C2,Centres[0],U));
            R.Add(FMath::Lerp(19.f*Thick,GR[0],FMath::SmoothStep(0.f,1.f,U)));
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
        Round(Branches,P,R,14,Seed,.03f);
        // Leafy twigs rise off the bough, above and beside the hands' line.
        FRandomStream Rng(400+Seed*17);
        auto* Crown=FindObjectFast<UHierarchicalInstancedStaticMeshComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyCrown%d"),Tree)));
        for(int32 K=1;K<Centres.Num();++K)
        {
            const float Sgn=K%2?1.f:-1.f;
            const FVector Dir=(Across(D)*Sgn*.9f+FVector(0,0,.8f)+D*.45f).GetSafeNormal();
            const FVector From=(Centres[K-1]+Centres[K])*.5f+FVector(0,0,GR[K]*.5f)+Across(D)*Sgn*GR[K]*.5f,End=From+Dir*Rng.FRandRange(K%2?75.f:45.f,K%2?105.f:65.f);
            Branch(Branches,From,From+Dir*25,End-FVector(0,0,8)+Across(D)*Sgn*10,End,GR[K]*.4f,.8f,6,6,K);
            if(K%2==0 && K!=Centres.Num()-1) continue;
            for(int32 L=0;L<26 && Crown;++L)
            {
                const FVector Off(Rng.FRandRange(-45,45),Rng.FRandRange(-45,45),Rng.FRandRange(-15,35));
                const FVector At=End+Off;
                if(!LeafClear(At)) {++Rejected;continue;}
                const float Scale=Rng.FRandRange(1.2f,2.f);
                Crown->AddInstance(FTransform(FRotator(Rng.FRandRange(-50,50),Rng.FRandRange(0,360),Rng.FRandRange(-25,25)),At,FVector(Scale)),false);++LeafCount;
            }
        }
    };
    int32 Failures=0,Registered=0;float MaxStep=0,MaxRise=0,MinStep=1e6f;
    auto Register=[&](const TArray<FVector>& Paws,FName Name,int32 BranchBreak)
    {
        for(int32 I=0;I<Paws.Num();++I)
        {
            const FVector Along=(Paws[FMath::Min(I+1,Paws.Num()-1)]-Paws[FMath::Max(I-1,0)]).GetSafeNormal2D();
            auto* Marker=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("%s_Paw%02d"),*Name.ToString(),I)));
            Marker->SetupAttachment(Owner->GetRootComponent());Marker->SetWorldLocationAndRotation(G+Paws[I],Along.Rotation());
            Marker->ComponentTags.Add(TEXT("ChultCanopyGrip"));if(I==BranchBreak)Marker->ComponentTags.Add(TEXT("ChultCanopyBranchJump"));Marker->RegisterComponent();
            FChuckSwingGrip Grip;Grip.Grip=G+Paws[I];Grip.Along=Along;AddChuckSwingGrip(Grip);
            Registered+=GripIndex(G+Paws[I])>=0;
            if(I)
            {
                const float Step=FVector::Dist2D(Paws[I-1],Paws[I]),Rise=FMath::Abs(Paws[I].Z-Paws[I-1].Z);
                MaxStep=FMath::Max(MaxStep,Step);MinStep=FMath::Min(MinStep,Step);MaxRise=FMath::Max(MaxRise,Rise);
                if(Step<40 || Step>AChuckCharacter::SwingLeapRange || Rise>70)++Failures;
            }
        }
    };
    for(int32 E=0;E<Data.Chains.Num();++E)
    {
        const auto& Chain=Data.Chains[E];const auto& Paws=Chain.Paws;
        for(int32 Half=0;Half<2;++Half)
        {
            TArray<FVector> Grips;
            if(Half) for(int32 K=Paws.Num()-1;K>=Chain.BranchBreak;--K) Grips.Add(Paws[K]);
            else for(int32 K=0;K<Chain.BranchBreak;++K) Grips.Add(Paws[K]);
            HandBough(Half?Chain.To:Chain.From,Half?Chain.LimbB:Chain.LimbA,Grips,E*2+Half,1.f+.08f*FMath::Sin(E*2.1f+Half*1.3f));
        }
        Register(Paws,Chain.Name,Chain.BranchBreak);
        // End grips hang GripOver above their walking limbs.
        Failures+=!FMath::IsNearlyEqual(Paws[0].Z-ChultCanopyLimbTop(Chain.From,Chain.LimbA,FirstGrip).Z,GripOver,1.f);
        Failures+=!FMath::IsNearlyEqual(Paws.Last().Z-ChultCanopyLimbTop(Chain.To,Chain.LimbB,FirstGrip).Z,GripOver,1.f);
    }
    int32 SpurGrips=0;
    for(int32 E=0;E<Data.Spurs.Num();++E)
    {
        const auto& Spur=Data.Spurs[E];
        HandBough(Spur.Tree,Spur.Limb,Spur.Paws,20+E,Data.Trees[Spur.Tree].LimbScale[Spur.Limb]*1.05f);
        Register(Spur.Paws,Spur.Name,INDEX_NONE);
        SpurGrips+=Spur.Paws.Num();
    }
    Finish(TEXT("ChultCanopySolidBark"),Solid,true);Finish(TEXT("ChultCanopyHandBranches"),Branches,false);Finish(TEXT("ChultCanopyVines"),Vines,false);
    int32 Grips=0;for(const auto& Chain:Data.Chains) Grips+=Chain.Paws.Num();
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY trees=%d chains=%d grips=%d spurs=%d spur_grips=%d registered_grips=%d lianas=%d triangles=%d"),
        Data.Trees.Num(),Data.Chains.Num(),Grips,Data.Spurs.Num(),SpurGrips,Registered,Data.Trees.Num(),(Solid.T.Num()+Branches.T.Num()+Vines.T.Num())/3);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_LAYOUT failures=%d branch_jumps=%d hand_gap=%.0f grip_over=%.0f min_step=%.1f max_step=%.1f max_rise=%.1f fins=%d roots=%d crown_boughs=%d leaves=%d leaves_cleared=%d missing_assets=%d"),
        Failures,Data.Chains.Num(),2*HalfGap,GripOver,MinStep,MaxStep,MaxRise,Fins,Roots,CrownBoughs,LeafCount,Rejected,(!Bark)+(!Leaf));
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
    FHitResult Hit;
    auto Fail=[&](const TCHAR* What,const FVector& At)
    {
        ++Failures;
        UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_CANOPY_FAIL %s at=%s hit=%s hit_at=%s"),What,*At.ToString(),*GetNameSafe(Hit.GetComponent()),*(Hit.ImpactPoint-G).ToString());
    };
    auto CheckLine=[&](const TArray<FVector>& Paws)
    {
        for(int32 I=0;I<Paws.Num();++I)
        {
            const FVector P=G+Paws[I];++Grips;
            if(GripIndex(P)<0) Fail(TEXT("unregistered_grip"),Paws[I]);
            // Hanging at rest, then the leap's own clearance checks (FindSwingTarget's).
            if(World->OverlapBlockingTestByChannel(P-FVector(0,0,Drop),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("hang_blocked"),Paws[I]);
            if(I)
            {
                const FVector A=G+Paws[I-1];++Leaps;
                if(World->LineTraceSingleByChannel(Hit,A,P,ECC_Visibility,Q)) Fail(TEXT("leap_line"),Paws[I]);
                if(World->SweepSingleByChannel(Hit,A-FVector(0,0,Drop),P-FVector(0,0,Drop),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("leap_sweep"),Paws[I]);
            }
        }
    };
    // End grips over limbs hang GripOver above bark to stand on (and to step down to).
    auto EndFloor=[&](const FVector& End)
    {
        const bool Floor=World->LineTraceSingleByChannel(Hit,G+End,G+End-FVector(0,0,160),ECC_Visibility,Q);
        if(!Floor || FMath::Abs(End.Z-GripOver-(Hit.ImpactPoint.Z-G.Z))>12 || Hit.ImpactNormal.Z<.8f) Fail(TEXT("end_grip_floor"),End);
    };
    for(const auto& Chain:Data.Chains) {CheckLine(Chain.Paws);EndFloor(Chain.Paws[0]);EndFloor(Chain.Paws.Last());}
    // Spurs: the outer end must have nothing under it to step down to (there a
    // jump goes back along the spur instead).
    for(const auto& Spur:Data.Spurs)
    {
        CheckLine(Spur.Paws);EndFloor(Spur.Paws[0]);
        const FVector Tip=G+Spur.Paws.Last()+(Spur.Paws.Last()-Spur.Paws[Spur.Paws.Num()-2]).GetSafeNormal2D()*40;
        if(World->LineTraceSingleByChannel(Hit,Tip,Tip-FVector(0,0,170),ECC_Visibility,Q)) Fail(TEXT("spur_tip_floor"),Spur.Paws.Last());
    }
    for(int32 I=0;I<Data.Trees.Num();++I)
    {
        const auto& T=Data.Trees[I];
        TArray<TPair<int32,float>> Walks;
        for(const auto& Chain:Data.Chains)
        {
            if(Chain.From==I) Walks.Add({Chain.LimbA,330.f});
            if(Chain.To==I) Walks.Add({Chain.LimbB,330.f});
        }
        for(const auto& Spur:Data.Spurs) if(Spur.Tree==I) Walks.Add({Spur.Limb,FVector::Dist2D(Spur.Paws[0],T.Trunk)+20});
        for(const auto& Walk:Walks)
            for(float S=90;S<=Walk.Value;S+=30)
            {
                const int32 L=Walk.Key;const FVector Top=G+ChultCanopyLimbTop(I,L,S);++LimbSamples;
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
    {
        // The canopy respawn: on stone, with room to stand.
        const FVector Spawn=ChultCanopyRespawn();
        const bool Floor=World->LineTraceSingleByChannel(Hit,Spawn+FVector(0,0,40),Spawn-FVector(0,0,Half+20),ECC_Visibility,Q);
        if(!Floor || FMath::Abs(Hit.ImpactPoint.Z-(Spawn.Z-34.65f))>6 || Hit.ImpactNormal.Z<.85f
            || World->OverlapBlockingTestByChannel(Spawn+FVector(0,0,1),FQuat::Identity,ECC_Pawn,Body,Q)) Fail(TEXT("respawn_stand"),Spawn-G);
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_CHECK failures=%d grips=%d leaps=%d limb_samples=%d forks=%d lianas=%d spurs=%d"),Failures,Grips,Leaps,LimbSamples,Data.Trees.Num(),Data.Trees.Num(),Data.Spurs.Num());
    return Failures;
}

void StartChultCanopyReview(AActor* Owner,AChuckCharacter* C)
{
    UWorld* World=Owner->GetWorld();auto* PC=Cast<APlayerController>(C->GetController());
    C->DisableInput(PC);C->SetLookLocked(true);PC->SetViewTarget(C);
    const bool bCapture=FParse::Param(FCommandLine::Get(),TEXT("ChuckCanopyCapture"));
    const int32 StaticFailures=CheckChultCanopy(Owner,C);
    // Trials in order; each starts where the last left him except where noted.
    enum EKind {KLianaUp,KCatch,KChain,KDismount,KReverse,KLetGo,KLianaDown,KSlabCatch,KSpur,KDeath};
    struct FTrial {EKind Kind;int32 Chain;FString Name;bool bReset=false;};
    TArray<FTrial> Trials={
        {KLianaUp,0,TEXT("liana_up")},{KCatch,0,TEXT("catch_0")},{KChain,0,TEXT("chain_0")},{KDismount,0,TEXT("dismount_0")},
        {KCatch,1,TEXT("catch_1")},{KChain,1,TEXT("chain_1")},{KDismount,1,TEXT("dismount_1")},
        {KCatch,2,TEXT("catch_2")},{KChain,2,TEXT("chain_2")},{KDismount,2,TEXT("dismount_2")},
        {KCatch,3,TEXT("catch_3")},{KChain,3,TEXT("chain_3")},{KDismount,3,TEXT("dismount_3")},
        {KCatch,0,TEXT("catch_again_0")},{KReverse,0,TEXT("out_and_back_0")},{KLetGo,0,TEXT("let_go_0")},{KLianaDown,0,TEXT("liana_down")},
        {KSlabCatch,0,TEXT("ruin_slab_to_catch_0")}};
    // The line (2026-10-14): from tree 2's crotch along every chain to the last
    // tree, stepping down at each, then down that tree's liana.
    for(int32 E=4;E<Canopy().Chains.Num();++E)
    {
        Trials.Add({KCatch,E,FString::Printf(TEXT("line_catch_%d"),E),E==4});
        Trials.Add({KChain,E,FString::Printf(TEXT("line_chain_%d"),E)});
        Trials.Add({KDismount,E,FString::Printf(TEXT("line_dismount_%d"),E)});
    }
    Trials.Add({KLianaDown,Canopy().Chains.Last().To,TEXT("line_liana_down")});
    // Deaths (user 2026-10-14): around the trees he comes back on the ruin top that
    // leads onto them; elsewhere in Chult, at the landing entrance. (Chain: 0 on a
    // line tree's crotch, 1 on the ground under the line, 2 on the landing's loop.)
    Trials.Add({KDeath,0,TEXT("death_in_tree")});
    Trials.Add({KDeath,1,TEXT("death_under_line")});
    Trials.Add({KDeath,2,TEXT("death_landing_loop")});
    // Every spur: walked out from its crotch, caught, swung out to its tip and
    // back, then stepped down onto its limb.
    for(int32 S=0;S<Canopy().Spurs.Num();++S) Trials.Add({KSpur,S,Canopy().Spurs[S].Name.ToString()});
    const int32 TrialCount=Trials.Num();
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
    World->GetTimerManager().SetTimer(Run->Timer,[World,C,PC,Run,G,bCapture,Trials,TrialCount]()
    {
        if(Run->Trial>=TrialCount) return;
        const float Now=World->GetTimeSeconds();if(Now==Run->Tick) return;Run->Tick=Now;
        const FCanopy& Shots=Canopy();
        if(bCapture && !Run->Camera) {Run->Camera=World->SpawnActor<ACameraActor>();Run->Camera->GetCameraComponent()->SetFieldOfView(70);}
        // Review stills from fixed cameras before the trials: overview, a tree in profile,
        // a crotch, buttresses and the liana, the branch gap.
        if(bCapture && Run->Shot<20)
        {
            const auto& T0=Shots.Trees[0];const auto& T1=Shots.Trees[1];const auto& Ch=Shots.Chains[0];
            const FVector Gap=(Ch.Paws[Ch.BranchBreak-1]+Ch.Paws[Ch.BranchBreak])*.5f;
            const FVector Front=(T0.Limbs[0]+T0.Limbs[1]).GetSafeNormal2D();
            const FVector Views[][2]={{{4800,-700,2000},{6900,1700,700}},{{7250,-700,700},T1.Trunk+FVector(0,0,700)},
                {T0.Fork+Front*350+FVector(0,0,280),T0.Fork+FVector(0,0,30)},{T0.LianaFoot+T0.LianaOut*420+FVector(0,0,250),T0.Trunk+FVector(0,0,160)},
                {Gap+Across((Ch.Paws.Last()-Ch.Paws[0]).GetSafeNormal2D())*900+FVector(0,0,150),Gap},{{9400,1800,950},Shots.Trees[2].Trunk+FVector(0,0,800)},
                {{-180,-260,260},{-760,200,200}},{T1.Trunk+FVector(-720,-520,230),T1.Trunk+FVector(0,0,190)},
                {{6500,-1500,5600},{10500,4500,0}},{Shots.Trees[2].Trunk+FVector(-700,-1300,1000),Shots.Trees[5].Trunk+FVector(0,0,600)}};
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
                // (Back to his camera when the first trial captures, not now: the shot renders this frame.)
            }
            return;
        }
        const FTrial& Trial=Trials[Run->Trial];
        const FCanopy& Data=Canopy();
        const auto& Chain=Data.Chains[Trial.Kind==KSpur?0:Trial.Chain];
        const FChultCanopySpur* Spur=Trial.Kind==KSpur?&Data.Spurs[Trial.Chain]:nullptr;
        auto* Movement=C->GetCharacterMovement();
        auto Grip=[&](int32 K){return GripIndex(G+Chain.Paws[K]);};
        if(Run->Start<0)
        {
            Run->Start=Now;Run->LastJump=-1;Run->Jumps=0;Run->Waypoint=0;Run->Phase=0;Run->Path.Reset();
            Run->Deaths=C->GetFallDeaths();Run->PullUps=C->GetPullUps();Run->Catches=C->GetSwingCatches();Run->Leaps=C->GetSwingLeaps();Run->Ladders=C->GetLadderMounts();
            C->SetTestStick(FVector2D::ZeroVector);C->SetRunHeld(false);
            const FChultCanopyTree& T0=Data.Trees[0];
            const FChultCanopyTree& TL=Data.Trees[Trial.Kind==KLianaDown?Trial.Chain:0];
            if(Trial.Kind==KLianaUp)
            {
                C->ResetAtLocation(G+T0.LianaFoot+T0.LianaOut*25+FVector(0,0,34.65f));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Walking);
                C->SetActorRotation((-T0.LianaOut).Rotation());C->Recenter();
            }
            else if(Trial.Kind==KLianaDown)
            {
                C->ResetAtLocation(G+TL.LianaLip-TL.LianaOut*28+FVector(0,0,40));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);
                C->SetActorRotation(TL.LianaOut.Rotation());C->Recenter();
            }
            else if(Trial.Kind==KSlabCatch)
            {
                // From the ruin's landing slab (Codex's causeway end): down onto the
                // slab limb, through the crotch and out to the first grip.
                C->ResetAtLocation(G+FVector(5560,940,670+35));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Walking);
                C->SetActorRotation(FRotator(0,90,0));C->Recenter();
                Run->Path={ChultCanopyLimbTop(0,2,190),T0.Fork,ChultCanopyLimbTop(0,0,180),ChultCanopyLimbTop(0,0,FirstGrip)};
            }
            else if(Trial.Kind==KDeath)
            {
                const auto& T5=Data.Trees[5],&T6=Data.Trees[6];
                const FVector Where=Trial.Chain==0?G+T5.Fork+FVector(0,0,40):Trial.Chain==1?G+(T5.Trunk+T6.Trunk)*.5f+FVector(0,0,34.65f)
                    :AChultLanding::Entrance()+FVector(700,0,0);
                C->ResetAtLocation(Where);Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);C->Recenter();
                C->SetSanity(1);
            }
            else if(Trial.Kind==KSpur)
            {
                // From the crotch: out along the spur's limb to under its first grip.
                const auto& T=Data.Trees[Spur->Tree];
                C->ResetAtLocation(G+T.Fork+FVector(0,0,40));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);
                C->SetActorRotation(T.Limbs[Spur->Limb].Rotation());C->Recenter();
                const float First=FVector::Dist2D(Spur->Paws[0],T.Trunk);
                Run->Path={ChultCanopyLimbTop(Spur->Tree,Spur->Limb,140),ChultCanopyLimbTop(Spur->Tree,Spur->Limb,First)};
            }
            else if(Trial.Kind==KCatch)
            {
                // From wherever he stands on this tree: into the crotch, out along the next limb, under the first grip.
                const int32 Tree=Chain.From;const auto& T=Data.Trees[Tree];
                if(Trial.bReset)
                {
                    C->ResetAtLocation(G+T.Fork+FVector(0,0,40));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);
                    C->SetActorRotation(T.Limbs[Chain.LimbA].Rotation());C->Recenter();
                }
                const FVector P=C->GetActorLocation()-G;
                if(FVector::Dist2D(P,T.Trunk)<160 || Run->Trial==1 || Trial.bReset) {}
                else
                {
                    // Back along whichever limb he stands on.
                    int32 On=0;float Best=-2;
                    for(int32 L=0;L<T.Limbs.Num();++L) {const float Dot=FVector::DotProduct((P-T.Trunk).GetSafeNormal2D(),T.Limbs[L]);if(Dot>Best){Best=Dot;On=L;}}
                    Run->Path.Add(ChultCanopyLimbTop(Tree,On,150));
                }
                if(Run->Trial!=1) Run->Path.Add(T.Fork);
                Run->Path.Add(ChultCanopyLimbTop(Tree,Chain.LimbA,180));Run->Path.Add(ChultCanopyLimbTop(Tree,Chain.LimbA,FirstGrip));
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
            if(Trial.bReset && Time<.6f) {}
            else if(Run->Waypoint<Run->Path.Num())
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
        case KDeath:
        {
            // A bite that empties his sanity once he stands there; then his astral return.
            C->SetTestStick(FVector2D::ZeroVector);
            if(Run->Phase==0 && Time>.8f && Movement->IsMovingOnGround()) {C->TakeBite(C->GetActorLocation()+C->GetActorForwardVector()*20,99);Run->Phase=1;Run->LastJump=Time;}
            if(Run->Phase==1 && C->IsAstral()) Run->Phase=2;
            const FVector Want=Trial.Chain<2?ChultCanopyRespawn():AChultLanding::Entrance();
            const FVector At=C->GetActorLocation();
            Pass=Run->Phase==2 && !C->IsAstral() && Movement->IsMovingOnGround() && FVector::Dist2D(At,Want)<30 && FMath::Abs(At.Z-Want.Z)<15
                && (Trial.Chain==2 || FMath::Abs(FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw,ChultCanopyRespawnYaw()))<20);
            Done=Pass && Time-Run->LastJump>4.f;
            Limit=20;
            break;
        }
        case KSpur:
        {
            // 0: walk out and jump to the first grip; 1: out to the tip, stick let go;
            // 2: back (a jump at the tip with nothing ahead returns); 3: step down.
            const int32 First=GripIndex(G+Spur->Paws[0]),Tip=GripIndex(G+Spur->Paws.Last());
            const float Top=ChultCanopyLimbTop(Spur->Tree,Spur->Limb,FVector::Dist2D(Spur->Paws[0],Data.Trees[Spur->Tree].Trunk)).Z;
            if(Run->Phase==0)
            {
                if(Run->Waypoint<Run->Path.Num())
                {
                    const FVector To=Run->Path[Run->Waypoint];
                    if(Time>.6f && FVector::Dist2D(P,To)<(Run->Waypoint==Run->Path.Num()-1?14.f:24.f)) ++Run->Waypoint;
                    else if(Time>.6f) Move(To-P);
                }
                else
                {
                    C->SetTestStick(FVector2D::ZeroVector);
                    if(bGround && Run->Jumps==0 && Movement->Velocity.Size2D()<30) Press();
                }
                if(C->IsSwinging() && C->GetSwingGrip()==First) Run->Phase=1;
            }
            else
            {
                C->SetTestStick(FVector2D::ZeroVector);
                if(Run->Phase==1 && C->IsSwinging() && C->GetSwingGrip()==Tip) Run->Phase=2;
                else if(Run->Phase==2 && C->IsSwinging() && C->GetSwingGrip()==First) Run->Phase=3;
                if(C->IsSwinging() && Time-Run->LastJump>.45f) Press();
            }
            Pass=Run->Phase==3 && bGround && P.Z>Top-30 && C->GetSwingLeaps()-Run->Leaps==2*(Spur->Paws.Num()-1);
            Done=(Pass && Time-Run->LastJump>.6f) || (Run->Phase>0 && !C->IsSwinging() && !C->IsSwingLeaping() && Run->Phase<3);
            Limit=40;
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
            const auto& T=Data.Trees[Trial.Chain];
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
            FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("T%02d_%s_%03d.png"),Run->Trial,*Trial.Name,Run->ShotIndex++),false,false);
            Run->NextShot+=Trial.Kind==KChain?.5f:1.f;
        }
        const bool Fell=Trial.Kind!=KDeath && (C->GetFallDeaths()>Run->Deaths || C->IsAstral());
        if(!Done && !Fell && Time<Limit) return;
        Pass=Pass && !Fell;Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CANOPY_TRIAL trial=%d name=%s pass=%d time=%.2f jumps=%d gait=%s grip=%d catches=%d leaps=%d ladders=%d pullups=%d at=%s"),
            Run->Trial,*Trial.Name,Pass,Time,Run->Jumps,C->GetGaitName(),C->GetSwingGrip(),C->GetSwingCatches()-Run->Catches,C->GetSwingLeaps()-Run->Leaps,
            C->GetLadderMounts()-Run->Ladders,C->GetPullUps()-Run->PullUps,*P.ToString());
        C->SetTestStick(FVector2D::ZeroVector);
        // A failure leaves nothing to carry on from: stop there.
        if(!Pass) {Run->Failures+=TrialCount-1-Run->Trial;Run->Trial=TrialCount;}
        else ++Run->Trial;
        Run->Start=-1;Run->ShotIndex=0;Run->NextShot=0;
        if(Run->Trial>=TrialCount)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CANOPY_TEST_COMPLETE failures=%d trials=%d static_failures=%d loop=1 reverse=1 let_go=1 lianas=2 slab=1 spurs=%d"),Run->Failures,TrialCount,Run->Static,Canopy().Spurs.Num());
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
