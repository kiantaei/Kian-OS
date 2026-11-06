# Kian OS GUI — native desktop environment (C++17, freestanding)

The GUI is the desktop subsystem of Kian OS. It runs on the Rust kernel's
ABI (`include/kian_abi.h`) directly on the framebuffer, with no Linux, no
libc, no libstdc++, no STL, no float/double, no global constructors.
It autostarts after the ksh login (`AUTOSTART_GUI` in `console/shell.c`);
`gui` in ksh starts it manually; ESC, Ctrl+Alt+Q or Start ▸ "Exit to
console" always return to the working ksh prompt.

## 1. Architecture

```
ksh (C console) ──gui_main(&fb)──▶  Shell (one instance, created in gui_main)
                                      ├─ Gfx      frame + scene buffers, primitives, fonts
                                      ├─ Input    event pump, cursor (scene-restore trick)
                                      ├─ WM       32-window pool, drag/resize/snap/Alt+Tab
                                      ├─ Desktop  taskbar, start menu, quick settings,
                                      │           calendar, toasts, task view, virtual desktops
                                      ├─ RAM fs   volatile tree of nodes (lost on reboot)
                                      └─ Apps     terminal, files, notepad, calc, settings,
                                                  task manager, paint, browser, run dialog
```

Everything is plain structs + free functions + function pointers
(`Window::draw/event/destroy`). No virtual functions, no RTTI, no
exceptions. One `Shell` object is `new`-ed inside `gui_main()` and
`delete`-d on exit; every allocation has an owner and a cleanup path.

## 2. Project structure

| File | Purpose |
|---|---|
| `gui/gui.h` / `gui.cpp` | `gui_main()`: init order G1→G6, event loop, shell shortcuts, Run dialog, Alt+Tab, Ctrl+Alt+Del screen, dirty-region redraw |
| `gui/gfx.h` / `gfx.cpp` | frame + scene buffers, clipping, primitives, alpha blending, gradients, `present()` with `rep movsd`, text engines |
| `gui/font.h` / `font.cpp` | **embedded bitmap fonts (generated, do not edit by hand)** |
| `gui/icons.h` / `icons.cpp` | every icon drawn in code (rects/lines/circles/arcs) |
| `gui/input.h` / `input.cpp` | `kian_poll_event` pump, double-click/drag/hover, 12x19 software cursor |
| `gui/widgets.h` / `widgets.cpp` | Button, Label, TextBox, Checkbox, Slider, ListView, ScrollBar, Menu, Tooltip, TabBar |
| `gui/wm.h` / `wm.cpp` | window pool, z-order, decorations, drag, 8px resize, snap preview, min/max/close |
| `gui/desktop.h` / `desktop.cpp` | wallpaper, desktop icons, taskbar + tray + clock, start menu, quick settings, calendar, toast, task view |
| `gui/login.cpp` | lock/login screen (`kian_auth_check`), shake on wrong password |
| `gui/ramfs.h` / `ramfs.cpp` | 128-node pool RAM filesystem (create/read/write/rename/trash/delete/list/resolve) |
| `gui/runtime.h` / `runtime.cpp` | `new/delete` → `kian_alloc/kian_free`, `__cxa_*` hooks |
| `gui/apps/terminal.cpp` | terminal (see §6) |
| `gui/apps/file_manager.cpp` | Files: sidebar, back/up, address bar, list/icon views, context menu, dialogs |
| `gui/apps/notepad.cpp` | mono text editor: caret, arrows/Home/End/PgUp/PgDn, mouse caret, File menu on RAM fs |
| `gui/apps/calculator.cpp` | 64-bit integer math, 4-decimal fixed point, string-based entry (correct decimals + backspace) |
| `gui/apps/settings.cpp` | 7 pages; accent/wallpaper/theme apply LIVE; honest placeholders elsewhere |
| `gui/apps/task_manager.cpp` | window list + End task, uptime, heap stats, clearly-labeled placeholder CPU graph |
| `gui/apps/paint.cpp` | canvas, 8 colors, 3 brush sizes, eraser, clear |
| `gui/apps/browser.cpp` | address bar + local start page; **networking not implemented, honestly labeled** |

