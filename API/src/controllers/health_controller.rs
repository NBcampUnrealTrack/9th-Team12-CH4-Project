use crate::{app_state::AppState, error::ApiResult, services::health_service};
use axum::{Json, extract::State};
use serde_json::{Value, json};
pub async fn health(State(state): State<AppState>) -> ApiResult<Json<Value>> {
    health_service::check_database(&state).await?;
    Ok(Json(json!({"status": "ok"})))
}
