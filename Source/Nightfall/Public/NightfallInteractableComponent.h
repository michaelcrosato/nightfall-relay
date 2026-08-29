// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NightfallInteractableComponent.generated.h"

class UNightfallInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNightfallInteractedSignature, UNightfallInteractableComponent*, Interactable, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNightfallFocusChangedSignature, UNightfallInteractableComponent*, Interactable, bool, bFocused);

/**
 * Marks a point on an actor as something the player can look at and use.
 *
 * It is a scene component rather than an actor component on purpose: the prompt, the
 * scanner ping and the carry attach point all need a position that is not the actor origin.
 *
 * The component carries no behaviour of its own. Whoever cares - a pylon, a door, or a
 * component injected by a Game Feature Plugin - binds to OnInteracted. That is what lets
 * the grid restoration plugin make pylons interactive without the pylon knowing it exists.
 */
UCLASS(ClassGroup = (Nightfall), meta = (BlueprintSpawnableComponent), HideCategories = (Sockets, Tags, ComponentTick, Activation, Cooking))
class NIGHTFALL_API UNightfallInteractableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UNightfallInteractableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What this is, for querying. Interactables should carry Nightfall.Interactable at minimum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTagContainer InteractionTags;

	/** Name shown in the focus prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText DisplayName;

	/** Verb shown in the focus prompt, for example "Energise". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Verb;

	/** Cleared while the interactable is busy or locked, which greys the prompt out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bInteractionEnabled = true;

	/** Hold duration in seconds. Zero interacts on press. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float HoldSeconds = 0.0f;

	/** Per-instance reach override in cm. Zero uses the project default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float MaxInteractionDistanceOverride = 0.0f;

	/** Fired when an interactor completes an interaction. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FNightfallInteractedSignature OnInteracted;

	/** Fired when this becomes, or stops being, the player's focus. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FNightfallFocusChangedSignature OnFocusChanged;

	/** True when the interactor is allowed to act on this right now. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool CanInteract(const AActor* Interactor) const;

	/** Run the interaction. Returns false when CanInteract would have refused. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract(AActor* Interactor);

	/** Called by the interactor as focus enters and leaves. */
	void SetFocused(bool bNewFocused);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsFocused() const { return bFocused; }

	/** Reach for this interactable in cm, resolving the override against project settings. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetEffectiveInteractionDistance() const;

	/** Prompt line the HUD draws, for example "Energise Relay Pylon". */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetPromptText() const;

private:
	bool bFocused = false;
};
