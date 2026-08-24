import { DatabaseSync } from 'node:sqlite';
import fs from 'fs';
import path from 'path';
import { config } from '../config/env';

let dbInstance: DatabaseSync | null = null;

const DEFAULT_SCHEMA_SQL = `
CREATE TABLE IF NOT EXISTS users (
  id            TEXT PRIMARY KEY,
  email         TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  display_name  TEXT,
  daily_goal_ml INTEGER DEFAULT 2000,
  created_at    TEXT DEFAULT (datetime('now')),
  updated_at    TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS devices (
  id             TEXT PRIMARY KEY,
  user_id        TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  device_token   TEXT UNIQUE NOT NULL,
  name           TEXT,
  last_seen_at   TEXT,
  created_at     TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS drink_records (
  id             TEXT PRIMARY KEY,
  event_id       TEXT UNIQUE,
  user_id        TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  device_id      TEXT REFERENCES devices(id) ON DELETE SET NULL,
  event_type     TEXT NOT NULL DEFAULT 'drink',
  amount_ml      INTEGER NOT NULL,
  remaining_ml   INTEGER,
  occurred_at    TEXT NOT NULL,
  synced_at      TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_records_user_date ON drink_records(user_id, occurred_at);
CREATE INDEX IF NOT EXISTS idx_records_event_id ON drink_records(event_id);
CREATE INDEX IF NOT EXISTS idx_devices_user_id ON devices(user_id);
CREATE INDEX IF NOT EXISTS idx_devices_token ON devices(device_token);
`;

function loadSchemaSql(): string {
  const possiblePaths = [
    path.resolve(__dirname, 'schema.sql'),
    path.resolve(__dirname, '../../src/database/schema.sql'),
    path.resolve(process.cwd(), 'src/database/schema.sql'),
    path.resolve(process.cwd(), 'backend/src/database/schema.sql'),
  ];

  for (const p of possiblePaths) {
    if (fs.existsSync(p)) {
      return fs.readFileSync(p, 'utf8');
    }
  }

  return DEFAULT_SCHEMA_SQL;
}

export function initDatabase(dbPath?: string): DatabaseSync {
  if (dbInstance) {
    return dbInstance;
  }

  const targetPath = dbPath || config.databasePath;

  if (targetPath !== ':memory:') {
    const dir = path.dirname(path.resolve(targetPath));
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
  }

  const db = new DatabaseSync(targetPath === ':memory:' ? ':memory:' : path.resolve(targetPath));
  db.exec('PRAGMA foreign_keys = ON;');

  if (targetPath !== ':memory:') {
    db.exec('PRAGMA journal_mode = WAL;');
  }

  // Run schema
  const schemaSql = loadSchemaSql();
  db.exec(schemaSql);

  dbInstance = db;
  return dbInstance;
}

export function getDatabase(): DatabaseSync {
  if (!dbInstance) {
    return initDatabase();
  }
  return dbInstance;
}

export function closeDatabase(): void {
  if (dbInstance) {
    dbInstance.close();
    dbInstance = null;
  }
}
