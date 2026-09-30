use super::request_auth;
use crate::{
    app_state::AppState,
    error::ApiResult,
    models::character::{CharacterListResponse, CreateCharacterRequest, CreateCharacterResponse},
    services::character_service,
};
use axum::{
    Json,
    extract::{Path, State},
    http::{HeaderMap, StatusCode},
};
pub async fn list_characters(
    State(state): State<AppState>,
    headers: HeaderMap,
) -> ApiResult<Json<CharacterListResponse>> {
    let account_id = request_auth::account_id(&state, &headers).await?;
    Ok(Json(
        character_service::list_characters(&state, account_id).await?,
    ))
}
pub async fn create_character(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<CreateCharacterRequest>,
) -> ApiResult<(StatusCode, Json<CreateCharacterResponse>)> {
    let account_id = request_auth::account_id(&state, &headers).await?;
    let response = character_service::create_character(&state, account_id, request).await?;
    Ok((StatusCode::CREATED, Json(response)))
}

pub async fn delete_character(
    State(state): State<AppState>,
    Path(character_id): Path<uuid::Uuid>,
    headers: HeaderMap,
) -> ApiResult<StatusCode> {
    let account_id = request_auth::account_id(&state, &headers).await?;
    character_service::delete_character(&state, account_id, character_id).await?;
    Ok(StatusCode::NO_CONTENT)
}
