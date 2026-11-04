#!/bin/bash
# Sourced by launch.sh. Keep memory/kernel evidence outside the engine, so a
# SIGKILL still leaves process samples and an after-exit kernel snapshot.

runtime_memory_snapshot() {
    local engine_pid="${1:-}" uptime="unknown" remainder=""
    [ ! -r /proc/uptime ] || read -r uptime remainder < /proc/uptime
    printf 'memory: uptime=%s pid=%s' "$uptime" "${engine_pid:-none}"
    local files=(/proc/meminfo /proc/vmstat)
    if [ -n "$engine_pid" ] && [ -r "/proc/$engine_pid/status" ]; then
        files+=("/proc/$engine_pid/status")
    fi
    awk '
        /^(MemTotal|MemAvailable|MemFree|SwapTotal|SwapFree|VmRSS|VmHWM|VmSwap|RssAnon|RssFile|RssShmem):/ {
            key=$1; sub(/:$/, "", key); printf " %s=%s%s", key, $2, $3
        }
        $1 == "oom_kill" { printf " oom_kill=%s", $2 }
        END { print "" }
    ' "${files[@]}" 2>/dev/null
}

runtime_system_snapshot() {
    runtime_memory_snapshot
    printf '\n--- kernel messages (last 160 lines; may require permissions) ---\n'
    if command -v dmesg >/dev/null 2>&1; then
        dmesg 2>&1 | tail -n 160
    else
        echo 'dmesg unavailable'
    fi
}

run_monitored_engine() {
    local engine_pid watcher_pid engine_status
    local resource_log="$STATE_DIR/resource.log"
    runtime_system_snapshot > "$STATE_DIR/system-before.log" 2>&1
    : > "$resource_log"
    echo "runtime: memory samples in $resource_log; kernel snapshots in system-before.log/system-after.log"
    "$RUNDIR/nfsu2" "$@" &
    engine_pid=$!
    (
        while kill -0 "$engine_pid" 2>/dev/null; do
            runtime_memory_snapshot "$engine_pid" >> "$resource_log"
            sleep 1
        done
    ) &
    watcher_pid=$!
    wait "$engine_pid"
    engine_status=$?
    kill "$watcher_pid" 2>/dev/null
    wait "$watcher_pid" 2>/dev/null
    runtime_system_snapshot > "$STATE_DIR/system-after.log" 2>&1
    echo "runtime: engine pid=$engine_pid exit=$engine_status"
    runtime_memory_snapshot
    sync "$resource_log" "$STATE_DIR/system-before.log" "$STATE_DIR/system-after.log" 2>/dev/null || sync
    return "$engine_status"
}
