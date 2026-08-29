// Copyright Nightfall Relay. All Rights Reserved.

#include "GridCellComponent.h"

#include "GridRestorationRuntime.h"
#include "GridRestorationSubsystem.h"
#include "GridRestorationTags.h"
#include "NightfallInteractableComponent.h"
#include "NightfallPhysicsProp.h"

#define LOCTEXT_NAMESPACE "GridRestoration"

namespace
{
	/** Live cells glow this colour. */
	const FLinearColor LiveCellColor(0.16f, 0.86f, 1.0f);

	/** Spent husks go dull amber so a delivered cell still reads on the ground. */
	const FLinearColor SpentCellColor(0.55f, 0.30f, 0.10f);
}

UGridCellComponent::UGridCellComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGridCellComponent::BeginPlay()
{
	Super::BeginPlay();

	Prop = Cast<ANightfallPhysicsProp>(GetOwner());
	if (!Prop.IsValid())
	{
		// The feature targets props by class, so this should not happen; if it ever does,
		// say so rather than sitting there inert.
		UE_LOG(LogGridRestoration, Warning,
			TEXT("UGridCellComponent landed on '%s', which is not a physics prop."), *GetNameSafe(GetOwner()));
		return;
	}

	// Only placements tagged as cells become cells. Everything else is rubble and is left
	// exactly as the level authored it.
	bIsCell = Prop->PropTags.HasTag(GridRestorationTags::Grid_PowerCell);
	if (!bIsCell)
	{
		return;
	}

	// A placement can also be authored as already spent.
	bSpent = bSpent || Prop->PropTags.HasTag(GridRestorationTags::Grid_SpentCell);

	ApplyPresentation();

	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->RegisterCell(this);
	}
}

void UGridCellComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsCell)
	{
		if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
		{
			Subsystem->UnregisterCell(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UGridCellComponent::Consume()
{
	if (!IsLive())
	{
		return;
	}

	bSpent = true;
	ApplyPresentation();

	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->NotifyProgressChanged();
	}
}

void UGridCellComponent::ApplyPresentation()
{
	ANightfallPhysicsProp* Owner = Prop.Get();
	if (!Owner)
	{
		return;
	}

	Owner->SetGlowColor(bSpent ? SpentCellColor : LiveCellColor);
	// A husk keeps a trace of light so it stays visible, and stays carryable so it can be
	// moved out of the way.
	Owner->SetEmissiveLevel(bSpent ? 0.12f : 1.0f);

	if (UNightfallInteractableComponent* Interactable = Owner->GetInteractable())
	{
		Interactable->DisplayName = bSpent
			? LOCTEXT("SpentCellName", "Spent Cell")
			: LOCTEXT("PowerCellName", "Power Cell");
		Interactable->Verb = LOCTEXT("CellVerb", "Pick Up");
	}
}

#undef LOCTEXT_NAMESPACE
