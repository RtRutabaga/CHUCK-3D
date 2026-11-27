// The Feywild's small delights (Claude, 2026-11-24). See FeyDelights.h.
#include "FeyDelights.h"
#include "FeySpores.h"
#include "FeyMap.h"
#include "FeyMesh.h"
#include "FeyLife.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

using namespace FeyMap;

namespace
{
    TArray<TWeakObjectPtr<AFeyDelights>> GDelights;
    constexpr float Stand = 34.65f;
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    FColor Col(float R, float G, float B, float A = 0.f) { return FLinearColor(R, G, B, A).ToFColor(false); }
    enum EMat { MPlant, MSign, MRainbow, MKoi, MatCount };
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyPlant.M_FeyPlant"), TEXT("/Game/Art/Feywild/M_FeySign.M_FeySign"), TEXT("/Game/Art/Feywild/M_FeyRainbow.M_FeyRainbow"), TEXT("/Game/Art/Feywild/M_FeyKoi.M_FeyKoi")};
    enum ESfx { SBell, SCaw0, SCaw1, SFlap, SSplash, SPuff, SfxCount };
    const TCHAR* SfxPaths[] = {TEXT("SFX_FeyFairyBell_00"), TEXT("SFX_FeyCrowCaw_00"), TEXT("SFX_FeyCrowCaw_01"), TEXT("SFX_FallFlap_00"), TEXT("SFX_StreamSplash_00"), TEXT("SFX_FeySporePuff_00")};
    // The sun (FeywildSetting.cpp's SunRot): the way its light travels, so also where a rainbow's centre lies from the eye.
    const FVector AntiSun = FRotator(-10.f, 168.f, 0.f).Vector();

    // ------------------------------------------------------------------ where they are (local Fey frame)
    // Fairy rings, out of the way of every road: by the riverbank's southern edge, in the orchard's far corner, deep in the
    // Deepwood's south-east, and on a knoll above the Lily Mere's eastern pools.
    const FVector2D RingSpots[] = {{-11600.f, 4400.f}, {-31200.f, -10200.f}, {-65800.f, -41200.f}, {-57200.f, 38600.f}};
    const int32 RingDistrict[] = {Riverbank, Orchard, Deepwood, LilyMere};
    constexpr float RingR = 165.f;
    // The wisps' way: from the Moonmoth Fen's end, round its pools, to a hidden pool's western shore.
    // (2026-11-25: the second point moved 2.8 m (from -67500,36700), so the way bends round an oak at -68133,36593 whose
    // roots reach 5.4 m and stopped him under the lead wisp on every run)
    const FVector2D WispPoints[] = {{-66900.f, 35700.f}, {-67300.f, 36900.f}, {-68300.f, 37600.f}, {-68700.f, 38700.f}, {-68350.f, 39650.f}};
    // The rapids' cascade (FeyMap.cpp: the river's level steps up 2.2 m about Y 14000): its spray at the foot.
    constexpr float CascadeY = 13650.f;

    // How far (X, Y) is from district D's roads and spur.
    float RoadDist(int32 D, const FVector2D& Q)
    {
        float Best = 1e9f;
        auto Line = [&](const TArray<FVector2D>& L) { for (int32 I = 0; I + 1 < L.Num(); ++I) { const FVector2D A = L[I], Ab = L[I + 1] - A; const float T = FMath::Clamp(FVector2D::DotProduct(Q - A, Ab) / FMath::Max(Ab.SizeSquared(), 1.f), 0.f, 1.f); Best = FMath::Min(Best, FVector2D::Distance(Q, A + Ab * T)); } };
        for (int32 T = 0; T < Thresholds; ++T) Line(RoadOf(D, T));
        Line(SpurOf(D));
        return Best;
    }
    // A fairy ring's ground: from its spot outward, the first place that is level across the ring, dry, inside its district
    // and well off the roads (the map's own land only, so the planting's clearings agree with it).
    FVector2D RingGround(int32 I)
    {
        static const TArray<FVector2D> S = []()
        {
            TArray<FVector2D> Out;
            for (int32 K = 0; K < UE_ARRAY_COUNT(RingSpots); ++K)
            {
                const int32 D = RingDistrict[K]; const FDistrict& Ds = FeyMap::District(D); FVector2D Found = RingSpots[K];
                for (float Rr = 0.f; Rr <= 3000.f && Found == RingSpots[K]; Rr += 150.f)
                    for (int32 A = 0; A < (Rr > 0.f ? 16 : 1); ++A)
                    {
                        const FVector2D C = RingSpots[K] + FVector2D(FMath::Cos(A * PI / 8.f), FMath::Sin(A * PI / 8.f)) * Rr;
                        if (C.X < Ds.X0 + 1200.f || C.X > Ds.X1 - 1200.f || C.Y < Ds.Y0 + 1200.f || C.Y > Ds.Y1 - 1200.f || RoadDist(D, C) < 700.f) continue;
                        const float Z0 = LandZ(D, C.X, C.Y); bool bOk = !InRiver(D, C.X, C.Y);
                        for (int32 J = 0; J < 12 && bOk; ++J)
                        {
                            const float Rj = J < 8 ? 280.f : 140.f, Aj = J * PI / (J < 8 ? 4.f : 2.f);
                            const FVector2D Q = C + FVector2D(FMath::Cos(Aj), FMath::Sin(Aj)) * Rj;
                            bOk = FMath::Abs(LandZ(D, Q.X, Q.Y) - Z0) < 30.f && !InRiver(D, Q.X, Q.Y) && !InDeepWater(Q.X, Q.Y);
                        }
                        if (bOk) { Found = C; break; }
                    }
                Out.Add(Found);
            }
            return Out;
        }();
        return S[I];
    }

