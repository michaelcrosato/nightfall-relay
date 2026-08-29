// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UNightfallWorldClockSubsystem;
class UWorld;

/**
 * Date, clock and surface temperature, top right.
 *
 * Reads UNightfallWorldClockSubsystem and nothing else. Every value is bound rather than
 * pushed, so the panel has no tick of its own and nothing has to tell it the day rolled
 * over. The temperature is coloured by its own value, which is what makes the walk into
 * dusk legible at a glance: the number goes blue as the ground gives its heat up.
 */
class NIGHTFALL_API SNightfallEnvironmentPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNightfallEnvironmentPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** The world clock, or null before the world has one. */
	const UNightfallWorldClockSubsystem* GetClock() const;

	TWeakObjectPtr<UWorld> WeakWorld;
};
