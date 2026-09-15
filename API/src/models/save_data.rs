use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::HashSet;

// 언리얼 JSON 필드명을 유지한다. 태그는 문자열로 저장하고 FastArray 복제용 ID는 제외한다.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct SaveData {
    pub save_version: i32,
    pub class_id: String,
    pub level: i32,
    pub exp: i32,
    pub skill_levels: Vec<Skill>,
    pub inventory_slot_capacity: i32,
    pub inventory_items: Vec<Item>,
    pub equipped_items: Vec<Item>,
    pub health_ratio: f64,
    pub mana_ratio: f64,
    pub last_zone_id: String,
    pub last_location: Position,
    #[serde(rename = "bHasSavedLocation")]
    pub has_saved_location: bool,
    pub quest_progress_tags: Vec<String>,
    pub claimed_chest_ids: Vec<String>,
    pub chest_claim_records: Vec<Chest>,
    pub seen_chapter_ids: Vec<String>,
    pub npc_gift_records: Vec<Gift>,
    pub quick_slots: Vec<QuickSlot>,
    pub gold: i32,
    pub quest_states: Vec<Quest>,
    pub affection_states: Vec<Affection>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Item {
    pub item_id: String,
    pub slot_index: i32,
    pub count: i32,
    pub enhance_level: i32,
    pub option_rarity: String,
    pub options: Vec<ItemOption>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct ItemOption {
    pub option_id: String,
    pub value: f64,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Skill {
    pub skill_id: String,
    pub level: i32,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Position {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Chest {
    pub chest_id: String,
    pub last_claim_kst_day_key: i32,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Gift {
    #[serde(rename = "NPCId")]
    pub npc_id: String,
    pub last_gift_kst_day_key: i32,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Affection {
    #[serde(rename = "NPCId")]
    pub npc_id: String,
    pub points: i32,
    pub last_gift_kst_day_key: i32,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct Quest {
    pub quest_id: String,
    pub state_tag: String,
    pub objective_progress: Vec<i32>,
    pub accept_sequence: i64,
    pub completed_kst_day_key: i32,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase", deny_unknown_fields)]
pub struct QuickSlot {
    #[serde(rename = "Type")]
    pub kind: String,
    pub id: String,
}

fn valid_id(s: &str) -> bool {
    !s.is_empty() && s.len() <= 256 && !s.chars().any(char::is_control)
}
fn unique<'a>(mut ids: impl Iterator<Item = &'a str>) -> bool {
    let mut seen = HashSet::new();
    ids.all(|s| valid_id(s) && seen.insert(s))
}
impl SaveData {
    pub fn initial(class_id: &str) -> Self {
        serde_json::from_value(json!({
            "SaveVersion":2,"ClassId":class_id,"Level":1,"Exp":0,"SkillLevels":[],
            "InventorySlotCapacity":40,"InventoryItems":[],"EquippedItems":[],
            "HealthRatio":1.0,"ManaRatio":1.0,"LastZoneId":"Zone.Region1.Town",
            "LastLocation":{"X":-738.964579,"Y":1455.173839,"Z":10285.933642},"bHasSavedLocation":true,
            "QuestProgressTags":[],"ClaimedChestIds":[],"ChestClaimRecords":[],"SeenChapterIds":[],
            "NpcGiftRecords":[],"QuickSlots":[],"Gold":0,"QuestStates":[],"AffectionStates":[]
        }))
        .expect("static initial save contract")
    }
    pub fn validate(&self) -> Result<(), &'static str> {
        if self.save_version != 2 {
            return Err("unsupported_save_version");
        }
        if !["Warrior", "Mage", "Archer"].contains(&self.class_id.as_str())
            || self.level < 1
            || self.exp < 0
            || self.gold < 0
            || !(1..=10000).contains(&self.inventory_slot_capacity)
        {
            return Err("invalid_progression");
        }
        if ![self.health_ratio, self.mana_ratio]
            .iter()
            .all(|v| v.is_finite() && (0.0..=1.0).contains(v))
            || ![
                self.last_location.x,
                self.last_location.y,
                self.last_location.z,
            ]
            .iter()
            .all(|v| v.is_finite())
            || (self.has_saved_location && !valid_id(&self.last_zone_id))
        {
            return Err("invalid_vitals_or_location");
        }
        for (items, capacity) in [
            (&self.inventory_items, self.inventory_slot_capacity),
            (&self.equipped_items, 6),
        ] {
            let mut slots = HashSet::new();
            for item in items {
                if !valid_id(&item.item_id)
                    || !(0..capacity).contains(&item.slot_index)
                    || !slots.insert(item.slot_index)
                    || item.count < 1
                    || item.enhance_level < 0
                    || !unique(item.options.iter().map(|v| v.option_id.as_str()))
                    || item.options.iter().any(|v| !v.value.is_finite())
                {
                    return Err("invalid_item");
                }
            }
        }
        if self.equipped_items.iter().any(|i| i.count != 1)
            || !unique(self.equipped_items.iter().map(|i| i.item_id.as_str()))
        {
            return Err("invalid_equipment");
        }
        if self.quick_slots.len() > 6
            || self
                .quick_slots
                .iter()
                .any(|q| !(q.kind == "Empty" || q.kind == "Item" && valid_id(&q.id)))
        {
            return Err("invalid_quick_slots");
        }
        if !unique(self.skill_levels.iter().map(|s| s.skill_id.as_str()))
            || self.skill_levels.iter().any(|s| s.level < 0)
        {
            return Err("invalid_skills");
        }
        if !unique(self.quest_states.iter().map(|q| q.quest_id.as_str()))
            || self.quest_states.iter().any(|q| {
                q.objective_progress.iter().any(|n| *n < 0)
                    || q.accept_sequence < 0
                    || q.completed_kst_day_key < 0
                    || ![
                        "Quest.State.Active",
                        "Quest.State.ReadyToTurnIn",
                        "Quest.State.Completed",
                    ]
                    .contains(&q.state_tag.as_str())
            })
        {
            return Err("invalid_quests");
        }
        if !unique(self.affection_states.iter().map(|a| a.npc_id.as_str()))
            || self
                .affection_states
                .iter()
                .any(|a| a.points < 0 || a.last_gift_kst_day_key < 0)
            || !unique(self.npc_gift_records.iter().map(|a| a.npc_id.as_str()))
            || self
                .npc_gift_records
                .iter()
                .any(|a| a.last_gift_kst_day_key < 0)
            || !unique(self.chest_claim_records.iter().map(|a| a.chest_id.as_str()))
            || self
                .chest_claim_records
                .iter()
                .any(|a| a.last_claim_kst_day_key < 0)
        {
            return Err("invalid_personal_progress");
        }
        for ids in [
            &self.quest_progress_tags,
            &self.claimed_chest_ids,
            &self.seen_chapter_ids,
        ] {
            if !unique(ids.iter().map(String::as_str)) {
                return Err("invalid_progress_ids");
            }
        }
        Ok(())
    }
    pub fn json(&self) -> Value {
        serde_json::to_value(self).expect("validated finite save")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn initial_round_trip() {
        let d = SaveData::initial("Warrior");
        assert!(d.validate().is_ok());
        let json = d.json();
        assert_eq!(json["LastZoneId"], "Zone.Region1.Town");
        assert_eq!(
            json["LastLocation"],
            serde_json::json!({"X":-738.964579,"Y":1455.173839,"Z":10285.933642})
        );
        assert_eq!(json["bHasSavedLocation"], true);
        assert_eq!(
            serde_json::from_value::<SaveData>(d.json()).unwrap().json(),
            d.json()
        );
    }
    #[test]
    fn invalid_version_and_money() {
        let mut d = SaveData::initial("Mage");
        d.save_version = 9;
        assert!(d.validate().is_err());
        d.save_version = 2;
        d.gold = -1;
        assert!(d.validate().is_err());
    }
    #[test]
    fn unknown_field_rejected() {
        let mut v = SaveData::initial("Archer").json();
        v["GoldTypo"] = json!(10);
        assert!(serde_json::from_value::<SaveData>(v).is_err());
    }
}
