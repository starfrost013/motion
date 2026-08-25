# DSD 5217: why only the first sector reaches main memory

Analysis of `src/component/storage/dsd5217.cpp` at commit `77f9d74` ("dsd sees valid dba").

> **Status:** section 5 has since been implemented in the working tree — `dsd5217.cpp` /
> `dsd5217.hpp` are rewritten along these lines and compile clean, but have not been run
> (no disk image available in this environment). Sections 1-4 are the diagnosis and stand
> on their own.

**Symptom:** the emulated controller reads all of the kernel off the disk image, sometimes from
wrong addresses, but only the very first sector ever lands in main memory — even though the
transfer loop in `ReadSector()` does not terminate early.

**Root cause:** the DSD permanently decodes a stale 256-byte Multibus window at `0x1000`-`0x10FF`,
which sits in the middle of the PROM's own DMA buffer. Single-sector transfers stay below it;
multi-sector transfers run through it, and every write that lands inside is swallowed by the
controller instead of reaching RAM — and lands on top of the live IOPB.

---

## 1. Method

The manuals alone can't settle this, because SGI's driver doesn't follow them literally. So the
starting point was a disassembly of `roms/iris3130/ip2/ip2_prom_3.0.10.bin` (capstone m68k,
`CS_MODE_M68K_020`, loaded at `0x30000000`).

The DSD driver is the **`md`** device. Its device-switch entry is at `0x30011d78`:
`{ strategy = 0x3000c2d8, open = 0x3000c158, close = 0x3000c2c6, ioctl = 0x3000c2d6 }`. The driver
body sits at `0xc8e2`-`0xda16` and owns the strings `md: Busy timeout`, `md: mstatus Timeout`,
`md Error`, `mdopen:(`, `read dev:unit:phybno:size:addr ...`.

Relevant PROM entry points:

| Address | What it is |
|---|---|
| `0x3000c158` | `mdopen` — probes the I/O port, then calls the init below |
| `0x3000c2d8` | `md` devsw **strategy** — calls `mdstrategy` with the caller's buffer and byte count |
| `0x3000c8e2` | DSD init — builds WUB/CCB/CIB, issues reset/clear/start |
| `0x3000cb24` | wait for CCB busy == 0 (timeout 10,000,000 → `md: Busy timeout`) |
| `0x3000cb74` | wait for CIB status semaphore != 0 (→ `md: mstatus Timeout`) |
| `0x3000d2cc` | drive Initialize (function `00h`), fills the INIB |
| `0x3000d5c6` | issue-command-and-wait |
| `0x3000d6ae` | `mdstrategy(func, block, buf, dev, count)` — fills the IOPB |
| `0x3000d92a` | fixed 512-byte read into the bounce buffer, then `bcopy` |
| `0x3000da16` | `ROL.L #16` helper used on every 32-bit control-block field |

---

## 2. Layout facts confirmed against the PROM

These validate most of what the current code already does, and explain the parts that look like
hacks. Worth reading before changing anything.

### 2.1 Byte lanes

The IP2 crosses the 68020's D0-7 with the Multibus' D8-15, so **the byte the controller sees at
Multibus address `N` is the byte the 68020 wrote at CPU address `N ^ 1`.** The 5215 User Guide
states this in the note under figure 4-3 ("The MC68000 looks at the bytes in reverse").

The PROM proves it: `dsdinit` writes `move.b #$ff,(a3)` / `move.b #$1,$1(a3)` into the CCB, while
the manual says Multibus byte 0 = `01H` (channel control word) and byte 1 = BUSY1.

`DSD5217::Write16()`'s little-endian split already models this correctly for 16-bit fields.
`Write8()` does not swap, which is why the current structs have every documented byte pair
transposed. That is self-consistent as long as it's applied everywhere.

### 2.2 32-bit fields are stored half-swapped

`0x3000da16` is literally:

```
    d0 = arg << 16
    d1 = arg >> 16
    return d0 | d1          ; ROL.L #16
```

Every pointer and byte count is passed through it before being stored, so that the little-endian
8085 reads the correct value out of the byte-swapped lanes. `mdstrategy` does the same inline for
the RBC (`0x3000d878`) and the debug printf un-swaps ATC/RBC/DBA before printing (`0x3000ccb4`).

Combined with the `Write16` split, **the emulator already sees the correct `dba` and `rbc`.** The
byte-swap hack added to `Read8()` in `77f9d74` (dsd5217.cpp:81-86) is compensating for nothing, and
corrupts the value the host reads back.

### 2.3 Block pointers are paragraph-masked

Control-block pointers are rounded down to a 16-byte boundary. SGI depends on this: `dsdinit`
hands the controller `lea $4(a2),a0` as the CIB pointer, then writes

