# PROTOCOL

cmenu speaks two different protocols, depending on the mode it runs in (see USAGE.md for how to
select a mode). Both are line-based (`\n`-terminated) and text-only.

All entry indices are 0-based, counting from the top of the list as currently loaded (indices
shift naturally as entries are added/removed).

## Normal mode

In normal mode, cmenu reads *command packs* from `-infd=FD` and writes results/events to
`-outfd=FD` for as long as it's running. Text (headers, column titles, entry field text) is
decoded from the locale's multibyte encoding (typically UTF-8) and truncated for display using
each character's display width (unprintable characters are shown as `.`). Each line of input given
to cmenu is exactly one field of one entry.

### Command packs (from controlling process to cmenu)

A *command pack* is a line of the form `n NUMBER\n`, followed by exactly `NUMBER` commands, each
on its own line (some commands are themselves followed by further lines; see below). After
reading and applying every command in the pack, cmenu writes `ok\n` to the output fd.

If cmenu hits end-of-file on the input fd, it quits silently without writing anything further.

If any line cmenu reads doesn't parse as a valid command pack header or a valid command (e.g. a
malformed index, or a command referring to an out-of-range index), cmenu treats this as a fatal
protocol error: it prints a diagnostic to stderr and exits with status 1.

A *command* is one of (`NCOLS` is the number of `-column=` arguments cmenu was started with):

  * `+\n`, followed by `NCOLS` lines (one per column, in column order): append a new entry to the
    end of the list.

  * `= INDEX\n`, followed by `NCOLS` lines: replace the entry at index `INDEX` with new field
    text. `INDEX` must currently be valid.

  * `- INDEX\n`: delete the entry at index `INDEX`. `INDEX` must currently be valid. Later entries
    shift down by one index.

  * `x\n`: delete all entries in the list.

  * `w\n`: enter wait mode: the main list is shown with a "..." (ellipsis) informational message
    which can not be discarded; so the user is made aware that they should wait, and any input from
    the user is blocked until `W` command is issue.

  * `W\n`: exit wait mode;

  * `v\n`: query cmenu's supported protocol versions. Currently, cmenu immediately writes `1\n` to
    the output fd (this is the only currently defined protocol version), *before* continuing to
    process the rest of the pack, so a pack containing `v` produces two writes to the output fd:
    `1\n`, and then (once the whole pack is done) `ok\n`.

  * `V VERSION\n`: meant for the controlling process to select which protocol version it wants to
    speak. Currently, `VERSION` must be `1`, or this is a fatal protocol error (see above). It
    produces no output and has no other observable effect on success.

A minimal round-trip for a 2-column menu, adding one entry:

```
controlling process writes:  n 1\n+\nfoo\nbar\n
cmenu writes back:           ok\n
```

### Events (from cmenu to controlling process)

Independently of command packs, user actions produce writes to the output fd. Some of these also
cause cmenu to quit (no further reads or writes will happen after that); others don't, and cmenu
keeps running and reading further command packs as normal.

  * **Quit** (`q` or Ctrl+X --- see CHEATSHEET.md): cmenu quits immediately. **Nothing** is written
    to the output fd.

  * **Confirm selection** (Enter, list non-empty):
    - By default, cmenu writes `result\nINDEX\n` and then quits.
    - If cmenu was started with `-persist=1`, it instead writes `enter\nINDEX\n` and does **not**
      quit --- it keeps running, still showing the list, and can go on receiving command packs and
      further key presses (including further Enter presses, each producing another
      `enter\nINDEX\n`).

  * **Custom command** (`:`, then the command's spelling character, then Enter --- only if the
    matching `-command=` was passed): cmenu writes `custom\n`, then the spelling character
    followed by `\n`, then --- only if the command was declared with the `%` form (acts on an
    entry) --- the currently selected index followed by `\n`. This does **not** cause cmenu to quit;
    it keeps running.

  * **Backslash** (`\`, entry highlighted --- only if `-report-backslash=1` was passed): cmenu
    writes `backslash\n`, then the currently selected index followed by `\n`. Like custom
    commands, this does **not** cause cmenu to quit.

If a write to the output fd ever fails (e.g. because the controlling process has closed its end of
the pipe), cmenu quits immediately regardless of which event triggered the write.

## Bare mode

In bare mode (`-mode=bare`), standard input and standard output take the place of
`-infd=`/`-outfd=`. There is no ongoing exchange of command packs: the entire list is read once, at
startup, and cmenu never reads from standard input again afterwards; there is no way to add, remove
or change entries once cmenu has started drawing the list.

### Initial list (from controlling process to cmenu, read once at startup)

A single header line `N NUMBER\n`, followed by `NUMBER` entries, each written as a `+\n` line
followed by `NCOLS` field lines (same field format as normal mode). No extra lines are accepted,
and nothing is written back.

### Results (from cmenu to controlling process)

Bare mode reports outcomes on a single line each, space-separated instead of newline-separated:

  * **Confirm selection** (Enter, list non-empty): cmenu writes `result INDEX\n` and quits.

  * **Combo** (typing a sequence registered with `-combo=`, then either completing it or pressing
    Enter; see CHEATSHEET.md): cmenu writes `combo COMBO\n`, where `COMBO` is the exact text that
    was typed, and quits. Unless `-require-enter-after-combo=1` was passed, this happens as soon
    as the typed text exactly matches a registered combo, so if that combo is also a prefix of a
    longer registered one, the longer one can never be typed.

  * **Quit** (Ctrl+X only; see CHEATSHEET.md, `q` does *not* quit in bare mode): cmenu quits.
    Nothing is written to standard output.
