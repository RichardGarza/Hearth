// A named place from the brain's world: camp, forest, river... Spawned from world_init.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HearthTypes.h"
#include "HearthLocation.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

UCLASS()
class HEARTH_API AHearthLocation : public AActor
{
	GENERATED_BODY()

public:
	AHearthLocation();

	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString LocationId;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FHearthLocationInfo Latest;

	void Init(const FHearthLocationInfo& Info);
	void ApplyInfo(const FHearthLocationInfo& Info);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hearth") void OnFireChanged(bool bLit);
	UFUNCTION(BlueprintImplementableEvent, Category = "Hearth") void OnShelterChanged(bool bBuilt, int32 Progress, int32 Required);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UStaticMeshComponent> Marker;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UPointLightComponent> FireLight;

	bool bLastFireLit = false;
	bool bLastShelterBuilt = false;
	int32 LastShelterProgress = -1;
};
