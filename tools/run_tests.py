#!/usr/bin/env python3
"""
Testarossa OS - automated test harness.

Drives QEMU through its monitor socket so the boot log, the text shell, and
the desktop can be checked without a human watching the window. It covers the
parts of to-test.txt that can be judged from serial output and screen dumps:
the boot log, the boot-time self-tests, and the text shell commands.

It does NOT cover mouse edge cases or long-run stability, because those need
timing a human can judge better than a script.

Usage:
    python3 tools/run_tests.py                  # build + run everything
    python3 tools/run_tests.py --boot-only      # boot log and self-tests only
    python3 tools/run_tests.py --shell-only     # text shell only
    python3 tools/run_tests.py --shots          # also write PNG screenshots
    python3 tools/run_tests.py --list           # show the tests, run nothing

Requirements: nasm, qemu-system-i386, make, gcc (same as the README), and
Python 3.8 or newer. No third-party Python packages are needed.

Exit status is 0 when every selected test passes, 1 otherwise.
"""

import argparse
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
KERNEL = os.path.join(BUILD, "testarossa.elf")
SHOTS = os.path.join(BUILD, "shots")
SERIAL = os.path.join(BUILD, "serial.log")
MONITOR_PORT = 45454

GREEN, RED, YELLOW, DIM, RESET = "\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[0m"


# --------------------------------------------------------------------------
# results
# --------------------------------------------------------------------------

class Results:
    def __init__(self):
        self.rows = []

    def add(self, name, ok, detail=""):
        self.rows.append((name, ok, detail))
        colour = GREEN if ok else RED
        mark = "PASS" if ok else "FAIL"
        print("  [%s%s%s] %-42s %s%s%s" % (colour, mark, RESET, name,
                                           DIM, detail, RESET))
        return ok

    @property
    def failed(self):
        return [r for r in self.rows if not r[1]]

    def summary(self):
        total = len(self.rows)
        bad = len(self.failed)
        print()
        if bad:
            print("%s%d of %d tests FAILED%s" % (RED, bad, total, RESET))
            for name, _, detail in self.failed:
                print("    %s: %s" % (name, detail))
        else:
            print("%sall %d tests passed%s" % (GREEN, total, RESET))
        return bad


# --------------------------------------------------------------------------
# serial log
# --------------------------------------------------------------------------

class SerialLog:
    """Reads the serial log file, tracking how much has already been consumed.

    QEMU writes with -serial file:..., which is append-only and flushed as the
    kernel writes. We keep an offset so each test only looks at output that
    arrived after the previous read.
    """

    def __init__(self, path):
        self.path = path
        self.offset = 0

    def new_text(self):
        try:
            with open(self.path, "rb") as fh:
                fh.seek(self.offset)
                data = fh.read()
                self.offset = fh.tell()
        except FileNotFoundError:
            return ""
        return data.decode("utf-8", errors="replace")

    def all_text(self):
        try:
            with open(self.path, "rb") as fh:
                return fh.read().decode("utf-8", errors="replace")
        except FileNotFoundError:
            return ""

    def wait_for(self, needle, timeout=20.0, interval=0.1):
        """Wait until needle shows up in newly arrived output."""
        deadline = time.time() + timeout
        seen = ""
        while time.time() < deadline:
            chunk = self.new_text()
            if chunk:
                seen += chunk
                if needle in seen:
                    return True
            time.sleep(interval)
        # Keep whatever we consumed; the caller can inspect via all_text().
        return needle in seen


# --------------------------------------------------------------------------
# QEMU monitor
# --------------------------------------------------------------------------

