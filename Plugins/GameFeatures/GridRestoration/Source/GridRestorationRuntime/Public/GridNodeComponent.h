// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GridNodeComponent.generated.h"

class ANightfallRelayPylon;
class UNightfallInteractableComponent;

/**
 * Turns a relay pylon into something the loop cares about.
 *
 * Added to every ANightfallRelayPylon by this feature's GameFeatureData. The pylon already
 * knew how to hold charge and light up; this component supplies the rule that charge
 * arrives by hand, in cells, and only when the player is carrying one.
 *
 * Remove the feature and pylons go back to being scenery that can be charged by anything
 * else that asks. Nothing in the core project changes either way.
 */
UCLASS(ClassGroup = (GridRestoration), meta = (BlueprintSpawnableComponent))
class GRIDRESTORATIONRUNTIME_API UGridNodeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGridNodeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The pylon this component drives, or null if it landed on something else. */
	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	ANightfallRelayPylon* GetPylon() const { return Pylon.Get(); }

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	bool IsOnline() const;

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	float GetChargeLevel() const;

	/** Take charge out of this node. Used by the alarm drain. */
	void DrainCharge(float Amount);

private:
	UFUNCTION()
	void HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor);

	UFUNCTION()
	void HandleFocusChanged(UNightfallInteractableComponent* Source, bool bFocused);

	UFUNCTION()
	void HandleChargeChanged(ANightfallRelayPylon* InPylon, float ChargeLevel);

	/** Update the prompt to say what the player can actually do right now. */
	void RefreshPrompt(AActor* Interactor);

	TWeakObjectPtr<ANightfallRelayPylon> Pylon;
};
