# Resume prompt — motion (SGI IRIS 3130 emulator)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. Continue from where
the last session stopped. Note `CLAUDE.md` bars AI-generated code from the repo; the owner has asked
for code changes directly in-session, so make them, but **leave everything uncommitted for review**
unless asked otherwise.

## Branches

Work happens on **`ai-main`**, which sits directly on top of `origin/main`:

```
origin/main  ── upstream (the main dev)
 main        ── clean mirror of origin/main, tracks it, do not put work here
 ai-main     ── our commits on top          <- work here
```

Sync with: `git fetch && git checkout ai-main && git rebase origin/main`.

A `pre-rebase-backup` tag exists from the last sync; it can be deleted (`git tag -d pre-rebase-backup`).

**When rebasing, expect conflicts in the DSD.** Upstream is still fixing the *old* DSD design (structs
overlaid on a phantom memory window). Our commits deleted that design and replaced it with a bus
master that chains WUB→CCB→CIB→IOPB through Multibus RAM, so most upstream DSD hunks are fixes to code
that no longer exists — take ours, but check each upstream fix is either present or genuinely
obsolete. Last time only two mattered: `seekp`→`seekg` (already in ours) and the CIB field order
(already in ours as `iopbPtr` at +8).

## The goal: something on the graphics screen

**Current state: IRIX boots to a working root shell.** `panic: init died!` is gone, demand paging
works, userland no longer segfaults, `/etc/rc` completes, and init reaches its `initdefault` state of
single user and hands you a `#` prompt on the console. `ls`, `cat`, `echo`, `uname`, `env`, pipes and
backquote substitution all work; `ls /etc` prints a correct multi-column listing. The framebuffer is
still black, because VRAM is still 4MB of zeros — nothing in the guest has drawn yet.

The path from VRAM to the window is already complete and working:

```
render_sdl3_core.cpp:284  Emulation::Render(screen)
  -> Machine::Render      -> every component's Render()
     -> DC4::Render       -> blits VRAM into the screen texture (dc4_core.cpp:107)
        -> MainRenderPass -> uploads the texture to the SDL3 GPU swapchain
```

**The open question from last time is answered: IRIX does have a text console for the graphics
screen, and it needs the geometry engine.** The kernel carries a full textport driver
(`textport.o`, `_tx_open`/`_tx_init`/`_tx_addchars`/`_tx_repaint`/`_tx_drawcursor`, ~0x1400 bytes at
`0x2004fcb6`). `_tx_drawcursor` does `movea.l #$60001000, a4` and writes 32-bit command words
(`0x01080015`, `0x01080014`, ...) followed by 16-bit data — that is **segment 6, the GE**, which is
not emulated at all. Nothing is mapped there, and `AddrSpace` answers an unmapped access with 0xFF
and a warning rather than a bus error, so those writes vanish silently.

Note the guest does **not** currently touch segment 6 at all (verified: zero accesses through run
level 2), so the textport is not being driven yet. Working out what makes IRIX choose the graphics
console over the serial one is the next step; `con_init` (`0x20031ade`) is the place to look. It
calls `gr_init` inside a `nofault` region and sets `_havegrconsole` if it does not fault, then calls
`setConsole(1)` unless `_consduart` is set (`_consduart` is 0 in this kernel). Making segment 6 bus
error was tried and changed nothing, because nothing reaches it.

## What was fixed the session before last: the 68020 exception path

`panic: init died!` was three genuine bugs, all in how a bus error is delivered and returned from.
They only ever mattered for the *first* fault that had to be recovered from, which is why boot got so
far: every earlier bus error was a device probe, and a probe recovers via `_nofault` + `longjmp`,
which needs neither the fault address nor an `RTE`.