    void Octa(FMapSurf& S, const FVector& C, float R, const FColor& Cl)
    {
        const FVector D[] = {{R, 0, 0}, {-R, 0, 0}, {0, R, 0}, {0, -R, 0}, {0, 0, R}, {0, 0, -R}};
        const int32 B = S.V.Num(); for (const FVector& Q : D) S.Vert(C + Q, Q.GetSafeNormal(), FVector2D::ZeroVector, Cl);
        const int32 F[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for (const auto& T : F) S.TriFacing(B + T[0], B + T[1], B + T[2], (D[T[0]] + D[T[1]] + D[T[2]]).GetSafeNormal());
    }
    // A small ellipsoid on axes F, Rt, Up (Rings x 8 round).
    void Blob(FMapSurf& S, const FVector& C, const FVector& F, const FVector& Rt, const FVector& Up, float Rx, float Ry, float Rz, const FColor& Cl, int32 Rings = 5)
    {
        const int32 B = S.V.Num(), Ar = 8;
        for (int32 I = 0; I <= Rings; ++I)
        {
            const float Ph = PI * I / Rings, Cx = FMath::Cos(Ph), Sx = FMath::Sin(Ph);
            for (int32 J = 0; J < Ar; ++J)
            {
                const float Th = 2 * PI * J / Ar; const FVector U(Cx, Sx * FMath::Cos(Th), Sx * FMath::Sin(Th));
                S.Vert(C + F * (U.X * Rx) + Rt * (U.Y * Ry) + Up * (U.Z * Rz), (F * (U.X / Rx) + Rt * (U.Y / Ry) + Up * (U.Z / Rz)).GetSafeNormal(), FVector2D(J / float(Ar), I / float(Rings)), Cl);
            }
        }
        for (int32 I = 0; I < Rings; ++I) for (int32 J = 0; J < Ar; ++J)
        { const int32 A = B + I * Ar + J, A1 = B + I * Ar + (J + 1) % Ar; S.TriFacing(A, A1, A + Ar, S.N[A]); S.TriFacing(A1, A1 + Ar, A + Ar, S.N[A1]); }
    }
    // A flat two-sided polygon (a wing, a tail) through the points, fanned from the first.
    void Flat(FMapSurf& S, TArrayView<const FVector> P, const FColor& Cl)
    {
        const FVector Nn = FVector::CrossProduct(P[1] - P[0], P[2] - P[0]).GetSafeNormal();
        for (const float Fc : {1.f, -1.f})
        {
            const int32 B = S.V.Num(); for (const FVector& Q : P) S.Vert(Q, Nn * Fc, FVector2D::ZeroVector, Cl);
            for (int32 I = 1; I + 1 < P.Num(); ++I) S.TriFacing(B, B + I, B + I + 1, Nn * Fc);
        }
    }
    void Emit(UProceduralMeshComponent* M, FMapSurf& S, UMaterialInterface* Mat, bool bShadow = false)
    {
        M->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, false);
        M->SetMaterial(0, Mat); M->SetCastShadow(bShadow); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // ------------------------------------------------------------------ shapes
    void Toadstool(FMapSurf& S, const FVector& At, float H, float Cap, float Seed)
    {
        const float Lean = .12f * FMath::Sin(Seed * 3.1f);
        const FVector Top = At + FVector(Lean * H, .7f * Lean * H, H);
        const FVector Pts[] = {At - FVector(0, 0, 1.f), FMath::Lerp(At, Top, .5f), Top};
        S.Pipe(Pts, Cap * .22f, Cap * .17f, 6, 2, Col(.86f, .82f, .7f, .03f), Seed);
        const FVector2D Dome[] = {{0.f, Cap * .62f}, {Cap * .45f, Cap * .55f}, {Cap * .82f, Cap * .32f}, {Cap, Cap * .05f}, {Cap * .9f, -Cap * .06f}, {Cap * .3f, -Cap * .1f}, {0.f, -Cap * .12f}};
        const float Tone = .5f + .5f * FMath::Sin(Seed * 7.7f);
        S.Lathe(Top, Dome, 10, Col(FMath::Lerp(.85f, .78f, Tone), FMath::Lerp(.74f, .62f, Tone), FMath::Lerp(.56f, .44f, Tone), .07f));
    }
    void Puffball(FMapSurf& S, float R, bool bGone)
    {
        const float Rz = bGone ? R * .38f : R * .82f, Rr = bGone ? R * 1.1f : R;
        Blob(S, FVector(0, 0, Rz - 1.f), FVector::ForwardVector, FVector::RightVector, FVector::UpVector, Rr, Rr, Rz, bGone ? Col(.55f, .48f, .32f) : Col(.92f, .88f, .74f, .03f), 6);
        if (bGone) Octa(S, FVector(0, 0, Rz * 2.f - 1.f), R * .28f, Col(.32f, .26f, .16f));   // (its torn mouth)
    }
    void SeedHead(FMapSurf& S, float H, bool bGone, float Seed)
    {
        const FVector Top(2.f * FMath::Sin(Seed), 1.5f * FMath::Cos(Seed), H);
        const FVector Pts[] = {FVector(0, 0, -1.f), FVector(.8f, 0, H * .5f), Top};
        S.Pipe(Pts, .9f, .6f, 5, 3, Col(.45f, .55f, .26f), Seed);
        Octa(S, Top, 1.6f, Col(.6f, .52f, .3f));
        if (bGone) return;
        FRandomStream R(int32(Seed * 1000.f));
        for (int32 I = 0; I < 46; ++I)
        {
            const FVector D = R.GetUnitVector(); if (D.Z < -.7f) continue;
            const FVector Tip = Top + D * 7.f;
            const FVector Line[] = {Top + D * 1.6f, Tip};
            S.Pipe(Line, .12f, .1f, 3, 1, Col(.92f, .92f, .88f, .02f), I);
            Octa(S, Tip, .55f, Col(.97f, .97f, .94f, .05f));
        }
    }
    // A mute swan (about 1.4 m long, 2.2 m across the wings), its body along +X; the wings built apart at the shoulders.
    void MakeSwanBody(FMapSurf& S)
    {
        const FVector F(1, 0, 0), Rt(0, 1, 0), Up(0, 0, 1);
        const FColor White = Col(.92f, .92f, .9f, .015f), Beak = Col(.85f, .32f, .05f), Black = Col(.03f, .03f, .03f);
        Blob(S, FVector::ZeroVector, F, Rt, Up, 48.f, 20.f, 17.f, White);
        Blob(S, FVector(-46.f, 0, 2.f), F, Rt, Up, 12.f, 9.f, 4.f, White, 4);                       // the tail
        const FVector Neck[] = {FVector(40.f, 0, 4.f), FVector(64.f, 0, 8.f), FVector(88.f, 0, 10.f), FVector(104.f, 0, 10.f)};   // (flying: stretched out ahead)
        S.Pipe(Neck, 6.5f, 4.2f, 7, 4, White, 0.f);
        Blob(S, FVector(110.f, 0, 11.f), F, Rt, Up, 7.f, 4.5f, 4.5f, White, 4);
        Blob(S, FVector(118.5f, 0, 9.5f), F, Rt, Up, 5.5f, 2.2f, 1.8f, Beak, 3);
        Blob(S, FVector(113.f, 0, 10.5f), F, Rt, Up, 2.f, 2.6f, 2.f, Black, 3);                    // (the black knob)
        Blob(S, FVector(-8.f, 0, -16.f), F, Rt, Up, 14.f, 3.f, 3.f, Black, 3);                      // (feet tucked back)
    }
    void MakeSwanWing(FMapSurf& S, float Side)
    {
        const FColor White = Col(.93f, .93f, .91f, .015f);
        const FVector P[] = {FVector(18.f, 0, 0), FVector(14.f, Side * 50.f, 2.f), FVector(-2.f, Side * 108.f, 0), FVector(-30.f, Side * 104.f, -1.f), FVector(-34.f, Side * 60.f, 0), FVector(-24.f, Side * 8.f, 0)};
        Flat(S, P, White);
    }
    // A carrion crow (about 46 cm), its body along +X, perched upright-ish; the wings apart at the shoulders.
    void MakeCrowBody(FMapSurf& S)
    {
        const FVector F(1, 0, 0), Rt(0, 1, 0), Up(0, 0, 1);
        const FColor Black = Col(.035f, .035f, .045f), Beak = Col(.02f, .02f, .02f), Eye = Col(.15f, .12f, .05f, .2f);
        Blob(S, FVector(0, 0, 0), F, Rt, Up, 13.f, 7.f, 7.5f, Black);
        Blob(S, FVector(13.f, 0, 5.5f), F, Rt, Up, 5.5f, 4.6f, 4.8f, Black, 4);
        Blob(S, FVector(20.5f, 0, 4.5f), F, Rt, Up, 4.5f, 1.6f, 1.6f, Beak, 3);
        for (const float Sd : {1.f, -1.f}) Octa(S, FVector(15.f, Sd * 3.9f, 6.8f), .7f, Eye);
        const FVector Tail[] = {FVector(-10.f, 0, 0), FVector(-27.f, 5.f, -3.f), FVector(-29.f, 0, -3.5f), FVector(-27.f, -5.f, -3.f)};
        Flat(S, Tail, Black);
        for (const float Sd : {1.f, -1.f})   // legs
        {
            const FVector Leg[] = {FVector(1.f, Sd * 2.5f, -5.f), FVector(2.f, Sd * 2.5f, -12.f)};
            S.Pipe(Leg, .6f, .5f, 4, 1, Beak, Sd);
        }
    }
    void MakeCrowWing(FMapSurf& S, float Side)
    {
        const FColor Black = Col(.03f, .03f, .04f);
        const FVector P[] = {FVector(6.f, 0, 0), FVector(4.f, Side * 18.f, 0), FVector(-6.f, Side * 42.f, 0), FVector(-18.f, Side * 38.f, 0), FVector(-17.f, Side * 14.f, 0), FVector(-10.f, Side * 2.f, 0)};
        Flat(S, P, Black);
    }
    // Where the ground (or a branch, a roof) is under (X, Y): a trace down; the map's land if it finds nothing level.
    bool GroundUnder(UWorld* World, int32 D, float X, float Y, float& Z, float Above = 500.f, float MinUp = .75f)
    {
        const float Land = LandZ(D, X, Y);
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyDelightGround), true);
        if (World && World->LineTraceSingleByChannel(Hit, FeyWorld(FVector(X, Y, Land + Above)), FeyWorld(FVector(X, Y, Land - 400.f)), ECC_WorldStatic, Q))
        {
            if (Hit.ImpactNormal.Z < MinUp) return false;
            Z = FeyLocal(Hit.ImpactPoint).Z; return true;
        }
        Z = Land; return true;
    }
}

