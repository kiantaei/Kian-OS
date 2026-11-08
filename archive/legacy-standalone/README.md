# Archived: legacy standalone Kian OS kernel (v0.2.0 – v0.3.0)

This directory preserves the **original from-scratch Kian OS** code: a freestanding
Rust kernel (`kernel/`, `x86_64-unknown-none`), C boot glue and console, and the C++
"Win11-style" desktop GUI that drew everything directly to the framebuffer.

## Why it was archived

Kian OS has been migrated to a **real Linux-based operating system**
(see `/README.md` and `/archiso/kian/`):

- The custom kernel cannot run real Linux applications (gcc, python, pacman, bash).
- The C++ GUI was tied to the custom kernel's ABI (direct framebuffer drawing,
  polled PS/2 input) and could not run on Linux without a full rewrite.
- Keeping two competing operating systems in one repository made both harder
  to maintain.

Per the migration rules, code that conflicted with the new architecture was
**archived, not deleted**, so the original branding, design ideas and history
remain available for reference. Files here are kept exactly as they were at
commit `3eae617` and are **no longer built**.

## What lives on

- The Kian OS **name, colors and desktop design** (dark navy `#1A2B4A` → purple
  `#3B0A45` gradient, `#0078D4` accent, bottom taskbar, start menu with search,
  big-clock login screen) are reproduced by the Linux desktop theming in
  `/archiso/kian/airootfs/`.
- App identities continue: **Xenox Browser** (now the Chromium engine with a Kian
  profile), **Kian Terminal** (now xfce4-terminal with the same dark look),
  File Manager, Notepad, Calculator, Settings, Task Manager.
- The `kian` / `kian` default account and the Asia/Tehran default timezone.

## Not migrated (honest list)

- The C++ code-drawn icons → replaced by real SVG icons + Papirus-Dark theme.
- The mini-python interpreter → replaced by real CPython.
- The Paint app → no replacement shipped in 0.4.0.
- The BlueShell (`bsh`) experiment → replaced by real Bash.
