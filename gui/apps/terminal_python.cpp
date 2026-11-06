// Kian OS GUI - Kian mini-python: a REAL (small) Python-subset interpreter.
//
// HONEST SCOPE (printed by the banner, documented in README):
//   supported: integers, strings, booleans, variables, arithmetic
//              (+ - * / // %), comparisons, and/or/not, if/elif/else,
//              while, for-in-range(), def + return, break/continue,
//              print/len/str/int/abs/min/max, exit()/quit()
//   NOT supported (and never faked): floats, lists/dicts/tuples, imports,
//              classes, string methods, input(), exceptions, stdlib.
//   '/' is integer division (documented; no floats exist in the GUI).
// Guards: bounded node pool, bounded call depth, bounded loop steps.
#include "terminal.h"
#include "../util.h"

// ---------------- values ----------------

enum VT { VT_INT = 0, VT_STR, VT_BOOL };
struct VVal {
    int t;
    long long i;
    char s[64];
};

static void vv_int(VVal *v, long long i) { v->t = VT_INT; v->i = i; v->s[0] = '\0'; }
static void vv_str(VVal *v, const char *s) {
    v->t = VT_STR;
    v->i = 0;
    int k = 0;
    for (; s[k] != '\0' && k < 62; k++) v->s[k] = s[k];
    v->s[k] = '\0';
}
static void vv_bool(VVal *v, bool b) { v->t = VT_BOOL; v->i = b ? 1 : 0; v->s[0] = '\0'; }

static void vv_to_text(const VVal *v, char *out, int cap) {
    if (v->t == VT_STR) {
        int k = 0;
        for (; v->s[k] != '\0' && k < cap - 1; k++) out[k] = v->s[k];
        out[k] = '\0';
        return;
    }
    if (v->t == VT_BOOL) {
        const char *b = (v->i != 0) ? "True" : "False";
        int k = 0;
        for (; b[k] != '\0' && k < cap - 1; k++) out[k] = b[k];
        out[k] = '\0';
        return;
    }
    unsigned long long m = (v->i < 0) ? (unsigned long long)(-v->i) : (unsigned long long)v->i;
    char tmp[24];
    int n = 0, k = 0;
    if (v->i < 0) out[k++] = '-';
    if (m == 0) tmp[n++] = '0';
    while (m > 0 && n < 22) { tmp[n++] = (char)('0' + (m % 10)); m /= 10; }
    while (n > 0 && k < cap - 1) out[k++] = tmp[--n];
    out[k] = '\0';
}

// ---------------- AST ----------------

enum NK {
    N_NUM, N_STR, N_VAR, N_TRUE, N_FALSE, N_NONE,
    N_BIN, N_UN, N_CALL,
    N_ASSIGN, N_AUG,
    N_IF, N_WHILE, N_FOR, N_DEF, N_RETURN,
    N_BREAK, N_CONTINUE, N_PASS, N_EXPRSTMT, N_BLOCK, N_ELIF, N_ELSE
};

// binary op codes
enum PO { P_ADD, P_SUB, P_MUL, P_DIV, P_FDIV, P_MOD,
          P_EQ, P_NE, P_LT, P_LE, P_GT, P_GE, P_AND, P_OR, P_NOT, P_NEG };

struct PNode {
    int k;
    long long iv;
    char sv[48];
    int a, b, c;      // children
    int next;         // statement chain / arg chain
};

const int PY_POOL = 768;
const int PY_SCOPES = 8;
const int PY_FUNCS = 12;

struct PyScope {
    char vn[32][24];
    VVal vv[32];
    int n;
};

struct PyFn {
    char name[24];
    char params[4][24];
    int nparam;
    int body;
};

struct PyInterp {
    PNode pool[PY_POOL];
    int pn;
    PyScope sc[PY_SCOPES];
    int depth;
    PyFn fns[PY_FUNCS];
    int nfn;
    // pending multi-line block (REPL + scripts)
    bool pend_open;
    int pend_kind;         // 0 if-chain, 1 while, 2 for, 3 def
    int pend_indent;
    struct {
        char header[96];
        char body[40][96];
        int bind[40];
        int bn;
    } segs[8];
    int nseg;
    bool repl;
    // flatten buffers used by run_pending (kept in the struct: no big stack)
    char rl[130][96];
    int rind[130];
    // guards + flow
    long long steps;
    int brk, cont, ret;
    VVal retval;
    bool exitreq;
    // errors
    bool err;
    char errmsg[80];
    int lineno;
    // output
    void (*out)(void *ud, const char *s, int tag);
    void *ud;
};

// ---------------- error + node helpers ----------------

static void pyerr(PyInterp *pi, const char *msg) {
    if (!pi->err) {
        pi->err = true;
        int k = 0;
        for (; msg[k] != '\0' && k < 78; k++) pi->errmsg[k] = msg[k];
        pi->errmsg[k] = '\0';
    }
}

static int node_new(PyInterp *pi, int k) {
    if (pi->pn >= PY_POOL) { pyerr(pi, "program too complex (node pool full)"); return -1; }
    PNode *n = &pi->pool[pi->pn];
    n->k = k;
    n->iv = 0;
    n->sv[0] = '\0';
    n->a = n->b = n->c = -1;
    n->next = -1;
    return pi->pn++;
}

static void node_set_str(PNode *n, const char *s) {
    int k = 0;
    for (; s[k] != '\0' && k < 46; k++) n->sv[k] = s[k];
    n->sv[k] = '\0';
}

// ---------------- tokenizer ----------------

enum TK { TK_END = 0, TK_NUM, TK_STR, TK_NAME, TK_OP, TK_BAD };

struct Tok {
    int k;
    long long iv;
    char sv[48];
    int op;
};

// op codes as chars: single char or two-char pairs
enum { T_PLUS, T_MINUS, T_STAR, T_SLASH, T_DSLASH, T_PERCENT,
       T_EQ2, T_NE, T_LE, T_GE, T_LT, T_GT, T_ASSIGN, T_PLUSEQ, T_MINUSEQ,
       T_LP, T_RP, T_COMMA, T_COLON };

static bool tok_is_kw(const Tok *t, const char *kw) {
    return t->k == TK_NAME && strcmp(t->sv, kw) == 0;
}

static const char *const KEYWORDS[] = {
    "if", "elif", "else", "while", "for", "in", "def", "return",
    "break", "continue", "pass", "and", "or", "not", "True", "False", "None"
};

static bool is_keyword(const char *s) {
    for (int i = 0; i < (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0])); i++) {
        if (strcmp(KEYWORDS[i], s) == 0) return true;
    }
    return false;
}

