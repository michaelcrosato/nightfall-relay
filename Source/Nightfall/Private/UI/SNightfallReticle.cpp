// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallReticle.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NightfallInteractableComponent.h"
#include "NightfallInteractorComponent.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace
{
	/** Width in slate units of the hold progress bar. */
	constexpr float HoldBarWidth = 128.0f;

}

void SNightfallReticle::Construct(const FArguments& InArgs)
{
	WeakWorld = InArgs._World;

	ChildSlot
	[
		SNew(SVerticalBox)

		// --- The dot ------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(5.0f).HeightOverride(5.0f)
			[
				SNew(SBorder)
				.BorderImage(FNightfallUIStyle::SolidBrush())
				.BorderBackgroundColor_Lambda([this]()
				{
					const UNightfallInteractorComponent* Interactor = GetInteractor();
					const bool bFocused = Interactor && Interactor->GetFocus() != nullptr;
					return FSlateColor(bFocused
						? FNightfallUIStyle::Accent()
						: FLinearColor(1.0f, 1.0f, 1.0f, 0.42f));
				})
			]
		]

		// --- Hold progress ---------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(HoldBarWidth)
			.HeightOverride(3.0f)
			.Visibility_Lambda([this]()
			{
				const UNightfallInteractorComponent* Interactor = GetInteractor();
				return (Interactor && Interactor->GetHoldProgress() > 0.0f) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SHorizontalBox)

				// Filled portion, sized directly from the hold fraction.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.WidthOverride_Lambda([this]()
					{
						const UNightfallInteractorComponent* Interactor = GetInteractor();
						return FOptionalSize(HoldBarWidth * (Interactor ? Interactor->GetHoldProgress() : 0.0f));
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
					.BorderBackgroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.16f)))
				]
			]
		]

		// --- Prompt -----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FNightfallUIStyle::GetTextFont(13))
			.ColorAndOpacity_Lambda([this]()
			{
				const UNightfallInteractorComponent* Interactor = GetInteractor();
				const UNightfallInteractableComponent* Focus = Interactor ? Interactor->GetFocus() : nullptr;
				// A focused but disabled interactable still names itself, greyed out, so
				// the player learns it exists and is simply not usable yet.
				return FSlateColor((Focus && Focus->bInteractionEnabled)
					? FNightfallUIStyle::TextPrimary()
					: FNightfallUIStyle::TextSecondary());
			})
			.Text_Lambda([this]()
			{
				const UNightfallInteractorComponent* Interactor = GetInteractor();
				const UNightfallInteractableComponent* Focus = Interactor ? Interactor->GetFocus() : nullptr;
				return Focus ? Focus->GetPromptText() : FText::GetEmpty();
			})
		]
	];
}

UNightfallInteractorComponent* SNightfallReticle::GetInteractor() const
{
	const UWorld* World = WeakWorld.Get();
	if (!World)
	{
		return nullptr;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	return Pawn ? Pawn->FindComponentByClass<UNightfallInteractorComponent>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
