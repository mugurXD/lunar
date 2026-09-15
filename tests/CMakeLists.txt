add_executable(lunar_tests
    "test_pool.cpp"
    "test_component_storage.cpp"
    "test_scene.cpp"
    "test_gameobject.cpp"
    "test_systems.cpp"
    "test_time.cpp"
)

target_link_libraries(lunar_tests PRIVATE lunar GTest::gtest_main)

add_executable(lunar_render_tests
    "test_render_device.cpp"
)

lunar_compile_shaders(lunar_test_shaders
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)

add_dependencies(lunar_render_tests lunar_test_shaders)
target_link_libraries(lunar_render_tests PRIVATE lunar GTest::gtest_main)
target_compile_definitions(lunar_render_tests PRIVATE LUNAR_TEST_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders")

include(GoogleTest)
gtest_discover_tests(lunar_tests)
gtest_discover_tests(lunar_render_tests PROPERTIES LABELS "gpu" ENVIRONMENT "VK_KHRONOS_VALIDATION_VALIDATE_SYNC=1")
