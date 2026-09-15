use serde::{Deserialize, Serialize};
use uuid::Uuid;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Credentials {
    pub login_id: String,
    pub password: String,
}

#[derive(Serialize)]
pub struct RegisterResponse {
    pub account_id: Uuid,
    pub login_id: String,
}

#[derive(Serialize)]
pub struct LoginResponse {
    pub account_id: Uuid,
    pub access_token: String,
    pub token_type: &'static str,
    pub expires_in: u64,
}
