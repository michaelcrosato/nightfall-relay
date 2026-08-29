// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallBriefing.h"

#include "Brushes/SlateColorBrush.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace
{
	/** Wide enough for prose at 13pt without the line length becoming tiring. */
	constexpr float BriefingWidth = 720.0f;

	/** Key column in the controls block, wide enough for "W A S D". */
	constexpr float KeyColumnWidth = 104.0f;

	const FSlateBrush* SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}
}

void SNightfallBriefing::AddHeading(const TSharedRef<SVerticalBox>& Container, const FText& Label)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 7.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(11))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::PanelBorder()))
		.Text(Label)
	];
}

void SNightfallBriefing::AddParagraph(const TSharedRef<SVerticalBox>& Container, const FText& Body)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetTextFont(13))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextPrimary()))
		.AutoWrapText(true)
		.Text(Body)
	];
}

void SNightfallBriefing::AddStep(const TSharedRef<SVerticalBox>& Container, const FText& Ordinal, const FText& Body)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FNightfallUIStyle::GetMonoFont(13))
			.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
			.Text(Ordinal)
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Font(FNightfallUIStyle::GetTextFont(13))
			.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextPrimary()))
			.AutoWrapText(true)
			.Text(Body)
		]
	];
}

void SNightfallBriefing::AddControl(const TSharedRef<SVerticalBox>& Container, const FText& Key, const FText& Action)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(KeyColumnWidth)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(12))
				.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
				.Text(Key)
			]
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Font(FNightfallUIStyle::GetTextFont(12))
			.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
			.Text(Action)
		]
	];
}

void SNightfallBriefing::Construct(const FArguments& InArgs)
{
	OnDismissRequested = InArgs._OnDismissRequested;

	const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	AddHeading(Body, LOCTEXT("BriefSituationHeading", "-- situation ---------------------------------------------"));
	AddParagraph(Body, LOCTEXT("BriefSituation1",
		"The solar relay field went dark before you got here. Six pylons stand across half a "
		"kilometre of desert and not one of them holds a charge."));
	AddParagraph(Body, LOCTEXT("BriefSituation2",
		"The sun is already on the horizon. When it is gone, the only light in this place is "
		"the light you put back into it."));

	AddHeading(Body, LOCTEXT("BriefWorkHeading", "-- your work ---------------------------------------------"));
	AddStep(Body, LOCTEXT("BriefStep1Ordinal", "1"), LOCTEXT("BriefStep1",
		"Find a power cell. They are scattered around the compound, and they glow - which is "
		"how you will find them once it is properly dark."));
	AddStep(Body, LOCTEXT("BriefStep2Ordinal", "2"), LOCTEXT("BriefStep2",
		"Carry it to a relay pylon and press E to seat it. You can only carry one at a time."));
	AddStep(Body, LOCTEXT("BriefStep3Ordinal", "3"), LOCTEXT("BriefStep3",
		"Two cells bring one pylon online. Its lights come up and the ground around it becomes "
		"somewhere you can see."));
	AddStep(Body, LOCTEXT("BriefStep4Ordinal", "4"), LOCTEXT("BriefStep4",
		"Six pylons restore the field. That is the whole job."));

	AddHeading(Body, LOCTEXT("BriefThreatHeading", "-- what else is out there --------------------------------"));
	AddParagraph(Body, LOCTEXT("BriefThreat",
		"Sentinel drones patrol the field, sweeping the ground with search beams. While one has "
		"you in its beam the grid bleeds charge back out of every pylon you have lit. You cannot "
		"fight them and you cannot switch them off. Break their line of sight, or keep out of it."));

	AddHeading(Body, LOCTEXT("BriefControlsHeading", "-- controls ----------------------------------------------"));

	const TSharedRef<SVerticalBox> LeftKeys = SNew(SVerticalBox);
	AddControl(LeftKeys, LOCTEXT("KeyMove", "W A S D"), LOCTEXT("KeyMoveAction", "move"));
	AddControl(LeftKeys, LOCTEXT("KeyLook", "mouse"), LOCTEXT("KeyLookAction", "look"));
	AddControl(LeftKeys, LOCTEXT("KeyJump", "Space"), LOCTEXT("KeyJumpAction", "jump"));
	AddControl(LeftKeys, LOCTEXT("KeySprint", "Shift"), LOCTEXT("KeySprintAction", "sprint"));
	AddControl(LeftKeys, LOCTEXT("KeyCrouch", "Ctrl"), LOCTEXT("KeyCrouchAction", "crouch"));

	const TSharedRef<SVerticalBox> RightKeys = SNew(SVerticalBox);
	AddControl(RightKeys, LOCTEXT("KeyInteract", "E"), LOCTEXT("KeyInteractAction", "pick up / seat a cell"));
	AddControl(RightKeys, LOCTEXT("KeyDrop", "Q"), LOCTEXT("KeyDropAction", "throw what you are holding"));
	AddControl(RightKeys, LOCTEXT("KeyScan", "F"), LOCTEXT("KeyScanAction", "survey pulse"));
	AddControl(RightKeys, LOCTEXT("KeyFly", "V"), LOCTEXT("KeyFlyAction", "free flight"));
	AddControl(RightKeys, LOCTEXT("KeyMenu", "Esc"), LOCTEXT("KeyMenuAction", "settings"));

	Body->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().FillWidth(0.44f)
		[
			LeftKeys
		]

		+ SHorizontalBox::Slot().FillWidth(0.56f)
		[
			RightKeys
		]
	];

	ChildSlot
	[
		// A dimmer across the whole viewport rather than a black screen: the dusk this card
		// is describing stays visible behind it.
		SNew(SBorder)
		.BorderImage(SolidBrush())
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f))
		[
			SNew(SBorder)
			.BorderImage(SolidBrush())
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.006f, 0.008f, 0.012f, 0.96f)))
			.Padding(FMargin(34.0f, 28.0f))
			[
				SNew(SBox).WidthOverride(BriefingWidth).MaxDesiredHeight(820.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(20))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
						.Text(LOCTEXT("BriefTitle", "NIGHTFALL RELAY"))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetTextFont(11))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
						.Text(LOCTEXT("BriefSubtitle", "relay field 7 - restoration order - one operator, no relief"))
					]

					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							Body
						]
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 22.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetMonoFont(14))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
						.Text(LOCTEXT("BriefDismiss", "press  Esc  to begin"))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FNightfallUIStyle::GetTextFont(10))
						.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
						.Text(LOCTEXT("BriefClockHint",
							"the day is holding while you read this. Nightfall.Briefing brings the card back."))
					]
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
