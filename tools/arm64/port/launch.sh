#!/bin/bash
# OpenUG2 - Need for Speed Underground 2 engine reimplementation (R36S port).
#
# This is the real launcher. It lives inside the port folder and is started by
# the thin menu entries next to it (OpenUG2.sh / NFSU2.sh), the same shape the
# Gothic port uses. It also works when started directly.
#
# The binary is copied to /tmp because the EASYROMS partition is often
# NTFS/exFAT and cannot be trusted to keep the executable bit.
#
# Writable state (config, saves, logs) prefers the port directory so it is easy
# to read from a PC. If the card is mounted read-only for the device user
# (common with NTFS), it falls back to $HOME, which is always writable.

set -u

PORTDIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PORTS_DIR="$(dirname -- "$PORTDIR")"

# Menu entries sometimes get copied into the port folder itself; in that case
# the binary sits one level up. Keep every entry point working.
if [ ! -f "$PORTDIR/nfsu2" ] && [ -f "$PORTDIR/../nfsu2" ]; then
    PORTDIR="$(cd -- "$PORTDIR/.." && pwd)"
    PORTS_DIR="$(dirname -- "$PORTDIR")"
fi

# Breadcrumb: survives a crash, so a blank screen can still be explained after
# the card comes back to the PC.
echo "$(date '+%Y-%m-%d %H:%M:%S') launch.sh start ($0)" >> "$PORTDIR/boot.log" 2>/dev/null

echo "OpenUG2: launcher started"
echo "OpenUG2: port directory $PORTDIR"

# User-editable settings; keep config.txt valid shell.
[ -f "$PORTDIR/config.txt" ] && . "$PORTDIR/config.txt"
if [ "${OPENUG2_COMPARE:-0}" = 1 ]; then
    OPENUG2_PERF_MONITOR=1
    OPENUG2_RESOURCE_MONITOR=1
fi

GAME="$PORTDIR/nfsu2"
DATA="$PORTDIR/data"
RUNDIR="/tmp/openug2"

# Pick a writable state directory. The probe creates a real file, because a
# directory can exist yet still be read-only.
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
echo "OpenUG2: state directory $STATE_DIR"

LOG="$STATE_DIR/last-run.log"
if [ "${OPENUG2_COMPARE:-0}" = 1 ]; then
    LOG=$(mktemp "$STATE_DIR/performance-entry-XXXXXX.log") || exit 1
fi
mkdir -p "$RUNDIR" 2>/dev/null

# Settings and profiles follow the writable state directory.
export XDG_CONFIG_HOME="$STATE_DIR/conf"
export XDG_DATA_HOME="$STATE_DIR/save"
export OPENUG2_CONFIG_DIR="$STATE_DIR/conf/openug2"

# Video/audio drivers: leave SDL on auto-detect unless config.txt overrides.
# Forcing kmsdrm breaks images whose SDL2 was built without it.
[ -n "${OPENUG2_VIDEODRIVER:-}" ] && export SDL_VIDEODRIVER="$OPENUG2_VIDEODRIVER"
[ -n "${OPENUG2_AUDIODRIVER:-}" ] && export SDL_AUDIODRIVER="$OPENUG2_AUDIODRIVER"

# Pad database: the user's own SDL DB first, then the system/PortMaster DBs,
# then the port's own fallback. SDL's built-in database still applies.
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

cd "$PORTDIR" || exit 1

{
    echo "=== OpenUG2 $(date '+%Y-%m-%d %H:%M:%S') ==="
    echo "port:  $PORTDIR"
    echo "data:  $DATA"
    echo "state: $STATE_DIR"
    echo "video: ${SDL_VIDEODRIVER:-auto}  audio: ${SDL_AUDIODRIVER:-auto}"
    echo "pads:  ${SDL_GAMECONTROLLERCONFIG_FILE:-(SDL built-in database)}"
} > "$LOG" 2>&1

# NTFS/exFAT: never rely on the stored executable bit.
if ! cp -f "$GAME" "$RUNDIR/nfsu2"; then
    echo "cannot copy $GAME to $RUNDIR" >> "$LOG"
    exit 1
fi
chmod +x "$RUNDIR/nfsu2" 2>/dev/null

