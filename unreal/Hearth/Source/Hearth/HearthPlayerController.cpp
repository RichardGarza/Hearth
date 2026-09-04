#include "HearthPlayerController.h"
#include "Hearth.h"
#include "HearthAgent.h"
#include "HearthVisitor.h"
#include "GameFramework/Character.h"
#include "HearthLocation.h"
#include "SHearthMinimap.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
#include "HAL/IConsoleManager.h"
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
	SetLumen(bLumen);   // apply the saved choice
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		B->OnReply.AddDynamic(this, &AHearthPlayerController::HandleReply);
		B->OnVisitorState.AddDynamic(this, &AHearthPlayerController::HandleVisitorState);
	}
	RefreshCarryText();
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
	Space.bConsumeInput = true;   // SPACE = jump
	FInputKeyBinding& SpaceUp = InputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AHearthPlayerController::OnSpaceReleased);
	SpaceUp.bConsumeInput = true;
	FInputKeyBinding& Enter = InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AHearthPlayerController::OnEnterPressed);
	Enter.bConsumeInput = false;  // ENTER = talk to a nearby AI person
	// cheat codes: letters typed while playing (not in the chat box)
	for (const FKey& K : {EKeys::H, EKeys::U, EKeys::M, EKeys::P, EKeys::D, EKeys::A, EKeys::N, EKeys::C, EKeys::E})
	{
		FInputKeyBinding& KB = InputComponent->BindKey(K, IE_Pressed, this, &AHearthPlayerController::OnCheatKey);   // handler receives the FKey
		KB.bConsumeInput = false;
	}
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
	SensitivityStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
	SensitivityStyle.SetBarThickness(12.f);
	SensitivityStyle.SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.22f, 0.22f, 1.f), 6.f));
	SensitivityStyle.SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(0.30f, 0.30f, 0.30f, 1.f), 6.f));
	SensitivityStyle.SetDisabledBarImage(FSlateRoundedBoxBrush(FLinearColor(0.15f, 0.15f, 0.15f, 1.f), 6.f));
	SensitivityStyle.SetNormalThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.75f, 0.35f, 1.f), 14.f, FVector2f(28.f, 28.f)));
	SensitivityStyle.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.85f, 0.55f, 1.f), 14.f, FVector2f(28.f, 28.f)));
	SensitivityStyle.SetDisabledThumbImage(FSlateRoundedBoxBrush(FLinearColor(0.5f, 0.5f, 0.5f, 1.f), 14.f, FVector2f(28.f, 28.f)));

	SAssignNew(RootWidget, SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 20.f, 24.f, 0.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f)).ShadowOffset(FVector2D(1.f, 1.f))
			.Text(FText::FromString(TEXT("VALLEY  ·  N up")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHearthMinimap).Size(230.f).WorldRange(6800.f).World(GetWorld())
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f, 20.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(PlaceText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.9f))
			.ShadowOffset(FVector2D(1.f, 1.f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SAssignNew(CarryText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
			.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 0.9f))
			.ShadowOffset(FVector2D(1.f, 1.f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)
		[
			SAssignNew(ToastText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(FLinearColor(0.6f, 1.f, 0.6f))
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Visibility(EVisibility::Collapsed)
		]
	]
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
					.ClearKeyboardFocusOnCommit(false)
					.SelectAllTextWhenFocused(false)
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
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(20.f, 10.f))
					.OnClicked(FOnClicked::CreateUObject(this, &AHearthPlayerController::OnLumenClicked))
					[
						SAssignNew(LumenText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(20.f, 10.f))
					.OnClicked(FOnClicked::CreateUObject(this, &AHearthPlayerController::OnVoicesClicked))
					[
						SAssignNew(VoicesText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 4.f)
				[
					SAssignNew(SensitivityText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
					.Text(FText::FromString(FString::Printf(TEXT("Look sensitivity: %.1f"), DisplayFromSens(MouseSensitivity))))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(36.f)
					[
						SAssignNew(SensitivitySlider, SSlider)
						.Style(&SensitivityStyle)
						.MinValue(1.f).MaxValue(10.f)
						.StepSize(0.5f).MouseUsesStep(true)
						.Value(DisplayFromSens(MouseSensitivity))
						.OnValueChanged(FOnFloatValueChanged::CreateUObject(this, &AHearthPlayerController::HandleSensitivityChanged))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)
					[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)).Text(FText::FromString(TEXT("1  slow"))) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)
					[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)).Text(FText::FromString(TEXT("fast  10"))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text(FText::FromString(TEXT("ESC resume   ·   ENTER near [ AI ] to talk   ·   SPACE jump   ·   Shift run")))
				]
			]
		]
	];
	GEngine->GameViewport->AddViewportWidgetContent(SNew(SWeakWidget).PossiblyNullContent(RootWidget.ToSharedRef()), 10);
	RefreshToggleTexts();
}

