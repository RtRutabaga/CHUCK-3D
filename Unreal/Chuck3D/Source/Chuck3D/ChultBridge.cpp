#include "ChultBridge.h"
#include "ChultCanyon.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ChuckClipData.h"
#include "DockGameMode.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Algo/Reverse.h"
#include "Misc/ScopeExit.h"
#include "UnrealClient.h"

namespace
{
    // The bridge spans post to post along the canyon frame; the deck sags a
    // metre at mid-span. A plank's top lies on DeckZ, its ends on two foot ropes.
    constexpr float PostS0=-240,PostS1=4800,PostT=84;
    constexpr float FootT=66,FootR=2.f,RailT=82,RailR=2.2f;
    constexpr float Pitch=30,FirstSlot=-205;     // a whole bridge's plank spacing
    constexpr float PlankHalf=75;                // across, end to end 150
    constexpr float NearGround=350;              // bank height at T=0 (ChultCanyonGroundZ)
    // One rail is snapped here (the +T side), its two ends hanging.
    constexpr float RailBreak0=2290,RailBreak1=2470;
    FVector Origin() {return ChultCanyonFrame(0,0,0);}
    FVector AlongDir() {return ChultCanyonFrame(1,0,0)-Origin();}
    FVector AcrossDir() {return ChultCanyonFrame(0,1,0)-Origin();}
    FVector ToFrame(const FVector& Local) {const FVector P=Local-Origin();return FVector(FVector::DotProduct(P,AlongDir()),FVector::DotProduct(P,AcrossDir()),P.Z);}
    float Span(float S) {return FMath::Clamp((S-PostS0)/(PostS1-PostS0),0.f,1.f);}
    float RailZ(float S) {return 590-92*FMath::Sin(Span(S)*PI);}
    float FootZ(float S) {return ChultBridgeDeckZ(S)-3.4f-FootR;}
    // The foot ropes come in from the posts' width to the planks' ends.
    float FootSide(float S) {const float D=FMath::Min(S-PostS0,PostS1-S);return FootT+(PostT-FootT)*(1-FMath::SmoothStep(0.f,90.f,D));}

    enum class ESlot : uint8 {Missing,Grip,Nub,Dangle};
    struct FSlot {float S=0;ESlot Kind=ESlot::Missing;bool bSplit=false;int32 Nubs=0;bool Remnant[2]={false,false};};
    struct FLayout {TArray<FSlot> Slots;TArray<FChultBridgePlank> Planks;};
    const FLayout& Layout()
    {
        static const FLayout L=[](){
            FLayout R;FRandomStream Rng(20261014);
            for(float S=FirstSlot;S<=PostS1-30;S+=Pitch) {FSlot Slot;Slot.S=S;R.Slots.Add(Slot);}
            // Surviving planks 4-7 slots apart (120-210 cm, inside his 240 cm
            // reach): the first over the near bank, the last over the far bank.
            TArray<int32> Grips={5};
            const int32 LastFrom=FMath::CeilToInt((4560-FirstSlot)/Pitch);
            const int32 Steps[]={4,5,5,5,6,6,6,7};
            while(true)
            {
                int32 Next=Grips.Last()+Steps[Rng.RandRange(0,UE_ARRAY_COUNT(Steps)-1)];
                if(Next>=LastFrom) {Grips.Add(FMath::Max(Grips.Last()+4,LastFrom));break;}
                Grips.Add(Next);
            }
            for(int32 I=0;I<Grips.Num();++I)
            {
                FSlot& Slot=R.Slots[Grips[I]];Slot.Kind=ESlot::Grip;
                Slot.bSplit=I>0 && I<Grips.Num()-1 && Rng.FRand()<.25f;
            }
            // Between them, each gap its own decay: bare (rope only), nubby (stubs
            // broken off at the ropes) or mixed; now and then one hangs by a lashing.
            int32 Dangles=0;
            for(int32 I=0;I+1<Grips.Num();++I)
            {
                const int32 Mode=Rng.RandRange(0,2);const float NubChance=Mode==0?.08f:Mode==1?.8f:.45f;
                for(int32 K=Grips[I]+1;K<Grips[I+1];++K)
                {
                    FSlot& Slot=R.Slots[K];
                    if(Mode>0 && Dangles<4 && Slot.S>400 && Slot.S<4100 && Rng.FRand()<.06f) {Slot.Kind=ESlot::Dangle;Slot.Nubs=Rng.FRand()<.5f?1:2;++Dangles;continue;}
                    if(Rng.FRand()<NubChance) {Slot.Kind=ESlot::Nub;const float U=Rng.FRand();Slot.Nubs=U<.35f?1:U<.7f?2:3;}
                    else for(int32 Side=0;Side<2;++Side) Slot.Remnant[Side]=Rng.FRand()<.3f;
                }
            }
            // Before the first and after the last: rotted away to the posts.
            for(FSlot& Slot:R.Slots) if(Slot.Kind==ESlot::Missing && (Slot.S<Grips[0]*Pitch+FirstSlot || Slot.S>Grips.Last()*Pitch+FirstSlot) && Rng.FRand()<.5f) {Slot.Kind=ESlot::Nub;Slot.Nubs=Rng.RandRange(1,3);}
            return R;
        }();
        return L;
    }

    // A plank's frame: across (its grain), along the bridge (its width, with the
    // deck's slope) and up, with a little random yaw and roll of its own.
    struct FBasis {FVector C,X,Y,Z;FVector P(float T,float S,float Z0) const {return C+X*T+Y*S+Z*Z0;}};
    FBasis SlotBasis(float S,FRandomStream& R)
    {
        const float Slope=-100*PI/(PostS1-PostS0)*FMath::Cos(Span(S)*PI);
        const FVector Y=(AlongDir()+FVector(0,0,Slope)).GetSafeNormal();
        FVector X=FQuat(FVector::UpVector,FMath::DegreesToRadians(R.FRandRange(-3.5f,3.5f))).RotateVector(AcrossDir());
        FVector Z=FVector::CrossProduct(X,Y).GetSafeNormal();if(Z.Z<0)Z=-Z;
        X=FVector::CrossProduct(Y,Z).GetSafeNormal();if(FVector::DotProduct(X,AcrossDir())<0)X=-X;
        const FQuat Roll(Y,FMath::DegreesToRadians(R.FRandRange(-2.f,2.f)));
        FBasis B;B.C=ChultCanyonFrame(S,0,ChultBridgeDeckZ(S)+R.FRandRange(-.6f,.6f));B.X=Roll.RotateVector(X);B.Y=Y;B.Z=Roll.RotateVector(Z);return B;
    }

