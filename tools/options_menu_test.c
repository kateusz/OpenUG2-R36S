/* options_menu_test -- runs src/options_menu.c against docs/cleanroom-settings/test-vectors-gui.
 *
 *   options_menu_test [VECTOR_DIR]
 *
 * Every vector file (tvg02..tvg06, tvg01 for package keys) is read at run time (a small JSON
 * reader, test_json.h). Beyond the vectors the test drives the screens through the public API:
 * persistence, dialog, exit animation, hub, pointer input, re-init hook. The state machine draws
 * nothing; the authored presentation is covered by gui_view_test.
 * Exit code is nonzero if any check fails. See docs/cleanroom-settings/IMPL_DESIGN_GUI.md. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options_menu.h"
#include "gui_timing.h"

static int g_checks, g_fail;
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; \
    if (g_fail <= 60) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static const char *g_dir = "docs/cleanroom-settings/test-vectors-gui";

#include "test_json.h"

/* ---- harness ----------------------------------------------------------------------- */
typedef struct Fx {
    OptionsMenu m;
    StGraphics g;
    StGfxCaps caps;
    StRegStore store;
    OmConfig cfg;
} Fx;

static void fx_open(Fx *f, OmContext ctx, int caret) {
    memset(f, 0, sizeof *f);
    st_gfx_defaults(&f->g);
    st_gfx_caps_full(&f->caps, 5);               /* FSAA cap 4 */
    st_reg_open_memory(&f->store);
    om_config_default(&f->cfg, ctx);
    f->cfg.row1_label_has_caret = caret;
    om_open(&f->m, &f->cfg, &f->g, &f->caps, &f->store, 0);
}
static void fx_close(Fx *f) { st_reg_close(&f->store); }
static void settle(Fx *f) { om_update(&f->m, 1.0f); }
static void press(Fx *f, OmInput in, int n) { for (int i = 0; i < n; i++) om_input(&f->m, in); }
static void fx_customize(Fx *f) {                /* Display row 3 -> Customize */
    press(f, OM_IN_DOWN, 2);
    om_input(&f->m, OM_IN_ACCEPT);
    settle(f);
}
static int find_row(const OptionsMenu *m, const char *label) {
    for (int i = 0; i < om_row_count(m); i++) {
        const OmRow *r = om_row(m, i);
        if (r->label && !strcmp(r->label, label)) return i;
    }
    return -1;
}
static void focus_row(Fx *f, int idx) {          /* walk the focus onto row idx (0-based) */
    int guard = 64;
    while (om_focus(&f->m) < idx && guard--) om_input(&f->m, OM_IN_DOWN);
    while (om_focus(&f->m) > idx && guard--) om_input(&f->m, OM_IN_UP);
}
static const char *kind_name(OmRowKind k) {
    return k == OM_ROW_CHOICE ? "choice" : k == OM_ROW_SLIDER ? "slider" : k == OM_ROW_BUTTON ? "button" : "spacer";
}

static const struct { const char *name; int slot; } k_caps[] = {
    {"CarEnvironmentMap", ST_GFX_CAR_ENV_MAP}, {"CarEnvironmentMapUpdateData", ST_GFX_CAR_ENV_MAP_UPDATE},
    {"CarShadow", ST_GFX_CAR_SHADOW}, {"CarHeadlight", ST_GFX_CAR_HEADLIGHT}, {"Crowd", ST_GFX_CROWD},
    {"RoadReflection", ST_GFX_ROAD_REFLECTION}, {"Fog", ST_GFX_FOG}, {"MotionBlur", ST_GFX_MOTION_BLUR},
    {"LightStreaks", ST_GFX_LIGHT_STREAKS}, {"LightGlow", ST_GFX_LIGHT_GLOW},
    {"ParticleSystem", ST_GFX_PARTICLES}, {"DepthOfField", ST_GFX_DEPTH_OF_FIELD},
    {"WorldLod", ST_GFX_WORLD_LOD}, {"CarLod", ST_GFX_CAR_LOD}, {"OverBright", ST_GFX_OVER_BRIGHT},
    {"BleachByPass", ST_GFX_BLEACH_BYPASS}, {"Tinting", ST_GFX_TINTING}, {"FSAA", ST_GFX_FSAA},
    {"HorizonFog", ST_GFX_HORIZON_FOG}, {"Rain", ST_GFX_RAIN}, {"TextureFiltering", ST_GFX_TEXTURE_FILTERING}
};
static void caps_from_json(StGfxCaps *c, const J *caps) {
    for (int i = 0; i < ST_GFX_NSLOTS; i++) c->cap[i] = 0;
    for (int i = 0; i < 6; i++) c->resolution_ok[i] = 1;
    for (size_t k = 0; k < sizeof k_caps / sizeof k_caps[0]; k++) {
        J *v = jget(caps, k_caps[k].name);
        CHECK(v != NULL, "vector caps lack %s", k_caps[k].name);
        if (v) c->cap[k_caps[k].slot] = (int)v->num;
    }
}

/* ---- tvg02: rows -------------------------------------------------------------------- */
static void test_tvg02(void) {
    J *j = jload("tvg02_rows.json");
    if (!j) return;
    J *cases = jget(j, "cases");
    for (int ci = 0; ci < cases->n; ci++) {
        J *c = cases->kid[ci];
        const char *id = jstrv(c, "id"), *screen = jstrv(c, "screen");
        J *rows = jget(c, "expected_rows");
        Fx f;
        fx_open(&f, OM_CTX_FRONTEND, jbool(jget(c, "input"), "label_0x7D3930E7_contains_caret"));
        J *capsj = jget(jget(c, "input"), "caps");
        if (!strcmp(screen, "customize")) {
            caps_from_json(&f.caps, capsj);
            fx_customize(&f);
            CHECK(om_screen(&f.m) == OM_SCREEN_CUSTOMIZE, "%s: not on Customize", id);
        } else {
            CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY, "%s: not on Display", id);
        }
        /* Historical retail vectors retain the serialized shadow row. This fork
           removes that row and compacts the authored OPTION slots. */
        int expected=rows->n;
        for(int i=0;i<rows->n;i++){const char *label=jstrv(rows->kid[i],"label_en");if(label && !strcmp(label,"Car Shadow / Neon"))expected--;}
        CHECK(om_row_count(&f.m) == expected, "%s: %d rows, expected %d", id, om_row_count(&f.m), expected);
        int actual=0;
        for (int i = 0; i < rows->n && actual < om_row_count(&f.m); i++) {
            J *e = rows->kid[i];
            const char *label=jstrv(e,"label_en");if(label && !strcmp(label,"Car Shadow / Neon"))continue;
            const OmRow *r = om_row(&f.m, actual++);
            const char *lk = jstrv(e, "label_key"), *len = jstrv(e, "label_en");
            CHECK(r->slot == actual, "%s row %d slot %d", id, i, r->slot);
            CHECK(!strcmp(kind_name(r->kind), jstrv(e, "kind")), "%s row %d kind %s", id, i, kind_name(r->kind));
            CHECK(r->label_key == (lk ? hexkey(lk) : 0u), "%s row %d key %08X", id, i, r->label_key);
            if (len) CHECK(r->label && !strcmp(r->label, len), "%s row %d label %s", id, i, r->label ? r->label : "(null)");
            CHECK(r->selectable == jbool(e, "selectable"), "%s row %d selectable", id, i);
            J *rg = jget(e, "range");
            if (rg) CHECK(r->min == (int)rg->kid[0]->num && r->max == (int)rg->kid[1]->num,
                          "%s row %d range %d..%d", id, i, r->min, r->max);
        }
        fx_close(&f);
    }
    jfree(j);
}

