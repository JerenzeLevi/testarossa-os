# Testarossa OS - UI / UX / GUI Design

Status: **LOCKED v1.0** (October 2026). Any change to this file goes through
a GitHub issue labeled `ux-design`, and needs the project owner's approval
before code changes that depend on it.

The goal of this document is to stop the look and feel of the OS from
drifting while several people write code. If the code and this document
disagree, the document wins until it is changed here first.


## 1. Visual north star

> An abandoned early-2000s computer, discovered inside a cyber-gothic digital cathedral.

The user should feel they are exploring an old personal computer that holds
forgotten files, journal entries, messages, and strange digital artifacts.
It should feel like the simulated computer in a careful indie mystery game,
but it is presented as a real operating system desktop.

Mood keywords: **nostalgic, quiet, mysterious, legible, elegant.**
Not: cluttered, hacker-cliche, neon-everything, cute, bubblegum.


## 2. Non-negotiables

1. The name is **Testarossa OS**. Never write "Vesper" in visible text.
2. Palette is violet / cyan / magenta on midnight. Never a red-dominant
   palette and no car or Ferrari theme.
3. The design is original. The mood of early-2000s internet nostalgia and
   archive exploration is the inspiration, but no assets, screens, layouts,
   characters, text, or logos from any existing game or product are copied.
4. The UI is 2D. "Spatial" means layered panels, shadows, and overlap.
   It does not mean 3D or VR.
5. Text the OS shows must be true. No fake memory numbers, file lists, or
   messages. Sample content is allowed only when it is labeled as a built-in demo.
6. Keyboard is the primary input. Every feature must work without a mouse.
7. Effects (sparkles, scanlines) come last and must never make text hard to read.


## 3. Colour palette (locked)

| Role              | Name            | Hex       | Notes                              |
|-------------------|-----------------|-----------|------------------------------------|
| Background base  | Midnight        | `#090A16` | Desktop and window body background |
| Panel shadow     | Deep violet     | `#30205E` | Panels, dock, drop shadows         |
| Illumination     | Amethyst        | `#9B7BFF` | Borders, hub glow, secondary text |
| Focus / select   | Electric cyan   | `#36E4F5` | Focused item, selected tab, cursor |
| Accent           | Hot magenta     | `#FF66C4` | Logo, small highlights, alerts     |
| Body text        | Pale lavender   | `#E8DFF5` | All main text                      |

Contrast rule: body text on Midnight must be at least **4.5 : 1**
(WCAG AA). Pale lavender on Midnight is about 15 : 1, which passes.
Electric cyan on Midnight is about 11 : 1.

### 3.1 Text-mode (VGA 80x25) mapping - APPLIED in v1.1

The VGA text screen has only 16 colours. Until the framebuffer exists,
the text screen uses this mapping. It replaces the earlier bubblegum look
(light magenta on black, "xoxo" banner), which leaned toward the cute,
sparkly style of Lily's World XD. That style was dropped on purpose: the
master plan asks for an original cyber-gothic look and no copied mood.
Constants are in `kernel/vga.h`.

| Use                    | VGA index | Name           | Closest to                |
|------------------------|-----------|----------------|---------------------------|
| Background             | 0         | black          | Midnight                  |
| Body text              | 7         | light grey     | Pale lavender             |
| Prompt and focus       | 11        | light cyan     | Electric cyan             |
| Titles, banner, errors | 13        | light magenta  | Hot magenta               |
| Dim hints              | 8         | dark grey      | Amethyst, low emphasis    |


## 4. Text-mode screen design (current milestone)

### 4.1 Boot banner (top of screen)

    ~*~ Testarossa OS ~*~  v0.1.0

    Shell ready. Type help.
    testarossa>

### 4.2 Prompt and input

- Prompt text is exactly `testarossa> ` (with the trailing space).
- Typed characters appear after the prompt. Backspace erases only the user's own
  characters, never the prompt.
- Maximum input length: 127 characters. Extra characters are ignored.

### 4.3 Message style

- Normal output is plain sentences, lowercase, no exclamation marks.
- Errors start with the item that failed, then a short fix:
  `unknown command: bogus  (type help)`
- "Not built yet" messages always say what is missing:
  `mem: not implemented yet (needs the physical memory allocator)`
