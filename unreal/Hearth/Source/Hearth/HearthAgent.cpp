#include "HearthAgent.h"
#include "Hearth.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "HearthAnim.h"

AHearthAgent::AHearthAgent()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 400.f;

	// Default body: the mannequin that ships with the engine, so people are visible before any
	// Fab/Sketchfab characters are imported. Override in a Blueprint subclass (BP_HearthAgent).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(HearthAnim::BodyMeshPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleClip(HearthAnim::IdlePath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkClip(HearthAnim::WalkPath);
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	IdleAnim = IdleClip.Succeeded() ? IdleClip.Object : nullptr;
	WalkAnim = WalkClip.Succeeded() ? WalkClip.Object : nullptr;

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(RootComponent);
	NameLabel->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetWorldSize(28.f);
	NameLabel->SetTextRenderColor(FColor::White);

	SpeechLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SpeechLabel"));
	SpeechLabel->SetupAttachment(RootComponent);
	SpeechLabel->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	SpeechLabel->SetHorizontalAlignment(EHTA_Center);
	SpeechLabel->SetWorldSize(22.f);
	SpeechLabel->SetTextRenderColor(FColor::Yellow);

	StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusLabel"));
	StatusLabel->SetupAttachment(RootComponent);
	StatusLabel->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	StatusLabel->SetHorizontalAlignment(EHTA_Center);
	StatusLabel->SetWorldSize(14.f);
	StatusLabel->SetTextRenderColor(FColor(180, 180, 180));

	// Marker for the one(s) driven by a real model: a floating "AI" tag and a soft cyan glow.
	AILabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AILabel"));
	AILabel->SetupAttachment(RootComponent);
	AILabel->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	AILabel->SetHorizontalAlignment(EHTA_Center);
	AILabel->SetWorldSize(34.f);
	AILabel->SetTextRenderColor(FColor::Cyan);
	AILabel->SetText(FText::FromString(TEXT("[ AI ]")));
	AILabel->SetVisibility(false);

	AIGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("AIGlow"));
	AIGlow->SetupAttachment(RootComponent);
	AIGlow->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	AIGlow->SetLightColor(FLinearColor(0.2f, 0.9f, 1.f));
	AIGlow->SetIntensity(3000.f);
	AIGlow->SetAttenuationRadius(500.f);
	AIGlow->SetVisibility(false);
}

void AHearthAgent::Init(const FString& InId, const FString& InName, bool bInIsAI)
{
	AgentId = InId;
	AgentName = InName;
	bIsAI = bInIsAI;
	NameLabel->SetText(FText::FromString(InName));
	if (bIsAI)
	{
		NameLabel->SetTextRenderColor(FColor::Cyan);
		AILabel->SetVisibility(true);
		AIGlow->SetVisibility(true);
	}
	SpeechLabel->SetText(FText::GetEmpty());
	StatusLabel->SetText(FText::GetEmpty());
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Agent_%s"), *InName));
#endif
}

void AHearthAgent::ApplySnapshot(const FHearthAgentSnapshot& Snap, const FVector& TargetWorldPos, float WalkSpeedUnitsPerSec)
{
	const bool bActionChanged = Snap.Action != LastAction;
	Latest = Snap;

	if (!Snap.bAlive)
	{
		if (bWasAlive)
		{
			bWasAlive = false;
			StatusLabel->SetText(FText::FromString(TEXT("(dead)")));
			NameLabel->SetTextRenderColor(FColor(120, 120, 120));
			if (AAIController* AI = Cast<AAIController>(GetController()))
			{
				AI->StopMovement();
			}
			OnDied();
		}
		return;
	}

	// Status line under the name: what they're doing + the need that matters most.
	FString Status = Snap.Action.IsEmpty() ? TEXT("idle") : Snap.Action;
	if (!Snap.ActionTarget.IsEmpty()) { Status += TEXT(" ") + Snap.ActionTarget; }
	const float Worst = FMath::Min(FMath::Min(Snap.Needs.Hunger, Snap.Needs.Thirst), FMath::Min(Snap.Needs.Energy, Snap.Needs.Warmth));
	if (Worst < 30.f)
	{
		Status += Snap.Needs.Thirst == Worst ? TEXT(" | thirsty") : Snap.Needs.Hunger == Worst ? TEXT(" | hungry")
			: Snap.Needs.Warmth == Worst ? TEXT(" | cold") : TEXT(" | exhausted");
	}
	StatusLabel->SetText(FText::FromString(Status));

	if (bActionChanged)
	{
		LastAction = Snap.Action;
		OnActionChanged(Snap.Action, Snap.ActionTarget);
	}

	// Movement: the brain gives us where they are heading; walk there with the nav system.
	GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(WalkSpeedUnitsPerSec, 150.f, 3000.f);
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const float Dist2D = FVector::Dist2D(GetActorLocation(), TargetWorldPos);
		if (Dist2D > 120.f)
		{
			if (AI->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				AI->MoveToLocation(TargetWorldPos, 60.f, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true);
			}
		}
	}
}

void AHearthAgent::Say(const FString& Text, const FString& ToName)
{
	const FString Shown = ToName.IsEmpty() ? Text : FString::Printf(TEXT("(to %s) %s"), *ToName, *Text);
	SpeechLabel->SetText(FText::FromString(Shown));
	GetWorldTimerManager().SetTimer(SpeechClearTimer, this, &AHearthAgent::ClearSpeech, SpeechDisplaySeconds, false);
	OnSay(Text, ToName);
}

void AHearthAgent::ClearSpeech()
{
	SpeechLabel->SetText(FText::GetEmpty());
}

void AHearthAgent::FaceCamera(UTextRenderComponent* Label) const
{
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - Label->GetComponentLocation();
		Label->SetWorldRotation(FRotationMatrix::MakeFromX(ToCam).Rotator());
	}
}

void AHearthAgent::UpdateLocomotionAnim()
{
	HearthAnim::Update(this, IdleAnim, WalkAnim, bAnimWalking);
}

void AHearthAgent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLocomotionAnim();
	FaceCamera(NameLabel);
	FaceCamera(SpeechLabel);
	FaceCamera(StatusLabel);
	if (bIsAI)
	{
		FaceCamera(AILabel);
		// gentle bob so the tag catches the eye
		AILabel->SetRelativeLocation(FVector(0.f, 0.f, 200.f + 8.f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.f)));
	}
}
