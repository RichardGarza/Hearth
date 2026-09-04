#include "HearthBridgeSubsystem.h"
#include "Hearth.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UHearthBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FModuleManager::Get().LoadModuleChecked(TEXT("WebSockets"));
	if (bAutoConnect)
	{
		Connect();
	}
}

void UHearthBridgeSubsystem::Deinitialize()
{
	bWantConnection = false;
	Disconnect();
	Super::Deinitialize();
}

void UHearthBridgeSubsystem::Connect()
{
	bWantConnection = true;
	if (Socket.IsValid() && Socket->IsConnected())
	{
		return;
	}
	if (!BrainUrl.StartsWith(TEXT("ws://")) && !BrainUrl.StartsWith(TEXT("wss://")))
	{
		// Ini values with an unquoted '//' get truncated by the config parser; fall back to the default.
		UE_LOG(LogHearth, Warning, TEXT("BrainUrl '%s' is not a ws:// URL; using ws://127.0.0.1:8765"), *BrainUrl);
		BrainUrl = TEXT("ws://127.0.0.1:8765");
	}
	UE_LOG(LogHearth, Log, TEXT("Connecting to brain at %s"), *BrainUrl);
	Socket = FWebSocketsModule::Get().CreateWebSocket(BrainUrl, TEXT(""));
	Socket->OnConnected().AddUObject(this, &UHearthBridgeSubsystem::HandleConnected);
	Socket->OnConnectionError().AddUObject(this, &UHearthBridgeSubsystem::HandleConnectionError);
	Socket->OnClosed().AddUObject(this, &UHearthBridgeSubsystem::HandleClosed);
	Socket->OnMessage().AddUObject(this, &UHearthBridgeSubsystem::HandleMessage);
	Socket->Connect();
}

void UHearthBridgeSubsystem::Disconnect()
{
	if (Socket.IsValid())
	{
		if (Socket->IsConnected())
		{
			Socket->Close();
		}
		Socket.Reset();
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(ReconnectTimer);
	}
}

bool UHearthBridgeSubsystem::IsConnected() const
{
	return Socket.IsValid() && Socket->IsConnected();
}

void UHearthBridgeSubsystem::SendCommand(const FString& Name, const FString& ExtraJson)
{
	if (!IsConnected())
	{
		UE_LOG(LogHearth, Warning, TEXT("SendCommand(%s): not connected"), *Name);
		return;
	}
	FString Body = FString::Printf(TEXT("{\"type\":\"command\",\"name\":\"%s\""), *Name);
	if (!ExtraJson.IsEmpty())
	{
		// ExtraJson is expected to be an object literal; splice its fields in.
		FString Inner = ExtraJson.TrimStartAndEnd();
		Inner.RemoveFromStart(TEXT("{"));
		Inner.RemoveFromEnd(TEXT("}"));
		if (!Inner.IsEmpty())
		{
			Body += TEXT(",") + Inner;
		}
	}
	Body += TEXT("}");
	Socket->Send(Body);
}

void UHearthBridgeSubsystem::SendJson(const TSharedRef<FJsonObject>& Obj)
{
	if (!IsConnected())
	{
		UE_LOG(LogHearth, Warning, TEXT("SendJson: not connected"));
		return;
	}
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj, Writer);
	Socket->Send(Out);
}

void UHearthBridgeSubsystem::SendTalk(const FString& AgentId, const FString& Text)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("type"), TEXT("talk"));
	Obj->SetStringField(TEXT("agent"), AgentId);
	Obj->SetStringField(TEXT("text"), Text);
	SendJson(Obj);
}

void UHearthBridgeSubsystem::SendTalkEnd(const FString& AgentId)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("type"), TEXT("talk_end"));
	Obj->SetStringField(TEXT("agent"), AgentId);
	SendJson(Obj);
}

// ---------------------------------------------------------------- socket callbacks

void UHearthBridgeSubsystem::HandleConnected()
{
	UE_LOG(LogHearth, Log, TEXT("Connected to brain"));
	OnConnectionChanged.Broadcast(true);
}

void UHearthBridgeSubsystem::HandleConnectionError(const FString& Error)
{
	UE_LOG(LogHearth, Warning, TEXT("Brain connection error: %s"), *Error);
	OnConnectionChanged.Broadcast(false);
	ScheduleReconnect();
}

void UHearthBridgeSubsystem::HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogHearth, Log, TEXT("Brain connection closed (%d): %s"), StatusCode, *Reason);
	OnConnectionChanged.Broadcast(false);
	ScheduleReconnect();
}

void UHearthBridgeSubsystem::ScheduleReconnect()
{
	if (!bWantConnection)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().SetTimer(ReconnectTimer, this, &UHearthBridgeSubsystem::Connect, 3.0f, false);
	}
}

