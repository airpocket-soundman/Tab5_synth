cmake_minimum_required(VERSION 3.20)
project(tab5_browser_simulators C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-sUSE_SDL=2)

if(NOT DEFINED LVGL_DIR)
    message(FATAL_ERROR "LVGL_DIR must point to an LVGL source checkout")
endif()
if(NOT DEFINED M5GFX_DIR)
    message(FATAL_ERROR "M5GFX_DIR must point to an M5GFX source checkout")
endif()

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_DIR}" "${CMAKE_BINARY_DIR}/lvgl")

add_executable(index
    main.c
    ../ui/lvgl_synth_ui.c
)

target_include_directories(index PRIVATE
    "${LVGL_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ui"
)

target_compile_options(index PRIVATE -O2)
target_link_libraries(index PRIVATE lvgl SDL2)
target_link_options(index PRIVATE
    -O2
    -sUSE_SDL=2
    -sSINGLE_FILE=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/../ui/assets/walnut-background.png@/assets/walnut-background.png"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/../ui/assets/metal-hairline-background.png@/assets/metal-hairline-background.png"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/../ui/assets/cyberdeck-digital-background.png@/assets/cyberdeck-digital-background.png"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

set_target_properties(index PROPERTIES SUFFIX ".html")

file(GLOB_RECURSE M5GFX_SOURCES CONFIGURE_DEPENDS
    "${M5GFX_DIR}/src/*.c"
    "${M5GFX_DIR}/src/*.cpp"
)

add_library(m5gfx STATIC ${M5GFX_SOURCES})
target_include_directories(m5gfx PUBLIC "${M5GFX_DIR}/src")
target_compile_options(m5gfx PRIVATE -O2)

add_executable(m5gfx_preview
    m5gfx/main.cpp
    ../src/AudioSourceType.cpp
    ../src/ParameterIcon.cpp
    ../src/PerformanceUi.cpp
    ../src/Waveform.cpp
)

target_include_directories(m5gfx_preview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/m5gfx"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_compile_options(m5gfx_preview PRIVATE -O2)
target_link_libraries(m5gfx_preview PRIVATE m5gfx SDL2)
target_link_options(m5gfx_preview PRIVATE
    -O2
    -sUSE_SDL=2
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/m5gfx/shell.html"
)

set_target_properties(m5gfx_preview PROPERTIES
    OUTPUT_NAME "m5gfx"
    SUFFIX ".html"
)
