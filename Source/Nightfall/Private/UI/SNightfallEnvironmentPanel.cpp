// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallEnvironmentPanel.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/World.h"
#include "NightfallWorldClockSubsystem.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace
{
	/** Keeps the panel from breathing as the clock and the temperature change width. */
	constexpr float PanelMinWidth = 228.0f;

	const FSlateBrush* SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	FText PhaseLabel(ENightfallTimePhase Phase)
	{
		switch (Phase)
		{
		case ENightfallTimePhase::Dawn:		return LOCTEXT("PhaseDawn", "DAWN");
		case ENightfallTimePhase::Day:		return LOCTEXT("PhaseDay", "DAY");
		case ENightfallTimePhase::Dusk:		return LOCTEXT("PhaseDusk", "DUSK");
		default:							return LOCTEXT("PhaseNight", "NIGHT");
		}
	}
}

void SNightfallEnvironmentPanel::Construct(const FArguments& InArgs)
{
	WeakWorld = InArgs._World;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(SolidBrush())
		.BorderBackgroundColor(FSlateColor(FNightfallUIStyle::PanelBackground()))
		.Padding(FMargin(14.0f, 10.0f))
		[
			SNew(SBox).MinDesiredWidth(PanelMinWidth)
			[
				SNew(SVerticalBox)

				// --- Which day it is, and the date it fell on -------------------------
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(10))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
						.Text_Lambda([this]()
						{
							const UNightfallWorldClockSubsystem* Clock = GetClock();
							return FText::Format(
								LOCTEXT("DayCounter", "DAY {0}"),
								FText::AsNumber(Clock ? Clock->GetDayIndex() + 1 : 1));
						})
					]

					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNullWidget::NullWidget
					]

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(10))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
						.Text_Lambda([this]()
						{
							const UNightfallWorldClockSubsystem* Clock = GetClock();
							return Clock
								? FText::FromString(Clock->GetDateString())
								: LOCTEXT("DateUnavailable", "-- --- ----");
						})
					]
				]

				// --- The hour, and what the ground is reading ---------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(26))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
						.Text_Lambda([this]()
						{
							const UNightfallWorldClockSubsystem* Clock = GetClock();
							return FText::FromString(Clock ? Clock->GetClockString() : TEXT("--:--"));
						})
					]

					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNullWidget::NullWidget
					]

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(18))
						.ColorAndOpacity_Lambda([this]()
						{
							const UNightfallWorldClockSubsystem* Clock = GetClock();
							return FSlateColor(Clock
								? FNightfallUIStyle::ColorForTemperature(Clock->GetTemperatureCelsius())
								: FNightfallUIStyle::TextSecondary());
						})
						.Text_Lambda([this]()
						{
							const UNightfallWorldClockSubsystem* Clock = GetClock();
							return FText::FromString(Clock ? Clock->GetTemperatureString() : TEXT("--"));
						})
					]
				]

				// --- The phase the temperature is answering to ---------------------------
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Font(FNightfallUIStyle::GetMonoFont(9))
					.ColorAndOpacity(FSlateColor(FNightfallUIStyle::PanelBorder()))
					.Text_Lambda([this]()
					{
						const UNightfallWorldClockSubsystem* Clock = GetClock();
						return Clock ? PhaseLabel(Clock->GetTimePhase()) : FText::GetEmpty();
					})
				]
			]
		]
	];
}

const UNightfallWorldClockSubsystem* SNightfallEnvironmentPanel::GetClock() const
{
	const UWorld* World = WeakWorld.Get();
	return World ? World->GetSubsystem<UNightfallWorldClockSubsystem>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
