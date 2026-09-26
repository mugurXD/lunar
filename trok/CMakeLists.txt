if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite.h")
    message("-- trok: Downloading 'FastNoiseLite.h' file")
    file(
        DOWNLOAD 
        https://raw.githubusercontent.com/Auburn/FastNoiseLite/refs/heads/master/Cpp/FastNoiseLite.h
        "${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite.h"
    )
else()
    message("-- trok: Found 'FastNoiseLite' header")
endif()

add_library(trok_core STATIC
    "FastNoiseLite.h"
    "inc/trok/gameplay/deliveries.hpp"
    "inc/trok/gameplay/delivery_beacon.hpp"
    "inc/trok/json_math.hpp"
    "inc/trok/vehicle/chase_camera.hpp"
    "inc/trok/vehicle/truck.hpp"
    "inc/trok/vehicle/truck_tuning.hpp"
    "inc/trok/world/biome.hpp"
    "inc/trok/world/biome_tuning.hpp"
    "inc/trok/world/climate.hpp"
    "inc/trok/world/dressers.hpp"
    "inc/trok/world/elevation.hpp"
    "inc/trok/world/region_plan.hpp"
    "inc/trok/world/river.hpp"
    "inc/trok/world/road.hpp"
    "inc/trok/world/shapers.hpp"
    "inc/trok/world/road_planner.hpp"
    "inc/trok/world/road_service.hpp"
    "inc/trok/world/stable_hash.hpp"
    "inc/trok/world/terrain_generator.hpp"
    "inc/trok/world/towns.hpp"
    "inc/trok/world/world_map.hpp"
    "src/gameplay/deliveries.cpp"
    "src/gameplay/delivery_beacon.cpp"
    "src/json_math.cpp"
    "src/vehicle/chase_camera.cpp"
    "src/vehicle/truck.cpp"
    "src/vehicle/truck_tuning.cpp"
    "src/world/biome.cpp"
    "src/world/biome_tuning.cpp"
    "src/world/climate.cpp"
    "src/world/dressers.cpp"
    "src/world/elevation.cpp"
    "src/world/region_plan.cpp"
    "src/world/river.cpp"
    "src/world/road.cpp"
    "src/world/shapers.cpp"
    "src/world/road_planner.cpp"
    "src/world/road_service.cpp"
    "src/world/terrain_generator.cpp"
    "src/world/towns.cpp"
    "src/world/world_map.cpp"
)

target_include_directories(trok_core PUBLIC inc)
target_link_libraries(trok_core PUBLIC lunar)

add_executable(trok "src/main.cpp")
target_link_libraries(trok PRIVATE trok_core)

if(${LUNAR_UNIT_TESTS})
    add_executable(trok_tests
        "tests/test_biome.cpp"
        "tests/test_chase_camera.cpp"
        "tests/test_climate.cpp"
        "tests/test_deliveries.cpp"
        "tests/test_elevation.cpp"
        "tests/test_region_plan.cpp"
        "tests/test_road.cpp"
        "tests/test_terrain_generator.cpp"
        "tests/test_truck.cpp"
    )

    target_link_libraries(trok_tests PRIVATE trok_core GTest::gtest_main)
    target_compile_definitions(trok_tests PRIVATE TROK_DATA_DIRECTORY="${CMAKE_SOURCE_DIR}/resources/data")

    include(GoogleTest)
    gtest_discover_tests(trok_tests)
endif()
