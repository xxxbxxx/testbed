project(testbed-openal)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)
aux_source_directory(imgui SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

SET(CMAKE_MODULE_PATH "${testbed-openal_SOURCE_DIR}")
FIND_PACKAGE(SDL2)

INCLUDE_DIRECTORIES(imgui ${SDL2_INCLUDE_DIR})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${SDL2_LIBRARY} GL)

