// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UScannerComponent;

/**
 * The scanner readout: a fixed stack of contact rows that fill in after a pulse.
 *
 * Rows are pre-made and collapse when unused, so the widget tree never changes shape as
 * contacts come and go.
 */
class SScannerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SScannerPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UScannerComponent>, Scanner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** One "<  18m  Power Cell" line bound to a contact index. */
	TSharedRef<class SWidget> MakeContactRow(int32 Index);

	TWeakObjectPtr<UScannerComponent> WeakScanner;
};
