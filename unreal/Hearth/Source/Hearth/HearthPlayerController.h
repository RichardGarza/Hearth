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
class UHearthBridgeSubsystem;

UCLASS()
class HEARTH_API AHearthPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** How close (units) you must be to an AI character for the prompt to show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearth") float TalkRange = 700.f;

	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bInDialogue = false;

	UFUNCTION(BlueprintCallable, Category = "Hearth") void OpenDialogue();
	UFUNCTION(BlueprintCallable, Category = "Hearth") void CloseDialogue();

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

	UHearthBridgeSubsystem* Bridge() const;
	void BuildWidgets();
	void UpdateNearby();
	void OnSpacePressed();
	void OnEscapePressed();
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply HandleInputKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	void AppendLine(const FString& Line);

	UFUNCTION() void HandleReply(const FString& AgentId, const FString& Text);
};