## 3. Fonts

The kernel's 8x8 console font stays untouched for ksh. The GUI embeds two
higher-quality bitmap fonts generated **offline** by `scripts/gen_font.py`
(Pillow, host-side) from real TrueType fonts — committed as plain integer
tables in `gui/font.cpp`:

| Font | Source | Layout | Used by |
|---|---|---|---|
| UI (proportional) | Liberation Sans, 16 px em | 18 rows, per-glyph ink width + advance | every widget, window, shell surface |
| Mono | DejaVu Sans Mono, 13 px in an **8x16 cell** | fixed 8 px advance | Terminal, Notepad editor |

- 1 bit per pixel, MSB = leftmost pixel, ASCII 32..126.
- `draw_text()` / `draw_text_mono()`; `text_center_y(box_y, box_h, scale)`
  vertically centers the cap band (constants `FONT_UI_CAP_TOP/BOT`).
- Scale 1..4 supported by integer pixel replication (login clock = 4).
- Bold = second draw with a 1 px offset. No float anywhere.

## 4. Terminal: editing, history and TAB completion

- **Caret editing**: Left/Right/Home/End/Del/Backspace insert and remove
  at the caret (bounded 94-char line), blinking 8x12 bar.
- **History**: Up/Down walk a ring of the last 16 commands (duplicate and
  empty entries skipped).
- **TAB completion**:
  - first word → command names (`help clear echo date uptime mem whoami
    uname pwd ls cd cat mkdir touch rm exit`);
  - later words → RAM FS paths (relative or absolute; `cd` completes dirs
    only, `cat` files only);
  - unique match → inserted + `/` for dirs + a space — e.g. typing
    `ls desk` + TAB becomes `ls Desktop/ `;
  - several matches → extended to the longest common prefix;
  - a second TAB with no progress → prints the candidate list.
- Extra command: `pwd`.

## 5. Memory model

- `frame` + `scene` = `2 × w × h × 4` bytes from `kian_alloc`, overflow-
  checked, freed on every exit path ("GUI: out of memory" + return 1).
- `Window windows[32]`, `RNode POOL[128]`, menu items `[12]`, history
  `[16][96]` — fixed pools, no per-frame allocation.
- Scrollback caps at ~8 KB; files grow by doubling with bounds checks.
- Gfx owns frame/scene; WM owns windows (not instances); apps own their
  instances and free them in `destroy`; ramfs owns node data. No
  dangling pointers after close (instance destroyed with the window).

## 6. Build integration

`build.sh` compiles `gui/*.cpp` and `gui/apps/*.cpp` with
`clang++ --target=x86_64-unknown-none-elf -std=gnu++17 -ffreestanding
-fno-exceptions -fno-rtti -fno-threadsafe-statics -mgeneral-regs-only
-mno-red-zone -Wglobal-constructors`, objects named
`gui_apps_terminal.o` (path separators → underscores), linked **before**
the Rust archive (`-z muldefs`, no `--gc-sections`), followed by the
existing `nm`/`readelf` ELF checks. No build step was removed.

## 7. Known limitations (honest)

- The RAM filesystem is **volatile** — everything is lost on reboot.
- Networking is not implemented; the Browser is a local start page.
- Network/audio toggles are visual only (no drivers exist).
- The Task Manager CPU graph is a clearly-labeled placeholder.
- Sleep/Restart use `kian_halt`/`kian_reboot`; no ACPI sleep.
- Task-view thumbnails are simplified previews, not real screenshots.
- Terminal output lines longer than the window are clipped, not wrapped.
- VMware absolute mouse (G9) is not implemented.
- **Compilation and execution have not been verified by the author** —
  only CI builds (`make iso`) and freestanding compile checks. Runtime
  verification happens through the GitHub Actions QEMU artifacts.

## 8. Debug artifacts

CI (`build-iso.yml`) boots the ISO in QEMU, sends the ksh login keys,
and uploads the `kian-debug` artifact: `gui.png` (desktop screenshot),
`serial.log` (G0–G6 step lines), `gui-serial.log`, `debugcon.log`,
`qemu-debug.log` and the ISO itself.
