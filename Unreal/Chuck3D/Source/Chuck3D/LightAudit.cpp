#include "LightAudit.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GroomComponent.h"
#include "UObject/UObjectIterator.h"

int32 ChuckLightAudit::Run(UWorld* World, const TCHAR* Where)
{
    if (!World) return 0;
    TArray<const UGroomComponent*> Hair;
    for (TObjectIterator<UGroomComponent> It; It; ++It)
        if (It->GetWorld() == World && It->IsRegistered() && It->IsVisible()) Hair.Add(*It);
    int32 Lights = 0, Risky = 0;
    for (TObjectIterator<UPointLightComponent> It; It; ++It)   // spot lights are point lights too
    {
        const UPointLightComponent* L = *It;
        if (L->GetWorld() != World || !L->IsRegistered() || !L->IsVisible() || L->Intensity <= 0.f) continue;
        ++Lights;
        if (L->CastShadows) continue;
        const FVector At = L->GetComponentLocation();
        const UGroomComponent* Near = nullptr; float Best = L->AttenuationRadius;
        for (const UGroomComponent* G : Hair)
        {
            const float D = FVector::Dist(At, G->Bounds.Origin) - G->Bounds.SphereRadius;
            if (D < Best) { Best = D; Near = G; }
        }
        if (!Near) continue;
        ++Risky;
        const AActor* LightOwner = L->GetOwner();
        const AActor* HairOwner = Near->GetOwner();
        UE_LOG(LogTemp, Warning, TEXT("CHUCK_LIGHT_AUDIT %s light=%s.%s at=%s radius=%.0f intensity=%.1f colour=%s hair=%s.%s distance=%.0f"),
            Where, LightOwner ? *LightOwner->GetName() : TEXT("-"), *L->GetName(), *At.ToCompactString(), L->AttenuationRadius, L->Intensity,
            *L->GetLightColor().ToFColor(true).ToHex(), HairOwner ? *HairOwner->GetName() : TEXT("-"), *Near->GetName(), FMath::Max(0.f, Best));
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_LIGHT_AUDIT_DONE where=%s lights=%d grooms=%d risky=%d"), Where, Lights, Hair.Num(), Risky);
    return Risky;
}
