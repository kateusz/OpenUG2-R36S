/* Headless check: watchdog keeps reporting while the main loop makes no
 * progress; input sampling tolerates no pad and records the 17th raw button. */
#include <stdlib.h>
#include "input_monitor.h"

int main(void) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1", 1);
    SDL_setenv("HOME", "build/input_monitor_tmp", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) return 1;
    SDL_Window *win = SDL_CreateWindow("monitor test", 0, 0, 64, 64, 0);
    if (!win) return 2;
    CxInput *c = (CxInput *)calloc(1, sizeof *c);
    if (!c) return 3;
    cx_init(c);
    InputMonitor disabled = {0};
    im_start(&disabled, 0, win, c);
    if (disabled.thread || disabled.duration) return 4;
    im_stop(&disabled);

    InputMonitor m = {0};
    im_start(&m, 2, win, c);
    if (!m.thread) return 5;
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    keys[SDL_SCANCODE_W] = 1;
    im_sample(&m, c, keys, 1, 1, 0, 1, 1, 0);
    SDL_AtomicSet(&m.frames, 7);
    SDL_AtomicSet(&m.events, 3);
    im_phase(&m, IM_PRESENT);
    SDL_Delay(1200); /* deliberately blocked main thread; watchdog must report */
    im_stop(&m);
    if (m.thread || SDL_AtomicGet(&m.frames) != 7) return 6;
    cx_shutdown(c);

#if SDL_VERSION_ATLEAST(2, 0, 14)
    int vi = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 4, 17, 0);
    if (vi < 0) return 7;
    cx_init(c);
    if (!c->joy[0]) return 8;
    SDL_JoystickSetVirtualButton(c->joy[0], 16, 1);
    SDL_JoystickUpdate();
    InputMonitor raw = {0};
    im_start(&raw, 1, win, c);
    im_sample(&raw, c, keys, 1, 1, 0, 1, 1, 0);
    im_stop(&raw);
    cx_shutdown(c);
    SDL_JoystickDetachVirtual(vi);
#endif
    free(c);
    SDL_DestroyWindow(win);
    SDL_Quit();
    puts("input_monitor_test: PASS");
    return 0;
}
