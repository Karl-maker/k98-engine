cmake_minimum_required(VERSION 3.20)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG v2.9.3
)
FetchContent_MakeAvailable(tinygltf)

enable_testing()

find_package(Threads REQUIRED)

include_directories(include)
include_directories(${tinygltf_SOURCE_DIR})

# App
add_executable(game_engine
  src/main.cpp
  src/utils/math.cpp
  src/core/GameLoop.cpp
  src/statemachine/StateMachine.cpp
  src/core/assets/importers/GltfModelImporter.cpp
)

target_include_directories(game_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game_engine PRIVATE Threads::Threads)

# Tests
add_executable(unit_tests
  tests/unit/math_test.cpp
  src/utils/math.cpp
)

target_include_directories(unit_tests PRIVATE include)

target_link_libraries(unit_tests GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
