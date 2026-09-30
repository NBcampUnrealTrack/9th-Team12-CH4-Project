use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult},
};
use axum::http::StatusCode;
use sha2::{Digest, Sha256};
use subtle::ConstantTimeEq;
pub(crate) fn hash_text(s: &str) -> String {
    hex::encode(Sha256::digest(s.as_bytes()))
}
pub fn validate_game_server_key(state: &AppState, supplied: &str) -> ApiResult<()> {
    if bool::from(
        hash_text(supplied)
            .as_bytes()
            .ct_eq(hash_text(&state.game_server_key).as_bytes()),
    ) {
        Ok(())
    } else {
        Err(ApiError(StatusCode::FORBIDDEN, "game_server_required"))
    }
}
