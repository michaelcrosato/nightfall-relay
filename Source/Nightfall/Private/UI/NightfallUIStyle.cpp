// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/NightfallUIStyle.h"

#include "Styling/CoreStyle.h"

FSlateFontInfo FNightfallUIStyle::GetMonoFont(int32 Size)
{
	return FCoreStyle::GetDefaultFontStyle("Mono", Size);
}

FSlateFontInfo FNightfallUIStyle::GetTextFont(int32 Size)
{
	return FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

FLinearColor FNightfallUIStyle::PanelBackground()
{
	return FLinearColor(0.008f, 0.010f, 0.014f, 0.84f);
}

FLinearColor FNightfallUIStyle::PanelBorder()
{
	return FLinearColor(0.30f, 0.35f, 0.42f, 0.55f);
}

FLinearColor FNightfallUIStyle::TextPrimary()
{
	return FLinearColor(0.86f, 0.90f, 0.95f, 1.0f);
}

FLinearColor FNightfallUIStyle::TextSecondary()
{
	return FLinearColor(0.46f, 0.52f, 0.60f, 1.0f);
}

FLinearColor FNightfallUIStyle::Accent()
{
	return FLinearColor(1.0f, 0.62f, 0.18f, 1.0f);
}

FLinearColor FNightfallUIStyle::Good()
{
	return FLinearColor(0.36f, 0.92f, 0.55f, 1.0f);
}

FLinearColor FNightfallUIStyle::Warning()
{
	return FLinearColor(1.0f, 0.76f, 0.22f, 1.0f);
}

FLinearColor FNightfallUIStyle::Bad()
{
	return FLinearColor(1.0f, 0.32f, 0.30f, 1.0f);
}

FLinearColor FNightfallUIStyle::Cold()
{
	return FLinearColor(0.38f, 0.68f, 1.0f, 1.0f);
}

FLinearColor FNightfallUIStyle::ColorForTemperature(float Celsius)
{
	// Two ramps meeting at freezing, which is where the readout should look neutral.
	if (Celsius <= 0.0f)
	{
		return FMath::Lerp(Cold(), TextPrimary(), FMath::Clamp((Celsius + 12.0f) / 12.0f, 0.0f, 1.0f));
	}
	return FMath::Lerp(TextPrimary(), Accent(), FMath::Clamp(Celsius / 20.0f, 0.0f, 1.0f));
}

FLinearColor FNightfallUIStyle::ColorForBudget(float Value, float Budget)
{
	if (Budget <= KINDA_SMALL_NUMBER)
	{
		return TextPrimary();
	}

	const float Ratio = Value / Budget;
	if (Ratio <= 0.8f)
	{
		return Good();
	}
	if (Ratio <= 1.0f)
	{
		return Warning();
	}
	return Bad();
}
