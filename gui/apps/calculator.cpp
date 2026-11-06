// Kian OS GUI - Calculator: 4x5 button grid, 64-bit integer math with a
// 4-decimal fixed-point display (values scaled by 10000). Keyboard input too.
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../util.h"

// value scale: 4 decimals (0.4 fixed point)
const long long CALC_SCALE = 10000;

struct CalcApp {
    long long acc;      // scaled
    long long entry;    // scaled value parsed from est[]
    char est[24];       // digits being typed ('-', digits, one '.')
    bool entering;      // typing digits into entry
    int op;             // 0 none, 1 +, 2 -, 3 *, 4 /
    bool fresh;         // next digit starts a new entry
    bool err;
    int hover_btn;
    char display[40];
};

static const int P10[5] = { 1, 10, 100, 1000, 10000 };

// parse the typed entry string into CALC_SCALE fixed point (no float)
static long long calc_parse(const char *s) {
    long long v = 0;
    int frac = -1;                     // -1 = integer part, else decimals typed
    bool neg = false;
    for (int i = 0; s[i] != '\0'; i++) {
        char ch = s[i];
        if (ch == '-') { neg = true; continue; }
        if (ch == '.') { frac = 0; continue; }
        if (ch < '0' || ch > '9') continue;
        int d = ch - '0';
        if (frac < 0) {
            if (v < (1LL << 60) / 10) v = v * 10 + (long long)d * CALC_SCALE;
        } else if (frac < 4) {
            v += (long long)d * (CALC_SCALE / P10[frac + 1]);
            frac++;
        }
    }
    return neg ? -v : v;
}

static void calc_fmt(CalcApp *c) {
    long long v = c->err ? 0 : (c->entering ? c->entry : c->acc);
    bool neg = v < 0;
    unsigned long long m = neg ? (unsigned long long)(-v) : (unsigned long long)v;
    unsigned long long ip = m / CALC_SCALE;
    unsigned long long fp = m % CALC_SCALE;
    char *s = c->display;
    int i = 0;
    if (neg) s[i++] = '-';
    // integer part
    char tmp[24];
    int n = 0;
    if (ip == 0) tmp[n++] = '0';
    while (ip > 0 && n < 22) { tmp[n++] = (char)('0' + ip % 10); ip /= 10; }
    while (n > 0 && i < 36) s[i++] = tmp[--n];
    if (i < 34) {
        s[i++] = '.';
        long long f = fp;
        for (int d = 0; d < 4 && i < 36; d++) {
            f *= 10;
            int digit = (int)(f / CALC_SCALE);
            s[i++] = (char)('0' + digit);
            f %= CALC_SCALE;
        }
    }
    s[i] = '\0';
    if (c->err) {
        const char *e = "Error";
        int k = 0;
        for (; e[k] != '\0' && k < 39; k++) s[k] = e[k];
        s[k] = '\0';
    }
}

// Manual 128-bit unsigned division by a 64-bit divisor (schoolbook on bits).
// Needed because __int128 "/" would emit a libgcc __divti3 call, which does
// not exist in a -nostdlib kernel. Quotient must fit in 64 bits.
static unsigned long long udiv128(unsigned long long hi, unsigned long long lo, unsigned long long d) {
    unsigned long long q = 0, rem = 0;
    if (d == 0) return 0;
    for (int i = 127; i >= 0; i--) {
        unsigned long long bit = (i >= 64) ? ((hi >> (i - 64)) & 1ULL) : ((lo >> i) & 1ULL);
        rem = (rem << 1) | bit;   // rem < d keeps this safe
        q <<= 1;
        if (rem >= d) { rem -= d; q |= 1ULL; }
    }
    return q;
}

// (a * b) / CALC_SCALE without libgcc: multiply in 128-bit (inlines to mulq),
// divide with our own bit-wise routine.
static long long calc_muldiv(long long a, long long b) {
    __int128 p = (__int128)a * ( __int128)b;
    bool neg = p < 0;
    unsigned __int128 up = (unsigned __int128)p;
    if (neg) up = (~up) + 1;
    unsigned long long hi = (unsigned long long)(up >> 64);
    unsigned long long lo = (unsigned long long)up;
    unsigned long long q = udiv128(hi, lo, (unsigned long long)CALC_SCALE);
    return neg ? -(long long)q : (long long)q;
}

