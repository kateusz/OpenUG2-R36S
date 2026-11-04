#!/bin/bash
# OpenUG2 - second Ports menu entry, under a name that cannot be mistaken for
# the "OpenUG2" folder. Identical to OpenUG2.sh; use whichever one appears.
PORTS="$(cd -- "$(dirname -- "$0")" && pwd)"
echo "$(date '+%Y-%m-%d %H:%M:%S') shim $(basename -- "$0") in $PORTS" \
    >> "$PORTS/.openug2-shim.log" 2>/dev/null

if [ -f "$PORTS/OpenUG2/launch.sh" ]; then
    TARGET="$PORTS/OpenUG2/launch.sh"
elif [ -f "$PORTS/launch.sh" ]; then
    TARGET="$PORTS/launch.sh"
else
    echo "OpenUG2: launch.sh not found near $PORTS" >&2
    exit 1
fi

exec bash "$TARGET" "$@"