int32 AFeyDelights::RingCigarettes = 0;
int32 AFeyDelights::RingVisits = 0, AFeyDelights::WispMoves = 0, AFeyDelights::KoiScatters = 0, AFeyDelights::Bursts = 0, AFeyDelights::SignTurns = 0, AFeyDelights::CrowFlights = 0;
float AFeyDelights::MusicDip = 1.f, AFeyDelights::SwanHold = -1.f;
const TArray<TWeakObjectPtr<AFeyDelights>>& AFeyDelights::All() { return GDelights; }

const TArray<FVector>& AFeyDelights::Clearings()
{
    static const TArray<FVector> S = []()
    {
        TArray<FVector> Out;
        for (int32 I = 0; I < UE_ARRAY_COUNT(RingSpots); ++I) { const FVector2D R = RingGround(I); Out.Add(FVector(R.X, R.Y, RingR + 260.f)); }
        for (const FVector2D& W : WispPoints) Out.Add(FVector(W, 300.f));   // (the wisps' way kept open)
        // (2026-11-25) and between the points too, every 2.5 m: something solid planted mid-way between the second and third
        // stopped him straight under the lead wisp on every run.
        for (int32 K = 0; K + 1 < UE_ARRAY_COUNT(WispPoints); ++K)
        {
            const FVector2D A = WispPoints[K], B = WispPoints[K + 1]; const int32 N = FMath::CeilToInt(FVector2D::Distance(A, B) / 250.f);
            for (int32 I = 1; I < N; ++I) Out.Add(FVector(FMath::Lerp(A, B, I / float(N)), 300.f));
        }
        Out.Add(FVector(WispPoints[UE_ARRAY_COUNT(WispPoints) - 1], 380.f));   // (the cigarettes' shore)
        return Out;
    }();
    return S;
}

AFeyDelights::AFeyDelights()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetActorEnableCollision(false);
}

void AFeyDelights::EndPlay(const EEndPlayReason::Type Reason)
{
    for (AActor* A : RingCigs) if (IsValid(A)) A->Destroy();   // (gone with the district)
    GDelights.RemoveAll([this](const TWeakObjectPtr<AFeyDelights>& P) { return !P.IsValid() || P.Get() == this; });
    if (Rings.Num()) MusicDip = 1.f;
    for (AActor* A : Spawned) if (IsValid(A)) A->Destroy();
    Super::EndPlay(Reason);
}

UProceduralMeshComponent* AFeyDelights::NewMesh(USceneComponent* Parent, const FName& Name)
{
    auto* M = NewObject<UProceduralMeshComponent>(this, Name); M->SetupAttachment(Parent ? Parent : RootComponent.Get());
    M->bUseAsyncCooking = false; M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->RegisterComponent(); return M;
}

void AFeyDelights::Play(int32 Which, const FVector& Local, float Volume, float Pitch)
{
    if (Sfx.IsValidIndex(Which) && Sfx[Which]) UGameplayStatics::PlaySoundAtLocation(this, Sfx[Which], FeyWorld(Local), Volume, Pitch * FMath::FRandRange(.96f, 1.04f), 0.f, Falloff);
}

void AFeyDelights::Build(int32 D)
{
    District = D; GDelights.Add(this);
    for (const TCHAR* Path : MatPaths) Mats.Add(LoadObject<UMaterialInterface>(nullptr, Path));
    if (Mats[MPlant]) { auto* Still = UMaterialInstanceDynamic::Create(Mats[MPlant], this); Still->SetScalarParameterValue(TEXT("Sway"), 0.f); Mats[MPlant] = Still; }
    if (Mats[MRainbow]) { auto* Bow = UMaterialInstanceDynamic::Create(Mats[MRainbow], this); Bow->SetVectorParameterValue(TEXT("Anti"), FLinearColor(AntiSun.X, AntiSun.Y, AntiSun.Z, 0)); Mats[MRainbow] = Bow; }
    for (const TCHAR* Name : SfxPaths) Sfx.Add(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name)));
    Falloff = NewObject<USoundAttenuation>(this); Falloff->Attenuation.bAttenuate = Falloff->Attenuation.bSpatialize = true;
    Falloff->Attenuation.AttenuationShape = EAttenuationShape::Sphere; Falloff->Attenuation.AttenuationShapeExtents = FVector(150.f); Falloff->Attenuation.FalloffDistance = 2600.f;
    MoteMesh = NewMesh(nullptr, TEXT("Motes"));
    // A part a frame (as AFeyLife), never all at once.
    Pending.Add([this]() { BuildRings(); BuildSigns(); });
    Pending.Add([this]() { BuildPuffs(); });
    Pending.Add([this]() { BuildWisps(); BuildKoi(); });
    Pending.Add([this]() { BuildSwans(); BuildRainbow(); });
    Pending.Add([this]() { BuildCrows(); });
    Pending.Add([this]()
    {
        if (auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) Respawns = Chuck->GetRespawns();
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_DELIGHTS_BUILT district=%s rings=%d puffs=%d wisps=%d koi=%d swans=%d rainbow=%d crows=%d signs=%d missing=%d"), FeyMap::District(District).Name, Rings.Num(), Puffs.Num(), Wisps.Num(), Koi.Num(), SwanBody.Num(), !RainbowAt.IsZero(), Crows.Num(), Signs.Num(),
            Mats.FilterByPredicate([](const UMaterialInterface* M) { return !M; }).Num() + Sfx.FilterByPredicate([](const USoundBase* S) { return !S; }).Num());
    });
}

