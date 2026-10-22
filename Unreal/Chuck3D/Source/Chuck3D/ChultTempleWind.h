#pragma once
#include "CoreMinimal.h"
#include "ChultTempleRun.h"
class AActor;
class AChuckCharacter;

// The Astral wind and the sanctum (Claude, 2026-10-22; docs/CHULT-TEMPLE-WIND.md): beyond the trap
// run's antechamber (its choked arch opened), a winding run of narrow tunnels after the 2D game's
// map 6. Four narrow legs, each with a spike pit crossed only by wall runs kicked back and forth
// between its walls, joined by three wider bends; Astral ruptures torn in the floors all along
// (walked round, the sewer's kind). Then the sanctum (2D map 9): a grand hall entered on a gallery,
// empty for now, the staging for the adventurers' battle with the beholder.
// Frame coordinates as in ChultTempleInterior.h (S toward the temple, T across, Z above the plaza).
namespace TempleWind
{
    constexpr float UZ = TempleRun::UZ;          // the tunnels' floor (the trap run's upper level)
    constexpr float Top = UZ + 400;              // their ceiling
    constexpr float Spring = UZ + 316;           // where their corbels start: the openings between them stop here
    constexpr float Narrow = 180, Wide = 300;    // a leg's width (the chained wall run needs the far wall close), a bend's
    constexpr float PitDepth = 120;

    // A run of the tunnels: a straight frame-aligned box. Axis 1 runs along T, 0 along S; Dir the way he goes.
    struct FSeg { float S0, S1, T0, T1; int32 Axis; float Dir; };
    // The exit passage from the antechamber (T AnteT1..ExitT1, the old choke) belongs to leg 1.
    constexpr float ExitT1 = TempleRun::AnteT1 + 380;
    constexpr FSeg Legs[] = {
        {TempleRun::FS0, TempleRun::FS1, ExitT1, -860, 1, 1.f},   // leg 1, +T
        {75420, 75600, -3000, -860, 1, -1.f},                      // leg 2, -T
        {77120, 77300, -3000, -860, 1, 1.f},                       // leg 3, +T
        {78620, 78800, -3400, -860, 1, -1.f}};                     // leg 4, -T, into the sanctum's gallery (4 m longer: its pit is 15 m)
    constexpr FSeg Bends[] = {
        {TempleRun::FS0, 75600, -860, -560, 0, 1.f},
        {75420, 77300, -3300, -3000, 0, 1.f},
        {77120, 78800, -860, -560, 0, 1.f}};

    // Each leg's spike pit: From (the lip he jumps from) and To along T, in the order he meets them.
    // Lengths measured with the real character (docs): 1 one kick; 2 two kicks; 3 two kicks, its walls
    // broken so the line must go +S, -S, +S; 4 the longest.
    struct FPit { float From, To; };
    constexpr FPit Pits[] = {{-2000, -1540}, {-1450, -2350}, {-2450, -1550}, {-1250, -2750}};
    // Leg 3's broken walls: the -S wall over the pit's first 2 m, the +S wall over its middle. Each punishes a kick
    // made too early (the catch would come in the gap); kicked 1.2-2 m into each run he meets wall both times.
    constexpr float BreakMinusT0 = -2450, BreakMinusT1 = -2250, BreakPlusT0 = -2100, BreakPlusT1 = -1900, BreakDeep = 110;

    // The sanctum: a hall 46 x 22 m, 10 m high, its floor the halls' (-1460). Entered from leg 4 on a gallery
    // across its +T end, twin stairs down its side walls, a processional between colonnades to a dais.
    constexpr float HS0 = 77600, HS1 = 79800, HT0 = -8000, HT1 = -3400, HZ = TempleRun::Floor, HTop = HZ + 1000;
    constexpr float GalleryT = HT1 - 400;            // the gallery's front edge (it is at UZ)
    constexpr float StairW = 160, StairFoot = GalleryT - 1140;
    constexpr float HallMid = (HS0 + HS1) * .5f;
    constexpr float DaisT0 = HT0, DaisT1 = -7200, DaisS0 = 77900, DaisS1 = 79500, DaisH = 96;
}

// Its Astral ruptures (frame, around a centre: tear outline points), for the build, the deaths and the checks.
// C is where it was placed; Mid the middle of its (bent) slit, always inside it.
struct FTempleWindTear { FVector2D C, Mid; TArray<FVector2D> Edge; FBox2D Box; float Radius = 0; int32 Seg = 0; };
const TArray<FTempleWindTear>& ChultTempleWindTears();
bool ChultTempleWindTearAt(const FVector2D& Frame, float Grow = 0);
// A walking line on the tunnels' floors from From to To, Clear cm round clear of the ruptures and pits (frame
// points at its corners). False if there is none.
bool ChultTempleWindPath(const FVector2D& From, const FVector2D& To, float Clear, TArray<FVector2D>& Out);

// -ChuckTempleWindTest (in a Chult run): stills, then the real character through each part.
void ReviewChultTempleWind(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
