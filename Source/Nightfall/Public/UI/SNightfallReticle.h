// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UNightfallInteractorComponent;
class UWorld;

/**
 * Screen-centre reticle and interaction prompt.
 *
 * Three states in one widget: a dim dot with nothing in reach, a bright accented dot with
 * the verb and target name under it when something is focused, and a filling bar while a
 * hold interaction is charging. Everything is bound, so the widget never needs telling
 * that the focus changed.
 */
class NIGHTFALL_API SNightfallReticle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNightfallReticle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** The local player's interactor, or null when there is no pawn yet. */
	UNightfallInteractorComponent* GetInteractor() const;

	TWeakObjectPtr<UWorld> WeakWorld;
};
