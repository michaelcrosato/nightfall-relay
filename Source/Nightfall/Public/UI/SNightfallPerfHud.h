// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UNightfallPerfSubsystem;
class UWorld;

/** How much of the performance HUD is shown. */
enum class ENightfallPerfHudMode : uint8
{
	Hidden,
	/** Frame rate, frame time and the budget verdict. */
	Compact,
	/** Adds thread breakdown, per-system rows, GPU pass rows and streaming cost. */
	Full,

	Count
};

/**
 * The performance HUD.
 *
 * Every line is a bound text attribute reading UNightfallPerfSubsystem, so there is no
 * update loop here and nothing to keep in sync. The row lists are a fixed number of slots
 * whose text collapses when the corresponding row is absent, which avoids rebuilding the
 * widget tree as costs come and go.
 *
 * Values are coloured against the project's frame budget: green under 80% of it, amber up
 * to it, red past it. Checking whether the slice holds 60 fps is meant to be a glance.
 */
class NIGHTFALL_API SNightfallPerfHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNightfallPerfHud) {}
		/** World the perf subsystem is read from. */
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		/** Current verbosity. */
		SLATE_ATTRIBUTE(ENightfallPerfHudMode, Mode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UNightfallPerfSubsystem* GetPerf() const;

	/** One fixed-width "name    value" line bound to an index in one of the row lists. */
	TSharedRef<class SWidget> MakeRowSlot(int32 Index, bool bGpuList);

	/** Section heading, visible only in Full mode. */
	TSharedRef<class SWidget> MakeSectionHeading(const FText& Label);

	EVisibility GetVisibilityForMode(ENightfallPerfHudMode Minimum) const;

	TWeakObjectPtr<UWorld> WeakWorld;
	TAttribute<ENightfallPerfHudMode> ModeAttribute;
};
