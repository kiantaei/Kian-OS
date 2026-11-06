// Kian OS GUI - Kian Terminal command registry + implementations.
// Every command is a real implementation over the RAM filesystem / kernel
// ABI. Bounded memory everywhere. The registry uses function pointers, so
// the dispatcher stays tiny (no giant switch). HONEST: python is a real
// (small) interpreter, g++ is an honest "not available", the filesystem
// is RAM-only.
#include "terminal.h"
#include "../apps.h"
#include "../gui.h"
#include "../ramfs.h"
#include "../theme.h"
#include "../emoji.h"
#include "../util.h"

// ---------------- output helpers ----------------

void cenv_out(CmdEnv *e, const char *s) { cenv_outc(e, s, TC_FG); }

void cenv_outc(CmdEnv *e, const char *s, int tag) {
    if (e->capture) {
        // bounded capture for pipes/redirection (8 KB per stage)
        if (e->cap && e->cap->length() < 8192) e->cap->append(s);
        return;
    }
    term_print(e->tab, s, tag);
}

void cenv_outnum(CmdEnv *e, long long v) {
    char tmp[24];
    int i = 0;
    unsigned long long m = (v < 0) ? (unsigned long long)(-v) : (unsigned long long)v;
    if (v < 0) tmp[i++] = '-';
    if (m == 0) tmp[i++] = '0';
    while (m > 0 && i < 22) { tmp[i++] = (char)('0' + (m % 10)); m /= 10; }
    tmp[i] = '\0';
    for (int a = 0, b = i - 1; a < b; a++, b--) { char t = tmp[a]; tmp[a] = tmp[b]; tmp[b] = t; }
    cenv_outc(e, tmp, TC_FG);
}

static void out_str(CmdEnv *e, const char *s, int tag) { cenv_outc(e, s, tag); }

// ---------------- path resolution (with . and ..) ----------------

static int split_path(const char *s, char parts[][64], int maxp) {
    int np = 0, i = 0;
    while (s[i] != '\0') {
        while (s[i] == '/') i++;
        if (s[i] == '\0') break;
        char buf[64];
        int j = 0;
        while (s[i] != '\0' && s[i] != '/' && j < 63) buf[j++] = s[i++];
        buf[j] = '\0';
        if (np < maxp) {
            int k = 0;
            for (; buf[k] != '\0' && k < 63; k++) parts[np][k] = buf[k];
            parts[np][k] = '\0';
            np++;
        }
    }
    return np;
}

void term_resolve_path(const char *cwd, const char *arg, char *out, int maxn) {
    char parts[24][64];
    int np = 0;
    if (arg[0] != '/' && arg[0] != '\0') {
        np = split_path(cwd, parts, 24);
    }
    char aparts[24][64];
    int an = split_path(arg, aparts, 24);
    for (int i = 0; i < an && np < 24; i++) {
        if (strcmp(aparts[i], ".") == 0) continue;
        if (strcmp(aparts[i], "..") == 0) { if (np > 0) np--; continue; }
        int k = 0;
        for (; aparts[i][k] != '\0' && k < 63; k++) parts[np][k] = aparts[i][k];
        parts[np][k] = '\0';
        np++;
    }
    int ol = 0;
    out[ol++] = '/';
    for (int i = 0; i < np && ol < maxn - 2; i++) {
        if (i > 0) out[ol++] = '/';
        for (int k = 0; parts[i][k] != '\0' && ol < maxn - 2; k++) out[ol++] = parts[i][k];
    }
    out[ol] = '\0';
}

// split "a/b/c.txt" into dir "/a/b" + base "c.txt"
static void split_dir_base(const char *path, char *dir, int dcap, char *base, int bcap) {
    int last = -1;
    for (int i = 0; path[i] != '\0'; i++) if (path[i] == '/') last = i;
    int d = 0;
    for (int i = 0; i <= last && d < dcap - 1; i++) dir[d++] = path[i];
    dir[d] = '\0';
    if (d == 0) { dir[0] = '/'; dir[1] = '\0'; }
    int b = 0;
    for (int i = last + 1; path[i] != '\0' && b < bcap - 1; i++) base[b++] = path[i];
    base[b] = '\0';
}

// ---------------- the commands ----------------

