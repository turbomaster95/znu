#!/usr/bin/env bash

HEADER="$1"
SHEADER=$(echo "$HEADER" | sed 's/\./\\./g')

rg -l -tc -e "#include\\s+[\"<]$SHEADER[\">]" \
  -g '!user/dash/*' \
  -g '!lib/uacpi/*' \
  -g '!lib/ulibc/third-party/linenoise/*' \
  -g '!scripts/kconfig/*' \
  -g '!scripts/basic/*' \
  -g '!scripts/limine/*' \
  -g '!*flanterm*'
