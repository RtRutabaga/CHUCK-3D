#include "ShipSetting.h"
#include "ShipCrew.h"
#include "ShipStorm.h"
#include "ShipRope.h"
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
#include "ChuckCharacter.h"
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
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "ShipEnding.h"

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
        // Real rope (2026-11-08): a smooth tube along Path with the rope scan's strands laid round it (ShipRope.h; M_Rope).
        void Cord(const TArray<FVector>& Path, float R)
        {
            ShipRope::FMesh M;
            ShipRope::Tube(M, Path, R, 0.f, R < 1.2f ? 8 : 10);
            const int32 Base = V.Num();
            V.Append(M.V); N.Append(M.N); UV.Append(M.UV); Tan.Append(M.Tan);
            for (const int32 I : M.T) T.Add(Base + I);
        }
    };

    struct FParts
    {
        FSurf Hull, Deck, Wood, WoodDecor, StoneDecor, Dark, Iron, Canvas, CanvasDecor, Stone, Brick, Brass, Rug, Glass, Flame, Hidden, Chest, Rigging, Lumber, Veil;
        FSurf Hemp;    // the running rigging and every other rope in natural hemp (2026-11-08; the standing rigging, Rigging, is tarred)
        FSurf Wheel;   // the helm's rim, spokes and handles, about its hub (2026-11-04: it turns)
        FSurf ForeRatlines;   // the fore lower shrouds' ratlines, which carry away in the storm (2026-11-08, ShipEnding.cpp)
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
    const TCHAR* RopeMat = TEXT("/Game/Art/Materials/M_Rope.M_Rope");                 // Tools/create_rope_material.py
    const TCHAR* TarredRopeMat = TEXT("/Game/Art/Materials/MI_RopeTarred.MI_RopeTarred");
    const TCHAR* SeaMat = TEXT("/Game/Art/Ship/M_ShipSea.M_ShipSea");   // Tools/create_ship_sea.py

    // ------------------------------------------------------------------ openings in the planking
    struct FOpening { float Side, X0, X1, Z0, Z1; bool bLid; };
    // Rows of the hull's mesh (cm above the keel): every opening's sill and head is one.
    // (Above the weather deck the planking stops a little over it: the waist's at WaistTopZ, the quarterdeck's at
    // QuarterTopZ. Posts and rails stand above, ShipDeck.inl.)
    const float Rows[] = {0, 1, 40, 80, 130, 190, 250, WaterZ, LowerZ, HoleZ0, 395, 435, HoleZ1, 500, DeckBeam, DeckZ, WaistTopZ, 650, 700, QuarterTopZ};
    float PlankTopAt(float X) { return X < QuarterFore - .1f ? QuarterTopZ : WaistTopZ; }
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

    // The bowsprit: its heel between the bitts on the deck, out over the stem and up (T 0..1 along it).
    FVector BowspritAt(float T) { return FMath::Lerp(FVector(BowX - 170, 0, DeckZ + 38), FVector(BowX + 650, 0, DeckZ + 360), T); }

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
                    if (CellOpen(Side, X0, X1, Z0, Z1) || Z1 > PlankTopAt((X0 + X1) * .5f) + .1f) continue;
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
        // The planking's top edge, the length of each side, and where the quarterdeck's higher planking steps down to
        // the waist's.
        for (float Side : {1.f, -1.f})
        {
            for (int32 I = 0; I + 1 < Xs.Num(); ++I)
            {
                const float X0 = Xs[I], X1 = Xs[I + 1], Z = PlankTopAt((X0 + X1) * .5f);
                P.Hull.Flat(HullPoint(X0, Z, Side, false), HullPoint(X1, Z, Side, false), HullPoint(X1, Z, Side, true), HullPoint(X0, Z, Side, true), FVector::UpVector);
            }
            P.Hull.Flat(HullPoint(QuarterFore, WaistTopZ, Side, false), HullPoint(QuarterFore, QuarterTopZ, Side, false), HullPoint(QuarterFore, QuarterTopZ, Side, true), HullPoint(QuarterFore, WaistTopZ, Side, true), FVector::ForwardVector);
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
        P.Wood.Spar(BowspritAt(0), BowspritAt(1), 20, 10);   // the bowsprit, its heel in the bitts on the deck
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
        // (The transom stands to QuarterRailZ: the taffrail. The quarterdeck's sides are planked to QuarterTopZ, with
        // posts and rails above, ShipDeck.inl.)
        P.Hull.B(SternX - 12, SternX + 2, -OuterAt(SternX + 1, RailZ) * .97f, OuterAt(SternX + 1, RailZ) * .97f, QuarterRailZ, QuarterRailZ + 8);
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
        // (2026-11-05: the posts stand 1 cm proud into the opening and stop under the lintel, and the lintel sits 1 cm
        // proud below the opening's head and a little proud of the posts' faces: before, the frame's faces lay in the
        // bulkhead's and each other's planes, and flickered.)
        for (const FDoor& D : Doors)
        {
            for (float F : {-1.f, 1.f})
                P.WoodDecor.B(X - Bulkhead - 4, X + Bulkhead + 4, D.Y + F * D.Half - (F > 0 ? 1 : 8), D.Y + F * D.Half + (F > 0 ? 8 : -1), Z0, Z0 + D.Height - 1);
            P.WoodDecor.B(X - Bulkhead - 4.6f, X + Bulkhead + 4.6f, D.Y - D.Half - 8.6f, D.Y + D.Half + 8.6f, Z0 + D.Height - 1, Z0 + D.Height + 10);
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

#include "ShipDeck.inl"
#include "ShipSea.inl"
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
        case EShipSpawn::Deck: return {{-80, 160}, DeckZ, 0};
        default: return {{235, 210}, LowerZ, 112};
        }
    }
}
FVector ShipSpawn(EShipSpawn S) { const FSpawnDef D = SpawnDef(S); return ShipWorld(FVector(D.At.X, D.At.Y, D.Floor + 34.65f)); }
float ShipSpawnYaw(EShipSpawn S) { return SpawnDef(S).Yaw; }

