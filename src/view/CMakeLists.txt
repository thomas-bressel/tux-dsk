# What a file of a disc is shown as: bytes, a listing, a Dams source, Z80
# instructions, a screen. Every source of this folder is part of it.
file(GLOB TUXDSK_VIEW_SOURCES CONFIGURE_DEPENDS *.cpp *.h *.inc)
add_library(tuxdsk_view STATIC ${TUXDSK_VIEW_SOURCES})
target_link_libraries(tuxdsk_view PUBLIC tuxdsk_disc)
