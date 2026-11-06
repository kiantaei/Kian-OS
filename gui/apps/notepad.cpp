// Kian OS GUI - Notepad: multi-line editor with caret, arrows, Home/End,
// Del/Backspace/Enter, mouse caret placement, scrollbar, status bar, and a
// File menu (New / Open / Save / Save As) backed by the RAM filesystem.
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"

enum { NDLG_NONE = 0, NDLG_OPEN, NDLG_SAVEAS };

static const int NOTE_COLS_MAX = 110;   // max rendered chars per editor line

struct NoteApp {
    Str text;          // content with '\n'
    int caret;
    char path[96];
    int dialog;
    TBox dlg;
    int scroll;        // first visible line
    bool dirty;
};

static void notes_line_col(NoteApp *n, int *line, int *col) {
    int l = 0, last_nl = -1;
    for (int i = 0; i < n->caret && i < n->text.length(); i++) {
        if (n->text[i] == '\n') { l++; last_nl = i; }
    }
    *line = l;
    *col = n->caret - last_nl - 1;
}

static int notes_count_lines(NoteApp *n) {
    int l = 1;
    for (int i = 0; i < n->text.length(); i++) {
        if (n->text[i] == '\n') l++;
    }
    return l;
}

static void notes_line_bounds(NoteApp *n, int line, int *s, int *e) {
    int l = 0, start = 0;
    int i = 0;
    for (; i < n->text.length(); i++) {
        if (n->text[i] == '\n') {
            if (l == line) break;
            l++;
            start = i + 1;
        }
    }
    *s = start;
    *e = (i >= n->text.length()) ? n->text.length() : i;
}

static void notes_move_v(NoteApp *n, int delta) {
    int line, col;
    notes_line_col(n, &line, &col);
    int target = iclamp(line + delta, 0, notes_count_lines(n) - 1);
    int s, e;
    notes_line_bounds(n, target, &s, &e);
    n->caret = iclamp(s + col, s, e);
}

static void notes_insert(NoteApp *n, char c) {
    n->text.insert(n->caret, c);
    n->caret++;
    n->dirty = true;
}

static void notes_backspace(NoteApp *n) {
    if (n->caret > 0) {
        n->text.remove_at(n->caret - 1);
        n->caret--;
        n->dirty = true;
    }
}

static void notes_del(NoteApp *n) {
    if (n->caret < n->text.length()) {
        n->text.remove_at(n->caret);
        n->dirty = true;
    }
}

static void notes_load(NoteApp *n, const char *path) {
    RNode *f = ramfs_resolve(path);
    if (!f || ramfs_is_dir(f)) return;
    n->text.set(ramfs_read(f));
    n->caret = 0;
    n->scroll = 0;
    int i = 0;
    for (; path[i] != '\0' && i < 94; i++) n->path[i] = path[i];
    n->path[i] = '\0';
    n->dirty = false;
}

static void notes_save(NoteApp *n) {
    if (n->path[0] != '/') return;
    RNode *f = ramfs_resolve(n->path);
    if (!f) {
        // create under the parent dir
        char parent[96];
        int last = -1;
        for (int i = 0; n->path[i] != '\0'; i++) if (n->path[i] == '/') last = i;
        if (last <= 0) return;
        int k = 0;
        for (int i = 0; i <= last && k < 95; i++) parent[k++] = n->path[i];
        parent[k] = '\0';
        RNode *pd = ramfs_resolve(parent);
        if (!pd || !ramfs_is_dir(pd)) return;
        f = ramfs_create(pd, n->path + last + 1, "");
    }
    if (f && !ramfs_is_dir(f)) {
        ramfs_write(f, n->text.c_str(), n->text.length());
        n->dirty = false;
    }
}

static void notes_menu_cb(void *ud, int id) {
    NoteApp *n = (NoteApp *)ud;
    if (id == 1) { // New
        n->text.clear();
        n->caret = 0;
        n->path[0] = '\0';
        n->scroll = 0;
    } else if (id == 2) { // Open
        n->dialog = NDLG_OPEN;
        tbox_init(&n->dlg, 0, 0, 200, 26, false);
        tbox_set_text(&n->dlg, "/Documents/readme.txt");
    } else if (id == 3) { // Save
        if (n->path[0] == '/') notes_save(n);
        else { n->dialog = NDLG_SAVEAS; tbox_init(&n->dlg, 0, 0, 200, 26, false); tbox_set_text(&n->dlg, "/Documents/untitled.txt"); }
    } else if (id == 4) { // Save As
        n->dialog = NDLG_SAVEAS;
        tbox_init(&n->dlg, 0, 0, 200, 26, false);
        tbox_set_text(&n->dlg, n->path[0] == '/' ? n->path : "/Documents/untitled.txt");
    }
}