class Monitor:
    """Minimal client for the QEMU human monitor over TCP."""

    def __init__(self, port=MONITOR_PORT, host="127.0.0.1", timeout=20.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(2.0)
        self._drain()

    def _drain(self):
        """Consume the banner and the first prompt."""
        deadline = time.time() + 10.0
        buf = ""
        while time.time() < deadline:
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            buf += chunk.decode("utf-8", errors="replace")
            if buf.rstrip().endswith("(qemu)"):
                break
        return buf

    def cmd(self, command, settle=0.08):
        self.sock.sendall((command + "\n").encode())
        time.sleep(settle)
        out = ""
        deadline = time.time() + 5.0
        while time.time() < deadline:
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            out += chunk.decode("utf-8", errors="replace")
            # The prompt is the end of every response.
            if out.rstrip().endswith("(qemu)"):
                break
        return out

    # -- convenience wrappers ------------------------------------------------

    def sendkey(self, *keys, delay=0.03):
        for key in keys:
            self.cmd("sendkey %s" % key)
            time.sleep(delay)

    def type_text(self, text, delay=0.02):
        """Type a literal string one character at a time."""
        for ch in text:
            self.sendkey(KEYMAP.get(ch, ch), delay=delay)

    def mouse_move(self, dx, dy):
        self.cmd("mouse_move %d %d" % (dx, dy))

    def mouse_click(self, button=1):
        self.cmd("mouse_button %d" % button)
        time.sleep(0.05)
        self.cmd("mouse_button 0")

    def screendump(self, path):
        self.cmd("screendump %s" % path, settle=0.4)
        return path


# QEMU sendkey names. Characters QEMU does not know by a single letter are
# mapped to the punctuation names below.
KEYMAP = {
    " ": "spc", "\n": "ret", "\t": "tab", "\b": "backspace",
    "-": "minus", "=": "equal", "[": "bracket_left", "]": "bracket_right",
    "\\": "backslash", ";": "semicolon", "'": "apostrophe",
    "`": "grave_accent", ",": "comma", ".": "dot", "/": "slash",
    "!": "shift-1", "@": "shift-2", "#": "shift-3", "$": "shift-4",
    "%": "shift-5", "^": "shift-6", "&": "shift-7", "*": "shift-8",
    "(": "shift-9", ")": "shift-0", "_": "shift-minus", "+": "shift-equal",
    "{": "shift-bracket_left", "}": "shift-bracket_right",
    ":": "shift-semicolon", '"': "shift-apostrophe", "~": "shift-grave_accent",
    "<": "shift-comma", ">": "shift-dot", "?": "shift-slash",
    "|": "shift-backslash",
}


# --------------------------------------------------------------------------
# PPM -> PNG, with no third-party packages
# --------------------------------------------------------------------------

def ppm_to_png(ppm_path, png_path):
    """Convert a binary PPM (P6) written by QEMU's screendump into a PNG."""
    with open(ppm_path, "rb") as fh:
        data = fh.read()

    if not data.startswith(b"P6"):
        raise ValueError("%s is not a binary PPM" % ppm_path)

    # Header: P6, then width height, then maxval, separated by whitespace.
    fields = []
    pos = 2
    while len(fields) < 3:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":                     # comment to end of line
            while pos < len(data) and data[pos] != 0x0A:
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    pos += 1                                               # single whitespace after maxval

    width, height, maxval = fields
    if maxval != 255:
        raise ValueError("only 8-bit PPM is supported, got maxval %d" % maxval)

    pixels = data[pos:pos + width * height * 3]
    if len(pixels) < width * height * 3:
        raise ValueError("truncated PPM payload")

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)                                      # filter type 0 (None)
        raw += pixels[y * stride:(y + 1) * stride]

    def chunk(tag, payload):
        body = tag + payload
        return (struct.pack(">I", len(payload)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")

    with open(png_path, "wb") as fh:
        fh.write(png)
    return png_path


# --------------------------------------------------------------------------
# build and QEMU lifecycle
# --------------------------------------------------------------------------

def have_tools():
    missing = [t for t in ("nasm", "make", "gcc", "qemu-system-i386")
               if shutil.which(t) is None]
    if missing:
        print("%sMissing tools: %s%s" % (RED, ", ".join(missing), RESET))
        print("Install them first:")
        print("    sudo apt update")
        print("    sudo apt install -y nasm qemu-system-x86 make gcc")
        return False
    return True


def run_make(target, extra=None):
    cmd = ["make", target]
    if extra:
        cmd += extra
    print("%s$ %s%s" % (DIM, " ".join(cmd), RESET))
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr)
    return proc.returncode == 0


class Qemu:
    def __init__(self, no_graphics=False):
        self.no_graphics = no_graphics
        self.proc = None
        self.monitor = None

    def start(self):
        for path in (SERIAL,):
            if os.path.exists(path):
                os.remove(path)
        os.makedirs(BUILD, exist_ok=True)
        os.makedirs(SHOTS, exist_ok=True)

        cmd = [
            "qemu-system-i386",
            "-kernel", KERNEL,
            "-serial", "file:%s" % SERIAL,
            "-monitor", "tcp:127.0.0.1:%d,server,nowait" % MONITOR_PORT,
            "-display", "none",
            "-no-reboot",
        ]
        # Both builds use -vga std. The text build is produced by compiling
        # without the graphics path, so fb_init is skipped and the kernel falls
        # through to shell_run(). QEMU has no "text" VGA type; asking for one
        # makes it exit before the monitor socket ever accepts a connection.
        cmd += ["-vga", "std"]
        print("%s$ %s%s" % (DIM, " ".join(cmd), RESET))
        self.proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.STDOUT)
        time.sleep(0.5)
        try:
            self.monitor = Monitor()
        except OSError as exc:
            out = ""
            if self.proc.poll() is not None:
                # QEMU already exited; surface why instead of a bare refusal.
                self.proc = subprocess.Popen(
                    cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True)
                out = (self.proc.communicate()[0] or "").strip()
            self.stop()
            raise SystemExit(
                "Could not open the QEMU monitor on port %d: %s\n%s"
                % (MONITOR_PORT, exc, out))
        return self

    def stop(self, graceful=False):
        """Stop QEMU. With graceful=True, ask the monitor to quit first.

        A crash build ends in `cli; hlt`, so QEMU never returns to its main
        loop and never flushes its serial stdio on its own. SIGTERM then throws
        the buffered tail away, which loses "system halted". Sending `quit`
        through the monitor makes QEMU exit cleanly and flush.
        """
        if self.proc and self.proc.poll() is None and graceful:
            try:
                self.monitor.cmd("quit", settle=0.5)
                self.proc.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                pass
        try:
            if self.monitor:
                self.monitor.sock.close()
        except OSError:
            pass
        if self.proc:
            if self.proc.poll() is None:
                self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()