/* ---- tvg03: navigation (list engine, then the real screens) ----------------------------- */
static int sl_at(const J *sel, int i) { return i < sel->n ? (int)sel->kid[i]->num : 0; }
static void test_tvg03(void) {
    J *j = jload("tvg03_navigation.json");
    if (!j) return;
    J *cases = jget(j, "cases");
    for (int ci = 0; ci < cases->n; ci++) {
        J *c = cases->kid[ci], *in = jget(c, "input"), *ex = jget(c, "expected");
        const char *id = jstrv(c, "id");
        int sel[OM_MAX_ROWS], n = (int)jnum(in, "rows", 0);
        J *sj = jget(in, "selectable");
        CHECK(n <= OM_MAX_ROWS && sj->n == n, "%s: %d rows", id, n);
        for (int i = 0; i < n; i++) sel[i] = sj->kid[i]->num != 0;
        CHECK((int)jnum(in, "visible_rows", 0) == OM_VISIBLE_ROWS, "%s: V", id);
        OmList l;
        om_list_init(&l, n, sel, (int)jnum(in, "initial_focus", 1) - 1);
        CHECK(l.top + 1 == (int)jnum(ex, "initial_top", -1), "%s: initial top %d", id, l.top + 1);
        J *evs = jget(in, "events"), *trace = jget(ex, "trace");
        CHECK(evs->n == trace->n, "%s: trace length", id);
        for (int k = 0; k < evs->n && k < trace->n; k++) {
            const char *ev = evs->kid[k]->s;
            om_list_move(&l, !strcmp(ev, "Up") ? -1 : 1);
            J *t = trace->kid[k], *vis = jget(t, "visible");
            CHECK(l.focus + 1 == (int)jnum(t, "focus", -1), "%s step %d: focus %d", id, k, l.focus + 1);
            CHECK(l.top + 1 == (int)jnum(t, "top", -1), "%s step %d: top %d", id, k, l.top + 1);
            int first = l.top, last = l.top + OM_VISIBLE_ROWS - 1 < n - 1 ? l.top + OM_VISIBLE_ROWS - 1 : n - 1;
            CHECK(vis->n == last - first + 1 && (int)vis->kid[0]->num == first + 1 &&
                  (int)vis->kid[vis->n - 1]->num == last + 1, "%s step %d: visible window", id, k);
        }
        /* the same trace through the real screens (both contexts) where the vector describes one */
        int on_display = !strncmp(id, "display_", 8), on_customize = !strncmp(id, "customize_", 10);
        /* Retail Customize navigation remains a generic list fixture above; fork-specific
           scrolling/caret/end checks below cover its shorter production list. */
        if (on_display && (int)jnum(in, "initial_focus", 1) == 1) {
            for (int ctx = 0; ctx < 2; ctx++) {
                Fx f;
                fx_open(&f, ctx ? OM_CTX_INGAME : OM_CTX_FRONTEND, sj->n == 23 && !sj->kid[1]->num);
                if (on_customize) fx_customize(&f);
                CHECK(om_row_count(&f.m) == n && om_screen(&f.m) == (on_customize ? OM_SCREEN_CUSTOMIZE : OM_SCREEN_DISPLAY), "%s: screen rows %d", id, om_row_count(&f.m));
                for (int i = 0; i < n && i < om_row_count(&f.m); i++)
                    CHECK(om_row(&f.m, i)->selectable == (sl_at(sj, i) != 0), "%s: row %d selectable", id, i);
                CHECK(om_top(&f.m) + 1 == (int)jnum(ex, "initial_top", -1), "%s: screen initial top", id);
                for (int k = 0; k < evs->n && k < trace->n; k++) {
                    om_input(&f.m, !strcmp(evs->kid[k]->s, "Up") ? OM_IN_UP : OM_IN_DOWN);
                    CHECK(om_focus(&f.m) + 1 == (int)jnum(trace->kid[k], "focus", -1) && om_top(&f.m) + 1 == (int)jnum(trace->kid[k], "top", -1),
                          "%s ctx %d step %d: screen focus %d top %d", id, ctx, k, om_focus(&f.m) + 1, om_top(&f.m) + 1);
                }
                fx_close(&f);
            }
        }
    }
    jfree(j);
}

/* ---- tvg04: slider -------------------------------------------------------------------- */
static void test_tvg04(void) {
    J *j = jload("tvg04_slider.json");
    if (!j) return;
    J *cases = jget(j, "cases");
    for (int ci = 0; ci < cases->n; ci++) {
        J *c = cases->kid[ci], *in = jget(c, "input"), *ex = jget(c, "expected");
        const char *id = jstrv(c, "id");
        int mn = (int)jnum(in, "min", 0), mx = (int)jnum(in, "max", 0);
        CHECK((int)jnum(in, "step", 1) == 1, "%s: step", id);
        J *stored = jget(in, "initial_stored");
        if (stored) {                             /* row-build clamp, checked through a real menu */
            Fx f;
            fx_open(&f, OM_CTX_FRONTEND, 0);
            f.g.v[ST_GFX_PERF_LEVEL] = (int)stored->num;
            om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
            int lod = find_row(&f.m, "Level Of Detail");
            CHECK(om_row_value(&f.m, lod) == (int)jnum(ex, "displayed_value", -1), "%s: displayed %d", id, om_row_value(&f.m, lod));
            CHECK(fabsf(om_row_fill(&f.m, lod) - (float)jnum(ex, "initial_fill_fraction", -1)) < 1e-5f, "%s: fill", id);
            CHECK(f.g.v[ST_GFX_PERF_LEVEL] == (int)stored->num, "%s: stored setting rewritten on build", id);
            CHECK(!om_apply_pending(&f.m), "%s: build requested an apply", id);
            fx_close(&f);
            continue;
        }
        int v = (int)jnum(in, "initial", 0);
        CHECK(fabsf(om_slider_fill(v, mn, mx) - (float)jnum(ex, "initial_fill_fraction", -1)) < 1e-5f, "%s: initial fill", id);
        J *evs = jget(in, "events"), *trace = jget(ex, "trace");
        for (int k = 0; k < evs->n; k++) {
            const char *ev = evs->kid[k]->s;
            int changed = !strcmp(ev, "Left") ? om_slider_step(&v, mn, mx, -1)
                        : !strcmp(ev, "Right") ? om_slider_step(&v, mn, mx, 1) : 0;   /* Accept does nothing */
            J *t = trace->kid[k];
            CHECK(v == (int)jnum(t, "value", -99), "%s step %d: value %d", id, k, v);
            CHECK(changed == jbool(t, "changed"), "%s step %d: changed %d", id, k, changed);
            CHECK(fabsf(om_slider_fill(v, mn, mx) - (float)jnum(t, "fill_fraction", -1)) < 1e-5f, "%s step %d: fill", id, k);
        }
    }
    jfree(j);
}

/* ---- tvg05: value text ------------------------------------------------------------------ */
static void test_tvg05(void) {
    J *j = jload("tvg05_value_text.json");
    if (!j) return;
    J *cases = jget(j, "cases");
    for (int ci = 0; ci < cases->n; ci++) {
        J *c = cases->kid[ci];
        const char *row = jstrv(c, "row");
        int value = (int)jnum(c, "value", 0);
        Fx f;
        fx_open(&f, OM_CTX_FRONTEND, 0);
        int resolution = !strcmp(row, "Resolution");
        if (resolution) f.g.v[ST_GFX_RESOLUTION] = value;
        else {
            fx_customize(&f);
        }
        int idx = find_row(&f.m, row);
        CHECK(idx >= 0, "tvg05: no row %s", row);
        if (idx >= 0) {
            /* set the stored value, then rebuild the screen so the row is built from it */
            if (!resolution) {
                int slot = om_row(&f.m, idx)->gfx;
                f.g.v[slot] = value;
                om_input(&f.m, OM_IN_BACK);       /* Customize -> Display */
                settle(&f);
                fx_customize(&f);
                idx = find_row(&f.m, row);
            } else {
                om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
            }
            uint32_t key = 0xDEADBEEFu;
            const char *t = om_row_value_text(&f.m, idx, &key);
            const char *ek = jstrv(c, "expected_key"), *et = jstrv(c, "expected_text"), *ee = jstrv(c, "expected_en");
            if (resolution && !et) {
                CHECK(t == NULL, "%s=%d: expected no text, got %s", row, value, t ? t : "");
            } else if (et) {
                CHECK(t && !strcmp(t, et) && key == 0, "%s=%d: text %s key %08X", row, value, t ? t : "(null)", key);
            } else {
                CHECK(t && key == hexkey(ek) && ee && !strcmp(t, ee), "%s=%d: key %08X text %s", row, value, key, t ? t : "(null)");
            }
        }
        fx_close(&f);
    }
    jfree(j);
}

