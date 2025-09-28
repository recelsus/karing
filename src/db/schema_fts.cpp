#include <drogon/orm/Exception.h>
#include "schemas.h"

namespace karing::db::schema {

void create_fts_objects_if_available(const drogon::orm::DbClientPtr& client) {
  try {
    client->execSqlSync(
        "CREATE VIRTUAL TABLE IF NOT EXISTS karing_fts\n"
        "USING fts5(content, filename, content='karing', content_rowid='id');");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_ai_text\n"
        "AFTER INSERT ON karing\n"
        "WHEN NEW.is_active = 1 AND NEW.is_file = 0\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(rowid, content, filename) VALUES (NEW.id, NEW.content, NULL);\n"
        "END;\n");
    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_ai_file\n"
        "AFTER INSERT ON karing\n"
        "WHEN NEW.is_active = 1 AND NEW.is_file = 1\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(rowid, content, filename) VALUES (NEW.id, NULL, NEW.filename);\n"
        "END;\n");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_au\n"
        "AFTER UPDATE OF content, filename, is_active, is_file ON karing\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(karing_fts, rowid) VALUES ('delete', OLD.id);\n"
        "  INSERT INTO karing_fts(rowid, content, filename) SELECT NEW.id, NEW.content, NULL WHERE NEW.is_active = 1 AND NEW.is_file = 0;\n"
        "  INSERT INTO karing_fts(rowid, content, filename) SELECT NEW.id, NULL, NEW.filename WHERE NEW.is_active = 1 AND NEW.is_file = 1;\n"
        "END;\n");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_ad\n"
        "AFTER DELETE ON karing\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(karing_fts, rowid) VALUES ('delete', OLD.id);\n"
        "END;\n");
  } catch (const drogon::orm::DrogonDbException&) {
    // FTS5 not available; continue without virtual table
  } catch (...) {
    // ignore
  }
}

}
