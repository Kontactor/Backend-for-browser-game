cmake_minimum_required(VERSION 3.11)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# обратите внимание на аргумент TARGETS у команды conan_basic_setup
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_server_lib STATIC
	src/application.h
	src/application.cpp
	src/collision_detector.h
	src/collision_detector.cpp
	src/geom.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/model.h
	src/model.cpp
	src/model_serialization.h
	src/tagged.h
	src/tagged_uuid.cpp
	src/tagged_uuid.h
	src/database.cpp
	src/database.h
	)

# используем "импортированную" цель CONAN_PKG::boost
target_include_directories(game_server_lib PUBLIC CONAN_PKG::boost)
target_link_libraries(game_server_lib PUBLIC CONAN_PKG::boost CONAN_PKG::libpqxx)

add_executable(game_server
	src/main.cpp
	src/api_handler.h
	src/api_handler.cpp
	src/boost_json.cpp
	src/extra_data.h
	src/extra_data.cpp
	src/handlers_utils.h
	src/handlers_utils.cpp
	src/http_server.h
	src/http_server.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/my_logger.h
	src/my_logger.cpp
	src/request_handler.h
	src/request_handler.cpp
	src/sdk.h
)
target_link_libraries(game_server game_server_lib)

add_executable(game_server_tests
	tests/collision-detector-tests.cpp
	tests/loot_generator_tests.cpp
	tests/state-serialization-tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 game_server_lib)