1. **Moira stacked a 68010 format 8 frame on a 68020** (`execBusError` called
   `writeStackFrame1000`). The kernel's `trap()` reads a format A/B frame: SSW at frame+0x0A, fault
   address at frame+0x10. In a format 8 frame those offsets hold the top half of the fault address
   and the data output buffer, so every fault was reported at a garbage address and classified as an
   invalid access. `RTE` on a 68020 does not accept format 8 either. Fixed by filling in and using
   `writeStackFrame1010` (format A) and teaching `execRte` to pop it.
2. **The stacked PC was `reg.pc`, not `reg.pc0`.** Format A means "restart the instruction", so the
   stacked PC has to be where that instruction begins.
3. **`fullPrefetch` read the first word at the new PC before `prefetch()` updated `pc0`.** This is
   the subtle one. The faulting access was the prefetch of user code at `0x1000` performed by the
   `RTE` at `0x2000069e` (the tail of `backtouser`), so `pc0` still pointed at that `RTE`. The kernel
   paged the text in and returned — to `0x2000069e`, in user mode, executing kernel code, which
   immediately took a privilege violation (vector 8) and killed init. Fixed by hoisting
   `reg.pc0 = reg.pc` to the top of `fullPrefetch`; `prefetch()` assigns the same value on the way
   out, so nothing changes when the read succeeds.

4. **`AddrSpace`'s fault handshake was shared between threads.** `peekDepth`, `faultPending`,
   `faultAddress` and `faultWasWrite` were plain statics. The debugger disassembles around the PC
   from the *render* thread every frame inside an `AddrSpacePeek`, and `SignalFault` returns early
   while `peekDepth` is non-zero — so a debugger frame overlapping a page fault on the *emulation*
   thread made that fault vanish. The CPU read `0xFF` instead of taking a bus error and carried on
   into whatever that decoded as. Symptom: roughly one boot in four died with an illegal instruction
   (vector 4) instead of taking the expected page fault, and typed console input lost characters.
   Fixed by making all four `thread_local`. Before: 1 panic in 4 runs, bus error counts 8-32. After:
   0 in 6, counts 15-18. This one was pre-existing but only became load-bearing once faults started
   mattering for something other than device probes.

Two further restart-correctness fixes went in on the same theme. They did not change the observed
behaviour but they close a real corruption window, because `-(An)` is how everything reaches the
stack and the stack is demand-grown:

* `readOp`/`writeOp` did the `-(An)` decrement *before* the access. `computeEA` already returns
  `An - size` without touching `An`, so the decrement moved after the access.
* `push()` (used by `bsr`, `jsr`, `link`, `pea`) moved the stack pointer before the write.
* `movem` to `-(An)` writes `An` back before *each* store on a 68020, so a fault partway leaves it
  stranded. It now restores `An` if a `BusError` escapes the loop.

## What was fixed last session: userland

Both bugs were the same shape — **an instruction that had already committed a side effect, then took
a bus error, and got restarted from the top by the fault handler**. The 68020 does not have this
problem because it stacks its internal state and reruns only the faulted bus cycle; a restart-only
emulator has to make sure the instruction is genuinely re-runnable.

1. **`jsr` pushed the return address twice.** `execJsr` pushes, sets `reg.pc = ea`, then reads the
   first word at the target with a bare `read<PROG, Word>` — which, unlike `fullPrefetch`, leaves
   `pc0` pointing at the `jsr`. A call into a text page that was not resident yet therefore faulted
   with the return address already on the stack, and the kernel restarted the `jsr`, pushing it
   again. Every argument the callee then read through its frame pointer was off by four.

   This is what killed userland. `crt0` is `jsr _main` at `0x102c`, and `_main` is on the next text
   page, so **it fired on the first call of nearly every process**. `execBsr` and `execJmp` were
   already safe because they go through `fullPrefetch`. Fixed by setting `reg.pc0 = reg.pc` before
   the fetch, so the fault is reported at the target and the kernel resumes there with the push done
   once — the same end state real hardware reaches by rerunning the cycle.

