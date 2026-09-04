// One character per person in the brain's world. Walks where the brain says, shows what it says.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HearthTypes.h"
#include "HearthAgent.generated.h"

class UTextRenderComponent;

UCLASS()
class HEARTH_API AHearthAgent : public ACharacter
{
	GENERATED_BODY()

public:
	AHearthAgent();

	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString AgentId;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString AgentName;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FHearthAgentSnapshot Latest;

	/** Seconds a spoken line stays visible above the head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearth") float SpeechDisplaySeconds = 6.f;

	void Init(const FString& InId, const FString& InName);
	void ApplySnapshot(const FHearthAgentSnapshot& Snap, const FVector& TargetWorldPos, float WalkSpeedUnitsPerSec);
	void Say(const FString& Text, const FString& ToName);

	/** Hooks for Blueprint subclasses: play animations, spatial audio, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hearth") void OnSay(const FString& Text, const FString& ToName);
	UFUNCTION(BlueprintImplementableEvent, Category = "Hearth") void OnActionChanged(const FString& Action, const FString& Target);
	UFUNCTION(BlueprintImplementableEvent, Category = "Hearth") void OnDied();

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UTextRenderComponent> NameLabel;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UTextRenderComponent> SpeechLabel;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UTextRenderComponent> StatusLabel;

	FTimerHandle SpeechClearTimer;
	FString LastAction;
	bool bWasAlive = true;

	void ClearSpeech();
	void FaceCamera(UTextRenderComponent* Label) const;
};