static bool scan_token(PyInterp *pi, const char *src, int *pos, Tok *t) {
    while (src[*pos] == ' ' || src[*pos] == '\t' || src[*pos] == '\n') (*pos)++;
    char c = src[*pos];
    t->k = TK_END;
    t->iv = 0;
    t->sv[0] = '\0';
    t->op = 0;
    if (c == '\0') return true;
    if (c >= '0' && c <= '9') {
        long long v = 0;
        while (src[*pos] >= '0' && src[*pos] <= '9') {
            v = v * 10 + (src[*pos] - '0');
            (*pos)++;
            if (v > 4611686018427387903LL) { pyerr(pi, "number too large"); return false; }
        }
        if (src[*pos] == '.') { pyerr(pi, "floats are not supported in this interpreter"); return false; }
        t->k = TK_NUM;
        t->iv = v;
        return true;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        int k = 0;
        while (((src[*pos] >= 'a' && src[*pos] <= 'z') ||
                (src[*pos] >= 'A' && src[*pos] <= 'Z') ||
                (src[*pos] >= '0' && src[*pos] <= '9') || src[*pos] == '_') && k < 40) {
            t->sv[k++] = src[*pos];
            (*pos)++;
        }
        t->sv[k] = '\0';
        t->k = TK_NAME;
        return true;
    }
    if (c == '"') {
        (*pos)++;
        int k = 0;
        while (src[*pos] != '\0' && src[*pos] != '"' && src[*pos] != '\n') {
            char ch = src[*pos];
            if (ch == '\\' && src[*pos + 1] != '\0') {
                (*pos)++;
                char e = src[*pos];
                ch = (e == 'n') ? '\n' : (e == 't') ? '\t' : e;
            }
            if (k < 44) t->sv[k++] = ch;
            (*pos)++;
        }
        if (src[*pos] != '"') { pyerr(pi, "unterminated string"); return false; }
        (*pos)++;
        t->sv[k] = '\0';
        t->k = TK_STR;
        return true;
    }
    // operators
    char n1 = src[*pos + 1];
    (*pos)++;
    switch (c) {
    case '+': if (n1 == '=') { (*pos)++; t->op = T_PLUSEQ; } else t->op = T_PLUS; break;
    case '-': if (n1 == '=') { (*pos)++; t->op = T_MINUSEQ; } else t->op = T_MINUS; break;
    case '*': t->op = T_STAR; break;
    case '/': if (n1 == '/') { (*pos)++; t->op = T_DSLASH; } else t->op = T_SLASH; break;
    case '%': t->op = T_PERCENT; break;
    case '=': if (n1 == '=') { (*pos)++; t->op = T_EQ2; } else t->op = T_ASSIGN; break;
    case '!': if (n1 == '=') { (*pos)++; t->op = T_NE; } else { pyerr(pi, "unexpected '!'"); return false; } break;
    case '<': if (n1 == '=') { (*pos)++; t->op = T_LE; } else t->op = T_LT; break;
    case '>': if (n1 == '=') { (*pos)++; t->op = T_GE; } else t->op = T_GT; break;
    case '(': t->op = T_LP; break;
    case ')': t->op = T_RP; break;
    case ',': t->op = T_COMMA; break;
    case ':': t->op = T_COLON; break;
    case '#': // comment: skip to end of line
        while (src[*pos] != '\0' && src[*pos] != '\n') (*pos)++;
        t->k = TK_END;
        return true;
    case '[': case ']': case '{': case '}':
        pyerr(pi, "lists/dicts are not supported in this interpreter");
        return false;
    default:
        pyerr(pi, "unexpected character");
        return false;
    }
    t->k = TK_OP;
    return true;
}

// ---------------- expression parser (precedence climbing) ----------------

struct Parser {
    PyInterp *pi;
    Tok cur;
    int pos;
    const char *src;
};

static bool tk_next(Parser *p) {
    if (!scan_token(p->pi, p->src, &p->pos, &p->cur)) return false;
    return true;
}

static bool parse_expr(Parser *p, int *out);

static bool parse_atom(Parser *p, int *out) {
    Tok *t = &p->cur;
    if (t->k == TK_NUM) {
        int n = node_new(p->pi, N_NUM);
        if (n < 0) return false;
        p->pi->pool[n].iv = t->iv;
        *out = n;
        return tk_next(p);
    }
    if (t->k == TK_STR) {
        int n = node_new(p->pi, N_STR);
        if (n < 0) return false;
        node_set_str(&p->pi->pool[n], t->sv);
        *out = n;
        return tk_next(p);
    }
    if (tok_is_kw(t, "True") || tok_is_kw(t, "False")) {
        int n = node_new(p->pi, t->sv[0] == 'T' ? N_TRUE : N_FALSE);
        if (n < 0) return false;
        *out = n;
        return tk_next(p);
    }
    if (tok_is_kw(t, "None")) {
        int n = node_new(p->pi, N_NONE);
        if (n < 0) return false;
        *out = n;
        return tk_next(p);
    }
    if (t->k == TK_NAME) {
        char name[48];
        int k = 0;
        for (; t->sv[k] != '\0' && k < 46; k++) name[k] = t->sv[k];
        name[k] = '\0';
        if (!tk_next(p)) return false;
        if (p->cur.k == TK_OP && p->cur.op == T_LP) {
            // call: name(args)
            int n = node_new(p->pi, N_CALL);
            if (n < 0) return false;
            node_set_str(&p->pi->pool[n], name);
            if (!tk_next(p)) return false;
            int last = -1;
            if (!(p->cur.k == TK_OP && p->cur.op == T_RP)) {
                for (;;) {
                    int arg = -1;
                    if (!parse_expr(p, &arg)) return false;
                    int an = node_new(p->pi, N_PASS);   // wrapper node holding the arg in a
                    if (an < 0) return false;
                    p->pi->pool[an].k = N_EXPRSTMT;
                    p->pi->pool[an].a = arg;
                    if (last < 0) p->pi->pool[n].a = an;
                    else p->pi->pool[last].next = an;
                    last = an;
                    if (p->cur.k == TK_OP && p->cur.op == T_COMMA) { if (!tk_next(p)) return false; continue; }
                    break;
                }
            }
            if (!(p->cur.k == TK_OP && p->cur.op == T_RP)) { pyerr(p->pi, "expected ')'"); return false; }
            *out = n;
            return tk_next(p);
        }
        int n = node_new(p->pi, N_VAR);
        if (n < 0) return false;
        node_set_str(&p->pi->pool[n], name);
        *out = n;
        return true;
    }
    if (t->k == TK_OP && t->op == T_LP) {
        if (!tk_next(p)) return false;
        if (!parse_expr(p, out)) return false;
        if (!(p->cur.k == TK_OP && p->cur.op == T_RP)) { pyerr(p->pi, "expected ')'"); return false; }
        return tk_next(p);
    }
    pyerr(p->pi, "unexpected token in expression");
    return false;
}

static bool parse_unary(Parser *p, int *out) {
    if (p->cur.k == TK_OP && p->cur.op == T_MINUS) {
        if (!tk_next(p)) return false;
        int a = -1;
        if (!parse_unary(p, &a)) return false;
        int n = node_new(p->pi, N_UN);
        if (n < 0) return false;
        p->pi->pool[n].iv = P_NEG;
        p->pi->pool[n].a = a;
        *out = n;
        return true;
    }
    if (tok_is_kw(&p->cur, "not")) {
        if (!tk_next(p)) return false;
        int a = -1;
        if (!parse_unary(p, &a)) return false;
        int n = node_new(p->pi, N_UN);
        if (n < 0) return false;
        p->pi->pool[n].iv = P_NOT;
        p->pi->pool[n].a = a;
        *out = n;
        return true;
    }
    return parse_atom(p, out);
}

static bool parse_mul(Parser *p, int *out) {
    if (!parse_unary(p, out)) return false;
    while (p->cur.k == TK_OP && (p->cur.op == T_STAR || p->cur.op == T_SLASH ||
                                 p->cur.op == T_DSLASH || p->cur.op == T_PERCENT)) {
        int op = p->cur.op;
        if (!tk_next(p)) return false;
        int rhs = -1;
        if (!parse_unary(p, &rhs)) return false;
        int n = node_new(p->pi, N_BIN);
        if (n < 0) return false;
        p->pi->pool[n].iv = (op == T_STAR) ? P_MUL : (op == T_SLASH) ? P_DIV
                          : (op == T_DSLASH) ? P_FDIV : P_MOD;
        p->pi->pool[n].a = *out;
        p->pi->pool[n].b = rhs;
        *out = n;
    }
    return true;
}

