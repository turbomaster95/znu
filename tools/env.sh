#!/bin/sh

# Appends the built tools into the PATH variable for easy use

PATH="$TOOLDIR/bin:$PATH"
export PATH

# Does some ccache stuff

if command -v ccache >/dev/null 2>&1; then
    export CC="ccache gcc"
    export CXX="ccache g++"
fi
