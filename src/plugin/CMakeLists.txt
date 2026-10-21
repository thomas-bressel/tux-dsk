set(CMAKE_AUTOMOC ON)

# The windows are a library, so that tests can drive them; the plugin itself
# is no more than what TuxAPE loads and talks to.
add_library(tuxdsk_ui STATIC
    # The manager's window and its two discs.
    managerwindow.cpp
    managerwindow.h
    discpane.cpp
    discpane.h
    # The windows that ask: what a file is to be on a disc, what it is
    # called, what a new disc is to be.
    dialogs.cpp
    dialogs.h
    prompts.cpp
    prompts.h
    # The windows that show: a file, as text or as a screen, and the sectors.
    viewerdialog.cpp
    viewerdialog.h
    screenwidget.cpp
    screenwidget.h
    sectordialog.cpp
    sectordialog.h
    # What they share.
    preferences.cpp
    preferences.h
    hostfiles.cpp
    hostfiles.h
    controls.h
    icons.cpp
    icons.h
)
target_include_directories(tuxdsk_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TUXAPE_SDK})
target_link_libraries(tuxdsk_ui PUBLIC tuxdsk_view tuxdsk_disc tuxdsk_base Qt6::Widgets)

# What TuxAPE loads: the plugin's class, and no more. tuxape_add_plugin()
# names it after the plugin's id.
add_library(tuxdsk MODULE
    tuxdskplugin.cpp
    tuxdskplugin.h
)
target_link_libraries(tuxdsk PRIVATE tuxdsk_ui)
