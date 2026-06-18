#!/bin/sh

# Core parser function
parse_zconf() {
    _file="$1"
    if [ ! -f "$_file" ]; then
        echo "Error: Configuration file '$_file' not found." >&2
        return 1
    fi

    # Reset variables before parsing a new file
    ZCONF_name=""
    ZCONF_finalname=""
    ZCONF_download=""
    ZCONF_depends=""

    # Read line-by-line
    while IFS= read -r line || [ -n "$line" ]; do
        # Ignore empty lines and comment lines starting with #
        case "$line" in
            ""|\#*) continue ;;
        esac

        # If the line doesn't contain an '=', skip it
        case "$line" in
            *=*) ;;
            *) continue ;;
        esac

        # Split into key and value around the first '='
        _key="${line%%=*}"
        _val="${line#*=}"

        # --- Clean Key ---
        while case "$_key" in ' '*|'	'*) true;; *) false;; esac; do _key="${_key#?}"; done
        while case "$_key" in *' '|*'	') true;; *) false;; esac; do _key="${_key%?}"; done

        case "$_key" in
            set\ *) _key="${_key#set }" ;;
        esac
        while case "$_key" in ' '*|'	'*) true;; *) false;; esac; do _key="${_key#?}"; done

        # --- Clean Value ---
        rm_space() {
            while case "$1" in ' '*|'	'*) true;; *) false;; esac; do $1="${1#?}"; done
        }
        while case "$_val" in ' '*|'	'*) true;; *) false;; esac; do _val="${_val#?}"; done
        while case "$_val" in *' '|*'	') true;; *) false;; esac; do _val="${_val%?}"; done

        # Strip outer double or single quotes
        case "$_val" in
            '"'*'"') _val="${_val#\"}"; _val="${_val%\"}" ;;
            "'"*"'") _val="${_val#\'}"; _val="${_val%\'}" ;;
        esac

        # --- Assign Values ---
        case "$_key" in
            name)       ZCONF_name="$_val" ;;
            finalname)  ZCONF_finalname="$_val" ;;
            download)   ZCONF_download="$_val" ;;
            depends)    ZCONF_depends="$_val" ;;
        esac

    done < "$_file"
}

# Helper function to iterate through comma-separated dependencies
process_dependencies() {
    _deps="$1"
    [ -z "$_deps" ] && return 0

    while [ -n "$_deps" ]; do
        _current_dep="${_deps%%,*}"

        while case "$_current_dep" in ' '*|'	'*) true;; *) false;; esac; do _current_dep="${_current_dep#?}"; done
        while case "$_current_dep" in *' '|*'	') true;; *) false;; esac; do _current_dep="${_current_dep%?}"; done

        if [ -n "$_current_dep" ]; then
            echo "   -> Dependency: $_current_dep"
        fi

        case "$_deps" in
            *,*) _deps="${_deps#*,}" ;;
            *) _deps="" ;;
        esac
    done
}

