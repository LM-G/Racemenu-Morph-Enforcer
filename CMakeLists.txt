cmake_minimum_required(VERSION 3.21)

# Optional: emit DLL as RacemenuMorphFixer.dll while keeping lowercase target
set(CMAKE_DEBUG_POSTFIX "") # no 'd' suffix if you want clean file name
set(PROJECT_NAME "RacemenuMorphFixer")
set(PROJECT_AUTHOR "Pliseman")
set(PROJECT_VERSION 1.0.0)
# source list
set(SRCS
        src/main.cpp
        src/logger.cpp
        src/morph_updater.cpp
        src/racemenu_watcher.cpp
        src/racemenu_event_watcher.cpp
        src/settings.cpp
        src/helpers/string.cpp
        src/helpers/ui.cpp
        src/helpers/keybind.cpp
        src/arrow_weight_sink.cpp
        src/hooks/gfx_invoke_hook.cpp
        src/hooks/gfx_ei_hook.cpp
        src/helpers/racemenu_ei_driver.cpp
)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# allow older third‑party configs without downgrading the project itself
if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()

# Setup your SKSE plugin as an SKSE plugin!
find_package(CommonLibSSE CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIR NAMES SimpleIni.h
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
        NO_DEFAULT_PATH)
if (NOT SIMPLEINI_INCLUDE_DIR)
    message(FATAL_ERROR "SimpleIni.h not found. Did vcpkg install 'simpleini'?")
endif ()

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${SRCS} AUTHOR ${PROJECT_AUTHOR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

# Name the produced DLL file explicitly
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

add_library(SimpleIni::SimpleIni INTERFACE IMPORTED)
target_include_directories(SimpleIni::SimpleIni INTERFACE ${SIMPLEINI_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE SimpleIni::SimpleIni)

# Pass DLL name to C++ via a compile definition
target_compile_definitions(${PROJECT_NAME} PRIVATE
        PLUGIN_NAME="${PROJECT_NAME}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
        $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

# Optional auto-deploy to SKSE/Plugins if you set one of these env vars
if (DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
elseif (DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
else ()
    set(OUTPUT_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/output/${PROJECT_NAME}")
endif ()

if (DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
    set(RES_FOLDER "${DLL_FOLDER}/${PROJECT_NAME}")
    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DLL_FOLDER}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
            # ini: keep the same filename on destination
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/racemenumorphfixer.ini"
            "${RES_FOLDER}/racemenumorphfixer.ini"
            VERBATIM
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
                "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
                VERBATIM
        )
    endif ()
endif ()