void UHearthBridgeSubsystem::HandleMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogHearth, Warning, TEXT("Bad JSON from brain: %s"), *Message.Left(200));
		return;
	}
	const FString Type = Obj->GetStringField(TEXT("type"));
	if (Type == TEXT("snapshot"))
	{
		ParseSnapshot(Obj);
	}
	else if (Type == TEXT("world_init"))
	{
		ParseWorldInit(Obj);
	}
	else if (Type == TEXT("speech"))
	{
		FString To;
		Obj->TryGetStringField(TEXT("to"), To);
		OnSpeech.Broadcast(Obj->GetStringField(TEXT("agent")), To, Obj->GetStringField(TEXT("text")));
	}
	else if (Type == TEXT("reply"))
	{
		OnReply.Broadcast(Obj->GetStringField(TEXT("agent")), Obj->GetStringField(TEXT("text")));
	}
	else if (Type == TEXT("event"))
	{
		FString Agent, Location;
		Obj->TryGetStringField(TEXT("agent"), Agent);
		Obj->TryGetStringField(TEXT("location"), Location);
		OnEvent.Broadcast(Obj->GetStringField(TEXT("kind")), Obj->GetStringField(TEXT("text")), Agent, Location);
	}
	else if (Type == TEXT("command_result"))
	{
		UE_LOG(LogHearth, Log, TEXT("Command %s: %s"), *Obj->GetStringField(TEXT("name")), *Obj->GetStringField(TEXT("result")));
	}
}

// ---------------------------------------------------------------- parsing

void UHearthBridgeSubsystem::ParseIntMap(const TSharedPtr<FJsonObject>& Obj, TMap<FString, int32>& Out)
{
	Out.Reset();
	if (!Obj.IsValid())
	{
		return;
	}
	for (const auto& Pair : Obj->Values)
	{
		double V = 0;
		if (Pair.Value->TryGetNumber(V))
		{
			Out.Add(FString(*Pair.Key), static_cast<int32>(V));
		}
	}
}

void UHearthBridgeSubsystem::ParseLocation(const TSharedPtr<FJsonObject>& Obj, FHearthLocationInfo& Out) const
{
	Out.Id = Obj->GetStringField(TEXT("id"));
	Obj->TryGetStringField(TEXT("name"), Out.Name);
	double X = 0, Y = 0;
	if (Obj->TryGetNumberField(TEXT("x"), X) && Obj->TryGetNumberField(TEXT("y"), Y))
	{
		Out.PositionMeters = FVector2D(X, Y);
	}
	const TSharedPtr<FJsonObject>* Res = nullptr;
	if (Obj->TryGetObjectField(TEXT("resources"), Res))
	{
		ParseIntMap(*Res, Out.Resources);
	}
	const TSharedPtr<FJsonObject>* Stock = nullptr;
	if (Obj->TryGetObjectField(TEXT("stockpile"), Stock))
	{
		ParseIntMap(*Stock, Out.Stockpile);
	}
	const TSharedPtr<FJsonObject>* Structures = nullptr;
	if (Obj->TryGetObjectField(TEXT("structures"), Structures))
	{
		const TSharedPtr<FJsonObject>* Fire = nullptr;
		if ((*Structures)->TryGetObjectField(TEXT("fire"), Fire))
		{
			Out.bHasFire = true;
			Out.bFireLit = (*Fire)->GetBoolField(TEXT("lit"));
			Out.FireFuel = static_cast<int32>((*Fire)->GetNumberField(TEXT("fuel")));
		}
		const TSharedPtr<FJsonObject>* Shelter = nullptr;
		if ((*Structures)->TryGetObjectField(TEXT("shelter"), Shelter))
		{
			Out.bHasShelter = true;
			Out.bShelterBuilt = (*Shelter)->GetBoolField(TEXT("built"));
			Out.ShelterProgress = static_cast<int32>((*Shelter)->GetNumberField(TEXT("progress")));
			Out.ShelterRequired = static_cast<int32>((*Shelter)->GetNumberField(TEXT("required")));
		}
	}
}

