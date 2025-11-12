// api_handler.cpp
#include "api_handler.h"

#include <boost/algorithm/string.hpp>

#include <algorithm>

namespace http_handler {

constexpr char KEY_x[] = "x";
constexpr char KEY_x0[] = "x0";
constexpr char KEY_x1[] = "x1";
constexpr char KEY_y[] = "y";
constexpr char KEY_y0[] = "y0";
constexpr char KEY_y1[] = "y1";
constexpr char KEY_w[] = "w";
constexpr char KEY_h[] = "h";
constexpr char KEY_roads[] = "roads";
constexpr char KEY_buildings[] = "buildings";
constexpr char KEY_lootTypes[] = "lootTypes";
constexpr char KEY_offices[] = "offices";
constexpr char KEY_offset_X[] = "offsetX";
constexpr char KEY_offset_Y[] = "offsetY";

array RoadsToJson(const model::Map& map) {
    array roads_array;

    for (const auto& road : map.GetRoads()) {
        object road_obj;
        model::Point start = road.GetStart();
        model::Point end = road.GetEnd();

        if (road.IsHorizontal()) {
            road_obj[KEY_x0] = start.x;
            road_obj[KEY_y0] = start.y;
            road_obj[KEY_x1] = end.x;
        } else {
            road_obj[KEY_x0] = start.x;
            road_obj[KEY_y0] = start.y;
            road_obj[KEY_y1] = end.y;
        }
        roads_array.emplace_back(road_obj);
    }
    return roads_array;
}

array BuildingsToJson(const model::Map& map) {
    array buildings_array;

    for (const auto& building : map.GetBuildings()) {
        object building_obj;
        model::Rectangle bounds = building.GetBounds();

        building_obj[KEY_x] = bounds.position.x;
        building_obj[KEY_y] = bounds.position.y;
        building_obj[KEY_w] = bounds.size.width;
        building_obj[KEY_h] = bounds.size.height;

        buildings_array.emplace_back(building_obj);
    }
    return buildings_array;
}

array OfficesToJson(const model::Map& map) {
    array offices_array;

    for (const auto& office : map.GetOffices()) {
        object office_obj;
        model::Point position = office.GetPosition();
        model::Offset offset = office.GetOffset();

        office_obj[KEY_id] = *office.GetId();
        office_obj[KEY_x] = position.x;
        office_obj[KEY_y] = position.y;
        office_obj[KEY_offset_X] = offset.dx;
        office_obj[KEY_offset_Y] = offset.dy;

        offices_array.emplace_back(office_obj);
    }
    return offices_array;
}

ApiRequestHandler::ApiRequestHandler(model::Game& game)
    : game_(game) {
}

std::string ApiRequestHandler::MapInfoToJson(const model::Map& map) {
    object map_obj;

    map_obj[KEY_id] = *map.GetId();
    map_obj[KEY_name] = map.GetName();

    // добавляем дороги
    map_obj[KEY_roads] = RoadsToJson(map);

    // добавляем здания
    map_obj[KEY_buildings] = BuildingsToJson(map);

    // добавляем офисы
    map_obj[KEY_offices] = OfficesToJson(map);

    // добавляем лут
    auto map_id = map.GetId();
    map_obj[KEY_lootTypes] = game_.GetLootTypesStorage().GetLootTypes(*map_id);

    return serialize(map_obj);
}

std::string ApiRequestHandler::MapsListToJson() {
    array maps_array;

    for (const auto& map : game_.GetMaps()) {
        object map_obj;

        map_obj[KEY_id] = *map.GetId();
        map_obj[KEY_name] = map.GetName();

        maps_array.emplace_back(map_obj);
    }
    return serialize(maps_array);
}

bool ApiRequestHandler::IsValidHexToken(const std::string token) {
    if (token.size() != 32) {
        return false;
    }

    return std::all_of(token.begin(), token.end(), isxdigit);
}

bool ApiRequestHandler::IsValidAction(const std::string action) {
    if (action == KEY_U || action == KEY_D || action == KEY_L ||
        action == KEY_R || action == "")
    {
        return true;
    }
    return false;
}

} // namespace http_handler