static void cmd_help(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, "Kian Terminal commands (TAB completes them all):\n", TC_DIM);
    for (int i = 0; i < term_cmd_count(); i++) {
        const CmdDef *c = term_cmd_at(i);
        out_str(e, "  ", TC_DIM);
        out_str(e, c->name, TC_PROMPT);
        out_str(e, "\n", TC_DIM);
    }
    out_str(e,
        "pipes & redirection: a | b   a < in   a > out   a >> out (RAM fs)\n"
        "TAB completes commands and paths (TAB TAB lists candidates).\n"
        "Ctrl+C cancels a line (^C), Ctrl+L clears, Up/Down = history.\n",
        TC_DIM);
}

static void cmd_clear(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!e->capture) { e->tab->rhead = 0; e->tab->rcount = 0; e->tab->scroll = 0; }
}

static void cmd_echo(CmdEnv *e, int argc, char **argv) {
    bool nonl = false;
    int start = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) { nonl = true; start = 2; }
    for (int i = start; i < argc; i++) {
        if (i > start) out_str(e, " ", TC_FG);
        out_str(e, argv[i], TC_FG);
    }
    if (!nonl) out_str(e, "\n", TC_FG);
    // echo with no args passes stdin through (pipeline support)
    if (argc <= 1 && e->stdin_text[0] != '\0') out_str(e, e->stdin_text, TC_FG);
}

static void cmd_pwd(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, e->cwd, TC_FG);
    out_str(e, "\n", TC_FG);
}

static void cmd_whoami(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, "kian\n", TC_USER);
}

static void cmd_hostname(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, "kian-os\n", TC_FG);
}

static void cmd_uname(CmdEnv *e, int argc, char **argv) {
    bool all = false;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "-a") == 0) all = true;
    if (all) {
        out_str(e, "KianOS kian-os 0.3.0 Horizon #1 x86_64 Rust kernel\n", TC_FG);
    } else {
        out_str(e, "KianOS 0.3.0 x86_64 (Rust kernel)\n", TC_FG);
    }
}

static void cmd_date(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
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
    out_str(e, d, TC_FG);
    out_str(e, " Asia/Tehran (UTC+03:30, no DST)\n", TC_DIM);
}

static void cmd_uptime(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned long long s = kian_uptime_ms() / 1000;
    out_str(e, "up ", TC_FG);
    cenv_outnum(e, (long long)(s / 3600));
    out_str(e, "h ", TC_FG);
    cenv_outnum(e, (long long)((s / 60) % 60));
    out_str(e, "m ", TC_FG);
    cenv_outnum(e, (long long)(s % 60));
    out_str(e, "s\n", TC_FG);
}

static void cmd_mem(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t used = 0, total = 0;
    kian_mem_stats(&used, &total);
    out_str(e, "heap: ", TC_FG);
    cenv_outnum(e, (long long)(used >> 10));
    out_str(e, " KiB used of ", TC_FG);
    cenv_outnum(e, (long long)(total >> 20));
    out_str(e, " MiB total\n", TC_FG);
    const kian_boot_info_t *bi = kian_boot_info();
    uint64_t us = 0;
    uint32_t regions = 0;
    for (uint32_t i = 0; i < bi->mem_count; i++) {
        if (bi->mem[i].usable) { us += bi->mem[i].length; regions++; }
    }
    out_str(e, "firmware: ", TC_DIM);
    cenv_outnum(e, (long long)regions);
    out_str(e, " usable region(s), ", TC_DIM);
    cenv_outnum(e, (long long)(us >> 20));
    out_str(e, " MiB\n", TC_DIM);
}

static void cmd_ls(CmdEnv *e, int argc, char **argv) {
    char path[160];
    if (argc > 1) term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    else term_resolve_path(e->cwd, "", path, sizeof(path));
    RNode *dir = ramfs_resolve(path);
    if (!dir) { out_str(e, "ls: no such file or directory\n", TC_ERR); return; }
    if (!ramfs_is_dir(dir)) {
        out_str(e, path, TC_FG); out_str(e, "\n", TC_FG);
        return;
    }
    RNode *items[64];
    int n = ramfs_list(dir, items, 64);
    for (int i = 0; i < n; i++) {
        if (ramfs_is_dir(items[i])) {
            out_str(e, items[i]->name, TC_PATH);
            out_str(e, "/\n", TC_PATH);
        } else {
            out_str(e, items[i]->name, TC_FG);
            out_str(e, "\n", TC_FG);
        }
    }
}

