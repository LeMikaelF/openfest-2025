```
.session open main mysession
.session list
.session attach mytable
.session changeset mychangeset
```

Demo:
1. create a.db, add users
2. copy a to b
3. start session in b, attach users table
4. modify a
5. write b changelog
6. apply changelog to a, showcasing conflict resolution
