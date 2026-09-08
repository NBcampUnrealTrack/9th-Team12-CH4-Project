#include "UI/Common/TDCheckBoxStyleDA.h"


#if WITH_EDITOR
#include "UI/Common/TDCheckBox.h"
#include "UObject/UObjectIterator.h"
#endif


#if WITH_EDITOR
namespace
{
	void RefreshStyleUsers(UTDCheckBoxStyleDA* Style)
	{
		for (TObjectIterator<UTDCheckBox> It; It; ++It)
		{
			if (It->GetStyleData() == Style)
			{
				It->RefreshStyle();
			}
		}
	}
}

void UTDCheckBoxStyleDA::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshStyleUsers(this);
}

void UTDCheckBoxStyleDA::PostEditUndo()
{
	Super::PostEditUndo();
	RefreshStyleUsers(this);
}
#endif
