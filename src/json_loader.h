// json_loader.h
#pragma once

#include "model.h"
#include "loot_generator.h"

#include <boost/json.hpp>
#include <filesystem>

namespace json_loader {

using namespace boost::json;

model::Game LoadGame(const std::filesystem::path& json_path);

void AddLootGeneratorConfig(model::Game& game, const value& json_document);

void AddBuildings(model::Map& new_map, const array arr_building);

void AddOffices(model::Map& new_map, const array arr_office);

void AddRoads(model::Map& new_map, const array arr_road);

void AddLootValue(model::Map& new_map, const array arr_loot);

}  // namespace json_loader