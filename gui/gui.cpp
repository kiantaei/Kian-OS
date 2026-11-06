// Kian OS GUI - main entry: shell loop, app registry, overlays, shortcuts,
// dirty-region redraw with the cursor trick. C++17, freestanding, integers.
#include "gui.h"
#include "apps.h"
#include "ramfs.h"
#include "input.h"
#include "icons.h"
#include "util.h"

// Set to false: ksh already asked for a login before starting the GUI.
// The lock screen (Win+L / Ctrl+Alt+L) still works.
static const bool GUI_REQUIRES_LOGIN = false;

Shell *SH = 0;
Theme *THEME = 0;   // single definition (declared extern in theme.h)

static void slog(const char *s) {
    kian_serial_write(s, strlen(s));
}

// ---------------- app registry ----------------

struct AppDef { const char *title; int icon; int w, h; };
static const AppDef APPS[9] = {
    { "", 0, 0, 0 },                                 // APP_NONE
    { "Xenox Browser", IC_XENOX, 720, 480 },         // APP_XENOX
    { "Kian Terminal", IC_TERMINAL, 640, 420 },      // APP_TERM
    { "File Manager", IC_FILES, 640, 420 },          // APP_FILES
    { "Notepad", IC_NOTES, 520, 400 },               // APP_NOTES
    { "Settings", IC_SETTINGS, 620, 440 },           // APP_SETTINGS
    { "Task Manager", IC_TASKMGR, 460, 440 },        // APP_TASKMGR
    { "Calculator", IC_CALC, 264, 360 },             // APP_CALC
    { "Paint", IC_PAINT, 560, 440 }                  // APP_PAINT
};

static bool app_attach(Window *w, int app, const char *path) {
    switch (app) {
    case APP_XENOX:
        w->inst = xenox_create(); w->draw = xenox_draw;
        w->event = xenox_event; w->destroy = xenox_destroy;
        return w->inst != 0;
    case APP_TERM:
        w->inst = term_create(); w->draw = term_draw;
        w->event = term_event; w->destroy = term_destroy;
        return w->inst != 0;
    case APP_FILES:
        w->inst = files_create(); w->draw = files_draw;
        w->event = files_event; w->destroy = files_destroy;
        return w->inst != 0;
    case APP_NOTES:
        w->inst = path ? notes_create_path(path) : notes_create();
        w->draw = notes_draw; w->event = notes_event; w->destroy = notes_destroy;
        return w->inst != 0;
    case APP_SETTINGS:
        w->inst = setapp_create(); w->draw = setapp_draw;
        w->event = setapp_event; w->destroy = setapp_destroy;
        return w->inst != 0;
    case APP_TASKMGR:
        w->inst = taskmgr_create(); w->draw = taskmgr_draw;
        w->event = taskmgr_event; w->destroy = taskmgr_destroy;
        return w->inst != 0;
    case APP_CALC:
        w->inst = calc_create(); w->draw = calc_draw;
        w->event = calc_event; w->destroy = calc_destroy;
        return w->inst != 0;
    case APP_PAINT:
        w->inst = paint_create(); w->draw = paint_draw;
        w->event = paint_event; w->destroy = paint_destroy;
        return w->inst != 0;
    default:
        return false;
    }
}

void app_open(int app) {
    if (app <= APP_NONE || app > APP_PAINT || !SH) return;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Window *ex = &SH->wm.win[i];
        if (ex->used && ex->app == app) {
            SH->desk.cur_desk = ex->desk_id;
            SH->wm.cur_desk = ex->desk_id;
            if (ex->state == WS_MIN) ex->state = WS_NORM;
            wm_focus(&SH->wm, ex);
            shell_full_redraw();
            return;
        }
    }
    const AppDef &def = APPS[app];
    Window *w = wm_open(&SH->wm, app, def.title, def.icon, def.w, def.h, SH->desk.cur_desk);
    if (!w) { shell_toast("Too many windows (32 max)"); return; }
    if (!app_attach(w, app, 0)) {
        wm_close_id(&SH->wm, w->id);
        return;
    }
    shell_full_redraw();
}

void app_open_path(int app, const char *path) {
    if (!SH || app != APP_NOTES || !path) return;
    Window *w = wm_open(&SH->wm, app, "Notepad", IC_NOTES, APPS[app].w, APPS[app].h, SH->desk.cur_desk);
    if (!w) return;
    if (!app_attach(w, app, path)) {
        wm_close_id(&SH->wm, w->id);
        return;
    }
    shell_full_redraw();
}

