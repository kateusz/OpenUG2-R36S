# OpenUG2 on R36S — full handoff

Last updated: 2026-11-04
Author: previous AI session (OpenCode)
Repository: `C:\Users\matku\repos\OpenUG2`
Branch: **`arm64-r36s`**
Fork: https://github.com/kateusz/OpenUG2-R36S

**Latest status - 2026-11-04:** the final 213532EA build has a returned physical
run with user-confirmed normal gameplay and higher FPS. Logs show exit 0, no
OOM messages, and 6.86 FPS (earlier capture 3.50). No replacement resident
activated: preparation takes 21-28 seconds and results become obsolete.
Streaming acceptance and GPU/display attribution remain open. The completed Performance run shows strong pixel sensitivity and rendering
completion waits; vsync-off was refused. See PERFORMANCE-RETURN-20261104.md
and PERFORMANCE-COMPARISON-20261104.md.

This document is written so a fresh agent can continue without re-deriving anything.
Read the latest status above first; older blocker sections are historical.

---

## 1. TL;DR status

| Area | State |
|---|---|
| aarch64 + GLES build | **works** (Docker/QEMU, Ubuntu 20.04 arm64) |
| Game data on card | **complete and verified** (~1.19 GB) |
| Port appears in ArkOS menu | **works** |
| Diagnostics port | **works** (pad + data audit logs written on device) |
| Game launches and renders | **works** (user confirmed: "it runs the game") |
| **Input** | **Works on physical R36S — user confirmed actions respond** |
| **Runtime stability** | **OOM during sustained driving in the last physical capture; new candidate awaiting handheld acceptance** |
| Performance | **3.50 FPS in returned capture; native CPU improvement measured, R36S follow-up pending** |

**Earlier 2026-11-04 investigation (historical):** a diagnostic build and a verified four-axis SDL
default fix have been deployed to the card. The user clarified that the image
was frozen as well as the controls. That blocker was subsequently cleared by
the user's successful physical-device smoke test recorded above.
See section 15 before following the older proposed debugging steps below.
**Latest device follow-up:** see section 16. The default preset is now 500 m
load / 450 m draw with memory and kernel evidence capture enabled.

Startup/input acceptance remains valid. Sustained driving failed in the returned
physical capture. Follow PERFORMANCE.md for the new candidate and its evidence.

---

## 2. Goal