static bool parse_add(Parser *p, int *out) {
    if (!parse_mul(p, out)) return false;
    while (p->cur.k == TK_OP && (p->cur.op == T_PLUS || p->cur.op == T_MINUS)) {
        int op = p->cur.op;
        if (!tk_next(p)) return false;
        int rhs = -1;
        if (!parse_mul(p, &rhs)) return false;
        int n = node_new(p->pi, N_BIN);
        if (n < 0) return false;
        p->pi->pool[n].iv = (op == T_PLUS) ? P_ADD : P_SUB;
        p->pi->pool[n].a = *out;
        p->pi->pool[n].b = rhs;
        *out = n;
    }
    return true;
}

static bool parse_cmp(Parser *p, int *out) {
    if (!parse_add(p, out)) return false;
    while (p->cur.k == TK_OP && (p->cur.op == T_EQ2 || p->cur.op == T_NE ||
                                 p->cur.op == T_LT || p->cur.op == T_LE ||
                                 p->cur.op == T_GT || p->cur.op == T_GE)) {
        int op = p->cur.op;
        if (!tk_next(p)) return false;
        int rhs = -1;
        if (!parse_add(p, &rhs)) return false;
        int n = node_new(p->pi, N_BIN);
        if (n < 0) return false;
        p->pi->pool[n].iv = (op == T_EQ2) ? P_EQ : (op == T_NE) ? P_NE
                          : (op == T_LT) ? P_LT : (op == T_LE) ? P_LE
                          : (op == T_GT) ? P_GT : P_GE;
        p->pi->pool[n].a = *out;
        p->pi->pool[n].b = rhs;
        *out = n;
    }
    return true;
}

static bool parse_and(Parser *p, int *out) {
    if (!parse_cmp(p, out)) return false;
    while (tok_is_kw(&p->cur, "and")) {
        if (!tk_next(p)) return false;
        int rhs = -1;
        if (!parse_cmp(p, &rhs)) return false;
        int n = node_new(p->pi, N_BIN);
        if (n < 0) return false;
        p->pi->pool[n].iv = P_AND;
        p->pi->pool[n].a = *out;
        p->pi->pool[n].b = rhs;
        *out = n;
    }
    return true;
}

static bool parse_expr(Parser *p, int *out) {
    if (!parse_and(p, out)) return false;
    while (tok_is_kw(&p->cur, "or")) {
        if (!tk_next(p)) return false;
        int rhs = -1;
        if (!parse_and(p, &rhs)) return false;
        int n = node_new(p->pi, N_BIN);
        if (n < 0) return false;
        p->pi->pool[n].iv = P_OR;
        p->pi->pool[n].a = *out;
        p->pi->pool[n].b = rhs;
        *out = n;
    }
    return true;
}

// ---------------- statement parser (single physical line) ----------------
// Returns a statement node chain for one simple statement.
// kind: "if"/"elif"/"else"/"while"/"for"/"def" headers handled by the caller.

static bool truthy(const VVal *v) {
    if (v->t == VT_STR) return v->s[0] != '\0';
    return v->i != 0;
}

static bool eval_node(PyInterp *pi, int idx, VVal *out);

static bool parse_simple_stmt(PyInterp *pi, const char *line, int *out) {
    Parser p;
    p.pi = pi;
    p.pos = 0;
    p.src = line;
    if (!tk_next(&p)) return false;
    if (p.cur.k == TK_END) { *out = node_new(pi, N_PASS); return *out >= 0; }
    if (tok_is_kw(&p.cur, "return")) {
        if (!tk_next(&p)) return false;
        int n = node_new(pi, N_RETURN);
        if (n < 0) return false;
        if (p.cur.k != TK_END) {
            int a = -1;
            if (!parse_expr(&p, &a)) return false;
            pi->pool[n].a = a;
        }
        if (p.cur.k != TK_END) { pyerr(pi, "extra tokens after return"); return false; }
        *out = n;
        return true;
    }
    if (tok_is_kw(&p.cur, "break")) {
        tk_next(&p);
        *out = node_new(pi, N_BREAK);
        return *out >= 0;
    }
    if (tok_is_kw(&p.cur, "continue")) {
        tk_next(&p);
        *out = node_new(pi, N_CONTINUE);
        return *out >= 0;
    }
    if (tok_is_kw(&p.cur, "pass")) {
        tk_next(&p);
        *out = node_new(pi, N_PASS);
        return *out >= 0;
    }
    // name = expr   /   name += expr   /   bare expression
    if (p.cur.k == TK_NAME && !is_keyword(p.cur.sv)) {
        char name[48];
        int k = 0;
        for (; p.cur.sv[k] != '\0' && k < 46; k++) name[k] = p.cur.sv[k];
        name[k] = '\0';
        if (!tk_next(&p)) return false;
        if (p.cur.k == TK_OP && p.cur.op == T_ASSIGN) {
            if (!tk_next(&p)) return false;
            int a = -1;
            if (!parse_expr(&p, &a)) return false;
            if (p.cur.k != TK_END) { pyerr(pi, "extra tokens after assignment"); return false; }
            int n = node_new(pi, N_ASSIGN);
            if (n < 0) return false;
            node_set_str(&pi->pool[n], name);
            pi->pool[n].a = a;
            *out = n;
            return true;
        }
        if (p.cur.k == TK_OP && (p.cur.op == T_PLUSEQ || p.cur.op == T_MINUSEQ)) {
            int op = p.cur.op;
            if (!tk_next(&p)) return false;
            int a = -1;
            if (!parse_expr(&p, &a)) return false;
            if (p.cur.k != TK_END) { pyerr(pi, "extra tokens after assignment"); return false; }
            int n = node_new(pi, N_AUG);
            if (n < 0) return false;
            node_set_str(&pi->pool[n], name);
            pi->pool[n].a = a;
            pi->pool[n].iv = (op == T_PLUSEQ) ? P_ADD : P_SUB;
            *out = n;
            return true;
        }
        // bare expression starting with a name: reparse from the start
        p.pos = 0;
        if (!tk_next(&p)) return false;
        int a = -1;
        if (!parse_expr(&p, &a)) return false;
        if (p.cur.k != TK_END) { pyerr(pi, "unexpected extra tokens"); return false; }
        int n = node_new(pi, N_EXPRSTMT);
        if (n < 0) return false;
        pi->pool[n].a = a;
        *out = n;
        return true;
    }
    // expression statement (e.g. print("hi"))
    int a = -1;
    if (!parse_expr(&p, &a)) return false;
    if (p.cur.k != TK_END) { pyerr(pi, "unexpected extra tokens"); return false; }
    int n = node_new(pi, N_EXPRSTMT);
    if (n < 0) return false;
    pi->pool[n].a = a;
    *out = n;
    return true;
}

// ---------------- evaluator ----------------

static PyScope *cur_scope(PyInterp *pi) { return &pi->sc[pi->depth]; }

static VVal *scope_find(PyScope *s, const char *name) {
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->vn[i], name) == 0) return &s->vv[i];
    }
    return 0;
}

static VVal *scope_add(PyScope *s, const char *name) {
    if (s->n >= 32) return 0;
    int k = 0;
    for (; name[k] != '\0' && k < 22; k++) s->vn[s->n][k] = name[k];
    s->vn[s->n][k] = '\0';
    VVal *v = &s->vv[s->n];
    v->t = VT_INT;
    v->i = 0;
    v->s[0] = '\0';
    s->n++;
    return v;
}

