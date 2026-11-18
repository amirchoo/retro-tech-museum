cmake_minimum_required(VERSION 3.24)
project(RetroTechMuseum LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies: downloaded at configure time, pinned versions.
# Nobody installs GLFW/GLM/ImGui by hand -- only CMake + a compiler are needed.
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.5.1
    GIT_SHALLOW    TRUE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    GIT_SHALLOW    TRUE)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
    GIT_SHALLOW    TRUE)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        d14d46f6b44456eb88beac58cba833a4135d4c89)

# imgui and stb have no CMakeLists, so MakeAvailable just downloads them.
FetchContent_MakeAvailable(glfw glm imgui stb)

# GLAD: generated for OpenGL 4.1 core (the macOS ceiling) and committed.
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# ImGui (GLFW + OpenGL3 backends)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# stb (header-only; implementation lives in src/core/stb_impl.cpp)
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# ---------------------------------------------------------------------------
# rtm: everything except main.cpp, so tests can link the same code.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE RTM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h)
list(REMOVE_ITEM RTM_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

add_library(rtm STATIC ${RTM_SOURCES})
target_include_directories(rtm PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rtm PUBLIC glad glfw glm::glm imgui stb)

# Shaders/assets are read straight from the source tree, so edits need no rebuild
# and the exe runs from any working directory (VS, Xcode, terminal).
target_compile_definitions(rtm PUBLIC
    RTM_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
    RTM_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/"
    GLM_FORCE_SILENT_WARNINGS)

if(APPLE)
    target_compile_definitions(rtm PUBLIC GL_SILENCE_DEPRECATION)
    target_link_libraries(rtm PUBLIC "-framework OpenGL")
endif()
if(MSVC)
    target_compile_definitions(rtm PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

function(rtm_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endfunction()
rtm_warnings(rtm)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
add_executable(museum src/main.cpp)
target_link_libraries(museum PRIVATE rtm)
rtm_warnings(museum)
if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT museum)
    set_property(TARGET museum PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
# Tests (CPU only, no GPU/window needed -> run in CI):  ctest --preset <mac|windows>
# ---------------------------------------------------------------------------
enable_testing()
add_executable(rtm_tests tests/test_main.cpp tests/test_gpu_layout.cpp)
target_link_libraries(rtm_tests PRIVATE rtm)
rtm_warnings(rtm_tests)
add_test(NAME rtm_tests COMMAND rtm_tests)

# Optional integration tests need a working OpenGL 4.1 display/context.
option(RTM_BUILD_GPU_TESTS "Build path tracer OpenGL integration tests" OFF)
if(RTM_BUILD_GPU_TESTS)
    add_executable(rtm_tracer_tests tests/test_path_tracer_gl.cpp)
    target_link_libraries(rtm_tracer_tests PRIVATE rtm)
    rtm_warnings(rtm_tracer_tests)
    add_test(NAME rtm_tracer_tests COMMAND rtm_tracer_tests)
    set_tests_properties(rtm_tracer_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
