#!/usr/bin/env python3
"""Generate embedded bitmap fonts for the Kian OS GUI.

Outputs (into /home/z/my-project/Kian-OS/gui/):
  font.h    - API + constants
  font.cpp  - glyph tables: UI font (proportional, Liberation Sans)
              and mono font (8x16 cell, DejaVu Sans Mono)

Each glyph = advance (px) + ink width (px) + H rows x 2 bytes (16 bits,
MSB = leftmost pixel). Integers only at runtime; this generator runs on
the host and only emits tables.
"""
from PIL import Image, ImageDraw, ImageFont

UI_TTF = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
MONO_TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
OUT_H = "/home/z/my-project/Kian-OS/gui/font.h"
OUT_CPP = "/home/z/my-project/Kian-OS/gui/font.cpp"
PREVIEW_UI = "/home/z/my-project/scripts/preview_ui.png"
PREVIEW_MONO = "/home/z/my-project/scripts/preview_mono.png"

FIRST, LAST = 32, 126          # printable ASCII
UI_H = 18                      # em rows for the UI font
MONO_H = 16                    # em rows for the terminal font
MONO_CELL_W = 8
UI_MAX_W = 16                  # ink width cap for UI glyphs


def render_char(font, ch, baseline, rows, canvas=96):
    """Render ch with baseline at y=baseline inside an em box of `rows` rows.
    Returns (advance, ink_w, [int(16-bit row words)])."""
    adv = int(round(font.getlength(ch)))
    img = Image.new("L", (canvas, canvas), 0)
    d = ImageDraw.Draw(img)
    d.text((8, baseline + 8), ch, font=font, fill=255, anchor="ls")
    bb = img.getbbox()  # (l, t, r, b) of ink
    words = []
    if bb is None:
        return adv, 0, [0] * rows
    x0, y0, x1, y1 = bb
    ink_w = x1 - x0
    for r in range(rows):
        yy = 8 + r - baseline + 8 - 8  # em row r -> image y = (baseline+8) + (r - baseline)
        yy = (baseline + 8) + (r - baseline)
        word = 0
        if 0 <= yy < canvas and ink_w > 0:
            for col in range(min(ink_w, 16)):
                if img.getpixel((x0 + col, yy)) >= 128:
                    word |= 0x8000 >> col
        words.append(word)
    return adv, min(ink_w, UI_MAX_W if ink_w <= UI_MAX_W else UI_MAX_W), words


def build(font_path, size, rows, cell_w=None, name=""):
    font = ImageFont.truetype(font_path, size)
    asc, desc = font.getmetrics()
    baseline = min(asc, rows - max(1, desc) - 1)
    glyphs = {}
    clipped = 0
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        adv, w, words = render_char(font, ch, baseline, rows)
        if cell_w is not None:
            adv = cell_w
            if w > cell_w:
                w = cell_w
                clipped += 1
                for r in range(rows):
                    mask = 0xFFFF << (16 - cell_w) & 0xFFFF
                    words[r] &= mask
        glyphs[code] = (adv, w, words)
        if w > (cell_w or UI_MAX_W):
            print(f"  ! {name} '{ch}' ink {w}px too wide")
    # cap band for vertical centering math: digits + flat-bottomed caps
    # (excludes Q, whose tail descends below the baseline)
    cap_rows = [0] * rows
    for code in list(range(48, 58)) + [ord(c) for c in "EHXZT"]:
        _, w, words = glyphs[code]
        for r in range(rows):
            if words[r]:
                cap_rows[r] = 1
    cap_top = next(i for i, v in enumerate(cap_rows) if v)
    cap_bot = rows - 1 - next(i for i, v in enumerate(reversed(cap_rows)) if v)
    print(f"{name}: size={size} ascent={asc} descent={desc} baseline={baseline} "
          f"cap_rows={cap_top}..{cap_bot} clipped={clipped}")
    return glyphs, cap_top, cap_bot


def emit_tables(glyphs, rows):
    out = []
    for code in range(FIRST, LAST + 1):
        adv, w, words = glyphs[code]
        rowbytes = ",".join("0x%02X,0x%02X" % ((wd >> 8) & 0xFF, wd & 0xFF) for wd in words)
        out.append("    { %d, %d, { %s } }," % (adv, w, rowbytes))
    return "\n".join(out)


