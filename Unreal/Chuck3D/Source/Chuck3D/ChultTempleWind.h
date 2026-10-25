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

    // The sanctum: a hall entered from leg 4 on a gallery across its +T end, twin stairs down its side walls, a
    // processional between colonnades to a dais. Made bigger and grander on 2026-10-23 (user): 54 x 34 m and 17.5 m
    // high (it was 46 x 22 x 10), its floor 7 m below the halls' (the plaza is above its ceiling), so the gallery,
    // at the tunnels' level, is 13 m up and the stairs run 25 m down the walls.
    constexpr float HS0 = 77000, HS1 = 80400, HT0 = -8800, HT1 = -3400, HZ = TempleRun::Floor - 700, HTop = HZ + 1750;   // 4 m under the plaza (the roots)
    constexpr float GalleryT = HT1 - 500;            // the gallery's front edge (it is at UZ)
    constexpr float StairW = 200, StairFoot = GalleryT - (UZ - HZ) * 1.9f;
    constexpr float HallMid = (HS0 + HS1) * .5f;
    constexpr float DaisT0 = HT0, DaisT1 = -7600, DaisS0 = 77600, DaisS1 = 79800, DaisH = 96;
    // The colonnades: two rows of piers either side of the nave, at these T (the gap at the stairs' foot is a lane).
    constexpr float NaveHalf = 900, PierHalf = 95;
    constexpr float PierT[] = {-4600, -5150, -5700, -6750, -7300};

    // The sanctum's broken floor (2026-10-24, user; docs/CHULT-TEMPLE-WIND.md#the-sanctums-ruptures), the cellar's
    // kind of Astral rupture (a ragged hole, the Astral's well under it, an oil haze round its rim), not the tunnels' slits:
    // - the pool: the whole floor between the stairs, from under the gallery to PoolT (ragged by PoolJag), so a jump off
    //   the gallery or the stairs' top lands in the Astral. A sprint leap from the stair's head carries about 9 m out from
    //   13 m up (420 cm/s, 2.2 s); about 4 m down the stair it clears the pool. The piers inside stand on islands of floor,
//   broad enough that a well-judged drop from the gallery can land on one (allowed: user, 2026-10-24).
    // - a few smaller ruptures in the aisles;
    // - the sea tear: to the wizard's left, torn open on another world's open sea (a fall death for now).
    constexpr float PoolS0 = HS0 + StairW, PoolS1 = HS1 - StairW, PoolT = -4975, PoolJag = 25;
    struct FSanctumRift { float S, T, RS, RT; };
    constexpr FSanctumRift SanctumRifts[] = {{77450, -5950, 48, 62}, {79960, -5600, 52, 70}, {77350, -7950, 56, 72}, {80060, -8300, 50, 62}};
    constexpr float SeaS = 78083, SeaT = -6710, SeaR = 110, SeaDepth = 820;
}

// What the sanctum's floor is at a frame point: 0 floor, 1 the Astral (the pool or a rupture), 2 the sea tear. Grow
// widens each hole (for keeping feet and bodies clear of it).
int32 ChultTempleSanctumHoleAt(const FVector2D& Frame, float Grow = 0);
// The islands of floor the pool's piers stand on (2026-10-24, user: "similar to the floor under the cheese crate in
// the cellar"): ragged round slabs, each a square box round its centre whose half-width is its mean radius.
const TArray<FBox2D>& ChultTempleSanctumIslands();
// Island I's rim radius at an angle round its centre (radians).
float ChultTempleSanctumIslandRadius(int32 I, float Angle);
// The sea tear's rim radius at an angle round its centre (radians).
float ChultTempleSanctumSeaRadius(float Angle);

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
