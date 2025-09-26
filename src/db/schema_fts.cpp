#include <drogon/orm/Exception.h>
#include "schemas.h"

namespace karing::db::schema {

void create_fts_objects_if_available(const drogon::orm::DbClientPtr& client) {
  try {
    client->execSqlSync(
        "CREATE VIRTUAL TABLE IF NOT EXISTS karing_fts\n"
        "USING fts5(content, content='karing', content_rowid='id');");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_ai\n"
        "AFTER INSERT ON karing\n"
        "WHEN NEW.is_active = 1 AND NEW.is_file = 0\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(rowid, content) VALUES (NEW.id, NEW.content);\n"
        "END;\n");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_au\n"
        "AFTER UPDATE OF content, is_active, is_file ON karing\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(karing_fts, rowid, content) VALUES ('delete', OLD.id, OLD.content);\n"
        "  INSERT INTO karing_fts(rowid, content)\n"
        "  SELECT NEW.id, NEW.content WHERE NEW.is_active = 1 AND NEW.is_file = 0;\n"
        "END;\n");

    client->execSqlSync(
        "CREATE TRIGGER IF NOT EXISTS karing_ad\n"
        "AFTER DELETE ON karing\n"
        "BEGIN\n"
        "  INSERT INTO karing_fts(karing_fts, rowid, content) VALUES ('delete', OLD.id, OLD.content);\n"
        "END;\n");
  } catch (const drogon::orm::DrogonDbException&) {
    // FTS5 not available; continue without virtual table
  } catch (...) {
    // ignore
  }
}

}
