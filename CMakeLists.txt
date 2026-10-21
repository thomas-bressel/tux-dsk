# TuxDSK: ManageDsk's disc image manager for the Amstrad CPC, as a TuxAPE plugin
# (see PLUGINS.md at TuxAPE's root).
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

# Qt, for the whole project: TuxAPE's, or an earlier Qt 6 (checked by
# tuxape_add_plugin).
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# TuxAPE's plugin SDK: plugin.h and TuxAPEPlugin.cmake, all the plugin
# needs of TuxAPE.
set(TUXAPE_SDK "${CMAKE_SOURCE_DIR}/../TuxAPE/sdk" CACHE PATH "TuxAPE's sdk folder")
if(NOT EXISTS "${TUXAPE_SDK}/plugin.h" OR NOT EXISTS "${TUXAPE_SDK}/TuxAPEPlugin.cmake")
    message(FATAL_ERROR "TuxAPE's SDK is not in ${TUXAPE_SDK}: set TUXAPE_SDK to TuxAPE's sdk folder.")
endif()
include("${TUXAPE_SDK}/TuxAPEPlugin.cmake")

option(TUXDSK_BUILD_TESTS "Build the test programs" ON)
# Disc images of one's own, for engine_disc_image to read and write back:
# each must come back the file it was. None if empty.
set(TUXDSK_DISC_DIR "" CACHE PATH "A folder of disc images the tests read and write back")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The discs and what shows their files, without Qt: tested by themselves,
# and what the command-line program is made of.
add_subdirectory(src/engine)
# tuxdsk: ManageDsk's command line.
add_subdirectory(src/cli)
# What TuxAPE loads: the windows, and the plugin's class.
add_subdirectory(src/plugin)

# The archive users install: dist/tuxdsk-<version>-<platform>.zip, with the
# command-line program in its bin folder and the plugin's translations
# built into its library; signed when the author's secret key is named
# (TUXAPE_SIGN_KEY).
tuxape_add_plugin(tuxdsk
    ID tuxdsk
    NAME "TuxDSK – Disc Manager"
    VERSION ${PROJECT_VERSION}
    AUTHOR "TuxAPE"
    LICENCE "MIT"
    DESCRIPTION "Manages the files of Amstrad CPC disc images (.DSK, .HFE), as ManageDsk by Ludovic Deplanque (Demoniak) does: adds, exports, renames and deletes them, shows them as listings, screens or code, puts the disc in TuxAPE's drives, and takes the tiles of Find Graphics and the files of other plugins."
    API 4
    FILES LICENSE
    PROGRAMS tuxdsk_cli
    TRANSLATIONS translations/tuxdsk_fr.ts
    TRANSLATE_TARGETS tuxdsk_ui)

if(TUXDSK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
