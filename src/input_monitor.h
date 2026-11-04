/* Opt-in bring-up diagnostics. Only the main thread touches SDL input/window
 * state; the watchdog reads atomic progress so a blocked GL swap is visible. */
#ifndef OPENUG2_INPUT_MONITOR_H
#define OPENUG2_INPUT_MONITOR_H

#include <stdio.h>
#include <SDL.h>
#include "controls_sdl.h"

enum { IM_SETUP, IM_EVENTS, IM_WORLD, IM_INPUT, IM_SIMULATION, IM_RENDER, IM_PRESENT, IM_IDLE };
typedef struct InputMonitor {
    SDL_Thread *thread;
    SDL_atomic_t stop, phase, frames, events;
    Uint32 started, duration, next_sample;
} InputMonitor;

static int im_watch(void *arg) {
    InputMonitor *m = (InputMonitor *)arg;
    const char *names[] = { "setup", "events", "world", "input", "simulation", "render", "present", "idle" };
    Uint32 next = 0;
    while (!SDL_AtomicGet(&m->stop)) {
        Uint32 elapsed = SDL_GetTicks() - m->started;
        if (elapsed >= m->duration) break;
        if (elapsed >= next) {
            int phase = SDL_AtomicGet(&m->phase);
            fprintf(stderr, "input-watch: t=%.1f frame=%d events=%d phase=%s\n",
                    elapsed / 1000.0, SDL_AtomicGet(&m->frames), SDL_AtomicGet(&m->events), names[phase]);
            fflush(stderr);
            next = elapsed + 1000;
        }
        SDL_Delay(50);
    }
    fprintf(stderr, "input-watch: stopped frame=%d\n", SDL_AtomicGet(&m->frames));
    fflush(stderr);
    return 0;
}

static void im_start(InputMonitor *m, int seconds, SDL_Window *win, const CxInput *c) {
    if (!seconds) return;
    m->started = SDL_GetTicks();
    m->duration = (Uint32)seconds * 1000u;
    m->next_sample = m->started;
    fprintf(stderr, "input-monitor: seconds=%d video=%s focus=%d flags=0x%x joystick-events=%d controller-events=%d background=%s\n",
            seconds, SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none",
            SDL_GetKeyboardFocus() == win, (unsigned)SDL_GetWindowFlags(win),
            SDL_JoystickEventState(SDL_QUERY), SDL_GameControllerEventState(SDL_QUERY),
            SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS) ? SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS) : "default");
    for (int i = 0; i < CB_MAX_PADS; i++) {
        const CbPad *p = &c->b.pad[i];
        if (!p->present) continue;
        fprintf(stderr, "input-monitor: pad=%d name=%s attached=%d buttons=%d axes-mask=0x%x hats=%d combined-axis=%d assigned=%d bindings=",
                i, SDL_JoystickName(c->joy[i]), SDL_JoystickGetAttached(c->joy[i]),
                p->info.buttons, p->info.axes_mask, SDL_JoystickNumHats(c->joy[i]),
                p->dev.axis[CB_ROLE_COMBINED] / 4, p->dev.assigned[CB_ROLE_COMBINED]);
        for (int b = 0; b < CB_BIND_BUTTONS; b++) fprintf(stderr, "%s%d", b ? "," : "", p->dev.binding[b]);
        fputc('\n', stderr);
    }
    fflush(stderr);
    m->thread = SDL_CreateThread(im_watch, "input-watch", m);
    if (!m->thread) fprintf(stderr, "input-monitor: watchdog unavailable: %s\n", SDL_GetError());
}

static void im_phase(InputMonitor *m, int phase) {
    if (m->duration) SDL_AtomicSet(&m->phase, phase);
}

static void im_sample(InputMonitor *m, const CxInput *c, const Uint8 *keys,
                      int race, int driving, int paused, int steps, int ride, float speed) {
    Uint32 now = SDL_GetTicks();
    if (!m->duration || now - m->started >= m->duration || !SDL_TICKS_PASSED(now, m->next_sample)) return;
    m->next_sample = now + 1000;
    /* Log all physical buttons, including the 17th which the legacy binding
     * record cannot represent (its last 16 slots are POV directions). */
    for (int i = 0; i < CB_MAX_PADS; i++) {
        if (!c->joy[i]) continue;
        CbPadSample s; cx_sample(c, i, &s);
        unsigned buttons = 0;
        int count = SDL_JoystickNumButtons(c->joy[i]);
        for (int b = 0; b < count && b < 32; b++) if (SDL_JoystickGetButton(c->joy[i], b)) buttons |= 1u << b;
        fprintf(stderr, "input-sample: pad=%d present=%d attached=%d axes=%d,%d,%d,%d buttons=0x%08x\n",
                i, c->b.pad[i].present, SDL_JoystickGetAttached(c->joy[i]),
                s.axis[0], s.axis[1], s.axis[2], s.axis[3], buttons);
    }
    fprintf(stderr, "input-state: frame=%d focus=%d keys=W%d,Up%d,Esc%d requested-throttle=%.2f requested-steer=%.2f race=%d driving=%d paused=%d steps=%d ride=%d speed=%.2f\n",
            SDL_AtomicGet(&m->frames), SDL_GetKeyboardFocus() != NULL,
            keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_UP], keys[SDL_SCANCODE_ESCAPE],
            cx_throttle(c, keys), cx_steer(c, keys), race, driving, paused, steps, ride, speed);
    fflush(stderr);
}

static void im_stop(InputMonitor *m) {
    SDL_AtomicSet(&m->stop, 1);
    if (m->thread) SDL_WaitThread(m->thread, NULL);
    m->thread = NULL;
}
#endif
