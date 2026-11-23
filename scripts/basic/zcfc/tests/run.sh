#!/bin/sh
set -eu
zcfc=${1:-./zcfc}
tmp=${TMPDIR:-/tmp}/zcfc-test-$$
trap 'rm -f "$tmp.ccb" "$tmp.rtm"' EXIT HUP INT TERM
"$zcfc" build "$(dirname "$0")/test.toml" -o "$tmp.ccb"
"$zcfc" dump "$tmp.ccb" > "$tmp.rtm"
test -s "$tmp.ccb"
test -s "$tmp.rtm"
printf '%s\n' 'zcfc smoke test passed'
