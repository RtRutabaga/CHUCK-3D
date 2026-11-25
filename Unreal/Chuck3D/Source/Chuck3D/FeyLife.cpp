// The Feywild's mechanics and small living hazards, stage 5 part one (Claude, 2026-11-21). See FeyLife.h.
#include "FeyLife.h"
#include "FeyDelights.h"
#include "FeyTrio.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FeyMap.h"
#include "FeyMesh.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "TempleSkeleton.h"
#include "ChultTempleInterior.h"
#include "ChuckFlames.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

using namespace FeyMap;

namespace
{
    TArray<TWeakObjectPtr<AFeyLife>> GLife;
    constexpr float Stand = 34.65f;
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    FColor Col(float R, float G, float B, float A = 0.f) { return FLinearColor(R, G, B, A).ToFColor(false); }
    enum EMat { MPetal, MBlade, MPlant, MRoot, MGround, MHedge, MStone };
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyPetal.M_FeyPetal"), TEXT("/Game/Art/Feywild/M_FeyBlade.M_FeyBlade"), TEXT("/Game/Art/Feywild/M_FeyPlant.M_FeyPlant"), TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot"),
                               TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround"), TEXT("/Game/Art/Feywild/M_FeyHedge.M_FeyHedge"), TEXT("/Game/Art/Feywild/M_FeyStone.M_FeyStone")};
    constexpr float OrchidBeat = 2.4f, OrchidWind = .7f, SeedSpeed = 1000.f, SeedRadius = 3.f, MothRadius = 22.f;
    // (2026-11-23) Targeting, as the spined devils: how far it sees him, how wide of its lane it turns, how often it throws wide.
    constexpr float OrchidSees = 1.25f, OrchidArc = -.25f, OrchidMiss = .5f;
    enum ESfx { SFlowerOpen, SFlowerClose, SVines, SOrchidSwell, SOrchidSpit, SOrchidHit, SPads, SMoth, SFire, SCauldron, SBees, SfxCount };
    const TCHAR* SfxNames[] = {TEXT("SFX_FeyFlowerOpen_00"), TEXT("SFX_FeyFlowerClose_00"), TEXT("SFX_FeyVines_00"), TEXT("SFX_FeyOrchidSwell_00"), TEXT("SFX_FeyOrchidSpit_00"),
                               TEXT("SFX_FeyOrchidHit_00"), TEXT("SFX_FeyPadsRise_00"), TEXT("SFX_FeyMothLoop_00"), TEXT("SFX_FeyFireLoop_00"), TEXT("SFX_FeyCauldronLoop_00"), TEXT("SFX_FeyBeeLoop_00")};

    // ------------------------------------------------------------------ what stands where (local Fey frame)
    struct FPlace
    {
        TArray<FFeyFlower> Flowers; TArray<FFeyVines> Vines; TArray<FVector> Rocks; TArray<FVector> Cigs;   // rocks (X, Y, scale)
        TArray<FFeyPollen> Pollen; TArray<FFeyOrchid> Orchids; TArray<FFeyMoth> Moths; TArray<FVector> Clear;
        // (part two, 2026-11-22) mites (X, Y, yaw); the camp (X, Y; 0 none), its way out toward the ring, its redcaps (X, Y, yaw),
        // the hollow behind it (X, Y, radius); the beast's round, the hollow logs (X, Y, yaw) and their refuges; pads a flower
        // raises and the islet they reach (X, Y, radius).
        TArray<FVector> Mites, Redcaps, Round, Logs; TArray<FFeyRefuge> Refuges; TArray<FFeyPadRise> Rises;
        FVector2D Camp = FVector2D::ZeroVector, CampOut = FVector2D::ZeroVector; FVector Cache = FVector::ZeroVector, Islet = FVector::ZeroVector;
        FVector2D Patrol = FVector2D::ZeroVector; TArray<FVector2D> Statues;   // (stage 6: the maze's redcaps keep near Patrol; statues that turn)
        // (2026-11-24, the user: more redcaps in the Deepwood, more displacer beasts, more mites) Redcaps that roam the wood in
        // pairs, each kept near its own spot (X, Y, yaw); more beasts, each with its own round (the refuges are shared).
        TArray<FVector> Roamers; TArray<TArray<FVector>> MoreRounds;
    };
    FVector2D Perp(const FVector2D& D) { return FVector2D(-D.Y, D.X); }
    const FPlace& Placed(int32 D)
    {
        static const TArray<FPlace> S = []()
        {
            TArray<FPlace> Out; Out.SetNum(Districts);
            {   // The riverbank: the first flower, safe and in sight of the road. Its vines shut the only way into a ring of boulders.
                FPlace& P = Out[Riverbank]; const FVector2D N(-14500.f, -3700.f);
                for (int32 K = 0; K < 10; ++K) { const float A = FMath::DegreesToRadians(90.f + 48.f + K * 264.f / 9.f); P.Rocks.Add(FVector(N.X + 460.f * FMath::Cos(A), N.Y + 460.f * FMath::Sin(A), 2.2f + .25f * FMath::Sin(K * 2.1f))); }
                FFeyVines V; V.C = FVector(N.X, N.Y + 440.f, LandZ(Riverbank, N.X, N.Y + 440.f)); V.Yaw = 0.f; V.Width = 330.f; V.Height = 210.f; P.Vines.Add(V);
                FFeyFlower F; const FVector2D Fp = N + FVector2D(420.f, 830.f); F.Pos = FVector(Fp.X, Fp.Y, LandZ(Riverbank, Fp.X, Fp.Y)); F.Yaw = 110.f; F.Vines = 0; P.Flowers.Add(F);
                for (const FVector2D& C : {FVector2D(-12.f, 0.f), FVector2D(10.f, 9.f), FVector2D(2.f, -13.f)}) P.Cigs.Add(FVector(N.X + C.X, N.Y + C.Y, LandZ(Riverbank, N.X + C.X, N.Y + C.Y)));
                P.Clear.Add(FVector(N.X, N.Y, 620.f)); P.Clear.Add(FVector(Fp.X, Fp.Y, 160.f));
            }
            {   // The orchard: a strip over its road a jump clears, then a broad bed; two more among its rows.
                FPlace& P = Out[Orchard]; const TArray<FVector2D>& R = RoadOf(Orchard, 0);
                const FVector2D A = R[1], B = R[2], C = R[3];
                auto Bed = [&](const FVector2D& At, const FVector2D& Dir, float Along, float Across) { FFeyPollen Pb; Pb.C = At; Pb.R = FVector2D(Along, Across); Pb.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); P.Pollen.Add(Pb); P.Clear.Add(FVector(At.X, At.Y, FMath::Max(Along, Across) + 60.f)); };
                Bed(FMath::Lerp(A, B, .5f), (B - A).GetSafeNormal(), 60.f, 420.f);
                Bed(FMath::Lerp(B, C, .45f), (C - B).GetSafeNormal(), 430.f, 620.f);
                Bed(FVector2D(-34000.f, -5500.f), FVector2D(1.f, .35f).GetSafeNormal(), 520.f, 380.f);
                Bed(FVector2D(-37000.f, 6500.f), FVector2D(1.f, -.55f).GetSafeNormal(), 600.f, 450.f);
                // The Needle Garden: orchids south of its road, spitting north across it, a beat apart.
                const TArray<FVector2D>& Rn = RoadOf(Orchard, 1); const FVector2D Na = Rn[1], Nb = Rn[2], Dn = (Nb - Na).GetSafeNormal(), Pn = Perp(Dn);
                for (int32 K = 0; K < 3; ++K)
                {
                    const FVector2D At = FMath::Lerp(Na, Nb, .25f + .25f * K) - Pn * 650.f; FFeyOrchid O;
                    O.Pos = FVector(At.X, At.Y, LandZ(Orchard, At.X, At.Y)); O.Dir = FVector(Pn.X, Pn.Y, 0.f); O.Phase = K * OrchidBeat / 3.f; P.Orchids.Add(O); for (int32 Q = 0; Q < 6; ++Q) { const FVector2D Lp = At + Pn * (Q * 260.f); P.Clear.Add(FVector(Lp.X, Lp.Y, 220.f)); }
                }
            }
            {   // (part two) Thorn mites after the pollen's lesson, by the way out to the root tunnel.
                FPlace& P = Out[Orchard];
                for (const FVector& M : {FVector(-44200.f, 3300.f, 200.f), FVector(-44600.f, 3700.f, 160.f), FVector(-44000.f, 3900.f, 230.f)}) P.Mites.Add(M);
                // (2026-11-24) A second nest off the orchard's first road, past the broad bed, a few metres to its side.
                const TArray<FVector2D>& R = RoadOf(Orchard, 0); const int32 K = FMath::Clamp(R.Num() - 3, 0, R.Num() - 2);
                const FVector2D Tn = (R[K + 1] - R[K]).GetSafeNormal(), Q = FMath::Lerp(R[K], R[K + 1], .5f) + Perp(Tn) * 420.f;
                for (const FVector& M : {FVector(0.f, 0.f, 20.f), FVector(-380.f, 260.f, 140.f), FVector(330.f, 300.f, 260.f)}) P.Mites.Add(FVector(Q.X + M.X, Q.Y + M.Y, M.Z));
            }
            {   // (part two) The Lily Mere: on the land inside the deep water, a flower whose scratch raises two pads to an islet
                // (FeyMap.cpp's moat: centre (-73500, 15000), deep from 1500 to 2650 out). Cigarettes on the islet.
                FPlace& P = Out[LilyMere]; const FVector2D Mc(-73500.f, 15000.f); const float A = FMath::DegreesToRadians(140.f), Af = FMath::DegreesToRadians(150.f);
                const FVector2D Dir(FMath::Cos(A), FMath::Sin(A)), Fp = Mc + FVector2D(FMath::Cos(Af), FMath::Sin(Af)) * 1280.f, Ip = Mc + Dir * 1900.f;
                FFeyPadRise Rise; for (const float R : {1572.f, 1690.f}) { const FVector2D Q = Mc + Dir * R; Rise.Pads.Add(FVector(Q.X, Q.Y, 48.f)); } P.Rises.Add(Rise);
                P.Islet = FVector(Ip.X, Ip.Y, 165.f);
                FFeyFlower F; F.Pos = FVector(Fp.X, Fp.Y, LandZ(LilyMere, Fp.X, Fp.Y)); F.Yaw = 300.f; F.Pads = 0; P.Flowers.Add(F); P.Clear.Add(FVector(Fp.X, Fp.Y, 180.f));
                for (const FVector2D& C : {FVector2D(-20.f, 10.f), FVector2D(15.f, 18.f), FVector2D(5.f, -22.f)}) P.Cigs.Add(FVector(Ip.X + C.X, Ip.Y + C.Y, 24.f));
            }
            {   // (part two) The Deepwood: mites by two of its paths; Redcap Hollow's camp.
                FPlace& P = Out[Deepwood];
                for (const FVector& M : {FVector(-67100.f, -24300.f, 0.f), FVector(-66800.f, -24600.f, 90.f), FVector(-67400.f, -24650.f, 200.f),
                                         FVector(-82600.f, -27400.f, 30.f), FVector(-82300.f, -27800.f, 250.f), FVector(-82900.f, -27900.f, 120.f),
                                         // (2026-11-24) two more nests, deeper in
                                         FVector(-77000.f, -35800.f, 40.f), FVector(-77350.f, -35500.f, 170.f), FVector(-76700.f, -35450.f, 280.f),
                                         FVector(-87600.f, -24800.f, 300.f), FVector(-87250.f, -25150.f, 60.f), FVector(-87900.f, -25200.f, 150.f)}) P.Mites.Add(M);
                // (2026-11-24) Two pairs of redcaps out in the wood, each near its own ground.
                for (const FVector& Rr : {FVector(-71500.f, -34000.f, 30.f), FVector(-71250.f, -33750.f, 210.f), FVector(-85500.f, -25800.f, 120.f), FVector(-85800.f, -26050.f, 300.f)}) P.Roamers.Add(Rr);
                const TArray<FVector2D>& Sp = SpurOf(Deepwood); P.Camp = Sp.Last(); P.CampOut = (Sp[Sp.Num() - 2] - P.Camp).GetSafeNormal();
                const FVector2D Dp = P.CampOut, Pp = Perp(Dp);
                for (int32 K = -1; K <= 1; ++K) { const FVector2D R = P.Camp - Dp * 240.f + Pp * (K * 215.f); P.Redcaps.Add(FVector(R.X, R.Y, FMath::RadiansToDegrees(FMath::Atan2(P.Camp.Y - R.Y, P.Camp.X - R.X)))); }
                const FVector2D Ch = P.Camp + Pp * 1150.f - Dp * 200.f; P.Cache = FVector(Ch.X, Ch.Y, 300.f);
                for (const FVector2D& C : {FVector2D(-15.f, 8.f), FVector2D(12.f, 14.f), FVector2D(0.f, -16.f)}) P.Cigs.Add(FVector(Ch.X + C.X, Ch.Y + C.Y, LandZ(Deepwood, Ch.X + C.X, Ch.Y + C.Y)));
                P.Clear.Add(FVector(Ch.X, Ch.Y, 560.f)); P.Clear.Add(FVector(P.Camp.X, P.Camp.Y, 1100.f));
            }
            {   // (part two) Highmeadow: the displacer beast's round across the way to the maze and the overlook's spur; hollow logs
                // beside the road and the spur; pollen on the road to the stair.
                FPlace& P = Out[Highmeadow];
                for (const FVector& L : {FVector(-104639.f, 3865.f, 161.6f), FVector(-109800.f, 4670.f, 180.f), FVector(-104157.f, 7303.f, 129.8f), FVector(-108782.f, 11034.f, 149.3f),
                                         // (2026-11-24) the two more beasts' meadows' logs
                                         FVector(-98200.f, -3000.f, 35.f), FVector(-100300.f, -5200.f, 120.f), FVector(-89700.f, 10200.f, 70.f), FVector(-91600.f, 12100.f, 160.f)})
                { P.Logs.Add(L); FFeyRefuge Rf; Rf.C = FVector2D(L.X, L.Y); Rf.R = 270.f; P.Refuges.Add(Rf); P.Clear.Add(FVector(L.X, L.Y, 420.f)); }
                // (2026-11-24) Two more beasts in the open meadow: south of the ring, and east of the rapids under the slope.
                for (const FVector2D& C : {FVector2D(-99300.f, -4100.f), FVector2D(-90600.f, 11100.f)})
                {
                    TArray<FVector>& Rd = P.MoreRounds.AddDefaulted_GetRef();
                    for (int32 K = 0; K < 5; ++K) { const float A = FMath::DegreesToRadians(K * 72.f + 15.f), Rr = K % 2 ? 2100.f : 2600.f; const FVector2D Q = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr; Rd.Add(FVector(Q.X, Q.Y, LandZ(Highmeadow, Q.X, Q.Y))); }
                }
                for (const FVector2D& Q : {FVector2D(-105500.f, 8600.f), FVector2D(-107500.f, 12600.f), FVector2D(-110800.f, 10600.f), FVector2D(-110200.f, 7600.f), FVector2D(-107200.f, 6900.f)})
                    P.Round.Add(FVector(Q.X, Q.Y, LandZ(Highmeadow, Q.X, Q.Y)));
                FFeyPollen Pb; Pb.C = FVector2D(-107800.f, 5000.f); Pb.R = FVector2D(260.f, 380.f); Pb.Yaw = 180.f; P.Pollen.Add(Pb); P.Clear.Add(FVector(Pb.C.X, Pb.C.Y, 440.f));
            }
            {   // (stage 6, 2026-11-22) The Great Hedge Maze: the heart behind vines a hit plant opens; a hidden hit plant at each
                // shortcut, revealed the longer he is lost; mites in two dead ends; two redcaps near the middle of the way; orchids
                // down straight runs; moths by the heart; statues in dead ends near the way.
                FPlace& P = Out[Maze]; FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
                const float Ha = MazeHeartAngle(); const FVector2D In(FMath::Cos(Ha), FMath::Sin(Ha)), Across(-In.Y, In.X);
                const FVector2D Gate = Hub + In * R0, Fp = Hub + In * (R0 + 170.f) + Across * 230.f;
                FFeyVines V; V.C = FVector(Gate.X, Gate.Y, LandZ(Maze, Gate.X, Gate.Y)); V.Yaw = FMath::RadiansToDegrees(Ha) + 90.f; V.Width = 300.f; V.Height = 330.f; P.Vines.Add(V);
                FFeyFlower F; F.Pos = FVector(Fp.X, Fp.Y, LandZ(Maze, Fp.X, Fp.Y)); F.Yaw = 40.f; F.Vines = 0; P.Flowers.Add(F);
                for (int32 K = 0; K < MazeShortcuts().Num(); ++K)
                { const FVector2D Q = MazeShortcuts()[K].Plant; FFeyFlower Fs; Fs.Pos = FVector(Q.X, Q.Y, LandZ(Maze, Q.X, Q.Y)); Fs.Yaw = K * 61.f; Fs.Sc = K; Fs.bHidden = true; P.Flowers.Add(Fs); }
                const TArray<FVector2D>& Rt = MazeRoute();
                // (2026-11-23, second round) The mites, the turning statues and cigarettes on the way through only (MazeMarks).
                for (const FMazeMark& Mk : MazeMarks())
                {
                    const FVector2D Tn(-Mk.In.Y, Mk.In.X);
                    if (Mk.Kind == 1) P.Statues.Add(Mk.P);
                    else if (Mk.Kind == 2) for (int32 M = 0; M < 2; ++M) { const FVector2D Q = Mk.P + Mk.In * 40.f + Tn * (M ? 60.f : -60.f); P.Mites.Add(FVector(Q.X, Q.Y, M * 140.f)); }
                    else if (Mk.Kind == 3) for (const FVector2D& C : {FVector2D(-12.f, 0.f), FVector2D(10.f, 9.f), FVector2D(2.f, -13.f)}) { const FVector2D Q = Mk.P + Mk.In * (14.f + C.Y) + Tn * C.X; P.Cigs.Add(FVector(Q.X, Q.Y, LandZ(Maze, Q.X, Q.Y))); }
                }
                P.Patrol = Rt[Rt.Num() / 2];
                for (int32 K = 0; K < 2; ++K) { const FVector2D Q = Rt[Rt.Num() / 2 + K * 2]; P.Redcaps.Add(FVector(Q.X, Q.Y, K * 180.f)); }
                for (int32 K = 0; K < MazeLanes().Num(); ++K)
                {
                    const FVector& L = MazeLanes()[K]; FFeyOrchid O; const FVector2D D2(FMath::Cos(L.Z), FMath::Sin(L.Z));
                    const FVector2D At = FVector2D(L.X, L.Y) - D2 * 60.f + FVector2D(-D2.Y, D2.X) * 70.f;
                    O.Pos = FVector(At.X, At.Y, LandZ(Maze, At.X, At.Y)); O.Dir = FVector(D2.X, D2.Y, 0.f); O.Range = 900.f; O.Phase = K * .8f; P.Orchids.Add(O);
                }
                for (int32 K = 0; K < 2; ++K)
                {
                    const float A = Ha + (K ? -.35f : .35f); const FVector2D Q = Hub + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R0 + Pitch * 1.5f); FFeyMoth M;
                    M.Home = FVector(Q.X, Q.Y, LandZ(Maze, Q.X, Q.Y) + 72.f); M.Axis = FVector(-FMath::Sin(A), FMath::Cos(A), 0.f); M.Range = 260.f; M.Bob = 24.f; M.Rate = 1.5f + .2f * K; M.Phase = 2.1f * K; P.Moths.Add(M);
                }
            }
            {   // The Moonmoth Fen: moths across the way out to it.
                FPlace& P = Out[LilyMere]; const TArray<FVector2D>& Sp = SpurOf(LilyMere); const FVector2D S0 = Sp[0], S1 = Sp.Last(), Ds = (S1 - S0).GetSafeNormal();
                for (int32 K = 0; K < 3; ++K)
                {
                    const FVector2D At = FMath::Lerp(S0, S1, .35f + .25f * K); FFeyMoth M;
                    M.Home = FVector(At.X, At.Y, LandZ(LilyMere, At.X, At.Y) + 72.f); M.Bob = 24.f;   // (at his shoulder: low enough to meet) M.Axis = FVector(Perp(Ds).X, Perp(Ds).Y, 0.f); M.Range = 300.f;
                    M.Rate = 1.45f + .2f * K; M.Phase = 1.7f * K; P.Moths.Add(M);
                }
            }
            return Out;
        }();
        return S[FMath::Clamp(D, 0, Districts - 1)];
    }

    // ------------------------------------------------------------------ shapes
    // A reactive flower's head at Open (0 a bud, 1 a star with curled tips) and Glow (its pulse), on its stem's top Top.
    void FlowerHead(FMapSurf& Petals, FMapSurf& Stamens, float Open, float Glow)
    {
        const FVector Top(0, 0, 52.f);
        for (int32 I = 0; I < 5; ++I)
        {
            const float A = 2 * PI * I / 5 + .3f, Lean = FMath::Sin(I * 2.3f) * .06f;
            const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            constexpr int32 Nu = 10, Ns = 6; constexpr float L = 30.f;
            TArray<FVector> Spine; FVector P = Top + Rd * FMath::Lerp(4.f, 7.f, Open);
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu;
                const float Closed = FMath::Lerp(-4.f, -38.f, Uf * Uf) + 8.f, Opened = FMath::Lerp(40.f, 92.f, FMath::Pow(Uf, 1.2f)) + 45.f * FMath::Pow(Uf, 4.f);
                const float Th = FMath::DegreesToRadians(FMath::Lerp(Closed, Opened, Open) + Lean * 57.f);
                Spine.Add(P); P += (Rd * FMath::Sin(Th) + FVector(0, 0, FMath::Cos(Th))) * (L / Nu);
            }
            const int32 Base = Petals.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu, W = 13.f * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Pow(Uf, .8f)), 0.f), .7f) + .3f;
                const FVector Along = (Spine[FMath::Min(U + 1, Nu)] - Spine[FMath::Max(U - 1, 0)]).GetSafeNormal(), Out = FVector::CrossProduct(Sd, Along).GetSafeNormal();
                for (int32 J = 0; J <= Ns; ++J)
                {
                    const float S = FMath::Lerp(-1.f, 1.f, float(J) / Ns), Cup = FMath::Lerp(4.f, 1.4f, Open) * S * S;
                    const FVector Q = Spine[U] + Sd * (S * W) + Out * Cup;
                    const float Tip = Smooth(.25f, .95f, Uf);
                    Petals.Vert(Q, -Out, FVector2D(Uf, (S + 1.f) * .5f), Col(FMath::Lerp(.9f, .84f, Tip), FMath::Lerp(.9f, .46f, Tip), FMath::Lerp(.48f, .86f, Tip), FMath::Lerp(.04f, .14f, Tip) + Glow * .35f));
                }
            }
            for (int32 U = 0; U < Nu; ++U) for (int32 J = 0; J < Ns; ++J) { const int32 Q = Base + U * (Ns + 1) + J; Petals.Quad(Q, Q + Ns + 1, Q + Ns + 2, Q + 1); }
        }
        // Stamens, curling up out of the heart (they show as it opens).
        for (int32 I = 0; I < 6; ++I)
        {
            const float A = 2 * PI * I / 6; const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f);
            const float H = FMath::Lerp(6.f, 20.f, Open);
            const FVector Pts[] = {Top, Top + FVector(0, 0, H * .5f) + Rd * 2.f, Top + FVector(0, 0, H) + Rd * FMath::Lerp(2.f, 7.f, Open), Top + FVector(0, 0, H - 2.f) + Rd * FMath::Lerp(3.f, 10.f, Open)};
            Stamens.Pipe(Pts, .5f, .35f, 5, 3, Col(.95f, .9f, .45f, .1f + Glow * .4f), I * 1.3f);
        }
    }
    void FlowerBody(FMapSurf& Leaves, FMapSurf& Stem, float Seed)
    {
        const FVector Pts[] = {FVector(0, 0, -6.f), FVector(1.f, 0, 20.f), FVector(-.5f, .6f, 40.f), FVector(0, 0, 53.f)};
        Stem.Pipe(Pts, 3.f, 2.2f, 8, 4, Col(.55f, .7f, .32f), Seed);
        for (int32 I = 0; I < 6; ++I)   // broad glossy leaves, low and drooping
        {
            const float A = 2 * PI * I / 6 + .5f + .3f * FMath::Sin(I * 3.7f + Seed), Len = 58.f + 12.f * FMath::Sin(I * 1.9f + Seed), Wm = 19.f;
            const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            constexpr int32 Nu = 10, Ns = 4; const int32 Base = Leaves.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu, W = Wm * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Pow(Uf, .7f)), 0.f), .8f) + .2f;
                const FVector Sp = Rd * (Uf * Len) + FVector(0, 0, 4.f + 22.f * FMath::Sin(PI * Uf * .8f) - 20.f * Uf * Uf);
                for (int32 J = 0; J <= Ns; ++J)
                {
                    const float S = FMath::Lerp(-1.f, 1.f, float(J) / Ns);
                    Leaves.Vert(Sp + Sd * (S * W) + FVector(0, 0, -3.f * S * S), FVector::UpVector, FVector2D(Uf, (S + 1.f) * .5f), Col(.42f, .58f, .24f));
                }
            }
            for (int32 U = 0; U < Nu; ++U) for (int32 J = 0; J < Ns; ++J) { const int32 Q = Base + U * (Ns + 1) + J; Leaves.Quad(Q, Q + 1, Q + Ns + 2, Q + Ns + 1); }
        }
    }
    // A curtain of vines across a gap (local to its centre, X across, Z up), with leaves along them.
    void VineCurtain(FMapSurf& Wood, FMapSurf& Leaf, float Width, float Height)
    {
        FRandomStream R(777);
        for (int32 K = 0; K < 13; ++K)   // across, waving
        {
            const float Z = 12.f + K * (Height - 24.f) / 12.f + R.FRandRange(-6.f, 6.f), Ph = R.FRandRange(0.f, 6.f);
            TArray<FVector> Pts; for (int32 I = 0; I <= 6; ++I) { const float X = FMath::Lerp(-Width * .5f - 25.f, Width * .5f + 25.f, I / 6.f); Pts.Add(FVector(X, R.FRandRange(-9.f, 9.f), Z + 11.f * FMath::Sin(Ph + I * 1.3f))); }
            Wood.Pipe(Pts, R.FRandRange(3.f, 5.f), R.FRandRange(2.5f, 4.f), 7, 5, Col(.4f, 1.f, 0.f), K * 1.7f);
        }
        for (int32 K = 0; K < 7; ++K)   // up from the ground, twisting
        {
            const float X0 = FMath::Lerp(-Width * .5f, Width * .5f, (K + .5f) / 7.f), Ph = R.FRandRange(0.f, 6.f);
            TArray<FVector> Pts; for (int32 I = 0; I <= 6; ++I) { const float Z = FMath::Lerp(-20.f, Height + 10.f, I / 6.f); Pts.Add(FVector(X0 + 18.f * FMath::Sin(Ph + I * 1.1f), 10.f * FMath::Cos(Ph + I), Z)); }
            Wood.Pipe(Pts, 6.f, 2.5f, 8, 5, Col(.4f, 1.f, 0.f), 40.f + K);
        }
        for (int32 K = 0; K < 90; ++K)   // leaves
        {
            const FVector At(R.FRandRange(-Width * .55f, Width * .55f), R.FRandRange(-12.f, 12.f), R.FRandRange(10.f, Height));
            const float A = R.FRandRange(0.f, 2 * PI), Len = R.FRandRange(14.f, 24.f);
            const FVector Dir = FVector(FMath::Cos(A) * .6f, FMath::Sin(A), R.FRandRange(-.5f, .3f)).GetSafeNormal(), Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
            const int32 Base = Leaf.V.Num();
            for (int32 U = 0; U <= 3; ++U) for (int32 J = 0; J <= 2; ++J)
            {
                const float Uf = U / 3.f, S = J - 1.f, W = 6.f * FMath::Sin(PI * FMath::Max(Uf, .05f));
                Leaf.Vert(At + Dir * (Uf * Len) + Side * (S * W), FVector::CrossProduct(Side, Dir).GetSafeNormal(), FVector2D(Uf, J * .5f), Col(.36f, .52f, .2f));
            }
            for (int32 U = 0; U < 3; ++U) for (int32 J = 0; J < 2; ++J) { const int32 Q = Base + U * 3 + J; Leaf.Quad(Q, Q + 1, Q + 4, Q + 3); }
        }
    }
    // A spitting orchid's bulb (local: its mouth toward +X), and its stem and leaves.
    void OrchidBulb(FMapSurf& S)
    {
        const FVector2D Prof[] = {{0.f, 11.f}, {6.f, 9.f}, {9.f, 4.f}, {9.5f, -2.f}, {7.f, -8.f}, {3.f, -12.f}, {0.f, -13.f}};
        S.Lathe(FVector::ZeroVector, Prof, 14, Col(.78f, .26f, .58f, .05f));
        for (int32 I = 0; I < 5; ++I)   // the lips round its mouth
        {
            const float A = 2 * PI * I / 5; const FVector Rd(0.f, FMath::Cos(A), FMath::Sin(A));
            const FVector Pts[] = {FVector(0, 0, 10.f) + Rd * 3.f, FVector(3.f, 0, 13.f) + Rd * 6.f, FVector(5.f, 0, 15.f) + Rd * 10.f};
            S.Pipe(Pts, 2.2f, .8f, 6, 3, Col(.95f, .85f, .35f, .2f), I * 2.f);
        }
    }
    void OrchidBody(FMapSurf& Leaves, FMapSurf& Stem, const FVector& Dir)
    {
        const FVector Pts[] = {FVector(0, 0, -5.f), FVector(0, 0, 20.f), Dir * 6.f + FVector(0, 0, 40.f), Dir * 4.f + FVector(0, 0, 52.f)};
        Stem.Pipe(Pts, 3.2f, 2.4f, 8, 4, Col(.38f, .5f, .22f), 3.f);
        for (int32 I = 0; I < 4; ++I)
        {
            const float A = 2 * PI * I / 4 + .7f; const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            const int32 Base = Leaves.V.Num();
            for (int32 U = 0; U <= 6; ++U) for (int32 J = 0; J <= 2; ++J)
            {
                const float Uf = U / 6.f, S = J - 1.f, W = 9.f * FMath::Sin(PI * FMath::Max(Uf, .06f));
                Leaves.Vert(Rd * (Uf * 40.f) + FVector(0, 0, 3.f + 12.f * FMath::Sin(PI * Uf * .7f) - 8.f * Uf * Uf) + Sd * (S * W), FVector::UpVector, FVector2D(Uf, J * .5f), Col(.3f, .5f, .2f));
            }
            for (int32 U = 0; U < 6; ++U) for (int32 J = 0; J < 2; ++J) { const int32 Q = Base + U * 3 + J; Leaves.Quad(Q, Q + 1, Q + 4, Q + 3); }
        }
    }
    // A lantern moth's wing (local: +X along its body, +Y out from it), lit from within: amber at its root, teal at its edge, an
    // eye on the forewing.
    void MothWing(FMapSurf& S, float Side)
    {
        auto Panel = [&](float Span, float Root, float Chord, float Sweep, bool bEye)
        {
            constexpr int32 Nu = 8, Nv = 5; const int32 Base = S.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
                for (int32 V = 0; V <= Nv; ++V)
                {
                    const float Uf = float(U) / Nu, Vf = float(V) / Nv, Ch = FMath::Lerp(Root, Chord, Uf) * FMath::Sqrt(FMath::Max(1.f - FMath::Pow(Uf, 6.f), .02f));
                    const FVector P(Sweep * Uf + (Vf - .5f) * Ch, Side * Span * Uf, 2.f * FMath::Sin(PI * Vf) * Uf);
                    const float Edge = FMath::Max(Smooth(.7f, 1.f, Uf), Smooth(.3f, .0f, FMath::Min(Vf, 1.f - Vf)) * Uf);
                    float R = FMath::Lerp(1.f, .45f, Edge), G = FMath::Lerp(.72f, .88f, Edge), B = FMath::Lerp(.38f, 1.f, Edge), A = FMath::Lerp(.32f, .55f, Edge);
                    if (bEye) { const float E = FVector2D::Distance(FVector2D(Uf, Vf), FVector2D(.62f, .5f)); const float Ring = Smooth(.2f, .14f, E) * Smooth(.06f, .1f, E); R = FMath::Lerp(R, .25f, Ring); G = FMath::Lerp(G, .18f, Ring); B = FMath::Lerp(B, .3f, Ring); A = FMath::Lerp(A, .05f, Ring); if (E < .06f) { A = .8f; R = 1.f; G = 1.f; B = .85f; } }
                    S.Vert(P, FVector::UpVector, FVector2D(Uf, Vf), Col(R, G, B, A));
                }
            for (int32 U = 0; U < Nu; ++U) for (int32 V = 0; V < Nv; ++V) { const int32 Q = Base + U * (Nv + 1) + V; S.Quad(Q, Q + 1, Q + Nv + 2, Q + Nv + 1); }
        };
        Panel(40.f, 12.f, 21.f, 8.f, true);     // forewing (spread, wider than he is tall)
        Panel(27.f, 13.f, 18.f, -12.f, false);  // hindwing
    }
    void MothBody(FMapSurf& S)
    {
        const FVector Pts[] = {FVector(-9.f, 0, -1.f), FVector(-3.f, 0, 0.f), FVector(3.f, 0, .5f), FVector(7.f, 0, .5f)};
        S.Pipe(Pts, 2.2f, 3.f, 8, 3, Col(.55f, .42f, .3f, .03f), 1.f);
        for (const float Sd : {-1.f, 1.f}) { const FVector A[] = {FVector(7.f, Sd * 1.f, 1.f), FVector(11.f, Sd * 4.f, 5.f), FVector(13.f, Sd * 7.f, 7.f)}; S.Pipe(A, .4f, .3f, 4, 3, Col(.9f, .8f, .5f, .3f), Sd); }
    }
    // A small ellipsoid (6 round, 5 rings) on axes F (forward), Rt, Up with radii Rx, Ry, Rz.
    void Blob(FMapSurf& S, const FVector& C, const FVector& F, const FVector& Rt, const FVector& Up, float Rx, float Ry, float Rz, const FColor& Cl)
    {
        const int32 B = S.V.Num();
        for (int32 I = 0; I <= 4; ++I)
        {
            const float Ph = PI * I / 4.f, Cx = FMath::Cos(Ph), Sx = FMath::Sin(Ph);
            for (int32 J = 0; J < 6; ++J)
            {
                const float Th = 2 * PI * J / 6.f; const FVector U(Cx, Sx * FMath::Cos(Th), Sx * FMath::Sin(Th));
                S.Vert(C + F * (U.X * Rx) + Rt * (U.Y * Ry) + Up * (U.Z * Rz), (F * (U.X / Rx) + Rt * (U.Y / Ry) + Up * (U.Z / Rz)).GetSafeNormal(), FVector2D(J / 6.f, I / 4.f), Cl);
            }
        }
        for (int32 I = 0; I < 4; ++I) for (int32 J = 0; J < 6; ++J)
        { const int32 A = B + I * 6 + J, A1 = B + I * 6 + (J + 1) % 6; S.TriFacing(A, A1, A + 6, S.N[A]); S.TriFacing(A1, A1 + 6, A + 6, S.N[A1]); }
    }
    // A bumblebee (about 6 cm, fey-sized): dark head, furred gold thorax, banded abdomen, two pale wings beating.
    void Bee(FMapSurf& S, const FVector& C, const FVector& Fwd, float Beat)
    {
        const FVector F = Fwd.GetSafeNormal(), Rt = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal(), Up = FVector::CrossProduct(F, Rt);
        const FColor Gold = Col(.95f, .66f, .1f, .03f), Black = Col(.03f, .025f, .02f), Wing = Col(.75f, .8f, .85f, .06f);
        Blob(S, C + F * 2.6f, F, Rt, Up, 1.1f, 1.1f, 1.f, Black);
        Blob(S, C + F * .9f, F, Rt, Up, 1.4f, 1.55f, 1.45f, Gold);
        Blob(S, C - F * 1.1f - Up * .2f, F, Rt, Up, 1.f, 1.7f, 1.6f, Black);
        Blob(S, C - F * 2.4f - Up * .4f, F, Rt, Up, 1.f, 1.6f, 1.5f, Gold);
        Blob(S, C - F * 3.5f - Up * .6f, F, Rt, Up, .9f, 1.1f, 1.f, Black);
        for (const float Sd : {1.f, -1.f})
        {
            const FVector Root = C + F * .6f + Up * 1.3f + Rt * (Sd * .8f), Out = (Rt * Sd * FMath::Cos(Beat) + Up * FMath::Sin(Beat)).GetSafeNormal();
            const FVector Nn = FVector::CrossProduct(F, Out).GetSafeNormal();
            const FVector Q[4] = {Root, Root + Out * 3.4f + F * .5f, Root + Out * 3.9f - F * 1.4f, Root + Out * 1.2f - F * 1.8f};
            for (const float Fc : {1.f, -1.f})
            { const int32 I0 = S.Vert(Q[0], Nn * Fc, FVector2D(0, 0), Wing), I1 = S.Vert(Q[1], Nn * Fc, FVector2D(1, 0), Wing), I2 = S.Vert(Q[2], Nn * Fc, FVector2D(1, 1), Wing), I3 = S.Vert(Q[3], Nn * Fc, FVector2D(0, 1), Wing);
              S.TriFacing(I0, I1, I2, Nn * Fc); S.TriFacing(I0, I2, I3, Nn * Fc); }
        }
    }
    void Octa(FMapSurf& S, const FVector& C, float R, const FColor& Cl)
    {
        const FVector D[] = {{R, 0, 0}, {-R, 0, 0}, {0, R, 0}, {0, -R, 0}, {0, 0, R}, {0, 0, -R}};
        const int32 B = S.V.Num(); for (const FVector& Q : D) S.Vert(C + Q, Q.GetSafeNormal(), FVector2D::ZeroVector, Cl);
        const int32 F[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for (const auto& T : F) S.TriFacing(B + T[0], B + T[1], B + T[2], (D[T[0]] + D[T[1]] + D[T[2]]).GetSafeNormal());
    }
    void Emit(UProceduralMeshComponent* M, FMapSurf& S, UMaterialInterface* Mat, bool bShadow = true)
    {
        M->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, false);
        M->SetMaterial(0, Mat); M->SetCastShadow(bShadow); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// ------------------------------------------------------------------ AFeyLife
bool AFeyLife::bCalm = false;
int32 AFeyLife::FlowerToggles = 0, AFeyLife::OrchidShots = 0, AFeyLife::OrchidHits = 0, AFeyLife::MothTouches = 0, AFeyLife::OrchidAimed = 0;
bool AFeyLife::bOrchidSure = false;
const TArray<TWeakObjectPtr<AFeyLife>>& AFeyLife::All() { return GLife; }
const TArray<FVector>& FeyLifeClearings()
{
    static const TArray<FVector> S = []() { TArray<FVector> Out; for (int32 D = 0; D < Districts; ++D) Out.Append(Placed(D).Clear); Out.Append(AFeyDelights::Clearings()); return Out; }();
    return S;
}

AFeyLife::AFeyLife()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AFeyLife::EndPlay(const EEndPlayReason::Type Reason)
{
    GLife.RemoveAll([this](const TWeakObjectPtr<AFeyLife>& P) { return !P.IsValid() || P.Get() == this; });
    for (AActor* A : Spawned) if (IsValid(A)) A->Destroy();
    for (AActor* A : Mites) if (IsValid(A)) A->Destroy();
    for (AActor* A : Redcaps) if (IsValid(A)) A->Destroy();
    if (IsValid(Beast)) Beast->Destroy();
    for (AActor* A : MoreBeasts) if (IsValid(A)) A->Destroy();
    Super::EndPlay(Reason);
}

UProceduralMeshComponent* AFeyLife::NewMesh(USceneComponent* Parent, const FName& Name)
{
    auto* M = NewObject<UProceduralMeshComponent>(this, Name); M->SetupAttachment(Parent ? Parent : RootComponent.Get());
    M->bUseAsyncCooking = false; M->RegisterComponent(); return M;
}

void AFeyLife::Build(int32 D)
{
    District = D; GLife.Add(this);
    const FPlace& P = Placed(D);
    for (const TCHAR* Path : MatPaths) Mats.Add(LoadObject<UMaterialInterface>(nullptr, Path));
    if (Mats[MPlant]) { auto* Still = UMaterialInstanceDynamic::Create(Mats[MPlant], this); Still->SetScalarParameterValue(TEXT("Sway"), 0.f); Mats[MPlant] = Still; }   // (2026-11-23: its own surfaces do not sway)
    for (const TCHAR* Name : SfxNames) Sfx.Add(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s.%s"), Name, Name)));
    Falloff = NewObject<USoundAttenuation>(this); Falloff->Attenuation.bAttenuate = Falloff->Attenuation.bSpatialize = true;
    Falloff->Attenuation.AttenuationShape = EAttenuationShape::Sphere; Falloff->Attenuation.AttenuationShapeExtents = FVector(150.f); Falloff->Attenuation.FalloffDistance = 2200.f;
    Flowers = P.Flowers; Vines = P.Vines; Pollen = P.Pollen; Orchids = P.Orchids; Moths = P.Moths;
    PadRises = P.Rises; Refuges = P.Refuges;
    // (2026-11-22, stage 8) The rest is built a part at a time over the next frames (Tick), not all in this one.
    Pending.Add([this, D, &P]()
    {
    // Flowers: body (leaves, stem) once, the head rebuilt as it changes.
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocationAndRotation(Flowers[I].Pos, FRotator(0, Flowers[I].Yaw, 0)); Pivot->SetRelativeScale3D(FVector(1.6f));   // (knee-high to a man: as tall as he is)
        FMapSurf Leaves, Stem; FlowerBody(Leaves, Stem, I * 1.7f);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("FlowerLeaves%d"), I)), Leaves, Mats[MBlade]);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("FlowerStem%d"), I)), Stem, Mats[MPlant]);
        auto* Head = NewMesh(Pivot, *FString::Printf(TEXT("FlowerHead%d"), I));
        auto* Stamen = NewMesh(Head, *FString::Printf(TEXT("FlowerStamens%d"), I));
        FlowerHeads.Add(Head); FlowerHeads.Add(Stamen); FlowerPivots.Add(Pivot);
        if (Flowers[I].bHidden) { Flowers[I].Grow = 0.f; Pivot->SetVisibility(false, true); Pivot->SetRelativeScale3D(FVector(.01f)); }
        ShowFlower(I);
    }
    for (int32 I = 0; I < Vines.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocationAndRotation(Vines[I].C, FRotator(0, Vines[I].Yaw, 0));
        FMapSurf Wood, Leaf; VineCurtain(Wood, Leaf, Vines[I].Width, Vines[I].Height);
        auto* W = NewMesh(Pivot, *FString::Printf(TEXT("Vines%d"), I)); Emit(W, Wood, Mats[MRoot]);
        auto* L = NewMesh(W, *FString::Printf(TEXT("VineLeaves%d"), I)); Emit(L, Leaf, Mats[MBlade]);
        VineMeshes.Add(W);
        auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(Pivot); B->SetBoxExtent(FVector(Vines[I].Width * .5f + 30.f, 22.f, Vines[I].Height * .5f + 40.f));
        B->SetRelativeLocation(FVector(0, 0, Vines[I].Height * .5f)); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true); B->RegisterComponent();
        VineBoxes.Add(B);
    }
    // The boulders round the flower's nook, and what lies inside.
    if (UStaticMesh* RockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/ChultNature/SM_ChultRock2.SM_ChultRock2")))
        for (int32 I = 0; I < P.Rocks.Num(); ++I)
        {
            auto* R = NewObject<UStaticMeshComponent>(this); R->SetupAttachment(RootComponent); R->SetStaticMesh(RockMesh);
            const FVector& Q = P.Rocks[I];
            // (sized from the rock itself: about 3 m across and 2.2 m high, so the ring shuts)
            const FVector Ext = RockMesh->GetBounds().BoxExtent; const float Sx = 150.f / FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y)) * Q.Z / 2.2f, Sz = FMath::Max(Sx, 115.f / FMath::Max(1.f, Ext.Z));
            R->SetRelativeTransform(FTransform(FRotator(0, I * 47.f, 0), FVector(Q.X, Q.Y, LandZ(D, Q.X, Q.Y) - 25.f), FVector(Sx, Sx, Sz)));
            R->SetCollisionProfileName(TEXT("BlockAll")); R->RegisterComponent();
        }
    for (const FVector& C : P.Cigs) if (ACigarettePickup* Cg = ACigarettePickup::Spawn(GetWorld(), FeyWorld(C + FVector(0, 0, 30.f)), FVector::ZeroVector, FeyWorld(C).Z)) Spawned.Add(Cg);   // (gone with the district)
    });
    Pending.Add([this, D, &P]()
    {
    // Pollen: low gold flowers thick over each bed, and spores over it.
    if (Pollen.Num())
    {
        FMapSurf Bloom; FRandomStream R(31 + D);
        for (const FFeyPollen& Pb : Pollen)
        {
            const FQuat Q(FRotator(0, Pb.Yaw, 0)); const int32 N = FMath::RoundToInt(PI * Pb.R.X * Pb.R.Y / 140.f);
            for (int32 K = 0; K < N; ++K)
            {
                const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand());
                const FVector L = FVector(Pb.C, 0.f) + Q.RotateVector(FVector(Pb.R.X * Rr * FMath::Cos(A), Pb.R.Y * Rr * FMath::Sin(A), 0.f));
                const FVector Head = FVector(L.X, L.Y, LandZ(D, L.X, L.Y) + R.FRandRange(5.f, 17.f)); const float Sz = R.FRandRange(4.5f, 8.f), Tw = R.FRandRange(0.f, 2 * PI);
                const FColor Cl = Col(1.f, R.FRandRange(.72f, .86f), R.FRandRange(.18f, .32f), R.FRandRange(.26f, .4f) * (Rr < .92f ? 1.f : .5f));
                const int32 B = Bloom.V.Num(); Bloom.Vert(Head + FVector(0, 0, .6f), FVector::UpVector, FVector2D(0.f, .5f), Cl);
                for (int32 Pt = 0; Pt < 10; ++Pt) { const float Ang = Tw + PI * Pt / 5.f, Rad = Pt % 2 ? Sz * .45f : Sz; Bloom.Vert(Head + FVector(Rad * FMath::Cos(Ang), Rad * FMath::Sin(Ang), 0.f), FVector::UpVector, FVector2D(Pt % 2 ? .5f : 1.f, .5f), Cl); }
                for (int32 Pt = 0; Pt < 10; ++Pt) Bloom.TriFacing(B, B + 1 + Pt, B + 1 + (Pt + 1) % 10, FVector::UpVector);
                if (K % 5 == 0) { SporeBase.Add(FVector(L.X, L.Y, LandZ(D, L.X, L.Y) + R.FRandRange(10.f, 70.f))); }
            }
        }
        Emit(NewMesh(nullptr, TEXT("PollenBlooms")), Bloom, Mats[MPlant], false);
        Spores = SporeBase; SporeMesh = NewMesh(nullptr, TEXT("PollenSpores"));
    }
    });
    Pending.Add([this, D, &P]()
    {
    // Orchids.
    for (int32 I = 0; I < Orchids.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocation(Orchids[I].Pos); Pivot->SetRelativeScale3D(FVector(1.8f));   // (taller than he is, readable over the garden)
        FMapSurf Leaves, Stem, Bulb; OrchidBody(Leaves, Stem, Orchids[I].Dir); OrchidBulb(Bulb);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("OrchidLeaves%d"), I)), Leaves, Mats[MBlade]);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("OrchidStem%d"), I)), Stem, Mats[MPlant]);
        auto* B = NewMesh(Pivot, *FString::Printf(TEXT("OrchidBulb%d"), I)); Emit(B, Bulb, Mats[MPlant]);
        B->SetRelativeLocationAndRotation(Orchids[I].Dir * 4.f + FVector(0, 0, 60.f), FRotator(-90.f, 0, 0) + Orchids[I].Dir.Rotation() + FRotator(90.f, 0, 0));
        B->SetRelativeRotation(FRotationMatrix::MakeFromZX(Orchids[I].Dir + FVector(0, 0, .25f), FVector::UpVector).Rotator());
        OrchidBulbs.Add(B);
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(B); L->SetRelativeLocation(FVector(0, 0, 14.f));
        L->SetLightColor(FLinearColor(1.f, .35f, .75f)); L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f); L->SetAttenuationRadius(260.f); L->SetCastShadows(false); L->RegisterComponent();
        OrchidLights.Add(L);
    }
    if (Orchids.Num()) SeedMesh = NewMesh(nullptr, TEXT("OrchidSeeds"));
    // Moths.
    for (int32 I = 0; I < Moths.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        FMapSurf Body; MothBody(Body); Emit(NewMesh(Pivot, *FString::Printf(TEXT("MothBody%d"), I)), Body, Mats[MPlant], false);
        for (const float Sd : {1.f, -1.f})
        {
            auto* Hinge = NewObject<USceneComponent>(this); Hinge->SetupAttachment(Pivot); Hinge->RegisterComponent(); Hinge->SetRelativeLocation(FVector(0, Sd * 1.5f, 1.f));
            FMapSurf Wing; MothWing(Wing, Sd); Emit(NewMesh(Hinge, *FString::Printf(TEXT("MothWing%d_%d"), I, Sd > 0 ? 1 : 0)), Wing, Mats[MPetal], false);
            (Sd > 0 ? MothWingR : MothWingL).Add(Hinge);
        }
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(Pivot); L->SetLightColor(FLinearColor(1.f, .78f, .5f)); L->SetIntensityUnits(ELightUnits::Candelas);
        L->SetIntensity(45.f); L->SetAttenuationRadius(480.f); L->SetCastShadows(false); L->RegisterComponent();
        MothPivots.Add(Pivot);
        Loop(SMoth, Pivot, FVector::ZeroVector, .55f, 900.f);   // (its wings, softly)
        Moths[I].Last = Moths[I].Home;
    }
    });
    Pending.Add([this, D, &P]()
    {
    // (part two) The creatures (not while the walker is calmed), the camp, the refuges, the pads.
    BuildPadRise(); BuildRefuges();
    });
    Pending.Add([this, D, &P]()
    {
    BuildCamp();
    if (District == Deepwood) BuildHag();
    if (District == FeyMap::Crossroads)
    {
        // (2026-11-23) The trio milling by the great tree: on its ring road to the north-west, between the roads to the
        // Lily Mere and Highmeadow, heading round it the way the clock's hands go back (their circle turns left, toward
        // the tree).
        const FVector2D Hub(-67750.f, 0.f); const float A = FMath::DegreesToRadians(145.f), Ring = 3100.f;
        const FVector2D At = Hub + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Ring;
        const float Yaw = FMath::RadiansToDegrees(A) + 90.f;
        if (AFeyTrio* Trio = AFeyTrio::Spawn(GetWorld(), FeyWorld(FVector(At.X, At.Y, LandZ(District, At.X, At.Y))), Yaw)) Spawned.Add(Trio);
    }
    });
    Pending.Add([this, D, &P]()
    {
    BuildMaze();
    });
    Pending.Add([this, D, &P]()
    {

    if (!bCalm)
    {
        FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        for (const FVector& M : P.Mites) if (AFeyMite* Mt = GetWorld()->SpawnActor<AFeyMite>(FeyWorld(FVector(M.X, M.Y, LandZ(D, M.X, M.Y))), FRotator::ZeroRotator, Sp)) { Mt->Init(FVector(M.X, M.Y, LandZ(D, M.X, M.Y)), M.Z); Mites.Add(Mt); }
        SpawnRedcaps();
        if (P.Round.Num()) if ((Beast = GetWorld()->SpawnActor<AFeyDisplacer>(FeyWorld(P.Round[0]), FRotator::ZeroRotator, Sp)) != nullptr) Beast->Init(P.Round, Refuges);
        for (const TArray<FVector>& Rd : P.MoreRounds) if (AFeyDisplacer* B = GetWorld()->SpawnActor<AFeyDisplacer>(FeyWorld(Rd[0]), FRotator::ZeroRotator, Sp)) { B->Init(Rd, Refuges); MoreBeasts.Add(B); }
    }
    if (auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) Respawns = Chuck->GetRespawns();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_BUILT district=%s flowers=%d vines=%d pollen=%d orchids=%d moths=%d mites=%d redcaps=%d beast=%d refuges=%d pads=%d"), FeyMap::District(D).Name, Flowers.Num(), Vines.Num(), Pollen.Num(), Orchids.Num(), Moths.Num(),
        Mites.Num(), Redcaps.Num(), Beast != nullptr, Refuges.Num(), PadRises.Num() ? PadRises[0].Pads.Num() : 0);
    if (MoreBeasts.Num()) UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_MORE_BEASTS district=%s beasts=%d"), FeyMap::District(D).Name, MoreBeasts.Num());
    });
    // (2026-11-24) The district's small delights (fairy rings, wisps, koi, puffballs, swans, rainbow, crows, turning signs).
    Pending.Add([this, D]()
    {
        FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AFeyDelights* De = GetWorld()->SpawnActor<AFeyDelights>(GetActorLocation(), GetActorRotation(), Sp)) { De->Build(D); Spawned.Add(De); }
    });
}

