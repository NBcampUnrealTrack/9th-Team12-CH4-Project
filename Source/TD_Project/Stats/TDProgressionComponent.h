#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDSkillEffectRow.h"
#include "Data/TDSkillPassiveRow.h"
#include "Data/TDSkillRow.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnProgressionChanged);

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

	/** 레벨이 그대로인 경험치 획득도 UI에 알린다. 서버와 복제 수신 양쪽에서 호출. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Progression")
	FTDOnProgressionChanged OnProgressionChanged;

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

	// ── 스킬 ──────────────────────────────────────────────
	//
	// 스킬 테이블의 조회 창구가 여기다. 액티브를 담당할 컴포넌트도 자기 테이블을 따로 들지 않고
	// 이쪽을 거친다 — 퀵슬롯이 UTDInventoryComponent::FindItemDefinition 을 거치는 것과 같다.
	// 같은 테이블 참조를 두 컴포넌트가 들면 한쪽만 지정해 놓고 왜 안 되는지 찾게 된다.

	/** DT_Skill 조회. 없으면 nullptr. */
	const FTDSkillRow* FindSkillRow(FName SkillId) const;

	/** 위와 같지만 블루프린트용. @return 행을 찾았으면 true. */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	bool GetSkillInfo(FName SkillId, FTDSkillRow& OutRow) const;

	/**
	 * 지금 직업이 쓰는 스킬 목록. 스킬창이 무엇을 그릴지 정하는 근거다.
	 *
	 * 정렬하지 않고 테이블 순서 그대로 돌려준다. 액티브를 Q·W·E 로 늘어놓을지
	 * 패시브와 한 목록에 섞을지는 화면이 정할 일이고, 그 판단에 필요한
	 * SkillType 과 SlotIndex 는 행에 들어 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	TArray<FName> GetClassSkills() const;

	/**
	 * Q·W·E 자리(1~3)에 놓인 내 직업의 액티브. 없으면 NAME_None.
	 *
	 * 키 입력은 "몇 번을 눌렀다" 만 아는데, 그 자리에 어느 스킬이 있는지는 직업이 정한다.
	 * 그 대응을 여기서 한 번만 풀어 둔다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	FName GetSkillForSlot(int32 SlotIndex) const;

	/**
	 * 그 스킬을 썼을 때 일어나는 일들. 액티브만 해당한다.
	 *
	 * 한 스킬이 효과를 여럿 가질 수 있어 배열이다("피해를 주면서 나를 회복").
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	TArray<FTDSkillEffectRow> GetSkillEffects(FName SkillId) const;

	/**
	 * 그 패시브가 올려주는 스탯들. 위와 대칭이고 보는 테이블만 다르다.
	 *
	 * 스탯 적용은 RefreshSkillModifiers 가 알아서 하므로 게임플레이에는 필요 없다.
	 * 툴팁이 "방어력이 몇 오르는가" 를 말하려면 있어야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	TArray<FTDSkillPassiveRow> GetSkillPassives(FName SkillId) const;

	/**
	 * 지금 이 스킬을 한 단계 올릴 수 있는가.
	 *
	 * UI 가 버튼을 회색으로 만들 때도 이 함수를 쓴다. 조건을 화면 쪽에 한 벌 더 적으면
	 * 규칙이 바뀔 때 한쪽만 고쳐져 "눌리는데 서버가 거부하는" 버튼이 생긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	bool CanUpgradeSkill(FName SkillId) const;

	/**
	 * 스킬을 한 단계 올린다. 스킬창 버튼이 부른다.
	 *
	 * 클라이언트는 "무엇을 올리겠다"만 보내고 조건은 서버가 자기 데이터로 다시 판단한다 —
	 * 화면이 낡았거나 위조된 요청은 조용히 무시된다(D56 과 같은 원칙).
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Skill")
	void ServerUpgradeSkill(FName SkillId);

#if !UE_BUILD_SHIPPING
	/**
	 * 개발용. 지금 직업의 스킬을 전부 지정 레벨로 맞춘다. 서버 전용.
	 *
	 * **캐릭터 레벨 조건과 잔여 포인트를 무시한다.** 액티브는 찍지 않으면 나가지 않아서,
	 * 테스트할 때마다 TD.SetLevel 로 레벨을 올리고 TD.SkillUp 을 여러 번 치게 되기 때문이다.
	 * 규칙을 우회하므로 UFUNCTION 이 아니고 Shipping 에서는 아예 사라진다.
	 *
	 * MaxLevel 과 "내 직업 스킬인가" 는 그대로 지킨다 — 그것까지 무시하면
	 * 테스트가 실제로 가능한 상태를 벗어난다.
	 *
	 * @param SkillLevel  맞출 레벨. 0 을 넣으면 전부 초기화된다.
	 * @return 실제로 값이 바뀐 스킬 수.
	 */
	int32 DebugLearnAllSkills(int32 SkillLevel = 1);
