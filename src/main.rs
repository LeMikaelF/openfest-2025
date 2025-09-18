use axum::{Json, Router, extract::Path, response::IntoResponse, routing::get};
use serde::Serialize;

#[tokio::main]
async fn main() {
    let app = Router::new().route("/cities/{city}", get(city_handler));
    let listener = tokio::net::TcpListener::bind("0.0.0.0:3000").await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

#[derive(Serialize)]
struct City {
    name: String,
}

async fn city_handler(Path(city): Path<String>) -> impl IntoResponse {
    Json(City { name: city }).into_response()
}
