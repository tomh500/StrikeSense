cmake_minimum_required(VERSION 3.20)
project(StrikeSense
    VERSION 1.0.0
    DESCRIPTION "StrikeSense Win32 Desktop Application"
    LANGUAGES CXX RC
)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Nightly" CACHE STRING "" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_NIGHTLY Release)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
set(VCPKG_HOST_TRIPLET "x64-windows-static" CACHE STRING "")


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W3 /utf-8 /permissive- /Zc:__cplusplus
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Nightly>>:/O2> $<$<OR:$<CONFIG:Release>,$<CONFIG:Nightly>>:/GS->
        $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Debug>:/Zi>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Nightly>>:/MT> $<$<CONFIG:Debug>:/MTd>
    )
    add_link_options($<$<CONFIG:Debug>:/DEBUG>)
endif()

add_definitions(-D_WINDOWS -DUNICODE -D_UNICODE)
add_compile_definitions(
    $<$<CONFIG:Release>:STRIKESENSE_BUILD_CODE=0>
    $<$<CONFIG:Nightly>:STRIKESENSE_BUILD_CODE=1>
    $<$<CONFIG:Debug>:STRIKESENSE_BUILD_CODE=2>
)

# 寻找第三方包
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)

set(SOURCES
    src/app/StrikeSense.cpp
    src/app/console.cpp
    src/app/sidebar.cpp
    src/app/ui_common.cpp
    src/core/antistupid.cpp
    src/core/config.cpp
    src/core/cscript.cpp
    src/core/console_log.cpp
    src/core/flashoverlay.cpp
    src/core/gsi_server.cpp
    src/core/hotkey.cpp
    src/core/i18n.cpp
    src/core/input_environment.cpp
    src/core/itemhelper_overlay.cpp
    src/core/kernelcore.cpp
    src/core/mouse_jitter.cpp
    src/core/module_notifications.cpp
    src/core/notifications_overlay.cpp
    src/core/normalgen.cpp
    src/core/quickstop.cpp
    src/core/sound_player.cpp
    src/core/steam_accounts.cpp
    src/core/steam_helper.cpp
    src/core/textgui_overlay.cpp
    src/core/ui_theme.cpp
    src/core/volume_mixer.cpp
    src/ui/Rage_page.cpp
    src/ui/cscript_panel.cpp
    src/ui/evolution_page.cpp
    src/ui/itemhelper_page.cpp
    src/ui/legalcfg_page.cpp
    src/ui/settings_page.cpp
    src/ui/sounds_page.cpp
    src/ui/sound_ui.cpp
    src/ui/vscript_page.cpp
    src/vscript/vscript.cpp
    src/vscript/vscript_eval.cpp
    src/vscript/vscript_runtime.cpp
    src/vscript/vscript_steam.cpp

)
set(HEADERS
    include/app/StrikeSense.h
    include/app/console.h
    include/core/antistupid.h
    include/core/config.h
    include/core/cscript.h
    include/core/console_log.h
    include/core/flashoverlay.h
    include/core/gsi_server.h
    include/core/hotkey.h
    include/core/i18n.h
    include/core/input_environment.h
    include/core/itemhelper_overlay.h
    include/core/kernelcore.h
    include/core/mouse_jitter.h
    include/core/module_notifications.h
    include/core/notifications_overlay.h
    include/core/normalgen.h
    include/core/quickstop.h
    include/core/sound_player.h
    include/core/steam_accounts.h
    include/core/steam_helper.h
    include/core/textgui_overlay.h
    include/core/ui_theme.h
    include/core/volume_mixer.h
    include/ui/itemhelper_page.h
    include/ui/cscript_panel.h
    include/ui/sound_ui.h
    include/vscript/vscript.h
    pages.h framework.h targetver.h Resource.h SteamHelper.h
)
set(RESOURCES StrikeSense.rc)

add_executable(StrikeSense WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
target_include_directories(StrikeSense PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/app
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vscript
)

# 💎 2. 链接外部你刚才生成的纯静态 SteamHelper.lib
# 假设你的 SteamHelper.lib 放在项目根目录下的 lib 文件夹中（可根据实际路径修改）
# target_link_libraries(StrikeSense PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/SteamHelper.lib")

target_link_libraries(StrikeSense PRIVATE
    nlohmann_json::nlohmann_json httplib::httplib yaml-cpp::yaml-cpp
    SDL2::SDL2 SDL2_mixer::SDL2_mixer-static SDL2_ttf::SDL2_ttf-static
    simdjson::simdjson
    # Win32 基础系统库
    kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
    advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib winmm.lib
    # GDI+ 所需的系统库（看你项目里有用 GDI+ 渲染，MT下必须手动带上它）
    gdiplus.lib
)

set_target_properties(StrikeSense PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# 移除重复的无用 install 行，保留一行规范化输出
install(TARGETS StrikeSense RUNTIME DESTINATION bin)

