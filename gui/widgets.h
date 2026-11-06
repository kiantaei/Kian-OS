// Kian OS GUI - widgets. Plain structs + free functions, NO virtual
// functions, NO std::function. Each widget has draw + event functions.
#pragma once
#include <kian_abi.h>
#include "gfx.h"

// ---- Button ----
struct Btn {
    int x, y, w, h;
    const char *label;
    int icon;            // IC_* or IC_NONE
    bool hover, armed, disabled;
};
void btn_init(Btn *b, int x, int y, int w, int h, const char *label, int icon);
void btn_draw(const Btn *b);
bool btn_event(Btn *b, const kian_event_t *e);   // 1 on click

// ---- Label (thin wrapper) ----
void label_draw(int x, int y, const char *s, uint32_t color, int scale, bool bold);

// ---- TextBox (single line, caret, optional password dots) ----
struct TBox {
    char buf[128];
    int len, caret;
    int x, y, w, h;
    bool focused;
    bool password;
    unsigned err_until;  // shake/red until this uptime ms
};
void tbox_init(TBox *t, int x, int y, int w, int h, bool password);
void tbox_set_text(TBox *t, const char *s);
const char *tbox_text(const TBox *t);
void tbox_draw(const TBox *t);
void tbox_focus(TBox *t, bool on);
bool tbox_key(TBox *t, const kian_event_t *e);   // 1 = Enter pressed
bool tbox_mouse(TBox *t, const kian_event_t *e); // click: focus + place caret

// ---- Checkbox ----
struct Chk {
    int x, y, w;         // w = total width for hover, height = 20
    const char *label;
    bool checked;
    bool hover;
};
void chk_init(Chk *c, int x, int y, const char *label);
void chk_draw(const Chk *c);
bool chk_event(Chk *c, const kian_event_t *e);   // 1 on toggle

// ---- Slider (horizontal, 0..100) ----
struct Slider {
    int x, y, w;         // track height 6, knob 14, total height ~20
    int value;
    bool dragging;
};
void slider_init(Slider *s, int x, int y, int w, int value);
void slider_draw(const Slider *s);
bool slider_event(Slider *s, const kian_event_t *e); // 1 if value changed

// ---- ListView (scrollable rows) ----
struct ListView {
    int x, y, w, h;
    int row_h;
    int n;
    int offset;          // first visible row
    int selected;        // -1 none
    int wheel_lines;
};
void listv_init(ListView *lv, int x, int y, int w, int h, int row_h);
void listv_set_count(ListView *lv, int n);
bool listv_event(ListView *lv, const kian_event_t *e); // wheel/click; updates selected/offset
void listv_draw(const ListView *lv, const char *const *labels);
bool listv_dbl_hit(const ListView *lv, int mx, int my); // row under mouse

// ---- ScrollBar (vertical) ----
struct SBar {
    int x, y, h;
    int total, visible, offset;
    bool dragging;
    int grab_dy;
};
void sbar_set(SBar *sb, int total, int visible, int offset);
void sbar_draw(const SBar *sb);
bool sbar_event(SBar *sb, const kian_event_t *e); // drag; 1 if offset changed
int  sbar_track_len(const SBar *sb);

// ---- Menu / ContextMenu (single active instance lives in Shell) ----
struct MenuItem { const char *label; int id; };  // id < 0 -> separator
struct Menu {
    bool open;
    int x, y, w;
    MenuItem items[12];
    int n;
    int hover;           // index or -1
    void (*cb)(void *ud, int id);
    void *ud;
};
void menu_open(Menu *m, const MenuItem *items, int n, void (*cb)(void *, int), void *ud, int x, int y);
void menu_close(Menu *m);
void menu_draw(const Menu *m);
bool menu_event(Menu *m, const kian_event_t *e); // 1 if the event was consumed

// ---- Tooltip ----
void tooltip_draw(int x, int y, const char *text);

// ---- TabBar ----
struct TabBar {
    int x, y, w;
    const char *const *tabs;
    int n;
    int active;
};
void tabbar_draw(const TabBar *tb);
int  tabbar_hit(const TabBar *tb, int mx, int my); // -1 none
