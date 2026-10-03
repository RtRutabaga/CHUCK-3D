#include "SewerSlide.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    // The tube: a low arch (1 m wide, 90 cm high) over a slightly hollowed
    // floor, steepening from 5 to 38 degrees and bending away out of sight.
    constexpr float Step = 20.f, Length = 1100.f, HalfWidth = 50.f, Rise = 90.f;
    TArray<FVector> Path, Along, Side;   // floor centre line, its direction, the horizontal right
    bool bExited = false;
    float Dip(float S) { return 6.f * FMath::Clamp(S / 60.f, 0.f, 1.f); }
    FVector Mouth(float A,float S)
    {
        const float Wear=1.f-FMath::SmoothStep(0.f,160.f,S);
        return FVector(HalfWidth*FMath::Cos(A)*(1.f+.06f*Wear*FMath::Sin(A*7.f)),0,
            Rise*FMath::Sin(A)+5.f*Wear*FMath::Sin(A*5.f+.7f)*FMath::Sin(A));
    }
    void Mesh(AActor* Owner, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& UV, UMaterialInterface* Material, bool bSolid)
    {
        auto* M = NewObject<UProceduralMeshComponent>(Owner);
        M->SetupAttachment(Owner->GetRootComponent());
        M->bUseComplexAsSimpleCollision = true;
        M->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
        M->SetLightingChannels(false, true, false);
        M->RegisterComponent();
        TArray<int32> Faces = T;   // seen from either side, as the sewer shell
        for (int32 I = 0; I < T.Num(); I += 3) Faces.Append({ T[I], T[I + 2], T[I + 1] });
        M->CreateMeshSection_LinearColor(0, V, Faces, N, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bSolid);
        M->SetMaterial(0, Material);
    }
    int32 Lower(float S) { return FMath::Clamp(FMath::FloorToInt(S / Step), 0, FMath::Max(0, Path.Num() - 2)); }
}

