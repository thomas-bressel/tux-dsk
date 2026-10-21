# What the plugin does, without Qt nor TuxAPE: plain C++, in three parts,
# each tested alone.
#   base   the version
#   disc   disc images (.DSK, .HFE) and AMSDOS's files on them
#   view   what a file is shown as: a listing, a Dams source, Z80
#          instructions, a screen
add_subdirectory(base)
add_subdirectory(disc)
add_subdirectory(view)