void UHearthBridgeSubsystem::ParseAgent(const TSharedPtr<FJsonObject>& Obj, FHearthAgentSnapshot& Out) const
{
	Out.Id = Obj->GetStringField(TEXT("id"));
	Obj->TryGetStringField(TEXT("name"), Out.Name);
	Obj->TryGetStringField(TEXT("voice"), Out.Voice);
	Obj->TryGetStringField(TEXT("body"), Out.Body);
	Obj->TryGetStringField(TEXT("location"), Out.LocationId);
	Out.MovingTo.Empty();
	Obj->TryGetStringField(TEXT("moving_to"), Out.MovingTo);      // null -> stays empty
	Out.Action.Empty();
	Obj->TryGetStringField(TEXT("action"), Out.Action);
	Out.ActionTarget.Empty();
	Obj->TryGetStringField(TEXT("action_target"), Out.ActionTarget);
	double X = 0, Y = 0;
	if (Obj->TryGetNumberField(TEXT("x"), X) && Obj->TryGetNumberField(TEXT("y"), Y))
	{
		Out.PositionMeters = FVector2D(X, Y);
	}
	Obj->TryGetBoolField(TEXT("alive"), Out.bAlive);
	Obj->TryGetBoolField(TEXT("ai"), Out.bIsAI);
	Obj->TryGetBoolField(TEXT("talking"), Out.bTalking);
	const TSharedPtr<FJsonObject>* Needs = nullptr;
	if (Obj->TryGetObjectField(TEXT("needs"), Needs))
	{
		Out.Needs.Hunger = (*Needs)->GetNumberField(TEXT("hunger"));
		Out.Needs.Thirst = (*Needs)->GetNumberField(TEXT("thirst"));
		Out.Needs.Energy = (*Needs)->GetNumberField(TEXT("energy"));
		Out.Needs.Warmth = (*Needs)->GetNumberField(TEXT("warmth"));
		Out.Needs.Health = (*Needs)->GetNumberField(TEXT("health"));
	}
	const TSharedPtr<FJsonObject>* Inv = nullptr;
	if (Obj->TryGetObjectField(TEXT("inventory"), Inv))
	{
		ParseIntMap(*Inv, Out.Inventory);
	}
}

void UHearthBridgeSubsystem::ParseWorldInit(const TSharedPtr<FJsonObject>& Obj)
{
	double M = 100;
	if (Obj->TryGetNumberField(TEXT("meters_to_units"), M)) { MetersToUnits = static_cast<float>(M); }
	double TS = 3;
	if (Obj->TryGetNumberField(TEXT("tick_seconds"), TS)) { TickSeconds = static_cast<float>(TS); }
	double TR = 400;
	if (Obj->TryGetNumberField(TEXT("travel_meters_per_tick"), TR)) { TravelMetersPerTick = static_cast<float>(TR); }

	Locations.Reset();
	for (const TSharedPtr<FJsonValue>& V : Obj->GetArrayField(TEXT("locations")))
	{
		FHearthLocationInfo Info;
		ParseLocation(V->AsObject(), Info);
		Locations.Add(Info.Id, Info);
	}
	Agents.Reset();
	AgentOrder.Reset();
	for (const TSharedPtr<FJsonValue>& V : Obj->GetArrayField(TEXT("agents")))
	{
		FHearthAgentSnapshot Snap;
		ParseAgent(V->AsObject(), Snap);
		Agents.Add(Snap.Id, Snap);
		AgentOrder.Add(Snap.Id);
	}
	bWorldInitialized = true;
	UE_LOG(LogHearth, Log, TEXT("World init: %d locations, %d agents"), Locations.Num(), Agents.Num());
	OnWorldInit.Broadcast();
}

void UHearthBridgeSubsystem::ParseSnapshot(const TSharedPtr<FJsonObject>& Obj)
{
	Tick = static_cast<int32>(Obj->GetNumberField(TEXT("tick")));
	Obj->TryGetStringField(TEXT("weather"), Weather);
	const TSharedPtr<FJsonObject>* T = nullptr;
	if (Obj->TryGetObjectField(TEXT("time"), T))
	{
		WorldTime.Day = static_cast<int32>((*T)->GetNumberField(TEXT("day")));
		WorldTime.Hour = static_cast<int32>((*T)->GetNumberField(TEXT("hour")));
		WorldTime.Minute = static_cast<int32>((*T)->GetNumberField(TEXT("minute")));
		WorldTime.bIsNight = (*T)->GetBoolField(TEXT("is_night"));
	}
	for (const TSharedPtr<FJsonValue>& V : Obj->GetArrayField(TEXT("locations")))
	{
		FHearthLocationInfo Info;
		ParseLocation(V->AsObject(), Info);
		if (FHearthLocationInfo* Existing = Locations.Find(Info.Id))
		{
			// snapshots omit nothing we need, but keep name/position from init in case
			Info.Name = Existing->Name;
			Info.PositionMeters = Existing->PositionMeters;
		}
		Locations.Add(Info.Id, Info);
	}
	for (const TSharedPtr<FJsonValue>& V : Obj->GetArrayField(TEXT("agents")))
	{
		FHearthAgentSnapshot Snap;
		ParseAgent(V->AsObject(), Snap);
		if (const FHearthAgentSnapshot* Existing = Agents.Find(Snap.Id))
		{
			Snap.Voice = Existing->Voice;
			Snap.Body = Existing->Body;
			Snap.bIsAI = Snap.bIsAI || Existing->bIsAI;
		}
		if (!Agents.Contains(Snap.Id))
		{
			AgentOrder.Add(Snap.Id);
		}
		Agents.Add(Snap.Id, Snap);
	}
	OnSnapshot.Broadcast();
}