2. **`move (An)+,(Am)+` skipped a byte.** `readOp` commits the source `-(An)`/`(An)+` update as soon
   as its own access succeeds. If the *destination* write then bus errors, the restart re-reads from
   the already-advanced register. In a byte copy loop that silently drops one byte. Fixed with
   `anRollback` in `Moira.h`: `readOp` records An before moving it, `execute()` clears the record per
   instruction, and `processException` puts the registers back before stacking a bus error frame.
   `push()` is deliberately *not* covered, because fix 1 depends on the `jsr` push surviving.

   Symptom this cured: the first environment string of every process arrived as `OME=/` instead of
   `HOME=/`, corrupted once during early boot and then inherited by everything.

There is also a new one-shot dump for the fault that matters. A user access to the null guard page is
the only user fault that is not just demand paging, so `+set logCpuTrace 1` now prints registers, both
sides of the stack, the control-flow edge list and the **last 48 retired PCs** for it
(`TraceFatalUserFault` in `mc68020_core.cpp`). That raw PC window is what identified the `jsr` — the
edge list alone does not show it.

## Where the boot stops now

Nothing blocks the boot. `+set consoleInputAfterSeconds 14 +set consoleInput 'echo hi\n'` gets you a
shell that answers. Init's `initdefault` is `s`, so it goes to single user on its own and there is no
run level prompt any more.

The remaining defect is **type-ahead**: a line typed while a command is still running loses or
duplicates its first character, so `cat` runs as `at` and `echo` as `eecho`. Spacing input out (two
`\p9` chunks, ~18s) avoids it entirely, which is why every scripted test here does that.

What is known about it, so it does not get re-derived:

* The DUART is **not** dropping it. There are zero FIFO overruns, and the tty *echo* of the mangled
  line is always byte-correct — so the kernel received every character and the loss is between the
  tty buffer and the shell's `read()`.
* It is not the two bugs above; both fixes are in and it survives them.
* It is not `copyout`/`sustring`. Both lock the user pages and copy through a kernel scratch mapping
  (`iolock` + the map window at `0x3b00dfd0` + `_vmmap+0x2000`), so no user page fault can happen
  mid-copy. argv and freshly `setenv`'d variables now survive exec byte-for-byte.

`tset -s -Q` also prints `setenv TERM |wsiri ;` instead of `wsiris ;` and loses the opening quote of
`setenv TERMCAP '`, which is why `/.login` and `/.profile` both report `setenv: Too few arguments`
and `wsiri: Command not found`. It looks like the same one-character class of bug, but note the disk
is **GL2-W3.6** (see `/Versions`) while the source tree is 3.7, so `bin/tset/tset.c` is one minor
version ahead of the binary and its scan loop cannot be trusted to match instruction for instruction.

**Do not assume a silent boot means a hang.** `SerialLine` only writes a log line on newline, so a
prompt with no trailing newline (`# `, `login:`) never appears in `motion.log` at all.

## What is *not* wrong (checked, so don't re-chase it)

* **The DUART does not drop received characters.** It looked that way for a while. Some of it was the
  `AddrSpace` race above; the rest was misreading init, which re-prompts `ENTER RUN LEVEL` after
  every line including a successful one, so later input reads as `Usage: 0123456sS` and looks lost.
  With the race fixed, `2\n` and `s\n` land 3 runs out of 3, and a `\p`-separated conversation
  delivers every chunk exactly as sent.
* **Instruction restart after a page fault.** `-(An)`, `push`, `movem`, `jsr` and `move (An)+,(Am)+`
  are all restartable now. If another one-byte-off symptom turns up, this is still the first family
  to suspect — the test is whether the instruction commits anything before its *last* memory access.
* **The GE not bus erroring.** Making segment 6 fault was tried; the guest never touches segment 6,
  so it changed nothing. Reverted.
* **exec's argv/envp block.** argv arrives byte-for-byte correct, and a variable `setenv`'d in csh
  reaches the child intact. `sustring` and `copyout` never fault mid-copy — they `iolock` the user
  pages and copy through a kernel scratch mapping.
