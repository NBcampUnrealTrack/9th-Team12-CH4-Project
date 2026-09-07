#include "TDWidgetAuthoringLibrary.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"

bool UTDWidgetAuthoringLibrary::SetWidgetRoot(UWidgetBlueprint* Blueprint, UWidget* Root)
{
	if (!Blueprint || !Blueprint->WidgetTree || !Root || Root->GetOuter() != Blueprint->WidgetTree) return false;
	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();
	Blueprint->WidgetTree->RootWidget = Root;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

void UTDWidgetAuthoringLibrary::SetNamedSlotContent(UUserWidget* Widget, FName SlotName, UWidget* Content)
{
	if (Widget)
	{
		Widget->Modify();
		Widget->SetContentForSlot(SlotName, Content);
	}
}

UWidget* UTDWidgetAuthoringLibrary::FindWidget(UUserWidget* Widget, FName Name)
{
	return Widget && Widget->WidgetTree ? Widget->WidgetTree->FindWidget(Name) : nullptr;
}

bool UTDWidgetAuthoringLibrary::RenderWidgetPNG(UUserWidget* Widget, FVector2D Size, const FString& Filename)
{
	if (!Widget || Size.X <= 0 || Size.Y <= 0) return false;
	FWidgetRenderer Renderer(true);
	UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget->TakeWidget(), Size);
	if (!Target) return false;
	FBufferArchive PNG;
	const bool bExported = FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	return bExported && FFileHelper::SaveArrayToFile(PNG, *Filename);
}