static VVal *var_lookup(PyInterp *pi, const char *name) {
    VVal *v = scope_find(cur_scope(pi), name);
    if (v) return v;
    if (pi->depth > 0) return scope_find(&pi->sc[0], name);  // global fallback (read)
    return 0;
}

static bool eval_call(PyInterp *pi, PNode *n, VVal *out) {
    // evaluate args
    VVal args[6];
    int na = 0;
    for (int a = n->a; a >= 0; a = pi->pool[a].next) {
        if (na >= 6) { pyerr(pi, "too many arguments (max 6)"); return false; }
        if (!eval_node(pi, pi->pool[a].a, &args[na])) return false;
        na++;
    }
    const char *name = n->sv;
    if (strcmp(name, "print") == 0) {
        char buf[TL_LEN + 4];
        int k = 0;
        for (int i = 0; i < na; i++) {
            if (i > 0 && k < TL_LEN) buf[k++] = ' ';
            char one[64];
            vv_to_text(&args[i], one, 64);
            for (int q = 0; one[q] != '\0' && k < TL_LEN - 1; q++) buf[k++] = one[q];
        }
        buf[k] = '\0';
        pi->out(pi->ud, buf, TC_FG);
        pi->out(pi->ud, "\n", TC_FG);
        vv_int(out, na);
        return true;
    }
    if (strcmp(name, "len") == 0) {
        if (na != 1 || args[0].t != VT_STR) { pyerr(pi, "len() needs one string"); return false; }
        long long L = 0;
        for (; args[0].s[L] != '\0'; L++) {}
        vv_int(out, L);
        return true;
    }
    if (strcmp(name, "str") == 0) {
        if (na != 1) { pyerr(pi, "str() needs one argument"); return false; }
        char txt[64];
        vv_to_text(&args[0], txt, 64);
        vv_str(out, txt);
        return true;
    }
    if (strcmp(name, "int") == 0) {
        if (na != 1) { pyerr(pi, "int() needs one argument"); return false; }
        if (args[0].t == VT_STR) {
            const char *s = args[0].s;
            int i = 0;
            bool neg = false;
            if (s[i] == '-') { neg = true; i++; }
            if (s[i] == '\0') { pyerr(pi, "int: invalid literal"); return false; }
            long long v = 0;
            for (; s[i] != '\0'; i++) {
                if (s[i] < '0' || s[i] > '9') { pyerr(pi, "int: invalid literal"); return false; }
                v = v * 10 + (s[i] - '0');
                if (v > 4611686018427387903LL) { pyerr(pi, "int: number too large"); return false; }
            }
            vv_int(out, neg ? -v : v);
            return true;
        }
        vv_int(out, args[0].i);
        return true;
    }
    if (strcmp(name, "abs") == 0) {
        if (na != 1 || args[0].t == VT_STR) { pyerr(pi, "abs() needs one number"); return false; }
        vv_int(out, args[0].i < 0 ? -args[0].i : args[0].i);
        return true;
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        if (na < 1) { pyerr(pi, "min/max need arguments"); return false; }
        bool ismin = (name[0] == 'm' && name[1] == 'i');
        long long best = args[0].i;
        for (int i = 1; i < na; i++) {
            if (args[i].t == VT_STR) { pyerr(pi, "min/max need numbers"); return false; }
            if (ismin ? (args[i].i < best) : (args[i].i > best)) best = args[i].i;
        }
        vv_int(out, best);
        return true;
    }
    if (strcmp(name, "exit") == 0 || strcmp(name, "quit") == 0) {
        pi->exitreq = true;
        vv_int(out, 0);
        return true;
    }
    if (strcmp(name, "input") == 0) {
        pyerr(pi, "input() is not supported (no keyboard stream)");
        return false;
    }
    if (strcmp(name, "range") == 0) {
        pyerr(pi, "range() works only in 'for' loops");
        return false;
    }
    // user function?
    for (int i = 0; i < pi->nfn; i++) {
        if (strcmp(pi->fns[i].name, name) == 0) {
            if (na != pi->fns[i].nparam) { pyerr(pi, "wrong argument count"); return false; }
            if (pi->depth >= PY_SCOPES - 1) { pyerr(pi, "too much recursion (depth 8 max)"); return false; }
            PyScope *frame = &pi->sc[pi->depth + 1];
            frame->n = 0;
            for (int a = 0; a < na; a++) {
                VVal *dst = scope_add(frame, pi->fns[i].params[a]);
                if (!dst) { pyerr(pi, "too many locals"); return false; }
                *dst = args[a];
            }
            int saved_depth = pi->depth;
            pi->depth++;
            int saved_ret = pi->ret;
            pi->ret = 0;
            int r = pi->fns[i].body;
            bool ok = true;
            while (r >= 0 && !pi->ret && !pi->err && pi->steps < 200000 && !pi->exitreq) {
                pi->steps++;
                VVal dummy;
                ok = exec_stmt(pi, r, &dummy);
                if (!ok) break;
                r = pi->pool[r].next;
            }
            pi->depth = saved_depth;
            if (!pi->ret && !pi->err) vv_int(out, 0);   // implicit None-ish 0
            else if (!pi->err) *out = pi->retval;
            pi->ret = saved_ret;
            return !pi->err;
        }
    }
    pyerr(pi, "unknown function");
    return false;
}

