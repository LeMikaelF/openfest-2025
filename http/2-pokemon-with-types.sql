SELECT name, group_concat(type) as type
FROM (
  WITH RECURSIVE
  constants(start_url, hdr) AS (
    SELECT
      'https://pokeapi.co/api/v2/pokemon?limit=5',
      http_headers('User-Agent', 'sqlite-http demo (+you@example.com)')
  ),
  pages(url, body) AS (
    SELECT start_url, http_get_body(start_url, hdr) FROM constants
    UNION ALL
    SELECT
      json_extract(p.body, '$.next'),
      http_get_body(json_extract(p.body, '$.next'), (SELECT hdr FROM constants))
    FROM pages AS p
    WHERE json_type(p.body, '$.next') = 'text'
  ),
  rows AS (
    SELECT j.value
    FROM pages p
    JOIN json_each(p.body, '$.results') AS j
  )
  SELECT
    json_extract(r.value, '$.name') AS name,
    json_extract(t.value, '$.type.name') AS type
  FROM rows AS r
  JOIN json_each(
    http_get_body(json_extract(r.value, '$.url'), (SELECT hdr FROM constants)),
    '$.types'
  ) AS t
  LIMIT 10
)
GROUP BY name;


