// model.h
#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "collision_detector.h"
#include "database.h"
#include "extra_data.h"
#include "loot_generator.h"
#include "tagged.h"

#include <algorithm>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace model {

static const double DEFAULT_DOG_SPEED = 1.0;
static const double DEFAULT_RETIREMENT_TIME = 60.0;
static const std::uint32_t DEFAULT_BAG_CAPACITY = 3;
const double DOG_WIDTH = 0.6;
const double LOOT_WIDTH = 0.0;
const double OFFICE_WIDTH = 0.5;
const double ROAD_HALF_WIDTH = 0.4;

using Dimension = int;
using Coord = Dimension;

namespace bg = boost::geometry;
namespace bgi = bg::index;

using PointBG = bg::model::point<double, 2, bg::cs::cartesian>;
using Box = bg::model::box<PointBG>;
using Segment = bg::model::segment<PointBG>;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

enum DIRECTION{
    NORTH,
    SOUTH,
    WEST,
    EAST,
    NONE
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept;
    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;

    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;

    Point GetEnd() const noexcept;

    void BuildBoundingBox();
    Box GetBoundingBox() const;
    
private:
    Point start_;
    Point end_;
    Box bounding_box_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;

    Offset GetOffset() const noexcept;

    Point GetPosition() const noexcept;

    double GetWidth() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
    double width_ = OFFICE_WIDTH;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    using RoadRTree = bgi::rtree<std::pair<Box, Road>, bgi::rstar<16>>;

    Map(Id id, std::string name) noexcept;

    void AddBuilding(const Building& building);
    const Buildings& GetBuildings() const noexcept;

    void SetDogSpeed(double dog_speed);
    double GetDogSpeed() const;

    void SetBagCapacity(std::uint32_t bag_capacity);
    std::uint32_t GetBagCapacity() const;
    
    const Id& GetId() const noexcept;

    void AddOffice(Office office);
    const Offices& GetOffices() const noexcept;

    const std::string& GetName() const noexcept;

    void AddRoad(const Road& road);
    const Roads& GetRoads() const noexcept;

    void BuildRoadRTree();
    RoadRTree GetRoadRTree() const;

    Point GetRandomPointOnRoad() const;

    void SetLootTypesCount(int loot_types_count);
    int GetLootTypesCount() const;

    void AddLootValue(std::uint32_t value);
    std::uint32_t GetLootValue(size_t index) const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t,
        util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_;
    std::uint32_t bag_capacity_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    RoadRTree road_rtree_;
    int loot_types_count_;
    std::vector<std::uint32_t> loot_value_;
};

class Loot {
public:
    Loot(
        const std::uint32_t type,
        const geom::Point2D position,
        std::uint32_t value);

    std::uint32_t GetType() const;

    std::uint32_t GetId() const;
    void SetId(std::uint32_t id);
    
    geom::Point2D GetPosition() const;

    double GetWidth() const;
    void SetWidth(double width);

    std::uint32_t GetValue() const;

    static std::uint32_t GetLootCounter();
    static void SetLootCounter(std::uint32_t counter);

private:
    std::uint32_t type_;
    std::uint32_t id_;
    geom::Point2D position_;
    double width_ = LOOT_WIDTH;
    std::uint32_t value_;
    static std::uint32_t loot_counter_;
};

class Dog {
public:
    enum class DOG_STATUS {
        ACTIVE,
        INACTIVE,
    };

    Dog(const std::string dog_name, geom::Point2D position);

    void SetDirection(const DIRECTION& new_direction);
    DIRECTION GetDirection() const;

    const std::string GetName() const;

    void SetPosition(geom::Point2D position);
    geom::Point2D GetPosition() const;

    void SetSpeed(const geom::Vec2D& new_speed);
    geom::Vec2D GetSpeed() const;

    void SetWidth(double width);
    double GetWidth() const;

    void SetId(std::uint32_t id);
    std::uint32_t GetId() const;

    void AddLoot(std::shared_ptr<Loot> loot);
    const std::vector<std::shared_ptr<Loot>>& GetLoot() const;
    std::uint32_t GetLootCountInBag() const;

    void ReleaseLoot();

    void SetScore(std::uint32_t score);
    std::uint32_t GetScore() const;

    static std::uint32_t GetDogCounter();
    static void SetDogCounter(std::uint32_t counter);

    void SetJoinTime(std::chrono::milliseconds join_time);
    std::chrono::milliseconds GetJoinTime() const;

    void SetStatus(DOG_STATUS status);
    DOG_STATUS GetStatus() const;

    void UpdateInactivityTime(double delta);
    void ResetInactivityTimer();
    double GetInactivityTime() const;

    void SetUUID(const std::string& uuid);;
    std::string GetUUID();

private:
    std::uint32_t id_;
    std::string dog_name_;
    geom::Point2D position_;
    geom::Vec2D speed_{0.0, 0.0};
    DIRECTION direction_{NORTH};
    std::vector<std::shared_ptr<Loot>> bag_;
    double width_ = DOG_WIDTH;
    std::uint32_t score_ = 0;
    static std::uint32_t dog_counter_;
    std::chrono::milliseconds join_time_;
    double inactivity_time_{0};
    DOG_STATUS status_{};
    std::string uuid_;
};