namespace
{
    int32 GLastSpace = -1;   // the last space he was in aboard (AShipSetting::Tick): over the side, he came from there
    bool OutsideHull(const FVector& L) { return FMath::Abs(L.Y) > OuterAt(L.X, FMath::Clamp(static_cast<float>(L.Z), 1.f, RailZ)) + 5 || L.X < SternX - 12 || L.X > BowX + 5; }
}
int32 ShipSpaceAt(const FVector& World)
{
    if (!InShip(World)) return -1;
    const FVector L = ShipLocal(World);
    // Outside the planking (gone over the side, or out along a yard): the space he was last in, or else by where along
    // the ship he is on the lower deck.
    if (OutsideHull(L))
    {
        if (GLastSpace >= 0) return GLastSpace;
        return L.X > ArrivalAft && L.X < ArrivalFore ? int32(EShipSpawn::Arrival) : L.X >= ArrivalFore ? int32(EShipSpawn::Galley) : L.X > CabinFore ? int32(EShipSpawn::Quarters) : int32(EShipSpawn::Cabin);
    }
    if (L.Z < LowerZ - 20 && L.Z > HoldZ - 40 && L.X > HoldAft && L.X < HoldFore) return int32(EShipSpawn::Hold);
    // On the weather deck or the quarterdeck, or above them (the rig).
    if (L.Z > (L.X < QuarterFore ? QuarterZ : DeckZ) - 10) return int32(EShipSpawn::Deck);
    if (L.X > ArrivalAft && L.X < ArrivalFore) return int32(EShipSpawn::Arrival);
    if (L.X >= ArrivalFore && L.X < GalleyFore) return int32(EShipSpawn::Galley);
    if (L.X > CabinFore && L.X <= ArrivalAft) return int32(EShipSpawn::Quarters);
    if (L.X > SternX && L.X <= CabinFore) return int32(EShipSpawn::Cabin);
    return -1;
}
const TCHAR* ShipSpaceName(int32 Space)
{
    static const TCHAR* Names[] = {TEXT("Arrival"), TEXT("Hold"), TEXT("Quarters"), TEXT("Galley"), TEXT("Cabin"), TEXT("Deck")};
    return Space >= 0 && Space < ShipSpawns ? Names[Space] : TEXT("None");
}
bool ShipRespawn(const FVector& World, FVector& Out)
{
    if (!InShip(World)) return false;
    const int32 Space = ShipSpaceAt(World);
    // (In the storm, once the rupture has closed the companion: always back on deck, 2026-11-04.)
    Out = ShipSpawn(ShipRuptureOpen() ? EShipSpawn::Deck : Space >= 0 ? EShipSpawn(Space) : EShipSpawn::Arrival);
    return true;
}
bool IsShipSpawn(const FVector& World, float& Yaw)
{
    for (int32 I = 0; I < ShipSpawns; ++I)
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
    auto* Sun_ = NewObject<UDirectionalLightComponent>(this);
    Sun_->SetMobility(EComponentMobility::Movable); Sun_->SetAtmosphereSunLight(true);
    Sun_->SetIntensity(5.f); Sun_->SetLightColor(FLinearColor(1.f, .95f, .86f));
    Sun_->DynamicShadowDistanceMovableLight = 6000; Sun_->DynamicShadowCascades = 3;
    Sun_->SetVolumetricScatteringIntensity(2.f); Sun_->bCastCloudShadows = true;
    Sun_->ForwardShadingPriority = 1;   // (over the storm's lightning light, ApplyStorm)
    Attach(Sun_, FVector::ZeroVector, FRotator(-38, 62, 0));
    Sun = Sun_;
    SunBase = Sun->GetComponentQuat();
    // The storm's lightning (ApplyStorm): a hard white light from the strike's side, casting the hull's and the rig's
    // shadows, on only during a flash.
    FlashLight = NewObject<UDirectionalLightComponent>(this);
    FlashLight->SetMobility(EComponentMobility::Movable); FlashLight->SetAtmosphereSunLight(false);
    FlashLight->SetIntensity(0.f); FlashLight->SetLightColor(FLinearColor(.86f, .9f, 1.f));
    FlashLight->DynamicShadowDistanceMovableLight = 5000; FlashLight->DynamicShadowCascades = 2;
    FlashLight->SetVolumetricScatteringIntensity(1.5f);
    Attach(FlashLight); FlashLight->SetVisibility(false);
    // The open sky (milestone 2: the sea box is gone). Its planet is centred under the waterline, and moved every frame
    // as the sea rocks so the horizon tilts with the sea (Rock); its ground, should any show, is the far sea's blue.
    Sky = NewObject<USkyAtmosphereComponent>(this);
    Sky->TransformMode = ESkyAtmosphereTransformMode::PlanetCenterAtComponentTransform;
    Sky->SetRayleighScattering(FLinearColor(.13f, .43f, .66f)); Sky->SetMieScatteringScale(.008f); Sky->SetHeightFogContribution(1.f);
    Sky->SetGroundAlbedo(FColor(8, 28, 44));
    Attach(Sky, FVector(0, 0, WaterZ - Sky->BottomRadius * 100000.f));
    // Fair-weather clouds far off (the fall cutscene's cloud material, thinner and higher: they read as cloud kilometres away).
    if (!FParse::Param(FCommandLine::Get(), TEXT("ChuckShipNoClouds")))
        if (auto* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Cutscenes/FallToChult/MI_FallClouds.MI_FallClouds")))
        {
            auto* Clouds = NewObject<UVolumetricCloudComponent>(this);
            Clouds->SetLayerBottomAltitude(1.4f); Clouds->SetLayerHeight(1.6f);
            auto* Mid = UMaterialInstanceDynamic::Create(CloudMaterial, this);
            Mid->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), -.18f);
            Mid->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), .01f);
            Clouds->SetMaterial(Mid);
            Attach(Clouds);
            CloudLayer = Clouds; CloudMid = Mid;
        }
    // Below decks the sky's fill is faint: without dynamic GI it would light the inside as if there were no hull.
    auto* Fill = NewObject<USkyLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->bRealTimeCapture = true;
    Fill->bLowerHemisphereIsBlack = true; Fill->SetLowerHemisphereColor(FLinearColor(.06f, .05f, .04f));
    Fill->SetIntensity(.22f); Attach(Fill, FVector(0, 0, 1500));
    SkyFill = Fill;
    auto* Fog = NewObject<UExponentialHeightFogComponent>(this);
    // Thin: the sea box beyond must stay clear (a thicker fog whitened every opening); the volumetric fog near by shows the shafts.
    Fog->SetFogDensity(.0006f); Fog->SetFogHeightFalloff(.02f); Fog->SetFogMaxOpacity(.25f); Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogScatteringDistribution(.6f); Fog->SetVolumetricFogExtinctionScale(1.4f);
    Fog->SetVolumetricFogDistance(2500); Fog->SetFogInscatteringColor(FLinearColor(.45f, .55f, .66f));   // (the sea's haze, as pale as the sky at the horizon)
    Attach(Fog, FVector(0, 0, WaterZ));
    HazeFog = Fog;
    auto* Post = NewObject<UPostProcessComponent>(this);
    Post->bUnbound = true; Post->Priority = 110;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Histogram;
    S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = -.6f;
    S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 5.f;   // (3 below decks in milestone 1; the open deck is brighter)
    S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -.4f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .45f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .7f;
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.95f, .95f, .92f, 1);
    S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = .7f;
    S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = .8f;
    Attach(Post);
    Grade = Post;
    // The open sea (BuildSea): a real surface to the horizon, its waves meeting the hull.
    BuildSea();
    // The sea against the hull and the ship's timbers working: a slow wash and creaks (SFX_ShipWash, SFX_ShipCreak_*,
    // generated); and since 2026-11-03 recordings (Tools/make_ship_sounds.py): the sea rushing along the hull and the wind
    // in the rigging on deck, water lapping at the planking below. Tick mixes them by where he is.
    auto Loop = [&](const TCHAR* Name) -> UAudioComponent*
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name));
        if (!Sound) { UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_SOUND_MISSING %s"), Name); return nullptr; }
        auto* A = NewObject<UAudioComponent>(this); A->SetSound(Sound); A->bAutoActivate = false; A->bIsUISound = false;
        A->bAllowSpatialization = false;
        Attach(A); A->SetVolumeMultiplier(0.f); A->Play();
        return A;
    };
    Wash = Loop(TEXT("SFX_ShipWash_00"));
    Water = Loop(TEXT("SFX_ShipWater_00"));
    Lap = Loop(TEXT("SFX_ShipLap_00"));
    Rigging = Loop(TEXT("SFX_ShipRigging_00"));
    auto Hearing = [&](float Inner, float Falloff)
    {
        auto* H = NewObject<USoundAttenuation>(this);
        H->Attenuation.bAttenuate = true; H->Attenuation.bSpatialize = true;
        H->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        H->Attenuation.AttenuationShapeExtents = FVector(Inner, 0.f, 0.f);
        H->Attenuation.FalloffDistance = Falloff;
        return H;
    };
    NearHearing = Hearing(250.f, 1600.f);   // feet on the planking
    FarHearing = Hearing(450.f, 3200.f);    // steel on steel, across the ship
}

void ShipSound(const UObject* Context, const TCHAR* Stem, int32 Count, const FVector& At, float Volume, bool bCarries)
{
    if (AShipSetting* S = Context ? AShipSetting::Get(Context->GetWorld()) : nullptr) S->PlayAt(Stem, Count, At, Volume, bCarries);
}

bool ShipHearsHold(const UObject* Context)
{
    const APawn* Pawn = Context ? UGameplayStatics::GetPlayerPawn(Context, 0) : nullptr;
    return Pawn && ShipSpaceAt(Pawn->GetActorLocation()) == int32(EShipSpawn::Hold);
}

