#include "HearthVisitor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "HearthAnim.h"
#include "HearthPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

AHearthVisitor::AHearthVisitor()
{
	PrimaryActorTick.bCanEverTick = true;

	// look with the mouse, body turns to face where you walk
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 480.f;    // SPACE jumps (when not next to an AI person)
	GetCharacterMovement()->AirControl = 0.25f;
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

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(HearthAnim::MannyPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleClip(HearthAnim::IdlePath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkClip(HearthAnim::WalkPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JogClip(HearthAnim::JogPath);
	IdleAnim = IdleClip.Succeeded() ? IdleClip.Object : nullptr;
	WalkAnim = WalkClip.Succeeded() ? WalkClip.Object : nullptr;
	JogAnim = JogClip.Succeeded() ? JogClip.Object : nullptr;
	HearthAnim::SetupBody(this, BodyMesh.Succeeded() ? BodyMesh.Object : nullptr);
}

void AHearthVisitor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	// The macOS game window can deliver one giant mouse delta the moment it first takes focus.
	// Arm the look guard from that moment, not from BeginPlay.
	if (!bSeenFocus && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		&& GEngine->GameViewport->Viewport->HasFocus())
	{
		bSeenFocus = true;
		LookEnabledAt = Now + 0.75f;
	}
	if (DanceUntil > 0.f)
	{
		TickDance(Now);
	}
	else
	{
		UpdateLocomotionAnim();
	}
}

void AHearthVisitor::UpdateLocomotionAnim()
{
	HearthAnim::Update(this, IdleAnim, WalkAnim, JogAnim, AnimState);
}

void AHearthVisitor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AHearthVisitor::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AHearthVisitor::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AHearthVisitor::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AHearthVisitor::LookUp);
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

void AHearthVisitor::BeginPlay()
{
	Super::BeginPlay();
	LookEnabledAt = GetWorld()->GetTimeSeconds() + 1.0f;
	MeshBaseOffset = GetMesh()->GetRelativeLocation();
}

void AHearthVisitor::StartDance(float Seconds)
{
	DanceUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

void AHearthVisitor::TickDance(float Now)
{
	USkeletalMeshComponent* M = GetMesh();
	if (Now < DanceUntil)
	{
		// pelvic thrust forward + bob + a slow spin, legs pumping via the jog clip
		const float T = DanceUntil - Now;
		const float Thrust = FMath::Max(0.f, FMath::Sin(Now * 9.f)) * 32.f;
		const float Bob = FMath::Abs(FMath::Sin(Now * 4.5f)) * 8.f;
		M->SetRelativeLocation(MeshBaseOffset + FVector(Thrust, 0.f, Bob));
		M->SetRelativeRotation(FRotator(0.f, -90.f, -12.f + 8.f * FMath::Sin(Now * 9.f)));
		AddActorWorldRotation(FRotator(0.f, 70.f * GetWorld()->GetDeltaSeconds(), 0.f));
		if (JogAnim && M->GetSingleNodeInstance() && M->GetSingleNodeInstance()->GetAnimationAsset() != JogAnim)
		{
			M->PlayAnimation(JogAnim, true);
			AnimState = 2;
		}
		M->SetPlayRate(1.6f);
		(void)T;
	}
	else if (DanceUntil > 0.f)
	{
		DanceUntil = 0.f;
		M->SetRelativeLocation(MeshBaseOffset);
		M->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		AnimState = -1;   // let the locomotion driver pick the right clip again
	}
}

// Mouse look with two guards: nothing for the first second (the window's initial mouse capture on
// macOS can deliver one giant delta that pitched the camera straight down), and no single-frame
// jumps bigger than a hand can make.
static float LookScale(const APawn* P)
{
	const AHearthPlayerController* PC = Cast<AHearthPlayerController>(P->GetController());
	return PC ? PC->MouseSensitivity : 0.35f;
}

void AHearthVisitor::Turn(float Value)
{
	if (!bSeenFocus || GetWorld()->GetTimeSeconds() < LookEnabledAt || FMath::Abs(Value) > 30.f) { return; }
	AddControllerYawInput(Value * LookScale(this));
}

void AHearthVisitor::LookUp(float Value)
{
	if (!bSeenFocus || GetWorld()->GetTimeSeconds() < LookEnabledAt || FMath::Abs(Value) > 30.f) { return; }
	AddControllerPitchInput(Value * LookScale(this));
}

void AHearthVisitor::StartRun() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void AHearthVisitor::StopRun() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }
