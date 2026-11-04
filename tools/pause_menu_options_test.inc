/* Pause menu <-> graphics options screens (SPEC_GUI, SPEC_GUI_ART 2.5, 5): the Options entry, the hub,
 * Display, Customize, the Default Settings dialog, Back/Start leaving, the apply/re-init hook, and the
 * authored views that follow it. Synthetic packages through the adapter's real load path; no runtime
 * resources. */
static SDL_Event options_key(int sym) {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = sym;
    return e;
}
static SDL_Event options_pad(int button) {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.button = button;
    return e;
}
static void key(PauseMenu *p, int sym) { SDL_Event e = options_key(sym); pause_menu_event(p, &e, NULL); }
static void pad(PauseMenu *p, int button) { SDL_Event e = options_pad(button); pause_menu_event(p, &e, NULL); }
/* settles the 320 ms exit scripts and the dialog exits */
static FeAction settle_ms(PauseMenu *p, int ms) {
    FeAction a = FE_ACTION_NONE;
    for (int t = 0; t < ms; t += 10) { FeAction x = pause_menu_update(p, .01f); if (x != FE_ACTION_NONE) a = x; }
    return a;
}
static const char *view_name(const PauseMenu *p) { return p->scr.loaded ? p->scr.name : "(none)"; }

static void options_checks(void) {
    /* without settings the catalogue has no Options entry: Return to Explore Mode, Replay, Quit */
    PauseMenu *plain = (PauseMenu *)calloc(1, sizeof *plain);
    plain->assets.main_face = -1;
    pause_menu_open(plain);
    assert(plain->ps.n == 3 && plain->ps.entries[0].id == PE_EXPLORE_FREE && plain->ps.entries[1].id == PE_REPLAY && plain->ps.entries[2].id == PE_QUIT_MAIN);
    assert(!pause_menu_options_open(plain) && !plain->visual);       /* no resources: logic only, nothing drawn */
    free(plain);

    PauseMenu *p = (PauseMenu *)calloc(1, sizeof *p);
    binding_fixture_init(p);
    st_gfx_defaults(&fx_gfx);
    fx_gfx.v[ST_GFX_RESOLUTION] = 2;
    assert(p->ps.n == 4 && p->ps.entries[1].id == PE_OPTIONS && !strcmp(pause_menu_caption(p), "Return to Explore Mode"));
    assert(p->view == PM_PAUSE && !strcmp(view_name(p), "UI_pause.fng"));
    pause_menu_update(p, 0);
    key(p, SDLK_RIGHT);
    pause_menu_update(p, 0);
    assert(p->ps.car.cur == 1 && !strcmp(pause_menu_caption(p), "Options"));
    assert(!strcmp(p->scr.scene.nodes[p->car_v.title].text_override, "Options (table)"));   /* the language table wins over the English fallback */
    key(p, SDLK_RETURN);
    pause_menu_update(p, 0);
    assert(p->ps.exiting && !pause_menu_options_open(p));
    assert(ug_scene_script_id(&p->scr.scene, p->scr.handler) == hn("FORWARD"));                /* the authored exit plays */
    assert(pause_menu_update(p, .3f) == FE_ACTION_NONE && !pause_menu_options_open(p));
    assert(pause_menu_update(p, .03f) == FE_ACTION_NONE);                                    /* the action stays inside the pause menu */
    assert(pause_menu_options_open(p) && p->open && om_screen(pause_menu_options(p)) == OM_SCREEN_HUB);
    assert(pause_menu_options(p)->cfg.ctx == OM_CTX_INGAME);
    assert(p->view == PM_HUB && !strcmp(view_name(p), "UI_PauseOptionsMain.fng") && p->visual);
    /* hub: Audio first, four entries, the header, no pills */
    OptionsMenu *om = pause_menu_options(p);
    assert(om_hub_focus(om) == 0 && !strcmp(pause_menu_caption(p), "Audio"));
    assert(!strcmp(p->scr.scene.nodes[p->car_v.header].text_override, "PAUSE OPTIONS"));
    assert(!pause_pills_visible(p));
    key(p, SDLK_UP); key(p, SDLK_DOWN);
    assert(om_hub_focus(om) == 0);                                                       /* Up/Down ignored */
    key(p, SDLK_RIGHT);
    pause_menu_update(p, 0);
    assert(om_hub_focus(om) == 1 && !strcmp(pause_menu_caption(p), "Display") && !strcmp(p->scr.scene.nodes[p->car_v.title].text_override, "Display"));
    assert(ug_scene_script_id(&p->scr.scene, p->car_v.arrow_r) == hn("Blink"));
    pause_menu_update(p, .35f);
    key(p, SDLK_RETURN);                                                                 /* Display: exit then page 1 */
    assert(om->exiting && om_screen(om) == OM_SCREEN_HUB);
    key(p, SDLK_RIGHT);
    assert(om_hub_focus(om) == 1);                                                       /* ignored while exiting */
    settle_ms(p, 330);
    assert(om_screen(om) == OM_SCREEN_DISPLAY && p->view == PM_LIST && !strcmp(view_name(p), "UI_PauseOptions.fng"));
    assert(!strcmp(p->scr.scene.nodes[p->list_v.header].text_override, "PAUSE OPTIONS") && !strcmp(p->scr.scene.nodes[p->list_v.header2].text_override, "DISPLAY"));
    assert(om_focus(om) == 0 && !p->scr.scene.nodes[p->list_v.row[1].name].app_hidden);

    /* input goes to the options screens: Right changes the resolution, nothing reaches the pause row */
    key(p, SDLK_RIGHT);
    pause_menu_update(p, 0);
    assert(fx_gfx.v[ST_GFX_RESOLUTION] == 3 && om_reinit_pending(om));
    assert(om_take_reinit_request(om) == 1 && om_take_reinit_request(om) == 0);                 /* hook: consumed once */
    assert(!strcmp(p->scr.scene.nodes[p->list_v.row[1].data].text_override, "1280x960"));
    key(p, SDLK_DOWN);
    assert(om_focus(om) == 1);
    pause_menu_pointer(p, 320 + 246.0f, 240 + om_row_center_y(om, 0), 640, 480);        /* complete click on the right arrow */
    assert(fx_gfx.v[ST_GFX_RESOLUTION] == 3);                                              /* row 1 is not focused: inert */
    /* Customize and back */
    key(p, SDLK_DOWN);
    key(p, SDLK_RETURN);
    settle_ms(p, 330);
    assert(om_screen(om) == OM_SCREEN_CUSTOMIZE && !strcmp(view_name(p), "UI_PC_Customize_Options.fng") && om_row_count(om) == 21);
    assert(!strcmp(p->scr.scene.nodes[p->list_v.header].text_override, "Customize Display Settings"));
    key(p, SDLK_ESCAPE);
    assert(om->exiting && om_exit_ms(OM_CTX_INGAME, OM_SCREEN_CUSTOMIZE) == 140);
    settle_ms(p, 90);
    assert(om_screen(om) == OM_SCREEN_CUSTOMIZE);
    settle_ms(p, 60);
    assert(om_screen(om) == OM_SCREEN_DISPLAY && !strcmp(view_name(p), "UI_PauseOptions.fng") && om_focus(om) == 0);

    /* Default Settings: the dialog opens focused on the second button (No), no wrap */
    key(p, SDLK_DOWN); key(p, SDLK_DOWN); key(p, SDLK_DOWN);
    fx_gfx.v[ST_GFX_LIGHT_GLOW] = 1;
    key(p, SDLK_RETURN);
    assert(om_dialog_is_open(om) && om_dialog_focus(om) == 1);
    pause_menu_update(p, 0);
    assert(p->dlg_loaded && !strcmp(p->dlg_v.scr.name, "GenericDialog_SMALL.fng") && p->dlg_v.scr.scene.focus == p->dlg_v.b2);
    assert(pause_menu_dialog_open(p));
    assert(!strcmp(p->dlg_v.scr.scene.nodes[p->dlg_v.msg].text_override, "Reset to default settings?"));
    key(p, SDLK_RIGHT);
    assert(om_dialog_focus(om) == 1);
    key(p, SDLK_LEFT);
    pause_menu_update(p, 0);
    assert(om_dialog_focus(om) == 0 && p->dlg_v.scr.scene.focus == p->dlg_v.b1);
    key(p, SDLK_LEFT);
    assert(om_dialog_focus(om) == 0);
    key(p, SDLK_RETURN);                                                                 /* Yes, after the 140 ms exit */
    assert(p->dlg_v.scr.exit_started == 0);
    pause_menu_update(p, .05f);
    assert(p->dlg_v.scr.exit_started && fx_gfx.v[ST_GFX_LIGHT_GLOW] == 1);
    settle_ms(p, 100);
    assert(!om_dialog_is_open(om) && !p->dlg_loaded && fx_gfx.v[ST_GFX_PERF_LEVEL] == 4 && om_reinit_pending(om) && om_focus(om) == 0);
    /* Back to the hub: the registry is written when the exit script ends (320 ms) */
    key(p, SDLK_ESCAPE);
    settle_ms(p, 300);
    { uint32_t v = 0; assert(!st_reg_get_dword(&fx_store, "g_RacingResolution", &v)); }                  /* not written yet */
    assert(om_screen(om) == OM_SCREEN_DISPLAY);
    settle_ms(p, 40);
    assert(om_screen(om) == OM_SCREEN_HUB && !strcmp(view_name(p), "UI_PauseOptionsMain.fng") && om_hub_focus(om) == 1);   /* reopens on Display */
    { uint32_t v = 0; assert(st_reg_get_dword(&fx_store, "g_VSyncOn", &v)); }
    assert(om_apply_pending(om));                                                          /* flags survive until consumed */
    /* Back from the hub: a fresh pause screen on entry 1 */
    key(p, SDLK_ESCAPE);
    settle_ms(p, 330);
    assert(!pause_menu_options_open(p) && p->open && p->view == PM_PAUSE && p->ps.car.cur == 0);
    assert(om_hub_remembered(om) == 0);
    pause_menu_update(p, 0);
    assert(pause_pills_visible(p));
    /* hub again, then controller Start: exit, resume at once afterwards, nothing written */
    key(p, SDLK_RIGHT);
    key(p, SDLK_RETURN);
    settle_ms(p, 330);
    assert(pause_menu_options_open(p) && om_screen(om) == OM_SCREEN_HUB);
    int writes = om->registry_writes;
    pad(p, SDL_CONTROLLER_BUTTON_START);
    assert(om->exiting && p->open);
    assert(settle_ms(p, 300) == FE_ACTION_NONE);
    assert(settle_ms(p, 40) == FE_ACTION_RESUME && !p->open && !pause_menu_options_open(p));
    assert(om->registry_writes == writes);
    /* Start on Display, after Left/Right moved the hub: resets the remembered entry */
    pause_menu_open(p);
    pause_menu_update(p, 0);
    key(p, SDLK_RIGHT); key(p, SDLK_RETURN); settle_ms(p, 330);                          /* pause -> hub */
    key(p, SDLK_RIGHT); key(p, SDLK_RETURN); settle_ms(p, 330);                          /* hub: Display */
    assert(om_screen(om) == OM_SCREEN_DISPLAY && om_hub_remembered(om) == 2);
    pad(p, SDL_CONTROLLER_BUTTON_START);
    assert(settle_ms(p, 400) == FE_ACTION_RESUME && !p->open);
    assert(om_hub_remembered(om) == 0 && om->registry_writes == writes);

    /* re-opening drops stale options state; Quit is entry 4 and opens its dialog */
    pause_menu_open(p);
    assert(!pause_menu_options_open(p) && p->ps.n == 4);
    pause_menu_update(p, 0);
    key(p, SDLK_RIGHT); key(p, SDLK_RIGHT); key(p, SDLK_RETURN);
    assert(p->ps.car.cur == 3 && ps_dialog_open(&p->ps));
    pause_menu_update(p, 0);
    assert(p->dlg_loaded && !strcmp(p->dlg_v.scr.name, "UI_InGameDialog.fng") && p->dlg_v.scr.scene.focus == p->dlg_v.b2);
    assert(!strcmp(p->dlg_v.scr.scene.nodes[p->dlg_v.lbl1].text_override, "OK") && !strcmp(p->dlg_v.scr.scene.nodes[p->dlg_v.lbl2].text_override, "Cancel"));
    assert(!strcmp(p->dlg_v.scr.scene.nodes[p->dlg_v.msg].text_override, "Quit to Main Menu?"));
    assert(!pause_pills_visible(p));
    /* Cancel by Back: the dialog plays its exit, the pause screen accepts input again */
    key(p, SDLK_ESCAPE);
    assert(settle_ms(p, 100) == FE_ACTION_NONE && ps_dialog_open(&p->ps));
    assert(settle_ms(p, 60) == FE_ACTION_NONE && !ps_dialog_open(&p->ps) && !p->dlg_loaded && p->ps.car.fade);
    key(p, SDLK_LEFT);
    assert(p->ps.car.cur == 1);
    /* OK quits after the dialog exit */
    key(p, SDLK_RIGHT); key(p, SDLK_RETURN);
    pause_menu_update(p, 0);
    key(p, SDLK_LEFT); key(p, SDLK_RETURN);
    assert(settle_ms(p, 130) == FE_ACTION_NONE && p->open);
    assert(settle_ms(p, 30) == FE_ACTION_QUIT && !p->open);
    fx_remove(p);
    free(p);
    puts("pause options checks: PASS");
}
