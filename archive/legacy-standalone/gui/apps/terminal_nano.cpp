// Kian OS GUI - nano: a GNU-nano-style full-screen editor inside Kian
// Terminal. Real editing over the RAM filesystem (bounded: 128 lines,
// 120 chars per line). ^O writes, ^X exits (asks on modified buffers),
// ^W searches, ^K/^U cut/paste a line. It does NOT fake a clipboard
// across applications (none exists) - cut/paste is internal.
#include "terminal.h"
#include "../ramfs.h"
#include "../theme.h"
#include "../util.h"

const int NANO_MAX_LINES = 128;
const int NANO_MAX_COLS = 120;

struct NanoState {
    char l[NANO_MAX_LINES][NANO_MAX_COLS + 1];
    int nlines;
    int cur, curcol;
    int off;                    // first visible line
    char filename[128];
    bool dirty;
    char cutbuf[NANO_MAX_COLS + 1];
    bool has_cut;
    // search / confirm prompts
    int prompt_kind;            // 0 none, 1 search, 2 confirm save
    char prompt[64];
    int prompt_len;
    char search[64];
    char status[80];
};

void *nano_open(const char *ramfs_path, int *err) {
    NanoState *n = new NanoState();
    for (int i = 0; i < NANO_MAX_LINES; i++) n->l[i][0] = '\0';
    n->nlines = 1;
    n->cur = 0;
    n->curcol = 0;
    n->off = 0;
    n->dirty = false;
    n->has_cut = false;
    n->cutbuf[0] = '\0';
    n->prompt_kind = 0;
    n->prompt[0] = '\0';
    n->prompt_len = 0;
    n->search[0] = '\0';
    int k = 0;
    for (; ramfs_path[k] != '\0' && k < 126; k++) n->filename[k] = ramfs_path[k];
    n->filename[k] = '\0';
    *err = 0;
    RNode *f = ramfs_resolve(n->filename);
    if (f && !ramfs_is_dir(f)) {
        // load content
        const char *s = ramfs_read(f);
        int ln = 0, col = 0;
        for (int i = 0; ; i++) {
            char c = s[i];
            if (c == '\n' || c == '\0') {
                n->l[ln][col] = '\0';
                ln++;
                col = 0;
                if (c == '\0') break;
                if (ln >= NANO_MAX_LINES) break;
            } else if (col < NANO_MAX_COLS) {
                n->l[ln][col++] = c;
            }
        }
        if (ln >= NANO_MAX_LINES) ln = NANO_MAX_LINES - 1;
        n->nlines = (ln > 0) ? ln : 1;
    } else if (f && ramfs_is_dir(f)) {
        *err = 1;
        delete n;
        return 0;
    }
    n->status[0] = '\0';
    int s2 = 0;
    const char *msg = "[ New file ]";
    for (; msg[s2] != '\0' && s2 < 78; s2++) n->status[s2] = msg[s2];
    n->status[s2] = '\0';
    return n;
}

void nano_close(void *h) {
    NanoState *n = (NanoState *)h;
    if (n) delete n;
}

const char *nano_status(void *h) {
    NanoState *n = (NanoState *)h;
    return n ? n->status : "";
}

static void nano_set_status(NanoState *n, const char *msg) {
    int k = 0;
    for (; msg[k] != '\0' && k < 78; k++) n->status[k] = msg[k];
    n->status[k] = '\0';
}