// ------------------------------------------------------------------ building
void AFeyDelights::BuildRings()
{
    for (int32 I = 0; I < UE_ARRAY_COUNT(RingSpots); ++I)
    {
        if (RingDistrict[I] != District) continue;
        const FVector2D G = RingGround(I);
        FRing R; float Z; GroundUnder(GetWorld(), District, G.X, G.Y, Z); R.C = FVector(G.X, G.Y, Z); R.R = RingR;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_FAIRY_RING_AT district=%s at=%.0f,%.0f moved=%.0f"), FeyMap::District(District).Name, G.X, G.Y, FVector2D::Distance(G, RingSpots[I]));
        FMapSurf S; FRandomStream Rs(900 + I);
        const int32 N = 17;
        for (int32 K = 0; K < N; ++K)
        {
            const float A = 2 * PI * K / N + Rs.FRandRange(-.1f, .1f), Rr = RingR + Rs.FRandRange(-14.f, 14.f);
            const float X = R.C.X + Rr * FMath::Cos(A), Y = R.C.Y + Rr * FMath::Sin(A); float Gz = R.C.Z; GroundUnder(GetWorld(), District, X, Y, Gz, 120.f, .5f);
            const float Big = K % 5 == 2 ? 1.5f : 1.f;
            Toadstool(S, FVector(X, Y, Gz) - R.C, Rs.FRandRange(7.f, 12.f) * Big, Rs.FRandRange(4.5f, 7.f) * Big, K * 1.37f + I);
            if (Rs.FRand() < .45f) { const float B = A + Rs.FRandRange(.04f, .12f); Toadstool(S, FVector(R.C.X + (Rr - 9.f) * FMath::Cos(B), R.C.Y + (Rr - 9.f) * FMath::Sin(B), Gz) - R.C, Rs.FRandRange(4.f, 6.f), Rs.FRandRange(2.5f, 3.6f), K * 2.1f); }
        }
        auto* M = NewMesh(nullptr, *FString::Printf(TEXT("FairyRing%d"), I)); M->SetRelativeLocation(R.C); Emit(M, S, Mats[MPlant], true);
        Rings.Add(R);
    }
}

void AFeyDelights::BuildSigns()
{
    if (!FeyMap::SignsTurn(District)) return;
    TArray<FeyMap::FSignOut> Posts; FeyMap::SignPostsOf(District, Posts);
    for (const FeyMap::FSignOut& Sp : Posts)
    {
        FSign Sg; Sg.At = FVector(Sp.At.X, Sp.At.Y, LandZ(District, Sp.At.X, Sp.At.Y)); Sg.Arrows = Sp.Arrows;
        for (int32 I = 0; I < Sp.Arrows.Num(); ++I)
        {
            auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
            Pivot->SetRelativeLocationAndRotation(FVector(Sg.At.X, Sg.At.Y, Sg.At.Z + FeyMap::SignArrowZ(I)), FRotator(0, Sp.Arrows[I].X, 0));
            FMapSurf S; FeyMap::SignBoardSurf(S, FVector::ZeroVector, 0.f, Sp.Arrows[I].Y, FeyMap::SignSeed(Sp.At, I));
            auto* M = NewMesh(Pivot, *FString::Printf(TEXT("SignArrow%d_%d"), Signs.Num(), I)); Emit(M, S, Mats[MSign], true);
            SignBoards.Add(Pivot); Sg.Yaw.Add(Sp.Arrows[I].X);
        }
        Signs.Add(Sg);
    }
}

void AFeyDelights::BuildPuffs()
{
    FRandomStream R(7300 + District);
    int32 Side = 1;
    TArray<const TArray<FVector2D>*> Lines;
    for (int32 T = 0; T < Thresholds; ++T) if (RoadOf(District, T).Num() > 1) Lines.Add(&RoadOf(District, T));
    if (SpurOf(District).Num() > 1) Lines.Add(&SpurOf(District));
    const FDistrict& Ds = FeyMap::District(District);
    for (const TArray<FVector2D>* L : Lines)
        for (int32 I = 0; I + 1 < L->Num() && Puffs.Num() < 36; ++I)
        {
            const FVector2D A = (*L)[I], B = (*L)[I + 1]; const float Len = FVector2D::Distance(A, B); const FVector2D Tn = (B - A).GetSafeNormal(), Pn(-Tn.Y, Tn.X);
            for (float U = 900.f + R.FRandRange(0.f, 900.f); U < Len - 600.f && Puffs.Num() < 36; U += R.FRandRange(1700.f, 2600.f))
            {
                Side = -Side;
                const FVector2D C = A + Tn * U + Pn * (Side * R.FRandRange(260.f, 480.f));
                if (C.X < Ds.X0 + 400.f || C.X > Ds.X1 - 400.f || C.Y < Ds.Y0 + 400.f || C.Y > Ds.Y1 - 400.f) continue;
                const int32 N = R.RandRange(2, 4);
                for (int32 K = 0; K < N; ++K)
                {
                    const FVector2D Q = C + FVector2D(R.FRandRange(-90.f, 90.f), R.FRandRange(-90.f, 90.f));
                    if (InDeepWater(Q.X, Q.Y) || IsPad(Q.X, Q.Y) || FeyMap::InRiver(District, Q.X, Q.Y)) continue;
                    float Z; if (!GroundUnder(GetWorld(), District, Q.X, Q.Y, Z, 200.f)) continue;
                    if (FMath::Abs(Z - LandZ(District, Q.X, Q.Y)) > 60.f) continue;   // (not on a rock or a root)
                    FPuff P; P.P = FVector(Q.X, Q.Y, Z); P.bSeed = R.FRand() < .42f; P.Size = R.FRandRange(.8f, 1.25f); Puffs.Add(P);
                }
            }
        }
    for (int32 I = 0; I < Puffs.Num(); ++I) { auto* M = NewMesh(nullptr, *FString::Printf(TEXT("Puff%d"), I)); M->SetRelativeLocation(Puffs[I].P); PuffMeshes.Add(M); ShowPuff(I); }
}

void AFeyDelights::ShowPuff(int32 I)
{
    const FPuff& P = Puffs[I]; FMapSurf S;
    if (P.bSeed) SeedHead(S, 34.f * P.Size, P.Gone >= 0.f, I * 1.7f);
    else Puffball(S, 10.f * P.Size, P.Gone >= 0.f);
    Emit(PuffMeshes[I], S, Mats[MPlant], true);
}

