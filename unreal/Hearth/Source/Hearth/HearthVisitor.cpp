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
	// Jump feel: ~15% higher than the stock 480 (height ~ v^2), rise at normal gravity, fall ~10% faster
	// (see Tick: gravity scale switches to FallGravity once we're heading down)
	GetCharacterMovement()->JumpZVelocity = 515.f;    // SPACE jumps
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->GravityScale = RiseGravity;
	JumpMaxHoldTime = 0.12f;  // ACharacter property: tap = shorter hop, hold = full height

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
	// asymmetric gravity: heavier on the way down so the landing feels grounded, not floaty
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = (Move->IsFalling() && Move->Velocity.Z < 0.f) ? FallGravity : RiseGravity;
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
		// Legs frozen in the idle pose so the pelvis is the only thing moving.
		if (IdleAnim && M->GetSingleNodeInstance() && M->GetSingleNodeInstance()->GetAnimationAsset() != IdleAnim)
		{
			M->PlayAnimation(IdleAnim, true);
			AnimState = 0;
		}
		M->SetPlayRate(0.f);

		// Sharp pelvic thrusts at 2.5 Hz: hips forward + lean back, then snap back.
		const float Phase = FMath::Fmod(Now * 2.5f, 1.f);                       // 0..1 per thrust
		const float Thrust = FMath::Pow(FMath::Max(0.f, FMath::Sin(Phase * PI)), 0.5f);   // fast in, fast out
		const float Forward = 55.f * Thrust;                                       // hips out
		const float Dip = -14.f * Thrust;                                          // knees bend
		const float LeanBack = 28.f * Thrust;                                      // degrees
		M->SetRelativeLocation(MeshBaseOffset + FVector(Forward, 0.f, Dip));
		const FQuat Base(FRotator(0.f, -90.f, 0.f));
		const FQuat Lean(FRotator(-LeanBack, 0.f, 0.f));                          // pitch back in actor space
		M->SetRelativeRotation((Lean * Base).Rotator());

		// A half-turn every 2.4 s so it reads as a dance, not a pivot in place.
		const float Cycle = FMath::Fmod(Now, 2.4f);
		if (Cycle < 0.4f)
		{
			AddActorWorldRotation(FRotator(0.f, 180.f / 0.4f * GetWorld()->GetDeltaSeconds(), 0.f));
		}
	}
	else if (DanceUntil > 0.f)
	{
		DanceUntil = 0.f;
		M->SetRelativeLocation(MeshBaseOffset);
		M->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		M->SetPlayRate(1.f);
		AnimState = -1;   // let the locomotion driver pick the right clip again
	}
}

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