// (a * CALC_SCALE) / v without libgcc.
static long long calc_divscaled(long long a, long long v) {
    if (v == 0) return 0;
    __int128 p = (__int128)a * ( __int128)CALC_SCALE;
    bool neg = p < 0;
    unsigned __int128 up = (unsigned __int128)p;
    if (neg) up = (~up) + 1;
    unsigned long long hi = (unsigned long long)(up >> 64);
    unsigned long long lo = (unsigned long long)up;
    unsigned long long q = udiv128(hi, lo, (unsigned long long)v);
    return neg ? -(long long)q : (long long)q;
}

static void calc_apply(CalcApp *c, int op, long long v) {
    switch (op) {
    case 1: c->acc = c->acc + v; break;
    case 2: c->acc = c->acc - v; break;
    case 3: c->acc = calc_muldiv(c->acc, v); break;
    case 4:
        if (v == 0) { c->err = true; c->acc = 0; }
        else c->acc = calc_divscaled(c->acc, v);
        break;
    default: c->acc = v; break;
    }
}

static const char *CALC_LBL[5][4] = {
    { "C", "CE", "<-", "/" },
    { "7", "8", "9", "*" },
    { "4", "5", "6", "-" },
    { "1", "2", "3", "+" },
    { "0", ".", "+/-", "=" }
};

static void calc_press(CalcApp *c, int idx) {
    if (idx < 0 || idx > 19) return;
    int r = idx / 4, col = idx % 4;
    const char *l = CALC_LBL[r][col];
    if (c->err && strcmp(l, "C") != 0 && strcmp(l, "CE") != 0) return;
    int elen = (int)strlen(c->est);
    if (l[0] >= '0' && l[0] <= '9' && l[1] == '\0') {
        // cap: 15 digits before the dot, 4 after (display is 4-dec)
        bool dot = false;
        int intd = 0;
        for (int i = 0; c->est[i] != '\0'; i++) {
            if (c->est[i] == '.') dot = true; else if (!dot) intd++;
        }
        if (elen < 22 && (dot || intd < 15)) {
            c->est[elen] = l[0];
            c->est[elen + 1] = '\0';
            c->entry = calc_parse(c->est);
            c->entering = true;
            c->fresh = false;
        }
    } else if (strcmp(l, ".") == 0) {
        if (elen > 0 && elen < 21) {
            bool has_dot = false;
            for (int i = 0; c->est[i] != '\0'; i++) if (c->est[i] == '.') has_dot = true;
            if (!has_dot) {
                c->est[elen] = '.';
                c->est[elen + 1] = '\0';
                c->entry = calc_parse(c->est);
                c->entering = true;
                c->fresh = false;
            }
        }
    } else if (strcmp(l, "+/-") == 0) {
        if (c->entering) {
            if (c->est[0] == '-') {
                for (int i = 0; c->est[i] != '\0'; i++) c->est[i] = c->est[i + 1];
            } else if (elen > 0 && elen < 22) {
                for (int i = elen; i >= 0; i--) c->est[i + 1] = c->est[i];
                c->est[0] = '-';
            }
            c->entry = calc_parse(c->est);
        } else {
            c->acc = -c->acc;
        }
    } else if (strcmp(l, "C") == 0) {
        c->acc = 0; c->entry = 0; c->est[0] = '\0';
        c->op = 0; c->entering = false; c->fresh = true; c->err = false;
    } else if (strcmp(l, "CE") == 0) {
        c->est[0] = '\0'; c->entry = 0;
        c->entering = false; c->fresh = true;
    } else if (strcmp(l, "<-") == 0) {
        if (c->entering && elen > 0) {
            c->est[elen - 1] = '\0';
            c->entry = calc_parse(c->est);
            if (c->est[0] == '\0') { c->entering = false; c->fresh = true; }
        }
    } else if (strcmp(l, "=") == 0) {
        long long v = c->entering ? c->entry : c->acc;
        calc_apply(c, c->op, v);
        c->op = 0; c->entering = false; c->est[0] = '\0'; c->fresh = true;
    } else {
        // operator
        long long v = c->entering ? c->entry : c->acc;
        if (c->op != 0 && !c->fresh) calc_apply(c, c->op, v);
        else c->acc = v;
        c->op = (l[0] == '+') ? 1 : (l[0] == '-') ? 2 : (l[0] == '*') ? 3 : 4;
        c->entering = false;
        c->est[0] = '\0';
        c->fresh = true;
    }
    calc_fmt(c);
}

