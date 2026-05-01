#[
  Copied from https://github.com/khaledh/fustion
  Minimal C library implementation for Nim's `any` target.
]#

{.used.}
{.compile: "libb/src/stdio.c".}

import std/strutils
import efi_types

type
  const_pointer {.importc: "const void *".} = pointer

proc fwrite(buf: const_pointer, size: csize_t, count: csize_t, stream: File): csize_t {.exportc.} =
  let str = $cast[cstring](buf)
  for line in str.splitLines(keepEOL = true):
    consoleOut(line)
    consoleOut("\r")
  return count

proc fflush(stream: File): cint {.exportc.} =
  return 0.cint

proc exit(status: cint) {.exportc, asmNoStackFrame.} =
  asm """
  .loop:
    cli
    hlt
    jmp .loop
  """
