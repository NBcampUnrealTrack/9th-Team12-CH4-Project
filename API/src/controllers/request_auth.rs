use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult},
    services::{auth_service, security},
};
use axum::http::{HeaderMap, StatusCode};
use uuid::Uuid;
pub(super) fn bearer_token(headers: &HeaderMap) -> ApiResult<&str> {
    headers
        .get("authorization")
        .and_then(|value| value.to_str().ok())
        .and_then(|value| value.strip_prefix("Bearer "))
        .filter(|value| value.len() == 64)
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "unauthorized"))
}
pub(super) async fn account_id(state: &AppState, headers: &HeaderMap) -> ApiResult<Uuid> {
    auth_service::authenticate_session(state, bearer_token(headers)?).await
}
pub(super) fn require_game_server(state: &AppState, headers: &HeaderMap) -> ApiResult<()> {
    let key = headers
        .get("x-game-server-key")
        .and_then(|value| value.to_str().ok())
        .unwrap_or("");
    security::validate_game_server_key(state, key)
}