static bool nano_save(NanoState *n) {
    // build the file content (bounded heap string)
    Str data;
    for (int i = 0; i < n->nlines; i++) {
        data.append(n->l[i]);
        if (i < n->nlines - 1) data.append('\n');
    }
    RNode *f = ramfs_resolve(n->filename);
    if (!f) {
        // create the file (parent must exist)
        char dir[160], base[64];
        int last = -1;
        for (int i = 0; n->filename[i] != '\0'; i++) if (n->filename[i] == '/') last = i;
        if (last <= 0) { nano_set_status(n, "Write failed: bad path"); return false; }
        int d = 0;
        for (int i = 0; i <= last && d < 158; i++) dir[d++] = n->filename[i];
        dir[d] = '\0';
        int b = 0;
        for (int i = last + 1; n->filename[i] != '\0' && b < 62; i++) base[b++] = n->filename[i];
        base[b] = '\0';
        RNode *pd = ramfs_resolve(dir);
        if (!pd || !ramfs_is_dir(pd)) { nano_set_status(n, "Write failed: parent missing"); return false; }
        f = ramfs_create(pd, base, "");
        if (!f) { nano_set_status(n, "Write failed: filesystem full"); return false; }
    }
    if (!ramfs_write(f, data.c_str(), data.length())) {
        nano_set_status(n, "Write failed");
        return false;
    }
    n->dirty = false;
    nano_set_status(n, "Written to RAM fs (lost on reboot)");
    return true;
}

static void nano_line_insert_char(NanoState *n, char c) {
    char *ln = n->l[n->cur];
    int len = (int)strlen(ln);
    if (len >= NANO_MAX_COLS) { nano_set_status(n, "Line too long (120 max)"); return; }
    for (int j = len; j > n->curcol; j--) ln[j] = ln[j - 1];
    ln[n->curcol++] = c;
    ln[len + 1] = '\0';
    n->dirty = true;
}

static void nano_split_line(NanoState *n) {
    if (n->nlines >= NANO_MAX_LINES) { nano_set_status(n, "Document full (128 lines)"); return; }
    for (int j = n->nlines; j > n->cur + 1; j--) {
        int k = 0;
        for (; n->l[j - 1][k] != '\0'; k++) n->l[j][k] = n->l[j - 1][k];
        n->l[j][k] = '\0';
    }
    // tail moves down
    char tail[NANO_MAX_COLS + 1];
    int t = 0;
    char *ln = n->l[n->cur];
    int len = (int)strlen(ln);
    for (int j = n->curcol; j < len; j++) tail[t++] = ln[j];
    tail[t] = '\0';
    ln[n->curcol] = '\0';
    int k = 0;
    for (; tail[k] != '\0'; k++) n->l[n->cur + 1][k] = tail[k];
    n->l[n->cur + 1][k] = '\0';
    n->nlines++;
    n->cur++;
    n->curcol = 0;
    n->dirty = true;
}

static void nano_join_next(NanoState *n) {
    if (n->cur + 1 >= n->nlines) return;
    char *ln = n->l[n->cur];
    int len = (int)strlen(ln);
    char *nx = n->l[n->cur + 1];
    int nlen = (int)strlen(nx);
    if (len + nlen > NANO_MAX_COLS) { nano_set_status(n, "Lines too long to join"); return; }
    for (int j = 0; j < nlen; j++) ln[len + j] = nx[j];
    ln[len + nlen] = '\0';
    for (int j = n->cur + 1; j < n->nlines - 1; j++) {
        int k = 0;
        for (; n->l[j + 1][k] != '\0'; k++) n->l[j][k] = n->l[j + 1][k];
        n->l[j][k] = '\0';
    }
    n->l[n->nlines - 1][0] = '\0';
    n->nlines--;
    n->dirty = true;
}

static void nano_search_next(NanoState *n) {
    if (n->search[0] == '\0') { nano_set_status(n, "No search text"); return; }
    for (int step = 1; step <= n->nlines; step++) {
        int row = (n->cur + step) % n->nlines;
        if (row == n->cur && step > n->nlines) break;
        const char *ln = n->l[row];
        for (int k = 0; ln[k] != '\0'; k++) {
            int m = 0;
            while (n->search[m] != '\0' && ln[k + m] == n->search[m]) m++;
            if (n->search[m] == '\0') {
                if (row == n->cur && k <= n->curcol && step == 1) continue; // skip own hit once
                n->cur = row;
                n->curcol = k;
                char msg[80];
                int q = 0;
                const char *p1 = "Found on line ";
                for (; p1[q] != '\0' && q < 60; q++) msg[q] = p1[q];
                put_num(msg, q, row + 1);
                msg[q] = '\0';
                nano_set_status(n, msg);
                return;
            }
        }
    }
    nano_set_status(n, "Not found");
}

