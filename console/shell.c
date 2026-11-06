/* Kian OS - login prompt + "ksh" text shell (C, freestanding).
 * Reads input with kian_poll_event (no interrupts), logs in against the
 * Rust kernel's SHA-256 password check, and offers the ksh built-ins. */

#include <kian_abi.h>

extern int  strcmp(const char *a, const char *b);
extern size_t strlen(const char *s);

static char CUR_USER[32] = "kian";

/* After a successful login, start the C++ desktop automatically.
 * gui_main returns 0 when the user exits (ESC / Exit to console), then ksh
 * continues with its prompt. Typing 'gui' in ksh still starts it manually. */
static const int AUTOSTART_GUI = 1;

static inline void kpause(void) {
    __asm__ volatile("pause");
}

static void print_banner(void) {
    console_set_color(0x00FFFFFF, 0x001A2B4A);
    console_clear();
    kprintf("\n");
    kprintf("  =====================================\n");
    kprintf("   Kian OS 0.3.0 \"Horizon\"\n");
    kprintf("   Rust kernel + C console + C++ GUI\n");
    kprintf("  =====================================\n");
    kprintf("\n");
}

/* Reads one line. echo_star: print '*' instead of each typed char. */
static int read_line(char *buf, int max, int echo_star) {
    int len = 0;
    buf[0] = '\0';
    for (;;) {
        kian_event_t ev;
        int got = kian_poll_event(&ev);
        if (!got) {
            kpause();
            continue;
        }
        if (ev.type != KIAN_EV_KEY_DOWN) continue;
        char c = (char)ev.b;
        if (c == '\n') {
            kprintf("\n");
            return len;
        }
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                kprintf("\b \b");
            }
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = c;
            buf[len] = '\0';
            kprintf(echo_star ? "*" : "%c", c);
        }
    }
}

static void login(void) {
    print_banner();
    for (;;) {
        int ok = 0;
        for (int tries = 0; tries < 3; tries++) {
            char user[32], pass[32];
            kprintf("kian login: ");
            read_line(user, sizeof(user), 0);
            kprintf("Password: ");
            read_line(pass, sizeof(pass), 1);
            kprintf("\n");
            if (kian_auth_check(user, pass)) {
                for (int i = 0; i < 31 && user[i] != '\0'; i++) CUR_USER[i] = user[i];
                CUR_USER[31] = '\0';
                ok = 1;
                break;
            }
            kprintf("Login incorrect\n\n");
        }
        if (ok) return;
        kprintf("Too many login failures, starting over.\n\n");
        print_banner();
    }
}

static void cmd_help(void) {
    kprintf("ksh built-ins:\n");
    kprintf("  help                 this text\n");
    kprintf("  clear                clear the screen\n");
    kprintf("  echo <text>          print text\n");
    kprintf("  uname                kernel name/version\n");
    kprintf("  whoami               current user\n");
    kprintf("  date                 Asia/Tehran local time (UTC+03:30, no DST)\n");
    kprintf("  uptime               time since boot\n");
    kprintf("  mem                  usable memory regions\n");
    kprintf("  gui                  start the C++ desktop (ESC / Exit to console returns)\n");
    kprintf("  reboot               reboot the machine\n");
    kprintf("  shutdown             power off\n");
    kprintf("  passwd               change password (partly working: stub)\n");
}

static void cmd_date(void) {
    kian_time_t t;
    kian_time_now(&t);
    kprintf("%04d-%02d-%02d %02d:%02d:%02d Asia/Tehran (UTC+03:30)\n",
            t.year, t.month, t.day, t.hour, t.minute, t.second);
}

static void cmd_uptime(void) {
    uint64_t ms = kian_uptime_ms();
    uint64_t s = ms / 1000;
    kprintf("up %llu days %02llu:%02llu:%02llu\n",
            (unsigned long long)(s / 86400), (unsigned long long)((s / 3600) % 24),
            (unsigned long long)((s / 60) % 60), (unsigned long long)(s % 60));
}

static void cmd_mem(void) {
    const kian_boot_info_t *bi = kian_boot_info();
    uint64_t total = 0;
    uint32_t regions = 0;
    for (uint32_t i = 0; i < bi->mem_count; i++) {
        if (bi->mem[i].usable) {
            total += bi->mem[i].length;
            regions++;
        }
    }
    kprintf("memory: %lu usable region(s), %lu MiB usable\n",
            (unsigned long)regions, (unsigned long)(total >> 20));
}

static void dispatch(char *cmd) {
    /* first word = command, rest = argument */
    char *arg = cmd;
    while (*arg != '\0' && *arg != ' ') arg++;
    if (*arg == ' ') { *arg = '\0'; arg++; }
    while (*arg == ' ') arg++;

    if (cmd[0] == '\0') return;
    if (strcmp(cmd, "help") == 0)       { cmd_help(); return; }
    if (strcmp(cmd, "clear") == 0)      { console_clear(); return; }
    if (strcmp(cmd, "echo") == 0)       { kprintf("%s\n", arg); return; }
    if (strcmp(cmd, "uname") == 0)      { kprintf("KianOS 0.3.0 x86_64 (Rust kernel)\n"); return; }
    if (strcmp(cmd, "whoami") == 0)     { kprintf("%s\n", CUR_USER); return; }
    if (strcmp(cmd, "date") == 0)       { cmd_date(); return; }
    if (strcmp(cmd, "uptime") == 0)     { cmd_uptime(); return; }
    if (strcmp(cmd, "mem") == 0)        { cmd_mem(); return; }
    if (strcmp(cmd, "gui") == 0) {
        kprintf("Starting GUI... (ESC or 'Exit to console' returns to ksh)\n");
        gui_main(&kian_boot_info()->fb);
        console_clear();
        kprintf("GUI exited back to ksh.\n");
        return;
    }
    if (strcmp(cmd, "reboot") == 0)     { kprintf("Rebooting...\n"); kian_reboot(); return; }
    if (strcmp(cmd, "shutdown") == 0)   { kprintf("Powering off...\n"); kian_shutdown(); return; }
    if (strcmp(cmd, "passwd") == 0)     { kprintf("passwd: not implemented yet (partly working stub)\n"); return; }
    kprintf("ksh: %s: command not found\n", cmd);
}

void kian_shell_main(void) {
    login();
    if (AUTOSTART_GUI) {
        kprintf("Starting GUI... (ESC or 'Exit to console' returns to ksh)\n");
        gui_main(&kian_boot_info()->fb);
        console_clear();
        kprintf("GUI exited back to ksh. Type 'gui' to start it again.\n");
    }
    kprintf("Welcome to ksh. Type 'help' for built-ins.\n");
    char line[160];
    for (;;) {
        kprintf("kian@kian-os:~$ ");
        read_line(line, sizeof(line), 0);
        dispatch(line);
    }
}
