use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult, bad_request},
    models::{character::*, save_data::SaveData},
};
use axum::http::StatusCode;
use serde_json::Value;
use sqlx::Row;
use uuid::Uuid;

pub async fn list_characters(
    state: &AppState,
    account_id: Uuid,
) -> ApiResult<CharacterListResponse> {
    let rows = sqlx::query(
        "SELECT id,slot_index,name,class_id,save_data,revision FROM td_characters WHERE account_id=$1 ORDER BY slot_index"
    )
        .bind(account_id)
        .fetch_all(&state.database)
        .await?;
    let characters = rows
        .into_iter()
        .map(|row| {
            let data: Value = row.get("save_data");
            CharacterSummary {
                character_id: row.get("id"),
                slot_index: row.get("slot_index"),
                character_name: row.get("name"),
                class_id: row.get("class_id"),
                level: data["Level"].clone(),
                equipped_item_ids: data["EquippedItems"]
                    .as_array()
                    .map(|items| items.iter().map(|item| item["ItemId"].clone()).collect())
                    .unwrap_or_default(),
                revision: row.get("revision"),
            }
        })
        .collect();
    Ok(CharacterListResponse {
        max_characters: 6,
        characters,
    })
}
pub async fn create_character(
    state: &AppState,
    account_id: Uuid,
    request: CreateCharacterRequest,
) -> ApiResult<CreateCharacterResponse> {
    let name = request.character_name.trim();
    if !(2..=16).contains(&name.chars().count())
        || !name.chars().all(|c| c.is_alphanumeric() || c == '_')
    {
        return Err(bad_request("invalid_character_name"));
    }
    let data = SaveData::initial(&request.class_id);
    data.validate().map_err(bad_request)?;
    let mut transaction = state.database.begin().await?;
    sqlx::query("SELECT id FROM td_accounts WHERE id=$1 FOR UPDATE")
        .bind(account_id)
        .fetch_one(&mut *transaction)
        .await?;
    let slots: Vec<i16> =
        sqlx::query_scalar("SELECT slot_index FROM td_characters WHERE account_id=$1")
            .bind(account_id)
            .fetch_all(&mut *transaction)
            .await?;
    let slot = (0i16..6)
        .find(|i| !slots.contains(i))
        .ok_or(ApiError(StatusCode::CONFLICT, "character_slots_full"))?;
    let character_id = Uuid::new_v4();
    sqlx::query("INSERT INTO td_characters(id,account_id,slot_index,name,name_key,class_id,save_data) VALUES($1,$2,$3,$4,$5,$6,$7)")
        .bind(character_id)
        .bind(account_id)
        .bind(slot)
        .bind(name)
        .bind(name.to_lowercase())
        .bind(&request.class_id)
        .bind(data.json())
        .execute(&mut *transaction)
        .await?;
    transaction.commit().await?;
    Ok(CreateCharacterResponse {
        character_id,
        slot_index: slot,
        character_name: name.to_owned(),
        class_id: request.class_id,
        revision: 0,
    })
}

pub async fn delete_character(
    state: &AppState,
    account_id: Uuid,
    character_id: Uuid,
) -> ApiResult<()> {
    let mut transaction = state.database.begin().await?;
    sqlx::query("SELECT id FROM td_accounts WHERE id=$1 FOR UPDATE")
        .bind(account_id)
        .fetch_one(&mut *transaction)
        .await?;
    sqlx::query("SELECT id FROM td_characters WHERE id=$1 AND account_id=$2 FOR UPDATE")
        .bind(character_id)
        .bind(account_id)
        .fetch_optional(&mut *transaction)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "character_not_found"))?;
    let in_use: bool = sqlx::query_scalar("SELECT EXISTS(SELECT 1 FROM td_character_leases WHERE character_id=$1 AND expires_at>now())")
        .bind(character_id).fetch_one(&mut *transaction).await?;
    if in_use {
        return Err(ApiError(StatusCode::CONFLICT, "character_in_use"));
    }
    let result = sqlx::query("DELETE FROM td_characters WHERE id=$1 AND account_id=$2")
        .bind(character_id)
        .bind(account_id)
        .execute(&mut *transaction)
        .await?;
    if result.rows_affected() == 0 {
        return Err(ApiError(StatusCode::NOT_FOUND, "character_not_found"));
    }
    transaction.commit().await?;
    Ok(())
}