void AFeyDelights::BuildWisps()
{
    if (District != LilyMere) return;
    for (const FVector2D& W : WispPoints) { float Z; GroundUnder(GetWorld(), District, W.X, W.Y, Z); WispWay.Add(FVector(W.X, W.Y, Z)); }
    for (int32 K = 0; K < 3; ++K)
    {
        FWisp W; W.At = W.From = WispWay[FMath::Min(K, WispWay.Num() - 1)] + FVector(0, 0, 95.f + 20.f * K); Wisps.Add(W);
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent(); Pivot->SetRelativeLocation(W.At);
        FMapSurf S;
        Blob(S, FVector::ZeroVector, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, 3.2f, 3.2f, 3.8f, Col(.7f, .92f, 1.f, .5f), 5);
        Blob(S, FVector::ZeroVector, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, 9.f, 9.f, 10.f, Col(.3f, .62f, .85f, .12f), 5);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("Wisp%d"), K)), S, Mats[MPlant]);
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(Pivot); L->SetLightColor(FLinearColor(.55f, .85f, 1.f)); L->SetIntensityUnits(ELightUnits::Candelas);
        L->SetIntensity(18.f); L->SetAttenuationRadius(420.f); L->SetCastShadows(false); L->RegisterComponent();   // (one light each)
        WispPivots.Add(Pivot); WispLights.Add(L);
    }
    // At the end of their way, by the hidden pool: cigarettes in the moss.
    const FVector E = WispWay.Last();
    for (const FVector2D& C : {FVector2D(-14.f, 6.f), FVector2D(11.f, 13.f), FVector2D(3.f, -15.f)})
        if (ACigarettePickup* Cg = ACigarettePickup::Spawn(GetWorld(), FeyWorld(FVector(E.X + C.X, E.Y + C.Y, E.Z + 30.f)), FVector::ZeroVector, FeyWorld(E).Z)) Spawned.Add(Cg);
}

void AFeyDelights::BuildKoi()
{
    if (District != LilyMere || !Mats[MKoi]) return;
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")); if (!Plane) return;
    KoiMesh = NewObject<UInstancedStaticMeshComponent>(this); KoiMesh->SetupAttachment(RootComponent); KoiMesh->SetStaticMesh(Plane);
    KoiMesh->SetMaterial(0, Mats[MKoi]); KoiMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); KoiMesh->SetCastShadow(false); KoiMesh->NumCustomDataFloats = 2;
    KoiMesh->SetMobility(EComponentMobility::Movable); KoiMesh->RegisterComponent();
    const TArray<FVector>& Pads = FeyMap::LilyPads(); FRandomStream R(4242);
    for (int32 I = 0; I < Pads.Num() && Koi.Num() < 11; I += FMath::Max(1, Pads.Num() / 11))
    {
        FKoi K; K.Home = FVector2D(Pads[I].X, Pads[I].Y) + FVector2D(R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f)); K.R = R.FRandRange(110.f, 230.f);
        K.Phase = R.FRandRange(0.f, 2 * PI); K.Dir = R.FRand() < .5f ? 1.f : -1.f; K.At = K.Home;
        const int32 Ix = KoiMesh->AddInstance(FTransform::Identity);
        KoiMesh->SetCustomDataValue(Ix, 0, float(Koi.Num() % 4)); KoiMesh->SetCustomDataValue(Ix, 1, K.Phase);
        Koi.Add(K);
    }
}

void AFeyDelights::BuildSwans()
{
    if (District != Highmeadow) return;
    for (int32 I = 0; I < 7; ++I)
    {
        auto* Body = NewObject<USceneComponent>(this); Body->SetupAttachment(RootComponent); Body->SetMobility(EComponentMobility::Movable); Body->RegisterComponent();
        FMapSurf S; MakeSwanBody(S); Emit(NewMesh(Body, *FString::Printf(TEXT("Swan%d"), I)), S, Mats[MPlant]);
        for (const float Sd : {1.f, -1.f})
        {
            auto* Hinge = NewObject<USceneComponent>(this); Hinge->SetupAttachment(Body); Hinge->SetMobility(EComponentMobility::Movable); Hinge->RegisterComponent(); Hinge->SetRelativeLocation(FVector(14.f, Sd * 12.f, 8.f));
            FMapSurf W; MakeSwanWing(W, Sd); Emit(NewMesh(Hinge, *FString::Printf(TEXT("SwanWing%d_%d"), I, Sd > 0)), W, Mats[MPlant]);
            (Sd > 0 ? SwanWingR : SwanWingL).Add(Hinge);
        }
        SwanBody.Add(Body);
    }
}

FVector AFeyDelights::GetSwanAt(int32 I) const { return SwanAt(I, SwanHold >= 0.f ? SwanHold : SwanClock); }

FVector AFeyDelights::SwanAt(int32 I, float Time)
{
    // A slow line crossing below the Maze Overlook, north to south, 55 m out and 4 to 6 m under the brink (2026-11-25: was 92 m
    // out and 10 m under, specks from the brink; nearer at the same angle down, so the near ridge still does not hide them).
    const FVector2D O = FeyMap::OverlookAt(); const float Oz = LandZ(Highmeadow, O.X, O.Y);
    constexpr float Speed = 380.f, Y0 = 26000.f, Y1 = -16000.f, Gap = 45.f;
    const float Period = (Y0 - Y1) / Speed + Gap, T = FMath::Fmod(Time, Period);
    const float Y = Y0 - (T - I * 1.45f) * Speed;
    const float X = O.X - 5500.f - 200.f * I + 90.f * FMath::Sin(Y / 4100.f);
    // (rising 5 m over the threshold's rock mass at about Y 7200, which the nearer line would otherwise skim)
    const float Ridge = FMath::Clamp(1.f - FMath::Abs(Y - 7200.f) / 2600.f, 0.f, 1.f), Lift = 500.f * Ridge * Ridge * (3.f - 2.f * Ridge);
    return FVector(X, Y, Oz - 400.f - 25.f * I + 40.f * FMath::Sin(Y / 2600.f + I) + Lift);
}

void AFeyDelights::BuildRainbow()
{
    if (District != Highmeadow || !Mats[MRainbow]) return;
    const float X = FeyMap::RapidXAt(CascadeY);
    RainbowAt = FVector(X, CascadeY, FeyMap::RapidWLAt(CascadeY) + 230.f);
    // A soft sheet of spray facing the eye (turned each frame), the bow drawn in it only where the sun is behind the eye.
    FMapSurf S; constexpr int32 Nu = 12, Nv = 8; constexpr float W = 560.f, H = 300.f;
    for (int32 V = 0; V <= Nv; ++V) for (int32 U = 0; U <= Nu; ++U)
        S.Vert(FVector(0.f, (U / float(Nu) - .5f) * 2.f * W, (V / float(Nv) - .5f) * 2.f * H), FVector(1, 0, 0), FVector2D(U / float(Nu), V / float(Nv)), FColor::White);
    for (int32 V = 0; V < Nv; ++V) for (int32 U = 0; U < Nu; ++U) { const int32 Q = V * (Nu + 1) + U; S.Quad(Q, Q + 1, Q + Nu + 2, Q + Nu + 1); }
    Spray = NewMesh(nullptr, TEXT("RainbowSpray")); Spray->SetRelativeLocation(RainbowAt); Emit(Spray, S, Mats[MRainbow]);
}