void AShipSetting::PlayAt(const TCHAR* Stem, int32 Count, const FVector& At, float Volume, bool bCarries)
{
    TArray<int32>* Set = SoundSets.Find(Stem);
    if (!Set)
    {
        Set = &SoundSets.Add(Stem);
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I)))
                Set->Add(SoundPool.Add(Sound));
        if (Set->Num() < Count) UE_LOG(LogTemp, Warning, TEXT("CHUCK_SHIP_SOUND_MISSING %s %d/%d"), Stem, Set->Num(), Count);
    }
    if (!Set->Num()) return;
    // Through a deck (the weather deck heard from below, or the lower decks from on deck): about a third as loud.
    float V = Volume;
    if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
        if ((ShipSpaceAt(Pawn->GetActorLocation()) == int32(EShipSpawn::Deck)) != (ShipSpaceAt(At) == int32(EShipSpawn::Deck))) V *= .3f;
    UGameplayStatics::PlaySoundAtLocation(this, SoundPool[(*Set)[FMath::RandRange(0, Set->Num() - 1)]], At, V, FMath::FRandRange(.95f, 1.05f), 0.f,
        bCarries ? FarHearing : NearHearing);
    ++SoundsPlayed.FindOrAdd(Stem);
}

void AShipSetting::Build()
{
    if (bBuilt) return;
    bBuilt = true;
    UWorld* World = GetWorld();
    BuildEnvironment();
    FParts P;
    GJars.Reset(); GLastSpace = -1;
    BuildHull(P);

    // ---- Decks and floors.
    const FHole MainHatch{HatchX0, HatchX1, -HatchHalf, HatchHalf};
    const FHole MainGrating{HatchX0, HatchX1, -HatchHalf, HatchHalf};
    const FHole ForeGrating{700, 820, -60, 60};
    const FHole AftGrating{-760, -640, -60, 60};
    const FHole Companion{CompX0, CompX1, CompY0, CompY1};
    const FHole Skylight{SkyX0, SkyX1, -SkyHalf, SkyHalf};
    Floor(P.Deck, HoldAft, HoldFore, HoldZ, 10, {});
    Floor(P.Deck, SternX + 2, GalleyFore, LowerZ, 15, {MainHatch});
    Beams(P.Wood, HoldAft, HoldFore, LowerBeam, LowerZ - 15 - LowerBeam, {MainHatch});
    // The weather deck, open to the sky (its gratings still light the lower deck; the companion hatch has a ladder
    // down to the crew quarters), and the quarterdeck, higher, over the cabin, with the cabin's skylight.
    Floor(P.Deck, QuarterFore, BowX, DeckZ, 15, {MainGrating, ForeGrating, AftGrating, Companion});
    Beams(P.Wood, QuarterFore, GalleyFore + 300, DeckBeam, DeckZ - 15 - DeckBeam, {MainGrating, ForeGrating, AftGrating, Companion});
    Floor(P.Deck, SternX + 2, QuarterFore + 8, QuarterZ, 15, {Skylight});
    Beams(P.Wood, SternX + 2, QuarterFore, QuarterZ - 25, 10, {Skylight});
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

    // ---- The masts, stepped on the keel, through both decks; their partners at each deck. Above the deck: the rig
    // (ShipDeck.inl).
    for (float MX : {MainMastX, ForeMastX})
    {
        P.Wood.Spar(FVector(MX, 0, HoldZ), FVector(MX, 0, DeckZ), MastR, MastR, 14);
        for (float Z : {LowerZ, DeckBeam}) P.Wood.B(MX - 48, MX + 48, -48, 48, Z == LowerZ ? Z : Z - 6, Z == LowerZ ? Z + 6 : Z);
        for (float Z : {HoldZ + 60, LowerZ + 80, LowerZ + 170}) P.Iron.Spar(FVector(MX, 0, Z), FVector(MX, 0, Z + 6), MastR + 1.5f, MastR + 1.5f, 14);
    }
    BuildWeatherDeck(P);
    BuildRig(P);
    for (const FChuckClimbable& L : DeckLadders()) { AddChuckClimbable(L); ++Ladders; }
    for (const FChuckClimbable& L : DeckShrouds()) { AddChuckClimbable(L); ++Shrouds; }
    for (const FChuckSwingGrip& G : YardGrips()) { AddChuckSwingGrip(G); ++Grips; }
    // Stanchions under the lower deck's beams.
    for (float X : {-950.f, -150.f, 750.f}) P.Wood.B(X - 8, X + 8, -8, 8, HoldZ, LowerBeam);

    // ---- The arrival compartment: a sail locker. In its starboard-aft corner a pile of the ship's spare lumber, stacked
    // boards with heavy timbers heaped on them up to the beams; the crawlway the escape ends in runs through the stack, and
    // where it comes out a small piece of the temple's masonry sits in the timber, its mouth ringed with the cellar's oil
    // haze: the two worlds run together there (user 2026-11-02: was a heap of temple blocks).
    {
        const float Z0 = LowerZ, T0 = SeamY - SeamHalf, T1 = SeamY + SeamHalf, Roof = Z0 + SeamH;
        const float Wall = -InnerAt(0, Z0 + 40) + 2;   // the hull's inside there
        const float Aft = ArrivalAft + Bulkhead, Inner = T1 + 62;   // the pile's aft end (the bulkhead) and its inboard side
        const float StoneX0 = SeamX1 - 52, StoneOut = SeamX1 + 6, Jamb = 30, StoneTop = Roof + 26;
        FRandomStream R(20261101);
        // What he can't pass: the crawlway's sides and roof and its blind end, as before (invisible; the boards dress them).
        P.Hidden.B(Aft, SeamX1, Wall, T0, Z0, Roof + 28);
        P.Hidden.B(Aft, SeamX1, T1, Inner, Z0, Roof + 20);
        P.Hidden.B(Aft, SeamX1, Wall, Inner, Roof, Roof + 34);
        P.Hidden.B(Aft, SeamX0, Wall, Inner, Z0, Roof + 34);
        // The stack: boards laid fore and aft in rows either side of the crawlway (its walls), then boards laid across over
        // it (its roof), each board's ends a little off its neighbours'. Over the masonry only the top rows run on forward.
        constexpr float Board = 6.5f;
        int32 Layer = 0;
        for (float Z = Z0; Z < Roof + 34 - 1; Z += Board, ++Layer)
        {
            const bool bAcross = Z >= Roof;
            const bool bOverStone = Z >= StoneTop;
            if (!bAcross)
            {
                // Rows from the hull in to the crawlway's starboard side, and from its inboard side in to the pile's edge.
                for (int32 Part = 0; Part < 2; ++Part)
                {
                    const float Y0 = Part == 0 ? Wall : T1, Y1 = Part == 0 ? T0 : Inner;
                    float Y = Y0;
                    while (Y < Y1 - 4)
                    {
                        const float W = FMath::Min(R.FRandRange(17, 27), Y1 - Y);
                        const float Ya = Y, Yb = Y + W - .8f;
                        // Beside the masonry (its jambs are Jamb wide) the boards stop at it; outside that they run on past.
                        const bool bByStone = (Part == 0 && Yb > T0 - Jamb) || (Part == 1 && Ya < T1 + Jamb);
                        const float X1 = bByStone ? StoneX0 : SeamX1 + R.FRandRange(-6, 10);
                        // The boards that face the crawlway are square to it, so its walls are straight inside.
                        const bool bFace = (Part == 0 && Yb >= T0 - 1) || (Part == 1 && Ya <= T1 + 1);
                        const float X0 = bFace ? Aft : Aft + R.FRandRange(0, 6);
                        P.Lumber.B(X0, X1, Ya, Yb, Z, Z + Board - .5f);
                        Y += W;
                    }
                }
            }
            else
            {
                const float X1 = bOverStone ? SeamX1 + 2 : StoneX0;
                float X = Aft;
                while (X < X1 - 4)
                {
                    const float W = FMath::Min(R.FRandRange(18, 28), X1 - X);
                    P.Lumber.B(X, X + W - .8f, Wall + R.FRandRange(0, 5), Inner + R.FRandRange(-6, 6), Z, Z + Board - .5f);
                    X += W;
                }
            }
        }
        P.Lumber.B(Aft, SeamX0, T0, T1, Z0, Roof);   // the boards' ends at the crawlway's blind end
        const float StackTop = Z0 + Layer * Board;
        // The masonry at the mouth: two jambs and a lintel of the temple's stone set into the stack, a block of its floor,
        // and a few broken pieces of it standing proud of the boards round its edge.
        P.Stone.B(StoneX0, SeamX1, T0, T1, Z0, Z0 + 2);
        P.Stone.B(StoneX0, StoneOut, T0 - Jamb, T0, Z0, StoneTop);
        P.Stone.B(StoneX0, StoneOut, T1, T1 + Jamb, Z0, StoneTop - 4);
        P.Stone.B(StoneX0, StoneOut, T0 - Jamb, T1 + Jamb, Roof, StoneTop);
        P.StoneDecor.TurnedBox(FVector(StoneOut - 14, T0 - Jamb - 4, StoneTop - 12), FVector(12, 9, 13), FRotator(4, -7, 6));
        P.StoneDecor.TurnedBox(FVector(StoneOut - 18, T1 + Jamb + 5, Z0 + 26), FVector(10, 8, 14), FRotator(-5, 9, -4));
        P.StoneDecor.TurnedBox(FVector(StoneOut - 22, SeamY + 18, StoneTop + 5), FVector(14, 18, 6), FRotator(3, 6, -5));
        // A little fine grit spilled from its mouth.
        for (int32 I = 0; I < 6; ++I) P.StoneDecor.TurnedBox(FVector(SeamX1 + R.FRandRange(8, 44), SeamY + R.FRandRange(-45, 45), Z0 + 1), FVector(R.FRandRange(2.5f, 5), R.FRandRange(2.5f, 5), 1.2f), FRotator(0, R.FRandRange(0, 90), 0));
        // Heavy timbers heaped on the stack, crossed, and two of them leant up against the hull's side under the deck beams.
        auto Timber = [](FSurf& S, const FVector& A, const FVector& Bp, float W, float H, float Roll)
        {
            FRotator Rt = (Bp - A).Rotation(); Rt.Roll = Roll;
            S.TurnedBox((A + Bp) * .5f, FVector(static_cast<float>(FVector::Dist(A, Bp)) * .5f, W * .5f, H * .5f), Rt);
        };
        const float HeapZ = StackTop;
        Timber(P.Wood, FVector(Aft + 6, -395, HeapZ + 10), FVector(SeamX1 - 10, -398, HeapZ + 10), 20, 20, 0);
        Timber(P.Wood, FVector(Aft + 20, -345, HeapZ + 10), FVector(StoneX0 + 30, -340, HeapZ + 10), 18, 20, 0);
        Timber(P.Wood, FVector(Aft + 2, -262, HeapZ + 9), FVector(SeamX1 - 30, -255, HeapZ + 9), 16, 18, 0);
        for (const float X : {-112.f, -18.f, 66.f}) Timber(P.Wood, FVector(X + R.FRandRange(-6, 6), Wall + 12, HeapZ + 29.5f), FVector(X + R.FRandRange(-12, 12), Inner - 4, HeapZ + 29.5f), 18, 19, 0);
        Timber(P.Lumber, FVector(-140, -372, HeapZ + 41.5f), FVector(34, -360, HeapZ + 41.5f), 26, 5, 0);
        Timber(P.Lumber, FVector(-120, -330, HeapZ + 41), FVector(58, -322, HeapZ + 41), 24, 4, 2);
        Timber(P.Lumber, FVector(-60, -248, HeapZ + 41.5f), FVector(80, -232, HeapZ + 39.5f), 22, 4, -3);
        // Leant against the hull: up from the heap's top to under the deck beams, their feet on the heap.
        Timber(P.Wood, FVector(-90, -330, HeapZ + 39), FVector(-110, Wall + 14, DeckBeam - 12), 16, 16, 0);
        Timber(P.Wood, FVector(40, -320, HeapZ + 39), FVector(70, Wall + 14, DeckBeam - 30), 16, 15, 0);
        Timber(P.Lumber, FVector(-20, -285, HeapZ + 41), FVector(-34, Wall + 8, DeckBeam - 40), 24, 4, 0);
        // Off-cuts on the floor before it.
        Timber(P.Lumber, FVector(SeamX1 + 40, -210, Z0 + 2.5f), FVector(SeamX1 + 120, -176, Z0 + 2.5f), 14, 4.5f, 0);
        Timber(P.Lumber, FVector(SeamX1 + 30, -402, Z0 + 2.5f), FVector(SeamX1 + 95, -398, Z0 + 2.5f), 18, 4.5f, 0);
        Timber(P.Wood, FVector(SeamX1 + 60, -390, Z0 + 7.5f), FVector(SeamX1 + 64, -330, Z0 + 7.5f), 16, 14, 0);
        // The oil haze round the masonry's mouth: a band standing off the stone's edge on the stack's face, fading out away
        // from it (the cellar's M_AstralOilMist fades up V and at U's ends, so its ends are at the floor).
        {
            TArray<FVector2D> Edge;   // (Y, Z) round the jambs and lintel, from the floor on the starboard side
            const float Ya = T0 - Jamb, Yb = T1 + Jamb, Za = StoneTop;
            for (float Z = Z0; Z < Za; Z += 8) Edge.Add({Ya, Z});
            for (float Y = Ya; Y < Yb; Y += 8) Edge.Add({Y, Za});
            for (float Z = Za; Z >= Z0 - .1f; Z -= 8) Edge.Add({Yb, Z});
            const FVector2D Mid((Ya + Yb) * .5f, Z0 + 10);
            constexpr int32 Levels = 8; constexpr float Reach = 46;
            const int32 N = Edge.Num();
            auto Pt = [&](int32 I, int32 J)
            {
                const float U = I / float(N - 1), H = J / float(Levels);
                const FVector2D E = Edge[I], Out = (E - Mid).GetSafeNormal();
                const FVector2D Q = E + Out * (H * Reach * (.85f + .15f * FMath::Sin(U * UE_TWO_PI * 4)));
                return FVector(StoneOut + 2 + H * H * 9 + 2 * FMath::Sin(U * UE_TWO_PI * 3), Q.X, FMath::Max(static_cast<float>(Q.Y), Z0 + 1.f));
            };
            for (int32 I = 0; I < N - 1; ++I) for (int32 J = 0; J < Levels; ++J)
            {
                const int32 First = P.Veil.V.Num();
                P.Veil.Flat(Pt(I, J), Pt(I + 1, J), Pt(I + 1, J + 1), Pt(I, J + 1), FVector(1, 0, 0));
                if (P.Veil.V.Num() == First) continue;
                const float U0 = I / float(N - 1), U1 = (I + 1) / float(N - 1), V0 = J / float(Levels), V1 = (J + 1) / float(Levels);
                P.Veil.UV[First] = {U0, V0}; P.Veil.UV[First + 1] = {U1, V0}; P.Veil.UV[First + 2] = {U1, V1}; P.Veil.UV[First + 3] = {U0, V1};
            }
        }

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
                P.Hemp.Cord({C - Ac + FVector(0, 0, Z + 1), C - Ac + FVector(0, 0, Top + 1), C + Ac + FVector(0, 0, Top + 1), C + Ac + FVector(0, 0, Z + 1)}, 1.4f);
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
        // The chest: iron-bound, a brass lock plate. Its lid, on its hinge, is AShipChest's (ShipStorm.cpp, 2026-11-03); the
        // dark of its inside shows when that opens.
        P.Chest.B(-1230, -1140, 150, 205, Z0, Z0 + 40);
        P.Chest.B(-1230, -1140, 150, 154, Z0 + 40, Z0 + 42); P.Chest.B(-1230, -1140, 201, 205, Z0 + 40, Z0 + 42);
        P.Chest.B(-1230, -1226, 154, 201, Z0 + 40, Z0 + 42); P.Chest.B(-1144, -1140, 154, 201, Z0 + 40, Z0 + 42);
        P.Dark.B(-1226, -1144, 154, 201, Z0 + 40, Z0 + 40.5f);
        for (float X : {-1222.f, -1148.f}) P.Iron.B(X - 3, X + 3, 149, 206, Z0, Z0 + 42.5f);
        P.Brass.B(-1190, -1180, 147, 150, Z0 + 30, Z0 + 41);
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
        if (Mesh == CoilMesh) Coils.Add(C);
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
    Prop(CoilMesh, FVector(-40, 350, LowerZ), FRotator(0, -30, 0));   // (on the floor: it hung 38 cm in the air, 2026-11-08)
    Barrel(285, -330, LowerZ, false, 20);
    for (float Y : {-260.f, -180.f, 220.f}) Barrel(1090, Y, LowerZ, false, Y);
    Sack(860, -250, LowerZ, 30); Sack(820, -300, LowerZ, -20);
    if (TankardMesh) for (const FVector& T : {FVector(-300, -135, 0), FVector(-370, -205, 0), FVector(-330, -235, 0)}) Prop(TankardMesh, FVector(T.X, T.Y, LowerZ + 76), FRotator(0, T.X, 0));
    // The weather deck's stores: crates lashed by the port rail forward (two high: from them he can see over it), water
    // casks aft of the main mast to starboard, coils of rope.
    for (const FVector& C : {FVector(640, 372, 0), FVector(706, 372, 0), FVector(640, 372, 1)})
    {
        const FVector At(C.X, C.Y, DeckZ + 30 + C.Z * 60);
        Prop(CrateMesh, At, FRotator(0, C.Z * 4 - 2, 0));
        P.Hidden.TurnedBox(At, FVector(30, 32.5f, 30), FRotator(0, C.Z * 4 - 2, 0));
    }
    for (const FVector& Bp : {FVector(-660, -330, 0), FVector(-660, -268, 0), FVector(-722, -300, 0)}) Barrel(Bp.X, Bp.Y, DeckZ, false, Bp.Y);
    Prop(CoilMesh, FVector(-560, 140, DeckZ), FRotator(0, 40, 0));
    Prop(CoilMesh, FVector(360, -160, DeckZ), FRotator(0, -20, 0));
    Prop(CoilMesh, FVector(1180, 170, DeckZ), FRotator(0, 70, 0));
    Prop(CoilMesh, FVector(-1330, 210, QuarterZ), FRotator(0, 10, 0));

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
        // (2026-11-02: the two that hung at -960 straddled the quarterdeck's break, their fore lanyards up through the main
        // deck into the open waist - the user's "floating mop" by the starboard break ladder; they hang forward of it now.)
        const FHam Hams[] = {{-560, 270, 0}, {-760, 260, 10}, {-460, 165, 20}, {-560, -270, 5}, {-760, -260, -5}, {-450, -352, 35}, {-660, 110, 40}, {-860, -110, 45}};
        for (const FHam& Hm : Hams)
        {
            const float Top = Hm.X < QuarterFore ? QuarterZ - 25 : DeckBeam;
            auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->SetWorldLocation(ShipWorld(FVector(Hm.X, Hm.Y, Top))); Pivot->RegisterComponent();
            FSurf C, R;
            constexpr int32 NX = 12, NY = 5; constexpr float Len = 100, Half = 34;
            const float Low = LowerZ + 100 + Hm.Z - Top, EndZ = Low + 46;   // the bed's low point ~1-1.45 m off the floor (2026-11-02: was 1.5-1.95)
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
                for (float V : {-1.f, 0.f, 1.f}) R.Cord({Pt(U < 0 ? 0 : NX, 2 + int32(V) * 2), End}, .5f);
                R.Cord({End, FVector(U * (Len + 40), 0, -2)}, .7f);
            }
            for (auto Pair : {TPair<FSurf*, const TCHAR*>(&C, CanvasMat), TPair<FSurf*, const TCHAR*>(&R, RopeMat)})
            {
                auto* M = NewObject<UProceduralMeshComponent>(this); M->SetupAttachment(Pivot); M->RegisterComponent();
                M->CreateMeshSection(0, Pair.Key->V, Pair.Key->T, Pair.Key->N, Pair.Key->UV, TArray<FColor>(), Pair.Key->Tan, false);
                M->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, Pair.Value)); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Triangles += Pair.Key->T.Num() / 3;
            }
            Swingers.Add(Pivot); SwingPhase.Add(Hm.X * .011f + 1.7f);
        }
    }

    // ---- The sails and the flag (rebuilt every frame as they breathe: UpdateSails).
    BuildSails();

    // ---- Emit.
    Emit(this, TEXT("ShipHull"), P.Hull, WoodMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipDeck"), P.Deck, DeckMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipWood"), P.Wood, WoodMat, true, true, Missing, Triangles);
    Emit(this, TEXT("ShipWoodDecor"), P.WoodDecor, WoodMat, false, true, Missing, Triangles);
    if ((Wheel = Emit(this, TEXT("ShipHelmWheel"), P.Wheel, WoodMat, false, true, Missing, Triangles)) != nullptr) Wheel->SetWorldLocation(ShipWorld(HelmHub()));
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
    Emit(this, TEXT("ShipRigging"), P.Rigging, TarredRopeMat, false, true, Missing, Triangles);   // the tarred standing rigging
    Emit(this, TEXT("ShipHemp"), P.Hemp, RopeMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipForeRatlines"), P.ForeRatlines, TarredRopeMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipLumber"), P.Lumber, DeckMat, false, true, Missing, Triangles);
    Emit(this, TEXT("ShipRiftVeil"), P.Veil, TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist"), false, false, Missing, Triangles);   // the cellar's oil haze
    Emit(this, TEXT("ShipHidden"), P.Hidden, nullptr, true, false, Missing, Triangles);

    // ---- The crawlway through the masonry: on all fours in it, and on the floor just out of its mouth.
    AChuckCharacter::RegisterCrawlSpace(World, FTransform(ShipWorld(FVector((SeamX0 + SeamX1) * .5f, SeamY, LowerZ + 40))), FVector((SeamX1 - SeamX0) * .5f, SeamHalf, 40));
    AChuckCharacter::RegisterCrawlSpace(World, FTransform(ShipWorld(FVector(SeamX1 + 30, SeamY, LowerZ + 40))), FVector(30, SeamHalf + 40, 40));

    // ---- The hold's life: jars on the shelves and floor (a cigarette each), the rats.
    for (const FVector& J : ShipJars()) if (AClayJar::PlaceAt(World, ShipWorld(J + FVector(0, 0, 2)), J.X * 3.f, 1)) ++JarsPlaced;
    for (const FVector& R : ShipRats()) if (AEnemyRat::PlaceAt(World, ShipWorld(R + FVector(0, 0, 2)), R.Y * 2.f)) ++RatsPlaced;
    // ---- The crew (milestone 3, ShipCrew.cpp).
    AShipCrew::Spawn(World);
    // ---- The captain's chest's lid and the storm that the ending's trigger brings (2026-11-03, ShipStorm.cpp).
    AShipChest::Spawn(World);
    AShipStorm::Spawn(World);
    // ---- The ending in the storm: the reef, the leap to the foremast, the fore topsail (2026-11-08, ShipEnding.cpp).
    AShipEnding::Spawn(World);

    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_BUILT tris=%d missing=%d props=%d lamps=%d swingers=%d ladders=%d grips=%d sails=%d jars=%d rats=%d"),
        Triangles, Missing, Props, Lamps.Num(), Swingers.Num(), Ladders, Grips, Sails.Num(), JarsPlaced, RatsPlaced);
}

