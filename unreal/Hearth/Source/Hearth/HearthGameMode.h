// Spawns and drives the visible world from the bridge's state. Set as the level's GameMode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HearthGameMode.generated.h"

class AHearthAgent;
class AHearthLocation;
class UHearthBridgeSubsystem;

UCLASS()
class HEARTH_API AHearthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHearthGameMode();

	/** Override with Blueprint subclasses (e.g. BP_HearthAgent with a skeletal mesh). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hearth") TSubclassOf<AHearthAgent> AgentClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hearth") TSubclassOf<AHearthLocation> LocationClass;

	/** How far agents at the same place stand from its center (units). */
	UPROPERTY(EditDefaultsOnly, Category = "Hearth") float GatherRadius = 250.f;

	/** Height to trace down from when placing things on the ground. */
	UPROPERTY(EditDefaultsOnly, Category = "Hearth") float GroundTraceHeight = 10000.f;

	virtual void BeginPlay() override;

protected:
	UPROPERTY() TMap<FString, TObjectPtr<AHearthAgent>> AgentActors;
	UPROPERTY() TMap<FString, TObjectPtr<AHearthLocation>> LocationActors;

	UFUNCTION() void HandleWorldInit();
	UFUNCTION() void HandleSnapshot();
	UFUNCTION() void HandleSpeech(const FString& AgentId, const FString& ToAgentId, const FString& Text);
	UFUNCTION() void HandleEvent(const FString& Kind, const FString& Text, const FString& AgentId, const FString& LocationId);

	UHearthBridgeSubsystem* Bridge() const;
	FVector Ground(const FVector& XY) const;
	FVector StandingSpot(const FString& LocationId, int32 AgentIndex) const;
};
