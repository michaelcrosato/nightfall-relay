// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UNightfallGameUserSettings;

/**
 * The settings menu.
 *
 * Every control is the same shape - a label, then a value between two arrows - because a
 * uniform stepper covers enums, booleans and quantised numbers alike, reads identically
 * with a mouse or a pad, and needs no style assets. Rows bind straight to
 * UNightfallGameUserSettings, so the menu has no state of its own to fall out of sync.
 *
 * DLSS rows are always listed. When the NVIDIA plugins are absent they are greyed with a
 * line saying so, rather than hidden, so it is obvious what the build supports.
 */
class NIGHTFALL_API SNightfallSettingsMenu : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnCloseRequested);

	SLATE_BEGIN_ARGS(SNightfallSettingsMenu) {}
		/** Raised by the Close button and by pressing the menu key. */
		SLATE_EVENT(FOnCloseRequested, OnCloseRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Settings object every row reads and writes. */
	static UNightfallGameUserSettings* GetSettings();

	/** A section heading. */
	void AddHeading(const TSharedRef<SVerticalBox>& Container, const FText& Label);

	/** A free-text note under a heading. */
	void AddNote(const TSharedRef<SVerticalBox>& Container, TFunction<FText()> GetText);

	/**
	 * One "label  < value >" row.
	 *
	 * @param Step      Called with -1 or +1 when an arrow is pressed.
	 * @param IsEnabled Rows that report false are greyed and stop responding.
	 */
	void AddStepperRow(
		const TSharedRef<SVerticalBox>& Container,
		const FText& Label,
		TFunction<FText()> GetValue,
		TFunction<void(int32)> Step,
		TFunction<bool()> IsEnabled);

	/** Convenience wrapper for a row that cycles an integer in [0, Count). */
	void AddIndexRow(
		const TSharedRef<SVerticalBox>& Container,
		const FText& Label,
		TFunction<int32()> GetIndex,
		TFunction<void(int32)> SetIndex,
		TArray<FText> Options,
		TFunction<bool()> IsEnabled);

	void BuildDisplaySection(const TSharedRef<SVerticalBox>& Container);
	void BuildUpscalingSection(const TSharedRef<SVerticalBox>& Container);
	void BuildLightingSection(const TSharedRef<SVerticalBox>& Container);
	void BuildQualitySection(const TSharedRef<SVerticalBox>& Container);
	TSharedRef<class SWidget> BuildButtonBar();

	void ApplyAndSave();
	void RestoreDefaults();

	FOnCloseRequested OnCloseRequested;
};
