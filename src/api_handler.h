// api_handler.h
#pragma once

#include "application.h"
#include "database.h"
#include "handlers_utils.h"
#include "model.h"

#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace http_handler {

constexpr char API_MAPS_PATH[] = "/api/v1/maps";
constexpr char API_GAME_ACTION_PATH[] = "/api/v1/game/player/action";
constexpr char API_GAME_JOIN_PATH[] = "/api/v1/game/join";
constexpr char API_GAME_PATH[] = "/api/v1/game";
constexpr char API_GAME_PLAYERS_PATH[] = "/api/v1/game/players";
constexpr char API_GAME_RECORDS_PATH[] = "/api/v1/game/records";
constexpr char API_GAME_STATE_PATH[] = "/api/v1/game/state";
constexpr char API_GAME_TICK_PATH[] = "/api/v1/game/tick";
constexpr char KEY_D[] = "D";
constexpr char KEY_L[] = "L";
constexpr char KEY_R[] = "R";
constexpr char KEY_U[] = "U";
constexpr char KEY_authorization[] = "authorization";
constexpr char KEY_authToken[] = "authToken";
constexpr char KEY_bag[] = "bag";
constexpr char KEY_dir[] = "dir";
constexpr char KEY_id[] = "id";
constexpr char KEY_lostObjects[] = "lostObjects";
constexpr char KEY_mapId[] = "mapId";
constexpr char KEY_maxItems[] = "maxItems";
constexpr char KEY_move[] = "move";
constexpr char KEY_name[] = "name";
constexpr char KEY_playerId[] = "playerId";
constexpr char KEY_players[] = "players";
constexpr char KEY_pos[] = "pos";
constexpr char KEY_score[] = "score";
constexpr char KEY_speed[] = "speed";
constexpr char KEY_start[] = "start";
constexpr char KEY_timeDelta[] = "timeDelta";
constexpr char KEY_type[] = "type";
constexpr char KEY_userName[] = "userName";

static const int DEFAULT_START_IN_RESULT = 0;
static const int DEFAULT_ROWS_NUMBER_IN_RESULT = 100;
static const int MAX_ROWS_NUMBER_IN_RESULT = 100;
constexpr double MsInSecond = 1000.0;

using namespace boost::json;
using namespace std::literals;

namespace beast = boost::beast;
namespace http = beast::http;

class ApiRequestHandler {
public:
    explicit ApiRequestHandler(model::Game& game);

    template <typename Body, typename Allocator>
    ServerResponse Handle(
        http::request<Body, http::basic_fields<Allocator>>&& req);

private:
    std::string MapInfoToJson(const model::Map& map);
    std::string MapsListToJson();

    bool IsValidHexToken(const std::string token);
    bool IsValidAction(const std::string action);

    template <typename Body, typename Allocator, typename Fn>
    ServerResponse ExecuteAuthorized(
        http::request<Body, http::basic_fields<Allocator>>&& req, Fn&& action);

