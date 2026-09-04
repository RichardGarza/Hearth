// WebSocket client to the Python brain. Keeps the latest world state and fires delegates
// when new frames arrive. Nothing here decides anything; the brain is authoritative.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HearthTypes.h"
#include "HearthBridgeSubsystem.generated.h"

class IWebSocket;
class FJsonObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHearthWorldInitDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHearthSnapshotDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHearthSpeechDelegate, const FString&, AgentId, const FString&, ToAgentId, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHearthEventDelegate, const FString&, Kind, const FString&, Text, const FString&, AgentId, const FString&, LocationId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHearthConnectionDelegate, bool, bConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHearthReplyDelegate, const FString&, AgentId, const FString&, Text);

UCLASS(Config = Game)
class HEARTH_API UHearthBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** ws://host:port of the running brain. Set in DefaultGame.ini or override at runtime. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Hearth")
	FString BrainUrl = TEXT("ws://127.0.0.1:8765");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Hearth")
	bool bAutoConnect = true;

	UFUNCTION(BlueprintCallable, Category = "Hearth")
	void Connect();

	UFUNCTION(BlueprintCallable, Category = "Hearth")
	void Disconnect();

	UFUNCTION(BlueprintPure, Category = "Hearth")
	bool IsConnected() const;

	/** Send a god-mode command, e.g. SendCommand("storm") or SendCommand("whisper", "{\"agent\":\"mara\",\"text\":\"...\"}"). */
	UFUNCTION(BlueprintCallable, Category = "Hearth")
	void SendCommand(const FString& Name, const FString& ExtraJson = "");

	/** Typed dialogue with an AI character. The answer arrives on OnReply. */
	UFUNCTION(BlueprintCallable, Category = "Hearth")
	void SendTalk(const FString& AgentId, const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "Hearth")
	void SendTalkEnd(const FString& AgentId);

	// ---- latest state (read after OnWorldInit / OnSnapshot) ----
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") TMap<FString, FHearthLocationInfo> Locations;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") TMap<FString, FHearthAgentSnapshot> Agents;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") TArray<FString> AgentOrder;   // stable order for layout
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FHearthWorldTime WorldTime;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") FString Weather = TEXT("clear");
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") int32 Tick = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") float MetersToUnits = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") float TickSeconds = 3.f;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") float TravelMetersPerTick = 400.f;
	UPROPERTY(BlueprintReadOnly, Category = "Hearth") bool bWorldInitialized = false;

	// ---- events ----
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthWorldInitDelegate OnWorldInit;
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthSnapshotDelegate OnSnapshot;
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthSpeechDelegate OnSpeech;
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthEventDelegate OnEvent;
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthConnectionDelegate OnConnectionChanged;
	UPROPERTY(BlueprintAssignable, Category = "Hearth") FHearthReplyDelegate OnReply;

	/** World position (units) for a sim position in meters. Z is left at 0; GameMode grounds it. */
	FVector SimToWorld(const FVector2D& Meters) const { return FVector(Meters.X * MetersToUnits, Meters.Y * MetersToUnits, 0.f); }

private:
	TSharedPtr<IWebSocket> Socket;
	FTimerHandle ReconnectTimer;
	bool bWantConnection = false;

	void HandleConnected();
	void HandleConnectionError(const FString& Error);
	void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleMessage(const FString& Message);
	void ScheduleReconnect();
	void SendJson(const TSharedRef<FJsonObject>& Obj);

	void ParseWorldInit(const TSharedPtr<FJsonObject>& Obj);
	void ParseSnapshot(const TSharedPtr<FJsonObject>& Obj);
	void ParseLocation(const TSharedPtr<FJsonObject>& Obj, FHearthLocationInfo& Out) const;
	void ParseAgent(const TSharedPtr<FJsonObject>& Obj, FHearthAgentSnapshot& Out) const;
	static void ParseIntMap(const TSharedPtr<FJsonObject>& Obj, TMap<FString, int32>& Out);
};
