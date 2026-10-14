#include <stdint.h>
#include "desktop.h"
#include "draw.h"
#include "framebuffer.h"
#include "heap.h"
#include "keyboard.h"
#include "pmm.h"
#include "ramfs.h"
#include "timer.h"
#include "version.h"

/* Palette from docs/DESIGN.md section 3. */
#define C_MIDNIGHT  0x090A16u
#define C_PANEL     0x12142Bu   /* a little lighter than midnight, for cards and windows */
#define C_VIOLET    0x30205Eu   /* deep violet: shadows, title bars */
#define C_AMETHYST  0x9B7BFFu   /* borders and glow */
#define C_CYAN      0x36E4F5u   /* focus and selection */
#define C_MAGENTA   0xFF66C4u   /* accents and close buttons */
#define C_LAVENDER  0xE8DFF5u   /* main text */
#define C_DIM       0x7D7AA0u   /* secondary text */
#define C_STAR_A    0x3B3470u   /* faint background stars */
#define C_STAR_B    0x5A4A9Cu

/* Layout, in pixels, for the 800x600 mode. */
#define TOPBAR_H   28
#define DOCK_H     36
#define RAIL_W     72
#define CARD_W     180
#define HUB_X      RAIL_W
#define HUB_Y      TOPBAR_H
#define HUB_W      (FB_WIDTH - RAIL_W - CARD_W)
#define HUB_H      (FB_HEIGHT - TOPBAR_H - DOCK_H)
#define TILE_W     236
#define TILE_H     150
#define TILE_GAP   20
#define TILE_SHADOW 6

enum tile { TILE_ARCHIVE, TILE_MESSAGES, TILE_JOURNAL, TILE_SYSTEM, TILE_COUNT };
enum window { WIN_NONE, WIN_ABOUT, WIN_SYSTEM, WIN_ARCHIVE, WIN_JOURNAL, WIN_MESSAGES };

static enum tile focus = TILE_ARCHIVE;
static enum window open_win = WIN_NONE;
static int archive_sel;
static int viewing;                    /* archive window: showing one file */

/* ---- number formatting (no printf in the kernel) ---- */

static int put_dec(char *out, uint32_t v) {
    char tmp[12];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    int len = n;
    while (n > 0)
        *out++ = tmp[--n];
    *out = '\0';
    return len;
}

static void put_2d(char *out, uint32_t v) {
    out[0] = (char)('0' + (v / 10) % 10);
    out[1] = (char)('0' + v % 10);
    out[2] = '\0';
}

/* Draw "label" then a number then "unit" on one line. */
static void draw_kv(int x, int y, const char *label, uint32_t value, const char *unit, uint32_t color) {
    char num[12];
    put_dec(num, value);
    draw_text(x, y, label, C_DIM);
    int lx = x + draw_text_width(label);
    draw_text(lx, y, num, color);
    draw_text(lx + draw_text_width(num), y, unit, C_DIM);
}

/* ---- scene pieces ---- */

static void draw_background(void) {
    draw_clear(C_MIDNIGHT);
    /* Fixed star positions, so the background looks the same every frame. */
    for (uint32_t i = 0; i < 90; i++) {
        int x = (int)((i * 7919u) % FB_WIDTH);
        int y = (int)((i * 104729u) % FB_HEIGHT);
        draw_pixel(x, y, (i & 1) ? C_STAR_A : C_STAR_B);
    }
}

static void draw_topbar(void) {
    draw_rect(0, 0, FB_WIDTH, TOPBAR_H, C_PANEL);
    draw_rect(0, TOPBAR_H - 1, FB_WIDTH, 1, C_AMETHYST);
    draw_text(12, 6, TESTAROSSA_NAME, C_MAGENTA);
    draw_text(12 + draw_text_width(TESTAROSSA_NAME) + 8, 6, "v" TESTAROSSA_VERSION, C_DIM);

    uint32_t sec = timer_ticks() / TIMER_HZ;
    char hh[3], mm[3], ss[3];
    put_2d(hh, (sec / 3600) % 100);
    put_2d(mm, (sec / 60) % 60);
    put_2d(ss, sec % 60);
    char clock[24] = "up ";
    int p = 3;
    for (int i = 0; hh[i]; i++) clock[p++] = hh[i];
    clock[p++] = ':';
    for (int i = 0; mm[i]; i++) clock[p++] = mm[i];
    clock[p++] = ':';
    for (int i = 0; ss[i]; i++) clock[p++] = ss[i];
    clock[p] = '\0';
    draw_text((int)FB_WIDTH - draw_text_width(clock) - 12, 6, clock, C_LAVENDER);
}