static bool eval_node(PyInterp *pi, int idx, VVal *out) {
    if (idx < 0 || pi->err) return false;
    PNode *n = &pi->pool[idx];
    switch (n->k) {
    case N_NUM: vv_int(out, n->iv); return true;
    case N_STR: vv_str(out, n->sv); return true;
    case N_TRUE: vv_bool(out, true); return true;
    case N_FALSE: vv_bool(out, false); return true;
    case N_NONE: vv_int(out, 0); out->t = VT_INT; return true;
    case N_VAR: {
        VVal *v = var_lookup(pi, n->sv);
        if (!v) {
            char msg[80];
            int k = 0;
            const char *p1 = "name '";
            for (; p1[k] != '\0'; k++) msg[k] = p1[k];
            int q = 0;
            for (; n->sv[q] != '\0' && k < 70; q++) msg[k++] = n->sv[q];
            const char *p2 = "' is not defined";
            for (; p2[q] != '\0' && k < 76; q++) msg[k++] = p2[q];
            msg[k] = '\0';
            pyerr(pi, msg);
            return false;
        }
        *out = *v;
        return true;
    }
    case N_UN: {
        VVal a;
        if (!eval_node(pi, n->a, &a)) return false;
        if (n->iv == P_NEG) {
            if (a.t == VT_STR) { pyerr(pi, "cannot negate a string"); return false; }
            vv_int(out, -a.i);
        } else {
            vv_bool(out, !truthy(&a));
        }
        return true;
    }
    case N_BIN: {
        VVal a, b;
        if (n->iv == P_AND) {
            if (!eval_node(pi, n->a, &a)) return false;
            if (!truthy(&a)) { vv_bool(out, false); return true; }
            if (!eval_node(pi, n->b, &b)) return false;
            vv_bool(out, truthy(&b));
            return true;
        }
        if (n->iv == P_OR) {
            if (!eval_node(pi, n->a, &a)) return false;
            if (truthy(&a)) { vv_bool(out, true); return true; }
            if (!eval_node(pi, n->b, &b)) return false;
            vv_bool(out, truthy(&b));
            return true;
        }
        if (!eval_node(pi, n->a, &a)) return false;
        if (!eval_node(pi, n->b, &b)) return false;
        pi->steps++;
        if (pi->steps > 200000) { pyerr(pi, "execution limit reached (possible infinite loop)"); return false; }
        // string ops
        if (a.t == VT_STR || b.t == VT_STR) {
            if (n->iv == P_ADD) {
                if (a.t != VT_STR || b.t != VT_STR) { pyerr(pi, "cannot mix string and number with +"); return false; }
                char res[64];
                int k = 0;
                for (; a.s[k] != '\0' && k < 60; k++) res[k] = a.s[k];
                for (int q = 0; b.s[q] != '\0' && k < 62; q++) res[k++] = b.s[q];
                res[k] = '\0';
                vv_str(out, res);
                return true;
            }
            if (n->iv == P_MUL && a.t == VT_STR && b.t == VT_INT) {
                if (b.i < 0 || b.i > 16) { pyerr(pi, "string repeat limited to 16"); return false; }
                char res[64];
                int k = 0;
                for (int r = 0; r < (int)b.i; r++) {
                    for (int q = 0; a.s[q] != '\0' && k < 62; q++) res[k++] = a.s[q];
                }
                res[k] = '\0';
                vv_str(out, res);
                return true;
            }
            if (n->iv == P_EQ || n->iv == P_NE) {
                bool eq = (a.t == b.t) && (strcmp(a.s, b.s) == 0);
                vv_bool(out, n->iv == P_EQ ? eq : !eq);
                return true;
            }
            pyerr(pi, "this operator does not work on strings");
            return false;
        }
        long long x = a.i, y = b.i, r = 0;
        switch (n->iv) {
        case P_ADD: r = x + y; break;
        case P_SUB: r = x - y; break;
        case P_MUL: r = x * y; break;
        case P_DIV:
            if (y == 0) { pyerr(pi, "division by zero"); return false; }
            r = x / y; break;   // integer division (no floats here)
        case P_FDIV:
            if (y == 0) { pyerr(pi, "division by zero"); return false; }
            r = x / y; break;
        case P_MOD:
            if (y == 0) { pyerr(pi, "modulo by zero"); return false; }
            r = x % y; break;
        case P_EQ: vv_bool(out, a.t == b.t && x == y); return true;
        case P_NE: vv_bool(out, !(a.t == b.t && x == y)); return true;
        case P_LT: vv_bool(out, x < y); return true;
        case P_LE: vv_bool(out, x <= y); return true;
        case P_GT: vv_bool(out, x > y); return true;
        case P_GE: vv_bool(out, x >= y); return true;
        default: pyerr(pi, "bad operator"); return false;
        }
        if (r > 4611686018427387903LL || r < -4611686018427387903LL) {
            pyerr(pi, "number too large");
            return false;
        }
        vv_int(out, r);
        return true;
    }
    case N_CALL: return eval_call(pi, n, out);
    case N_PASS: vv_int(out, 0); return true;
    case N_EXPRSTMT: {
        VVal v;
        if (!eval_node(pi, n->a, &v)) return false;
        *out = v;
        return true;
    }
    case N_ASSIGN: {
        VVal v;
        if (!eval_node(pi, n->a, &v)) return false;
        VVal *dst = scope_find(cur_scope(pi), n->sv);
        if (!dst) dst = scope_add(cur_scope(pi), n->sv);
        if (!dst) { pyerr(pi, "too many variables"); return false; }
        *dst = v;
        *out = v;
        return true;
    }
    case N_AUG: {
        VVal v;
        if (!eval_node(pi, n->a, &v)) return false;
        VVal *dst = scope_find(cur_scope(pi), n->sv);
        if (!dst) { pyerr(pi, "name is not defined"); return false; }
        if (dst->t == VT_STR && v.t == VT_STR && n->iv == P_ADD) {
            char res[64];
            int k = 0;
            for (; dst->s[k] != '\0' && k < 60; k++) res[k] = dst->s[k];
            for (int q = 0; v.s[q] != '\0' && k < 62; q++) res[k++] = v.s[q];
            res[k] = '\0';
            vv_str(dst, res);
            *out = *dst;
            return true;
        }
        if (dst->t == VT_STR || v.t == VT_STR) { pyerr(pi, "+= needs matching types"); return false; }
        dst->i = (n->iv == P_ADD) ? (dst->i + v.i) : (dst->i - v.i);
        *out = *dst;
        return true;
    }
    default:
        pyerr(pi, "cannot evaluate this statement as an expression");
        return false;
    }
}

// exec one statement node (block heads handled here too)
static bool exec_stmt(PyInterp *pi, int idx, VVal *out);

static bool exec_block(PyInterp *pi, int chain) {
    int r = chain;
    while (r >= 0 && !pi->brk && !pi->cont && !pi->ret && !pi->err && pi->steps < 200000 && !pi->exitreq) {
        pi->steps++;
        VVal dummy;
        if (!exec_stmt(pi, r, &dummy)) return false;
        r = pi->pool[r].next;
    }
    return !pi->err;
}

static bool exec_stmt(PyInterp *pi, int idx, VVal *out) {
    if (idx < 0 || pi->err) return false;
    PNode *n = &pi->pool[idx];
    switch (n->k) {
    case N_IF: {
        VVal c;
        if (!eval_node(pi, n->a, &c)) return false;
        if (truthy(&c)) return exec_block(pi, n->b);
        if (n->c >= 0) return exec_stmt(pi, n->c, out);   // elif/else chain
        return true;
    }
    case N_ELIF: {
        // same shape as N_IF
        VVal c;
        if (!eval_node(pi, n->a, &c)) return false;
        if (truthy(&c)) return exec_block(pi, n->b);
        if (n->c >= 0) return exec_stmt(pi, n->c, out);
        return true;
    }
    case N_ELSE:
        return exec_block(pi, n->b);
    case N_WHILE: {
        long long guard = 0;
        while (pi->steps < 200000 && !pi->brk && !pi->err && !pi->exitreq) {
            pi->steps++;
            if (++guard > 100000) { pyerr(pi, "while loop limit (100000) reached"); return false; }
            VVal c;
            if (!eval_node(pi, n->a, &c)) return false;
            if (!truthy(&c)) break;
            if (!exec_block(pi, n->b)) return false;
            if (pi->brk) { pi->brk = 0; break; }
            pi->cont = 0;
        }
        pi->brk = 0;
        return !pi->err;
    }
    case N_FOR: {
        // header: var = n->sv, range expr = n->a, body = n->b
        // range expr is an N_CALL to range with 1-3 int args
        PNode *rc = &pi->pool[n->a];
        long long r0 = 0, r1 = 0, step = 1;
        if (rc->k != N_CALL || strcmp(rc->sv, "range") != 0) {
            pyerr(pi, "for supports 'for x in range(...):' only");
            return false;
        }
        VVal args[3];
        int na = 0;
        for (int a = rc->a; a >= 0; a = pi->pool[a].next) {
            if (na >= 3) { pyerr(pi, "range takes 1-3 arguments"); return false; }
            if (!eval_node(pi, pi->pool[a].a, &args[na])) return false;
            if (args[na].t == VT_STR) { pyerr(pi, "range needs integers"); return false; }
            na++;
        }
        if (na == 1) { r1 = args[0].i; }
        else if (na == 2) { r0 = args[0].i; r1 = args[1].i; }
        else if (na == 3) { r0 = args[0].i; r1 = args[1].i; step = args[2].i; }
        else { pyerr(pi, "range takes 1-3 arguments"); return false; }
        if (step == 0) { pyerr(pi, "range step cannot be 0"); return false; }
        long long guard = 0;
        for (long long i = r0; (step > 0) ? (i < r1) : (i > r1); i += step) {
            if (pi->brk || pi->err || pi->exitreq) break;
            if (++guard > 100000) { pyerr(pi, "for loop limit (100000) reached"); return false; }
            VVal *dst = scope_find(cur_scope(pi), n->sv);
            if (!dst) dst = scope_add(cur_scope(pi), n->sv);
            if (!dst) { pyerr(pi, "too many variables"); return false; }
            vv_int(dst, i);
            if (!exec_block(pi, n->b)) return false;
            if (pi->brk) { pi->brk = 0; break; }
            pi->cont = 0;
        }
        pi->brk = 0;
        return !pi->err;
    }
    case N_RETURN: {
        if (n->a >= 0) {
            if (!eval_node(pi, n->a, &pi->retval)) return false;
        } else {
            vv_int(&pi->retval, 0);
        }
        pi->ret = 1;
        return true;
    }
    case N_DEF: {
        if (!register_def(pi, n)) return false;
        return true;
    }
    case N_BREAK: pi->brk = 1; return true;
    case N_CONTINUE: pi->cont = 1; return true;
    case N_PASS: return true;
    case N_EXPRSTMT: case N_ASSIGN: case N_AUG: {
        VVal v;
        if (!eval_node(pi, idx, &v)) return false;
        if (out) *out = v;
        return true;
    }
    default:
        pyerr(pi, "unexpected statement");
        return false;
    }
}

