#!/bin/bash
# Fresh processes, fixed pose and isolated logs. Normal-play evidence is preserved.
run_performance_comparison() {
    local capture_root case_name scene head shader order status
    local original_state="$STATE_DIR" original_memory="${OPENUG2_MEMORY_LOG-}"
    local original_conf="${XDG_CONFIG_HOME:-$STATE_DIR/conf}" original_save="${XDG_DATA_HOME:-$STATE_DIR/save}"
    local original_config_dir="${OPENUG2_CONFIG_DIR:-$STATE_DIR/conf/openug2}"
    status=0
    capture_root=$(mktemp -d "$STATE_DIR/perf-compare-XXXXXX") || return 1
    echo "comparison: logs in $capture_root"
    {
        echo 'Eight fresh 120-frame cases; fixed STREAML4RA spawn/heading/camera; deterministic AI ticks.'
        echo 'Vehicle shadows removed in every case; display, world range and geometry unchanged.'
        echo 'Exclude first 20 frames and final screenshot frame. Actual settings are recorded in each CSV.'
        echo 'Over 5% native OR optimized repeat drift invalidates performance attribution.'
        sha256sum "$RUNDIR/nfsu2" 2>/dev/null || true
    } > "$capture_root/manifest.txt"
    . "$PORTDIR/runtime-diagnostics.sh"
    for case_name in native-a scene-480 scene-320 headlights world-shader depth optimized-repeat native-b; do
        scene=320x240;head=reference;shader=legacy;order=legacy
        case "$case_name" in
            native-a|native-b) scene=native ;;
            scene-480) scene=480x360 ;;
            headlights) head=fast ;;
            world-shader) head=fast;shader=auto ;;
            depth|optimized-repeat) head=fast;shader=auto;order=depth ;;
        esac
        STATE_DIR="$capture_root/$case_name"
        if ! mkdir -p "$STATE_DIR/conf/openug2" "$STATE_DIR/save"; then status=1;break;fi
        # Every process starts from the same normal settings/profile snapshot.
        # Writes stay inside the case; normal saves/settings are never changed.
        if [ -d "$original_conf" ] && ! cp -R "$original_conf/." "$STATE_DIR/conf/"; then status=1;break;fi
        if [ -d "$original_save" ] && ! cp -R "$original_save/." "$STATE_DIR/save/"; then status=1;break;fi
        export XDG_CONFIG_HOME="$STATE_DIR/conf" XDG_DATA_HOME="$STATE_DIR/save"
        export OPENUG2_CONFIG_DIR="$STATE_DIR/conf/openug2"
        find "$STATE_DIR/conf" "$STATE_DIR/save" -type f -exec sha256sum {} \; > "$STATE_DIR/config-before.sha256"
        export OPENUG2_MEMORY_LOG="$STATE_DIR/memory.csv"
        echo "$case_name requested scene=$scene headlights=$head shader=$shader order=$order" >> "$capture_root/manifest.txt"
        run_monitored_engine "$DATA" --track STREAML4RA \
            --resolution "${OPENUG2_RESOLUTION:-640x480}" \
            --tier "${OPENUG2_TIER:-baseline}" \
            --load-radius "${OPENUG2_LOAD_RADIUS:-500}" \
            --view-distance "${OPENUG2_VIEW_DISTANCE:-450}" \
            --vehicle-quality "${OPENUG2_VEHICLE_QUALITY:-low}" \
            --weather-quality "${OPENUG2_WEATHER_QUALITY:-low}" \
            --texture-detail "${OPENUG2_TEXTURE_DETAIL:-2}" \
            --hud --hud-fit "${OPENUG2_HUD_FIT:-4:3}" \
            --heading 0 --perf-still --frames 120 --shot "$STATE_DIR/scene.png" \
            --perf-csv "$STATE_DIR/performance.csv" --perf-vsync 1 \
            --scene-resolution "$scene" --headlight-shadows "$head" \
            --world-shader "$shader" --world-order "$order" > "$STATE_DIR/engine.log" 2>&1
        status=$?
        echo "$case_name exit=$status" | tee -a "$capture_root/manifest.txt"
        sync
        if [ "$status" -ne 0 ]; then break; fi
    done
    STATE_DIR="$original_state"
    export XDG_CONFIG_HOME="$original_conf" XDG_DATA_HOME="$original_save" OPENUG2_CONFIG_DIR="$original_config_dir"
    if [ -n "$original_memory" ]; then export OPENUG2_MEMORY_LOG="$original_memory"; else unset OPENUG2_MEMORY_LOG; fi
    echo "comparison: finished status=$status; evidence in $capture_root"
    return "$status"
}
