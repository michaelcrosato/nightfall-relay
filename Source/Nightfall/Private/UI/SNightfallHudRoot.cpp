// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallHudRoot.h"

#include "Nightfall.h"
#include "NightfallGameplayTags.h"
#include "UI/SNightfallEnvironmentPanel.h"
#include "UI/SNightfallReticle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

void SNightfallHudRoot::Construct(const FArguments& InArgs)
{
	WeakWorld = InArgs._World;

	ChildSlot
	[
		SNew(SOverlay)

		// --- Performance panel, top left ---------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Left).VAlign(VAlign_Top)
		.Padding(FMargin(24.0f, 24.0f, 0.0f, 0.0f))
		[
			SNew(SNightfallPerfHud)
			.World(WeakWorld)
			.Mode_Lambda([this]() { return PerfHudMode; })
		]

		// --- Date, clock and temperature, then plugin HUD panels under them, top right ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Right).VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 24.0f, 24.0f, 0.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				SNew(SNightfallEnvironmentPanel).World(WeakWorld)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SAssignNew(HudLayerBox, SVerticalBox)
			]
		]

		// --- Reticle and prompt, centre ------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SNightfallReticle).World(WeakWorld)
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SAssignNew(ReticleLayerBox, SVerticalBox)
			]
		]

		// --- Plugin objective panels, bottom centre --------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 48.0f))
		[
			SAssignNew(ObjectiveLayerBox, SVerticalBox)
		]
	];
}

TSharedPtr<SVerticalBox> SNightfallHudRoot::FindLayerBox(FGameplayTag Layer) const
{
	if (Layer == NightfallTags::UI_Layer_Hud.GetTag())
	{
		return HudLayerBox;
	}
	if (Layer == NightfallTags::UI_Layer_Objective.GetTag())
	{
		return ObjectiveLayerBox;
	}
	if (Layer == NightfallTags::UI_Layer_Reticle.GetTag())
	{
		return ReticleLayerBox;
	}
	return nullptr;
}

bool SNightfallHudRoot::AddPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel)
{
	const TSharedPtr<SVerticalBox> Box = FindLayerBox(Layer);
	if (!Box.IsValid())
	{
		UE_LOG(LogNightfall, Warning,
			TEXT("No HUD layer '%s'. Valid layers are the children of Nightfall.UI.Layer."),
			*Layer.ToString());
		return false;
	}

	Box->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 4.0f)
	[
		Panel
	];
	return true;
}

void SNightfallHudRoot::RemovePanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel)
{
	if (const TSharedPtr<SVerticalBox> Box = FindLayerBox(Layer))
	{
		Box->RemoveSlot(Panel);
	}
}

void SNightfallHudRoot::CyclePerfHudMode()
{
	const uint8 Next = (static_cast<uint8>(PerfHudMode) + 1) % static_cast<uint8>(ENightfallPerfHudMode::Count);
	PerfHudMode = static_cast<ENightfallPerfHudMode>(Next);
}