void shell_set_title(const char *text) {
    if (!SH || !text) return;
    Window *fw = wm_focused(&SH->wm);
    if (!fw) return;
    int n = imin(31, (int)strlen(text));
    for (int i = 0; i < n; i++) fw->title[i] = text[i];
    fw->title[n] = '\0';
    fw->dirty = true;
    SH->taskbar_dirty = true;
}

void shell_full_redraw() { if (SH) SH->need_full = true; }

void shell_mark_content() {
    if (!SH) return;
    Window *fw = wm_focused(&SH->wm);
    if (fw) fw->dirty = true;
}

void shell_close_focused() {
    if (!SH) return;
    Window *fw = wm_focused(&SH->wm);
    if (fw) wm_close_id(&SH->wm, fw->id);
    shell_full_redraw();
}

void shell_toast(const char *text) {
    if (SH) toast_show(&SH->desk, text);
}

// ---------------- overlays (Ctrl+Alt+Del, Run, Alt+Tab) ----------------

static void draw_ctrldel() {
    blend_rect(0, 0, GFX->w, GFX->h, 0x00000000, 190);
    int w = 360, h = 240;
    int x = (GFX->w - w) / 2, y = (GFX->h - h) / 2;
    fill_round_rect(x, y, w, h, 8, THEME->menu_border);
    fill_round_rect(x + 1, y + 1, w - 2, h - 2, 8, THEME->menu_bg);
    draw_text_bold(x + 16, y + 14, "Ctrl+Alt+Del", THEME->text, 2);
    static const char *lbl[4] = { "Lock", "Task Manager", "Sign out", "Shut down" };
    for (int i = 0; i < 4; i++) {
        int by = y + 56 + i * 42;
        bool hov = mouse_in(x + 16, by, w - 32, 34);
        fill_round_rect(x + 16, by, w - 32, 34, 6, hov ? THEME->start_bg_active : THEME->button_bg);
        draw_text(x + 28, text_center_y(by, 34, 1), lbl[i], THEME->button_fg, 1);
    }
}

static bool ctrldel_event(const kian_event_t *e) {
    if (e->type == KIAN_EV_KEY_DOWN && (e->a == KIAN_KEY_ESC || e->b == 27)) {
        SH->ctrldel = false;
        return true;
    }
    if (e->type != KIAN_EV_MOUSE_BTN || !IN->press_l) return false;
    int w = 360, h = 240;
    int x = (GFX->w - w) / 2, y = (GFX->h - h) / 2;
    for (int i = 0; i < 4; i++) {
        int by = y + 56 + i * 42;
        if (mouse_in(x + 16, by, w - 32, 34)) {
            SH->ctrldel = false;
            if (i == 0) SH->lock_pending = true;
            else if (i == 1) app_open(APP_TASKMGR);
            else if (i == 2) SH->running = false; // sign out -> back to ksh
            else if (i == 3) kian_shutdown();
            return true;
        }
    }
    SH->ctrldel = false; // click outside closes
    return true;
}

static void draw_run() {
    int w = 380, h = 150;
    int x = (GFX->w - w) / 2, y = (GFX->h - h) / 2;
    fill_round_rect(x, y, w, h, 8, THEME->menu_border);
    fill_round_rect(x + 1, y + 1, w - 2, h - 2, 8, THEME->menu_bg);
    draw_text_bold(x + 16, y + 12, "Run", THEME->text, 2);
    draw_text(x + 16, y + 50, "Type a program name (xenox, terminal, explorer, taskmgr...)", THEME->text_dim, 1);
    SH->run_box.x = x + 16;
    SH->run_box.y = y + 72;
    SH->run_box.w = w - 32;
    SH->run_box.h = 30;
    tbox_draw(&SH->run_box);
}

static void run_launch(const char *name) {
    if (strcmp(name, "xenox") == 0 || strcmp(name, "browser") == 0 || strcmp(name, "web") == 0) app_open(APP_XENOX);
    else if (strcmp(name, "terminal") == 0 || strcmp(name, "term") == 0 || strcmp(name, "ksh") == 0) app_open(APP_TERM);
    else if (strcmp(name, "explorer") == 0 || strcmp(name, "files") == 0) app_open(APP_FILES);
    else if (strcmp(name, "notepad") == 0 || strcmp(name, "edit") == 0) app_open(APP_NOTES);
    else if (strcmp(name, "calc") == 0 || strcmp(name, "calculator") == 0) app_open(APP_CALC);
    else if (strcmp(name, "settings") == 0) app_open(APP_SETTINGS);
    else if (strcmp(name, "taskmgr") == 0 || strcmp(name, "task_manager") == 0 || strcmp(name, "task manager") == 0) app_open(APP_TASKMGR);
    else if (strcmp(name, "paint") == 0) app_open(APP_PAINT);
    else if (strcmp(name, "about") == 0) app_open(APP_SETTINGS);
    else shell_toast("Run: program not found");
}