void *notes_create() {
    NoteApp *n = new NoteApp();
    n->caret = 0;
    n->path[0] = '\0';
    n->dialog = NDLG_NONE;
    n->scroll = 0;
    n->dirty = false;
    return n;
}

void *notes_create_path(const char *ramfs_path_) {
    NoteApp *n = (NoteApp *)notes_create();
    notes_load(n, ramfs_path_);
    return n;
}

void notes_destroy(void *inst) {
    NoteApp *n = (NoteApp *)inst;
    delete n;
}

void notes_draw(void *inst, Rect r) {
    NoteApp *n = (NoteApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->input_bg);

    // toolbar: File menu items as small buttons
    static const char *tb[4] = { "New", "Open", "Save", "Save As" };
    int tx = r.x + 6;
    for (int i = 0; i < 4; i++) {
        int bw = text_width(tb[i], 1) + 16;
        bool hov = mouse_in(tx, r.y + 4, bw, 26);
        fill_round_rect(tx, r.y + 4, bw, 26, 4, hov ? THEME->start_bg_active : THEME->button_bg);
        draw_text(tx + 8, text_center_y(r.y + 4, 26, 1), tb[i], THEME->button_fg, 1);
        tx += bw + 4;
    }
    if (n->dirty) draw_text(r.x + r.w - 10, r.y + 10, "*", THEME->accent, 1);

    // text area
    Rect area = { r.x + 4, r.y + 36, r.w - 16, r.h - 36 - 22 };
    fill_round_rect(area.x, area.y, area.w, area.h, 4, THEME->win_bg);
    int rowh = text_line_height_mono(1);          // 16
    int lines_vis = imax(1, (area.h - 8) / rowh);
    int total = notes_count_lines(n);
    n->scroll = iclamp(n->scroll, 0, imax(0, total - lines_vis));

    // draw visible lines + caret (mono editor like a real text editor)
    char buf[NOTE_COLS_MAX + 2];
    int line = 0, start = 0, drawn = 0;
    int i = 0;
    for (i = 0; i <= n->text.length() && drawn < lines_vis; i++) {
        bool nl = (i == n->text.length()) || (n->text[i] == '\n');
        if (nl) {
            if (line >= n->scroll) {
                int len = i - start;
                if (len > NOTE_COLS_MAX) len = NOTE_COLS_MAX;
                for (int k = 0; k < len; k++) buf[k] = n->text[start + k];
                buf[len] = '\0';
                draw_text_mono(area.x + 4, area.y + 4 + drawn * rowh, buf, THEME->text, 1);
                drawn++;
            }
            line++;
            start = i + 1;
        }
    }
    // caret
    int cl, cc;
    notes_line_col(n, &cl, &cc);
    if (cl >= n->scroll && cl < n->scroll + lines_vis) {
        int cy2 = area.y + 4 + (cl - n->scroll) * rowh;
        int cx2 = area.x + 4 + cc * glyph_advance_mono(1);
        if (((now_ms() / 400) & 1) == 0) vline(cx2 + 1, mono_caret_y(cy2), mono_caret_h(), THEME->accent);
    }
    // mini scrollbar
    if (total > lines_vis) {
        int bh = imax(10, area.h * lines_vis / total);
        int by = area.y + (area.h - bh) * n->scroll / (total - lines_vis);
        fill_rect(area.x + area.w - 4, area.y, 3, area.h, THEME->icon_bg);
        fill_rect(area.x + area.w - 4, by, 3, bh, THEME->menu_border);
    }

    // dialog overlay
    if (n->dialog != NDLG_NONE) {
        int dw = 260, dh = 90;
        int dx = r.x + (r.w - dw) / 2, dy = r.y + (r.h - dh) / 2;
        fill_round_rect(dx, dy, dw, dh, 6, THEME->menu_border);
        fill_round_rect(dx + 1, dy + 1, dw - 2, dh - 2, 6, THEME->menu_bg);
        draw_text(dx + 10, dy + 10, n->dialog == NDLG_OPEN ? "Open (RAM FS path):" : "Save As (RAM FS path):", THEME->text, 1);
        n->dlg.x = dx + 10;
        n->dlg.y = dy + 30;
        n->dlg.w = dw - 20;
        n->dlg.h = 26;
        tbox_draw(&n->dlg);
        draw_text(dx + 10, dy + dh - 24, "Enter = OK   Esc = Cancel", THEME->text_dim, 1);
    }

    // status bar
    char st[128];
    int k = 0;
    st[0] = '\0';
    const char *p1 = "Ln ";
    for (; *p1 != '\0'; p1++) st[k++] = *p1;
    put_num(st, k, cl + 1);
    st[k++] = ',';
    const char *p2 = " Col ";
    for (; *p2 != '\0'; p2++) st[k++] = *p2;
    put_num(st, k, cc + 1);
    st[k++] = ' ';
    st[k++] = '|';
    st[k++] = ' ';
    const char *p3 = (n->path[0] == '/') ? n->path : "(unsaved - RAM only)";
    for (; *p3 != '\0' && k < 120; p3++) st[k++] = *p3;
    st[k] = '\0';
    fill_rect(r.x, r.y + r.h - 20, r.w, 20, THEME->icon_bg);
    draw_text(r.x + 8, text_center_y(r.y + r.h - 20, 20, 1), st, THEME->text_dim, 1);
}

