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
# App
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(museum ${APP_SOURCES})
target_include_directories(museum PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(museum PRIVATE glad glfw glm::glm imgui stb)

# Shaders/assets are read straight from the source tree, so edits need no rebuild
# and the exe runs from any working directory (VS, Xcode, terminal).
target_compile_definitions(museum PRIVATE
    RTM_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
    RTM_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/"
    GLM_FORCE_SILENT_WARNINGS)

if(APPLE)
    target_compile_definitions(museum PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(museum PRIVATE "-framework OpenGL")
endif()

if(MSVC)
    target_compile_options(museum PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(museum PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT museum)
    set_property(TARGET museum PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    target_compile_options(museum PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
