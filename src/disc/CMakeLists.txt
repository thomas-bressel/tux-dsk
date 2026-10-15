# Disc images and AMSDOS's files on them. Every source of this folder is
# part of it.
file(GLOB TUXDSK_DISC_SOURCES CONFIGURE_DEPENDS *.cpp *.h)
add_library(tuxdsk_disc STATIC ${TUXDSK_DISC_SOURCES})
target_link_libraries(tuxdsk_disc PUBLIC tuxdsk_base)
