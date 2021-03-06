# PROTOCOL

`cmenu` reads *command packs* from the input file descriptor and writes `ok\n` to the output file
descriptor after having read and applied all the commands from the pack.

A *command pack* consists of the line of form `n NUMBER\n`, where `NUMBER` is the number of commands
in the pack, then `NUMBER` commands in the following lines.

A *command* is either (`NCOLS` is the number of columns):

  * `+\n`, then `NCOLS` lines, each representing the text in the next column: add entry;

  * `= INDEX\n`, then `NCOLS` lines, each representing the text in the next column: change entry with index `INDEX`;

  * `- INDEX\n`: delete entry with index `INDEX`;

  * `x\n`: delete all entries.

If the user presses the `q` key, cmenu quits without writing anything to the output file descriptor.

If the user presses the `c` key and the “custom” command was enabled (the `-enable-custom` option
was passed), cmenu writes `custom\n` to the output file descriptor, and then quits.

If the user presses the Enter key and the list is not empty, cmenu writes `result\nINDEX\n` to the
output file descriptor, where `INDEX` is the index of the selected entry, and then quits.
