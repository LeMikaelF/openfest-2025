#!/usr/bin/env node

import { rmSync } from 'node:fs';
import Database from 'better-sqlite3';

const DB = process.env.SQLITE_DEMO_DB || 'demo.db';
const db = new Database(DB);
db.exec(`
  CREATE TABLE IF NOT EXISTS todo(
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    done INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
  )
`);

const [, , cmd, ...args] = process.argv;
const id = x => { const n = +x; if (!Number.isInteger(n)) die('numeric id'); return n; };
const die = msg => (console.error(msg), process.exit(1));

({
  add() {
    const title = args.join(' ').trim();
    if (!title) die('todo add "text"');
    const r = db.prepare('INSERT INTO todo(title) VALUES(?)').run(title);
    console.log(`#${r.lastInsertRowid} ${title}`);
  },
  ls() {
    const rows = db.prepare('SELECT id, title, done FROM todo ORDER BY id').all();
    rows.length ? console.table(rows) : console.log('empty');
  },
  done() {
    const info = db.prepare('UPDATE todo SET done=1 WHERE id=?').run(id(args[0]));
    console.log(info.changes ? 'ok' : 'not found');
  },
  undo() {
    const info = db.prepare('UPDATE todo SET done=0 WHERE id=?').run(id(args[0]));
    console.log(info.changes ? 'ok' : 'not found');
  },
  rm() {
    const info = db.prepare('DELETE FROM todo WHERE id=?').run(id(args[0]));
    console.log(info.changes ? 'ok' : 'not found');
  },
  reset() {
    db.close();
    rmSync(DB, { force: true });
    console.log('reset');
  }
}[cmd] || (() => console.log(`usage:
  todo add "text"
  todo ls
  todo done <id>
  todo undo <id>
  todo rm <id>
  todo reset
`)))();

