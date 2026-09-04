#include "HearthGameMode.h"
#include "Hearth.h"
#include "HearthAgent.h"
#include "HearthBridgeSubsystem.h"
#include "HearthLocation.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

AHearthGameMode::AHearthGameMode()
{
	AgentClass = AHearthAgent::StaticClass();
	LocationClass = AHearthLocation::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();   // fly around and watch
}

UHearthBridgeSubsystem* AHearthGameMode::Bridge() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHearthBridgeSubsystem>() : nullptr;
}

void AHearthGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UHearthBridgeSubsystem* B = Bridge())
	{
		B->OnWorldInit.AddDynamic(this, &AHearthGameMode::HandleWorldInit);
		B->OnSnapshot.AddDynamic(this, &AHearthGameMode::HandleSnapshot);
		B->OnSpeech.AddDynamic(this, &AHearthGameMode::HandleSpeech);
		B->OnEvent.AddDynamic(this, &AHearthGameMode::HandleEvent);
		if (B->bWorldInitialized)
		{
			HandleWorldInit();   // late join: brain was already streaming
		}
	}
}

FVector AHearthGameMode::Ground(const FVector& XY) const
{
	FHitResult Hit;
	const FVector Start(XY.X, XY.Y, GroundTraceHeight);
	const FVector End(XY.X, XY.Y, -GroundTraceHeight);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
	{
		return Hit.ImpactPoint;
	}
	return FVector(XY.X, XY.Y, 0.f);
}

FVector AHearthGameMode::StandingSpot(const FString& LocationId, int32 AgentIndex) const
{
	const UHearthBridgeSubsystem* B = Bridge();
	const FHearthLocationInfo* Loc = B ? B->Locations.Find(LocationId) : nullptr;
	FVector Center = Loc ? B->SimToWorld(Loc->PositionMeters) : FVector::ZeroVector;
	const float Angle = AgentIndex * (2.f * PI / 6.f);
	Center += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * GatherRadius;
	return Ground(Center);
}

void AHearthGameMode::HandleWorldInit()
{
	UHearthBridgeSubsystem* B = Bridge();
	if (!B) { return; }

	for (const auto& Pair : B->Locations)
	{
		if (LocationActors.Contains(Pair.Key)) { continue; }
		const FVector Pos = Ground(B->SimToWorld(Pair.Value.PositionMeters));
		AHearthLocation* Actor = GetWorld()->SpawnActor<AHearthLocation>(LocationClass, Pos, FRotator::ZeroRotator);
		if (Actor)
		{
			Actor->Init(Pair.Value);
			LocationActors.Add(Pair.Key, Actor);
		}
	}

	int32 Index = 0;
	for (const FString& Id : B->AgentOrder)
	{
		const FHearthAgentSnapshot* Snap = B->Agents.Find(Id);
		if (!Snap || AgentActors.Contains(Id)) { ++Index; continue; }
		FVector Pos = StandingSpot(Snap->LocationId, Index);
		Pos.Z += 100.f;  // capsule half-height; the character settles onto the ground
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AHearthAgent* Actor = GetWorld()->SpawnActor<AHearthAgent>(AgentClass, Pos, FRotator::ZeroRotator, Params);
		if (Actor)
		{
			Actor->Init(Snap->Id, Snap->Name);
			AgentActors.Add(Id, Actor);
		}
		++Index;
	}
	UE_LOG(LogHearth, Log, TEXT("Spawned %d locations, %d agents"), LocationActors.Num(), AgentActors.Num());
	FrameCameraOnCamp();
}

void AHearthGameMode::FrameCameraOnCamp()
{
	// Put the spectator above and beside camp, looking at it. WASD/mouse still work afterwards.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const UHearthBridgeSubsystem* B = Bridge();
	if (!PC || !B) { return; }
	const FHearthLocationInfo* Camp = B->Locations.Find(TEXT("camp"));
	const FVector CampPos = Ground(Camp ? B->SimToWorld(Camp->PositionMeters) : FVector::ZeroVector);
	const FVector Eye = CampPos + FVector(-1400.f, -1400.f, 800.f);
	if (APawn* P = PC->GetPawn())
	{
		P->SetActorLocation(Eye);
	}
	PC->SetControlRotation(UKismetMathLibrary::FindLookAtRotation(Eye, CampPos + FVector(0.f, 0.f, 100.f)));
}

void AHearthGameMode::HandleSnapshot()
{
	UHearthBridgeSubsystem* B = Bridge();
	if (!B) { return; }
	if (AgentActors.Num() == 0 && B->bWorldInitialized)
	{
		HandleWorldInit();
	}

	for (const auto& Pair : B->Locations)
	{
		if (TObjectPtr<AHearthLocation>* Actor = LocationActors.Find(Pair.Key))
		{
			(*Actor)->ApplyInfo(Pair.Value);
		}
	}

	// Walk speed so a trip takes about as long on screen as it does in the sim.
	const float UnitsPerSec = (B->TravelMetersPerTick * B->MetersToUnits) / FMath::Max(B->TickSeconds, 0.25f);

	int32 Index = 0;
	for (const FString& Id : B->AgentOrder)
	{
		const FHearthAgentSnapshot* Snap = B->Agents.Find(Id);
		TObjectPtr<AHearthAgent>* Actor = AgentActors.Find(Id);
		if (Snap && Actor)
		{
			const FString& Dest = Snap->MovingTo.IsEmpty() ? Snap->LocationId : Snap->MovingTo;
			(*Actor)->ApplySnapshot(*Snap, StandingSpot(Dest, Index), UnitsPerSec);
		}
		++Index;
	}
}

void AHearthGameMode::HandleSpeech(const FString& AgentId, const FString& ToAgentId, const FString& Text)
{
	UHearthBridgeSubsystem* B = Bridge();
	FString ToName;
	if (B && !ToAgentId.IsEmpty())
	{
		if (const FHearthAgentSnapshot* To = B->Agents.Find(ToAgentId)) { ToName = To->Name; }
	}
	if (TObjectPtr<AHearthAgent>* Actor = AgentActors.Find(AgentId))
	{
		(*Actor)->Say(Text, ToName);
	}
	UE_LOG(LogHearth, Log, TEXT("%s%s: %s"), *AgentId, ToName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" -> %s"), *ToName), *Text);
}

void AHearthGameMode::HandleEvent(const FString& Kind, const FString& Text, const FString& AgentId, const FString& LocationId)
{
	UE_LOG(LogHearth, Log, TEXT("[%s] %s"), *Kind, *Text);
}
