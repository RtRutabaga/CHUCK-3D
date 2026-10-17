#include "ChultGorge.h"
#include "ChultGrid.h"
#include "ChultLandform.h"
#include "ChultCanyon.h"
#include "ChultExpansion.h"
#include "ChultRiver.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    float Smooth(float A,float B,float X){return Ease((X-A)/(B-A));}
    float Noise(float X,float Y,float Scale,float Seed){return FMath::PerlinNoise2D(FVector2D(X/Scale+17.31f+Seed*3.17f,Y/Scale-41.7f+Seed*1.93f));}
    float SMin(float A,float B,float K){const float H=FMath::Clamp(.5f+.5f*(B-A)/K,0.f,1.f);return FMath::Lerp(B,A,H)-K*H*(1-H);}
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}
    float BoxDistance(FVector2D P,FVector2D Center,FVector2D Extent)
    {
        const FVector2D D(FMath::Abs(P.X-Center.X)-Extent.X,FMath::Abs(P.Y-Center.Y)-Extent.Y);
        return FVector2D(FMath::Max(D.X,0.),FMath::Max(D.Y,0.)).Size()+FMath::Min(FMath::Max(D.X,D.Y),0.);
    }

    // The gorge winds, so a view down it ends in overlapping spurs rather
    // than a vanishing corridor. It is narrowest (45 m) at the crossing.
    float Center(float T){return 2250+3600*(FMath::Sin(T*1.15e-4f+.4f)-FMath::Sin(.4f))+1700*(FMath::Sin(T*2.9e-4f+1.9f)-FMath::Sin(1.9f));}
    float HalfWidth(float T)
    {
        const float E=Ease((FMath::Abs(T)-5000)/14000);
        return 2250+E*(1700+1100*(.5f+.5f*FMath::Sin(T*1.9e-4f+2.6f)));
    }

    // Tributaries: valleys cut into each plateau and hang above the gorge,
    // notching the rim where their streams fall.
    struct FValley{int32 Side;float T,Notch,Recess,Width,Length;};
    const FValley Valleys[]={
        {0,-21000,750,1500,1100,26000},{0,27500,900,1700,1300,30000},{0,-37500,1000,1900,1500,32000},{0,45000,800,1600,1200,26000},{0,-61000,900,1800,1400,30000},
        {1,-9500,650,1300,1000,20000},{1,15500,800,1500,1200,24000},{1,-25500,950,1800,1400,30000},{1,36000,850,1600,1300,28000},{1,-50000,900,1700,1300,26000}};
    float Recess(int32 Side,float T)
    {
        float R=0;for(const FValley& V:Valleys)if(V.Side==Side)R+=V.Recess*FMath::Exp(-FMath::Square((T-V.T)/(V.Width*1.7f)));
        return R*Ease((FMath::Abs(T)-(Side==0?4500:6500))/2000);
    }

    // Existing floors, exactly as the earlier vista reserved them, plus the
    // far bank and river. New land starts at their edges.
    struct FFloorEdge{float Distance,Base;};
    FFloorEdge FloorEdge(FVector P)
    {
        const FVector F=Frame(P);
        FFloorEdge E{BoxDistance(FVector2D(P.X,P.Y),FVector2D::ZeroVector,FVector2D(13000)),0};
        float D=BoxDistance(FVector2D(P.X,P.Y),FVector2D(14500,8750),FVector2D(7500,6750));
        if(D<E.Distance){FVector Q(FMath::Clamp(P.X,7000.,22000.),FMath::Clamp(P.Y,2000.,15500.),0);E={D,ChultExpansionGroundZ(Q)};}
        D=BoxDistance(FVector2D(F.X,F.Y),FVector2D(-3250,0),FVector2D(3250,4500));
        if(D<E.Distance){E={D,ChultCanyonGroundZ(ChultCanyonFrame(FMath::Clamp(F.X,-6500.,0.),FMath::Clamp(F.Y,-4500.,4500.)))};}
        // The far bank, to its upstream end (S=64000 since 2026-10-15).
        D=BoxDistance(FVector2D(F.X,F.Y),FVector2D((4500+ChultFarFloorEnd)*.5f,0),FVector2D((ChultFarFloorEnd-4500)*.5f,6500));
        if(D<E.Distance){E={D,ChultCanyonGroundZ(ChultCanyonFrame(FMath::Clamp(F.X,4500.,static_cast<double>(ChultFarFloorEnd)),FMath::Clamp(F.Y,-6500.,6500.)))};}
        // The temple precinct, wider (2026-10-16).
        D=BoxDistance(FVector2D(F.X,F.Y),FVector2D((ChultTemplePrecinctFrom+ChultFarFloorEnd)*.5f,0),FVector2D((ChultFarFloorEnd-ChultTemplePrecinctFrom)*.5f,ChultTemplePrecinctHalf));
        if(D<E.Distance){E={D,ChultCanyonGroundZ(ChultCanyonFrame(FMath::Clamp(F.X,static_cast<double>(ChultTemplePrecinctFrom),static_cast<double>(ChultFarFloorEnd)),FMath::Clamp(F.Y,-static_cast<double>(ChultTemplePrecinctHalf),static_cast<double>(ChultTemplePrecinctHalf))))};}
        return E;
    }

    float Rim(int32 Side,float T)
    {return 950+1500*Ease((FMath::Abs(T)-6000)/30000)+650*Noise(T,Side*91.f,11000,1)+300*Noise(T,Side*37.f,3700,2);}
    float Upland(int32 Side,float S,float T,float U,float Ur)
    {
        // A plateau climbing gently away from the rim, rolling hills (calmer
        // right at the edge), then ridged ranges far inland. Ur is U for the
        // plateau and ranges: on the far side it is held to the distance from
        // the playable floor, so they stand back from its upstream valley.
        float H=Rim(Side,T)+.045f*FMath::Min(Ur,30000.f)+.02f*FMath::Max(Ur-30000.f,0.f);
        const float W=Noise(S,T,13000,3)*3200;
        H+=(1100*Noise(S+W,T,8200,4)+420*Noise(S,T+W,2900,5)+140*Noise(S,T,950,6)+45*Noise(S,T,330,7))*(.3f+.7f*Ease(U/2500));
        // Forested ranges: broad ridged masses, softened, never knife-edged.
        const float R1=1-FMath::Abs(Noise(S+W,T-W,26000,8)),R2=1-FMath::Abs(Noise(S-W,T,14000,9));
        H+=(10500*R1*R1+2600*R1*R2*R2)*Ease((Ur-14000)/50000);
        for(const FValley& V:Valleys)
        {
            if(V.Side!=Side)continue;const float Fade=1-Ease((U-V.Length)/7000);if(Fade<=0)continue;
            const float Tc=V.T+1500*FMath::Sin(U/5200+V.T*.001f)*Ease(U/3500),X=T-Tc;if(FMath::Abs(X)>14000)continue;
            // V-shaped, round-bottomed, its floor rising gently upstream.
            const float Floor=Rim(Side,V.T)-V.Notch+.028f*U,Carved=Floor+(.42f+V.Width*.00012f)*FMath::Sqrt(X*X+260.f*260.f);
            H=FMath::Lerp(H,SMin(H,Carved,450),Fade);
        }
        return H;
    }

    // Stratigraphy shared by both walls, dipping gently down the gorge: hard
    // beds stand as cliffs, soft ones weather back into vegetated benches.
    // Beds dip down the gorge and undulate gently, so benches rise and fall.
    float Layer(float Z,float T){return Z-.011f*T+380*Noise(T,0,7000,21)+140*Noise(T,0,2300,22);}
    float Hard(float L){return FMath::Sin(L/260+1.6f*FMath::Sin(L/830))+.4f*FMath::Sin(L/97+2);}
    float BenchSlope(float L){return .9f*Smooth(-.6f,-.9f,Hard(L));}
    constexpr float StrataLow=-36000,StrataStep=10;
    const TArray<float>& StrataTable()
    {
        static TArray<float> Table;
        if(Table.Num()==0)
        {
            Table.SetNum(4800);float Sum=0;
            for(int32 I=0;I<Table.Num();++I){Table[I]=Sum;Sum+=BenchSlope(StrataLow+I*StrataStep)*StrataStep;}
        }
        return Table;
    }
    // Total horizontal set-back of the soft beds below layer height L.
    float Strata(float L)
    {
        const auto& Table=StrataTable();const float X=FMath::Clamp((L-StrataLow)/StrataStep,0.f,Table.Num()-1.001f);
        const int32 I=FMath::FloorToInt(X);return FMath::Lerp(Table[I],Table[I+1],X-I);
    }

    struct FWall{float S,Rock,Crag,Wet,Bench;};
    FWall Wall(int32 Side,float T,float Z,float Top,float Lip,bool bFalls=true);
    struct FFall{int32 Side;float T,Width,Speed,Top,Lip,Alcove,AlcoveDepth;};
    const TArray<FFall>& Falls()
    {
        static TArray<FFall> List;
        if(List.Num()==0)
        {
            // The creek's outfall and the broad river keep their existing lips.
            List.Add({0,-1800,125,170,50,0,380,420});
            List.Add({1,2200,920,300,280,4500,950,700});
            for(const FValley& V:Valleys)
            {
                const float Lip=ChultGorgeLip(V.Side,V.T);
                List.Add({V.Side,V.T,260+V.Width*.25f,240,ChultGorgeLandZ(Lip,V.T,V.Side)+12,Lip,520,520});
            }
            // Springs seep from bedding planes in sight of the bridge.
            const FVector Springs[]={{0,6300,-700},{1,-5600,-450},{0,-7600,-1300},{1,8700,-1000},{0,13200,-500},{1,-12800,-900}};
            for(const FVector& Spring:Springs)
            {
                const int32 Side=Spring.X;const float T=Spring.Y,Z=Spring.Z;
                const float Lip=Wall(Side,T,Z,ChultGorgeTop(Side,T),ChultGorgeLip(Side,T),false).S;
                List.Add({Side,T,110+40*FMath::Abs(FMath::Sin(T)),70,Z,Lip,260,180});
            }
        }
        return List;
    }

    FWall Wall(int32 Side,float T,float Z,float Top,float Lip,bool bFalls)
    {
        const float Sg=Side==0?1.f:-1.f,A=FMath::Abs(T),D=FMath::Max(Top-Z,0.f),Away=Ease((A-4500)/8000);
        // Plunge alcoves: water undercuts the face behind every fall.
        float Ac=0,AcDepth=0;
        if(bFalls)for(const FFall& F:Falls())if(F.Side==Side){const float W=FMath::Exp(-FMath::Square((T-F.T)/F.Alcove));if(W>Ac){Ac=W;AcDepth=F.AlcoveDepth;}}
        const float AcD=Ac*Ease(D/600);
        // Soil and roots roll over the rim before the rock drops away.
        const float R=FMath::Lerp(80.f,650.f,Away)*(1+.3f*Noise(T,Side*7.f,2100,11));
        const float Roll=(D<R?R*FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(1-D/R))):R)*(1-Ac);
        // A caprock: the top seven metres stand vertical everywhere.
        // Some stretches are massive sheer walls; others weather into many benches.
        const float Massive=.35f+.65f*Ease(.5f+.9f*Noise(T,Side*3.f,5200,23));
        const float L=Layer(Z,T),Out=Massive*FMath::Max(0.f,Strata(Layer(Top-700,T))-Strata(L));
        const float H0=Hard(L),Rising=Hard(L+30)-Hard(L-30);
        const float Under=Rising>0?150*Smooth(-.35f,-.6f,H0)*(1-Smooth(-.75f,-1.f,H0))*Ease((D-700)/300):0;
        // Buttresses between gullies, deepening below the rim.
        const float Warp=Noise(T,Side*50.f,6000,12)*1400,Rib=1-FMath::Abs(Noise(T+Warp,Side*13.f,2300,13));
        const float Chute=FMath::Abs(Noise(T,Side*29.f+D*.2f,900,14));
        const float Butt=((Rib*Rib-.4f)*900+(Chute-.3f)*300)*Ease((D-500)/2600)*FMath::Lerp(.45f,1.f,Away);
        const float Rough=(60*Noise(T,Z,260,15)+24*Noise(T,Z,90,16))*Ease((D-150)/300)*(1-Ac*.7f);
        float S=Lip+Sg*(Roll+(Out+Butt-Under)*(1-Ac)+Rough-AcD*AcDepth);
        // The face always stands behind falling water, cut back into a
        // deepening plunge alcove beneath each lip.
        if(bFalls)for(const FFall& F:Falls())
        {
            if(F.Side!=Side)continue;const float Drop=FMath::Max(F.Top-Z,0.f),Half=F.Width*(1+.35f*Drop/2500)*.5f+260,Lat=FMath::Abs(T-F.T);
            if(Lat>Half+900)continue;
            const float Time=FMath::Sqrt(2*Drop/980.f),Water=F.Speed*1.1f*(1-FMath::Exp(-Time/1.1f));
            const float Behind=F.Lip+Sg*(Water-90-.12f*Drop),Pushed=Side==0?FMath::Min(S,Behind):FMath::Max(S,Behind);
            S=FMath::Lerp(S,Pushed,(1-Ease((Lat-Half)/900))*Ease(Drop/150));
        }
        const float Mid=(ChultGorgeLip(0,T)+ChultGorgeLip(1,T))*.5f;
        S=Side==0?SMin(S,Mid-150,250):-SMin(-S,-(Mid+150),250);
        const float Bench=BenchSlope(L)/.9f*Ease((D-700)/200)*(1-Ac)*Massive*Ease(.6f+1.2f*Noise(T,L,3000,24));
        const float Soil=1-Ease((D-R*.3f)/(R*.9f+60));
        // Mats of fern, moss and root cling to the faces in patches, thickest
        // in the gullies and on the damp lower walls.
        const float Mats=Ease((Noise(T*.8f,Z,1500,25)+.6f*Noise(T,Z,480,26)+.25f*(Chute-.3f)+.2f*Ease((D-2500)/4000)+.16f)/.35f)*Ease((D-250)/400);
        float Rock=(1-.8f*Bench)*(1-Soil)*(1-.75f*Mats)+.18f*Noise(T,Z,700,17);
        const float Crag=Ease((D-1500)/6000)*.75f+.3f*Noise(T,Z,1600,18);
        const float Wet=FMath::Max(Ac*Ease((D-30)/200)*.9f,.0f);
        return {S,FMath::Clamp(Rock,0.f,1.f),FMath::Clamp(Crag,0.f,1.f),Wet,Bench};
    }
    FColor Weights(float Rock,float Crag,float Wet){return FColor(FMath::RoundToInt(Rock*255),FMath::RoundToInt(Crag*255),FMath::RoundToInt(FMath::Clamp(Wet,0.f,1.f)*255),255);}

    TArray<float> Columns()
    {
        TArray<float> Half;
        for(float X=0;X<130000;)
        {
            Half.Add(X);const float Step=X<9000?100:X<20000?200:X<50000?500:1500;X=FMath::Min(X+Step,130000.f);
        }
        Half.Add(130000);TArray<float> T;
        for(int32 I=Half.Num()-1;I>0;--I)T.Add(-Half[I]);
        T.Append(Half);return T;
    }
    TArray<float> WallDepths(float Bottom)
    {
        TArray<float> D={0,10,25,45,70,100,140,190,250,320,400};
        for(float X=520;X<6000;X+=120)D.Add(X);
        for(float X=6000;X<13000;X+=300)D.Add(X);
        for(float X=13000;X<Bottom;X+=1000)D.Add(X);
        D.Add(Bottom);return D;
    }
}

