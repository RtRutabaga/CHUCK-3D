#pragma once
#include "CoreMinimal.h"
#include "ChultTempleWind.h"

// The rubble map (Claude, 2026-10-26; docs/CHULT-TEMPLE-RUBBLE.md), after the 2D game's temple_rubble map: where the
// wizard's Fireball throws Chuck. A chamber beside the sanctum, at its floor's depth, whose vault has caved in: choked
// with great broken blocks of masonry and fallen columns, and torn through by the Astral (the sanctum's kind of
// rupture: fall in and he is gone). One clear lane winds from where he comes to, past torches and braziers, to the
// one way out: a crevice low in the far wall, too low to stand in, that he crawls along on all fours. The crevice's
// end (a crack of warm light) is where the escape to the ship will begin; that is not built yet.
// Frame coordinates as in ChultTempleInterior.h (S toward the temple, T across, Z above the plaza).
namespace TempleRubble
{
    constexpr float RS0 = 74300, RS1 = 76700, RT0 = -8700, RT1 = -5100;   // 24 x 36 m, beside the sanctum (HS0 77000)
    constexpr float RZ = TempleWind::HZ, RTop = RZ + 1000;                // its floor at the sanctum's; a 10 m vault
    // The lane's corners, from where he comes to (the first) to the crevice's mouth (the last, on the -S wall).
    struct FPt { float S, T; };
    constexpr FPt Lane[] = {{76250, -5600}, {75550, -5600}, {75550, -6700}, {76150, -6700}, {76150, -7700}, {75000, -7700}, {75000, -8250}, {74300, -8250}};
    constexpr float LaneHalf = 110;
    // The crevice: its mouth in the -S wall at CrawlT, along -S to the bend at CrawlBendS, then along -T to its end.
    // 80 cm wide and 44 cm high (he is 65 cm standing, 33 on all fours).
    constexpr float CrawlT = -8250, CrawlHalf = 40, CrawlH = 44, CrawlBendS = 73760, CrawlEndT = -8640;
    // The Astral ruptures in the floor: centre, radii (ragged ellipses as the sanctum's aisle ruptures).
    struct FRift { float S, T, RS, RT; };
    constexpr FRift Rifts[] = {{74800, -5700, 200, 150}, {74650, -6600, 170, 240}, {75050, -6300, 120, 160}, {74750, -7300, 230, 160},
        {76250, -6150, 140, 110}, {75600, -8250, 260, 150}, {76350, -8300, 160, 230}, {75550, -7300, 170, 120}, {74550, -7850, 110, 110}};
    // Where the vault has come down: ragged ellipses of the ceiling fallen in, earth and roots over each, its fall heaped
    // under it (four since 2026-10-26, user: "more ceiling collapses"). Their earth cavities (CaveRoom x the radii) never overlap.
    struct FCave { float S, T, RS, RT; };
    constexpr FCave Caves[] = {{75050, -6750, 480, 720}, {76050, -8150, 360, 340}, {76050, -6200, 280, 330}, {75100, -8250, 320, 260}};
    constexpr float CaveRoom = 1.3f;
}

// The collapsed vault's hole a frame point (S, T) is under, or -1. Grow widens each.
int32 ChultTempleRubbleCaveAt(float S, float T, float Grow = 0);
// What the rubble map's floor is at a frame point: 0 floor, 1 the Astral. Grow widens each rupture.
int32 ChultTempleRubbleHoleAt(const FVector2D& Frame, float Grow = 0);
// Distance (frame, cm) from the lane's centre line.
float ChultTempleRubbleLaneDistance(const FVector2D& Frame);
// The rubble: turned boxes (frame centre, half-size, turn) and fallen column drums (ends, radius). Each is solid; all
// keep off the lane, the ruptures, the braziers' spots and the crevice's mouth.
struct FTempleRubbleBlock { FVector Centre, Half; FRotator Turn; bool bMoss = false; };
struct FTempleRubbleDrum { FVector A, B; float Radius = 0; };
const TArray<FTempleRubbleBlock>& ChultTempleRubbleBlocks();
const TArray<FTempleRubbleDrum>& ChultTempleRubbleDrums();
// The braziers along the lane and the pots by it (two cigarettes each: the Fireball halved his Sanity), frame, on the floor.
const TArray<FVector2D>& ChultTempleRubbleBraziers();
const TArray<FVector2D>& ChultTempleRubbleJars();
// The crevice's two straight runs as frame boxes (S in X, T in Y) and the crawl spaces (frame, with Z) that put him
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
