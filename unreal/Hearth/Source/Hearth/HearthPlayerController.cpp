#include "HearthPlayerController.h"
#include "Hearth.h"
#include "HearthAgent.h"
#include "HearthBridgeSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Text/STextBlock.h"

UHearthBridgeSubsystem* AHearthPlayerController::Bridge() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHearthBridgeSubsystem>() : nullptr;
}

void AHearthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -55.f;
		PlayerCameraManager->ViewPitchMax = 35.f;
	}
	BuildWidgets();
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		B->OnReply.AddDynamic(this, &AHearthPlayerController::HandleReply);
	}
}

void AHearthPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bInDialogue)
	{
		CloseDialogue();
	}
	if (GEngine && GEngine->GameViewport && RootWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
	}
	Super::EndPlay(EndPlayReason);
}

void AHearthPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	FInputKeyBinding& Space = InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHearthPlayerController::OnSpacePressed);
	Space.bConsumeInput = true;   // keep the spectator pawn from flying up on the same key
	FInputKeyBinding& Esc = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHearthPlayerController::OnEscapePressed);
	Esc.bConsumeInput = true;
	Esc.bExecuteWhenPaused = true;
}

// ---------------------------------------------------------------- widgets

void AHearthPlayerController::BuildWidgets()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	SAssignNew(RootWidget, SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 80.f)
	[
		SAssignNew(PromptText, STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
		.ColorAndOpacity(FLinearColor(0.6f, 1.f, 1.f))
		.ShadowOffset(FVector2D(1.f, 1.f))
		.Visibility(EVisibility::Collapsed)
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 40.f)
	[
		SAssignNew(DialoguePanel, SBox).WidthOverride(960.f).Visibility(EVisibility::Collapsed)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.Padding(FMargin(16.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(TitleText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
					.ColorAndOpacity(FLinearColor(0.6f, 1.f, 1.f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f)
				[
					SAssignNew(TranscriptText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
					.ColorAndOpacity(FLinearColor::White)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(InputBox, SEditableTextBox)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
					.HintText(FText::FromString(TEXT("Type and press Enter. ESC to walk away.")))
					.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &AHearthPlayerController::HandleTextCommitted))
					.OnKeyDownHandler(FOnKeyDown::CreateUObject(this, &AHearthPlayerController::HandleInputKeyDown))
				]
			]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SAssignNew(MenuPanel, SBox).WidthOverride(420.f).Visibility(EVisibility::Collapsed)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			.Padding(FMargin(28.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 18.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
					.ColorAndOpacity(FLinearColor(1.f, 0.75f, 0.35f))
					.Text(FText::FromString(TEXT("Hearth")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(20.f, 12.f))
					.OnClicked(FOnClicked::CreateUObject(this, &AHearthPlayerController::OnResumeClicked))
					[
						SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 18)).Text(FText::FromString(TEXT("Resume")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(20.f, 12.f))
					.OnClicked(FOnClicked::CreateUObject(this, &AHearthPlayerController::OnQuitClicked))
					[
						SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 18)).Text(FText::FromString(TEXT("Quit to Desktop")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 4.f)
				[
					SAssignNew(SensitivityText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
					.Text(FText::FromString(FString::Printf(TEXT("Look sensitivity: %.2f"), MouseSensitivity)))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SensitivitySlider, SSlider)
					.MinValue(0.05f).MaxValue(1.5f)
					.Value(MouseSensitivity)
					.OnValueChanged(FOnFloatValueChanged::CreateUObject(this, &AHearthPlayerController::HandleSensitivityChanged))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text(FText::FromString(TEXT("ESC to resume   ·   SPACE near [ AI ] to talk   ·   Shift to run")))
				]
			]
		]
	];
	GEngine->GameViewport->AddViewportWidgetContent(SNew(SWeakWidget).PossiblyNullContent(RootWidget.ToSharedRef()), 10);
}

// ---------------------------------------------------------------- proximity

void AHearthPlayerController::UpdateNearby()
{
	const APawn* P = GetPawn();
	if (!P || !GetWorld())
	{
		NearbyAI = nullptr;
		return;
	}
	const FVector Me = P->GetActorLocation();
	AHearthAgent* Best = nullptr;
	float BestDist = TalkRange;
	for (TActorIterator<AHearthAgent> It(GetWorld()); It; ++It)
	{
		if (!It->bIsAI || !It->Latest.bAlive) { continue; }
		const float D = FVector::Dist(Me, It->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	NearbyAI = Best;
}

void AHearthPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateNearby();
	if (PromptText.IsValid())
	{
		const bool bShow = !bInDialogue && NearbyAI.IsValid();
		PromptText->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		if (bShow)
		{
			PromptText->SetText(FText::FromString(FString::Printf(TEXT("Press SPACE to talk to %s"), *NearbyAI->AgentName)));
		}
	}
	// walked away mid-conversation
	if (bInDialogue && DialogueWith.IsValid() && GetPawn()
		&& FVector::Dist(GetPawn()->GetActorLocation(), DialogueWith->GetActorLocation()) > TalkRange * 2.f)
	{
		AppendLine(TEXT("(you walked away)"));
		CloseDialogue();
	}
}

// ---------------------------------------------------------------- dialogue

void AHearthPlayerController::OnSpacePressed()
{
	if (!bInDialogue && !bMenuOpen && NearbyAI.IsValid())
	{
		OpenDialogue();
	}
}

void AHearthPlayerController::OnEscapePressed()
{
	if (bInDialogue)
	{
		CloseDialogue();
	}
	else if (bMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

// ---------------------------------------------------------------- escape menu

void AHearthPlayerController::OpenMenu()
{
	if (!MenuPanel.IsValid() || bMenuOpen)
	{
		return;
	}
	bMenuOpen = true;
	MenuPanel->SetVisibility(EVisibility::Visible);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void AHearthPlayerController::CloseMenu()
{
	if (!bMenuOpen)
	{
		return;
	}
	bMenuOpen = false;
	if (MenuPanel.IsValid())
	{
		MenuPanel->SetVisibility(EVisibility::Collapsed);
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AHearthPlayerController::QuitGame()
{
	UE_LOG(LogHearth, Log, TEXT("Quit requested from menu"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, true);
	// if the console route didn't take (packaged/-game edge cases), force the process to exit
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, []() { FPlatformMisc::RequestExit(false); }, 1.0f, false);
}

void AHearthPlayerController::HandleSensitivityChanged(float Value)
{
	MouseSensitivity = Value;
	if (SensitivityText.IsValid())
	{
		SensitivityText->SetText(FText::FromString(FString::Printf(TEXT("Look sensitivity: %.2f"), MouseSensitivity)));
	}
	SaveConfig();   // persists to Saved/Config/<platform>/Game.ini
}

FReply AHearthPlayerController::OnResumeClicked()
{
	CloseMenu();
	return FReply::Handled();
}

FReply AHearthPlayerController::OnQuitClicked()
{
	QuitGame();
	return FReply::Handled();
}

void AHearthPlayerController::OpenDialogue()
{
	if (!NearbyAI.IsValid() || !DialoguePanel.IsValid())
	{
		return;
	}
	CloseMenu();
	DialogueWith = NearbyAI;
	bInDialogue = true;
	Transcript.Empty();
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("Talking to %s"), *DialogueWith->AgentName)));
	TranscriptText->SetText(FText::GetEmpty());
	InputBox->SetText(FText::GetEmpty());
	DialoguePanel->SetVisibility(EVisibility::Visible);

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(InputBox);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
	UE_LOG(LogHearth, Log, TEXT("Dialogue opened with %s"), *DialogueWith->AgentId);
}

void AHearthPlayerController::CloseDialogue()
{
	if (!bInDialogue)
	{
		return;
	}
	bInDialogue = false;
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		if (DialogueWith.IsValid())
		{
			B->SendTalkEnd(DialogueWith->AgentId);
		}
	}
	if (DialoguePanel.IsValid())
	{
		DialoguePanel->SetVisibility(EVisibility::Collapsed);
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	DialogueWith = nullptr;
	bWaitingForReply = false;
	UE_LOG(LogHearth, Log, TEXT("Dialogue closed"));
}

void AHearthPlayerController::AppendLine(const FString& Line)
{
	if (!Transcript.IsEmpty())
	{
		Transcript += TEXT("\n");
	}
	Transcript += Line;
	// keep the box from growing forever: last ~12 lines
	TArray<FString> Lines;
	Transcript.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() > 12)
	{
		Lines.RemoveAt(0, Lines.Num() - 12);
		Transcript = FString::Join(Lines, TEXT("\n"));
	}
	if (TranscriptText.IsValid())
	{
		TranscriptText->SetText(FText::FromString(Transcript));
	}
}

void AHearthPlayerController::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter || !bInDialogue || !DialogueWith.IsValid())
	{
		return;
	}
	const FString Line = Text.ToString().TrimStartAndEnd();
	if (Line.IsEmpty())
	{
		return;
	}
	AppendLine(TEXT("You: ") + Line);
	InputBox->SetText(FText::GetEmpty());
	bWaitingForReply = true;
	AppendLine(FString::Printf(TEXT("%s: ..."), *DialogueWith->AgentName));
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		B->SendTalk(DialogueWith->AgentId, Line);
	}
	FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
}

FReply AHearthPlayerController::HandleInputKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialogue();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void AHearthPlayerController::HandleReply(const FString& AgentId, const FString& Text)
{
	if (!bInDialogue || !DialogueWith.IsValid() || DialogueWith->AgentId != AgentId)
	{
		return;
	}
	// replace the "..." placeholder with the real answer
	if (bWaitingForReply && Transcript.EndsWith(TEXT(": ...")))
	{
		int32 Idx;
		if (Transcript.FindLastChar(TEXT('\n'), Idx))
		{
			Transcript.LeftInline(Idx);
		}
		else
		{
			Transcript.Empty();
		}
	}
	bWaitingForReply = false;
	AppendLine(FString::Printf(TEXT("%s: %s"), *DialogueWith->AgentName, *Text));
}
