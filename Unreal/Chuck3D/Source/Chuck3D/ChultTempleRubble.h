#pragma once
#include "CoreMinimal.h"
#include "ChultTempleWind.h"

// The rubble map (Claude, 2026-10-26; docs/CHULT-TEMPLE-RUBBLE.md), after the 2D game's temple_rubble map: where the
// wizard's Fireball throws Chuck. Since 2026-11-04 (user: "make the rubble map look more convincingly like the temple
// sanctum had partially collapsed in. there should be rubble from the balcony, blocked off stair cases, and only torches
// where there were torches in the temple sanctum map") it is the sanctum itself, fallen in: the same hall, gallery,
// stairs, colonnades, dais and torches (built by the sanctum's own builder, TempleWindBuild::Sanctum with an
// FSanctumFall), its vault caved in four places, the gallery's middle fallen into the Astral pool below it, both stairs
// choked with what came down, two piers broken off and lying in the nave, the sea tear buried. One clear lane winds
// from where he comes to, to the one way out: a crevice low in the -S wall, too low to stand in, that he crawls along on
// all fours. Its end (a crack of warm light) is where the escape to the ship will begin; that is not built yet.
// The copy stands DropZ under the sanctum (the white-out carries him there), so the battle's hall is never touched.
// Frame coordinates as in ChultTempleInterior.h (S toward the temple, T across, Z above the plaza).
namespace TempleRubble
{
    constexpr float DropZ = -4000;   // the fallen hall's depth under the sanctum's (its cavities stay clear of the sanctum's wells)
    constexpr float RS0 = TempleWind::HS0, RS1 = TempleWind::HS1, RT0 = TempleWind::HT0, RT1 = TempleWind::HT1;
    constexpr float RZ = TempleWind::HZ + DropZ, RTop = TempleWind::HTop + DropZ;   // its floor and its vault
    constexpr float RUZ = TempleWind::UZ + DropZ;                                    // the gallery's floor
    // The gallery's runs still standing at the stairs' heads (the middle 16 m fell into the pool), and the piers brought
    // down (row 0 the -S row, 1 the +S; index in TempleWind::PierT): the -S row's fourth, the +S row's second.
    constexpr float GalleryKeep0 = 77900, GalleryKeep1 = 79500;
    constexpr int32 FallenPiers[] = {3, 6};
    // The mouth is a gap at the foot of a pile of fallen blocks heaped against the -S stair's masonry (its open face at
    // MouthS), PileS1 out from it, where the flight is 2.2 m overhead; the passage runs through the pile, under the stair
    // and through the -S wall into the rock. (The aisle's rupture there, SanctumRifts[0], lies buried under the pile.)
    constexpr float MouthS = RS0 + TempleWind::StairW, PileS1 = MouthS + 170;
    constexpr uint32 OpenSanctumRifts = 0xE;   // bit K: TempleWind::SanctumRifts[K] still open
    // The lane's corners, from where he comes to (the first: the +S aisle by the dais) across the hall to the crevice's mouth.
    struct FPt { float S, T; };
    constexpr FPt Lane[] = {{79950, -7650}, {79950, -6250}, {PileS1 + 100, -6250}, {PileS1 + 100, -5950}};
    constexpr float LaneHalf = 110;
    // The crevice: its mouth in the -S wall at CrawlT, along -S to the bend at CrawlBendS, then along -T to its end.
    // 80 cm wide and 44 cm high (he is 65 cm standing, 33 on all fours).
    constexpr float CrawlT = -5950, CrawlHalf = 40, CrawlH = 44, CrawlBendS = RS0 - 540, CrawlEndT = CrawlT - 390;
    // The stairs broken (2026-11-04, user: "blocked off stair cases ... close access with new Astral sea areas"): the
    // middle of each flight, 3.1 m from 4.9 to 6.5 m up (StairMass's 7th and 8th pieces), fell with its masonry into an
    // Astral rupture torn in the floor under it. The flight below ends at the break; the one above is out of reach.
    constexpr float StairPiece = (TempleWind::GalleryT - TempleWind::StairFoot) / 16;
    constexpr float StairBreak0 = TempleWind::StairFoot + 6 * StairPiece, StairBreak1 = TempleWind::StairFoot + 8 * StairPiece;
    // The fall's own Astral ruptures (the sanctum's pool and aisle ruptures are there too: ChultTempleSanctumHoleAt):
    // two in the aisles, then the two under the stairs' breaks (their walls' side clipped by the walls).
    struct FRift { float S, T, RS, RT; };
    constexpr FRift Rifts[] = {{79250, -7000, 130, 110}, {77480, -6950, 110, 150},
        {RS0 + 80, (StairBreak0 + StairBreak1) * .5f, 190, 200}, {RS1 - 80, (StairBreak0 + StairBreak1) * .5f, 190, 200}};
    // The sanctum's own rupture the rift-fall check walks into: the +S aisle's, past the lane's first leg.
    constexpr float FallRiftS = 79960, FallRiftT = -5600;
    // Where the vault has come down: ragged ellipses of the ceiling fallen in, earth and roots over each, its fall heaped
    // under it: over the gallery's middle and the pool before it (what brought the gallery down), the nave over the buried
    // sea tear, over the crevice's pile, before the dais. Their earth cavities (CaveRoom x the radii) never overlap, nor
    // reach the colonnades' beams. Every piece of rubble lies within reach of one of these, a fallen pier, the gallery's
    // fall or a stair's break (ChultTempleRubbleSourced).
    struct FCave { float S, T, RS, RT; };
    constexpr FCave Caves[] = {{78700, -4200, 630, 520}, {78550, -6700, 470, 560}, {77500, -6250, 180, 330}, {78400, -8150, 370, 290}};
    constexpr float CaveRoom = 1.3f;
}