static void draw_dock(void) {
    int y = FB_HEIGHT - DOCK_H;
    draw_rect(0, y, FB_WIDTH, DOCK_H, C_PANEL);
    draw_rect(0, y, FB_WIDTH, 1, C_AMETHYST);
    draw_text(12, y + 10, "Tab move   arrows move   Enter open   Esc close   A about", C_DIM);
}

static const char *rail_labels[4] = { "AR", "ME", "JO", "SY" };

static void draw_rail(void) {
    draw_rect(0, TOPBAR_H, RAIL_W, FB_HEIGHT - TOPBAR_H - DOCK_H, C_PANEL);
    draw_rect(RAIL_W - 1, TOPBAR_H, 1, FB_HEIGHT - TOPBAR_H - DOCK_H, C_VIOLET);
    for (int i = 0; i < 4; i++) {
        int y = TOPBAR_H + 24 + i * 72;
        int active = (int)focus == i;
        draw_rect(12, y, 48, 48, active ? C_VIOLET : C_MIDNIGHT);
        draw_frame(12, y, 48, 48, 2, active ? C_CYAN : C_AMETHYST);
        draw_text(20, y + 16, rail_labels[i], active ? C_CYAN : C_LAVENDER);
    }
}

static void draw_tile(enum tile t, int x, int y, const char *title, const char *sub1, const char *sub2) {
    int focused = (focus == t);
    /* Violet drop shadow, then the card. */
    draw_rect(x + TILE_SHADOW, y + TILE_SHADOW, TILE_W, TILE_H, C_VIOLET);
    draw_rect(x, y, TILE_W, TILE_H, C_PANEL);
    draw_frame(x, y, TILE_W, TILE_H, focused ? 3 : 2, focused ? C_CYAN : C_AMETHYST);

    /* Small pixel icon, one per tile. */
    int ix = x + 18, iy = y + 16;
    switch (t) {
    case TILE_ARCHIVE:  /* a box */
        draw_frame(ix, iy + 6, 32, 24, 2, C_MAGENTA);
        draw_rect(ix + 10, iy, 12, 6, C_MAGENTA);
        break;
    case TILE_MESSAGES: /* a speech bubble */
        draw_frame(ix, iy, 32, 22, 2, C_MAGENTA);
        draw_rect(ix + 6, iy + 22, 6, 6, C_MAGENTA);
        break;
    case TILE_JOURNAL:  /* ruled lines */
        draw_frame(ix, iy, 28, 32, 2, C_MAGENTA);
        for (int k = 0; k < 3; k++)
            draw_rect(ix + 6, iy + 8 + k * 7, 16, 2, C_MAGENTA);
        break;
    case TILE_SYSTEM:   /* a monitor */
        draw_frame(ix, iy, 34, 24, 2, C_MAGENTA);
        draw_rect(ix + 14, iy + 24, 6, 6, C_MAGENTA);
        break;
    default:
        break;
    }

    draw_text(x + 64, y + 22, title, focused ? C_CYAN : C_LAVENDER);
    draw_text(x + 18, y + 72, sub1, C_DIM);
    draw_text(x + 18, y + 92, sub2, C_DIM);
}

static void draw_hub(void) {
    int gx = HUB_X + (HUB_W - (2 * TILE_W + TILE_GAP)) / 2;
    int gy = HUB_Y + (HUB_H - (2 * TILE_H + TILE_GAP)) / 2;

    char count[12];
    put_dec(count, (uint32_t)ramfs_count());
    char archive_sub[32] = "";
    int n = 0;
    for (int i = 0; count[i]; i++) archive_sub[n++] = count[i];
    const char *tail = " built-in files";
    for (int i = 0; tail[i]; i++) archive_sub[n++] = tail[i];
    archive_sub[n] = '\0';

    draw_tile(TILE_ARCHIVE,  gx,                  gy,                  "Archive",  archive_sub, "read-only, from memory");
    draw_tile(TILE_MESSAGES, gx + TILE_W + TILE_GAP, gy,               "Messages", "none yet",  "messaging is not built");
    draw_tile(TILE_JOURNAL,  gx,                  gy + TILE_H + TILE_GAP, "Journal", "entry 01, built-in demo", "archive copy");
    draw_tile(TILE_SYSTEM,   gx + TILE_W + TILE_GAP, gy + TILE_H + TILE_GAP, "System", "uptime, memory, heap", "live values");
}

