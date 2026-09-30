use argon2::{Argon2, PasswordHasher, password_hash::SaltString};
use rand_core::OsRng;
use sqlx::{PgPool, postgres::PgPoolOptions};
use std::{
    collections::HashMap,
    net::IpAddr,
    sync::Arc,
    time::{Duration, Instant},
};
use tokio::sync::{Mutex, Semaphore};
use uuid::Uuid;
#[derive(Clone)]
pub struct AppState {
    pub(crate) database: PgPool,
    pub(crate) game_server_key: Arc<String>,
    pub(crate) dummy_password_hash: Arc<String>,
    pub(crate) password_tasks: Arc<Semaphore>,
    pub(crate) login_attempts: Arc<Mutex<HashMap<IpAddr, (Instant, u32)>>>,
}

impl AppState {
    pub async fn initialize() -> Result<Self, Box<dyn std::error::Error>> {
        let key = std::env::var("GAME_SERVER_API_KEY")?;
        if key.len() < 32 {
            return Err("GAME_SERVER_API_KEY must contain at least 32 bytes".into());
        }
        let database = PgPoolOptions::new()
            .max_connections(10)
            .acquire_timeout(Duration::from_secs(5))
            .connect(&std::env::var("DATABASE_URL")?)
            .await?;
        sqlx::migrate!("./migrations").run(&database).await?;
        let dummy_password_hash = Argon2::default()
            .hash_password(Uuid::new_v4().as_bytes(), &SaltString::generate(&mut OsRng))
            .map_err(|_| "hash initialization failed")?
            .to_string();
        let state = Self {
            database,
            game_server_key: Arc::new(key),
            dummy_password_hash: Arc::new(dummy_password_hash),
            password_tasks: Arc::new(Semaphore::new(4)),
            login_attempts: Arc::new(Mutex::new(HashMap::new())),
        };

        Ok(state)
    }
}