// The collapsed vault's hole a frame point (S, T) is under, or -1. Grow widens each.
int32 ChultTempleRubbleCaveAt(float S, float T, float Grow = 0);
// Whether a frame point is somewhere rubble could have come down (user 2026-11-04: no unexplained piles): under or
// beside a hole in the vault, round a fallen pier's stump and along where it fell, the gallery's fall into the pool and
// its far rim, a stair's break, the crevice's pile.
bool ChultTempleRubbleSourced(const FVector2D& Frame);
// Blocks lying anywhere else (0 expected).
int32 ChultTempleRubbleUnsourced();
// What the rubble map's floor is at a frame point: 0 floor, 1 the Astral (the sanctum's pool and ruptures, and the
// fall's own). Grow widens each rupture.
int32 ChultTempleRubbleHoleAt(const FVector2D& Frame, float Grow = 0);
// What the fallen hall's own masonry puts under a frame point (frame Z): the floor, the dais, the stairs' treads, the
// gallery's standing runs.
float ChultTempleRubbleBaseAt(const FVector2D& Frame);
// Distance (frame, cm) from the lane's centre line.
float ChultTempleRubbleLaneDistance(const FVector2D& Frame);
// The rubble: turned boxes (frame centre, half-size, turn) and fallen column drums (ends, radius). Each is solid; all
// keep off the lane, the ruptures, the hall's fittings and the crevice's mouth.
struct FTempleRubbleBlock { FVector Centre, Half; FRotator Turn; bool bMoss = false; };
struct FTempleRubbleDrum { FVector A, B; float Radius = 0; };
const TArray<FTempleRubbleBlock>& ChultTempleRubbleBlocks();
// Blocks resting on nothing: every point of the underside more than Tolerance cm above what is under it. 0 expected.
int32 ChultTempleRubbleFloating(float Tolerance = 8.f);
const TArray<FTempleRubbleDrum>& ChultTempleRubbleDrums();
// The hall's braziers (the sanctum's: kept clear of rubble) and the pots by the lane (two cigarettes each: the Fireball
// halved his Sanity), frame, on the floor.
const TArray<FVector2D>& ChultTempleRubbleBraziers();
const TArray<FVector2D>& ChultTempleRubbleJars();
// The crevice's straight runs (through the pile, along -S, along -T) as frame boxes (S in X, T in Y) and the crawl spaces (frame, with Z) that put him
// on all fours: the runs, and 80 cm of the chamber's floor before the mouth.
void ChultTempleRubbleCrawlRuns(TArray<FBox2D>& Out);
void ChultTempleRubbleCrawlSpaces(TArray<FBox>& Out);
/** In the rubble map or its crevice (world): below the world's fall-death line like the sanctum, and not off the world. */
bool InChultTempleRubble(const FVector& World);
/** Within Margin of the crevice's end (world): the escape's handoff point, for later. */
bool AtChultTempleCreviceEnd(const FVector& World, float Margin = 60);

class AActor;
class AChuckCharacter;
// -ChuckTempleRubbleTest (in a Chult run): stills, then the real character on the lane, through the crevice, into a rupture.
void ReviewChultTempleRubble(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
