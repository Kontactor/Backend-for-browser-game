#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

// Напишите здесь тесты для функции collision_detector::FindGatherEvents

using namespace collision_detector;

class TestItemGathererProvider:public ItemGathererProvider {
public:

    TestItemGathererProvider(std::vector<Item> items,
                             std::vector<Gatherer> gatherers)
        : items_(items)
        , gatherers_(gatherers) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        if (idx >= items_.size() || idx < 0) {
            throw std::out_of_range("Item index out of range");
        }
        return items_[idx];
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        if (idx >= gatherers_.size() || idx < 0) {
            throw std::out_of_range("Gatherer index out of range");
        }
        return gatherers_[idx];
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

SCENARIO("Gather Events") {
    GIVEN("A TestItemGathererProvider with items and gatherers") {
        std::vector<Item> items({
            Item{geom::Point2D{3.0, 0.5}, 0.1},  // 0 Should be collected
            Item{geom::Point2D{5.0, 1.5}, 0.1},  // 1 Should be collected twice
            Item{geom::Point2D{1.0, 1.5}, 0.1},  // 2 Too far - should not be collected
            Item{geom::Point2D{9.0, 3.0}, 0.1},  // 3 Should be collected
            Item{geom::Point2D{5.0, 0.0}, 0.1},  // 4 Too far - should not be collected
            Item{geom::Point2D{3.0, 3.0}, 0.1},  // 5 Should be collected
            Item{geom::Point2D{6.0, 3.0}, 0.1}   // 6 Too far - should not be collected
        });
        std::vector<Gatherer> gatherers({
            Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 3.0}, 1.0},
            Gatherer{geom::Point2D{6.5, 0.0}, geom::Point2D{2.5, 4.0}, 1.0}
        });
        TestItemGathererProvider provider(items, gatherers);

        THEN("Items and gatherers count are correct") {
            REQUIRE(provider.ItemsCount() == size_t(7));
            REQUIRE(provider.GatherersCount() == size_t(2));
        }

        WHEN("FindGatherEvents is called") {
            auto result = collision_detector::FindGatherEvents(provider);

            THEN("Correct number of events is returned") {
                REQUIRE(result.size() == size_t(6));
            }

            THEN("Events are sorted by time in ascending order") {
                for (size_t i = 1; i < result.size(); ++i) {
                    REQUIRE(result[i].time >= result[i-1].time);
                }
            }

            THEN("All times between 0 and 1") {
                for (size_t i = 0; i < result.size(); ++i) {
                    REQUIRE(result[i].time >= 0);
                    REQUIRE(result[i].time <= 1);
                }
            }

            THEN("Items collected in right order") {
                std::vector<size_t> expected_item_ids = {4, 0, 1, 1, 5, 3};
                std::vector<size_t> item_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    item_ids.emplace_back(result[i].item_id);
                }

                REQUIRE(item_ids == expected_item_ids);
            }

            THEN("Gatherers collected in right order") {
                std::vector<size_t> expected_gatherer_ids = {1, 0, 1, 0, 1, 0};
                std::vector<size_t> gatherer_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    gatherer_ids.emplace_back(result[i].gatherer_id);
                }

                REQUIRE(gatherer_ids == expected_gatherer_ids);
            }
        }
    }
}

SCENARIO("Gather Events - Edge cases") {
    GIVEN("No items") {
        std::vector<Item> items;
        std::vector<Gatherer> gatherers{
            Gatherer{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 1}
        };
        
        TestItemGathererProvider provider(items, gatherers);
        
        WHEN("FindGatherEvents is called") {
            auto result = FindGatherEvents(provider);
            
            THEN("No events are returned") {
                REQUIRE(result.empty());
            }
        }
    }

    GIVEN("No gatherers") {
        std::vector<Item> items{
            Item{geom::Point2D{5.0, 0.0}, 0.1}
        };
        std::vector<Gatherer> gatherers;
        
        TestItemGathererProvider provider(items, gatherers);
        
        WHEN("FindGatherEvents is called") {
            auto result = FindGatherEvents(provider);
            
            THEN("No events are returned") {
                REQUIRE(result.empty());
            }
        }
    }
}

