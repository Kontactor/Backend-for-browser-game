#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Vector serialization") {
    GIVEN("A vector") {
        const geom::Vec2D p{10, 20};
        WHEN("vector is serialized") {
            output_archive << p;

            THEN("it is equal to vector after serialization") {
                InputArchive input_archive{strm};
                geom::Vec2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Loot item serialization") {
    GIVEN("A loot item") {
        model::Loot item{10, {20, 30}, 40};
        item.SetId(3);
        item.SetWidth(0.0);
        model::Loot::SetLootCounter(5);
        WHEN("loot item is serialized") {
            {
                serialization::LootRepr repr{item};
                output_archive << repr;
            }

            THEN("it is equal to loot item after serialization") {
                InputArchive input_archive{strm};
                serialization::LootRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(item.GetType() == restored.GetType());
                CHECK(item.GetId() == restored.GetId());
                CHECK(item.GetPosition() == restored.GetPosition());
                CHECK(item.GetValue() == restored.GetValue());
                CHECK(item.GetWidth() == restored.GetWidth());
                CHECK(item.GetLootCounter() == restored.GetLootCounter());
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            model::Dog dog{"Pluto"s, {42.2, 12.5}};
            dog.SetId(2);
            dog.SetSpeed({2.3, -1.2});
            dog.SetDirection(model::DIRECTION::EAST);

            model::Loot item_1{5, {20, 3}, 40};
            item_1.SetId(1);
            item_1.SetWidth(0.0);

            model::Loot item_2{10, {2, 30}, 20};
            item_2.SetId(3);
            item_2.SetWidth(0.0);
            model::Loot::SetLootCounter(5);

            auto item_1_ptr = std::make_shared<model::Loot>(item_1);
            auto item_2_ptr = std::make_shared<model::Loot>(item_2);

            dog.AddLoot(item_1_ptr);
            dog.AddLoot(item_2_ptr),

            dog.SetWidth(0.5);
            dog.SetScore(42);
            model::Dog::SetDogCounter(5);
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetId() == restored.GetId());
                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetPosition() == restored.GetPosition());
                CHECK(dog.GetSpeed() == restored.GetSpeed());
                CHECK(dog.GetDirection() == restored.GetDirection());
                CHECK(dog.GetLoot().size() == restored.GetLoot().size());
                CHECK(dog.GetWidth() == restored.GetWidth());
                CHECK(dog.GetScore() == restored.GetScore());
                CHECK(dog.GetDogCounter() == restored.GetDogCounter());
            }
        }
    }
}