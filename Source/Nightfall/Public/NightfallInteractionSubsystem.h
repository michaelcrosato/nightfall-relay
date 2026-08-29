// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightfallInteractionSubsystem.generated.h"

class UNightfallInteractableComponent;

/**
 * Registry of every live interactable in the world.
 *
 * The player's own focus trace does not need this - it uses a sweep. The registry exists
 * so that systems which need to reason about interactables in bulk can do so without
 * iterating actors: the survey scanner plugin asks "every power node within 60 metres"
 * and gets an answer in one pass over a flat array.
 */
UCLASS()
class NIGHTFALL_API UNightfallInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Convenience accessor from any object with a world. Returns null outside a game world. */
	static UNightfallInteractionSubsystem* Get(const UObject* WorldContextObject);

	void RegisterInteractable(UNightfallInteractableComponent* Interactable);
	void UnregisterInteractable(UNightfallInteractableComponent* Interactable);

	/**
	 * Every registered interactable carrying RequiredTag within Radius of Origin, nearest
	 * first. A zero radius means unbounded; an empty tag matches everything.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Interaction")
	TArray<UNightfallInteractableComponent*> QueryInteractables(FGameplayTag RequiredTag, FVector Origin, float Radius) const;

	/** The nearest interactable carrying RequiredTag, or null. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Interaction")
	UNightfallInteractableComponent* FindNearest(FGameplayTag RequiredTag, FVector Origin, float Radius) const;

	UFUNCTION(BlueprintPure, Category = "Nightfall|Interaction")
	int32 GetRegisteredCount() const { return Registered.Num(); }

private:
	/** Weak, because interactables are destroyed with their actors as cells stream out. */
	TArray<TWeakObjectPtr<UNightfallInteractableComponent>> Registered;
};
