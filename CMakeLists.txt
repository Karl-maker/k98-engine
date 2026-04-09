cmake_minimum_required(VERSION 3.20)
project(GameEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

# libwebp: decode .webp HDRI / textures (stb_image does not support WebP).
FetchContent_Declare(
  libwebp
  URL https://github.com/webmproject/libwebp/archive/refs/tags/v1.4.0.zip
)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libwebp)

FetchContent_Declare(
  miniaudio
  URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
)
FetchContent_MakeAvailable(miniaudio)

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

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

if(NOT APPLE)
  FetchContent_Declare(
    glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
  )
  FetchContent_GetProperties(glew)
  if(NOT glew_POPULATED)
    FetchContent_Populate(glew)
  endif()
endif()

enable_testing()

find_package(Threads REQUIRED)

include_directories(include)
include_directories(${tinygltf_SOURCE_DIR})

# App
add_executable(game_engine
  src/main.cpp
  src/domain/SuperHero.cpp
  src/graphics/opengl/OpenGLVer2Renderer.cpp
  src/utils/math.cpp
  src/core/ThreadService.cpp
  src/core/GameLoop.cpp
  src/statemachine/StateMachine.cpp
  src/core/assets/importers/GltfModelImporter.cpp
  src/core/assets/importers/AudioImporter.cpp
  src/audio/MiniaudioImpl.cpp
  src/audio/AudioEngine.cpp
  src/audio/AudioTypes.cpp
  src/audio/AudioClipCache.cpp
  src/systems/AudioSystem.cpp
)

target_include_directories(game_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${miniaudio_SOURCE_DIR}
)

# Lets the game find assets/sci-fi/... when the working directory is build/ or arbitrary.
target_compile_definitions(game_engine PRIVATE "GAME_ENGINE_PROJECT_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

if(NOT APPLE)
  target_include_directories(game_engine PRIVATE ${glew_SOURCE_DIR}/include)
  target_sources(game_engine PRIVATE ${glew_SOURCE_DIR}/src/glew.c)
  target_compile_definitions(game_engine PRIVATE GLEW_STATIC)
  find_package(OpenGL REQUIRED)
  target_link_libraries(game_engine PRIVATE Threads::Threads glfw OpenGL::GL webp ${CMAKE_DL_LIBS})
else()
  target_link_libraries(game_engine PRIVATE Threads::Threads glfw webp "-framework OpenGL")
endif()

# Tests
add_executable(unit_tests
  tests/unit/math_test.cpp
  src/utils/math.cpp
)

target_include_directories(unit_tests PRIVATE include)

target_link_libraries(unit_tests GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
