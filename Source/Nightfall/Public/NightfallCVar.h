// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Writing a console variable on the player's behalf.
 *
 * Every renderer setting the menu offers is a console variable underneath, and a console
 * variable write can be refused. The engine ranks writes by who made them, and the tier
 * `UGameUserSettings` writes at sits *below* the one the project's own renderer settings
 * use - so any variable named in `[/Script/Engine.RendererSettings]` silently ignores the
 * player. Two of the four lighting toggles shipped that way: the menu remembered the
 * choice, saved it, read it back, and changed nothing on screen.
 *
 * So this is the only place in the project that writes one. It writes at the tier the
 * engine reserves for exactly this case, and it checks afterwards that the write actually
 * landed - because the failure is otherwise one line in a log nobody reads.
 */
namespace NightfallCVar
{
	/** What became of a write. */
	enum class EWriteResult : uint8
	{
		/** The renderer will read the value that was asked for. */
		Applied,

		/** No such variable. Expected for a vendor plugin that is not installed. */
		NotFound,

		/** Something with a stronger claim owns the value, and the player's choice was dropped. */
		Refused
	};

	/** Set an integer variable on the player's behalf. */
	NIGHTFALL_API EWriteResult Set(const TCHAR* Name, int32 Value, bool bWarnIfNotFound = true);

	/** Set a float variable on the player's behalf. */
	NIGHTFALL_API EWriteResult Set(const TCHAR* Name, float Value, bool bWarnIfNotFound = true);
}
