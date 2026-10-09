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

    struct FCue
    {
        FString Text;
        int32 Weight = 0;
    };

    inline TArray<FCue> Group(const TArray<FString>& Parts, float Seconds)
    {
        constexpr float MinReadableSeconds = 2.25f;
        int32 Total = 0;
        for (const FString& Part : Parts) Total += Part.Len();
        const float PerCharacter = Seconds / FMath::Max(1, Total);
        TArray<FCue> Cues;
        for (int32 I = 0; I < Parts.Num(); ++I)
        {
            FCue Cue{Parts[I], Parts[I].Len()};
            // Show a quick phrase above its successor for their combined time.
            // Keep the original weights, so later sentences keep their timing.
            while (Cue.Weight * PerCharacter < MinReadableSeconds && I + 1 < Parts.Num())
            {
                Cue.Text += TEXT("\n") + Parts[++I];
                Cue.Weight += Parts[I].Len();
            }
            // A short final phrase has no successor: retain it with the previous cue.
            if (Cue.Weight * PerCharacter < MinReadableSeconds && !Cues.IsEmpty())
            {
                Cues.Last().Text += TEXT("\n") + Cue.Text;
                Cues.Last().Weight += Cue.Weight;
            }
            else Cues.Add(MoveTemp(Cue));
        }
        return Cues;
    }

    inline FString At(const FString& Whole, float Seconds, float Elapsed, float OpeningPause = 0.f)
    {
        if (Elapsed < OpeningPause) return TEXT("... ");
        const TArray<FCue> Cues = Group(Split(Whole), FMath::Max(.1f, Seconds - OpeningPause));
        int32 Total = 0;
        for (const FCue& Cue : Cues) Total += Cue.Weight;
        float Into = FMath::Clamp((Elapsed - OpeningPause) / FMath::Max(.1f, Seconds - OpeningPause), 0.f, 1.f) * Total;
        for (const FCue& Cue : Cues)
            if ((Into -= Cue.Weight) < 0.f) return Cue.Text;
        return Cues.IsEmpty() ? Whole : Cues.Last().Text;
    }
}
