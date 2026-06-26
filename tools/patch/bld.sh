#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znpatch" ]; then
  if [ "$FETCH_ONLY" = "yes" ]; then
    download_src "https://ftp.gnu.org/gnu/patch/patch-2.8.tar.gz"
    exit 0
  fi

  cd "$DL_DIR"
  [ ! -d "patch-2.8" ] && tar -xzf patch-2.8.tar.gz

  cd patch-2.8
  ./configure --prefix="$TOOLDIR" $ZCONFLAGS AR=znar RANLIB=znranlib

  zngmake -j$(nproc)
  zngmake install

  mv "$TOOLDIR/bin/patch" "$TOOLDIR/bin/znpatch"
fi