# --------------------------------------------------------------------------
# the tests
# --------------------------------------------------------------------------

BOOT_LINES = [
    "Testarossa OS v0.1.0 booting...",
    "GDT loaded",
    "IDT loaded",
    "PIC remapped to vectors 32-47",
    "PIT programmed to 100 Hz",
    "PMM: memory map read",
    "PMM self-test: PASS",
    "Paging on: first 1 GiB identity-mapped with 4 MiB pages",
    "paging self-test: PASS",
    "heap self-test: PASS",
]

SELFTESTS = [
    "PMM self-test: PASS",
    "paging self-test: PASS",
    "heap self-test: PASS",
]

SHELL_CASES = [
    # (keys to type, what serial must contain afterwards)
    (["help"], "Commands:"),
    (["version"], "Testarossa OS 0.1.0"),
    # The shell echoes every typed character to serial, so "hello there" is
    # already in the log before echo runs. Match the start of a line instead,
    # which only the command's own output can produce.
    (["echo hello there"], "\nhello there"),
    (["uptime"], "ticks at 100 Hz"),
    (["mem"], "usable"),
    (["ls"], "3 files"),
    (["cat /readme.txt"], "Testarossa OS keeps"),
    (["cat /archive/entry01.txt"], "ENTRY 01"),
    (["cat /system/about.txt"], "TESTAROSSA OS 0.1.0"),
    (["cat /nope.txt"], "no such file"),
    (["bogus"], "unknown command: bogus"),
]


