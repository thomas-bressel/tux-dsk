find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# The tests of the engine's parts: every disc_*.cpp and view_*.cpp of this
# folder is a program of its own, linked with its library and nothing else.
# What ManageDsk itself made of the same files is in tests/golden, for them
# to compare with.
foreach(part disc view)
    file(GLOB TUXDSK_PART_TESTS CONFIGURE_DEPENDS ${part}_*.cpp)
    foreach(source ${TUXDSK_PART_TESTS})
        get_filename_component(test ${source} NAME_WE)
        add_executable(${test} ${source})
        target_link_libraries(${test} PRIVATE tuxdsk_${part})
        target_compile_definitions(${test} PRIVATE TUXDSK_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endforeach()

# The discs of TuxAPE's library, if it has one: each is read and written
# back, and must be the file it was.
target_compile_definitions(disc_image PRIVATE TUXDSK_LIBRARY_DIR="${TUXAPE_DIR}/library")

# The command-line program: its line read, and the program run as a user
# runs it, against the discs ManageDsk made with the same commands.
foreach(test cli_commands cli_golden)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE tuxdsk_run)
    target_compile_definitions(${test} PRIVATE
        TUXDSK_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
        TUXDSK_PROGRAM="$<TARGET_FILE:tuxdsk_cli>")
    add_dependencies(${test} tuxdsk_cli)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# The plugin as TuxAPE loads it: found as a library, started with a host of
# the test's own, its window shown.
add_executable(plugin_window plugin_window.cpp)
target_include_directories(plugin_window PRIVATE ${TUXAPE_DIR}/src/app)
target_link_libraries(plugin_window PRIVATE Qt6::Widgets Qt6::Test)
add_dependencies(plugin_window tuxdsk)
target_compile_definitions(plugin_window PRIVATE TUXDSK_PLUGIN="$<TARGET_FILE:tuxdsk>")
add_test(NAME plugin_window COMMAND plugin_window)
set_tests_properties(plugin_window PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The windows, driven as a user drives them, with the discs doing the work
# behind: the manager's window and its two discs, and the windows that show
# a file and the sectors.
foreach(test gui_manager gui_viewers)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE tuxdsk_ui Qt6::Test)
    target_compile_definitions(${test} PRIVATE TUXDSK_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endforeach()
