use super::security::hash_text;
use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult, bad_request},
    models::save::{LoadSaveResponse, SaveRequest, SaveResponse},
};
use axum::http::StatusCode;
use serde_json::json;
use sqlx::Row;
use uuid::Uuid;
pub async fn load_character_save(
    state: &AppState,
    account_id: Uuid,
    character_id: Uuid,
) -> ApiResult<LoadSaveResponse> {
    let row =
        sqlx::query("SELECT save_data,revision FROM td_characters WHERE id=$1 AND account_id=$2")
            .bind(character_id)
            .bind(account_id)
            .fetch_optional(&state.database)
            .await?
            .ok_or(ApiError(StatusCode::NOT_FOUND, "character_not_found"))?;
    Ok(LoadSaveResponse {
        character_id,
        revision: row.get("revision"),
        data: row.get("save_data"),
    })
}
pub async fn save_character(
    state: &AppState,
    account_id: Uuid,
    character_id: Uuid,
    request: SaveRequest,
    lease: Option<&str>,
) -> ApiResult<SaveResponse> {
    request.data.validate().map_err(bad_request)?;
    if request.expected_revision < 0 {
        return Err(bad_request("invalid_revision"));
    }
    let data = request.data.json();
    let payload_hash =
        hash_text(&json!({"expected_revision":request.expected_revision,"data":data}).to_string());
    let mut transaction = state.database.begin().await?;
    let row = sqlx::query(
        "SELECT class_id,revision FROM td_characters WHERE id=$1 AND account_id=$2 FOR UPDATE",
    )
    .bind(character_id)
    .bind(account_id)
    .fetch_optional(&mut *transaction)
    .await?
    .ok_or(ApiError(StatusCode::NOT_FOUND, "character_not_found"))?;
    // Lock the lease through commit so release/expiry cannot authorize a stale writer.
    let active = sqlx::query("SELECT token_hash,expires_at>now() AS live FROM td_character_leases WHERE character_id=$1 FOR UPDATE")
        .bind(character_id).fetch_optional(&mut *transaction).await?;
    match (lease, active) {
        (Some(token), Some(active))
            if active.get::<bool, _>("live")
                && active.get::<String, _>("token_hash") == hash_text(token) => {}
        (Some(_), _) => return Err(ApiError(StatusCode::CONFLICT, "lease_lost")),
        (None, Some(active)) if active.get::<bool, _>("live") => {
            return Err(ApiError(StatusCode::CONFLICT, "character_in_use"));
        }
        _ => {}
    }
    let previous_receipt = sqlx::query(
        "SELECT payload_hash,revision FROM td_save_receipts WHERE character_id=$1 AND request_id=$2",
    )
        .bind(character_id)
        .bind(request.request_id)
        .fetch_optional(&mut *transaction)
        .await?;
    // 과거 저장 요청의 재시도는 당시 결과만 반환하고 최신 데이터를 덮어쓰지 않는다.
    if let Some(receipt) = previous_receipt {
        if receipt.get::<String, _>("payload_hash") != payload_hash {
            return Err(ApiError(StatusCode::CONFLICT, "idempotency_key_reused"));
        }
        return Ok(SaveResponse {
            character_id,
            request_id: request.request_id,
            revision: receipt.get("revision"),
            replayed: true,
        });
    }
    if row.get::<String, _>("class_id") != request.data.class_id {
        return Err(bad_request("class_change_not_allowed"));
    }
    let current: i64 = row.get("revision");
    if current != request.expected_revision {
        return Err(ApiError(StatusCode::CONFLICT, "save_revision_conflict"));
    }
    let next = current
        .checked_add(1)
        .ok_or(bad_request("revision_overflow"))?;
    sqlx::query("UPDATE td_characters SET save_data=$1,revision=$2,updated_at=now() WHERE id=$3")
        .bind(data)
        .bind(next)
        .bind(character_id)
        .execute(&mut *transaction)
        .await?;
    sqlx::query("INSERT INTO td_save_receipts(character_id,request_id,payload_hash,revision) VALUES($1,$2,$3,$4)")
        .bind(character_id)
        .bind(request.request_id)
        .bind(payload_hash)
        .bind(next)
        .execute(&mut *transaction)
        .await?;
    transaction.commit().await?;
    Ok(SaveResponse {
        character_id,
        request_id: request.request_id,
        revision: next,
        replayed: false,
    })
}
