// model_serialization.h
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include "application.h"
#include "model.h"

namespace geom {

template <typename Archive>
void serialize(
    Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version)
{
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(
    Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version)
{
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace serialization {

class LootRepr {
public:
    LootRepr() = default;
    
    explicit LootRepr(const model::Loot& loot)
        : type_(loot.GetType())
        , id_(loot.GetId())
        , value_(loot.GetValue())
        , position_(loot.GetPosition())
        , width_(loot.GetWidth())
        , loot_counter_(model::Loot::GetLootCounter()) {
    }

    model::Loot Restore() const {
        model::Loot loot(type_, position_, value_);
        loot.SetId(id_);
        loot.SetWidth(width_);
        model::Loot::SetLootCounter(loot_counter_);
        return loot;
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned version) {
        ar& type_;
        ar& id_;
        ar& value_;
        ar& position_;
        ar& width_;
        ar& loot_counter_;
    }

private:
    std::uint32_t type_;
    std::uint32_t id_;
    std::uint32_t value_;
    geom::Point2D position_;
    double width_;
    std::uint32_t loot_counter_;
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , dog_name_(dog.GetName())
        , position_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , width_(dog.GetWidth())
        , score_(dog.GetScore())
        , dog_counter_(model::Dog::GetDogCounter())
    {
        for (const auto& loot_ptr : dog.GetLoot()) {
            if (loot_ptr) {
                bag_.push_back(std::make_shared<LootRepr>(*loot_ptr));
            }
        }
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{dog_name_, position_};
        dog.SetId(id_);
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);

        for (const auto& loot_repr_ptr : bag_) {
            if (loot_repr_ptr) {
                auto loot = loot_repr_ptr->Restore();
                dog.AddLoot(std::make_shared<model::Loot>(loot));
            }
        }

        dog.SetWidth(width_);
        dog.SetScore(score_);
        model::Dog::SetDogCounter(dog_counter_);

        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& dog_name_;
        ar& position_;
        ar& speed_;
        ar& direction_;
        ar& bag_;
        ar& width_;
        ar& score_;
        ar& dog_counter_;
    }

private:
    std::uint32_t id_;
    std::string dog_name_;
    geom::Point2D position_;
    geom::Vec2D speed_;
    model::DIRECTION direction_;
    std::vector<std::shared_ptr<LootRepr>> bag_;
    double width_;
    std::uint32_t score_;
    std::uint32_t dog_counter_;
};

class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(const model::GameSession& session)
        : map_id_(*session.GetMap()->GetId())
        , session_id_(session.GetId())
        , session_counter_(model::GameSession::GetSessionCounter())
    {
        for (const auto& dog_ptr : session.GetDogs()) {
            if (dog_ptr) {
                dogs_.push_back(std::make_shared<DogRepr>(*dog_ptr));
            }
        }
        
        for (const auto& loot_ptr : session.GetLoot()) {
            if (loot_ptr) {
                loot_.push_back(std::make_shared<LootRepr>(*loot_ptr));
            }
        }
    }

    [[nodiscard]] model::GameSession Restore(const model::Game& game) const {
        
        model::GameSession session{game.FindMap(model::Map::Id(map_id_))};

        for (const auto& dog_repr_ptr : dogs_) {
            if (dog_repr_ptr) {
                auto dog = dog_repr_ptr->Restore();
                session.AddDog(std::make_shared<model::Dog>(dog));
            }
        }

        for (const auto& loot_repr_ptr : loot_) {
            if (loot_repr_ptr) {
                auto loot = loot_repr_ptr->Restore();
                session.AddLoot(std::make_shared<model::Loot>(loot));
            }
        }

        session.SetId(session_id_);

        model::GameSession::SetSessionCounter(session_counter_);

        return session;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_;
        ar& dogs_;
        ar& loot_;
        ar& session_id_;
        ar& session_counter_;
    }

private:
    std::string map_id_;
    std::vector<std::shared_ptr<DogRepr>> dogs_;
    std::list<std::shared_ptr<LootRepr>> loot_;
    std::uint32_t session_id_;
    std::uint32_t session_counter_;
};

class PlayerRepr {
public:
    PlayerRepr() = default;

    explicit PlayerRepr(const app::Player& player) 
        : session_id_(player.GetSession()->GetId())
        , dog_id_(player.GetDog()->GetId())
        , token_(*player.GetToken())
        , id_(player.GetId())
        , player_counter_(app::Player::GetPlayerCounter()) {
    }

    [[nodiscard]] app::Player Restore(const model::Game& game) const {

        std::shared_ptr<model::GameSession> session_ptr =
            game.FindSessionById(session_id_);

        std::shared_ptr<model::Dog> dog_ptr = session_ptr->GetDogById(dog_id_);

        app::Player player{session_ptr, dog_ptr};

        player.SetToken(app::Token(token_));
        player.SetId(id_);
        app::Player::SetPlayerCounter(player_counter_);

        return player;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& session_id_;
        ar& dog_id_;
        ar& token_;
        ar& id_;
        ar& player_counter_;
    }

private:
    uint32_t session_id_;
    uint32_t dog_id_;
    std::string token_;
    uint32_t id_;
    std::uint32_t player_counter_;
};

}  // namespace serialization