void AFeyLife::ShowFlower(int32 I)
{
    FFeyFlower& F = Flowers[I];
    const float Glow = F.Pulse >= 0.f ? FMath::Sin(FMath::Clamp(F.Pulse / .9f, 0.f, 1.f) * PI) * (.6f + .4f * FMath::Sin(F.Pulse * 30.f)) : 0.f;
    const float Key = F.Open * 100.f + Glow * 10.f;
    if (FMath::IsNearlyEqual(Key, F.Shown, .05f)) return;
    F.Shown = Key;
    FMapSurf Petals, Stamens; FlowerHead(Petals, Stamens, F.Open, Glow);
    Emit(FlowerHeads[I * 2], Petals, Mats[MPetal]); Emit(FlowerHeads[I * 2 + 1], Stamens, Mats[MPlant], false);
    // (a shake as it pulses)
    FlowerHeads[I * 2]->SetRelativeRotation(FRotator(FMath::Sin(F.Pulse * 40.f) * 4.f * Glow, 0.f, FMath::Cos(F.Pulse * 33.f) * 4.f * Glow));
}

void AFeyLife::ResetAll()
{
    if (Pending.Num()) return;   // (not built yet: nothing to reset)
    for (int32 I = 0; I < Flowers.Num(); ++I) { Flowers[I].Open = Flowers[I].Want = 0.f; Flowers[I].Pulse = -1.f; ShowFlower(I); }
    for (FFeyVines& V : Vines) V.Up = V.Want = 1.f;
    for (FFeyPadRise& R : PadRises) R.Up = R.Want = 0.f;
    for (int32 K = 0; K < ScUp.Num(); ++K) ScUp[K] = ScWant[K] = 1.f;   // (the shortcuts close again; the plants once revealed stay)
    if (bCalm && District == Maze && Flowers.Num() && Vines.Num()) { Flowers[0].Open = Flowers[0].Want = 1.f; Vines[0].Up = Vines[0].Want = 0.f; ShowFlower(0); }   // (the walker goes straight in)
    Seeds.Reset();
    for (AFeyMite* M : Mites) if (IsValid(M)) M->ResetMite();
    if (IsValid(Beast)) Beast->ResetBeast();
    for (AFeyDisplacer* B : MoreBeasts) if (IsValid(B)) B->ResetBeast();
    for (AActor* A : Redcaps) if (IsValid(A)) A->Destroy();
    Redcaps.Reset(); SpawnRedcaps();
}

