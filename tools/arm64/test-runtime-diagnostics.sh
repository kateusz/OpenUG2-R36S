#!/bin/bash
# Linux-only launcher regression; uses a stub engine, no game data or GL.
set -eu
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work=$(mktemp -d /tmp/openug2-runtime-check.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/bin" "$work/port"
cp "$here/port/launch.sh" "$here/port/runtime-diagnostics.sh" "$here/port/config.txt" "$work/port/"
sed -i 's/\r$//' "$work/port/"*
cat > "$work/port/nfsu2" <<'ENGINE'
#!/bin/bash
echo "stub-engine: $*"
echo "stub-input-watch: t=0 frame=1 phase=present"
sleep 4
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
