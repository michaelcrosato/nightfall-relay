// Copyright Nightfall Relay. All Rights Reserved.

#include "SGridObjectivePanel.h"

#include "Engine/World.h"
#include "GridRestorationSubsystem.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GridRestoration"

namespace
{
	constexpr float ProgressBarWidth = 320.0f;

}

void SGridObjectivePanel::Construct(const FArguments& InArgs)
{
	WeakWorld = InArgs._World;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FNightfallUIStyle::SolidBrush())
		.BorderBackgroundColor(FSlateColor(FNightfallUIStyle::PanelBackground()))
		.Padding(FMargin(18.0f, 12.0f))
		[
			SNew(SVerticalBox)

			// --- Headline: pylons online, or the completion line --------------------
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(15))
				.ColorAndOpacity_Lambda([this]()
				{
					const UGridRestorationSubsystem* Grid = GetSubsystem();
					return FSlateColor((Grid && Grid->IsFieldRestored())
						? FNightfallUIStyle::Good()
						: FNightfallUIStyle::Accent());
				})
				.Text_Lambda([this]()
				{
					const UGridRestorationSubsystem* Grid = GetSubsystem();
					if (!Grid)
					{
						return FText::GetEmpty();
					}
					if (Grid->IsFieldRestored())
					{
						return LOCTEXT("FieldRestored", "RELAY FIELD RESTORED");
					}
					return FText::Format(
						LOCTEXT("PylonProgress", "PYLONS ONLINE  {0} / {1}"),
						FText::AsNumber(Grid->GetNodesOnline()),
						FText::AsNumber(Grid->GetNodeCount()));
				})
			]

			// --- Charge bar across the whole field ------------------------------------
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(ProgressBarWidth).HeightOverride(4.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox)
						.WidthOverride_Lambda([this]()
						{
							const UGridRestorationSubsystem* Grid = GetSubsystem();
							const int32 Count = Grid ? Grid->GetNodeCount() : 0;
							const float Fraction = (Count > 0) ? (Grid->GetTotalCharge() / Count) : 0.0f;
							return FOptionalSize(ProgressBarWidth * FMath::Clamp(Fraction, 0.0f, 1.0f));
						})
						[
							SNew(SBorder)
							.BorderImage(FNightfallUIStyle::SolidBrush())
							.BorderBackgroundColor(FSlateColor(FNightfallUIStyle::Accent()))
						]
					]

					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FNightfallUIStyle::SolidBrush())
						.BorderBackgroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.14f)))
					]
				]
			]

			// --- Cells left to collect --------------------------------------------------
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(11))
				.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
				.Text_Lambda([this]()
				{
					const UGridRestorationSubsystem* Grid = GetSubsystem();
					if (!Grid)
					{
						return FText::GetEmpty();
					}
					return FText::Format(
						LOCTEXT("CellsRemaining", "power cells in reach: {0}"),
						FText::AsNumber(Grid->GetCellsRemaining()));
				})
			]

			// --- Alarm warning, only while sentinels have eyes on the player -------------
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(12))
				.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Bad()))
				.Visibility_Lambda([this]()
				{
					const UGridRestorationSubsystem* Grid = GetSubsystem();
					return (Grid && Grid->IsAlarmActive()) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Text_Lambda([this]()
				{
					const UGridRestorationSubsystem* Grid = GetSubsystem();
					if (!Grid)
					{
						return FText::GetEmpty();
					}
					return FText::Format(
						LOCTEXT("AlarmDraining", "TRACKED BY {0} - GRID DRAINING"),
						FText::AsNumber(Grid->GetAlarmCount()));
				})
			]
		]
	];
}

UGridRestorationSubsystem* SGridObjectivePanel::GetSubsystem() const
{
	const UWorld* World = WeakWorld.Get();
	return World ? World->GetSubsystem<UGridRestorationSubsystem>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