bool AFeyLife::TryScratch(const FVector& Location, const FVector& Ahead, float Reach)
{
    const FVector L = FeyLocal(Location); bool bAny = false;
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        FFeyFlower& F = Flowers[I]; const FVector To = F.Pos - L;
        if (F.bHidden || F.Grow < .9f) continue;
        if (To.Size2D() - 24.f > Reach || FMath::Abs(To.Z + Stand) > 90.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead.GetSafeNormal2D()) < .2f && To.Size2D() > 30.f) continue;
        bAny = true;
        if (F.Pulse >= 0.f) continue;   // (already changing)
        F.Pulse = 0.f; F.Want = F.Want > .5f ? 0.f : 1.f; ++FlowerToggles;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_FLOWER_SCRATCHED district=%s flower=%d open=%d"), District >= 0 ? FeyMap::District(District).Name : TEXT("-"), I, F.Want > .5f);
    }
    return bAny;
}

bool AFeyLife::InPollen(const FVector& L) const
{
    for (const FFeyPollen& Pb : Pollen)
    {
        const FVector Q = FQuat(FRotator(0, -Pb.Yaw, 0)).RotateVector(FVector(L.X - Pb.C.X, L.Y - Pb.C.Y, 0.f));
        if (FMath::Square(Q.X / Pb.R.X) + FMath::Square(Q.Y / Pb.R.Y) < 1.f) return true;
    }
    return false;
}

