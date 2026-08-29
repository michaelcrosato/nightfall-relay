// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallInteractableComponent.h"

#include "NightfallGameplayTags.h"
#include "NightfallInteractionSubsystem.h"
#include "NightfallRuntimeSettings.h"

#define LOCTEXT_NAMESPACE "Nightfall"

UNightfallInteractableComponent::UNightfallInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InteractionTags.AddTag(NightfallTags::Interactable);
	DisplayName = LOCTEXT("DefaultInteractableName", "Device");
	Verb = LOCTEXT("DefaultInteractableVerb", "Use");
}

void UNightfallInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UNightfallInteractionSubsystem* Subsystem = UNightfallInteractionSubsystem::Get(this))
	{
		Subsystem->RegisterInteractable(this);
	}
}

void UNightfallInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNightfallInteractionSubsystem* Subsystem = UNightfallInteractionSubsystem::Get(this))
	{
		Subsystem->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UNightfallInteractableComponent::CanInteract(const AActor* Interactor) const
{
	return bInteractionEnabled && Interactor != nullptr && GetOwner() != nullptr;
}

bool UNightfallInteractableComponent::TryInteract(AActor* Interactor)
{
	if (!CanInteract(Interactor))
	{
		return false;
	}

	OnInteracted.Broadcast(this, Interactor);
	return true;
}

void UNightfallInteractableComponent::SetFocused(bool bNewFocused)
{
	if (bFocused == bNewFocused)
	{
		return;
	}

	bFocused = bNewFocused;
	OnFocusChanged.Broadcast(this, bFocused);
}

float UNightfallInteractableComponent::GetEffectiveInteractionDistance() const
{
	if (MaxInteractionDistanceOverride > 0.0f)
	{
		return MaxInteractionDistanceOverride;
	}
	return UNightfallRuntimeSettings::Get().InteractionTraceDistance;
}

FText UNightfallInteractableComponent::GetPromptText() const
{
	return FText::Format(LOCTEXT("InteractionPrompt", "{0} {1}"), Verb, DisplayName);
}

#undef LOCTEXT_NAMESPACE