- opstatus at `a2+0`, status semaphore at `a2+2` (`clr.b $2(a4)`, `tst.b $2(a5)`)
- IOPB pointer at `a2+8` (`move.l d0, $8(a2)`)

Only with `& 0xFFFFF0` applied to the CIB pointer do all three land exactly where the manual puts
them (Multibus CIB+1 = operation status, CIB+3 = status semaphore, CIB+8..11 = IOPB address).

**So the manual's CIB layout is right and `iopbPtr` belongs at CIB+8, not CIB+4.** The current
struct works only by accident: `Write8()` computes its offset from the *unmasked* pointer
(dsd5217.cpp:212) while `Read8()` uses the masked one (dsd5217.cpp:108), and the two errors cancel.
The same mismatch means `clr.b (a4)` / `clr.b $2(a4)` currently write to `((uint8_t*)&cib)[-4]` and
`[-2]` — out of bounds, into the tail of `ccb`.

The data buffer address (DBA) is **not** masked.

### 2.4 Everything else in the current structs is correct

Verified byte-for-byte against the PROM:

- **IOPB** (Multibus offsets): `atc@4`, `device@8`, `unit@0xa`, `function@0xb`, `modifier@0xc`,
  `cylinder@0xe`, `head@0x10`, `sector@0x11`, `dba@0x12`, `rbc@0x16`, `generalPtr@0x1a`.
- **INIB**: `mdopen` programs `move.w #$7d0,(a4)` / `#$1,$3(a4)` / `#$11,$5(a4)` / `#$2,$7(a4)`
  → 2000 cylinders, 1 fixed head, 17 sectors/track, 512 bytes/sector.
- **CHS conversion**: `mdstrategy` computes `cyl = blk / (heads*spt)`, `sec = blk % spt`,
  `head = (blk % (heads*spt)) / spt`. `CHSToLinear()` inverts this exactly. It is correct.

### 2.5 Where the PROM puts its control blocks

`dsdinit` (`0x3000c8e2`):

```
    [0x210161c] = mbmalloc(0x1400)          ; 5 KB - the DMA / bounce buffer
    [0x210160c] = [0x210161c] + 0x400       ; control block base
```

| Block | Address |
|---|---|
| WUB | Multibus `0x7F000` (hardcoded, also read from RAM) |
| CIB | `base + 0x10` (after paragraph masking) |
| CCB | `base + 0x20` |
| IOPB | `base + 0x30` |
| INIB | `base + 0x50` |
| tape status buffer | `base + 0x90` |
| **DMA buffer** | **`base - 0x400`**, 5 KB total allocation |

---

## 3. The bug

`DSD5217::Start()` registers a **second, placeholder Multibus slot** at `0x1000`-`0x10FF`, intended
to be relocated once the host publishes the CCB pointer:

```cpp
// dsd5217.cpp:43-48
// bogus, will be overwritten
ccbMapping.memStart = 0x1000;
ccbMapping.memEnd = 0x10ff;
ccbMapping.id = DSD5217_MULTIBUS_SLOTNUM;
multibus->AddSlotMapping(ccbMapping);
```

```cpp
// dsd5217.cpp:174-178
// Now the CCB Pointer is specified. It's time to update our mapping.
// It's a reference so we can do this.
ccbMapping.memStart = wub.ccbPtr;
ccbMapping.memEnd = wub.ccbPtr + 0xFF;
```

It is **not** a reference. `Multibus::AddSlotMapping(SlotMapping& slot)` ends with
`slotMappings.push_back(slot)` (multibus.cpp:178) — a copy. The registered slot stays at
`0x1000`-`0x10FF` for the life of the process; the member being mutated is orphaned. (It also
misses the rebasing `AddSlotMapping` applies, so even as a reference it would be wrong by
`multibusMemoryStart`.)

Two consequences:

**(a)** The window only covers the control blocks by luck. Given §2.5, the blocks span
`base+0x10 .. base+0x9B`, so `base` must be ≈`0x1000` for anything to work at all — which in turn
pins the DMA buffer at `buf ≈ 0xC00`.

**(b)** That same 256-byte window is a hole punched **in the middle of the PROM's own DMA buffer**
(`0xC00 .. 0x1FFF`).

That produces exactly the observed split:

- **Volume header** — `0x3000d42c` → `0x3000d92a`, hard-coded `pea $200`: 512 bytes into `buf`
  (≈`0xC00`-`0xDFF`), entirely below the hole, so it reaches RAM and the subsequent `bcopy` works.
  **This is the "first sector, different code path".**
