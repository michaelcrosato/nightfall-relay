// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NightfallConspicuous.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UNightfallConspicuous : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something that can give itself away by being lit.
 *
 * A sentinel's sensing is geometry - range, cone, occlusion - and geometry alone says a
 * figure crossing bare ground in the dark is as visible as one holding a lamp. This is the
 * one thing it asks of whatever it is looking at, so the drone still senses "a conspicuous
 * thing" rather than knowing what a player or a flashlight is.
 *
 * It is deliberately a single scalar. Anything richer would be a perception system, and
 * this game does not have one.
 */
class NIGHTFALL_API INightfallConspicuous
{
	GENERATED_BODY()

public:
	/**
	 * How much this actor is advertising itself, from 0 to 1.
	 *
	 * 0 is as dark as the ground it stands on and is what an unlit player returns; 1 is
	 * carrying every light it owns. Implementations should stay inside that range - the
	 * sentinel scales its own sensing by it and does not clamp on the caller's behalf.
	 */
	virtual float GetConspicuity() const = 0;
};