// ---------------- block-line parser (indentation based) ----------------

struct BLines {
    const char (*l)[96];
    const int *ind;
    int n;
};

// parses lines[from..] at exactly `indent`; stops before a line with less
// indent; returns the node chain in *out and the stop index in *stop
static bool parse_block_lines(PyInterp *pi, BLines *L, int from, int indent, int *stop, int *out) {
    int head = -1, stmt_tail = -1, if_tail = -1;
    int i = from;
    *out = -1;
    while (i < L->n) {
        if (L->l[i][0] == '\0') { i++; continue; }   // blank (already stripped)
        if (L->ind[i] < indent) break;
        if (L->ind[i] > indent) { pyerr(pi, "unexpected indent"); return false; }
        const char *ln = L->l[i];
        int st = -1;
        // header? (if/elif/else/while/for/def ending with ':')
        int len = 0;
        for (; ln[len] != '\0'; len++) {}
        if (len > 0 && ln[len - 1] == ':') {
            char word[16];
            int w = 0;
            int p = 0;
            while (ln[p] == ' ') p++;
            for (; ln[p] != '\0' && ln[p] != ' ' && w < 15; p++) word[w++] = ln[p];
            word[w] = '\0';
            int node = -1;
            if (strcmp(word, "if") == 0 || strcmp(word, "elif") == 0) {
                // cond = text between keyword and ':'
                char cond[96];
                int c = 0;
                p++; // skip space
                for (; ln[p] != ':' && ln[p] != '\0' && c < 94; p++) cond[c++] = ln[p];
                cond[c] = '\0';
                Parser pp;
                pp.pi = pi;
                pp.pos = 0;
                pp.src = cond;
                if (!tk_next(&pp)) return false;
                int a = -1;
                if (!parse_expr(&pp, &a)) return false;
                node = node_new(pi, strcmp(word, "if") == 0 ? N_IF : N_ELIF);
                if (node < 0) return false;
                pi->pool[node].a = a;
            } else if (strcmp(word, "else") == 0) {
                node = node_new(pi, N_ELSE);
                if (node < 0) return false;
            } else if (strcmp(word, "while") == 0) {
                char cond[96];
                int c = 0;
                p++;
                for (; ln[p] != ':' && ln[p] != '\0' && c < 94; p++) cond[c++] = ln[p];
                cond[c] = '\0';
                Parser pp;
                pp.pi = pi;
                pp.pos = 0;
                pp.src = cond;
                if (!tk_next(&pp)) return false;
                int a = -1;
                if (!parse_expr(&pp, &a)) return false;
                node = node_new(pi, N_WHILE);
                if (node < 0) return false;
                pi->pool[node].a = a;
            } else if (strcmp(word, "for") == 0) {
                // for <name> in range(...):
                char name[24];
                int q = 0;
                p++;
                while (ln[p] == ' ') p++;
                for (; ln[p] != '\0' && ln[p] != ' ' && q < 22; p++) name[q++] = ln[p];
                name[q] = '\0';
                while (ln[p] == ' ') p++;
                if (!(ln[p] == 'i' && ln[p + 1] == 'n' && (ln[p + 2] == ' ' || ln[p + 2] == '\0'))) {
                    pyerr(pi, "for supports 'for x in range(...):' only");
                    return false;
                }
                p += 2;
                while (ln[p] == ' ') p++;
                char rexpr[96];
                int c = 0;
                for (; ln[p] != ':' && ln[p] != '\0' && c < 94; p++) rexpr[c++] = ln[p];
                rexpr[c] = '\0';
                Parser pp;
                pp.pi = pi;
                pp.pos = 0;
                pp.src = rexpr;
                if (!tk_next(&pp)) return false;
                int a = -1;
                if (!parse_expr(&pp, &a)) return false;
                node = node_new(pi, N_FOR);
                if (node < 0) return false;
                node_set_str(&pi->pool[node], name);
                pi->pool[node].a = a;
            } else if (strcmp(word, "def") == 0) {
                // def <name>(<params>):
                char fname[24];
                int q = 0;
                p++;
                while (ln[p] == ' ') p++;
                for (; ln[p] != '\0' && ln[p] != '(' && q < 22; p++) fname[q++] = ln[p];
                fname[q] = '\0';
                if (ln[p] != '(') { pyerr(pi, "def needs '('"); return false; }
                p++;
                // params are allocated as CONSECUTIVE nodes rooted at 'c'
                int pfirst = pi->pn;   // plan: node_new for the header comes first
                node = node_new(pi, N_DEF);
                if (node < 0) return false;
                node_set_str(&pi->pool[node], fname);
                pi->pool[node].iv = 0;
                pi->pool[node].c = -1;
                int np = 0;
                for (;;) {
                    while (ln[p] == ' ') p++;
                    if (ln[p] == ')') break;
                    if (np >= 4) { pyerr(pi, "max 4 parameters per function"); return false; }
                    char pname[24];
                    int q2 = 0;
                    while (ln[p] != '\0' && ln[p] != ',' && ln[p] != ')' && q2 < 22) {
                        if (ln[p] != ' ') pname[q2++] = ln[p];
                        p++;
                    }
                    pname[q2] = '\0';
                    if (pname[0] == '\0') { pyerr(pi, "bad parameter"); return false; }
                    int pk = node_new(pi, N_PASS);
                    if (pk < 0) return false;
                    node_set_str(&pi->pool[pk], pname);
                    if (np == 0) pi->pool[node].c = pk;
                    pi->pool[node].iv = np + 1;
                    np++;
                    while (ln[p] == ' ') p++;
                    if (ln[p] == ',') { p++; continue; }
                    break;
                }
                while (ln[p] == ' ') p++;
                if (ln[p] != ')') { pyerr(pi, "expected ')' in def"); return false; }
                (void)pfirst;
            } else {
                pyerr(pi, "unknown block (if/elif/else/while/for/def only)");
                return false;
            }
            if (node < 0) return false;
            // body: following lines with greater indent
            int j = i + 1;
            while (j < L->n && L->l[j][0] == '\0') j++;   // skip blanks
            if (j < L->n && L->ind[j] > indent) {
                int body_indent = L->ind[j];
                int body = -1;
                if (!parse_block_lines(pi, L, j, body_indent, &j, &body)) return false;
                pi->pool[node].b = body;                    // body always in b
            } else {
                pyerr(pi, "expected an indented block");
                return false;
            }
            st = node;
            i = j;
        } else {
            if (!parse_simple_stmt(pi, ln, &st)) return false;
            i++;
        }
        if (st >= 0) {
            // if/elif/else chains are linked through 'c'; the statement chain
            // keeps 'next' pointing from the FIRST if to the following stmt
            bool st_is_if    = (pi->pool[st].k == N_IF);
            bool st_is_elif  = (pi->pool[st].k == N_ELIF);
            bool st_is_else  = (pi->pool[st].k == N_ELSE);
            if (st_is_elif || st_is_else) {
                if (if_tail < 0 || pi->pool[if_tail].k == N_ELSE) {
                    pyerr(pi, st_is_else ? "'else' without 'if'" : "'elif' without 'if'");
                    return false;
                }
                pi->pool[if_tail].c = st;
                if_tail = st;
            } else {
                // a real statement (N_IF opens a new chain)
                if (head < 0) head = st;
                else pi->pool[stmt_tail].next = st;
                stmt_tail = st;
                if_tail = st_is_if ? st : -1;
            }
        }
    }
    *stop = i;
    *out = head;
    return true;
}