void AFeyDelights::BuildCrows()
{
    if (District != Deepwood) return;
    FRandomStream R(6161);
    TArray<FVector> Perches;
    // Along the Deepwood's roads, a few metres off them: the first thing under a point (a limb, a root, a rock, the ground).
    TArray<const TArray<FVector2D>*> Lines;
    for (int32 T = 0; T < Thresholds; ++T) if (RoadOf(District, T).Num() > 1) Lines.Add(&RoadOf(District, T));
    if (SpurOf(District).Num() > 1) Lines.Add(&SpurOf(District));
    for (const TArray<FVector2D>* L : Lines)
        for (int32 I = 0; I + 1 < L->Num(); ++I)
        {
            const FVector2D A = (*L)[I], B = (*L)[I + 1]; const float Len = FVector2D::Distance(A, B); const FVector2D Tn = (B - A).GetSafeNormal(), Pn(-Tn.Y, Tn.X);
            for (float U = 1200.f; U < Len - 800.f; U += R.FRandRange(2600.f, 3800.f))
                for (int32 Try = 0; Try < 4; ++Try)
                {
                    const FVector2D Q = A + Tn * (U + R.FRandRange(-300.f, 300.f)) + Pn * (R.FRandRange(450.f, 900.f) * (R.FRand() < .5f ? 1.f : -1.f));
                    float Z; if (!GroundUnder(GetWorld(), District, Q.X, Q.Y, Z, 650.f, .55f)) continue;
                    Perches.Add(FVector(Q.X, Q.Y, Z)); break;
                }
        }
    // Two on the hag's cottage's ridge.
    {
        for (const float Along : {-160.f, 140.f})
        {
            const FVector P = FeyHagPoint(FVector(0.f, Along, 0.f)); float Z;
            if (GroundUnder(GetWorld(), District, P.X, P.Y, Z, 900.f, .3f) && Z > P.Z + 150.f) Perches.Add(FVector(P.X, P.Y, Z));
        }
    }
    // Ten crows; the other perches are where they fly to.
    for (int32 I = 0; I < Perches.Num() && Crows.Num() < 10; I += 2)
    {
        FCrow C; C.Perch = C.At = Perches[I]; C.Yaw = C.Look = R.FRandRange(0.f, 360.f); C.Seed = R.FRand() * 10.f; Crows.Add(C);
    }
    for (int32 I = 1; I < Perches.Num(); I += 2) Spare.Add(Perches[I]);   // (where they fly to)
    for (int32 I = 0; I < Crows.Num(); ++I)
    {
        auto* Body = NewObject<USceneComponent>(this); Body->SetupAttachment(RootComponent); Body->SetMobility(EComponentMobility::Movable); Body->RegisterComponent();
        Body->SetRelativeLocationAndRotation(Crows[I].At + FVector(0, 0, 12.f), FRotator(0, Crows[I].Yaw, 0));
        FMapSurf S; MakeCrowBody(S); Emit(NewMesh(Body, *FString::Printf(TEXT("Crow%d"), I)), S, Mats[MPlant], true);
        for (const float Sd : {1.f, -1.f})
        {
            auto* Hinge = NewObject<USceneComponent>(this); Hinge->SetupAttachment(Body); Hinge->SetMobility(EComponentMobility::Movable); Hinge->RegisterComponent(); Hinge->SetRelativeLocation(FVector(4.f, Sd * 5.f, 4.f));
            FMapSurf W; MakeCrowWing(W, Sd); Emit(NewMesh(Hinge, *FString::Printf(TEXT("CrowWing%d_%d"), I, Sd > 0)), W, Mats[MPlant], true);
            (Sd > 0 ? CrowWingR : CrowWingL).Add(Hinge);
        }
        CrowBody.Add(Body);
    }
}

// ------------------------------------------------------------------ living
void AFeyDelights::Burst(const FVector& At, int32 Count, float Speed, const FColor& Cl, float Rise)
{
    // (2026-11-26, the user: the spore clouds to look better) AFeySpores draws them now: glowing grains, and for a real burst
    // (a puffball, a seed head) billows of spore cloud on the CC0 WispySmoke flipbook. The old specks below go unused.
    if (AFeySpores* Sp = AFeySpores::Get(GetWorld()))
    {
        const FLinearColor Lc(Cl.R / 255.f, Cl.G / 255.f, Cl.B / 255.f);
        Sp->Puff(FeyWorld(At), Count, Speed, Lc, Rise, Count >= 30 ? 5 : 0, 60.f + Count * .7f);
        return;
    }
    for (int32 I = 0; I < Count && Motes.Num() < 600; ++I)
    {
        FMote M; M.P = At + FMath::VRand() * FMath::FRandRange(2.f, 10.f);
        const FVector D = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-.1f, .5f)).GetSafeNormal();
        M.V = D * Speed * FMath::FRandRange(.4f, 1.f) + FVector(0, 0, Rise * FMath::FRandRange(.6f, 1.2f));
        M.Life = FMath::FRandRange(2.f, 3.4f); M.Size = FMath::FRandRange(.9f, 1.8f); M.C = Cl; Motes.Add(M);
    }
}

bool AFeyDelights::TryScratch(const FVector& Location, const FVector& Ahead, float Reach)
{
    const FVector L = FeyLocal(Location); bool bAny = false;
    for (int32 I = 0; I < Puffs.Num(); ++I)
    {
        FPuff& P = Puffs[I]; if (P.Gone >= 0.f) continue;
        const FVector To = P.P - L;
        if (To.Size2D() > Reach + 20.f || FMath::Abs(To.Z + Stand) > 70.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead.GetSafeNormal2D()) < .1f && To.Size2D() > 30.f) continue;
        P.Gone = 0.f; ShowPuff(I); ++Bursts; bAny = true;
        Burst(P.P + FVector(0, 0, P.bSeed ? 34.f * P.Size : 8.f), 70, 130.f, Col(1.f, .8f, .28f, .55f), 55.f); Play(SPuff, P.P, .8f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_SPORES district=%s puff=%d seed=%d scratched=1"), FeyMap::District(District).Name, I, P.bSeed);
    }
    return bAny;
}