namespace
{
    TArray<FSailDef> GSails;
    constexpr int32 SailU = 32, SailV = 24, FlagU = 14, FlagV = 9;   // (the sails finer since 2026-11-05: they give way round him)
    // A two-sided sheet over a grid (U across, V down) from a shape function; Tris only when it is first made.
    void GridMesh(TFunctionRef<FVector(float, float)> At, int32 NU, int32 NV, float SizeU, float SizeV, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& UV, TArray<int32>* Tris)
    {
        V.Reset(); N.Reset(); UV.Reset();
        const int32 Count = (NU + 1) * (NV + 1);
        V.SetNumUninitialized(Count * 2); N.SetNumUninitialized(Count * 2); UV.SetNumUninitialized(Count * 2);
        for (int32 J = 0; J <= NV; ++J)
            for (int32 I = 0; I <= NU; ++I)
            {
                const float U = I / float(NU), W = J / float(NV);
                const FVector P = At(U, W);
                // The normal from nearby samples (kept off a staysail's head, where its two head corners meet).
                const float E = .02f, Wn = FMath::Clamp(W, .03f, .97f);
                const FVector DU = At(FMath::Min(1.f, U + E), Wn) - At(FMath::Max(0.f, U - E), Wn);
                const FVector DV = At(U, FMath::Min(1.f, Wn + E)) - At(U, FMath::Max(0.f, Wn - E));
                const FVector Nn = FVector::CrossProduct(DU, DV).GetSafeNormal();
                const int32 K = J * (NU + 1) + I;
                V[K] = P; N[K] = Nn; UV[K] = FVector2D(U * SizeU, W * SizeV) / 150;
                V[K + Count] = P; N[K + Count] = -Nn; UV[K + Count] = UV[K];
            }
        if (!Tris) return;
        Tris->Reset();
        for (int32 J = 0; J < NV; ++J)
            for (int32 I = 0; I < NU; ++I)
            {
                const int32 A = J * (NU + 1) + I, B = A + 1, C = A + NU + 2, D = A + NU + 1;
                Tris->Append({A, C, B, A, D, C});
                Tris->Append({A + Count, B + Count, C + Count, A + Count, C + Count, D + Count});
            }
    }
    float SailWidth(const FSailDef& S) { return FMath::Max(FVector::Dist(S.A, S.B), FVector::Dist(S.D, S.C)); }
    float SailHeight(const FSailDef& S) { return FMath::Max(FVector::Dist(S.A, S.D), FVector::Dist(S.B, S.C)); }
}

