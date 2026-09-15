use crate::{
    app_state::AppState,
    controllers::{auth_controller, character_controller, health_controller, save_controller},
};
use axum::{
    Router,
    extract::DefaultBodyLimit,
    routing::{delete, get, post},
};
pub fn create_router(state: AppState) -> Router {
    Router::new()
        .route("/health", get(health_controller::health))
        .route("/v1/auth/register", post(auth_controller::register))
        .route("/v1/auth/login", post(auth_controller::login))
        .route("/v1/auth/logout", post(auth_controller::logout))
        .route(
            "/v1/characters",
            get(character_controller::list_characters).post(character_controller::create_character),
        )
        .route(
            "/v1/characters/{id}/save",
            get(save_controller::load_save).put(save_controller::save_character),
        )
        .route(
            "/v1/characters/{id}",
            delete(character_controller::delete_character),
        )
        .layer(DefaultBodyLimit::max(1024 * 1024))
        .with_state(state)
}