void AFeyDelights::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Pending.Num()) { TFunction<void()> F = MoveTemp(Pending[0]); Pending.RemoveAt(0); F(); return; }
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, .05f); Clock += Dt; SwanClock += Dt;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); if (!Chuck) return;
    const FVector C = FeyLocal(Chuck->GetActorLocation()), Feet = C - FVector(0, 0, Stand);
    const bool bGrounded = Chuck->GetCharacterMovement()->IsMovingOnGround();
    if (Chuck->GetRespawns() != Respawns)
    {
        Respawns = Chuck->GetRespawns(); Lead = 0;
        for (int32 K = 0; K < Wisps.Num(); ++K) { Wisps[K].At = Wisps[K].From = WispWay[FMath::Min(K, WispWay.Num() - 1)] + FVector(0, 0, 95.f + 20.f * K); Wisps[K].Move = 1.f; }
        for (int32 I = 0; I < Puffs.Num(); ++I) if (Puffs[I].Gone >= 0.f) { Puffs[I].Gone = -1.f; ShowPuff(I); }
    }
    APlayerController* PC = GetWorld()->GetFirstPlayerController(); FVector Eye = Chuck->GetActorLocation(); FRotator View = Chuck->GetActorRotation();
    if (PC) PC->GetPlayerViewPoint(Eye, View);
    const FVector EyeL = FeyLocal(Eye), Look = View.Vector();

    // Fairy rings: inside, the music dips, a faint bell (not again for a while) and motes rising round him.
    int32 Now = -1;
    for (int32 I = 0; I < Rings.Num(); ++I) if (FVector::Dist2D(Feet, Rings[I].C) < Rings[I].R - 25.f && FMath::Abs(Feet.Z - Rings[I].C.Z) < 90.f) Now = I;
    BellWait -= Dt;
    if (Now >= 0 && InRing < 0)
    {
        ++RingVisits; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_FAIRY_RING district=%s ring=%d"), FeyMap::District(District).Name, Now);
        if (BellWait <= 0.f) { Play(SBell, Feet + FVector(0, 0, 60.f), .55f); BellWait = 9.f; }
        // (2026-11-26, the user: when the ring does its thing, a cigarette appears) the first time in each ring, one rises out
        // of the grass at its middle in a puff of motes, and falls back to lie there for him.
        if (!Rings[Now].bGave)
        {
            Rings[Now].bGave = true;
            const FVector Mid = Rings[Now].C;
            if (ACigarettePickup* Cg = ACigarettePickup::Spawn(GetWorld(), FeyWorld(Mid + FVector(0, 0, 20.f)), FVector(0, 0, 260.f), FeyWorld(Mid).Z)) RingCigs.Add(Cg);
            Burst(Mid + FVector(0, 0, 30.f), 26, 60.f, Col(.75f, 1.f, .7f, .55f), 50.f);
            ++RingCigarettes;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_RING_CIGARETTE district=%s ring=%d"), FeyMap::District(District).Name, Now);
        }
        for (int32 K = 0; K < 28; ++K) { const float A = FMath::FRandRange(0.f, 2 * PI), Rr = FMath::FRandRange(40.f, 140.f); Burst(Feet + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), 4.f), 1, 6.f, Col(.75f, 1.f, .7f, .5f), 38.f); }
    }
    if (Now >= 0 && FMath::FRand() < Dt * 6.f) { const float A = FMath::FRandRange(0.f, 2 * PI), Rr = FMath::FRandRange(40.f, 150.f); Burst(Feet + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), 4.f), 1, 5.f, Col(.75f, 1.f, .7f, .5f), 34.f); }
    InRing = Now;
    if (Rings.Num()) MusicDip = FMath::FInterpConstantTo(MusicDip, Now >= 0 ? .35f : 1.f, Dt, Now >= 0 ? .55f : .4f);

    // Puffballs and seed heads: walked through, they burst too; they grow back.
    for (int32 I = 0; I < Puffs.Num(); ++I)
    {
        FPuff& P = Puffs[I];
        if (P.Gone >= 0.f)
        {
            P.Gone += Dt;
            if (P.Gone > 50.f && FVector::Dist2D(P.P, Feet) > 1200.f) { P.Gone = -1.f; ShowPuff(I); }
            continue;
        }
        if (bGrounded && FVector::Dist2D(P.P, Feet) < 26.f + 8.f * P.Size && FMath::Abs(P.P.Z - Feet.Z) < 50.f && Chuck->GetVelocity().Size2D() > 30.f)
        {
            P.Gone = 0.f; ShowPuff(I); ++Bursts;
            Burst(P.P + FVector(0, 0, P.bSeed ? 30.f : 8.f), 55, 105.f, Col(1.f, .8f, .28f, .55f), 50.f); Play(SPuff, P.P, .65f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_SPORES district=%s puff=%d seed=%d scratched=0"), FeyMap::District(District).Name, I, P.bSeed);
        }
    }

    // Wisps: the nearest drifts on as he comes near, the others after it; at the end they settle over the cigarettes.
    if (Wisps.Num())
    {
        if (Lead + 1 < WispWay.Num() && Wisps[0].Move >= .8f && FVector::Dist2D(Feet, Wisps[0].At) < 620.f)
        {
            ++Lead; ++WispMoves;
            for (int32 K = 0; K < Wisps.Num(); ++K) { Wisps[K].From = Wisps[K].At; Wisps[K].Move = 0.f; }
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_WISPS lead=%d"), Lead);
        }
        for (int32 K = 0; K < Wisps.Num(); ++K)
        {
            FWisp& W = Wisps[K];
            const bool bEnd = Lead + K >= WispWay.Num() - 1;
            const FVector Settle = bEnd ? FVector(28.f * FMath::Cos(K * 2.1f), 28.f * FMath::Sin(K * 2.1f), 70.f + 18.f * K) : FVector(0, 0, 95.f + 20.f * K);
            const FVector To = WispWay[FMath::Min(Lead + K, WispWay.Num() - 1)] + Settle;
            W.Move = FMath::Min(1.f, W.Move + Dt / (3.2f + .5f * K));
            const float E = Smooth(0.f, 1.f, W.Move);
            W.At = FMath::Lerp(W.From, To, E) + FVector(0, 0, 70.f * FMath::Sin(PI * E));
            const FVector Bob(9.f * FMath::Sin(Clock * .9f + K), 9.f * FMath::Cos(Clock * .7f + K * 2.f), 11.f * FMath::Sin(Clock * 1.3f + K * 1.4f));
            WispPivots[K]->SetRelativeLocation(W.At + Bob);
            WispLights[K]->SetIntensity(18.f * (.8f + .2f * FMath::Sin(Clock * 2.3f + K)));
        }
    }

    // Koi: slow circles under their pads; a landing near them scatters them, then they drift back.
    if (KoiMesh && Koi.Num())
    {
        const bool bLanded = bGrounded && !bWasGrounded;
        const bool bNear = FVector::Dist2D(Feet, FVector(Koi[0].Home, 0.f)) < 8000.f;
        for (int32 I = 0; I < Koi.Num(); ++I)
        {
            FKoi& K = Koi[I];
            if (bLanded && FVector2D::Distance(K.At, FVector2D(Feet)) < 520.f && K.Flee <= 0.f)
            { K.Flee = 1.4f; K.Away = (K.At - FVector2D(Feet)).GetSafeNormal(); if (K.Away.IsNearlyZero()) K.Away = FVector2D(1, 0); ++KoiScatters; if (I % 3 == 0) Play(SSplash, FVector(K.At, 0.f), .35f, 1.3f); }
            K.Phase += Dt * K.Dir * 55.f / K.R;
            const FVector2D Circle = K.Home + FVector2D(FMath::Cos(K.Phase), FMath::Sin(K.Phase)) * K.R;
            FVector2D V;
            if (K.Flee > 0.f) { K.Flee -= Dt; V = K.Away * 620.f * Smooth(0.f, .5f, K.Flee); }
            else V = (Circle - K.At) * .9f;
            const FVector2D Was = K.At; K.At += V * Dt;
            if ((K.At - Was).SizeSquared() > .01f) K.Yaw = FMath::RadiansToDegrees(FMath::Atan2(K.At.Y - Was.Y, K.At.X - Was.X));
            if (bNear) KoiMesh->UpdateInstanceTransform(I, FTransform(FRotator(0, K.Yaw, 0), FVector(K.At, .25f), FVector(.62f, .26f, 1.f)), false, I == Koi.Num() - 1, true);
        }
    }
    bWasGrounded = bGrounded;

    // Swans: a slow line, wings beating slowly.
    for (int32 I = 0; I < SwanBody.Num(); ++I)
    {
        const float T = SwanHold >= 0.f ? SwanHold : SwanClock; const FVector P = SwanAt(I, T), P2 = SwanAt(I, T + .2f);
        const FVector Dir = (P2 - P).GetSafeNormal();
        SwanBody[I]->SetRelativeLocationAndRotation(P, FRotator(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f))), Dir.Rotation().Yaw, 0.f));
        const float Beat = 30.f * FMath::Sin(SwanClock * 2.3f * PI + I * .8f);
        SwanWingR[I]->SetRelativeRotation(FRotator(0, 0, Beat)); SwanWingL[I]->SetRelativeRotation(FRotator(0, 0, -Beat));
    }

    if (Spray) Spray->SetRelativeRotation(FRotator(0, (EyeL - RainbowAt).Rotation().Yaw, 0));   // (the spray turned to the eye)

    TickCrows(Dt, C, Chuck);

    // Signposts: while he is not looking, an arrow on a post near him turns somewhere else.
    for (int32 S = 0, Bi = 0; S < Signs.Num(); Bi += Signs[S].Arrows.Num(), ++S)
    {
        FSign& Sg = Signs[S];
        const FVector Top = Sg.At + FVector(0, 0, 130.f); const float D = FVector::Dist2D(Top, C);
        if (D > 3200.f || D < 250.f) continue;
        const bool bSeen = FVector::DotProduct((Top - EyeL).GetSafeNormal(), Look) > .3f;
        if (bSeen || FMath::FRand() > Dt * .35f) continue;
        const int32 A = FMath::RandRange(0, Sg.Arrows.Num() - 1);
        const float To = FMath::FRand() < .4f ? (C - Sg.At).Rotation().Yaw + FMath::FRandRange(-20.f, 20.f) : Sg.Yaw[A] + FMath::FRandRange(70.f, 290.f);
        Sg.Yaw[A] = FRotator::NormalizeAxis(To); SignBoards[Bi + A]->SetRelativeRotation(FRotator(0, Sg.Yaw[A], 0)); ++SignTurns;
    }

    // The motes (spores, the rings' lights): drift, slow, fade.
    if (MoteMesh)
    {
        FMapSurf S;
        for (int32 I = Motes.Num() - 1; I >= 0; --I)
        {
            FMote& M = Motes[I]; M.Age += Dt; if (M.Age >= M.Life) { Motes.RemoveAtSwap(I); continue; }
            M.V *= FMath::Pow(.25f, Dt); M.V.Z += 8.f * Dt;
            M.P += M.V * Dt + FVector(6.f * FMath::Sin(Clock * 1.7f + I), 6.f * FMath::Cos(Clock * 1.3f + I * .7f), 0.f) * Dt;
            const float A = FMath::Sin(PI * FMath::Min(1.f, M.Age / M.Life)) * (M.C.A / 255.f);
            Octa(S, M.P, M.Size, FColor(M.C.R, M.C.G, M.C.B, uint8(255.f * A)));
        }
        if (S.V.Num() || MoteMesh->GetNumSections()) { if (S.V.Num()) Emit(MoteMesh, S, Mats[MPlant]); else MoteMesh->ClearAllMeshSections(); }
    }
}