    struct FMesh
    {
        TArray<FVector> V,N;TArray<int32> Idx;TArray<FVector2D> UV;TArray<FColor> Col;TArray<FProcMeshTangent> Tangent;
        int32 Vert(const FVector& P,const FVector& Normal)
        {
            FVector Side=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();if(Side.IsNearlyZero())Side=AlongDir();
            V.Add(P);N.Add(Normal);UV.Add(FVector2D(FVector::DotProduct(P,Side)/120,FVector::DotProduct(P,FVector::CrossProduct(Normal,Side))/120));Col.Add(FColor::White);Tangent.Add(FProcMeshTangent(Side,false));
            return V.Num()-1;
        }
        // UE renders clockwise fronts (as ChultCanyon.cpp): order each triangle
        // so the side facing Out is its front.
        void Tri(int32 A,int32 B,int32 D,const FVector& Out)
        {
            if(FVector::DotProduct(FVector::CrossProduct(V[B]-V[A],V[D]-V[A]),Out)>0) Idx.Append({A,D,B}); else Idx.Append({A,B,D});
        }
        // Eight corners, index bits: 1 across, 2 along, 4 up. Flat faces.
        void Hex(const FVector (&K)[8])
        {
            FVector Mid=FVector::ZeroVector;for(const FVector& P:K)Mid+=P/8;
            const int32 Faces[6][4]={{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
            for(const auto& F:Faces)
            {
                const FVector Centre=(K[F[0]]+K[F[1]]+K[F[2]]+K[F[3]])/4,Out=Centre-Mid;
                FVector Normal=FVector::CrossProduct(K[F[1]]-K[F[0]],K[F[3]]-K[F[0]]).GetSafeNormal();if(FVector::DotProduct(Normal,Out)<0)Normal=-Normal;
                int32 I[4];for(int32 J=0;J<4;++J)I[J]=Vert(K[F[J]],Normal);
                Tri(I[0],I[1],I[2],Normal);Tri(I[0],I[2],I[3],Normal);
            }
        }
        // A board piece in a basis: T0-T1 across, S0-S1 along, Z0-Z1 up.
        void Board(const FBasis& B,float T0,float T1,float S0,float S1,float Z0,float Z1)
        {
            FVector K[8];for(int32 I=0;I<8;++I)K[I]=B.P(I&1?T1:T0,I&2?S1:S0,I&4?Z1:Z0);Hex(K);
        }
        // A splinter: from a board's broken end at T, tapering to a point Length further on.
        void Splinter(const FBasis& B,float T,float Dir,float S0,float S1,float Z0,float Z1,float Length,float Droop)
        {
            const float Sm=(S0+S1)*.5f+(S1-S0)*.2f,Zm=Z1-(Z1-Z0)*.4f-Droop;
            FVector K[8];
            for(int32 I=0;I<8;++I)
            {
                if(I&1) K[I]=B.P(T+Dir*Length,Sm+(I&2?.4f:-.4f),Zm+(I&4?.3f:-.3f));
                else K[I]=B.P(T,I&2?S1:S0,I&4?Z1:Z0);
            }
            Hex(K);
        }
        // Smooth round rope through Points (parallel-transported rings).
        void Rope(const TArray<FVector>& Points,float Radius,int32 Sides,bool bClosed=false,bool bCaps=true)
        {
            const int32 Count=Points.Num();if(Count<2)return;
            FVector Ref;int32 First=V.Num();
            for(int32 I=0;I<Count;++I)
            {
                const FVector Prev=bClosed?Points[(I-1+Count)%Count]:Points[FMath::Max(I-1,0)],Next=bClosed?Points[(I+1)%Count]:Points[FMath::Min(I+1,Count-1)];
                const FVector Dir=(Next-Prev).GetSafeNormal();
                if(I==0) {Ref=FVector::CrossProduct(Dir,FVector::UpVector);if(Ref.SizeSquared()<.01f)Ref=FVector::CrossProduct(Dir,AlongDir());Ref.Normalize();}
                else {Ref=(Ref-Dir*FVector::DotProduct(Ref,Dir)).GetSafeNormal();}
                const FVector Bi=FVector::CrossProduct(Dir,Ref);
                for(int32 J=0;J<Sides;++J) {const float A=J*2*PI/Sides;const FVector Normal=Ref*FMath::Cos(A)+Bi*FMath::Sin(A);Vert(Points[I]+Normal*Radius,Normal);}
            }
            const int32 Rings=bClosed?Count:Count-1;
            for(int32 I=0;I<Rings;++I)for(int32 J=0;J<Sides;++J)
            {
                const int32 A=First+I*Sides+J,B=First+I*Sides+(J+1)%Sides,D=First+((I+1)%Count)*Sides+(J+1)%Sides,E=First+((I+1)%Count)*Sides+J;
                const FVector Out=N[A]+N[B];Tri(A,B,D,Out);Tri(A,D,E,Out);
            }
            if(bClosed||!bCaps)return;
            for(int32 End=0;End<2;++End)
            {
                const int32 Ring=End?First+(Count-1)*Sides:First;const FVector Out=End?(Points[Count-1]-Points[Count-2]).GetSafeNormal():(Points[0]-Points[1]).GetSafeNormal();
                const int32 Centre=Vert(End?Points[Count-1]:Points[0],Out);
                for(int32 J=0;J<Sides;++J)Tri(Centre,Ring+J,Ring+(J+1)%Sides,Out);
            }
        }
        // A main rope with its lay: resampled every 3 cm, a six-sided ring with
        // three raised strands, each ring turned 30 degrees on from the last,
        // so the strands wind round it every 12 cm.
        void LaidRope(const TArray<FVector>& Points,float Radius)
        {
            TArray<FVector> Fine;Fine.Add(Points[0]);
            for(int32 I=1;I<Points.Num();++I)
            {
                const float Len=FVector::Dist(Points[I-1],Points[I]);const int32 Steps=FMath::Max(1,FMath::RoundToInt(Len/3));
                for(int32 K=1;K<=Steps;++K)Fine.Add(FMath::Lerp(Points[I-1],Points[I],float(K)/Steps));
            }
            const int32 Count=Fine.Num(),First=V.Num();FVector Ref;
            for(int32 I=0;I<Count;++I)
            {
                const FVector Dir=(Fine[FMath::Min(I+1,Count-1)]-Fine[FMath::Max(I-1,0)]).GetSafeNormal();
                if(I==0) {Ref=FVector::CrossProduct(Dir,FVector::UpVector);if(Ref.SizeSquared()<.01f)Ref=FVector::CrossProduct(Dir,AlongDir());Ref.Normalize();}
                else Ref=(Ref-Dir*FVector::DotProduct(Ref,Dir)).GetSafeNormal();
                const FVector Bi=FVector::CrossProduct(Dir,Ref);
                for(int32 J=0;J<6;++J)
                {
                    const float A=(J*60+I*30)*PI/180;const FVector Out=Ref*FMath::Cos(A)+Bi*FMath::Sin(A);
                    // Strand crowns lean along the lay; the grooves between them turn in.
                    const FVector Normal=(J%2==0?Out:(Out*.8f-Dir*.35f)).GetSafeNormal();
                    Vert(Fine[I]+Out*Radius*(J%2==0?1.12f:.84f),Normal);
                }
            }
            for(int32 I=0;I+1<Count;++I)for(int32 J=0;J<6;++J)
            {
                const int32 A=First+I*6+J,B=First+I*6+(J+1)%6,D=First+(I+1)*6+(J+1)%6,E=First+(I+1)*6+J;
                const FVector Out=(V[A]+V[B])*.5f-Fine[I];Tri(A,B,D,Out);Tri(A,D,E,Out);
            }
        }
        // A broken rope end: its strands splayed and unravelling.
        void Fray(const FVector& End,const FVector& Dir,float Radius,FRandomStream& R)
        {
            for(int32 I=0;I<4;++I)
            {
                const FVector Spread=(Dir+R.GetUnitVector()*.45f).GetSafeNormal();const float Len=R.FRandRange(3.f,9.f);
                Rope({End-Dir*1.5f,End+Spread*Len*.5f+FVector(0,0,-Len*.1f),End+Spread*Len+FVector(0,0,-Len*.3f)},Radius*.33f,4);
            }
        }
        // Lashing at T: turns bound tight round the plank's end and the foot
        // rope under it, each a little skewed, so they read as wound cord.
        void Lash(const FBasis& B,float T,float S0,float S1,float Th,float Radius,int32 Turns,FRandomStream& R)
        {
            const float Rr=FootR+Radius,Zr=-Th-FootR,Lip=Radius;
            for(int32 Turn=0;Turn<Turns;++Turn)
            {
                const float Tt=T+(Turn-(Turns-1)*.5f)*Radius*2.1f,Skew=R.FRandRange(-1.2f,1.2f);
                TArray<FVector> Path={B.P(Tt-Skew,S0-Lip+1.5f,Lip),B.P(Tt+Skew,S1+Lip-1.5f,Lip),B.P(Tt+Skew,S1+Lip,Lip-1.5f),B.P(Tt+Skew*.6f,S1+Lip,-Th)};
                for(int32 K=1;K<=7;++K) {const float A=-PI*K/8;Path.Add(B.P(Tt+Skew*FMath::Cos(-A)*.6f,Rr*FMath::Cos(A),Zr+Rr*FMath::Sin(A)));}
                Path.Append({B.P(Tt-Skew*.6f,S0-Lip,-Th),B.P(Tt-Skew,S0-Lip,Lip-1.5f)});
                Rope(Path,Radius,4,true);
            }
        }
    };
    UProceduralMeshComponent* Finish(AActor* Owner,const TCHAR* Name,FMesh& M,const TCHAR* Material,bool bSolid,int32& Missing)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->RegisterComponent();
        Mesh->bUseComplexAsSimpleCollision=true;Mesh->CreateMeshSection(0,M.V,M.Idx,M.N,M.UV,M.Col,M.Tangent,bSolid);
        auto* Mat=LoadObject<UMaterialInterface>(nullptr,Material);Missing+=!Mat;Mesh->SetMaterial(0,Mat);
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        return Mesh;
    }
    float NubInner=1e3f;   // closest any broken piece comes to the centre line (layout check)
}

float ChultBridgeDeckZ(float S) {return 472-100*FMath::Sin(Span(S)*PI);}
const TArray<FChultBridgePlank>& ChultBridgePlanks()
{
    // Built with the meshes (a split plank's grip is on its surviving half);
    // before that, the layout alone.
    static TArray<FChultBridgePlank> Planks;
    if(Planks.Num()==0)
        for(const FSlot& Slot:Layout().Slots) if(Slot.Kind==ESlot::Grip)
        {
            FChultBridgePlank P;P.Name=FName(*FString::Printf(TEXT("ChultBridgePlank%02d"),Planks.Num()));P.S=Slot.S;P.bSplit=Slot.bSplit;
            P.Grip=ChultCanyonFrame(Slot.S,0,ChultBridgeDeckZ(Slot.S)-2.3f);Planks.Add(P);
        }
    return Planks;
}
FVector ChultBridgeNearStand() {const float S=ChultBridgePlanks()[0].S-4;return ChultCanyonFrame(S,0,ChultCanyonGroundZ(ChultCanyonFrame(S,0)));}
FVector ChultBridgeFarStand() {const float S=ChultBridgePlanks().Last().S+4;return ChultCanyonFrame(S,0,ChultCanyonGroundZ(ChultCanyonFrame(S,0)));}

void BuildChultBridge(AActor* Owner)
{
    FMesh Boards,Ropes,Posts;
    int32 Grips=0,Split=0,Nubs=0,Missing=0,Dangles=0,Lashings=0,Remnants=0;
    const auto& Slots=Layout().Slots;
    // Planks may only be built once; the grips are recorded from the boards.
    TArray<FChultBridgePlank>& Planks=const_cast<TArray<FChultBridgePlank>&>(ChultBridgePlanks());
    NubInner=1e3f;
    for(int32 K=0;K<Slots.Num();++K)
    {
        const FSlot& Slot=Slots[K];FRandomStream R(1000+K);
        const FBasis B=SlotBasis(Slot.S,R);
        const float Half=PlankHalf+R.FRandRange(-3.f,3.f),Depth=R.FRandRange(17.f,22.f),Thick=R.FRandRange(3.f,3.8f);
        // Two or three strips along the grain, so ends and breaks are ragged.
        const int32 Strips=R.RandRange(2,3);TArray<float> Cuts={-Depth*.5f};
        for(int32 I=1;I<Strips;++I)Cuts.Add(-Depth*.5f+Depth*I/Strips+R.FRandRange(-1.5f,1.5f));Cuts.Add(Depth*.5f);
        auto LashEnd=[&](float Side,float S0,float S1,float Th)
        {
            Ropes.Lash(B,Side*FootT,S0,S1,Th,.75f,3,R);++Lashings;
        };
        if(Slot.Kind==ESlot::Grip)
        {
            ++Grips;
            FChultBridgePlank& Plank=Planks[Grips-1];
            if(!Slot.bSplit)
            {
                for(int32 I=0;I<Strips;++I) {const float Th=Thick+R.FRandRange(-.4f,.25f);Boards.Board(B,-Half+R.FRandRange(-2.5f,2.5f),Half+R.FRandRange(-2.5f,2.5f),Cuts[I],Cuts[I+1],-Th,R.FRandRange(-.3f,.3f));}
                Plank.Grip=B.P(0,0,-Thick*.5f);
                for(float Side:{-1.f,1.f})LashEnd(Side,-Depth*.5f,Depth*.5f,Thick);
            }
            else
            {
                // Split along its grain: one thin half spans; the other half broke off
                // at the ropes, leaving stubs. The torn edge wanders.
                ++Split;
                const float Width=R.FRandRange(7.f,9.5f);const bool Low=R.FRand()<.5f;
                const float Outer=Low?-Depth*.5f:Depth*.5f,Dir=Low?1.f:-1.f;
                for(int32 I=0;I<4;++I)
                {
                    const float T0=-Half+2*Half*I/4,T1=-Half+2*Half*(I+1)/4,W=Width+R.FRandRange(-1.8f,1.8f);
                    Boards.Board(B,T0,T1,FMath::Min(Outer,Outer+Dir*W),FMath::Max(Outer,Outer+Dir*W),-Thick,0);
                }
                Plank.Grip=B.P(0,Outer+Dir*Width*.5f,-Thick*.5f);
                for(float Side:{-1.f,1.f})
                {
                    const float Len=R.FRandRange(15.f,24.f),S0=FMath::Min(Outer+Dir*Width,-Outer),S1=FMath::Max(Outer+Dir*Width,-Outer);
                    Boards.Board(B,Side>0?Half-Len:-Half,Side>0?Half:-Half+Len,S0,S1,-Thick+.3f,-.2f);
                    NubInner=FMath::Min(NubInner,Half-Len);
                    LashEnd(Side,-Depth*.5f,Depth*.5f,Thick);
                }
            }
            Plank.S=Slot.S;Plank.bSplit=Slot.bSplit;
            auto* Marker=NewObject<USceneComponent>(Owner,Plank.Name);Marker->SetupAttachment(Owner->GetRootComponent());
            Marker->SetWorldLocationAndRotation(AChultLanding::FloorOrigin()+Plank.Grip,AlongDir().Rotation());Marker->ComponentTags.Add(TEXT("ChultBridgeGrip"));Marker->RegisterComponent();
            FChuckSwingGrip Swing;Swing.Grip=AChultLanding::FloorOrigin()+Plank.Grip;Swing.Along=AlongDir();AddChuckSwingGrip(Swing);
        }
        else if(Slot.Kind==ESlot::Nub)
        {
            // Broken off near the ropes: a stub at one end, the other, or both,
            // every strip snapped at its own length, most ending in a splinter.
            ++Nubs;
            for(float Side:{-1.f,1.f})
            {
                if(!(Slot.Nubs&(Side<0?1:2)))continue;
                for(int32 I=0;I<Strips;++I)
                {
                    const float Len=R.FRandRange(16.f,30.f),Th=Thick+R.FRandRange(-.4f,.25f),End=Side*(Half-Len);
                    Boards.Board(B,Side<0?-Half+R.FRandRange(-2.f,2.f):End,Side<0?End:Half+R.FRandRange(-2.f,2.f),Cuts[I],Cuts[I+1],-Th,0);
                    float Reach=Half-Len;
                    if(R.FRand()<.65f) {const float L=R.FRandRange(4.f,10.f);Boards.Splinter(B,End,-Side,Cuts[I],Cuts[I+1],-Th,0,L,R.FRandRange(0.f,1.5f));Reach-=L;}
                    NubInner=FMath::Min(NubInner,Reach);
                }
                LashEnd(Side,-Depth*.5f,Depth*.5f,Thick);
            }
        }
        else if(Slot.Kind==ESlot::Dangle)
        {
            // Torn from one rope, hanging by the other's lashing.
            ++Dangles;
            const float Side=Slot.Nubs==1?-1.f:1.f,Swing=FMath::DegreesToRadians(R.FRandRange(66.f,84.f));
            const FVector Pivot=B.P(Side*FootT,0,-Thick-FootR),Out=-Side*B.X;
            FBasis H;H.C=Pivot;H.X=Out*FMath::Cos(Swing)-B.Z*FMath::Sin(Swing);H.Y=B.Y;H.Z=Out*FMath::Sin(Swing)+B.Z*FMath::Cos(Swing);
            const float Length=2*Half*R.FRandRange(.45f,.95f);
            for(int32 I=0;I<Strips;++I) Boards.Board(H,-(Half-FootT)+3,Length-(Half-FootT),Cuts[I],Cuts[I+1],FootR,FootR+Thick);
            Ropes.Rope({Pivot+B.Z*(FootR+.7f),Pivot+B.Y*(Depth*.3f),Pivot-B.Z*(FootR+.7f),Pivot-B.Y*(Depth*.3f)},.6f,4,true);++Lashings;
            NubInner=FMath::Min(NubInner,FootT-85*FMath::Cos(Swing));   // within his hanging depth
        }
        else
        {
            ++Missing;
            // What is left of a lost plank's lashing: a loop round the rope, its tail hanging.
            for(int32 Side=0;Side<2;++Side)
            {
                if(!Slot.Remnant[Side])continue;
                ++Remnants;
                const float Sg=Side?1.f:-1.f;const FVector At=ChultCanyonFrame(Slot.S,Sg*FootSide(Slot.S),FootZ(Slot.S));
                const FVector Up=FVector::UpVector,Out=AcrossDir()*Sg;const float Rr=FootR+.7f;
                Ropes.Rope({At+Up*Rr,At+Out*Rr,At-Up*Rr,At-Out*Rr},.6f,4,true);
                const FVector Tail=At-Up*Rr+Out*.5f;const float Drop=R.FRandRange(4.f,11.f);
                Ropes.Rope({Tail,Tail+FVector(0,0,-Drop*.5f)+AlongDir()*R.FRandRange(-1.f,1.f),Tail+FVector(0,0,-Drop)+AlongDir()*R.FRandRange(-2.f,2.f)},.55f,4);
                Ropes.Fray(Tail+FVector(0,0,-Drop),FVector(0,0,-1),.55f,R);
            }
        }
    }
    FRandomStream R(20261015);
    // Foot ropes: continuous, post to post, under every plank end.
    for(float Sg:{-1.f,1.f})
    {
        TArray<FVector> Points;
        for(float S=PostS0;S<=PostS1+1;S+=20) Points.Add(ChultCanyonFrame(S,Sg*FootSide(S),FootZ(S)));
        Ropes.LaidRope(Points,FootR);
    }
    // Hand ropes; the +T one snapped mid-span, both ends hanging and frayed.
    for(float Sg:{-1.f,1.f})
    {
        auto Rail=[&](float S0,float S1){TArray<FVector> P;const int32 Count=FMath::CeilToInt((S1-S0)/20);for(int32 I=0;I<=Count;++I){const float S=S0+(S1-S0)*I/Count;P.Add(ChultCanyonFrame(S,Sg*RailT,RailZ(S)));}return P;};
        if(Sg<0) {Ropes.LaidRope(Rail(PostS0,PostS1),RailR);continue;}
        for(int32 Piece=0;Piece<2;++Piece)
        {
            TArray<FVector> P=Piece==0?Rail(PostS0,RailBreak0):Rail(RailBreak1,PostS1);
            if(Piece==1)Algo::Reverse(P);
            const float S=Piece==0?RailBreak0:RailBreak1,Dir=Piece==0?1.f:-1.f;const float Hang=Piece==0?135.f:95.f;
            const FVector Break=ChultCanyonFrame(S,RailT,RailZ(S));
            for(int32 I=1;I<=8;++I) {const float U=I/8.f;P.Add(Break+AlongDir()*Dir*(14*FMath::Sin(U*PI*.5f))+AcrossDir()*(-6*U)+FVector(0,0,-Hang*FMath::Pow(U,.85f)));}
            Ropes.LaidRope(P,RailR);
            Ropes.Fray(P.Last(),(P.Last()-P[P.Num()-2]).GetSafeNormal(),RailR,R);
        }
    }
    // Suspenders, hand rope to foot rope; a few snapped, a stub left hanging.
    int32 Suspenders=0,Snapped=0;
    for(float S=-120;S<=PostS1-120;S+=120)for(float Sg:{-1.f,1.f})
    {
        if(Sg>0 && S>RailBreak0-60 && S<RailBreak1+60)continue;
        const FVector Top=ChultCanyonFrame(S,Sg*RailT,RailZ(S)),Foot=ChultCanyonFrame(S,Sg*FootSide(S),FootZ(S)+FootR);
        Ropes.Rope({Top+FVector(0,0,RailR+.6f)+AlongDir()*1.5f,Top+FVector(0,0,RailR+.6f)-AlongDir()*1.5f},.9f,5,false);
        if(R.FRand()<.14f)
        {
            ++Snapped;const float Len=R.FRandRange(15.f,45.f);
            const FVector End=Top+FVector(0,0,-Len)+AcrossDir()*Sg*R.FRandRange(-2.f,2.f);
            Ropes.Rope({Top,(Top+End)*.5f+AlongDir()*R.FRandRange(-1.5f,1.5f),End},1.f,5);Ropes.Fray(End,FVector(0,0,-1),1.f,R);
            continue;
        }
        ++Suspenders;
        Ropes.Rope({Top,(Top+Foot)*.5f+AcrossDir()*Sg*2.f+AlongDir()*R.FRandRange(-2.f,2.f),Foot},1.f,5);
        // Tied round the foot rope.
        const FVector At=ChultCanyonFrame(S,Sg*FootSide(S),FootZ(S));const float Rr=FootR+.7f;
        Ropes.Rope({At+FVector(0,0,Rr),At+AcrossDir()*Sg*Rr,At-FVector(0,0,Rr),At-AcrossDir()*Sg*Rr},.6f,4,true);
    }
    // Solid anchor posts, the ropes wound round them.
    for(float S:{PostS0,PostS1})for(float Sg:{-1.f,1.f})
    {
        const FVector Base=ChultCanyonFrame(S,Sg*PostT,0);const float Ground=ChultCanyonGroundZ(Base);
        FBasis B;B.C=Base+FVector(0,0,Ground);B.X=AcrossDir();B.Y=AlongDir();B.Z=FVector::UpVector;
        const float Lean=Sg*R.FRandRange(.5f,1.5f);
        FVector K[8];for(int32 I=0;I<8;++I){const float Z=I&4?265.f:-50.f;K[I]=B.P((I&1?11.f:-11.f)+(Z>0?Lean*Z/265:0),(I&2?11.f:-11.f),Z);}
        Posts.Hex(K);
        for(float Z:{FootZ(S)-Ground,RailZ(S)-Ground})for(int32 Turn=0;Turn<3;++Turn)
        {
            const float Zt=Z+Turn*2.6f-2.6f,W=13.4f;
            Ropes.Rope({B.P(-W,-W,Zt),B.P(W,-W,Zt),B.P(W,W,Zt),B.P(-W,W,Zt)},1.4f,5,true);
        }
    }
    int32 MissingAssets=0;
    const TCHAR* Wood=TEXT("/Game/Art/Materials/M_Wood.M_Wood"),*Hemp=TEXT("/Game/Art/Materials/M_Canvas.M_Canvas");
    Finish(Owner,TEXT("ChultBridgePlanks"),Boards,Wood,false,MissingAssets);
    Finish(Owner,TEXT("ChultBridgeRopes"),Ropes,Hemp,false,MissingAssets);
    Finish(Owner,TEXT("ChultBridgePosts"),Posts,Wood,true,MissingAssets);
    float MinStep=1e3f,MaxStep=0;
    for(int32 I=1;I<Planks.Num();++I){const float D=FVector::Dist2D(Planks[I-1].Grip,Planks[I].Grip);MinStep=FMath::Min(MinStep,D);MaxStep=FMath::Max(MaxStep,D);}
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BRIDGE slots=%d grips=%d split=%d nubs=%d missing=%d dangling=%d remnants=%d lashings=%d suspenders=%d snapped=%d rail_breaks=1 min_step=%.0f max_step=%.0f nub_inner=%.0f missing_assets=%d deck_collision=0 rope_collision=0 triangles=%d"),
        Slots.Num(),Grips,Split,Nubs,Missing,Dangles,Remnants,Lashings,Suspenders,Snapped,MinStep,MaxStep,NubInner,MissingAssets,(Boards.Idx.Num()+Ropes.Idx.Num()+Posts.Idx.Num())/3);
}

