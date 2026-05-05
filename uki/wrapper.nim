import efi_types, malloc

# ──────────────────────────────────────────────────────────────────────────────
# Compile-time knob
#   -d:useRamdiskOnly  → skip SFSP, always use the in-memory ramdisk
# ──────────────────────────────────────────────────────────────────────────────
const UseRamdiskOnly {.booldefine.} = false

proc panicOverride(msg: string) {.noreturn.} =
  while true:
    asm "hlt"

proc nimGetMem(size: int): pointer {.cdecl.} = malloc(cast[csize_t](size))
proc nimFreeMem(p: pointer) {.cdecl.} = free(p)

{.compile: "stubs.c".}

proc copyBytes(dest: pointer, src: pointer, n: uint) =
  let d = cast[ptr UncheckedArray[uint8]](dest)
  let s = cast[ptr UncheckedArray[uint8]](src)
  for i in 0 ..< n: d[i] = s[i]

proc zeroBytes(dest: pointer, n: uint) =
  let d = cast[ptr UncheckedArray[uint8]](dest)
  for i in 0 ..< n: d[i] = 0

proc print(conOut: ptr SimpleTextOutput, s: string) =
  var buf: array[512, uint16]
  for i, c in s:
    if i < 511: buf[i] = uint16(c)
  buf[min(s.len, 511)] = 0
  discard conOut.outputString(conOut, cast[WideCString](addr buf))

# ──────────────────────────────────────────────────────────────────────────────
# Compile-time embedded binaries
# ──────────────────────────────────────────────────────────────────────────────
const kernelData     = slurp("../znu")
const initrdData     = slurp("../configs/iso_root/boot/initramfs.cpio")
const limineEfiData  = slurp("../scripts/limine/bin/BOOTX64.EFI")
const limineConfData = slurp("../configs/limine.conf")

# ──────────────────────────────────────────────────────────────────────────────
# Proc-type aliases
# ──────────────────────────────────────────────────────────────────────────────
type
  HandleProtocolProc = proc(handle: EfiHandle, protocol: ptr EfiGuid,
      iface: ptr pointer): EfiStatus {.cdecl.}
  LoadImageProc = proc(bootPolicy: bool, parent: EfiHandle, dp: pointer,
      srcBuf: pointer, srcSize: uint, imgHandle: ptr EfiHandle): EfiStatus {.cdecl.}
  StartImageProc = proc(imgHandle: EfiHandle, exitSize: ptr uint,
      exitData: ptr pointer): EfiStatus {.cdecl.}
  OpenVolumeProc = proc(this: ptr EfiSimpleFileSystemProtocol,
      root: ptr ptr EfiFileProtocol): EfiStatus {.cdecl.}
  FileOpenProc = proc(this: ptr EfiFileProtocol,
      newHandle: ptr ptr EfiFileProtocol, fileName: ptr uint16,
      openMode: uint64, attrs: uint64): EfiStatus {.cdecl.}
  FileCloseProc = proc(this: ptr EfiFileProtocol): EfiStatus {.cdecl.}
  FileWriteProc = proc(this: ptr EfiFileProtocol, bufSize: ptr uint,
      buf: pointer): EfiStatus {.cdecl.}

const
  FILE_MODE_RWC = 0x8000000000000003'u64
  FILE_ATTR_DIR = 0x10'u64

var wbuf1: array[64, uint16]
var wbuf2: array[64, uint16]

proc toWide(dst: var array[64, uint16], s: string): ptr uint16 =
  for i, c in s:
    if i < 63: dst[i] = uint16(c)
  dst[min(s.len, 63)] = 0
  return addr dst[0]

# ──────────────────────────────────────────────────────────────────────────────
# SMBIOS Type-11 injection
# ──────────────────────────────────────────────────────────────────────────────
proc injectSmbios(st: ptr EfiSystemTable, oemStr: string): EfiStatus =
  var ep: ptr Smbios3EntryPoint = nil
  for i in 0 ..< st.numTableEntries:
    if st.configTable[i].vendorGuid == EfiSmbiosTableGuid:
      ep = cast[ptr Smbios3EntryPoint](st.configTable[i].vendorTable)
      break
  if ep.isNil: return 1
  let newSz = ep.tableMaximumSize + uint(5 + oemStr.len + 2)
  var newTbl: pointer = nil
  discard st.bootServices.allocatePool(EfiLoaderData, newSz, addr newTbl)
  if newTbl.isNil: return 1
  copyBytes(newTbl, cast[pointer](ep.tableAddress), ep.tableMaximumSize)
  let t11 = cast[ptr SmbiosType11](cast[uint64](newTbl) + ep.tableMaximumSize)
  t11.header.`type` = 11; t11.header.length = 5
  t11.header.handle = 0xDEFA; t11.count = 1
  let sp = cast[ptr UncheckedArray[uint8]](cast[uint64](newTbl) +
      ep.tableMaximumSize + 5)
  for i, c in oemStr: sp[i] = uint8(c)
  sp[oemStr.len] = 0; sp[oemStr.len + 1] = 0
  var nep: ptr Smbios3EntryPoint = nil
  discard st.bootServices.allocatePool(EfiLoaderData,
    uint(sizeof(Smbios3EntryPoint)), cast[ptr pointer](addr nep))
  copyBytes(nep, ep, uint(sizeof(Smbios3EntryPoint)))
  nep.tableAddress = cast[uint64](newTbl)
  nep.tableMaximumSize = cast[uint32](newSz)
  nep.checksum = 0
  for i in 0 ..< st.numTableEntries:
    if st.configTable[i].vendorGuid == EfiSmbiosTableGuid:
      st.configTable[i].vendorTable = cast[uint64](nep); break
  return EfiSuccess