static void draw_cards(void) {
    int x = FB_WIDTH - CARD_W + 12;
    int w = CARD_W - 24;
    int y = HUB_Y + 16;
    for (int i = 0; i < 3; i++) {
        int cy = y + i * 110;
        draw_rect(x + 4, cy + 4, w, 92, C_VIOLET);
        draw_rect(x, cy, w, 92, C_PANEL);
        draw_frame(x, cy, w, 92, 1, C_AMETHYST);
    }
    uint32_t sec = timer_ticks() / TIMER_HZ;
    draw_text(x + 10, y + 10, "UPTIME", C_MAGENTA);
    draw_kv(x + 10, y + 34, "", sec, " s", C_LAVENDER);

    draw_text(x + 10, y + 120, "MEMORY", C_MAGENTA);
    draw_kv(x + 10, y + 144, "", pmm_free_frames() * 4, " KiB free", C_LAVENDER);

    draw_text(x + 10, y + 230, "HEAP", C_MAGENTA);
    draw_kv(x + 10, y + 254, "", heap_free_bytes() / 1024, " KiB free", C_LAVENDER);
}

/* ---- windows ---- */

#define WIN_X  150
#define WIN_Y  112
#define WIN_W  500
#define WIN_H  360
#define WIN_BAR 26

static const char *window_title(enum window w) {
    switch (w) {
    case WIN_ABOUT:    return "About Testarossa";
    case WIN_SYSTEM:   return "System Monitor";
    case WIN_ARCHIVE:  return viewing ? "Archive - file" : "Archive";
    case WIN_JOURNAL:  return "Journal";
    case WIN_MESSAGES: return "Messages";
    default:           return "";
    }
}

/* Draw text that may contain newlines, starting at (x, y). */
static void draw_lines(int x, int y, const char *s, uint32_t color, int max_lines) {
    int line = 0;
    int cx = x;
    for (; *s && line < max_lines; s++) {
        if (*s == '\n') {
            line++;
            cx = x;
            y += 20;
            continue;
        }
        draw_char(cx, y, *s, color);
        cx += 8;
    }
}

static void draw_archive_list(int x, int y) {
    int n = ramfs_count();
    for (int i = 0; i < n; i++) {
        int row_y = y + i * 26;
        if (i == archive_sel) {
            draw_rect(x - 6, row_y - 3, WIN_W - 48, 22, C_VIOLET);
            draw_rect(x - 6, row_y - 3, 3, 22, C_CYAN);
        }
        draw_text(x, row_y, ramfs_name(i), i == archive_sel ? C_CYAN : C_LAVENDER);
    }
    draw_text(x, y + n * 26 + 14, "Up/Down choose   Enter open", C_DIM);
}

static void draw_window_body(int bx, int by) {
    switch (open_win) {
    case WIN_ABOUT:
        draw_text(bx, by, TESTAROSSA_NAME " " TESTAROSSA_VERSION, C_MAGENTA);
        draw_text(bx, by + 28, "A from-scratch 32-bit x86 educational OS.", C_LAVENDER);
        draw_text(bx, by + 52, "CPU: i386 protected mode, flat segments", C_LAVENDER);
        draw_text(bx, by + 72, "Kernel: C and NASM, built with gcc -m32", C_LAVENDER);
        draw_text(bx, by + 92, "Memory: 4 KiB frame allocator, 512 KiB heap", C_LAVENDER);
        draw_text(bx, by + 112, "Display: VGA Bochs VBE, 800x600x32 linear", C_LAVENDER);
        draw_text(bx, by + 132, "Paging: 4 MiB identity map, PSE", C_LAVENDER);
        draw_text(bx, by + 160, "Everything on this screen is drawn by the kernel.", C_DIM);
        break;

    case WIN_SYSTEM: {
        uint32_t t = timer_ticks();
        draw_kv(bx, by, "uptime       ", t / TIMER_HZ, " s", C_LAVENDER);
        draw_kv(bx, by + 22, "timer ticks  ", t, " at 100 Hz", C_LAVENDER);
        draw_kv(bx, by + 52, "usable RAM   ", pmm_total_frames() * 4, " KiB", C_LAVENDER);
        draw_kv(bx, by + 74, "free frames  ", pmm_free_frames() * 4, " KiB", C_LAVENDER);
        draw_kv(bx, by + 96, "heap free    ", heap_free_bytes() / 1024, " KiB", C_LAVENDER);
        draw_kv(bx, by + 118, "heap size    ", heap_total_bytes() / 1024, " KiB", C_LAVENDER);
        draw_text(bx, by + 150, "Values are read from the kernel each second.", C_DIM);
        break;
    }

    case WIN_ARCHIVE:
        if (viewing) {
            draw_lines(bx, by, ramfs_read(ramfs_name(archive_sel)), C_LAVENDER, 12);
            draw_text(bx, by + 290, "Esc back to the list", C_DIM);
        } else {
            draw_archive_list(bx, by);
        }
        break;

    case WIN_JOURNAL:
        draw_text(bx, by, "built-in demo entry, copied from the archive", C_DIM);
        draw_lines(bx, by + 26, ramfs_read("/archive/entry01.txt"), C_LAVENDER, 8);
        break;

    case WIN_MESSAGES:
        draw_text(bx, by, "No messages.", C_LAVENDER);
        draw_text(bx, by + 28, "Messaging is not built yet.", C_DIM);
        break;

    default:
        break;
    }
}

