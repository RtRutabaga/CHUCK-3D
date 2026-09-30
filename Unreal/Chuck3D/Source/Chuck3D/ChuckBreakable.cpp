#include "ChuckBreakable.h"

namespace
{
    TArray<TWeakObjectPtr<AChuckBreakable>> Registry;
}

const TArray<TWeakObjectPtr<AChuckBreakable>>& AChuckBreakable::All()
{
    Registry.RemoveAll([](const TWeakObjectPtr<AChuckBreakable>& Entry) { return !Entry.IsValid(); });
    return Registry;
}

void AChuckBreakable::BeginPlay()
{
    Super::BeginPlay();
    Registry.Add(this);
}

void AChuckBreakable::EndPlay(const EEndPlayReason::Type Reason)
{
    Registry.Remove(this);
    Super::EndPlay(Reason);
}