void AShipSetting::BuildSea()
{
    FSeaMesh M;
    BuildSeaMesh(M);
    SeaSurface = NewObject<UProceduralMeshComponent>(this, TEXT("ShipSeaSurface"));
    SeaSurface->SetupAttachment(RootComponent); SeaSurface->SetWorldLocation(ShipOrigin()); SeaSurface->RegisterComponent();
    SeaSurface->CreateMeshSection_LinearColor(0, M.V, M.T, M.N, M.UV0, M.UV1, M.UV2, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
    SeaSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision); SeaSurface->SetCastShadow(false);
    SeaSurface->ComponentTags.Add(TEXT("Ship"));
    if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, SeaMat))
    {
        SeaMid = UMaterialInstanceDynamic::Create(Mat, this);
        float Speed = 110.f; FParse::Value(FCommandLine::Get(), TEXT("ChuckShipSeaSpeed="), Speed);
        SeaMid->SetScalarParameterValue(TEXT("Speed"), Speed);
        SeaSurface->SetMaterial(0, SeaMid);
    }
    else ++Missing;
    Triangles += M.T.Num() / 3;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_SEA verts=%d tris=%d material=%s"), M.V.Num(), M.T.Num() / 3, *GetNameSafe(SeaMid));
}

void AShipSetting::BuildSails()
{
    GSails = SailDefs();
    // (The sails' canvas lets the sun through, as thin cloth does: Tools/create_ship_sea.py.)
    auto* Canvas = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Ship/M_ShipSail.M_ShipSail"));
    auto* Black = LoadObject<UMaterialInterface>(nullptr, DarkMat);
    // (The flag: the user's mustachioed skull and crossed cutlasses, 2026-11-02: Tools/create_ship_flag.py.)
    auto* FlagMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Ship/M_ShipFlag.M_ShipFlag"));
    Missing += !Canvas + !Black + !FlagMat;
    TArray<FVector> V, N; TArray<FVector2D> UV; TArray<int32> T;
    auto Make = [&](const TCHAR* Name, UMaterialInterface* Mat) -> UProceduralMeshComponent*
    {
        auto* M = NewObject<UProceduralMeshComponent>(this, Name);
        M->SetupAttachment(RootComponent); M->SetWorldLocation(ShipOrigin()); M->RegisterComponent();
        M->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        M->SetMaterial(0, Mat); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(true);
        M->ComponentTags.Add(TEXT("Ship"));
        Triangles += T.Num() / 3;
        return M;
    };
    for (int32 I = 0; I < GSails.Num(); ++I)
    {
        const FSailDef& S = GSails[I];
        GridMesh([&S](float U, float W) { return SailPoint(S, U, W, 0); }, SailU, SailV, SailWidth(S), SailHeight(S), V, N, UV, &T);
        Sails.Add(Make(*FString::Printf(TEXT("ShipSail%d"), I), Canvas));
    }
    GridMesh([](float U, float W) { return FlagPoint(U, W, 0); }, FlagU, FlagV, FlagFly, FlagDrop, V, N, UV, &T);
    Flag = Make(TEXT("ShipFlag"), FlagMat ? FlagMat : Black);
}