class GameSession {
public:
    GameSession(const Map* map_);

    void SetId(std::uint32_t id);
    std::uint32_t GetId() const;

    void AddDog(std::shared_ptr<Dog> dog);
    std::vector<std::shared_ptr<Dog>>& GetDogs();
    const std::vector<std::shared_ptr<Dog>>& GetDogs() const;
    std::shared_ptr<Dog> GetDogById(std::uint32_t id) const;

    void AddLoot(std::shared_ptr<Loot> loot);
    std::list<std::shared_ptr<Loot>> GetLoot() const;

    const Map* GetMap() const;

    std::shared_ptr<Loot> GatherLoot(size_t loot_id);

    static std::uint32_t GetSessionCounter();
    static void SetSessionCounter(std::uint32_t counter);

    void RemoveDog(std::uint32_t dog_id);

private:
    const Map* map_;
    std::vector<std::shared_ptr<Dog>> dogs_;
    std::list<std::shared_ptr<Loot>> loot_;
    std::uint32_t session_id_;
    static std::uint32_t session_counter_;
};

class Game {
public:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using Maps = std::vector<Map>;
    
    enum GAME_MODE {
        NORMAL,
        TEST
    };

    enum SPAWN_MODE {
        RANDOM,
        FIX
    };

    void AddMap(Map map);
    const Map* FindMap(const Map::Id& id) const noexcept;
    const Maps& GetMaps() const noexcept;

    std::shared_ptr<GameSession> FindSessionById(std::uint32_t session_id) const;

    void SetGameMode(bool test_mode);
    GAME_MODE GetGameMode() const;

    void SetDogSpawnMode(bool spawn_mode);
    SPAWN_MODE GetDogSpawnMode() const;

    void SetSaveFilePath(const std::string& path);

    std::shared_ptr<GameSession> AddDogToSession(
        std::shared_ptr<model::Dog> dog,
        const model::Map::Id& map_id);

    void SetLootGenerator(std::unique_ptr<loot_gen::LootGenerator> generator);
    loot_gen::LootGenerator& GetLootGenerator() const;

    void SetLootTypesStorage(std::unique_ptr<extra_data::LootTypesStorage> storage);
    extra_data::LootTypesStorage& GetLootTypesStorage() const;

    void SetSavePeriod(int64_t period);

    void Update(std::int64_t time_delta);

    void SaveState() const;

    void LoadState();

    void SetDogRetirementTime(double retirement_time_seconds);
    double GetDogRetirementTime() const;

    void SetDBConnectionPool(std::shared_ptr<database::ConnectionPool> pool);
    std::shared_ptr<database::ConnectionPool> GetDBConnectionPool() const;

    void AddTestTime(std::chrono::milliseconds delta);

    void SetStartTime(std::chrono::steady_clock::time_point start_time);

    std::chrono::milliseconds GetCurrentTime();
private:
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    void UpdateDogsPosition(
        std::shared_ptr<GameSession>& session,
        std::int64_t time_delta,
        collision_detector::GathererProvider& gatherer_provider);

    PointBG CalculateNewDogPosition(
        std::shared_ptr<Dog>& dog,
        double time_delta,
        const Map::RoadRTree& roads);

    PointBG GetStopPointForDirection(
        const PointBG& pos,
        const Box& road_bb,
        DIRECTION dir);

    bool IsPointOnRoad(const PointBG& point, const Road& road) const;

    PointBG FindStopPoint(const PointBG& from, const PointBG& to, 
                            const Road& road, DIRECTION dir) const;

    void UpdateLoot(
        std::shared_ptr<GameSession>& session,
        std::int64_t time_delta);

    void AddLootToGathererProvider(
        std::shared_ptr<GameSession>& session,
        collision_detector::GathererProvider& gatherer_provider) const;

    void AddOfficesToGathererProvider(
        std::shared_ptr<GameSession>& session,
        collision_detector::GathererProvider& gatherer_provider) const;

    void GatherLoot(
        std::shared_ptr<GameSession>& session,
        collision_detector::GathererProvider&& gatherer_provider) const;

    void RemoveInactiveDogs(std::shared_ptr<GameSession>& session);
    std::chrono::milliseconds GetTestTime();
    std::chrono::milliseconds GetRealTime();

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    GAME_MODE game_mode_;
    SPAWN_MODE dog_spawn_mode_;
    std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
    std::unique_ptr<extra_data::LootTypesStorage> loot_types_storage_;
    std::string save_file_path_;
    std::chrono::milliseconds save_interval_;
    std::chrono::milliseconds save_test_timer_;
    bool save_enabled_{false};
    double dog_retirement_time_seconds_{DEFAULT_RETIREMENT_TIME};
    std::shared_ptr<database::ConnectionPool> pool_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::milliseconds accumulated_time_{0};

    std::unordered_map<
        Map::Id,
        std::shared_ptr<GameSession>,
        MapIdHasher> sessions_;
};

}  // namespace model
