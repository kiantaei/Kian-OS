// Kian OS GUI - Kian Terminal app: professional Linux-style terminal.
// Tabs (bounded pool of 4, independent state), colored prompt, colored
// scrollback with wheel scrolling + scrollbar, caret line editing,
// history, TAB completion over the command registry and RAM FS paths,
// Ctrl+C (^C), Ctrl+L, Ctrl+U, nano mode, python REPL mode.
// HONEST: the filesystem is RAM-only and is lost on reboot.
#include "terminal.h"
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"
#include "../theme.h"

static bool g_term_milestone = false;

static const int TABBAR_H = 24;
static const int TABW = 116;

// ---------------- theme-aware palette ----------------

uint32_t term_color(int tag) {
    bool dark = (THEME->wallpaper_top == 0x001A2B4A);
    switch (tag) {
    case TC_BG:     return dark ? 0x000C0C0C : 0x00F4F4F2;
    case TC_FG:     return dark ? 0x00D4D4D4 : 0x00202428;
    case TC_DIM:    return dark ? 0x00708090 : 0x00707880;
    case TC_USER:   return dark ? 0x004ADE80 : 0x00168838;
    case TC_PATH:   return dark ? 0x0060A5FA : 0x001A66CC;
    case TC_PROMPT: return dark ? 0x004CC2FF : 0x000B69B0;
    case TC_ERR:    return dark ? 0x00F87171 : 0x00C42B1C;
    case TC_WARN:   return dark ? 0x00FBBF24 : 0x00A66A00;
    case TC_OK:     return dark ? 0x004ADE80 : 0x00168838;
    case TC_ACCENT: return THEME->accent;
    }
    return THEME->text;
}

// ---------------- scrollback ----------------

void term_print_split(TermTab *t, const char *s, int tag, int split, int tag2) {
    if (!t) return;
    TLine *ln = &t->ring[t->rhead];
    int k = 0;
    for (; s[k] != '\0' && k < TL_LEN; k++) ln->t[k] = s[k];
    ln->t[k] = '\0';
    ln->col = (uint8_t)tag;
    ln->col2 = (uint8_t)tag2;
    ln->split = (uint8_t)((split > k) ? k : split);
    t->rhead = (t->rhead + 1) % TL_RING;
    if (t->rcount < TL_RING) t->rcount++;
}

void term_print(TermTab *t, const char *s, int tag) {
    if (!t) return;
    char chunk[TL_LEN + 1];
    int cl = 0;
    for (int i = 0; ; i++) {
        char c = s[i];
        if (c == '\n' || c == '\0') {
            chunk[cl] = '\0';
            term_print_split(t, chunk, tag, 0, tag);
            cl = 0;
            if (c == '\0') break;
            continue;
        }
        chunk[cl++] = c;
        if (cl == TL_LEN) {
            chunk[cl] = '\0';
            term_print_split(t, chunk, tag, 0, tag);
            cl = 0;
        }
    }
}

// ---------------- tab helpers ----------------

static TermTab *tcur(TermApp *a) { return &a->tab[a->active]; }

static void tab_reset(TermTab *t) {
    t->rhead = 0;
    t->rcount = 0;
    t->ed.line[0] = '\0';
    t->ed.llen = 0;
    t->ed.lcaret = 0;
    t->cwd[0] = '/';
    t->cwd[1] = '\0';
    t->hist_n = 0;
    t->hist_head = 0;
    t->hist_view = -1;
    t->py = 0;
    t->nano = 0;
    t->mode = TM_SHELL;
    t->scroll = 0;
    t->py_input[0] = '\0';
    t->py_len = 0;
    t->py_caret = 0;
}

static void tab_close_extra(TermTab *t) {
    if (t->py) { py_free(t->py); t->py = 0; }
    if (t->nano) { nano_close(t->nano); t->nano = 0; }
}

// ---------------- line editing helpers ----------------

static void line_insert(TermTab *t, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    for (int k = 0; k < n; k++) {
        if (t->ed.llen >= 94) break;
        for (int j = t->ed.llen; j > t->ed.lcaret; j--) t->ed.line[j] = t->ed.line[j - 1];
        t->ed.line[t->ed.lcaret++] = s[k];
        t->ed.llen++;
        t->ed.line[t->ed.llen] = '\0';
    }
}

static void line_remove_at(TermTab *t, int idx) {
    if (idx < 0 || idx >= t->ed.llen) return;
    for (int j = idx; j < t->ed.llen - 1; j++) t->ed.line[j] = t->ed.line[j + 1];
    t->ed.llen--;
    t->ed.line[t->ed.llen] = '\0';
    if (t->ed.lcaret > t->ed.llen) t->ed.lcaret = t->ed.llen;
}