# ──────────────────────────────────────────────────────────────────────────────
# SFSP helpers
# ──────────────────────────────────────────────────────────────────────────────
proc writeToVolume(root: ptr EfiFileProtocol, path: ptr uint16,
    data: string): bool =
  var f: ptr EfiFileProtocol = nil
  let openF = cast[FileOpenProc](root.open)
  if openF(root, addr f, path, FILE_MODE_RWC, 0) != EfiSuccess or f == nil:
    return false
  var sz = uint(data.len)
  discard cast[FileWriteProc](f.write)(f, addr sz, cast[pointer](cstring(data)))
  discard cast[FileCloseProc](f.close)(f)
  return true

proc trySfsp(bs: ptr EfiBootServices,
             hp: HandleProtocolProc,
             ourDevice: EfiHandle,
             conOut: ptr SimpleTextOutput): bool =
  var sfsp: ptr EfiSimpleFileSystemProtocol = nil
  var guidSFSP = EfiSimpleFileSystemProtocolGuid
  discard hp(ourDevice, addr guidSFSP, cast[ptr pointer](addr sfsp))
  if sfsp == nil:
    conOut.print("[SFSP] Not found\r\n"); return false
  conOut.print("[SFSP] OK\r\n")

  var root: ptr EfiFileProtocol = nil
  discard cast[OpenVolumeProc](sfsp.openVolume)(sfsp, addr root)
  if root == nil:
    conOut.print("[SFSP] Root open FAIL\r\n"); return false

  if not writeToVolume(root, toWide(wbuf1, "limine.conf"), limineConfData):
    conOut.print("[SFSP] limine.conf FAIL\r\n")
    discard cast[FileCloseProc](root.close)(root); return false
  conOut.print("[SFSP] limine.conf OK\r\n")

  var bootDir: ptr EfiFileProtocol = nil
  discard cast[FileOpenProc](root.open)(root, addr bootDir,
    toWide(wbuf1, "boot"), FILE_MODE_RWC, FILE_ATTR_DIR)
  if bootDir != nil: discard cast[FileCloseProc](bootDir.close)(bootDir)

  if not writeToVolume(root, toWide(wbuf1, "boot\\kernel.bin"), kernelData):
    conOut.print("[SFSP] kernel.bin FAIL\r\n")
    discard cast[FileCloseProc](root.close)(root); return false
  conOut.print("[SFSP] kernel.bin OK\r\n")

  if not writeToVolume(root, toWide(wbuf2, "boot\\initramfs.cpio"), initrdData):
    conOut.print("[SFSP] initramfs.cpio FAIL\r\n")
    discard cast[FileCloseProc](root.close)(root); return false
  conOut.print("[SFSP] initramfs.cpio OK\r\n")

  discard cast[FileCloseProc](root.close)(root)
  return true

# ──────────────────────────────────────────────────────────────────────────────
# Ramdisk: allocate EfiLoaderData pages for each blob.
# The pages survive ExitBootServices.  Limine is pointed at ourDevice for its
# protocol lookup; the physical addresses of the blobs can be referenced in
# limine.conf via a physical-address or memory: URI if your Limine build
# supports it, or passed through the SMBIOS-injected config string above.
# ──────────────────────────────────────────────────────────────────────────────
type RamdiskSlot = object
  base: pointer
  size: uint

var rdConf*:   RamdiskSlot
var rdKernel*: RamdiskSlot
var rdInitrd*: RamdiskSlot

proc rdAlloc(bs: ptr EfiBootServices, data: string,
             slot: ptr RamdiskSlot): bool =
  let sz    = uint(data.len)
  let pages = (sz + 4095) div 4096
  var phys: EfiPhysicalAddress = 0
  if bs.allocatePages(AllocateAnyPages, EfiLoaderData,
                      pages, addr phys) != EfiSuccess or phys == 0:
    return false
  zeroBytes(cast[pointer](phys), pages * 4096)
  copyBytes(cast[pointer](phys), cast[pointer](cstring(data)), sz)
  slot.base = cast[pointer](phys)
  slot.size = sz
  return true

