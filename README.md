cmenu is a multi-column dynamic menu for the terminal.

It parses column information (column widths and headers) passed in arguments,
shows the menu and then waits for commands from the input file descriptor.

Currently, the following commands are supported:
 * `+\n`, then N lines with columns texts, where N is the number of columns.
 * `- INDEX\n`, where `INDEX` is a valid entry index: remove entry.
 * `x\n`: remove all entries.

Once the user selects an entry, it prints its index to the output file descriptor
and exits.
Additionally, if the `-enable-custom` option is passed, it prints `c` to the output
file descriptor and exists when the user presses the key `c`.