Run [OpenUG2](https://github.com/whoismept/OpenUG2) — an open-source reimplementation
of the Need for Speed Underground 2 engine — **natively** on an R36S handheld
(aarch64 + Mali-G31 + OpenGL ES), using the retail game data at
`D:\Need for Speed Underground 2`.

Constraints / decisions already made (do not revisit without reason):

- **Native ARM64 + GLES2 build.** box64 / GL4ES / x86 emulation was rejected as too slow.
- **Build inside Docker with QEMU arm64 emulation**, Ubuntu 20.04 (glibc 2.31 baseline).
  Debian bullseye was abandoned because its security pool 404s.
- **`make gles`** → `-DN2_GLES`, GLES2 / GLSL ES 1.00.
- `libpng` and `libcrypto` are **statically** linked; SDL2, GLESv2, zlib, libm, libc stay dynamic.
- **No full game**: no career, no frontend, no shop. It boots straight into free roam.
  Car/track are chosen with launch arguments.
- Install manually under `/roms/ports` (no PortMaster packaging).

---

## 3. Hardware and firmware facts (measured on the device)

- Device: **R36S**, SoC RK3326, GPU **Mali-G31**, 640×480 screen, 1 GB RAM.
- Firmware: **ArkOS / dArkOS**. Ports live in `/roms/ports`.
- Card partition mounted on the PC as **`F:`**, label `EASYROMS`, filesystem **NTFS**,
  ~4.3 GB free. It is the only partition visible from Windows.
- On the device the card is mounted at `/roms` (so the port dir is `/roms/ports/OpenUG2`).
- **The card is writable from the device** (proved: logs written, Gothic saves written).
- Device clock is wrong before NTP: files created at boot show **2024-09-02**
  (that is not a stale file — it is the device's default date).

### Device SDL / pad (from `input-info.log`)

```
input-info: SDL 2.30.3
input-info: video=KMSDRM audio=(none)
input-info: joysticks=1
input-info: [0] name="GO-Super Gamepad"
input-info: [0] guid=1900bb3e4b4800000011000000010000 type=gamecontroller gamecontroller=yes
input-info: [0] vendor=0x484b product=0x1100
input-info: [0] axes=4 buttons=17 hats=0 balls=0
input-info: [0] mapping=1900bb3e4b4800000011000000010000,GO-Super Gamepad,x:b2,a:b1,b:b0,y:b3,back:b12,start:b13,dpleft:b10,dpdown:b9,dpright:b11,dpup:b8,leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,leftstick:b14,rightstick:b15,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,
```

Key points:

- The pad **is** recognised as a GameController with a full mapping, supplied by
  `/opt/inttools/gamecontrollerdb.txt` (the launcher prefers that file).
- 4 axes, 17 buttons, **0 hats** (the d-pad is reported as buttons b8–b11).
- `audio=(none)` because `--input-info` only initialises video + gamecontroller.

---

## 4. Repo state

```
branch: arm64-r36s
base:   424da39 Merge pull request #125 from whoismept/development
remote: https://github.com/whoismept/OpenUG2.git
```

Uncommitted:

```
 M .gitignore          (added /dist/)
 M src/main.c          (input-info early exit, --load-radius, --view-distance, progress)
?? tools/arm64/        (whole new build/deploy/port toolchain)
```

`dist/` and `nfsu2` are gitignored. The branch has **not** been pushed.
If you commit: `git add .gitignore src/main.c tools/arm64 && git commit`.

### Engine changes made in `src/main.c`

1. **`--input-info` now exits before any data access.** It used to run after
   `world_load_ex()`, which loads ~1.2 GB and takes minutes on the handheld.
   Now it runs immediately after argument parsing (SDL init → dump → `SDL_Quit` → return).
2. **`--load-radius M`** (200..5000, default 1400) and **`--view-distance M`**
   (100..5000, default tier-derived). `--load-radius` scales the whole
   `WResidentPolicy` (`resident 1400 / draw 933 / safety 67 / cell 400`) by
   `load_radius / 1400`, so `world_resident_policy_valid()` still holds.
3. **Startup progress on stderr:** `world: loading …`, `world: loaded N meshes in X s`,
   `video: opening …`, `video: ready`.
4. `--tier baseline` is unchanged (fixed 700 m draw gate) but `--view-distance`
   now overrides it.

`--input-info` implementation is near the top of `src/main.c`
(`joy_type_name`, `dump_input_info`); the early-exit block is right after the
argument loop (`else dataroot = argv[i]; }`).

---

## 5. Build

Everything lives in `tools/arm64/`.

| File | Purpose |
|---|---|
| `Dockerfile` | Ubuntu 20.04 arm64 build image (glibc 2.31) |
| `build.ps1` | build image + run container + stage payload into `dist/OpenUG2-r36s/` |
| `build-in-container.sh` | runs inside the container: CRLF→LF copy, `make gles`, ELF verify |
| `deploy.ps1` | copy payload + game data to `F:\ports` (additive robocopy) |
| `port/` | the port payload (launchers, config, README, pad DB) |

Build command (already automated):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\build.ps1
```

- Docker image name: `openug2-arm64-builder`, built with `--platform linux/arm64`.
- Full build takes **~30 min** under QEMU. The apt layer is cached; only the C
  compile is slow.
- `-StageOnly` re-stages `tools/arm64/port/*` into `dist/` **without recompiling** —
  use this for launcher/config changes.

Inside the container the actual build is:

```sh
make gles CC=gcc \
  UGUI_IMAGE_LIBS='-Wl,-Bstatic -lpng16 -lcrypto -Wl,-Bdynamic -ldl -lpthread'
```

(The Makefile appends `-lz -lm`.)

### Current binary

```
dist/OpenUG2-r36s/nfsu2
size:   1,676,568 bytes
sha256: 467D74A4E05AE01F4769FD2FB6EA0A3A8F6621A074D041FA8A4E4C19EC70A03A
ELF:    aarch64, dynamically linked, interpreter /lib/ld-linux-aarch64.so.1
NEEDED: libSDL2-2.0.so.0, libGLESv2.so.2, libpthread.so.0, libz.so.1,
        libm.so.6, libc.so.6, ld-linux-aarch64.so.1
max GLIBC symbol: GLIBC_2.29
```

The same binary is on the card at `F:\ports\OpenUG2\nfsu2` (hash verified).

### Container smoke tests that already pass

```powershell
# pad path, no data needed, exits immediately
docker run --rm --platform linux/arm64 -v "C:/Users/matku/repos/OpenUG2/dist/OpenUG2-r36s:/out:ro" `
  openug2-arm64-builder sh -c 'cp /out/nfsu2 /tmp/nfsu2 && chmod +x /tmp/nfsu2 && SDL_VIDEODRIVER=dummy /tmp/nfsu2 --input-info'

# data audit at the handheld radius
docker run --rm --platform linux/arm64 `
  -v "C:/Users/matku/repos/OpenUG2/dist/OpenUG2-r36s:/out:ro" `
  -v "D:/Need for Speed Underground 2:/game:ro" `
  openug2-arm64-builder sh -c 'cp /out/nfsu2 /tmp/nfsu2 && chmod +x /tmp/nfsu2 && /tmp/nfsu2 /game --instance-audit --load-radius 700 --view-distance 600'
```

---

## 6. Deployment to the SD card

```powershell
# everything (binary, launchers, data) — additive, never deletes
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\deploy.ps1

# only binary + launchers (normal after a rebuild)
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\deploy.ps1 -SkipData

# only the 1.19 GB game data
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\deploy.ps1 -DataOnly
```

`deploy.ps1` refuses to run if `F:\ports` is missing, and checks free space before
copying data. It uses `robocopy /E` (additive), so saves/logs on the card survive.

### Card layout

```
F:\ports\
├─ NFSU2.sh                     <- Ports menu entry (recommended name)
├─ OpenUG2.sh                   <- Ports menu entry (same game)
├─ OpenUG2 Diagnostics.sh       <- pad + data diagnostics entry
└─ OpenUG2\
   ├─ nfsu2                     <- the binary (copied to /tmp at launch)
   ├─ launch.sh                 <- the real launcher
   ├─ config.txt                <- user-editable options
   ├─ gamecontrollerdb.txt      <- fallback pad DB (last resort only)
   ├─ README.txt                <- user-facing doc
   ├─ boot.log                  <- breadcrumb, written by launch.sh (created on device)
   ├─ last-run.log              <- engine output of the last game run
   ├─ input-info.log            <- Diagnostics: pad report
   ├─ instance-audit.log        <- Diagnostics: data audit
   ├─ conf\openug2\             <- settings (XDG_CONFIG_HOME)
   ├─ save\                     <- profiles (XDG_DATA_HOME)
   └─ data\
      ├─ CARS\       724.4 MB, 231 files
      ├─ TRACKS\     386.6 MB, 502 files
      ├─ GLOBAL\      21.8 MB,  29 files
      ├─ FRONTEND\    35.7 MB,  16 files
      ├─ LANGUAGES\    4.7 MB,  26 files
      └─ SOUND\       21.3 MB, 284 files
```

Only those six data directories are copied. `movies/`, `cars/` etc. from the
retail install are **not** needed.

---

## 7. Launcher architecture (learned the hard way)

### 7.1 Why shims

The first two device tests produced **no logs at all**: the menu entry never ran.
The working Gothic port on the same card uses a tiny shim (`/roms/ports/gothic.sh`)
that immediately `exec`s a `launch.sh` inside the game folder. OpenUG2 now does the
same:

```
F:\ports\NFSU2.sh
F:\ports\OpenUG2.sh
        └─ exec bash "$PORTS/OpenUG2/launch.sh" "$@"
```

Both shims also handle being copied *inside* the port folder (they fall back to
`$PORTS/launch.sh`). `launch.sh` additionally detects a nested invocation and
climbs one level if `nfsu2` is not where it expects.

**If the menu entry ever "does nothing" again, check `F:\ports\.openug2-shim.log`
and `F:\ports\OpenUG2\boot.log` first.** They are written before anything else and
tell you exactly how far it got.

### 7.2 NTFS permissions — critical, bit us twice

`F:\ports\OpenUG2` was created from Windows and inherited
`Authenticated Users: Modify`. ArkOS's NTFS mount maps its user such that this is
**not** enough: every write failed, the launcher died on its first redirect, and
the screen stayed blank with no log.

Fix that must be re-applied to **any new directory or file created from Windows**:

```powershell
icacls "F:\ports\OpenUG2" /grant "*S-1-1-0:(OI)(CI)F" /T /C
icacls "F:\ports\NFSU2.sh" /grant "*S-1-1-0:F"
icacls "F:\ports\OpenUG2.sh" /grant "*S-1-1-0:F"
icacls "F:\ports\OpenUG2 Diagnostics.sh" /grant "*S-1-1-0:F"
```

Verify:

```powershell
(Get-Acl 'F:\ports\OpenUG2').Access | Where-Object { $_.IdentityReference -match 'Everyone|Wszyscy' }
```

Device-created folders (e.g. `F:\ports\gothic\saves`) carry `Everyone: FullControl`
by themselves — that was the clue that solved it.

### 7.3 Writable state directory

`launch.sh` probes, in order:

1. `$PORTDIR` (`/roms/ports/OpenUG2`) — normal case, state is easy to read from a PC
2. `$HOME/.local/share/openug2`
3. `/tmp/openug2-state`

The probe creates a real file, because a directory can exist and still be read-only.
The chosen path is printed on screen and recorded in `last-run.log` (`state:` line).
If state ended up in `$HOME`, the log is copied back to the port folder at exit.

Exports:

```
XDG_CONFIG_HOME=$STATE_DIR/conf
XDG_DATA_HOME=$STATE_DIR/save
OPENUG2_CONFIG_DIR=$STATE_DIR/conf/openug2
```

### 7.4 Other launcher behaviour

- Binary is copied to `/tmp/openug2/nfsu2` and `chmod +x` — the EASYROMS partition
  cannot be trusted to keep the executable bit.
- SDL drivers are **not** forced; `config.txt` can override via
  `OPENUG2_VIDEODRIVER` / `OPENUG2_AUDIODRIVER`.
- Pad DB search order: `~/.config/SDL-GameControllerDB/gamecontrollerdb.txt`,
  `/opt/inttools/gamecontrollerdb.txt`, port `gamecontrollerdb.txt`,
  PortMaster DBs. First non-empty file wins and is exported as
  `SDL_GAMECONTROLLERCONFIG_FILE`.
- Output is mirrored to the screen with `tee` so a failed launch is never a blank screen.
- `sync` is called before starting the engine so `last-run.log` survives a hard hang.

---

## 8. Verified device results (evidence)

### 8.1 Diagnostics (works)

`F:\ports\OpenUG2\input-info.log` — full pad report quoted in section 3, exit status 0.

`F:\ports\OpenUG2\instance-audit.log` — the important lines:

```
world: loading STREAML4RA (radius 700 m)...
world: loaded 39419 meshes in 17.8 s
INSTANCE AUDIT bundle=STREAML4RA home=711 regions=130/351 radius=700.0
INSTANCE counts seen=48270 in-range=10649 meshes=30704 missing=8 own-matrix=5512 instanced-ground=144 rejected=0 triangles=910859 vista=102
INSTANCE finite-coordinate-failures=0
INSTANCE support spawn=(-13.872,40.967,8.270) reference=8.270 category=ROAD
```

Interpretation:

- 700 m loads **130 of 351 regions**, 30,704 meshes, **910,859 triangles** (vs 275
  regions / 57,319 meshes / 1.75 M triangles at the 1400 m default).
- 17.8 s load on the RK3326 — acceptable, and the launcher prints progress meanwhile.
- Spawn is on a real road, all coordinates finite.
- `missing=8` is expected/benign (known missing placements in the retail data).

### 8.2 Game (runs, input dead)

User report after the shim fix: **"it runs the game, but nothing happened when I use
any key or analog"**. The continuation captured `last-run.log`; see section 15.

---

## 9. Current blocker: input does nothing

### 9.1 What is already ruled out

- **Pad not detected** — no: SDL sees it, names it, maps it (section 3).
- **Pad DB missing** — no: `/opt/inttools/gamecontrollerdb.txt` provides a full mapping.
- **Permissions on `/dev/input`** — no: `--input-info` read the device as the same user.
- **Launcher/state** — no: diagnostics wrote logs; the game launches.
- **SDL KMS/DRM focus** — SDL 2.30.3's `KMSDRM_CreateWindow()` calls
  `SDL_SetKeyboardFocus(window)` (verified in upstream source), so the
  "no focus ⇒ joystick events filtered" theory is **unlikely** (but see 9.3 H3).

### 9.2 Relevant engine code

| Location | What it does |
|---|---|
| `src/main.c` ~4009 | `SDL_Init(SDL_INIT_VIDEO \| SDL_INIT_AUDIO \| SDL_INIT_GAMECONTROLLER)` |
| `src/main.c` ~4041 | opens the first controller with `SDL_GameControllerOpen()` |
| `src/main.c` ~4070 | `cx_init(&g_cx)` — opens joysticks again via `controls_sdl.c` |
| `src/main.c` 6681 | `while (SDL_PollEvent(&e))` — main event loop |
| `src/main.c` 6689 | pause via `SDL_CONTROLLERBUTTONDOWN` + `SDL_CONTROLLER_BUTTON_START` |
| `src/main.c` 7683 | `const Uint8 *ks = SDL_GetKeyboardState(NULL);` |
| `src/main.c` 7898 | `if (race_state == 1 && driving_keys) throttle = cx_throttle(...)` |
| `src/main.c` 7904 | `steer = cx_steer(&g_cx, ks)` |
| `src/controls_sdl.c` 28–52 | `open_pad()` — `SDL_JoystickOpen`, fills `present`, GUID, axes mask |
| `src/controls_sdl.c` 164–171 | `pad_action_down()` — iterates `binding[0..31]` |
| `src/controls_bind.c` 175–206 | `pad_default_bindings()` — A=throttle, B=brake, d-pad=steer |
| `src/player_motion.h` | holds the car until a throttle is requested (clears on first press) |

Default bindings for this pad (17 buttons, 4 axes, no hats):

```
binding[0] = BRAKE        (raw button 0 = B)
binding[1] = THROTTLE     (raw button 1 = A)
binding[2..9] = 9,10,6,5,8,7,22,19
d-pad (POV slots, but this pad has 0 hats -> these do nothing)
```

Steering axis: `CB_ROLE_STEER = 4 * AX_X` → raw axis 0 = left stick X.
Combined pedal axis: `CB_ROLE_COMBINED` — **see bug in section 10**.

So on paper: left stick steers, **A accelerates, B brakes**, Start should open the
pause menu. If none of that works, input is not reaching the engine at all.

### 9.3 Hypotheses, ranked

**H1 — events never reach the engine (loop starved or not pumping).**
If the render loop blocks (e.g. `SDL_GL_SwapWindow`/vsync on KMSDRM never returns,
or the frame takes seconds), `SDL_PollEvent` is never called and everything is dead,
including pause. Test: run with `--perf-csv F:\ports\OpenUG2\perf.csv` (see
`src/main.c` 2449/2712) and check whether rows accumulate; or add an FPS line to
stderr. If no rows → this is it.

**H2 — `cx_init()` sees no pad (`present == 0`).**
`open_pad()` can bail if `SDL_JoystickOpen()` fails. The controller is already opened
at main.c:4041, but SDL refcounts, so this *should* work. Test: print
`g_cx.b.pad[0].present`, `info.buttons`, `info.axes_mask` right after `cx_init`.
Cheap and decisive.

**H3 — SDL focus filter.**
Even though KMSDRM sets focus, forcing the hint is harmless and rules it out:
`SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");` before `SDL_Init`.
Also try `SDL_GameControllerEventState(SDL_ENABLE)`.

**H4 — the pad is grabbed by another process (gptokeyb / retrogame).**
Unlikely because `--input-info` saw it, but ArkOS starts `gptokeyb` for some ports.
Check with `ps aux | grep -E 'gptokeyb|retrogame'` over SSH while the game runs.

**H5 — the engine is in a non-driving state.**
`race_state` is `1` in normal builds (`OPENUG2_MENU` not defined), and
`player_motion_tick` clears `awaiting_drive` on the first throttle request.
Low probability, but a one-line `fprintf` of `race_state`, `throttle`, `steer`,
`g_ride_ready` per second settles it.

### 9.4 Recommended next step (do this first)

Add a **live input monitor** to the engine, gated behind a flag, then rebuild once:

```
--input-monitor SECONDS   print pad 0 axes/buttons + keyboard throttle key state
                          to stderr once per second
```

Implementation sketch (in the main loop, cheap):

```c
static Uint32 mon_next = 0;
if (input_monitor && SDL_TICKS_PASSED(SDL_GetTicks(), mon_next)) {
    mon_next = SDL_GetTicks() + 1000;
    fprintf(stderr, "mon: present=%d axes=%d,%d,%d,%d buttons=0x%05x race=%d thr=%.2f steer=%.2f\n",
            g_cx.b.pad[0].present,
            SDL_JoystickGetAxis(g_cx.joy[0],0), SDL_JoystickGetAxis(g_cx.joy[0],1),
            SDL_JoystickGetAxis(g_cx.joy[0],2), SDL_JoystickGetAxis(g_cx.joy[0],3),
            (unsigned)cx_sample_buttons(&g_cx,0), race_state, throttle, steer);
    fflush(stderr);
}
```

Then run on device with `OPENUG2_ARGS="--input-monitor 60"` and read
`last-run.log`:

- **values change when you press** → engine receives input; the bug is downstream
  (bindings/state) → go to H5 / check `binding[]` values.
- **all zeros** → SDL/evdev level → H1/H2/H3/H4.

Also print `SDL_GetCurrentVideoDriver()` and `SDL_GetKeyboardFocus() != NULL` once.

### 9.5 Things to ask the user for

- `F:\ports\OpenUG2\last-run.log` after a game run (it should exist now).
- Whether the **Start** button opens the pause menu (this is a single decisive test:
  pause is handled directly from the event loop, before any driving logic).
- Whether a **USB keyboard** works (arrow keys / W / Esc), if they have one.

---

## 10. Engine bug found while investigating (real, worth fixing)

`src/controls_bind.c`, `cb_pad_defaults()` lines ~228–232:

```c
int slot = popcount(info->axes_mask) < 4 ? AX_Y : AX_RZ;
...
d->axis[CB_ROLE_COMBINED] = 4 * slot;
```

`AX_RZ` is **slot 5**. The R36S pad reports **4 axes (0–3)**, so
`popcount == 4` → `slot = 5` → `axis_unit()` returns 0 forever because
`slot >= SDL_JoystickNumAxes()`. Result: the analog combined throttle/brake axis is
dead; only the A/B buttons work.

This is **not** the cause of "nothing at all works" (buttons would still work), but
it should be fixed:

```c
int slot = AX_Y;
if (has_axis(info, AX_RZ))      slot = AX_RZ;
else if (has_axis(info, AX_Z))  slot = AX_Z;
```

For a standard gamepad layout the combined pedal should be the **right stick Y**,
which on this pad is axis 3 (SDL order: 0=leftx, 1=lefty, 2=rightx, 3=righty).
Note the engine's `AX_*` enum uses DirectInput-ish indices, not SDL order — verify
against `open_pad()` which stores raw SDL indices.

---

## 11. Command cheat sheet

```powershell
# --- build ---
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\build.ps1 -StageOnly

# --- deploy ---
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\deploy.ps1 -SkipData
powershell -NoProfile -ExecutionPolicy Bypass -File tools\arm64\deploy.ps1

# --- verify card payload hashes ---
foreach ($f in @('OpenUG2.sh','NFSU2.sh','launch.sh','OpenUG2 Diagnostics.sh','config.txt','README.txt','nfsu2')) {
  $a=(Get-FileHash "dist\OpenUG2-r36s\$f" -Algorithm SHA256).Hash
  $b=(Get-FileHash "F:\ports\OpenUG2\$f" -Algorithm SHA256).Hash
  "{0,-26} {1}" -f $f, $(if ($a -eq $b) {'match'} else {'DIFFER'})
}

# --- re-apply NTFS permissions after creating anything from Windows ---
icacls "F:\ports\OpenUG2" /grant "*S-1-1-0:(OI)(CI)F" /T /C

# --- read device logs ---
Get-Content 'F:\ports\OpenUG2\input-info.log'
Get-Content 'F:\ports\OpenUG2\instance-audit.log'
Get-Content 'F:\ports\OpenUG2\last-run.log'
Get-Content 'F:\ports\OpenUG2\boot.log'
Get-Content 'F:\ports\.openug2-shim.log'

# --- bash syntax check (Git Bash) ---
& "C:\Program Files\Git\bin\bash.exe" -n tools/arm64/port/launch.sh
```

---

## 12. Config currently on the card (`config.txt`)

```sh
OPENUG2_RESOLUTION=640x480
OPENUG2_TIER=baseline
OPENUG2_LOAD_RADIUS=700
OPENUG2_VIEW_DISTANCE=600
OPENUG2_VEHICLE_QUALITY=low
OPENUG2_WEATHER_QUALITY=low
OPENUG2_TEXTURE_DETAIL=2
OPENUG2_HUD_FIT=4:3
OPENUG2_ARGS="--input-monitor 120"
# OPENUG2_VIDEODRIVER=kmsdrm
# OPENUG2_AUDIODRIVER=alsa
```

Resulting command line:

```
nfsu2 /roms/ports/OpenUG2/data --resolution 640x480 --tier baseline
      --load-radius 700 --view-distance 600
      --vehicle-quality low --weather-quality low --texture-detail 2
      --hud --hud-fit 4:3
```

Default car/track: **HUMMER** / **STREAML4RA** (override with
`OPENUG2_ARGS="--car SKYLINE --track STREAML4RA"`).

Tuning levers if performance/RAM are bad: lower `OPENUG2_LOAD_RADIUS` to 500 and
`OPENUG2_VIEW_DISTANCE` to 450 (the launcher forwards them; `launch.sh` also passes
them to the diagnostics audit).

---

## 13. Known gotchas / lessons

- **Never trust a Windows-created folder on this card.** Always apply the ACL fix.
- `F:` can drop off Windows transiently; check `Test-Path F:\ports` before deploying.
- The repo checkout is **CRLF**; the container strips CRLF on a throwaway copy.
  Port files are staged **LF-only, no BOM** — keep it that way (`config.txt` is sourced by bash).
- QEMU arm64 compile is slow; use `-StageOnly` for launcher-only changes.
- The engine loads the **world before creating the window**; the launcher's progress
  lines are the only feedback during those ~18 s.
- `--input-info` and `--instance-audit` are both GL-free and safe to run on device.
- Don't set `SDL_VIDEODRIVER` unless debugging — auto-detect picks `KMSDRM` correctly.
- ArkOS hides file extensions in the Ports list: `OpenUG2.sh` and the `OpenUG2` folder
  both appear as "OpenUG2". That is why `NFSU2.sh` exists.

---

## 14. Open TODO

1. **Fix input** (section 9) — the only functional blocker.
2. Capture `last-run.log` from a device run and check FPS / whether the loop advances.
3. Once input works: verify driving feel, camera, HUD at 640×480, audio.
4. Measure RAM at 700 m (`free -m` over SSH while running) and tune radius if needed.
5. Commit `arm64-r36s` (`.gitignore`, `src/main.c`, `tools/arm64/`) and optionally push.
6. Completed in the continuation: added the R36S mapping line to `tools/arm64/port/gamecontrollerdb.txt`
   as a true fallback (currently the `/opt` DB is always preferred):

   ```
   1900bb3e4b4800000011000000010000,GO-Super Gamepad,x:b2,a:b1,b:b0,y:b3,back:b12,start:b13,dpleft:b10,dpdown:b9,dpright:b11,dpup:b8,leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,leftstick:b14,rightstick:b15,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,
   ```

---

## 15. Codex continuation — 2026-11-04

### Current state and evidence

- The user clarified: **nothing responded and the image was stationary**. This
  makes blocked/very slow frame processing a priority hypothesis, not a proven cause.
- Captured the existing card log before deployment. It reaches scene setup and
  the stance print, with no clean exit status and a NUL-filled tail. It contains
  no frame timings or live input, so it cannot locate the stall.
- Preserved original logs/config and the previous binary under
  `dist/input-investigation/` (`last-run.log`, `input-info.log`, `boot.log`,
  `config.txt`, `nfsu2-before`). `dist/` remains ignored.

### Implemented

- `--input-monitor SECONDS` (strict range 1..3600), integrated into `src/main.c`.
  It enables line-buffered stdout and logs startup SDL focus/event settings,
  opened pads, physical axes/buttons, resolved throttle/steering, race/pause/
  driving state, simulation steps, ride state and speed once per second.
- `src/input_monitor.h` also runs an **independent SDL thread**. Its
  `input-watch:` lines report elapsed time, frame/event counts and phase even
  if the main loop blocks. Phases: `setup`, `events`, `world`, `input`,
  `simulation`, `render`, `present`, `idle`. `render` includes frame-begin and
  draw work; `present` surrounds `rhi_frame_end` / `SDL_GL_SwapWindow`.
  The monitor stops logging after the requested interval, without stopping play.
- Fixed the verified four-axis issue **in `controls_sdl.c`**, leaving the retail
  DirectInput model in `controls_bind.c` intact. Before persisted bindings load,
  an assigned combined axis absent from raw SDL axes is replaced by mapped
  right-stick Y, raw axis 3 for an unmapped four-axis device, or available Y.
  Wheels and existing valid defaults stay intact; user profiles still override
  defaults. Existing saved invalid bindings are not automatically rewritten.
- The fallback DB now includes the exact GO-Super mapping measured on this card.
  System/user DB priority stays as before. No speculative SDL background-event,
  focus, driver or event-state changes were made.
- The port config now has `OPENUG2_ARGS="--input-monitor 120"` for the next run.
- During this trace the launcher flushes the log every three seconds from a
  separate worker, then stops that worker at game exit. This reduces loss of
  diagnostic lines after a forced reset; it cannot guarantee crash-proof NTFS.

### Validation

- Original control model: **1904 checks, 0 failed**.
- SDL virtual joystick test: **71 checks, 0 failed**. The new four-axis / 17-button
  / no-hat regression fails three checks on the original adapter (axis 5,
  right-Y throttle, right-Y brake) and passes after the fix. A fresh HOME isolates
  its profile from the six-axis virtual pad, which shares SDL's virtual GUID.
- `make input-monitor-test`: PASS with a dummy SDL window, deliberately blocked
  main thread, independent heartbeat, no-pad keyboard sample, cancellation and
  the 17th physical button (`buttons=0x00010000`).
- Launcher smoke: PASS for periodic flushing, worker cleanup, a leading-zero
  duration, and preservation of a simulated engine exit status 7. Bash syntax
  checks pass. The temporary check script is in `dist/input-investigation/`.
- GLES x86_64 Linux smoke using real retail data, Mesa offscreen, 320x240,
  radius 200 / draw 100, 3 frames: exit 0 and a rendered PNG. The monitor observed
  first-frame software rendering taking several seconds and then advancing.
  This is PC/container evidence, **not a device performance measurement**.
- Built ARM64 with the existing Ubuntu 20.04/QEMU pipeline. ELF dependencies
  unchanged; highest GLIBC version remains 2.29. ARM `--input-info` with dummy
  video passes, and invalid/missing monitor operands return status 2.
- Deployed with `deploy.ps1 -SkipData`, applied Everyone FullControl to the port
  and all three menu shims, and verified SHA256 for all eight payload files and
  the three top-level menu entries. Data, profiles, saves and old logs preserved.
  Manifest: `dist/input-investigation/deployment-verification.json`.

### Next physical test (required)

1. Safely eject the card and launch **NFSU2** from Ports.
2. After the scene appears, hold A for 3–5 seconds, move/hold the left stick for
   3–5 seconds, test right stick Y, then press Start. Allow another 30 seconds
   even if it appears frozen so the watchdog can capture progress/stall state.
3. Return the card and read `F:\ports\OpenUG2\last-run.log`. It is overwritten
   at the next launch, so preserve it before another run.
4. Repeated frame number + phase locates a stalled stage. Advancing frames with
   changing raw samples directs investigation to the resolved/gated inputs.
   Advancing frames with unchanged samples directs investigation toward SDL
   event delivery/focus/device access. Do not claim the full input/freeze issue
   fixed until this device result is captured.

Changes remain uncommitted on `arm64-r36s`. In addition to the earlier dirty
files, this continuation changes `Makefile`, `src/controls_sdl.c`,
`tools/controls_sdl_test.c`, and adds `src/input_monitor.h` and
`tools/input_monitor_test.c`. No push was performed.

---

## 16. Device crash follow-up — 2026-11-04

The user reported a crash and confirmed that the game **closed by itself**,
without an exit shortcut or forced close. The card was available as F:.

### What the device log proves

- Correct deployed binary: SHA256 remains
  `467D74A4E05AE01F4769FD2FB6EA0A3A8F6621A074D041FA8A4E4C19EC70A03A`.
- World radius 700 / draw 600; 39,419 loaded meshes; world load 17.6 s.
- KMSDRM focus is active, joystick/controller events enabled, pad attached;
  corrected `combined-axis=3` is present.
- The loop does advance: at monitor t=22.1 s it is on frame 1; t=26.1 s frame 9.
  This is only roughly 2 FPS over that short initial interval, not a permanent
  first-frame/swap lock.
- A raw button press reaches SDL (`buttons=0x00000004`, event count 7 -> 9).
  Raw button 2 is Look Back under current bindings, so resolved throttle 0 is
  expected. No axis motion or successful driving is demonstrated by this trace.
- The final line ends in **`exit status: 137`**: SIGKILL. This is not evidence of
  a segmentation fault. OOM is a leading hypothesis, but there is no kernel log
  or device RAM sample from this run to prove who sent SIGKILL.
- The original crash log, boot log and config were preserved in
  `dist/input-investigation/device-crash-20261104/` before deployment.

### Changes for the next run

- Reduced default residency **700 -> 500 m**, draw **600 -> 450 m**, in config
  and launcher fallbacks. This is a measured memory-pressure mitigation, not a
  claim that physical-device stability has been fixed.
- `OPENUG2_RESOURCE_MONITOR=1` enables `runtime-diagnostics.sh`. The launcher
  starts the engine, samples `/proc/<pid>/status` plus system memory every second
  to **resource.log**, and preserves the engine's real exit status after wait.
- **system-before.log** and **system-after.log** capture system memory, the
  kernel `oom_kill` counter when available, and the last 160 dmesg lines. The
  after snapshot runs outside the engine and therefore can survive an engine
  SIGKILL. Firmware may deny dmesg; that denial is recorded without sudo.
- Periodic flushing now includes resource evidence. The game ELF is unchanged;
  no ARM rebuild or game-data copy is required.

### Validation and limits

- `tools/arm64/test-runtime-diagnostics.sh`: PASS for normal exit 0, simulated
  failure 7, and an actual self-SIGKILL giving status 137. Checks include process
  memory samples, before/after kernel snapshots and diagnostic-worker cleanup.
- ARM64 container launcher smoke with the real binary and `--input-info`:
  PASS, exit 0, both system snapshots written. Bash syntax and diff checks pass.
- Native x86_64 GLES/Mesa profiling, real retail data, 640x480, HUMMER, HUD,
  low vehicle/weather detail, 60 frames, no driving:

  | Preset | Meshes | Peak process RSS | Exit |
  |---|---:|---:|---:|
  | 700 m load / 600 m draw | 39,419 | 706,244 KiB (689.7 MiB) | 0 |
  | 500 m load / 450 m draw | 26,745 | 574,272 KiB (560.8 MiB) | 0 |

  This is about **129 MiB / 19% less peak RSS**, with 32% fewer meshes. These
  final `memory-*-cpu-*` logs explicitly report **CPU DXT decode**, matching the
  handheld texture path. Mesa compression extensions including the sRGB variant
  were disabled and verified with a GL-extension probe. The 1.5 GiB container
  limits did not trigger OOM. Earlier `memory-700-*` / `memory-500-*` profiles
  used S3TC directly and are not the final comparison.
- These RSS values are **Linux PC/container evidence**, not device RAM/GPU
  measurements. They support choosing a smaller preset; they do not prove the
  reason for the R36S SIGKILL or that 500 m is safe on a 1 GiB handheld.

### Next device evidence

Launch NFSU2 once with the new 500/450 preset. If it closes again, return the
card and preserve **last-run.log, resource.log, system-before.log and
system-after.log** before another run. Check the minimum MemAvailable, process
VmRSS/VmHWM/VmSwap, OOM counter delta, and kernel `Out of memory` / `Killed
process ... nfsu2` / Mali messages. Successful survival, driving, pause, frame
rate and long-run streaming remain unverified on the physical device.

## 17. Performance evidence capture � 2026-11-04

The user confirmed working gameplay/controls, with very low FPS. Added default
launcher profiling using the existing --perf-csv, process CPU and frame wall
time, one-second CPU/system/sensor samples and periodic log flushing. Temporary
input tracing is disabled. Graphics and world distance settings stay unchanged.
See [PERFORMANCE.md](PERFORMANCE.md) for collection and interpretation. Physical
R36S performance capture and CPU/GPU attribution are pending.

## Device capture returned: OOM during driving - 2026-11-04

The instrumented physical run was killed by the kernel OOM handler during
world-prepare (exit 137). Startup and inputs work, but sustained driving is
currently failing. 217 saved frames averaged 3.50 FPS excluding the first
setup-contaminated row. Memory/streaming peak reduction takes priority over
render tuning. See PERFORMANCE-DEVICE-20261104.md for evidence and limits.

## 18. Memory/AI candidate and controlled comparison - 2026-11-04

The new candidate uses mapped archives, a bounded decoded prototype cache,
resident texture references with idle eviction, local ground GPU uploads,
retirement arena reclamation and accelerated ground-query blocks. Global CPU
navigation/collision ground is retained. Graphics preset remains 640x480,
500 m load / 450 m draw.

Native forced-decode testing under 700 MiB / no swap completed two exact-image
route transitions and a 6000-frame drive with three background swaps, exit 0
and no additional OOM events. The isolated AI section fell from 1.8514 to
0.9070 ms in a matching native comparison; no R36S FPS gain is claimed yet.
All nine relevant native test targets, renderer boundary, launcher failure
cases and five visible render/presentation comparison cases pass.

Final ARM GLES build / --input-info smoke pass, GLIBC max 2.29. Deployed binary:
213532EADAA62DF9C86FED53E5EF7F9C1C666E7E43C1546C494C513AB6428B7F.
Eleven payloads and four menu entries match staging; card ACLs applied; old
returned logs preserved. OpenUG2 Performance now runs the five-case comparison
into a fresh perf-compare directory. Normal play also writes memory.csv.

See PERFORMANCE-CHANGES-20261104.md for details/evidence/limits and PERFORMANCE.md
for the physical driving acceptance and comparison sequence. Do not mark the
handheld OOM resolved until a returned R36S drive survives this candidate.


19. Rendering candidate and eight-case comparison - 2026-11-04

Vehicle shadows are removed throughout this fork. The R36S launcher now defaults
to a 320x240 RGBA8/D24 3D scene, bilinearly composed onto the native 640x480
HUD/menu/pause display, with fast 256x256 / one-sample headlight world shadows.
Load/view remain 500/450 m. Specialized dry-world shading and 32 m depth ordering
are implemented but remain opt-in (legacy defaults) pending handheld acceptance.

Final ARM GLES build and QEMU startup/input discovery pass (GLIBC max 2.29).
Deployed SHA-256: 4F10439F7E5944F851401AD596ABA03F94C28E9CBAA9D4B1B38D440767D2B921.
Eleven payloads/four menu entries match staging; LF/BOM and ACL checks pass.
Existing card evidence and returned comparisons are preserved; data not recopied.

GLES composition/D24/HUD/failure and material/headlight parity fixtures pass,
as do targeted renderer/UI/residency tests, native desktop and Vulkan builds
and validation smoke. Final PC eight-case run completed 120 frames/case with
zero vehicle-shadow time/fallbacks and pixel-identical shader/order/repeat
screenshots. Repeat drift +1.10% native and -3.75% optimized passed the timing
gate. PC software-renderer timing does not establish a handheld FPS gain.

OpenUG2 Performance now runs eight fresh cases with isolated logs/settings/save
snapshots, preserving normal evidence. First 20 and screenshot frames excluded;
over 5% repeat drift invalidates attribution. Return its new perf-compare folder,
then a normal NFSU2 driving run. Physical new-build acceptance is PENDING.
Streaming preparation/stale targets remain a separate unresolved task.

See RENDERING-OPTIMIZATIONS-20261104.md and PERFORMANCE.md for details, regression
coverage, artifact locations and the exact next device run.
