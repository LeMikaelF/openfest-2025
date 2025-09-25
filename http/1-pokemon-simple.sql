WITH RECURSIVE
constants(start_url) AS (
  SELECT 'https://pokeapi.co/api/v2/pokemon?limit=5'
),
pages(url, body) AS (
  SELECT start_url, http_get_body(start_url)
  FROM constants
  UNION ALL
  SELECT json_extract(body, '$.next'),
         http_get_body(json_extract(body, '$.next'))
  FROM pages
  WHERE json_extract(body, '$.next') IS NOT NULL
),
all_rows AS (
  SELECT j.value
  FROM pages p, json_each(p.body, '$.results') AS j
)
SELECT
  json_extract(value, '$.name') AS name,
  json_extract(value, '$.url') AS url
FROM all_rows
LIMIT 1;