int32 CheckChultBridge(AActor* Owner,AChuckCharacter* C)
{
    int32 Failures=0,Registered=0,Bodies=0,Leaps=0,OverVoid=0,NoDeck=0;
    UWorld* W=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();const auto& Planks=ChultBridgePlanks();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultBridgeCheck),false,C);
    const float Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Drop=ChuckClipData::SwingGripZ-Half;
    const auto Body=FCollisionShape::MakeCapsule(Radius-1,Half-1);
    const auto& Swing=GetChuckSwingGrips();
    for(int32 I=0;I<Planks.Num();++I)
    {
        const int32 Before=Failures;ON_SCOPE_EXIT{if(Failures>Before)UE_LOG(LogTemp,Warning,TEXT("CHUCK_CHULT_BRIDGE_CHECK_FAIL plank=%d s=%.0f failures=%d"),I,Planks[I].S,Failures-Before);};
        const FVector Grip=G+Planks[I].Grip;
        Registered+=Swing.ContainsByPredicate([&](const FChuckSwingGrip& S){return FVector::Dist(S.Grip,Grip)<1;});
        auto* Marker=FindObjectFast<USceneComponent>(Owner,Planks[I].Name);Failures+=!Marker||FVector::Dist(Marker->GetComponentLocation(),Grip)>.1f;
        // Hanging there: clear of everything solid.
        FHitResult Hit;const FVector Hang=Grip-FVector(0,0,Drop);
        Failures+=W->SweepSingleByChannel(Hit,Hang,Hang+AlongDir()*2,FQuat::Identity,ECC_Pawn,Body,Q);++Bodies;
        // Nothing to stand on: no deck under or at his paws.
        Failures+=W->LineTraceSingleByChannel(Hit,Grip+FVector(0,0,60),Grip-FVector(0,0,25),ECC_Pawn,Q)||W->LineTraceSingleByChannel(Hit,Grip+FVector(0,0,60),Grip-FVector(0,0,25),ECC_Visibility,Q);++NoDeck;
        // The ends stand over the banks at a standing hop's reach; between, only the gorge.
        const bool Floor=W->LineTraceSingleByChannel(Hit,Grip,Grip-FVector(0,0,400),ECC_Visibility,Q);
        if(I==0) Failures+=!Floor||Grip.Z-Hit.ImpactPoint.Z<Drop+Half+.5f||Grip.Z-Hit.ImpactPoint.Z>125;
        else if(I==Planks.Num()-1) Failures+=!Floor||Grip.Z-Hit.ImpactPoint.Z<Drop+Half+.5f||Grip.Z-Hit.ImpactPoint.Z>165;
        else {Failures+=Floor && Grip.Z-Hit.ImpactPoint.Z<Drop+Half+20;OverVoid+=!Floor;}
        if(I)
        {
            const FVector From=G+Planks[I-1].Grip;const float D=FVector::Dist2D(From,Grip);
            Failures+=D<100||D>215||D>AChuckCharacter::SwingLeapRange||FMath::Abs(Grip.Z-From.Z)>30;
            Failures+=W->LineTraceSingleByChannel(Hit,From,Grip,ECC_Visibility,Q)||W->SweepSingleByChannel(Hit,From-FVector(0,0,Drop),Grip-FVector(0,0,Drop),FQuat::Identity,ECC_Pawn,Body,Q);++Leaps;
        }
    }
    Failures+=Registered!=Planks.Num();
    for(const TCHAR* Name:{TEXT("ChultBridgePlanks"),TEXT("ChultBridgeRopes")}){auto* Mesh=FindObjectFast<UProceduralMeshComponent>(Owner,FName(Name));Failures+=!Mesh||Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;}
    // Broken pieces stay out by the ropes, clear of his paws and body on the centre line.
    Failures+=NubInner<30;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_BRIDGE_CHECK failures=%d grips=%d registered=%d hang_bodies=%d leaps=%d over_void=%d no_deck=%d nub_inner=%.0f active_crossing=1"),Failures,Planks.Num(),Registered,Bodies,Leaps,OverVoid,NoDeck,NubInner);
    return Failures;
}

