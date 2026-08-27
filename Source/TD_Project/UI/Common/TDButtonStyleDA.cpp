#include "TDButtonStyleDA.h"

FButtonStyle UTDButtonStyleDA::GetStyle(bool bSelected) const
{
	return bSelected && bUseSelectedStyle
		? SelectedStyle
		: DefaultStyle;
}
