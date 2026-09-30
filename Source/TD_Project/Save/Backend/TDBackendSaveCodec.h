#pragma once
#include "CoreMinimal.h"
#include "Save/TDPlayerSaveData.h"
#include "Dom/JsonObject.h"

namespace TDBackendSaveCodec
{
TD_PROJECT_API TSharedPtr<FJsonObject> Encode(const FTDPlayerSaveData &Data);
// Decode into a temporary first: malformed data never partially changes the caller.
TD_PROJECT_API bool Decode(const TSharedPtr<FJsonObject> &Json, FTDPlayerSaveData &Out);
} // namespace TDBackendSaveCodec