static void line_remove_range(TermTab *t, int at, int count) {
    if (at < 0 || count <= 0 || at + count > t->ed.llen) return;
    for (int j = at; j + count < t->ed.llen; j++) t->ed.line[j] = t->ed.line[j + count];
    t->ed.llen -= count;
    t->ed.line[t->ed.llen] = '\0';
    if (t->ed.lcaret > at + count) t->ed.lcaret -= count;
    else if (t->ed.lcaret > t->ed.llen) t->ed.lcaret = t->ed.llen;
}

static void line_set(TermTab *t, const char *s) {
    int i = 0;
    for (; s[i] != '\0' && i < 94; i++) t->ed.line[i] = s[i];
    t->ed.line[i] = '\0';
    t->ed.llen = i;
    t->ed.lcaret = i;
}

static void pyline_insert(TermTab *t, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    for (int k = 0; k < n; k++) {
        if (t->py_len >= 94) break;
        for (int j = t->py_len; j > t->py_caret; j--) t->py_input[j] = t->py_input[j - 1];
        t->py_input[t->py_caret++] = s[k];
        t->py_len++;
        t->py_input[t->py_len] = '\0';
    }
}

// ---------------- history ----------------

static void hist_push(TermTab *t, const char *s) {
    if (s[0] == '\0') return;
    int last = (t->hist_head - 1 + TERM_HIST) % TERM_HIST;
    if (t->hist_n > 0 && strcmp(t->hist[last], s) == 0) return;
    int k = 0;
    for (; s[k] != '\0' && k < 94; k++) t->hist[t->hist_head][k] = s[k];
    t->hist[t->hist_head][k] = '\0';
    t->hist_head = (t->hist_head + 1) % TERM_HIST;
    if (t->hist_n < TERM_HIST) t->hist_n++;
    t->hist_view = -1;
}

static void hist_nav(TermTab *t, int dir) {
    if (t->hist_n == 0) return;
    if (dir < 0) {
        if (t->hist_view == -1) t->hist_view = (t->hist_head - 1 + TERM_HIST) % TERM_HIST;
        else t->hist_view = (t->hist_view - 1 + TERM_HIST) % TERM_HIST;
    } else {
        if (t->hist_view == -1) return;
        int next = (t->hist_view + 1) % TERM_HIST;
        if (next == t->hist_head) { t->hist_view = -1; line_set(t, ""); return; }
        t->hist_view = next;
    }
    if (t->hist_view != -1) line_set(t, t->hist[t->hist_view]);
}

// ---------------- TAB completion ----------------

static bool starts_with(const char *s, const char *prefix) {
    for (int i = 0; prefix[i] != '\0'; i++) {
        if (s[i] != prefix[i]) return false;
    }
    return true;
}

static void cand_add(char cand[][64], int *ncand, const char *s) {
    if (*ncand >= 16) return;
    int k = 0;
    for (; s[k] != '\0' && k < 62; k++) cand[*ncand][k] = s[k];
    cand[*ncand][k] = '\0';
    (*ncand)++;
}