namespace
{
    FVector BridgeSpot(float S) {const FVector P=ChultCanyonFrame(S,0);return AChultLanding::FloorOrigin()+P+FVector(0,0,ChultCanyonGroundZ(P)+34.65f);}
}
bool ChultBridgeRespawn(const FVector& DeathWorld,FVector& OutWorld)
{
    // Fallen into the gorge (anywhere along it): back on the bank he set out
    // from - by the far lip, the far bank; otherwise the near one, by the bridge.
    const FVector Local=DeathWorld-AChultLanding::FloorOrigin();
    if(!InChultCanyon(Local))return false;
    const FVector F=ToFrame(Local);
    if(F.X<-60||F.X>4560)return false;
    OutWorld=BridgeSpot(F.X>=4450?4820.f:-300.f);
    return true;
}
bool IsChultBridgeRespawn(const FVector& World) {return FVector::Dist(World,BridgeSpot(-300))<1||FVector::Dist(World,BridgeSpot(4820))<1;}
float ChultBridgeRespawnYaw() {return AlongDir().Rotation().Yaw;}
FVector ChultBridgeSpawn(bool bFar) {return BridgeSpot(bFar?4820.f:-300.f);}
bool InChultBridgeMusicZone(const FVector& World,bool bWasInside)
{
    const FVector Local=World-AChultLanding::FloorOrigin();
    if(!InChultCanyon(Local))return false;
    return ToFrame(Local).X>(bWasInside?-220.f:-40.f);
}