void BuildDockSewerSlide(AActor* Owner, const TArray<FVector>& Route, const TArray<FVector>& Right,
    float OuterHalfWidth, float OuterHeight, float LastStreamOffset, UMaterialInterface* Stone, UMaterialInterface* Water)
{
    const int32 Count = Route.Num();
    bExited = false; // New world starts in morning; R only resets Chuck's position.
    if (Count < 6) return;
    const FVector End = Route.Last(), R0 = Right.Last(), D0(-R0.Y, R0.X, 0);
    Path.Reset(); Along.Reset(); Side.Reset();
    FVector P = End;
    for (float S = 0; S <= Length + 1.f; S += Step)
    {
        const float Yaw = 18.f * FMath::SmoothStep(250.f, Length, S);
        const float Pitch = FMath::DegreesToRadians(FMath::Lerp(5.f, 38.f, FMath::SmoothStep(40.f, 420.f, S)));
        const FVector H = D0.RotateAngleAxis(Yaw, FVector::UpVector);
        const FVector F = H * FMath::Cos(Pitch) - FVector::UpVector * FMath::Sin(Pitch);
        Path.Add(P); Along.Add(F); Side.Add(FVector(H.Y, -H.X, 0));
        P += F * Step;
    }
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV;
    // The end wall, now a ring round the slide's mouth (the old cap's outline).
    constexpr int32 Arc = 32;
    for (int32 J = 0; J <= Arc; ++J)
    {
        const float A = PI * J / Arc;
        V.Add(End + R0 * (OuterHalfWidth * FMath::Cos(A)) + FVector(0, 0, OuterHeight * FMath::Sin(A)));
        const FVector Inner=Mouth(A,0);
        V.Add(End+R0*Inner.X+FVector(0,0,Inner.Z));
        N.Add(-D0); N.Add(-D0);
        UV.Add(FVector2D(J / float(Arc), 1)); UV.Add(FVector2D(J / float(Arc), .75f));
        if (J > 0) { const int32 B = J * 2; T.Append({ B - 2, B - 1, B, B - 1, B + 1, B }); }
    }
    Mesh(Owner, V, T, N, UV, Stone, true);
    // A fallen, angular rock face hides the flat structural cap. Keep the
    // channel's centre and its full low opening clear for the actual slide.
    V.Reset(); N.Reset(); T.Reset(); UV.Reset();
    FRandomStream Rubble(20261004);
    const float G=(1.f+FMath::Sqrt(5.f))*.5f;
    const FVector Corners[]={FVector(-1,G,0),FVector(1,G,0),FVector(-1,-G,0),FVector(1,-G,0),
        FVector(0,-1,G),FVector(0,1,G),FVector(0,-1,-G),FVector(0,1,-G),
        FVector(G,0,-1),FVector(G,0,1),FVector(-G,0,-1),FVector(-G,0,1)};
    const int32 Faces[]={0,11,5,0,5,1,0,1,7,0,7,10,0,10,11,1,5,9,5,11,4,11,10,2,10,7,6,7,1,8,
        3,9,4,3,4,2,3,2,6,3,6,8,3,8,9,4,9,5,2,4,11,6,2,10,8,6,7,9,8,1};
    int32 RockCount=0;
    for(int32 Row=0;Row<4;++Row) for(int32 J=0;J<=10;++J)
    {
        const float A=PI*J/10.f+Rubble.FRandRange(-.035f,.035f);
        const float X=(Row==0?62.f:78.f+(Row-1)*50.f)*FMath::Cos(A);
        const float Z=(Row==0?7.f:25.f)+(Row==0?95.f:122.f+(Row-1)*88.f)*FMath::Sin(FMath::Clamp(A,0.f,PI));
        const FVector Centre=End+R0*X+FVector(0,0,Z)-D0*Rubble.FRandRange(12.f,45.f);
        const FVector Size=Row==0?FVector(14,24,18):FVector(22.f+(Row-1)*8.f,35.f+(Row-1)*7.f,30.f+(Row-1)*9.f);
        const FQuat Turn=FRotator(Rubble.FRandRange(-12.f,12.f),Rubble.FRandRange(-20.f,20.f),Rubble.FRandRange(-12.f,12.f)).Quaternion();
        TArray<FVector> Points;
        for(const FVector& C : Corners)
        {
            const FVector Local=Turn.RotateVector(C.GetSafeNormal()*Rubble.FRandRange(.88f,1.14f))*Size;
            Points.Add(Centre+R0*Local.X+D0*Local.Y+FVector(0,0,Local.Z));
        }
        for(int32 F=0;F<UE_ARRAY_COUNT(Faces);F+=3)
        {
            const FVector A0=Points[Faces[F]],B0=Points[Faces[F+1]],C0=Points[Faces[F+2]];
            const FVector Normal=FVector::CrossProduct(B0-A0,C0-A0).GetSafeNormal();
            const int32 B=V.Num(); V.Append({A0,B0,C0}); T.Append({B,B+1,B+2});
            for(int32 K=0;K<3;++K) {N.Add(Normal);UV.Add(FVector2D(K==1?1:0,K==2?1:0));}
        }
        ++RockCount;
    }
    Mesh(Owner,V,T,N,UV,Stone,true);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SLIDE_RUBBLE rocks=%d collision=1"),RockCount);
    // The tube, closed at its far end (long out of view by then).
    V.Reset(); N.Reset(); T.Reset(); UV.Reset();
    constexpr int32 Roof = 16, Floor = 6, Loop = Roof + Floor;
    for (int32 K = 0; K < Path.Num(); ++K)
    {
        const float S = K * Step;
        for (int32 J = 0; J <= Roof; ++J)
        {
            const float A = PI * J / Roof;
            const FVector Inner=Mouth(A,S);
            V.Add(Path[K]+Side[K]*Inner.X+FVector(0,0,Inner.Z));
            N.Add((-Side[K] * (FMath::Cos(A) / HalfWidth) - FVector::UpVector * (FMath::Sin(A) / Rise)).GetSafeNormal());
            UV.Add(FVector2D(S / 100.f, J * .2f));
        }
        for (int32 J = 1; J < Floor; ++J)
        {
            const float X = -HalfWidth + 2.f * HalfWidth * J / Floor;
            V.Add(Path[K] + Side[K] * X - FVector(0, 0, Dip(S) * (1.f - FMath::Square(X / HalfWidth))));
            N.Add(FVector::UpVector);
            UV.Add(FVector2D(S / 100.f, (Roof + J) * .2f));
        }
        if (K > 0)
            for (int32 J = 0; J < Loop; ++J)
            {
                const int32 A = (K - 1) * Loop + J, B = (K - 1) * Loop + (J + 1) % Loop;
                T.Append({ A, B, A + Loop, B, B + Loop, A + Loop });
            }
    }
    const int32 Last = (Path.Num() - 1) * Loop, Centre = V.Num();
    V.Add(Path.Last() + FVector(0, 0, Rise * .4f)); N.Add(-Along.Last()); UV.Add(FVector2D(0, 0));
    for (int32 J = 0; J < Loop; ++J) T.Append({ Centre, Last + J, Last + (J + 1) % Loop });
    Mesh(Owner, V, T, N, UV, Stone, true);
    // The stream carries on through the mouth and down: from where the
    // sewer's own water ends (four samples short), easing onto the centre line.
    V.Reset(); N.Reset(); T.Reset(); UV.Reset();
    for (int32 I = Count - 5; I < Count; ++I)
    {
        const float Blend = (Count - 1 - I) / 4.f;
        for (float S : { -1.f, 1.f })
        {
            V.Add(Route[I] + Right[I] * (S * 41.5f + LastStreamOffset * Blend) + FVector(0, 0, I == Count - 5 ? -2.5f : .6f));
            N.Add(FVector::UpVector); UV.Add(FVector2D((S + 1) * .5f, I * .65f));
        }
    }
    for (int32 K = 1; K < Path.Num(); ++K)
        for (float S : { -1.f, 1.f })
        {
            V.Add(Path[K] + Side[K] * (S * 41.5f) + FVector(0, 0, .6f));
            N.Add(FVector::UpVector); UV.Add(FVector2D((S + 1) * .5f, (Count - 1) * .65f + K * Step / 100.f));
        }
    for (int32 I = 0; I < V.Num() / 2 - 1; ++I) { const int32 A = I * 2; T.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 }); }
    Mesh(Owner, V, T, N, UV, Water, false);
    // A faint cold light just inside, so the mouth reads as a way down rather than a black patch.
    auto* Light = NewObject<UPointLightComponent>(Owner);
    Light->SetupAttachment(Owner->GetRootComponent());
    Light->SetRelativeLocation(Path[3] + FVector(0, 0, 45));
    Light->SetIntensity(1100); Light->SetAttenuationRadius(420);
    Light->SetLightColor(FLinearColor(.42f, .44f, .47f));
    Light->SetLightingChannels(false, true, false); Light->SetCastShadows(false);
    Light->RegisterComponent();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SEWER_SLIDE_BUILT length_cm=%.0f drop_cm=%.0f samples=%d"), Length, End.Z - Path.Last().Z, Path.Num());
}