- **Kernel** — devsw `md` strategy `0x3000c2d8` → `mdstrategy` with the caller's `i_cc`: a
  multi-sector transfer starting at `buf` and running *up through* `0x1000`. Every `WriteMB16()`
  that lands in `0x1000`-`0x10FF` is routed by `Multibus::Write16` into `DSD5217::Write16` →
  `Write8`, never reaches RAM, and instead falls into the CCB/CIB/IOPB structure-write branches —
  writing **disk data on top of `iopb.dba` (IOPB+0x12), `iopb.rbc` (+0x16) and
  `iopb.actualTransfers` (+0x04)** while the transfer is still running.

Hence: the loop doesn't terminate early (it keeps iterating on a corrupted `actualTransfers` /
`rbc`), the data goes to a corrupted `dba` — "sometimes from wrong addresses" — and nothing past
the first sectors ends up where the host expects it.

---

## 4. Secondary defects

All real, all in the same path.

### 4.1 Programmed I/O port semantics (dsd5217.cpp:149-162)

```cpp
ccb.busy = (state != DSD5217_MBIO_STATUS_IS_READY);
```

5215 User Guide §4.6.1: only the **two least significant bits** are decoded —
`00h` = clear interrupt / remove reset, `01h` = start operation, `02h` = reset controller. The busy
flag in the CCB is set by the *host* and cleared by the *controller*; the port value must never
touch it.

The PROM writes `01` to start and then `00` to clear after **every** command (`0x3000d67a`,
`0x3000d68c`), so the trailing `00` currently latches `ccb.busy = 1`.

**This is currently masked by an off-by-one.** The CIB range check is
`addr <= (ccb.cibPtr & 0xFFFFF0) + sizeof(CIB)` — inclusive, one byte too many — so it overlaps the
CCB's first byte, and the host's busy read returns `((uint8_t*)&cib)[16]` (i.e. the first byte of
`iopb`, which is always 0). Fix either one of these alone and the machine will start printing
`md: Busy timeout`. They have to be fixed together.

### 4.2 Sticky iostream state (dsd5217.cpp:323-327)

```cpp
hdd->stream.seekp(diskLinear + iopb.actualTransfers, std::ios_base::beg);
hdd->stream.read((char*)sectorBuffer, bytesToReadThisSector);
if (hdd->stream.eof())
    stop = true;
```

`eofbit`/`failbit` are never cleared. The first short read latches them, after which every
`seekp`/`read` becomes a silent no-op — and `stop` is then true on the first iteration of every
later command, so each one copies exactly one sector of **stale buffer contents** and returns. On
its own this reproduces the reported symptom too.

