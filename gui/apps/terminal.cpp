// Kian OS GUI - Terminal app: scrolling output, caret-based line editing,
// command history (Up/Down), TAB completion (command names + RAM FS paths,
// double-TAB lists the candidates), and commands on the RAM filesystem.
// HONEST: the filesystem is RAM only and is lost on reboot.
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../input.h"
#include "../util.h"

#define TERM_COLS_MAX 128   // buffer cap for one rendered row (8 px per cell)

struct TermApp {
    Str buf;                 // scrollback (capped ~8 KB)
    char line[96];           // input line
    int llen;
    int lcaret;              // caret inside line
    char cwd[128];
    // bounded history (ring of 16)
    char hist[16][96];
    int hist_n;
    int hist_head;           // next write slot
    int hist_view;           // -1 = not browsing
    // tab completion
    unsigned last_tab_ms;
    int last_tab_len;        // llen after the previous TAB
};

static const char *CMD_LIST[] = {
    "help", "clear", "echo", "date", "uptime", "mem", "whoami", "uname",
    "pwd", "ls", "cd", "cat", "mkdir", "touch", "rm", "exit"
};
static const int CMD_N = (int)(sizeof(CMD_LIST) / sizeof(CMD_LIST[0]));

static void term_out(TermApp *t, const char *s) {
    t->buf.append(s);
    // cap scrollback: drop whole lines above ~8 KB
    while (t->buf.length() > 8192) {
        int nl = t->buf.find('\n');
        if (nl < 0) { t->buf.clear(); break; }
        for (int i = 0; i <= nl; i++) t->buf.remove_at(0);
    }
}

static void term_out_num(TermApp *t, long long v) {
    char tmp[24];
    int i = 0;
    unsigned long long m = (v < 0) ? (unsigned long long)(-v) : (unsigned long long)v;
    if (v < 0) tmp[i++] = '-';
    if (m == 0) tmp[i++] = '0';
    while (m > 0 && i < 22) { tmp[i++] = (char)('0' + (m % 10)); m /= 10; }
    while (i > 0) t->buf.append(tmp[--i]);
}

static void term_resolve(TermApp *t, const char *arg, char *out, int maxn) {
    if (arg[0] == '/') {
        int i = 0;
        for (; arg[i] != '\0' && i < maxn - 1; i++) out[i] = arg[i];
        out[i] = '\0';
        return;
    }
    int i = 0;
    for (; t->cwd[i] != '\0' && i < maxn - 2; i++) out[i] = t->cwd[i];
    if (i > 0 && out[i - 1] != '/') out[i++] = '/';
    for (int k = 0; arg[k] != '\0' && i < maxn - 1; k++) out[i++] = arg[k];
    out[i] = '\0';
}

// ---------------- line editing helpers ----------------

static void term_line_insert(TermApp *t, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    for (int k = 0; k < n; k++) {
        if (t->llen >= 94) break;
        for (int j = t->llen; j > t->lcaret; j--) t->line[j] = t->line[j - 1];
        t->line[t->lcaret++] = s[k];
        t->llen++;
        t->line[t->llen] = '\0';
    }
}

static void term_line_remove_at(TermApp *t, int idx) {
    if (idx < 0 || idx >= t->llen) return;
    for (int j = idx; j < t->llen - 1; j++) t->line[j] = t->line[j + 1];
    t->llen--;
    t->line[t->llen] = '\0';
    if (t->lcaret > t->llen) t->lcaret = t->llen;
}

static void term_line_set(TermApp *t, const char *s) {
    int i = 0;
    for (; s[i] != '\0' && i < 94; i++) t->line[i] = s[i];
    t->line[i] = '\0';
    t->llen = i;
    t->lcaret = i;
}

// ---------------- history ----------------

static void term_hist_push(TermApp *t, const char *s) {
    if (s[0] == '\0') return;
    // skip immediate duplicate
    int last = (t->hist_head - 1 + 16) % 16;
    if (t->hist_n > 0 && strcmp(t->hist[last], s) == 0) return;
    int k = 0;
    for (; s[k] != '\0' && k < 94; k++) t->hist[t->hist_head][k] = s[k];
    t->hist[t->hist_head][k] = '\0';
    t->hist_head = (t->hist_head + 1) % 16;
    if (t->hist_n < 16) t->hist_n++;
    t->hist_view = -1;
}

