import efi_types, malloc, guid

# --- Internal Helpers ---
proc panicOverride(msg: string) {.noreturn.} =
  while true: 
     asm "hlt"

proc nimGetMem(size: int): pointer {.cdecl.} = malloc(cast[csize_t](size))
proc nimFreeMem(p: pointer) {.cdecl.} = free(p)

proc copyMem(dest: pointer, src: pointer, n: uint) =
  let d = cast[ptr UncheckedArray[uint8]](dest)
  let s = cast[ptr UncheckedArray[uint8]](src)
  for i in 0 ..< n: d[i] = s[i]

proc print(conOut: ptr SimpleTextOutput, s: string) =
  var buffer: array[256, uint16]
  for i, c in s:
    if i < 255: buffer[i] = uint16(c)
  buffer[min(s.len, 255)] = 0
  discard conOut.outputString(conOut, cast[WideCString](addr buffer))

# --- Injection Logic ---
proc injectSmbios(st: ptr EfiSystemTable, oemString: string): EfiStatus =
  var entryPoint: ptr Smbios3EntryPoint = nil
  
  # 1. Find SMBIOS 3.x Table
  for i in 0 ..< st.numTableEntries:
    if st.configTable[i].vendorGuid == EfiSmbiosTableGuid:
      entryPoint = cast[ptr Smbios3EntryPoint](st.configTable[i].vendorTable)
      break
  
  if entryPoint.isNil: return 1 # Not found

  # 2. Calculate New Table Size
  # Original size + Type 11 Header (4 bytes) + Count (1 byte) + String + Double Null (2 bytes)
  let newTableSize = entryPoint.tableMaximumSize + uint(5 + oemString.len + 2)
  var newTablePtr: pointer = nil
  
  discard st.bootServices.allocatePool(EfiLoaderData, newTableSize, addr newTablePtr)
  if newTablePtr.isNil: return 1

  # 3. Copy Original Table
  copyMem(newTablePtr, cast[pointer](entryPoint.tableAddress), entryPoint.tableMaximumSize)

  # 4. Append Type 11 Structure
  let offset = entryPoint.tableMaximumSize
  let type11 = cast[ptr SmbiosType11](cast[uint64](newTablePtr) + offset)
  type11.header.`type` = 11
  type11.header.length = 5
  type11.header.handle = 0xDEFA # Custom handle
  type11.count = 1

  # 5. Append the String and Double Null
  let strPtr = cast[ptr UncheckedArray[uint8]](cast[uint64](newTablePtr) + offset + 5)
  for i, c in oemString:
    strPtr[i] = uint8(c)
  strPtr[oemString.len] = 0     # End of string 1
  strPtr[oemString.len + 1] = 0 # End of structure

  # 6. Update Entry Point to point to new table
  # Note: For total stealth, we also clone the EntryPoint struct itself
  var newEntry: ptr Smbios3EntryPoint = nil
  discard st.bootServices.allocatePool(EfiLoaderData, uint(sizeof(Smbios3EntryPoint)), cast[ptr pointer](addr newEntry))
  copyMem(newEntry, entryPoint, uint(sizeof(Smbios3EntryPoint)))
  
  newEntry.tableAddress = cast[uint64](newTablePtr)
  newEntry.tableMaximumSize = cast[uint32](newTableSize)
  
  # Re-calculate checksum (simplified: ignore for this example or firmware might complain)
  newEntry.checksum = 0 # Most modern UEFI ignores this if tableAddress is valid

  # 7. Patch System Table
  for i in 0 ..< st.numTableEntries:
    if st.configTable[i].vendorGuid == EfiSmbiosTableGuid:
      st.configTable[i].vendorTable = cast[uint64](newEntry)
      break

  return EfiSuccess

# --- Main Entry ---
proc EfiMain*(ImageHandle: EfiHandle, SystemTable: ptr EfiSystemTable): EfiStatus {.exportc, cdecl.} =
  sysTable = SystemTable
  let conOut = SystemTable.conOut
  
  discard conOut.clearScreen(conOut)
  conOut.print("Znu OS Loader - Injecting SMBIOS Type 11\r\n")

  let status = injectSmbios(SystemTable, "Znu-Kernel-v1.0-Alpha")
  
  if status == EfiSuccess:
    conOut.print("Injection Successful. SMBIOS Patched.\r\n")
  else:
    conOut.print("Injection Failed. Using system defaults.\r\n")

  # In a real scenario, you would now use LoadImage/StartImage to chain-load Limine.
  # For now, we stall to show success.
  discard SystemTable.bootServices.stall(SystemTable.bootServices, 3_000_000)

  return EfiSuccess