void *calc_create() {
    CalcApp *c = new CalcApp();
    c->acc = 0; c->entry = 0; c->est[0] = '\0'; c->entering = false;
    c->op = 0; c->fresh = true; c->err = false;
    c->hover_btn = -1;
    c->display[0] = '\0';
    calc_fmt(c);
    return c;
}

void calc_destroy(void *inst) {
    CalcApp *c = (CalcApp *)inst;
    delete c;
}

void calc_draw(void *inst, Rect r) {
    CalcApp *c = (CalcApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);
    // display
    fill_round_rect(r.x + 8, r.y + 8, r.w - 16, 34, 6, THEME->input_bg);
    int dw = text_width(c->display, 1);
    draw_text(r.x + r.w - 14 - dw, text_center_y(r.y + 8, 34, 1), c->display, THEME->text, 1);
    // grid: 4x5
    int gx = r.x + 8, gy = r.y + 50;
    int bw = (r.w - 16 - 12) / 4, bh = (r.h - 58 - 16) / 5;
    for (int i = 0; i < 20; i++) {
        int row = i / 4, col = i % 4;
        int bx = gx + col * (bw + 4), by = gy + row * (bh + 4);
        bool op_btn = (i >= 3 && i <= 7) || i == 11 || i == 15 || i == 19;
        uint32_t bg = (row == 0) ? THEME->icon_bg : op_btn ? THEME->start_bg : THEME->button_bg;
        if (c->hover_btn == i) bg = THEME->start_bg_active;
        fill_round_rect(bx, by, bw, bh, 6, bg);
        draw_text(bx + (bw - text_width(CALC_LBL[row][col], 1)) / 2, text_center_y(by, bh, 1), CALC_LBL[row][col], THEME->button_fg, 1);
    }
}

static int calc_hit(Rect r, int mx, int my) {
    int gx = r.x + 8, gy = r.y + 50;
    int bw = (r.w - 16 - 12) / 4, bh = (r.h - 58 - 16) / 5;
    for (int i = 0; i < 20; i++) {
        int row = i / 4, col = i % 4;
        if (mx >= gx + col * (bw + 4) && mx < gx + col * (bw + 4) + bw &&
            my >= gy + row * (bh + 4) && my < gy + row * (bh + 4) + bh) return i;
    }
    return -1;
}

void calc_event(void *inst, const kian_event_t *e, Rect r) {
    CalcApp *c = (CalcApp *)inst;
    if (e->type == KIAN_EV_MOUSE_MOVE) {
        c->hover_btn = calc_hit(r, IN->mx, IN->my);
        shell_mark_content();
        return;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        int idx = calc_hit(r, IN->mx, IN->my);
        if (idx >= 0) { calc_press(c, idx); shell_mark_content(); }
        return;
    }
    if (e->type != KIAN_EV_KEY_DOWN) return;
    char ch = (char)e->b;
    int idx = -1;
    if (ch >= '0' && ch <= '9') {
        int row = (ch == '0') ? 4 : (ch <= '3') ? 3 : (ch <= '6') ? 2 : 1;
        int col = (ch == '0') ? 0 : 3 - ((('9' - ch)) % 3);
        // map digit to grid index: 7 8 9 / 4 5 6 / 1 2 3 / 0
        if (ch == '7') idx = 4; else if (ch == '8') idx = 5; else if (ch == '9') idx = 6;
        else if (ch == '4') idx = 8; else if (ch == '5') idx = 9; else if (ch == '6') idx = 10;
        else if (ch == '1') idx = 12; else if (ch == '2') idx = 13; else if (ch == '3') idx = 14;
        else if (ch == '0') idx = 16;
        (void)row; (void)col;
    } else if (ch == '+') idx = 15;
    else if (ch == '-') idx = 11;
    else if (ch == '*') idx = 7;
    else if (ch == '/') idx = 3;
    else if (ch == '\n' || ch == '=') idx = 19;
    else if (ch == 8) idx = 2;
    else if (ch == 27) idx = 0;
    else if (ch == '.') idx = 17;
    if (idx >= 0) { calc_press(c, idx); shell_mark_content(); }
}
