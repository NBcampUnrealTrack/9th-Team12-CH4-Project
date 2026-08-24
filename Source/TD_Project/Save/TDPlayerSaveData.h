#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/TDItemTypes.h"
#include "TDPlayerSaveData.generated.h"

/**
 * 스킬 하나에 찍어놓은 레벨.
 *
 * TMap<FName, int32> 가 자연스러워 보이지만 배열로 둔다.
 * TMap 은 복제되지 않고, 직렬화했을 때 순서가 보장되지 않아 저장 데이터 비교가 어렵다.
 */
USTRUCT(BlueprintType)
struct FTDSkillLevel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	int32 Level = 0;

	FTDSkillLevel() = default;

	FTDSkillLevel(FName InSkillId, int32 InLevel)
		: SkillId(InSkillId), Level(InLevel)
	{
	}
};

/**
 * 캐릭터 선택 화면의 한 칸.
 *
 * 계정이 보유한 캐릭터를 나열하는 데만 쓰는 요약본이다. 실제 플레이에 필요한 값은
 * 선택이 끝난 뒤 FTDPlayerSaveData 로 따로 읽는다 — 목록을 띄우려고 인벤토리까지
 * 전부 보내면 캐릭터 수만큼 낭비가 된다.
 *
 * 이 배열을 채우는 것은 세이브  쪽에서 작업해야 한다. 여기서는 구조체와 복제까지만 정의한다.
 * TODO: 배열 채우기
 */
USTRUCT(BlueprintType)
struct FTDCharacterSummary
{
	GENERATED_BODY()

	/** 코디. 장착 중인 아이템의 ItemId 로, 프리뷰 스프라이트를 갈아 끼우는 데 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Character")
	TArray<FName> EquippedItemIds;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Character")
	FString CharacterName;

	/** DT_ClassGrowth 의 ClassId 이자 프리뷰 외형을 정하는 값이다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Character")
	FName ClassId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Character")
	int32 Level = 1;
};

/**
 * 저장소에 넘기는 값 묶음
 * SaveGame 을 사용하면 로컬 저장 방식만 사용 가능해서 구조체로 묶는다.
 * 플레이어의 최종 수치가 아닌 원본 능력치만 저장한다.
 * 현재 수치를 저장해 두면 밸런스를 고쳐도 플레이어 스텟이 변하지 않고, 세이브를 조작할 수 있기 때문이다.
 *
 * 스탯은 여기 없다. 레벨과 직업만 있으면 DT_ClassGrowth 로 전부 다시 계산된다.
 */
USTRUCT(BlueprintType)
struct FTDPlayerSaveData
{
	GENERATED_BODY()

	/**
	 * 저장 형식 버전. 필드가 늘거나 의미가 바뀌었을 때 옛 데이터를 읽는 분기점이 된다.
	 * 나중에 넣으려면 버전 없는 데이터가 이미 쌓여 있어 구분할 방법이 없으므로 처음부터 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	int32 SaveVersion = 1;

	/**
	 * 직업. DT_ClassGrowth 의 ClassId 와 짝이다.
	 * 아직 직업 선택이 없어 비어 있을 수 있으며, 그때는 Default 성장만 적용된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	FName ClassId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	int32 Exp = 0;

	/**
	 * 스킬창에서 올린 내역. 플레이어의 선택이라 재현할 수 없으므로 반드시 저장한다.
	 * 스킬 시스템 자체는 아직 없지만, 나중에 필드를 추가하면 SaveVersion 을 올리고
	 * 변환 분기를 만들어야 하므로 자리를 미리 잡아둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	TArray<FTDSkillLevel> SkillLevels;

	// ── 인벤토리 ──────────────────────────────────────────

	/** 확장권으로 늘어나므로 플레이어의 선택이고, 따라서 저장한다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	int32 InventorySlotCapacity = 40;

	/**
	 * 소지품. 강화 레벨과 추가 옵션이 개체마다 달라 재현할 수 없으므로 통째로 저장한다.
	 * 아이템의 이름·아이콘·고정 스탯은 담지 않는다 — DT_ItemDefinition 에서 언제든 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	TArray<FTDItemInstance> InventoryItems;

	/** 장착 중인 장신구. SlotIndex 가 장착 칸(0~5) 번호다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	TArray<FTDItemInstance> EquippedItems;

	// ── 상태 ──────────────────────────────────────────────
	// 계산으로 복원할 수 없는 값이라 저장한다. 다만 절대값이 아니라 비율로 담는다. (체력 1200/1000 같은 상황 방지)

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	float HealthRatio = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	float ManaRatio = 1.f;

	/**
	 * 마지막으로 있던 존. 다시 접속하면 이 존의 시작 지점에서 시작한다.
	 *
	 * 좌표를 저장하지 않는 이유는 지형 때문이다. 맵을 수정한 뒤 옛 좌표로 복원하면
	 * 벽 안이나 허공에 떨어질 수 있다. 존의 시작 지점은 항상 안전한 자리다.
	 *
	 * 비어 있으면(신규 캐릭터) 기본 시작 존으로 보낸다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Save")
	FGameplayTag LastZoneId;
};
