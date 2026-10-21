# The command-line program: ManageDsk's command line, TuxDSK's discs. It
# needs nothing of Qt.
#
# Everything but main(), for the tests: the line read, and what it asks for
# done.
add_library(tuxdsk_run STATIC
    commands.cpp
    commands.h
    files.cpp
    files.h
)
# Its headers are found as cli/...
target_include_directories(tuxdsk_run PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tuxdsk_run PUBLIC tuxdsk_disc tuxdsk_base)

# The program is called tuxdsk; its target cannot be, the plugin's is.
add_executable(tuxdsk_cli main.cpp)
set_target_properties(tuxdsk_cli PROPERTIES OUTPUT_NAME tuxdsk)
target_link_libraries(tuxdsk_cli PRIVATE tuxdsk_run)
