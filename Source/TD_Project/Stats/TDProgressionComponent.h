#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"
#include "TDProgressionComponent.generated.h"

class UDataTable;
class UTDStatComponent;

/**
 * 캐릭터의 성장을 담당하는 컴포넌트. 레벨, 경험치, 직업, 스킬 레벨을 소유한다.
 *
 * UTDStatComponent 와 나눠 둔 이유는 소유권이다. 여기 있는 값들은 플레이어가 선택했거나
 * 겪은 것이라 저장해야 하지만, 스탯 컴포넌트가 들고 있는 것은 전부 이 값들로부터
 * 다시 계산되는 파생물이라 저장할 필요가 없다.
 *
 * 스탯은 레벨업 시 DT_ClassGrowth 에 따라 자동으로 오른다. 플레이어가 직접 분배하는 것은
 * 스킬 포인트뿐이며, 그래서 스탯 쪽에는 클라이언트가 조작할 여지가 애초에 없다.
 *
 * 계산은 하지 않는다. 성장분을 모디파이어로 바꿔 스탯 컴포넌트에 등록할 뿐이며,
 * 실제 합산은 모든 모디파이어가 모인 그쪽에서 한 번에 이뤄진다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDProgressionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 레벨 1당 주어지는 스킬 포인트. */
	static constexpr int32 SkillPointsPerLevel = 3;

	// ── 성장 (서버 전용) ──────────────────────────────────

	/** 레벨을 지정하고 성장 모디파이어를 갱신한다. @return 서버가 아니면 false. */
	bool SetLevel(int32 NewLevel);

	/**
	 * 직업을 지정하고 성장 모디파이어를 갱신한다. DT_ClassGrowth 의 ClassId 와 짝이다.
	 * @return 서버가 아니면 false.
	 */
	bool SetClassId(FName NewClassId);

	/** 경험치를 누적한다. 레벨업 판정은 아직 없다 — 필요 경험치 곡선 테이블이 생긴 뒤에 붙인다. */
	void AddExp(int32 Amount);

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetExp() const { return Exp; }

	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	FName GetClassId() const { return ClassId; }

	/**
	 * 아직 쓸 수 있는 스킬 포인트. 저장하지 않고 매번 계산한다.
	 *
	 * 별도 필드로 두면 레벨업과 스킬 습득 양쪽에서 증감시켜야 하고, 한 군데라도 빠뜨리면
	 * 포인트가 사라지거나 무한히 늘어난다. 레벨과 스킬 레벨만 정확하면 이 값은 항상 맞다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetRemainingSkillPoints() const;

	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetSkillLevel(FName SkillId) const;

	// ── 세이브 구조체 ─────────────────────────────────────
	// 값을 옮겨 담기만 한다. 파일이나 DB 는 저장 담당자의 몫이다.

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	virtual void BeginPlay() override;

	/** DT_ClassGrowth. 직업별 레벨 성장량을 여기서 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> ClassGrowthTable;

private:
	/**
	 * 레벨과 직업에 따른 성장분을 모디파이어로 바꿔 스탯 컴포넌트에 다시 등록한다.
	 *
	 * 레벨이 바뀌면 성장 대상 스탯 전부의 수치가 동시에 달라지므로,
	 * 일부만 고치는 것보다 묶음을 새로 만들어 갈아끼우는 편이 단순하다.
	 */
	void RefreshStatModifiers();

	UTDStatComponent* FindStatComponent() const;

	bool HasAuthorityToModify() const;

	// ── 복제되는 상태 ─────────────────────────────────────
	// 스탯 컴포넌트와 달리 이쪽은 원본 데이터라 클라이언트도 알아야 한다.
	// UI 가 레벨·경험치·잔여 스킬포인트를 표시하고, 남의 레벨도 보여야 하므로
	// 소유자 전용이 아니라 전원에게 보낸다.

	UPROPERTY(Replicated)
	int32 Level = 1;

	UPROPERTY(Replicated)
	int32 Exp = 0;

	/** 비어 있으면 Default 성장만 적용된다. */
	UPROPERTY(Replicated)
	FName ClassId;

	/** 잔여 스킬포인트를 계산하려면 클라이언트도 이 목록이 필요하다. */
	UPROPERTY(Replicated)
	TArray<FTDSkillLevel> SkillLevels;

	/** RefreshStatModifiers 가 등록한 소스. 갱신할 때 이 핸들로 이전 것을 걷어낸다. */
	FTDStatSourceHandle StatSourceHandle;
};