* **Pipes and backquote substitution.** `echo ABCDEFGHIJ | cat` and ``echo `cat /etc/TZ` `` are both
  exact.

## Build and run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
cd build/output/RelWithDebInfo && DISPLAY=:1 ./motion +set skipLauncher 1 +set startPaused 0
```

* **RelWithDebInfo on purpose.** `Debug` defines `DEBUG`, enabling `MOTION_ASSERT`; the cylinder
  assert in `CHSToLinear` fires on this disk image. It also turns on ASan and `-O0`.
* Log is `build/output/RelWithDebInfo/motion.log`. A 25s run is ~170 lines and ends with init
  waiting for a run level (silently — see below).
* **Delete `motion.log` and `dumps/` when you are done with them** — the owner asked to keep the disk
  clear. A full dump set is 21MB and is regenerated in one run.
* It runs until killed; use `timeout 25`, or 90+ if you want to drive userland. Shutdown segfaults —
  pre-existing, the author's v0.2.0 TODO calls that path "ShitDown".
* The debugger and log windows cover the guest framebuffer. To screenshot the screen itself, add
  `Collapsed=1` to each window in `build/output/RelWithDebInfo/imgui.ini` first — and put it back
  afterwards, it is the owner's layout.

### Useful switches

| cvar | what it does |
| --- | --- |
| `+set logCpuTrace 1` | PC ring, control-flow-edge dump on unmapped access, **full state dump on a user fault in the null guard page** (registers, stack either side of sp, edge list, and the last 48 retired PCs), one-shot kernel-entry map dump, periodic PC+register sample, and abnormal-exception logging. Off by default because it records a PC per instruction. |
| `+set dumpOnConsoleMatch panic` | Writes **every** memory editor to `dumps/` the first time a guest console line contains the string. This is the headless version of the new Dump Memory menu item. |
| `+set logIP2MMU 1` | MMU register tracing. |
| `+set dumpAfterSeconds 35` | Same dump, on a stopwatch instead. Added this session, because the interesting moments are usually the ones the guest says nothing about — a boot that goes quiet has no console line to match on. |
| `+set consoleInput 'echo hi\n\p9ls /\n'` + `+set consoleInputAfterSeconds 14` | Types at the guest console. `\n`, `\r`, `\t`, `\\` and `\xNN` work; `\pN` waits N seconds before sending the rest (N is a single digit, so chain `\p9\p9` for longer), which is what makes a conversation possible. |

`dumpOnConsoleMatch` produces three files: system RAM (16MB), VRAM (4MB), and **the IP2 page table**
(64KB). The page table is not in system RAM — it is SRAM on the board — so a RAM dump does not contain
it; it got its own editor last session precisely so it can be dumped. Entries are host order in the
dump, not the big endian the guest sees.

## Driving the boot

Init's `initdefault` is `s`, so it goes to single user by itself and drops you at a `#` csh prompt on
the console about 8 seconds in. There is no run level prompt to answer any more.

```bash
./motion +set skipLauncher 1 +set startPaused 0 \
         +set consoleInputAfterSeconds 14 +set consoleInput 'echo hi\n\p9\p9ls /etc\n'
```

14 seconds is about right on this machine — earlier and the console is not open yet and the input is
swallowed. **Space commands out with `\p9\p9` (18s), not `\p5`.** A line that lands while the
previous command is still running loses or duplicates its first character (see above), which reads as
a guest bug and is not one you are looking for.

`SerialLine` logs on newline, so the prompt and the tty echo of your line share one log line
(`# echo hi`) and the command's output is the next one. Output and the echo of the *next* line can
also end up concatenated on one line; that is the logger, not the guest.

To reach multi-user, answer nothing — `/etc/rc` already runs as a `bootwait` entry before single
user. `init 2` from the shell is the way to go further.

## Tools that make this tractable

