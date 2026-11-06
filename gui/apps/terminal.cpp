// Kian OS GUI - Terminal app: scrolling output, line editor, commands on the
// RAM filesystem (RAM only, lost on reboot).
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../input.h"
#include "../util.h"

struct TermApp {
    Str buf;            // scrollback
    char line[96];
    int llen;
    char cwd[128];
};

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
        term_out(t, "commands: help clear echo date uptime mem whoami uname\n"
                    "          ls cd cat mkdir touch rm exit\n");
    } else if (strcmp(cmd, "clear") == 0) {
        t->buf.clear();
    } else if (strcmp(cmd, "echo") == 0) {
        term_out(t, arg);
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
}

void *term_create() {
    TermApp *t = new TermApp();
    t->cwd[0] = '/';
    t->cwd[1] = '\0';
    t->line[0] = '\0';
    t->llen = 0;
    term_out(t, "Kian OS terminal - type 'help'. 'exit' closes.\n");
    return t;
}

void term_destroy(void *inst) {
    TermApp *t = (TermApp *)inst;
    delete t;
}

void term_draw(void *inst, Rect r) {
    TermApp *t = (TermApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->input_bg);
    int maxlines = r.h / 10;
    if (maxlines < 2) return;
    // draw last (maxlines-1) lines of the buffer, bottom-aligned
    int total = 1;
    for (int i = 0; i < t->buf.length(); i++) {
        if (t->buf[i] == '\n') total++;
    }
    int skip = total - (maxlines - 1);
    if (skip < 0) skip = 0;
    char line[128];
    int y = r.y + 4;
    int lineno = 0, start = 0, drawn = 0;
    for (int i = 0; i <= t->buf.length() && drawn < maxlines - 1; i++) {
        bool nl = (i == t->buf.length()) || (t->buf[i] == '\n');
        if (nl) {
            if (lineno >= skip) {
                int len = i - start;
                if (len > 120) len = 120;
                for (int k = 0; k < len; k++) line[k] = t->buf[start + k];
                line[len] = '\0';
                draw_text(r.x + 4, y, line, THEME->text, 1);
                y += 10;
                drawn++;
            }
            lineno++;
            start = i + 1;
        }
    }
    // prompt + input line + caret
    char pl[180];
    int k = 0;
    const char *p1 = "kian@kian-os:";
    for (; p1[k] != '\0' && k < 170; k++) pl[k] = p1[k];
    for (int m = 0; t->cwd[m] != '\0' && k < 170; m++) pl[k++] = t->cwd[m];
    if (k < 170) pl[k++] = '$';
    if (k < 170) pl[k++] = ' ';
    for (int m = 0; t->line[m] != '\0' && k < 170; m++) pl[k++] = t->line[m];
    pl[k] = '\0';
    draw_text(r.x + 4, y, pl, THEME->text, 1);
    if (((now_ms() / 400) & 1) == 0) {
        vline(r.x + 6 + text_width(pl, 1), y, 9, THEME->accent);
    }
}

void term_event(void *inst, const kian_event_t *e, Rect r) {
    (void)r;
    TermApp *t = (TermApp *)inst;
    if (e->type != KIAN_EV_KEY_DOWN) return;
    char ch = (char)e->b;
    if (ch == '\n') {
        t->line[t->llen] = '\0';
        term_exec(t);
    } else if (ch == 8) {
        if (t->llen > 0) t->line[--t->llen] = '\0';
    } else if (ch >= 32 && ch < 127 && t->llen < 94) {
        t->line[t->llen++] = ch;
        t->line[t->llen] = '\0';
    }
}