float ChultGorgeLip(int32 Side,float T)
{
    if(Side==0)return Ease((FMath::Abs(T)-4500)/9000)*(Center(T)-HalfWidth(T))-Recess(0,T);
    return 4500+Ease((FMath::Abs(T)-6500)/9000)*(Center(T)+HalfWidth(T)-4500)+Recess(1,T);
}
float ChultGorgeLandZ(float S,float T,int32 Side)
{
    const FFloorEdge E=FloorEdge(ChultCanyonFrame(S,T));if(E.Distance<=0)return E.Base;
    if(Side<0)Side=S<(ChultGorgeLip(0,T)+ChultGorgeLip(1,T))*.5f?0:1;
    const float U=FMath::Max(Side==0?ChultGorgeLip(0,T)-S:S-ChultGorgeLip(1,T),0.f);
    // The existing boundary rule: eleven metres up within seventeen of a floor.
    const float Ur=Side==1?FMath::Min(U,E.Distance+6000):U;
    return FMath::Lerp(E.Base,FMath::Max(Upland(Side,S,T,U,Ur),E.Base+1100),Ease(E.Distance/1700));
}
float ChultGorgeTop(int32 Side,float T){return ChultGorgeLandZ(ChultGorgeLip(Side,T),T,Side);}
float ChultGorgeBench(int32 Side,float T,float Z){return Wall(Side,T,Z,ChultGorgeTop(Side,T),ChultGorgeLip(Side,T)).Bench;}
float ChultGorgeFloorDistance(float S,float T){return FloorEdge(ChultCanyonFrame(S,T)).Distance;}
TArray<float> ChultGorgeColumns(){return Columns();}
float ChultGorgeWallS(int32 Side,float T,float Z){return Wall(Side,T,Z,ChultGorgeTop(Side,T),ChultGorgeLip(Side,T)).S;}
int32 ChultGorgeFallCount(){return Falls().Num();}
int32 ChultGorgeFallSide(int32 I){return Falls()[I].Side;}
float ChultGorgeFallDrop(int32 I){return Falls()[I].Top-(ChultGorgeMistZ-4300);}
FVector ChultGorgeFall(int32 I,float Drop,float AcrossFall)
{
    // Water leaves the lip with the stream's speed, slowed by the air, and
    // spreads as it falls.
    const FFall& F=Falls()[I];const float Sg=F.Side==0?1.f:-1.f;
    const float Time=FMath::Sqrt(2*FMath::Max(Drop,0.f)/980.f),Tau=1.1f;
    const float Out=F.Speed*Tau*(1-FMath::Exp(-Time/Tau))+18*Ease(Drop/60);
    const float Spread=F.Width*(1+.35f*Drop/2500);
    const float T=F.T+AcrossFall*Spread+FMath::Sin(Drop*.004f+AcrossFall*3)*20*Ease(Drop/400);
    return ChultCanyonFrame(F.Lip+Sg*Out,T,F.Top-Drop);
}

