import efi_types, malloc

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

proc print(conOut: ptr SimpleTextOutput, s: string) =
  var buf: array[512, uint16]
  for i, c in s:
    if i < 511: buf[i] = uint16(c)
  buf[min(s.len, 511)] = 0
  discard conOut.outputString(conOut, cast[WideCString](addr buf))

# Compile-time embedded binaries
const kernelData = slurp("../znu")
const initrdData = slurp("../configs/iso_root/boot/initramfs.cpio")
const limineEfiData = slurp("../scripts/limine/bin/BOOTX64.EFI")
const limineConfData = slurp("../configs/limine.conf")

# Correct BootServices proc types (all take ptr EfiGuid)
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
  FILE_MODE_RWC = 0x8000000000000003'u64 # READ|WRITE|CREATE
  FILE_ATTR_DIR = 0x10'u64

# Global wide-char path buffers (reused per call — never overlapping)
var wbuf1: array[64, uint16]
var wbuf2: array[64, uint16]

proc toWide(dst: var array[64, uint16], s: string): ptr uint16 =
  for i, c in s:
    if i < 63: dst[i] = uint16(c)
  dst[min(s.len, 63)] = 0
  return addr dst[0]

# --- SMBIOS Type 11 Injection ---
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

proc writeToVolume(root: ptr EfiFileProtocol, path: ptr uint16,
    data: string): bool =
  var f: ptr EfiFileProtocol = nil
  let openF = cast[FileOpenProc](root.open)
  if openF(root, addr f, path, FILE_MODE_RWC, 0) != EfiSuccess or f == nil:
    return false
  var sz = uint(data.len)
  let writeF = cast[FileWriteProc](f.write)
  discard writeF(f, addr sz, cast[pointer](cstring(data)))
  let closeF = cast[FileCloseProc](f.close)
  discard closeF(f)
  return true

# --- EFI Entry Point ---
proc EfiMain*(ImageHandle: EfiHandle,
    SystemTable: ptr EfiSystemTable): EfiStatus {.exportc, cdecl.} =
  sysTable = SystemTable
  let bs = SystemTable.bootServices
  let conOut = SystemTable.conOut

  discard conOut.clearScreen(conOut)
  conOut.print("Znu UKI Loader\r\n")

  let hp = cast[HandleProtocolProc](bs.handleProtocol)
  conOut.print(string(limineConfData))
  conOut.print("\r\n")
  # 1. Inject Limine config via SMBIOS Type 11
  let cfg = "limine:config:" & limineConfData
  if injectSmbios(SystemTable, cfg) == EfiSuccess:
    conOut.print("[1] SMBIOS OK\r\n")
  else:
    conOut.print("[1] SMBIOS FAIL\r\n")

  # 2. Get our own device handle from LoadedImageProtocol
  var selfImg: ptr EfiLoadedImageProtocol = nil
  var guidLIP = EfiLoadedImageProtocolGuid
  discard hp(ImageHandle, addr guidLIP, cast[ptr pointer](addr selfImg))
  if selfImg == nil:
    conOut.print("[2] LoadedImage FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess
  let ourDevice = selfImg.deviceHandle
  conOut.print("[2] Device handle OK\r\n")

  # 3. Open SimpleFileSystemProtocol on our device
  var sfsp: ptr EfiSimpleFileSystemProtocol = nil
  var guidSFSP = EfiSimpleFileSystemProtocolGuid
  discard hp(ourDevice, addr guidSFSP, cast[ptr pointer](addr sfsp))
  if sfsp == nil:
    conOut.print("[3] SFSP FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess
  conOut.print("[3] SFSP OK\r\n")

  # 4. Open root and write kernel + initramfs into /boot/
  var root: ptr EfiFileProtocol = nil
  let openVol = cast[OpenVolumeProc](sfsp.openVolume)
  discard openVol(sfsp, addr root)
  if root == nil:
    conOut.print("[4] Root open FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess

  # Write limine.conf to root — Limine scans for this file on the boot device
  if writeToVolume(root, toWide(wbuf1, "limine.conf"), limineConfData):
    conOut.print("[4] limine.conf OK\r\n")
  else:
    conOut.print("[4] limine.conf FAIL\r\n")

  # Create /boot directory and write kernel + initramfs
  var bootDir: ptr EfiFileProtocol = nil
  let openF = cast[FileOpenProc](root.open)
  discard openF(root, addr bootDir, toWide(wbuf1, "boot"),
                FILE_MODE_RWC, FILE_ATTR_DIR)
  if bootDir != nil:
    let closeF = cast[FileCloseProc](bootDir.close)
    discard closeF(bootDir)

  if writeToVolume(root, toWide(wbuf1, "boot\\kernel.bin"), kernelData):
    conOut.print("[4] kernel.bin OK\r\n")
  else:
    conOut.print("[4] kernel.bin FAIL\r\n")

  if writeToVolume(root, toWide(wbuf2, "boot\\initramfs.cpio"), initrdData):
    conOut.print("[4] initramfs.cpio OK\r\n")
  else:
    conOut.print("[4] initramfs.cpio FAIL\r\n")

  let closeF = cast[FileCloseProc](root.close)
  discard closeF(root)

  # 5. Load Limine BOOTX64.EFI from embedded buffer
  conOut.print("[5] Loading Limine...\r\n")
  var limineHandle: EfiHandle = nil
  let loadImg = cast[LoadImageProc](bs.loadImage)
  let loadSt = loadImg(false, ImageHandle, nil,
                        cast[pointer](cstring(limineEfiData)),
                        uint(limineEfiData.len), addr limineHandle)
  if loadSt != EfiSuccess or limineHandle == nil:
    conOut.print("[5] Limine load FAIL\r\n")
    discard bs.stall(bs, 5_000_000); return EfiSuccess

  # 6. Patch Limine's DeviceHandle → our FAT volume
  var limineImg: ptr EfiLoadedImageProtocol = nil
  discard hp(limineHandle, addr guidLIP, cast[ptr pointer](addr limineImg))
  if limineImg != nil:
    limineImg.deviceHandle = ourDevice
    conOut.print("[6] DeviceHandle patched\r\n")
  else:
    conOut.print("[6] Patch FAIL — starting anyway\r\n")

  # 7. Hand off to Limine
  conOut.print("[7] Starting Limine...\r\n")
  let startImg = cast[StartImageProc](bs.startImage)
  discard startImg(limineHandle, nil, nil)

  conOut.print("Limine returned unexpectedly\r\n")
  discard bs.stall(bs, 8_000_000)
  return EfiSuccess