static bool run_event(const kian_event_t *e) {
    if (e->type == KIAN_EV_KEY_DOWN) {
        if (e->a == KIAN_KEY_ESC || e->b == 27) { SH->run_open = false; return true; }
        if (tbox_key(&SH->run_box, e)) {
            SH->run_open = false;
            run_launch(tbox_text(&SH->run_box));
            return true;
        }
        return true; // run dialog owns keys while open
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        int w = 380, h = 150;
        int x = (GFX->w - w) / 2, y = (GFX->h - h) / 2;
        if (!rect_hit(IN->mx, IN->my, Rect{ x, y, w, h })) { SH->run_open = false; return true; }
        tbox_mouse(&SH->run_box, e);
        return true;
    }
    return false;
}

// ---------------- Alt+Tab ----------------

static const int ALTMAX = 32;

static int alt_list(int *ids, int cur_desk) {
    int n = 0;
    for (int i = SH->wm.order_n - 1; i >= 0 && n < ALTMAX; i--) {
        Window *w = &SH->wm.win[SH->wm.order[i]];
        if (w->used && w->desk_id == cur_desk) ids[n++] = w->id;
    }
    return n;
}

static void alttab_cycle(int cur_desk) {
    int ids[ALTMAX];
    int n = alt_list(ids, cur_desk);
    if (n == 0) return;
    if (!SH->alttab) {
        SH->alttab = true;
        SH->alttab_sel = (n > 1) ? 1 : 0;
    } else {
        SH->alttab_sel = (SH->alttab_sel + 1) % n;
    }
}

static void alttab_commit(int cur_desk) {
    int ids[ALTMAX];
    int n = alt_list(ids, cur_desk);
    SH->alttab = false;
    if (n == 0) return;
    int idx = iclamp(SH->alttab_sel, 0, n - 1);
    Window *w = wm_get(&SH->wm, ids[idx]);
    if (w) {
        if (w->state == WS_MIN) w->state = WS_NORM;
        wm_focus(&SH->wm, w);
    }
    shell_full_redraw();
}

static void draw_alttab(int cur_desk) {
    int ids[ALTMAX];
    int n = alt_list(ids, cur_desk);
    if (n == 0) return;
    int bw = 340, bh = 40 + n * 30;
    int x = (GFX->w - bw) / 2, y = (GFX->h - bh) / 2;
    fill_round_rect(x, y, bw, bh, 8, THEME->menu_border);
    fill_round_rect(x + 1, y + 1, bw - 2, bh - 2, 8, THEME->menu_bg);
    for (int i = 0; i < n; i++) {
        Window *w = wm_get(&SH->wm, ids[i]);
        if (!w) continue;
        int iy = y + 24 + i * 30;
        bool sel = (i == iclamp(SH->alttab_sel, 0, n - 1));
        if (sel) fill_round_rect(x + 8, iy - 4, bw - 16, 28, 4, THEME->start_bg_active);
        icon_draw(w->icon, x + 16, iy + 2, 16, THEME->accent);
        draw_text(x + 40, text_center_y(iy - 4, 28, 1), w->title, sel ? 0x00FFFFFF : THEME->text, 1);
    }
}

// ---------------- keyboard shortcuts ----------------