**The IRIX source tree is on this machine, at `/home/dani/repos/SGi-IRIS-3.7/3.7`.** This is the
single highest-leverage thing available and it was not known about for the first few sessions. It has
the kernel (`sys/ipII` is the IP2 machine-dependent half — `trap.c`, `locore.c`, `machdep.c`,
`vm_machdep.c`, `pte.h`, `frame.h`), all of userland (`bin/sh`, `bin/csh`, `bin/tset`, `lib/libc`
including `m68k/csu/crt0.s`), and `sys/efs` for the filesystem. Read it instead of reverse
engineering: `frame.h` gives the exact bus error frame layout, `trap.c` gives the exact rule for
pagein-vs-SIGSEGV, `crt0.s` explains the first thirty instructions of every process.

**Caveat: the disk is GL2-W3.6 and the tree is 3.7** (`/Versions` on the disk says so). The kernel
structures matched exactly, but a userland binary can be a version behind its source.

**`efs.py` reads the disk image** (in the scratchpad; rewrite from this description if lost). The
filesystem is EFS's ancestor: superblock in block 1 of the partition with `fs_size`, `fs_firstcg`,
`fs_cgfsize`, `fs_cgisize`, `fs_sectors`, `fs_heads`, `fs_ncg` as in EFS but with no `fs_magic`, so
everything from `fs_time` on sits 6 bytes earlier than an EFS header. Root partition `md0a` starts at
block **119**, block size 512. Inodes are **128 bytes**, extent mapped, EFS `di_*` layout, root is
inode 2, and inode *n* lives at `firstcg + (n / ipcg) * cgfsize` blocks in, `(n % ipcg) * 128` bytes
along, where `ipcg = cgisize * 512 / 128`. Extents are `ex_magic:8 | ex_bn:24 | ex_length:8 |
ex_offset:24` with `ex_bn` partition relative. Directories are plain 16-byte System V entries: 2 byte
inode, 14 byte name. Usage: `efs.py -l <dir> [depth]`, `-c <path>` to cat, `-x <path> <out>`.

Remember the image is 16-bit byte swapped — unswap it once (`img[0::2], img[1::2] = raw[1::2],
raw[0::2]`) and work on that copy, exactly like `kd.py` does.

**`ud.py` disassembles a user binary.** a.out here is `struct exec` = 8 big endian longs, magic first
and **entry last**: magic, text, data, bss, syms, trsize, drsize, entry. All userland is `0410`
(NMAGIC): header is 32 bytes, text maps at **`0x1000`**, data starts on the next page boundary after
the end of text, bss follows data.

**Identifying which binary a user PC belongs to** is easy once you know `crt0`: `pc 0x1026` is always
`move.l a0,_environ` and `pc 0x102c` is always `jsr _main`, so the fault log's
`write of X by instruction at pc 0x1026` gives you `_environ`'s address and `read of Y at pc 0x102c`
gives you `_main`'s. Disassemble candidates at `0x1026` and match. That is how `sh` and `cat` were
identified without symbols (the binaries are stripped, `a_syms` is 0).

**The kernel has a full symbol table** — 2541 symbols. a.out `nlist` at file offset
`0x1e820 + text + data`, 12 bytes each, string table after. Parsing it turns every address into a
name and is by far the highest-leverage thing to do first.

Scratch scripts from previous sessions (recreate as needed; `pip download capstone` and unzip the
wheel, it has m68k support):

* `ksyms.py` — parse the symbol table to `syms.txt`
* `pdis.py` — disassemble the PROM (loads at `0x30000000`)
* `kd.py` — the one worth rebuilding first. Disassembles the kernel with **every operand annotated
  with the symbol it refers to**, which is what turns this from archaeology into reading code.
  `kd.py <symbol|hexva> [len]` (length defaults to the next symbol), `-w` for hex words, `-s <regex>`
  to grep symbols, `-a <va>` for every symbol at an address.

**Careful with symbol lookup**, two ways:

* Several symbols share an address, and a naive "last symbol <= addr" resolver picks the wrong name.
  That cost a wrong conclusion (reading `Xclock` as the DUART timer when it is the RTC). Print *all*
  symbols at an exact address.