Also: `seekp` is the put pointer; `seekg` is the correct one for reading (they happen to share a
position in `basic_filebuf`, but it's misleading).

### 4.3 Wrong per-iteration clamp (dsd5217.cpp:320-321)

```cpp
if (iopb.rbc < bytesToReadThisSector)
    bytesToReadThisSector = iopb.rbc;
```

Should be `iopb.rbc - iopb.actualTransfers`. As written, a non-sector-multiple RBC overruns the
host's buffer by up to a sector. 5215 §4.7.5: *"Only enough data to exhaust the count is moved to
the Multibus buffer."*

### 4.4 Unsigned underflow in the transfer loop (dsd5217.cpp:331)

```cpp
for (int32_t i = 0; i < bytesToReadThisSector - 1; i += 2)
```

`bytesToReadThisSector` is `uint32_t`, so the comparison is unsigned. If `bytesPerSector` is ever 0
(no valid Initialize yet), the bound becomes `0xFFFFFFFF` — roughly two billion iterations of
out-of-bounds reads from `sectorBuffer` scribbled across Multibus memory. Odd counts silently drop
the last byte, and once 4.3 is fixed a non-even residual count makes the outer loop spin forever.

### 4.5 `actualTransfers` is never written back

The manual marks IOPB+4 "returned at end of operation". The emulator updates its private copy only.

### 4.6 WUB CCB pointer truncated to 16 bits (dsd5217.cpp:166-180)

Only `0x7F002` and `0x7F003` are handled; bytes 4-5 of the 24-bit pointer fall through to
`default`. Works only while the CCB lives below 64 KB.

### 4.7 Vestigial DBA swap (dsd5217.cpp:81-86)

See §2.2 — unnecessary, and it corrupts host read-back of the DBA.

---

## 5. Proposed fix

**Stop snooping the control blocks; fetch them.** The WUB, CCB, CIB, IOPB and every data buffer are
ordinary Multibus RAM. The controller is a bus master that walks them itself — 5215 §4.6.5: *"the
controller goes to this address, fetches the WUB, and internally saves the CCB address"*. Modelling
them as a device window is what created the phantom hole in §3, and it's why the alignment hacks and
the read/write base mismatch exist at all. The header comment in `dsd5217.hpp` already says as much.

This is a smaller amount of code than what's there now, and it removes §3, §4.1, §4.6 and §4.7
outright.

1. **Register one slot, I/O only** (`0x50007F00`-`0x50007FFF`). Delete `ccbMapping`,
   `ReadBuffer`/`WriteBuffer`, `bufferType` and the hardcoded `iipbStart` window. Host accesses to
   the control blocks then fall through `Multibus::Read8`/`Write8` to real RAM, and no DMA write can
   be swallowed.

2. **Add byte-lane helpers** (§2.1):
   ```cpp
   uint8_t  MBRead8 (size_t m) { return multibus->ReadMB8 (m ^ 1); }
   void     MBWrite8(size_t m, uint8_t v) { multibus->WriteMB8(m ^ 1, v); }
   ```
   and build the 16/32-bit accessors from those, little-endian (the controller's view). Note
   `Memory::Read16`/`Read32` index `ram[addr>>1]` / `ram[addr>>2]` and silently truncate odd
   addresses, so never hand them an unaligned address.

3. **On a start command**, chain `WUB(0x7F000)` → CCB → CIB → IOPB, masking each *block* pointer
   with `0xFFFFF0` (not the DBA). Field offsets then follow the manual directly:
   - CCB: `ccw1@0, busy@1, cibPtr@2, ccw2@8, busy2@9, cpPtr@0xa, controlPtr@0xe`
   - CIB: `opStatus@1, commandSemaphore@2, statusSemaphore@3, iopbPtr@8`
   - IOPB: as listed in §2.4
   - INIB: `cylinders@0, fixedHeads@2, removableHeads@3, sectorsPerTrack@4, bytesPerSecLo@5,
     bytesPerSecHi@6, altCylinders@7`

   Keep the existing struct and field *names* so `dsd5217_debug.cpp` still compiles — they just
   become decoded copies rather than memory overlays, and the packing/transposition can go.

4. **Decode the I/O port on `value & 3`** (§4.1):
   - `02h` reset — drop the fetched tables, clear the IRQ
   - `00h` clear — clear the IRQ, leave reset; **do not touch busy**
   - `01h` start — the first one after a reset only chains the tables and clears CCB busy with no
     status returned; every subsequent one fetches the IOPB and executes it

5. **On completion**, write the operation status byte to CIB+1
   (`COMPLETE | ((unit & 3) << 4)`, plus `SUMMARY|HARD` on failure — the PROM checks
   `opStatus & 0xC0` at `0x3000d694`), set the status semaphore at CIB+3 to `0xFF`, clear busy at
   CCB+1, and raise the IRQ only when `!(modifier & 1)`. The PROM sets `modifier = 1` on every disk
   command (`0x3000d5fa`), so in practice it polls.

6. **Rewrite the `ReadSector` loop**: `clear()` the stream before each seek, use `seekg`, drive
   termination off `gcount()` rather than the sticky `eof()`, clamp to `rbc - transferred`, handle
   odd and zero counts, keep the existing `(buf[i+1] << 8) | buf[i]` + `WriteMB16` fast path when
   `dba + transferred` is even — that pairing is correct, it *is* the byte-lane swap — with a byte
   fallback otherwise, then write the transfer count back to IOPB+4.

Also worth doing:

- Change `Multibus::AddSlotMapping` to take the slot by value, or to return a handle/pointer to the
  stored entry. The current signature takes a non-const reference, mutates it, and then stores a
  copy — which is precisely the trap that caused this bug.
- Warn when a DMA address exceeds the emulated 1 MB Multibus window. `ReadMB*`/`WriteMB*` mask with
  `0xFFFFF`, but the 5217 is in 24-bit linear mode and can legally address 16 MB, so anything above
  the window currently wraps silently.

### Two things to be careful of

- **Don't enforce end-of-media from the INIB geometry.** The PROM programs a synthetic
  2000 x 1 x 17 x 512 = 17,408,000 bytes, which is almost certainly smaller than a real image.
  Use actual file EOF as end-of-media instead, or you'll truncate valid reads.
- **Keep unimplemented function codes returning "complete, no error"** as they do today. Returning
  a hard error for, say, a Seek would newly abort a boot that currently gets through.

---

## 6. Verification hints

The PROM prints these when verbose boot is enabled (switch register bit 5 clear):

- `md: Busy timeout` — CCB busy never cleared (§4.1)
- `md: mstatus Timeout` — CIB status semaphore never posted
- `read dev:unit:phybno:size:addr %d:0x%x:0x%x:0x%x:0x%x` — one line per successful read; `size`
  here is the RBC, so this tells you directly whether the failing transfers are multi-sector
- `%s: MAX retries` — the 5-attempt retry loop gave up

Logging `dba`, `rbc` and the computed block number per command, plus a warning whenever a DMA write
falls inside any registered Multibus slot, should make §3 visible immediately.
