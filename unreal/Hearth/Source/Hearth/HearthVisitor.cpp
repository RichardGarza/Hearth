#include "HearthVisitor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AHearthVisitor::AHearthVisitor()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 88.f);

	// look with the mouse, body turns to face where you walk
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 0.f;      // no jumping, no flying
	GetCharacterMovement()->AirControl = 0.f;
	GetCharacterMovement()->GravityScale = 1.f;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 380.f;
	Boom->SocketOffset = FVector(0.f, 0.f, 70.f);
	Boom->bUsePawnControlRotation = true;
	Boom->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_AnimBlueprint"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		if (BodyAnim.Succeeded())
		{
			GetMesh()->SetAnimInstanceClass(BodyAnim.Class);
		}
	}
}

void AHearthVisitor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AHearthVisitor::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AHearthVisitor::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AHearthVisitor::StartRun);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AHearthVisitor::StopRun);
}

void AHearthVisitor::MoveForward(float Value)
{
	if (Controller && Value != 0.f)
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value);
	}
}

void AHearthVisitor::MoveRight(float Value)
{
	if (Controller && Value != 0.f)
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Value);
	}
}

void AHearthVisitor::StartRun() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void AHearthVisitor::StopRun() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }
