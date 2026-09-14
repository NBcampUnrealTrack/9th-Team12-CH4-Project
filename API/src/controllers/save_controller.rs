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
    let account_id = request_auth::account_id(&state, &headers).await?;
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
    let account_id = request_auth::account_id(&state, &headers).await?;
    Ok(Json(
        save_service::save_character(&state, account_id, character_id, request).await?,
    ))
}
