// Kian OS GUI - Task Manager: open windows + End task, uptime, heap usage,
// CPU graph (honest placeholder).
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../util.h"

struct TaskMgrApp {
    ListView list;
    int sel;
    char names[32][36];
    const char *ptrs[32];
    int ids[32];
    int n;
};

void *taskmgr_create() {
    TaskMgrApp *t = new TaskMgrApp();
    listv_init(&t->list, 0, 0, 300, 200, 22);
    t->sel = -1;
    t->n = 0;
    return t;
}

void taskmgr_destroy(void *inst) {
    TaskMgrApp *t = (TaskMgrApp *)inst;
    delete t;
}

static void taskmgr_refresh(TaskMgrApp *t) {
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS && n < 32; i++) {
        Window *w = &SH->wm.win[i];
        if (!w->used) continue;
        int k = 0;
        const char *src = w->title;
        for (; src[k] != '\0' && k < 34; k++) t->names[n][k] = src[k];
        t->names[n][k] = '\0';
        t->ptrs[n] = t->names[n];
        t->ids[n] = w->id;
        n++;
    }
    t->n = n;
    listv_set_count(&t->list, n);
    if (t->sel >= n) t->sel = -1;
    t->list.selected = t->sel;
}

void taskmgr_draw(void *inst, Rect r) {
    TaskMgrApp *t = (TaskMgrApp *)inst;
    taskmgr_refresh(t);
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);

    draw_text_bold(r.x + 10, r.y + 8, "Task Manager", THEME->text, 2);

    // window list
    Rect lr = { r.x + 10, r.y + 40, r.w - 20, 140 };
    t->list.x = lr.x;
    t->list.y = lr.y;
    t->list.w = lr.w;
    t->list.h = lr.h;
    clip_set(lr);
    listv_draw(&t->list, t->ptrs);
    clip_reset();
    t->sel = t->list.selected;

    // End task button
    bool hov = mouse_in(r.x + 10, r.y + 188, 100, 26);
    fill_round_rect(r.x + 10, r.y + 188, 100, 26, 6, hov ? THEME->close_btn_bg : THEME->button_bg);
    draw_text(r.x + 30, text_center_y(r.y + 188, 26, 1), "End task", THEME->button_fg, 1);

    // stats
    uint64_t used = 0, total = 0;
    kian_mem_stats(&used, &total);
    unsigned long long s = kian_uptime_ms() / 1000;
    char line[96];
    int k = 0;
    line[0] = '\0';
    const char *p1 = "Up ";
    for (; *p1 != '\0'; p1++) line[k++] = *p1;
    put_num(line, k, (int)(s / 3600));
    line[k++] = 'h';
    const char *p2 = "  Heap: ";
    for (; *p2 != '\0' && k < 80; p2++) line[k++] = *p2;
    put_num(line, k, (int)(used >> 20));
    line[k++] = '/';
    put_num(line, k, (int)(total >> 20));
    line[k++] = ' ';
    line[k++] = 'M';
    line[k++] = 'i';
    line[k++] = 'B';
    line[k] = '\0';
    draw_text(r.x + 120, text_center_y(r.y + 188, 26, 1), line, THEME->text_dim, 1);

    // CPU graph placeholder (clearly labeled)
    Rect gr = { r.x + 10, r.y + 226, r.w - 20, r.h - 240 };
    if (gr.h > 20) {
        draw_text(r.x + 10, gr.y - 12, "CPU (placeholder graph - no timers yet)", THEME->text_dim, 1);
        fill_round_rect(gr.x, gr.y, gr.w, gr.h, 4, THEME->input_bg);
        int bars = gr.w / 12;
        for (int i = 0; i < bars; i++) {
            int seed = (int)((kian_uptime_ms() / 500) + i * 7);
            int hgt = 6 + (seed * 31) % (imax(1, gr.h - 8));
            fill_rect(gr.x + 4 + i * 12, gr.y + gr.h - 4 - hgt, 8, hgt, THEME->accent);
        }
    }
}

void taskmgr_event(void *inst, const kian_event_t *e, Rect r) {
    TaskMgrApp *t = (TaskMgrApp *)inst;
    if (listv_event(&t->list, e)) t->sel = t->list.selected;
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l && mouse_in(r.x + 10, r.y + 188, 100, 26)) {
        if (t->sel >= 0 && t->sel < t->n) {
            wm_close_id(&SH->wm, t->ids[t->sel]);
            t->sel = -1;
            shell_full_redraw();
        }
    }
}