static void cmd_cd(CmdEnv *e, int argc, char **argv) {
    char path[160];
    if (argc <= 1 || strcmp(argv[1], "~") == 0) {
        // cd with no args -> home (logical Kian OS home, RAM fs)
        RNode *home = ramfs_resolve("/home/kian");
        if (home && ramfs_is_dir(home)) {
            ramfs_path(home, e->cwd, 128);
            if (!e->capture && e->tab) strcpy(e->tab->cwd, e->cwd);
        } else {
            e->cwd[0] = '/'; e->cwd[1] = '\0';
            if (!e->capture && e->tab) strcpy(e->tab->cwd, e->cwd);
        }
        return;
    }
    term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    RNode *d = ramfs_resolve(path);
    if (!d || !ramfs_is_dir(d)) { out_str(e, "cd: no such directory: ", TC_ERR); out_str(e, argv[1], TC_ERR); out_str(e, "\n", TC_ERR); return; }
    ramfs_path(d, e->cwd, 128);
    if (!e->capture && e->tab) strcpy(e->tab->cwd, e->cwd);
}

static void cmd_cat(CmdEnv *e, int argc, char **argv) {
    if (argc <= 1) {
        // no file: pass stdin through (pipelines)
        if (e->stdin_text[0] != '\0') out_str(e, e->stdin_text, TC_FG);
        return;
    }
    for (int a = 1; a < argc; a++) {
        char path[160];
        term_resolve_path(e->cwd, argv[a], path, sizeof(path));
        RNode *f = ramfs_resolve(path);
        if (!f || ramfs_is_dir(f)) {
            out_str(e, "cat: no such file: ", TC_ERR);
            out_str(e, argv[a], TC_ERR);
            out_str(e, "\n", TC_ERR);
            continue;
        }
        out_str(e, ramfs_read(f), TC_FG);
        if (!e->capture) out_str(e, "\n", TC_FG);
    }
}

static void cmd_mkdir(CmdEnv *e, int argc, char **argv) {
    if (argc <= 1) { out_str(e, "usage: mkdir <dir>\n", TC_WARN); return; }
    char path[160], dir[160], base[64];
    term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    split_dir_base(path, dir, sizeof(dir), base, sizeof(base));
    RNode *pd = ramfs_resolve(dir);
    if (!pd || !ramfs_is_dir(pd)) { out_str(e, "mkdir: parent directory missing\n", TC_ERR); return; }
    if (!ramfs_mkdir(pd, base)) { out_str(e, "mkdir: failed (exists or filesystem full)\n", TC_ERR); }
}

static void cmd_touch(CmdEnv *e, int argc, char **argv) {
    if (argc <= 1) { out_str(e, "usage: touch <file>\n", TC_WARN); return; }
    char path[160], dir[160], base[64];
    term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    split_dir_base(path, dir, sizeof(dir), base, sizeof(base));
    RNode *pd = ramfs_resolve(dir);
    if (!pd || !ramfs_is_dir(pd)) { out_str(e, "touch: parent directory missing\n", TC_ERR); return; }
    if (!ramfs_find(pd, base) && !ramfs_create(pd, base, "")) {
        out_str(e, "touch: failed\n", TC_ERR);
    }
}

static bool rdelete_recursive(RNode *n, int depth) {
    if (!n || depth > 6) return false;
    if (ramfs_is_dir(n)) {
        RNode *items[16];
        for (;;) {
            int cnt = ramfs_list(n, items, 16);
            if (cnt == 0) break;
            bool stuck = true;
            for (int i = 0; i < cnt; i++) {
                if (rdelete_recursive(items[i], depth + 1)) { stuck = false; }
            }
            if (stuck) return false;
        }
        return ramfs_delete(n);
    }
    return ramfs_delete(n);
}

static void cmd_rm(CmdEnv *e, int argc, char **argv) {
    bool rec = false;
    const char *target = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rf") == 0) rec = true;
        else target = argv[i];
    }
    if (!target) { out_str(e, "usage: rm [-r] <path>\n", TC_WARN); return; }
    char path[160];
    term_resolve_path(e->cwd, target, path, sizeof(path));
    RNode *n = ramfs_resolve(path);
    if (!n || n == ramfs_root() || n == ramfs_trash()) { out_str(e, "rm: no such file: ", TC_ERR); out_str(e, target, TC_ERR); out_str(e, "\n", TC_ERR); return; }
    if (ramfs_is_dir(n)) {
        if (!rec) { out_str(e, "rm: is a directory (use rm -r)\n", TC_WARN); return; }
        if (!rdelete_recursive(n, 0)) out_str(e, "rm: could not remove directory fully\n", TC_ERR);
        return;
    }
    if (!ramfs_trash_node(n)) out_str(e, "rm: failed\n", TC_ERR);
}

