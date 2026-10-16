# Contributing to Testarossa OS

The full rules live in the [Contributing section of the README](README.md#contributing).
This file is the short version, and GitHub shows it on every new issue and pull
request. If the two ever disagree, the README wins.

## Before you change anything

1. Read [docs/DESIGN.md](docs/DESIGN.md) and [docs/FUNCTIONS.md](docs/FUNCTIONS.md).
   The design is locked, and FUNCTIONS.md is where a new function gets declared
   before it gets written.
2. Run the build first, so you know what was already broken:
   ```sh
   make clean && make
   ```

## Rules that are not negotiable

- **Never claim a feature works until you have run it.** A change is not done
  until it has booted in QEMU, and the result goes in [VERIFY.md](VERIFY.md).
- **Do not fake anything.** A panel that shows made-up data is worse than no
  panel. Say "not implemented yet" instead.
- **Update the docs in the same commit.** That means FUNCTIONS.md for functions,
  VERIFY.md for test results, to-test.txt if a test changes, and the Status table
  in the README.
- **Do not change the design** (colours, layout, names, user-facing wording)
  without an approved `ux-design` issue first.
- **Keep the build clean.** No new warnings. The RWX load-segment warning from
  the linker is the only one allowed.

## Branches and commits

- One change per branch, named `feature/<short-name>`, for example
  `feature/mouse`.
- One summary line in the commit message, then a short body saying what changed
  and **how you tested it**.
- Do not commit straight to `main`.

## Testing your change

Run the harness. It builds the kernel, boots it in QEMU, drives it through the
monitor, and checks the boot log and the shell against what the README says:

```sh
python3 tools/run_tests.py                 # everything
python3 tools/run_tests.py --boot-only     # boot log and self-tests
python3 tools/run_tests.py --shell-only    # shell commands
python3 tools/run_tests.py --with-crash    # also the divide-error and page-fault builds
python3 tools/run_tests.py --shots         # save PNG screenshots to build/shots/
```

It needs the same tools as the README (`nasm`, `qemu-system-i386`, `make`,
`gcc`) and Python 3.8 or newer. No third-party Python packages.

If your change touches something the harness does not cover, by hand and say so
in the VERIFY.md entry. Mouse edge cases and long-run stability still need a
human.

## Finding bugs

Use the **Bug report** template. For results from working through to-test.txt,
use the **Test report** template. Write what you pressed, what you expected, and
what you saw. Do not fix the debug challenges in `to-test.txt`, report them
instead: some of them are deliberate.

## Honest status

If a feature is planned but not written, say "not implemented yet" on screen and
in the docs. The project is judged on being defensible in an exam, and a claim
that cannot be demonstrated costs more than a missing feature.