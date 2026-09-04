// Top-right minimap: north-up view of the valley with places, people, and the player's heading.
#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SLeafWidget.h"

class UWorld;

class SHearthMinimap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHearthMinimap) : _Size(230.f), _WorldRange(6800.f) {}
		SLATE_ARGUMENT(float, Size)          // pixels, square
		SLATE_ARGUMENT(float, WorldRange)    // world units from center to the map edge
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }

private:
	float Size = 230.f;
	float WorldRange = 6800.f;
	TWeakObjectPtr<UWorld> World;
	FSlateRoundedBoxBrush Background = FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.08f, 0.05f, 0.78f), 10.f);
	FSlateRoundedBoxBrush Dot = FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FVector2f(10.f, 10.f));
	FSlateRoundedBoxBrush PlaceDot = FSlateRoundedBoxBrush(FLinearColor::White, 3.f, FVector2f(6.f, 6.f));

	FVector2f ToMap(const FVector& WorldPos) const;
};
