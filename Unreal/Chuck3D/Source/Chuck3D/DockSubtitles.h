#pragma once

#include "CoreMinimal.h"

namespace DockSubtitles
{
    // Keep the user's words, but reveal one sentence at a time. Very long
    // sentences divide at a word boundary so they never become a paragraph.
    inline TArray<FString> Split(const FString& Whole)
    {
        TArray<FString> Parts;
        FString Part;
        for (int32 I = 0; I < Whole.Len(); ++I)
        {
            Part.AppendChar(Whole[I]);
            const bool End = FString(TEXT(".!?\u2026")).Contains(FString::Chr(Whole[I]))
                && (I + 1 == Whole.Len() || FChar::IsWhitespace(Whole[I + 1]));
            if (End || I + 1 == Whole.Len())
            {
                Part.TrimStartAndEndInline();
                while (Part.Len() > 110)
                {
                    int32 Cut = 110;
                    while (Cut > 0 && !FChar::IsWhitespace(Part[Cut])) --Cut;
                    if (!Cut) Cut = 110;
                    Parts.Add(Part.Left(Cut));
                    Part = Part.Mid(Cut).TrimStartAndEnd();
                }
                if (!Part.IsEmpty()) Parts.Add(Part);
                Part.Reset();
            }
        }
        return Parts;
    }

    inline FString At(const FString& Whole, float Seconds, float Elapsed, float OpeningPause = 0.f)
    {
        if (Elapsed < OpeningPause) return TEXT("... ");
        const TArray<FString> Parts = Split(Whole);
        int32 Total = 0;
        for (const FString& Part : Parts) Total += Part.Len();
        float Into = FMath::Clamp((Elapsed - OpeningPause) / FMath::Max(.1f, Seconds - OpeningPause), 0.f, 1.f) * Total;
        for (const FString& Part : Parts)
            if ((Into -= Part.Len()) < 0.f) return Part;
        return Parts.IsEmpty() ? Whole : Parts.Last();
    }
}
