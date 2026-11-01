#include "ShipSetting.h"
#include "ChuckCharacter.h"
#include "ChuckClimbable.h"
#include "ClayJar.h"
#include "EnemyRat.h"
#include "FallCutscene.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Info.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

// The pirate ship's setting (Claude, 2026-11-01; ShipSetting.h, docs/SHIP.md). Everything is built in the ship's frame
// (X to the bow, Y to port, Z up from the keel) on procedural meshes, one per material, from the existing wood, canvas,
// iron, temple stone, brick and glass materials; the cargo is the docks' crates, barrels and rope coils.
using namespace Ship;

namespace
{
    // ------------------------------------------------------------------ the hull's lines
    // Its plan at the deck (a fraction of HalfBeam): a parallel middle body, a full round bow, a transom stern.
    float Plan(float X)
    {
        if (X >= 0) { if (X <= 500) return 1; const float T = FMath::Clamp((X - 500) / (BowX - 500), 0.f, 1.f); return FMath::Pow(FMath::Max(0.f, 1 - FMath::Pow(T, 2.2f)), .55f); }
        if (X >= -700) return 1;
        const float T = FMath::Clamp((-700 - X) / (-700 - SternX), 0.f, 1.f);
        return 1 - .28f * FMath::Pow(T, 1.8f);
    }
    // The bottom rises toward the stem and, less, toward the transom.
    float BottomZ(float X)
    {
        if (X > 900) { const float T = (X - 900) / (BowX - 900); return 260 * T * T; }
        if (X < -1050) { const float T = (-1050 - X) / (-1050 - SternX); return 120 * T * T; }
        return 0;
    }
    constexpr float Shell = 10;      // the planking's thickness
    float OuterAt(float X, float Z)
    {
        if (X > BowX || X < SternX) return 0;
        const float B = BottomZ(X);
        if (Z < B) return 0;
        const float K = FMath::Clamp((Z - B) / FMath::Max(1.f, DeckZ - B), 0.f, 1.f);
        float H = HalfBeam * Plan(X) * (.62f + .38f * (1 - (1 - K) * (1 - K)));   // a full, flat-floored merchant's section
        if (Z > DeckZ) H *= 1 - .035f * (Z - DeckZ) / 100;   // a little tumblehome over the deck
        return H;
    }
    float InnerAt(float X, float Z) { return FMath::Max(0.f, OuterAt(X, Z) - Shell); }

