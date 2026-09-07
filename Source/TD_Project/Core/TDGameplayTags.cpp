#include "Core/TDGameplayTags.h"

// 여기 적는 주석은 에디터의 게임플레이 태그 목록에 설명으로 표시됨
// 기획자가 태그를 고를 때 보게 되는 문구이므로, 코드 사정이 아니라 의미를 적는게 좋다

namespace TDTags
{
	// ── 스탯 관련 ────────────────────────────────────────────

	// ── 공격 ──────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_Damage, "Stat.Offense.Damage",
		"전체공격력. 여기 붙은 옵션은 물리·마법을 가리지 않고 모든 공격에 적용된다.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_Damage_Physical, "Stat.Offense.Damage.Physical", "물리공격력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_Damage_Magical, "Stat.Offense.Damage.Magical", "마법공격력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_CritChance, "Stat.Offense.CritChance", "치명타 확률 (0~1)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_CritDamage, "Stat.Offense.CritDamage", "치명타 피해 배율");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_ArmorPenetration, "Stat.Offense.ArmorPenetration",
		"방어력 무시. 대상 방어력을 이 비율만큼 무시하고 피해를 계산한다.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Offense_BossDamage, "Stat.Offense.BossDamage",
		"보스 몬스터에게 주는 추가 피해.");

	// ── 방어 ──────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Defense_Armor, "Stat.Defense.Armor", "방어력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Defense_DamageReduction, "Stat.Defense.DamageReduction",
		"받는 피해 감소. 0.2 면 받는 피해가 20% 줄어든다. 1.0 이면 무적이 되므로 상한을 반드시 건다.");

	// ── 자원 ──────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Resource_Health_Max, "Stat.Resource.Health.Max", "최대 체력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Resource_Health_Regen, "Stat.Resource.Health.Regen", "초당 체력 재생");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Resource_Mana_Max, "Stat.Resource.Mana.Max", "최대 마나");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Resource_Mana_Regen, "Stat.Resource.Mana.Regen", "초당 마나 재생");

	// ── 유틸리티 ──────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Utility_CooldownRecoveryRate, "Stat.Utility.CooldownRecoveryRate",
		"쿨다운 회복률. 실제 쿨다운은 기본값을 (1 + 이 수치) 로 나눠서 구한다. 감소율이 아니므로 쌓아도 0 이 되지 않는다.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Utility_MoveSpeed, "Stat.Utility.MoveSpeed", "이동 속도");


	// ── 아이템 ────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Accessory, "Item.Type.Accessory",
		"장신구. 장착할 수 있고 추가 옵션을 가진다. 스택되지 않는다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Consumable, "Item.Type.Consumable",
		"소비 아이템. 포션, 인벤토리 확장권 등. 스택된다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Misc, "Item.Type.Misc",
		"기타 아이템. 재료 등. 스택된다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Quest, "Item.Type.Quest",
		"퀘스트 아이템. 버릴 수 없다.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Rarity_Common, "Item.Rarity.Common", "일반");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Rarity_Rare, "Item.Rarity.Rare", "희귀");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Rarity_Epic, "Item.Rarity.Epic", "영웅");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Rarity_Legendary, "Item.Rarity.Legendary", "전설");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Effect_ExpandInventory, "Item.Effect.ExpandInventory",
		"인벤토리 칸을 Value 만큼 늘린다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Effect_RestoreHealth, "Item.Effect.RestoreHealth",
		"체력을 Value 만큼 회복한다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Effect_RestoreMana, "Item.Effect.RestoreMana",
		"마나를 Value 만큼 회복한다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Effect_GainExp, "Item.Effect.GainExp",
		"경험치를 Value 만큼 얻는다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Effect_LevelUp, "Item.Effect.LevelUp",
		"Value 레벨 미만이면 즉시 1레벨업, 그 이상이면 Value 레벨 한 구간만큼의 경험치만 얻는다.");


	// ── 존(맵) ────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1, "Zone.Region1",
		"첫 번째 지역 전체. 마을과 사냥터, 보스방을 포함한다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Town, "Zone.Region1.Town", "지역1 마을");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Field01, "Zone.Region1.Field01", "지역1 사냥터 1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Field02, "Zone.Region1.Field02", "지역1 사냥터 2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Field03, "Zone.Region1.Field03", "지역1 사냥터 3");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Boss, "Zone.Region1.Boss", "지역1 보스 구역 (하위 방들의 상위 태그)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Boss_Room01, "Zone.Region1.Boss.Room01", "지역1 보스방 1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Boss_Room02, "Zone.Region1.Boss.Room02", "지역1 보스방 2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Boss_Room03, "Zone.Region1.Boss.Room03", "지역1 보스방 3");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region1_Boss_Room04, "Zone.Region1.Boss.Room04", "지역1 보스방 4");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2, "Zone.Region2",
		"두 번째 지역 전체.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Town, "Zone.Region2.Town", "지역2 마을");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Field01, "Zone.Region2.Field01", "지역2 사냥터 1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Field02, "Zone.Region2.Field02", "지역2 사냥터 2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Boss, "Zone.Region2.Boss", "지역2 보스 구역 (하위 방들의 상위 태그)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Boss_Room01, "Zone.Region2.Boss.Room01", "지역2 보스방 1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Boss_Room02, "Zone.Region2.Boss.Room02", "지역2 보스방 2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Boss_Room03, "Zone.Region2.Boss.Room03", "지역2 보스방 3");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Zone_Region2_Boss_Room04, "Zone.Region2.Boss.Room04", "지역2 보스방 4");


	// ── 모디파이어 출처 ───────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Progression, "Source.Progression",
		"레벨 성장에서 온 모디파이어.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Equipment, "Source.Equipment",
		"장착 중인 장신구와 세트 효과에서 온 모디파이어.");
	
	
	
	// ── 퀘스트 ────────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Type_Main,
		"Quest.Type.Main",
		"메인 퀘스트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Type_Sub,
		"Quest.Type.Sub",
		"서브 퀘스트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_State_Active,
		"Quest.State.Active",
		"진행 중인 퀘스트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_State_ReadyToTurnIn,
		"Quest.State.ReadyToTurnIn",
		"목표를 달성하여 완료 보고할 수 있는 퀘스트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_State_Completed,
		"Quest.State.Completed",
		"보상까지 받은 완료 퀘스트");

	// ── 대화 작업 ─────────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Dialogue_Action_AcceptQuest,
		"Dialogue.Action.AcceptQuest",
		"대화 진행 시 퀘스트 수락");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Dialogue_Action_TurnInQuest,
		"Dialogue.Action.TurnInQuest",
		"대화 진행 시 퀘스트 완료 및 보상 지급");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Dialogue_Action_ReportQuestEvent,
		"Dialogue.Action.ReportQuestEvent",
		"대화 진행 시 퀘스트 목표 진행");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Dialogue_Action_CompleteDialogueQuest,
		"Dialogue.Action.CompleteDialogueQuest",
		"대화 목표를 진행하고 같은 입력에서 퀘스트 완료와 보상 지급까지 처리");
	
	// ── 퀘스트 진행 단계 ──────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Main_Prologue01_Accepted,
		"Quest.Main.Prologue01.Accepted",
		"메인 퀘스트 Prologue01을 수락하여 진행 중인 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Main_Prologue01_Ready,
		"Quest.Main.Prologue01.Ready",
		"메인 퀘스트 Prologue01의 목표를 달성하여 완료 보고 가능한 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Main_Prologue01_Completed,
		"Quest.Main.Prologue01.Completed",
		"메인 퀘스트 Prologue01의 보상까지 받은 완료 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Sub_HelpVillage01_Accepted,
		"Quest.Sub.HelpVillage01.Accepted",
		"서브 퀘스트 HelpVillage01을 수락하여 진행 중인 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Sub_HelpVillage01_Ready,
		"Quest.Sub.HelpVillage01.Ready",
		"서브 퀘스트 HelpVillage01의 목표를 달성하여 완료 보고 가능한 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Sub_HelpVillage01_Completed,
		"Quest.Sub.HelpVillage01.Completed",
		"서브 퀘스트 HelpVillage01의 보상까지 받은 완료 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Sub_Treasure01_Accepted,
		"Quest.Sub.Treasure01.Accepted",
		"개인 보물상자 테스트용 퀘스트를 수락한 상태");

	// ── 퀘스트 이벤트 ─────────────────────────────────────

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_GoHojin,
		"Quest.Event.Talk.GoHojin",
		"고호진 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_KimSoohyun,
		"Quest.Event.Talk.KimSoohyun",
		"김수현 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_KimHeejin,
		"Quest.Event.Talk.KimHeejin",
		"김희진 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_ParkJihoon,
		"Quest.Event.Talk.ParkJihoon",
		"박지훈 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_SeoAhyoung,
		"Quest.Event.Talk.SeoAhyoung",
		"서아영 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_LeeKyungjun,
		"Quest.Event.Talk.LeeKyungjun",
		"이경준 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_JangGoeun,
		"Quest.Event.Talk.JangGoeun",
		"장고은 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_HanSuhyun,
		"Quest.Event.Talk.HanSuhyun",
		"한수현 NPC와 필요한 대화를 완료한 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Quest_Event_Talk_HongJongwoo,
		"Quest.Event.Talk.HongJongwoo",
		"홍종우 NPC와 필요한 대화를 완료한 이벤트");

}
