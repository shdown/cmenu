# USAGE

Normal mode (default):
```
cmenu {-column=:TITLE | -column=WIDTH:TITLE | -column=@WIDTH:TITLE [...]} [OPTIONS...]
      -infd=FD -outfd=FD
```

Bare mode:
```
cmenu {-column=:TITLE | -column=WIDTH:TITLE | -column=@WIDTH:TITLE [...]} [OPTIONS...]
      -mode=bare
```

There is no `-h`/`--help` flag. On a command-line error, cmenu prints the problem and a short
usage summary to stderr and exits with status `2`. See "Exit codes" below.

## Column arguments (required, one or more)

At least one `-column=` argument is required. Columns appear on screen left-to-right in the order
they're given on the command line, and every entry (see PROTOCOL.md) must supply exactly that many
fields, in that order.

* `-column=WIDTH:TITLE`, where `WIDTH` is a positive integer, specifies a **variable-width**
  column. Its actual on-screen width is

  ```
  TOTAL_WIDTH_FOR_V * (WIDTH / V_WIDTH_SUM)
  ```

  where `TOTAL_WIDTH_FOR_V` is the terminal width minus the total width of all fixed-width
  columns, and `V_WIDTH_SUM` is the sum of `WIDTH` over all variable-width columns. Any leftover
  space caused by rounding is added to the **last** variable-width column (in command-line order).

* `-column=@WIDTH:TITLE`, where `WIDTH` is a positive integer, specifies a **fixed-width** column
  of exactly `WIDTH` terminal cells, regardless of terminal size.

* `-column=:TITLE` is shorthand for `-column=1:TITLE` (a variable-width column with weight 1).

`WIDTH` must be greater than zero in all forms. `TITLE` is everything after the first `:` in the
value (so a title may itself contain `:` characters).

If the terminal is too small to fit all fixed-width columns (or is otherwise smaller than 3x3),
cmenu shows `(Need more size)` in place of the list until it's resized large enough.

## Mode selection

* `-mode={normal|bare}`: selects the operating mode. Default is `normal`. See README.md for a
  description of the two modes, and CHEATSHEET.md for how key bindings differ between them.

  A number of options are mutually exclusive with `-mode=bare`; cmenu reports a command-line error
  if they're combined. These are called out below.

## Normal-mode-only required arguments

* `-infd=FD`: the file descriptor cmenu reads *command packs* from (see PROTOCOL.md). Required,
  and only allowed, in normal mode.

* `-outfd=FD`: the file descriptor cmenu writes results and events to. Required, and only allowed,
  in normal mode.

  For both, `FD` must be a currently-open file descriptor number `>= 3` (descriptors 0, 1, and 2
  are refused, since they're stdin/stdout/stderr). cmenu checks at startup that the descriptor is
  actually open and exits with an error if not.

  In bare mode, standard input and standard output are used for this purpose instead, so
  `-infd=`/`-outfd=` are not accepted.

## Options

* `-style-header=STYLE`: style of the column header row. Default: bold white on green.

* `-style-hi=STYLE`: style of the currently-highlighted (selected) list entry. Default: white on
  blue.

* `-style-entry=STYLE`: style of non-highlighted list entries. Default: terminal's default colors.

* `-style-error=STYLE`: style used for the info/error message bar (shown e.g. after pressing
  Ctrl+G, or when a key press or command doesn't apply) and for the "invalid combo" indicator in
  bare mode. Default: white on red.

* `-escdelay=MILLISECONDS`: how long (in milliseconds) ncurses waits after seeing an Escape byte
  before deciding it really was a lone `<ESC>` keypress rather than the start of a longer escape
  sequence (e.g. an arrow key). If omitted, the `ESCDELAY` environment variable is honored if set;
  otherwise it defaults to 50 ms.

* `-command=SPELLING`, where `SPELLING` is a single character in `[a-zA-Z0-9_]`: define a custom
  command that does *not* act on a particular list entry. Invoked by pressing `:`, typing
  `SPELLING`, then Enter. Normal-mode only.

* `-command=%SPELLING`: same, but defines a custom command that *does* act on the currently
  selected list entry (its index is reported along with the command). Requires the list to be
  non-empty when invoked.

* `-command=SPELLING:DESCRIPTION` / `-command=%SPELLING:DESCRIPTION`: same as above, but also
  registers `DESCRIPTION` as the human-readable description shown for this command in the built-in
  help screen (opened with `:?` then Enter). A command defined without a description shows
  `(not documented)` there instead.

  Each `SPELLING` character may only be used for one custom command. `-command=` is not allowed in
  bare mode (use `-combo=` instead).

* `-combo=COMBO`, where `COMBO` is one or more characters from `[a-zA-Z0-9_]`: register a
  multi-character shortcut ("combo") the user can type (without needing to enter any special mode
  first) to select an action independent of the highlighted entry. Only allowed in bare mode; may
  be given multiple times. See PROTOCOL.md and CHEATSHEET.md for how combos are typed and
  reported.

* `-require-enter-after-combo={0|1}`: controls what happens once the characters typed so far
  become an exact match for a registered combo. Default `0`: the combo is reported the instant it
  becomes an exact match, without waiting for Enter, which means that if two combos are
  registered where one is a prefix of the other (e.g. both `a` and `ab`), the shorter one (`a`) is
  reported the moment it's typed and there is no way to go on to type the longer one (`ab`); the
  extra characters can never be entered. Passing `1` disables this immediate reporting: an exact
  match is held pending until the user presses Enter (still showing no error), so they get the
  chance to keep typing towards a longer combo, and Enter is always required to confirm any combo.
  Only allowed in bare mode.

* `-report-backslash={0|1|1:DESCRIPTION}`: if `1` (or `1:DESCRIPTION`), pressing `\` while an
  entry is highlighted is reported to the controlling process (see PROTOCOL.md); the list must be
  non-empty. If `0` (the default), pressing `\` shows a "Backslash is not supported" message
  instead. `1:DESCRIPTION` additionally registers `DESCRIPTION` as the description shown for `\`
  in the help screen (otherwise `(not documented)`). Not allowed in bare mode.

* `-persist={0|1}`: if `1`, pressing Enter reports the current selection to the controlling
  process but does **not** cause cmenu to quit afterwards (it keeps running, still showing the
  list, and can keep receiving command packs and further key presses). If `0` (the default),
  pressing Enter reports the selection and quits. Not allowed in bare mode.

## Styles

Each `STYLE` string is a comma-separated list of *style specifiers*. Each specifier is one of:

 * `normal`;
 * `bold`;
 * `blink`;
 * `dim`;
 * `reverse`;
 * `standout`;
 * `underline`;
 * `f=COLOR`: set foreground color;
 * `b=COLOR`: set background color,

where `COLOR` is a non-negative integer (0-7 for the 8 standard ANSI colors; higher values are
accepted if the terminal reports support for more colors via `COLORS`).

If a style specifier string omits `f=` and/or `b=`, that channel falls back to the terminal's own
default foreground/background.

## Exit codes

* `0`: cmenu ran and exited normally (the user quit, made a selection, etc. --- see PROTOCOL.md for
  how the outcome is reported).
* `1`: a runtime error occurred (a malformed command pack, a protocol violation, an ncurses
  failure, an unexpectedly-closed file descriptor, unexpected end-of-file while reading the bare
  mode list, etc.). A message is printed to stderr.
* `2`: a command-line usage error (unknown option, missing required argument, options combined
  incompatibly with the selected mode, etc.). A message and a short usage summary are printed to
  stderr.