void AShipSetting::UpdateSails()
{
    TArray<FVector> V, N; TArray<FVector2D> UV;
    const float T = Clock;
    GSailWind = StormWind; GFlutter = SailClock;
    GSailBodyR = 0;
    if (const AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FVector C = ShipLocal(Chuck->GetActorLocation());
        if (C.Z > MainTopZ - 400)   // (up in the rig, among the sails)
        {
            const FVector Up = Chuck->GetMesh()->GetUpVector();
            GSailBodyA = C - Up * 26; GSailBodyB = C + Up * 52; GSailBodyR = 20;
        }
    }
    for (int32 I = 0; I < Sails.Num() && I < GSails.Num(); ++I)
    {
        if (!Sails[I]) continue;
        const FSailDef& S = GSails[I];
        GridMesh([&S, T](float U, float W) { return SailPoint(S, U, W, T); }, SailU, SailV, SailWidth(S), SailHeight(S), V, N, UV, nullptr);
        Sails[I]->UpdateMeshSection(0, V, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>());
    }
    if (Flag)
    {
        GridMesh([T](float U, float W) { return FlagPoint(U, W, T); }, FlagU, FlagV, FlagFly, FlagDrop, V, N, UV, nullptr);
        Flag->UpdateMeshSection(0, V, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>());
    }
}

void AShipSetting::Rock()
{
    // The ship rides a long swell (user 2026-11-01: the sea and the horizon rock round a still hull, so nobody's feet,
    // ladder or grip ever moves): the sea, its horizon, the sky and the sun roll about 1.5 degrees on the lamps' 7.3 s
    // swell and pitch a little on a longer one. The sea's ring on the hull follows the planking's flare (M_ShipSea).
    // In the storm (2026-11-03) the swell builds: she rolls about 6.5 degrees and pitches 3, a shorter chop rides on it, and
    // the gale heels her to leeward (port) by up to 3.5 degrees.
    // Once it intensifies (2026-11-04) she rolls about 10 degrees each way and pitches 5, the chop rides harder on both and
    // the heel deepens; the swell comes quicker (AShipSetting::Tick).
    const float Roll = (FMath::Lerp(1.5f, 6.5f, StormSea) + 3.5f * StormIntense) * FMath::Sin(RollPhase) + (1.1f * StormSea + 1.f * StormIntense) * FMath::Sin(ChopPhase)
        + 3.5f * FMath::Min(StormWind, 1.f) + 1.f * StormIntense;
    const float Pitch = (FMath::Lerp(.6f, 3.f, StormSea) + 2.2f * StormIntense) * FMath::Sin(PitchPhase + .8f) + (.7f * StormSea + .8f * StormIntense) * FMath::Sin(ChopPhase * 1.37f + 1.f);
    Tilt = FVector2D(FMath::Lerp(Pitch, Pitch * .35f, KnockWeight), FMath::Lerp(Roll, KnockRoll, KnockWeight));
    const FQuat Q(FRotator(Tilt.X, 0, Tilt.Y));
    const FVector Nn = Q.RotateVector(FVector::UpVector);
    if (SeaMid) SeaMid->SetVectorParameterValue(TEXT("Tilt"), FLinearColor(-Nn.X / Nn.Z, -Nn.Y / Nn.Z, 0, 0));
    if (Sun) Sun->SetWorldRotation(Q * SunBase);
    if (Sky) Sky->SetWorldLocation(ShipWorld(FVector(0, 0, WaterZ)) - Nn * (Sky->BottomRadius * 100000.f));
}

