// model.cpp
#include "model.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include "application.h"
#include "database.h"
#include "model_serialization.h"
#include "my_logger.h"
#include "tagged_uuid.h"

#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

namespace model {

constexpr double MS_IN_SECONDS = 1000.0;

using namespace boost::json;
using namespace std::literals;
namespace bg = boost::geometry;
namespace fs = std::filesystem;
namespace logging = boost::log;

int GetRandomId(int max_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, max_id);
    return dist(gen);
}

Road::Road(HorizontalTag, Point start, Coord end_x) noexcept
    : start_{start}
    , end_{end_x, start.y}
{
    this->BuildBoundingBox();
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept
    : start_{start}
    , end_{start.x, end_y}
{
    this->BuildBoundingBox();
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

void Road::BuildBoundingBox() {
    if (this->IsVertical()) {
        double x = start_.x;
        double y_min = std::min(start_.y, end_.y);
        double y_max = std::max(start_.y, end_.y);
        bounding_box_ = Box(
            PointBG(x - ROAD_HALF_WIDTH, y_min),
            PointBG(x + ROAD_HALF_WIDTH, y_max));
    } else {
        double y = start_.y;
        double x_min = std::min(start_.x, end_.x);
        double x_max = std::max(start_.x, end_.x);
        bounding_box_ = Box(
            PointBG(x_min, y - ROAD_HALF_WIDTH),
            PointBG(x_max, y + ROAD_HALF_WIDTH));
    }
}

Box Road::GetBoundingBox() const {
    return bounding_box_;
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{bounds} {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{std::move(id)}
    , position_{position}
    , offset_{offset} {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

double Office::GetWidth() const noexcept {
    return width_;
}

Map::Map(Id id, std::string name) noexcept
    : id_(std::move(id))
    , name_(std::move(name)) {
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

void Map::SetDogSpeed(double dog_speed) {
    dog_speed_ = dog_speed;
}

double Map::GetDogSpeed() const {
    return dog_speed_;
}

void Map::SetBagCapacity(std::uint32_t bag_capacity) {
    bag_capacity_ = bag_capacity;
}

std::uint32_t Map::GetBagCapacity() const {
    return bag_capacity_;
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

void Map::BuildRoadRTree() {
    for (const Road& road : roads_) {
        road_rtree_.insert(std::make_pair(
            road.GetBoundingBox(),
            road
        ));
    }
}

Map::RoadRTree Map::GetRoadRTree() const {
    return road_rtree_;
}

Point Map::GetRandomPointOnRoad() const {
    std::random_device rd;
    static std::mt19937 gen(rd());
    int number_of_roads = int(roads_.size()) - 1;
    std::uniform_int_distribution<int> dist(0, number_of_roads);
    auto& random_road = roads_[dist(gen)];

    int x1 = random_road.GetStart().x;
    int y1 = random_road.GetStart().y;
    int x2 = random_road.GetEnd().x;
    int y2 = random_road.GetEnd().y;
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    std::uniform_int_distribution<int> dist_x(x1, x2);
    std::uniform_int_distribution<int> dist_y(y1, y2);
    int random_x = dist_x(gen);
    int random_y = dist_y(gen);

    return {random_x, random_y};
}

void Map::SetLootTypesCount(int loot_types_count) {
    loot_types_count_ = loot_types_count;
}

int Map::GetLootTypesCount() const {
    return loot_types_count_;
}

void Map::AddLootValue(std::uint32_t value) {
    loot_value_.push_back(value);
}

std::uint32_t Map::GetLootValue(size_t index) const {
    if (index < loot_value_.size()) {
        return loot_value_.at(index);
    } else {
        throw std::invalid_argument("Index out of range");
    }
}

std::uint32_t Dog::dog_counter_{0};

Dog::Dog(const std::string dog_name, geom::Point2D position)
    : dog_name_(dog_name)
    , position_(position) 
    , id_(dog_counter_++)
    , uuid_(util::detail::UUIDToString(util::detail::NewUUID())) {
}

void Dog::SetDirection(const DIRECTION& new_direction) {
    direction_ = new_direction;
}

DIRECTION Dog::GetDirection() const {
    return direction_;
}

const std::string Dog::GetName() const {
    return dog_name_;
}

void Dog::SetPosition(geom::Point2D position) {
    position_ = position;
}

geom::Point2D Dog::GetPosition() const {
    return position_;
}

void Dog::SetSpeed(const geom::Vec2D& new_speed) {
    speed_ = new_speed;
}

geom::Vec2D Dog::GetSpeed() const {
    return speed_;
}

void Dog::SetWidth(double width) {
    width_ = width;
}

double Dog::GetWidth() const {
    return width_;
}

void Dog::SetId(std::uint32_t id) {
    id_ = id;
}

std::uint32_t Dog::GetId() const {
    return id_;
}

void Dog::AddLoot(std::shared_ptr<Loot> loot) {
    bag_.emplace_back(loot);
}

const std::vector<std::shared_ptr<Loot>>& Dog::GetLoot() const {
    return bag_;
}

std::uint32_t Dog::GetLootCountInBag() const {
    return bag_.size();
}

void Dog::ReleaseLoot() {
    for (auto item : bag_) {
        score_ += item->GetValue();
    }
    bag_.clear();
}

void Dog::SetScore(std::uint32_t score) {
    score_ = score;
}

std::uint32_t Dog::GetScore() const {
    return score_;
}

std::uint32_t Dog::GetDogCounter() {
    return dog_counter_;
}

void Dog::SetDogCounter(std::uint32_t counter) {
    dog_counter_ = counter;
}

void Dog::SetJoinTime(std::chrono::milliseconds join_time) {
    join_time_ = join_time;
}
std::chrono::milliseconds Dog::GetJoinTime() const {
    return join_time_;
}

void Dog::SetStatus(DOG_STATUS status) {
    status_ = status;
}

Dog::DOG_STATUS Dog::GetStatus() const {
    return status_;
}

void Dog::UpdateInactivityTime(double delta) {
    inactivity_time_ += delta;
}

void Dog::ResetInactivityTimer() {
    inactivity_time_ = 0.0;
}

double Dog::GetInactivityTime() const {
    return inactivity_time_;
}

void Dog::SetUUID(const std::string& uuid) {
    uuid_ = uuid;
}

std::string Dog::GetUUID() {
    return uuid_;
}

std::uint32_t Loot::loot_counter_{0};

Loot::Loot(
    const std::uint32_t type,
    const geom::Point2D position,
    std::uint32_t loot_value
)
    : type_(type)
    , position_(position)
    , value_(loot_value)
    , id_(loot_counter_++) {
}

std::uint32_t Loot::GetType() const {
    return type_;
}

std::uint32_t Loot::GetId() const {
    return id_;
}

void Loot::SetId(std::uint32_t id) {
    id_ = id;
}

geom::Point2D Loot::GetPosition() const {
    return position_;
}

double Loot::GetWidth() const {
    return width_;
}

void Loot::SetWidth(double width) {
    width_ = width;
}

std::uint32_t Loot::GetValue() const {
    return value_;
}

std::uint32_t Loot::GetLootCounter() {
    return loot_counter_;
}

void Loot::SetLootCounter(std::uint32_t counter) {
    loot_counter_ = counter;
}

std::uint32_t GameSession::session_counter_{0};

GameSession::GameSession(const Map* map)
    : map_(map)
    , session_id_(session_counter_++) {
}

void GameSession::SetId(std::uint32_t id) {
    session_id_ = id;
}
std::uint32_t GameSession::GetId() const {
    return session_id_;
}

void GameSession::AddDog(std::shared_ptr<model::Dog> dog) {
    dogs_.push_back(dog);
}

std::vector<std::shared_ptr<Dog>>& GameSession::GetDogs() {
    return dogs_;
}

const std::vector<std::shared_ptr<Dog>>& GameSession::GetDogs() const {
    return dogs_;
}

std::shared_ptr<Dog> GameSession::GetDogById(std::uint32_t dog_id) const {
    auto it = std::find_if(dogs_.begin(), dogs_.end(), 
    [dog_id](const std::shared_ptr<Dog>& dog_ptr) {
        return dog_ptr->GetId() == dog_id;
    });

    if (it != dogs_.end()) {
        return *it;
    }

    return nullptr;
}

void GameSession::AddLoot(std::shared_ptr<model::Loot> loot) {
    loot_.emplace_back(loot);
}

std::list<std::shared_ptr<Loot>> GameSession::GetLoot() const {
    return loot_;
}

const Map* GameSession::GetMap() const {
    return map_;
}

std::shared_ptr<Loot> GameSession::GatherLoot(size_t loot_id) {
    auto it = std::find_if(loot_.begin(), loot_.end(), 
    [loot_id](const std::shared_ptr<Loot>& loot) {
        return loot->GetId() == loot_id;
    });

    if (it == loot_.end()) {
        throw std::invalid_argument("Loot with id "s + std::to_string(loot_id) +
            " not found"s);
    }

    std::shared_ptr<Loot> result = *it;

    loot_.erase(it);

    return result;
}

std::uint32_t GameSession::GetSessionCounter() {
    return session_counter_;
}

void GameSession::SetSessionCounter(std::uint32_t counter) {
    session_counter_ = counter;
}

void GameSession::RemoveDog(std::uint32_t dog_id) {
    auto it = std::find_if(
            dogs_.begin(), 
            dogs_.end(),
            [dog_id](const std::shared_ptr<Dog>& dog) {
                return dog && dog->GetId() == dog_id;
            }
        );

        if (it != dogs_.end()) {
            dogs_.erase(it);
        }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();

    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
        !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() +
                                    " already exists"s);
    } else {
        try {
            map.BuildRoadRTree();
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

std::shared_ptr<GameSession> Game::FindSessionById(
    std::uint32_t session_id) const
{
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [session_id](const auto& pair) {
            return pair.second->GetId() == session_id;
        });
    
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void Game::SetGameMode(bool test_mode) {
    if (test_mode) {
        game_mode_ = TEST;
    } else {
        game_mode_ = NORMAL;
    }
}

Game::GAME_MODE Game::GetGameMode() const {
    return game_mode_;
}

void Game::SetDogSpawnMode(bool random_spawn) {
    if (random_spawn) {
        dog_spawn_mode_ = RANDOM;
    } else {
        dog_spawn_mode_ = FIX;
    }
}

Game::SPAWN_MODE Game::GetDogSpawnMode() const {
    return dog_spawn_mode_;
}


void Game::SetSaveFilePath(const std::string& path) {
    save_file_path_ = path;
}

std::shared_ptr<GameSession> Game::AddDogToSession(
    std::shared_ptr<model::Dog> dog,
    const model::Map::Id& map_id)
{
    if (!sessions_.contains(map_id)) {
        auto session_ptr = std::make_shared<GameSession>(FindMap(map_id));
        sessions_[map_id] = session_ptr;
    }

    sessions_[map_id]->AddDog(dog);
    return sessions_[map_id];
}

void Game::SetLootGenerator(std::unique_ptr<loot_gen::LootGenerator> generator)
{
    if (generator) {
        loot_generator_ = std::move(generator);
    }
}


loot_gen::LootGenerator& Game::GetLootGenerator() const {
    if (loot_generator_) {
        return *loot_generator_;
    } else {
        throw std::runtime_error("Loot generator is not set");
    }
}

void Game::SetLootTypesStorage(
    std::unique_ptr<extra_data::LootTypesStorage> storage)
{
    if (storage) {
        loot_types_storage_ = std::move(storage);
    }
}

extra_data::LootTypesStorage& Game::GetLootTypesStorage() const {
    if (loot_types_storage_) {
        return *loot_types_storage_;
    }
    throw std::runtime_error("LootTyapesStorage is not set");
}

void Game::SetSavePeriod(int64_t period) {
    save_interval_ = std::chrono::milliseconds(period);
    save_test_timer_ = std::chrono::milliseconds(0);
    save_enabled_ = true;
}

void Game::Update(std::int64_t time_delta) {
    try {
        for (auto& session : sessions_) {
            collision_detector::GathererProvider gatherer_provider;

            UpdateDogsPosition(session.second, time_delta, gatherer_provider);
            UpdateLoot(session.second, time_delta);

            AddLootToGathererProvider(session.second, gatherer_provider);
            AddOfficesToGathererProvider(session.second, gatherer_provider);

            GatherLoot(session.second, std::move(gatherer_provider));

            RemoveInactiveDogs(session.second);
        }

        save_test_timer_ += std::chrono::milliseconds(time_delta);

        if (save_enabled_ && save_test_timer_ >= save_interval_) {
            SaveState();
            save_test_timer_ = std::chrono::milliseconds(0);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in Game::Update: " << e.what() << std::endl;
        throw;
    }
}

void Game::SaveState() const {
    fs::path final_save_path(save_file_path_);
    fs::path temp_save_path;

    if (!final_save_path.parent_path().empty() &&
        !fs::exists(final_save_path.parent_path()))
    {
        try {
            fs::create_directories(final_save_path.parent_path());
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create directory: " << e.what() << "\n";
            throw;
        }
    }

    try {
        temp_save_path = final_save_path.parent_path() / 
                        (final_save_path.filename().string() + ".tmp");
    }
    catch (const fs::filesystem_error&) {
        temp_save_path = final_save_path.string() + ".tmp";
    }

    try {
        std::ofstream ofs(temp_save_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot open temporary file for writing: "
                                     + temp_save_path.string());
        }

        boost::archive::text_oarchive oa{ofs};

        oa << std::string("sessions");

        size_t sessions_count = sessions_.size();
        oa << sessions_count;

        std::vector<serialization::GameSessionRepr> session_reprs;

        for (auto session : sessions_) {
            session_reprs.emplace_back(*session.second);
        }
        oa << session_reprs;

        oa << std::string("players");
        size_t players_count = app::Players::GetPlayers().size();
        oa << players_count;

        
        for (auto player : app::Players::GetPlayers()) {
            serialization::PlayerRepr player_repr{*player};
            oa << player_repr;
        }

        ofs.close();

        fs::rename(temp_save_path, final_save_path);

        value custom_data{final_save_path.string()};
        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(my_logger::additional_data, custom_data)
            << "game state successfully saved"sv;

    }
    catch (const std::exception& e) {
        try {
            if (fs::exists(temp_save_path)) {
                fs::remove(temp_save_path);
            }
        }
        catch (...) {
            // Игнорируем ошибки при удалении временного файла
        }
        
        std::cerr << "Failed to save game state: " << e.what() << "\n";
        throw;
    }
}

void Game::LoadState() {
    fs::path save_file_path(save_file_path_);

    try {
        std::ifstream ifs(save_file_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("Cannot open save file: "
                                     + save_file_path.string());
        }

        boost::archive::text_iarchive ia{ifs};

        sessions_.clear();

        std::string s_string;
        ia >> s_string;

        size_t sessions_count = 0;
        ia >> sessions_count;

        std::vector<serialization::GameSessionRepr> session_reprs;
        ia >> session_reprs;
        
        for (auto s_repr : session_reprs) {
            try {
                auto session = s_repr.Restore(*this);
                sessions_[session.GetMap()->GetId()] =
                    std::make_shared<GameSession>(session);

                value custom_data{session.GetId()};
                BOOST_LOG_TRIVIAL(info)
                    << logging::add_value(
                        my_logger::additional_data,
                        custom_data
                    )
                    << "session successfully loaded"sv;
            }
            catch (const boost::archive::archive_exception& e) {
                std::cerr <<
                    "Warning: Failed to load session from archive: " <<
                    e.what() << "\n";
                break;
            }
        }

        std::string p_string;
        ia >> p_string;
        size_t players_count = 0;
        ia >> players_count;
        
        for (size_t i = 0; i < players_count; ++i) {
            try {
                serialization::PlayerRepr repr;
                ia >> repr;
                app::Player player = repr.Restore(*this);
                app::Players::AddPlayer(std::make_shared<app::Player>(player));

                value custom_data{player.GetId()};
                BOOST_LOG_TRIVIAL(info)
                    << logging::add_value(
                        my_logger::additional_data,
                        custom_data)
                    << "player successfully loaded"sv;
            }
            catch (const boost::archive::archive_exception& e) {
                std::cerr <<
                    "Warning: Failed to load player from archive: " <<
                    e.what() << "\n";
                break;
            }
        }

        ifs.close();

        value custom_data{save_file_path.string()};
        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(my_logger::additional_data, custom_data)
            << "game state successfully loaded"sv;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load game state: " << e.what() << "\n";
        throw;
    }
}

void Game::SetDogRetirementTime(double retirement_time_seconds) {
    dog_retirement_time_seconds_ = retirement_time_seconds;
}

double Game::GetDogRetirementTime() const {
    return dog_retirement_time_seconds_;
}

void Game::SetDBConnectionPool(std::shared_ptr<database::ConnectionPool> pool) {
    pool_ = pool;
}

std::shared_ptr<database::ConnectionPool> Game::GetDBConnectionPool() const {
    return pool_;
}

void Game::AddTestTime(std::chrono::milliseconds delta) {
    if (GetGameMode() != GAME_MODE::TEST) return;

    accumulated_time_ += delta;
}

void Game::SetStartTime(std::chrono::steady_clock::time_point start_time) {
    start_time_ = start_time;
}

std::chrono::milliseconds Game::GetCurrentTime() {
    if (GetGameMode() == GAME_MODE::TEST) {
        return GetTestTime();
    }
    return GetRealTime();
}

std::chrono::milliseconds Game::GetTestTime() {
    return accumulated_time_;
}

std::chrono::milliseconds Game::GetRealTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
}

void Game::RemoveInactiveDogs(std::shared_ptr<GameSession>& session) {
    auto& dogs = session->GetDogs();

    std::vector<std::shared_ptr<Dog>> inactive_dogs;
    std::copy_if(dogs.begin(), dogs.end(), std::back_inserter(inactive_dogs),
        [this](const auto& dog) {
            return dog->GetInactivityTime() >= GetDogRetirementTime();
        });

    for (auto& dog : inactive_dogs) {
        database::Database::SaveRecord(GetDBConnectionPool(),
            database::PlayerRecord{
                dog->GetUUID(),
                dog->GetName(),
                dog->GetScore(),
                static_cast<std::uint64_t>(
                    (GetCurrentTime() - dog->GetJoinTime()).count())
            }
        );
        
        app::Players::RemovePlayerFromGameByDogId(dog->GetId());
    }

    dogs.erase(
        std::remove_if(dogs.begin(), dogs.end(),
            [this](const auto& dog) {
                return dog->GetInactivityTime() >= GetDogRetirementTime();
            }),
        dogs.end()
    );
}

void Game::UpdateDogsPosition(
        std::shared_ptr<GameSession>& session,
        std::int64_t time_delta_ms,
        collision_detector::GathererProvider& gatherer_provider)
{
    const Map* map = session->GetMap();
    double time_delta_s = time_delta_ms / MS_IN_SECONDS;

    for (std::shared_ptr<Dog>& dog : session->GetDogs()) {
        auto old_position = dog->GetPosition();
        auto new_dog_position = CalculateNewDogPosition(
            dog,
            time_delta_s,
            map->GetRoadRTree());

        dog->SetPosition({
            bg::get<0>(new_dog_position),
            bg::get<1>(new_dog_position)});

        gatherer_provider.AddGatherer({
            {old_position.x, old_position.y},
            {bg::get<0>(new_dog_position), bg::get<1>(new_dog_position)},
            dog->GetWidth(),
            dog->GetId()});

        if (old_position.x == bg::get<0>(new_dog_position) && 
            old_position.y == bg::get<1>(new_dog_position))
        {
            dog->SetStatus(Dog::DOG_STATUS::INACTIVE);
            dog->UpdateInactivityTime(time_delta_s);
        } else {
            dog->SetStatus(Dog::DOG_STATUS::ACTIVE);
            dog->ResetInactivityTimer();
        }
    }
}

PointBG Game::CalculateNewDogPosition(
    std::shared_ptr<Dog>& dog,
    double time_delta_s,
    const Map::RoadRTree& roads)
{
    const PointBG current_pos{dog->GetPosition().x, dog->GetPosition().y};
    const geom::Vec2D speed = dog->GetSpeed();
    const DIRECTION dir = dog->GetDirection();

    if (dir == DIRECTION::NONE || 
        (std::abs(speed.x) < std::numeric_limits<double>::epsilon() &&
        std::abs(speed.y) < std::numeric_limits<double>::epsilon()))
    {
        return current_pos;
    }

    PointBG target_pos = current_pos;
    bg::set<0>(target_pos, bg::get<0>(current_pos) + speed.x * time_delta_s);
    bg::set<1>(target_pos, bg::get<1>(current_pos) + speed.y * time_delta_s);

    Segment movement{current_pos, target_pos};
    std::vector<std::pair<Box, Road>> relevant_roads;
    roads.query(
        bgi::satisfies([&](const std::pair<Box, Road>& item) {
            return bg::intersects(movement, item.first) || 
                   bg::within(current_pos, item.first);
        }),
        std::back_inserter(relevant_roads));

    if (relevant_roads.empty()) {
        return current_pos;
    }

    bool can_move_to_target = std::any_of(
        relevant_roads.begin(), 
        relevant_roads.end(),
        [&target_pos, this](const auto& road) {
            return IsPointOnRoad(target_pos, road.second);
        });

    if (can_move_to_target) {
        return target_pos;
    }

    PointBG stop_pos = current_pos;
    double max_distance = std::numeric_limits<double>::min();

    for (const auto& [box, road] : relevant_roads) {
        if (IsPointOnRoad(current_pos, road)) {
            PointBG candidate = FindStopPoint(current_pos, target_pos, road, dir);

            double dist = bg::distance(current_pos, candidate);

            if (dist > max_distance) {
                max_distance = dist;
                stop_pos = candidate;
            }
        }
    }

    dog->SetSpeed({0, 0});
    return stop_pos;
}

bool Game::IsPointOnRoad(const PointBG& point, const Road& road) const {
    const double x = bg::get<0>(point);
    const double y = bg::get<1>(point);
    
    if (road.IsHorizontal()) {
        const double min_x =
            std::min(road.GetStart().x, road.GetEnd().x) - ROAD_HALF_WIDTH;
        const double max_x =
            std::max(road.GetStart().x, road.GetEnd().x) + ROAD_HALF_WIDTH;

        return x >= min_x && x <= max_x &&
               y >= road.GetStart().y - ROAD_HALF_WIDTH && 
               y <= road.GetStart().y + ROAD_HALF_WIDTH;
    } else {
        const double min_y =
            std::min(road.GetStart().y, road.GetEnd().y) - ROAD_HALF_WIDTH;
        const double max_y =
            std::max(road.GetStart().y, road.GetEnd().y) + ROAD_HALF_WIDTH;

        return y >= min_y && y <= max_y &&
               x >= road.GetStart().x - ROAD_HALF_WIDTH && 
               x <= road.GetStart().x + ROAD_HALF_WIDTH;
    }
}

PointBG Game::FindStopPoint(const PointBG& from, const PointBG& to, 
                           const Road& road, DIRECTION dir) const {
    double x = bg::get<0>(from);
    double y = bg::get<1>(from);

    const double min_x = std::min(road.GetStart().x, road.GetEnd().x);
    const double max_x = std::max(road.GetStart().x, road.GetEnd().x);
    const double min_y = std::min(road.GetStart().y, road.GetEnd().y);
    const double max_y = std::max(road.GetStart().y, road.GetEnd().y);

    if (dir == DIRECTION::EAST) {
        x = std::min(bg::get<0>(to), max_x + ROAD_HALF_WIDTH);
        return PointBG{x, y};
    } else if (dir == DIRECTION::WEST) {
        x = std::max(bg::get<0>(to), min_x - ROAD_HALF_WIDTH);
        return PointBG{x, y};
    } else if (dir == DIRECTION::NORTH) {
        y = std::max(bg::get<1>(to), min_y - ROAD_HALF_WIDTH);
        return PointBG{x, y};
    } else if (dir == DIRECTION::SOUTH) {
        y = std::min(bg::get<1>(to), max_y + ROAD_HALF_WIDTH);
        return PointBG{x, y};
    }
    return from;
}

void Game::UpdateLoot(
    std::shared_ptr<GameSession>& session,
    std::int64_t time_delta)
{
    const Map* map = session->GetMap();
    auto time_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::duration<double>(time_delta)
    );

    unsigned loot_count = session->GetLoot().size();
    unsigned looter_count = session->GetDogs().size();

    unsigned new_loot_generated = loot_generator_->Generate(
        time_delta_ms,
        loot_count,
        looter_count
    );

    for (unsigned i = 0; i < new_loot_generated; ++i) {
        Point point = map->GetRandomPointOnRoad();
        geom::Point2D position = {point.x * 1.0, point.y * 1.0};
        int loot_type = GetRandomId(map->GetLootTypesCount() - 1);

        std::uint32_t value = map->GetLootValue(loot_type);

        auto loot = std::make_shared<model::Loot>(loot_type, position, value);
        session->AddLoot(loot);
    }
}

void Game::AddLootToGathererProvider(
    std::shared_ptr<GameSession>& session,
    collision_detector::GathererProvider& gatherer_provider) const
{
    auto loot_list = session->GetLoot();
    for (auto item : loot_list) {
        auto position = item->GetPosition();
        gatherer_provider.AddItem({
            {position.x, position.y},
            item->GetWidth(),
            item->GetId(),
            collision_detector::ItemType::LOOT
        });
    }
}

void Game::AddOfficesToGathererProvider(
    std::shared_ptr<GameSession>& session,
    collision_detector::GathererProvider& gatherer_provider) const
{
    auto office_list = session->GetMap()->GetOffices();
    for (auto office : office_list) {
        auto position = office.GetPosition();
        std::string office_id = *office.GetId();
        gatherer_provider.AddItem({
            {static_cast<double>(position.x), static_cast<double>(position.y)},
            office.GetWidth(),
            static_cast<std::uint32_t>(std::stoul(office_id.substr(1))),
            collision_detector::ItemType::OFFICE
        });
    }
}
void Game::GatherLoot(
        std::shared_ptr<GameSession>& session,
        collision_detector::GathererProvider&& gatherer_provider) const
{
    auto collision_events = FindGatherEvents(gatherer_provider);
    std::vector<uint32_t> collected_loot_id;

    auto existing_loot = session->GetLoot();
    std::vector<uint32_t> existing_loot_ids;
    for (const auto& loot : existing_loot) {
        existing_loot_ids.push_back(loot->GetId());
    }

    for (auto& event : collision_events) {
        if (event.item_type_ != collision_detector::ItemType::OFFICE) {
            if (std::find(
                existing_loot_ids.begin(),
                existing_loot_ids.end(),
                event.item_id) == existing_loot_ids.end())
            {
                continue;
            }
        }

        auto dog = session->GetDogs()[event.gatherer_id];

        bool already_collected = std::find(
            collected_loot_id.begin(),
            collected_loot_id.end(),
            event.item_id
        ) != collected_loot_id.end();
        
        if (event.item_type_ == collision_detector::ItemType::OFFICE) {
            dog->ReleaseLoot();
        } else {
            if (dog != nullptr && !already_collected) {
                std::uint32_t bag_capacity = session->GetMap()->GetBagCapacity();
                std::uint32_t loot_count_in_bag = dog->GetLootCountInBag();

                if (loot_count_in_bag < bag_capacity) {
                    auto loot = session->GatherLoot(event.item_id);
                    if (loot) {
                        dog->AddLoot(loot);
                        collected_loot_id.push_back(event.item_id);
                    }
                }
            }
        }
    }
}

}  // namespace model