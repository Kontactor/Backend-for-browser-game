// application.cpp
#include "application.h"

namespace app {

constexpr char KEY_U[] = "U";
constexpr char KEY_D[] = "D";
constexpr char KEY_L[] = "L";
constexpr char KEY_R[] = "R";

std::uint32_t Player::player_counter_{0};
std::vector<std::shared_ptr<Player>> Players::players_{};

Player::Player(
    std::shared_ptr<model::GameSession> session,
    std::shared_ptr<model::Dog> dog)
    : dog_(dog)
    , session_(session)
    , token_(PlayerTokens().GenerateToken())
    , id_(player_counter_++) {
}

void Player::MakeAction(const std::string move) {
    const auto speed = session_->GetMap()->GetDogSpeed();

    if (move == KEY_U) {
            dog_->SetSpeed({0, -speed});
            dog_->SetDirection(model::DIRECTION::NORTH);
    } else if (move == KEY_D) {
            dog_->SetSpeed({0, speed});
            dog_->SetDirection(model::DIRECTION::SOUTH);
    } else if (move == KEY_L) {
            dog_->SetSpeed({-speed, 0});
            dog_->SetDirection(model::DIRECTION::WEST);
    } else if (move == KEY_R) {
            dog_->SetSpeed({speed, 0});
            dog_->SetDirection(model::DIRECTION::EAST);
    } else {
            dog_->SetSpeed({0, 0});
            dog_->SetDirection(model::DIRECTION::NONE);
    }
    dog_->SetStatus(model::Dog::DOG_STATUS::ACTIVE);
    dog_->ResetInactivityTimer();
}

const std::shared_ptr<model::Dog> Player::GetDog() const {
    return dog_;
}

void Player::SetId(uint32_t id) {
    id_ = id;
}

const uint32_t Player::GetId() const {
    return id_;
}

const std::string Player::GetName() const {
    return dog_->GetName();
}

const std::shared_ptr<model::GameSession> Player::GetSession() const {
    return session_;
}

void Player::SetToken(Token token) {
    token_ = token;
}

const Token Player::GetToken() const {
    return token_;
}

std::uint32_t Player::GetPlayerCounter() {
    return player_counter_;
}

void Player::SetPlayerCounter(std::uint32_t counter) {
    player_counter_ = counter;
}

std::shared_ptr<Player> Players::AddPlayer(
    std::shared_ptr<model::GameSession> session,
    std::shared_ptr<model::Dog> dog)
{   
    auto new_player_ptr = std::make_shared<Player>(session, dog);

    players_.push_back(new_player_ptr);

    return players_.back();
}

void Players::AddPlayer(std::shared_ptr<Player> player) {
    players_.push_back(player);
}

std::optional<std::shared_ptr<Player>> Players::FindPlayerByToken(
    const Token& token)
{
    for (auto& player : players_) {
        if (player->GetToken() == token) {
            return player;
        }
    }
    return std::nullopt;
}

std::vector<std::shared_ptr<Player>> Players::FindPlayersInSession(
    const Token& token)
{
    std::vector<std::shared_ptr<Player>> players_in_session;

    if (FindPlayerByToken(token)) {
        auto session = FindPlayerByToken(token).value()->GetSession();
        for (auto& player : players_) {
            if (player->GetSession() == session) {
                players_in_session.push_back(player);
            }
        }
    }
    return players_in_session;
}

std::list<std::shared_ptr<model::Loot>> Players::FindLootInSession(
        const Token& token) 
{
    if (!FindPlayerByToken(token)) {
        return {};
    }
    auto session = FindPlayerByToken(token).value()->GetSession();
    return session->GetLoot();
}

std::vector<std::shared_ptr<Player>>& Players::GetPlayers() {
    return players_;
}

void Players::RemovePlayerFromGameByDogId(std::uint32_t dog_id) {
    auto it = std::find_if(players_.begin(), players_.end(),
        [dog_id](const auto& player) {
            return player->GetDog()->GetId() == dog_id;
        }
    );

    if (it != players_.end()) {
        (*it)->GetSession()->RemoveDog(dog_id);
        players_.erase(it);
    }
}

std::shared_ptr<Player> JoinGameUseCase::execute(
    model::Game& game,
    const std::string& user_name,
    const model::Map::Id& map_id)
{
    const auto& map = game.FindMap(map_id);
    geom::Point2D position;

    if (game.GetDogSpawnMode() == model::Game::SPAWN_MODE::RANDOM) {
        model::Point point = map->GetRandomPointOnRoad();
        position = {point.x * 1.0, point.y * 1.0};
    } else {
        const auto& roads = map->GetRoads();
        const auto& coord = roads[0].GetStart();
        position = {coord.x * 1.0, coord.y * 1.0};
    }
    
    std::shared_ptr<model::Dog> new_dog_ptr =
        std::make_shared<model::Dog>(user_name, position);

    new_dog_ptr->SetJoinTime(game.GetCurrentTime());

    std::shared_ptr<model::GameSession> session_ptr =
        game.AddDogToSession(new_dog_ptr, map_id);
    
    return Players::AddPlayer(session_ptr, new_dog_ptr);
}

std::shared_ptr<Player> Application::join_game(
    model::Game& game,
    const std::string& user_name,
    const model::Map::Id& map_id)
{
    return join_game_use_case.execute(game, user_name, map_id);
}

} // namespace app