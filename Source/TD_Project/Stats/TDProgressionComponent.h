#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"
#include "TDProgressionComponent.generated.h"

class UDataTable;
class UTDStatComponent;

/**
 * 레벨이 올랐을 때. 연출·UI 가 구독한다.
 *
 * 한 번에 여러 레벨이 오를 수 있으므로(보스 처치·퀘스트 보상) 이전 레벨도 함께 넘긴다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnLevelUp, int32, NewLevel, int32, PreviousLevel);

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

	/**
	 * 경험치를 누적하고 레벨을 다시 계산한다.
	 *
	 * 경험치는 **누적값**이다. 레벨업해도 깎이지 않으며, 레벨은 이 값으로부터
	 * DT_LevelExp 를 보고 매번 구하는 파생값이다. 곡선을 조정하면 기존 캐릭터의
	 * 레벨도 새 기준으로 다시 계산된다(D12).
	 *
	 * 만렙에 도달한 뒤에도 경험치는 계속 쌓인다. 나중에 만렙을 올리면 그만큼 반영된다.
	 */
	void AddExp(int32 Amount);

	/** 레벨이 올랐을 때. 서버·클라 양쪽에서 불린다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Progression")
	FTDOnLevelUp OnLevelUp;

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

	// ── 경험치 표시용 ─────────────────────────────────────
	// 누적값을 그대로 보여주면 "15,800" 처럼 의미를 알기 어려우므로,
	// UI 가 쓰기 좋은 형태로 바꿔주는 것까지 여기서 담당한다.

	/** 다음 레벨까지 남은 경험치. 만렙이면 0. */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetExpToNextLevel() const;

	/** 현재 레벨 구간의 진행도(0~1). 경험치 바에 그대로 쓴다. 만렙이면 1. */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	float GetLevelProgress() const;

	/** 테이블의 마지막 행. 테이블이 없으면 1. */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetMaxLevel() const;

	/**
	 * 그 레벨에서 다음 레벨로 가는 데 필요한 경험치(구간 요구량).
	 *
	 * 현재 레벨과 무관하게 곡선만 보고 답한다. "30레벨의 한 칸만큼" 같은
	 * 고정량 보상을 계산할 때 쓴다. 만렙이거나 테이블에 없으면 0.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	int32 GetExpSpanForLevel(int32 InLevel) const;

	// ── 세이브 구조체 ─────────────────────────────────────
	// 값을 옮겨 담기만 한다. 파일이나 DB 는 저장 담당자의 몫이다.

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	virtual void BeginPlay() override;

	/** DT_ClassGrowth. 직업별 레벨 성장량을 여기서 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> ClassGrowthTable;

	/**
	 * DT_LevelExp. 레벨별 누적 요구 경험치.
	 *
	 * 지정하지 않으면 경험치가 쌓이기만 하고 레벨이 오르지 않는다.
	 * 조용히 실패하면 원인을 찾기 어려우므로 BeginPlay 에서 경고한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> LevelExpTable;

private:
	/**
	 * 누적 경험치로부터 레벨을 구한다. 테이블이 없으면 현재 레벨을 그대로 돌려준다.
	 *
	 * 매번 테이블을 순회하는 것이 낭비처럼 보이지만, 불리는 시점이
	 * 경험치 획득·세이브 로드·디버그 명령뿐이라 문제가 되지 않는다.
	 */
	int32 CalculateLevelFromExp(int32 TotalExp) const;

	/** 해당 레벨에 도달하는 데 필요한 누적 경험치. 테이블에 없으면 0. */
	int32 GetRequiredExpForLevel(int32 InLevel) const;

	/** 레벨이 바뀌었으면 성장 모디파이어를 갱신하고 알린다. */
	void ApplyLevelChange(int32 NewLevel);

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
