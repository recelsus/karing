#pragma once

#include <json/json.h>

#include "dao/karing_dao.h"

namespace karing::http {

Json::Value record_to_json(const karing::dao::KaringRecord& record);
Json::Value record_to_live_json(const karing::dao::KaringRecord& record);

}