def test_boot_log(log, res, text=None):
    """to-test.txt TEST 2: every boot line appears, in order."""
    if text is None:
        text = log.all_text()

    reached = "framebuffer:" in text
    res.add("Boot log reached the framebuffer line", reached,
            "" if reached else "no framebuffer line; serial had %d bytes" % len(text))

    missing = [line for line in BOOT_LINES if line not in text]
    res.add("Boot log has every expected line", not missing,
            "missing: %s" % ", ".join(missing) if missing else "10 of 10 lines")

    # The lines must appear in the documented order. Only compare pairs where
    # both lines were actually found: a missing line yields -1 and would make
    # an unrelated pair look out of order, failing this check twice for one
    # problem. The missing-lines check above already reports it.
    positions = [text.find(line) for line in BOOT_LINES]
    pairs = [(a, b) for a, b in zip(positions, positions[1:])
             if a >= 0 and b >= 0]
    in_order = all(a < b for a, b in pairs)
    res.add("Boot log lines are in order", in_order, "" if in_order else "out of order")

    failed = [name for name in SELFTESTS if name not in text]
    no_fail = "self-test: FAIL" not in text
    res.add("No self-test reports FAIL", no_fail,
            "did not report: %s" % ", ".join(failed) if failed else "")
    return True


def test_selftests(log, res, text=None):
    """to-test.txt TESTS 20-21: the self-tests ran, not just the boot lines."""
    if text is None:
        text = log.all_text()
    passed = [name for name in SELFTESTS if name in text]
    res.add("All three self-tests reported PASS", len(passed) == len(SELFTESTS),
            "%d of %d" % (len(passed), len(SELFTESTS)))


def test_framebuffer(log, res, text=None, shots=False):
    """The desktop framebuffer line, from to-test.txt TEST 2."""
    if text is None:
        text = log.all_text()
    match = re.search(r"framebuffer: ([^\r\n]+)", text)
    if not match:
        res.add("Framebuffer reported a mode", False, "no framebuffer line")
        return
    detail = match.group(1).strip()
    ok = "800x600x32" in detail
    res.add("Framebuffer reported a mode", ok, detail)


def run_shell_tests(log, mon, res, shots=False):
    """to-test.txt PART C: the text shell commands."""
    if not log.wait_for("Shell ready", timeout=20):
        res.add("Text shell started", False, "no 'Shell ready' on serial")
        return

    res.add("Text shell started", True)

    for keys, expect in SHELL_CASES:
        label = "shell: %s" % " ".join(keys)
        log.new_text()                                     # discard anything pending
        # sendkey takes one key name per call, so "help" must go as h, e, l, p.
        # type_text does that and maps punctuation and spaces to QEMU key names.
        mon.type_text(" ".join(keys))
        mon.sendkey("ret")
        got = log.wait_for(expect, timeout=8)
        res.add(label, got, "" if got else "serial never showed %r" % expect)


def run_crash_test(res, log):
    """to-test.txt TESTS 23-24: the crash builds report and halt."""
    for target, needle, label in (
        ("fault-test", "EXCEPTION divide error", "Divide-by-zero build reports #DE"),
        ("pf-test", "EXCEPTION page fault", "Page-fault build reports #PF"),
    ):
        if not run_make(target):
            res.add(label, False, "make %s failed" % target)
            run_make("clean")
            continue

        if os.path.exists(SERIAL):
            os.remove(SERIAL)

        qemu = Qemu().start()
        try:
            got = SerialLog(SERIAL)
            # Wait for the halt itself, not just the exception line, so we know
            # the whole message is in the log before we ask QEMU to quit.
            got.wait_for("system halted", timeout=25)
        finally:
            # quit first: this build halts the CPU, so QEMU would otherwise be
            # killed and its buffered serial tail (including "system halted")
            # would be lost. Read the file only after QEMU has flushed.
            qemu.stop(graceful=True)

        text = got.all_text()

        ok = needle in text and "system halted" in text
        if needle not in text:
            detail = "no %r on serial" % needle
        elif "system halted" not in text:
            detail = "reported the fault but did not halt"
        else:
            detail = ""
        res.add(label, ok, detail)

        addr = re.search(r"address=0x[0-9A-Fa-f]+", text)
        if addr:
            res.add("Page fault printed the faulting address", True, addr.group(0))

        run_make("clean")

    # Leave the normal kernel behind. The crash builds end on `make clean`,
    # which would otherwise leave no build at all, exactly like to-test.txt
    # warns after TESTS 23-24.
    run_make("all")


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

