// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "NightfallInteractorComponent.generated.h"

class UNightfallInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightfallFocusSignature, UNightfallInteractableComponent*, NewFocus);

/**
 * The player's end of the interaction handshake.
 *
 * Sweeps a short sphere down the view ray every frame, keeps at most one focused
 * interactable, and runs press or hold interactions against it. A sphere rather than a
 * line because the targets here are thin: pylon collars, cell handles, door panels.
 */
UCLASS(ClassGroup = (Nightfall), meta = (BlueprintSpawnableComponent))
class NIGHTFALL_API UNightfallInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightfallInteractorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Begin an interaction. Instant interactables fire here; hold ones start charging. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Interaction")
	void BeginInteract();

	/** Release. Cancels an incomplete hold. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Interaction")
	void EndInteract();

	UFUNCTION(BlueprintPure, Category = "Nightfall|Interaction")
	UNightfallInteractableComponent* GetFocus() const { return Focus.Get(); }

	/** Hold completion in the range [0,1]. Zero when nothing is being held. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Interaction")
	float GetHoldProgress() const;

	/** Fires whenever the focused interactable changes, including to null. */
	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Interaction")
	FNightfallFocusSignature OnFocusChanged;

private:
	void UpdateFocus();
	void SetFocus(UNightfallInteractableComponent* NewFocus);
	void CompleteInteract();

	TWeakObjectPtr<UNightfallInteractableComponent> Focus;

	/** Seconds the interact input has been held against the current focus. */
	float HoldElapsed = 0.0f;

	bool bInputHeld = false;
};
