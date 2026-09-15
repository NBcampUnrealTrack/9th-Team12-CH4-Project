use super::security::hash_text;
use crate::{
    app_state::AppState,
    error::{ApiError, ApiResult, bad_request},
    models::auth::{Credentials, LoginResponse, RegisterResponse},
};
use argon2::{Argon2, PasswordHash, PasswordHasher, PasswordVerifier, password_hash::SaltString};
use axum::http::StatusCode;
use rand_core::OsRng;
use sqlx::Row;
use std::{
    net::IpAddr,
    time::{Duration, Instant},
};
use uuid::Uuid;

pub async fn authenticate_session(state: &AppState, access_token: &str) -> ApiResult<Uuid> {
    sqlx::query_scalar(
        "SELECT account_id FROM td_sessions WHERE token_hash=$1 AND expires_at>now()",
    )
    .bind(hash_text(access_token))
    .fetch_optional(&state.database)
    .await?
    .ok_or(ApiError(StatusCode::UNAUTHORIZED, "unauthorized"))
}
async fn check_login_rate_limit(state: &AppState, ip: IpAddr) -> ApiResult<()> {
    let mut login_attempts = state.login_attempts.lock().await;
    login_attempts.retain(|_, v| v.0.elapsed() < Duration::from_secs(60));
    let v = login_attempts.entry(ip).or_insert((Instant::now(), 0));
    v.1 += 1;
    if v.1 > 30 {
        Err(ApiError(StatusCode::TOO_MANY_REQUESTS, "auth_rate_limited"))
    } else {
        Ok(())
    }
}
fn normalize_login_id(s: &str) -> ApiResult<String> {
    let s = s.trim().to_ascii_lowercase();
    if !(2..=32).contains(&s.len()) || !s.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'_') {
        Err(bad_request("invalid_login_id"))
    } else {
        Ok(s)
    }
}
pub async fn register_account(
    state: &AppState,
    client_ip: IpAddr,
    request: Credentials,
) -> ApiResult<RegisterResponse> {
    check_login_rate_limit(state, client_ip).await?;
    let login = normalize_login_id(&request.login_id)?;
    if !(8..=128).contains(&request.password.len()) {
        return Err(bad_request("password_length_8_to_128_bytes"));
    }
    let permit = state
        .password_tasks
        .clone()
        .acquire_owned()
        .await
        .map_err(|_| bad_request("unavailable"))?;
    let hash = tokio::task::spawn_blocking(move || {
        let _permit = permit;
        Argon2::default()
            .hash_password(
                request.password.as_bytes(),
                &SaltString::generate(&mut OsRng),
            )
            .map(|h| h.to_string())
    })
    .await
    .map_err(|_| bad_request("hash_failed"))?
    .map_err(|_| bad_request("hash_failed"))?;
    let id = Uuid::new_v4();
    sqlx::query("INSERT INTO td_accounts(id,login_id,password_hash) VALUES($1,$2,$3)")
        .bind(id)
        .bind(&login)
        .bind(hash)
        .execute(&state.database)
        .await?;
    Ok(RegisterResponse {
        account_id: id,
        login_id: login,
    })
}
pub async fn login_account(
    state: &AppState,
    client_ip: IpAddr,
    request: Credentials,
) -> ApiResult<LoginResponse> {
    check_login_rate_limit(state, client_ip).await?;
    let login = normalize_login_id(&request.login_id)?;
    if request.password.len() > 128 {
        return Err(ApiError(StatusCode::UNAUTHORIZED, "invalid_credentials"));
    }
    let row = sqlx::query("SELECT id,password_hash FROM td_accounts WHERE login_id=$1")
        .bind(login)
        .fetch_optional(&state.database)
        .await?;
    let hash = row
        .as_ref()
        .map(|r| r.get::<String, _>("password_hash"))
        .unwrap_or_else(|| (*state.dummy_password_hash).clone());
    let permit = state
        .password_tasks
        .clone()
        .acquire_owned()
        .await
        .map_err(|_| bad_request("unavailable"))?;
    let valid = tokio::task::spawn_blocking(move || {
        let _permit = permit;
        PasswordHash::new(&hash).is_ok_and(|h| {
            Argon2::default()
                .verify_password(request.password.as_bytes(), &h)
                .is_ok()
        })
    })
    .await
    .unwrap_or(false);
    if !valid || row.is_none() {
        return Err(ApiError(StatusCode::UNAUTHORIZED, "invalid_credentials"));
    }
    let id: Uuid = row.unwrap().get("id");
    let bearer = format!("{}{}", Uuid::new_v4().simple(), Uuid::new_v4().simple());
    // 계정당 세션은 하나만 유지하며, 재로그인하면 이전 토큰을 폐기한다.
    let mut tx = state.database.begin().await?;
    sqlx::query("SELECT id FROM td_accounts WHERE id=$1 FOR UPDATE")
        .bind(id)
        .fetch_one(&mut *tx)
        .await?;
    sqlx::query("DELETE FROM td_sessions WHERE account_id=$1 OR expires_at<=now()")
        .bind(id)
        .execute(&mut *tx)
        .await?;
    sqlx::query("INSERT INTO td_sessions(token_hash,account_id,expires_at) VALUES($1,$2,now()+interval '24 hours')")
        .bind(hash_text(&bearer))
        .bind(id)
        .execute(&mut *tx)
        .await?;
    tx.commit().await?;
    Ok(LoginResponse {
        account_id: id,
        access_token: bearer,
        token_type: "Bearer",
        expires_in: 86400,
    })
}
pub async fn logout_account(state: &AppState, access_token: &str) -> ApiResult<()> {
    authenticate_session(state, access_token).await?;
    sqlx::query("DELETE FROM td_sessions WHERE token_hash=$1")
        .bind(hash_text(access_token))
        .execute(&state.database)
        .await?;
    Ok(())
}
