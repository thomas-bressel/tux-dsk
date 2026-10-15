# What the rest is built on: the version.
add_library(tuxdsk_base STATIC
    version.cpp
    version.h
)
target_include_directories(tuxdsk_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(tuxdsk_base PRIVATE TUXDSK_VERSION="${PROJECT_VERSION}")
