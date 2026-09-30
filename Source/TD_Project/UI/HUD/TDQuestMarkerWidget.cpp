#include "UI/HUD/TDQuestMarkerWidget.h"

void UTDQuestMarkerWidget::SetMarkerData(
	const FTDQuestMarkerView& InData)
{
	MarkerData = InData;
	BP_OnMarkerDataChanged(MarkerData);
}