FVector AFeyLife::MothAt(int32 I) const
{
    const FFeyMoth& M = Moths[I]; const float A = Clock * M.Rate + M.Phase; const FVector Side(-M.Axis.Y, M.Axis.X, 0.f);
    return M.Home + M.Axis * (FMath::Sin(A) * M.Range) + Side * (FMath::Sin(A * 3.1f + M.Phase * 2.f) * M.Weave) + FVector(0, 0, FMath::Sin(A * 2.3f + 1.f + M.Phase) * M.Bob);
}

void AFeyLife::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Pending.Num())   // still building: a part or more each frame, within about 4 ms
    {
        const double T0 = FPlatformTime::Seconds();
        do { TFunction<void()> F = MoveTemp(Pending[0]); Pending.RemoveAt(0); F(); } while (Pending.Num() && FPlatformTime::Seconds() - T0 < .004);
        if (Pending.Num()) return;
    }
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, .05f); Clock += Dt;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck) return;
    if (Chuck->GetRespawns() != Respawns) { Respawns = Chuck->GetRespawns(); ResetAll(); }   // (back as they began after a death)
    const FVector C = FeyLocal(Chuck->GetActorLocation()), Feet = C - FVector(0, 0, Stand);
    // Flowers: the pulse, then the change.
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        FFeyFlower& F = Flowers[I];
        if (F.Pulse >= 0.f)
        {
            F.Pulse += Dt;
            if (F.Pulse > .7f && F.Pulse - Dt <= .7f)
            {
                Play(F.Want > .5f ? SFlowerOpen : SFlowerClose, F.Pos + FVector(0, 0, 70.f));
                if (F.Vines >= 0 && Vines.IsValidIndex(F.Vines)) Play(SVines, Vines[F.Vines].C + FVector(0, 0, 100.f));
                if (F.Sc >= 0) Play(SVines, F.Pos + FVector(0, 0, 150.f));
                if (F.Pads >= 0 && PadRises.IsValidIndex(F.Pads) && PadRises[F.Pads].Pads.Num()) Play(SPads, PadRises[F.Pads].Pads[0]);
            }
            if (F.Pulse > .7f)
            {
                F.Open = FMath::FInterpConstantTo(F.Open, F.Want, Dt, 1.2f);
                if (F.Vines >= 0) Vines[F.Vines].Want = 1.f - F.Want;
                if (F.Pads >= 0 && PadRises.IsValidIndex(F.Pads)) PadRises[F.Pads].Want = F.Want;
                if (F.Sc >= 0 && ScWant.IsValidIndex(F.Sc)) ScWant[F.Sc] = 1.f - F.Want;
            }
            if (F.Pulse > .7f && FMath::IsNearlyEqual(F.Open, F.Want)) F.Pulse = -1.f;
        }
        ShowFlower(I);
    }
    for (int32 I = 0; I < Vines.Num(); ++I)
    {
        FFeyVines& V = Vines[I];
        // Never grows back over him.
        const FVector Q = FQuat(FRotator(0, -V.Yaw, 0)).RotateVector(C - V.C);
        const bool bInside = FMath::Abs(Q.X) < V.Width * .5f + 70.f && FMath::Abs(Q.Y) < 90.f && Q.Z < V.Height + 80.f;
        const float Want = V.Want > V.Up && bInside ? V.Up : V.Want;
        V.Up = FMath::FInterpConstantTo(V.Up, Want, Dt, .8f);
        const float S = FMath::Lerp(.03f, 1.f, Smooth(0.f, 1.f, V.Up));
        VineMeshes[I]->SetRelativeScale3D(FVector(1.f + .1f * (1.f - S), 1.f, S));
        VineMeshes[I]->SetVisibility(V.Up > .02f);
        const bool bShut = V.Up > .5f;
        if ((VineBoxes[I]->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bShut) VineBoxes[I]->SetCollisionEnabled(bShut ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
    if (District == Maze) TickMaze(Dt, Chuck, C);
    // (part two) Pads rising out of the deep water (and sinking back), solid once up; the camp's fire flickering.
    for (int32 R = 0, Pi = 0; R < PadRises.Num(); ++R)
    {
        FFeyPadRise& Pr = PadRises[R]; Pr.Up = FMath::FInterpConstantTo(Pr.Up, Pr.Want, Dt, .6f);
        for (int32 K = 0; K < Pr.Pads.Num(); ++K, ++Pi)
        {
            if (!RisePadMeshes.IsValidIndex(Pi)) break;
            const float U = Smooth(K * .25f, K * .25f + .75f, Pr.Up);
            const FVector& Pd = Pr.Pads[K];
            RisePadMeshes[Pi]->SetRelativeLocationAndRotation(FVector(Pd.X, Pd.Y, FMath::Lerp(-70.f, .4f, U) + 2.f * FMath::Sin(Clock * 1.3f + K) * U), FRotator(FMath::Lerp(25.f, 0.f, U), K * 77.f, FMath::Lerp(-15.f, 0.f, U)));
            RisePadMeshes[Pi]->SetVisibility(U > .02f);
            const bool bSolid = U > .9f;
            if ((RisePadBoxes[Pi]->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bSolid) RisePadBoxes[Pi]->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
    if (FireLight) { FireFlicker = FMath::FInterpTo(FireFlicker, FMath::FRandRange(.75f, 1.15f), Dt, 12.f); FireLight->SetIntensity(55.f * FireFlicker); }
    if (HagLight) HagLight->SetIntensity(55.f * FMath::Lerp(.8f, 1.1f, .5f + .5f * FMath::Sin(Clock * 9.f) * FMath::Sin(Clock * 3.7f)));
    // Spores drift up and round, then begin again.
    if (SporeMesh && Spores.Num())
    {
        FMapSurf S;
        for (int32 I = 0; I < Spores.Num(); ++I)
        {
            const float T = FMath::Frac(Clock * .07f + I * .137f);
            const FVector P = SporeBase[I] + FVector(14.f * FMath::Sin(Clock * .6f + I), 14.f * FMath::Cos(Clock * .5f + I * 1.3f), 80.f * T);
            Octa(S, P, 1.3f, Col(1.f, .9f, .55f, .7f * FMath::Sin(PI * T)));
        }
        Emit(SporeMesh, S, Mats[MPlant], false);
    }
    // Orchids (2026-11-23: as the spined devils): asleep facing their lanes until he is in front, in sight and in reach; then
    // the bulb follows him, swells, and spits at where he was as it began to swell, often wide. A shot under way finishes.
    if (!OrchidSwelled.IsValidIndex(Orchids.Num() - 1)) OrchidSwelled.SetNumZeroed(Orchids.Num());
    const FVector Chest = Feet + FVector(0, 0, 34.f);
    for (int32 I = 0; I < Orchids.Num(); ++I)
    {
        FFeyOrchid& O = Orchids[I];
        if (O.Look.IsZero()) O.Look = O.Dir;
        const FVector Mouth = OrchidBulbs[I]->GetComponentLocation() - GetActorLocation();
        bool bSees = false;
        {
            const FVector To = Chest - Mouth;
            if (!bCalm && !Chuck->IsAstral() && To.Size2D() < O.Range * OrchidSees && To.Size2D() > 60.f && FVector::DotProduct(To.GetSafeNormal2D(), O.Dir) > OrchidArc)
            {
                FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(OrchidAim), false, this); Q.AddIgnoredActor(Chuck);
                bSees = !GetWorld()->LineTraceSingleByChannel(Hit, FeyWorld(Mouth + To.GetSafeNormal() * 30.f), FeyWorld(Chest), ECC_Visibility, Q);
            }
        }
        if (O.Beat < 0.f)
        {
            // Asleep: back toward its lane; wakes (staggered with its neighbours) when it sees him.
            if (bSees) { O.Wait += Dt; if (O.Wait > .35f + FMath::Fmod(O.Phase, 1.2f)) { O.Beat = OrchidBeat - OrchidWind - .5f; O.Wait = 0.f; } }
            else O.Wait = 0.f;
        }
        else
        {
            O.Beat += Dt;
            if (O.Beat >= OrchidBeat)
            {
                // the spit
                O.Beat -= OrchidBeat; ++OrchidShots; OrchidSwelled[I] = false; Play(SOrchidSpit, O.Pos + FVector(0, 0, 100.f));
                FFeySeed Sd; Sd.P = Mouth + O.Look * 25.f;
                const FVector Aim = O.bAimed ? (O.AimAt - Sd.P).GetSafeNormal() : O.Look;
                Sd.V = Aim * SeedSpeed; Sd.Life = FMath::Max(O.Range, O.bAimed ? FVector::Dist(O.AimAt, Sd.P) * 1.6f : 0.f) / SeedSpeed;
                Seeds.Add(Sd); O.bAimed = false;
                if (!bSees) O.Beat = -1.f;   // (he has gone: back to sleep after this one)
            }
            if (O.Beat >= OrchidBeat - OrchidWind && !OrchidSwelled[I])
            {
                // the wind-up: it takes aim where he is now, and usually wide of him
                OrchidSwelled[I] = true; Play(SOrchidSwell, O.Pos + FVector(0, 0, 100.f), .7f);
                if (bSees)
                {
                    const bool bWide = !bOrchidSure && FMath::FRand() < OrchidMiss;
                    const float Miss = bWide ? FMath::FRandRange(30.f, 95.f) : FMath::FRandRange(0.f, 8.f), A = FMath::FRandRange(0.f, 2.f * PI);
                    const FVector Side = FVector::CrossProduct((Chest - Mouth).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
                    O.AimAt = Chest + Side * FMath::Cos(A) * Miss + FVector::UpVector * FMath::Sin(A) * Miss * .5f; O.bAimed = true; ++OrchidAimed;
                }
                else O.Beat = -1.f;   // (lost him before it swelled: no shot at nothing)
            }
        }
        // The bulb turns to follow him while awake (to its aim once wound up), else back to its lane.
        FVector Want = O.Dir;
        if (O.Beat >= 0.f) Want = (O.bAimed ? O.AimAt - Mouth : Chest - Mouth).GetSafeNormal();
        if (Want.IsNearlyZero()) Want = O.Dir;
        O.Look = FMath::VInterpNormalRotationTo(O.Look, Want, Dt, O.Beat >= 0.f ? 160.f : 45.f);
        OrchidBulbs[I]->SetRelativeRotation(FRotationMatrix::MakeFromZX(O.Look + FVector(0, 0, .25f), FVector::UpVector).Rotator());
        const float T = O.Beat < 0.f ? 1.f : O.Beat, Wind = O.Beat < 0.f ? 0.f : Smooth(OrchidBeat - OrchidWind, OrchidBeat, T), After = O.Beat < 0.f ? 0.f : Smooth(.25f, 0.f, T);
        OrchidBulbs[I]->SetRelativeScale3D(FVector(1.f + .35f * Wind - .15f * After));
        OrchidLights[I]->SetIntensity(70.f * Wind + 25.f * After + (O.Beat >= 0.f ? 8.f : 0.f));
    }
    if (SeedMesh)
    {
        FCollisionQueryParams Qp(SCENE_QUERY_STAT(FeySeed), false, Chuck); Qp.AddIgnoredActor(this);
        for (int32 I = Seeds.Num() - 1; I >= 0; --I)
        {
            FFeySeed& Sd = Seeds[I]; const FVector From = Sd.P; Sd.P += Sd.V * Dt; Sd.Life -= Dt;
            FHitResult Hit;
            const bool bStop = Sd.Life <= 0.f || GetWorld()->LineTraceSingleByChannel(Hit, FeyWorld(From), FeyWorld(Sd.P), ECC_Visibility, Qp);
            const FVector Near = FMath::ClosestPointOnSegment(Sd.P, Feet + FVector(0, 0, 8.f), Feet + FVector(0, 0, 58.f));
            if (!bCalm && FVector::Dist(Sd.P, Near) < SeedRadius + 15.f && !Chuck->IsAstral())
            {
                Play(SOrchidHit, Sd.P, .8f);
                if (Chuck->TakeBite(FeyWorld(C - Sd.V.GetSafeNormal() * 100.f), 1, true, false)) { ++OrchidHits; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_ORCHID_HIT sanity=%d"), Chuck->GetSanity()); }
                Seeds.RemoveAt(I); continue;
            }
            if (bStop) { if (Sd.Life > 0.f) Play(SOrchidHit, Sd.P, .5f); Seeds.RemoveAt(I); }
        }
        FMapSurf S; for (const FFeySeed& Sd : Seeds) Octa(S, Sd.P, SeedRadius, Col(.5f, .2f, .12f, .02f));
        Emit(SeedMesh, S, Mats[MPlant], false);
    }
    // Moths.
    for (int32 I = 0; I < Moths.Num(); ++I)
    {
        FFeyMoth& M = Moths[I]; const FVector P = MothAt(I), V = (P - M.Last) / FMath::Max(Dt, 1e-3f); M.Last = P;
        if (V.SizeSquared2D() > 100.f) M.Facing = FMath::RInterpTo(M.Facing, FRotator(FMath::Clamp(-V.Z * .08f, -20.f, 20.f), V.Rotation().Yaw, 0.f), Dt, 7.f);
        MothPivots[I]->SetRelativeLocationAndRotation(P, M.Facing);
        const float Flap = 38.f * FMath::Sin(Clock * 14.f + M.Phase) + 8.f;
        MothWingR[I]->SetRelativeRotation(FRotator(0, 0, Flap)); MothWingL[I]->SetRelativeRotation(FRotator(0, 0, -Flap));
        M.TouchWait -= Dt;
        const FVector Near = FMath::ClosestPointOnSegment(P, Feet + FVector(0, 0, 8.f), Feet + FVector(0, 0, 58.f));
        if (!bCalm && M.TouchWait <= 0.f && FVector::Dist(P, Near) < MothRadius + 16.f && !Chuck->IsAstral())
        {
            M.TouchWait = 1.2f;
            if (Chuck->TakeBite(FeyWorld(P), 1, true, false)) { ++MothTouches; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MOTH_TOUCH sanity=%d"), Chuck->GetSanity()); }
        }
    }
}

// ------------------------------------------------------------------ part two (2026-11-22): the camp, the refuges, the pads
namespace
{
    // A hollow log along X, centred at the origin: bark outside, worn wood inside, open at both ends.
    void HollowLog(FMapSurf& S, float Len, float Ro, float Ri, float Seed)
    {
        constexpr int32 Nx = 14, Na = 18;
        auto Ring = [&](int32 I, float R0, bool bOut)
        {
            TArray<int32> Ids; const float X = FMath::Lerp(-Len * .5f, Len * .5f, float(I) / Nx);
            for (int32 J = 0; J < Na; ++J)
            {
                const float A = 2 * PI * J / Na, N = FMath::PerlinNoise2D(FVector2D(X / 90.f + Seed, J * .7f));
                const float R = R0 * (1.f + (bOut ? .07f : .04f) * N) * (1.f + (bOut ? .06f : 0.f) * FMath::Sin(X / 60.f + Seed));
                const FVector Rd(0.f, FMath::Cos(A), FMath::Sin(A));
                Ids.Add(S.Vert(FVector(X, 0, 0) + Rd * R, bOut ? Rd : -Rd, FVector2D(X / 120.f, J / float(Na) * 3.f), bOut ? Col(.33f, .62f + .12f * N, 0.f) : Col(.42f, .4f, 0.f)));
            }
            return Ids;
        };
        TArray<TArray<int32>> Out, In;
        for (int32 I = 0; I <= Nx; ++I) { Out.Add(Ring(I, Ro, true)); In.Add(Ring(I, Ri, false)); }
        for (int32 I = 0; I < Nx; ++I)
            for (int32 J = 0; J < Na; ++J)
            {
                const int32 J1 = (J + 1) % Na;
                const FVector Mo = (S.V[Out[I][J]] + S.V[Out[I + 1][J1]]) * .5f, Mi = (S.V[In[I][J]] + S.V[In[I + 1][J1]]) * .5f;
                const FVector No = FVector(0, Mo.Y, Mo.Z).GetSafeNormal();
                S.TriFacing(Out[I][J], Out[I + 1][J], Out[I + 1][J1], No); S.TriFacing(Out[I][J], Out[I + 1][J1], Out[I][J1], No);
                const FVector Ni = -FVector(0, Mi.Y, Mi.Z).GetSafeNormal();
                S.TriFacing(In[I][J], In[I + 1][J], In[I + 1][J1], Ni); S.TriFacing(In[I][J], In[I + 1][J1], In[I][J1], Ni);
            }
        for (const int32 I : {0, Nx})   // the ends: a ring of worn wood
            for (int32 J = 0; J < Na; ++J)
            {
                const int32 J1 = (J + 1) % Na; const FVector Ne(I ? 1.f : -1.f, 0, 0);
                S.TriFacing(Out[I][J], In[I][J], In[I][J1], Ne); S.TriFacing(Out[I][J], In[I][J1], Out[I][J1], Ne);
            }
    }
    // Ragged cloth hung between two edges A0-A1 (top) and B0-B1 (bottom), sagging, its lower edge torn.
    void Cloth(FMapSurf& S, const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1, const FColor& Cl, float Seed)
    {
        constexpr int32 Nu = 8, Nv = 6; const int32 Base = S.V.Num();
        for (int32 V = 0; V <= Nv; ++V)
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu, Vf = float(V) / Nv;
                const float Tear = V == Nv ? 18.f * FMath::Abs(FMath::Sin(Uf * 9.f + Seed)) : 0.f;
                const FVector Top = FMath::Lerp(A0, A1, Uf), Bot = FMath::Lerp(B0, B1, Uf);
                FVector P = FMath::Lerp(Top, Bot, Vf) - FVector(0, 0, 10.f * FMath::Sin(PI * Uf) * FMath::Sin(PI * Vf) + Tear);
                const FVector N = FVector::CrossProduct(A1 - A0, B0 - A0).GetSafeNormal();
                S.Vert(P, N, FVector2D(Uf, Vf), Cl);
            }
        for (int32 V = 0; V < Nv; ++V) for (int32 U = 0; U < Nu; ++U) { const int32 Q = Base + V * (Nu + 1) + U; S.Quad(Q, Q + 1, Q + Nu + 2, Q + Nu + 1); }
    }
    UProceduralMeshComponent* Solid(AFeyLife* Owner, UProceduralMeshComponent* M, FMapSurf& S, UMaterialInterface* Mat)
    {
        M->bUseComplexAsSimpleCollision = true;
        M->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, true);
        M->SetMaterial(0, Mat); M->SetCollisionProfileName(TEXT("BlockAll")); M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        return M;
    }
}

void AFeyLife::Play(int32 Which, const FVector& Local, float Volume)
{
    if (Sfx.IsValidIndex(Which) && Sfx[Which]) UGameplayStatics::PlaySoundAtLocation(this, Sfx[Which], FeyWorld(Local), Volume, FMath::FRandRange(.96f, 1.04f), 0.f, Falloff);
}

void AFeyLife::Loop(int32 Which, USceneComponent* At, const FVector& Local, float Volume, float Radius)
{
    if (!Sfx.IsValidIndex(Which) || !Sfx[Which]) return;
    auto* A = NewObject<UAudioComponent>(this); A->SetupAttachment(At ? At : RootComponent.Get()); A->SetRelativeLocation(Local);
    auto* F = NewObject<USoundAttenuation>(this); F->Attenuation = Falloff->Attenuation; F->Attenuation.FalloffDistance = Radius; F->Attenuation.AttenuationShapeExtents = FVector(Radius * .1f);
    A->SetSound(Sfx[Which]); A->AttenuationSettings = F; A->SetVolumeMultiplier(Volume); A->bAutoActivate = false; A->RegisterComponent();
    A->Play(FMath::FRandRange(0.f, 1.f)); Loops.Add(A);
}

FVector AFeyLife::GetIslet() const { return Placed(District).Islet; }
FVector AFeyLife::GetCache() const { return Placed(District).Cache; }
FVector AFeyLife::GetCamp() const { const FVector2D C = Placed(District).Camp; return FVector(C.X, C.Y, C.IsZero() ? 0.f : LandZ(District, C.X, C.Y)); }

void AFeyLife::SpawnRedcaps()
{
    const FPlace& P = Placed(District);
    if (bCalm) return;
    for (const FVector& R : P.Redcaps)
    {
        FTempleSkeletonPost Post; Post.Kind = 3; Post.Space = TEXT("feywild"); Post.Yaw = R.Z;
        Post.Feet = FeyWorld(FVector(R.X, R.Y, LandZ(District, R.X, R.Y)));
        const FVector2D Lc = P.Camp.IsZero() ? P.Patrol : P.Camp; const float Lr = P.Camp.IsZero() ? 1300.f : 1900.f;
        Post.Leash = FBox2D(Lc - FVector2D(Lr, Lr), Lc + FVector2D(Lr, Lr));
        if (ATempleSkeleton* S = ATempleSkeleton::SpawnAt(GetWorld(), Post)) { S->SetFeyFill(District == Maze ? 120.f : 80.f); Redcaps.Add(S); }
    }
    for (const FVector& R : P.Roamers)   // (2026-11-24; after the camp's, which the tests take first)
    {
        FTempleSkeletonPost Post; Post.Kind = 3; Post.Space = TEXT("feywild"); Post.Yaw = R.Z;
        Post.Feet = FeyWorld(FVector(R.X, R.Y, LandZ(District, R.X, R.Y)));
        Post.Leash = FBox2D(FVector2D(R.X, R.Y) - FVector2D(1350.f, 1350.f), FVector2D(R.X, R.Y) + FVector2D(1350.f, 1350.f));
        if (ATempleSkeleton* S = ATempleSkeleton::SpawnAt(GetWorld(), Post)) { S->SetFeyFill(80.f); Redcaps.Add(S); }
    }
}

void AFeyLife::BuildHag()
{
    // The cauldron's fire between its three stones, and its light; a little fire hidden in the chimney's top for its smoke; the
    // fire's crackle and the brew's bubbling (FeyMap.cpp places the cottage; its own frame: +X the front).
    const FVector Fire = FeyHagPoint(FVector(440.f, -220.f, 2.f)), Chimney = FeyHagPoint(FVector(-14.f, 318.f, 596.f));
    AChuckFlames* Fl = AChuckFlames::Get(GetWorld());
    if (Fl) { Fl->Add(this, FeyWorld(Fire), 38.f, 42.f); Fl->Add(this, FeyWorld(Chimney), 64.f, 12.f); }   // (the chimney's: wide, for a broad slow plume)
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HAG_FIRE flames=%d fire=%s chimney=%s"), Fl != nullptr, *Fire.ToCompactString(), *Chimney.ToCompactString());
    HagLight = NewObject<UPointLightComponent>(this); HagLight->SetupAttachment(RootComponent); HagLight->SetRelativeLocation(Fire + FVector(-70.f, 0, 125.f));   // (beside and over the pot, not inside it)
    HagLight->SetLightColor(FLinearColor(1.f, .5f, .2f)); HagLight->SetIntensityUnits(ELightUnits::Candelas); HagLight->SetIntensity(38.f);
    HagLight->SetAttenuationRadius(1500.f); HagLight->SetCastShadows(false); HagLight->RegisterComponent();
    Loop(SFire, nullptr, Fire + FVector(0, 0, 20.f), .55f, 1800.f); Loop(SCauldron, nullptr, Fire + FVector(0, 0, 90.f), .6f, 1000.f);
}

void AFeyLife::BuildCamp()
{
    const FPlace& P = Placed(District);
    if (P.Camp.IsZero()) return;
    const FVector2D C = P.Camp, Dp = P.CampOut, Pp = Perp(Dp);
    const float Z = LandZ(District, C.X, C.Y);
    // The fire in the ring of stones, its light; a tripod over it with a cauldron hung from it.
    if (AChuckFlames* Fl = AChuckFlames::Get(GetWorld())) Fl->Add(this, FeyWorld(FVector(C.X, C.Y, Z + 4.f)), 40.f, 55.f);
    FireLight = NewObject<UPointLightComponent>(this); FireLight->SetupAttachment(RootComponent); FireLight->SetRelativeLocation(FVector(C.X, C.Y, Z + 70.f));
    FireLight->SetLightColor(FLinearColor(1.f, .48f, .18f)); FireLight->SetIntensityUnits(ELightUnits::Candelas); FireLight->SetIntensity(55.f);
    FireLight->SetAttenuationRadius(1500.f); FireLight->SetCastShadows(false); FireLight->RegisterComponent();
    Loop(SFire, nullptr, FVector(C.X, C.Y, Z + 20.f), .9f, 2400.f); Loop(SCauldron, nullptr, FVector(C.X, C.Y, Z + 110.f), .5f, 1200.f);
    FMapSurf Wood, Iron, Rag;
    const FColor Pole = Col(.38f, .8f, 0.f), Black = Col(.09f, .09f, .1f, 0.f), Red = Col(.42f, .07f, .07f, 0.f), Stew = Col(.4f, .14f, .06f, .12f);
    const FVector Apex(C.X, C.Y, Z + 235.f);
    for (int32 K = 0; K < 3; ++K)
    {
        const float A = 2 * PI * K / 3 + .4f; const FVector Foot(C.X + 135.f * FMath::Cos(A), C.Y + 135.f * FMath::Sin(A), Z - 10.f);
        const FVector Pts[] = {Foot, FMath::Lerp(Foot, Apex, .5f) + FVector(0, 0, 3.f), Apex + (Apex - Foot).GetSafeNormal() * 22.f};
        Wood.Pipe(Pts, 4.2f, 3.f, 7, 4, Pole, K * 1.9f);
    }
    { const FVector Ch[] = {Apex - FVector(0, 0, 4.f), FVector(C.X, C.Y, Z + 122.f)}; Iron.Pipe(Ch, .9f, .9f, 5, 2, Black, 0.f); }
    {
        const FVector2D Pot[] = {{25.f, 42.f}, {22.5f, 40.f}, {26.f, 33.f}, {27.f, 18.f}, {20.f, 5.f}, {1.f, 0.f}};
        Iron.Lathe(FVector(C.X, C.Y, Z + 78.f), Pot, 16, Black);
        const FVector2D Top[] = {{.5f, 38.5f}, {22.f, 38.5f}};
        Iron.Lathe(FVector(C.X, C.Y, Z + 78.f), Top, 16, Stew);
        for (const float Sd : {-1.f, 1.f}) { const FVector H[] = {FVector(C.X, C.Y + Sd * 25.f, Z + 120.f), FVector(C.X, C.Y + Sd * 14.f, Z + 124.f), FVector(C.X, C.Y, Z + 122.f)}; Iron.Pipe(H, .7f, .7f, 4, 3, Black, Sd); }
    }
    // Two lean-tos behind the fire: posts, a ridge pole, poles slanting back to the ground, ragged red cloth over them.
    for (const float Sd : {-1.f, 1.f})
    {
        const FVector2D M = C - Dp * 680.f + Pp * (Sd * 430.f), Along = Pp, Back = -Dp;
        const float Zm = LandZ(District, M.X, M.Y);
        const FVector L0(M.X - Along.X * 120.f, M.Y - Along.Y * 120.f, Zm), L1(M.X + Along.X * 120.f, M.Y + Along.Y * 120.f, Zm);
        const FVector Up(0, 0, 165.f), Bk(Back.X * 190.f, Back.Y * 190.f, 0.f);
        for (const FVector& F : {L0, L1})
        {
            const FVector Post[] = {F - FVector(0, 0, 10.f), F + Up * .5f, F + Up + FVector(0, 0, 12.f)}; Wood.Pipe(Post, 4.f, 3.2f, 7, 3, Pole, F.X * .01f);
            const FVector Slant[] = {F + Up, F + Up * .5f + Bk * .5f, F + Bk - FVector(0, 0, 8.f)}; Wood.Pipe(Slant, 3.2f, 2.6f, 6, 3, Pole, F.Y * .01f);
        }
        { const FVector Ridge[] = {L0 + Up - (L1 - L0).GetSafeNormal() * 20.f, (L0 + L1) * .5f + Up + FVector(0, 0, 2.f), L1 + Up + (L1 - L0).GetSafeNormal() * 20.f}; Wood.Pipe(Ridge, 3.5f, 3.f, 6, 3, Pole, Sd); }
        Cloth(Rag, L0 + Up + FVector(0, 0, 4.f), L1 + Up + FVector(0, 0, 4.f), L0 + Bk + FVector(0, 0, 4.f), L1 + Bk + FVector(0, 0, 4.f), Red, Sd * 3.f);
        Cloth(Rag, L0 + Up + FVector(0, 0, 2.f) - Bk * .02f, L1 + Up + FVector(0, 0, 2.f) - Bk * .02f, L0 + FVector(0, 0, 70.f) - Bk * .05f, L1 + FVector(0, 0, 70.f) - Bk * .05f, Col(.3f, .05f, .05f), Sd * 5.f);   // (a torn curtain at its front)
    }
    Emit(NewMesh(nullptr, TEXT("CampWood")), Wood, Mats[MRoot]);
    Emit(NewMesh(nullptr, TEXT("CampIron")), Iron, Mats[MPlant]);
    Emit(NewMesh(nullptr, TEXT("CampCloth")), Rag, Mats[MPlant]);
    // The hollow behind the camp: a ring of boulders, its one way in under a root too low for a redcap.
    const FVector2D H(P.Cache.X, P.Cache.Y); const float Hr = P.Cache.Z * .5f + 150.f;   // (rocks' ring: 300)
    const FVector2D In = (C - H).GetSafeNormal(); const float A0 = FMath::Atan2(In.Y, In.X);
    if (UStaticMesh* RockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/ChultNature/SM_ChultRock2.SM_ChultRock2")))
        for (int32 K = 0; K < 10; ++K)
        {
            const float A = A0 + FMath::DegreesToRadians(32.f + K * 296.f / 9.f); const FVector2D Q = H + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Hr;
            auto* R = NewObject<UStaticMeshComponent>(this); R->SetupAttachment(RootComponent); R->SetStaticMesh(RockMesh);
            const FVector Ext = RockMesh->GetBounds().BoxExtent; const float Sx = 112.f / FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y)), Sz = FMath::Max(Sx, 120.f / FMath::Max(1.f, Ext.Z));
            R->SetRelativeTransform(FTransform(FRotator(0, K * 53.f, 0), FVector(Q.X, Q.Y, LandZ(District, Q.X, Q.Y) - 20.f), FVector(Sx, Sx, Sz)));
            R->SetCollisionProfileName(TEXT("BlockAll")); R->RegisterComponent();
        }
    {
        FMapSurf Gate; const FColor Root = Col(.36f, .75f, 0.f);
        FVector Posts[2];
        for (int32 K = 0; K < 2; ++K)
        {
            const float A = A0 + FMath::DegreesToRadians(K ? 21.f : -21.f); const FVector2D Q = H + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Hr; const float Zq = LandZ(District, Q.X, Q.Y);
            Posts[K] = FVector(Q.X, Q.Y, Zq);
            const FVector Pts[] = {FVector(Q.X, Q.Y, Zq - 15.f), FVector(Q.X, Q.Y, Zq + 60.f), FVector(Q.X, Q.Y, Zq + 128.f)}; Gate.Pipe(Pts, 16.f, 12.f, 9, 3, Root, K * 2.3f);
        }
        const FVector Mid = (Posts[0] + Posts[1]) * .5f;
        const FVector Lintel[] = {Posts[0] + FVector(0, 0, 100.f), Mid + FVector(0, 0, 94.f), Posts[1] + FVector(0, 0, 100.f)};
        Gate.Pipe(Lintel, 11.f, 11.f, 9, 4, Root, 7.f);   // (its underside about 82 cm up: he passes, a redcap does not)
        const FVector Over[] = {Posts[0] + FVector(0, 0, 125.f), Mid + FVector(0, 0, 150.f), Posts[1] + FVector(0, 0, 125.f)};
        Gate.Pipe(Over, 13.f, 13.f, 9, 4, Root, 9.f);
        Solid(this, NewMesh(nullptr, TEXT("CacheGate")), Gate, Mats[MRoot]);
        auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(RootComponent);
        const FVector Span = Posts[1] - Posts[0];
        B->SetBoxExtent(FVector(30.f, Span.Size2D() * .5f + 20.f, 110.f));
        B->SetRelativeLocationAndRotation(Mid + FVector(0, 0, 205.f), FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Span.Y, Span.X)) - 90.f, 0));
        B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true); B->RegisterComponent();
    }
}