    // ------------------------------------------------------------------ mesh building
    struct FSurf
    {
        TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
        // UVs by facing, so the grain runs along the ship on the hull and floors and across it on the bulkheads.
        void Vert(const FVector& P, const FVector& Nn)
        {
            V.Add(P); N.Add(Nn);
            if (FMath::Abs(Nn.Z) > .7f) UV.Add(FVector2D(P.X, P.Y) / 120);
            else if (FMath::Abs(Nn.X) > FMath::Abs(Nn.Y)) UV.Add(FVector2D(P.Y, P.Z) / 120);
            else UV.Add(FVector2D(P.X, P.Z) / 120);
            const FVector Ax = FMath::Abs(Nn.Z) > .95f ? FVector::ForwardVector : FVector::UpVector;
            Tan.Add(FProcMeshTangent(FVector::CrossProduct(Ax, Nn).GetSafeNormal(), false));
        }
        void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& ND)
        {
            const FVector G = FVector::CrossProduct(B - A, C - A) + FVector::CrossProduct(C - A, D - A);
            if (G.SizeSquared() < 1e-6f) return;
            const int32 I = V.Num(); Vert(A, NA); Vert(B, NB); Vert(C, NC); Vert(D, ND);
            if (FVector::DotProduct(G, NA + NB + NC + ND) >= 0) T.Append({I, I + 2, I + 1, I, I + 3, I + 2}); else T.Append({I, I + 1, I + 2, I, I + 2, I + 3});
        }
        void Flat(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Out) { Quad(A, B, C, D, Out, Out, Out, Out); }
        void Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Out)
        {
            const int32 I = V.Num(); Vert(A, Out); Vert(B, Out); Vert(C, Out);
            if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Out) >= 0) T.Append({I, I + 2, I + 1}); else T.Append({I, I + 1, I + 2});
        }
        void Box(const FVector& C, const FVector& X, const FVector& Y, const FVector& Z)
        {
            auto P = [&](float A, float B, float Cc) { return C + X * A + Y * B + Z * Cc; };
            const FVector Xn = X.GetSafeNormal(), Yn = Y.GetSafeNormal(), Zn = Z.GetSafeNormal();
            Flat(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Xn); Flat(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -Xn);
            Flat(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), Yn); Flat(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Yn);
            Flat(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Zn); Flat(P(-1, -1, -1), P(1, -1, -1), P(1, 1, -1), P(-1, 1, -1), -Zn);
        }
        // An axis-aligned box by its extents.
        void B(float X0, float X1, float Y0, float Y1, float Z0, float Z1)
        {
            if (X1 - X0 < .01f || Y1 - Y0 < .01f || Z1 - Z0 < .01f) return;
            Box(FVector((X0 + X1) * .5f, (Y0 + Y1) * .5f, (Z0 + Z1) * .5f), FVector((X1 - X0) * .5f, 0, 0), FVector(0, (Y1 - Y0) * .5f, 0), FVector(0, 0, (Z1 - Z0) * .5f));
        }
        void TurnedBox(const FVector& C, const FVector& H, const FRotator& R)
        {
            const FQuat Q(R);
            Box(C, Q.RotateVector(FVector(H.X, 0, 0)), Q.RotateVector(FVector(0, H.Y, 0)), Q.RotateVector(FVector(0, 0, H.Z)));
        }
        // A tapered round spar from A to B.
        void Spar(const FVector& A, const FVector& Bp, float RA, float RB, int32 Sides = 10, bool bCaps = true)
        {
            const FVector D = (Bp - A).GetSafeNormal(); FVector X = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();
            if (X.IsNearlyZero()) X = FVector::RightVector;
            const FVector Y = FVector::CrossProduct(D, X);
            auto Dir = [&](float K) { const float An = K * 2 * PI / Sides; return X * FMath::Cos(An) + Y * FMath::Sin(An); };
            for (int32 I = 0; I < Sides; ++I)
            {
                const FVector N0 = Dir(I), N1 = Dir(I + 1);
                Quad(A + N0 * RA, A + N1 * RA, Bp + N1 * RB, Bp + N0 * RB, N0, N1, N1, N0);
                if (bCaps) { Tri(A, A + N0 * RA, A + N1 * RA, -D); Tri(Bp, Bp + N0 * RB, Bp + N1 * RB, D); }
            }
        }
        void Rope(const TArray<FVector>& Path, float R) { for (int32 I = 1; I < Path.Num(); ++I) Spar(Path[I - 1], Path[I], R, R, 6, false); }
    };

    struct FParts
    {
        FSurf Hull, Deck, Wood, WoodDecor, StoneDecor, Dark, Iron, Canvas, CanvasDecor, Stone, Brick, Brass, Rug, Glass, Flame, Hidden, Chest;
    };

    UProceduralMeshComponent* Emit(AActor* Owner, const TCHAR* Name, FSurf& S, const TCHAR* Material, bool bSolid, bool bShadow, int32& Missing, int32& Triangles)
    {
        if (S.T.IsEmpty()) return nullptr;
        auto* Mesh = NewObject<UProceduralMeshComponent>(Owner, FName(Name)); Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(ShipOrigin());
        Mesh->bUseComplexAsSimpleCollision = true; Mesh->RegisterComponent();
        Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, TArray<FColor>(), S.Tan, bSolid);
        if (Material) { auto* M = LoadObject<UMaterialInterface>(nullptr, Material); Missing += !M; Mesh->SetMaterial(0, M); }
        Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        if (bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCastShadow(bShadow);
        if (!Material) { Mesh->SetHiddenInGame(true); Mesh->SetCastShadow(false); }
        Mesh->ComponentTags.Add(TEXT("Ship"));
        Triangles += S.T.Num() / 3;
        return Mesh;
    }

    const TCHAR* WoodMat = TEXT("/Game/Art/Materials/M_Wood.M_Wood");
    const TCHAR* DeckMat = TEXT("/Game/Art/Materials/M_WoodLight.M_WoodLight");
    const TCHAR* CanvasMat = TEXT("/Game/Art/Materials/M_Canvas.M_Canvas");
    const TCHAR* IronMat = TEXT("/Game/Art/Materials/M_RustIron.M_RustIron");
    const TCHAR* DarkMat = TEXT("/Game/Art/Materials/M_Dark.M_Dark");
    const TCHAR* StoneMat = TEXT("/Game/Art/ChultTemple/M_TempleDungeonStone.M_TempleDungeonStone");
    const TCHAR* BrickMat = TEXT("/Game/Art/Materials/M_ForgeBrick.M_ForgeBrick");
    const TCHAR* BrassMat = TEXT("/Game/Art/Materials/M_Metal.M_Metal");
    const TCHAR* RugMat = TEXT("/Game/Art/Materials/M_Navy.M_Navy");
    const TCHAR* GlassMat = TEXT("/Game/Art/Materials/M_WindowGlass.M_WindowGlass");
    const TCHAR* FlameMat = TEXT("/Game/Art/Materials/M_TorchFlame.M_TorchFlame");
    const TCHAR* ChestMat = TEXT("/Game/Art/Materials/M_AgedDockTimber.M_AgedDockTimber");   // sea chests and the captain's
    const TCHAR* SeaMat = TEXT("/Game/Art/ChultTemple/M_TempleRiftSea.M_TempleRiftSea");

    // ------------------------------------------------------------------ openings in the planking
    struct FOpening { float Side, X0, X1, Z0, Z1; bool bLid; };
    // Rows of the hull's mesh (cm above the keel): every opening's sill and head is one.
    const float Rows[] = {0, 1, 40, 80, 130, 190, 250, WaterZ, LowerZ, HoleZ0, 395, 435, HoleZ1, 500, DeckBeam, DeckZ, 630, RailZ};
    constexpr float PortZ0 = 395, PortZ1 = 450;
    const TArray<FOpening>& Openings()
    {
        static const TArray<FOpening> O = {
            // The arrival compartment: the broken planking that shows the sea (port), small scuttles.
            {1, HoleX0, HoleX1, HoleZ0, HoleZ1, false}, {1, -125, -75, PortZ0, PortZ1, false}, {1, 250, 300, PortZ0, PortZ1, false}, {-1, 200, 250, PortZ0, PortZ1, false},
            // The galley's gunports, their lids raised.
            {1, 525, 575, PortZ0, PortZ1, true}, {1, 625, 675, PortZ0, PortZ1, true}, {-1, 525, 575, PortZ0, PortZ1, true}, {-1, 975, 1025, PortZ0, PortZ1, true},
            // The crew quarters'.
            {1, -425, -375, PortZ0, PortZ1, true}, {1, -775, -725, PortZ0, PortZ1, true}, {-1, -425, -375, PortZ0, PortZ1, true}, {-1, -775, -725, PortZ0, PortZ1, true},
            // The captain's cabin's side lights.
            {1, -1375, -1325, PortZ0, PortZ1, false}, {-1, -1375, -1325, PortZ0, PortZ1, false}};
        return O;
    }
    bool CellOpen(float Side, float X0, float X1, float Z0, float Z1)
    {
        for (const FOpening& O : Openings())
            if (O.Side == Side && X0 >= O.X0 - .1f && X1 <= O.X1 + .1f && Z0 >= O.Z0 - .1f && Z1 <= O.Z1 + .1f) return true;
        return false;
    }
    FVector HullPoint(float X, float Z, float Side, bool bInner)
    {
        const float H = bInner ? InnerAt(X, Z) : OuterAt(X, Z);
        return FVector(X, Side * H, Z);
    }
    FVector HullNormal(float X, float Z, float Side)
    {
        constexpr float E = 2;
        const float Hx = (OuterAt(FMath::Min(X + E, BowX), Z) - OuterAt(FMath::Max(X - E, SternX), Z)) / (2 * E);
        const float Hz = (OuterAt(X, Z + E) - OuterAt(X, FMath::Max(0.f, Z - E))) / (2 * E);
        return FVector(-Hx, Side, -Hz).GetSafeNormal();
    }

    void BuildHull(FParts& P)
    {
        TArray<float> Xs;
        for (float X = SternX; X < BowX - 1; X += 25) Xs.Add(X);
        Xs.Add(BowX);
        constexpr int32 NR = UE_ARRAY_COUNT(Rows);
        for (float Side : {1.f, -1.f})
            for (int32 I = 0; I + 1 < Xs.Num(); ++I)
                for (int32 J = 0; J + 1 < NR; ++J)
                {
                    const float X0 = Xs[I], X1 = Xs[I + 1], Z0 = Rows[J], Z1 = Rows[J + 1];
                    if (CellOpen(Side, X0, X1, Z0, Z1)) continue;
                    auto Pt = [&](float X, float Z, bool bIn) { return J == 0 && Z < .5f ? FVector(X, 0, FMath::Max(Z, BottomZ(X))) : HullPoint(X, FMath::Max(Z, BottomZ(X)), Side, bIn); };
                    const FVector N00 = HullNormal(X0, Z0, Side), N10 = HullNormal(X1, Z0, Side), N11 = HullNormal(X1, Z1, Side), N01 = HullNormal(X0, Z1, Side);
                    P.Hull.Quad(Pt(X0, Z0, false), Pt(X1, Z0, false), Pt(X1, Z1, false), Pt(X0, Z1, false), N00, N10, N11, N01);
                    // The inside face, above the hold's floor (below it nothing is seen).
                    if (Z1 > HoldZ - 20) P.Hull.Quad(Pt(X0, Z0, true), Pt(X1, Z0, true), Pt(X1, Z1, true), Pt(X0, Z1, true), -N00, -N10, -N11, -N01);
                }
        // The openings' reveals through the planking.
        for (const FOpening& O : Openings())
        {
            const float S = O.Side;
            auto Out = [&](float X, float Z) { return HullPoint(X, Z, S, false); };
            auto In = [&](float X, float Z) { return HullPoint(X, Z, S, true); };
            P.Hull.Flat(Out(O.X0, O.Z0), Out(O.X1, O.Z0), In(O.X1, O.Z0), In(O.X0, O.Z0), FVector::UpVector);
            P.Hull.Flat(Out(O.X0, O.Z1), Out(O.X1, O.Z1), In(O.X1, O.Z1), In(O.X0, O.Z1), FVector::DownVector);
            P.Hull.Flat(Out(O.X0, O.Z0), Out(O.X0, O.Z1), In(O.X0, O.Z1), In(O.X0, O.Z0), FVector::ForwardVector);
            P.Hull.Flat(Out(O.X1, O.Z0), Out(O.X1, O.Z1), In(O.X1, O.Z1), In(O.X1, O.Z0), FVector::BackwardVector);
            if (O.bLid)
            {
                // The lid, hinged at the head and propped out and up.
                const FVector Hinge = Out((O.X0 + O.X1) * .5f, O.Z1) + FVector(0, S * 3, 2);
                const FVector Down = FVector(0, S * .64f, .77f);   // its free edge raised about 50 degrees out from the hull
                const float L = O.Z1 - O.Z0 + 8;
                P.Wood.Box(Hinge + Down * L * .5f, FVector((O.X1 - O.X0) * .5f + 6, 0, 0), FVector::CrossProduct(Down, FVector::ForwardVector).GetSafeNormal() * 4, Down * L * .5f);
            }
            else if (O.X1 - O.X0 < 60)
            {
                // A scuttle: an iron rim round it inside.
                const float Y = S * (InnerAt((O.X0 + O.X1) * .5f, (O.Z0 + O.Z1) * .5f) - 2);
                P.Iron.B(O.X0 - 5, O.X1 + 5, Y - 2, Y + 2, O.Z0 - 5, O.Z0);
                P.Iron.B(O.X0 - 5, O.X1 + 5, Y - 2, Y + 2, O.Z1, O.Z1 + 5);
                P.Iron.B(O.X0 - 5, O.X0, Y - 2, Y + 2, O.Z0, O.Z1);
                P.Iron.B(O.X1, O.X1 + 5, Y - 2, Y + 2, O.Z0, O.Z1);
            }
        }
        // The rail's cap, the length of each side.
        for (float Side : {1.f, -1.f})
            for (int32 I = 0; I + 1 < Xs.Num(); ++I)
            {
                const float X0 = Xs[I], X1 = Xs[I + 1];
                P.Hull.Flat(HullPoint(X0, RailZ, Side, false), HullPoint(X1, RailZ, Side, false), HullPoint(X1, RailZ, Side, true), HullPoint(X0, RailZ, Side, true), FVector::UpVector);
            }
        // The wales: two heavy strakes along the outside, and the stem.
        for (float Z : {LowerZ - 20, DeckZ + 10})
            for (float Side : {1.f, -1.f})
                for (int32 I = 0; I + 1 < Xs.Num(); ++I)
                {
                    const float X0 = Xs[I], X1 = Xs[I + 1];
                    if (OuterAt(X1, Z) < 20 || OuterAt(X0, Z) < 20) continue;
                    const FVector N0 = HullNormal(X0, Z, Side), N1 = HullNormal(X1, Z, Side);
                    const FVector A = HullPoint(X0, Z, Side, false), Bp = HullPoint(X1, Z, Side, false);
                    P.Wood.Quad(A + N0 * 7 + FVector(0, 0, 9), Bp + N1 * 7 + FVector(0, 0, 9), Bp + N1 * 7 - FVector(0, 0, 9), A + N0 * 7 - FVector(0, 0, 9), N0, N1, N1, N0);
                    P.Wood.Flat(A + FVector(0, 0, 9), Bp + FVector(0, 0, 9), Bp + N1 * 7 + FVector(0, 0, 9), A + N0 * 7 + FVector(0, 0, 9), FVector::UpVector);
                    P.Wood.Flat(A - FVector(0, 0, 9), Bp - FVector(0, 0, 9), Bp + N1 * 7 - FVector(0, 0, 9), A + N0 * 7 - FVector(0, 0, 9), FVector::DownVector);
                }
        P.Wood.Box(FVector(BowX + 6, 0, (BottomZ(BowX) + RailZ + 60) * .5f), FVector(10, 0, 0), FVector(0, 9, 0), FVector(0, 0, (RailZ + 60 - BottomZ(BowX)) * .5f));
        P.Wood.Spar(FVector(BowX - 100, 0, RailZ + 10), FVector(BowX + 650, 0, RailZ + 260), 20, 10);   // the bowsprit
        P.Wood.Box(FVector(0, 0, -8), FVector(1500, 0, 0), FVector(0, 14, 0), FVector(0, 0, 9));          // the keel

        // The transom: planked across the stern, with the cabin's four stern windows.
        const float WZ0 = 380, WZ1 = 500;
        const float WY[] = {-240, -80, 80, 240};
        constexpr float WHalf = 32;
        for (float Z = BottomZ(SternX); Z < QuarterZ + 100; Z += 20)
        {
            const float Za = Z, Zb = FMath::Min(Z + 20, QuarterZ + 100.f);
            const float H = OuterAt(SternX + 1, FMath::Min(Za, RailZ)) * (Za > RailZ ? .97f : 1.f);
            if (H < 1) continue;
            const bool bWin = Za >= WZ0 - .1f && Zb <= WZ1 + .1f;
            float Y0 = -H;
            if (bWin)
                for (float C : WY) { P.Hull.B(SternX - 12, SternX + 2, Y0, C - WHalf, Za, Zb); Y0 = C + WHalf; }
            P.Hull.B(SternX - 12, SternX + 2, Y0, H, Za, Zb);
        }
        for (float C : WY)
        {
            // Frame, a mullion and a transom bar, and the glass (which also keeps him in).
            P.Wood.B(SternX - 4, SternX + 6, C - WHalf - 4, C + WHalf + 4, WZ0 - 6, WZ0);
            P.Wood.B(SternX - 4, SternX + 6, C - WHalf - 4, C + WHalf + 4, WZ1, WZ1 + 6);
            P.Wood.B(SternX - 4, SternX + 6, C - 2, C + 2, WZ0, WZ1);
            P.Wood.B(SternX - 4, SternX + 6, C - WHalf, C + WHalf, (WZ0 + WZ1) * .5f - 2, (WZ0 + WZ1) * .5f + 2);
            P.Hidden.B(SternX - 12, SternX + 2, C - WHalf, C + WHalf, WZ0, WZ1);
        }
        // The stern's bulwark round the quarterdeck.
        for (float Side : {1.f, -1.f})
            for (float X = SternX; X < QuarterFore; X += 25)
            {
                const float H0 = OuterAt(X, RailZ), H1 = OuterAt(X + 25, RailZ);
                P.Hull.Flat(FVector(X, Side * H0, RailZ), FVector(X + 25, Side * H1, RailZ), FVector(X + 25, Side * H1 * .99f, QuarterZ + 100), FVector(X, Side * H0 * .99f, QuarterZ + 100), FVector(0, Side, 0));
                P.Hull.Flat(FVector(X, Side * (H0 - Shell), RailZ), FVector(X + 25, Side * (H1 - Shell), RailZ), FVector(X + 25, Side * (H1 - Shell), QuarterZ + 100), FVector(X, Side * (H0 - Shell), QuarterZ + 100), FVector(0, -Side, 0));
            }
    }

    // ------------------------------------------------------------------ floors, decks, bulkheads
    struct FHole { float X0, X1, Y0, Y1; };
    // A planked floor from X0 to X1, its top at Z, filling the hull's breadth, with holes.
    void Floor(FSurf& S, float X0, float X1, float Z, float Thick, const TArray<FHole>& Holes)
    {
        TArray<float> Cuts;
        for (float X = X0; X < X1; X += 25) Cuts.Add(X);
        Cuts.Add(X1);
        for (const FHole& H : Holes) { Cuts.Add(FMath::Clamp(H.X0, X0, X1)); Cuts.Add(FMath::Clamp(H.X1, X0, X1)); }
        Cuts.Sort();
        for (int32 I = 0; I + 1 < Cuts.Num(); ++I)
        {
            const float A = Cuts[I], Bx = Cuts[I + 1];
            if (Bx - A < .5f) continue;
            const float W = FMath::Min(InnerAt(A, Z - Thick), InnerAt(Bx, Z - Thick)) + 4;
            if (W < 5) continue;
            TArray<TPair<float, float>> Spans = {{-W, W}};
            for (const FHole& H : Holes)
            {
                if (A < H.X0 - .1f || Bx > H.X1 + .1f) continue;
                TArray<TPair<float, float>> Next;
                for (const auto& Sp : Spans) { if (H.Y0 > Sp.Key) Next.Add({Sp.Key, FMath::Min(H.Y0, Sp.Value)}); if (H.Y1 < Sp.Value) Next.Add({FMath::Max(H.Y1, Sp.Key), Sp.Value}); }
                Spans = Next;
            }
            for (const auto& Sp : Spans) S.B(A, Bx, Sp.Key, Sp.Value, Z - Thick, Z);
        }
    }
    struct FDoor { float Y, Half, Height; };
    // A bulkhead across the ship at X, from Z0 to Z1, filling the hull's breadth, with doorways from Z0 up.
    void BuildBulkhead(FParts& P, float X, float Z0, float Z1, const TArray<FDoor>& Doors)
    {
        TArray<float> Zs;
        for (float Z = Z0; Z < Z1; Z += 25) Zs.Add(Z);
        Zs.Add(Z1);
        for (const FDoor& D : Doors) Zs.Add(Z0 + D.Height);
        Zs.Sort();
        for (int32 I = 0; I + 1 < Zs.Num(); ++I)
        {
            const float Za = Zs[I], Zb = Zs[I + 1];
            if (Zb - Za < .5f) continue;
            const float W = FMath::Max(InnerAt(X - Bulkhead, Za), InnerAt(X + Bulkhead, Za)) + 4;
            float Y0 = -W;
            TArray<FDoor> Here;
            for (const FDoor& D : Doors) if (Zb <= Z0 + D.Height + .1f) Here.Add(D);
            Here.Sort([](const FDoor& A, const FDoor& Bd) { return A.Y < Bd.Y; });
            for (const FDoor& D : Here) { P.Wood.B(X - Bulkhead, X + Bulkhead, Y0, D.Y - D.Half, Za, Zb); Y0 = D.Y + D.Half; }
            P.Wood.B(X - Bulkhead, X + Bulkhead, Y0, W, Za, Zb);
        }
        // Battens on both faces, and each doorway's posts and lintel.
        for (float Y = -400; Y <= 400; Y += 70)
        {
            bool bDoor = false;
            for (const FDoor& D : Doors) bDoor |= FMath::Abs(Y - D.Y) < D.Half + 12;
            const float W = InnerAt(X, Z0 + 20);
            if (bDoor || FMath::Abs(Y) > W - 10) continue;
            for (float F : {-1.f, 1.f}) P.WoodDecor.B(X + F * Bulkhead - (F < 0 ? 3 : 0), X + F * Bulkhead + (F > 0 ? 3 : 0), Y - 5, Y + 5, Z0, Z1);
        }
        for (const FDoor& D : Doors)
            for (float F : {-1.f, 1.f})
            {
                P.WoodDecor.B(X - Bulkhead - 4, X + Bulkhead + 4, D.Y + F * D.Half - (F > 0 ? 0 : 8), D.Y + F * D.Half + (F > 0 ? 8 : 0), Z0, Z0 + D.Height + 10);
                P.WoodDecor.B(X - Bulkhead - 4, X + Bulkhead + 4, D.Y - D.Half - 8, D.Y + D.Half + 8, Z0 + D.Height, Z0 + D.Height + 10);
            }
    }
    // Beams across under a deck, every Step, skipping holes.
    void Beams(FSurf& S, float X0, float X1, float Under, float Depth, const TArray<FHole>& Holes, float Step = 60)
    {
        for (float X = X0 + Step * .5f; X < X1; X += Step)
        {
            const float W = InnerAt(X, Under) + 4;
            TArray<TPair<float, float>> Spans = {{-W, W}};
            for (const FHole& H : Holes)
            {
                if (X + 7 < H.X0 || X - 7 > H.X1) continue;
                TArray<TPair<float, float>> Next;
                for (const auto& Sp : Spans) { if (H.Y0 > Sp.Key) Next.Add({Sp.Key, FMath::Min(H.Y0, Sp.Value)}); if (H.Y1 < Sp.Value) Next.Add({FMath::Max(H.Y1, Sp.Key), Sp.Value}); }
                Spans = Next;
            }
            for (const auto& Sp : Spans) S.B(X - 7, X + 7, Sp.Key, Sp.Value, Under, Under + Depth);
        }
    }
    // A grating in a deck: a lattice of bars over a hole, its coaming round it.
    void Grating(FParts& P, const FHole& H, float Top, float Thick)
    {
        for (float X = H.X0 + 7.5f; X < H.X1; X += 15) P.Wood.B(X - 2.5f, X + 2.5f, H.Y0, H.Y1, Top - Thick, Top);
        for (float Y = H.Y0 + 7.5f; Y < H.Y1; Y += 15) P.Wood.B(H.X0, H.X1, Y - 2, Y + 2, Top - Thick, Top - 2);
        P.Wood.B(H.X0 - 8, H.X1 + 8, H.Y0 - 8, H.Y0, Top - Thick, Top + 14);
        P.Wood.B(H.X0 - 8, H.X1 + 8, H.Y1, H.Y1 + 8, Top - Thick, Top + 14);
        P.Wood.B(H.X0 - 8, H.X0, H.Y0, H.Y1, Top - Thick, Top + 14);
        P.Wood.B(H.X1, H.X1 + 8, H.Y0, H.Y1, Top - Thick, Top + 14);
    }
    // Frames up the inside of the hull, every Step, from Z0 to Z1.
    void Frames(FSurf& S, float X0, float X1, float Z0, float Z1, float Step = 60)
    {
        for (float X = X0 + Step * .5f; X < X1; X += Step)
            for (float Side : {1.f, -1.f})
            {
                bool bPort = false;
                for (const FOpening& O : Openings()) bPort |= O.Side == Side && X > O.X0 - 9 && X < O.X1 + 9 && Z1 > O.Z0;
                // A ribbon 12 cm wide standing 8 cm off the planking, following its curve up the side.
                for (float Z = Z0; Z < Z1 - .5f; Z += 15)
                {
                    const float Zb = FMath::Min(Z + 15, Z1);
                    if (bPort && Zb > PortZ0 - 30 && Z < HoleZ1 + 5) continue;
                    const float Ha = InnerAt(X, Z) + 1, Hb = InnerAt(X, Zb) + 1;
                    if (Ha < 30 || Hb < 30) continue;
                    const FVector N(0, -Side, 0);
                    auto Pt = [&](float Dx, float H, float Z_, float Off) { return FVector(X + Dx, Side * (H - Off), Z_); };
                    S.Flat(Pt(-6, Ha, Z, 8), Pt(6, Ha, Z, 8), Pt(6, Hb, Zb, 8), Pt(-6, Hb, Zb, 8), N);
                    S.Flat(Pt(-6, Ha, Z, 0), Pt(-6, Ha, Z, 8), Pt(-6, Hb, Zb, 8), Pt(-6, Hb, Zb, 0), FVector(-1, 0, 0));
                    S.Flat(Pt(6, Ha, Z, 0), Pt(6, Ha, Z, 8), Pt(6, Hb, Zb, 8), Pt(6, Hb, Zb, 0), FVector(1, 0, 0));
                }
            }
    }

    // ------------------------------------------------------------------ furniture
    // A table: a top, four legs.
    void Table(FSurf& S, float X0, float X1, float Y0, float Y1, float Top, float Thick = 6, float Leg = 7)
    {
        S.B(X0, X1, Y0, Y1, Top - Thick, Top);
        for (float X : {X0 + 6, X1 - 6 - Leg}) for (float Y : {Y0 + 6, Y1 - 6 - Leg}) S.B(X, X + Leg, Y, Y + Leg, Top - 200, Top - Thick);
    }
    // A shelf unit along the hull: uprights and boards (heights above the floor), its back to the side.
    TArray<FVector> GJars;   // where the jars stand (ship frame), gathered as the shelves are built
    void Shelf(FSurf& S, float X0, float X1, float Side, float Floor, float Depth, const TArray<float>& Boards, int32 JarBoards = 0)
    {
        const float H = InnerAt((X0 + X1) * .5f, Floor + 60);
        const float YIn = Side * (H - Depth), YOut = Side * (H + 2);
        const float Ya = FMath::Min(YIn, YOut), Yb = FMath::Max(YIn, YOut);
        const float Top = Floor + Boards.Last() + 30;
        for (float X = X0; X <= X1 + .1f; X += (X1 - X0) / FMath::Max(1, FMath::RoundToInt((X1 - X0) / 90))) S.B(X - 3, X + 3, Ya, Yb, Floor, Top);
        for (float Bz : Boards) S.B(X0 - 3, X1 + 3, Ya, Yb, Floor + Bz - 3, Floor + Bz);
        for (int32 K = 0; K < JarBoards && K < Boards.Num(); ++K)
            for (float X = X0 + 28 + K * 14; X < X1 - 20; X += 62) GJars.Add(FVector(X, Side * (H - Depth * .5f), Floor + Boards[K]));
    }
    // A hanging lantern: an iron frame, glass, a flame; a light under the beam, swung by the sea.
    struct FLamp { FVector At; float Intensity, Radius; };
}

