// Kian OS GUI - terminal line parser: quoting, escapes, pipes, redirection.
// Bounded and allocation-free (writes into the caller's TPipe storage).
//
// Grammar:  job ( '|' job )*        job := word* ('<' word)? ('>'|'>>' word)?
// A word is a run of quoted chunks and bare characters; backslash escapes
// the next character inside double quotes and outside quotes.
#include "terminal.h"
#include "../util.h"

static void perr(char *err, int errcap, const char *msg) {
    int i = 0;
    for (; msg[i] != '\0' && i < errcap - 1; i++) err[i] = msg[i];
    err[i] = '\0';
}

static void set_tok(char *dst, int cap, const char *src, int n) {
    if (n >= cap) n = cap - 1;
    for (int i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = '\0';
}

// state shared across the scan
struct PState {
    TPipe *jobs;
    int job;
    char tok[TP_ARGLEN];
    int tl;
    bool have_tok;
    int redir;      // 0 none, 1 '<', 2 '>' or '>>'
};

int term_parse_jobs(const char *line, TPipe *jobs, char *err, int errcap) {
    perr(err, errcap, "");
    jobs->njobs = 0;
    PState st;
    st.jobs = jobs;
    st.job = 0;
    st.tl = 0;
    st.have_tok = false;
    st.redir = 0;
    jobs->argc[0] = 0;
    jobs->infile[0][0] = '\0';
    jobs->outfile[0][0] = '\0';
    jobs->append[0] = false;

    // move the last token into argv (or into a pending redirect target)
    auto flush_tok = [&](bool at_word_end) {
        if (at_word_end && st.redir != 0) {
            // this word is the redirect target, not an argument
            if (st.tl == 0) { perr(err, errcap, "redirect needs a file"); return false; }
            if (st.redir == 1) {
                if (jobs->infile[st.job][0] != '\0') { perr(err, errcap, "only one < per command"); return false; }
                set_tok(jobs->infile[st.job], 96, st.tok, st.tl);
            } else {
                if (jobs->outfile[st.job][0] != '\0') { perr(err, errcap, "only one > per command"); return false; }
                set_tok(jobs->outfile[st.job], 96, st.tok, st.tl);
            }
            st.tl = 0;
            st.have_tok = false;
            st.redir = 0;
            return true;
        }
        if (!st.have_tok) return true;
        if (jobs->argc[st.job] < TP_ARGC) {
            set_tok(jobs->argv[st.job][jobs->argc[st.job]], TP_ARGLEN, st.tok, st.tl);
            jobs->argc[st.job]++;
        }
        st.tl = 0;
        st.have_tok = false;
        return true;
    };

    auto new_job = [&]() -> bool {
        if (st.redir != 0) { perr(err, errcap, "redirect needs a file"); return false; }
        flush_tok(false);
        if (st.job + 1 >= TP_JOBS) { perr(err, errcap, "too many pipeline stages (max 4)"); return false; }
        if (jobs->njobs <= st.job) jobs->njobs = st.job + 1;
        st.job++;
        jobs->argc[st.job] = 0;
        jobs->infile[st.job][0] = '\0';
        jobs->outfile[st.job][0] = '\0';
        jobs->append[st.job] = false;
        return true;
    };

    int i = 0;
    while (line[i] == ' ') i++;
    bool in_dq = false, in_sq = false;

    for (; line[i] != '\0'; i++) {
        char c = line[i];
        if (in_dq) {
            if (c == '"') { in_dq = false; continue; }
            if (c == '\\' && line[i + 1] != '\0') {
                char n = line[++i];
                if (st.tl < TP_ARGLEN - 1) st.tok[st.tl++] = (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
                st.have_tok = true;
                continue;
            }
            if (st.tl < TP_ARGLEN - 1) st.tok[st.tl++] = c;
            st.have_tok = true;
            continue;
        }
        if (in_sq) {
            if (c == '\'') { in_sq = false; continue; }
            if (st.tl < TP_ARGLEN - 1) st.tok[st.tl++] = c;
            st.have_tok = true;
            continue;
        }
        if (c == '"') { in_dq = true; st.have_tok = true; continue; }
        if (c == '\'') { in_sq = true; st.have_tok = true; continue; }
        if (c == '\\' && line[i + 1] != '\0') {
            char n = line[++i];
            if (st.tl < TP_ARGLEN - 1) st.tok[st.tl++] = (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
            st.have_tok = true;
            continue;
        }
        if (c == ' ') {
            // spaces before a redirect target are skipped, others end a word
            if (st.redir == 0) { if (!flush_tok(false)) return 0; }
            continue;
        }
        if (c == '|') { if (!new_job()) return 0; continue; }
        if (c == '<') {
            if (st.redir != 0) { perr(err, errcap, "redirect needs a file"); return 0; }
            flush_tok(false);
            st.redir = 1;
            continue;
        }
        if (c == '>') {
            if (st.redir != 0) { perr(err, errcap, "redirect needs a file"); return 0; }
            flush_tok(false);
            if (line[i + 1] == '>') { jobs->append[st.job] = true; i++; }
            st.redir = 2;
            continue;
        }
        // plain character: extend the current word
        if (st.tl < TP_ARGLEN - 1) st.tok[st.tl++] = c;
        st.have_tok = true;
        // does the word end here? (next char is a separator or the line ends)
        char n1 = line[i + 1];
        if (n1 == '\0' || n1 == ' ' || n1 == '|' || n1 == '<' || n1 == '>') {
            if (!flush_tok(true)) return 0;
        }
    }
    if (in_dq || in_sq) { perr(err, errcap, "unterminated quote"); return 0; }
    if (!flush_tok(st.redir != 0)) return 0;
    if (st.redir != 0) { perr(err, errcap, "redirect needs a file"); return 0; }
    if (jobs->njobs <= st.job) jobs->njobs = st.job + 1;
    if (jobs->argc[0] == 0) { jobs->njobs = 0; return 0; }   // empty line: caller treats as fresh prompt
    return jobs->njobs;
}