void AFeyLife::BuildRefuges()
{
    const FPlace& P = Placed(District);
    for (int32 I = 0; I < P.Logs.Num(); ++I)
    {
        const FVector& L = P.Logs[I];
        FMapSurf S; HollowLog(S, 520.f, 80.f, 60.f, I * 3.1f);
        auto* M = NewMesh(nullptr, *FString::Printf(TEXT("RefugeLog%d"), I));
        M->SetRelativeLocationAndRotation(FVector(L.X, L.Y, LandZ(District, L.X, L.Y) + 48.f), FRotator(0, L.Z, 0));
        Solid(this, M, S, Mats[MRoot]);
    }
}

void AFeyLife::BuildPadRise()
{
    const FPlace& P = Placed(District);
    if (PadRises.IsEmpty()) return;
    UStaticMesh* Pad = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Feywild/SM_FeyLilyPad.SM_FeyLilyPad"));
    for (const FFeyPadRise& R : PadRises)
        for (const FVector& Pd : R.Pads)
        {
            auto* M = NewObject<UStaticMeshComponent>(this); M->SetupAttachment(RootComponent); M->SetStaticMesh(Pad);
            M->SetRelativeScale3D(FVector(Pd.Z / 50.f, Pd.Z / 50.f, .6f)); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetVisibility(false); M->RegisterComponent();
            RisePadMeshes.Add(M);
            auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(RootComponent); B->SetBoxExtent(FVector(Pd.Z * .8f, Pd.Z * .8f, 6.f));
            B->SetRelativeLocation(FVector(Pd.X, Pd.Y, -4.f)); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetCollisionEnabled(ECollisionEnabled::NoCollision); B->SetHiddenInGame(true); B->RegisterComponent();
            RisePadBoxes.Add(B);
        }
    if (!P.Islet.IsZero())
    {
        // The islet: a low mossy mound out of the deep water.
        FMapSurf S; const float R = P.Islet.Z;
        const FVector2D Prof[] = {{.5f, 24.f}, {R * .35f, 22.f}, {R * .7f, 15.f}, {R * .92f, 5.f}, {R, -6.f}, {R * 1.12f, -90.f}};
        S.Lathe(FVector(P.Islet.X, P.Islet.Y, 0.f), Prof, 28, FLinearColor(0.f, .9f, .3f, 1.f).ToFColor(false));
        Solid(this, NewMesh(nullptr, TEXT("Islet")), S, Mats[MGround]);
    }
}