FVector ShipOrigin() { return AFallCutscene::LandingSpot() + FVector(0, -800000, 0); }

bool InShip(const FVector& World)
{
    const FVector L = ShipLocal(World);
    return L.X > -3000 && L.X < 3500 && FMath::Abs(L.Y) < 2500 && L.Z > -1500 && L.Z < 4000;
}
float ShipInnerHalf(float X, float Z) { return InnerAt(X, Z); }
float ShipOuterHalf(float X, float Z) { return OuterAt(X, Z); }

namespace
{
    struct FSpawnDef { FVector2D At; float Floor, Yaw; };
    FSpawnDef SpawnDef(EShipSpawn S)
    {
        switch (S)
        {
        case EShipSpawn::Hold: return {{80, 0}, HoldZ, 180};
        case EShipSpawn::Quarters: return {{-250, 60}, LowerZ, 180};
        case EShipSpawn::Galley: return {{400, 160}, LowerZ, 0};
        case EShipSpawn::Cabin: return {{-1140, 0}, LowerZ, 180};
        default: return {{235, 210}, LowerZ, 112};
        }
    }
}
FVector ShipSpawn(EShipSpawn S) { const FSpawnDef D = SpawnDef(S); return ShipWorld(FVector(D.At.X, D.At.Y, D.Floor + 34.65f)); }
float ShipSpawnYaw(EShipSpawn S) { return SpawnDef(S).Yaw; }

