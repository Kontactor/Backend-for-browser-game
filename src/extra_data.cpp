// extra_data.cpp
#include "extra_data.h"

namespace extra_data {

void LootTypesStorage::AddLootTypes(const std::string& map_id, array loot_types) {
    loot_types_[map_id] = std::move(loot_types);
}

array LootTypesStorage::GetLootTypes(const std::string& map_id) const {
    if (loot_types_.count(map_id) == 0) {
        return {};
    } else {
        return loot_types_.at(map_id);
    }
}

}  // namespace extra_data