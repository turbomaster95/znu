--cpu:amd64
--os:any
--backend:c
--nomain:on
--mm:arc
--threads:off
--define:no_runtime
--define:useMalloc
--define:nimNoLibc
--define:noSignalHandler
--define:nimBuiltinSetjmp

# Compiler flags
switch("passC", "-target x86_64-unknown-windows")
switch("passC", "-Ilibb/include")
switch("passC", "-ffreestanding")
switch("passC", "-fno-stack-protector")
switch("passC", "-mno-red-zone")

# Linker flags
switch("passL", "-target x86_64-unknown-windows")
switch("passL", "-fuse-ld=lld-link")
switch("passL", "-nostdlib")
switch("passL", "-Wl,-entry:EfiMain")
switch("passL", "-Wl,-subsystem:efi_application")
switch("passL", "-nodefaultlibs")
