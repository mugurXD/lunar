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
    "inc/trok/world/biome.hpp"
    "inc/trok/world/climate.hpp"
    "inc/trok/world/region_plan.hpp"
    "inc/trok/world/stable_hash.hpp"
    "inc/trok/world/terrain_generator.hpp"
    "src/world/biome.cpp"
    "src/world/climate.cpp"
    "src/world/region_plan.cpp"
    "src/world/terrain_generator.cpp"
)

target_include_directories(trok_core PUBLIC inc)
target_link_libraries(trok_core PUBLIC lunar)

add_executable(trok "src/main.cpp")
target_link_libraries(trok PRIVATE trok_core)

if(${LUNAR_UNIT_TESTS})
    add_executable(trok_tests
        "tests/test_biome.cpp"
        "tests/test_climate.cpp"
        "tests/test_region_plan.cpp"
        "tests/test_terrain_generator.cpp"
    )

    target_link_libraries(trok_tests PRIVATE trok_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(trok_tests)
endif()