static void cmd_cp(CmdEnv *e, int argc, char **argv) {
    if (argc <= 2) { out_str(e, "usage: cp <src> <dst>\n", TC_WARN); return; }
    char sp[160], dp[160], sdir[160], sbase[64], ddir[160], dbase[64];
    term_resolve_path(e->cwd, argv[1], sp, sizeof(sp));
    term_resolve_path(e->cwd, argv[2], dp, sizeof(dp));
    RNode *src = ramfs_resolve(sp);
    if (!src || ramfs_is_dir(src)) { out_str(e, "cp: source must be a file\n", TC_ERR); return; }
    RNode *dst = ramfs_resolve(dp);
    RNode *parent;
    const char *name;
    if (dst && ramfs_is_dir(dst)) {
        parent = dst;
        split_dir_base(sp, sdir, sizeof(sdir), sbase, sizeof(sbase));
        name = sbase;
        if (ramfs_find(parent, name)) { out_str(e, "cp: target exists\n", TC_ERR); return; }
    } else {
        split_dir_base(dp, ddir, sizeof(ddir), dbase, sizeof(dbase));
        parent = ramfs_resolve(ddir);
        name = dbase;
        if (!parent || !ramfs_is_dir(parent)) { out_str(e, "cp: target directory missing\n", TC_ERR); return; }
        if (ramfs_find(parent, name)) { out_str(e, "cp: target exists\n", TC_ERR); return; }
    }
    if (!ramfs_create(parent, name, ramfs_read(src))) out_str(e, "cp: failed (filesystem full?)\n", TC_ERR);
}

static void cmd_mv(CmdEnv *e, int argc, char **argv) {
    if (argc <= 2) { out_str(e, "usage: mv <src> <dst>\n", TC_WARN); return; }
    char sp[160], dp[160], ddir[160], dbase[64], sdir[160], sbase[64];
    term_resolve_path(e->cwd, argv[1], sp, sizeof(sp));
    term_resolve_path(e->cwd, argv[2], dp, sizeof(dp));
    RNode *src = ramfs_resolve(sp);
    if (!src || src == ramfs_root() || src == ramfs_trash()) { out_str(e, "mv: no such file\n", TC_ERR); return; }
    RNode *dst = ramfs_resolve(dp);
    if (dst && ramfs_is_dir(dst)) {
        // move into the directory, keeping the name
        if (!ramfs_move(src, dst)) out_str(e, "mv: failed (exists or full)\n", TC_ERR);
        return;
    }
    split_dir_base(sp, sdir, sizeof(sdir), sbase, sizeof(sbase));
    split_dir_base(dp, ddir, sizeof(ddir), dbase, sizeof(dbase));
    RNode *pdir = ramfs_resolve(ddir);
    if (!pdir || !ramfs_is_dir(pdir)) { out_str(e, "mv: target directory missing\n", TC_ERR); return; }
    bool same_parent = (pdir == src->parent);
    if (same_parent) {
        if (!ramfs_rename(src, dbase)) out_str(e, "mv: rename failed\n", TC_ERR);
    } else {
        if (!ramfs_move(src, pdir)) { out_str(e, "mv: move failed\n", TC_ERR); return; }
        if (!ramfs_rename(src, dbase)) out_str(e, "mv: renamed path only (name busy)\n", TC_WARN);
    }
}

static void tree_walk(CmdEnv *e, RNode *dir, const char *prefix, int depth, int *budget) {
    if (depth > 3 || *budget <= 0) return;
    RNode *items[16];
    int n = ramfs_list(dir, items, 16);
    for (int i = 0; i < n && *budget > 0; i++) {
        bool last = (i == n - 1);
        out_str(e, prefix, TC_DIM);
        out_str(e, last ? "`-- " : "|-- ", TC_DIM);
        if (ramfs_is_dir(items[i])) {
            out_str(e, items[i]->name, TC_PATH);
            out_str(e, "/\n", TC_PATH);
            (*budget)--;
            char pfx[80];
            int k = 0;
            for (; prefix[k] != '\0' && k < 60; k++) pfx[k] = prefix[k];
            for (const char *s = last ? "    " : "|   "; *s && k < 76; s++) pfx[k++] = *s;
            pfx[k] = '\0';
            tree_walk(e, items[i], pfx, depth + 1, budget);
        } else {
            out_str(e, items[i]->name, TC_FG);
            out_str(e, "\n", TC_FG);
            (*budget)--;
        }
    }
}