int32 ShipSpaceAt(const FVector& World)
{
    if (!InShip(World)) return -1;
    const FVector L = ShipLocal(World);
    // Outside the planking (gone over the side), by where along the ship he is on the lower deck.
    const bool bOutside = FMath::Abs(L.Y) > OuterAt(L.X, FMath::Clamp(static_cast<float>(L.Z), 1.f, RailZ)) + 5 || L.X < SternX - 12;
    if (bOutside) return L.X > ArrivalAft && L.X < ArrivalFore ? int32(EShipSpawn::Arrival) : L.X >= ArrivalFore ? int32(EShipSpawn::Galley) : L.X > CabinFore ? int32(EShipSpawn::Quarters) : int32(EShipSpawn::Cabin);
    if (L.Z < LowerZ - 20 && L.Z > HoldZ - 40 && L.X > HoldAft && L.X < HoldFore) return int32(EShipSpawn::Hold);
    if (L.Z > QuarterZ + 40) return -1;
    if (L.X > ArrivalAft && L.X < ArrivalFore) return int32(EShipSpawn::Arrival);
    if (L.X >= ArrivalFore && L.X < GalleyFore) return int32(EShipSpawn::Galley);
    if (L.X > CabinFore && L.X <= ArrivalAft) return int32(EShipSpawn::Quarters);
    if (L.X > SternX && L.X <= CabinFore) return int32(EShipSpawn::Cabin);
    return -1;
}
const TCHAR* ShipSpaceName(int32 Space)
{
    static const TCHAR* Names[] = {TEXT("Arrival"), TEXT("Hold"), TEXT("Quarters"), TEXT("Galley"), TEXT("Cabin")};
    return Space >= 0 && Space < 5 ? Names[Space] : TEXT("None");
}
bool ShipRespawn(const FVector& World, FVector& Out)
{
    if (!InShip(World)) return false;
    const int32 Space = ShipSpaceAt(World);
    Out = ShipSpawn(Space >= 0 ? EShipSpawn(Space) : EShipSpawn::Arrival);
    return true;
}
bool IsShipSpawn(const FVector& World, float& Yaw)
{
    for (int32 I = 0; I < 5; ++I)
        if (FVector::Dist(World, ShipSpawn(EShipSpawn(I))) < 1.f) { Yaw = ShipSpawnYaw(EShipSpawn(I)); return true; }
    return false;
}
bool ShipSeaSwallows(const FVector& World)
{
    if (!InShip(World)) return false;
    const FVector L = ShipLocal(World);
    if (L.Z > WaterZ - 20) return false;
    return FMath::Abs(L.Y) > OuterAt(L.X, FMath::Max(L.Z, 1.f)) || L.X < SternX - 15 || L.X > BowX;
}

const TArray<FVector>& ShipJars() { return GJars; }
const TArray<FVector>& ShipRats()
{
    // (None within 4.5 m of the ladder's foot, where the hold's checkpoint and respawn stand.)
    static const TArray<FVector> R = {
        {-1150, 150, HoldZ}, {-1190, -110, HoldZ}, {-960, 90, HoldZ}, {-860, 200, HoldZ}, {-760, -200, HoldZ}, {-600, 60, HoldZ},
        {-500, -120, HoldZ}, {-420, 230, HoldZ}, {-480, 120, HoldZ}, {560, 130, HoldZ}, {560, -130, HoldZ}, {600, 60, HoldZ},
        {760, -120, HoldZ}, {850, 240, HoldZ}, {980, 150, HoldZ}, {1060, -150, HoldZ}};
    return R;
}

// ====================================================================== the actor
AShipSetting::AShipSetting()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(TEXT("Ship"));
}

AShipSetting* AShipSetting::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AShipSetting> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AShipSetting* AShipSetting::Enter(UWorld* World)
{
    if (!World) return nullptr;
    if (AShipSetting* Existing = Get(World)) return Existing;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    // Whatever stage stands (Waterdeep, or Chult and its temple) goes dark, still and silent, as AChultLanding::EnterPreview
    // does Waterdeep: there is no way back from the ship.
    int32 Hidden = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (A == Chuck || A->GetAttachParentActor() == Chuck || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>()) continue;
        if (A->IsA<AInfo>())
        {
            // The game mode and the like keep running; but fog, sky lights and atmospheres are AInfo actors too (Waterdeep's
            // harbour fog whitened every opening aboard until 2026-11-01): their parts go dark.
            TInlineComponentArray<UActorComponent*> Parts(A);
            for (auto* Part : Parts)
                if (Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>())
                    Cast<USceneComponent>(Part)->SetVisibility(false);
            continue;
        }
        if (A->GetClass()->GetName().Contains(TEXT("ShipEscape"))) continue;
        if (auto* Volume = Cast<APostProcessVolume>(A)) Volume->bEnabled = false;
        A->SetActorHiddenInGame(true);
        A->SetActorEnableCollision(false);
        A->SetActorTickEnabled(false);
        TInlineComponentArray<UActorComponent*> Parts(A);
        for (auto* Part : Parts)
        {
            if (auto* Audio = Cast<UAudioComponent>(Part)) Audio->Stop();
            else if (auto* Post = Cast<UPostProcessComponent>(Part)) Post->bEnabled = false;
            else if (Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>())
                Cast<USceneComponent>(Part)->SetVisibility(false);
            if (Part != A->GetRootComponent()) Part->SetComponentTickEnabled(false);
        }
        ++Hidden;
    }
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Ship = World->SpawnActor<AShipSetting>(ShipOrigin(), FRotator::ZeroRotator, P);
    Ship->Build();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_ENTER hidden=%d"), Hidden);
    return Ship;
}

void AShipSetting::BuildEnvironment()
{
    const FVector G = ShipOrigin();
    auto Attach = [this, G](USceneComponent* C, FVector At = FVector::ZeroVector, FRotator R = FRotator::ZeroRotator)
    { C->SetupAttachment(RootComponent); C->SetWorldLocationAndRotation(G + At, R); C->RegisterComponent(); };
    // A clear afternoon at sea: the sun high on the starboard quarter, so its light comes in through the starboard
    // ports and down through the gratings.
    auto* Sun = NewObject<UDirectionalLightComponent>(this);
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetAtmosphereSunLight(true);
    Sun->SetIntensity(5.f); Sun->SetLightColor(FLinearColor(1.f, .95f, .86f));
    Sun->DynamicShadowDistanceMovableLight = 6000; Sun->DynamicShadowCascades = 3;
    Sun->SetVolumetricScatteringIntensity(2.f);
    Attach(Sun, FVector::ZeroVector, FRotator(-38, 62, 0));
    // (Its sky is hidden behind the sea box; the sky light's capture needs it.)
    auto* Sky = NewObject<USkyAtmosphereComponent>(this); Sky->SetRayleighScattering(FLinearColor(.13f, .43f, .66f)); Attach(Sky, FVector(0, 0, -WaterZ));
    // Below decks the sky's fill is faint: without dynamic GI it would light the inside as if there were no hull.
    auto* Fill = NewObject<USkyLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->bRealTimeCapture = true;
    Fill->bLowerHemisphereIsBlack = true; Fill->SetLowerHemisphereColor(FLinearColor(.06f, .05f, .04f));
    Fill->SetIntensity(.22f); Attach(Fill, FVector(0, 0, 1500));
    auto* Fog = NewObject<UExponentialHeightFogComponent>(this);
    // Thin: the sea box beyond must stay clear (a thicker fog whitened every opening); the volumetric fog near by shows the shafts.
    Fog->SetFogDensity(.0006f); Fog->SetFogHeightFalloff(.02f); Fog->SetFogMaxOpacity(.25f); Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogScatteringDistribution(.6f); Fog->SetVolumetricFogExtinctionScale(1.4f);
    Fog->SetVolumetricFogDistance(2500); Fog->SetFogInscatteringColor(FLinearColor(.25f, .3f, .36f));
    Attach(Fog, FVector(0, 0, WaterZ));
    auto* Post = NewObject<UPostProcessComponent>(this);
    Post->bUnbound = true; Post->Priority = 110;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Histogram;
    S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = -.6f;
    S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 3.f;
    S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -.4f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .45f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .7f;
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.95f, .95f, .92f, 1);
    S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = .7f;
    S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = .8f;
    Attach(Post);
    // The open sea all round, seen through every opening: the sanctum's sea-tear material, which draws the sea and the
    // sky from the view ray alone (Tools/create_temple_rift_sea.py), on a great box round the ship; its sea at the waterline.
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* SeaBox = NewObject<UStaticMeshComponent>(this, TEXT("ShipSea"));
    SeaBox->SetStaticMesh(Cube); SeaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); SeaBox->SetCastShadow(false);
    SeaBox->SetWorldScale3D(FVector(3000, 3000, 3000));
    Attach(SeaBox, FVector(0, 0, WaterZ));
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, SeaMat))
    {
        auto* Mid = UMaterialInstanceDynamic::Create(M, this);
        Mid->SetScalarParameterValue(TEXT("SeaZ"), static_cast<float>(G.Z + WaterZ));
        float SeaGain = .22f; FParse::Value(FCommandLine::Get(), TEXT("ChuckShipSeaGain="), SeaGain);
        Mid->SetScalarParameterValue(TEXT("Gain"), SeaGain);   // (tuned for the sanctum's darkness; the ship's interior is brighter)
        SeaBox->SetMaterial(0, Mid);
    }
    else ++Missing;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SEA material=%s visible=%d bounds=%.0f at=%s"), *GetNameSafe(SeaBox->GetMaterial(0)), SeaBox->IsVisible(), SeaBox->Bounds.SphereRadius, *SeaBox->GetComponentLocation().ToString());
    // The sea against the hull and the ship's timbers working: a slow wash and creaks (SFX_ShipWash, SFX_ShipCreak_*).
    if (USoundBase* WashSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_ShipWash_00.SFX_ShipWash_00")))
    {
        Wash = NewObject<UAudioComponent>(this); Wash->SetSound(WashSound); Wash->bAutoActivate = false; Wash->bIsUISound = false;
        Attach(Wash); Wash->SetVolumeMultiplier(.0f); Wash->FadeIn(4.f, .5f);
    }
}

