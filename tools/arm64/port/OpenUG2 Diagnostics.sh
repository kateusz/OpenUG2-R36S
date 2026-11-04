#!/bin/bash
# OpenUG2 diagnostics: pad discovery and a GL-free data audit.
#
# Launch "OpenUG2 Diagnostics" from the Ports menu once, then copy
# input-info.log and instance-audit.log off the SD card. The first tells us
# the R36S pad GUID and mapping; the second proves the game data subset is
# complete. Both logs also land in $HOME if the card is read-only.

set -u

echo "OpenUG2 diagnostics: launcher started"

PORTS_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PORTDIR="$PORTS_DIR/OpenUG2"
RUNDIR="/tmp/openug2"
echo "OpenUG2 diagnostics: port directory $PORTDIR"

# Same user-editable settings as the game launcher, so a driver override
# applies to diagnostics too.
[ -f "$PORTDIR/config.txt" ] && . "$PORTDIR/config.txt"

STATE_DIR=""
for candidate in "$PORTDIR" "${HOME:-/tmp}/.local/share/openug2" "/tmp/openug2-state"; do
    if mkdir -p "$candidate/conf/openug2" "$candidate/save" 2>/dev/null &&
       : > "$candidate/.openug2-write-test" 2>/dev/null; then
        rm -f "$candidate/.openug2-write-test" 2>/dev/null
        STATE_DIR="$candidate"
        break
    fi
done
[ -n "$STATE_DIR" ] || { echo "OpenUG2: no writable state directory"; exit 1; }
echo "OpenUG2 diagnostics: state directory $STATE_DIR"

mkdir -p "$RUNDIR" 2>/dev/null
export XDG_CONFIG_HOME="$STATE_DIR/conf"
export XDG_DATA_HOME="$STATE_DIR/save"
export OPENUG2_CONFIG_DIR="$STATE_DIR/conf/openug2"
# Leave SDL on auto-detect; the diagnostics log records what it picked.
[ -n "${OPENUG2_VIDEODRIVER:-}" ] && export SDL_VIDEODRIVER="$OPENUG2_VIDEODRIVER"

for db in \
    "${HOME:-/root}/.config/SDL-GameControllerDB/gamecontrollerdb.txt" \
    "/opt/inttools/gamecontrollerdb.txt" \
    "$PORTDIR/gamecontrollerdb.txt" \
    "/roms/ports/PortMaster/gamecontrollerdb.txt" \
    "$PORTS_DIR/PortMaster/gamecontrollerdb.txt"
do
    if [ -s "$db" ] && grep -qv '^[[:space:]]*\(#\|$\)' "$db"; then
        export SDL_GAMECONTROLLERCONFIG_FILE="$db"
        break
    fi
done

cp -f "$PORTDIR/nfsu2" "$RUNDIR/nfsu2" || exit 1
chmod +x "$RUNDIR/nfsu2" 2>/dev/null

cd "$PORTDIR" || exit 1

INPUT_LOG="$STATE_DIR/input-info.log"
AUDIT_LOG="$STATE_DIR/instance-audit.log"

# A blocked KMS/DRM init must not hang the whole diagnostic; timeout(1) is
# present on ArkOS, and the dummy-driver retry below still reports the pad.
TIMEOUT=""
command -v timeout >/dev/null 2>&1 && TIMEOUT="timeout 20"

{
    echo "=== input-info $(date '+%Y-%m-%d %H:%M:%S') ==="
    echo "state: $STATE_DIR"
    echo "video: ${SDL_VIDEODRIVER:-auto}  audio: ${SDL_AUDIODRIVER:-auto}"
    echo "pads:  ${SDL_GAMECONTROLLERCONFIG_FILE:-(SDL built-in database)}"
} > "$INPUT_LOG" 2>&1

echo "OpenUG2 diagnostics: reading the pad (a few seconds)..."
$TIMEOUT "$RUNDIR/nfsu2" --input-info 2>&1 | tee -a "$INPUT_LOG"
STATUS=${PIPESTATUS[0]}
echo "exit status: $STATUS" >> "$INPUT_LOG"

# The pad data does not need a real display. If KMS/DRM is busy or denied,
# enumerate again with SDL's dummy video driver so the log is still useful.
if [ "$STATUS" -ne 0 ]; then
    echo "--- video init failed or timed out, retrying with SDL_VIDEODRIVER=dummy ---" >> "$INPUT_LOG"
    $TIMEOUT env SDL_VIDEODRIVER=dummy "$RUNDIR/nfsu2" --input-info 2>&1 | tee -a "$INPUT_LOG"
    echo "exit status (dummy): ${PIPESTATUS[0]}" >> "$INPUT_LOG"
fi

{
    echo "=== instance-audit $(date '+%Y-%m-%d %H:%M:%S') ==="
} > "$AUDIT_LOG" 2>&1
echo "OpenUG2 diagnostics: auditing the data (this can take a few minutes)..."
"$RUNDIR/nfsu2" "$PORTDIR/data" --instance-audit \
    --load-radius "${OPENUG2_LOAD_RADIUS:-700}" \
    --view-distance "${OPENUG2_VIEW_DISTANCE:-600}" 2>&1 | tee -a "$AUDIT_LOG"
echo "exit status: ${PIPESTATUS[0]}" >> "$AUDIT_LOG"

rm -f "$RUNDIR/nfsu2"

# If state had to live in $HOME, leave a copy where it is easy to find.
if [ "$STATE_DIR" != "$PORTDIR" ]; then
    cp -f "$INPUT_LOG" "$PORTDIR/input-info.log" 2>/dev/null
    cp -f "$AUDIT_LOG" "$PORTDIR/instance-audit.log" 2>/dev/null
fi
echo "OpenUG2 diagnostics: done."
