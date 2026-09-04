// The visitor. Fly around; near an AI character press SPACE to talk, type, ESC to leave.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"
#include "HearthPlayerController.generated.h"

class AHearthAgent;
class AHearthLocation;
class SOverlay;
class STextBlock;
class SEditableTextBox;
class SBox;
class SButton;
class SSlider;
class UHearthBridgeSubsystem;

UCLASS(Config = Game)
class HEARTH_API AHearthPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** How close (units) you must be to an AI character for the prompt to show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearth") float TalkRange = 700.f;

	/** Mouse look multiplier (0.25..1.1). Shown to the player as 1..10. Saved to the user's Game.ini. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Hearth") float MouseSensitivity = 0.35f;
	static constexpr float SensMin = 0.25f;
	static constexpr float SensMax = 1.1f;
	static float DisplayFromSens(float S) { return 1.f + (S - SensMin) / (SensMax - SensMin) * 9.f; }
	static float SensFromDisplay(float D) { return SensMin + (D - 1.f) / 9.f * (SensMax - SensMin); }

	/** Lumen global illumination + reflections. Off saves GPU/unified memory for the local model. Saved. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Hearth") bool bLumen = true;
	/** Voices (text-to-speech in the brain). Off = lines still appear as text. */
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bVoices = true;

	UFUNCTION(BlueprintCallable, Category = "Hearth") void SetLumen(bool bOn);
	UFUNCTION(BlueprintCallable, Category = "Hearth") void SetVoices(bool bOn);

	/** Seconds standing at a resource place before you pick one up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearth") float GatherSeconds = 10.f;

	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bInDialogue = false;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bMenuOpen = false;

	UFUNCTION(BlueprintCallable, Category = "Hearth") void OpenDialogue();
	UFUNCTION(BlueprintCallable, Category = "Hearth") void CloseDialogue();
	UFUNCTION(BlueprintCallable, Category = "Hearth") void OpenMenu();
	UFUNCTION(BlueprintCallable, Category = "Hearth") void CloseMenu();
	UFUNCTION(BlueprintCallable, Category = "Hearth") void QuitGame();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void SetupInputComponent() override;

private:
	TWeakObjectPtr<AHearthAgent> NearbyAI;       // closest AI character in range, if any
	TWeakObjectPtr<AHearthAgent> DialogueWith;
	FString Transcript;
	bool bWaitingForReply = false;

	TSharedPtr<SOverlay> RootWidget;
	TSharedPtr<STextBlock> PromptText;
	TSharedPtr<SBox> DialoguePanel;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> TranscriptText;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<SBox> MenuPanel;
	TSharedPtr<SSlider> SensitivitySlider;
	TSharedPtr<STextBlock> SensitivityText;
	FSliderStyle SensitivityStyle;
	void HandleSensitivityChanged(float Value);

	// gathering
	TSharedPtr<STextBlock> PlaceText;
	TSharedPtr<STextBlock> CarryText;
	TSharedPtr<STextBlock> ToastText;
	TWeakObjectPtr<AHearthLocation> NearbyPlace;
	float GatherTimer = 0.f;
	float ToastUntil = 0.f;
	bool bDepositedHere = false;
	void UpdateGathering(float DeltaSeconds);
	void RefreshCarryText();
	UFUNCTION() void HandleVisitorState(const FString& LastText);

	UHearthBridgeSubsystem* Bridge() const;
	void BuildWidgets();
	void UpdateNearby();
	void OnSpacePressed();
	void OnSpaceReleased();
	void OnEnterPressed();
	void OnCheatKey(FKey Key);
	FString CheatBuffer;
	float CheatLastKeyTime = 0.f;
	void OnEscapePressed();
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply HandleInputKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	void AppendLine(const FString& Line);
	FReply OnResumeClicked();
	FReply OnQuitClicked();
	FReply OnLumenClicked();
	FReply OnVoicesClicked();
	TSharedPtr<STextBlock> LumenText;
	TSharedPtr<STextBlock> VoicesText;
	void RefreshToggleTexts();

	UFUNCTION() void HandleReply(const FString& AgentId, const FString& Text);
};
