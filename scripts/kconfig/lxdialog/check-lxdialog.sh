#!/bin/sh
# Dynamic ncurses checker for Termux/Linux

ldflags() {
    if pkg-config --exists ncursesw 2>/dev/null; then
        pkg-config --libs ncursesw
    elif pkg-config --exists ncurses 2>/dev/null; then
        pkg-config --libs ncurses
    else
        echo "-lncurses"
    fi
}

ccflags() {
    if pkg-config --exists ncursesw 2>/dev/null; then
        pkg-config --cflags ncursesw
    elif pkg-config --exists ncurses 2>/dev/null; then
        pkg-config --cflags ncurses
    else
        echo "-I/usr/include -DCURSES_LOC=\"<ncurses.h>\""
    fi
}

check() {
    CC_CMD="${cc:-gcc}"
    $CC_CMD -xc - -o .lxdialog.tmp $(ccflags) $(ldflags) 2>/dev/null << 'INNER_EOF'
#include <ncurses.h>
int main(void) { return 0; }
INNER_EOF
    if [ $? -ne 0 ]; then
        echo " *** Unable to find ncurses. Please install with: pkg install ncurses" >&2
        exit 1
    fi
}

case "$1" in
    "-check") cc="$2"; check ;;
    "-ccflags") ccflags ;;
    "-ldflags") ldflags ;;
    *) exit 1 ;;
esac