static void term_hist_nav(TermApp *t, int dir) {
    if (t->hist_n == 0) return;
    if (dir < 0) { // older
        if (t->hist_view == -1) t->hist_view = (t->hist_head - 1 + 16) % 16;
        else t->hist_view = (t->hist_view - 1 + 16) % 16;
    } else { // newer
        if (t->hist_view == -1) return;
        int next = (t->hist_view + 1) % 16;
        if (next == t->hist_head) { t->hist_view = -1; term_line_set(t, ""); return; }
        t->hist_view = next;
    }
    if (t->hist_view != -1) term_line_set(t, t->hist[t->hist_view]);
}

// ---------------- TAB completion ----------------

static void term_cand_add(char cand[][64], int *ncand, const char *s) {
    if (*ncand >= 16) return;
    int k = 0;
    for (; s[k] != '\0' && k < 62; k++) cand[*ncand][k] = s[k];
    cand[*ncand][k] = '\0';
    (*ncand)++;
}

static bool term_starts_with(const char *s, const char *prefix) {
    for (int i = 0; prefix[i] != '\0'; i++) {
        if (s[i] != prefix[i]) return false;
    }
    return true;
}

// Completes the word that ends at the caret. First word -> command names,
// later words -> RAM FS paths (dirs get a trailing '/'). Unique match is
// inserted with a trailing space; several matches extend to the common
// prefix, and a second TAB with no progress prints the candidates.
static void term_line_remove_range(TermApp *t, int at, int count);

static void term_tab_complete(TermApp *t) {
    int tok_start = t->lcaret;
    while (tok_start > 0 && t->line[tok_start - 1] != ' ') tok_start--;
    bool first_word = true;
    for (int i = 0; i < tok_start; i++) {
        if (t->line[i] != ' ') { first_word = false; break; }
    }
    int plen = t->lcaret - tok_start;
    char prefix[96];
    int pc = 0;
    for (int i = tok_start; i < t->lcaret && pc < 94; i++) prefix[pc++] = t->line[i];
    prefix[pc] = '\0';

    char cand[16][64];
    int ncand = 0;
    bool cand_is_dir[16];

    if (first_word) {
        for (int i = 0; i < CMD_N; i++) {
            if (term_starts_with(CMD_LIST[i], prefix)) {
                term_cand_add(cand, &ncand, CMD_LIST[i]);
                cand_is_dir[ncand - 1] = false;
            }
        }
    } else {
        // path completion: split the token into dir part + base name
        char dirpart[128], base[96];
        int last_slash = -1;
        for (int i = 0; i < pc; i++) if (prefix[i] == '/') last_slash = i;
        int bl = 0;
        for (int i = last_slash + 1; i < pc; i++) base[bl++] = prefix[i];
        base[bl] = '\0';
        if (last_slash >= 0) {
            int dl = 0;
            for (int i = 0; i <= last_slash; i++) dirpart[dl++] = prefix[i];
            dirpart[dl] = '\0';
        } else {
            dirpart[0] = '\0';
        }
        // which command owns the token? (cd wants dirs, cat wants files)
        char cmd[32];
        int cc = 0;
        int cs = 0;
        while (cs < t->llen && t->line[cs] == ' ') cs++;
        for (; cs < t->llen && t->line[cs] != ' ' && cc < 31; cs++) cmd[cc++] = t->line[cs];
        cmd[cc] = '\0';
        bool want_dirs = (strcmp(cmd, "cd") == 0);
        bool want_files = (strcmp(cmd, "cat") == 0);

        char full[192];
        if (prefix[0] == '/') {
            // absolute: dir part starts with '/'
            if (last_slash == 0) { full[0] = '/'; full[1] = '\0'; }
            else {
                int k = 0;
                for (; k < last_slash && k < 190; k++) full[k] = prefix[k];
                full[k] = '\0';
            }
        } else if (last_slash >= 0) {
            term_resolve(t, dirpart, full, sizeof(full));
        } else {
            int k = 0;
            for (; t->cwd[k] != '\0' && k < 190; k++) full[k] = t->cwd[k];
            full[k] = '\0';
        }
        RNode *dir = ramfs_resolve(full);
        if (dir && ramfs_is_dir(dir)) {
            RNode *items[64];
            int n = ramfs_list(dir, items, 64);
            for (int i = 0; i < n && ncand < 16; i++) {
                if (want_dirs && !ramfs_is_dir(items[i])) continue;
                if (want_files && ramfs_is_dir(items[i])) continue;
                if (!term_starts_with(items[i]->name, base)) continue;
                term_cand_add(cand, &ncand, items[i]->name);
                cand_is_dir[ncand - 1] = ramfs_is_dir(items[i]);
            }
        }
    }

    if (ncand == 0) { t->last_tab_ms = now_ms(); t->last_tab_len = t->llen; return; }

    if (ncand == 1) {
        char ins[70];
        int k = 0;
        for (; cand[0][k] != '\0'; k++) ins[k] = cand[0][k];
        if (cand_is_dir[0] && k < 68) ins[k++] = '/';
        ins[k++] = ' ';
        ins[k] = '\0';
        term_line_remove_range(t, tok_start, plen);
        term_line_insert(t, ins, k);
    } else {
        // longest common prefix
        int cp = (int)strlen(cand[0]);
        for (int i = 1; i < ncand; i++) {
            int l = (int)strlen(cand[i]);
            int j = 0;
            for (; j < cp && j < l && cand[i][j] == cand[0][j]; j++) {}
            cp = j;
        }
        bool progressed = false;
        if (cp > plen) {
            term_line_remove_range(t, tok_start, plen);
            term_line_insert(t, cand[0], cp);
            progressed = true;
        }
        // double TAB with no progress -> list the candidates
        if (!progressed && t->last_tab_len == t->llen && now_ms() - t->last_tab_ms < 700) {
            term_out(t, "\n");
            for (int i = 0; i < ncand; i++) {
                term_out(t, cand[i]);
                if (cand_is_dir[i]) term_out(t, "/");
                term_out(t, (i % 4 == 3 || i == ncand - 1) ? "\n" : "   ");
            }
        }
    }
    t->last_tab_ms = now_ms();
    t->last_tab_len = t->llen;
}

