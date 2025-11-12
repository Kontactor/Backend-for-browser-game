// main.cpp
#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "database.h"
#include "json_loader.h"
#include "my_logger.h"
#include "request_handler.h"

using namespace std::literals;
using namespace boost::json;
using namespace boost::asio::ip;

namespace beast = boost::beast;
namespace net = boost::asio;
namespace sys = boost::system;
namespace logging = boost::log;

const static size_t DEFAULT_POOL_SIZE = 1;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);

    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this(), this] {
            last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

struct Args {
    int64_t tick_period;
    std::string config_file_path;
    std::string www_root_path;
    bool randomize_spawn_points;
    bool game_test_mode = false;
    std::string state_file_path;
    int64_t save_state_period;
    bool save_state_period_set = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(
    int argc,
    const char* const argv[])
{
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t",
            po::value<int64_t>(&args.tick_period)->value_name("milliseconds"s),
            "set tick period")
        ("config-file,c",
            po::value(&args.config_file_path)->value_name("file"s),
            "set config file path")
        ("www-root,w",
            po::value(&args.www_root_path)->value_name("file"s),
            "set static files root")
        ("randomize-spawn-points",
            "spawn dogs at random positions")
        ("state-file,s",
            po::value(&args.state_file_path)->value_name("file"s),
            "set state file root")
        ("save-state-period",
            po::value(&args.save_state_period)->value_name("milliseconds"s),
            "set save state period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file is not specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Source folder path is not specified"s);
    }

    if (vm.contains("state-file"s) && vm.contains("save-state-period"s)) {
        args.save_state_period_set = true;
    }
    
    if (vm.count("tick-period"s) == false) {
        args.game_test_mode = true;
    }

    if (vm.count("randomize-spawn-points"s) == false) {
        args.randomize_spawn_points = false;
    } else {
        args.randomize_spawn_points = true;
    }

    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    Args args;
    try {
        if (ParseCommandLine(argc, argv).has_value()) {
            args = ParseCommandLine(argc, argv).value();
        } else {
            return 0;
        }
    }
    catch (const std::exception& ec) {
        std::cerr << ec.what() << std::endl;
        return EXIT_FAILURE;
    }

    const char* db_url = std::getenv("GAME_DB_URL");
    if (!db_url) {
        std::cerr << "GAME_DB_URL environment variable is not set" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        my_logger::InitBoostLogFilter();
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args.config_file_path);

        if (!args.state_file_path.empty()) {
            game.SetSaveFilePath(args.state_file_path);

            if (std::filesystem::exists(
                std::filesystem::path(args.state_file_path)))
            {
                try {
                    game.LoadState();
                }
                catch (const std::exception& ex) {
                    std::cerr << ex.what() << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }

        game.SetDogSpawnMode(args.randomize_spawn_points);
        game.SetGameMode(args.game_test_mode);

        if (args.save_state_period_set) {
            game.SetSavePeriod(args.save_state_period);
        }

        auto conn_pool = std::make_shared<database::ConnectionPool>(DEFAULT_POOL_SIZE, [db_url] {
            return std::make_shared<pqxx::connection>(db_url);
        });        
        try{
            database::Database::Initialize(conn_pool);
            game.SetDBConnectionPool(conn_pool);
        }
        catch (std::exception& e) {
            std::cerr << "postgres::Database::Init exception: "sv << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {
                    ioc.stop();

                    value custom_data{{"code"s, 0}};
                    BOOST_LOG_TRIVIAL(info)
                        << logging::add_value(my_logger::additional_data,
                                              custom_data)
                        << "server exited"sv;
                }
            }
        );

        // 4. Создаём обработчик HTTP-запросов, связываем его с моделью игры и
        // указываем корневой каталог
        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, api_strand, args.www_root_path
        );
        http_handler::LoggingRequestHandler logging_handler(*handler);

        if (!args.game_test_mode) {
            game.SetStartTime(std::chrono::steady_clock::now());
            auto ticker = std::make_shared<Ticker>(api_strand, std::chrono::milliseconds(args.tick_period),
                [&game](std::chrono::milliseconds delta) {
                    game.Update(delta.count());
                }
            );
            ticker->Start();
        }

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(
            ioc,
            {address, port},
            [&logging_handler](auto&& req, const auto& remote_endpoint,
                               auto&& send) {
                logging_handler(
                    std::forward<decltype(req)>(req),
                    remote_endpoint,
                    std::forward<decltype(send)>(send)
                );
            }
        );

        // Эта надпись сообщает тестам о том, что сервер запущен и
        // готов обрабатывать запросы
        value custom_data{{"port"s, port}, {"address", address.to_string()}};
        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(my_logger::additional_data, custom_data)
            << "server started"sv;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        // 7. Сохраняем состояние игры
        if (!args.state_file_path.empty()) {
            game.SaveState();
        }

    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
