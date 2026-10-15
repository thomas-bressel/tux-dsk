#!/bin/bash
# Makes tests/golden again with the real ManageDsk (ManageDsk_V0.20g/
# ManageDsk.exe), run by wine from its command line.
#
#   tools/managedsk-goldens/run.sh
#
# ManageDsk is a 32-bit program made with MFC: wine needs MFC42.DLL, which
# "winetricks mfc42" installs. WINEPREFIX says which wine folder to use, as
# for wine itself; on a system whose wineserver is not in the PATH (Ubuntu's
# wine64), it is in /usr/lib/x86_64-linux-gnu/wine.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
golden="$root/tests/golden"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

export PATH="/usr/lib/x86_64-linux-gnu/wine:$PATH" WINEDEBUG=-all
cp "$root/ManageDsk_V0.20g/ManageDsk.exe" "$work/"
python3 "$here/inputs.py" "$work" big
mkdir -p "$golden/inputs"
python3 "$here/inputs.py" "$golden/inputs"

# ManageDsk says what it does in French, in Windows' characters.
managedsk() {
    (cd "$work" && wine ManageDsk.exe "$@" 2>/dev/null | iconv -f CP1252 -t UTF-8)
}

# An empty disc.
managedsk -C -Sempty.dsk
# Files added as they are, each longer than the one before: ManageDsk leaves
# what remains of a longer file after a shorter one (see notes/ecarts.md).
managedsk -C -Ahello.bas -Acode.bin -Awithhdr.bin -Ak17.dat -Sfiles.dsk
# What names become, and a file added twice.
managedsk -C -Amy.file.name.txt -Alowercase.bas -Averylongfilename.binary '-Aa#b$c&.x1' -Anoext -AUPPER.BIN \
    -Ad1g1t5.b4s -Anoext -Snames.dsk
# Files given a header.
managedsk -C -Icode.bin/CODE.BIN/BIN/32768/32784 -Icode.bin/PROG.BAS/BAS/368 -Ihello.bas/HELLO.TXT/ASCII -Simport.dsk
# A disc that grows: 192K on its 42 tracks.
fill=()
for index in 00 01 02 03 04 05 06 07 08 09 10 11; do fill+=("-Afill$index.dat"); done
managedsk -C "${fill[@]}" -Sgrown.dsk

for disc in empty files names import grown; do
    cp "$work/$disc.dsk" "$golden/$disc.dsk"
done
ls -l "$golden"