int nano_key(void *h, const kian_event_t *e) {
    NanoState *n = (NanoState *)h;
    if (!n || e->type != KIAN_EV_KEY_DOWN) return 0;
    int a = e->a;
    char ch = (char)e->b;
    bool ctrl = (e->c & 2) != 0;

    // ---- active prompt (search / confirm) ----
    if (n->prompt_kind == 2) {
        // confirm save on exit
        if (ch == 'y' || ch == 'Y') {
            nano_save(n);
            return 1;   // exit after saving
        }
        if (ch == 'n' || ch == 'N') return 1;   // abandon
        if (a == KIAN_KEY_ESC || ch == 27) {
            n->prompt_kind = 0;
            nano_set_status(n, "Cancelled");
        }
        return 0;
    }
    if (n->prompt_kind == 1) {
        if (a == KIAN_KEY_ESC || ch == 27) { n->prompt_kind = 0; nano_set_status(n, "Search cancelled"); return 0; }
        if (a == KIAN_KEY_ENTER || ch == '\n') {
            n->prompt_kind = 0;
            int k = 0;
            for (; n->prompt[k] != '\0' && k < 62; k++) n->search[k] = n->prompt[k];
            n->search[k] = '\0';
            nano_search_next(n);
            return 0;
        }
        if (a == KIAN_KEY_BACKSPACE || ch == 8) { if (n->prompt_len > 0) n->prompt[--n->prompt_len] = '\0'; return 0; }
        if (!ctrl && ch >= 32 && ch < 127 && n->prompt_len < 62) { n->prompt[n->prompt_len++] = ch; n->prompt[n->prompt_len] = '\0'; }
        return 0;
    }

    // ---- shortcuts ----
    if (ctrl) {
        if (ch == 'o' || ch == 'O') { nano_save(n); return 0; }
        if (ch == 'x' || ch == 'X') {
            if (n->dirty) {
                n->prompt_kind = 2;
                nano_set_status(n, "")
                ;
                return 0;   // wait for Y/N in the prompt handler
            }
            return 1;       // unmodified: exit at once
        }
        if (ch == 'w' || ch == 'W') {
            n->prompt_kind = 1;
            n->prompt[0] = '\0';
            n->prompt_len = 0;
            nano_set_status(n, "");
            return 0;
        }
        if (ch == 'k' || ch == 'K') {
            // cut current line
            int k = 0;
            for (; n->l[n->cur][k] != '\0' && k < NANO_MAX_COLS; k++) n->cutbuf[k] = n->l[n->cur][k];
            n->cutbuf[k] = '\0';
            n->has_cut = true;
            if (n->nlines > 1) {
                for (int j = n->cur; j < n->nlines - 1; j++) {
                    int q = 0;
                    for (; n->l[j + 1][q] != '\0'; q++) n->l[j][q] = n->l[j + 1][q];
                    n->l[j][q] = '\0';
                }
                n->nlines--;
                if (n->cur >= n->nlines) n->cur = n->nlines - 1;
            } else {
                n->l[0][0] = '\0';
                n->curcol = 0;
            }
            n->dirty = true;
            nano_set_status(n, "Line cut (^U pastes)");
            return 0;
        }
        if (ch == 'u' || ch == 'U') {
            if (n->has_cut && n->nlines < NANO_MAX_LINES) {
                for (int j = n->nlines; j > n->cur + 1; j--) {
                    int q = 0;
                    for (; n->l[j - 1][q] != '\0'; q++) n->l[j][q] = n->l[j - 1][q];
                    n->l[j][q] = '\0';
                }
                int k = 0;
                for (; n->cutbuf[k] != '\0'; k++) n->l[n->cur + 1][k] = n->cutbuf[k];
                n->l[n->cur + 1][k] = '\0';
                n->nlines++;
                n->cur++;
                n->curcol = 0;
                n->dirty = true;
                nano_set_status(n, "Pasted");
            }
            return 0;
        }
        if (ch == 'c' || ch == 'C') {
            char msg[80];
            int q = 0;
            const char *p1 = "line ";
            for (; p1[q] != '\0' && q < 60; q++) msg[q] = p1[q];
            put_num(msg, q, n->cur + 1);
            msg[q++] = ' ';
            const char *p2 = "col ";
            for (; p2[0] != '\0' && q < 66; ) { msg[q++] = p2[0]; p2++; }
            put_num(msg, q, n->curcol + 1);
            msg[q] = '\0';
            nano_set_status(n, msg);
            return 0;
        }
        if (ch == 'l' || ch == 'L') return 0;   // refresh (redraw happens anyway)
        return 0;
    }

    // ---- normal editing ----
    if (ch == '\n') { nano_split_line(n); return 0; }
    if (a == KIAN_KEY_BACKSPACE || ch == 8) {
        if (n->curcol > 0) {
            char *ln = n->l[n->cur];
            for (int j = n->curcol - 1; ln[j] != '\0'; j++) ln[j] = ln[j + 1];
            n->curcol--;
            n->dirty = true;
        } else if (n->cur > 0) {
            // join with previous line
            int prevlen = (int)strlen(n->l[n->cur - 1]);
            if (prevlen + (int)strlen(n->l[n->cur]) <= NANO_MAX_COLS) {
                char *pv = n->l[n->cur - 1];
                char *cu = n->l[n->cur];
                int q = prevlen;
                for (int j = 0; cu[j] != '\0'; j++) pv[q++] = cu[j];
                pv[q] = '\0';
                for (int j = n->cur; j < n->nlines - 1; j++) {
                    int k2 = 0;
                    for (; n->l[j + 1][k2] != '\0'; k2++) n->l[j][k2] = n->l[j + 1][k2];
                    n->l[j][k2] = '\0';
                }
                n->nlines--;
                n->cur--;
                n->curcol = prevlen;
                n->dirty = true;
            }
        }
        return 0;
    }
    if (a == KIAN_KEY_DEL) {
        char *ln = n->l[n->cur];
        if (ln[n->curcol] != '\0') {
            for (int j = n->curcol; ln[j] != '\0'; j++) ln[j] = ln[j + 1];
            n->dirty = true;
        } else {
            nano_join_next(n);
        }
        return 0;
    }
    if (a == KIAN_KEY_LEFT)  { if (n->curcol > 0) n->curcol--; return 0; }
    if (a == KIAN_KEY_RIGHT) {
        if (n->l[n->cur][n->curcol] != '\0') n->curcol++;
        else if (n->cur + 1 < n->nlines) { n->cur++; n->curcol = 0; }
        return 0;
    }
    if (a == KIAN_KEY_UP) {
        if (n->cur > 0) {
            n->cur--;
            if (n->curcol > (int)strlen(n->l[n->cur])) n->curcol = (int)strlen(n->l[n->cur]);
        }
        return 0;
    }
    if (a == KIAN_KEY_DOWN) {
        if (n->cur + 1 < n->nlines) {
            n->cur++;
            if (n->curcol > (int)strlen(n->l[n->cur])) n->curcol = (int)strlen(n->l[n->cur]);
        }
        return 0;
    }
    if (a == KIAN_KEY_HOME) { n->curcol = 0; return 0; }
    if (a == KIAN_KEY_END)  { n->curcol = (int)strlen(n->l[n->cur]); return 0; }
    if (a == KIAN_KEY_PGUP) { n->cur = (n->cur > 10) ? n->cur - 10 : 0; if (n->curcol > (int)strlen(n->l[n->cur])) n->curcol = (int)strlen(n->l[n->cur]); return 0; }
    if (a == KIAN_KEY_PGDN) { n->cur = (n->cur + 10 < n->nlines) ? n->cur + 10 : n->nlines - 1; if (n->curcol > (int)strlen(n->l[n->cur])) n->curcol = (int)strlen(n->l[n->cur]); return 0; }
    if (ch == '\t') {
        for (int i = 0; i < 4; i++) nano_line_insert_char(n, ' ');
        return 0;
    }
    if (ch >= 32 && ch < 127) { nano_line_insert_char(n, ch); return 0; }
    return 0;
}

