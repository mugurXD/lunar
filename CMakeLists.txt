cmake_minimum_required(VERSION 3.29 FATAL_ERROR)
project(lunar VERSION 0.2.0)

message("-- lunar: Version ${lunar_VERSION_MAJOR}.${lunar_VERSION_MINOR}.${lunar_VERSION_PATCH}")

set(LUNAR_RENDER_BACKEND "OPENGL")
set(LUNAR_BUILD_EXAMPLE ON)
set(LUNAR_UNIT_TESTS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(LUNAR_ADD_INSTALL_PATHS)
    list(APPEND CMAKE_PREFIX_PATH 
        "deps/glm/install"
        "deps/glfw/install"
        "deps/fastgltf/install"
        "deps/nlohmann-json/install"
        "deps/reactphysics3d/install"
    )
endif()

find_package(ReactPhysics3D REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)

file(GLOB_RECURSE LUNAR_HEADERS "inc/lunar/*.hpp")
file(GLOB_RECURSE LUNAR_SRC 
    "src/core/*.cpp"
    "src/debug/*.cpp"
    "src/utils/*.cpp"
    "src/file/*.cpp"
    "src/exp/*.cpp"
)

message(${LUNAR_HEADERS})
message(${LUNAR_SRC})

add_library(lunar STATIC)
target_sources(lunar PRIVATE 
    ${LUNAR_SRC} 
    ${LUNAR_HEADERS}
    "src/render/context.cpp"
    "src/render/mesh.cpp"
    "src/render/mesh_builder.cpp"
    "src/render/texture_builder.cpp"
    "src/render/program.cpp"
    "src/render/components.cpp"
    "src/render/window.cpp"
    "src/stb_image.cpp"
)

target_compile_definitions(lunar PUBLIC LUNAR_EXPERIMENTAL) # will be removed 

if(ON)
    message("-- lunar: Using imgui library")
    add_library(imgui STATIC)
    target_sources(imgui PRIVATE
        "deps/imgui/imgui.h"
        "deps/imgui/imgui.cpp"
        "deps/imgui/imgui_demo.cpp"
        "deps/imgui/imgui_draw.cpp"
        "deps/imgui/imgui_widgets.cpp"
        "deps/imgui/imgui_tables.cpp"
        "deps/imgui/backends/imgui_impl_glfw.cpp"
        "deps/imgui/backends/imgui_impl_glfw.h"
    )
    target_include_directories(imgui PUBLIC "deps/imgui" "deps/imgui/backends")
    target_link_libraries(imgui PUBLIC glfw)
    target_include_directories(imgui PUBLIC glfw)

    if(${LUNAR_RENDER_BACKEND} STREQUAL "VULKAN")
        target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
        target_include_directories(imgui PUBLIC Vulkan::Vulkan)
        target_sources(imgui PRIVATE
            "deps/imgui/backends/imgui_impl_vulkan.h"
            "deps/imgui/backends/imgui_impl_vulkan.cpp"
        )
    endif()

    if(${LUNAR_RENDER_BACKEND} STREQUAL "OPENGL")
        target_sources(imgui PRIVATE
            "deps/imgui/backends/imgui_impl_opengl3.h"
            "deps/imgui/backends/imgui_impl_opengl3.cpp"
        )
    endif()
    
    target_link_libraries(lunar PUBLIC imgui)
    target_compile_definitions(lunar PUBLIC LUNAR_IMGUI=1)
endif()

if(${LUNAR_RENDER_BACKEND} STREQUAL "OPENGL")
    add_subdirectory(glad)
    target_link_libraries(lunar PUBLIC glad)

    message("-- lunar: Using OpenGL backend")
    target_sources(lunar PRIVATE
        "inc/lunar/render/imp/gl/base.hpp"
        "inc/lunar/render/imp/gl/buffer.hpp"
        "inc/lunar/render/imp/gl/program.hpp"
        "inc/lunar/render/imp/gl/texture.hpp"
        "src/render/gl/buffer.cpp"
        "src/render/gl/program.cpp"
        "src/render/gl/texture.cpp"
        "src/render/gl/cubemap.cpp"
        "src/render/gl/init.cpp"
        "src/render/gl/draw.cpp"
    )

    target_compile_definitions(lunar PUBLIC GLFW_INCLUDE_NONE)
    target_compile_definitions(lunar PUBLIC LUNAR_OPENGL)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/stb_image.h")
    message("-- lunar: Downloading 'stb_image.h' file")
    file(
        DOWNLOAD 
        https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h 
        "${CMAKE_CURRENT_LIST_DIR}/stb_image.h"
    )
else()
    message("-- lunar: Found 'stb_image' header")
endif()

target_sources(lunar PRIVATE "stb_image.h")

target_include_directories(lunar PUBLIC inc)
target_link_libraries(lunar PUBLIC 
    nlohmann_json::nlohmann_json
    glm::glm 
    glfw
    fastgltf::fastgltf 
    ReactPhysics3D::ReactPhysics3D
)

target_compile_definitions(lunar PRIVATE LUNAR_LIBRARY_EXPORT)
target_compile_definitions(lunar PUBLIC 
    LUNAR_RENDER_BACKEND="${LUNAR_RENDER_BACKEND}"
    LUNAR_VER_MAJOR=${lunar_VERSION_MAJOR}
    LUNAR_VER_MINOR=${lunar_VERSION_MINOR}
    LUNAR_VER_PATCH=${lunar_VERSION_PATCH}
)

if(MSVC)
    target_compile_options(lunar PUBLIC /Zc:preprocessor)
    message("-- lunar: Detected MSVC compiler")
endif()

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
    message("-- lunar: Found MinGW build environment.")
endif()

if(${LUNAR_BUILD_EXAMPLE})
    message("-- lunar: Building example application")
    add_executable(lunar_example "examples/app.cpp")
    target_link_libraries(lunar_example PUBLIC lunar)
    target_include_directories(lunar_example PUBLIC lunar)
endif()

if(${LUNAR_UNIT_TESTS})
    message("-- lunar: Building unit tests")

    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(tests)
endif()