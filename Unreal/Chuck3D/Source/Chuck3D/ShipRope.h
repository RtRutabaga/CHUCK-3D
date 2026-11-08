#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Rope (2026-11-07; user: "the ropes that the crew pull on during storm sequence looks like sharp angles that are completely
 * unrealistic, i want realistic rope bends for how they're holding it").
 *
 * A rope is a tube along a path that bends the way rope does: round a sheave, a belaying pin or a fist in a smooth arc (Bent),
 * hanging slack from where it is held down to the deck (Droop), lying along the deck and flaked down in a coil (Coil). Its
 * surface is M_Rope (ambientCG Rope001, CC0: SourceAssets/Surfaces/Rope): the scan's strands run 8 to a repeat along U and 7
 * down V, so with the rope's length along U (LayRepeat cm a repeat) and 3/7 of V round it, three strands lie round the rope
 * in a lay of 9.75 cm, as on hawser-laid hemp. UOffset runs the strands along the rope as it is hauled through.
 *
 * Meant for every rope that moves, not only the storm's lines (the fore ratlines torn loose in the storm, later).
 */
namespace ShipRope
{
    constexpr float LayRepeat = 26.f;          // cm of rope a texture repeat along it
    constexpr float AroundRepeat = 3.f / 7.f;   // of the texture's V round it (three strands)

    /** A point the rope passes and its bend there: the radius of the arc it takes round it (0: a sharp turn, at an end).
     *  Hold: the furthest the arc may pass from the point (a fist round it: the rope stays in the hand however sharply it
     *  turns there; 0 no limit). */
    struct FKnot { FVector P; float Bend = 0.f; float Hold = 0.f; };

    /** The path through Knots, each corner rounded to its bend radius (less where its neighbours are close). */
    void Bent(const TArray<FKnot>& Knots, TArray<FVector>& Out);
    /** The centre of the arc Bent takes round Corner coming from Prev and going on to Next (a sheave's axle). */
    FVector ArcCentre(const FVector& Prev, const FVector& Corner, const FVector& Next, float Bend);

    /** Over a sheave (centre C, radius R, turning on Axle) from In to Out: appends the arc the rope lies in on it, from where
     *  it comes on (a tangent from In) to where it leaves for Out. In and Out are taken in the sheave's plane. */
    void Sheave(const FVector& In, const FVector& C, const FVector& Axle, float R, const FVector& Out, TArray<FVector>& Path);

    /** Slack rope from From (where it is let go, leaving it along Leaving) down to lie on the deck at height Z heading Way
     *  (horizontal): it carries on a little the way it left, falls, and settles flat. Appends from just after From to the
     *  point where it touches down; Touch is that point. */
    void Droop(const FVector& From, const FVector& Leaving, const FVector& Way, float Z, TArray<FVector>& Out, FVector& Touch);

    /** A coil flaked down flat on the deck: starting at Start heading Way (along its outer turn), turning toward Side and in
     *  from Outer to Inner radius, the turns side by side. Appends from just after Start. */
    void Coil(const FVector& Start, const FVector& Way, const FVector& Side, float Outer, float Inner, TArray<FVector>& Out);

    /** Mesh arrays for a procedural mesh section. */
    struct FMesh
    {
        TArray<FVector> V, N;
        TArray<int32> T;
        TArray<FVector2D> UV;
        TArray<FProcMeshTangent> Tan;
        void Reset() { V.Reset(); N.Reset(); T.Reset(); UV.Reset(); Tan.Reset(); }
        void Into(class UProceduralMeshComponent* Mesh, int32 Section, bool bUpdate) const;
    };

    /** The rope's tube along Path (radius R, Sides round), its ends capped; UOffset (cm) runs the strands along it. */
    void Tube(FMesh& M, const TArray<FVector>& Path, float R, float UOffset = 0.f, int32 Sides = 10);
    /** A wooden block's shell: an ellipsoid at C, Radii along Long, across its cheeks' plane and through its cheeks (Cheek). */
    void Shell(FMesh& M, const FVector& C, const FVector& Long, const FVector& Cheek, const FVector& Radii);

    float Length(const TArray<FVector>& Path);
}
