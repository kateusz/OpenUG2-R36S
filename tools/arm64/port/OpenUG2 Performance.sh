#!/bin/bash
# Fixed-pose render/presentation comparison. Returns to Ports when complete.
PORTS="$(cd -- "$(dirname -- "$0")" && pwd)"
export OPENUG2_COMPARE=1
if [ -f "$PORTS/OpenUG2/launch.sh" ]; then
    exec bash "$PORTS/OpenUG2/launch.sh"
elif [ -f "$PORTS/launch.sh" ]; then
    exec bash "$PORTS/launch.sh"
fi
echo 'OpenUG2: launch.sh not found' >&2
exit 1
