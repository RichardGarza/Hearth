// Plain data mirrored from the brain's snapshot frames. See docs/PROTOCOL.md.
#pragma once

#include "CoreMinimal.h"
#include "HearthTypes.generated.h"

USTRUCT(BlueprintType)
struct FHearthNeeds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) float Hunger = 0.f;
	UPROPERTY(BlueprintReadOnly) float Thirst = 0.f;
	UPROPERTY(BlueprintReadOnly) float Energy = 0.f;
	UPROPERTY(BlueprintReadOnly) float Warmth = 0.f;
	UPROPERTY(BlueprintReadOnly) float Health = 0.f;
};

USTRUCT(BlueprintType)
struct FHearthAgentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Voice;
	UPROPERTY(BlueprintReadOnly) FString LocationId;
	UPROPERTY(BlueprintReadOnly) FString MovingTo;       // empty when not travelling
	UPROPERTY(BlueprintReadOnly) FString Action;         // empty when idle
	UPROPERTY(BlueprintReadOnly) FString ActionTarget;
	UPROPERTY(BlueprintReadOnly) FVector2D PositionMeters = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bAlive = true;
	UPROPERTY(BlueprintReadOnly) FHearthNeeds Needs;
	UPROPERTY(BlueprintReadOnly) TMap<FString, int32> Inventory;
};

USTRUCT(BlueprintType)
struct FHearthLocationInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FVector2D PositionMeters = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) TMap<FString, int32> Resources;
	UPROPERTY(BlueprintReadOnly) TMap<FString, int32> Stockpile;
	UPROPERTY(BlueprintReadOnly) bool bHasFire = false;
	UPROPERTY(BlueprintReadOnly) bool bFireLit = false;
	UPROPERTY(BlueprintReadOnly) int32 FireFuel = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasShelter = false;
	UPROPERTY(BlueprintReadOnly) bool bShelterBuilt = false;
	UPROPERTY(BlueprintReadOnly) int32 ShelterProgress = 0;
	UPROPERTY(BlueprintReadOnly) int32 ShelterRequired = 0;
};

USTRUCT(BlueprintType)
struct FHearthWorldTime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Day = 1;
	UPROPERTY(BlueprintReadOnly) int32 Hour = 6;
	UPROPERTY(BlueprintReadOnly) int32 Minute = 0;
	UPROPERTY(BlueprintReadOnly) bool bIsNight = false;
};
