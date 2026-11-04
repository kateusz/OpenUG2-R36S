OpenUG2 for ArkOS / dArkOS (R36S)
=================================

DEVICE STATUS (2026-11-04)
  User confirmed the game runs correctly and responds to actions on a physical
  R36S with the 500 m load / 450 m draw preset. FPS remains very low.
  Performance investigation is deferred; no FPS improvement is claimed.

WHAT THIS IS
  An open-source reimplementation of the Need for Speed Underground 2 engine,
  built for aarch64 + OpenGL ES. It is a prototype: there is no career, no
  frontend and no shop. It boots straight into a race with a fixed car/track.

HOW TO START
  EmulationStation -> Ports -> "NFSU2" (or "OpenUG2").
  Both entries start the same game. If one of them only opens a folder or
  does nothing, use the other one.
  The screen shows "OpenUG2: starting..." while the world loads; on the R36S
  the first load takes about 20 seconds.

CONFIGURE
  Edit config.txt (next to this file) and restart the port:
    OPENUG2_RESOLUTION   screen mode, default 640x480
    OPENUG2_TIER         baseline / ordinary / full
    OPENUG2_LOAD_RADIUS  world residency in metres, default 500
    OPENUG2_VIEW_DISTANCE drawing distance in metres, default 450
    OPENUG2_VEHICLE_QUALITY   low / medium / high
    OPENUG2_WEATHER_QUALITY   low / medium / high
    OPENUG2_HUD_FIT      4:3 / wide
    OPENUG2_ARGS         extra arguments, e.g. --car SKYLINE --track STREAML4RA
    OPENUG2_RESOURCE_MONITOR  1 records memory and kernel evidence, 0 disables

  The initial 700 m load / 600 m draw device run ended in SIGKILL (status 137).
  The current 500 m / 450 m preset reduces memory pressure. An out-of-memory
  kill is suspected; a kernel OOM message is required to confirm the cause.

FILES
  nfsu2                  the game binary (copied to /tmp at launch)
  data\                  game data: CARS, TRACKS, GLOBAL, FRONTEND,
                         LANGUAGES, SOUND
  conf\openug2\          settings (controls, graphics)
  save\                  profiles and saved data
  last-run.log           output of the last game launch
  boot.log               one line per launch; proves the launcher started
  input-info.log         pad discovery output (Diagnostics port)
  instance-audit.log     data completeness audit (Diagnostics port)
  resource.log           one-second process RSS/high-water/swap and system RAM
  system-before.log      memory/OOM counter and kernel messages before launch
  system-after.log       same snapshot after exit, including a killed engine

  If the card is read-only for the device user, the same conf/save/logs are
  kept in ~/.local/share/openug2 instead, and copied back to this folder when
  possible. The launch log records which location was used.

TROUBLESHOOTING
  If the game exits immediately, read last-run.log. The launcher prints the
  engine's startup output on screen as well, so a failed launch is no longer
  a blank screen.
  If the pad does not respond, run "OpenUG2 Diagnostics" from the Ports menu,
  then copy input-info.log off the SD card.

  The current bring-up config adds --input-monitor 120. In last-run.log:
    input-watch:  independent one-second heartbeat (frame/event counts, phase)
    input-sample: raw axes and physical buttons
    input-state:  resolved driving inputs and pause/race/simulation state
  If frames stop increasing, the phase locates the blocked loop stage (e.g.
  present = waiting to present a frame). If frames advance and raw inputs
  change, inspect requested-throttle/driving/paused instead. The monitor stops
  logging after 120 seconds; it does not stop the game. Remove the argument
  from OPENUG2_ARGS after diagnosis.
  Runtime evidence is collected outside the engine, so a killed process can
  still leave system-after.log. Kernel messages may be denied by firmware;
  the log records that error. Exit 137 alone identifies SIGKILL, not its cause.
