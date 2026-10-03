#pragma once
#include "CoreMinimal.h"
class AActor;
class UMaterialInterface;

/**
 * The sewer's way out (Claude; user 2026-10-03 sewer plan, as in the 2D game):
 * at the end of the route the stream runs through a low arch into a narrow
 * water slide that drops away into the dark. Entering it carries Chuck down
 * (AChuckCharacter's slide phases), the view fades, and he comes up hanging on
 * the end of the court pier and climbs out. The slide replaces the temporary
 * collapsed end wall; BuildDockSewer calls this with its route.
 */
void BuildDockSewerSlide(AActor* Owner, const TArray<FVector>& Route, const TArray<FVector>& Right,
    float OuterHalfWidth, float OuterHeight, float LastStreamOffset, UMaterialInterface* Stone, UMaterialInterface* Water);
/** Inside the slide tube (counts as the sewer: lighting, respawn area). */
bool IsInDockSewerSlide(const FVector& Position);
/** How far into the slide a capsule centre is (cm past the mouth), or -1 when not in it. */
float DockSewerSlideEntry(const FVector& Position);
/** The slide's floor centre line and direction at a distance along it. */
FVector DockSewerSlidePoint(float Distance);
FVector DockSewerSlideDirection(float Distance);
float DockSewerSlideLength();
/** Standing in the sewer 1.5 m before the mouth, and the way in (for tests). */
FVector DockSewerSlideApproach();
FVector DockSewerSlideInward();
/** Where he comes out: just off the outer face of the court pier's end, in the water. */
FVector DockPierExitProbe();
/** Set once Chuck has come out of the sewer by the slide (for the evening return, the closed grate, the open tavern). */
bool HasExitedDockSewer();
void MarkDockSewerExited();