void AShipSetting::Build()
{
    if (bBuilt) return;
    bBuilt = true;
    UWorld* World = GetWorld();
    BuildEnvironment();
    FParts P;
    GJars.Reset();
    BuildHull(P);

    // ---- Decks and floors.
    const FHole MainHatch{HatchX0, HatchX1, -HatchHalf, HatchHalf};
    const FHole MainGrating{HatchX0, HatchX1, -HatchHalf, HatchHalf};
    const FHole ForeGrating{700, 820, -60, 60};
    const FHole AftGrating{-760, -640, -60, 60};
    Floor(P.Deck, HoldAft, HoldFore, HoldZ, 10, {});
    Floor(P.Deck, SternX + 2, GalleyFore, LowerZ, 15, {MainHatch});
    Beams(P.Wood, HoldAft, HoldFore, LowerBeam, LowerZ - 15 - LowerBeam, {MainHatch});
    // The weather deck (closed for now, lit through its three gratings) and the quarterdeck, higher, over the cabin.
    Floor(P.Deck, QuarterFore, BowX, DeckZ, 15, {MainGrating, ForeGrating, AftGrating});
    Beams(P.Wood, QuarterFore, GalleyFore + 300, DeckBeam, DeckZ - 15 - DeckBeam, {MainGrating, ForeGrating, AftGrating});
    Floor(P.Deck, SternX + 2, QuarterFore, QuarterZ, 15, {});
    Beams(P.Wood, SternX + 2, QuarterFore, QuarterZ - 25, 10, {});
    for (const FHole& H : {MainGrating, ForeGrating, AftGrating}) Grating(P, H, DeckZ, 15);
    // The quarterdeck's break: its face over the main deck, and under it inside, down to the main deck's beams.
    for (float Z = DeckBeam; Z < QuarterZ; Z += 25)
    {
        const float Zb = FMath::Min(Z + 25, QuarterZ - 15.f);
        const float W = InnerAt(QuarterFore, FMath::Min(Z, RailZ)) + 4;
        P.Wood.B(QuarterFore - 8, QuarterFore + 8, -W, W, Z, Zb);
    }
    // The main hatch's coaming on three sides (its fore side is the ladder's lip).
    P.Wood.B(HatchX0 - 8, HatchX0, -HatchHalf - 8, HatchHalf + 8, LowerZ, LowerZ + 12);
    P.Wood.B(HatchX0 - 8, HatchX1, -HatchHalf - 8, -HatchHalf, LowerZ, LowerZ + 12);
    P.Wood.B(HatchX0 - 8, HatchX1, HatchHalf, HatchHalf + 8, LowerZ, LowerZ + 12);
    // Hatch-well lining from the hold's beams up to the floor.
    P.Wood.B(HatchX0 - 4, HatchX0, -HatchHalf, HatchHalf, LowerBeam, LowerZ);
    P.Wood.B(HatchX0, HatchX1, -HatchHalf - 4, -HatchHalf, LowerBeam, LowerZ);
    P.Wood.B(HatchX0, HatchX1, HatchHalf, HatchHalf + 4, LowerBeam, LowerZ);
    P.Wood.B(HatchX1, HatchX1 + 4, -HatchHalf, HatchHalf, LowerBeam, LowerZ);

    // ---- Bulkheads.
    BuildBulkhead(P, HoldAft, HoldZ, LowerZ - 15, {});
    BuildBulkhead(P, HoldFore, HoldZ, LowerZ - 15, {});
    BuildBulkhead(P, CabinFore, LowerZ, QuarterZ - 25, {{CabinDoorY, DoorHalf, DoorH}});
    BuildBulkhead(P, ArrivalAft, LowerZ, DeckBeam, {{QuartersDoorY, DoorHalf, DoorH}});
    BuildBulkhead(P, ArrivalFore, LowerZ, DeckBeam, {{GalleyDoorY, DoorHalf, DoorH}});
    BuildBulkhead(P, GalleyFore, LowerZ, DeckBeam, {});
    // Frames up the hull's inside, and a clamp along each side under the beams.
    Frames(P.WoodDecor, HoldAft, HoldFore, HoldZ, LowerBeam);
    Frames(P.WoodDecor, SternX + 20, GalleyFore, LowerZ, DeckBeam);
    for (float Side : {1.f, -1.f})
        for (float X = SternX + 20; X < GalleyFore; X += 25)
        {
            const float Z = DeckBeam - 14, H = FMath::Min(InnerAt(X, Z), InnerAt(X + 25, Z));
            P.WoodDecor.B(X, X + 25, Side > 0 ? H - 10 : -H, Side > 0 ? H : -H + 10, Z, Z + 14);
        }

    // ---- The masts, stepped on the keel, through both decks and up into the sky; their partners at each deck.
    for (float MX : {MainMastX, ForeMastX})
    {
        P.Wood.Spar(FVector(MX, 0, HoldZ), FVector(MX, 0, DeckZ), MastR, MastR, 14);
        P.WoodDecor.Spar(FVector(MX, 0, DeckZ), FVector(MX, 0, MX < 0 ? 2700 : 2450), MastR, 16, 14);
        for (float Z : {LowerZ, DeckBeam}) P.Wood.B(MX - 48, MX + 48, -48, 48, Z == LowerZ ? Z : Z - 6, Z == LowerZ ? Z + 6 : Z);
        for (float Z : {HoldZ + 60, LowerZ + 80, LowerZ + 170}) P.Iron.Spar(FVector(MX, 0, Z), FVector(MX, 0, Z + 6), MastR + 1.5f, MastR + 1.5f, 14);
        // A fighting top and a yard (the sails come with the deck).
        const float Top = MX < 0 ? 1900 : 1720;
        P.WoodDecor.B(MX - 90, MX + 90, -110, 110, Top - 8, Top + 4);
        P.WoodDecor.Spar(FVector(MX, -620, Top - 250), FVector(MX, 620, Top - 250), 9, 9, 8);
    }
    // Stanchions under the lower deck's beams.
    for (float X : {-950.f, -150.f, 750.f}) P.Wood.B(X - 8, X + 8, -8, 8, HoldZ, LowerBeam);

    // ---- The arrival compartment: a sail locker. In its starboard-aft corner the temple's masonry has come through
    // the ship's timbers, and the crawlway the escape ends in runs through it.
    {
        const float Z0 = LowerZ, T0 = SeamY - SeamHalf, T1 = SeamY + SeamHalf, Roof = Z0 + SeamH;
        const float Wall = -InnerAt(0, Z0 + 40) + 2;   // the hull's inside there
        // The crawlway's floor, walls and roof (straight faces), and its blind end.
        P.Stone.B(SeamX0 - 8, SeamX1, T0, T1, Z0, Z0 + 2);
        P.Stone.B(SeamX0 - 8, SeamX1, Wall, T0, Z0, Roof + 28);
        P.Stone.B(SeamX0 - 8, SeamX1, T1, T1 + 62, Z0, Roof + 20);
        P.Stone.B(SeamX0 - 8, SeamX1, Wall, T1 + 62, Roof, Roof + 34);
        P.Stone.B(ArrivalAft + Bulkhead, SeamX0, Wall, T1 + 62, Z0, Roof + 34);
        // Great blocks of the temple's masonry heaped on it and jammed against the hull and up into the beams, set a
        // little askew, as the rubble map's are.
        FRandomStream R(20261101);
        struct FBlk { float X, Y, Z, HX, HY, HZ; };
        const FBlk Blocks[] = {
            {-90, -380, Roof + 70, 70, 34, 38}, {30, -370, Roof + 64, 60, 40, 30}, {-40, -300, Roof + 48, 58, 46, 16}, {95, -250, Roof + 46, 38, 30, 14},
            {-120, -385, Roof + 150, 34, 26, 46}, {-120, -390, DeckBeam - 40, 32, 22, 40}, {60, -228, Roof + 22, 26, 18, 22}, {120, -330, Roof + 40, 22, 52, 10},
            {-10, -232, Z0 + 18, 22, 10, 18}, {150, -360, Z0 + 14, 18, 26, 14}, {165, -220, Z0 + 9, 14, 12, 9}};
        for (const FBlk& K : Blocks)
            P.Stone.TurnedBox(FVector(K.X, K.Y, K.Z), FVector(K.HX, K.HY, K.HZ), FRotator(R.FRandRange(-6, 6), R.FRandRange(-9, 9), R.FRandRange(-6, 6)));
        // The ship's timbers split round it: a bulkhead board and two of the hull's ceiling planks torn and bent away.
        P.WoodDecor.TurnedBox(FVector(-140, -205, Z0 + 150), FVector(4, 40, 9), FRotator(-24, 18, 0));
        P.WoodDecor.TurnedBox(FVector(-60, -402, Z0 + 210), FVector(70, 3, 7), FRotator(0, 0, -28));
        P.WoodDecor.TurnedBox(FVector(40, -398, Z0 + 196), FVector(55, 3, 6), FRotator(8, -6, -34));
        P.WoodDecor.TurnedBox(FVector(-30, -360, DeckBeam - 6), FVector(5, 50, 6), FRotator(0, -14, 12));
        // A fan of fine grit spilled from its mouth.
        for (int32 I = 0; I < 9; ++I) P.StoneDecor.TurnedBox(FVector(SeamX1 + R.FRandRange(6, 50), SeamY + R.FRandRange(-60, 60), Z0 + 1), FVector(R.FRandRange(3, 7), R.FRandRange(3, 6), 1.5f), FRotator(0, R.FRandRange(0, 90), 0));

        // Sailcloth: folded spare sails stacked by the port side, aft; coils of rope; a spare spar along starboard.
        // Each folded sail is a stack of thin, uneven folds with a lashing round it.
        auto Folded = [&](float X, float Y, float Z, float HX, float HY, int32 Folds, float Yaw)
        {
            float Top = Z;
            for (int32 K = 0; K < Folds; ++K)
            {
                const float T = R.FRandRange(4.5f, 7.f);
                P.Canvas.TurnedBox(FVector(X + R.FRandRange(-4, 4), Y + R.FRandRange(-5, 5), Top + T * .5f), FVector(HX * R.FRandRange(.94f, 1.02f), HY * R.FRandRange(.93f, 1.02f), T * .5f), FRotator(R.FRandRange(-1.5f, 1.5f), Yaw + R.FRandRange(-3, 3), R.FRandRange(-1.5f, 1.5f)));
                Top += T * .92f;
            }
            const FQuat Q(FRotator(0, Yaw, 0));
            for (float U : {-.45f, .45f})
            {
                const FVector C = FVector(X, Y, 0) + Q.RotateVector(FVector(U * HX, 0, 0));
                const FVector Ac = Q.RotateVector(FVector(0, HY + 3, 0));
                P.CanvasDecor.Rope({C - Ac + FVector(0, 0, Z + 1), C - Ac + FVector(0, 0, Top + 1), C + Ac + FVector(0, 0, Top + 1), C + Ac + FVector(0, 0, Z + 1)}, 1.4f);
            }
            P.Hidden.TurnedBox(FVector(X, Y, (Z + Top) * .5f), FVector(HX, HY, (Top - Z) * .5f), FRotator(0, Yaw, 0));
            return Top;
        };
        const float Fold1 = Folded(-90, 245, Z0, 60, 85, 6, 0);
        const float Fold2 = Folded(-95, 240, Fold1, 52, 70, 5, 6);
        Folded(-80, 232, Fold2, 40, 58, 4, -10);
        Folded(-105, 375, Z0, 45, InnerAt(-100, Z0 + 20) - 377, 4, 0);
        // The mouth's lamplight, low: the light he crawls toward.
        auto* Mouth = NewObject<UPointLightComponent>(this); Mouth->SetupAttachment(RootComponent); Mouth->SetMobility(EComponentMobility::Movable);
        Mouth->SetWorldLocation(ShipWorld(FVector(SeamX1 + 70, SeamY + 60, Z0 + 55))); Mouth->SetIntensity(700); Mouth->SetAttenuationRadius(320);
        Mouth->SetLightColor(FLinearColor(1.f, .66f, .36f)); Mouth->SetCastShadows(false); Mouth->RegisterComponent();
        P.Wood.Spar(FVector(180, -385, Z0 + 10), FVector(310, -385, Z0 + 10), 10, 9, 10);
        P.Hidden.B(180, 310, -395, -375, Z0, Z0 + 20);
    }

    // ---- The galley.
    {
        const float Z0 = LowerZ;
        // The hearth: a brick firebox under an iron cooking top, its pipe up through the deck.
        P.Brick.B(900, 1060, -80, 80, Z0, Z0 + 80);
        P.Iron.B(910, 1050, -70, 70, Z0 + 80, Z0 + 140);
        P.Dark.B(904, 912, -35, 35, Z0 + 20, Z0 + 62);
        P.Flame.B(912, 930, -28, 28, Z0 + 20, Z0 + 34);
        P.Iron.Spar(FVector(1010, 0, Z0 + 140), FVector(1010, 0, DeckZ + 120), 12, 12, 10);
        P.Iron.Spar(FVector(960, -30, Z0 + 140), FVector(960, -30, Z0 + 175), 22, 20, 12);   // a great pot
        // A work table amidships, a counter along the port side (open underneath), a rack of shelves to starboard.
        Table(P.Wood, 560, 800, -75, 75, Z0 + 86);
        const float PH = InnerAt(880, Z0 + 60);
        Table(P.Wood, 720, 1060, PH - 62, PH + 2, Z0 + 90);
        P.Wood.B(720, 1060, PH - 60, PH, Z0 + 26, Z0 + 30);          // its low board: jars stand on it
        Shelf(P.Wood, 680, 930, -1, Z0, 44, {35, 85, 140}, 1);
        for (float X : {770.f, 870.f, 970.f}) GJars.Add(FVector(X, PH - 30, Z0 + 30));
        // Pots hanging from a rail over the table.
        P.Iron.B(600, 760, -3, 3, DeckBeam - 6, DeckBeam);
        for (float X : {620.f, 670.f, 720.f, 750.f})
        {
            P.Iron.Rope({FVector(X, 0, DeckBeam - 6), FVector(X, 0, DeckBeam - 40)}, .8f);
            P.Iron.Spar(FVector(X, 0, DeckBeam - 58), FVector(X, 0, DeckBeam - 40), 10 + (int32(X) % 3) * 2, 9, 10);
        }
    }

    // ---- The crew quarters: a big round mess table and stools, sea chests, hammocks slung under the deck beams.
    {
        const float Z0 = LowerZ;
        // (The table stands to starboard of the way through from the arrival compartment's door to the cabin's.)
        constexpr float TX = -340, TY = -175;
        P.Wood.Spar(FVector(TX, TY, Z0 + 70), FVector(TX, TY, Z0 + 76), 100, 100, 28);
        P.Wood.Spar(FVector(TX, TY, Z0), FVector(TX, TY, Z0 + 70), 14, 14, 12);
        P.Wood.B(TX - 60, TX + 60, TY - 6, TY + 6, Z0, Z0 + 8); P.Wood.B(TX - 6, TX + 6, TY - 60, TY + 60, Z0, Z0 + 8);
        for (int32 K = 0; K < 5; ++K)
        {
            const float A = K * 72.f + 20, Rd = 130;
            const FVector C(TX + Rd * FMath::Cos(FMath::DegreesToRadians(A)), TY + Rd * FMath::Sin(FMath::DegreesToRadians(A)), Z0);
            P.Wood.Spar(C + FVector(0, 0, 36), C + FVector(0, 0, 41), 20, 20, 12);
            for (int32 L = 0; L < 3; ++L) { const float LA = FMath::DegreesToRadians(L * 120.f + A); P.Wood.Spar(C + FVector(14 * FMath::Cos(LA), 14 * FMath::Sin(LA), 0), C + FVector(9 * FMath::Cos(LA), 9 * FMath::Sin(LA), 36), 3, 3, 6); }
            P.Hidden.B(C.X - 15, C.X + 15, C.Y - 15, C.Y + 15, Z0, Z0 + 41);
        }
        for (float X : {-560.f, -880.f, -1000.f})
            for (float Side : {1.f, -1.f})
            {
                const float H = InnerAt(X, Z0 + 30);
                P.Chest.B(X - 40, X + 40, Side > 0 ? H - 50 : -H + 4, Side > 0 ? H - 4 : -H + 50, Z0, Z0 + 42);
                P.Iron.B(X - 41, X + 41, Side > 0 ? H - 51 : -H + 3, Side > 0 ? H - 3 : -H + 51, Z0 + 30, Z0 + 34);
            }
    }

    // ---- The captain's cabin: a brass-trimmed bed, a desk, a rug, the chest, a bench under the stern windows.
    {
        const float Z0 = LowerZ;
        P.Rug.B(-1480, -1170, -130, 130, Z0, Z0 + 1);
        P.Wood.B(SternX + 2, SternX + 50, -InnerAt(SternX + 30, Z0 + 20) + 2, InnerAt(SternX + 30, Z0 + 20) - 2, Z0, Z0 + 45);
        const float SB = -InnerAt(-1350, Z0 + 40);
        P.Wood.B(-1460, -1240, SB + 2, SB + 120, Z0, Z0 + 40);           // the bed's frame
        P.Canvas.B(-1455, -1245, SB + 6, SB + 116, Z0 + 40, Z0 + 62);    // mattress
        P.Hidden.B(-1455, -1245, SB + 6, SB + 116, Z0 + 40, Z0 + 66);
        P.Rug.B(-1400, -1244, SB + 4, SB + 118, Z0 + 62, Z0 + 66);        // a blanket over it
        P.Canvas.B(-1452, -1418, SB + 20, SB + 100, Z0 + 62, Z0 + 76);   // the pillow
        for (float X : {-1460.f, -1250.f}) for (float Y : {SB + 2, SB + 112}) P.Brass.Spar(FVector(X + 5, Y + 4, Z0), FVector(X + 5, Y + 4, Z0 + 150), 4, 3.5f, 10);
        P.Brass.Spar(FVector(-1455, SB + 6, Z0 + 140), FVector(-1455, SB + 116, Z0 + 140), 2.5f, 2.5f, 8);
        P.Brass.Spar(FVector(-1245, SB + 6, Z0 + 120), FVector(-1245, SB + 116, Z0 + 120), 2.5f, 2.5f, 8);
        const float PB = InnerAt(-1340, Z0 + 40);
        Table(P.Wood, -1420, -1290, PB - 75, PB - 6, Z0 + 78);
        P.Canvas.B(-1400, -1330, PB - 60, PB - 20, Z0 + 78, Z0 + 79);          // a chart
        P.Brass.Spar(FVector(-1312, PB - 30, Z0 + 78), FVector(-1312, PB - 30, Z0 + 95), 5, 3, 10);   // the desk lamp
        P.Wood.B(-1300, -1255, PB - 60, PB - 20, Z0, Z0 + 46);                 // his chair
        P.Wood.B(-1300, -1296, PB - 60, PB - 20, Z0 + 46, Z0 + 110);
        // The chest (closed until milestone 4): iron-bound, a brass lock plate.
        P.Chest.B(-1230, -1140, 150, 205, Z0, Z0 + 42);
        P.Chest.B(-1230, -1140, 150, 205, Z0 + 42, Z0 + 56);
        for (float X : {-1222.f, -1148.f}) P.Iron.B(X - 3, X + 3, 149, 206, Z0, Z0 + 57);
        P.Brass.B(-1190, -1180, 147, 150, Z0 + 36, Z0 + 48);
    }

    // ---- The hold: cargo stowed in tiers, the pantry's kind of shelving along the sides with jars, sacks.
    struct FPropSet { UStaticMesh* Mesh; FString Name; int32 N = 0; };
    auto* CrateMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* BarrelMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    auto* CoilMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/SM_RopeCoil.SM_RopeCoil"));
    auto* TankardMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (UStaticMesh* T = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Humans/Props/SM_Tankard.SM_Tankard"))) TankardMesh = T;
    Missing += !CrateMesh + !BarrelMesh + !CoilMesh;
    int32 Props = 0;
    auto Prop = [&](UStaticMesh* Mesh, const FVector& Local, const FRotator& Rot, const FVector& Scale = FVector(1), UMaterialInterface* Mat = nullptr)
    {
        if (!Mesh) return;
        auto* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ShipProp%d"), Props++)); C->SetupAttachment(RootComponent);
        C->SetWorldTransform(FTransform(Rot, ShipWorld(Local), Scale)); C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); if (Mat) C->SetMaterial(0, Mat);
        C->ComponentTags.Add(TEXT("Ship")); C->RegisterComponent();
    };
    auto Crate = [&](float X, float Y, int32 Tier, float Yaw)
    {
        const FVector At(X, Y, HoldZ + 30 + Tier * 60);
        Prop(CrateMesh, At, FRotator(0, Yaw, 0));
        P.Hidden.TurnedBox(At, FVector(30, 32.5f, 30), FRotator(0, Yaw, 0));
    };
    auto Barrel = [&](float X, float Y, float Floor, bool bLying, float Yaw)
    {
        const FVector At(X, Y, Floor + (bLying ? 31 : 45));
        Prop(BarrelMesh, At, FRotator(bLying ? 90 : 0, Yaw, 0));
        if (bLying) P.Hidden.TurnedBox(At, FVector(45, 29, 29), FRotator(0, Yaw, 0)); else P.Hidden.B(X - 28, X + 28, Y - 28, Y + 28, Floor, Floor + 90);
    };
    // A sack: a lumpy, slumped canvas body with a flat bottom and a tied neck lying over.
    FRandomStream SackR(20261102);
    auto Sack = [&](float X, float Y, float Floor, float Yaw)
    {
        const FQuat Q(FRotator(0, Yaw, 0));
        constexpr int32 NU = 14, NV = 9;
        const float RX = SackR.FRandRange(25, 31), RY = SackR.FRandRange(17, 21), RZ = SackR.FRandRange(14, 18);
        const float Ph = SackR.FRandRange(0, 6.28f);
        auto Pt = [&](int32 I, int32 J)
        {
            const float U = I * 2 * PI / NU, V = -PI * .5f + J * PI / NV;
            const float Lump = 1 + .07f * FMath::Sin(3 * U + Ph) * FMath::Cos(2 * V) + .05f * FMath::Sin(5 * U + 2 * V + Ph);
            float Z = RZ * FMath::Sin(V) * Lump; Z = FMath::Max(Z, -RZ * .55f);
            const float W = FMath::Cos(V) * Lump * (1 + .18f * FMath::Max(0.f, -FMath::Sin(V)));   // slumped wider low
            return FVector(X, Y, Floor + RZ * .55f) + Q.RotateVector(FVector(RX * W * FMath::Cos(U), RY * W * FMath::Sin(U), Z));
        };
        for (int32 I = 0; I < NU; ++I) for (int32 J = 0; J < NV; ++J)
        {
            const FVector A = Pt(I, J), Bv = Pt(I + 1, J), Cv = Pt(I + 1, J + 1), D = Pt(I, J + 1);
            const FVector Mid = (A + Bv + Cv + D) * .25f - (FVector(X, Y, Floor + RZ * .55f));
            P.CanvasDecor.Quad(A, Bv, Cv, D, Mid.GetSafeNormal(), Mid.GetSafeNormal(), Mid.GetSafeNormal(), Mid.GetSafeNormal());
        }
        const FVector Neck = FVector(X, Y, Floor) + Q.RotateVector(FVector(RX * .9f, 0, RZ * 1.1f));
        P.CanvasDecor.Spar(Neck, Neck + Q.RotateVector(FVector(14, 0, -6)), 6, 4, 8);
        P.CanvasDecor.Spar(Neck - Q.RotateVector(FVector(2, 0, 0)), Neck + Q.RotateVector(FVector(2, 0, 0)), 7, 7, 8);
        P.Hidden.TurnedBox(FVector(X, Y, Floor + RZ * .7f), FVector(RX * .9f, RY * .9f, RZ * .7f), FRotator(0, Yaw, 0));
    };
    {
        // A raised way over the rats along the starboard side, aft from the ladder: a sack to step on, then crates.
        Sack(40, -175, HoldZ, 10); Sack(-5, -205, HoldZ, -20);
        Crate(-60, -250, 0, 3); Crate(-125, -250, 0, -4); Crate(-190, -250, 0, 0); Crate(-190, -250, 1, 6);
        Crate(-255, -250, 0, -2); Crate(-255, -250, 1, -5); Crate(-320, -250, 0, 4); Crate(-320, -250, 1, 1); Crate(-385, -252, 0, 0);
        Crate(-385, -252, 1, -3); Crate(-450, -250, 0, 2);
        // Tiers of cargo through the hold, each a block a rat goes round.
        for (const FVector& C : {FVector(-700, 230, 0), FVector(-765, 230, 0), FVector(-700, 230, 1), FVector(-1050, -150, 0), FVector(-1050, -215, 0), FVector(-1115, -180, 0),
                FVector(-1050, -180, 1), FVector(500, 250, 0), FVector(565, 250, 0), FVector(500, 250, 1), FVector(835, -200, 0), FVector(900, -200, 0), FVector(965, -200, 0), FVector(900, -200, 1),
                FVector(-560, -40, 0), FVector(820, 100, 0)})
            Crate(C.X, C.Y, int32(C.Z), FMath::Fmod(C.X * .37f, 9.f) - 4.5f);
        for (const FVector& Bp : {FVector(-850, -60, 0), FVector(-850, 0, 0), FVector(-910, -30, 0), FVector(250, 120, 0), FVector(310, 150, 0), FVector(650, -140, 0),
                FVector(1060, 60, 0), FVector(1060, 0, 0), FVector(-1180, 120, 0), FVector(-300, 100, 1)})
            Barrel(Bp.X, Bp.Y, HoldZ, Bp.Z > 0, Bp.X * .7f);
        for (const FVector& S : {FVector(-620, -200, 0), FVector(-660, -180, 0), FVector(-640, -230, 0), FVector(420, -90, 0), FVector(460, -60, 0), FVector(-200, 200, 0), FVector(1000, -120, 0)})
            Sack(S.X, S.Y, HoldZ, S.X);
        Prop(CoilMesh, FVector(-430, 120, HoldZ), FRotator(0, 30, 0));
        // Shelving along both sides: boards 36 and 86 cm up (he scratches the jars off them).
        Shelf(P.Wood, -1030, -850, 1, HoldZ, 42, {36, 86, 140}, 2);
        Shelf(P.Wood, -230, -110, 1, HoldZ, 42, {36, 86}, 2);
        Shelf(P.Wood, 640, 760, -1, HoldZ, 42, {36, 86, 140}, 2);
        for (const FVector& J : {FVector(-640, 180, HoldZ), FVector(300, -230, HoldZ), FVector(1000, 60, HoldZ), FVector(-1150, -40, HoldZ)}) GJars.Add(J);
    }
    // The arrival compartment's coils and a barrel; the galley's water casks and sacks; tankards on the mess table.
    Prop(CoilMesh, FVector(-120, 120, LowerZ), FRotator(0, 15, 0));
    Prop(CoilMesh, FVector(-40, 360, LowerZ + 38), FRotator(0, -30, 0));
    Barrel(285, -330, LowerZ, false, 20);
    for (float Y : {-260.f, -180.f, 220.f}) Barrel(1090, Y, LowerZ, false, Y);
    Sack(860, -250, LowerZ, 30); Sack(820, -300, LowerZ, -20);
    if (TankardMesh) for (const FVector& T : {FVector(-300, -135, 0), FVector(-370, -205, 0), FVector(-330, -235, 0)}) Prop(TankardMesh, FVector(T.X, T.Y, LowerZ + 76), FRotator(0, T.X, 0));

    // ---- The ladder from the arrival compartment down into the hold (ChuckClimbable): rails and rungs on the hatch's fore edge.
    for (float Y : {-24.f, 24.f}) P.Wood.B(HatchX1 - 9, HatchX1 - 3, Y - 3, Y + 3, HoldZ, LowerZ + 15);
    for (float Z = HoldZ + 24; Z <= LowerZ - 4; Z += 28) P.Wood.B(HatchX1 - 10, HatchX1 - 3, -27, 27, Z - 2.5f, Z + 2.5f);
    {
        FChuckClimbable L;
        L.Foot = ShipWorld(FVector(HatchX1 - 26.5f, 0, HoldZ)); L.TopZ = ShipOrigin().Z + LowerZ; L.Out = FVector(-1, 0, 0); L.HalfWidth = 22;
        L.Lip = ShipWorld(FVector(HatchX1, 0, LowerZ));
        AddChuckClimbable(L); ++Ladders;
    }

    // ---- Lanterns: iron frames and glass round a flame, hung from the beams; their lights.
    const TArray<FLamp> LampsAt = {
        {{60, 140, DeckBeam - 70}, 2600, 640}, {{-60, -150, DeckBeam - 70}, 1600, 520},                          // arrival
        {{640, 0, DeckBeam - 75}, 2800, 700}, {{960, 180, DeckBeam - 70}, 2000, 600},                           // galley
        {{-340, -150, DeckBeam - 60}, 2600, 700}, {{-700, 180, DeckBeam - 70}, 2200, 650}, {{-950, -200, QuarterZ - 120}, 1800, 600},   // quarters
        {{-1300, 0, QuarterZ - 110}, 3000, 760},                                                                // cabin
        {{-700, 0, LowerBeam - 50}, 1500, 620}, {{-150, 150, LowerBeam - 50}, 1300, 560}, {{650, 0, LowerBeam - 50}, 1500, 620}};   // hold
    for (const FLamp& Lp : LampsAt)
    {
        const FVector A = Lp.At;
        const float Hook = A.Z + 70;
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->SetWorldLocation(ShipWorld(FVector(A.X, A.Y, Hook))); Pivot->RegisterComponent();
        FSurf Frame, Lit;
        const FVector L(0, 0, A.Z - Hook);
        Frame.Rope({FVector(0, 0, 0), L + FVector(0, 0, 22)}, .6f);
        Frame.B(-9, 9, -9, 9, L.Z + 16, L.Z + 20); Frame.B(-10, 10, -10, 10, L.Z - 18, L.Z - 14);
        for (float X : {-8.f, 8.f}) for (float Y : {-8.f, 8.f}) Frame.B(X - 1, X + 1, Y - 1, Y + 1, L.Z - 14, L.Z + 16);
        Lit.B(-7, 7, -7, 7, L.Z - 14, L.Z + 16);
        int32 Dummy = 0;
        for (auto Pair : {TPair<FSurf*, const TCHAR*>(&Frame, IronMat), TPair<FSurf*, const TCHAR*>(&Lit, FlameMat)})
        {
            auto* M = NewObject<UProceduralMeshComponent>(this); M->SetupAttachment(Pivot); M->RegisterComponent();
            M->CreateMeshSection(0, Pair.Key->V, Pair.Key->T, Pair.Key->N, Pair.Key->UV, TArray<FColor>(), Pair.Key->Tan, false);
            if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, Pair.Value)) M->SetMaterial(0, Mat); else ++Missing;
            M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(Pair.Value == IronMat); Dummy += Pair.Key->T.Num() / 3;
        }
        Triangles += Dummy;
        auto* Light = NewObject<UPointLightComponent>(this); Light->SetupAttachment(Pivot); Light->SetMobility(EComponentMobility::Movable);
        Light->SetRelativeLocation(L); Light->SetIntensity(Lp.Intensity); Light->SetAttenuationRadius(Lp.Radius);
        Light->SetLightColor(FLinearColor(1.f, .62f, .32f)); Light->SetSourceRadius(4); Light->SetCastShadows(true);
        Light->RegisterComponent();
        Lamps.Add(Light); LampBase.Add(Lp.Intensity); Swingers.Add(Pivot); SwingPhase.Add(A.X * .013f + A.Y * .007f);
    }
    // The hearth's glow, and light through each grating.
    {
        auto* Glow = NewObject<UPointLightComponent>(this); Glow->SetupAttachment(RootComponent); Glow->SetMobility(EComponentMobility::Movable);
        Glow->SetWorldLocation(ShipWorld(FVector(880, 0, LowerZ + 30))); Glow->SetIntensity(1400); Glow->SetAttenuationRadius(420);
        Glow->SetLightColor(FLinearColor(1.f, .42f, .14f)); Glow->SetCastShadows(false); Glow->RegisterComponent(); Lamps.Add(Glow); LampBase.Add(1400);
    }

    // ---- The hammocks in the crew quarters: canvas slung fore and aft from hooks on the beams, sagging; swung by the sea.
    {
        struct FHam { float X, Y, Z; };
        const FHam Hams[] = {{-560, 270, 0}, {-760, 260, 10}, {-960, 250, 30}, {-560, -270, 5}, {-760, -260, -5}, {-960, -250, 25}, {-660, 110, 40}, {-860, -110, 45}};
        for (const FHam& Hm : Hams)
        {
            const float Top = Hm.X < QuarterFore ? QuarterZ - 25 : DeckBeam;
            auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->SetWorldLocation(ShipWorld(FVector(Hm.X, Hm.Y, Top))); Pivot->RegisterComponent();
            FSurf C, R;
            constexpr int32 NX = 12, NY = 5; constexpr float Len = 100, Half = 34;
            const float Low = LowerZ + 150 + Hm.Z - Top, EndZ = Low + 46;
            auto Pt = [&](int32 I, int32 J)
            {
                const float U = I / float(NX) * 2 - 1, V = J / float(NY) * 2 - 1;
                return FVector(U * Len, V * Half * (1 - .35f * U * U), FMath::Lerp(Low, EndZ, U * U) - 10 * (1 - V * V));
            };
            for (int32 I = 0; I < NX; ++I) for (int32 J = 0; J < NY; ++J)
            {
                const FVector A = Pt(I, J), Bv = Pt(I + 1, J), Cv = Pt(I + 1, J + 1), D = Pt(I, J + 1);
                const FVector N = FVector::CrossProduct(Bv - A, D - A).GetSafeNormal() * (FVector::CrossProduct(Bv - A, D - A).Z < 0 ? -1 : 1);
                C.Quad(A, Bv, Cv, D, N, N, N, N); C.Quad(A, Bv, Cv, D, -N, -N, -N, -N);
            }
            for (float U : {-1.f, 1.f})
            {
                const FVector End(U * (Len + 4), 0, EndZ + 4);
                for (float V : {-1.f, 0.f, 1.f}) R.Rope({Pt(U < 0 ? 0 : NX, 2 + int32(V) * 2), End}, .5f);
                R.Rope({End, FVector(U * (Len + 40), 0, -2)}, .7f);
            }
            for (auto Pair : {TPair<FSurf*, const TCHAR*>(&C, CanvasMat), TPair<FSurf*, const TCHAR*>(&R, CanvasMat)})
            {
                auto* M = NewObject<UProceduralMeshComponent>(this); M->SetupAttachment(Pivot); M->RegisterComponent();
                M->CreateMeshSection(0, Pair.Key->V, Pair.Key->T, Pair.Key->N, Pair.Key->UV, TArray<FColor>(), Pair.Key->Tan, false);
                M->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, Pair.Value)); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Triangles += Pair.Key->T.Num() / 3;
            }
            Swingers.Add(Pivot); SwingPhase.Add(Hm.X * .011f + 1.7f);
        }
    }

    // ---- Emit.
    Emit(this, TEXT("ShipHull"), P.Hull, WoodMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipDeck"), P.Deck, DeckMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipWood"), P.Wood, WoodMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipWoodDecor"), P.WoodDecor, WoodMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipDark"), P.Dark, DarkMat, false, false, Missing, Triangles);
    Emit(this, TEXT("ShipIron"), P.Iron, IronMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipCanvas"), P.Canvas, CanvasMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipCanvasDecor"), P.CanvasDecor, CanvasMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipStone"), P.Stone, StoneMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipStoneDecor"), P.StoneDecor, StoneMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipBrick"), P.Brick, BrickMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipBrass"), P.Brass, BrassMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipRug"), P.Rug, RugMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipChest"), P.Chest, ChestMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipGlass"), P.Glass, GlassMat, false, false, Missing, Triangles);
    Emit(this, TEXT("ShipFlame"), P.Flame, FlameMat, false, false, Missing, Triangles);
    Emit(this, TEXT("ShipHidden"), P.Hidden, nullptr, true, false, Missing, Triangles);

    // ---- The crawlway through the masonry: on all fours in it, and on the floor just out of its mouth.
    AChuckCharacter::RegisterCrawlSpace(World, FTransform(ShipWorld(FVector((SeamX0 + SeamX1) * .5f, SeamY, LowerZ + 40))), FVector((SeamX1 - SeamX0) * .5f, SeamHalf, 40));
    AChuckCharacter::RegisterCrawlSpace(World, FTransform(ShipWorld(FVector(SeamX1 + 30, SeamY, LowerZ + 40))), FVector(30, SeamHalf + 40, 40));

    // ---- The hold's life: jars on the shelves and floor (a cigarette each), the rats.
    for (const FVector& J : ShipJars()) if (AClayJar::PlaceAt(World, ShipWorld(J + FVector(0, 0, 2)), J.X * 3.f, 1)) ++JarsPlaced;
    for (const FVector& R : ShipRats()) if (AEnemyRat::PlaceAt(World, ShipWorld(R + FVector(0, 0, 2)), R.Y * 2.f)) ++RatsPlaced;

    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_BUILT tris=%d missing=%d props=%d lamps=%d swingers=%d ladders=%d jars=%d rats=%d"),
        Triangles, Missing, Props, Lamps.Num(), Swingers.Num(), Ladders, JarsPlaced, RatsPlaced);
}

