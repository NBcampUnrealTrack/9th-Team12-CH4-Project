use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct ReqRegister {
    pub username: String,
    pub password: String,
}

#[derive(Debug, Deserialize)]
pub struct ReqLogin {
    pub username: String,
    pub password: String,
}