// Completes the word at the caret. First word -> registry command names,
// later words -> RAM FS paths (dirs get a trailing '/'). Unique match is
// inserted with a trailing space; several matches extend to the common
// prefix; TAB TAB with no progress lists the candidates.
static void tab_complete(TermApp *a, TermTab *t) {
    (void)a;
    int tok_start = t->ed.lcaret;
    while (tok_start > 0 && t->ed.line[tok_start - 1] != ' ') tok_start--;
    bool first_word = true;
    for (int i = 0; i < tok_start; i++) {
        if (t->ed.line[i] != ' ') { first_word = false; break; }
    }
    int plen = t->ed.lcaret - tok_start;
    char prefix[96];
    int pc = 0;
    for (int i = tok_start; i < t->ed.lcaret && pc < 94; i++) prefix[pc++] = t->ed.line[i];
    prefix[pc] = '\0';

    char cand[16][64];
    bool cand_dir[16];
    int ncand = 0;

    if (first_word) {
        for (int i = 0; i < term_cmd_count(); i++) {
            const CmdDef *c = term_cmd_at(i);
            if (starts_with(c->name, prefix)) {
                cand_add(cand, &ncand, c->name);
                cand_dir[ncand - 1] = false;
            }
        }
    } else {
        char dirpart[160], base[96];
        int last_slash = -1;
        for (int i = 0; i < pc; i++) if (prefix[i] == '/') last_slash = i;
        int bl = 0;
        for (int i = last_slash + 1; i < pc; i++) base[bl++] = prefix[i];
        base[bl] = '\0';
        char full[192];
        if (prefix[0] == '/' || last_slash >= 0) {
            char arg[128];
            int al = 0;
            for (int i = 0; i <= last_slash && al < 126; i++) arg[al++] = prefix[i];
            arg[al] = '\0';
            if (prefix[0] == '/') {
                int k = 0;
                for (; arg[k] != '\0' && k < 190; k++) full[k] = arg[k];
                full[k] = '\0';
            } else {
                term_resolve_path(t->cwd, arg, full, sizeof(full));
            }
        } else {
            int k = 0;
            for (; t->cwd[k] != '\0' && k < 190; k++) full[k] = t->cwd[k];
            full[k] = '\0';
        }
        (void)dirpart;
        RNode *dir = ramfs_resolve(full);
        if (dir && ramfs_is_dir(dir)) {
            // which command owns the token? cd wants dirs, cat wants files
            char cmd[32];
            int cc = 0;
            int cs = 0;
            while (cs < t->ed.llen && t->ed.line[cs] == ' ') cs++;
            for (; cs < t->ed.llen && t->ed.line[cs] != ' ' && cc < 31; cs++) cmd[cc++] = t->ed.line[cs];
            cmd[cc] = '\0';
            bool want_dirs = (strcmp(cmd, "cd") == 0 || strcmp(cmd, "ls") == 0);
            bool want_files = (strcmp(cmd, "cat") == 0 || strcmp(cmd, "nano") == 0);
            RNode *items[64];
            int n = ramfs_list(dir, items, 64);
            for (int i = 0; i < n && ncand < 16; i++) {
                if (want_dirs && !ramfs_is_dir(items[i])) continue;
                if (want_files && ramfs_is_dir(items[i])) continue;
                if (!starts_with(items[i]->name, base)) continue;
                cand_add(cand, &ncand, items[i]->name);
                cand_dir[ncand - 1] = ramfs_is_dir(items[i]);
            }
        }
    }

    if (ncand == 0) {
        a->last_tab_ms = now_ms();
        a->last_tab_len = t->ed.llen;
        return;
    }

    if (ncand == 1) {
        char ins[70];
        int k = 0;
        for (; cand[0][k] != '\0'; k++) ins[k] = cand[0][k];
        if (cand_dir[0] && k < 68) ins[k++] = '/';
        ins[k++] = ' ';
        ins[k] = '\0';
        line_remove_range(t, tok_start, plen);
        line_insert(t, ins, k);
    } else {
        int cp = (int)strlen(cand[0]);
        for (int i = 1; i < ncand; i++) {
            int l = (int)strlen(cand[i]);
            int j = 0;
            for (; j < cp && j < l && cand[i][j] == cand[0][j]; j++) {}
            cp = j;
        }
        bool progressed = false;
        if (cp > plen) {
            line_remove_range(t, tok_start, plen);
            char ins[70];
            int k = 0;
            for (; k < cp; k++) ins[k] = cand[0][k];
            ins[k] = '\0';
            line_insert(t, ins, k);
            progressed = true;
        }
        if (!progressed && a->last_tab_len == t->ed.llen && now_ms() - a->last_tab_ms < 700) {
            term_print(t, "\n", TC_DIM);
            for (int i = 0; i < ncand; i++) {
                term_print(t, cand[i], cand_dir[i] ? TC_PATH : TC_FG);
                if (cand_dir[i]) term_print(t, "/", TC_PATH);
                term_print(t, (i % 4 == 3 || i == ncand - 1) ? "\n" : "   ", TC_DIM);
            }
        }
    }
    a->last_tab_ms = now_ms();
    a->last_tab_len = t->ed.llen;
}

// ---------------- prompt + execution ----------------

static void prompt_echo(TermTab *t) {
    char full[TL_LEN + 8];
    int k = 0;
    const char *p1 = "kian@kian-os:";
    for (; p1[k] != '\0' && k < TL_LEN - 2; k++) full[k] = p1[k];
    for (int m = 0; t->cwd[m] != '\0' && k < TL_LEN - 4; m++) full[k++] = t->cwd[m];
    if (k < TL_LEN - 4) full[k++] = '$';
    if (k < TL_LEN - 4) full[k++] = ' ';
    int split = k;
    for (int m = 0; t->ed.line[m] != '\0' && k < TL_LEN - 1; m++) full[k++] = t->ed.line[m];
    full[k] = '\0';
    term_print_split(t, full, TC_FG, split, TC_PATH);
}

static void run_job(TermTab *t, int ji, const char *stdin_text, Str *cap, bool capture_on) {
    CmdEnv env;
    env.tab = t;
    env.stdin_text = stdin_text;
    env.capture = capture_on;
    env.cap = cap;
    int c = 0;
    for (; t->cwd[c] != '\0' && c < 126; c++) env.cwd[c] = t->cwd[c];
    env.cwd[c] = '\0';
    env.hist = t->hist;
    env.hist_n = t->hist_n;
    env.hist_head = t->hist_head;
    int argc = t->jobs.argc[ji];
    char *argv[TP_ARGC];
    for (int i = 0; i < argc; i++) argv[i] = t->jobs.argv[ji][i];
    const CmdDef *cmd = term_cmd_find(t->jobs.argv[ji][0]);
    if (!cmd) {
        term_print(t, "ksh: ", TC_ERR);
        term_print(t, t->jobs.argv[ji][0], TC_ERR);
        term_print(t, ": command not found (try 'help')\n", TC_ERR);
        return;
    }
    cmd->fn(&env, argc, argv);
    // 'cd' may have changed env.cwd -> sync back to the tab
    c = 0;
    for (; env.cwd[c] != '\0' && c < 126; c++) t->cwd[c] = env.cwd[c];
    t->cwd[c] = '\0';
}

