```
select http_get_body('https://api.github.com/repos/sqlite/sqlite') ->> '$.description' as description;

select name, value from http_headers_each(http_get_headers('https://api.census.gov/data/'));

.mode json
.once output.json
.read http/demo.sql
```
