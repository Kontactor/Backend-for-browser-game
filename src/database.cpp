// database.cpp
#include "database.h"
#include <stdexcept>


namespace database {

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection() {
    std::unique_lock lock{mutex_};
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });

    return {std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
    {
        std::lock_guard lock{mutex_};
        assert(used_connections_ != 0);
        pool_[--used_connections_] = std::move(conn);
    }
    cond_var_.notify_one();
}

void Database::Initialize(std::shared_ptr<ConnectionPool> pool) {
    auto connection_ = pool->GetConnection();
    pqxx::work work{*connection_};
    work.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players  (
            id UUID PRIMARY KEY,
            name VARCHAR(100),
            score int,
            play_time_ms int
        )
    )"_zv);
    work.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_retired_players_score_playtime_name
        ON retired_players (
            score DESC, play_time_ms ASC, name ASC
        )
    )"_zv);
    work.commit();
}

std::vector<PlayerRecord> Database::GetPlayersRecords(
    std::shared_ptr<ConnectionPool> pool, int start, int max_items) {
    
    std::vector<PlayerRecord> result;
    auto connection_ = pool->GetConnection();
    pqxx::work work{*connection_};

    auto query_text = 
        "SELECT id, name, score, play_time_ms FROM retired_players "
        "ORDER BY score desc, play_time_ms, name OFFSET "s;
    query_text += std::to_string(start);
    query_text += " LIMIT ";
    query_text += std::to_string(max_items);
    query_text += ";";
    pqxx::zview query_zv_(query_text);

    for (auto& [id_uuid, name, score, play_time_ms] : 
        work.query<std::string, std::string, int, int>(query_zv_)) {
        result.push_back({
            id_uuid,
            name,
            static_cast<std::uint32_t>(score),
            static_cast<std::uint64_t>(play_time_ms)
        });
    }
    return result;
}

void Database::SaveRecord(std::shared_ptr<ConnectionPool> pool, 
                         PlayerRecord record) {
    auto connection_ = pool->GetConnection();
    pqxx::work work{*connection_};
    work.exec_params(
        R"(INSERT INTO retired_players (id, name, score, play_time_ms)
        VALUES($1, $2, $3, $4) 
        ON CONFLICT (id) 
        DO UPDATE SET name = excluded.name, score = excluded.score, 
                      play_time_ms = excluded.play_time_ms;)"_zv,
        record.id_uuid, record.name, record.score, record.play_time_ms);
    work.commit();
}

} // namespace database