use super::request_auth;
use crate::{
    app_state::AppState,
    error::ApiResult,
    models::save::{LoadSaveResponse, SaveRequest, SaveResponse},
    services::save_service,
};
use axum::{
    Json,
    extract::{Path, State},
    http::HeaderMap,
};
use uuid::Uuid;
pub async fn load_save(
    State(state): State<AppState>,
    Path(character_id): Path<Uuid>,
    headers: HeaderMap,
) -> ApiResult<Json<LoadSaveResponse>> {
    let account_id = if let Some(token) = lease_token(&headers)? {
        request_auth::require_game_server(&state, &headers)?;
        crate::services::lease_service::account(&state, character_id, &token).await?
    } else {
        request_auth::account_id(&state, &headers).await?
    };
    Ok(Json(
        save_service::load_character_save(&state, account_id, character_id).await?,
    ))
}
pub async fn save_character(
    State(state): State<AppState>,
    Path(character_id): Path<Uuid>,
    headers: HeaderMap,
    Json(request): Json<SaveRequest>,
) -> ApiResult<Json<SaveResponse>> {
    request_auth::require_game_server(&state, &headers)?;
    let lease = lease_token(&headers)?;
    let account_id = if let Some(token) = lease.as_deref() {
        crate::services::lease_service::account(&state, character_id, token).await?
    } else {
        request_auth::account_id(&state, &headers).await?
    };
    Ok(Json(
        save_service::save_character(&state, account_id, character_id, request, lease.as_deref())
            .await?,
    ))
}

fn lease_token(headers: &HeaderMap) -> ApiResult<Option<String>> {
    match headers.get("x-character-lease") {
        None => Ok(None),
        Some(v) => {
            let token = v.to_str().unwrap_or("");
            if token.len() != 64 || !token.bytes().all(|b| b.is_ascii_hexdigit()) {
                return Err(crate::error::bad_request("invalid_lease_token"));
            }
            Ok(Some(token.to_owned()))
        }
    }
}

pub async fn claim_lease(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
    headers: HeaderMap,
) -> ApiResult<axum::http::StatusCode> {
    request_auth::require_game_server(&state, &headers)?;
    let account = request_auth::account_id(&state, &headers).await?;
    let token = lease_token(&headers)?.ok_or(crate::error::bad_request("missing_lease_token"))?;
    crate::services::lease_service::claim(&state, account, id, &token).await?;
    Ok(axum::http::StatusCode::NO_CONTENT)
}
pub async fn renew_lease(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
    headers: HeaderMap,
) -> ApiResult<axum::http::StatusCode> {
    request_auth::require_game_server(&state, &headers)?;
    let token = lease_token(&headers)?.ok_or(crate::error::bad_request("missing_lease_token"))?;
    crate::services::lease_service::renew(&state, id, &token).await?;
    Ok(axum::http::StatusCode::NO_CONTENT)
}
pub async fn release_lease(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
    headers: HeaderMap,
) -> ApiResult<axum::http::StatusCode> {
    request_auth::require_game_server(&state, &headers)?;
    let token = lease_token(&headers)?.ok_or(crate::error::bad_request("missing_lease_token"))?;
    crate::services::lease_service::release(&state, id, &token).await?;
    Ok(axum::http::StatusCode::NO_CONTENT)
}