void notes_event(void *inst, const kian_event_t *e, Rect r) {
    NoteApp *n = (NoteApp *)inst;
    if (n->dialog != NDLG_NONE) {
        if (e->type == KIAN_EV_KEY_DOWN) {
            if (e->a == KIAN_KEY_ESC || e->b == 27) { n->dialog = NDLG_NONE; return; }
            if (tbox_key(&n->dlg, e)) {
                if (n->dialog == NDLG_OPEN) notes_load(n, tbox_text(&n->dlg));
                else {
                    const char *p = tbox_text(&n->dlg);
                    int i = 0;
                    for (; p[i] != '\0' && i < 94; i++) n->path[i] = p[i];
                    n->path[i] = '\0';
                    notes_save(n);
                }
                n->dialog = NDLG_NONE;
            }
            return;
        }
        tbox_mouse(&n->dlg, e);
        return;
    }

    // toolbar clicks
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        static const char *tb[4] = { "New", "Open", "Save", "Save As" };
        int tx = r.x + 6;
        for (int i = 0; i < 4; i++) {
            int bw = text_width(tb[i], 1) + 16;
            if (mouse_in(tx, r.y + 4, bw, 26)) { notes_menu_cb(n, i + 1); return; }
            tx += bw + 4;
        }
        // caret placement by click (mono cell = 8 px)
        Rect area = { r.x + 4, r.y + 36, r.w - 16, r.h - 36 - 22 };
        if (rect_hit(IN->mx, IN->my, area)) {
            int rowh = text_line_height_mono(1);
            int target_line = n->scroll + (IN->my - area.y - 4) / rowh;
            target_line = iclamp(target_line, 0, notes_count_lines(n) - 1);
            int s, e2;
            notes_line_bounds(n, target_line, &s, &e2);
            int col = (IN->mx - area.x - 4) / glyph_advance_mono(1);
            col = iclamp(col, 0, e2 - s);
            n->caret = s + col;
        }
        return;
    }
    if (e->type == KIAN_EV_MOUSE_WHEEL) {
        n->scroll += (e->a > 0) ? -3 : 3;
        return;
    }
    if (e->type != KIAN_EV_KEY_DOWN) return;

    int a = e->a;
    char ch = (char)e->b;
    bool ctrl = (e->c & 2) != 0;   // no clipboard/selection: Ctrl+letters are no-ops
    if (a == KIAN_KEY_LEFT) {
        if (n->caret > 0) n->caret--;
    } else if (a == KIAN_KEY_RIGHT) {
        if (n->caret < n->text.length()) n->caret++;
    } else if (a == KIAN_KEY_UP) {
        notes_move_v(n, -1);
    } else if (a == KIAN_KEY_DOWN) {
        notes_move_v(n, 1);
    } else if (a == KIAN_KEY_HOME) {
        int line, col;
        notes_line_col(n, &line, &col);
        int s, e2;
        notes_line_bounds(n, line, &s, &e2);
        n->caret = s;
    } else if (a == KIAN_KEY_END) {
        int line, col;
        notes_line_col(n, &line, &col);
        int s, e2;
        notes_line_bounds(n, line, &s, &e2);
        n->caret = e2;
    } else if (a == KIAN_KEY_PGUP) {
        notes_move_v(n, -10);
    } else if (a == KIAN_KEY_PGDN) {
        notes_move_v(n, 10);
    } else if (ch == 8) {
        notes_backspace(n);
    } else if (a == KIAN_KEY_DEL) {
        notes_del(n);
    } else if (ch == '\n') {
        notes_insert(n, '\n');
    } else if (ch == '\t' && !ctrl) {
        notes_insert(n, ' ');
        notes_insert(n, ' ');
    } else if (ch >= 32 && ch < 127 && !ctrl) {
        notes_insert(n, ch);
    }
    // keep the caret visible: scroll if it left the viewport
    int cl, cc;
    notes_line_col(n, &cl, &cc);
    int lines_vis = imax(1, (r.h - 58) / text_line_height_mono(1));
    if (cl < n->scroll) n->scroll = cl;
    if (cl >= n->scroll + lines_vis) n->scroll = cl - lines_vis + 1;
}