static void term_exec(TermApp *a, TermTab *t) {
    (void)a;
    prompt_echo(t);
    hist_push(t, t->ed.line);

    char perr[96];
    int njobs = term_parse_jobs(t->ed.line, &t->jobs, perr, sizeof(perr));
    t->ed.line[0] = '\0';
    t->ed.llen = 0;
    t->ed.lcaret = 0;
    t->hist_view = -1;
    t->scroll = 0;

    if (njobs == 0) {
        if (perr[0] != '\0') {
            term_print(t, perr, TC_ERR);
            term_print(t, "\n", TC_ERR);
        }
        return;   // empty line -> just a new prompt
    }

    Str capA, capB;
    Str indata;
    const char *stdin_text = "";
    // '< file' on the first job feeds the pipe
    if (t->jobs.infile[0][0] != '\0') {
        char path[160];
        term_resolve_path(t->cwd, t->jobs.infile[0], path, sizeof(path));
        RNode *f = ramfs_resolve(path);
        if (!f || ramfs_is_dir(f)) {
            term_print(t, "ksh: no such file: ", TC_ERR);
            term_print(t, t->jobs.infile[0], TC_ERR);
            term_print(t, "\n", TC_ERR);
            return;
        }
        indata.append(ramfs_read(f));
        stdin_text = indata.c_str();
    }

    for (int ji = 0; ji < njobs; ji++) {
        bool last = (ji == njobs - 1);
        bool to_file = (t->jobs.outfile[ji][0] != '\0');
        Str *cap = (ji % 2 == 0) ? &capA : &capB;
        cap->clear();
        // capture when the output is piped or redirected (bounded 8 KB)
        run_job(t, ji, stdin_text, cap, !last || to_file);
        if (to_file) {
            // write/append the captured output to the RAM fs file
            char path[160];
            term_resolve_path(t->cwd, t->jobs.outfile[ji], path, sizeof(path));
            char dir[160], base[64];
            int last2 = -1;
            for (int k = 0; path[k] != '\0'; k++) if (path[k] == '/') last2 = k;
            int d = 0;
            for (int k = 0; k <= last2 && d < 158; k++) dir[d++] = path[k];
            dir[d] = '\0';
            if (d == 0) { dir[0] = '/'; dir[1] = '\0'; }
            int b = 0;
            for (int k = last2 + 1; path[k] != '\0' && b < 62; k++) base[b++] = path[k];
            base[b] = '\0';
            RNode *pd = ramfs_resolve(dir);
            RNode *f = pd ? ramfs_find(pd, base) : 0;
            if (!f && pd && ramfs_is_dir(pd)) f = ramfs_create(pd, base, "");
            if (!f) {
                term_print(t, "ksh: cannot write ", TC_ERR);
                term_print(t, t->jobs.outfile[ji], TC_ERR);
                term_print(t, "\n", TC_ERR);
            } else if (t->jobs.append[ji]) {
                ramfs_append(f, cap->c_str());
            } else {
                ramfs_write(f, cap->c_str(), cap->length());
            }
            if (!last) stdin_text = "";   // redirected away: pipe gets EOF
        } else if (!last) {
            stdin_text = cap->c_str();
        }
    }
}

// ---------------- Ctrl+C / clear ----------------

static void term_ctrl_c(TermApp *a, TermTab *t) {
    (void)a;
    if (t->mode == TM_PYTHON && t->py) {
        term_print(t, "^C", TC_WARN);
        term_print(t, "\n", TC_FG);
        py_free(t->py);
        t->py = 0;
        t->mode = TM_SHELL;
        t->py_len = 0;
        t->py_input[0] = '\0';
        return;
    }
    if (t->mode == TM_NANO) return;   // nano handles Ctrl+C itself (position)
    // shell: echo prompt + line, then ^C, like a real terminal
    prompt_echo(t);
    term_print(t, "^C", TC_WARN);
    t->ed.line[0] = '\0';
    t->ed.llen = 0;
    t->ed.lcaret = 0;
    t->hist_view = -1;
}

// ---------------- python mode output thunk ----------------

static void term_py_out(void *ud, const char *s, int tag) {
    term_print((TermTab *)ud, s, tag);
}

