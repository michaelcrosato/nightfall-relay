// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * The card the player reads before the first shift.
 *
 * One page: what happened here, what to do about it, what is hunting while you do it, and
 * the controls. It is shown over the world rather than over a black screen, so the dusk the
 * text is describing is visible behind it - which is also why UNightfallUISubsystem holds
 * the day clock for as long as this is up.
 *
 * The widget takes no input of its own. Nothing in this project takes keyboard focus, and
 * the game runs in FInputModeGameOnly where viewport widgets receive no events at all, so
 * dismissal arrives from the existing menu key binding and this only raises the delegate.
 */
class NIGHTFALL_API SNightfallBriefing : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnDismissRequested);

	SLATE_BEGIN_ARGS(SNightfallBriefing) {}
		/** Raised when the player asks to leave the card. */
		SLATE_EVENT(FOnDismissRequested, OnDismissRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A section rule, in the same register as the settings menu's headings. */
	static void AddHeading(const TSharedRef<SVerticalBox>& Container, const FText& Label);

	/** A wrapped paragraph of body prose. */
	static void AddParagraph(const TSharedRef<SVerticalBox>& Container, const FText& Body);

	/** One numbered step of the loop. */
	static void AddStep(const TSharedRef<SVerticalBox>& Container, const FText& Ordinal, const FText& Body);

	/** One "KEY  what it does" line in the controls block. */
	static void AddControl(const TSharedRef<SVerticalBox>& Container, const FText& Key, const FText& Action);

	FOnDismissRequested OnDismissRequested;
};