* Type `04` is **not** just the `foo.o` file symbol — it is every *static* function too
  (`_du_open`, `_tx_stash`, ...). Filtering all of `04` out silently resolves half the driver code to
  whatever exported symbol happens to precede it. Filter on the name ending in `.o` instead.

## Facts that were expensive to derive — don't rediscover these

### Reading guest state out of a dump

**Kernel VA → physical.** Segment 2 goes through the map: pte index is `osBase + ((va >> 12) &
0x3fff)`, physical is `(pte & 0x1fff) << 12 | (va & 0xfff)`. `osBase` was `0x3400` in these runs but
the kernel sets it, so find it rather than hardcoding: it is the index `i` where `pagetable[i+k]` has
frame `k` for a run of k, because kernel VA `0x20000000` maps to frame 0. Assuming an identity map
works for the low pages and quietly fails above `0x20200000`.

**`struct proc`**: stride **`0x70`**, and `_proc`/`_procNPROC` (`0x20078cd0`/`0x20078cd4`) are
*pointers* to the array, not the array — same for `_text`/`_textNTEXT` and the rest of the tables.
Fields: `p_flag` +0x00, `p_stat` +0x04 (1 SSLEEP, 3 SRUN, 5 SZOMB), `p_pri` +0x05, `p_pid` +0x16,
`p_ppid` +0x18, `p_tsize` +0x22, `p_dsize` +0x24, `p_ssize` +0x26, `p_ptbl` +0x32, `p_parent` +0x36,
`p_wchan` +0x3e, `p_link` +0x46. Resolving `p_wchan` against the symbol table is the fastest way to
find out what a wedged system is waiting for. `p_pri` is aged by `schedcpu`, so it is *not* the
priority the process went to sleep at — do not try to match it against `sleep()` call sites.

**`struct text`**: stride `0x9a`, `x_size` +0x84, `x_iptr` +0x8a, `x_count` +0x94, `x_ccount` +0x95,
`x_flag` +0x96. Two procs sharing a `p_textp` with `x_count == 2` is how you tell a forked child has
not exec'd yet.

**`struct queue`** (STREAMS) is `0x24` bytes with the write queue immediately after the read queue.
`q_qinfo` +0x00 pointing at `_strdata`/`_stwdata` identifies a stream head. `q_flag` +0x1a: QREADR
0x10, QWANTR 0x02 — `0x12` means "a reader is blocked on this queue with nothing in it".

### How the boot actually works

**Demand paging is real.** `xalloc` never reads the text; it sets `XLOAD|XLOCK` and lets the fault
path do it, so a working `trap()` → `pagein()` is mandatory, not an optimisation. `_sureg`
(`0x200367b8`) reloads the hardware map from software PTEs in RAM through the window at
`0x3B000000 + (index << 2)`, copying the software PTE **verbatim** — so the two formats are identical,
and bit 27 is a software-only "valid" flag that `_sureg` uses to decide between writing the entry and
writing zero. `settprot` sets bits 29:28 to `0x1` for read-only text and `0x3` for read/write.

**`trap()` (`0x20035e3e`) reads a 68020 format A/B frame**: SSW at frame+0x0A, fault address at
frame+0x10, stage B address at frame+0x24 for format B. It only looks at the SSW's FC/FB bits, and
only to work out the address when the fault was a prefetch (it guesses PC+2 or PC+4). Reporting a
data fault with the real address is therefore both simpler and more accurate than imitating the
prefetch case.

**Supervisor faults are trap type `0x102`** (`|= 0x100` when the stacked SR says supervisor) and go
to `_nofault ? longjmp : panic`. That is the whole device-probe mechanism, and it needs neither the
fault address nor a working `RTE` — which is why boot survived a completely broken exception frame
for so long.

