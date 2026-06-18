#!/bin/sh
set -e
if [ ! -f "$TOOLDIR/bin/znaxel" ]; then
  if [ "$FETCH_ONLY" = "yes" ]; then
    # Using a stable release tarball from GitHub
    download_src "https://github.com/axel-download-accelerator/axel/releases/download/v2.17.14/axel-2.17.14.tar.xz"
    exit 0
  fi

  cd "$DL_DIR"
  [ ! -d "axel-2.17.14" ] && tar -xzf axel-2.17.14.tar.xz

  cd axel-2.17.14
  ./configure --prefix="$TOOLDIR" --disable-nls

  zngmake -j$(nproc)
  zngmake install

  mv "$TOOLDIR/bin/axel" "$TOOLDIR/bin/znaxel"
fi