// ---------------------------------------------------------------- toggles

static void SetCVarInt(const TCHAR* Name, int32 Value)
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		Var->Set(Value, ECVF_SetByGameSetting);
	}
}

void AHearthPlayerController::SetLumen(bool bOn)
{
	bLumen = bOn;
	// 1 = Lumen, 0 = none (no dynamic GI / screen-space-free reflections). Takes effect immediately.
	SetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"), bOn ? 1 : 0);
	SetCVarInt(TEXT("r.ReflectionMethod"), bOn ? 1 : 0);
	SetCVarInt(TEXT("r.Lumen.Reflections.Allow"), bOn ? 1 : 0);
	SaveConfig();
	RefreshToggleTexts();
	UE_LOG(LogHearth, Log, TEXT("Lumen %s"), bOn ? TEXT("on") : TEXT("off"));
}

void AHearthPlayerController::SetVoices(bool bOn)
{
	bVoices = bOn;
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		B->SendCommand(bOn ? TEXT("unmute") : TEXT("mute"));
	}
	RefreshToggleTexts();
}

void AHearthPlayerController::RefreshToggleTexts()
{
	if (LumenText.IsValid())
	{
		LumenText->SetText(FText::FromString(bLumen ? TEXT("Lighting: Lumen ON  (click to turn off, saves memory)") : TEXT("Lighting: Lumen OFF  (click to turn on)")));
	}
	if (VoicesText.IsValid())
	{
		VoicesText->SetText(FText::FromString(bVoices ? TEXT("Voices: ON  (click to mute)") : TEXT("Voices: OFF  (click to unmute)")));
	}
}

FReply AHearthPlayerController::OnLumenClicked()
{
	SetLumen(!bLumen);
	return FReply::Handled();
}

