use super::request_auth::bearer_token;
use crate::{
    app_state::AppState,
    error::ApiResult,
    models::auth::{Credentials, LoginResponse, RegisterResponse},
    services::auth_service,
};
use axum::{
    Json,
    extract::{ConnectInfo, State},
    http::{HeaderMap, StatusCode},
};
use std::net::SocketAddr;
pub async fn register(
    State(state): State<AppState>,
    ConnectInfo(address): ConnectInfo<SocketAddr>,
    Json(request): Json<Credentials>,
) -> ApiResult<(StatusCode, Json<RegisterResponse>)> {
    let response = auth_service::register_account(&state, address.ip(), request).await?;
    Ok((StatusCode::CREATED, Json(response)))
}
pub async fn login(
    State(state): State<AppState>,
    ConnectInfo(address): ConnectInfo<SocketAddr>,
    Json(request): Json<Credentials>,
) -> ApiResult<Json<LoginResponse>> {
    Ok(Json(
        auth_service::login_account(&state, address.ip(), request).await?,
    ))
}
pub async fn logout(State(state): State<AppState>, headers: HeaderMap) -> ApiResult<StatusCode> {
    auth_service::logout_account(&state, bearer_token(&headers)?).await?;
    Ok(StatusCode::NO_CONTENT)
}