static void py_submit(TermApp *a, TermTab *t) {
    (void)a;
    int rc = py_line(t->py, t->py_input, term_py_out, t);
    t->py_input[0] = '\0';
    t->py_len = 0;
    t->py_caret = 0;
    t->scroll = 0;
    if (rc == PY_EXIT) {
        py_free(t->py);
        t->py = 0;
        t->mode = TM_SHELL;
    }
}

// ---------------- tab management ----------------

static void term_new_tab(TermApp *a) {
    if (a->ntabs >= TERM_MAX_TABS) {
        shell_toast("Kian Terminal: tab pool full (4 tabs)");
        return;
    }
    TermTab *nt = &a->tab[a->ntabs++];
    tab_reset(nt);
    term_print(nt, "Kian Terminal - new tab\n", TC_DIM);
    a->active = a->ntabs - 1;
}

static void term_close_tab(TermApp *a, int idx) {
    if (idx < 0 || idx >= a->ntabs) return;
    TermTab *ct = &a->tab[idx];
    if (a->ntabs == 1) {
        // closing the only tab closes the terminal window
        tab_close_extra(ct);
        shell_close_focused();
        return;
    }
    tab_close_extra(ct);
    for (int i = idx; i < a->ntabs - 1; i++) {
        a->tab[i] = a->tab[i + 1];
        // fix internal pointers? py/nano pointers move with the struct copy
        // (they point at heap blocks, not into the tab) - safe.
    }
    a->ntabs--;
    if (a->active >= a->ntabs) a->active = a->ntabs - 1;
}

// ---------------- draw ----------------

struct Cell { char c; uint8_t col; };

static int build_input_cells(TermApp *a, TermTab *t, Cell *stream, int cap) {
    // prompt segments
    int n = 0;
    if (t->mode == TM_PYTHON) {
        static const char *pp = ">>> ";
        static const char *pc = "... ";
        const char *p = (t->py && py_pending(t->py)) ? pc : pp;
        for (; *p != '\0' && n < cap; p++) { stream[n].c = *p; stream[n].col = (uint8_t)TC_ACCENT; n++; }
        for (int i = 0; i < t->py_len && n < cap; i++) {
            stream[n].c = t->py_input[i];
            stream[n].col = (uint8_t)TC_FG;
            n++;
        }
        return n;
    }
    const char *p1 = "kian@kian-os:";
    for (; *p1 != '\0' && n < cap; p1++) { stream[n].c = *p1; stream[n].col = (uint8_t)TC_USER; n++; }
    for (int i = 0; t->cwd[i] != '\0' && n < cap; i++) { stream[n].c = t->cwd[i]; stream[n].col = (uint8_t)TC_PATH; n++; }
    if (n < cap) { stream[n].c = '$'; stream[n].col = (uint8_t)TC_PROMPT; n++; }
    if (n < cap) { stream[n].c = ' '; stream[n].col = (uint8_t)TC_PROMPT; n++; }
    for (int i = 0; i < t->ed.llen && n < cap; i++) {
        stream[n].c = t->ed.line[i];
        stream[n].col = (uint8_t)TC_FG;
        n++;
    }
    (void)a;
    return n;
}

