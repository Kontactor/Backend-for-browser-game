// json_loader.cpp
#include "json_loader.h"

#include <fstream>
#include <stdexcept>

namespace json_loader {

constexpr char KEY_x[] = "x";
constexpr char KEY_x0[] = "x0";
constexpr char KEY_x1[] = "x1";
constexpr char KEY_y[] = "y";
constexpr char KEY_y0[] = "y0";
constexpr char KEY_y1[] = "y1";
constexpr char KEY_w[] = "w";
constexpr char KEY_h[] = "h";
constexpr char KEY_bagCapacity[] = "bagCapacity";
constexpr char KEY_buildings[] = "buildings";
constexpr char KEY_defaultBagCapacity[] = "defaultBagCapacity";
constexpr char KEY_defaultDogSpeed[] = "defaultDogSpeed";
constexpr char KEY_dogRetirementTime[] = "dogRetirementTime";
constexpr char KEY_dogSpeed[] = "dogSpeed";
constexpr char KEY_id[] = "id";
constexpr char KEY_lootGeneratorConfig[] = "lootGeneratorConfig";
constexpr char KEY_lootTypes[] = "lootTypes";
constexpr char KEY_maps[] = "maps";
constexpr char KEY_name[] = "name";
constexpr char KEY_offices[] = "offices";
constexpr char KEY_offset_X[] = "offsetX";
constexpr char KEY_offset_Y[] = "offsetY";
constexpr char KEY_period[] = "period";
constexpr char KEY_probability[] = "probability";        
constexpr char KEY_roads[] = "roads";
constexpr char KEY_value[] = "value";

using namespace boost::json;
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
    if (!std::filesystem::exists(json_path)) {
        throw std::runtime_error("File not found"s);
    }

    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file"s);
    }

    try {
        // Читаем файл в строковую переменную
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        value json_document = parse(content);
        array arr_maps = json_document.at(KEY_maps).as_array();

        model::Game game;
        double default_dog_speed = model::DEFAULT_DOG_SPEED;
        int default_bag_capacity = model::DEFAULT_BAG_CAPACITY;

        if (json_document.as_object().contains(KEY_defaultDogSpeed)) {
            default_dog_speed =
                json_document.at(KEY_defaultDogSpeed).as_double();
        }

        if (json_document.as_object().contains(KEY_dogRetirementTime)) {
            game.SetDogRetirementTime(
                json_document.at(KEY_dogRetirementTime).as_double());
        }

        if (json_document.as_object().contains(KEY_defaultBagCapacity)) {
            default_bag_capacity =
                json_document.at(KEY_defaultBagCapacity).as_int64();
        }

        AddLootGeneratorConfig(game, json_document);

        auto loot_types_storage_ptr =
            std::make_unique<extra_data::LootTypesStorage>();
        game.SetLootTypesStorage(std::move(loot_types_storage_ptr));

        for (auto& map_obj : arr_maps) {
            object obj_map = map_obj.as_object();

            double dog_speed = default_dog_speed;
            int bag_capacity = default_bag_capacity;

            if (obj_map.contains(KEY_dogSpeed)) {
                dog_speed = obj_map.at(KEY_dogSpeed).as_double();
            }

            if (obj_map.contains(KEY_bagCapacity)) {
                bag_capacity = obj_map.at(KEY_bagCapacity).as_int64();
            }

            model::Map::Id id(obj_map[KEY_id].as_string().c_str());
            model::Map new_map(id, obj_map[KEY_name].as_string().c_str());

            new_map.SetDogSpeed(dog_speed);
            new_map.SetBagCapacity(bag_capacity);

            // Добавляем дороги
            AddRoads(new_map, obj_map[KEY_roads].as_array());
            
            // Добавляем здания
            AddBuildings(new_map, obj_map[KEY_buildings].as_array());
            
            // Добавляем офисы
            AddOffices(new_map, obj_map[KEY_offices].as_array());
            
            // Добавляем типы лута
            game.GetLootTypesStorage().
                AddLootTypes(*id, obj_map[KEY_lootTypes].as_array());
            new_map.SetLootTypesCount(obj_map[KEY_lootTypes].as_array().size());

            AddLootValue(new_map, obj_map[KEY_lootTypes].as_array());

            game.AddMap(new_map);
        }

        return std::move(game);
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("JSON parsing error: "s) +
                                ex.what());
    }
}

void AddLootGeneratorConfig(model::Game& game, const value& json_document) {
    object obj_loot_generator =
        json_document.at(KEY_lootGeneratorConfig).as_object();
    double period_double = obj_loot_generator.at(KEY_period).as_double();
    auto period = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::duration<double>(period_double)
    );

    double probability = obj_loot_generator.at(KEY_probability).as_double();

    auto loot_generator_ptr =
        std::make_unique<loot_gen::LootGenerator>(period, probability);

    game.SetLootGenerator(std::move(loot_generator_ptr));
}

void AddRoads(model::Map& new_map, const array arr_road) {
    for (auto& road : arr_road) {
        object obj_road = road.as_object();

        if (obj_road.contains(KEY_x1)) {
            model::Road r({model::Road::HORIZONTAL, 
                            {static_cast<int>(obj_road[KEY_x0].as_int64()),
                            static_cast<int>(obj_road[KEY_y0].as_int64())},
                            static_cast<int>(obj_road[KEY_x1].as_int64())});
            new_map.AddRoad(r);
        } else {
            model::Road r({model::Road::VERTICAL, 
                            {static_cast<int>(obj_road[KEY_x0].as_int64()),
                            static_cast<int>(obj_road[KEY_y0].as_int64())},
                            static_cast<int>(obj_road[KEY_y1].as_int64())});
            new_map.AddRoad(r);
        }
    }
}

void AddBuildings(model::Map& new_map, const array arr_building) {
    for (auto& building : arr_building) {
        object obj_bldg = building.as_object();

        model::Point point(
            obj_bldg[KEY_x].as_int64(),
            obj_bldg[KEY_y].as_int64());
        model::Size size(obj_bldg[KEY_w].as_int64(), obj_bldg[KEY_h].as_int64());
        model::Building b({point, size});

        new_map.AddBuilding(b);
    }
}

void AddOffices(model::Map& new_map, const array arr_office) {
    for (auto& office : arr_office) {
        object ofc_obj = office.as_object();

        model::Point point(ofc_obj[KEY_x].as_int64(), ofc_obj[KEY_y].as_int64());
        model::Offset offset(ofc_obj[KEY_offset_X].as_int64(),
                             ofc_obj[KEY_offset_Y].as_int64());
        model::Office::Id id(ofc_obj[KEY_id].as_string().c_str());

        model::Office o(id, point, offset);

        new_map.AddOffice(o);
    }
}

void AddLootValue(model::Map& new_map, const array arr_loot) {
    for (auto& loot : arr_loot) {
        object obj_loot = loot.as_object();

        new_map.AddLootValue(obj_loot[KEY_value].as_int64());
    }
}

}  // namespace json_loader