static void cmd_tree(CmdEnv *e, int argc, char **argv) {
    char path[160];
    if (argc > 1) term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    else term_resolve_path(e->cwd, "", path, sizeof(path));
    RNode *d = ramfs_resolve(path);
    if (!d || !ramfs_is_dir(d)) { out_str(e, "tree: no such directory\n", TC_ERR); return; }
    out_str(e, path, TC_PATH);
    out_str(e, "\n", TC_PATH);
    int budget = 64;
    tree_walk(e, d, "", 0, &budget);
    out_str(e, "(", TC_DIM);
    cenv_outnum(e, 64 - budget);
    out_str(e, " entries, depth 3 max)\n", TC_DIM);
}

static void cmd_history(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    int total = e->hist_n;
    for (int i = 0; i < total; i++) {
        int idx = (e->hist_head - total + i + TERM_HIST * 2) % TERM_HIST;
        cenv_outnum(e, i + 1);
        out_str(e, "  ", TC_FG);
        out_str(e, e->hist[idx], TC_FG);
        out_str(e, "\n", TC_FG);
    }
}

static void cmd_grep(CmdEnv *e, int argc, char **argv) {
    bool icase = false;
    const char *pat = 0, *file = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) icase = true;
        else if (!pat) pat = argv[i];
        else if (!file) file = argv[i];
    }
    if (!pat) { out_str(e, "usage: grep [-i] <pattern> [file]\n", TC_WARN); return; }
    char upper[128];
    if (icase) {
        int k = 0;
        for (; pat[k] != '\0' && k < 120; k++) upper[k] = (pat[k] >= 'A' && pat[k] <= 'Z') ? pat[k] + 32 : pat[k];
        upper[k] = '\0';
        pat = upper;
    }
    const char *text = e->stdin_text;
    if (file) {
        char path[160];
        term_resolve_path(e->cwd, file, path, sizeof(path));
        RNode *f = ramfs_resolve(path);
        if (!f || ramfs_is_dir(f)) { out_str(e, "grep: no such file\n", TC_ERR); return; }
        text = ramfs_read(f);
    }
    if (text[0] == '\0') return;
    // walk lines, print matches (bounded line length like the terminal)
    char line[256];
    int li = 0, nline = 0, nmatch = 0;
    for (int i = 0; ; i++) {
        char c = text[i];
        if (c == '\n' || c == '\0') {
            line[li] = '\0';
            nline++;
            bool match = false;
            for (int k = 0; line[k] != '\0'; k++) {
                int m = 0;
                while (pat[m] != '\0') {
                    char a = line[k + m], b = pat[m];
                    if (icase) { if (a >= 'A' && a <= 'Z') a += 32; }
                    if (a != b) break;
                    m++;
                }
                if (pat[m] == '\0') { match = true; break; }
            }
            if (match) {
                cenv_outnum(e, nline);
                out_str(e, ": ", TC_DIM);
                out_str(e, line, TC_FG);
                out_str(e, "\n", TC_FG);
                nmatch++;
            }
            li = 0;
            if (c == '\0') break;
        } else if (li < 255) {
            line[li++] = c;
        }
    }
    if (nmatch == 0) out_str(e, "(no matches)\n", TC_DIM);
}

static void cmd_wc(CmdEnv *e, int argc, char **argv) {
    const char *text = e->stdin_text;
    char base[64];
    base[0] = '\0';
    if (argc > 1) {
        char path[160], ddir[160];
        term_resolve_path(e->cwd, argv[1], path, sizeof(path));
        RNode *f = ramfs_resolve(path);
        if (!f || ramfs_is_dir(f)) { out_str(e, "wc: no such file\n", TC_ERR); return; }
        text = ramfs_read(f);
        split_dir_base(path, ddir, sizeof(ddir), base, sizeof(base));
    }
    long long lines = 0, words = 0, bytes = 0;
    bool inword = false;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        bytes++;
        if (c == '\n') lines++;
        if (c == ' ' || c == '\n' || c == '\t') inword = false;
        else if (!inword) { inword = true; words++; }
    }
    if (bytes > 0 && text[bytes - 1] != '\n') lines++;
    cenv_outnum(e, lines);
    out_str(e, " ", TC_FG);
    cenv_outnum(e, words);
    out_str(e, " ", TC_FG);
    cenv_outnum(e, bytes);
    if (base[0] != '\0') { out_str(e, " ", TC_FG); out_str(e, base, TC_FG); }
    out_str(e, "\n", TC_FG);
}