float AShipSetting::GetSunIntensity() const { return (Sun ? Sun->Intensity : 0.f) + (FlashLight && FlashLight->IsVisible() ? FlashLight->Intensity : 0.f); }

void AShipSetting::ApplyStorm(float Dt)
{
    // The storm (2026-11-03, ShipStorm.h): the clouds close over and come down, the light dims and goes grey, the haze
    // thickens with the rain, the sea builds (M_ShipSea's Storm: a long storm swell, darker water, spume); a lightning
    // flash is the sun's light swung for a moment to the strike's side (so it casts the hull's shadows and only comes
    // below decks through the openings) and turned up hard.
    const AShipStorm* Storm = AShipStorm::Get(GetWorld());
    const float Dark = Storm ? Storm->GetDark() : 0.f, Rain = Storm ? Storm->GetRain() : 0.f, Flash = Storm ? Storm->GetFlash() : 0.f;
    if (SeaMid) SeaMid->SetScalarParameterValue(TEXT("Storm"), StormSea + .7f * StormIntense);   // (M_ShipSea's swell and foam carry on past 1)
    if (Dark <= 0.f && Flash <= 0.f && CloudSet == 0.f) return;   // calm, and already set so
    if (Sun)
    {
        // (The storm clouds' shadow dims it further; in a flash it comes up only slowly, lighting the clouds.)
        FlashSlow = FMath::FInterpTo(FlashSlow, Flash, Dt, Flash > FlashSlow ? 10.f : 3.f);
        Sun->SetIntensity(FMath::Lerp(5.f, 2.6f, Dark) + 1.6f * FlashSlow);
        Sun->SetLightColor(FMath::Lerp(FLinearColor(1.f, .95f, .86f), FLinearColor(.74f, .8f, .9f), Dark));
        Sun->SetVolumetricScatteringIntensity(FMath::Lerp(2.f, .5f, Dark));
    }
    if (FlashLight)
    {
        const bool bOn = Flash > .02f && Storm;
        if (FlashLight->IsVisible() != bOn) FlashLight->SetVisibility(bOn);
        if (bOn) { FlashLight->SetIntensity(7.f * Flash); FlashLight->SetWorldRotation((-Storm->GetFlashFrom()).Rotation()); }
    }
    if (Sky)
    {
        Sky->SetRayleighScatteringScale(.0331f * FMath::Lerp(1.f, .45f, Dark));
        Sky->SetMieScatteringScale(FMath::Lerp(.008f, .03f, Dark));
    }
    if (CloudMid && CloudLayer && FMath::Abs(Dark - CloudSet) > .003f)
    {
        CloudSet = Dark;
        CloudMid->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), FMath::Lerp(-.18f, .62f, Dark));
        CloudMid->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), FMath::Lerp(.01f, .05f, Dark));
        CloudMid->SetScalarParameterValue(TEXT("StormClouds"), Dark);
        CloudLayer->SetLayerBottomAltitude(FMath::Lerp(1.4f, .75f, Dark));
        CloudLayer->SetLayerHeight(FMath::Lerp(1.6f, 3.5f, Dark));
    }
    if (HazeFog)
    {
        HazeFog->SetFogDensity(FMath::Lerp(.0006f, .0014f, Dark) + .0009f * Rain);
        HazeFog->SetFogMaxOpacity(FMath::Lerp(.25f, .75f, Dark));
        HazeFog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.45f, .55f, .66f), FLinearColor(.2f, .23f, .26f), Dark) * (1.f + 1.5f * Flash));
        HazeFog->SetVolumetricFogExtinctionScale(FMath::Lerp(1.4f, 2.2f, Dark));
    }
    if (Grade)
    {
        auto& S = Grade->Settings;
        S.AutoExposureBias = FMath::Lerp(-.4f, -.55f, Dark);
        S.ColorSaturation = FVector4(FMath::Lerp(.95f, .62f, Dark), FMath::Lerp(.95f, .64f, Dark), FMath::Lerp(.92f, .68f, Dark), 1);
        S.BloomIntensity = .45f + .35f * Dark;
        S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .4f + .25f * Dark;
    }
    if (Dark <= 0.f && Flash <= 0.f) CloudSet = 0.f;
}

float AShipSetting::SeaPlaneZ(const FVector& World) const
{
    const FVector Nn = FQuat(FRotator(Tilt.X, 0, Tilt.Y)).RotateVector(FVector::UpVector);
    const FVector W0 = ShipWorld(FVector(0, 0, WaterZ));
    return static_cast<float>(W0.Z - (Nn.X * (World.X - W0.X) + Nn.Y * (World.Y - W0.Y)) / Nn.Z);
}


void AShipSetting::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    {
        const AShipStorm* Storm = AShipStorm::Get(GetWorld());
        StormSea = Storm ? Storm->GetSea() : 0.f; StormIntense = Storm ? Storm->GetIntense() : 0.f; StormWind = ShipStormWind();
        RollPhase += Dt * 2 * PI / (FMath::Lerp(7.3f, 6.2f, StormSea) - .8f * StormIntense);
        PitchPhase += Dt * 2 * PI / (FMath::Lerp(9.1f, 5.8f, StormSea) - .7f * StormIntense);
        ChopPhase += Dt * 2 * PI / (3.1f - .4f * StormIntense);
        // The helm (2026-11-04): the wheel turns as the captain steers.
        if (Wheel)
        {
            const AShipCrew* Crew = AShipCrew::Get(GetWorld());
            const ADockNPC* Captain = Crew ? Crew->Find(EDockHuman::PirateCaptain) : nullptr;
            const float Angle = Captain ? Captain->GetHelmAngle() : 0.f;
            if (FMath::Abs(Angle - WheelSet) > .01f) { WheelSet = Angle; Wheel->SetWorldRotation(HelmTurn(Angle)); }
        }
        SailClock += Dt * (1.f + 2.4f * StormWind);
    }
    Rock();
    ApplyStorm(Dt);
    UpdateSails();
    // Where he was last aboard (over the side, he returns there); the sky's fill fuller when the view is out on deck.
    if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
        if (InShip(Pawn->GetActorLocation()) && !OutsideHull(ShipLocal(Pawn->GetActorLocation()))) GLastSpace = ShipSpaceAt(Pawn->GetActorLocation());
    if (SkyFill)
        if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
        {
            const FVector C = ShipLocal(Cam->GetCameraLocation());
            const bool bOut = OutsideHull(C) || C.Z > (C.X < QuarterFore ? QuarterZ : DeckZ) - 5;
            // (In the storm: more, as the overcast lights the deck from all over (the sky it captures is far darker); a lightning
            // flash lights it at once.)
            const AShipStorm* Storm = AShipStorm::Get(GetWorld());
            const float Dark = Storm ? Storm->GetDark() : 0.f, Flash = Storm ? Storm->GetFlash() : 0.f;
            const float Want = (bOut ? FMath::Lerp(.75f, 1.15f, Dark) : FMath::Lerp(.22f, .26f, Dark)), Now = SkyFill->Intensity;
            if (Flash > .05f) SkyFill->SetIntensity(Want + Flash * (bOut ? .9f : .2f));
            else if (FMath::Abs(Want - Now) > .002f) SkyFill->SetIntensity(FMath::Lerp(Now, Want, 1 - FMath::Exp(-4.f * Dt)));
        }
    // The ship rides a long swell: the lamps and hammocks swing with it (about a 7 s roll), the lamps' flames flicker.
    const float Roll = (4.f + 7.f * StormSea) * FMath::Sin(RollPhase) + 2.f * StormSea * FMath::Sin(ChopPhase), Pitch = (1.6f + 4.f * StormSea) * FMath::Sin(PitchPhase + .8f);
    for (int32 I = 0; I < Swingers.Num(); ++I)
        if (Swingers[I]) Swingers[I]->SetRelativeRotation(FRotator(Pitch * (1 + .2f * FMath::Sin(SwingPhase[I])), 0, Roll * (1 + .15f * FMath::Cos(SwingPhase[I] * 1.3f))));
    for (int32 I = 0; I < Lamps.Num(); ++I)
        if (Lamps[I] && LampBase.IsValidIndex(I)) Lamps[I]->SetIntensity(LampBase[I] * (1.f + .06f * FMath::Sin(Clock * 11.f + I * 1.7f) + .04f * FMath::Sin(Clock * 23.f + I)));
    // The ambience by where he is: on deck (or up the rig), the sea rushing along the hull and the wind in the rigging (the
    // higher he climbs, the more wind), the wash under them; below, the wash and the water lapping at the planking (closest
    // in the hold), the sea along the hull faint through it. Levels follow him over a second or so; all fade in at first.
    {
        const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
        const int32 Space = Pawn ? ShipSpaceAt(Pawn->GetActorLocation()) : -1;
        const bool bDeck = Space == int32(EShipSpawn::Deck), bHold = Space == int32(EShipSpawn::Hold);
        const float Up = Pawn && bDeck ? FMath::Clamp(static_cast<float>(ShipLocal(Pawn->GetActorLocation()).Z - DeckZ - 150.f) / 900.f, 0.f, 1.f) : 0.f;
        const float Want[4] = {
            bDeck ? .2f : .5f,                          // the wash
            bDeck ? .6f - .2f * Up : .14f,              // the sea along the hull
            bDeck ? 0.f : bHold ? 1.f : .7f,            // the lapping
            bDeck ? .35f + .4f * Up : .04f };           // the rigging
        UAudioComponent* Loops[4] = { Wash, Water, Lap, Rigging };
        const float Open = FMath::Clamp(Clock / 4.f, 0.f, 1.f);
        for (int32 I = 0; I < 4; ++I)
        {
            // (The storm: the sea louder along the hull and against it below; its own sounds are AShipStorm's.)
            const float Storm = I == 0 ? 1.f + .8f * StormSea : I == 1 ? 1.f + .7f * StormSea : I == 2 ? 1.f + .3f * StormSea : 1.f + .6f * StormWind;
            AmbNow[I] = FMath::FInterpTo(AmbNow[I], Want[I] * Storm, Dt, 1.5f);
            if (Loops[I]) Loops[I]->SetVolumeMultiplier(FMath::Max(.001f, AmbNow[I] * Open));
        }
    }
    // Now and then a timber creaks somewhere close (quieter since 2026-11-03, the user: about two fifths of before).
    if (Clock > NextCreak)
    {
        // (Working hard in the storm: more often and louder.)
        NextCreak = Clock + FMath::FRandRange(3.5f, 8.f) * FMath::Lerp(1.f, .3f, StormSea);
        static const TCHAR* CreakNames[] = {TEXT("SFX_ShipCreak_00"), TEXT("SFX_ShipCreak_01"), TEXT("SFX_ShipCreak_02"), TEXT("SFX_ShipCreak_03")};
        const TCHAR* Name = CreakNames[FMath::RandRange(0, 3)];
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name)))
            if (auto* Chuck = UGameplayStatics::GetPlayerPawn(this, 0))
            {
                UGameplayStatics::PlaySoundAtLocation(this, S, Chuck->GetActorLocation() + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-350.f, 350.f), 120.f), FMath::FRandRange(.14f, .22f) * (1.f + 1.4f * StormSea), FMath::FRandRange(.9f, 1.1f) * (1.f - .12f * StormSea));
                ++Creaks;
            }
    }
}

