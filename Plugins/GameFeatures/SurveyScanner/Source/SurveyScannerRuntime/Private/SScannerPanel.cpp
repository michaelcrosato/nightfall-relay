// Copyright Nightfall Relay. All Rights Reserved.

#include "SScannerPanel.h"

#include "Brushes/SlateColorBrush.h"
#include "ScannerComponent.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SurveyScanner"

namespace
{
	/** Must be at least UScannerComponent::MaxContacts. */
	constexpr int32 MaxContactRows = 8;

	const FSlateBrush* SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** A coarse arrow for a relative bearing, which reads faster than a number. */
	FString BearingGlyph(float RelativeBearing)
	{
		const float Absolute = FMath::Abs(RelativeBearing);
		if (Absolute < 18.0f)
		{
			return TEXT(" ^ ");
		}
		if (Absolute > 150.0f)
		{
			return TEXT(" v ");
		}
		return (RelativeBearing < 0.0f) ? TEXT(" < ") : TEXT(" > ");
	}
}

void SScannerPanel::Construct(const FArguments& InArgs)
{
	WeakScanner = InArgs._Scanner;

	const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	// --- Heading and cooldown ---------------------------------------------------------
	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(11))
		.ColorAndOpacity_Lambda([this]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			return FSlateColor((Scanner && Scanner->GetCooldownRemaining() > 0.0f)
				? FNightfallUIStyle::TextSecondary()
				: FNightfallUIStyle::Accent());
		})
		.Text_Lambda([this]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			if (!Scanner)
			{
				return FText::GetEmpty();
			}
			const float Cooldown = Scanner->GetCooldownRemaining();
			if (Cooldown > 0.0f)
			{
				return FText::FromString(FString::Printf(TEXT("SURVEY   recharging %3.1fs"), Cooldown));
			}
			return LOCTEXT("ScannerReady", "SURVEY   ready");
		})
	];

	for (int32 Index = 0; Index < MaxContactRows; ++Index)
	{
		Body->AddSlot().AutoHeight()[ MakeContactRow(Index) ];
	}

	// --- Nothing found -----------------------------------------------------------------
	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
		.Visibility_Lambda([this]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			const bool bEmptyScan = Scanner && Scanner->IsScanActive() && Scanner->GetContacts().Num() == 0;
			return bEmptyScan ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.Text(LOCTEXT("NoContacts", "no returns"))
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(SolidBrush())
		.BorderBackgroundColor(FSlateColor(FNightfallUIStyle::PanelBackground()))
		.Padding(FMargin(12.0f, 9.0f))
		[
			SNew(SBox).MinDesiredWidth(250.0f)
			[
				Body
			]
		]
	];
}

TSharedRef<SWidget> SScannerPanel::MakeContactRow(int32 Index)
{
	return SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.ColorAndOpacity_Lambda([this, Index]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			if (!Scanner || !Scanner->GetContacts().IsValidIndex(Index))
			{
				return FSlateColor(FNightfallUIStyle::TextPrimary());
			}
			// Carryables are what the player is actually hunting, so they lead in accent.
			return FSlateColor(Scanner->GetContacts()[Index].bPriority
				? FNightfallUIStyle::Accent()
				: FNightfallUIStyle::TextPrimary());
		})
		.Visibility_Lambda([this, Index]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			return (Scanner && Scanner->GetContacts().IsValidIndex(Index))
				? EVisibility::Visible
				: EVisibility::Collapsed;
		})
		.Text_Lambda([this, Index]()
		{
			const UScannerComponent* Scanner = WeakScanner.Get();
			if (!Scanner || !Scanner->GetContacts().IsValidIndex(Index))
			{
				return FText::GetEmpty();
			}

			const FScannerContact& Contact = Scanner->GetContacts()[Index];
			return FText::FromString(FString::Printf(
				TEXT("%s%4.0fm  %s"),
				*BearingGlyph(Contact.RelativeBearing),
				Contact.Distance,
				*Contact.Name.ToString()));
		});
}

#undef LOCTEXT_NAMESPACE
