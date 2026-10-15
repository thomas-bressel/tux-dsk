#!/bin/bash
# Makes the golden .HFE files of tests/golden again, with the converter of
# the HxC Floppy Emulator's own software, hxcfe.
#
#   HXCFE=<folder of hxcfe> tools/hxc-goldens/run.sh
#
# hxcfe is built from https://github.com/jfdelnero/HxCFloppyEmulator with
# "make -C build HxCFloppyEmulator_cmdline"; it is then in its build folder,
# beside the libraries it needs, which is the folder HXCFE names.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
golden="$(cd "$here/../.." && pwd)/tests/golden"
: "${HXCFE:?HXCFE must name the folder hxcfe is in}"

python3 "$here/discs.py" "$golden"
for disc in hfe_data hfe_odd; do
    rm -f "$golden/$disc.hfe"
    (cd "$HXCFE" && LD_LIBRARY_PATH=. ./hxcfe -finput:"$golden/$disc.dsk" -conv:HXC_HFE -foutput:"$golden/$disc.hfe" > /dev/null)
done
ls -l "$golden"/hfe_*