/* ---- tvg06: flow -------------------------------------------------------------------------- */
static int reg_has_gfx(StRegStore *s) {
    uint32_t v;
    return st_reg_get_dword(s, "g_RacingResolution", &v) || st_reg_get_dword(s, "g_VSyncOn", &v);
}
static void test_tvg06(void) {
    J *j = jload("tvg06_flow.json");
    if (!j) return;
    J *cases = jget(j, "cases");
    for (int ci = 0; ci < cases->n; ci++) {
        J *c = cases->kid[ci], *ex = jget(c, "expected");
        const char *id = jstrv(c, "id");
        OmContext ctx = !strcmp(jstrv(c, "context"), "in-game") ? OM_CTX_INGAME : OM_CTX_FRONTEND;
        int none = !!strcmp(jstrv(c, "context"), "in-game") && !!strcmp(jstrv(c, "context"), "frontend");   /* "either" */
        OmContext ctxs[2] = { OM_CTX_FRONTEND, OM_CTX_INGAME };
        for (int cc = 0; cc < (none ? 2 : 1); cc++) {
            if (none) ctx = ctxs[cc];
            Fx f;
            fx_open(&f, ctx, 0);
            f.g.v[ST_GFX_RESOLUTION] = 2;         /* distinguish written values from absent ones */

            if (!strcmp(id, "fe_back_from_display") || !strcmp(id, "ig_back_from_display")) {
                int exit_ms = ctx == OM_CTX_INGAME ? 320 : 140;          /* SPEC_GUI_ART 1.4 */
                CHECK(om_exit_ms(ctx, OM_SCREEN_DISPLAY) == exit_ms, "%s: exit length %d", id, om_exit_ms(ctx, OM_SCREEN_DISPLAY));
                om_input(&f.m, OM_IN_BACK);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE && !reg_has_gfx(&f.store), "%s: left before the exit animation ended", id);
                om_input(&f.m, OM_IN_BACK);       /* second Back is ignored while it runs */
                om_update(&f.m, 0.01f);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE, "%s: left early", id);
                om_update(&f.m, (exit_ms - 11) / 1000.0f);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE && !reg_has_gfx(&f.store), "%s: left 1 ms early", id);
                om_update(&f.m, 0.002f);
                CHECK(om_poll_event(&f.m) == OM_EV_LEAVE_TO_HUB, "%s: no leave event", id);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE, "%s: leave event delivered twice", id);
                CHECK(jbool(ex, "graphics_written_to_registry") == reg_has_gfx(&f.store) && f.m.registry_writes == 1, "%s: writes %d", id, f.m.registry_writes);
                uint32_t v = 0;
                CHECK(st_reg_get_dword(&f.store, "g_RacingResolution", &v) && v == 2, "%s: stored resolution", id);
                CHECK(!strcmp(om_hub_package_name(ctx), jstrv(ex, "next_package")), "%s: hub %s", id, om_hub_package_name(ctx));
                if (ctx == OM_CTX_INGAME)         /* the in-game hub is a screen of this menu */
                    CHECK(om_screen(&f.m) == OM_SCREEN_HUB && !strcmp(om_package_name(ctx, om_screen(&f.m)), jstrv(ex, "next_package")), "%s: not on the hub", id);
                else
                    CHECK(!om_is_open(&f.m), "%s: still open", id);
            } else if (!strcmp(id, "ig_start_from_display")) {
                om_input(&f.m, OM_IN_START);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE && om_is_open(&f.m), "%s: resumed before the exit script ended", id);
                om_update(&f.m, 0.3f);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE, "%s: resumed 20 ms early", id);
                om_update(&f.m, 0.03f);
                CHECK(om_poll_event(&f.m) == OM_EV_RESUME_GAME, "%s: no resume", id);
                CHECK(reg_has_gfx(&f.store) == jbool(ex, "graphics_written_to_registry") && f.m.registry_writes == 0, "%s: wrote", id);
                CHECK(!om_is_open(&f.m), "%s: still open", id);
                CHECK(f.g.v[ST_GFX_RESOLUTION] == 2, "%s: values lost", id);
            } else if (!strcmp(id, "customize_open")) {
                press(&f, OM_IN_DOWN, 2);
                om_input(&f.m, OM_IN_ACCEPT);
                CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY, "%s: switched before the exit animation ended", id);
                om_update(&f.m, (om_exit_ms(ctx, OM_SCREEN_DISPLAY) - 5) / 1000.0f);
                CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY, "%s: switched 5 ms early", id);
                settle(&f);
                CHECK(om_screen(&f.m) == OM_SCREEN_CUSTOMIZE, "%s: not on Customize", id);
                CHECK(!strcmp(om_package_name(ctx, om_screen(&f.m)), jstrv(ex, "next_package")), "%s: package", id);
                CHECK(!reg_has_gfx(&f.store) && f.m.registry_writes == 0, "%s: wrote", id);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE, "%s: event", id);
            } else if (!strncmp(id, "customize_back", 14)) {
                fx_customize(&f);
                press(&f, OM_IN_DOWN, 3);
                om_input(&f.m, OM_IN_BACK);
                settle(&f);
                CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY, "%s: not on Display", id);
                CHECK(!strcmp(om_package_name(ctx, om_screen(&f.m)), jstrv(ex, "next_package")), "%s: package %s", id, om_package_name(ctx, om_screen(&f.m)));
                CHECK(!reg_has_gfx(&f.store) && f.m.registry_writes == 0, "%s: wrote", id);
                CHECK(om_poll_event(&f.m) == OM_EV_NONE, "%s: event", id);
                if (jget(ex, "display_initial_focus_row"))
                    CHECK(om_focus(&f.m) + 1 == (int)jnum(ex, "display_initial_focus_row", -1) && om_top(&f.m) == 0, "%s: focus %d", id, om_focus(&f.m) + 1);
            } else if (!strcmp(id, "defaults_yes")) {
                f.g.v[ST_GFX_LIGHT_GLOW] = 1;
                f.g.v[ST_GFX_CROWD] = 1;
                f.g.v[ST_GFX_PERF_LEVEL] = 1;
                press(&f, OM_IN_DOWN, 3);
                om_take_reinit_request(&f.m);
                om_take_apply_request(&f.m);
                om_input(&f.m, OM_IN_ACCEPT);
                CHECK(om_dialog_is_open(&f.m), "%s: no dialog", id);
                CHECK(!strcmp(om_dialog_package_name(), jstrv(ex, "dialog_package")), "%s: dialog package", id);
                J *bt = jget(ex, "buttons");
                CHECK(om_dialog_button_key(0) == hexkey(bt->kid[0]->s) && om_dialog_button_key(1) == hexkey(bt->kid[1]->s), "%s: buttons", id);
                CHECK(OM_KEY_DEFAULTS_MESSAGE == hexkey(jstrv(ex, "message_key")), "%s: message", id);
                /* corrected by review 2026-10-14 (SPEC_GUI_ART 6): focus starts on the second button (No) */
                CHECK(strstr(jstrv(ex, "initial_dialog_focus"), "button 2") != NULL, "%s: vector states button 2", id);
                CHECK(om_dialog_focus(&f.m) == 1, "%s: initial dialog focus %d", id, om_dialog_focus(&f.m));
                CHECK(f.g.v[ST_GFX_LIGHT_GLOW] == 1, "%s: changed before Yes", id);
                om_input(&f.m, OM_IN_RIGHT);
                CHECK(om_dialog_focus(&f.m) == 1, "%s: no wrap on the second button", id);
                om_input(&f.m, OM_IN_LEFT);        /* the Yes path: the user moves the focus to Yes */
                CHECK(om_dialog_focus(&f.m) == 0, "%s: Left focuses Yes", id);
                om_input(&f.m, OM_IN_LEFT);
                CHECK(om_dialog_focus(&f.m) == 0, "%s: no wrap on the first button", id);
                om_input(&f.m, OM_IN_ACCEPT);     /* Yes */
                CHECK(om_dialog_is_open(&f.m) && f.g.v[ST_GFX_LIGHT_GLOW] == 1, "%s: the dialog plays its exit first", id);
                om_update(&f.m, 0.139f);
                CHECK(om_dialog_is_open(&f.m) && f.g.v[ST_GFX_LIGHT_GLOW] == 1, "%s: result before the 140 ms exit completed", id);
                om_update(&f.m, 0.002f);
                CHECK(!om_dialog_is_open(&f.m), "%s: dialog stays", id);
                /* auto-detect for modern hardware (SPEC 4.5): P = 4 */
                StGraphics ref;
                st_gfx_defaults(&ref);
                ref.v[ST_GFX_RESOLUTION] = 2;
                st_gfx_autodetect(&ref, &f.caps, st_perf_level(f.cfg.cpu_mhz, f.cfg.gpu_score), f.cfg.gpu_score);
                CHECK(!memcmp(&ref, &f.g, sizeof ref) || (ref.v[ST_GFX_LIGHT_GLOW] == f.g.v[ST_GFX_LIGHT_GLOW] &&
                      ref.v[ST_GFX_RESOLUTION] == f.g.v[ST_GFX_RESOLUTION] && ref.v[ST_GFX_PERF_LEVEL] == f.g.v[ST_GFX_PERF_LEVEL]),
                      "%s: auto-detect result differs", id);
                CHECK(f.g.v[ST_GFX_PERF_LEVEL] == 4 && f.g.v[ST_GFX_CROWD] == 1, "%s: preset not applied", id);
                CHECK(om_reinit_pending(&f.m) && om_apply_pending(&f.m), "%s: no re-init request", id);
                CHECK(om_focus(&f.m) == 0 && om_top(&f.m) == 0 && om_screen(&f.m) == OM_SCREEN_DISPLAY, "%s: rebuild focus %d", id, om_focus(&f.m) + 1);
                CHECK(jbool(ex, "graphics_written_to_registry") == reg_has_gfx(&f.store) && f.m.registry_writes == 0, "%s: wrote", id);
            } else if (!strcmp(id, "defaults_no")) {
                f.g.v[ST_GFX_LIGHT_GLOW] = 1;
                press(&f, OM_IN_DOWN, 3);
                for (int variant = 0; variant < 4; variant++) {
                    StGraphics before = f.g;
                    om_take_reinit_request(&f.m);
                    om_take_apply_request(&f.m);
                    om_input(&f.m, OM_IN_ACCEPT);
                    CHECK(om_dialog_is_open(&f.m) && om_dialog_focus(&f.m) == 1, "%s: no dialog on No", id);
                    if (variant == 0) om_input(&f.m, OM_IN_ACCEPT);                       /* Accept on the default focus = No */
                    else if (variant == 1) om_input(&f.m, OM_IN_BACK);                    /* Back = No */
                    else if (variant == 2) { om_input(&f.m, OM_IN_LEFT); om_input(&f.m, OM_IN_RIGHT); om_input(&f.m, OM_IN_RIGHT);
                                             CHECK(om_dialog_focus(&f.m) == 1, "%s: wrapped", id); om_input(&f.m, OM_IN_ACCEPT); }
                    else { om_input(&f.m, OM_IN_LEFT); om_input(&f.m, OM_IN_BACK); }      /* Back cancels whatever has the focus */
                    CHECK(om_dialog_is_open(&f.m), "%s v%d: closes only after its exit", id, variant);
                    settle(&f);
                    CHECK(!om_dialog_is_open(&f.m), "%s v%d: dialog stays", id, variant);
                    CHECK(!memcmp(&before, &f.g, sizeof before) == !jbool(ex, "settings_changed"), "%s v%d: settings changed", id, variant);
                    CHECK(om_focus(&f.m) == 3 && !om_reinit_pending(&f.m) && !om_apply_pending(&f.m), "%s v%d: focus %d / requests", id, variant, om_focus(&f.m) + 1);
                }
            } else if (!strcmp(id, "change_requires_reinit") || !strcmp(id, "change_no_reinit")) {
                int light = !strcmp(id, "change_requires_reinit");
                const char *label = light ? "Light Glow" : "Crowds";
                fx_customize(&f);
                int idx = find_row(&f.m, label);
                CHECK(idx >= 0, "%s: no row", id);
                focus_row(&f, idx);
                om_take_reinit_request(&f.m);
                om_take_apply_request(&f.m);
                om_input(&f.m, OM_IN_RIGHT);
                int slot = om_row(&f.m, idx)->gfx;
                CHECK(f.g.v[slot] == (int)jnum(ex, "value", -1), "%s: value %d", id, f.g.v[slot]);
                if (light) {
                    CHECK(om_reinit_pending(&f.m), "%s: no immediate re-init request", id);
                    CHECK(!reg_has_gfx(&f.store) && f.m.registry_writes == 0, "%s: written", id);
                } else {
                    CHECK(!om_reinit_pending(&f.m) && om_apply_pending(&f.m), "%s: re-init %d apply %d", id, om_reinit_pending(&f.m), om_apply_pending(&f.m));
                }
            } else if (!strcmp(id, "toggle_cap_zero_hidden")) {
                f.caps.cap[ST_GFX_FOG] = 0;
                fx_customize(&f);
                CHECK(find_row(&f.m, "Fog") < 0, "%s: Fog row present", id);
                CHECK(om_row_count(&f.m) == 20, "%s: rows %d", id, om_row_count(&f.m));
            } else {
                CHECK(0, "tvg06: unhandled case %s", id);
            }
            fx_close(&f);
        }
    }
    jfree(j);
}

