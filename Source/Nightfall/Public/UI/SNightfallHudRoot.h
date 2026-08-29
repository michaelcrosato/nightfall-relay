// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/SNightfallPerfHud.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UWorld;

/**
 * The in-game HUD.
 *
 * Owns the fixed furniture - performance panel, date/clock/temperature readout, reticle -
 * and hosts a small number of named layers that anything else can add a widget to. A Game
 * Feature Plugin builds its own Slate panel and drops it into Nightfall.UI.Layer.Objective
 * without this class ever hearing about that plugin; when the feature deactivates, the
 * panel is removed again. That is the whole extension surface for HUD content.
 */
class NIGHTFALL_API SNightfallHudRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNightfallHudRoot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Add a widget to a named layer. Unknown layers are refused and logged. */
	bool AddPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel);

	/** Remove a previously added widget. */
	void RemovePanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel);

	void SetPerfHudMode(ENightfallPerfHudMode NewMode) { PerfHudMode = NewMode; }
	ENightfallPerfHudMode GetPerfHudMode() const { return PerfHudMode; }

	/** Step through hidden, compact and full. */
	void CyclePerfHudMode();

private:
	TSharedPtr<SVerticalBox> FindLayerBox(FGameplayTag Layer) const;

	TWeakObjectPtr<UWorld> WeakWorld;

	TSharedPtr<SVerticalBox> HudLayerBox;
	TSharedPtr<SVerticalBox> ObjectiveLayerBox;
	TSharedPtr<SVerticalBox> ReticleLayerBox;

	ENightfallPerfHudMode PerfHudMode = ENightfallPerfHudMode::Compact;
};
