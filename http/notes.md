```
select http_get_body('https://api.github.com/repos/sqlite/sqlite');
select http_get_body('https://api.github.com/repos/sqlite/sqlite') ->> '$.description' as description;

.mode json
.once output.json
.read http/demo.sql
```