/* ---- tvg01: package names and keys ------------------------------------------------------ */
static uint32_t name_hash(const char *s) {
    uint32_t h = 0xFFFFFFFFu;
    for (; *s; s++) h = h * 33u + (uint32_t)((*s >= 'a' && *s <= 'z') ? *s - 32 : *s);
    return h;
}
static void test_tvg01(void) {
    J *j = jload("tvg01_packages.json");
    if (!j) return;
    J *pk = jget(j, "packages");
    int seen = 0;
    for (int i = 0; i < pk->n; i++) {
        const char *name = jstrv(pk->kid[i], "lookup_name"), *key = jstrv(pk->kid[i], "key");
        CHECK(name_hash(name) == hexkey(key), "tvg01: hash of %s", name);
        for (int ctx = 0; ctx < 2; ctx++)
            for (int sc = OM_SCREEN_DISPLAY; sc <= OM_SCREEN_CUSTOMIZE; sc++)
                if (!strcmp(name, om_package_name((OmContext)ctx, (OmScreen)sc))) {
                    CHECK(om_package_key((OmContext)ctx, (OmScreen)sc) == hexkey(key), "tvg01: key of %s", name);
                    seen++;
                }
        if (!strcmp(name, om_dialog_package_name())) seen++;
    }
    CHECK(seen == 5, "tvg01: %d of the 5 package names/contexts matched", seen);   /* Options, PauseOptions, Customize x2, dialog */
    jfree(j);
}

