cmake_minimum_required(VERSION 3.20)
project(StrikeSense
    VERSION 1.0.0
    DESCRIPTION "StrikeSense Win32 Desktop Application"
    LANGUAGES CXX RC
)

# ============================================================
# C++ Standard
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 静态链接 MSVC 运行时库 (MT / MTd)
# ============================================================
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    add_compile_options(
        /W3
        /utf-8
        /permissive-
        /Zc:__cplusplus
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GS->
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
    )
    add_link_options(
        $<$<CONFIG:Debug>:/DEBUG>
    )
endif()

# ============================================================
# 预处理器定义
# ============================================================
add_definitions(
    -D_WINDOWS
    -DUNICODE
    -D_UNICODE
)

# ============================================================
# vcpkg — 查找已安装的包
# ============================================================
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# ============================================================
# 源文件
# ============================================================
set(SOURCES
    StrikeSense.cpp
    console.cpp
)

set(HEADERS
    StrikeSense.h
    console.h
    framework.h
    targetver.h
    Resource.h
)

set(RESOURCES
    StrikeSense.rc
)

# ============================================================
# 可执行文件目标
# ============================================================
add_executable(StrikeSense WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# ============================================================
# 包含目录
# ============================================================
target_include_directories(StrikeSense PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# 链接 vcpkg 库
# ============================================================
target_link_libraries(StrikeSense PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    yaml-cpp::yaml-cpp
)

# ============================================================
# 链接 Windows SDK 系统库
# ============================================================
target_link_libraries(StrikeSense PRIVATE
    kernel32.lib
    user32.lib
    gdi32.lib
    winspool.lib
    comdlg32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    odbc32.lib
    odbccp32.lib
)

# ============================================================
# 输出目录
# ============================================================
set_target_properties(StrikeSense PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ============================================================
# 安装
# ============================================================
install(TARGETS StrikeSense
    RUNTIME DESTINATION bin
)