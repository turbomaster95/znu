#!/usr/bin/env bash
# deps.sh — print the #include dependency tree of a .c/.h file
# Usage: ./deps.sh [-I dir]... file.c|file.h

set -euo pipefail

SEARCH_DIRS=(".")
TARGET=""
for arg in "$@"; do
    case "$arg" in
        -I*) SEARCH_DIRS+=("${arg#-I}") ;;
        -*)  ;;
        *)   TARGET="$arg" ;;
    esac
done

TARGET="${TARGET:?Usage: $0 [-I dir]... file.c|file.h}"

declare -A VISITED

find_header() {
    local name="$1" from_dir="$2"
    local d
    for d in "${SEARCH_DIRS[@]}"; do
        if [[ -f "$d/$name" ]]; then
            (cd "$d" && realpath "$name")
            return 0
        fi
    done
    if [[ -f "$from_dir/$name" ]]; then
        (cd "$from_dir" && realpath "$name")
        return 0
    fi
    printf '__EXTERNAL__:%s' "$name"
}

get_includes() {
    # Portable: extract header names from #include lines
    grep -E '^[[:space:]]*#[[:space:]]*include' "$1" 2>/dev/null \
        | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*/\1/' \
        || true
}

print_tree() {
    local file="$1" prefix="$2" is_last="$3"

    local base
    base="$(basename "$file")"

    local connector
    if [[ "$is_last" == "yes" ]]; then
        connector="└── "
    else
        connector="├── "
    fi

    if [[ "$file" == __EXTERNAL__:* ]]; then
        local sysname="${file#__EXTERNAL__:}"
        printf '%s%s%s\n' "$prefix" "$connector" "$sysname"
        return
    fi

    if [[ -n "${VISITED[$file]:-}" ]]; then
        printf '%s%s%s (already included)\n' "$prefix" "$connector" "$base"
        return
    fi
    VISITED["$file"]=1

    printf '%s%s%s\n' "$prefix" "$connector" "$base"

    local includes=()
    while IFS= read -r line; do
        [[ -n "$line" ]] && includes+=("$line")
    done < <(get_includes "$file")

    local n=${#includes[@]}
    local i=0
    local child_prefix
    if [[ "$is_last" == "yes" ]]; then
        child_prefix="${prefix}    "
    else
        child_prefix="${prefix}│   "
    fi

    for inc in "${includes[@]}"; do
        (( i++ ))
        local is_last_child="no"
        [[ $i -eq $n ]] && is_last_child="yes"

        local resolved
        resolved="$(find_header "$inc" "$(dirname "$file")")"
        print_tree "$resolved" "$child_prefix" "$is_last_child"
    done
}

# --- Main ---
target_real="$(realpath "$TARGET")"
printf '%s\n' "$(basename "$TARGET")"
VISITED["$target_real"]=1

root_includes=()
while IFS= read -r line; do
    [[ -n "$line" ]] && root_includes+=("$line")
done < <(get_includes "$target_real")

rn=${#root_includes[@]}
ri=0
for inc in "${root_includes[@]}"; do
    (( ri++ ))
    is_last="no"
    [[ $ri -eq $rn ]] && is_last="yes"

    resolved="$(find_header "$inc" "$(dirname "$target_real")")"
    print_tree "$resolved" "" "$is_last"
done   