/* ---- behaviour beyond the vectors --------------------------------------------------------- */
static void test_behaviour(void) {
    Fx f;
    /* Display: Left/Right on the resolution row, supported indices only, no wrapping (SPEC 4.4) */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    f.caps.resolution_ok[2] = 0;
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
    CHECK(om_focus(&f.m) == 0, "initial focus");
    om_input(&f.m, OM_IN_LEFT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 0 && !om_reinit_pending(&f.m) && !om_apply_pending(&f.m), "Left at 0 does nothing");
    om_input(&f.m, OM_IN_RIGHT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 1 && om_reinit_pending(&f.m), "Right -> 1");
    om_input(&f.m, OM_IN_RIGHT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 3, "unsupported index 2 skipped");
    { uint32_t k; const char *t = om_row_value_text(&f.m, 0, &k); CHECK(t && !strcmp(t, "1280x960"), "text follows the value"); }
    om_input(&f.m, OM_IN_RIGHT); om_input(&f.m, OM_IN_RIGHT); om_input(&f.m, OM_IN_RIGHT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 5, "stops at 5, no wrap");
    om_input(&f.m, OM_IN_ACCEPT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 5, "Accept on Resolution does nothing");
    /* resolution outside 0..5 is left alone and keeps its previous text (SPEC_GUI 3.1) */
    f.g.v[ST_GFX_RESOLUTION] = 9;
    om_take_reinit_request(&f.m);
    om_input(&f.m, OM_IN_LEFT);
    CHECK(f.g.v[ST_GFX_RESOLUTION] == 9 && !om_reinit_pending(&f.m), "out-of-range resolution untouched");
    { uint32_t k; const char *t = om_row_value_text(&f.m, 0, &k); CHECK(t && !strcmp(t, "1600x1200"), "text left unchanged for index 9"); }
    /* buttons and sliders: Left/Right on a button, Accept on a slider do nothing */
    om_input(&f.m, OM_IN_DOWN);
    { StGraphics before = f.g; om_input(&f.m, OM_IN_ACCEPT); CHECK(!memcmp(&before, &f.g, sizeof before), "Accept on slider"); }
    press(&f, OM_IN_DOWN, 1);
    { StGraphics before = f.g; om_input(&f.m, OM_IN_LEFT); om_input(&f.m, OM_IN_RIGHT);
      CHECK(!memcmp(&before, &f.g, sizeof before) && om_screen(&f.m) == OM_SCREEN_DISPLAY, "Left/Right on a button"); }
    fx_close(&f);

    /* Level Of Detail: records the level, applies preset + clamp, refreshes the resolution text */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    f.g.v[ST_GFX_PERF_LEVEL] = 3;
    f.g.v[ST_GFX_RESOLUTION] = 3;
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
    om_input(&f.m, OM_IN_DOWN);
    om_take_reinit_request(&f.m);
    om_input(&f.m, OM_IN_RIGHT);
    {
        StGraphics ref;
        st_gfx_defaults(&ref);
        ref.v[ST_GFX_RESOLUTION] = 3;
        st_gfx_set_level(&ref, &f.caps, 4, f.cfg.gpu_score);
        CHECK(!memcmp(&ref, &f.g, sizeof ref), "LOD 4 == preset + clamp");
        CHECK(f.g.v[ST_GFX_PERF_LEVEL] == 4 && om_reinit_pending(&f.m), "LOD 4 recorded, re-init requested");
        uint32_t k;
        const char *t = om_row_value_text(&f.m, 0, &k);
        CHECK(t && !strcmp(t, "800x600") && f.g.v[ST_GFX_RESOLUTION] == 1, "resolution text refreshed by the preset (GPU score 5)");
    }
    om_input(&f.m, OM_IN_RIGHT);
    om_take_reinit_request(&f.m);
    om_take_apply_request(&f.m);
    om_input(&f.m, OM_IN_RIGHT);
    CHECK(f.g.v[ST_GFX_PERF_LEVEL] == 5 && !om_reinit_pending(&f.m) && !om_apply_pending(&f.m), "LOD at max does nothing");
    fx_close(&f);

    /* persistence: written on Back from Display only, 29 values in the 2.3 layout */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    f.g.v[ST_GFX_VSYNC] = 1;
    f.g.v[ST_GFX_TEXTURE_FILTERING] = 2;
    om_input(&f.m, OM_IN_BACK);
    settle(&f);
    { StGraphics back; st_gfx_defaults(&back); st_gfx_load(&f.store, &back);
      CHECK(back.v[ST_GFX_VSYNC] == 1 && back.v[ST_GFX_TEXTURE_FILTERING] == 2, "values round-trip through the store"); }
    fx_close(&f);

    /* no store: nothing persisted, still leaves; input and updates after close are inert */
    memset(&f, 0, sizeof f);
    st_gfx_defaults(&f.g); st_gfx_caps_full(&f.caps, 1); om_config_default(&f.cfg, OM_CTX_FRONTEND);
    om_open(&f.m, &f.cfg, &f.g, &f.caps, NULL, 0);
    om_input(&f.m, OM_IN_BACK); settle(&f);
    CHECK(om_poll_event(&f.m) == OM_EV_LEAVE_TO_HUB && f.m.registry_writes == 0, "leave without a store");
    om_input(&f.m, OM_IN_DOWN); om_input(&f.m, OM_IN_ACCEPT); settle(&f);
    CHECK(!om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_NONE, "closed menu ignores input");

    /* input is ignored while the exit animation runs; Start does nothing in the frontend / on Customize */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    om_input(&f.m, OM_IN_START);
    CHECK(om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_NONE, "frontend Start is inert");
    om_input(&f.m, OM_IN_BACK);
    om_input(&f.m, OM_IN_DOWN); om_input(&f.m, OM_IN_RIGHT);
    CHECK(om_focus(&f.m) == 0 && f.g.v[ST_GFX_RESOLUTION] == 0, "input ignored while exiting");
    fx_close(&f);
    fx_open(&f, OM_CTX_INGAME, 0);
    fx_customize(&f);
    om_input(&f.m, OM_IN_START);
    CHECK(om_screen(&f.m) == OM_SCREEN_CUSTOMIZE && om_poll_event(&f.m) == OM_EV_NONE, "Start on Customize is inert");
    fx_close(&f);

    /* in game the Display screen always opens on row 1: every path leaving it resets the remembered row (SPEC_GUI_ART 4.7) */
    fx_open(&f, OM_CTX_INGAME, 0);
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 3);
    CHECK(om_focus(&f.m) == 0, "in-game Display opens on row 1 whatever the remembered row");
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 9);
    CHECK(om_focus(&f.m) == 0, "remembered focus out of range");
    fx_close(&f);
    fx_open(&f, OM_CTX_FRONTEND, 0);
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 3);
    CHECK(om_focus(&f.m) == 0, "frontend ignores the remembered focus");
    fx_close(&f);

    /* Customize with 21 rows: scrolling, scrollbar, slider rows, values and toggles */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    fx_customize(&f);
    CHECK(om_row_count(&f.m) == 21 && om_scrollbar_visible(&f.m) && om_scroll_tab(&f.m) == 0.0f, "Customize 21 rows");
    press(&f, OM_IN_DOWN, 20);
    CHECK(om_focus(&f.m) == 20 && om_top(&f.m) == 13 && om_scroll_tab(&f.m) == 1.0f, "bottom: focus %d top %d", om_focus(&f.m), om_top(&f.m));
    CHECK(om_row_visible(&f.m, 13) && om_row_visible(&f.m, 20) && !om_row_visible(&f.m, 12), "visible window");
    press(&f, OM_IN_DOWN, 1);
    CHECK(om_focus(&f.m) == 20, "no wrap at the end");
    { const OmRow *r = om_row(&f.m, 20); CHECK(r->id == OM_ID_OPTION && r->gfx == ST_GFX_VSYNC, "last row is Vsync");
      om_input(&f.m, OM_IN_ACCEPT); CHECK(f.g.v[ST_GFX_VSYNC] == 1 && om_reinit_pending(&f.m), "Accept flips a toggle");
      om_input(&f.m, OM_IN_LEFT); CHECK(f.g.v[ST_GFX_VSYNC] == 0, "Left flips a toggle");
      f.g.v[ST_GFX_VSYNC] = 7; om_input(&f.m, OM_IN_RIGHT); CHECK(f.g.v[ST_GFX_VSYNC] == 0, "non-zero flips to 0"); }
    press(&f, OM_IN_UP, 20);
    CHECK(om_focus(&f.m) == 0 && om_top(&f.m) == 0, "back at the top");
    { StGraphics before = f.g; om_take_apply_request(&f.m); om_input(&f.m, OM_IN_ACCEPT);   /* slider */
      CHECK(!memcmp(&before, &f.g, sizeof before) && !om_apply_pending(&f.m), "Accept on a slider"); }
    om_input(&f.m, OM_IN_RIGHT);
    CHECK(f.g.v[ST_GFX_CAR_ENV_MAP] == 1 && om_reinit_pending(&f.m), "car reflection rate slider requests re-init");
    fx_close(&f);

    /* slider value above the cap is clamped for display and does not move on Right */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    f.caps.cap[ST_GFX_CAR_SHADOW] = 1;
    f.g.v[ST_GFX_CAR_SHADOW] = 2;
    fx_customize(&f);
    CHECK(find_row(&f.m,"Car Shadow / Neon") < 0,"vehicle shadow option removed");
    CHECK(f.g.v[ST_GFX_CAR_SHADOW]==2,"legacy shadow value preserved without rendering effect");
    /* FSAA without a known cap and zero FSAA levels: row absent */
    f.caps.cap[ST_GFX_FSAA] = ST_NO_CAP;
    om_input(&f.m, OM_IN_BACK); settle(&f); fx_customize(&f);
    CHECK(find_row(&f.m, "Full Screen Anti-Aliasing") < 0, "FSAA without a cap is hidden");
    fx_close(&f);

    /* caret spacer: focus skips it, window is built over 22 rows */
    fx_open(&f, OM_CTX_FRONTEND, 1);
    fx_customize(&f);
    CHECK(om_row_count(&f.m) == 22 && om_row(&f.m, 1)->kind == OM_ROW_SPACER && !om_row(&f.m, 1)->selectable, "spacer row");
    om_input(&f.m, OM_IN_DOWN);
    CHECK(om_focus(&f.m) == 2, "Down skips the spacer");
    om_input(&f.m, OM_IN_UP);
    CHECK(om_focus(&f.m) == 0, "Up skips the spacer");
    fx_close(&f);
    /* no first row (cap 0): the spacer is not inserted */
    fx_open(&f, OM_CTX_FRONTEND, 1);
    f.caps.cap[ST_GFX_CAR_ENV_MAP] = 0;
    fx_customize(&f);
    CHECK(om_row_count(&f.m) == 20 && om_row(&f.m, 0)->kind != OM_ROW_SPACER, "no spacer without row 1");
    fx_close(&f);

    /* resolution lowered by the host's display reset (SPEC_GUI 5.5) */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    f.g.v[ST_GFX_RESOLUTION] = 4;
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
    press(&f, OM_IN_DOWN, 2);
    f.g.v[ST_GFX_RESOLUTION] = 1;
    om_notify_resolution_lowered(&f.m);
    { uint32_t k; const char *t = om_row_value_text(&f.m, 0, &k);
      CHECK(t && !strcmp(t, "800x600") && om_focus(&f.m) == 0, "frontend Display rebuilt after the reset lowered the resolution"); }
    fx_close(&f);
    fx_open(&f, OM_CTX_INGAME, 0);
    f.g.v[ST_GFX_RESOLUTION] = 4;
    om_open(&f.m, &f.cfg, &f.g, &f.caps, &f.store, 0);
    press(&f, OM_IN_DOWN, 2);
    f.g.v[ST_GFX_RESOLUTION] = 1;
    om_notify_resolution_lowered(&f.m);
    { uint32_t k; const char *t = om_row_value_text(&f.m, 0, &k);
      CHECK(t && !strcmp(t, "1280x1024") && om_focus(&f.m) == 2, "in-game Display is not refreshed"); }
    fx_close(&f);

    /* pointer: focused row arrows and value text, button labels, dialog is modal (SPEC_GUI_ART 4.6) */
    fx_open(&f, OM_CTX_FRONTEND, 0);
    {
        float y0 = om_row_center_y(&f.m, 0);
        CHECK(NEAR(y0, -120.0f, 0.001f) && NEAR(om_row_center_y(&f.m, 3), -12.0f, 0.001f), "row centres follow TITLE_MASTER: %.2f", y0);
        om_pointer_click(&f.m, 246.2f, y0);                 /* right arrow */
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 1, "right arrow click = Right");
        om_pointer_click(&f.m, 127.8f, y0);                 /* value text */
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 2, "value text click = Right");
        om_pointer_click(&f.m, 6.2f, y0);                   /* left arrow */
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 1, "left arrow click = Left");
        om_pointer_click(&f.m, 6.0f - 12.0f, y0 - 12.0f);   /* edges are inclusive */
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 0, "arrow hit box edges are inclusive");
        om_pointer_click(&f.m, 6.0f - 12.1f, y0);
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 0, "just outside the arrow");
        om_pointer_click(&f.m, -100.0f, y0);                /* label of a choice row */
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 0, "choice label click is inert");
        om_pointer_click(&f.m, 246.2f, om_row_center_y(&f.m, 5)); /* outside the rows */
        om_pointer_click(&f.m, 400.0f, y0);
        CHECK(f.g.v[ST_GFX_RESOLUTION] == 0, "clicks outside the controls are inert");
        om_pointer_click(&f.m, -100.0f, om_row_center_y(&f.m, 3));/* Default Settings label */
        CHECK(om_dialog_is_open(&f.m) && om_focus(&f.m) == 3, "button label click opens the confirmation");
        om_pointer_click(&f.m, 246.2f, y0);
        CHECK(om_dialog_is_open(&f.m) && f.g.v[ST_GFX_RESOLUTION] == 0, "dialog is modal for the pointer");
        om_input(&f.m, OM_IN_UP);
        CHECK(om_focus(&f.m) == 3, "Up/Down do not reach the dialog");
        om_pointer_click(&f.m, 150.0f, 74.0f);               /* No button (centre +150, y 53..95) */
        settle(&f);
        CHECK(!om_dialog_is_open(&f.m) && f.g.v[ST_GFX_PERF_LEVEL] == f.g.v[ST_GFX_PERF_LEVEL], "pointer on No closes the dialog");
        om_pointer_click(&f.m, -100.0f, om_row_center_y(&f.m, 3));
        om_pointer_click(&f.m, -150.0f, 74.0f);              /* Yes */
        settle(&f);
        CHECK(!om_dialog_is_open(&f.m) && om_reinit_pending(&f.m), "pointer on Yes resets the defaults");
        om_pointer_click(&f.m, -100.0f, om_row_center_y(&f.m, 2));/* Customize Display Settings label */
        settle(&f);
        CHECK(om_screen(&f.m) == OM_SCREEN_CUSTOMIZE, "label click activates Customize");
    }
    fx_close(&f);

    /* title and package queries */
    fx_open(&f, OM_CTX_INGAME, 0);
    { uint32_t k; const char *t = om_header(OM_CTX_INGAME, OM_SCREEN_DISPLAY, 0, &k); CHECK(k == OM_KEY_TITLE_PAUSE && t && !strcmp(t, "PAUSE OPTIONS"), "in-game Header_text");
      t = om_header(OM_CTX_INGAME, OM_SCREEN_DISPLAY, 1, &k); CHECK(k == OM_KEY_TITLE_PAUSE_DISPLAY && t && !strcmp(t, "DISPLAY"), "in-game Header_text2");
      t = om_header(OM_CTX_INGAME, OM_SCREEN_CUSTOMIZE, 0, &k); CHECK(k == OM_KEY_TITLE_CUSTOMIZE && t && !strcmp(t, "Customize Display Settings"), "Customize header");
      CHECK(om_header(OM_CTX_INGAME, OM_SCREEN_CUSTOMIZE, 1, &k) == NULL && k == 0, "Customize has no second header");
      t = om_header(OM_CTX_INGAME, OM_SCREEN_HUB, 0, &k); CHECK(k == OM_KEY_TITLE_PAUSE && t && !strcmp(t, "PAUSE OPTIONS"), "hub header"); }
    fx_close(&f);
    fx_open(&f, OM_CTX_FRONTEND, 0);
    { uint32_t k; const char *t = om_header(OM_CTX_FRONTEND, OM_SCREEN_DISPLAY, 0, &k); CHECK(k == OM_KEY_TITLE_DISPLAY && t && !strcmp(t, "Display"), "frontend title");
      CHECK(om_header(OM_CTX_FRONTEND, OM_SCREEN_DISPLAY, 1, &k) == NULL, "frontend Display has one header"); }
    fx_close(&f);

}