    template <typename Body, typename Allocator>
    std::optional<app::Token> TryExtractToken(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse BadRequestResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse GetHeadOnlyResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse GetPlayersResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse GetRecordsResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse GetStateResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse InvalidPlayerNameResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse InvalidTokenResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse JoinGameResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse JoinRequestParseErrorResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse MapInfoResponse (
        http::request<Body, http::basic_fields<Allocator>>&& req, 
        const model::Map::Id map_id);

    template <typename Body, typename Allocator>
    ServerResponse MapNotFoundResponse (
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse PlayerActionResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse PostOnlyResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse TickResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse TickRequestParseErrorResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    template <typename Body, typename Allocator>
    ServerResponse UnknownTokenResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    model::Game& game_;
};

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::Handle(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    ServerResponse resp;
    std::string full_target = UrlDecode(std::string(req.target()));

    size_t query_pos = full_target.find('?');
    std::string target = full_target.substr(0, query_pos);

    if (target.starts_with(API_MAPS_PATH)) {
        if (req.method() == http::verb::get ||
            req.method() == http::verb::head)
        {
            if (target == API_MAPS_PATH) {
                resp = std::move(MakeStringResponse(
                    http::status::ok,
                    MapsListToJson(),
                    req.version(),
                    req.keep_alive(),
                    ContentType::APP_JSON
                ));
            } else if (target.starts_with(API_MAPS_PATH + "/"s)) {
                target.erase(0, std::string(API_MAPS_PATH + "/"s).size());
                model::Map::Id map_id(target);
                resp = std::move(MapInfoResponse(
                    std::forward<decltype(req)>(req), map_id));
            } else {
                resp = std::move(BadRequestResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else {
            resp = std::move(GetHeadOnlyResponse(
                std::forward<decltype(req)>(req)));
        }
    } else if (target.starts_with(API_GAME_PATH)) {
        if (target == API_GAME_ACTION_PATH) {
            if (req.method() == http::verb::post)
            {
                resp = std::move(PlayerActionResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(PostOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else if (target == API_GAME_JOIN_PATH) {
            if (req.method() == http::verb::post) {
                resp = std::move(JoinGameResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(PostOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else if (target == API_GAME_PLAYERS_PATH) {
            if (req.method() == http::verb::get ||
                req.method() == http::verb::head)
            {
                resp = std::move(GetPlayersResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(GetHeadOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else if (target == API_GAME_RECORDS_PATH) {
            if (req.method() == http::verb::get ||
                req.method() == http::verb::head)
            {
                resp = std::move(GetRecordsResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(GetHeadOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else if (target == API_GAME_STATE_PATH) {
            if (req.method() == http::verb::get ||
                req.method() == http::verb::head)
            {
                resp = std::move(GetStateResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(GetHeadOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        
        } else if (target == API_GAME_TICK_PATH &&
                    game_.GetGameMode() == model::Game::GAME_MODE::TEST) {
            if (req.method() == http::verb::post)
            {
                resp = std::move(TickResponse(
                    std::forward<decltype(req)>(req)));
            } else {
                resp = std::move(PostOnlyResponse(
                    std::forward<decltype(req)>(req)));
            }
        } else {
            resp = std::move(BadRequestResponse(
                std::forward<decltype(req)>(req)));
        }
    } else {
        resp = std::move(BadRequestResponse(
            std::forward<decltype(req)>(req)));
    }
    return resp;
}

template <typename Body, typename Allocator, typename Fn>
ServerResponse ApiRequestHandler::ExecuteAuthorized(
    http::request<Body, http::basic_fields<Allocator>>&& req, Fn&& action)
{
    auto token = TryExtractToken(std::forward<decltype(req)>(req));

    if (!token) {
        return std::move(InvalidTokenResponse(std::forward<decltype(req)>(req)));
    }
    if (!app::Players::FindPlayerByToken(*token)) {
        return std::move(UnknownTokenResponse(std::forward<decltype(req)>(req)));
    }
    return action(*token);
}

template <typename Body, typename Allocator>
std::optional<app::Token> ApiRequestHandler::TryExtractToken(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    auto it = req.find(KEY_authorization);

    std::string auth_value(it->value().data(), it->value().size());

    std::istringstream iss(auth_value);
    std::string bearer_type, token_str;
    iss >> bearer_type >> token_str;

    if (it == req.end() || !IsValidHexToken(token_str)) {
        return std::nullopt;
    }

    return app::Token(token_str);
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::BadRequestResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::bad_request,
        R"({"code": "badRequest","message": "Bad request"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::GetHeadOnlyResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::method_not_allowed,
        R"({"code": "invalidMethod","message": "Only GET & HEAD method is expected"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"},
        {http::field::allow, "GET, HEAD"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::GetPlayersResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return ExecuteAuthorized(std::forward<decltype(req)>(req),
        [&req] (const app::Token& token)
    {
        const auto& players = app::Players::FindPlayersInSession(token);
        object body;

        for (auto& player : players) {
            value name{{KEY_name, player->GetName()}};
            body.emplace(std::to_string(player->GetId()), name);
        }

        return std::move(MakeStringResponse(
            http::status::ok,
            serialize(body),
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON,
            {{http::field::cache_control, "no-cache"}}));
    });
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::GetRecordsResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    std::string target = std::string(req.target());
    size_t query_start = target.find('?');
    
    int start = DEFAULT_START_IN_RESULT;
    int max_items = DEFAULT_ROWS_NUMBER_IN_RESULT;
    
    if (query_start != std::string::npos) {
        std::string query = target.substr(query_start + 1);
        std::istringstream iss(query);
        std::string param;
        
        while (std::getline(iss, param, '&')) {
            size_t eq_pos = param.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = param.substr(0, eq_pos);
                std::string value = param.substr(eq_pos + 1);
                
                if (key == "start") {
                    try {
                        start = std::stoi(value);
                        if (start < 0) start = 0;
                    } catch (...) {
                        value custom_data{
                            {"parameter"s, key},
                            {"value", value}, {"target",
                            target}};
                        BOOST_LOG_TRIVIAL(warning)
                            << logging::add_value(
                                my_logger::additional_data,
                                custom_data)
                            << "failed to parse start parameter"sv;
                    }
                } else if (key == "maxItems") {
                    try {
                        max_items = std::stoi(value);
                        if (max_items < 0) max_items = 0;
                        if (max_items > MAX_ROWS_NUMBER_IN_RESULT) {
                            return std::move(MakeStringResponse(
                                http::status::bad_request,
                                R"({"code": "invalidArgument","message": "maxItems cannot exceed 100"})",
                                req.version(),
                                req.keep_alive(),
                                ContentType::APP_JSON,
                                {{http::field::cache_control, "no-cache"}}));
                        }
                    } catch (...) {
                        value custom_data{
                            {"parameter"s, key},
                            {"value", value},
                            {"target", target}};
                        BOOST_LOG_TRIVIAL(warning)
                            << logging::add_value(
                                my_logger::additional_data,
                                custom_data)
                            << "failed to parse maxItems parameter"sv;
                    }
                }
            }
        }
    }

    try {
        auto pool = game_.GetDBConnectionPool();
        auto records = database::Database::GetPlayersRecords(
            pool, start, max_items);

        array records_array;

        for (auto& [id, name, score, play_time ] : records) {
            object record_obj;
            record_obj["name"] = name;
            record_obj["score"] = score;
            record_obj["playTime"] = play_time / MsInSecond;
            records_array.push_back(record_obj);
        }

        return std::move(MakeStringResponse(
            http::status::ok,
            serialize(records_array),
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON,
            {{http::field::cache_control, "no-cache"}}));
            
    } catch (const std::exception& e) {
        return std::move(MakeStringResponse(
            http::status::internal_server_error,
            R"({"code": "internalError","message": "Failed to retrieve records"})",
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON,
            {{http::field::cache_control, "no-cache"}}));
    }
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::GetStateResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return ExecuteAuthorized(std::forward<decltype(req)>(req),
        [&req] (const app::Token& token)
    {
        const auto& players = app::Players::FindPlayersInSession(token);
        object players_obj_body;

        for (auto& player : players) {
            std::shared_ptr<model::Dog> dog_ptr = player->GetDog();
            geom::Point2D position = dog_ptr->GetPosition();
            geom::Vec2D speed = dog_ptr->GetSpeed();

            object player_stats;

            player_stats[KEY_pos] = {position.x, position.y};
            player_stats[KEY_speed] = {speed.x, speed.y};

            switch (dog_ptr->GetDirection()) {
                case model::DIRECTION::NORTH:
                    player_stats[KEY_dir] = KEY_U;
                    break;
                case model::DIRECTION::SOUTH:
                    player_stats[KEY_dir] = KEY_D;
                    break;
                case model::DIRECTION::WEST:
                    player_stats[KEY_dir] = KEY_L;
                    break;
                case model::DIRECTION::EAST:
                    player_stats[KEY_dir] = KEY_R;
                    break;
                case model::DIRECTION::NONE:
                    player_stats[KEY_dir] = "";
                    break;
            }

            auto loot = dog_ptr->GetLoot();
            array bag_array;
            for (auto& loot_item : loot) {
                object loot_stats;
                loot_stats[KEY_id] = loot_item->GetId();
                loot_stats[KEY_type] = loot_item->GetType();
                bag_array.push_back(loot_stats);
            }
            player_stats[KEY_bag] = bag_array;
            player_stats[KEY_score] = dog_ptr->GetScore();

            players_obj_body[std::to_string(player->GetId())] = player_stats;
        }

        const auto& loot = app::Players::FindLootInSession(token);
        object loot_obj_body;

        for (auto& loot_item : loot) {
            std::uint32_t loot_type = loot_item->GetType();
            geom::Point2D position = loot_item->GetPosition();

            object loot_stats;
            loot_stats[KEY_type] = loot_type;
            loot_stats[KEY_pos] = {position.x, position.y};

            loot_obj_body[std::to_string(loot_item->GetId())] = loot_stats;
        }

        object result;
        result[KEY_players] = players_obj_body;
        result[KEY_lostObjects] = loot_obj_body;

        return std::move(MakeStringResponse(
            http::status::ok,
            serialize(result),
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON,
            {{http::field::cache_control, "no-cache"}}));
    });
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::InvalidPlayerNameResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::bad_request,
        R"({"code": "invalidArgument","message": "Invalid player name"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::InvalidTokenResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::unauthorized,
        R"({"code": "invalidToken","message": "Authorization header is missing"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::JoinGameResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    error_code ec;
    value req_body = parse(req.body(), ec);

    if (ec || !req_body.as_object().contains(KEY_userName) ||
        !req_body.as_object().contains(KEY_mapId))
    {
        return std::move(JoinRequestParseErrorResponse(
            std::forward<decltype(req)>(req)));
    }

    model::Map::Id map_id(req_body.at(KEY_mapId).as_string().c_str());
    std::string user_name(req_body.at(KEY_userName).as_string().c_str());

    if (user_name.empty()) {
        return std::move(InvalidPlayerNameResponse(
            std::forward<decltype(req)>(req)));
    }

    if (game_.FindMap(map_id) == nullptr) {
        return std::move(MapNotFoundResponse(std::forward<decltype(req)>(req)));
    }

    std::shared_ptr<app::Player> new_player =
        app::Application::join_game(game_, user_name, map_id);

    value body{
        {KEY_authToken, *new_player->GetToken()},
        {KEY_playerId, new_player->GetId()}
    };

    return std::move(MakeStringResponse(
        http::status::ok,
        serialize(body),
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::JoinRequestParseErrorResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::bad_request,
        R"({"code": "invalidArgument","message": "Join game request parse error"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::MapInfoResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req, 
    const model::Map::Id map_id)
{
    if (game_.FindMap(map_id) == nullptr) {
        return std::move(MapNotFoundResponse(std::forward<decltype(req)>(req)));
    }
    return std::move(MakeStringResponse(
        http::status::ok,
        MapInfoToJson(*game_.FindMap(map_id)),
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::MapNotFoundResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::not_found,
        R"({"code": "mapNotFound","message": "Map not found"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::PlayerActionResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return ExecuteAuthorized(std::forward<decltype(req)>(req),
        [this, &req] (const app::Token& token) mutable
    {
        auto it = req.find("Content-Type");

        std::string auth_value(it->value().data(), it->value().size());

        std::istringstream iss(auth_value);
        std::string content_type;
        iss >> content_type;

        if (it == req.end() || content_type != "application/json") {
            return std::move(MakeStringResponse(
                http::status::bad_request,
                R"({"code": "invalidArgument","message": "Invalid content type"})",
                req.version(),
                req.keep_alive(),
                ContentType::APP_JSON,
                {{http::field::cache_control, "no-cache"}}));
        }

        error_code ec;
        value action = parse(req.body(), ec);

        if (ec || !action.as_object().contains(KEY_move) ||
            !this->IsValidAction(std::move(
                action.at(KEY_move).as_string().c_str())))
        {
            return std::move(MakeStringResponse(
                http::status::bad_request,
                R"({"code": "invalidArgument","message": "Failed to parse action"})",
                req.version(),
                req.keep_alive(),
                ContentType::APP_JSON,
                {{http::field::cache_control, "no-cache"}}));
        }

        std::shared_ptr<app::Player> player_ptr =
            app::Players::FindPlayerByToken(token).value();

        player_ptr->MakeAction(action.at(KEY_move).as_string().c_str());

        object body;

        return std::move(MakeStringResponse(
            http::status::ok,
            serialize(body),
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON,
            {{http::field::cache_control, "no-cache"}}));
    });
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::PostOnlyResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::method_not_allowed,
        R"({"code": "invalidMethod","message": "Only POST method is expected"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"},
        {http::field::allow, "POST"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::TickResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    error_code ec;
    value req_body = parse(req.body(), ec);

    if (ec || !req_body.as_object().contains(KEY_timeDelta) ||
        !req_body.at(KEY_timeDelta).is_int64())
    {
        return std::move(TickRequestParseErrorResponse(
            std::forward<decltype(req)>(req)));
    }
    std::uint64_t time_delta(req_body.at(KEY_timeDelta).as_int64());

    game_.AddTestTime(std::chrono::milliseconds(time_delta));
    game_.Update(time_delta);

    object body;

    return std::move(MakeStringResponse(
        http::status::ok,
        serialize(body),
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::TickRequestParseErrorResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::bad_request,
        R"({"code": "invalidArgument","message": "Failed to parse tick request JSON"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

template <typename Body, typename Allocator>
ServerResponse ApiRequestHandler::UnknownTokenResponse(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    return std::move(MakeStringResponse(
        http::status::unauthorized,
        R"({"code": "unknownToken","message": "Player token has not been found"})",
        req.version(),
        req.keep_alive(),
        ContentType::APP_JSON,
        {{http::field::cache_control, "no-cache"}}));
}

}  // namespace http_handler