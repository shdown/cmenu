cmenu is a multi-column dynamic menu for the terminal.
It is written in standard C99 using the POSIX standard.

It parses column information (column widths and headers) passed in arguments,
shows the menu to the user and interacts with the controlling process using
two file descriptors (which must be passed via `-infd=FD` and `-outfd=FD`).
For more information on usage, see “USAGE.md”.

Once the user selects an entry, it reports this to the controlling process and exits.

The protocol of communication with the controlling process is very simple and line-based
(see “PROTOCOL.md”); the controlling process can even be a shell script.

The `wifi_menu.py` is an example that presents an interactive menu for choosing a Wi-Fi
network to connect to. It uses the [iwd](https://iwd.wiki.kernel.org/) D-Bus API and the `iwctl`
binary from the iwd project.

![screenshot](https://user-images.githubusercontent.com/5462697/110216539-b6c33880-7ec0-11eb-8a7d-a5ee3321efc8.png)