static bool shell_key(const kian_event_t *e) {
    int a = e->a, b = e->b, c = e->c;
    bool ctrl = (c & 2) != 0, alt = (c & 4) != 0, shift = (c & 1) != 0, win = (c & 8) != 0;

    // emergency exit: Ctrl+Alt+Q
    if (ctrl && alt && (b == 'q' || b == 'Q')) {
        SH->running = false;
        return true;
    }
    // lock: Win+L or Ctrl+Alt+L
    if ((win && (b == 'l' || b == 'L')) || (ctrl && alt && (b == 'l' || b == 'L'))) {
        SH->lock_pending = true;
        return true;
    }
    // Ctrl+Shift+Esc -> Task Manager
    if (ctrl && shift && a == KIAN_KEY_ESC) {
        app_open(APP_TASKMGR);
        return true;
    }
    // Ctrl+Alt+Del screen
    if (ctrl && alt && a == KIAN_KEY_DEL) {
        SH->ctrldel = true;
        return true;
    }
    // Alt+Tab switcher
    if (alt && a == KIAN_KEY_TAB) {
        alttab_cycle(SH->desk.cur_desk);
        return true;
    }
    // Win+Tab -> task view
    if (win && a == KIAN_KEY_TAB) {
        SH->desk.taskview = !SH->desk.taskview;
        return true;
    }
    // Ctrl+Win+Left/Right -> virtual desktops
    if (ctrl && win && a == KIAN_KEY_LEFT) { desk_switch(&SH->desk, -1); return true; }
    if (ctrl && win && a == KIAN_KEY_RIGHT) { desk_switch(&SH->desk, 1); return true; }
    // Win+R -> Run
    if (win && (b == 'r' || b == 'R')) {
        SH->run_open = true;
        tbox_init(&SH->run_box, 0, 0, 340, 30, false);
        tbox_focus(&SH->run_box, true);
        return true;
    }
    // Win+D -> show desktop / restore
    static bool desk_min = false;
    if (win && (b == 'd' || b == 'D')) {
        if (desk_min) wm_restore_all(&SH->wm, SH->desk.cur_desk);
        else wm_minimize_all(&SH->wm, SH->desk.cur_desk);
        desk_min = !desk_min;
        return true;
    }
    // Win+Arrows -> snap
    if (win && a == KIAN_KEY_LEFT && wm_focused(&SH->wm)) {
        Window *w = wm_focused(&SH->wm);
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        Rect r = wm_snap_rect(1, GFX->w, GFX->h);
        w->x = r.x; w->y = r.y; w->w = r.w; w->h = r.h;
        w->dirty = true;
        return true;
    }
    if (win && a == KIAN_KEY_RIGHT && wm_focused(&SH->wm)) {
        Window *w = wm_focused(&SH->wm);
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        Rect r = wm_snap_rect(2, GFX->w, GFX->h);
        w->x = r.x; w->y = r.y; w->w = r.w; w->h = r.h;
        w->dirty = true;
        return true;
    }
    if (win && a == KIAN_KEY_UP && wm_focused(&SH->wm)) {
        wm_toggle_max(wm_focused(&SH->wm), GFX->w, GFX->h);
        return true;
    }
    // Alt+Shift -> ENG/FA tray label (label only)
    if (alt && shift) {
        SH->desk.lang_fa = !SH->desk.lang_fa;
        return false;
    }
    return false;
}

// ---------------- redraw ----------------

static Rect screen_rect() {
    Rect r = { 0, 0, GFX->w, GFX->h };
    return r;
}

static void apply_screen_overlays(Rect r) {
    // brightness dimming
    int dim = ((100 - SH->desk.brightness) * 180) / 100;
    if (dim > 0) blend_rect(r.x, r.y, r.w, r.h, 0x00000000, iclamp(dim, 0, 180));
    // night light: warm tint
    if (SH->desk.night) blend_rect(r.x, r.y, r.w, r.h, 0x00A06010, 36);
}

static void mark_region(Rect r) {
    // repaint everything that intersects r, then present exactly r
    Rect wr = r;
    wr.x -= 8; wr.y -= 8; wr.w += 16; wr.h += 16;

    // wallpaper behind the region
    gradient_v(wr.x, wr.y, wr.w, wr.h, THEME->wallpaper_top, THEME->wallpaper_bottom);

    // desktop icons
    for (int i = 0; i < 4; i++) {
        Rect ir = { 12, 12 + i * 96, 76, 88 };
        if (rect_overlap(ir, wr)) {
            clip_set(wr);
            desk_draw_icons(&SH->desk);
            clip_reset();
            break;
        }
    }

    // windows bottom -> top (full window draw is deterministic; only r is presented)
    for (int i = 0; i < SH->wm.order_n; i++) {
        Window *w = &SH->wm.win[SH->wm.order[i]];
        if (!w->used || w->desk_id != SH->desk.cur_desk || w->state == WS_MIN) continue;
        Rect wr2 = { w->x, w->y, w->w, w->h };
        if (rect_overlap(wr2, wr)) wm_draw_one(&SH->wm, w);
    }

    // taskbar if touched
    Rect tb = { 0, GFX->h - 48, GFX->w, 48 };
    if (rect_overlap(tb, wr)) {
        taskbar_draw(&SH->desk);
        if (r.y + r.h > GFX->h - 48) r.h = GFX->h - r.y; // present down to screen bottom
    }

    apply_screen_overlays(r);
    present(r.x, r.y, r.w, r.h);
    scene_snap_rect(r.x, r.y, r.w, r.h);
}

