// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallMachineProfile.h"

const FNightfallMachinePart* UNightfallMachineProfile::FindPart(FName PartName) const
{
	return Parts.FindByPredicate([PartName](const FNightfallMachinePart& Part)
	{
		return Part.PartName == PartName;
	});
}
