// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallCVar.h"

#include "HAL/IConsoleManager.h"
#include "Nightfall.h"

namespace NightfallCVar
{
namespace
{
	/**
	 * The tier the engine reserves for a player overriding a project or device default -
	 * its own comment on the flag says so. It outranks `SetByProjectSetting` and
	 * `SetByDeviceProfile`, which is the whole point, and is outranked by the command line
	 * and the console, so scrubbing a variable by hand while the game runs still wins.
	 */
	constexpr EConsoleVariableFlags PlayerPriority = ECVF_SetByGameOverride;

	IConsoleVariable* Find(const TCHAR* Name, bool bWarnIfNotFound)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls=*/false);
		if (!Variable && bWarnIfNotFound)
		{
			UE_LOG(LogNightfall, Warning, TEXT("Console variable '%s' not found; setting had no effect."), Name);
		}
		return Variable;
	}

	/**
	 * A write that is allowed leaves its own tier on the variable, so the variable still
	 * carrying somebody else's tier means the value the renderer reads is not the one that
	 * was asked for.
	 */
	EWriteResult ReportOutcome(const TCHAR* Name, const IConsoleVariable& Variable)
	{
		const EConsoleVariableFlags SetBy =
			static_cast<EConsoleVariableFlags>(Variable.GetFlags() & ECVF_SetByMask);

		if (SetBy != PlayerPriority)
		{
			UE_LOG(LogNightfall, Warning,
				TEXT("Console variable '%s' refused the player's value and stays at '%s', set by %s. ")
				TEXT("The setting that writes it is inert."),
				Name, *Variable.GetString(), GetConsoleVariableSetByName(SetBy));
			return EWriteResult::Refused;
		}

		return EWriteResult::Applied;
	}
}

EWriteResult Set(const TCHAR* Name, int32 Value, bool bWarnIfNotFound)
{
	IConsoleVariable* Variable = Find(Name, bWarnIfNotFound);
	if (!Variable)
	{
		return EWriteResult::NotFound;
	}

	Variable->Set(Value, PlayerPriority);
	return ReportOutcome(Name, *Variable);
}

EWriteResult Set(const TCHAR* Name, float Value, bool bWarnIfNotFound)
{
	IConsoleVariable* Variable = Find(Name, bWarnIfNotFound);
	if (!Variable)
	{
		return EWriteResult::NotFound;
	}

	Variable->Set(Value, PlayerPriority);
	return ReportOutcome(Name, *Variable);
}
}
