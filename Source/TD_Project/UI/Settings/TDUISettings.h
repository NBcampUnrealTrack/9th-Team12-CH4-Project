#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDUISettings.generated.h"

class UInputAction;
class UTDNameplateWidget;
class UTDChatBubbleWidget;
class UTDTypographyThemeDA;
class UTDWindowBaseWidget;
class UTDItemTooltipWidget;
class UDataTable;
class UTDRespawnWidget;
class UTDLoginWidget;
class UUserWidget;

/** Project Settings > Game > TD UI에 표시되는 프로젝트 공용 UI 설정. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TD UI"))
class TD_PROJECT_API UTDUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Account")
		TSoftClassPtr<UTDLoginWidget> LoginWidgetClass;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Account|Pages")
		TSoftClassPtr<UUserWidget> AccountLoginPage;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Account|Pages")
		TSoftClassPtr<UUserWidget> AccountRegisterPage;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Account|Pages")
		TSoftClassPtr<UUserWidget> AccountSelectPage;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Account|Pages")
		TSoftClassPtr<UUserWidget> AccountCreatePage;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Respawn")
		TSoftClassPtr<UTDRespawnWidget> RespawnWidgetClass;

	/** 아이템/향후 스킬의 공용 카드. 기존 HUD 에셋을 변경하지 않고 이 클래스만 교체한다. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftClassPtr<UTDItemTooltipWidget> ItemTooltipClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftObjectPtr<UDataTable> TooltipItemTable;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftObjectPtr<UDataTable> TooltipItemStatTable;
	/** 세트 이름 표시용. 효과 수치는 ItemUseComponent의 테이블을 사용한다. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftObjectPtr<UDataTable> TooltipItemSetTable;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftObjectPtr<UDataTable> TooltipStatDefinitionTable;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
		TSoftObjectPtr<UDataTable> TooltipUseEffectTable;

	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	/** 모든 TD Text 위젯이 기본으로 사용할 Typography Theme. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Typography")
		TSoftObjectPtr<UTDTypographyThemeDA> DefaultTypographyTheme;

	/** WindowLayer에 생성할 위젯 클래스. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> InventoryWindowClass;

	/** Nav의 캐릭터 버튼으로 열 캐릭터 정보 창. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> CharacterWindowClass;

	/** Nav 스킬 버튼으로 열 직업별 스킬 목록. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> SkillWindowClass;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> PartyWindowClass;

	/**
	 * 설정 창(그래픽·사운드·화면·단축키).
	 *
	 * Nav의 시스템 버튼과 ESC 메뉴가 같은 창을 연다. 둘을 따로 만들면 한쪽만
	 * 고쳤을 때 화면이 갈라진다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> SystemWindowClass;

	/** 하단 퀘스트 버튼으로 열 퀘스트 창입니다. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> QuestWindowClass;

	/** 유니온 창. 부모 클래스가 UTDUnionWindowWidget 인 WBP 를 지정한다. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> UnionWindowClass;

	/**
	 * 추가 옵션(잠재능력) 재설정 창. 부모 클래스가 UTDOptionWindowWidget 인 WBP 를 지정한다.
	 *
	 * NPC 와 대화를 마치면 열린다. 다른 창들과 달리 단축키로는 열 수 없다 —
	 * 강화와 같이 NPC 앞에서만 쓰는 기능이다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> OptionWindowClass;

	/**
	 * 거래소. 부모 클래스가 UTDMarketWindowWidget 인 WBP 를 지정한다.
	 *
	 * 강화·추가 옵션과 달리 NPC 세션이 없다 — 거리나 존을 따지지 않으므로
	 * 인벤토리 창처럼 단축키로 어디서나 연다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
		TSoftClassPtr<UTDWindowBaseWidget> MarketWindowClass;

	/**
	 * 창 단축키. 창 종류 → 입력 액션.
	 *
	 * 키를 코드에 적지 않고 입력 액션으로 받아야 설정창에서 바꿀 수 있다. 액션의
	 * Player Mappable Key Settings(Name · Display Name)가 설정창 목록의 한 줄이 되고,
	 * 기본 키는 IMC_Player 에서 정한다. 새 창에 단축키를 붙일 때 코드는 고치지 않는다.
	 *
	 * System(ESC)은 넣지 않는다 — 고정 키다. 다른 키로 바꿨다가 잊으면 설정창을 다시 열 길이 없다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shortcuts")
		TMap<ETDNavMenuType, TSoftObjectPtr<UInputAction>> MenuInputActions;

	/**
	 * 플레이어 머리 위 이름표. 부모 클래스가 UTDNameplateWidget 인 WBP 를 지정한다.
	 *
	 * 비워 두면 이름표만 뜨지 않고 나머지는 그대로 동작한다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Nameplate")
		TSoftClassPtr<UTDNameplateWidget> NameplateWidgetClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chat")
		TSoftClassPtr<UTDChatBubbleWidget> ChatBubbleWidgetClass;
};