// ---------------- pending multi-line block state machine ----------------
// Headers (if/while/for/def) open a pending block; indented lines append;
// elif/else at the same header indent extends an if-chain; a line at or
// below the header indent (or a blank line in REPL) runs the block.

// flatten the collected segments into one line array + run it
static bool run_pending(PyInterp *pi) {
    BLines L;
    L.l = pi->rl;
    L.ind = pi->rind;
    L.n = 0;
    for (int s = 0; s < pi->nseg && L.n < 126; s++) {
        // header line at exactly pend_indent
        int k = 0;
        for (; pi->segs[s].header[k] != '\0' && k < 94; k++) pi->rl[L.n][k] = pi->segs[s].header[k];
        pi->rl[L.n][k] = '\0';
        pi->rind[L.n] = pi->pend_indent;
        L.n++;
        for (int b = 0; b < pi->segs[s].bn && L.n < 126; b++) {
            k = 0;
            for (; pi->segs[s].body[b][k] != '\0' && k < 94; k++) pi->rl[L.n][k] = pi->segs[s].body[b][k];
            pi->rl[L.n][k] = '\0';
            pi->rind[L.n] = pi->segs[s].bind[b];
            L.n++;
        }
    }
    int chain = -1, stop = 0;
    if (!parse_block_lines(pi, &L, 0, pi->pend_indent, &stop, &chain)) return false;
    if (chain < 0) return true;   // block with only comments
    // execute at global scope
    pi->brk = pi->cont = pi->ret = 0;
    VVal dummy;
    if (!exec_stmt(pi, chain, &dummy)) return false;
    return true;
}

// register a def (executed N_DEF)
static bool register_def(PyInterp *pi, PNode *n) {
    if (pi->nfn >= PY_FUNCS) { pyerr(pi, "too many functions (12 max)"); return false; }
    PyFn *f = &pi->fns[pi->nfn];
    int k = 0;
    for (; n->sv[k] != '\0' && k < 22; k++) f->name[k] = n->sv[k];
    f->name[k] = '\0';
    f->nparam = (int)n->iv;
    for (int p = 0; p < f->nparam; p++) {
        int pk = n->c + p;   // params were allocated consecutively
        int q = 0;
        for (; pi->pool[pk].sv[q] != '\0' && q < 22; q++) f->params[p][q] = pi->pool[pk].sv[q];
        f->params[p][q] = '\0';
    }
    f->body = n->b;
    pi->nfn++;
    return true;
}

static bool is_elif_else_line(const char *s) {
    int p = 0;
    while (s[p] == ' ') p++;
    if (s[p] == 'e') {
        if (s[p + 1] == 'l' && s[p + 2] == 's' && s[p + 3] == 'e' &&
            (s[p + 4] == ':' || s[p + 4] == '\0' || s[p + 4] == ' ')) return true;
        if (s[p + 1] == 'l' && s[p + 2] == 'i' && s[p + 3] == 'f' &&
            (s[p + 4] == ' ' || s[p + 4] == '\0')) return true;
    }
    return false;
}

static bool find_top_colon(const char *s, int *pos) {
    int depth = 0;
    bool in_dq = false;
    for (int i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (in_dq) {
            if (c == '"') in_dq = false;
            continue;
        }
        if (c == '"') { in_dq = true; continue; }
        if (c == '(') depth++;
        else if (c == ')') depth--;
        else if (c == ':' && depth == 0) { *pos = i; return true; }
        else if (c == '#') return false;
    }
    return false;
}

static bool header_keyword_ok(const char *s) {
    static const char *kw[6] = { "if", "elif", "else", "while", "for", "def" };
    int p = 0;
    while (s[p] == ' ') p++;
    for (int i = 0; i < 6; i++) {
        int k = 0;
        for (; kw[i][k] != '\0'; k++) {
            if (s[p + k] != kw[i][k]) break;
        }
        if (kw[i][k] == '\0') {
            char n = s[p + k];
            if (n == ' ' || n == '\0' || n == '(') return true;
        }
    }
    return false;
}

// run one complete single-line statement (or one-line suite)
static int py_run_line(PyInterp *pi, const char *trimmed, int indent) {
    (void)indent;
    if (trimmed[0] == '#' || trimmed[0] == '\0') return PY_DONE;
    int cpos = -1;
    if (find_top_colon(trimmed, &cpos)) {
        if (!header_keyword_ok(trimmed)) { pyerr(pi, "invalid syntax"); return PY_ERR; }
        // inline suite: "if x: print(1)" etc. -> header + one-statement body
        char lines[2][96];
        int ind2[2];
        int hlen = cpos < 94 ? cpos : 94;
        int k = 0;
        for (; k < hlen; k++) lines[0][k] = trimmed[k];
        lines[0][k] = '\0';
        const char *rest = trimmed + cpos + 1;
        int rlen = 0;
        while (*rest == ' ') rest++;
        for (; *rest != '\0' && rlen < 94; rest++) lines[1][rlen++] = *rest;
        lines[1][rlen] = '\0';
        if (rlen == 0) { pyerr(pi, "expected a statement after ':'"); return PY_ERR; }
        ind2[0] = 0;
        ind2[1] = 4;
        BLines L;
        L.l = lines;
        L.ind = ind2;
        L.n = 2;
        int chain = -1, stop = 0;
        if (!parse_block_lines(pi, &L, 0, 0, &stop, &chain)) return PY_ERR;
        pi->brk = pi->cont = pi->ret = 0;
        VVal dummy;
        if (!exec_stmt(pi, chain, &dummy)) return PY_ERR;
        if (pi->exitreq) return PY_EXIT;
        return PY_DONE;
    }
    int st = -1;
    if (!parse_simple_stmt(pi, trimmed, &st)) return PY_ERR;
    pi->brk = pi->cont = pi->ret = 0;
    VVal dummy;
    if (!exec_stmt(pi, st, &dummy)) return PY_ERR;
    if (pi->exitreq) return PY_EXIT;
    return PY_DONE;
}

