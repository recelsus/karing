CREATE TABLE IF NOT EXISTS metadata (
  key TEXT PRIMARY KEY,
  value_text TEXT NOT NULL,
  updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS store_state (
  singleton_id INTEGER PRIMARY KEY CHECK (singleton_id = 1),
  max_items INTEGER NOT NULL,
  next_id INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  CHECK (max_items >= 1),
  CHECK (next_id >= 1)
);

CREATE TABLE IF NOT EXISTS entries (
  id INTEGER PRIMARY KEY,
  used INTEGER NOT NULL DEFAULT 0 CHECK (used IN (0, 1)),
  source_kind TEXT,
  media_kind TEXT,
  content_text TEXT,
  file_path TEXT,
  original_filename TEXT,
  mime_type TEXT,
  size_bytes INTEGER NOT NULL DEFAULT 0 CHECK (size_bytes >= 0),
  stored_at INTEGER,
  updated_at INTEGER
);

CREATE INDEX IF NOT EXISTS idx_entries_used_updated
ON entries(used, updated_at DESC, id DESC);

CREATE INDEX IF NOT EXISTS idx_entries_used_stored
ON entries(used, stored_at DESC, id DESC);

CREATE INDEX IF NOT EXISTS idx_entries_filename
ON entries(original_filename);

CREATE INDEX IF NOT EXISTS idx_entries_mime
ON entries(mime_type);
