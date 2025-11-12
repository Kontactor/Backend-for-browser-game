// extra_data.h
#pragma once

#include <boost/json.hpp>

#include <string>
#include <unordered_map>

namespace extra_data {

using namespace boost::json;

class LootTypesStorage {
public:
    void AddLootTypes(const std::string& map_id, array loot_types);
    array GetLootTypes(const std::string& map_id) const;

private:
    std::unordered_map<std::string, array> loot_types_;
};

}  // namespace extra_data