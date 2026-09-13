cmake_minimum_required(VERSION 3.14)
project(Oxygen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SYSTEM_VERSION "10.0.19041.0")

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

include(FetchContent)

FetchContent_Declare(
  minhook
  GIT_REPOSITORY "https://github.com/TsudaKageyu/minhook.git"
  GIT_TAG        "master"
)
FetchContent_MakeAvailable(minhook)

FetchContent_Declare(
  safetyhook
  GIT_REPOSITORY "https://github.com/cursey/safetyhook.git"
  GIT_TAG        "origin/main"
  GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(safetyhook)

FetchContent_Declare(
  libhat
  GIT_REPOSITORY https://github.com/BasedInc/libhat.git
  GIT_TAG        v0.9.0
)
FetchContent_MakeAvailable(libhat)

FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.14.0
)
FetchContent_MakeAvailable(entt)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui)

set(FONT_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FONT_GEN_DIR}")

file(GLOB FONT_FILES "resources/fonts/*.ttf")
list(LENGTH FONT_FILES FONT_COUNT)

set(FONT_RC "${FONT_GEN_DIR}/fonts.rc")
file(WRITE "${FONT_RC}" "// Auto-generated font resource file\n")

set(FONT_H "${FONT_GEN_DIR}/font_resources.h")
file(WRITE "${FONT_H}" "#pragma once\n\nnamespace FontData {\n    struct FontEntry {\n        const char* name;\n        const char* resName;\n    };\n    inline constexpr FontEntry g_fonts[] = {\n")

foreach(f ${FONT_FILES})
    get_filename_component(basename ${f} NAME_WE)
    string(TOUPPER ${basename} res_name)
    string(REPLACE "-" "_" res_name "${res_name}")
    string(REPLACE "\\" "/" posix_path "${f}")
    file(APPEND "${FONT_RC}" "${res_name} RCDATA \"${posix_path}\"\n")
    file(APPEND "${FONT_H}" "        {\"${basename}\", \"${res_name}\"},\n")
endforeach()

file(APPEND "${FONT_H}" "    };\n    inline constexpr int g_fontCount = ${FONT_COUNT};\n}\n")

set(ASSET_GEN_DIR "${CMAKE_BINARY_DIR}/generated")

file(GLOB ASSET_FILES "resources/assets/*.png")
list(LENGTH ASSET_FILES ASSET_COUNT)

set(ASSET_RC "${ASSET_GEN_DIR}/assets.rc")
file(WRITE "${ASSET_RC}" "// Auto-generated asset resource file\n")

set(ASSET_H "${ASSET_GEN_DIR}/asset_resources.h")
file(WRITE "${ASSET_H}" "#pragma once\n\nnamespace AssetData {\n    struct AssetEntry {\n        const char* fileName;\n        const char* resName;\n    };\n    inline constexpr AssetEntry g_assets[] = {\n")

foreach(a ${ASSET_FILES})
    get_filename_component(basename ${a} NAME)
    get_filename_component(basewe ${a} NAME_WE)
    string(TOUPPER ${basewe} upper_basewe)
    string(REPLACE "\\" "/" posix_path "${a}")
    file(APPEND "${ASSET_RC}" "${upper_basewe}_PNG RCDATA \"${posix_path}\"\n")
    file(APPEND "${ASSET_H}" "        {\"${basename}\", \"${upper_basewe}_PNG\"},\n")
endforeach()

file(APPEND "${ASSET_H}" "    };\n    inline constexpr int g_assetCount = ${ASSET_COUNT};\n}\n")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "resources/fonts/*.ttf")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "resources/assets/*.png")

file(GLOB_RECURSE OXYGEN_SOURCES
    src/*.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/visual/ClickGui.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/visual/ClickGui.cpp)
    message(STATUS "Added ClickGui.cpp explicitly")
else()
    message(WARNING "ClickGui.cpp not found at src/features/modules/visual/ClickGui.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/Test.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/Test.cpp)
    message(STATUS "Added Test.cpp explicitly")
else()
    message(WARNING "Test.cpp not found at src/features/modules/misc/Test.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/BrewTimer.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/BrewTimer.cpp)
    message(STATUS "Added BrewTimer.cpp explicitly")
else()
    message(WARNING "BrewTimer.cpp not found at src/features/modules/misc/BrewTimer.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/visual/NameProtect.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/visual/NameProtect.cpp)
    message(STATUS "Added NameProtect.cpp explicitly")
else()
    message(WARNING "NameProtect.cpp not found at src/features/modules/visual/NameProtect.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/Interface.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/Interface.cpp)
    message(STATUS "Added Interface.cpp explicitly")
else()
    message(WARNING "Interface.cpp not found at src/features/modules/misc/Interface.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/Hitbox.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/Hitbox.cpp)
    message(STATUS "Added Hitbox.cpp explicitly")
else()
    message(WARNING "Hitbox.cpp not found at src/features/modules/misc/Hitbox.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/Blur.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/Blur.cpp)
    message(STATUS "Added Blur.cpp explicitly")
else()
    message(WARNING "Blur.cpp not found at src/features/modules/misc/Blur.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks/background_blur.cpp")
    list(APPEND OXYGEN_SOURCES src/hooks/background_blur.cpp)
    message(STATUS "Added background_blur.cpp explicitly")
else()
    message(WARNING "background_blur.cpp not found at src/hooks/background_blur.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/movement/Sprint.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/movement/Sprint.cpp)
    message(STATUS "Added Sprint.cpp explicitly")
else()
    message(WARNING "Sprint.cpp not found at src/features/modules/movement/Sprint.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/visual/ArrayList.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/visual/ArrayList.cpp)
    message(STATUS "Added ArrayList.cpp explicitly")
else()
    message(WARNING "ArrayList.cpp not found at src/features/modules/visual/ArrayList.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/visual/ESP.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/visual/ESP.cpp)
    message(STATUS "Added ESP.cpp explicitly")
else()
    message(WARNING "ESP.cpp not found at src/features/modules/visual/ESP.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/visual/Notifications.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/visual/Notifications.cpp)
    message(STATUS "Added Notifications.cpp explicitly")
else()
    message(WARNING "Notifications.cpp not found at src/features/modules/visual/Notifications.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/features/modules/misc/Radio.cpp")
    list(APPEND OXYGEN_SOURCES src/features/modules/misc/Radio.cpp)
    message(STATUS "Added Radio.cpp explicitly")
else()
    message(WARNING "Radio.cpp not found at src/features/modules/misc/Radio.cpp")
endif()

add_library(${PROJECT_NAME} SHARED
    ${OXYGEN_SOURCES}
    "${FONT_RC}"
    "${ASSET_RC}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Oxygen")

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${minhook_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}
    ${entt_SOURCE_DIR}/src
    "${FONT_GEN_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    safetyhook::safetyhook
    minhook
    libhat::libhat
    d3d12
    dxgi
    windowscodecs
    version
    winmm
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WINDLL
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)