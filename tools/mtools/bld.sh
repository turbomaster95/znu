#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znmformat" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    rm -rf mtools-4.0.49
    tar -zxf "mtools-4.0.49.tar.gz"
    cd mtools-4.0.49

    znpatch -p1 -f < "$TOOLS_DIR/mtools/shim.patch"
    sed -i 's/strtonum.o @FLOPPYD_IO_OBJ@ @XDF_IO_OBJ@/strtonum.o android_shim.o @FLOPPYD_IO_OBJ@ @XDF_IO_OBJ@/' Makefile.in

    ./configure --prefix="$MULTICALLBIN" $ZCONFLAGS
    zngmake -j"$JOBS"
    zngmake install

    cp -r "$MULTICALLBIN/share" "$TOOLDIR"
    rm -rf "$MULTICALLBIN/share"
fi

SOURCE_DIR="$MULTICALLBIN/bin"
OUTPUT_DIR="$TOOLDIR/bin"
mkdir -p "$OUTPUT_DIR"

for binary in "$SOURCE_DIR"/*; do
    if [ -f "$binary" ] && [ -x "$binary" ]; then
        filename=$(basename "$binary")
        
        wrapper_name="zn$filename"
            
        cat << EOF > "$OUTPUT_DIR/$wrapper_name"
#!/bin/bash
exec -a "$filename" "$binary" "\$@"
EOF
        chmod +x "$OUTPUT_DIR/$wrapper_name"
    fi
done
