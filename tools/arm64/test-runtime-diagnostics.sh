#!/bin/bash
# Linux-only launcher regression; uses a stub engine, no game data or GL.
set -eu
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work=$(mktemp -d /tmp/openug2-runtime-check.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/bin" "$work/port"
cp "$here/port/launch.sh" "$here/port/runtime-diagnostics.sh" "$here/port/performance-comparison.sh" "$here/port/config.txt" "$work/port/"
sed -i 's/\r$//' "$work/port/"*
cat > "$work/port/nfsu2" <<'ENGINE'
#!/bin/bash
echo "stub-engine: $*"
mkdir -p "$OPENUG2_CONFIG_DIR" "$XDG_DATA_HOME"
printf '%s\n' "$*" > "$OPENUG2_CONFIG_DIR/test-write.txt"
printf '%s\n' "$*" > "$XDG_DATA_HOME/test-write.txt"
echo "stub-input-watch: t=0 frame=1 phase=present"
sleep "${ENGINE_SLEEP:-4}"
if [ -n "${ENGINE_FAIL_CASE:-}" ] && [[ "${OPENUG2_MEMORY_LOG:-}" == *"/$ENGINE_FAIL_CASE/"* ]]; then exit 7; fi
if [ "${ENGINE_EXIT:-0}" = 137 ]; then kill -KILL "$$"; fi
exit "${ENGINE_EXIT:-0}"
ENGINE
cat > "$work/bin/sync" <<'SYNC'
#!/bin/bash
echo "sync: parent=$PPID args=$*" >> "$SYNC_TRACE"
SYNC
chmod +x "$work/bin/sync"
export SYNC_TRACE="$work/sync.log"
export PATH="$work/bin:$PATH"
sed -i 's/--input-monitor 120/--input-monitor 008/' "$work/port/config.txt"
for status in 0 7 137; do
    export ENGINE_EXIT="$status"
    set +e
    bash "$work/port/launch.sh" > "$work/output.log" 2>&1
    rc=$?
    set -e
    [ "$rc" = "$status" ]
    grep -q "exit status: $status" "$work/port/last-run.log"
    grep -q "runtime: engine pid=.* exit=$status" "$work/port/last-run.log"
    grep -q 'stub-input-watch' "$work/port/last-run.log"
    grep -q -- '--perf-csv' "$work/port/last-run.log"
    grep -q 'cpu_ticks: cpu' "$work/port/resource.log"
    grep -q 'process_stat:' "$work/port/resource.log"
    grep -q 'VmRSS=' "$work/port/resource.log"
    grep -q 'MemAvailable=' "$work/port/resource.log"
    grep -q 'oom_kill=' "$work/port/system-before.log"
    grep -q 'oom_kill=' "$work/port/system-after.log"
    grep -q 'kernel messages' "$work/port/system-after.log"
    # Every diagnostic child should have exited with the launcher. A killed
    # sleep may briefly retain the pipe but cannot write another sample.
    watcher_pids=$(awk '{for(i=1;i<=NF;i++)if($i~/^parent=/){sub(/^parent=/,"",$i);print $i}}' "$SYNC_TRACE" | sort -u)
    for pid in $watcher_pids; do
        if kill -0 "$pid" 2>/dev/null; then echo "FAIL: worker survived exit: $pid"; exit 1; fi
    done
    echo "runtime-launcher-test: exit=$status PASS (memory, kernel snapshots, worker cleanup)"
done
# Preserve the exact normal-play evidence across the entire comparison entry.
(cd "$work/port" && sha256sum last-run.log resource.log memory.csv system-before.log system-after.log conf/openug2/test-write.txt save/test-write.txt > "$work/normal.sha")
export ENGINE_EXIT=0 ENGINE_SLEEP=0.1 OPENUG2_COMPARE=1
bash "$work/port/launch.sh" > "$work/compare-output.log" 2>&1
capture=$(find "$work/port" -maxdepth 1 -type d -name 'perf-compare-*' | head -1)
for name in native-a scene-480 scene-320 headlights world-shader depth optimized-repeat native-b; do
    test -s "$capture/$name/engine.log"
    grep -q "$name exit=0" "$capture/manifest.txt"
done
grep -q -- '--scene-resolution native --headlight-shadows reference --world-shader legacy --world-order legacy' "$capture/native-a/engine.log"
grep -q -- '--scene-resolution 320x240 --headlight-shadows fast --world-shader auto --world-order depth' "$capture/depth/engine.log"
! grep -q -- '--perf-pixel-scale' "$capture/scene-320/engine.log"
(cd "$work/port" && sha256sum -c "$work/normal.sha")
echo 'runtime-comparison-test: eight isolated cases; normal-play logs/settings/saves preserved PASS'
export ENGINE_FAIL_CASE=scene-480
set +e
bash "$work/port/launch.sh" > "$work/compare-failure.log" 2>&1
rc=$?
set -e
[ "$rc" = 7 ]
grep -q 'finished status=7' "$work/compare-failure.log"
(cd "$work/port" && sha256sum -c "$work/normal.sha")
echo 'runtime-comparison-test: failure stops later cases and preserves status/logs PASS'
