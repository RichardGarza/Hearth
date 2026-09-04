// The visitor. Fly around; near an AI character press SPACE to talk, type, ESC to leave.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HearthPlayerController.generated.h"

class AHearthAgent;
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

	/** Mouse look multiplier. Saved to the user's Game.ini when changed from the menu. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Hearth") float MouseSensitivity = 0.35f;

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
	void HandleSensitivityChanged(float Value);

	UHearthBridgeSubsystem* Bridge() const;
	void BuildWidgets();
	void UpdateNearby();
	void OnSpacePressed();
	void OnEscapePressed();
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply HandleInputKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	void AppendLine(const FString& Line);
	FReply OnResumeClicked();
	FReply OnQuitClicked();

	UFUNCTION() void HandleReply(const FString& AgentId, const FString& Text);
};
