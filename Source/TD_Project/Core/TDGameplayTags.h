#pragma once

#include "NativeGameplayTags.h"

/**
 * 프로젝트에서 쓰는 게임플레이 태그를 모으는 곳
 * 에디터의 Project Settings > GameplayTags 에서 관리한다.
 */
namespace TDTags
{
	// ── 스탯 관련 ────────────────────────────────────────────

	// ── 공격 ──────────────────────────────────────────────

	/**
	 * 전체공격력. 여기 붙은 모디파이어는 하위 공격 타입 전부에 적용된다.
	 * 속성 공격(Fire/Cold/Lightning)을 나중에 추가할 때도 이 아래에 형제로 붙인다 —
	 * 물리든 마법이든 공격 타입만 맞으면 증가하도록 하기 위해서다.
	 *
	 * 주의: 계층이 적용되는 것은 모디파이어뿐이다. DT_StatDefinition 의 DefaultValue 는
	 * 태그마다 독립이라, 이 태그에 기본값을 넣어도 Physical/Magical 계산에는 들어가지 않는다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_Damage);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_Damage_Physical);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_Damage_Magical);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_CritChance);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_CritDamage);

	/** 방어력 무시 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_ArmorPenetration);

	/** 보스 상대 추가 피해. 조건부 모디파이어가 아니라 별도 스탯이라 스탯창에 숫자로 표시된다. */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_BossDamage);

	// ── 방어 ──────────────────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_Armor);

	/** 받는 피해 감소. 1.0 이면 무적이 되므로 DT_StatDefinition 에서 반드시 상한을 건다. */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_DamageReduction);

	// ── 자원 ──────────────────────────────────────────────
	//Max와 Regen만 관리

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Resource_Health_Max);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Resource_Health_Regen);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Resource_Mana_Max);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Resource_Mana_Regen);

	// ── 유틸리티 ──────────────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Utility_CooldownRecoveryRate);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Utility_MoveSpeed);


	// ── 아이템 ────────────────────────────────────────────

	/**
	 * 아이템 종류. 장착 가능 여부와 스택 여부를 가르는 기준이다.
	 * 나중에 Item.Type.Consumable.Potion 처럼 세분화해도 코드를 고칠 필요가 없다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Accessory);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consumable);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Misc);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Quest);

	/** 등급. 추가 옵션 풀을 거르는 데만 쓴다. */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Rarity_Common);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Rarity_Rare);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Rarity_Epic);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Rarity_Legendary);

	/**
	 * 소비 아이템을 썼을 때 일어나는 일. DT_ItemUseEffect 가 이 태그로 무엇을 할지 지정한다.
	 *
	 * 장비의 지속 효과는 여기 없다. 그쪽은 스탯 모디파이어라 DT_ItemStat 이 담당한다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Effect_ExpandInventory);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Effect_RestoreHealth);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Effect_RestoreMana);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Effect_GainExp);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Effect_LevelUp);


	// ── 스킬 ──────────────────────────────────────────────

	/**
	 * 스킬이 **어디를** 때리는가. DT_Skill.ShapeTag 가 이 중 하나를 고른다.
	 *
	 * 무엇이 일어나는가(Skill.Effect.*)와 분리한 이유는 조합 때문이다.
	 * 모양 3종 × 효과 3종이면 코드 6개로 스킬 9종을 덮는다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_ForwardBox);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_SelfRadius);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Self);

	/**
	 * 대상에게 **무엇이** 일어나는가. DT_SkillEffect 가 이 태그로 지정한다.
	 *
	 * Item.Effect.* 와 같은 구조지만 태그를 공유하지 않는다 — 소비 아이템의 회복량은
	 * 절대값이고 스킬의 피해는 공격력 배율이라, 같은 태그에 두면 값의 의미가 갈린다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Damage);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Heal);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_RestoreMana);

	/**
	 * 지속 효과. 위 셋과 달리 Duration 이 지나야 끝난다.
	 *
	 * Buff 는 스탯 모디파이어를 시간 제한으로 붙이는 것이고(장비와 같은 경로),
	 * Invulnerable 은 스탯이 아니다 — Stat.Defense.DamageReduction 은 1.0 미만으로
	 * 상한이 걸려 있어(무적 버그를 막는 옳은 설정) 100% 를 표현할 수 없다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Buff);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Invulnerable);


	// ── 존(맵) ────────────────────────────────────────────
	/**
	 * 현재 위치한 맵. ATDGameState 가 복제한다.
	 *
	 * FName 대신 태그를 쓰는 이유는 계층 때문이다. "지역1 어디서든"을 상위 태그 하나로
	 * 물을 수 있어서, 지역 단위 버프나 퀘스트 조건에 맵을 일일이 나열하지 않아도 된다.
	 *
	 *   ZoneId.MatchesTag(Zone_Region1)   →  마을이든 사냥터든 지역1이면 참
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Town);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Field01);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Field02);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Field03);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Field04);
	
	// 보스방은 여러 개다. 상위 태그로 "보스 구역인가"를 한 번에 물어볼 수 있다.
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Boss);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Boss_Room01);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Boss_Room02);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Boss_Room03);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region1_Boss_Room04);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Town);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Field01);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Field02);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Boss);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Boss_Room01);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Boss_Room02);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Boss_Room03);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Zone_Region2_Boss_Room04);


	// ── 모디파이어 출처 ───────────────────────────────────
	// 스탯 컴포넌트에 등록하는 묶음이 어디서 왔는지 표시한다.
	// 스탯 자체가 아니라 소스에 붙는 태그라 Stat. 계층과 분리한다.

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Progression);

	/**
	 * 장착 중인 장신구에서 온 모디파이어.
	 *
	 * 아이템별로 나누지 않고 전부 한 소스로 묶는다. 세트 효과 때문에 반지 하나만 빼도
	 * 세트 단계가 바뀌어 다른 아이템의 효과까지 달라지므로, 어차피 전체를 다시 계산해야 한다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Equipment);

	/**
	 * 패시브 스킬에서 온 모디파이어.
	 *
	 * 스킬별로 나누지 않고 전부 한 소스로 묶는다. 스킬 레벨이 하나 오르면 어차피
	 * 그 직업의 패시브를 다시 읽어 등록하는 편이 단순하다 — Source.Equipment 와 같은 판단이다.
	 */
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Skill);

	
	
	
	// ── 퀘스트 ────────────────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest_Type_Main);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest_Type_Sub);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest_State_Active);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest_State_ReadyToTurnIn);
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest_State_Completed);

	// ── 대화 작업 ─────────────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Dialogue_Action_AcceptQuest);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Dialogue_Action_TurnInQuest);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Dialogue_Action_ReportQuestEvent);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Dialogue_Action_CompleteDialogueQuest);
	
	// ── 퀘스트 진행 단계 ──────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Main_Prologue01_Accepted);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Main_Prologue01_Ready);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Main_Prologue01_Completed);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_HelpVillage01_Accepted);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_HelpVillage01_Ready);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_HelpVillage01_Completed);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_Treasure01_Accepted);

	// ── 퀘스트 이벤트 ─────────────────────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_GoHojin);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_KimSoohyun);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_KimHeejin);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_ParkJihoon);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_SeoAhyoung);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_LeeKyungjun);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_JangGoeun);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_HanSuhyun);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_HongJongwoo);
	
	
	// ── 테스트 서브 퀘스트 진행 단계 ───────────────────────

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_GoblinHunt01_Accepted);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_GoblinHunt01_Ready);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_GoblinHunt01_Completed);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu01_Accepted);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu01_Ready);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu01_Completed);
	
	// 서아영 점심 메뉴 테스트 퀘스트
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu02_Accepted);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu02_Ready);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Sub_LunchMenu02_Completed);
	
	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
	Quest_Event_Talk_N1);

	TD_PROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
		Quest_Event_Talk_N2);
	
	
}
