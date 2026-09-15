use super::save_data::SaveData;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use uuid::Uuid;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct SaveRequest {
    pub request_id: Uuid,
    pub expected_revision: i64,
    pub data: SaveData,
}

#[derive(Serialize)]
pub struct LoadSaveResponse {
    pub character_id: Uuid,
    pub revision: i64,
    pub data: Value,
}

#[derive(Serialize)]
pub struct SaveResponse {
    pub character_id: Uuid,
    pub request_id: Uuid,
    pub revision: i64,
    pub replayed: bool,
}
