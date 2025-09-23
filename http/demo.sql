WITH
  params AS (
    SELECT
      'kubernetes' AS owner,
      'kubernetes' AS repo,
      '2025-06-01T00:00:00Z' AS since_iso,  -- only fetch recent activity
      25          AS max_pages,              -- safety cap
      100         AS page_size,
      :token      AS token                   -- pass a PAT for 5k req/hr
  ),
  base AS (
    SELECT
      'https://api.github.com/repos/'||owner||'/'||repo||
      '/issues?state=closed&per_page='||page_size||
      '&since='||since_iso||
      '&page=' AS url_prefix,
      http_headers(
        'Accept','application/vnd.github+json',
        'X-GitHub-Api-Version','2022-11-28',
        'Authorization','Bearer '||token
      ) AS hdrs,
      max_pages
    FROM params
  ),
  pages(page, url, hdrs, max_pages) AS (
    SELECT 1, url_prefix||'1', hdrs, max_pages FROM base
    UNION ALL
    SELECT page+1, replace(url, 'page='||page, 'page='||(page+1)), hdrs, max_pages
    FROM pages
    WHERE page < max_pages
  ),
  raw AS (
    SELECT page, http_get_body(url, hdrs) AS body
    FROM pages
  ),
  items AS (
    SELECT page, value AS item
    FROM raw, json_each(body)
    WHERE json_array_length(body) > 0
  ),
  prs AS (
    SELECT
      json_extract(item,'$.id')                 AS issue_id,
      json_extract(item,'$.user.login')         AS author,
      json_extract(item,'$.created_at')         AS created_at,
      json_extract(item,'$.closed_at')          AS closed_at
    FROM items
    -- "Issues" API returns both issues and PRs; PRs have a "pull_request" key
    WHERE json_type(item,'$.pull_request')='object'
      AND closed_at IS NOT NULL
  ),
  durations AS (
    SELECT
      author,
      created_at,
      closed_at,
      (julianday(closed_at) - julianday(created_at)) * 24.0 AS hours_to_close
    FROM prs
  ),
  per_author AS (
    SELECT
      author,
      count(*)                         AS prs_closed,
      avg(hours_to_close)              AS avg_hours_to_close
    FROM durations
    GROUP BY author
  ),
  medians AS (
    SELECT author, avg(hours_to_close) AS median_hours_to_close
    FROM (
      SELECT
        author, hours_to_close,
        row_number() OVER (
          PARTITION BY author ORDER BY hours_to_close
        ) AS rn,
        count(*) OVER (PARTITION BY author) AS cnt
      FROM durations
    )
    WHERE rn IN ((cnt+1)/2, (cnt+2)/2)       -- median for odd/even
    GROUP BY author
  ),
  ranked AS (
    SELECT
      a.author,
      a.prs_closed,
      a.avg_hours_to_close,
      m.median_hours_to_close,
      sum(a.prs_closed) OVER () AS total_prs,
      sum(a.prs_closed) OVER (
        ORDER BY a.prs_closed DESC
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
      ) AS cum_prs
    FROM per_author a
    JOIN medians m USING(author)
  )
SELECT
  author,
  prs_closed,
  round(avg_hours_to_close, 1)    AS avg_h_to_close,
  round(median_hours_to_close, 1) AS p50_h_to_close,
  round(100.0 * cum_prs / total_prs, 1) AS cum_share_pct
FROM ranked
ORDER BY prs_closed DESC
LIMIT 25;
