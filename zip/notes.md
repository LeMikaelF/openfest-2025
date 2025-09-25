```
select * from zipfile('zip/aoptalk.zip');
create virtual table archive using zipfile('zip/aoptalk.zip');
update archive set data = edit(data, 'vim') where name = '...';

WITH contents(name, data) AS (
  VALUES('a.txt', 'abc'),
        ('b.txt', '123')
)
SELECT writefile('test.zip', zipfile(name, data)) FROM contents;

.system open test.zip

sqlite3 zip/aoptalk.zip
```
