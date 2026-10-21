cmake_minimum_required(VERSION 3.21)

project(TuxDSK
    VERSION 0.2.1
    DESCRIPTION "Disc image manager for the Amstrad CPC: a plugin for TuxAPE, after ManageDsk"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The engine goes into the plugin, a shared library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# TuxAPE's sources: all the plugin needs of them is its plugin SDK, sdk/plugin.h.
set(TUXAPE_DIR "${CMAKE_SOURCE_DIR}/../TuxAPE" CACHE PATH "TuxAPE's source tree")
if(NOT EXISTS "${TUXAPE_DIR}/sdk/plugin.h")
    message(FATAL_ERROR "TuxAPE's plugin.h is not in ${TUXAPE_DIR}/sdk: set TUXAPE_DIR to TuxAPE's source tree.")
endif()

option(TUXDSK_BUILD_TESTS "Build the test programs" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# What the machine is called in a plugin's plugin.json: as TuxAPE's
# PluginManager::platform() gives it.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TUXDSK_SYSTEM)
if(TUXDSK_SYSTEM STREQUAL "darwin")
    set(TUXDSK_SYSTEM "macos")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TUXDSK_PROCESSOR)
if(TUXDSK_PROCESSOR STREQUAL "amd64")
    set(TUXDSK_PROCESSOR "x86_64")
elseif(TUXDSK_PROCESSOR STREQUAL "aarch64")
    set(TUXDSK_PROCESSOR "arm64")
endif()
set(TUXDSK_PLATFORM "${TUXDSK_SYSTEM}-${TUXDSK_PROCESSOR}")

# The discs and what shows their files have no dependency on Qt: they are
# tested by themselves and are what the command-line program is made of.
add_subdirectory(src/base)
add_subdirectory(src/disc)
add_subdirectory(src/view)
add_subdirectory(src/cli)
add_subdirectory(src/plugin)

if(TUXDSK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- The archive users install: dist/tuxdsk-<version>-<platform>.zip ----
# It holds the plugin's folder, as TuxAPE's plugins folder does.
set(TUXDSK_STAGE "${CMAKE_BINARY_DIR}/stage")
set(TUXDSK_ARCHIVE "${CMAKE_SOURCE_DIR}/dist/tuxdsk-${PROJECT_VERSION}-${TUXDSK_PLATFORM}.zip")
configure_file(plugin.json.in "${TUXDSK_STAGE}/tuxdsk/plugin.json" @ONLY)
# The command-line program goes with it, in the folder's bin: TuxAPE makes
# what it unpacks there a program that can be run.
# Signed with its author's secret key when there is one (TUXAPE_SIGN_KEY,
# made by TuxAPE's tuxape-plugin-sign, which TUXAPE_SIGN_TOOL names): the
# signature goes in the folder, over its other files, before it is packed.
set(TUXAPE_SIGN_KEY "$ENV{HOME}/.config/tuxape-signing/tuxape.key" CACHE FILEPATH "The secret key the plugin is signed with")
set(TUXAPE_SIGN_TOOL "${TUXAPE_DIR}/build/src/sign/tuxape-plugin-sign" CACHE FILEPATH "TuxAPE's tuxape-plugin-sign")
if(EXISTS "${TUXAPE_SIGN_KEY}" AND EXISTS "${TUXAPE_SIGN_TOOL}")
    set(TUXDSK_SIGN "${TUXAPE_SIGN_TOOL}" sign "${TUXDSK_STAGE}/tuxdsk" "${TUXAPE_SIGN_KEY}")
else()
    message(STATUS "The archive is not signed: no secret key (TUXAPE_SIGN_KEY) or no tuxape-plugin-sign (TUXAPE_SIGN_TOOL)")
    set(TUXDSK_SIGN "${CMAKE_COMMAND}" -E rm -f "${TUXDSK_STAGE}/tuxdsk/signature.json")
endif()
add_custom_target(plugin_zip ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TUXDSK_STAGE}/tuxdsk/${TUXDSK_PLATFORM}" "${TUXDSK_STAGE}/tuxdsk/bin"
            "${CMAKE_SOURCE_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxdsk>" "${TUXDSK_STAGE}/tuxdsk/${TUXDSK_PLATFORM}/"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxdsk_cli>" "${TUXDSK_STAGE}/tuxdsk/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${TUXDSK_STAGE}/tuxdsk/"
    COMMAND ${TUXDSK_SIGN}
    COMMAND ${CMAKE_COMMAND} -E chdir "${TUXDSK_STAGE}"
            ${CMAKE_COMMAND} -E tar cf "${TUXDSK_ARCHIVE}" --format=zip tuxdsk
    DEPENDS tuxdsk tuxdsk_cli
    COMMENT "Packing ${TUXDSK_ARCHIVE}"
    VERBATIM)
