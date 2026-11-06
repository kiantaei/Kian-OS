// Kian OS GUI - Kian Terminal: shared types for the terminal subsystem.
//
// Files:
//   terminal.h           shared structs + color tags (this file)
//   terminal.cpp         app UI: tabs, scrollback rendering, editing,
//                        TAB completion, shortcuts, Ctrl+C/^C
//   terminal_parser.*    line parser: quoting, escapes, | pipes, < > >>
//   terminal_commands.*  command registry (function pointers) + all commands
//   terminal_python.*    Kian mini-python (real Python-subset interpreter)
//   terminal_nano.*      nano-like full-screen editor
#pragma once
#include <kian_abi.h>
#include "../gfx.h"
#include "../util.h"

// ---- semantic color tags (mapped to real RGB by term_color() in terminal.cpp,
// dark and light theme aware) ----
enum {
    TC_BG = 0,     // terminal background (#0C0C0C dark / paper light)
    TC_FG,         // normal text        (#D4D4D4 dark)
    TC_DIM,        // secondary text
    TC_USER,       // username green     (#4ADE80)
    TC_PATH,       // path blue          (#60A5FA)
    TC_PROMPT,     // prompt cyan        (#4CC2FF)
    TC_ERR,        // error red          (#F87171)
    TC_WARN,       // warning amber      (#FBBF24)
    TC_OK,         // success green
    TC_ACCENT      // theme accent
};

// one scrollback line (input is wrapped at insert time, like a real terminal)
const int TL_LEN = 100;
struct TLine {
    char t[TL_LEN + 1];
    uint8_t col;      // color of the text after `split`
    uint8_t col2;     // color of the first `split` chars (prompt echo)
    uint8_t split;    // 0 = single color line
};

const int TL_RING = 256;          // scrollback ring per tab
const int TERM_MAX_TABS = 4;      // bounded terminal tab pool
const int TERM_HIST = 16;         // command history ring per tab

// line editing state (shell mode)
struct TEdit {
    char line[96];
    int llen;
    int lcaret;
};

// one parsed pipeline job (bounded argv, redirection, storage in the tab)
const int TP_ARGC = 8;
const int TP_ARGLEN = 64;
const int TP_JOBS = 4;
struct TPipe {
    char argv[TP_JOBS][TP_ARGC][TP_ARGLEN];
    int argc[TP_JOBS];
    char infile[TP_JOBS][96];
    char outfile[TP_JOBS][96];
    bool append[TP_JOBS];
    int njobs;
};

// terminal tab: independent scrollback, history, cwd, input buffer, mode
enum { TM_SHELL = 0, TM_PYTHON = 1, TM_NANO = 2 };

struct TermTab {
    TLine ring[TL_RING];
    int rhead, rcount;
    TEdit ed;
    char cwd[128];
    char hist[TERM_HIST][96];
    int hist_n, hist_head, hist_view;
    TPipe jobs;
    char perr[96];
    void *py;                 // PyInterp* when mode == TM_PYTHON
    void *nano;               // NanoState* when mode == TM_NANO
    int mode;
    int scroll;               // scrollback view offset (0 = live bottom)
    char py_input[96];        // pending python line editing
    int py_len, py_caret;
};

struct TermApp {
    TermTab tab[TERM_MAX_TABS];
    int ntabs;
    int active;
    // TAB completion double-TAB timing
    unsigned last_tab_ms;
    int last_tab_len;
};

// ---- terminal.cpp services used by the other terminal files ----
uint32_t term_color(int tag);                       // tag -> RGB (theme aware)
void term_print(TermTab *t, const char *s, int tag);
void term_print_split(TermTab *t, const char *s, int tag, int split, int tag2);

// ---- parser (terminal_parser.cpp) ----
// Splits a command line into up to TP_JOBS pipeline jobs. Handles double
// quotes, single quotes, backslash escapes, a | pipes and < in, > out,
// >> append. Returns jobs->njobs (>= 1) or 0 with a message in err.
int term_parse_jobs(const char *line, TPipe *jobs, char *err, int errcap);

// ---- command registry (terminal_commands.cpp) ----
struct CmdEnv {
    TermTab *tab;
    const char *stdin_text;       // text piped into this command ("" if none)
    bool capture;                 // true: output must go into cap
    Str *cap;
    char cwd[128];
    const char (*hist)[96];       // tab history ring (for `history`)
    int hist_n, hist_head;
};
typedef void (*CmdFn)(CmdEnv *e, int argc, char **argv);
struct CmdDef { const char *name; CmdFn fn; };

const CmdDef *term_cmd_find(const char *name);
const CmdDef *term_cmd_at(int i);
int term_cmd_count();
void cenv_out(CmdEnv *e, const char *s);              // TC_FG output
void cenv_outc(CmdEnv *e, const char *s, int tag);    // colored output
void cenv_outnum(CmdEnv *e, long long v);
// cwd helper shared by cd/pwd/prompt
void term_resolve_path(const char *cwd, const char *arg, char *out, int maxn);

// ---- mini-python (terminal_python.cpp) - a REAL Python-subset interpreter ----
enum { PY_DONE = 0, PY_MORE = 1, PY_EXIT = 2, PY_ERR = 3 };
void *py_new();
void py_free(void *pi);
bool py_pending(void *pi);                  // true while a block is open
void py_set_repl(void *pi, bool repl);      // false for script runs
// Feed one physical line. PY_MORE = inside an unfinished block (indented or
// open ':'), PY_EXIT = exit() called, PY_ERR = runtime error already printed.
int py_line(void *pi, const char *line, void (*out)(void *ud, const char *s, int tag), void *ud);
const char *py_banner_line(int i);   // 3 banner lines, honest about limits

// ---- nano editor (terminal_nano.cpp) ----
void *nano_open(const char *ramfs_path, int *err);   // err: 0 ok, 1 open failed
void nano_close(void *n);
// 0 = keep editing, 1 = exit editor (status line via nano_status())
int nano_key(void *n, const kian_event_t *e);
void nano_draw(void *n, Rect r);
const char *nano_status(void *n);
