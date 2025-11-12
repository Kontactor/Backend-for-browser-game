// application.h
#pragma once

#include "model.h"
#include "tagged.h"

#include <iomanip>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <string>

namespace app {

using namespace std::literals;

namespace detail {

struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    Token GenerateToken() {
        uint64_t num1 = generator1_();
        uint64_t num2 = generator2_();

        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << num1
           << std::setw(16) << num2;

        return Token(ss.str());
    }

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[&] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[&] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

class Player {
public:
    Player(
        std::shared_ptr<model::GameSession> session,
        std::shared_ptr<model::Dog> dog);

    void MakeAction(const std::string move);

    const std::shared_ptr<model::Dog> GetDog() const;

    void SetId(uint32_t id);
    const uint32_t GetId() const;

    const std::string GetName() const;

    const std::shared_ptr<model::GameSession> GetSession() const;

    void SetToken(Token token);
    const Token GetToken() const;

    static std::uint32_t GetPlayerCounter();
    static void SetPlayerCounter(std::uint32_t counter);

private:
    std::shared_ptr<model::GameSession> session_;
    std::shared_ptr<model::Dog> dog_;
    Token token_{"00000000000000000000000000000000"s};
    uint32_t id_;
    static std::uint32_t player_counter_;
};


class Players {
public:
    Players() = delete;

    static std::shared_ptr<Player> AddPlayer(
        std::shared_ptr<model::GameSession> session,
        std::shared_ptr<model::Dog> dog);

    static void AddPlayer(std::shared_ptr<Player> player);

    static std::optional<std::shared_ptr<Player>> FindPlayerByToken(
        const Token& token);

    static std::vector<std::shared_ptr<Player>> FindPlayersInSession(
        const Token& token);

    static std::list<std::shared_ptr<model::Loot>> FindLootInSession(
        const Token& token);

    static std::vector<std::shared_ptr<Player>>& GetPlayers();

    static void RemovePlayerFromGameByDogId(std::uint32_t dogId);

private:
    static std::vector<std::shared_ptr<Player>> players_;
};

class JoinGameUseCase {
public:
    static std::shared_ptr<Player> execute(
        model::Game& game,
        const std::string& user_name,
        const model::Map::Id& map_id);
};

class Application {
public:
    static std::shared_ptr<Player> join_game(
        model::Game& game,
        const std::string& user_name,
        const model::Map::Id& map_id);

private:
    static JoinGameUseCase join_game_use_case;
};

} // namespace app