# TTY and Terminal

Znu has a kernel TTY layer in `kernel/tty.c`.

It is separate from the terminal renderer: Flanterm/terminal output renders characters, while the TTY layer handles input buffering, line discipline and file operations.

## TTY objects

The implementation maintains:

- an array of TTYs;
- an array of TTY device objects;
- corresponding VFS nodes;
- an active TTY index.

Each TTY has termios state and input buffers.

## Default line discipline

TTY initialization enables:

```text
ICANON
ECHO
ISIG
```

for local flags.

Input flags include:

```text
ICRNL
```

and output processing includes:

```text
OPOST
ONLCR
```

This gives the default terminal the familiar canonical/echo behavior.

## Input

`tty_input_char()` handles incoming characters.

In canonical mode:

1. carriage return can be translated to newline;
2. backspace removes a character from the current line;
3. echo writes the character to the terminal;
4. characters are accumulated in the line buffer;
5. newline transfers the completed line into the cooked buffer;
6. a blocked reader is woken.

Non-canonical input writes directly to the cooked buffer and wakes a reader.

## Blocking reads

`tty_read()` blocks when no input is available unless the file was opened with `O_NONBLOCK`.

The current process is marked:

```text
TASK_WAITING
WAIT_TTY
```

and the TTY stores the waiting reader.

When input completes a readable line, the TTY wakes the process.

## Output

`tty_write()` sends output through the terminal character path.

With `OPOST|ONLCR`, newline output is preceded by carriage return.

The write path also mirrors characters to the kernel debug output.

## ioctl

The current TTY ioctl implementation handles termios get/set requests and window-size reporting.

The reported terminal size is currently:

```text
rows: 25
columns: 80
```

with zero pixel dimensions.

## Device integration

TTYs are registered as VFS device nodes during `tty_init()`.

This allows normal file operations to reach the TTY implementation.

## Keyboard path

The architecture tree contains a PS/2 controller implementation, while `kernel/keyboard.c` handles keyboard input at the kernel level.

The TTY receives characters through `tty_input_char()`.

## Terminal renderer

Znu already uses a terminal rendering layer, so the TTY subsystem should not be confused with a terminal emulator.

The current architecture is approximately:

```text
PS/2 keyboard
      |
      v
keyboard/input handling
      |
      v
    TTY
      |
      +----> read() -> userspace
      |
      +----> write() <- userspace
      |
      v
terminal renderer / Flanterm
      |
      v
 framebuffer
```

## PTYs

Pseudo-terminals are not currently documented as implemented. The existing TTY implementation represents kernel TTY devices; it does not by itself provide the master/slave PTY abstraction used by terminal emulators such as `xterm`, `tmux`, or SSH sessions.
