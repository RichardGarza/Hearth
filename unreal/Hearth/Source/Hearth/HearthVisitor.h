// The player's body: a third-person walker. Same mannequin as everyone else so you belong in the world.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HearthVisitor.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAnimSequence;

UCLASS()
class HEARTH_API AHearthVisitor : public ACharacter
{
	GENERATED_BODY()

public:
	AHearthVisitor();

	UPROPERTY(EditAnywhere, Category = "Hearth") float WalkSpeed = 380.f;
	UPROPERTY(EditAnywhere, Category = "Hearth") float RunSpeed = 720.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere, Category = "Hearth") TObjectPtr<UCameraComponent> Camera;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	// Idle/walk driven from velocity (see HearthAnim.h)
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkAnim;
	bool bAnimWalking = false;
	void UpdateLocomotionAnim();

	void MoveForward(float Value);
	void MoveRight(float Value);
	void StartRun();
	void StopRun();
	void Turn(float Value);
	void LookUp(float Value);
	float LookEnabledAt = 0.f;   // ignore mouse deltas for the first moment (initial capture jumps)
};
