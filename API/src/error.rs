use axum::{
    Json,
    http::StatusCode,
    response::{IntoResponse, Response},
};
use serde_json::json;
pub struct ApiError(pub StatusCode, pub &'static str);
impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        tracing::warn!(
            status = self.0.as_u16(),
            code = self.1,
            "API request rejected"
        );
        (self.0, Json(json!({"error":{"code":self.1}}))).into_response()
    }
}
impl From<sqlx::Error> for ApiError {
    fn from(e: sqlx::Error) -> Self {
        if e.as_database_error()
            .is_some_and(|x| x.is_unique_violation())
        {
            return Self(StatusCode::CONFLICT, "already_exists");
        }
        tracing::error!(error=%e,"database operation failed");
        Self(StatusCode::INTERNAL_SERVER_ERROR, "database_error")
    }
}
pub type ApiResult<T> = Result<T, ApiError>;
pub fn bad_request(s: &'static str) -> ApiError {
    ApiError(StatusCode::BAD_REQUEST, s)
}
