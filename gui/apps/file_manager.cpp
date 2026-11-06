// Kian OS GUI - File Manager (Explorer style) over the RAM filesystem.
// HONEST: RAM only, lost on reboot (status bar says so).
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"

enum { FDLG_NONE = 0, FDLG_NEWFOLDER, FDLG_NEWFILE, FDLG_RENAME };
enum { FS_HOME = 0, FS_DESKTOP, FS_DOCS, FS_DOWN, FS_TRASH };

struct FileApp {
    RNode *cur;
    RNode *hist[16];
    int nhist;
    bool icons_view;
    int sel;
    RNode *items[64];
    int nitems;
    char status[96];
    int dialog;        // FDLG_*
    TBox dlg;
    ListView list;
};

static void files_refresh(FileApp *f) {
    f->nitems = ramfs_list(f->cur, f->items, 64);
    if (f->sel >= f->nitems) f->sel = f->nitems - 1;
}

static void files_nav(FileApp *f, RNode *dir) {
    if (!dir || !ramfs_is_dir(dir)) return;
    if (f->nhist < 16) f->hist[f->nhist++] = f->cur;
    f->cur = dir;
    f->sel = -1;
    files_refresh(f);
    f->status[0] = '\0';
}

static void files_back(FileApp *f) {
    if (f->nhist > 0) {
        f->cur = f->hist[--f->nhist];
        f->sel = -1;
        files_refresh(f);
    }
}

static void files_up(FileApp *f) {
    if (f->cur->parent && f->cur->parent != ramfs_trash()) files_nav(f, f->cur->parent);
}

static void files_set_status_sel(FileApp *f) {
    if (f->sel >= 0 && f->sel < f->nitems) {
        RNode *n = f->items[f->sel];
        char num[16];
        int i = 0;
        num[0] = '\0';
        put_num(num, i, ramfs_is_dir(n) ? (int)n->nchild : ramfs_size(n));
        num[i] = '\0';
        char *s = f->status;
        int k = 0;
        const char *p1 = ramfs_is_dir(n) ? "items: " : "bytes: ";
        for (; *p1 != '\0'; p1++) s[k++] = *p1;
        for (int m = 0; num[m] != '\0' && k < 90; m++) s[k++] = num[m];
        s[k] = '\0';
    } else {
        char num[16];
        int i = 0;
        num[0] = '\0';
        put_num(num, i, f->nitems);
        num[i] = '\0';
        char *s = f->status;
        int k = 0;
        const char *p1 = "RAM only \x7c items: ";
        for (; *p1 != '\0'; p1++) s[k++] = *p1;
        for (int m = 0; num[m] != '\0' && k < 90; m++) s[k++] = num[m];
        s[k] = '\0';
    }
}

static void files_menu_cb(void *ud, int id);

static void files_open_item(FileApp *f, int idx) {
    if (idx < 0 || idx >= f->nitems) return;
    RNode *n = f->items[idx];
    if (ramfs_is_dir(n)) { files_nav(f, n); return; }
    // text files open in Notepad
    char path[192];
    ramfs_path(n, path, sizeof(path));
    app_open_path(APP_NOTES, path);
}

static void files_do_dialog_ok(FileApp *f) {
    const char *name = tbox_text(&f->dlg);
    if (name[0] != '\0') {
        bool ok = false;
        if (f->dialog == FDLG_NEWFOLDER) ok = (ramfs_mkdir(f->cur, name) != 0);
        else if (f->dialog == FDLG_NEWFILE) ok = (ramfs_create(f->cur, name, "") != 0);
        else if (f->dialog == FDLG_RENAME && f->sel >= 0) ok = ramfs_rename(f->items[f->sel], name);
        if (!ok) { const char *m = "Failed (name exists or full)"; int k = 0; for (; m[k] != '\0'; k++) f->status[k] = m[k]; f->status[k] = '\0'; }
    }
    f->dialog = FDLG_NONE;
    files_refresh(f);
}

static void files_menu_cb(void *ud, int id) {
    FileApp *f = (FileApp *)ud;
    if (id == 1) {
        f->dialog = FDLG_NEWFOLDER;
        tbox_init(&f->dlg, 0, 0, 180, 26, false);
        tbox_set_text(&f->dlg, "New folder");
    } else if (id == 2) {
        f->dialog = FDLG_NEWFILE;
        tbox_init(&f->dlg, 0, 0, 180, 26, false);
        tbox_set_text(&f->dlg, "new.txt");
    } else if (id == 3) {
        if (f->sel >= 0) {
            f->dialog = FDLG_RENAME;
            tbox_init(&f->dlg, 0, 0, 180, 26, false);
            tbox_set_text(&f->dlg, f->items[f->sel]->name);
        }
    } else if (id == 4) {
        if (f->sel >= 0) ramfs_trash_node(f->items[f->sel]);
        files_refresh(f);
    } else if (id == 5) {
        if (f->sel >= 0) files_set_status_sel(f);
    }
}