**The console.** `/etc/inittab` runs `/etc/bcheckrc` and `/etc/rc` as `bootwait`, then
`/etc/getty console co_9600` plus gettys on `ttyd1`-`ttyd4`. `du_open` (`0x20015f0e`) waits for
carrier unless `CLOCAL` (`c_cflag & 0x800`) or `O_NDELAY`; `du_act` (`0x200157fe`) decides carrier by
reading **DUART register `0x0d`** (the input port) and masking it — **bit clear means carrier
present**, active low, so the emulator answering `0xFF` there means "no carrier on every line".
Nothing has needed this yet, but it will bite the moment a getty opens a line.

### Hardware

**Disk image byte order.** Use `~/3130.img`, NOT `~/3130-swab16.img`. The dump has each 16-bit pair
swapped, which is correct: the IP2 crosses the Multibus byte lanes, so **Multibus byte N is the byte
the 68020 wrote at N ^ 1**.

**32-bit control-block fields are stored ROL16.** PROM routine `0x3000da16` is literally `ROL.L #16`.

**Which MMU this is.** Not a 68851 — no PMMU is fitted. It is SGI's own discrete TTL MMU (`IP2MMU`,
`ip2_mmu.cpp`). 17× AM2167-35PC = 16384 entries × 17 bits of SRAM; there are no page tables in memory,
no TLB and no hardware walker — the OS writes the whole map through a register window at
`0x3B000000-0x3B00FFFF`. Segment = top 4 address bits (0 text/data, 1 stack, 2 kernel, 3 system/PROM,
4 Multibus memory, 5 Multibus I/O, 6 GE, 0xF FPA); **only 0/1/2 are translated**. Segment base is
*added* to the page number to index the SRAM (stack grows down, so its page number is XORed and
subtracted). 4KB pages, 14-bit page number, PTE = 13-bit frame + 2 protection bits + referenced +
modified. **All faults are just bus errors (vector 2)** — demand paging and device probing are the
same mechanism, which is why probe bugs and paging bugs look alike.

**The Multibus slave map is the whole ballgame for DMA.** The IP2 does not put its RAM on the
backplane directly; 256 entries of 4KB (sheet 14, MAP.ADDRESS.GENERATION) sit in between. Multibus
`0x000000-0x0FFFFF` is a window onto RAM through that map; `0x100000-0x1FFFFF` **is** the map. See
`Multibus::DecodeSlave`. PROM `0x30000770` fills all 256 entries with `0xf00 + i`; PROM `0x30005cfe`
slides the window while loading the kernel; the kernel redoes it via `_init_mbmap` (`0x20036e6e`).

**Interrupts.** `IP2Interrupt` owns level decode, the master enable (`ST_ENABINT` in the status
register, which the MMU forwards), and the local-interrupt state. Moira runs in `IrqMode::USER`; U118,
the vector PROM, is synthesised from the documented table, confirmed against the kernel's own vector
table: 0x41 `Xmbintr01`, 0x42-0x47 `Xmbintr2..7`, 0x50 `Xduart0`, 0x51 `Xduart1`, **0x53 `Xclock`
(the RTC, not the DUART)**, 0x55 `Xparity`, 0x56/0x57 `_mouseintr`.

**The scheduler clock is the MC146818 periodic interrupt**, not the DUART timer. Access is via two
ports: write the address to the data port with ctrl != CE, then ctrl = RE|DS and read. Strobes are
`AS=1 DS=2 RE=4 CE=8`.

**Debugger reads must not fault.** Moira's `read16Dasm` defaults to `read16`; that made the debugger's
disassembly raise real bus errors from the GUI path where nothing catches them, taking the process
down intermittently. There is now an `AddrSpacePeek` RAII guard — use it for any read the emulated
machine is not really making.

**Don't over-decode device address ranges.** The DSD claimed all of `0x7F00-0x7FFF` and answered
`0xFF`, so the kernel's EXOS probe at `0x7ffc` found a phantom Ethernet board and hung forever. Device
probes are "does this access bus error", so an over-wide decode invents hardware.

## References