// Small helper kept separate so the compiler cannot reorder the bounds
// checks with the memmove-style shift above.
static void term_line_remove_range(TermApp *t, int at, int count) {
    if (at < 0 || count <= 0 || at + count > t->llen) return;
    for (int j = at; j + count < t->llen; j++) t->line[j] = t->line[j + count];
    t->llen -= count;
    t->line[t->llen] = '\0';
    if (t->lcaret > at + count) t->lcaret -= count;
    else if (t->lcaret > t->llen) t->lcaret = t->llen;
}

// ---------------- commands ----------------

static void cmd_ls(TermApp *t, const char *arg) {
    char path[160];
    RNode *dir;
    if (arg[0] == '\0') { dir = ramfs_resolve(t->cwd); }
    else { term_resolve(t, arg, path, sizeof(path)); dir = ramfs_resolve(path); }
    if (!dir || !ramfs_is_dir(dir)) { term_out(t, "ls: no such directory\n"); return; }
    RNode *items[64];
    int n = ramfs_list(dir, items, 64);
    for (int i = 0; i < n; i++) {
        term_out(t, items[i]->name);
        term_out(t, ramfs_is_dir(items[i]) ? "/\n" : "\n");
    }
}

static void cmd_cat(TermApp *t, const char *arg) {
    if (arg[0] == '\0') { term_out(t, "usage: cat <file>\n"); return; }
    char path[160];
    term_resolve(t, arg, path, sizeof(path));
    RNode *f = ramfs_resolve(path);
    if (!f || ramfs_is_dir(f)) { term_out(t, "cat: no such file\n"); return; }
    term_out(t, ramfs_read(f));
    term_out(t, "\n");
}

static void cmd_cd(TermApp *t, const char *arg) {
    char path[160];
    if (arg[0] == '\0') { t->cwd[0] = '/'; t->cwd[1] = '\0'; return; }
    term_resolve(t, arg, path, sizeof(path));
    RNode *d = ramfs_resolve(path);
    if (!d || !ramfs_is_dir(d)) { term_out(t, "cd: no such directory\n"); return; }
    ramfs_path(d, t->cwd, sizeof(t->cwd));
}

static void cmd_mkdir_touch(TermApp *t, const char *arg, bool dir) {
    if (arg[0] == '\0') { term_out(t, dir ? "usage: mkdir <dir>\n" : "usage: touch <file>\n"); return; }
    char path[160], parent[160];
    term_resolve(t, arg, path, sizeof(path));
    int last = -1;
    for (int k = 0; path[k] != '\0'; k++) if (path[k] == '/') last = k;
    if (last < 0) { term_out(t, "bad path\n"); return; }
    int k2 = 0;
    for (int k = 0; k <= last && k2 < 159; k++) parent[k2++] = path[k];
    parent[k2] = '\0';
    RNode *pd = ramfs_resolve(parent);
    if (!pd || !ramfs_is_dir(pd)) { term_out(t, "no such directory\n"); return; }
    bool ok = dir ? (ramfs_mkdir(pd, path + last + 1) != 0)
                  : (ramfs_create(pd, path + last + 1, "") != 0);
    if (!ok) term_out(t, dir ? "mkdir: failed (exists or full)\n" : "touch: failed\n");
}

