cmake_minimum_required(VERSION 3.20)
project(StrikeSense
    VERSION 1.0.0
    DESCRIPTION "StrikeSense Win32 Desktop Application"
    LANGUAGES CXX RC
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/W3 /utf-8 /permissive- /Zc:__cplusplus
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/GS->
        $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Debug>:/Zi>)
    add_link_options($<$<CONFIG:Debug>:/DEBUG>)
endif()

add_definitions(-D_WINDOWS -DUNICODE -D_UNICODE)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

set(SOURCES
    StrikeSense.cpp console.cpp steam_helper.cpp gsi_server.cpp
    config.cpp sound_player.cpp sound_ui.cpp kernelcore.cpp normalgen.cpp antistupid.cpp
    ui_common.cpp sidebar.cpp sounds_page.cpp settings_page.cpp evolution_page.cpp
    legalcfg_page.cpp Rage_page.cpp itemhelper_page.cpp i18n.cpp
)
set(HEADERS
    StrikeSense.h console.h steam_helper.h gsi_server.h config.h
    sound_player.h sound_ui.h kernelcore.h normalgen.h antistupid.h
    pages.h i18n.h framework.h targetver.h Resource.h
)
set(RESOURCES StrikeSense.rc)

add_executable(StrikeSense WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
target_include_directories(StrikeSense PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(StrikeSense PRIVATE
    nlohmann_json::nlohmann_json httplib::httplib yaml-cpp::yaml-cpp
    SDL2::SDL2 SDL2_mixer::SDL2_mixer-static SDL2_ttf::SDL2_ttf-static
    kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
    advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
)

set_target_properties(StrikeSense PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
install(TARGETS StrikeSense RUNTIME DESTINATION bin)