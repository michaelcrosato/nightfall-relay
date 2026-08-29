// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/** Tags owned by this feature. The core project does not declare or reference any of them. */
namespace GridRestorationTags
{
	/** Placed on a ANightfallPhysicsProp to say that placement is a power cell. */
	GRIDRESTORATIONRUNTIME_API extern FNativeGameplayTag Grid_PowerCell;

	/** Placed on a cell that has already been spent. */
	GRIDRESTORATIONRUNTIME_API extern FNativeGameplayTag Grid_SpentCell;
}
