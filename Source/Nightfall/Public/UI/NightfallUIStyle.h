// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * The visual language of the interface, in one place.
 *
 * All of the UI in this project is hand-built Slate rather than widget blueprints, so
 * there are no UI assets to generate and nothing to keep in sync between code and content.
 * The palette matches the world: cold desaturated panels, sodium amber for anything the
 * grid owns, and one alert red.
 */
struct NIGHTFALL_API FNightfallUIStyle
{
	/** Monospaced face for numbers, so columns line up as values change width. */
	static FSlateFontInfo GetMonoFont(int32 Size);

	/** Proportional face for prose. */
	static FSlateFontInfo GetTextFont(int32 Size);

	/** Panel background: near black, mostly opaque. */
	static FLinearColor PanelBackground();

	/** Hairline used to separate sections. */
	static FLinearColor PanelBorder();

	/** Default body text. */
	static FLinearColor TextPrimary();

	/** Labels and units. */
	static FLinearColor TextSecondary();

	/** Sodium amber. Anything to do with the grid. */
	static FLinearColor Accent();

	/** Within budget. */
	static FLinearColor Good();

	/** Approaching budget. */
	static FLinearColor Warning();

	/** Over budget, or hostile. */
	static FLinearColor Bad();

	/** Deep cold. The far end of the temperature readout. */
	static FLinearColor Cold();

	/**
	 * Colour for a surface temperature: hard blue in the deep cold, through ordinary text
	 * at freezing, to sodium amber once the ground is genuinely warm. The readout carries
	 * how cold it is getting without the player having to read the number.
	 */
	static FLinearColor ColorForTemperature(float Celsius);

	/**
	 * Colour for a measured value against a budget: green up to 80% of it, amber to 100%,
	 * red beyond. Used by every number on the performance HUD.
	 */
	static FLinearColor ColorForBudget(float Value, float Budget);
};