void BuildChultGorge(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    auto* Rock=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultGorge/M_ChultGorge.M_ChultGorge"));
    auto* Water=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultGorge/M_ChultPlunge.M_ChultPlunge"));
    auto* Stream=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultTerrain/M_ChultStream.M_ChultStream"));
    int32 Missing=!Rock+!Water+!Stream,Triangles=0,WallChunks=0,SolidChunks=0;
    const TArray<float> Cols=Columns();
    // Both walls: one grid each, columns along the gorge, rows down from the rim.
    const float Bottom=26000;
    for(int32 Side=0;Side<2;++Side)
    {
        const float Sg=Side==0?1.f:-1.f;TArray<float> Top,Lip;
        for(float T:Cols){Lip.Add(ChultGorgeLip(Side,T));Top.Add(ChultGorgeLandZ(Lip.Last(),T,Side));}
        const TArray<float> Depth=WallDepths(Bottom);FChultGrid Grid;Grid.Init(Cols.Num(),Depth.Num());
        for(int32 J=0;J<Depth.Num();++J)for(int32 I=0;I<Cols.Num();++I)
        {
            const float Z=Top[I]-Depth[J];const FWall W=Wall(Side,Cols[I],Z,Top[I],Lip[I]);
            Grid.P[Grid.Id(I,J)]=ChultCanyonFrame(W.S,Cols[I],Z);Grid.C[Grid.Id(I,J)]=Weights(W.Rock,W.Crag,W.Wet);
        }
        for(int32 I0=0;I0<Cols.Num()-1;I0+=64)
        {
            const int32 I1=FMath::Min(I0+64,Cols.Num()-1);
            const bool Solid=FMath::Abs(Cols[I0])<9500||FMath::Abs(Cols[I1])<9500||(Cols[I0]<0&&Cols[I1]>0);
            if(auto* Mesh=Grid.Emit(Owner,FString::Printf(TEXT("ChultGorgeWall%d_%d"),Side,I0),Rock,I0,I1,0,Depth.Num()-1,[&](int32,int32){return Along*Sg;},Solid,true,Triangles))
            {Mesh->ComponentTags.Add(TEXT("ChultGorgeWall"));Mesh->ComponentTags.Add(TEXT("ChultStartVista"));++WallChunks;SolidChunks+=Solid;}
        }
    }

    // Plunging falls: a glassy crest breaking into lace, a thinner sheet
    // behind, and a wide veil of spray, all fading into the mist below.
    FChultLandform Sheets,Streams;int32 FallSamples=0;
    auto Ribbon=[&](int32 Fall,float Offset,float WidthScale,float Alpha,float Start,float UOffset)
    {
        const float Drop=ChultGorgeFallDrop(Fall);const int32 Rows=90,ColsAcross=10;
        const float Sg=ChultGorgeFallSide(Fall)==0?1.f:-1.f;
        auto Point=[&](float D,float A){return ChultGorgeFall(Fall,D,A*WidthScale)+Along*Sg*Offset*Ease(D/300);};
        for(int32 R=0;R<Rows;++R)for(int32 K=0;K<ColsAcross;++K)
        {
            const float V0=Start+(Drop-Start)*FMath::Pow(R/float(Rows),1.6f),V1=Start+(Drop-Start)*FMath::Pow((R+1)/float(Rows),1.6f);
            const float A0=K/float(ColsAcross)-.5f,A1=(K+1)/float(ColsAcross)-.5f;
            const FVector P[]={Point(V0,A0),Point(V0,A1),Point(V1,A1),Point(V1,A0)};
            const int32 First=Sheets.V.Num();Sheets.Quad(P[0],P[1],P[2],P[3]);
            const float Dv[]={V0,V0,V1,V0,V1,V1},Av[]={A0,A1,A1,A0,A1,A0};
            for(int32 X=0;X<6;++X)
            {
                const float D=Dv[X],Width=FVector::Dist(Point(D,-.5f),Point(D,.5f));
                const float Fade=1-Ease((D-(Drop-3600))/3600);
                Sheets.UV[First+X]=FVector2D((Av[X]+.5f)*Width/200+UOffset,D/200);
                Sheets.C[First+X]=FColor(FMath::RoundToInt(Ease(D/650)*255),FMath::RoundToInt((Av[X]+.5f)*255),0,FMath::RoundToInt(Alpha*Fade*255));
            }
            // Face the viewer side of the fall: both sides render (two-sided material).
        }
        ++FallSamples;
    };
    for(int32 F=0;F<ChultGorgeFallCount();++F)
    {
        Ribbon(F,0,1,1,0,0);
        Ribbon(F,-45,.82f,.55f,40,3.7f);
        Ribbon(F,35,1.35f,.28f,350,7.3f);
    }
    // The hanging valleys' streams, from upstream to their lips.
    for(int32 F=2;F<2+static_cast<int32>(UE_ARRAY_COUNT(Valleys));++F)
    {
        const FFall& Fall=Falls()[F];const float Sg=Fall.Side==0?1.f:-1.f;
        for(float U=3000;U>0;U-=100)for(int32 K=0;K<4;++K)
        {
            auto At=[&](float Up,float A)
            {
                const float Tc=Fall.T+1500*FMath::Sin(Up/5200+Fall.T*.001f)*Ease(Up/3500)+A*Fall.Width*.8f;
                const float S=ChultGorgeLip(Fall.Side,Fall.T)-Sg*Up;FVector P=ChultCanyonFrame(S,Tc);
                P.Z=FMath::Min(ChultGorgeLandZ(S,Tc,Fall.Side),Fall.Top-12+.028f*Up)+12;return P;
            };
            const float A0=K/4.f-.5f,A1=(K+1)/4.f-.5f;const int32 First=Streams.V.Num();
            Streams.Quad(At(U,A0),At(U,A1),At(U-100,A1),At(U-100,A0));
            const FVector2D UVs[]={{A0+.5f,-U/480},{A1+.5f,-U/480},{A1+.5f,-(U-100)/480},{A0+.5f,-U/480},{A1+.5f,-(U-100)/480},{A0+.5f,-(U-100)/480}};
            for(int32 X=0;X<6;++X){Streams.UV[First+X]=UVs[X];Streams.C[First+X]=FColor(255,255,255,UVs[X].X<=0||UVs[X].X>=1?0:225);}
        }
    }
    if(auto* Mesh=Sheets.Finish(Owner,TEXT("ChultGorgeFalls"),TEXT("/Game/Art/ChultGorge/M_ChultPlunge.M_ChultPlunge"),false)){Mesh->SetCastShadow(false);Triangles+=Sheets.T.Num()/3;}
    if(auto* Mesh=Streams.Finish(Owner,TEXT("ChultGorgeStreams"),TEXT("/Game/Art/ChultTerrain/M_ChultStream.M_ChultStream"),false)){Mesh->SetCastShadow(false);Triangles+=Streams.T.Num()/3;}

    // The mist sea: low billows along the gorge, heavier spray where each fall
    // meets it, and fog volumes giving the layer body.
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Puff=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/M_FallCloudPuff.M_FallCloudPuff"));
    Missing+=!Sphere+!Puff;
    auto* Puffs=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,TEXT("ChultGorgeMist"));
    Puffs->SetStaticMesh(Sphere);Puffs->SetMaterial(0,Puff);Puffs->SetupAttachment(Owner->GetRootComponent());Puffs->SetWorldLocation(G);
    Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);Puffs->SetCastShadow(false);Puffs->ComponentTags.Add(TEXT("ChultStartVista"));Puffs->RegisterComponent();
    FRandomStream R(2026101521);int32 Billows=0;
    for(float T=-42000;T<42000;)
    {
        const float A=FMath::Abs(T),Lo=ChultGorgeLip(0,T),Hi=ChultGorgeLip(1,T),Mid=(Lo+Hi)*.5f,Half=(Hi-Lo)*.5f;
        const float Size=FMath::Lerp(14.f,42.f,Ease(A/30000))*R.FRandRange(.7f,1.3f);
        const FVector At=ChultCanyonFrame(Mid+R.FRandRange(-.55f,.55f)*Half,T,ChultGorgeMistZ+R.FRandRange(-700.f,450.f));
        Puffs->AddInstance(FTransform(FRotator(0,R.FRandRange(0,360),0),G+At,FVector(Size*R.FRandRange(1.3f,2.2f),Size,Size*R.FRandRange(.28f,.45f))),true);++Billows;
        T+=A<6000?260:A<20000?650:1400;
    }
    for(int32 F=0;F<ChultGorgeFallCount();++F)
    {
        const float Drop=Falls()[F].Top-ChultGorgeMistZ;
        for(int32 K=0;K<12;++K)
        {
            const FVector At=ChultGorgeFall(F,Drop+R.FRandRange(-500.f,300.f),R.FRandRange(-.7f,.7f))+FVector(R.FRandRange(-500.f,500.f),R.FRandRange(-500.f,500.f),0);
            const float Size=FMath::Lerp(5.f,16.f,FMath::Min(Falls()[F].Width/900,1.f))*R.FRandRange(.6f,1.3f);
            Puffs->AddInstance(FTransform(FRotator(0,R.FRandRange(0,360),0),G+At,FVector(Size,Size*R.FRandRange(.8f,1.2f),Size*R.FRandRange(.55f,.8f))),true);++Billows;
        }
    }
    int32 Volumes=0;
    for(float T=-36000;T<=36000;T+=4500)
    {
        const float Mid=(ChultGorgeLip(0,T)+ChultGorgeLip(1,T))*.5f;
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());
        Fog->SetWorldLocation(G+ChultCanyonFrame(Mid,T,ChultGorgeMistZ-2600));Fog->SetWorldScale3D(FVector(9));
        Fog->RadialFogExtinction=.35f;Fog->HeightFogExtinction=.22f;Fog->HeightFogFalloff=900;Fog->HeightFogOffset=.45f;
        Fog->FogAlbedo=FLinearColor(.8f,.86f,.88f);Fog->FogEmissive=FLinearColor(.07f,.08f,.085f);
        Fog->ComponentTags.Add(TEXT("ChultStartVista"));Fog->RegisterComponent();++Volumes;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_GORGE wall_chunks=%d solid_wall_chunks=%d falls=%d valleys=%d mist_billows=%d mist_volumes=%d triangles=%d missing_assets=%d falls_collision=0 build_ms=%.1f"),
        WallChunks,SolidChunks,ChultGorgeFallCount(),static_cast<int32>(UE_ARRAY_COUNT(Valleys)),Billows,Volumes,Triangles,Missing,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultGorge(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Faces=0,FallProbes=0,Voids=0;const FVector G=AChultLanding::FloorOrigin();auto* World=Owner->GetWorld();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultGorge),false,Chuck);
    // Each wall stands where its function says (real triangles, solid near the bridge).
    for(int32 Side=0;Side<2;++Side)for(float T:{-8000.f,-4000.f,-1000.f,0.f,1000.f,4000.f,8000.f})for(float Z:{-400.f,-1500.f,-3000.f,-4800.f})
    {
        const float S=ChultGorgeWallS(Side,T,Z),Sg=Side==0?1.f:-1.f;FHitResult Hit;
        const FVector From=G+ChultCanyonFrame(S+Sg*400,T,Z),To=G+ChultCanyonFrame(S-Sg*400,T,Z);
        const bool Found=World->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Q)&&Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultGorgeWall"));
        const float Error=Found?FMath::Abs(Frame(Hit.ImpactPoint-G).X-S):1.e6f;
        if(!Found||Error>200){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_GORGE_WALL_FAIL side=%d t=%.0f z=%.0f found=%d error=%.1f"),Side,T,Z,Found,Error);}++Faces;
    }
    // The crossing's open air: nothing new between the banks above the death line.
    for(float S=200;S<=4300;S+=250)for(float T=-4000;T<=4000;T+=500)
    {
        FHitResult Hit;if(World->LineTraceSingleByChannel(Hit,G+ChultCanyonFrame(S,T,650),G+ChultCanyonFrame(S,T,-80),ECC_Visibility,Q)&&Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultGorgeWall")))
        {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_GORGE_VOID_FAIL s=%.0f t=%.0f"),S,T);}++Voids;
    }
    // Every fall stays in front of its wall all the way down to the mist.
    for(int32 F=0;F<ChultGorgeFallCount();++F)for(float U:{.02f,.08f,.2f,.4f,.65f,.9f})for(float A:{-.5f,0.f,.5f})
    {
        const FVector P=ChultGorgeFall(F,U*(ChultGorgeFallDrop(F)-3600),A),PF=Frame(P);const int32 Side=ChultGorgeFallSide(F);
        const float Gap=(Side==0?1:-1)*(PF.X-ChultGorgeWallS(Side,PF.Y,PF.Z));
        if(Gap<8){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_GORGE_FALL_IN_ROCK fall=%d u=%.2f across=%.1f gap=%.1f"),F,U,A,Gap);}++FallProbes;
    }
    Failures+=FVector::Dist(ChultGorgeFall(0,0,0),ChultCanyonFrame(0,-1800,50))>.1f;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_GORGE_CHECK failures=%d wall_faces=%d crossing_voids=%d fall_probes=%d"),Failures,Faces,Voids,FallProbes);
    return Failures;
}
