// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NightfallMachine.h"
#include "NightfallSentinelDrone.generated.h"

class ULocalFogVolumeComponent;
class USpotLightComponent;

/**
 * A named sentinel behaviour preset.
 *
 * Rows live in a DataTable so a designer can retune every drone of a type at once, while a
 * single placement can still override any value on the actor. The actor's own properties
 * are the defaults; a row, when one is named, replaces them at BeginPlay.
 */
USTRUCT(BlueprintType)
struct NIGHTFALL_API FNightfallSentinelTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float AlertSpeed = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float DetectionRange = 2900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float DetectionHalfAngle = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float TimeToAcquire = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float TimeToLose = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float InvestigateDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ClampMin = "0.0"))
	float BeamIntensity = 11000.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNightfallDroneAlertSignature, class ANightfallSentinelDrone*, Drone, bool, bAlerted);

/**
 * The autonomous machine: a hovering sentinel that patrols, looks around, and locks on.
 *
 * Its whole body is a rigid hierarchy - hull, yaw ring, pitch arm, sensor pod, four
 * rotors - and every bit of motion is a relative transform written each frame. The hull
 * banks into its own velocity, the rotors spin faster under load, the sensor sweeps while
 * searching and tracks while alerted. Nothing here is a pose or a clip.
 *
 * The drone reports what it sees and does nothing about it. Consequences belong to
 * whichever feature plugin is listening.
 */
UCLASS()
class NIGHTFALL_API ANightfallSentinelDrone : public ANightfallMachine
{
	GENERATED_BODY()

public:
	ANightfallSentinelDrone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Patrol ----------------------------------------------------------------------

	/**
	 * Waypoints in the drone's own space at spawn. Two or more makes a route; fewer means
	 * the drone holds station and sweeps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone")
	TArray<FVector> PatrolOffsets;

	/** Cruise speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 340.0f;

	/** Speed while closing on a target, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone", meta = (ClampMin = "0.0"))
	float AlertSpeed = 620.0f;

	/** How close counts as arrived, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone", meta = (ClampMin = "1.0"))
	float WaypointTolerance = 120.0f;

	/** Amplitude of the idle hover bob, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone")
	float HoverAmplitude = 11.0f;

	/** Hover bob rate, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone")
	float HoverFrequency = 0.55f;

	/** Degrees of bank at full cruise speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone")
	float MaxBankDegrees = 17.0f;

	// --- Sensing ---------------------------------------------------------------------

	/** Sight range in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float DetectionRange = 2900.0f;

	/** Half angle of the sight cone, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float DetectionHalfAngle = 42.0f;

	/** Seconds of unbroken sight before the drone commits to an alert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float TimeToAcquire = 0.65f;

	/** Seconds without sight before the alert drops back to investigating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float TimeToLose = 2.6f;

	/** Seconds spent searching the last known position before resuming patrol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float InvestigateDuration = 6.0f;

	/** Degrees either side of centre the sensor sweeps while searching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing")
	float SearchSweepDegrees = 62.0f;

	/** Sensor beam colour while patrolling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing")
	FLinearColor PatrolBeamColor = FLinearColor(0.24f, 0.72f, 1.0f);

	/** Sensor beam colour while alerted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing")
	FLinearColor AlertBeamColor = FLinearColor(1.0f, 0.10f, 0.16f);

	/** Sensor beam output in lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float BeamIntensity = 11000.0f;

	/**
	 * Extra sight range against a fully lit target, as a fraction of DetectionRange. At 0.5
	 * a patrol sentinel reaches 43.5 m instead of 29 m for someone with the phone out and a
	 * live cell in their hands.
	 *
	 * Deliberately an actor property rather than a tuning row field: ApplyTuningRow copies a
	 * fixed list of eight values, so a row field added without touching it would silently
	 * never reach any drone in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float ConspicuousRangeBonus = 0.5f;

	/** How much faster a fully lit target fills the acquisition window. 1.0 is twice as fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Sensing", meta = (ClampMin = "0.0"))
	float ConspicuousAcquireBonus = 1.0f;

	// --- Rotor downwash ----------------------------------------------------------------

	/** Height above ground in cm at or above which the rotors raise nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "0.0"))
	float DownwashMaxAltitude = 1400.0f;

	/** Height above ground in cm at or below which the wash is at full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "0.0"))
	float DownwashMinAltitude = 300.0f;

	/** Density of the plume at full strength over fully dusty ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "0.0"))
	float DownwashMaxExtinction = 0.85f;

	/** Plume radius in cm when the drone is low over the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "1.0"))
	float DownwashLowRadius = 800.0f;

	/** Plume radius in cm when the drone is at the top of its band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "1.0"))
	float DownwashHighRadius = 1700.0f;

	/** Seconds between ground samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash", meta = (ClampMin = "0.0"))
	float DownwashTraceInterval = 0.15f;

	/** Actor tag marking a surface as loose dust. Placed on the terrain by the level build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Drone|Downwash")
	FName DustySurfaceTag = TEXT("NF_Dusty");

	UFUNCTION(BlueprintPure, Category = "Nightfall|Drone")
	bool IsAlerted() const { return bAlerted; }

	/** Fires when the drone acquires or drops its target. */
	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Drone")
	FNightfallDroneAlertSignature OnAlertChanged;

	/** Force the drone back to patrol, clearing any alert. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Drone")
	void ResetToPatrol();

	/** Behaviour preset table. Leave unset to use this actor's own values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Drone|Tuning")
	TObjectPtr<UDataTable> TuningTable;

	/** Row to apply from TuningTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Drone|Tuning")
	FName TuningRowName;

private:
	/** Copy a named row over this drone's defaults. */
	void ApplyTuningRow();

	void UpdateSensing(float DeltaSeconds);
	void UpdateMovement(float DeltaSeconds);
	void UpdateSensorRig(float DeltaSeconds);
	void UpdateRotors(float DeltaSeconds);
	void UpdateDownwash(float DeltaSeconds);
	void SetAlerted(bool bNewAlerted);

	/**
	 * True when the player pawn is in range, in cone, and not occluded. Also reports how lit
	 * the target is, which widens the range gate here and speeds acquisition in UpdateSensing.
	 */
	bool HasLineOfSightToTarget(FVector& OutTargetLocation, float& OutConspicuity) const;

	/** World position the drone is currently flying toward. */
	FVector ResolveDesiredLocation() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> YawRing;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PitchArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SensorPod;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Rotors;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> SensorBeam;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<ULocalFogVolumeComponent> Downwash;

	/** Patrol route in world space, resolved from PatrolOffsets at BeginPlay. */
	TArray<FVector> WorldRoute;

	/** Where the body would be with no hover bob applied. */
	FVector BaseLocation = FVector::ZeroVector;

	FVector SmoothedVelocity = FVector::ZeroVector;
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	/** Ground height under the body, its dustiness, and the sampling countdown. */
	float GroundZ = 0.0f;
	float GroundDustiness = 0.0f;
	float GroundTraceTimer = 0.0f;

	/** Eased plume strength, and the last density actually pushed to the render state. */
	float DustAlpha = 0.0f;
	float LastAppliedExtinction = -1.0f;

	int32 RouteIndex = 0;
	float HoverPhase = 0.0f;
	float RotorAngle = 0.0f;
	float SightSeconds = 0.0f;
	float UnsightSeconds = 0.0f;
	float InvestigateRemaining = 0.0f;
	float SweepPhase = 0.0f;

	/** Current sensor rig angles in the hull's space, in degrees. */
	float SensorYaw = 0.0f;
	float SensorPitch = 0.0f;

	bool bAlerted = false;
};