void AFeyDelights::TickCrows(float Dt, const FVector& C, AChuckCharacter* Chuck)
{
    if (!Crows.Num()) return;
    for (int32 I = 0; I < Crows.Num(); ++I)
    {
        FCrow& K = Crows[I];
        const float D = FVector::Dist2D(K.At, C);
        if (K.Fly < 0.f)
        {
            // Perched: it turns to watch him while he is near, with small jerks of the head and body.
            if (D < 2200.f)
            {
                const float Want = (C - K.At).Rotation().Yaw;
                if (FMath::Abs(FMath::FindDeltaAngleDegrees(K.Yaw, Want)) > 35.f && FMath::FRand() < Dt * 1.2f) K.Yaw = Want + FMath::FRandRange(-15.f, 15.f);
            }
            else if (FMath::FRand() < Dt * .15f) K.Yaw += FMath::FRandRange(-60.f, 60.f);
            K.Look = FMath::FixedTurn(K.Look, K.Yaw, 400.f * Dt);
            // Off: when he comes close, or now and then one of those that can see him; with a call.
            const bool bStartle = D < 380.f, bWhim = D < 2600.f && FMath::FRand() < Dt / 40.f;
            if ((bStartle || bWhim) && Spare.Num())
            {
                int32 Best = -1; float Bd = 1e9f;
                for (int32 P = 0; P < Spare.Num(); ++P)
                {
                    const float Dp = FVector::Dist2D(Spare[P], K.At);
                    if (Dp > 900.f && Dp < 4500.f && FVector::Dist2D(Spare[P], C) > 900.f && Dp < Bd) { Bd = Dp; Best = P; }
                }
                if (Best >= 0)
                {
                    const FVector Old = K.Perch; K.Away = Old; K.Perch = Spare[Best]; Spare[Best] = Old; K.Fly = 0.f; ++CrowFlights;
                    Play(FMath::RandBool() ? SCaw0 : SCaw1, K.At + FVector(0, 0, 20.f), .8f, FMath::FRandRange(.93f, 1.07f)); Play(SFlap, K.At, .45f, 1.25f);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_CROW_FLIGHT crow=%d startled=%d"), I, bStartle);
                }
            }
            CrowBody[I]->SetRelativeLocationAndRotation(K.At + FVector(0, 0, 12.f), FRotator(12.f, K.Look, 0));
            CrowWingR[I]->SetRelativeRotation(FRotator(0, 78.f, 6.f)); CrowWingL[I]->SetRelativeRotation(FRotator(0, -78.f, -6.f));   // (folded back along the body)
            if (FMath::FRand() < Dt / 25.f && D < 2600.f) Play(FMath::RandBool() ? SCaw0 : SCaw1, K.At + FVector(0, 0, 20.f), .5f, FMath::FRandRange(.9f, 1.1f));
        }
        else
        {
            // Flying (as Chult's birds: flapping hard as it climbs, a glide, then braking down onto the new perch).
            const float Dist = FVector::Dist(K.Away, K.Perch), Dur = FMath::Clamp(Dist / 520.f, 2.5f, 7.f);
            K.Fly += Dt; const float U = FMath::Min(1.f, K.Fly / Dur), E = Smooth(0.f, 1.f, U);
            const FVector P = FMath::Lerp(K.Away, K.Perch, E) + FVector(0, 0, (350.f + Dist * .12f) * FMath::Sin(PI * U));
            const FVector V = (P - K.At) / FMath::Max(Dt, 1e-3f); K.At = P;
            if (V.SizeSquared2D() > 100.f) K.Look = V.Rotation().Yaw;
            const float Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(V.Z, V.Size2D())) * .6f, -25.f, 25.f);
            const float Flap = U < .35f || U > .85f ? 1.f : .25f, Beat = Flap * 50.f * FMath::Sin(K.Fly * 2 * PI * 4.2f) + 8.f;
            CrowBody[I]->SetRelativeLocationAndRotation(K.At + FVector(0, 0, 12.f), FRotator(Pitch, K.Look, 0));
            CrowWingR[I]->SetRelativeRotation(FRotator(0, 0, Beat)); CrowWingL[I]->SetRelativeRotation(FRotator(0, 0, -Beat));
            if (U >= 1.f) { K.Fly = -1.f; K.At = K.Perch; K.Yaw = K.Look; }
        }
    }
}