int32 AShipSetting::Check() const
{
    UWorld* World = GetWorld();
    int32 Failures = 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCheck), false);
    if (const APawn* Chuck = UGameplayStatics::GetPlayerPawn(World, 0)) Q.AddIgnoredActor(Chuck);
    // Every spawn stands on its floor, with room for him.
    for (int32 I = 0; I < ShipSpawns; ++I)
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
    // The deck's rail: the gaps between its posts leave him no room to stand on the planking and slip through.
    int32 RailRoom = 0;
    for (float Side : {1.f, -1.f})
        for (float X : {-1300.f, -620.f, 30.f, 600.f, 1200.f})
            for (float Dx : {0.f, 27.f})
            {
                const float Xx = X + Dx, Top = PlankTopAt(Xx);
                const FVector At = ShipWorld(FVector(Xx, Side * (OuterAt(Xx, Top) - 5), Top + 36));
                RailRoom += !World->OverlapAnyTestByChannel(At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(15.f, 32.5f), Q);
            }
    Failures += RailRoom;
    // The main top: a floor with room, forward of the mast.
    {
        const FVector At = ShipWorld(FVector(MainMastX + 150, 0, MainTopZ + 36));
        FHitResult Hit;
        const bool bFloor = World->LineTraceSingleByChannel(Hit, At, At - FVector(0, 0, 60), ECC_Visibility, Q) && FMath::Abs(Hit.ImpactPoint.Z - (At.Z - 36)) < 3;
        const bool bRoom = !World->OverlapAnyTestByChannel(At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(15.f, 32.5f), Q);
        Failures += !(bFloor && bRoom);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_TOP floor=%d room=%d rail_room=%d"), bFloor, bRoom, RailRoom);
    }
    // Every rope coil rests on what is under it (2026-11-08: one below decks floated in the air).
    int32 Floating = 0;
    for (const TWeakObjectPtr<UStaticMeshComponent>& C : Coils)
    {
        if (!C.IsValid()) continue;
        const FBoxSphereBounds B = C->Bounds;
        const FVector Under(B.Origin.X, B.Origin.Y, B.Origin.Z - B.BoxExtent.Z);
        FHitResult Hit;
        const bool bOn = World->LineTraceSingleByChannel(Hit, Under + FVector(0, 0, 4), Under - FVector(0, 0, 80), ECC_Visibility, Q);
        const float Gap = bOn ? static_cast<float>(Under.Z - Hit.ImpactPoint.Z) : 999.f;
        const bool bResting = bOn && Gap > -2.f && Gap < 1.5f;
        Floating += !bResting;
        const FVector L = ShipLocal(Under);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_COIL at=(%.0f,%.0f,%.0f) gap=%.1f resting=%d"), L.X, L.Y, L.Z, Gap, bResting);
    }
    Failures += Floating;
    // The fore course stands clear of the fore ratlines (2026-11-08: it ran through the port net).
    {
        int32 Crossings = 0; float Clearance = 0;
        CourseNetClearance(Crossings, Clearance);
        const bool bClear = Crossings == 0 && Clearance > 15.f;
        Failures += !bClear;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_COURSE crossings=%d clearance_cm=%.1f pass=%d"), Crossings, Clearance, bClear);
    }
    // The masts above the tops are solid to the truck (2026-11-08): a trace across each at the topmast and topgallant.
    {
        int32 Solid = 0;
        for (const FMast* Mp : {&MainMast(), &ForeMast()})
            for (float Z : {Mp->Top + 400.f, Mp->Top + 890.f, Mp->Top + 1100.f, Mp->Top + 1340.f})
            {
                const float X = Z < Mp->Top + 850.f ? TopmastX(*Mp) : GallantX(*Mp);
                FHitResult Hit;
                Solid += World->LineTraceSingleByChannel(Hit, ShipWorld(FVector(X - 80, 0, Z)), ShipWorld(FVector(X + 80, 0, Z)), ECC_Visibility, Q)
                    && World->LineTraceSingleByChannel(Hit, ShipWorld(FVector(X, 80, Z)), ShipWorld(FVector(X, -80, Z)), ECC_Visibility, Q);
            }
        Failures += Solid != 8;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_MASTS solid=%d of=8"), Solid);
    }
    Failures += Missing > 0;
    Failures += Ladders != 4 || Shrouds != 8;
    Failures += Grips != 40;   // (the course and topsail yards of both masts, 2026-11-05)
    Failures += Sails.Num() != 8 || !Flag || !SeaMid;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SHIP_CHECK failures=%d missing=%d tris=%d jars=%d rats=%d ladders=%d shrouds=%d grips=%d sails=%d sea=%d"), Failures, Missing, Triangles, JarsPlaced, RatsPlaced, Ladders, Shrouds, Grips, Sails.Num(), SeaMid != nullptr);
    return Failures;
}