static void files_sidebar(FileApp *f, Rect r) {
    static const char *labels[5] = { "Home", "Desktop", "Documents", "Downloads", "Trash" };
    static const int icons[5] = { IC_PC, IC_GRID, IC_NOTES, IC_FILES, IC_TRASH };
    static const char *paths[5] = { "/", "/Desktop", "/Documents", "/Downloads", "" };
    for (int i = 0; i < 5; i++) {
        int y = r.y + 8 + i * 30;
        bool hov = mouse_in(r.x + 4, y, 120, 26);
        bool act = (strcmp(f->cur->name, labels[i]) == 0) || (i == FS_TRASH && f->cur == ramfs_trash());
        if (act) fill_round_rect(r.x + 4, y, 120, 26, 4, THEME->select_bg);
        else if (hov) fill_round_rect(r.x + 4, y, 120, 26, 4, THEME->start_bg_active);
        icon_draw(icons[i], r.x + 12, y + 5, 16, THEME->accent);
        draw_text(r.x + 34, y + 9, labels[i], THEME->text, 1);
        (void)paths;
    }
    // remember hit rows for the event handler
}

static int files_sidebar_hit(Rect r, int mx, int my) {
    for (int i = 0; i < 5; i++) {
        if (mouse_in(mx - r.x, my - r.y, 1, 1)) { /* fallthrough */ }
        if (mx >= r.x + 4 && mx < r.x + 124 && my >= r.y + 8 + i * 30 && my < r.y + 34 + i * 30) return i;
    }
    return -1;
}

void *files_create() {
    FileApp *f = new FileApp();
    f->cur = ramfs_root();
    f->nhist = 0;
    f->icons_view = false;
    f->sel = -1;
    f->nitems = 0;
    f->status[0] = '\0';
    f->dialog = FDLG_NONE;
    listv_init(&f->list, 0, 0, 300, 200, 22);
    files_refresh(f);
    return f;
}

void files_destroy(void *inst) {
    FileApp *f = (FileApp *)inst;
    delete f;
}

void files_draw(void *inst, Rect r) {
    FileApp *f = (FileApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);

    // left sidebar
    Rect side = { r.x, r.y, 132, r.h };
    files_sidebar(f, side);

    // toolbar: back, up, address bar, view toggle
    int tx = side.x + side.w + 6;
    int ty = r.y + 6;
    fill_round_rect(tx, ty, 24, 24, 4, mouse_in(tx, ty, 24, 24) ? THEME->start_bg_active : THEME->button_bg);
    icon_draw(IC_BACK, tx + 4, ty + 4, 16, THEME->button_fg);
    tx += 28;
    fill_round_rect(tx, ty, 24, 24, 4, mouse_in(tx, ty, 24, 24) ? THEME->start_bg_active : THEME->button_bg);
    icon_draw(IC_UPDIR, tx + 4, ty + 4, 16, THEME->button_fg);
    tx += 28;
    char path[192];
    ramfs_path(f->cur, path, sizeof(path));
    fill_round_rect(tx, ty, r.x + r.w - 30 - tx, 24, 4, THEME->input_bg);
    draw_text(tx + 6, ty + 8, path, THEME->text, 1);
    tx = r.x + r.w - 28;
    fill_round_rect(tx, ty, 24, 24, 4, mouse_in(tx, ty, 24, 24) ? THEME->start_bg_active : THEME->button_bg);
    icon_draw(f->icons_view ? IC_NOTES : IC_GRID, tx + 4, ty + 4, 16, THEME->button_fg);

    // content area
    Rect area = { side.x + side.w + 6, r.y + 36, r.w - side.w - 12, r.h - 36 - 22 };
    fill_round_rect(area.x, area.y, area.w, area.h, 4, THEME->input_bg);
    f->list.x = area.x + 2;
    f->list.y = area.y + 2;
    f->list.w = area.w - 4;
    f->list.h = area.h - 4;

    if (f->icons_view) {
        int cols = imax(1, f->list.w / 96);
        int shown = 0;
        for (int i = 0; i < f->nitems; i++) {
            int ix = f->list.x + (shown % cols) * 96;
            int iy = f->list.y + (shown / cols) * 84;
            bool sel = (i == f->sel);
            bool hov = mouse_in(ix, iy, 92, 80);
            if (sel) fill_round_rect(ix, iy, 92, 80, 4, THEME->select_bg);
            else if (hov) fill_round_rect(ix, iy, 92, 80, 4, THEME->start_bg_active);
            icon_draw(ramfs_is_dir(f->items[i]) ? IC_FILES : IC_NOTES, ix + 34, iy + 12, 24, THEME->accent);
            draw_text(ix + (92 - text_width(f->items[i]->name, 1)) / 2, iy + 50, f->items[i]->name, THEME->text, 1);
            shown++;
            if (iy + 84 > f->list.y + f->list.h) break;
        }
    } else {
        // list view rows (labels built in-place)
        const char *labels[64];
        char names[64][48];
        for (int i = 0; i < f->nitems; i++) {
            int k = 0;
            const char *src = f->items[i]->name;
            for (; src[k] != '\0' && k < 46; k++) names[i][k] = src[k];
            if (ramfs_is_dir(f->items[i])) names[i][k++] = '/';
            names[i][k] = '\0';
            labels[i] = names[i];
        }
        // clip the list drawing to the area
        clip_set(area);
        listv_draw(&f->list, labels);
        clip_reset();
    }

    // dialog overlay (new folder / new file / rename)
    if (f->dialog != FDLG_NONE) {
        int dw = 220, dh = 90;
        int dx = r.x + (r.w - dw) / 2, dy = r.y + (r.h - dh) / 2;
        fill_round_rect(dx, dy, dw, dh, 6, THEME->menu_border);
        fill_round_rect(dx + 1, dy + 1, dw - 2, dh - 2, 6, THEME->menu_bg);
        draw_text(dx + 10, dy + 10, f->dialog == FDLG_NEWFOLDER ? "New folder name:"
                  : f->dialog == FDLG_NEWFILE ? "New file name:" : "New name:", THEME->text, 1);
        f->dlg.x = dx + 10;
        f->dlg.y = dy + 30;
        f->dlg.w = dw - 20;
        f->dlg.h = 26;
        tbox_draw(&f->dlg);
        draw_text(dx + 10, dy + dh - 24, "Enter = OK   Esc = Cancel", THEME->text_dim, 1);
    }

    // status bar
    fill_rect(r.x, r.y + r.h - 20, r.w, 20, THEME->icon_bg);
    draw_text(r.x + 8, r.y + r.h - 16, f->status, THEME->text_dim, 1);
}