/* ---- hub (SPEC_GUI_ART 2.5) ---------------------------------------------------------------- */
static void hub_open(Fx *f) {
    memset(f, 0, sizeof *f);
    st_gfx_defaults(&f->g);
    st_gfx_caps_full(&f->caps, 5);
    st_reg_open_memory(&f->store);
    om_config_default(&f->cfg, OM_CTX_INGAME);
    om_open_hub(&f->m, &f->cfg, &f->g, &f->caps, &f->store);
}
static void test_hub(void) {
    Fx f;
    hub_open(&f);
    CHECK(om_screen(&f.m) == OM_SCREEN_HUB && !strcmp(om_package_name(OM_CTX_INGAME, OM_SCREEN_HUB), "UI_PauseOptionsMain.fng") &&
          om_package_key(OM_CTX_INGAME, OM_SCREEN_HUB) == 0x73F51C74u, "hub package");
    CHECK(om_exit_ms(OM_CTX_INGAME, OM_SCREEN_HUB) == 320, "hub exit 320 ms");
    CHECK(om_hub_focus(&f.m) == 0 && om_hub_remembered(&f.m) == 0, "hub opens on Audio");
    const char *names[4] = { "Audio", "Display", "Gameplay", "Player" };
    const uint32_t keys[4] = { 0xE76CD783u, 0x4204D527u, 0xD0CF6EE1u, 0xF760EABEu };
    const uint32_t icons[4] = { 0xF37AF144u, 0x8A006328u, 0x4DF98FB2u, 0xD708EFEFu };
    for (int i = 0; i < OM_HUB_ENTRIES; i++) {
        const OmHubEntry *e = om_hub_entry(i);
        CHECK(e && !strcmp(e->label, names[i]) && e->label_key == keys[i] && e->icon_key == icons[i] && e->page == i, "hub entry %d", i);
    }
    CHECK(om_hub_entry(4) == NULL && om_hub_entry(-1) == NULL, "hub entry bounds");
    CHECK(f.m.hub.n == 4 && !gc_left_arrow_visible(&f.m.hub) && gc_right_arrow_visible(&f.m.hub), "hub carousel of 4");
    CHECK(NEAR(f.m.hub.rail_w, 564.0f, 0), "hub rail width");
    om_input(&f.m, OM_IN_UP); om_input(&f.m, OM_IN_DOWN);
    CHECK(om_hub_focus(&f.m) == 0, "Up/Down are ignored");
    om_input(&f.m, OM_IN_LEFT);
    CHECK(om_hub_focus(&f.m) == 0, "no wrap on the left end");
    unsigned b0 = f.m.blink_serial;
    om_input(&f.m, OM_IN_RIGHT);
    CHECK(om_hub_focus(&f.m) == 1 && om_hub_remembered(&f.m) == 2 && f.m.blink_serial == b0 + 1 && f.m.blink_dir == 1, "Right moves and is remembered");
    om_input(&f.m, OM_IN_RIGHT); om_input(&f.m, OM_IN_RIGHT); om_input(&f.m, OM_IN_RIGHT);
    CHECK(om_hub_focus(&f.m) == 3 && om_hub_remembered(&f.m) == 4, "no wrap on the right end");
    om_input(&f.m, OM_IN_LEFT); om_input(&f.m, OM_IN_LEFT);
    CHECK(om_hub_focus(&f.m) == 1, "back on Display");
    /* Accept on Display: 320 ms exit, then Display page 1 (rebuilt, row 1) */
    om_input(&f.m, OM_IN_ACCEPT);
    CHECK(om_screen(&f.m) == OM_SCREEN_HUB && f.m.exiting && f.m.page == 1, "Accept starts the exit and sets the page index");
    om_input(&f.m, OM_IN_LEFT); om_input(&f.m, OM_IN_BACK);
    CHECK(om_hub_focus(&f.m) == 1, "input ignored while the exit runs");
    om_update(&f.m, 0.319f);
    CHECK(om_screen(&f.m) == OM_SCREEN_HUB, "switch waits for exit-complete");
    om_update(&f.m, 0.002f);
    CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY && om_focus(&f.m) == 0 && om_row_count(&f.m) == 4, "Display after 320 ms");
    /* Display -> Back writes and returns to the hub on the remembered entry */
    f.g.v[ST_GFX_RESOLUTION] = 2;
    om_input(&f.m, OM_IN_BACK);
    om_update(&f.m, 0.319f);
    CHECK(om_screen(&f.m) == OM_SCREEN_DISPLAY && f.m.registry_writes == 0, "Display exit takes 320 ms in game");
    om_update(&f.m, 0.002f);
    CHECK(om_screen(&f.m) == OM_SCREEN_HUB && om_hub_focus(&f.m) == 1 && f.m.registry_writes == 1, "hub reopens on Display");
    CHECK(om_poll_event(&f.m) == OM_EV_LEAVE_TO_HUB && om_poll_event(&f.m) == OM_EV_NONE, "leave-to-hub event once");
    /* Audio/Gameplay/Player are outside this project's scope: activation does nothing */
    om_input(&f.m, OM_IN_LEFT);
    om_input(&f.m, OM_IN_ACCEPT);
    CHECK(om_screen(&f.m) == OM_SCREEN_HUB && !f.m.exiting, "Audio page is not implemented: stays on the hub");
    /* Back leaves to the pause screen and forgets the remembered entry */
    om_input(&f.m, OM_IN_BACK);
    CHECK(om_is_open(&f.m) && f.m.exiting, "Back starts the exit");
    om_update(&f.m, 0.35f);
    CHECK(!om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_LEAVE_TO_PAUSE && om_hub_remembered(&f.m) == 0, "hub Back: pause screen next, remembered entry reset");
    CHECK(f.m.registry_writes == 1, "hub Back writes nothing");
    /* Start: exit, then resume, remembered reset, nothing written */
    om_open_hub(&f.m, &f.cfg, &f.g, &f.caps, &f.store);
    om_input(&f.m, OM_IN_RIGHT);
    om_input(&f.m, OM_IN_START);
    om_update(&f.m, 0.1f);
    CHECK(om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_NONE, "Start waits for the exit");
    om_update(&f.m, 0.3f);
    CHECK(!om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_RESUME_GAME && om_hub_remembered(&f.m) == 0 && f.m.registry_writes == 1, "hub Start resumes");
    /* the pause screen resets the remembered entry when it is built */
    om_open_hub(&f.m, &f.cfg, &f.g, &f.caps, &f.store);
    om_input(&f.m, OM_IN_RIGHT);
    om_close(&f.m);
    CHECK(om_hub_remembered(&f.m) == 2, "closing keeps the remembered entry");
    om_open_hub(&f.m, &f.cfg, &f.g, &f.caps, &f.store);
    CHECK(om_hub_focus(&f.m) == 1, "reopened on the remembered entry");
    om_close(&f.m);
    om_reset_session(&f.m);
    om_open_hub(&f.m, &f.cfg, &f.g, &f.caps, &f.store);
    CHECK(om_hub_focus(&f.m) == 0, "session reset forgets it");
    /* Start on Display: exit 320 ms then resume, remembered reset */
    om_input(&f.m, OM_IN_RIGHT);
    om_input(&f.m, OM_IN_ACCEPT); om_update(&f.m, 0.4f);
    om_input(&f.m, OM_IN_START);
    om_update(&f.m, 0.3f);
    CHECK(om_is_open(&f.m), "Display Start waits for its 320 ms exit");
    om_update(&f.m, 0.03f);
    CHECK(!om_is_open(&f.m) && om_poll_event(&f.m) == OM_EV_RESUME_GAME && om_hub_remembered(&f.m) == 0, "Start on Display resumes and resets the remembered entry");
    /* pointer: arrows step at once, a centred click activates */
    om_open_hub(&f.m, &f.cfg, &f.g, &f.caps, &f.store);
    om_pointer_click(&f.m, GC_ARROW_L_X, GC_ARROW_Y);
    CHECK(om_hub_focus(&f.m) == 0, "hidden left arrow is inert");
    om_pointer_click(&f.m, GC_ARROW_R_X, GC_ARROW_Y);
    CHECK(om_hub_focus(&f.m) == 1, "right arrow steps");
    om_pointer_click(&f.m, GC_ARROW_R_X - 13.0f, GC_ARROW_Y + 13.0f);
    CHECK(om_hub_focus(&f.m) == 2, "arrow box edges are inclusive");
    om_pointer_click(&f.m, GC_ARROW_L_X, GC_ARROW_Y);
    CHECK(om_hub_focus(&f.m) == 1, "left arrow steps");
    om_pointer_click(&f.m, 158.0f, GC_ARROW_Y);              /* the neighbouring slot is inert */
    CHECK(om_hub_focus(&f.m) == 1 && !f.m.exiting, "neighbour click is inert");
    om_pointer_click(&f.m, 0.388f, -14.296f);                /* the selection slot */
    CHECK(f.m.exiting && f.m.page == 1, "click on the slot activates Display");
    st_reg_close(&f.store);
    puts("hub checks done");
}