def preview(glyphs, rows, baseline, path, lines, cell_w=None, scale=2):
    lh = rows * scale + 6
    W = max((sum(glyphs[ord(c)][0] if cell_w is None else cell_w for c in ln) for ln in lines)) * scale + 24
    img = Image.new("RGB", (W, lh * len(lines) + 16), (16, 22, 38))
    px = img.load()
    for li, ln in enumerate(lines):
        x = 8
        y = 8 + li * lh
        for ch in ln:
            code = ord(ch)
            if code not in glyphs:
                x += 8 * scale
                continue
            adv, w, words = glyphs[code]
            for r, wd in enumerate(words):
                for col in range(16):
                    if wd & (0x8000 >> col):
                        for sy in range(scale):
                            for sx in range(scale):
                                px[x + col * scale + sx, y + r * scale + sy] = (232, 232, 240)
            x += (adv if cell_w is None else cell_w) * scale
    img = img.resize((img.width, img.height)) if False else img
    img.save(path)
    print("wrote", path, img.size)


def main():
    ui, ui_top, ui_bot = build(UI_TTF, 16, UI_H, None, "UI  ")
    mono, m_top, m_bot = build(MONO_TTF, 13, MONO_H, MONO_CELL_W, "MONO")

    hdr = f"""// Kian OS GUI - embedded bitmap fonts (generated by scripts/gen_font.py,
// do not edit by hand). UI font: Liberation Sans 16px em (18 rows,
// proportional). Mono font: DejaVu Sans Mono 13px in an 8x16 cell.
// 1 bit per pixel, MSB = leftmost pixel. ASCII 32..126 only.
#pragma once
#include <kian_abi.h>

const int FONT_UI_ROWS = {UI_H};          // em rows, UI font
const int FONT_MONO_ROWS = {MONO_H};      // em rows, mono font
const int FONT_MONO_CELL = {MONO_CELL_W}; // advance of every mono glyph
const int FONT_UI_CAP_TOP = {ui_top};     // first ink row of A-Z0-9 (UI font)
const int FONT_UI_CAP_BOT = {ui_bot};     // last ink row of A-Z0-9 (UI font)
const int FONT_MONO_CAP_TOP = {m_top};
const int FONT_MONO_CAP_BOT = {m_bot};

struct GlyphBits {{
    int adv;          // advance in px (scale 1)
    int w;            // ink width in px (scale 1)
    int rows;         // em rows
    const uint8_t *bits;  // rows x 2 bytes, MSB-left
}};

bool font_glyph_ui(char c, GlyphBits *out);    // proportional UI font
bool font_glyph_mono(char c, GlyphBits *out);  // 8x16 terminal font
"""
    with open(OUT_H, "w") as f:
        f.write(hdr)

    cpp = f"""// Kian OS GUI - font tables (generated by scripts/gen_font.py).
#include "font.h"

struct GlyphRec {{
    uint8_t adv;
    uint8_t w;
    uint8_t bits[{UI_H} * 2];
}};
struct MonoRec {{
    uint8_t adv;
    uint8_t w;
    uint8_t bits[{MONO_H} * 2];
}};

static const GlyphRec UI_TABLE[{LAST - FIRST + 1}] = {{
{emit_tables(ui, UI_H)}
}};

static const MonoRec MONO_TABLE[{LAST - FIRST + 1}] = {{
{emit_tables(mono, MONO_H)}
}};

static bool pick(const void *tab, int rows, char c, GlyphBits *out) {{
    int idx = (int)c - {FIRST};
    if (idx < 0 || idx > {LAST - FIRST}) return false;
    if (rows == {UI_H}) {{
        const GlyphRec *g = &((const GlyphRec *)tab)[idx];
        out->adv = g->adv; out->w = g->w; out->rows = {UI_H}; out->bits = g->bits;
    }} else {{
        const MonoRec *g = &((const MonoRec *)tab)[idx];
        out->adv = g->adv; out->w = g->w; out->rows = {MONO_H}; out->bits = g->bits;
    }}
    return true;
}}

bool font_glyph_ui(char c, GlyphBits *out) {{ return pick(UI_TABLE, {UI_H}, c, out); }}
bool font_glyph_mono(char c, GlyphBits *out) {{ return pick(MONO_TABLE, {MONO_H}, c, out); }}
"""
    with open(OUT_CPP, "w") as f:
        f.write(cpp)

    preview(ui, UI_H, None, PREVIEW_UI,
            ["Kian OS Desktop", "Settings & Personalization 0.3.0", "Task Manager - Files - Paint",
             "Hostname: kian-os (UTC+03:30)", "jelly quips? yes! [g/y/p] {foggy}"],
            None, 2)
    preview(mono, MONO_H, None, PREVIEW_MONO,
            ["kian@kian-os:/$ ls Desktop", "welcome.txt  readme.txt/", "0123456789 12.5000 +/-"],
            MONO_CELL_W, 2)
    # quick sanity dump of a few advances
    for ch in " ilmwMW@":
        print(f"UI adv '{ch}' = {ui[ord(ch)][0]}, w={ui[ord(ch)][1]}")


if __name__ == "__main__":
    main()
