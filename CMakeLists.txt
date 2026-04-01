cmake_minimum_required(VERSION 3.20)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

include_directories(include)

# App
add_executable(game_engine
  src/main.cpp
  src/utils/math.cpp
  src/core/GameLoop.cpp
  src/game/DemoGame.cpp
)

# Tests
add_executable(unit_tests
  tests/unit/math_test.cpp
  src/utils/math.cpp
)

target_include_directories(unit_tests PRIVATE include)

target_link_libraries(unit_tests GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)