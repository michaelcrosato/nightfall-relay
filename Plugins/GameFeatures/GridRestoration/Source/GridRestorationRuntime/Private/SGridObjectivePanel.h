// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UGridRestorationSubsystem;
class UWorld;

/**
 * This feature's HUD panel.
 *
 * Lives entirely inside the plugin and is handed to the core HUD through a layer tag. It
 * reads the feature's own subsystem and nothing else, so the core UI has no knowledge of
 * pylons, cells or alarms.
 */
class SGridObjectivePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGridObjectivePanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UGridRestorationSubsystem* GetSubsystem() const;

	TWeakObjectPtr<UWorld> WeakWorld;
};
