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
            "/v1/characters/{id}/lease",
            post(save_controller::claim_lease)
                .put(save_controller::renew_lease)
                .delete(save_controller::release_lease),
        )
        .route(
            "/v1/characters/{id}",
            delete(character_controller::delete_character),
        )
        .layer(DefaultBodyLimit::max(1024 * 1024))
        .layer(axum::middleware::from_fn(log_request))
        .with_state(state)
}

async fn log_request(
    request: axum::extract::Request,
    next: axum::middleware::Next,
) -> axum::response::Response {
    let method = request.method().clone();
    let route = request
        .extensions()
        .get::<axum::extract::MatchedPath>()
        .map(|path| path.as_str().to_owned())
        .unwrap_or_else(|| "unmatched".to_owned());
    let started = std::time::Instant::now();
    let response = next.run(request).await;
    tracing::info!(%method, %route, status = response.status().as_u16(), elapsed_ms = started.elapsed().as_millis() as u64, "HTTP request completed");
    response
}
