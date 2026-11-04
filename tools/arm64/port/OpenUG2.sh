#!/bin/bash
# OpenUG2 - Ports menu entry.
#
# Deliberately tiny, like the Gothic port's shim: EmulationStation runs this
# file and it immediately hands over to the launcher inside the port folder.
# Keeping the work out of the menu entry means the entry itself can never be
# confused with the "OpenUG2" folder that holds the game data.
PORTS="$(cd -- "$(dirname -- "$0")" && pwd)"
echo "$(date '+%Y-%m-%d %H:%M:%S') shim $(basename -- "$0") in $PORTS" \
    >> "$PORTS/.openug2-shim.log" 2>/dev/null

# Normally /roms/ports/<shim> -> /roms/ports/OpenUG2/launch.sh. The second
# form covers a copy of this file that ended up inside the port folder.
if [ -f "$PORTS/OpenUG2/launch.sh" ]; then
    TARGET="$PORTS/OpenUG2/launch.sh"
elif [ -f "$PORTS/launch.sh" ]; then
    TARGET="$PORTS/launch.sh"
else
    echo "OpenUG2: launch.sh not found near $PORTS" >&2
    exit 1
fi

exec bash "$TARGET" "$@"