void AShipSetting::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    // The ship rides a long swell: the lamps and hammocks swing with it (about a 7 s roll), the lamps' flames flicker.
    const float Roll = 4.f * FMath::Sin(Clock * 2 * PI / 7.3f), Pitch = 1.6f * FMath::Sin(Clock * 2 * PI / 9.1f + .8f);
    for (int32 I = 0; I < Swingers.Num(); ++I)
        if (Swingers[I]) Swingers[I]->SetRelativeRotation(FRotator(Pitch * (1 + .2f * FMath::Sin(SwingPhase[I])), 0, Roll * (1 + .15f * FMath::Cos(SwingPhase[I] * 1.3f))));
    for (int32 I = 0; I < Lamps.Num(); ++I)
        if (Lamps[I] && LampBase.IsValidIndex(I)) Lamps[I]->SetIntensity(LampBase[I] * (1.f + .06f * FMath::Sin(Clock * 11.f + I * 1.7f) + .04f * FMath::Sin(Clock * 23.f + I)));
    // Now and then a timber creaks somewhere close.
    if (Clock > NextCreak)
    {
        NextCreak = Clock + FMath::FRandRange(3.5f, 8.f);
        static const TCHAR* Creaks[] = {TEXT("SFX_ShipCreak_00"), TEXT("SFX_ShipCreak_01"), TEXT("SFX_ShipCreak_02"), TEXT("SFX_ShipCreak_03")};
        const TCHAR* Name = Creaks[FMath::RandRange(0, 3)];
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name)))
            if (auto* Chuck = UGameplayStatics::GetPlayerPawn(this, 0))
                UGameplayStatics::PlaySoundAtLocation(this, S, Chuck->GetActorLocation() + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-350.f, 350.f), 120.f), FMath::FRandRange(.35f, .55f), FMath::FRandRange(.9f, 1.1f));
    }
}

