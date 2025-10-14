# Demo instructions

## Word list prep

Use the SCOWLv2 project (https://github.com/en-wl/wordlist).

```
make
sqlite3 scowl.db ".mode csv; .once wl.txt; select distinct word, (100 - size) as rank, null, null, null, null from scowl_v0 where size <= 60 and variant_level <= 1 and spelling in ('A','_') and region in ('','US') group by word;"
```

then, move the `wl.txt` file to where you need it.

## Demo

In the sqlite console:

```
.load spellfix1
create virtual table spell using spellfix1
.import --csv wl.txt spell
.mode box
select * from spell where word match 'mikael';
```

## To compile the spellfix1 extension (Apple Silicon)

In the SQLite source tree:

```
export MACOSX_DEPLOYMENT_TARGET=10.13
export LDFLAGS="-Wl,-macosx_version_min,10.13"
export CFLAGS="-mmacosx-version-min=10.13 -arch arm64 -Os -DQSLITE_DQS=0"
./configure --enable-threadsafe
cc -O2 -fPIC -I. -dynamiclib -arch arm64 -o spellfix1.dylib ext/misc/spellfix.c
```

then, move the `spellfix1.dylib` to where you need it.
