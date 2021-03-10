# USAGE
```
cmenu [OPTIONS] -infd=FD -outfd=FD {-column=:TITLE | -column=WIDTH:TITLE | -column=@WIDTH:TITLE [...]}
```

## Required arguments

### File descriptor arguments

`-infd=FD` specifies the input file descriptor for communication with the controlling process;
and `-outfd=FD` specifies the output file descriptor.

`FD` must denote a valid file descriptor number.

### Column arguments

`-column=WIDTH:TITLE`, where `WIDTH` is integer, specifies a variable-width column; its width will
be `TOTAL_WIDTH_FOR_V * (WIDTH / V_WIDTH_SUM)`, where `TOTAL_WIDTH_FOR_V` is the width of the
terminal minus the total width for fixed-width columns, `V_WIDTH_SUM` is the sum of `WIDTH`s over
all variable-width columns.

`-column=@WIDTH:TITLE`, where `WIDTH` is integer, specifies a fixed-width column with width of
`WIDTH` spaces.

`-column=:TITLE` is equivalent to `-column=1:TITLE`.

## Options

Supported OPTIONS:

 * `-style-header=STYLE`: set the style of the header.

 * `-style-entry=STYLE`: set the style for (non-highlighted) list entries.

 * `-style-hi=STYLE`: set the style for the highlighted list entry.

 * `-command=SPELLING`, where `SPELLING` is a single character in `[a-zA-Z0-9_]`: add custom command (that does *not* act on a list entry).

 * `-command=%SPELLING`, where `SPELLING` is a single character in `[a-zA-Z0-9_]`: add custom command (that *does* act on a list entry).

## Styles

Each `STYLE` string must be comma-separated list of *style specifiers*. Each *style specifiers* must
be one of the following:

 * `normal`;

 * `bold`;

 * `blink`;

 * `dim`;

 * `reverse`;

 * `standout`;

 * `underline`;

 * `f=COLOR`: set foreground color;

 * `b=COLOR`: set background color,

where `COLOR` must be integer (0...7, or greater if your terminal supports additional colors).