void term_draw(void *inst, Rect r) {
    TermApp *a = (TermApp *)inst;
    TermTab *t = tcur(a);

    // ---- tab strip ----
    uint32_t strip = (THEME->wallpaper_top == 0x001A2B4A) ? 0x00101018 : 0x00DADCDC;
    uint32_t actbg = (THEME->wallpaper_top == 0x001A2B4A) ? 0x001E2026 : 0x00F4F4F2;
    fill_rect(r.x, r.y, r.w, TABBAR_H, strip);
    int maxtabs = (r.w - 34) / (TABW + 2);
    if (maxtabs < 1) maxtabs = 1;
    for (int i = 0; i < a->ntabs && i < maxtabs; i++) {
        Rect tr = { r.x + 4 + i * (TABW + 2), r.y + 2, TABW, TABBAR_H - 4 };
        bool act = (i == a->active);
        fill_round_rect(tr.x, tr.y, tr.w, tr.h, 5, act ? actbg : strip);
        if (act) hline(tr.x + 3, tr.y + tr.h, tr.w - 6, term_color(TC_ACCENT));
        // mode label
        const char *label = (a->tab[i].mode == TM_PYTHON) ? "python" : (a->tab[i].mode == TM_NANO) ? "nano" : "shell";
        char lbl[24];
        int k = 0;
        put_num(lbl, k, i + 1);
        lbl[k++] = ':';
        lbl[k++] = ' ';
        for (const char *q = label; *q != '\0' && k < 22; q++) lbl[k++] = *q;
        lbl[k] = '\0';
        draw_text_mono(tr.x + 8, text_center_y(tr.y, tr.h, 1) - 1, lbl, act ? term_color(TC_FG) : term_color(TC_DIM), 1);
        Rect xz = { tr.x + tr.w - 16, tr.y + 3, 13, tr.h - 6 };
        bool xh = mouse_in(xz.x, xz.y, xz.w, xz.h);
        if (xh) fill_round_rect(xz.x, xz.y, xz.w, xz.h, 3, THEME->close_btn_bg);
        icon_draw(IC_CLOSE, xz.x + 1, xz.y + 2, 10, xh ? 0x00FFFFFF : (act ? term_color(TC_DIM) : term_color(TC_DIM)));
    }
    if (a->ntabs < TERM_MAX_TABS && a->ntabs < maxtabs) {
        Rect nb = { r.x + 4 + a->ntabs * (TABW + 2), r.y + 5, 20, TABBAR_H - 10 };
        bool nh = mouse_in(nb.x, nb.y, nb.w, nb.h);
        if (nh) fill_round_rect(nb.x, nb.y, nb.w, nb.h, 4, THEME->start_bg_active);
        icon_draw(IC_PLUS, nb.x + 3, nb.y + 3, 14, nh ? THEME->text : term_color(TC_DIM));
    }

    // ---- content ----
    Rect cr = { r.x, r.y + TABBAR_H, r.w, r.h - TABBAR_H };
    if (t->mode == TM_NANO && t->nano) {
        nano_draw(t->nano, cr);
        return;
    }
    fill_rect(cr.x, cr.y, cr.w, cr.h, term_color(TC_BG));

    int rowh = text_line_height_mono(1);
    int cellw = glyph_advance_mono(1);
    int cols = (cr.w - 12 - 10) / cellw;   // 10px right margin for the scrollbar
    if (cols < 20) cols = 20;

    // input visual block (prompt + line, wrapped like a real terminal)
    Cell stream[320];
    int ncells = build_input_cells(a, t, stream, 320);
    int input_rows = (ncells + cols - 1) / cols;
    if (input_rows < 1) input_rows = 1;

    int maxlines = (cr.h - 8) / rowh;
    if (maxlines < input_rows + 1) maxlines = input_rows + 1;
    int hist_rows = maxlines - input_rows;
    if (hist_rows < 0) hist_rows = 0;

    // scroll offset clamp
    int max_scroll = (t->rcount > hist_rows) ? (t->rcount - hist_rows) : 0;
    if (t->scroll > max_scroll) t->scroll = max_scroll;
    if (t->scroll < 0) t->scroll = 0;

    int view_start = t->rcount - hist_rows - t->scroll;
    if (view_start < 0) view_start = 0;

    // draw scrollback slice
    char line[TL_LEN + 2];
    for (int i = 0; i < hist_rows; i++) {
        int idx = view_start + i;
        if (idx >= t->rcount) break;
        int ring = (t->rhead - t->rcount + idx + TL_RING * 2) % TL_RING;
        TLine *ln = &t->ring[ring];
        int len = (int)strlen(ln->t);
        if (len > cols) len = cols;
        for (int k = 0; k < len; k++) line[k] = ln->t[k];
        line[len] = '\0';
        int y = cr.y + 4 + i * rowh;
        if (ln->split > 0) {
            // two-tone line: [0, split) col2, [split, end) col
            if (ln->split > cols) ln->split = (uint8_t)cols;
            char part[TL_LEN + 2];
            int k = 0;
            for (; k < ln->split && k < len; k++) part[k] = ln->t[k];
            part[k] = '\0';
            draw_text_mono(cr.x + 6, y, part, term_color(ln->col2), 1);
            for (; k < len; k++) part[k - ln->split] = ln->t[k];
            part[len - ln->split] = '\0';
            draw_text_mono(cr.x + 6 + ln->split * cellw, y, part, term_color(ln->col), 1);
        } else {
            draw_text_mono(cr.x + 6, y, line, term_color(ln->col), 1);
        }
    }

    // draw the input block (wrapped, colored per cell)
    int base_row = cr.y + 4 + hist_rows * rowh;
    for (int vr = 0; vr < input_rows; vr++) {
        char row[TL_LEN + 2];
        int k = 0;
        uint8_t rowcol = 0xFF;
        bool mixed = false;
        for (int c = 0; c < cols; c++) {
            int idx = vr * cols + c;
            if (idx >= ncells) { row[k] = '\0'; break; }
            row[k++] = stream[idx].c;
            if (rowcol == 0xFF) rowcol = stream[idx].col;
            else if (rowcol != stream[idx].col) mixed = true;
        }
        row[k] = '\0';
        int y = base_row + vr * rowh;
        if (!mixed) {
            draw_text_mono(cr.x + 6, y, row, term_color(rowcol == 0xFF ? TC_FG : rowcol), 1);
        } else {
            // mixed colors: draw cell by cell
            int x = cr.x + 6;
            for (int c = vr * cols; c < ncells && c < (vr + 1) * cols; c++) {
                char one[2] = { stream[c].c, '\0' };
                draw_text_mono(x, y, one, term_color(stream[c].col), 1);
                x += cellw;
            }
        }
        if (k == 0 && vr >= input_rows - 1) { /* empty last row: keep space */ }
    }

    // caret (blink)
    if (((now_ms() / 400) & 1) == 0) {
        int cpos = 0;
        if (t->mode == TM_PYTHON) cpos = 4 + t->py_caret;
        else cpos = (int)strlen("kian@kian-os:") + (int)strlen(t->cwd) + 2 + t->ed.lcaret;
        int crow = cpos / cols;
        int ccol = cpos % cols;
        int cy = base_row + crow * rowh;
        if (crow < input_rows) {
            vline(cr.x + 6 + ccol * cellw + 1, mono_caret_y(cy), mono_caret_h(), term_color(TC_ACCENT));
        }
    }

    // scrollbar
    int total_rows = t->rcount + input_rows;
    if (total_rows > maxlines) {
        int track_y = cr.y + 2, track_h = cr.h - 4;
        int sbx = cr.x + cr.w - 7;
        fill_rect(sbx, track_y, 5, track_h, (THEME->wallpaper_top == 0x001A2B4A) ? 0x00181820 : 0x00E2E2DE);
        int visible = maxlines;
        int th = imax(10, track_h * visible / total_rows);
        int maxoff = imax(1, total_rows - visible);
        int top = view_start;
        int by = track_y + (track_h - th) * imin(top, maxoff) / maxoff;
        fill_rect(sbx + 1, by, 3, th, term_color(TC_DIM));
    }
}