FReply AHearthPlayerController::OnVoicesClicked()
{
	SetVoices(!bVoices);
	return FReply::Handled();
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
	UpdateGathering(DeltaSeconds);
	if (ToastText.IsValid() && ToastUntil > 0.f && GetWorld()->GetTimeSeconds() > ToastUntil)
	{
		ToastText->SetVisibility(EVisibility::Collapsed);
		ToastUntil = 0.f;
	}
	if (PromptText.IsValid())
	{
		const bool bShow = !bInDialogue && NearbyAI.IsValid();
		PromptText->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		if (bShow)
		{
			PromptText->SetText(FText::FromString(FString::Printf(TEXT("Press ENTER to talk to %s"), *NearbyAI->AgentName)));
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
	if (bInDialogue || bMenuOpen)
	{
		return;
	}
	if (ACharacter* C = Cast<ACharacter>(GetPawn()))
	{
		C->Jump();
	}
}

void AHearthPlayerController::OnEnterPressed()
{
	if (!bInDialogue && !bMenuOpen && NearbyAI.IsValid())
	{
		OpenDialogue();
	}
}

void AHearthPlayerController::OnCheatKey(FKey Key)
{
	if (bInDialogue || bMenuOpen || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - CheatLastKeyTime > 2.f)
	{
		CheatBuffer.Empty();
	}
	CheatLastKeyTime = Now;
	CheatBuffer += Key.GetFName().ToString().ToLower();
	if (CheatBuffer.Len() > 8)
	{
		CheatBuffer.RightInline(8);
	}
	AHearthVisitor* V = Cast<AHearthVisitor>(GetPawn());
	if (V && CheatBuffer.EndsWith(TEXT("hump")))
	{
		CheatBuffer.Empty();
		V->StartDance(6.f);
		if (ToastText.IsValid())
		{
			ToastText->SetText(FText::FromString(TEXT("cheat: hump")));
			ToastText->SetVisibility(EVisibility::HitTestInvisible);
			ToastUntil = Now + 2.f;
		}
	}
}

void AHearthPlayerController::OnSpaceReleased()
{
	if (ACharacter* C = Cast<ACharacter>(GetPawn()))
	{
		C->StopJumping();
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
	const float Display = FMath::RoundToFloat(Value * 2.f) / 2.f;   // 1.0, 1.5, ... 10.0
	MouseSensitivity = SensFromDisplay(Display);
	if (SensitivityText.IsValid())
	{
		SensitivityText->SetText(FText::FromString(FString::Printf(TEXT("Look sensitivity: %.1f"), Display)));
	}
	SaveConfig();   // persists to Saved/Config/<platform>/Game.ini
}

// ---------------------------------------------------------------- gathering

static const TCHAR* PrimaryResourceFor(const FString& LocationId)
{
	if (LocationId == TEXT("forest")) return TEXT("wood");
	if (LocationId == TEXT("river")) return TEXT("fish");
	if (LocationId == TEXT("meadow")) return TEXT("berries");
	if (LocationId == TEXT("quarry")) return TEXT("stone");
	return nullptr;
}

void AHearthPlayerController::UpdateGathering(float DeltaSeconds)
{
	const APawn* P = GetPawn();
	UHearthBridgeSubsystem* B = Bridge();
	if (!P || !B || !GetWorld() || !PlaceText.IsValid()) { return; }

	// nearest place within ~6 m
	AHearthLocation* Best = nullptr;
	float BestDist = 600.f;
	for (TActorIterator<AHearthLocation> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(P->GetActorLocation(), It->GetActorLocation());
		if (D < BestDist) { BestDist = D; Best = *It; }
	}
	if (Best != NearbyPlace.Get())
	{
		NearbyPlace = Best;
		GatherTimer = 0.f;
		bDepositedHere = false;
	}

	if (!NearbyPlace.IsValid())
	{
		PlaceText->SetText(FText::GetEmpty());
		return;
	}
	const FString& Id = NearbyPlace->LocationId;
	const FString& Name = NearbyPlace->Latest.Name;

	if (Id == TEXT("camp"))
	{
		PlaceText->SetText(FText::FromString(TEXT("At camp")));
		if (!bDepositedHere && B->VisitorInventory.Num() > 0)
		{
			bDepositedHere = true;
			B->SendVisitorDeposit();
		}
		return;
	}

	const TCHAR* Res = PrimaryResourceFor(Id);
	if (!Res)
	{
		PlaceText->SetText(FText::FromString(FString::Printf(TEXT("At the %s"), *Name)));
		return;
	}
	GatherTimer += DeltaSeconds;
	const float Left = FMath::Max(0.f, GatherSeconds - GatherTimer);
	PlaceText->SetText(FText::FromString(FString::Printf(TEXT("At the %s — gathering %s… %.0fs"), *Name, Res, FMath::CeilToFloat(Left))));
	if (GatherTimer >= GatherSeconds)
	{
		GatherTimer = 0.f;
		B->SendVisitorGather(Id);
	}
}

void AHearthPlayerController::RefreshCarryText()
{
	UHearthBridgeSubsystem* B = Bridge();
	if (!B || !CarryText.IsValid()) { return; }
	FString Items;
	for (const auto& Pair : B->VisitorInventory)
	{
		if (Pair.Value > 0)
		{
			Items += (Items.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("%s %d"), *Pair.Key, Pair.Value);
		}
	}
	CarryText->SetText(FText::FromString(Items.IsEmpty() ? TEXT("Carrying nothing") : TEXT("Carrying: ") + Items));
}

void AHearthPlayerController::HandleVisitorState(const FString& LastText)
{
	RefreshCarryText();
	if (!LastText.IsEmpty() && ToastText.IsValid() && GetWorld())
	{
		ToastText->SetText(FText::FromString(LastText));
		ToastText->SetVisibility(EVisibility::HitTestInvisible);
		ToastUntil = GetWorld()->GetTimeSeconds() + 3.f;
	}
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		if (B->VisitorInventory.Num() == 0) { bDepositedHere = false; }
	}
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
