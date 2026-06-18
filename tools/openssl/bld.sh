#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znopenssl" ] && [ ! -f "$TOOLDIR/lib/libcrypto.a" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://github.com/openssl/openssl/releases/download/openssl-3.4.1/openssl-3.4.1.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "openssl-3.4.1.tar.gz"
    cd openssl-3.4.1

    # Stripping out everything Axel doesn't care about
    ./config \
        --prefix="$TOOLDIR" \
        --openssldir="$TOOLDIR/ssl" \
        no-shared \
        no-tests \
        no-legacy \
        no-engine \
        no-deprecated \
        no-comp \
        no-ui-console

    make -j"$JOBS"
    
    make install_sw

    if [ -f "$TOOLDIR/bin/openssl" ]; then
        mv "$TOOLDIR/bin/openssl" "$TOOLDIR/bin/znopenssl"
    fi
fi
