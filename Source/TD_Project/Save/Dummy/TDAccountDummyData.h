#pragma once

#include "CoreMinimal.h"
#include "Core/TDAccountSubSystem.h"

/** 개발용 계정 초기값. 호출할 때마다 독립된 복사본을 반환한다. Shipping에서는 비어 있다. */
namespace TDAccountDummyData
{
    TArray<FTDDummyAccountRecord> CreateAccounts();
}
