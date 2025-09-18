use anyhow::Result;
use axum::{
    Json, Router,
    extract::{Path, State},
    http::StatusCode,
    response::{IntoResponse, Response},
    routing::{get, post},
};
use sqlx::{SqlitePool, sqlite::SqlitePoolOptions};
use std::net::SocketAddr;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let app_state = App::new().await?;

    let app = Router::new()
        .route("/cities/{city}", get(App::get_city_handler))
        .route("/cities", post(App::post_city_handler))
        .with_state(app_state);

    let addr: SocketAddr = "0.0.0.0:3000".parse().unwrap();
    axum::serve(tokio::net::TcpListener::bind(addr).await?, app).await?;
    Ok(())
}

#[derive(Clone)]
struct App {
    pool: SqlitePool,
}

impl App {
    async fn new() -> Result<Self> {
        let pool = SqlitePoolOptions::new()
            .max_connections(16)
            .connect(":memory:")
            .await?;

        Self::init_db(&pool).await?;

        Ok(Self { pool })
    }

    async fn init_db(pool: &SqlitePool) -> Result<()> {
        sqlx::query("create table cities(name primary key, comment)")
            .execute(pool)
            .await?;

        Ok(())
    }

    async fn get_city_handler(
        State(app): State<App>,
        Path(city): Path<String>,
    ) -> Result<String, AppError> {
        let (name, comment): (String, String) =
            sqlx::query_as("select name, comment from cities where name = ?1")
                .bind(&city)
                .fetch_one(&app.pool)
                .await?;
        Ok(format!("You wrote this about {name}: {comment}"))
    }
    async fn post_city_handler(
        State(app): State<App>,
        Json(city): Json<CreateCityRequest>,
    ) -> Result<String, AppError> {
        sqlx::query("insert into cities(name, comment) values (?1, ?2)")
            .bind(&city.name)
            .bind(&city.comment)
            .execute(&app.pool)
            .await?;
        Ok("ok".to_owned())
    }
}

#[derive(Debug)]
enum AppError {
    NotFound,
    Db(sqlx::Error),
}

impl From<sqlx::Error> for AppError {
    fn from(e: sqlx::Error) -> Self {
        match e {
            sqlx::Error::RowNotFound => AppError::NotFound,
            other => AppError::Db(other),
        }
    }
}

impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        match self {
            AppError::NotFound => (StatusCode::NOT_FOUND, "not found").into_response(),
            AppError::Db(e) => {
                (StatusCode::INTERNAL_SERVER_ERROR, format!("db error: {e}")).into_response()
            }
        }
    }
}

#[derive(serde::Deserialize)]
struct CreateCityRequest {
    name: String,
    comment: String,
}
