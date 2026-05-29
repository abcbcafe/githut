cmake_minimum_required(VERSION 3.24)

project(pathtracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Output: the shared library is dropped where the .gdextension descriptor
# expects it (game/addons/pathtracer/bin/).
# ---------------------------------------------------------------------------
set(GAME_ADDON_BIN "${CMAKE_SOURCE_DIR}/../game/addons/pathtracer/bin")

# ---------------------------------------------------------------------------
# godot-cpp (git submodule, checked out on the 4.6 branch).
# ---------------------------------------------------------------------------
set(GODOT_CPP_DIR "${CMAKE_SOURCE_DIR}/thirdparty/godot-cpp")
if(NOT EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "godot-cpp not found at ${GODOT_CPP_DIR}.\n"
        "Run: git submodule update --init --recursive")
endif()
add_subdirectory("${GODOT_CPP_DIR}" godot-cpp)

# ---------------------------------------------------------------------------
# Vulkan (ray tracing). Required on the build/run workstation.
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# VMA is header-only; added as a submodule.
set(VMA_DIR "${CMAKE_SOURCE_DIR}/thirdparty/VulkanMemoryAllocator")

# ---------------------------------------------------------------------------
# Extension library.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PATHTRACER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(pathtracer SHARED ${PATHTRACER_SOURCES})

target_include_directories(pathtracer PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${VMA_DIR}/include")

target_link_libraries(pathtracer PRIVATE
    godot-cpp
    Vulkan::Vulkan)

# Enable validation layers + verbose logging in debug builds.
target_compile_definitions(pathtracer PRIVATE
    $<$<CONFIG:Debug>:PATHTRACER_DEBUG=1>)

set_target_properties(pathtracer PROPERTIES
    PREFIX "lib"
    LIBRARY_OUTPUT_DIRECTORY "${GAME_ADDON_BIN}")

# Copy the freshly built library into the addon bin dir (covers multi-config).
add_custom_command(TARGET pathtracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ADDON_BIN}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:pathtracer>" "${GAME_ADDON_BIN}/"
    COMMENT "Copying pathtracer extension into game/addons/pathtracer/bin")
