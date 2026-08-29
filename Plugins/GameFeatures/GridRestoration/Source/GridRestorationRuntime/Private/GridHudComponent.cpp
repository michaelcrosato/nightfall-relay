// Copyright Nightfall Relay. All Rights Reserved.

#include "GridHudComponent.h"

#include "NightfallGameplayTags.h"
#include "SGridObjectivePanel.h"
#include "UI/NightfallUISubsystem.h"

UGridHudComponent::UGridHudComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGridHudComponent::BeginPlay()
{
	Super::BeginPlay();

	UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this);
	if (!UI)
	{
		return;
	}

	Panel = SNew(SGridObjectivePanel).World(GetWorld());
	UI->RegisterHudPanel(NightfallTags::UI_Layer_Objective, Panel.ToSharedRef());
}

void UGridHudComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Panel.IsValid())
	{
		if (UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this))
		{
			UI->UnregisterHudPanel(NightfallTags::UI_Layer_Objective, Panel.ToSharedRef());
		}
		Panel.Reset();
	}

	Super::EndPlay(EndPlayReason);
}
