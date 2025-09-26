#pragma once
#include <drogon/orm/DbClient.h>

namespace karing::db::schema {

using drogon::orm::DbClientPtr;

void apply_pragmas(const DbClientPtr& client);
void create_config_table(const DbClientPtr& client);
void create_karing_table_and_indexes(const DbClientPtr& client);
void create_fts_objects_if_available(const DbClientPtr& client);
void create_auth_tables(const DbClientPtr& client);
void create_overwrite_log_table(const DbClientPtr& client);

}

