cmake_minimum_required(VERSION 3.10)
project(game_of_life)

find_package(raylib CONFIG REQUIRED)

file(GLOB SRC_FILES src/*.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
