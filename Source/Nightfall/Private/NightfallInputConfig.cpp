// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallInputConfig.h"

#include "InputAction.h"
#include "Nightfall.h"

const UInputAction* UNightfallInputConfig::FindAction(FGameplayTag InputTag, bool bLogIfMissing) const
{
	for (const FNightfallInputActionBinding& Binding : Actions)
	{
		if (Binding.InputTag == InputTag && Binding.InputAction)
		{
			return Binding.InputAction;
		}
	}

	if (bLogIfMissing)
	{
		UE_LOG(LogNightfall, Warning, TEXT("Input config '%s' has no action bound to tag '%s'."),
			*GetNameSafe(this), *InputTag.ToString());
	}

	return nullptr;
}