void nano_draw(void *h, Rect r) {
    NanoState *n = (NanoState *)h;
    if (!n) return;
    fill_rect(r.x, r.y, r.w, r.h, term_color(TC_BG));
    uint32_t fg = term_color(TC_FG);
    uint32_t dim = term_color(TC_DIM);
    uint32_t accent = term_color(TC_ACCENT);
    int rowh = text_line_height_mono(1);
    int foot_h = 56;   // 3 status/footer rows
    int rows = (r.h - foot_h) / rowh;
    if (rows < 1) return;
    // keep the cursor visible
    if (n->cur < n->off) n->off = n->cur;
    if (n->cur >= n->off + rows) n->off = n->cur - rows + 1;
    int numw = 4 * 8;   // "NNN " gutter
    for (int i = 0; i < rows; i++) {
        int row = n->off + i;
        int y = r.y + i * rowh;
        if (row >= n->nlines) break;
        char num[8];
        int k = 0;
        num[0] = '\0';
        put_num(num, k, row + 1);
        char gutter[8];
        int g = 0;
        for (int pad = k; pad < 3; pad++) gutter[g++] = ' ';
        for (int q = 0; num[q] != '\0'; q++) gutter[g++] = num[q];
        gutter[g++] = ' ';
        gutter[g] = '\0';
        draw_text_mono(r.x + 4, y, gutter, dim, 1);
        draw_text_mono(r.x + numw, y, n->l[row], fg, 1);
    }
    // caret
    if (((now_ms() / 400) & 1) == 0 && n->cur >= n->off && n->cur < n->off + rows) {
        int cy = r.y + (n->cur - n->off) * rowh;
        int cx = r.x + numw + n->curcol * glyph_advance_mono(1);
        vline(cx + 1, mono_caret_y(cy), mono_caret_h(), accent);
    }
    // status bar: title / message / shortcuts (or the active prompt)
    int sy = r.y + r.h - foot_h;
    hline(r.x, sy, r.w, THEME->menu_border);
    char head[160];
    int k = 0;
    head[0] = '\0';
    const char *pfx = "nano: ";
    for (; pfx[k] != '\0' && k < 150; k++) head[k] = pfx[k];
    for (int q = 0; n->filename[q] != '\0' && k < 150; q++) head[k++] = n->filename[q];
    if (n->dirty) { const char *m = "  [ Modified ]"; for (int q = 0; m[q] != '\0' && k < 150; q++) head[k++] = m[q]; }
    head[k] = '\0';
    draw_text_mono(r.x + 4, sy + 4, head, accent, 1);
    draw_text_mono(r.x + 4, sy + 4 + 16, n->status, dim, 1);
    if (n->prompt_kind == 2) {
        draw_text_mono(r.x + 4, sy + 4 + 32, "Save modified buffer? (Y)es / (N)o / ESC cancels", term_color(TC_WARN), 1);
    } else if (n->prompt_kind == 1) {
        char sp[160];
        int q = 0;
        const char *p1 = "Search: ";
        for (; p1[q] != '\0' && q < 150; q++) sp[q] = p1[q];
        for (int z = 0; n->prompt[z] != '\0' && q < 150; z++) sp[q++] = n->prompt[z];
        sp[q] = '\0';
        draw_text_mono(r.x + 4, sy + 4 + 32, sp, accent, 1);
    } else {
        draw_text_mono(r.x + 4, sy + 4 + 32, "^O WriteOut  ^W Search  ^K Cut  ^U Paste  ^C Pos  ^X Exit", dim, 1);
    }
}