#endif

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

	/** DT_Skill. 스킬의 정적 정의. 지정하지 않으면 스킬을 찍을 수 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> SkillTable;

	/** DT_SkillPassive. 패시브가 올려주는 스탯. 지정하지 않으면 패시브를 찍어도 수치가 그대로다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> SkillPassiveTable;

	/** DT_SkillEffect. 액티브를 썼을 때 일어나는 일. 지정하지 않으면 스킬이 나가도 아무 일이 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Progression")
	TObjectPtr<UDataTable> SkillEffectTable;

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

	/**
	 * 찍어 둔 패시브를 모디파이어로 바꿔 Source.Skill 로 다시 등록한다.
	 *
	 * 성장(Source.Progression)과 나눠 둔 이유는 바뀌는 시점이 다르기 때문이다.
	 * 성장은 레벨이 오를 때, 패시브는 스킬을 찍을 때 달라진다. 하나로 묶으면
	 * 레벨업 때마다 패시브 테이블까지 훑게 된다.
	 *
	 * 스킬별로 소스를 나누지 않고 통째로 갈아 끼운다 — 장비를 Source.Equipment
	 * 하나로 묶은 것과 같은 판단이다.
	 */
	void RefreshSkillModifiers();

	UTDStatComponent* FindStatComponent() const;

	bool HasAuthorityToModify() const;

	// ── 복제되는 상태 ─────────────────────────────────────
	// 스탯 컴포넌트와 달리 이쪽은 원본 데이터라 클라이언트도 알아야 한다.
	// UI 가 레벨·경험치·잔여 스킬포인트를 표시하고, 남의 레벨도 보여야 하므로
	// 소유자 전용이 아니라 전원에게 보낸다.

	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Exp)
	int32 Exp = 0;

	UFUNCTION()
	void OnRep_Level(int32 PreviousLevel);

	UFUNCTION()
	void OnRep_Exp();

	/** 비어 있으면 Default 성장만 적용된다. */
	UPROPERTY(Replicated)
	FName ClassId;

	/** 잔여 스킬포인트를 계산하려면 클라이언트도 이 목록이 필요하다. */
	UPROPERTY(ReplicatedUsing = OnRep_SkillLevels)
	TArray<FTDSkillLevel> SkillLevels;

	/** 스킬창이 다시 그려질 신호다. 서버에서는 불리지 않으므로 그쪽은 직접 알린다. */
	UFUNCTION()
	void OnRep_SkillLevels();

	/** RefreshStatModifiers 가 등록한 소스. 갱신할 때 이 핸들로 이전 것을 걷어낸다. */
	FTDStatSourceHandle StatSourceHandle;

	/** RefreshSkillModifiers 가 등록한 소스. 위와 같은 역할이고 대상만 다르다. */
	FTDStatSourceHandle SkillStatSourceHandle;
};
