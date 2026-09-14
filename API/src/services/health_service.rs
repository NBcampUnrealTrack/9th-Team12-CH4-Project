use crate::{app_state::AppState, error::ApiResult};
pub async fn check_database(state: &AppState) -> ApiResult<()> {
    sqlx::query("SELECT 1").execute(&state.database).await?;
    Ok(())
}
