CREATE VIRTUAL TABLE IF NOT EXISTS entries_fts
USING fts5(
  content_text,
  original_filename,
  content='entries',
  content_rowid='id'
);

CREATE TRIGGER IF NOT EXISTS entries_ai
AFTER INSERT ON entries
BEGIN
  INSERT INTO entries_fts(rowid, content_text, original_filename)
  SELECT NEW.id, NEW.content_text, NEW.original_filename
  WHERE NEW.used = 1;
END;

CREATE TRIGGER IF NOT EXISTS entries_au
AFTER UPDATE OF used, content_text, original_filename ON entries
BEGIN
  INSERT INTO entries_fts(entries_fts, rowid) VALUES ('delete', OLD.id);
  INSERT INTO entries_fts(rowid, content_text, original_filename)
  SELECT NEW.id, NEW.content_text, NEW.original_filename
  WHERE NEW.used = 1;
END;

CREATE TRIGGER IF NOT EXISTS entries_ad
AFTER DELETE ON entries
BEGIN
  INSERT INTO entries_fts(entries_fts, rowid) VALUES ('delete', OLD.id);
END;