SCENARIO("Gather Events one gatherer") {
    GIVEN("A TestItemGathererProvider with items and gatherers") {
        std::vector<Item> items({
            Item{geom::Point2D{10.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{30.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{50.0, 0.0}, 0.1},  // Should be collected
        });
        std::vector<Gatherer> gatherers({
            Gatherer{geom::Point2D{0, 0}, geom::Point2D{60, 0}, 1}
        });
        TestItemGathererProvider provider(items, gatherers);

        THEN("Items and gatherers count are correct") {
            REQUIRE(provider.ItemsCount() == size_t(3));
            REQUIRE(provider.GatherersCount() == size_t(1));
        }

        WHEN("FindGatherEvents is called") {
            auto result = collision_detector::FindGatherEvents(provider);

            THEN("Correct number of events is returned") {
                REQUIRE(result.size() == size_t(3));
            }

            THEN("Events are sorted by time in ascending order") {
                for (size_t i = 1; i < result.size(); ++i) {
                    REQUIRE(result[i].time >= result[i-1].time);
                }
            }

            THEN("Items collected in right order") {
                std::vector<size_t> expected_item_ids = {0, 1, 2};
                std::vector<size_t> item_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    item_ids.emplace_back(result[i].item_id);
                }

                REQUIRE(item_ids == expected_item_ids);
            }

            THEN("Gatherers collected in right order") {
                std::vector<size_t> expected_gatherer_ids = {0, 0, 0};
                std::vector<size_t> gatherer_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    gatherer_ids.emplace_back(result[i].gatherer_id);
                }

                REQUIRE(gatherer_ids == expected_gatherer_ids);
            }
        }
    }
}

SCENARIO("Gather Events two gatherers with different ways") {
    GIVEN("A TestItemGathererProvider with items and gatherers") {
        std::vector<Item> items({
            Item{geom::Point2D{10.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{30.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{50.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{20.0, 3.0}, 0.1},  // Should be collected
            Item{geom::Point2D{40.0, 3.0}, 0.1},  // Should be collected
        });
        std::vector<Gatherer> gatherers({
            Gatherer{geom::Point2D{0, 0}, geom::Point2D{60, 0}, 1},
            Gatherer{geom::Point2D{0, 3}, geom::Point2D{60, 3}, 1}
        });
        TestItemGathererProvider provider(items, gatherers);

        THEN("Items and gatherers count are correct") {
            REQUIRE(provider.ItemsCount() == size_t(5));
            REQUIRE(provider.GatherersCount() == size_t(2));
        }

        WHEN("FindGatherEvents is called") {
            auto result = collision_detector::FindGatherEvents(provider);

            THEN("Correct number of events is returned") {
                REQUIRE(result.size() == size_t(5));
            }

            THEN("Events are sorted by time in ascending order") {
                for (size_t i = 1; i < result.size(); ++i) {
                    REQUIRE(result[i].time >= result[i-1].time);
                }
            }

            THEN("Items collected in right order") {
                std::vector<size_t> expected_item_ids = {0, 3, 1, 4, 2};
                std::vector<size_t> item_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    item_ids.emplace_back(result[i].item_id);
                }

                REQUIRE(item_ids == expected_item_ids);
            }

            THEN("Gatherers collected in right order") {
                std::vector<size_t> expected_gatherer_ids = {0, 1, 0, 1, 0};
                std::vector<size_t> gatherer_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    gatherer_ids.emplace_back(result[i].gatherer_id);
                }

                REQUIRE(gatherer_ids == expected_gatherer_ids);
            }
        }
    }
}

SCENARIO("Gather Events two gatherers with same way") {
    GIVEN("A TestItemGathererProvider with items and gatherers") {
        std::vector<Item> items({
            Item{geom::Point2D{10.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{30.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{50.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{20.0, 0.0}, 0.1},  // Should be collected
            Item{geom::Point2D{40.0, 0.0}, 0.1},  // Should be collected
        });
        std::vector<Gatherer> gatherers({
            Gatherer{geom::Point2D{5, 0}, geom::Point2D{60, 0}, 1},
            Gatherer{geom::Point2D{0, 0}, geom::Point2D{60, 0}, 1}
        });
        TestItemGathererProvider provider(items, gatherers);

        THEN("Items and gatherers count are correct") {
            REQUIRE(provider.ItemsCount() == size_t(5));
            REQUIRE(provider.GatherersCount() == size_t(2));
        }

        WHEN("FindGatherEvents is called") {
            auto result = collision_detector::FindGatherEvents(provider);

            THEN("Correct number of events is returned") {
                REQUIRE(result.size() == size_t(10));
            }

            THEN("Events are sorted by time in ascending order") {
                for (size_t i = 1; i < result.size(); ++i) {
                    REQUIRE(result[i].time >= result[i-1].time);
                }
            }

            THEN("Items collected in right order") {
                std::vector<size_t> expected_item_ids = {0, 0, 3, 3, 1, 1, 4, 4, 2, 2};
                std::vector<size_t> item_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    item_ids.emplace_back(result[i].item_id);
                }

                REQUIRE(item_ids == expected_item_ids);
            }

            THEN("Gatherers collected in right order") {
                std::vector<size_t> expected_gatherer_ids = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
                std::vector<size_t> gatherer_ids;
                for (size_t i = 0; i < result.size(); ++i) {
                    gatherer_ids.emplace_back(result[i].gatherer_id);
                }

                REQUIRE(gatherer_ids == expected_gatherer_ids);
            }
        }
    }
}