// ------------------------------------------------------------------ stage 6 (2026-11-22): the Great Hedge Maze
int32 AFeyLife::BeeKnocks = 0, AFeyLife::StatueTurns = 0, AFeyLife::Reveals = 0, AFeyLife::ThornPricks = 0;
float AFeyLife::RevealAfter = 240.f, AFeyLife::RevealEvery = 150.f;

void AFeyLife::BuildMaze()
{
    if (District != Maze) return;
    const FPlace& P = Placed(District);
    FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
    // The shortcuts' hedges: each its own pivot at the ground, so it can sink away; leaves as the maze's own.
    UStaticMesh* LeafMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Feywild/SM_FeyHedgeLeaves.SM_FeyHedgeLeaves"));
    for (int32 K = 0; K < MazeShortcuts().Num(); ++K)
    {
        const FMazeShortcut& Sc = MazeShortcuts()[K]; const float Zm = LandZ(Maze, Sc.Mid.X, Sc.Mid.Y);
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocation(FVector(Sc.Mid.X, Sc.Mid.Y, Zm));
        FMapSurf S; auto* Leaves = LeafMesh ? NewObject<UInstancedStaticMeshComponent>(this) : nullptr;
        if (Leaves) { Leaves->SetupAttachment(Pivot); Leaves->SetStaticMesh(LeafMesh); Leaves->SetCollisionEnabled(ECollisionEnabled::NoCollision); Leaves->RegisterComponent(); }
        FBox2D Foot(ForceInit);
        for (const FVector4& H : Sc.Hedges)
        {
            const FVector C(H.X - Sc.Mid.X, H.Y - Sc.Mid.Y, Tall * .5f + LandZ(Maze, H.X, H.Y) - Zm - 6.f), Ext(H.Z, 60.f, Tall * .5f);
            S.Hedge(C, Ext, H.W, K * 3.7f + H.X * .001f);
            const float Yr = FMath::DegreesToRadians(H.W); const FVector Along(FMath::Cos(Yr), FMath::Sin(Yr), 0.f);
            Foot += FVector2D(C + Along * H.Z); Foot += FVector2D(C - Along * H.Z);
            if (Leaves)
            {
                const int32 N = FMath::Max(1, FMath::RoundToInt(2.f * H.Z / 300.f)); const float Len = 2.f * H.Z / N;
                for (int32 J = 0; J < N; ++J) Leaves->AddInstance(FTransform(FRotator(0, H.W + (J % 2 ? 180.f : 0.f), 0), C + Along * ((J + .5f) * Len - H.Z), FVector(Len / 300.f, 1.f, Tall * .5f / 420.f)));
            }
        }
        auto* M = NewMesh(Pivot, *FString::Printf(TEXT("ShortcutHedge%d"), K));
        M->bUseComplexAsSimpleCollision = true; M->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, true);
        M->SetMaterial(0, Mats[MHedge]); M->SetCollisionProfileName(TEXT("BlockAll"));
        ShortcutPivots.Add(Pivot); ScUp.Add(1.f); ScWant.Add(1.f);
    }
    // Statues that turn to face him while he is not looking (stone, hooded, on a plinth).
    for (int32 K = 0; K < P.Statues.Num(); ++K)
    {
        const FVector2D Q = P.Statues[K];
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocationAndRotation(FVector(Q.X, Q.Y, LandZ(Maze, Q.X, Q.Y)), FRotator(0, K * 83.f, 0));
        FMapSurf St; const FColor Stone(140, 0, 0, 255);
        const FVector2D Plinth[] = {{.5f, 70.f}, {52.f, 70.f}, {56.f, 62.f}, {56.f, 8.f}, {62.f, 0.f}, {62.f, -10.f}};
        St.Lathe(FVector::ZeroVector, Plinth, 12, Stone);
        const FVector2D Robe[] = {{.5f, 238.f}, {14.f, 236.f}, {22.f, 222.f}, {24.f, 200.f}, {30.f, 170.f}, {36.f, 120.f}, {42.f, 72.f}};
        St.Lathe(FVector::ZeroVector, Robe, 14, Stone);
        const FVector2D Hood[] = {{.5f, 282.f}, {12.f, 278.f}, {19.f, 264.f}, {20.f, 246.f}, {15.f, 234.f}};
        St.Lathe(FVector(2.f, 0, 0), Hood, 12, Stone);
        const FVector Arm[] = {FVector(4.f, 20.f, 205.f), FVector(22.f, 26.f, 175.f), FVector(48.f, 12.f, 168.f)};   // (pointing, a little)
        St.Pipe(Arm, 7.f, 5.f, 8, 4, Stone, K * 1.3f);
        St.Box(FVector(18.f, 0.f, 262.f), FVector(6.f, 12.f, 9.f), 0.f, FColor(40, 0, 0, 255));   // (the shadow of its face)
        auto* M = NewMesh(Pivot, *FString::Printf(TEXT("Statue%d"), K)); Emit(M, St, Mats[MStone]);
        M->bUseComplexAsSimpleCollision = true; M->CreateMeshSection(0, St.V, St.T, St.N, St.UV, St.C, St.Tan, true); M->SetCollisionProfileName(TEXT("BlockAll"));
        Statues.Add(Pivot); StatueYaw.Add(K * 83.f);
    }
    // The bees on the hedge tops: specks, shown only while they swarm; their buzz.
    BeeMesh = NewMesh(nullptr, TEXT("Bees"));
    if (Sfx.IsValidIndex(SBees) && Sfx[SBees])
    {
        BeeAudio = NewObject<UAudioComponent>(this); BeeAudio->SetupAttachment(RootComponent); BeeAudio->SetSound(Sfx[SBees]); BeeAudio->bAutoActivate = false;
        BeeAudio->AttenuationSettings = Falloff; BeeAudio->RegisterComponent();
    }
    NextReveal = RevealAfter;
    if (bCalm && Flowers.Num() && Vines.Num()) { Flowers[0].Open = Flowers[0].Want = 1.f; Vines[0].Up = Vines[0].Want = 0.f; ShowFlower(0); }   // (the walker goes straight in)
}