static void test_mouse_look_setting(void) {
    Fx f;fx_open(&f,OM_CTX_INGAME,0);
    int enabled=0;f.cfg.mouse_look=&enabled;
    StGraphics original=f.g;
    om_open_hub(&f.m,&f.cfg,&f.g,&f.caps,&f.store);
    press(&f,OM_IN_RIGHT,2);om_input(&f.m,OM_IN_ACCEPT);settle(&f);
    CHECK(om_screen(&f.m)==OM_SCREEN_GAMEPLAY,"Gameplay opens optional mouse setting");
    CHECK(om_row_count(&f.m)==1 && om_row_value(&f.m,0)==0,"mouse look starts Off");
    const char *package=om_package_name(OM_CTX_INGAME,OM_SCREEN_GAMEPLAY);
    CHECK(package && !strcmp(package,"UI_PauseOptions.fng"),"Gameplay reuses authored options list");
    om_input(&f.m,OM_IN_RIGHT);CHECK(enabled==1 && !strcmp(om_row_value_text(&f.m,0,NULL),"On"),"Right enables mouse look");
    om_input(&f.m,OM_IN_ACCEPT);CHECK(enabled==0,"Accept disables mouse look");
    om_input(&f.m,OM_IN_LEFT);CHECK(enabled==1,"Left enables mouse look");
    om_input(&f.m,OM_IN_BACK);settle(&f);
    CHECK(om_screen(&f.m)==OM_SCREEN_HUB && om_hub_focus(&f.m)==2,"Gameplay Back returns to remembered hub entry");
    CHECK(f.m.registry_writes==0 && !memcmp(&original,&f.g,sizeof original),"mouse setting does not write graphics or alter tuning");
    om_input(&f.m,OM_IN_ACCEPT);settle(&f);om_input(&f.m,OM_IN_START);settle(&f);
    CHECK(!om_is_open(&f.m) && enabled==1 && om_poll_event(&f.m)==OM_EV_RESUME_GAME,"Gameplay Start resumes with chosen value");
    fx_close(&f);
}

int main(int argc, char **argv) {
    if (argc > 1) g_dir = argv[1];
    test_tvg01();
    test_tvg02();
    test_tvg03();
    test_tvg04();
    test_tvg05();
    test_tvg06();
    test_behaviour();
    test_hub();
    test_mouse_look_setting();
    printf("options_menu_test: %d checks, %d failed -> %s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
