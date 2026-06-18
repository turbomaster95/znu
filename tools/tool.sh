#!/bin/sh
set -e

TOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"

. "$TOOLS_DIR/zconf.sh"

# Helper: Download static web archives
download_src() {
    _url="$1"
    _file=$(basename "$_url")
    _dest="$DL_DIR/$_file"
    _part="$_dest.part"

    if [ -f "$_dest" ] && [ ! -f "$_dest.st" ]; then
        _corrupted=false
        case "$_file" in
            *.tar.gz|*.tgz) gzip -t "$_dest" >/dev/null 2>&1 || _corrupted=true ;;
            *.tar.xz)       xz -t "$_dest" >/dev/null 2>&1 || _corrupted=true ;;
            *.tar.bz2|*.tbz2) bzip2 -t "$_dest" >/dev/null 2>&1 || _corrupted=true ;;
         Tara)          tar -tf "$_dest" >/dev/null 2>&1 || _corrupted=true ;;
        esac

        if $_corrupted; then
            echo "--> [!] $_file is corrupted or truncated. Purging to re-download..."
            rm -f "$_dest"
        else
            echo "Source Verified: $_file"
            return 0
        fi
    fi

    if [ -x "$TOOLDIR/bin/znaxel" ]; then
        echo "--> Fast Downloading $_file via Axel..."
        # Axel creates its own '.st' state file to manage chunk resumes natively
        "$TOOLDIR/bin/znaxel" -n 8 -o "$_dest" "$_url"
    else
        echo "--> Downloading $_file via curl..."
        curl -LsS -C - "$_url" -o "$_part"

        mv "$_part" "$_dest"
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

echo "===> Parsing..."
ALL_TOOLS=""
for script in "$TOOLS_DIR"/*/bld.sh; do
    if [ -f "$script" ]; then
        _t=$(basename "$(dirname "$script")")
        ALL_TOOLS="$ALL_TOOLS $_t"
    fi
done

echo "===> Bootstrapping..."

if [ -d "$TOOLS_DIR/gmake" ]; then
    echo "Processing Bootstrap: gmake"
    ( export FETCH_ONLY=yes; . "$TOOLS_DIR/gmake/bld.sh" )
    ( . "$TOOLS_DIR/gmake/bld.sh" )
fi

if [ -d "$TOOLS_DIR/axel" ]; then
    echo "Processing Bootstrap: axel"
    ( export FETCH_ONLY=yes; . "$TOOLS_DIR/axel/bld.sh" )
    ( . "$TOOLS_DIR/axel/bld.sh" )
fi

echo "===> Now building tools..."
export FETCH_ONLY=yes

for _t in $ALL_TOOLS; do
    # Skip infrastructure pieces we just finished building
    [ "$_t" = "gmake" ] || [ "$_t" = "axel" ] && continue

    _script="$TOOLS_DIR/$_t/bld.sh"
    if [ -f "$_script" ]; then
        (
            _dir="$(dirname "$_script")"
            if [ -f "$_dir/info.txt" ]; then
                parse_zconf "$_dir/info.txt"
            fi
            . "$_script"
        ) &
    fi
done

# Wait for all background downloader parallel subshells to resolve
wait
unset FETCH_ONLY

# Seed our list tracking complete binaries with what we bootstrapped
BUILT_LIST="gmake axel"

# Isolate remainder targets
REMAINING_TOOLS=""
for _t in $ALL_TOOLS; do
    [ "$_t" = "gmake" ] || [ "$_t" = "aria2" ] && continue
    REMAINING_TOOLS="$REMAINING_TOOLS $_t"
done

while [ -n "$REMAINING_TOOLS" ]; do
    _progress=false
    _deferred=""

    for _t in $REMAINING_TOOLS; do
        _tdir="$TOOLS_DIR/$_t"
        
        if [ -f "$_tdir/info.txt" ]; then
            parse_zconf "$_tdir/info.txt"
            _deps="$ZCONF_depends"
        else
            _deps=""
        fi

        # Verify tracking map
        _deps_satisfied=true
        _dep_list="$_deps"
        while [ -n "$_dep_list" ]; do
            _d="${_dep_list%%,*}"
            while case "$_d" in ' '*|'	'*) true;; *) false;; esac; do _d="${_d#?}"; done
            while case "$_d" in *' '|*'	') true;; *) false;; esac; do _d="${_d%?}"; done

            if [ -n "$_d" ]; then
                case " $BUILT_LIST " in
                    *" $_d "*) ;; 
                    *) _deps_satisfied=false; break ;; 
                esac
            fi

            case "$_dep_list" in
                *,*) _dep_list="${_dep_list#*,}" ;;
                *) _dep_list="" ;;
            esac
        done

        if $_deps_satisfied; then
            if [ -f "$_tdir/info.txt" ]; then
                parse_zconf "$_tdir/info.txt"
                echo "========================================="
                echo "Building Target: ${ZCONF_name:-$_t}"
                [ -n "$ZCONF_finalname" ] && echo "Binary Name:     $ZCONF_finalname"
                echo "========================================="
            else
                echo "========================================="
                echo "Building Target: $_t"
                echo "========================================="
            fi
            
            ( . "$_tdir/bld.sh" )
            
            BUILT_LIST="$BUILT_LIST $_t"
            _progress=true
        else
            _deferred="$_deferred $_t"
        fi
    done

    if [ "$_progress" = "false" ]; then
        echo "Error: Unresolvable or circular tracking loops inside:$_deferred" >&2
        exit 1
    fi

    REMAINING_TOOLS="$_deferred"
done

echo "===> All tools built!"