proc tryRamdisk(bs: ptr EfiBootServices,
                conOut: ptr SimpleTextOutput): bool =
  conOut.print("[RD] Allocating...\r\n")
  if not rdAlloc(bs, limineConfData, addr rdConf):
    conOut.print("[RD] limine.conf FAIL\r\n"); return false
  conOut.print("[RD] limine.conf OK\r\n")
  if not rdAlloc(bs, kernelData, addr rdKernel):
    conOut.print("[RD] kernel FAIL\r\n"); return false
  conOut.print("[RD] kernel OK\r\n")
  if not rdAlloc(bs, initrdData, addr rdInitrd):
    conOut.print("[RD] initrd FAIL\r\n"); return false
  conOut.print("[RD] initrd OK\r\n")
  return true

# ──────────────────────────────────────────────────────────────────────────────
# EFI entry point
# ──────────────────────────────────────────────────────────────────────────────
proc EfiMain*(ImageHandle: EfiHandle,
    SystemTable: ptr EfiSystemTable): EfiStatus {.exportc, cdecl.} =
  sysTable = SystemTable
  let bs     = SystemTable.bootServices
  let conOut = SystemTable.conOut

  discard conOut.clearScreen(conOut)
  conOut.print("[znu] UKI Loader\r\n")

  when UseRamdiskOnly:
    conOut.print("[znu] Mode: RAMDISK ONLY\r\n")
  else:
    conOut.print("[znu] Mode: SFSP + ramdisk fallback\r\n")

  let hp = cast[HandleProtocolProc](bs.handleProtocol)

  # 1. SMBIOS
  let cfg = "limine:config:" & limineConfData
  if injectSmbios(SystemTable, cfg) == EfiSuccess:
    conOut.print("[1] SMBIOS OK\r\n")
  else:
    conOut.print("[1] SMBIOS FAIL\r\n")

  # 2. Our own device handle
  var selfImg: ptr EfiLoadedImageProtocol = nil
  var guidLIP = EfiLoadedImageProtocolGuid
  discard hp(ImageHandle, addr guidLIP, cast[ptr pointer](addr selfImg))
  if selfImg == nil:
    conOut.print("[2] LoadedImage FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess
  let ourDevice = selfImg.deviceHandle
  conOut.print("[2] Device handle OK\r\n")

  # 3. Write files to SFSP or ramdisk
  var ramdiskActive = false

  when UseRamdiskOnly:
    if not tryRamdisk(bs, conOut):
      conOut.print("[znu] Ramdisk FAIL — halting\r\n")
      discard bs.stall(bs, 8_000_000); return EfiSuccess
    ramdiskActive = true
  else:
    if trySfsp(bs, hp, ourDevice, conOut):
      conOut.print("[znu] SFSP ready\r\n")
    else:
      conOut.print("[znu] SFSP failed — ramdisk fallback\r\n")
      if not tryRamdisk(bs, conOut):
        conOut.print("[znu] Ramdisk FAIL — halting\r\n")
        discard bs.stall(bs, 8_000_000); return EfiSuccess
      ramdiskActive = true

  # 4. Load Limine from embedded buffer
  conOut.print("[4] Loading Limine...\r\n")
  var limineHandle: EfiHandle = nil
  let loadImg = cast[LoadImageProc](bs.loadImage)
  if loadImg(false, ImageHandle, nil,
             cast[pointer](cstring(limineEfiData)),
             uint(limineEfiData.len), addr limineHandle) != EfiSuccess or
      limineHandle == nil:
    conOut.print("[4] Limine load FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess

  # 5. Patch Limine's deviceHandle → ourDevice in both paths.
  #    SFSP: Limine will OpenVolume on ourDevice and find the files we wrote.
  #    Ramdisk: ourDevice is still valid; the blob pages are EfiLoaderData and
  #    survive ExitBootServices so Limine can reach them by physical address.
  var limineImg: ptr EfiLoadedImageProtocol = nil
  discard hp(limineHandle, addr guidLIP, cast[ptr pointer](addr limineImg))
  if limineImg != nil:
    limineImg.deviceHandle = ourDevice
    if ramdiskActive:
      conOut.print("[5] deviceHandle → ourDevice (ramdisk)\r\n")
    else:
      conOut.print("[5] deviceHandle → ourDevice (SFSP)\r\n")
  else:
    conOut.print("[5] Patch FAIL — starting anyway\r\n")

  # 6. Hand off to Limine
  conOut.print("[6] Starting Limine...\r\n")
  discard cast[StartImageProc](bs.startImage)(limineHandle, nil, nil)

  conOut.print("Limine returned unexpectedly\r\n")
  discard bs.stall(bs, 8_000_000)
  return EfiSuccess