void AFeyLife::RevealNext(const FVector& Near)
{
    if (Pending.Num() || FlowerPivots.Num() != Flowers.Num()) return;
    int32 Best = -1; float Bd = 1e12f;
    for (int32 I = 0; I < Flowers.Num(); ++I) if (Flowers[I].bHidden) { const float D = FVector::DistSquared2D(Flowers[I].Pos, Near); if (D < Bd) { Bd = D; Best = I; } }
    if (Best < 0) return;
    Flowers[Best].bHidden = false; Flowers[Best].Grow = 0.f; ++Reveals;
    FlowerPivots[Best]->SetVisibility(true, true);
    Play(SFlowerOpen, Flowers[Best].Pos + FVector(0, 0, 60.f), 1.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_REVEAL flower=%d shortcut=%d at=%s maze_s=%.0f"), Best, Flowers[Best].Sc, *Flowers[Best].Pos.ToCompactString(), MazeTime);
}

void AFeyLife::TickMaze(float Dt, AChuckCharacter* Chuck, const FVector& C)
{
    FVector2D Hub; float R0, Pitch, Rmax, Tall; MazeRings(Hub, R0, Pitch, Rmax, Tall);
    const FVector Feet = C - FVector(0, 0, Stand);
    const float R = FVector2D::Distance(FVector2D(Feet), Hub);
    const bool bInMaze = R > R0 - 50.f && R < Rmax + 100.f;
    // (2026-11-23) Thorns across the way: on the ground among them he goes slowly (FeyGroundSpeedScale), and each second there
    // costs a Sanity point (at most one each 1.6 s) and a stumble back the way he came. Over them (the trellis, the hoops, the
    // walkway, a jump) nothing.
    if (bInMaze && !bCalm && Clock >= NextPrick && Chuck->GetCharacterMovement()->IsMovingOnGround() && InMazeThorns(Feet))
    {
        const FVector V = Chuck->GetVelocity().GetSafeNormal2D();
        if (Chuck->TakeBite(Chuck->GetActorLocation() + (V.IsNearlyZero() ? Chuck->GetActorForwardVector() : V) * 60.f, 1, true, false))
        { ++ThornPricks; NextPrick = Clock + 1.6f; Play(SVines, Feet, .6f); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_THORN_PRICK sanity=%d"), Chuck->GetSanity()); }
    }
    // Lost: time in the maze brings out the hit plants, nearest him first.
    if (bInMaze && !bCalm)
    {
        MazeTime += Dt;
        if (MazeTime >= NextReveal) { RevealNext(Feet); NextReveal = MazeTime + RevealEvery; }
    }
    for (int32 I = 0; I < Flowers.Num(); ++I)
        if (!Flowers[I].bHidden && Flowers[I].Grow < 1.f)
        {
            Flowers[I].Grow = FMath::Min(1.f, Flowers[I].Grow + Dt / 2.2f);
            const float G = Smooth(0.f, 1.f, Flowers[I].Grow);
            FlowerPivots[I]->SetRelativeScale3D(FVector(1.6f * FMath::Max(.01f, G)));
        }
    // The shortcuts' hedges sink into the ground (never rising over him).
    for (int32 K = 0; K < ScUp.Num(); ++K)
    {
        const FVector2D Mid = MazeShortcuts()[K].Mid;
        bool bOver = false; for (const FVector4& H : MazeShortcuts()[K].Hedges) bOver |= FVector2D::Distance(FVector2D(H.X, H.Y), FVector2D(Feet)) < H.Z + 90.f;
        const float Want = ScWant[K] > ScUp[K] && bOver ? ScUp[K] : ScWant[K];
        ScUp[K] = FMath::FInterpConstantTo(ScUp[K], Want, Dt, .45f);
        USceneComponent* Pv = ShortcutPivots[K];
        Pv->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(.02f, Smooth(0.f, 1.f, ScUp[K]))));
        Pv->SetVisibility(ScUp[K] > .03f, true);
        for (USceneComponent* Ch : Pv->GetAttachChildren())
            if (auto* Pm = Cast<UPrimitiveComponent>(Ch)) { const bool bSolid = ScUp[K] > .35f; if ((Pm->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bSolid && Cast<UProceduralMeshComponent>(Pm)) Pm->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision); }
    }
    // Statues: while he looks away, each near one turns to face him.
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye, View); const FVector Look = View.Vector();
        for (int32 K = 0; K < Statues.Num(); ++K)
        {
            const FVector Sw = Statues[K]->GetComponentLocation() + FVector(0, 0, 200.f);
            const float D = FVector::Dist2D(Sw, Chuck->GetActorLocation());
            if (D > 2600.f || D < 150.f) continue;
            const bool bSeen = FVector::DotProduct((Sw - Eye).GetSafeNormal(), Look) > .35f;
            const float Want = (Chuck->GetActorLocation() - Sw).Rotation().Yaw;
            if (!bSeen && FMath::Abs(FMath::FindDeltaAngleDegrees(StatueYaw[K], Want)) > 25.f)
            { StatueYaw[K] = Want; Statues[K]->SetRelativeRotation(FRotator(0, Want, 0)); ++StatueTurns; }
        }
    }
    // Bees on the hedge tops: a warning buzz, then the swarm knocks him back down into the maze (never a death).
    const float Gz = LandZ(Maze, Feet.X, Feet.Y);
    const bool bTop = bInMaze && Feet.Z > Gz + Tall - 120.f && Feet.Z < Gz + Tall + 220.f && !Chuck->IsAstral();   // (on the hedges, not on the stair high over them)
    OnTop = bTop ? OnTop + Dt : 0.f;
    if (bTop && BeeAudio && !BeeAudio->IsPlaying()) BeeAudio->Play();
    if (BeeAudio) { BeeAudio->SetRelativeLocation(Feet + FVector(0, 0, 60.f)); if (!bTop && BeeShow <= 0.f && BeeAudio->IsPlaying()) BeeAudio->FadeOut(.6f, 0.f); }
    if (bTop) BeeShow = FMath::Max(BeeShow, .3f + FMath::Min(OnTop, 1.5f));
    if (OnTop > 1.4f)
    {
        // Down into the nearest corridor.
        const float Ring = FMath::Clamp(FMath::RoundToFloat((R - R0) / Pitch - .5f), 0.f, (Rmax - R0) / Pitch - 1.f);
        const FVector2D Dir = (FVector2D(Feet) - Hub).GetSafeNormal(), To = Hub + Dir * (R0 + (Ring + .5f) * Pitch);
        const FVector2D Push = (To - FVector2D(Feet)).Size() > 20.f ? (To - FVector2D(Feet)).GetSafeNormal() : Dir;
        Chuck->LaunchCharacter(FVector(Push.X * 380.f, Push.Y * 380.f, 120.f), true, true);
        ++BeeKnocks; OnTop = 0.f; BeeShow = 2.5f;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MAZE_BEES knock=%d at=%s"), BeeKnocks, *Feet.ToCompactString());
    }
    if (BeeMesh)
    {
        BeeShow = FMath::Max(0.f, BeeShow - Dt);
        FMapSurf S;
        if (BeeShow > 0.f)
        {
            BeeAt = FMath::VInterpTo(BeeAt.IsZero() ? Feet : BeeAt, Feet + FVector(0, 0, 50.f), Dt, 4.f);
            const int32 N = FMath::RoundToInt(16.f + 40.f * FMath::Min(1.f, BeeShow / 1.5f));
            for (int32 I = 0; I < N; ++I)
            {
                const float A = Clock * (2.f + (I % 5) * .4f) + I * 2.399f, Rr = 40.f + 50.f * FMath::Frac(I * .618f) + 18.f * FMath::Sin(Clock * 3.f + I);
                const FVector Pb = BeeAt + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), 30.f * FMath::Sin(A * 1.7f + I));
                const float Wr = 2.f + (I % 5) * .4f;   // (the tangent of its circle, with its bob)
                const FVector Vel(-Rr * FMath::Sin(A), Rr * FMath::Cos(A), 30.f * 1.7f * FMath::Cos(A * 1.7f + I) * .4f);
                Bee(S, Pb, Vel * Wr, .5f * FMath::Sin(Clock * 95.f + I * 1.3f));
            }
        }
        Emit(BeeMesh, S, Mats[MPlant], false);
    }
}

float FeyGroundSpeedScale(const FVector& World)
{
    if (!InFeyMap(World)) return 1.f;
    const FVector L = FeyLocal(World) - FVector(0, 0, Stand);
    for (const TWeakObjectPtr<AFeyLife>& P : GLife) if (P.IsValid() && P->InPollen(L)) return .5f;
    if (InMazeThorns(L)) return .45f;   // (2026-11-23) pushing through thorns
    return 1.f;
}

bool FeyScratch(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach)
{
    if (!Chuck || !InFeyMap(Location)) return false;
    bool bAny = FeyCreaturesScratch(Location, Ahead, Reach);
    for (const TWeakObjectPtr<AFeyLife>& P : GLife) if (P.IsValid()) bAny |= P->TryScratch(Location, Ahead, Reach);
    for (const TWeakObjectPtr<AFeyDelights>& P : AFeyDelights::All()) if (P.IsValid()) bAny |= P->TryScratch(Location, Ahead, Reach);   // (puffballs, seed heads)
    return bAny;
}
