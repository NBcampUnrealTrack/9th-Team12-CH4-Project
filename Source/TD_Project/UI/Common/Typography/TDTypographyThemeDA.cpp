#include "TDTypographyThemeDA.h"

TSubclassOf<UCommonTextStyle> UTDTypographyThemeDA::GetStyle(
	ETDTextStyleRole Role) const
{
	switch (Role)
	{
	case ETDTextStyleRole::H1:
		return H1;
	case ETDTextStyleRole::H2:
		return H2;
	case ETDTextStyleRole::H3:
		return H3;
	case ETDTextStyleRole::H4:
		return H4;
	case ETDTextStyleRole::H5:
		return H5;
	case ETDTextStyleRole::BodyLarge:
		return BodyLarge;
	case ETDTextStyleRole::Body:
		return Body;
	case ETDTextStyleRole::BodySmall:
		return BodySmall;
	case ETDTextStyleRole::Caption:
		return Caption;
	case ETDTextStyleRole::Number:
		return Number;
	default:
		return Body;
	}
}
