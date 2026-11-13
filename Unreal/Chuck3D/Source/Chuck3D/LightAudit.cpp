#include "LightAudit.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GroomComponent.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"

int32 ChuckLightAudit::Run(UWorld* World, const TCHAR* Where, float Threshold)
{
    if (!World) return 0;
    // With voxels on (near the temple heroes, ChuckGroom::Want) the strands shadow themselves: nothing to flag.
    if (const IConsoleVariable* Vox = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Voxelization")); Vox && Vox->GetInt())
    {
        UE_LOG(LogTemp, Display, TEXT("CHUCK_LIGHT_AUDIT_DONE where=%s voxels=1 risky=0 (strands self-shadowed here)"), Where);
        return 0;
    }
    TArray<const UGroomComponent*> Hair;
    for (TObjectIterator<UGroomComponent> It; It; ++It)
        if (It->GetWorld() == World && It->IsRegistered() && It->IsVisible()) Hair.Add(*It);
    int32 Lights = 0, Risky = 0;
    for (TObjectIterator<UPointLightComponent> It; It; ++It)   // spot lights are point lights too
    {
        const UPointLightComponent* L = *It;
        if (L->GetWorld() != World || !L->IsRegistered() || !L->IsVisible() || L->Intensity <= 1.f) continue;
        ++Lights;
        if (L->CastShadows) continue;
        const FVector At = L->GetComponentLocation();
        // Roughly the light falling on the hair: the intensity over distance squared (metres, at least half a
        // metre), times UE's inverse-square window, which fades it to nothing at the attenuation radius.
        const UGroomComponent* Near = nullptr; float Best = 0.f, BestD = 0.f;
        for (const UGroomComponent* G : Hair)
        {
            const float D = FMath::Max(0.f, FVector::Dist(At, G->Bounds.Origin) - G->Bounds.SphereRadius);
            if (D >= L->AttenuationRadius) continue;
            const float Window = FMath::Square(FMath::Clamp(1.f - FMath::Pow(D / L->AttenuationRadius, 4.f), 0.f, 1.f));
            const float Lux = L->Intensity * Window / FMath::Square(FMath::Max(.5f, D / 100.f));
            if (Lux > Best) { Best = Lux; BestD = D; Near = G; }
        }
        // Not calibrated into a pass/fail: by this estimate the sanctum torches on the heroes (reported) were
        // about 270 and Waterdeep's lamps on the keeper (never reported) about 150. What made the reported cases
        // read badly was more the light behind the head against a dark room, or a strong colour (purple). Read the
        // list as candidates to look at, brightest first.
        if (!Near || Best < Threshold) continue;
        ++Risky;
        const AActor* LightOwner = L->GetOwner();
        const AActor* HairOwner = Near->GetOwner();
        UE_LOG(LogTemp, Warning, TEXT("CHUCK_LIGHT_AUDIT %s light=%s.%s at=%s radius=%.0f intensity=%.1f colour=%s hair=%s.%s distance=%.0f lux=%.0f"),
            Where, LightOwner ? *LightOwner->GetName() : TEXT("-"), *L->GetName(), *At.ToCompactString(), L->AttenuationRadius, L->Intensity,
            *L->GetLightColor().ToFColor(true).ToHex(), HairOwner ? *HairOwner->GetName() : TEXT("-"), *Near->GetName(), BestD, Best);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_LIGHT_AUDIT_DONE where=%s lights=%d grooms=%d risky=%d"), Where, Lights, Hair.Num(), Risky);
    return Risky;
}