// ---------------- neofetch ----------------

static const char *fetch_pad(int row) {
    return (row == 0) ? "   /\\_/\\     "
         : (row == 1) ? "  ( o.o )    "
         : (row == 2) ? "   > ^ <     "
         : "             ";
}

static void fetch_line(CmdEnv *e, int row, const char *label, const char *value) {
    const char *pad = fetch_pad(row);
    char full[TL_LEN + 8];
    int k = 0;
    for (; *pad != '\0' && k < TL_LEN - 2; pad++) full[k++] = *pad;
    for (const char *p = label; *p != '\0' && k < TL_LEN - 2; p++) full[k++] = *p;
    int split2 = k;   // pad + label share the accent color
    for (const char *p = value; *p != '\0' && k < TL_LEN - 2; p++) full[k++] = *p;
    full[k] = '\0';
    if (!e->capture) {
        term_print_split(e->tab, full, TC_FG, split2, TC_ACCENT);
    } else if (e->cap) {
        e->cap->append(full);
        e->cap->append("\n");
    }
}

static void cmd_neofetch(CmdEnv *e, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            out_str(e, "neofetch (Kian OS) 0.3.0\n", TC_FG);
            return;
        }
    }
    // header row: cat face (accent) + user@host (green), one scrollback line
    if (!e->capture) term_print_split(e->tab, "   /\\_/\\      kian@kian-os", TC_USER, 13, TC_ACCENT);
    else if (e->cap) e->cap->append("   /\\_/\\      kian@kian-os\n");
    fetch_line(e, 1, "--------------", "");

    char v1[TL_LEN + 8];

    fetch_line(e, 2, "OS: ", "Kian OS 0.3.0 \"Horizon\"");
    fetch_line(e, 3, "Kernel: ", "Rust (x86_64-unknown-none)");
    fetch_line(e, 4, "Shell: ", "ksh + Kian Terminal");
    fetch_line(e, 5, "Terminal: ", "Kian Terminal");
    // Resolution (real framebuffer)
    if (GFX) {
        int k = 0;
        v1[0] = '\0';
        put_num(v1, k, GFX->w); v1[k++] = 'x';
        put_num(v1, k, GFX->h); v1[k] = '\0';
        fetch_line(e, 6, "Resolution: ", v1);
    } else {
        fetch_line(e, 6, "Resolution: ", "Unknown");
    }
    // Uptime (real)
    unsigned long long s = kian_uptime_ms() / 1000;
    {
        char up[TL_LEN + 8];
        int k = 0;
        up[0] = '\0';
        put_num(up, k, (int)(s / 3600)); up[k++] = 'h';
        put_num(up, k, (int)((s / 60) % 60)); up[k++] = 'm';
        put_num(up, k, (int)(s % 60)); up[k] = '\0';
        fetch_line(e, 7, "Uptime: ", up);
    }
    // Memory (real)
    uint64_t used = 0, total = 0;
    kian_mem_stats(&used, &total);
    {
        char mems[64];
        int k = 0;
        mems[0] = '\0';
        put_num(mems, k, (int)(used >> 20)); mems[k++] = '/';
        put_num(mems, k, (int)(total >> 20)); mems[k++] = '\0';
        const char *mib = " MiB";
        for (int m = 0; mib[m] != '\0' && k < 62; m++) mems[k++] = mib[m];
        mems[k] = '\0';
        fetch_line(e, 8, "Memory: ", mems);
    }
    // Desktop + theme + accent (real theme state)
    bool dark = (THEME->wallpaper_top == 0x001A2B4A);
    fetch_line(e, 9, "Desktop: ", "Kian OS Desktop (Win11-style)");
    fetch_line(e, 10, "Theme: ", dark ? "Dark" : "Light");
    if (THEME->accent == 0x0078D4) {
        fetch_line(e, 11, "Accent: ", "Blue (#0078D4)");
    } else {
        // accent color as hex - real value from the theme
        char acc[32];
        int k = 0;
        acc[0] = '\0';
        const char *p = "Accent #";
        for (; p[k] != '\0'; k++) acc[k] = p[k];
        uint32_t a = THEME->accent & 0xFFFFFF;
        char hx[8];
        int hn = 0;
        if (a == 0) hx[hn++] = '0';
        while (a > 0 && hn < 7) {
            int d = a % 16;
            hx[hn++] = (d < 10) ? (char)('0' + d) : (char)('a' + d - 10);
            a /= 16;
        }
        for (int q = hn - 1; q >= 0 && k < 30; q--) acc[k++] = hx[q];
        acc[k] = '\0';
        fetch_line(e, 11, "", acc);
    }
}