// ---------------- events ----------------

void term_event(void *inst, const kian_event_t *e, Rect r) {
    TermApp *a = (TermApp *)inst;
    TermTab *t = tcur(a);

    // ---- mouse: tab strip + scrollback wheel ----
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        int maxtabs = (r.w - 34) / (TABW + 2);
        if (maxtabs < 1) maxtabs = 1;
        for (int i = 0; i < a->ntabs && i < maxtabs; i++) {
            Rect tr = { r.x + 4 + i * (TABW + 2), r.y + 2, TABW, TABBAR_H - 4 };
            Rect xz = { tr.x + tr.w - 16, tr.y + 3, 13, tr.h - 6 };
            if (rect_hit(IN->mx, IN->my, xz)) { term_close_tab(a, i); return; }
            if (rect_hit(IN->mx, IN->my, tr)) { a->active = i; shell_full_redraw(); return; }
        }
        if (a->ntabs < TERM_MAX_TABS && a->ntabs < maxtabs) {
            Rect nb = { r.x + 4 + a->ntabs * (TABW + 2), r.y + 5, 20, TABBAR_H - 10 };
            if (rect_hit(IN->mx, IN->my, nb)) { term_new_tab(a); shell_full_redraw(); return; }
        }
        return;   // clicks inside the terminal are consumed
    }
    if (e->type == KIAN_EV_MOUSE_WHEEL) {
        int dir = (e->a > 0) ? 3 : -3;
        t->scroll += dir;
        return;
    }

    if (e->type != KIAN_EV_KEY_DOWN) return;
    int akey = e->a;
    char ch = (char)e->b;
    bool ctrl = (e->c & 2) != 0;
    bool shift = (e->c & 1) != 0;

    // ---- global terminal shortcuts ----
    if (ctrl && shift && (ch == 't' || ch == 'T')) { term_new_tab(a); return; }
    if (ctrl && shift && (ch == 'w' || ch == 'W')) { term_close_tab(a, a->active); return; }
    if (ctrl && !shift && akey == KIAN_KEY_TAB) {
        a->active = (a->active + 1) % a->ntabs;
        shell_full_redraw();
        return;
    }
    if (ctrl && (ch == 'c' || ch == 'C') && t->mode != TM_NANO) { term_ctrl_c(a, t); return; }
    // nano mode handles ^C itself (shows the cursor position)
    if (t->mode == TM_SHELL && ctrl && (ch == 'l' || ch == 'L')) {
        t->rhead = 0;
        t->rcount = 0;
        t->scroll = 0;
        return;
    }

    // ---- nano mode ----
    if (t->mode == TM_NANO) {
        if (!t->nano) { t->mode = TM_SHELL; return; }
        int rc = nano_key(t->nano, e);
        if (rc == 1) {
            const char *st = nano_status(t->nano);
            if (st[0] != '\0') term_print(t, st, TC_DIM);
            term_print(t, "\n", TC_FG);
            nano_close(t->nano);
            t->nano = 0;
            t->mode = TM_SHELL;
        }
        return;
    }

    // ---- python mode ----
    if (t->mode == TM_PYTHON) {
        if (akey == KIAN_KEY_TAB) {
            for (int i = 0; i < 4; i++) {
                char sp[2] = { ' ', '\0' };
                pyline_insert(t, sp, 1);
            }
            return;
        }
        if (akey == KIAN_KEY_UP || akey == KIAN_KEY_DOWN) return;   // no python history (kept honest)
        if (akey == KIAN_KEY_LEFT) { if (t->py_caret > 0) t->py_caret--; return; }
        if (akey == KIAN_KEY_RIGHT) { if (t->py_caret < t->py_len) t->py_caret++; return; }
        if (akey == KIAN_KEY_HOME) { t->py_caret = 0; return; }
        if (akey == KIAN_KEY_END) { t->py_caret = t->py_len; return; }
        if (akey == KIAN_KEY_BACKSPACE || ch == 8) {
            if (t->py_caret > 0) {
                t->py_caret--;
                for (int j = t->py_caret; j < t->py_len - 1; j++) t->py_input[j] = t->py_input[j + 1];
                t->py_len--;
                t->py_input[t->py_len] = '\0';
            }
            return;
        }
        if (akey == KIAN_KEY_DEL) {
            for (int j = t->py_caret; j < t->py_len - 1; j++) t->py_input[j] = t->py_input[j + 1];
            if (t->py_caret < t->py_len) t->py_len--;
            t->py_input[t->py_len] = '\0';
            return;
        }
        if (ch == '\n') { py_submit(a, t); return; }
        if (ch >= 32 && ch < 127 && !ctrl) {
            char one[2] = { ch, '\0' };
            pyline_insert(t, one, 1);
        }
        return;
    }

    // ---- shell mode ----
    if (akey == KIAN_KEY_TAB || ch == '\t') { tab_complete(a, t); return; }
    if (akey == KIAN_KEY_UP) { hist_nav(t, -1); return; }
    if (akey == KIAN_KEY_DOWN) { hist_nav(t, 1); return; }
    if (akey == KIAN_KEY_LEFT) { if (t->ed.lcaret > 0) t->ed.lcaret--; return; }
    if (akey == KIAN_KEY_RIGHT) { if (t->ed.lcaret < t->ed.llen) t->ed.lcaret++; return; }
    if (akey == KIAN_KEY_HOME) { t->ed.lcaret = 0; return; }
    if (akey == KIAN_KEY_END) { t->ed.lcaret = t->ed.llen; return; }
    if (ctrl && (ch == 'a' || ch == 'A')) { t->ed.lcaret = 0; return; }
    if (ctrl && (ch == 'e' || ch == 'E')) { t->ed.lcaret = t->ed.llen; return; }
    if (ctrl && (ch == 'u' || ch == 'U')) {
        // kill the whole line (bash-like)
        t->ed.line[0] = '\0';
        t->ed.llen = 0;
        t->ed.lcaret = 0;
        return;
    }
    if (akey == KIAN_KEY_BACKSPACE || ch == 8) {
        if (t->ed.lcaret > 0) {
            t->ed.lcaret--;
            line_remove_at(t, t->ed.lcaret);
        }
        return;
    }
    if (akey == KIAN_KEY_DEL) { line_remove_at(t, t->ed.lcaret); return; }
    if (ch == '\n') { term_exec(a, t); return; }
    if (ch >= 32 && ch < 127 && !ctrl) {
        char one[2] = { ch, '\0' };
        line_insert(t, one, 1);
    }
}