static void redraw_all() {
    clip_reset();
    desk_draw_wallpaper(&SH->desk);
    desk_draw_icons(&SH->desk);
    wm_draw_all(&SH->wm, SH->desk.cur_desk);
    taskbar_draw(&SH->desk);
    if (SH->desk.start_open) start_menu_draw(&SH->desk);
    if (SH->desk.quick_open) quick_draw(&SH->desk);
    if (SH->desk.cal_open) cal_draw(&SH->desk);
    if (SH->desk.toast_on) toast_draw(&SH->desk);
    if (SH->desk.taskview) taskview_draw(&SH->desk);
    if (SH->ctrldel) draw_ctrldel();
    if (SH->run_open) draw_run();
    if (SH->alttab) draw_alttab(SH->desk.cur_desk);
    if (SH->wm.snap_zone != 0) {
        Rect pr = wm_snap_rect(SH->wm.snap_zone, GFX->w, GFX->h);
        blend_rect(pr.x, pr.y, pr.w, pr.h, THEME->accent, 90);
        rect_outline(pr.x, pr.y, pr.w, pr.h, THEME->accent);
    }
    apply_screen_overlays(screen_rect());
    scene_snap_all();
    present(0, 0, GFX->w, GFX->h);
    cursor_reset_anchor();
    cursor_present();
    SH->need_full = false;
    SH->taskbar_dirty = false;
    SH->clock_second = 0xFFFFFFFF;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) SH->wm.win[i].dirty = false;
}

// ---------------- main ----------------

