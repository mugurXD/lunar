add_executable(lunar_tests
    "test_pool.cpp"
    "test_component_storage.cpp"
    "test_scene.cpp"
    "test_gameobject.cpp"
    "test_systems.cpp"
    "test_time.cpp"
)

target_link_libraries(lunar_tests PRIVATE lunar GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lunar_tests)