void files_event(void *inst, const kian_event_t *e, Rect r) {
    FileApp *f = (FileApp *)inst;
    // dialog first
    if (f->dialog != FDLG_NONE) {
        if (e->type == KIAN_EV_KEY_DOWN) {
            if (e->a == KIAN_KEY_ESC || e->b == 27) { f->dialog = FDLG_NONE; return; }
            if (tbox_key(&f->dlg, e)) files_do_dialog_ok(f);
            return;
        }
        tbox_mouse(&f->dlg, e);
        return;
    }

    // toolbar hits
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        int tx = r.x + 132 + 6;
        if (mouse_in(tx, r.y + 6, 24, 24)) { files_back(f); return; }
        tx += 28;
        if (mouse_in(tx, r.y + 6, 24, 24)) { files_up(f); return; }
        if (mouse_in(r.x + r.w - 28, r.y + 6, 24, 24)) { f->icons_view = !f->icons_view; return; }
        // sidebar
        Rect side = { r.x, r.y, 132, r.h };
        int sb = files_sidebar_hit(side, IN->mx, IN->my);
        if (sb >= 0) {
            if (sb == FS_TRASH) files_nav(f, ramfs_trash());
            else {
                static const char *paths[4] = { "/", "/Desktop", "/Documents", "/Downloads" };
                RNode *d = ramfs_resolve(paths[sb]);
                files_nav(f, d ? d : ramfs_root());
            }
            return;
        }
    }

    // content area: selection, open, wheel, right-click menu
    Rect area = { r.x + 132 + 6, r.y + 36, r.w - 132 - 12, r.h - 36 - 22 };
    if (e->type == KIAN_EV_MOUSE_WHEEL && rect_hit(IN->mx, IN->my, area)) {
        int dir = (e->a > 0) ? -3 : 3;
        int maxoff = imax(0, f->nitems - f->list.h / f->list.row_h);
        f->list.offset = iclamp(f->list.offset + dir, 0, maxoff);
        return;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l && rect_hit(IN->mx, IN->my, area)) {
        // pick item under mouse
        if (f->icons_view) {
            int cols = imax(1, f->list.w / 96);
            int shown = 0;
            for (int i = 0; i < f->nitems; i++) {
                int ix = f->list.x + (shown % cols) * 96;
                int iy = f->list.y + (shown / cols) * 84;
                if (rect_hit(IN->mx, IN->my, Rect{ ix, iy, 92, 80 })) {
                    f->sel = i;
                    if (IN->dbl_click) files_open_item(f, i);
                    files_set_status_sel(f);
                    return;
                }
                shown++;
            }
            f->sel = -1;
        } else {
            int row = (IN->my - f->list.y) / f->list.row_h;
            if (row >= 0) {
                int idx = row + f->list.offset;
                if (idx >= 0 && idx < f->nitems) {
                    f->sel = idx;
                    if (IN->dbl_click) files_open_item(f, idx);
                    files_set_status_sel(f);
                    return;
                }
            }
            f->sel = -1;
        }
        return;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_r && rect_hit(IN->mx, IN->my, area)) {
        static const MenuItem items[5] = {
            { "New folder", 1 }, { "New text file", 2 }, { "Rename", 3 }, { "Delete", 4 }, { "Properties", 5 }
        };
        menu_open(&SH->menu, items, 5, files_menu_cb, f, IN->mx, IN->my);
        return;
    }
}
