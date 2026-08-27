use axum::{extract::State, http::StatusCode, Json};
use sqlx::{PgPool, Row};

use crate::structs::auth::{ReqLogin, ReqRegister};
const ACCOUNT_CREDENTIALS_EXIST: &str = r#"
      SELECT (
          FROM User_Auth
          WHERE username = $1
            AND password_hash = $2
      )
  "#;
const INSERT_ACCOUNT: &str = r#"
    INSERT INTO User_Auth (username, password_hash)
    VALUES ($1, $2)
"#;
pub async fn register(
    State(db): State<PgPool>,
    Json(dto): Json<ReqRegister>,
) -> Result<StatusCode, StatusCode> {
    let regex_username = dto.username.trim().to_owned();
    let regex_password: String = dto.password.trim().to_owned();
    // DT 규격
    sqlx::query(INSERT_ACCOUNT)
        .bind(regex_username)
        // .bind(password)
        .bind(regex_password)
        .execute(&db)
        .await
        .map_err(|error| {
            if error
                .as_database_error()
                .and_then(|db_error| db_error.code())
                .as_deref()
                == Some("23505")
            {
                StatusCode::CONFLICT
            } else {
                eprintln!("account insert failed: {error}");
                StatusCode::INTERNAL_SERVER_ERROR
            }
        })?;

    Ok(StatusCode::CREATED)
}

pub async fn login(State(db): State<PgPool>, Json(dto): Json<ReqLogin>) -> StatusCode {
    let regex_username = dto.username.trim().to_owned();
    let regex_password: String = dto.password.trim().to_owned();

    let row = sqlx::query(ACCOUNT_CREDENTIALS_EXIST)
        .bind(regex_username)
        .bind(regex_password)
        .fetch_one(&db)
        .await;

    let exists = match row {
        Ok(row) => row.get::<bool, _>(0),
        Err(error) => {
            eprintln!("login query failed: {error}");
            return StatusCode::INTERNAL_SERVER_ERROR;
        }
    };

    StatusCode::OK // 200 번
}