int32 AShipSetting::Check() const
{
    UWorld* World = GetWorld();
    int32 Failures = 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCheck), false);
    if (const APawn* Chuck = UGameplayStatics::GetPlayerPawn(World, 0)) Q.AddIgnoredActor(Chuck);
    // Every spawn stands on its floor, with room for him.
    for (int32 I = 0; I < 5; ++I)
    {
        const FVector S = ShipSpawn(EShipSpawn(I));
        FHitResult Hit;
        const bool bFloor = World->LineTraceSingleByChannel(Hit, S, S - FVector(0, 0, 60), ECC_Visibility, Q);
        const float Gap = bFloor ? static_cast<float>(S.Z - 34.65f - Hit.ImpactPoint.Z) : 999.f;
        const bool bRoom = !World->OverlapAnyTestByChannel(S + FVector(0, 0, 2), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(15.f, 32.5f), Q);
        Failures += !(bFloor && FMath::Abs(Gap) < 3 && bRoom);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SPAWN space=%s floor=%d gap=%.1f room=%d"), ShipSpaceName(I), bFloor, Gap, bRoom);
    }
    // Each doorway is open to a standing human (180 cm) and to him.
    struct FDoorCheck { const TCHAR* Name; float X, Y; };
    const FDoorCheck Doors[] = {{TEXT("cabin"), CabinFore, CabinDoorY}, {TEXT("quarters"), ArrivalAft, QuartersDoorY}, {TEXT("galley"), ArrivalFore, GalleyDoorY}};
    for (const FDoorCheck& D : Doors)
    {
        const FVector A = ShipWorld(FVector(D.X - 60, D.Y, LowerZ + 100)), Bp = ShipWorld(FVector(D.X + 60, D.Y, LowerZ + 100));
        const bool bBlocked = World->SweepTestByChannel(A, Bp, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(30.f, 88.f), Q);
        Failures += bBlocked;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_DOOR %s open=%d"), D.Name, !bBlocked);
    }
    // The hull keeps the sea out below the waterline: a trace outward from the hold's middle meets planking each side.
    for (float Side : {1.f, -1.f})
    {
        FHitResult Hit;
        const bool bHull = World->LineTraceSingleByChannel(Hit, ShipWorld(FVector(0, 0, HoldZ + 60)), ShipWorld(FVector(0, Side * 700, HoldZ + 60)), ECC_Visibility, Q);
        Failures += !bHull;
    }
    Failures += Missing > 0;
    Failures += Ladders != 1;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CHECK failures=%d missing=%d tris=%d jars=%d rats=%d ladders=%d"), Failures, Missing, Triangles, JarsPlaced, RatsPlaced, Ladders);
    return Failures;
}
