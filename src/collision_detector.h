// collision_detector.h
#pragma once

#include "geom.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace collision_detector {

enum ItemType {
    LOOT,
    OFFICE
};

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 &&
               proj_ratio <= 1 &&
               sq_distance <= collect_radius * collect_radius;
    }

    // квадрат расстояния до точки
    double sq_distance;

    // доля пройденного отрезка
    double proj_ratio;
};

CollectionResult TryCollectPoint(
    geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    geom::Point2D position;
    double width;
    std::uint32_t item_id_;
    ItemType item_type_;
};

struct Gatherer {
    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
    std::uint32_t gatherer_id_;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
    ItemType item_type_;
};

class GathererProvider: public ItemGathererProvider {
public:

    GathererProvider();
    GathererProvider(std::vector<Item> items,
                             std::vector<Gatherer> gatherers);

    size_t ItemsCount() const override;

    void AddItem(Item item);
    Item GetItem(size_t idx) const override;

    size_t GatherersCount() const override;

    void AddGatherer(Gatherer gatherer);
    Gatherer GetGatherer(size_t idx) const override;

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

std::vector<GatheringEvent> FindGatherEvents(
    const ItemGathererProvider& provider);

}  // namespace collision_detector