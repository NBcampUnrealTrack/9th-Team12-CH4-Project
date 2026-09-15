use serde::{Deserialize, Serialize};
use serde_json::Value;
use uuid::Uuid;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CreateCharacterRequest {
    pub character_name: String,
    pub class_id: String,
}

#[derive(Serialize)]
pub struct CreateCharacterResponse {
    pub character_id: Uuid,
    pub slot_index: i16,
    pub character_name: String,
    pub class_id: String,
    pub revision: i64,
}

#[derive(Serialize)]
pub struct CharacterSummary {
    pub character_id: Uuid,
    pub slot_index: i16,
    pub character_name: String,
    pub class_id: String,
    pub level: Value,
    pub equipped_item_ids: Vec<Value>,
    pub revision: i64,
}

#[derive(Serialize)]
pub struct CharacterListResponse {
    pub max_characters: usize,
    pub characters: Vec<CharacterSummary>,
}