void StartChultBridgeReview(AActor* Owner,AChuckCharacter* C)
{
    UWorld* World=Owner->GetWorld();auto* PC=Cast<APlayerController>(C->GetController());
    C->DisableInput(PC);C->SetLookLocked(true);PC->SetViewTarget(C);
    const bool bCapture=FParse::Param(FCommandLine::Get(),TEXT("ChuckBridgeCapture"));
    const int32 Static=CheckChultBridge(Owner,C);
    enum EKind {KCatch,KCross,KStepDown,KBack,KLetGo,KNoStand};
    struct FTrial {EKind Kind;const TCHAR* Name;};
    const TArray<FTrial> Trials={{KCatch,TEXT("catch_first")},{KCross,TEXT("cross")},{KStepDown,TEXT("step_down_far")},{KBack,TEXT("cross_back")},{KLetGo,TEXT("let_go_fall")},{KNoStand,TEXT("no_standing")}};
    struct FRun
    {
        int32 Trial=0,Failures=0,Phase=0,Jumps=0,Leaps=0,Deaths=0,Shot=0,ShotIndex=0;
        float Start=-1,LastJump=-1,NextShot=0,Tick=-1;bool bMusicFar=false,bMusicBack=false;
        ACameraActor* Camera=nullptr;FTimerHandle Timer;
    };
    auto Run=MakeShared<FRun>();Run->Failures=Static;
    const FVector G=AChultLanding::FloorOrigin();
    auto GripIndex=[G](int32 Plank){const FVector At=G+ChultBridgePlanks()[Plank].Grip;return GetChuckSwingGrips().IndexOfByPredicate([&](const FChuckSwingGrip& S){return FVector::Dist(S.Grip,At)<1;});};
    World->GetTimerManager().SetTimer(Run->Timer,[World,C,PC,Run,G,Trials,GripIndex,bCapture,Static]()
    {
        if(Run->Trial>=Trials.Num())return;
        const float Now=World->GetTimeSeconds();if(Now==Run->Tick)return;Run->Tick=Now;
        auto* Movement=C->GetCharacterMovement();const auto& Planks=ChultBridgePlanks();const int32 Last=Planks.Num()-1;
        auto* Mode=World->GetAuthGameMode<ADockGameMode>();const int32 Music=Mode?Mode->GetMusicRegion():-1;
        if(bCapture && !Run->Camera) {Run->Camera=World->SpawnActor<ACameraActor>();Run->Camera->GetCameraComponent()->SetFieldOfView(65);}
        // Review stills: from the near bank, from the side, from below, a close look.
        if(bCapture && Run->Shot<10)
        {
            const FVector Views[][2]={{ChultCanyonFrame(-520,40,520),ChultCanyonFrame(900,0,420)},{ChultCanyonFrame(2250,-1700,560),ChultCanyonFrame(2250,0,380)},
                {ChultCanyonFrame(1500,-260,180),ChultCanyonFrame(1750,0,390)},{ChultCanyonFrame(Planks[3].S-120,-170,470),ChultCanyonFrame(Planks[3].S+60,0,405)},
                {ChultCanyonFrame(Planks[8].S-90,140,520),ChultCanyonFrame(Planks[8].S+150,0,370)}};
            const int32 View=Run->Shot/2;
            if(Run->Start<0) {Run->Start=Now;C->ResetAtLocation(BridgeSpot(-700));Run->Camera->SetActorLocationAndRotation(G+Views[View][0],(Views[View][1]-Views[View][0]).Rotation());PC->SetViewTarget(Run->Camera);}
            else if(Run->Shot%2==0 && Now-Run->Start>1.5f) ++Run->Shot;
            else if(Run->Shot%2==1)
            {
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Bridge");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View_%d.png"),View),false,false);
                ++Run->Shot;Run->Start=-1;
            }
            return;
        }
        // (A frame for the last still to be taken before the view goes back to him.)
        if(bCapture && Run->Shot==10) {++Run->Shot;return;}
        if(bCapture && Run->Shot==11) {++Run->Shot;Run->Start=-1;PC->SetViewTarget(C);}
        const FTrial& Trial=Trials[Run->Trial];
        auto Place=[&](const FVector& Local,float Yaw)
        {
            C->ResetAtLocation(G+Local+FVector(0,0,34.65f));Movement->StopMovementImmediately();
            C->SetActorRotation(FRotator(0,Yaw,0));C->Recenter();
        };
        const float Yaw=ChultBridgeRespawnYaw();
        if(Run->Start<0)
        {
            Run->Start=Now;Run->Phase=0;Run->Jumps=0;Run->Leaps=C->GetSwingLeaps();Run->Deaths=C->GetFallDeaths();Run->LastJump=0;
            if(Trial.Kind==KCatch||Trial.Kind==KLetGo) Place(ChultBridgeNearStand(),Yaw);
            else if(Trial.Kind==KBack) Place(ChultBridgeFarStand(),Yaw+180);
            else if(Trial.Kind==KNoStand)
            {
                // Dropped onto a surviving plank from above, as if he'd landed on the deck.
                const FVector Top=Planks[6].Grip+FVector(0,0,8);
                C->ResetAtLocation(G+Top+FVector(0,0,34.65f));Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Falling);
            }
        }
        const float Time=Now-Run->Start;const FVector P=C->GetActorLocation()-G;const FVector F=ToFrame(P);
        auto Press=[&](){C->JumpPressed();Run->LastJump=Time;++Run->Jumps;};
        const bool bGround=Movement->IsMovingOnGround();
        // A hop from standing that left him standing (the press is intermittently lost;
        // seen 2026-10-19 in the old launcher build too): press again, as a player would.
        auto Retry=[&](){return Run->Jumps>0 && Run->Jumps<4 && bGround && !C->IsSwinging() && Time-Run->LastJump>1.f;};
        const int32 Grip=C->GetSwingGrip();
        bool Pass=false,Done=false;float Limit=30;
        C->SetTestStick(FVector2D::ZeroVector);
        switch(Trial.Kind)
        {
        case KCatch:
            if((Run->Jumps==0 && Time>.6f && bGround) || Retry()) Press();
            Pass=C->IsSwinging() && Grip==GripIndex(0);
            Done=Pass && Time-Run->LastJump>.5f;Limit=6;
            break;
        case KCross:
            // Stick let go: on along the bridge plank to plank, a jump at each.
            if(C->IsSwinging() && Grip!=GripIndex(Last) && Time-Run->LastJump>.45f) Press();
            Run->bMusicFar|=Music==4;
            Pass=C->IsSwinging() && Grip==GripIndex(Last) && C->GetSwingLeaps()-Run->Leaps==Last && Run->bMusicFar;
            Done=(Pass && Time-Run->LastJump>.6f) || (!C->IsSwinging() && !C->IsSwingLeaping());Limit=40;
            break;
        case KStepDown:
            if(Run->Jumps==0 && C->IsSwinging() && Time>.2f) Press();
            Pass=Run->Jumps>0 && bGround && F.X>4500 && FMath::Abs(P.Z-34.65f-ChultCanyonGroundZ(P))<20 && Music==4;
            Done=(Pass && Time-Run->LastJump>.6f) || (Run->Jumps>0 && C->IsSwinging() && Time-Run->LastJump>.8f);Limit=6;
            break;
        case KBack:
            // Facing home: caught, then steered back with the stick, then stepped down.
            if(Run->Phase==0) {if((Run->Jumps==0 && Time>.6f && bGround) || Retry()) Press();if(C->IsSwinging() && Grip==GripIndex(Last)) Run->Phase=1;}
            else if(Run->Phase==1)
            {
                C->SetTestStickWorld(-AlongDir());
                if(C->IsSwinging() && Grip==GripIndex(0)) {Run->Phase=2;C->SetTestStick(FVector2D::ZeroVector);}
                else if(C->IsSwinging() && Time-Run->LastJump>.45f) Press();
            }
            else if(Run->Phase==2 && C->IsSwinging() && Time-Run->LastJump>.5f) {Press();Run->Phase=3;}
            Pass=Run->Phase==3 && bGround && F.X<0 && FMath::Abs(P.Z-34.65f-ChultCanyonGroundZ(P))<20 && C->GetSwingLeaps()-Run->Leaps==Last;
            Done=(Pass && Time-Run->LastJump>.6f) || (Run->Phase>0 && !C->IsSwinging() && !C->IsSwingLeaping() && Run->Phase<3);Limit=45;
            break;
        case KLetGo:
            // Out four planks, then let go over the gorge: back on the near bank, its score back.
            if(Run->Phase==0) {if((Run->Jumps==0 && Time>.6f && bGround) || Retry()) Press();if(C->IsSwinging()) Run->Phase=1;}
            else if(Run->Phase==1)
            {
                if(C->IsSwinging() && Grip==GripIndex(4) && Time-Run->LastJump>.5f) {C->DodgeToward(FVector2D::ZeroVector);Run->Phase=2;Run->LastJump=Time;}
                else if(C->IsSwinging() && Time-Run->LastJump>.45f) Press();
            }
            Run->bMusicFar|=Music==4;
            if(Run->Phase==2 && C->GetFallDeaths()>Run->Deaths) Run->Phase=3;
            Pass=Run->Phase==3 && bGround && IsChultBridgeRespawn(C->GetActorLocation()) && FMath::Abs(FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw,ChultBridgeRespawnYaw()))<20 && Music==3;
            Run->bMusicBack|=Pass;
            Done=Pass && Time-Run->LastJump>2.f;Limit=25;
            break;
        case KNoStand:
            // He can't stand on it: through it, or hanging beneath; never on top.
            Pass=Time>1.2f && !bGround && P.Z<Planks[6].Grip.Z && FCString::Strcmp(C->GetGaitName(),TEXT("Hang"))!=0 && FCString::Strcmp(C->GetGaitName(),TEXT("Climb"))!=0;
            Done=Time>1.2f;Limit=3;
            break;
        }
        if(bCapture && Time>=Run->NextShot && (Trial.Kind==KCross||Trial.Kind==KBack||Trial.Kind==KCatch))
        {
            const FVector Eye=G+ChultCanyonFrame(F.X-60,-330,F.Z+25);
            Run->Camera->SetActorLocationAndRotation(Eye,(C->GetActorLocation()-Eye).Rotation());PC->SetViewTarget(Run->Camera);
            const FString Folder=FPaths::ScreenShotDir()/TEXT("Bridge");IFileManager::Get().MakeDirectory(*Folder,true);
            FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("T%d_%s_%03d.png"),Run->Trial,Trial.Name,Run->ShotIndex++),false,false);
            Run->NextShot+=.6f;
        }
        const bool Fell=Trial.Kind!=KLetGo && Trial.Kind!=KNoStand && C->GetFallDeaths()>Run->Deaths;
        if(!Done && !Fell && Time<Limit)return;
        Pass=Pass && !Fell;Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_BRIDGE_TRIAL trial=%d name=%s pass=%d time=%.2f jumps=%d gait=%s grip=%d leaps=%d music=%d fall_deaths=%d at=%s"),
            Run->Trial,Trial.Name,Pass,Time,Run->Jumps,C->GetGaitName(),Grip,C->GetSwingLeaps()-Run->Leaps,Music,C->GetFallDeaths()-Run->Deaths,*F.ToString());
        if(!Pass) {Run->Failures+=Trials.Num()-1-Run->Trial;Run->Trial=Trials.Num();}
        else ++Run->Trial;
        Run->Start=-1;Run->ShotIndex=0;Run->NextShot=0;PC->SetViewTarget(C);
        if(Run->Trial>=Trials.Num())
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_BRIDGE_TEST_COMPLETE failures=%d trials=%d static_failures=%d grips=%d music_far=%d music_back=%d"),Run->Failures,Trials.Num(),Static,ChultBridgePlanks().Num(),Run->bMusicFar,Run->bMusicBack);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
