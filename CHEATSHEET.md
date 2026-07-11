# CHEATSHEET

Key bindings differ between normal mode and bare mode (see USAGE.md/README.md). Where a binding
only applies to one mode, it's called out explicitly below.

`<ESC>` always returns focus to the main list (dismissing any message, command prompt, combo
prompt, or the help screen) --- it never quits cmenu, and it's handled the same way everywhere.
`<CTRL>+L` and terminal resize always requery the screen size and force a full redraw, everywhere.

List (main screen)
===

Available in both modes unless noted:

* `<UP>`, `<CTRL>+P`, and (**normal mode only**) `k`: move selection 1 entry up (wraps around)

* `<DOWN>`, `<CTRL>+N`, and (**normal mode only**) `j`: move selection 1 entry down (wraps around)

* `<HOME>`, and (**normal mode only**) `g`: select first entry

* `<END>`, and (**normal mode only**) `G`: select last entry

* `<CTRL>+G`: show a tooltip with the index of the selected entry, the total number of entries,
  and whether it's the first/last entry or what percentage through the list it is

* `<PAGE_UP>`, `<CTRL>+B`: move selection up by one screenful

* `<PAGE_DOWN>`, `<CTRL>+F`: move selection down by one screenful

* `<CTRL>+U`: move selection up by half a screenful

* `<CTRL>+D`: move selection down by half a screenful

* `<ENTER>`, `<CTRL+M>`, `<CTRL+J>`: confirm selection

* `<CTRL>+X`, and (**normal mode only**) `q`: quit

* `:`: enter command mode (**normal mode onyl**): open the `:` prompt for typing a custom command's
  spelling character

* `\`: report the highlighted entry's index as a "backslash" event (**normal mode only**, and only
  if `-report-backslash=1` was passed)

* Any other character in `[A-Za-z0-9_]` (**bare mode only**): start typing a combo (see "Combo
  mode" below). This is why letters like `q`/`g`/`G`/`j`/`k`, which are vi-style shortcuts in
  normal mode, instead begin a combo in bare mode.

Command mode (normal mode only)
===

Entered with `:` from the list. Lets the user type the single-character spelling of a custom
command (`-command=SPELLING`/`-command=%SPELLING`).

* `<ESC>`: leave command mode, back to the list

* `<CTRL>+L`: requery screen size and redraw

* Any one character in `[A-Za-z0-9_]`: enter it as the command to run (replaces whatever was typed
  before --- only one character is remembered at a time)

* `?`: instead of a command character, open the built-in help screen listing every registered
  custom command (and `\`, if `-report-backslash=1`) and its description, once confirmed with
  Enter. If nothing is registered, shows a "No help because no extra keys defined" message
  instead.

* `<ENTER>`, `<CTRL+M>`, `<CTRL+J>`: confirm the typed command (or open help, if `?` was typed),
  does nothing if nothing has been typed yet

* `<BACKSPACE>`, `<DELETE>`, `<CTRL>+H`: erase the typed character; if nothing was typed, leave
  command mode instead

* `<CTRL>+W`, `<CTRL>+U`: erase the typed character (same effect as backspace when one is typed)

Combo mode (bare mode only)
===

Entered automatically by typing any character in `[A-Za-z0-9_]` from the list (there's no
dedicated "enter combo mode" key --- the first character you type both enters combo mode and becomes
part of the combo). Lets the user type a multi-character combo registered with `-combo=`.

* `<ESC>`: abandon the combo and return to the list

* `<CTRL>+L`: requery screen size and redraw

* Any character in `[A-Za-z0-9_]`: append it to the combo being typed. As you type, an indicator
  (styled with `-style-error=`) is shown if what you've typed so far doesn't match the prefix of
  any registered combo. If what you've typed so far is an *exact* match for a registered combo,
  and `-require-enter-after-combo=1` was **not** passed (the default), the combo is accepted
  **immediately, on that very keystroke** --- no Enter needed, and no chance to type further
  characters. This means that if a shorter combo is a prefix of a longer one (e.g. both `a` and
  `ab` are registered), the shorter one always wins the instant it's typed, and the longer one can
  never actually be entered unless `-require-enter-after-combo=1` is passed (see USAGE.md).

* `<ENTER>`, `<CTRL+M>`, `<CTRL+J>`: confirm the typed combo. If it's an exact match for a
  registered combo, it's accepted (see PROTOCOL.md); otherwise a "No such combo" message is shown
  and the typed text is discarded. Needed to confirm combos that are prefixes of longer combos
  when `-require-enter-after-combo=1` is set.

* `<BACKSPACE>`, `<DELETE>`, `<CTRL>+H`: erase the last typed character; if that empties the
  combo, leave combo mode

* `<CTRL>+W`, `<CTRL>+U`: abandon the combo and leave combo mode immediately

Info/error message bar
===

Shown after actions like `<CTRL>+G`, or after a key press/command that couldn't be carried out
(e.g. "No items", "No such combo", "No such custom command", etc).

* `<ESC>`, `<ENTER>`, `<CTRL+M>`, `<CTRL+J>`: hide the message, back to the list

Help screen (normal mode only)
===

Opened with `:?` then Enter from the list (see "Command mode" above). Lists every registered
custom command and, if enabled, `\`, each with its description. It's implemented by internally
reusing the same list widget as the main screen. Same navigation keys as the main list apply.
