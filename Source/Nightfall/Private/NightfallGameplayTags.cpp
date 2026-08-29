// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallGameplayTags.h"

namespace NightfallTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interactable, "Nightfall.Interactable", "Root tag for anything the interaction trace should consider.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interactable_PowerNode, "Nightfall.Interactable.PowerNode", "A relay pylon or other grid node that accepts charge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interactable_Portable, "Nightfall.Interactable.Portable", "A rigid-body prop the player can carry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interactable_Fixture, "Nightfall.Interactable.Fixture", "A mounted mechanical fixture such as a blast door or lift.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Move, "Nightfall.Input.Move", "2D ground movement axis.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Look, "Nightfall.Input.Look", "2D look axis.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Jump, "Nightfall.Input.Jump", "Jump.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Sprint, "Nightfall.Input.Sprint", "Hold to sprint.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Crouch, "Nightfall.Input.Crouch", "Hold to crouch.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Interact, "Nightfall.Input.Interact", "Use the focused interactable.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Drop, "Nightfall.Input.Drop", "Release a carried prop.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_ToggleFly, "Nightfall.Input.ToggleFly", "Enter or leave free flight.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_ToggleMenu, "Nightfall.Input.ToggleMenu", "Open or close the settings menu.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_TogglePerfHud, "Nightfall.Input.TogglePerfHud", "Cycle the performance HUD verbosity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_ToggleFlashlight, "Nightfall.Input.ToggleFlashlight", "Turn the phone light on or off.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Machine_State_Dormant, "Nightfall.Machine.State.Dormant", "Powered down, no sensing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Machine_State_Patrol, "Nightfall.Machine.State.Patrol", "Following its patrol route.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Machine_State_Investigate, "Nightfall.Machine.State.Investigate", "Moving to the last known stimulus.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Machine_State_Alert, "Nightfall.Machine.State.Alert", "Target acquired and tracked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Machine_State_Disabled, "Nightfall.Machine.State.Disabled", "Knocked out of service.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fixture_State_Closed, "Nightfall.Fixture.State.Closed", "Fixture fully closed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fixture_State_Opening, "Nightfall.Fixture.State.Opening", "Fixture travelling open.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fixture_State_Open, "Nightfall.Fixture.State.Open", "Fixture fully open.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fixture_State_Closing, "Nightfall.Fixture.State.Closing", "Fixture travelling closed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fixture_State_Locked, "Nightfall.Fixture.State.Locked", "Fixture refuses to actuate.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Power_Node_Dormant, "Nightfall.Power.Node.Dormant", "Node holds no charge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Power_Node_Charging, "Nightfall.Power.Node.Charging", "Node is accumulating charge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Power_Node_Online, "Nightfall.Power.Node.Online", "Node is energised and feeding the grid.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Movement_Walking, "Nightfall.Player.Movement.Walking", "Grounded at walk speed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Movement_Sprinting, "Nightfall.Player.Movement.Sprinting", "Grounded at sprint speed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Movement_Crouching, "Nightfall.Player.Movement.Crouching", "Grounded and crouched.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Movement_Airborne, "Nightfall.Player.Movement.Airborne", "Not grounded.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Movement_Flying, "Nightfall.Player.Movement.Flying", "In free flight, ignoring gravity and collision.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Hud, "Nightfall.UI.Layer.Hud", "General HUD corner panels.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Objective, "Nightfall.UI.Layer.Objective", "Objective / progress panels.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Reticle, "Nightfall.UI.Layer.Reticle", "Screen-centre reticle and focus prompt.");
}