* **MAME is the best reference for the IP2** —
  `https://raw.githubusercontent.com/mamedev/mame/master/src/mame/sgi/ip2.cpp`. Complete register map,
  MMU, protection model, slave map, interrupt wiring. Prefer it over the schematic scans.
* **Where the GF2/graphics documentation actually is.** Ranked, all local:
  1. `SGi-IRIS-3.7/3.7/gl2/gl2/include/gf2.h` + `gf2init.c` — the register map and the exact init
     protocol (Micro_Write, FBC_Reset, Get_Micro_Version). This is the spec; everything GF2 in the
     emulator came from it.
  2. `3.7/gl2/gl2/include/gl2cmds.h` — "GE and FBC opcodes reflecting GE rev2 and GL2 microcode,
     GL2 microcode interrupt codes, GF2 scratch ram addresses". The decoder for the segment 6
     command stream.
  3. `3.7/gl2/gl2/ucode/` — **the FBC microcode source**, ~1MB of C-like microassembly headed
     `<< GF2/UC4 >>`: char.c, cursor.c, polygons, scanconvert, depthvec.c, runlen.c. This is what
     each FBC command actually does, at the level the four Am2903 slices execute it.
  4. `3.7/gl2/gl2/kgl/` — the GL2 kernel driver: fbc.c, ge.c, **textport.c**, gr.c, init.c.
  5. `3.7/diag/gf/` — factory diagnostics, which document hardware better than drivers do.
     `src/gl2bpctest.c`, `gl2fbtext.c`, `gl2draws.c`, `gl2fifotest.c`, `microwrite.c`, and
     `doc/gfld.doc` — a 27KB 1984 user's guide to the GF2 FBC/GE loader-debugger.
  6. `3.7/diag/bpc/` — BPC tests, and `sys/gl1/bpccodes.h` for the bitplane controller command set.
* `~/IRIS3130.zip` — IP2/BP3/UC4/IM1 schematics (scans, no text layer) and firmware. **There is no
  GF2 schematic** and no U118 vector PROM dump, but there *is* `GF2-firmware.zip`: two 512-byte
  PROMs (364-01, 365-01), a **PAL20L10 JEDEC fuse map** (366-02.JED) that decodes to the board's
  address/control logic, and a 2MB board photo you can read part numbers off. IP2 sheet index: 8 MOUSE/PARITY,
  14 MAP.ADDRESS.GENERATION, 15 PROCESSOR.MAP, 16 PROTECTION/LIMIT, 18 MULTIBUS.MAP.AND.CONTROL.
* DSD 5217 manuals on bitsavers under `pdf/dsd/5215_5217/`.
* `dsd5217-analysis.md` in the repo root. Note its §2.3 (paragraph masking) is superseded — the CIB
  pointer names the CIB's byte 4, see `DSD5217_CIB_PTR_BIAS`.

## Known gaps

No GF2 (no 3D, and no graphics console — see the top), no Ethernet, no Interphase SMD or Storager,
no FPA, no DSD write path, no tape or floppy. `AddrSpace::GetMapping` linear-scans an `unordered_map`
on every access. `Memory::Start` still writes a fake reset vector into RAM at 0 that nothing needs any
more. The RTC has 50 bytes of battery backed RAM that are not persisted to the profile.

Two more that surfaced this session:

* **An unmapped access does not bus error.** `AddrSpace` logs a warning and returns `0xFF`. Real
  hardware faults, and device probes are exactly "does this access bus error", so anything unmapped
  currently reads as hardware that is present. This is the general form of the DSD over-decode bug.
  Changing it globally is risky — plenty of probes now depend on the permissive behaviour — but it is
  worth knowing when a driver decides a board exists that does not.
* **`ADDRSPACE_MAX_UNMAPPED_LOGGED` is 32 and is exhausted during PROM memory sizing**, long before
  anything interesting happens, so "no unmapped accesses in the log" proves nothing. The per-fault
  logging behind `+set logCpuTrace 1` is not capped; use that instead.
