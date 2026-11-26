#!/usr/bin/env bash
# deps.sh — print the #include dependency tree
# Usage: ./deps.sh [-I dir]... file.c|file.h

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
            return
        fi
    done
    if [[ -f "$from_dir/$name" ]]; then
        (cd "$from_dir" && realpath "$name")
        return
    fi
    printf '__EXT__:%s' "$name"
}

get_includes() {
    # Extract just the header name from each #include line
    sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]\([^>"]*\)[>"].*$/\1/p' "$1"
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

    if [[ "$file" == __EXT__:* ]]; then
        printf '%s%s%s\n' "$prefix" "$connector" "${file#__EXT__:}"
        return
    fi

    if [[ -n "${VISITED[$file]:-}" ]]; then
        printf '%s%s%s (already included)\n' "$prefix" "$connector" "$base"
        return
    fi
    VISITED["$file"]=1

    printf '%s%s%s\n' "$prefix" "$connector" "$base"

    local includes
    includes="$(get_includes "$file")"
    [[ -z "$includes" ]] && return

    local n
    n=$(echo "$includes" | wc -l)
    local i=0
    local child_prefix
    if [[ "$is_last" == "yes" ]]; then
        child_prefix="${prefix}    "
    else
        child_prefix="${prefix}│   "
    fi

    while IFS= read -r inc; do
        [[ -z "$inc" ]] && continue
        i=$((i + 1))
        local is_last_child="no"
        [[ $i -eq $n ]] && is_last_child="yes"

        local resolved
        resolved="$(find_header "$inc" "$(dirname "$file")")"
        print_tree "$resolved" "$child_prefix" "$is_last_child"
    done <<< "$includes"
}

# --- Main ---
target_real="$(realpath "$TARGET")"
printf '%s\n' "$(basename "$TARGET")"
VISITED["$target_real"]=1

root_includes="$(get_includes "$target_real")"
[[ -z "$root_includes" ]] && exit 0

rn=$(echo "$root_includes" | wc -l)
ri=0
while IFS= read -r inc; do
    [[ -z "$inc" ]] && continue
    ri=$((ri + 1))
    is_last="no"
    [[ $ri -eq $rn ]] && is_last="yes"

    resolved="$(find_header "$inc" "$(dirname "$target_real")")"
    print_tree "$resolved" "" "$is_last"
done <<< "$root_includes"
