#include "HearthLocation.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AHearthLocation::AHearthLocation()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Root);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		Marker->SetStaticMesh(Cyl.Object);
		Marker->SetRelativeScale3D(FVector(3.f, 3.f, 0.1f));
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
	Label->SetTextRenderColor(FColor::Cyan);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Root);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	FireLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	FireLight->SetIntensity(8000.f);
	FireLight->SetAttenuationRadius(1500.f);
	FireLight->SetVisibility(false);
}

void AHearthLocation::Init(const FHearthLocationInfo& Info)
{
	LocationId = Info.Id;
	Label->SetText(FText::FromString(Info.Name));
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Loc_%s"), *Info.Name));
#endif
	ApplyInfo(Info);
}

void AHearthLocation::ApplyInfo(const FHearthLocationInfo& Info)
{
	Latest = Info;
	if (Info.bHasFire && Info.bFireLit != bLastFireLit)
	{
		bLastFireLit = Info.bFireLit;
		FireLight->SetVisibility(Info.bFireLit);
		OnFireChanged(Info.bFireLit);
	}
	if (Info.bHasShelter && (Info.bShelterBuilt != bLastShelterBuilt || Info.ShelterProgress != LastShelterProgress))
	{
		bLastShelterBuilt = Info.bShelterBuilt;
		LastShelterProgress = Info.ShelterProgress;
		OnShelterChanged(Info.bShelterBuilt, Info.ShelterProgress, Info.ShelterRequired);
	}

	// Label doubles as a tiny HUD for what's here.
	FString Text = Info.Name;
	for (const auto& P : Info.Resources)
	{
		if (P.Value > 0 && P.Value < 900) { Text += FString::Printf(TEXT("\n%s %d"), *P.Key, P.Value); }
	}
	if (Info.bHasFire)
	{
		Text += Info.bFireLit ? TEXT("\nfire: lit") : TEXT("\nfire: out");
	}
	if (Info.bHasShelter)
	{
		Text += Info.bShelterBuilt ? TEXT("\nshelter: built") : FString::Printf(TEXT("\nshelter: %d/%d"), Info.ShelterProgress, Info.ShelterRequired);
	}
	for (const auto& P : Info.Stockpile)
	{
		if (P.Value > 0) { Text += FString::Printf(TEXT("\n[stock] %s %d"), *P.Key, P.Value); }
	}
	Label->SetText(FText::FromString(Text));
}

void AHearthLocation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - Label->GetComponentLocation();
		Label->SetWorldRotation(FRotationMatrix::MakeFromX(ToCam).Rotator());
	}
}