bool IsInDockSewerSlide(const FVector& P)
{
    if (Path.Num() < 2 || P.Z > Path[0].Z + 150.f || P.Z < Path.Last().Z - 50.f) return false;
    for (int32 K = 1; K < Path.Num(); ++K)
        if (FMath::PointDistToSegment(P, Path[K - 1] + FVector(0, 0, 40), Path[K] + FVector(0, 0, 40)) < 110.f) return true;
    return false;
}
float DockSewerSlideEntry(const FVector& P)
{
    if (Path.Num() < 2 || !IsInDockSewerSlide(P)) return -1.f;
    const FVector Local = P - Path[0];
    const float In = static_cast<float>(FVector::DotProduct(Local, DockSewerSlideInward()));
    return In > 0.f && FMath::Abs(FVector::DotProduct(Local, Side[0])) < HalfWidth + 10.f ? In : -1.f;
}
FVector DockSewerSlidePoint(float S)
{
    if (Path.Num() < 2) return FVector::ZeroVector;
    const int32 K = Lower(S);
    return FMath::Lerp(Path[K], Path[K + 1], FMath::Clamp(S / Step - K, 0.f, 1.f));
}
FVector DockSewerSlideDirection(float S) { return Path.Num() < 2 ? FVector::ForwardVector : Along[Lower(S)]; }
float DockSewerSlideLength() { return Path.Num() < 2 ? 0.f : (Path.Num() - 1) * Step; }
FVector DockSewerSlideInward() { return Path.Num() < 2 ? FVector::ForwardVector : FVector(Along[0].X, Along[0].Y, 0).GetSafeNormal(); }
FVector DockSewerSlideApproach() { return Path.Num() < 2 ? FVector::ZeroVector : Path[0] - DockSewerSlideInward() * 150.f + FVector(0, 0, 34.65f); }
// The court pier's end deck (DockSetting.cpp: centre 1910,3080, 840 x 300 cm) ends at x 2330.
FVector DockPierExitProbe() { return FVector(2420, 3080, -20); }
bool HasExitedDockSewer() { return bExited; }
void MarkDockSewerExited() { bExited = true; }