- The mystery tone comes from the wording of the about text and the file
  content, not from decoration. Keep the screen clean.

### 4.4 Command output layout

- One blank line is not added between commands. Output goes straight under the
  command line, then the next prompt.
- `clear` empties the screen and prints the next prompt at the top.


## 5. Desktop design (planned, Week 3-4)

Target: a framebuffer mode provided by the boot path. The resolution is taken
from the boot information. It is **not** hardcoded. The layout below is
given as proportions so it works at any size of 800x600 or larger.

### 5.1 Regions

    +----------------------------------------------------------+
    | TOP BAR  TESTAROSSA OS   v0.1.0      ticks / uptime      |  5% height
    +----+-----------------------------------------------+-----+
    |    |                                               |     |
    | N  |            CENTRAL HUB                         | S   |
    | A  |   [ Archive ] [ Messages ]                     | T   |
    | V  |   [ Journal ] [ System   ]                     | A   |
    |    |                                               | T   |
    |    |        (open windows float here)              | U   |
    |    |                                               | S   |
    +----+-----------------------------------------------+-----+
    | DOCK   [ System ] [ About ] [ Terminal ]               |  6% height
    +----------------------------------------------------------+

- **Top bar**: name, version, uptime. Background Deep violet.
- **Navigation rail (left, about 8% width)**: shortcuts. Keyboard focus cycles here.
- **Central hub**: four tiles in a 2x2 grid. Each tile is a floating card
  with a 2-pixel Amethyst border and a violet shadow offset of 4 px.
- **Status cards (right, about 18% width)**: truthful values only.
- **Dock (bottom)**: one button per open window, plus the fixed launchers.
- **Background**: a dark archive motif, drawn once. Keep it subtle so panels read clearly.

### 5.2 Window chrome

- Title bar height: 20 px. Title text in Pale lavender, bold-looking font.
- Border: 1 px Amethyst. Focused window border: Electric cyan.
- Close button: a small square at the right of the title bar, Hot magenta when focused.
- Drop shadow: Deep violet, offset 4 px right and down.
- Window body background: Midnight.

### 5.3 Font

- Built-in 8x16 bitmap font, one file (`kernel/gui/font.c`). No external font files.
- Supports printable ASCII 32-126 only.

### 5.4 Keyboard interaction (required)

| Key            | Action                                              |
|----------------|-----------------------------------------------------|
| Tab            | Move focus to the next item (hub tile, dock, rail)  |
| Shift + Tab    | Move focus to the previous item                     |
| Arrow keys     | Move focus inside the hub grid                      |
| Enter          | Open the focused item                               |
| Esc            | Close the focused window, or go back to the hub     |
| 1 - 9          | Jump to a hub tile or dock item (optional)          |

- The focused item always shows a visible cyan outline. The user must always
  be able to see where focus is.
- Mouse support (click, drag) is optional and comes last.

### 5.5 Panels for the exam (priority order)

1. **About Testarossa** - name, version, architecture, build details. Static, real.
2. **System Monitor** - real uptime and tick count; memory only if the allocator exists.
3. **Terminal** - the same shell from the text milestone, drawn in a window.
4. **Archive / Files** - only if RAMFS exists. Shows built-in read-only files.
5. **Messages / Journal** - decorative demo content, clearly labeled as a built-in demo.


## 6. Interaction and feedback rules

- Every key press gets a visible result within one frame. Nothing waits silently.
- Opening or closing a window never blocks the input loop.
- Animations, if any, take under 300 ms and can be turned off.
- No flashing content faster than 3 times per second.
- Errors never freeze the screen. They appear as text, then the system keeps running.


## 7. Accessibility

- Keyboard-only use must be possible for everything.
- Text contrast meets section 3.
- Font size is not smaller than 8x16 pixels on the desktop.
- Scanlines and sparkles have an off switch (planned: a `settings` command).


## 8. Process for changing the design

1. Open a GitHub issue with the label `ux-design`. Describe the change and why.
2. The owner decides. If accepted, update this file in the same pull request as the code.
3. Screenshots of the change go in `docs/screenshots/`.
4. Do not change colours, names, or layout in code without this document saying so.