static void term_exec(TermApp *t) {
    term_out(t, "kian@kian-os:");
    term_out(t, t->cwd);
    term_out(t, "$ ");
    term_out(t, t->line);
    term_out(t, "\n");
    term_hist_push(t, t->line);

    char cmd[32], arg[96];
    int i = 0, c = 0;
    while (t->line[i] == ' ') i++;
    for (; t->line[i] != '\0' && t->line[i] != ' ' && c < 31; i++) cmd[c++] = t->line[i];
    cmd[c] = '\0';
    while (t->line[i] == ' ') i++;
    int a = 0;
    for (; t->line[i] != '\0' && a < 95; i++) arg[a++] = t->line[i];
    arg[a] = '\0';

    if (cmd[0] == '\0') { /* empty enter: new prompt */ }
    else if (strcmp(cmd, "help") == 0) {
        term_out(t,
            "commands: help clear echo date uptime mem whoami uname pwd\n"
            "          ls cd cat mkdir touch rm exit\n"
            "TAB completes commands and file names (TAB TAB lists).\n"
            "Up/Down walk the command history. 'exit' closes.\n");
    } else if (strcmp(cmd, "clear") == 0) {
        t->buf.clear();
    } else if (strcmp(cmd, "echo") == 0) {
        term_out(t, arg);
        term_out(t, "\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        term_out(t, t->cwd);
        term_out(t, "\n");
    } else if (strcmp(cmd, "date") == 0) {
        kian_time_t tm;
        kian_time_now(&tm);
        char d[64];
        int k = 0;
        d[0] = '\0';
        put_num(d, k, tm.year); d[k++] = '-';
        put_pad2(d, k, tm.month); d[k++] = '-';
        put_pad2(d, k, tm.day); d[k++] = ' ';
        put_pad2(d, k, tm.hour); d[k++] = ':';
        put_pad2(d, k, tm.minute); d[k++] = ':';
        put_pad2(d, k, tm.second); d[k++] = '\0';
        term_out(t, d);
        term_out(t, " Asia/Tehran (UTC+03:30)\n");
    } else if (strcmp(cmd, "uptime") == 0) {
        unsigned long long s = kian_uptime_ms() / 1000;
        term_out(t, "up ");
        term_out_num(t, (long long)(s / 3600));
        term_out(t, "h ");
        term_out_num(t, (long long)((s / 60) % 60));
        term_out(t, "m ");
        term_out_num(t, (long long)(s % 60));
        term_out(t, "s\n");
    } else if (strcmp(cmd, "mem") == 0) {
        uint64_t used = 0, total = 0;
        kian_mem_stats(&used, &total);
        term_out(t, "heap: ");
        term_out_num(t, (long long)(used >> 10));
        term_out(t, " KiB used of ");
        term_out_num(t, (long long)(total >> 20));
        term_out(t, " MiB\n");
    } else if (strcmp(cmd, "whoami") == 0) {
        term_out(t, "kian\n");
    } else if (strcmp(cmd, "uname") == 0) {
        term_out(t, "KianOS 0.3.0 x86_64 (Rust kernel)\n");
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(t, arg);
    } else if (strcmp(cmd, "cd") == 0) {
        cmd_cd(t, arg);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(t, arg);
    } else if (strcmp(cmd, "mkdir") == 0) {
        cmd_mkdir_touch(t, arg, true);
    } else if (strcmp(cmd, "touch") == 0) {
        cmd_mkdir_touch(t, arg, false);
    } else if (strcmp(cmd, "rm") == 0) {
        char path[160];
        term_resolve(t, arg, path, sizeof(path));
        RNode *n = ramfs_resolve(path);
        if (!n || n == ramfs_root() || n == ramfs_trash()) term_out(t, "rm: no such file\n");
        else if (!ramfs_trash_node(n)) term_out(t, "rm: failed\n");
    } else if (strcmp(cmd, "exit") == 0) {
        shell_close_focused(); // 'exit' closes this window
        return;
    } else {
        term_out(t, "ksh: ");
        term_out(t, cmd);
        term_out(t, ": command not found\n");
    }

    t->line[0] = '\0';
    t->llen = 0;
    t->lcaret = 0;
}

// ---------------- app interface ----------------

void *term_create() {
    TermApp *t = new TermApp();
    t->cwd[0] = '/';
    t->cwd[1] = '\0';
    t->line[0] = '\0';
    t->llen = 0;
    t->lcaret = 0;
    t->hist_n = 0;
    t->hist_head = 0;
    t->hist_view = -1;
    t->last_tab_ms = 0;
    t->last_tab_len = 0;
    term_out(t, "Kian OS terminal - 'help' lists commands. TAB completes.\n");
    return t;
}

void term_destroy(void *inst) {
    TermApp *t = (TermApp *)inst;
    delete t;
}

static int term_count_lines(TermApp *t) {
    int total = 1;
    for (int i = 0; i < t->buf.length(); i++) {
        if (t->buf[i] == '\n') total++;
    }
    return total;
}

void term_draw(void *inst, Rect r) {
    TermApp *t = (TermApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->input_bg);
    int rowh = text_line_height_mono(1);   // 16
    int maxlines = (r.h - 8) / rowh;
    if (maxlines < 2) return;
    // draw last (maxlines-1) lines of the buffer, bottom-aligned
    int total = term_count_lines(t);
    int skip = total - (maxlines - 1);
    if (skip < 0) skip = 0;
    char line[TERM_COLS_MAX + 2];
    int y = r.y + 4;
    int lineno = 0, start = 0, drawn = 0;
    int blen = t->buf.length();
    for (int i = 0; i <= blen && drawn < maxlines - 1; i++) {
        bool nl = (i == blen) || (t->buf[i] == '\n');
        if (nl) {
            if (lineno >= skip) {
                int len = i - start;
                if (len > TERM_COLS_MAX) len = TERM_COLS_MAX;
                for (int k = 0; k < len; k++) line[k] = t->buf[start + k];
                line[len] = '\0';
                draw_text_mono(r.x + 4, y, line, THEME->text, 1);
                y += rowh;
                drawn++;
            }
            lineno++;
            start = i + 1;
        }
    }
    // prompt + input line + caret (mono cell = 8 px, caret is mid-line)
    char pl[TERM_COLS_MAX + 2];
    int k = 0;
    const char *p1 = "kian@kian-os:";
    for (; p1[k] != '\0' && k < TERM_COLS_MAX; k++) pl[k] = p1[k];
    for (int m = 0; t->cwd[m] != '\0' && k < TERM_COLS_MAX; m++) pl[k++] = t->cwd[m];
    if (k < TERM_COLS_MAX) pl[k++] = '$';
    if (k < TERM_COLS_MAX) pl[k++] = ' ';
    int prompt_len = k;
    for (int m = 0; t->line[m] != '\0' && k < TERM_COLS_MAX; m++) pl[k++] = t->line[m];
    pl[k] = '\0';
    draw_text_mono(r.x + 4, y, pl, THEME->text, 1);
    if (((now_ms() / 400) & 1) == 0) {
        int cx = r.x + 4 + (prompt_len + t->lcaret) * glyph_advance_mono(1);
        vline(cx + 1, mono_caret_y(y), mono_caret_h(), THEME->accent);
    }
}

void term_event(void *inst, const kian_event_t *e, Rect r) {
    (void)r;
    TermApp *t = (TermApp *)inst;
    if (e->type != KIAN_EV_KEY_DOWN) return;
    int a = e->a;
    char ch = (char)e->b;

    if (a == KIAN_KEY_TAB || ch == '\t') {
        term_tab_complete(t);
        return;
    }
    if (a == KIAN_KEY_UP) { term_hist_nav(t, -1); return; }
    if (a == KIAN_KEY_DOWN) { term_hist_nav(t, 1); return; }
    if (a == KIAN_KEY_LEFT) { if (t->lcaret > 0) t->lcaret--; return; }
    if (a == KIAN_KEY_RIGHT) { if (t->lcaret < t->llen) t->lcaret++; return; }
    if (a == KIAN_KEY_HOME) { t->lcaret = 0; return; }
    if (a == KIAN_KEY_END) { t->lcaret = t->llen; return; }
    if (a == KIAN_KEY_BACKSPACE || ch == 8) {
        if (t->lcaret > 0) {
            t->lcaret--;
            term_line_remove_at(t, t->lcaret);
        }
        return;
    }
    if (a == KIAN_KEY_DEL) { term_line_remove_at(t, t->lcaret); return; }

    if (ch == '\n') {
        t->line[t->llen] = '\0';
        term_exec(t);
        return;
    }
    if (ch >= 32 && ch < 127) {
        char one[2] = { ch, '\0' };
        term_line_insert(t, one, 1);
    }
}
