use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult},
    services::security::hash_text,
};
use axum::http::StatusCode;
use sqlx::Row;
use uuid::Uuid;

// Account then character is the common lock order for claim/delete/create.
pub async fn claim(state: &AppState, account: Uuid, character: Uuid, token: &str) -> ApiResult<()> {
    let mut tx = state.database.begin().await?;
    sqlx::query("SELECT id FROM td_accounts WHERE id=$1 FOR UPDATE")
        .bind(account)
        .fetch_one(&mut *tx)
        .await?;
    sqlx::query("SELECT id FROM td_characters WHERE id=$1 AND account_id=$2 FOR UPDATE")
        .bind(character)
        .bind(account)
        .fetch_optional(&mut *tx)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "character_not_found"))?;
    sqlx::query("DELETE FROM td_character_leases WHERE account_id=$1 AND expires_at<=now()")
        .bind(account)
        .execute(&mut *tx)
        .await?;
    let hash = hash_text(token);
    if let Some(row) =
        sqlx::query("SELECT character_id,token_hash FROM td_character_leases WHERE account_id=$1")
            .bind(account)
            .fetch_optional(&mut *tx)
            .await?
    {
        if row.get::<Uuid, _>("character_id") != character
            || row.get::<String, _>("token_hash") != hash
        {
            return Err(ApiError(StatusCode::CONFLICT, "character_in_use"));
        }
    }
    sqlx::query("INSERT INTO td_character_leases(character_id,account_id,token_hash,expires_at) VALUES($1,$2,$3,now()+interval '120 seconds') ON CONFLICT(character_id) DO UPDATE SET expires_at=EXCLUDED.expires_at")
        .bind(character).bind(account).bind(hash).execute(&mut *tx).await?;
    tx.commit().await?;
    Ok(())
}

pub async fn renew(state: &AppState, character: Uuid, token: &str) -> ApiResult<()> {
    let n = sqlx::query("UPDATE td_character_leases SET expires_at=now()+interval '120 seconds' WHERE character_id=$1 AND token_hash=$2 AND expires_at>now()")
        .bind(character).bind(hash_text(token)).execute(&state.database).await?.rows_affected();
    if n == 0 {
        return Err(ApiError(StatusCode::CONFLICT, "lease_lost"));
    }
    Ok(())
}

pub async fn release(state: &AppState, character: Uuid, token: &str) -> ApiResult<()> {
    sqlx::query("DELETE FROM td_character_leases WHERE character_id=$1 AND token_hash=$2")
        .bind(character)
        .bind(hash_text(token))
        .execute(&state.database)
        .await?;
    Ok(())
}

pub async fn account(state: &AppState, character: Uuid, token: &str) -> ApiResult<Uuid> {
    sqlx::query_scalar("SELECT account_id FROM td_character_leases WHERE character_id=$1 AND token_hash=$2 AND expires_at>now()")
        .bind(character).bind(hash_text(token)).fetch_optional(&state.database).await?
        .ok_or(ApiError(StatusCode::CONFLICT, "lease_lost"))
}