// ---------------- preinstalled dev tools ----------------

// callback thunk: the python engine emits through a void* context
static void py_out_thunk(void *ud, const char *s, int tag) {
    cenv_outc((CmdEnv *)ud, s, tag);
}

static void cmd_python(CmdEnv *e, int argc, char **argv) {
    // --version
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            out_str(e, py_banner_line(0), TC_FG);
            out_str(e, "\n", TC_FG);
            return;
        }
    }
    if (argc > 1) {
        // script mode: really run the file through the interpreter
        char path[160];
        term_resolve_path(e->cwd, argv[1], path, sizeof(path));
        RNode *f = ramfs_resolve(path);
        if (!f || ramfs_is_dir(f)) {
            out_str(e, "python: can't open file '", TC_ERR);
            out_str(e, argv[1], TC_ERR);
            out_str(e, "'\n", TC_ERR);
            return;
        }
        void *pi = py_new();
        if (!pi) { out_str(e, "python: out of memory\n", TC_ERR); return; }
        py_set_repl(pi, false);   // script mode: blank lines do not close blocks
        const char *src = ramfs_read(f);
        char line[256];
        int li = 0;
        int rc = PY_DONE;
        for (int i = 0; ; i++) {
            char c = src[i];
            if (c == '\n' || c == '\0') {
                line[li] = '\0';
                rc = py_line(pi, line, py_out_thunk, e);
                li = 0;
                if (rc == PY_ERR || rc == PY_EXIT) break;
                if (c == '\0') break;
            } else if (li < 255) {
                line[li++] = c;
            }
        }
        py_free(pi);
        return;
    }
    // interactive REPL: the terminal app switches this tab's mode
    if (e->capture || !e->tab) { out_str(e, "python: interactive mode needs a real terminal\n", TC_WARN); return; }
    if (e->tab->py) py_free(e->tab->py);
    e->tab->py = py_new();
    if (!e->tab->py) { out_str(e, "python: out of memory\n", TC_ERR); return; }
    e->tab->mode = TM_PYTHON;
    e->tab->py_len = 0;
    e->tab->py_caret = 0;
    e->tab->py_input[0] = '\0';
    out_str(e, py_banner_line(0), TC_ACCENT);
    out_str(e, "\n", TC_ACCENT);
    out_str(e, py_banner_line(1), TC_DIM);
    out_str(e, "\n", TC_DIM);
    out_str(e, py_banner_line(2), TC_DIM);
    out_str(e, "\n", TC_DIM);
}

static void cmd_gpp(CmdEnv *e, int argc, char **argv) {
    bool ver = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) ver = true;
    }
    if (ver) {
        out_str(e, "g++ (Kian OS) 0.0.0-native-none -- REAL COMPILER NOT PORTED\n", TC_WARN);
        return;
    }
    out_str(e, "g++: a native C++ compiler is NOT available in Kian OS.\n", TC_WARN);
    out_str(e, "A real g++ needs an executable loader, processes, an\n", TC_DIM);
    out_str(e, "assembler + linker and a C++ runtime. The kernel does not\n", TC_DIM);
    out_str(e, "provide those yet, so nothing was compiled and nothing\n", TC_DIM);
    out_str(e, "was faked.\n", TC_DIM);
    out_str(e, "missing: exe loader, process creation, linker, libstdc++ subset\n", TC_DIM);
}