int py_line(void *handle, const char *line, void (*out)(void *ud, const char *s, int tag), void *ud) {
    PyInterp *pi = (PyInterp *)handle;
    if (!pi) return PY_ERR;
    pi->out = out;
    pi->ud = ud;
    pi->lineno++;

    // normalize: strip trailing whitespace, count indent
    char trimmed[96];
    int indent = 0;
    int i = 0;
    while (line[i] == ' ' || line[i] == '\t') { indent += (line[i] == '\t') ? 4 : 1; i++; }
    int t = 0;
    int end = 0;
    while (line[end] != '\0') end++;
    while (end > i && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r' || line[end - 1] == '\n')) end--;
    for (; i < end && t < 94; i++) trimmed[t++] = line[i];
    trimmed[t] = '\0';

    if (pi->pend_open) {
        if (trimmed[0] == '\0' || (trimmed[0] == '#' )) {
            if (pi->repl) {
                pi->pend_open = false;
                pi->nseg = 0;
                if (!run_pending(pi)) {
                    char msg[96];
                    int k = 0;
                    const char *p1 = "line: ";
                    for (; p1[k] != '\0'; k++) msg[k] = p1[k];
                    char num[12];
                    int q = 0;
                    put_num(num, q, pi->lineno);
                    for (int z = 0; num[z] != '\0' && k < 80; z++) msg[k++] = num[z];
                    if (k < 90) msg[k++] = ' ';
                    for (int z = 0; pi->errmsg[z] != '\0' && k < 90; z++) msg[k++] = pi->errmsg[z];
                    msg[k] = '\0';
                    out(ud, msg, TC_ERR);
                    out(ud, "\n", TC_ERR);
                    pi->err = false;
                    return PY_ERR;
                }
                if (pi->exitreq) return PY_EXIT;
                return PY_DONE;
            }
            return PY_MORE;   // blank line inside a script block: keep going
        }
        if (indent > pi->pend_indent) {
            if (pi->nseg > 0 && pi->segs[pi->nseg - 1].bn < 40) {
                int b = pi->segs[pi->nseg - 1].bn;
                int k = 0;
                for (; trimmed[k] != '\0' && k < 94; k++) pi->segs[pi->nseg - 1].body[b][k] = trimmed[k];
                pi->segs[pi->nseg - 1].body[b][k] = '\0';
                pi->segs[pi->nseg - 1].bind[b] = indent;
                pi->segs[pi->nseg - 1].bn++;
                return PY_MORE;
            }
            {
                out(ud, "block too long (40 lines max)\n", TC_ERR);
                pi->pend_open = false;
                pi->nseg = 0;
                return PY_ERR;
            }
        }
        if (indent == pi->pend_indent && is_elif_else_line(trimmed)) {
            // extends the if-chain with a new segment
            if (pi->nseg < 8) {
                int k = 0;
                for (; trimmed[k] != '\0' && k < 94; k++) pi->segs[pi->nseg].header[k] = trimmed[k];
                pi->segs[pi->nseg].header[k] = '\0';
                pi->segs[pi->nseg].bn = 0;
                pi->nseg++;
                return PY_MORE;
            }
            out(ud, "if-chain too long (8 branches max)\n", TC_ERR);
            pi->pend_open = false;
            pi->nseg = 0;
            return PY_ERR;
        }
        // block finished: run it, then process this line as a fresh statement
        pi->pend_open = false;
        pi->nseg = 0;
        if (!run_pending(pi)) {
            char msg[96];
            int k = 0;
            const char *p1 = "line: ";
            for (; p1[k] != '\0'; k++) msg[k] = p1[k];
            char num[12];
            int q = 0;
            put_num(num, q, pi->lineno);
            for (int z = 0; num[z] != '\0' && k < 80; z++) msg[k++] = num[z];
            if (k < 90) msg[k++] = ' ';
            for (int z = 0; pi->errmsg[z] != '\0' && k < 90; z++) msg[k++] = pi->errmsg[z];
            msg[k] = '\0';
            out(ud, msg, TC_ERR);
            out(ud, "\n", TC_ERR);
            pi->err = false;
            return PY_ERR;
        }
        if (pi->exitreq) return PY_EXIT;
        // fall through: handle this line as a new statement below
    }

    if (trimmed[0] == '\0' || trimmed[0] == '#') return PY_DONE;

    int cpos = -1;
    bool has_colon = find_top_colon(trimmed, &cpos);
    bool colon_end = has_colon && trimmed[cpos + 1] == '\0';
    if (colon_end) {
        if (!header_keyword_ok(trimmed)) {
            out(ud, "invalid syntax: block header must be if/elif/else/while/for/def\n", TC_ERR);
            return PY_ERR;
        }
        if (pi->nseg >= 8) {
            out(ud, "block too complex\n", TC_ERR);
            return PY_ERR;
        }
        int k = 0;
        for (; trimmed[k] != '\0' && k < 94; k++) pi->segs[0].header[k] = trimmed[k];
        pi->segs[0].header[k] = '\0';
        pi->segs[0].bn = 0;
        pi->nseg = 1;
        pi->pend_open = true;
        pi->pend_indent = indent;
        return PY_MORE;
    }

    int rc = py_run_line(pi, trimmed, indent);
    if (rc == PY_ERR) {
        char msg[96];
        int k = 0;
        const char *p1 = "line: ";
        for (; p1[k] != '\0'; k++) msg[k] = p1[k];
        char num[12];
        int q = 0;
        put_num(num, q, pi->lineno);
        for (int z = 0; num[z] != '\0' && k < 80; z++) msg[k++] = num[z];
        if (k < 90) msg[k++] = ' ';
        for (int z = 0; pi->errmsg[z] != '\0' && k < 90; z++) msg[k++] = pi->errmsg[z];
        msg[k] = '\0';
        out(ud, msg, TC_ERR);
        out(ud, "\n", TC_ERR);
        pi->err = false;
    }
    return rc;
}

void *py_new() {
    PyInterp *pi = new PyInterp();
    if (!pi) return 0;
    pi->pn = 0;
    pi->depth = 0;
    pi->nfn = 0;
    pi->pend_open = false;
    pi->pend_kind = 0;
    pi->pend_indent = 0;
    pi->nseg = 0;
    pi->repl = true;
    pi->steps = 0;
    pi->brk = pi->cont = pi->ret = 0;
    pi->retval.t = VT_INT;
    pi->retval.i = 0;
    pi->retval.s[0] = '\0';
    pi->exitreq = false;
    pi->err = false;
    pi->errmsg[0] = '\0';
    pi->lineno = 0;
    pi->out = 0;
    pi->ud = 0;
    for (int s = 0; s < PY_SCOPES; s++) pi->sc[s].n = 0;
    return pi;
}

void py_free(void *handle) {
    PyInterp *pi = (PyInterp *)handle;
    if (pi) delete pi;
}

const char *py_banner_line(int i) {
    switch (i) {
    case 0: return "Kian mini-python 0.1 (Python-subset interpreter)";
    case 1: return "integers only - no floats, imports, lists, dicts, classes or input()";
    case 2: return "type exit() or quit() to leave; Ctrl+C aborts";
    default: return "";
    }
}

bool py_pending(void *pi) {
    PyInterp *p = (PyInterp *)pi;
    return p ? p->pend_open : false;
}

void py_set_repl(void *pi, bool repl) {
    PyInterp *p = (PyInterp *)pi;
    if (p) p->repl = repl;
}
