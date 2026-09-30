#include "TDItemDragDropOperation.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UTDItemDragDropOperation::SetDragIcon(UTexture2D* Texture)
{
	UImage* Image = NewObject<UImage>(this);
	Image->SetBrushFromTexture(Texture);
	Image->SetDesiredSizeOverride(FVector2D(48.f, 48.f));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	DefaultDragVisual = Image;
	Pivot = EDragPivot::CenterCenter;
}
