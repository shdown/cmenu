cmenu is a multi-column dynamic menu for the terminal.
It should work under Linux and all ✳BSD's.
It requires a C++17 compiler ncurses (wide-character build).

cmenu draws a scrollable, multi-column list in the terminal, lets the user move around it with
the keyboard, and reports the outcome (the selected entry, or a custom command/shortcut the user
invoked) back to whatever process started it. Column widths and headers are given on the command
line; the list contents themselves are supplied separately, either fed in live over a pair of
file descriptors, or loaded once from standard input.

cmenu has two operating modes:

* **Normal mode** (the default) is meant to be driven by a long-running controlling process.
  The list starts out empty and is populated, updated, and cleared by writing simple line-based
  *commands* to a file descriptor that cmenu reads from (`-infd=FD`); cmenu reports the user's
  actions (selection, custom commands, quitting, ...) by writing to a second file descriptor
  (`-outfd=FD`). The controlling process can be a shell script, since the protocol is just text
  lines. Normal mode also supports single-character custom commands (bound with `-command=`) and
  a built-in help screen listing them.

* **Bare mode** (`-mode=bare`) is meant for simple "pick one of these" prompts. The entire list is
  read once from standard input when cmenu starts, and cmenu writes its result to standard output
  when the user picks an entry, quits, or enters one of a fixed set of predefined multi-character
  "combos" (bound with `-combo=`) instead of arbitrary single-character custom commands. There is
  no live updating of the list after startup, and no `-infd=`/`-outfd=` are used since standard
  input/output serve that purpose.

Both modes share the same list-navigation keys, but some keys behave differently between them.
See CHEATSHEET.md for the full key reference.

For the full command-line reference, see USAGE.md. For the exact wire protocol used to feed
entries into cmenu and read results back out, see PROTOCOL.md.

![screenshot](https://user-images.githubusercontent.com/5462697/110216539-b6c33880-7ec0-11eb-8a7d-a5ee3321efc8.png)

# Building and installing

## Build

```bash
cmake . && make
```

## Install (after building)

```bash
sudo make install
```

# License

Unlicense (see https://unlicense.org/).

![Unlicense logo](https://github.com/user-attachments/assets/493002dd-e684-48cb-9b28-1f9f03042dff)
