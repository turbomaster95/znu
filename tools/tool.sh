#!/bin/sh
set -e
TOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"

# Helper: Download static web archives
download_src() {
    _url="$1"
    _file=$(basename "$_url")
    
    if [ -f "$DL_DIR/$_file" ]; then
        echo "Source Already Exists: $_file"
    else
        if [ -x "$TOOLDIR/bin/znaria2c" ]; then
            echo "--> Fast-downloading $_file via aria2c..."
            "$TOOLDIR/bin/znaria2c" -s 8 -x 8 -k 1M -d "$DL_DIR" -o "$_file" "$_url"
        else
            echo "--> Downloading $_file via curl (fallback)..."
            curl -LsS -C - "$_url" -o "$DL_DIR/$_file"
        fi
    fi
}

# Helper: Clone active Git repositories
download_src_git() {
    _url="$1"
    _name="$2"
    [ -z "$_name" ] && _name=$(basename "$_url" .git)

    if [ -d "$DL_DIR/$_name" ]; then
        echo "Source Already Exists: $_name"
    else
        echo "--> Cloning $_url (shallow)..."
        git clone --depth 1 "$_url" "$DL_DIR/$_name"
    fi
}

echo "===> Pre-fetching all tool sources..."
export FETCH_ONLY=yes

for script in "$TOOLS_DIR"/*/bld.sh; do
    if [ -f "$script" ]; then
        # Run inside a background subshell so downloads stream simultaneously
        (
            # Intercept download helpers to exit the subshell early 
            # before any compilation blocks can execute!
            . "$script"
        ) &
    fi
done

wait
unset FETCH_ONLY

echo "===> Downloaded All Sources, Going to build NOW!!"
sleep 3
for script in "$TOOLS_DIR"/*/bld.sh; do
    if [ -f "$script" ]; then
        ( . "$script" )
    fi
done