static void draw_window(void) {
    if (open_win == WIN_NONE)
        return;
    /* Shadow, body, title bar, border. */
    draw_rect(WIN_X + 8, WIN_Y + 8, WIN_W, WIN_H, C_VIOLET);
    draw_rect(WIN_X, WIN_Y, WIN_W, WIN_H, C_PANEL);
    draw_rect(WIN_X, WIN_Y, WIN_W, WIN_BAR, C_VIOLET);
    draw_frame(WIN_X, WIN_Y, WIN_W, WIN_H, 2, C_AMETHYST);
    draw_text(WIN_X + 12, WIN_Y + 5, window_title(open_win), C_LAVENDER);
    draw_text(WIN_X + WIN_W - 20, WIN_Y + 5, "x", C_MAGENTA);
    draw_window_body(WIN_X + 20, WIN_Y + WIN_BAR + 20);
}

static void render(void) {
    draw_background();
    draw_topbar();
    draw_rail();
    draw_hub();
    draw_cards();
    draw_dock();
    draw_window();
    fb_present();
}

/* ---- input ---- */

static void open_tile(enum tile t) {
    switch (t) {
    case TILE_ARCHIVE:  open_win = WIN_ARCHIVE; break;
    case TILE_MESSAGES: open_win = WIN_MESSAGES; break;
    case TILE_JOURNAL:  open_win = WIN_JOURNAL; break;
    case TILE_SYSTEM:   open_win = WIN_SYSTEM; break;
    default:            break;
    }
    viewing = 0;
}

static void move_focus(int dx, int dy) {
    /* Tiles form a 2x2 grid: 0 1 / 2 3. */
    int col = (int)focus % 2;
    int row = (int)focus / 2;
    col += dx;
    row += dy;
    if (col < 0) col = 0;
    if (col > 1) col = 1;
    if (row < 0) row = 0;
    if (row > 1) row = 1;
    focus = (enum tile)(row * 2 + col);
}

static void handle_key(int c) {
    if (open_win != WIN_NONE) {
        if (open_win == WIN_ARCHIVE && !viewing) {
            if (c == KEY_UP && archive_sel > 0)
                archive_sel--;
            else if (c == KEY_DOWN && archive_sel < ramfs_count() - 1)
                archive_sel++;
            else if (c == '\n') {
                viewing = 1;
                return;
            }
        }
        if (c == 27) {                      /* Esc: back from a file, or close the window */
            if (viewing)
                viewing = 0;
            else
                open_win = WIN_NONE;
        }
        return;
    }

    switch (c) {
    case KEY_LEFT:  move_focus(-1, 0); break;
    case KEY_RIGHT: move_focus(1, 0);  break;
    case KEY_UP:    move_focus(0, -1); break;
    case KEY_DOWN:  move_focus(0, 1);  break;
    case '\t':      focus = (enum tile)(((int)focus + 1) % TILE_COUNT); break;
    case '\n':      open_tile(focus); break;
    case 'a':                           /* About has no tile; A opens it directly */
    case 'A':       open_win = WIN_ABOUT; viewing = 0; break;
    default:        break;
    }
}

void desktop_run(void) {
    uint32_t last_sec = (uint32_t)-1;
    for (;;) {
        __asm__ volatile("cli");
        int c = keyboard_getc();
        uint32_t sec = timer_ticks() / TIMER_HZ;
        if (c < 0 && sec == last_sec) {
            /* Nothing to do: sleep until the next interrupt. "sti; hlt" is atomic. */
            __asm__ volatile("sti; hlt");
            continue;
        }
        __asm__ volatile("sti");

        if (c >= 0)
            handle_key(c);
        last_sec = sec;
        render();
    }
}
