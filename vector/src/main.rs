use anyhow::Result;
use reqwest::blocking::Client;
use rusqlite::{Connection, ffi::sqlite3_auto_extension, params};
use serde::{Deserialize, Serialize};
use sqlite_vec::sqlite3_vec_init;
use std::time::Duration;

fn autoload_sqlite_vec() {
    unsafe {
        #[expect(clippy::missing_transmute_annotations)]
        sqlite3_auto_extension(Some(std::mem::transmute(sqlite3_vec_init as *const ())));
    }
}

#[derive(Serialize)]
struct OllamaEmbedReq<'a> {
    model: &'a str,
    prompt: &'a str,
}

#[derive(Deserialize)]
struct OllamaEmbedResp {
    embedding: Vec<f32>,
}

const MODEL: &str = "all-minilm";
const OLLAMA_URL: &str = "http://localhost:11434";

const TITLES: [&str; 12] = [
    "React state management patterns for shared components",
    "Using CSS Grid for responsive dashboard layouts",
    "Training a transformer on small datasets with regularization",
    "Vector embeddings enable semantic search over documents",
    "Kafka partitions and consumer groups for scalable throughput",
    "Optimizing Spark DataFrames with predicate pushdown",
    "Designing idempotent Airflow DAGs for reliability",
    "Docker multi-stage builds to keep images small",
    "Kubernetes Ingress vs Service: routing trade-offs",
    "Time-series downsampling with Rust and Polars",
    "Postgres: B-tree vs GIN indexes for text search",
    "RAG: chunking, embeddings, and prompt assembly",
];

fn embed_text(client: &Client, base: &str, text: &str) -> Result<Vec<f32>> {
    let url = format!("{base}/api/embeddings");
    let req = OllamaEmbedReq {
        model: MODEL,
        prompt: text,
    };
    let resp = client.post(&url).json(&req).send()?.error_for_status()?;
    let body: OllamaEmbedResp = resp.json()?;
    Ok(body.embedding)
}

fn main() -> Result<()> {
    autoload_sqlite_vec();

    let http = Client::builder().timeout(Duration::from_secs(60)).build()?;

    let db = Connection::open_in_memory()?;
    // see https://alexgarcia.xyz/sqlite-vec/features/vec0.html
    db.execute_batch(
        r"
        create virtual table vec_demo using vec0(
          embedding float[384],
          +title    text
        );
        ",
    )?;

    for s in TITLES.iter() {
        let embedding = embed_text(&http, OLLAMA_URL, s)?;
        anyhow::ensure!(
            embedding.len() == 384,
            "embedding dimension changed: {}",
            embedding.len()
        );
        db.execute(
            r"
            insert into vec_demo(embedding, title)
            values (vec_f32(?1), ?2)
            ",
            params![serde_json::to_string(&embedding)?, s],
        )?;
    }

    let query = "databases"; // This is out query

    // ---- 3) run KNN (top-K) ----
    println!("🧪 query: {query:?}   model: {MODEL}\n");
    let embedding = embed_text(&http, OLLAMA_URL, query)?;
    let formatted_vector = serde_json::to_string(&embedding)?;

    println!("=== Top-K (KNN) ===");
    let mut knn = db.prepare(
        "select title, distance
           from vec_demo
           where embedding match vec_f32(?1) and k = 8
           order by distance",
    )?;
    let mut rows = knn.query([&formatted_vector])?;
    let mut rank = 1;
    while let Some(r) = rows.next()? {
        let title: String = r.get(0)?;
        let dist: f64 = r.get(1)?;
        println!("{rank:>2}. d={dist:.3}  {title}");
        rank += 1;
    }

    Ok(())
}