extern "C" int gui_main(const kian_fb_t *fb) {
    // Emergency check: GUI needs >= 800x600 at 32 bpp
    if (fb->bpp != 32 || fb->width < 800 || fb->height < 600) {
        kprintf("GUI: needs >= 800x600 at 32bpp (got %lux%lu bpp=%lu)\n",
                (unsigned long)fb->width, (unsigned long)fb->height, (unsigned long)fb->bpp);
        return 1;
    }

    // G1: gfx core (frame + scene). Check the heap had room.
    if (!gfx_init(fb)) {
        kprintf("GUI: out of memory (frame + scene buffers)\n");
        return 1;
    }
    slog("GUI: step G1 ok (gfx)\n");

    Shell *sh = new Shell();
    if (!sh) {
        kprintf("GUI: out of memory (shell)\n");
        gfx_free();
        return 1;
    }
    SH = sh;
    theme_set(&sh->theme, true, 0x0078D4);
    THEME = &sh->theme;
    input_init(&sh->input);
    sh->menu.open = false;
    slog("GUI: step G2 ok (input/cursor)\n");

    ramfs_init();
    slog("GUI: step G3 ok (ramfs)\n");

    wm_init(&sh->wm);
    slog("GUI: step G4 ok (wm)\n");

    desk_init(&sh->desk, &sh->wm);
    sh->wm.cur_desk = 0;
    slog("GUI: step G5 ok (desktop shell)\n");

    if (!login_run(GUI_REQUIRES_LOGIN)) {
        delete sh;
        SH = 0;
        THEME = 0;
        gfx_free();
        return 0;
    }
    slog("GUI: step G6 ok (login path)\n");

    toast_show(&sh->desk, "Welcome, kian!");
    sh->running = true;
    sh->need_full = true;
    sh->taskbar_dirty = false;
    sh->clock_second = 0xFFFFFFFF;

    int exit_code = 0;
    while (sh->running) {
        // ---- events ----
        for (;;) {
            kian_event_t ev;
            if (!input_next(&sh->input, &ev)) break;

            bool consumed = false;
            if (ev.type == KIAN_EV_KEY_DOWN) consumed = shell_key(&ev);
            if (!consumed && ev.type == KIAN_EV_KEY_UP && sh->alttab && (ev.c & 4) == 0) {
                alttab_commit(sh->desk.cur_desk);
                consumed = true;
            }

            if (!consumed && sh->menu.open) consumed = menu_event(&sh->menu, &ev);
            if (!consumed && sh->run_open) consumed = run_event(&ev);
            if (!consumed && sh->ctrldel) consumed = ctrldel_event(&ev);
            if (!consumed && sh->desk.taskview) consumed = taskview_event(&sh->desk, &ev);
            if (!consumed && sh->desk.start_open) consumed = start_menu_event(&sh->desk, &ev);
            if (!consumed && sh->desk.quick_open) consumed = quick_event(&sh->desk, &ev);
            if (!consumed && sh->desk.cal_open) consumed = cal_event(&sh->desk, &ev);

            if (!consumed && (ev.type == KIAN_EV_MOUSE_MOVE || ev.type == KIAN_EV_MOUSE_BTN || ev.type == KIAN_EV_MOUSE_WHEEL)) {
                consumed = taskbar_event(&sh->desk, &ev);
                if (!consumed && ev.type != KIAN_EV_MOUSE_MOVE && sh->input.my < GFX->h - 48) {
                    consumed = desk_event(&sh->desk, &ev);
                }
                if (!consumed) consumed = wm_mouse(&sh->wm, &ev);
            }

            if (!consumed) {
                Window *fw = wm_focused(&sh->wm);
                if (fw && fw->desk_id == sh->desk.cur_desk && fw->state != WS_MIN && fw->event) {
                    Rect c2 = wm_content(fw);
                    clip_set(c2);
                    fw->event(fw->inst, &ev, c2);
                    clip_reset();
                    fw->dirty = true;
                    if (ev.type == KIAN_EV_KEY_DOWN && (ev.a == KIAN_KEY_ESC || ev.b == 27)) {
                        consumed = true;   // the focused app handled ESC itself
                    }
                }
            }

            // emergency exit #2: ESC only when nothing consumed it
            if (!consumed && ev.type == KIAN_EV_KEY_DOWN && (ev.a == KIAN_KEY_ESC || ev.b == 27)) {
                sh->running = false;
                break;
            }

            if (consumed) sh->need_full = true;
            if (ev.type == KIAN_EV_MOUSE_MOVE) {
                if (!sh->need_full && !sh->wm.dragging && sh->wm.resize_mode == 0) {
                    cursor_move_redraw();
                }
            }
            if (!sh->running) break;
        }
        if (!sh->running) break;

        // ---- clock + toast ticks ----
        kian_time_t now;
        kian_time_now(&now);
        if ((unsigned)now.second != sh->clock_second) {
            sh->taskbar_dirty = true;
            sh->clock_second = (unsigned)now.second;
        }
        bool toast_was = sh->desk.toast_on;
        toast_tick(&sh->desk);
        if (toast_was && !sh->desk.toast_on) {
            Rect tr = toast_region(&sh->desk);
            mark_region(tr);
        }
        if (sh->desk.exit_gui) {
            sh->running = false; // "Exit to console" (emergency exit #3)
            break;
        }
        if (sh->lock_pending) {
            sh->lock_pending = false;
            login_lock();
            sh->need_full = true;
        }

        // ---- redraw ----
        if (sh->need_full) {
            redraw_all();
        } else {
            for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                Window *w = &sh->wm.win[i];
                if (w->used && w->dirty && w->desk_id == sh->desk.cur_desk && w->state != WS_MIN) {
                    mark_region(wm_content(w));
                    w->dirty = false;
                }
            }
            if (sh->taskbar_dirty) {
                taskbar_draw(&sh->desk);
                Rect tb = { 0, GFX->h - 48, GFX->w, 48 };
                apply_screen_overlays(tb);
                present(tb.x, tb.y, tb.w, tb.h);
                scene_snap_rect(tb.x, tb.y, tb.w, tb.h);
                cursor_present();
                sh->taskbar_dirty = false;
            }
        }

        // ---- idle: low CPU, small spin between frames ----
        unsigned t0 = now_ms();
        while (now_ms() - t0 < 8) {
            __asm__ volatile("pause");
        }
    }

    exit_code = 0; // Ctrl+Alt+Q / ESC / "Exit to console" all return 0
    // cleanup: destroy all app instances, then the shell
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Window *w = &sh->wm.win[i];
        if (w->used && w->destroy && w->inst) w->destroy(w->inst);
        w->inst = 0;
        w->used = false;
    }
    delete sh;
    SH = 0;
    THEME = 0;
    gfx_free();
    return exit_code;
}
