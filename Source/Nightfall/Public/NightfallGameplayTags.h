// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native gameplay tags owned by the core runtime.
 *
 * Game Feature Plugins reference these to describe intent without linking to concrete
 * classes: a plugin asks "which actors carry Nightfall.Interactable.PowerNode" rather
 * than casting to ANightfallRelayPylon. Plugins declare their own tags in their own
 * modules; nothing here needs editing when a feature is added.
 */
namespace NightfallTags
{
	// --- Interaction -------------------------------------------------------
	NIGHTFALL_API extern FNativeGameplayTag Interactable;
	NIGHTFALL_API extern FNativeGameplayTag Interactable_PowerNode;
	NIGHTFALL_API extern FNativeGameplayTag Interactable_Portable;
	NIGHTFALL_API extern FNativeGameplayTag Interactable_Fixture;

	// --- Input actions (resolved through UNightfallInputConfig) ------------
	NIGHTFALL_API extern FNativeGameplayTag Input_Move;
	NIGHTFALL_API extern FNativeGameplayTag Input_Look;
	NIGHTFALL_API extern FNativeGameplayTag Input_Jump;
	NIGHTFALL_API extern FNativeGameplayTag Input_Sprint;
	NIGHTFALL_API extern FNativeGameplayTag Input_Crouch;
	NIGHTFALL_API extern FNativeGameplayTag Input_Interact;
	NIGHTFALL_API extern FNativeGameplayTag Input_Drop;
	NIGHTFALL_API extern FNativeGameplayTag Input_ToggleFly;
	NIGHTFALL_API extern FNativeGameplayTag Input_ToggleMenu;
	NIGHTFALL_API extern FNativeGameplayTag Input_TogglePerfHud;
	NIGHTFALL_API extern FNativeGameplayTag Input_ToggleFlashlight;

	// --- Machine states ----------------------------------------------------
	NIGHTFALL_API extern FNativeGameplayTag Machine_State_Dormant;
	NIGHTFALL_API extern FNativeGameplayTag Machine_State_Patrol;
	NIGHTFALL_API extern FNativeGameplayTag Machine_State_Investigate;
	NIGHTFALL_API extern FNativeGameplayTag Machine_State_Alert;
	NIGHTFALL_API extern FNativeGameplayTag Machine_State_Disabled;

	// --- Fixture states ----------------------------------------------------
	NIGHTFALL_API extern FNativeGameplayTag Fixture_State_Closed;
	NIGHTFALL_API extern FNativeGameplayTag Fixture_State_Opening;
	NIGHTFALL_API extern FNativeGameplayTag Fixture_State_Open;
	NIGHTFALL_API extern FNativeGameplayTag Fixture_State_Closing;
	NIGHTFALL_API extern FNativeGameplayTag Fixture_State_Locked;

	// --- Power grid --------------------------------------------------------
	NIGHTFALL_API extern FNativeGameplayTag Power_Node_Dormant;
	NIGHTFALL_API extern FNativeGameplayTag Power_Node_Charging;
	NIGHTFALL_API extern FNativeGameplayTag Power_Node_Online;

	// --- Player ------------------------------------------------------------
	NIGHTFALL_API extern FNativeGameplayTag Player_Movement_Walking;
	NIGHTFALL_API extern FNativeGameplayTag Player_Movement_Sprinting;
	NIGHTFALL_API extern FNativeGameplayTag Player_Movement_Crouching;
	NIGHTFALL_API extern FNativeGameplayTag Player_Movement_Airborne;
	NIGHTFALL_API extern FNativeGameplayTag Player_Movement_Flying;

	// --- UI layers that plugins may register panels into -------------------
	NIGHTFALL_API extern FNativeGameplayTag UI_Layer_Hud;
	NIGHTFALL_API extern FNativeGameplayTag UI_Layer_Objective;
	NIGHTFALL_API extern FNativeGameplayTag UI_Layer_Reticle;
}
