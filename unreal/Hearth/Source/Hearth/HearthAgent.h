// One character per person in the brain's world. Walks where the brain says, shows what it says.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HearthTypes.h"
#include "HearthAgent.generated.h"

class UTextRenderComponent;
class UPointLightComponent;
class UAnimSequence;

UCLASS()
class HEARTH_API AHearthAgent : public ACharacter
{
	GENERATED_BODY()

public:
	AHearthAgent();

	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString AgentId;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString AgentName;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FHearthAgentSnapshot Latest;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bIsAI = false;

	/** Seconds a spoken line stays visible above the head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearth") float SpeechDisplaySeconds = 6.f;

	void Init(const FString& InId, const FString& InName, bool bInIsAI);
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
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UTextRenderComponent> AILabel;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UPointLightComponent> AIGlow;

	FTimerHandle SpeechClearTimer;
	FString LastAction;
	bool bWasAlive = true;

	void ClearSpeech();
	void FaceCamera(UTextRenderComponent* Label) const;
	// Idle/walk driven from velocity (see HearthAnim.h)
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkAnim;
	bool bAnimWalking = false;
	void UpdateLocomotionAnim();

};