# shellcheck disable=SC2086  # OPENUG2_ARGS is a deliberately word-split list
set -- "$DATA" \
    --resolution "${OPENUG2_RESOLUTION:-640x480}" \
    --scene-resolution "${OPENUG2_SCENE_RESOLUTION:-320x240}" \
    --headlight-shadows "${OPENUG2_HEADLIGHT_SHADOWS:-fast}" \
    --world-shader "${OPENUG2_WORLD_SHADER:-legacy}" \
    --world-order "${OPENUG2_WORLD_ORDER:-legacy}" \
    --tier "${OPENUG2_TIER:-baseline}" \
    --load-radius "${OPENUG2_LOAD_RADIUS:-500}" \
    --view-distance "${OPENUG2_VIEW_DISTANCE:-450}" \
    --vehicle-quality "${OPENUG2_VEHICLE_QUALITY:-low}" \
    --weather-quality "${OPENUG2_WEATHER_QUALITY:-low}" \
    --texture-detail "${OPENUG2_TEXTURE_DETAIL:-2}" \
    --hud --hud-fit "${OPENUG2_HUD_FIT:-4:3}" \
    ${OPENUG2_ARGS:-}

if [ "${OPENUG2_PERF_MONITOR:-0}" = 1 ] && [ "${OPENUG2_COMPARE:-0}" != 1 ]; then
    set -- "$@" --perf-csv "$STATE_DIR/performance.csv"
    export OPENUG2_MEMORY_LOG="$STATE_DIR/memory.csv"
    : > "$OPENUG2_MEMORY_LOG"
    echo "performance: frame timings in $STATE_DIR/performance.csv" >> "$LOG"
fi

echo "exec: $RUNDIR/nfsu2 $*" >> "$LOG"
echo "OpenUG2: starting (state: $STATE_DIR)"

# Flush the log to the card before the engine runs: if the GPU hangs hard, the
# log still explains what happened.
sync

# A hard reset previously left a NUL-filled tail on NTFS. During an input
# trace or performance capture, flush files from a separate process so a stuck GL
# call cannot prevent the last completed watchdog lines reaching the card.
MONITOR_SECONDS=0
previous_arg=""
for arg in "$@"; do
    if [ "$previous_arg" = "--input-monitor" ]; then MONITOR_SECONDS="$arg"; fi
    previous_arg="$arg"
done
LOG_SYNC_PID=""
stop_log_sync() {
    if [ -n "$LOG_SYNC_PID" ]; then
        kill "$LOG_SYNC_PID" 2>/dev/null
        wait "$LOG_SYNC_PID" 2>/dev/null
        LOG_SYNC_PID=""
    fi
}
trap stop_log_sync EXIT
if [ "${OPENUG2_PERF_MONITOR:-0}" = 1 ] || { [[ "$MONITOR_SECONDS" =~ ^[0-9]+$ ]] && (( 10#$MONITOR_SECONDS > 0 && 10#$MONITOR_SECONDS <= 3600 )); }; then
    if [[ "$MONITOR_SECONDS" =~ ^[0-9]+$ ]]; then MONITOR_SECONDS=$((10#$MONITOR_SECONDS)); else MONITOR_SECONDS=0; fi
    (
        elapsed=0
        while { [ "${OPENUG2_PERF_MONITOR:-0}" = 1 ] || (( elapsed < MONITOR_SECONDS )); } && kill -0 "$$" 2>/dev/null; do
            sleep 3
            if [ "${OPENUG2_COMPARE:-0}" = 1 ]; then sync; else
                sync "$LOG" "$STATE_DIR/resource.log" "$STATE_DIR/performance.csv" "$STATE_DIR/memory.csv" "$STATE_DIR/system-before.log" 2>/dev/null || sync "$LOG" 2>/dev/null || sync
            fi
            elapsed=$((elapsed + 3))
        done
    ) &
    LOG_SYNC_PID=$!
fi

run_game() {
    if [ "${OPENUG2_COMPARE:-0}" = 1 ]; then
        . "$PORTDIR/performance-comparison.sh"
        run_performance_comparison "$@"
        return $?
    fi
    if [ "${OPENUG2_RESOURCE_MONITOR:-0}" = 1 ] && [ -r "$PORTDIR/runtime-diagnostics.sh" ]; then
        . "$PORTDIR/runtime-diagnostics.sh"
        run_monitored_engine "$@"
    else
        "$RUNDIR/nfsu2" "$@"
    fi
}

if command -v tee >/dev/null 2>&1; then
    run_game "$@" 2>&1 | tee -a "$LOG"
    STATUS=${PIPESTATUS[0]}
else
    run_game "$@" >> "$LOG" 2>&1
    STATUS=$?
fi

echo "exit status: $STATUS" >> "$LOG"
stop_log_sync
sync "$LOG" 2>/dev/null || sync
rm -f "$RUNDIR/nfsu2"

# If state had to live in $HOME, leave a copy where it is easy to find.
if [ "$STATE_DIR" != "$PORTDIR" ] && [ "${OPENUG2_COMPARE:-0}" != 1 ]; then
    cp -f "$LOG" "$PORTDIR/last-run.log" 2>/dev/null
fi
exit "$STATUS"