// ---------------- app interface ----------------

void *term_create() {
    TermApp *a = new TermApp();
    a->ntabs = 1;
    a->active = 0;
    a->last_tab_ms = 0;
    a->last_tab_len = 0;
    for (int i = 0; i < TERM_MAX_TABS; i++) tab_reset(&a->tab[i]);
    TermTab *t = &a->tab[0];
    term_print(t, "Kian Terminal", TC_ACCENT);
    term_print(t, "\n", TC_FG);
    term_print(t, "commands: help neofetch ls cd cat echo nano python g++ ... (TAB completes)\n", TC_DIM);
    term_print(t, "TAB completes commands and paths - TAB TAB lists candidates.\n", TC_DIM);
    term_print(t, "Ctrl+C cancels a line (^C) - Ctrl+L clears - Up/Down = history.\n", TC_DIM);
    term_print(t, "pipes: ls | wc   redirection: echo hi > out.txt   (RAM fs)\n", TC_DIM);
    term_print(t, "HONEST: the filesystem is RAM-only and is lost on reboot.\n", TC_DIM);
    if (!g_term_milestone) {
        g_term_milestone = true;
        kian_serial_write("GUI: advanced terminal initialized\n", 35);
    }
    return a;
}

void term_destroy(void *inst) {
    TermApp *a = (TermApp *)inst;
    for (int i = 0; i < a->ntabs; i++) tab_close_extra(&a->tab[i]);
    delete a;
}
