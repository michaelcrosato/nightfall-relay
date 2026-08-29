// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallInteractionSubsystem.h"

#include "Engine/World.h"
#include "NightfallInteractableComponent.h"
#include "NightfallStats.h"

bool UNightfallInteractionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UNightfallInteractionSubsystem* UNightfallInteractionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UNightfallInteractionSubsystem>() : nullptr;
}

void UNightfallInteractionSubsystem::RegisterInteractable(UNightfallInteractableComponent* Interactable)
{
	if (Interactable)
	{
		Registered.AddUnique(Interactable);
	}
}

void UNightfallInteractionSubsystem::UnregisterInteractable(UNightfallInteractableComponent* Interactable)
{
	if (Interactable)
	{
		Registered.RemoveSingleSwap(Interactable, EAllowShrinking::No);
	}
}

TArray<UNightfallInteractableComponent*> UNightfallInteractionSubsystem::QueryInteractables(FGameplayTag RequiredTag, FVector Origin, float Radius) const
{
	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Interaction);

	const float RadiusSquared = Radius * Radius;
	const bool bUnbounded = Radius <= 0.0f;

	// Gather with distances so the sort does not have to recompute them.
	TArray<TPair<double, UNightfallInteractableComponent*>> Scored;
	Scored.Reserve(Registered.Num());

	for (const TWeakObjectPtr<UNightfallInteractableComponent>& Weak : Registered)
	{
		UNightfallInteractableComponent* Interactable = Weak.Get();
		if (!Interactable)
		{
			continue;
		}

		if (RequiredTag.IsValid() && !Interactable->InteractionTags.HasTag(RequiredTag))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(Interactable->GetComponentLocation(), Origin);
		if (!bUnbounded && DistanceSquared > RadiusSquared)
		{
			continue;
		}

		Scored.Emplace(DistanceSquared, Interactable);
	}

	Scored.Sort([](const TPair<double, UNightfallInteractableComponent*>& A, const TPair<double, UNightfallInteractableComponent*>& B)
	{
		return A.Key < B.Key;
	});

	TArray<UNightfallInteractableComponent*> Result;
	Result.Reserve(Scored.Num());
	for (const TPair<double, UNightfallInteractableComponent*>& Entry : Scored)
	{
		Result.Add(Entry.Value);
	}
	return Result;
}

UNightfallInteractableComponent* UNightfallInteractionSubsystem::FindNearest(FGameplayTag RequiredTag, FVector Origin, float Radius) const
{
	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Interaction);

	const float RadiusSquared = Radius * Radius;
	const bool bUnbounded = Radius <= 0.0f;

	UNightfallInteractableComponent* Best = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();

	for (const TWeakObjectPtr<UNightfallInteractableComponent>& Weak : Registered)
	{
		UNightfallInteractableComponent* Interactable = Weak.Get();
		if (!Interactable)
		{
			continue;
		}

		if (RequiredTag.IsValid() && !Interactable->InteractionTags.HasTag(RequiredTag))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(Interactable->GetComponentLocation(), Origin);
		if (!bUnbounded && DistanceSquared > RadiusSquared)
		{
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Interactable;
		}
	}

	return Best;
}