ALL_TESTS = [
    ("boot log", test_boot_log),
    ("self-tests", test_selftests),
    ("framebuffer mode", test_framebuffer),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--boot-only", action="store_true",
                        help="boot log, self-tests, and framebuffer mode only")
    parser.add_argument("--shell-only", action="store_true",
                        help="text shell commands only")
    parser.add_argument("--shots", action="store_true",
                        help="write PNG screenshots to build/shots/")
    parser.add_argument("--with-crash", action="store_true",
                        help="also build and run fault-test and pf-test")
    parser.add_argument("--list", action="store_true",
                        help="list the test names and exit")
    args = parser.parse_args()

    if args.list:
        for name, _ in ALL_TESTS:
            print(name)
        print("shell: %d command checks" % len(SHELL_CASES))
        return 0

    if not have_tools():
        return 1

    res = Results()
    want_boot = not args.shell_only
    want_shell = not args.boot_only

    if want_boot:
        print("\nBuilding the desktop kernel")
        if not run_make("clean") or not run_make("all"):
            print("%sBuild failed. Stopping.%s" % (RED, RESET))
            return 1

        print("\nBoot checks (to-test.txt TEST 2, TESTS 20-22)")
        # Let the kernel finish booting, then stop QEMU cleanly before reading
        # the log. Reading while QEMU runs truncates the framebuffer line
        # mid-write, which made this check flap between "pitch 3200", "pit" and
        # just "PCI slot 2," on different runs.
        qemu = Qemu().start()
        log = SerialLog(SERIAL)
        try:
            # The framebuffer line is the last thing a normal boot prints, so
            # waiting for its trailing "pitch N" means the boot is complete.
            log.wait_for("pitch", timeout=30)
        finally:
            qemu.stop(graceful=True)

        text = log.all_text()
        for name, fn in ALL_TESTS:
            fn(log, res, text)
            if args.shots:
                ppm = qemu.monitor.screendump(os.path.join(SHOTS, "desktop.ppm"))
                try:
                    out = ppm_to_png(ppm, os.path.join(SHOTS, "desktop.png"))
                    print("%s  wrote %s%s" % (DIM, out, RESET))
                except (OSError, ValueError) as exc:
                    print("%s  screenshot skipped: %s%s" % (YELLOW, exc, RESET))

    if want_shell:
        print("\nBuilding the text-only kernel (-DTESTAROSSA_NO_GRAPHICS)")
        run_make("clean")
        build = subprocess.run(
            ["make", "EXTRA_CFLAGS=-DTESTAROSSA_NO_GRAPHICS", "all"],
            cwd=ROOT, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout, build.stderr)
            print("%sText build failed. Stopping.%s" % (RED, RESET))
            return 1

        print("\nShell checks (to-test.txt TESTS 12-19)")
        with Qemu(no_graphics=True) as qemu:
            log = SerialLog(SERIAL)
            run_shell_tests(log, qemu.monitor, res, shots=args.shots)

        run_make("clean")
        run_make("all")                                    # leave the normal kernel

    if args.with_crash:
        print("\nCrash builds (to-test.txt TESTS 23-24)")
        log = SerialLog(SERIAL)
        run_crash_test(res, log)

    return 1 if res.summary() else 0


if __name__ == "__main__":
    sys.exit(main())