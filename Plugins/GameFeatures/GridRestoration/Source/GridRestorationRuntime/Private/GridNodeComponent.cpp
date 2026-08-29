// Copyright Nightfall Relay. All Rights Reserved.

#include "GridNodeComponent.h"

#include "GridCellComponent.h"
#include "GridRestorationRuntime.h"
#include "GridRestorationSubsystem.h"
#include "NightfallCarryComponent.h"
#include "NightfallInteractableComponent.h"
#include "NightfallRelayPylon.h"

#define LOCTEXT_NAMESPACE "GridRestoration"

UGridNodeComponent::UGridNodeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGridNodeComponent::BeginPlay()
{
	Super::BeginPlay();

	Pylon = Cast<ANightfallRelayPylon>(GetOwner());
	if (!Pylon.IsValid())
	{
		UE_LOG(LogGridRestoration, Warning,
			TEXT("UGridNodeComponent landed on '%s', which is not a relay pylon."), *GetNameSafe(GetOwner()));
		return;
	}

	if (UNightfallInteractableComponent* Interactable = Pylon->GetInteractable())
	{
		Interactable->OnInteracted.AddDynamic(this, &UGridNodeComponent::HandleInteracted);
		Interactable->OnFocusChanged.AddDynamic(this, &UGridNodeComponent::HandleFocusChanged);
	}
	Pylon->OnChargeChanged.AddDynamic(this, &UGridNodeComponent::HandleChargeChanged);

	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->RegisterNode(this);
	}
}

void UGridNodeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANightfallRelayPylon* Owner = Pylon.Get())
	{
		if (UNightfallInteractableComponent* Interactable = Owner->GetInteractable())
		{
			Interactable->OnInteracted.RemoveDynamic(this, &UGridNodeComponent::HandleInteracted);
			Interactable->OnFocusChanged.RemoveDynamic(this, &UGridNodeComponent::HandleFocusChanged);
		}
		Owner->OnChargeChanged.RemoveDynamic(this, &UGridNodeComponent::HandleChargeChanged);
	}

	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->UnregisterNode(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UGridNodeComponent::IsOnline() const
{
	const ANightfallRelayPylon* Owner = Pylon.Get();
	return Owner && Owner->IsOnline();
}

float UGridNodeComponent::GetChargeLevel() const
{
	const ANightfallRelayPylon* Owner = Pylon.Get();
	return Owner ? Owner->GetChargeLevel() : 0.0f;
}

void UGridNodeComponent::DrainCharge(float Amount)
{
	if (ANightfallRelayPylon* Owner = Pylon.Get())
	{
		Owner->SetChargeLevel(Owner->GetChargeLevel() - Amount);
	}
}

void UGridNodeComponent::HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor)
{
	ANightfallRelayPylon* Owner = Pylon.Get();
	if (!Owner || !Interactor || Owner->IsOnline())
	{
		return;
	}

	// The rule of the loop, in one place: charge only arrives in the player's hands.
	UNightfallCarryComponent* Carry = Interactor->FindComponentByClass<UNightfallCarryComponent>();
	AActor* Carried = Carry ? Carry->GetCarriedActor() : nullptr;
	UGridCellComponent* Cell = Carried ? Carried->FindComponentByClass<UGridCellComponent>() : nullptr;

	if (!Cell || !Cell->IsLive())
	{
		return;
	}

	Owner->AddCharge(Cell->ChargeUnits);
	Cell->Consume();

	// Let go of the husk so the player's hands are free for the next one.
	Carry->Release();

	RefreshPrompt(Interactor);
}

void UGridNodeComponent::HandleFocusChanged(UNightfallInteractableComponent* Source, bool bFocused)
{
	if (!bFocused)
	{
		return;
	}

	// Recompute the prompt at the moment the player looks at the pylon, rather than
	// polling it every frame.
	const UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	RefreshPrompt(Controller ? Controller->GetPawn() : nullptr);
}

void UGridNodeComponent::HandleChargeChanged(ANightfallRelayPylon* InPylon, float ChargeLevel)
{
	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->NotifyProgressChanged();
	}
}

void UGridNodeComponent::RefreshPrompt(AActor* Interactor)
{
	ANightfallRelayPylon* Owner = Pylon.Get();
	UNightfallInteractableComponent* Interactable = Owner ? Owner->GetInteractable() : nullptr;
	if (!Interactable)
	{
		return;
	}

	if (Owner->IsOnline())
	{
		Interactable->Verb = LOCTEXT("PylonOnlineVerb", "Online:");
		Interactable->bInteractionEnabled = false;
		return;
	}

	UNightfallCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UNightfallCarryComponent>() : nullptr;
	AActor* Carried = Carry ? Carry->GetCarriedActor() : nullptr;
	const UGridCellComponent* Cell = Carried ? Carried->FindComponentByClass<UGridCellComponent>() : nullptr;

	const bool bHasLiveCell = Cell && Cell->IsLive();
	Interactable->bInteractionEnabled = bHasLiveCell;
	Interactable->Verb = bHasLiveCell
		? LOCTEXT("PylonInsertVerb", "Insert Cell:")
		: LOCTEXT("PylonNeedsCellVerb", "Needs a Power Cell:");
}

#undef LOCTEXT_NAMESPACE