static void cmd_nano(CmdEnv *e, int argc, char **argv) {
    if (e->capture || !e->tab) { out_str(e, "nano: needs a real terminal\n", TC_WARN); return; }
    char path[160];
    if (argc > 1) term_resolve_path(e->cwd, argv[1], path, sizeof(path));
    else term_resolve_path(e->cwd, "untitled.txt", path, sizeof(path));
    int err = 0;
    void *n = nano_open(path, &err);
    if (!n) {
        out_str(e, "nano: cannot open ", TC_ERR);
        out_str(e, path, TC_ERR);
        out_str(e, "\n", TC_ERR);
        return;
    }
    e->tab->nano = n;
    e->tab->mode = TM_NANO;
}

static void cmd_reboot(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, "Rebooting...\n", TC_WARN);
    kian_reboot();
}

static void cmd_shutdown(CmdEnv *e, int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(e, "Powering off...\n", TC_WARN);
    kian_shutdown();
}

static void cmd_exit(CmdEnv *e, int argc, char **argv) {
    (void)e; (void)argc; (void)argv;
    shell_close_focused();  // closes this terminal window (real effect)
}

// ---------------- GUI app launchers (real: raise / open windows) -------

static void cmd_xenox(CmdEnv *e, int argc, char **argv)    { (void)e; (void)argc; (void)argv; app_open(APP_XENOX); }
static void cmd_terminal(CmdEnv *e, int argc, char **argv) { (void)e; (void)argc; (void)argv; app_open(APP_TERM); }
static void cmd_explorer(CmdEnv *e, int argc, char **argv) { (void)e; (void)argc; (void)argv; app_open(APP_FILES); }
static void cmd_notepad(CmdEnv *e, int argc, char **argv)  { (void)e; (void)argc; (void)argv; app_open(APP_NOTES); }
static void cmd_calculator(CmdEnv *e, int argc, char **argv) { (void)e; (void)argc; (void)argv; app_open(APP_CALC); }
static void cmd_settings(CmdEnv *e, int argc, char **argv) { (void)e; (void)argc; (void)argv; app_open(APP_SETTINGS); }
static void cmd_taskmgr(CmdEnv *e, int argc, char **argv)  { (void)e; (void)argc; (void)argv; app_open(APP_TASKMGR); }
static void cmd_paint(CmdEnv *e, int argc, char **argv)    { (void)e; (void)argc; (void)argv; app_open(APP_PAINT); }

// ---------------- registry ----------------

static const CmdDef REGISTRY[] = {
    { "help",      cmd_help },
    { "neofetch",  cmd_neofetch },
    { "ls",        cmd_ls },
    { "cd",        cmd_cd },
    { "pwd",       cmd_pwd },
    { "cat",       cmd_cat },
    { "echo",      cmd_echo },
    { "mkdir",     cmd_mkdir },
    { "touch",     cmd_touch },
    { "rm",        cmd_rm },
    { "cp",        cmd_cp },
    { "mv",        cmd_mv },
    { "tree",      cmd_tree },
    { "grep",      cmd_grep },
    { "wc",        cmd_wc },
    { "history",   cmd_history },
    { "clear",     cmd_clear },
    { "date",      cmd_date },
    { "uptime",    cmd_uptime },
    { "mem",       cmd_mem },
    { "whoami",    cmd_whoami },
    { "hostname",  cmd_hostname },
    { "uname",     cmd_uname },
    { "python",    cmd_python },
    { "python3",   cmd_python },
    { "g++",       cmd_gpp },
    { "nano",      cmd_nano },
    { "xenox",     cmd_xenox },
    { "terminal",  cmd_terminal },
    { "explorer",  cmd_explorer },
    { "notepad",   cmd_notepad },
    { "calculator", cmd_calculator },
    { "settings",  cmd_settings },
    { "taskmgr",   cmd_taskmgr },
    { "paint",     cmd_paint },
    { "reboot",    cmd_reboot },
    { "shutdown",  cmd_shutdown },
    { "exit",      cmd_exit }
};

const CmdDef *term_cmd_find(const char *name) {
    for (int i = 0; i < (int)(sizeof(REGISTRY) / sizeof(REGISTRY[0])); i++) {
        if (strcmp(REGISTRY[i].name, name) == 0) return &REGISTRY[i];
    }
    return 0;
}

const CmdDef *term_cmd_at(int i) {
    if (i < 0 || i >= (int)(sizeof(REGISTRY) / sizeof(REGISTRY[0]))) return 0;
    return &REGISTRY[i];
}

int term_cmd_count() { return (int)(sizeof(REGISTRY) / sizeof(REGISTRY[0])); }
