#include "SHearthMinimap.h"
#include "HearthAgent.h"
#include "HearthLocation.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SHearthMinimap::Construct(const FArguments& InArgs)
{
	Size = InArgs._Size;
	WorldRange = InArgs._WorldRange;
	World = InArgs._World;
	SetVisibility(EVisibility::HitTestInvisible);
}

FVector2f SHearthMinimap::ToMap(const FVector& P) const
{
	// north (+X) up, east (+Y) right, camp (0,0) in the middle
	const float S = (Size * 0.5f) / WorldRange;
	return FVector2f(Size * 0.5f + P.Y * S, Size * 0.5f - P.X * S);
}

int32 SHearthMinimap::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// background
	FSlateDrawElement::MakeBox(Out, LayerId, Geo.ToPaintGeometry(), &Background, ESlateDrawEffect::None, Background.TintColor.GetSpecifiedColor());
	++LayerId;

	UWorld* W = World.Get();
	if (!W)
	{
		return LayerId;
	}
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo Tiny = FCoreStyle::GetDefaultFontStyle("Bold", 9);

	// places
	for (TActorIterator<AHearthLocation> It(W); It; ++It)
	{
		const FVector2f M = ToMap(It->GetActorLocation());
		const FLinearColor C = It->LocationId == TEXT("camp") ? FLinearColor(1.f, 0.7f, 0.3f) : FLinearColor(0.5f, 0.9f, 0.5f);
		FSlateDrawElement::MakeBox(Out, LayerId, Geo.ToPaintGeometry(FVector2f(6.f, 6.f), FSlateLayoutTransform(M - FVector2f(3.f, 3.f))), &PlaceDot, ESlateDrawEffect::None, C);
		FSlateDrawElement::MakeText(Out, LayerId + 1, Geo.ToPaintGeometry(FVector2f(80.f, 12.f), FSlateLayoutTransform(M + FVector2f(5.f, -6.f))), It->Latest.Name, Small, ESlateDrawEffect::None, C);
	}

	// people
	for (TActorIterator<AHearthAgent> It(W); It; ++It)
	{
		if (!It->Latest.bAlive) { continue; }
		const FVector2f M = ToMap(It->GetActorLocation());
		const FLinearColor C = It->bIsAI ? FLinearColor(0.2f, 0.95f, 1.f) : FLinearColor(1.f, 1.f, 1.f);
		const float R = It->bIsAI ? 6.f : 4.f;
		FSlateDrawElement::MakeBox(Out, LayerId + 2, Geo.ToPaintGeometry(FVector2f(R * 2.f, R * 2.f), FSlateLayoutTransform(M - FVector2f(R, R))), &Dot, ESlateDrawEffect::None, C);
		FSlateDrawElement::MakeText(Out, LayerId + 3, Geo.ToPaintGeometry(FVector2f(80.f, 12.f), FSlateLayoutTransform(M + FVector2f(R + 2.f, -6.f))), It->AgentName, Tiny, ESlateDrawEffect::None, C);
	}

	// the player: an arrow pointing where the camera looks
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0))
	{
		if (const APawn* P = PC->GetPawn())
		{
			const FVector2f M = ToMap(P->GetActorLocation());
			const float Yaw = FMath::DegreesToRadians(PC->GetControlRotation().Yaw);
			// forward in map space: world +X is up (-y on screen), +Y is right
			const FVector2f Fwd(FMath::Sin(Yaw), -FMath::Cos(Yaw));
			const FVector2f Right(-Fwd.Y, Fwd.X);
			TArray<FVector2f> Tri = { M + Fwd * 10.f, M - Fwd * 6.f + Right * 6.f, M - Fwd * 6.f - Right * 6.f, M + Fwd * 10.f };
			FSlateDrawElement::MakeLines(Out, LayerId + 4, Geo.ToPaintGeometry(), Tri, ESlateDrawEffect::None, FLinearColor(1.f, 0.85f, 0.2f), true, 2.f);
		}
	}
	return LayerId + 5;
}
