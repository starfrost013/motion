# Resume prompt — motion (SGI IRIS 3130 emulator)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. Continue from where
the last session stopped.

`CLAUDE.md` bars AI-generated code from the repository, and that means **never push**. The owner does
want the code written, and commits on the local `ai-main2` branch are fine and expected - they are
what makes rebasing onto upstream possible at all. Every commit gets a `Co-Authored-By: Claude`
trailer so the provenance is obvious and the work is easy to keep out of upstream.

The single most useful thing to know: **the full IRIX 3.7 source tree is on this machine**, at
`/home/dani/repos/SGi-IRIS-3.7/3.7`. Read it instead of reverse engineering the guest. It has the
kernel (`sys/ipII` is the IP2-specific half), all of userland, and the complete GL2 graphics stack.
Caveat: the disk image is GL2-W3.6, one minor version behind the tree, so kernel structures match
exactly but a userland binary may not.

## Branches

Work happens on **`ai-main2`**, which sits directly on top of `origin/main`:

```
origin/main  ── upstream (starfrost013, the main dev)
 danifunker  ── the owner's own fork, has its own main
 main        ── clean mirror of origin/main, tracks it, do not put work here
 ai-main     ── the previous AI branch, superseded, kept for reference
 ai-main2    ── our commits on top of current origin/main   <- work here
```

Nothing has been pushed, and per `CLAUDE.md` nothing should be. Commits carry a
`Co-Authored-By: Claude` trailer so the AI-generated work is easy to identify and keep out of
upstream. `ai-main-backup-20260823` tags the last state of the old `ai-main` if anything needs
recovering.

**The harness now refuses `git add` and `git commit` outright**, so recent work sits in the working
tree rather than in a commit. Don't spend time trying to get around it - write the code, build it,
test it, and say plainly at the end that it is uncommitted so the owner can commit it themselves.
Leave a prepared commit message in `scratch/`.

**Keep scratch files in `scratch/` inside the repo, not in `/tmp`.** The owner asked for that so the
tooling survives between sessions; the directory is gitignored and `scratch/README.md` says what is
in it.

**The last rebase is worth reading before doing the next one.** Upstream's five commits were all one
Multibus paging refactor, and our second commit rewrote `multibus.cpp` almost entirely, so replaying
commit by commit meant resolving intermediate states that get overwritten. Re-fitting the *final*
state onto their base once was far cleaner. Only four source files ever collide - `multibus.cpp/hpp`
and `ip2_mmu.cpp/hpp` - everything else applies untouched.

Both sides independently implemented the Multibus slave map and independently made the same MMU fix
(dropping segment 4 from the page map). Ours is kept because it is a superset: it decodes the map
SRAM for reads, raises a bus error when nothing answers (device probes are exactly "does this access
bus error"), and masks master accesses to 24 bits for the 5217. Two upstream bugs are deliberately
not carried in - `Write32` dispatching to the slot's `Write16`, and a map write at `0x40100000` also
falling through into RAM. `multibus.hpp` carries a note mapping our bus-relative constant names onto
upstream's absolute ones.

## The goal: something on the graphics screen — reached

**Current state: with `+set enableGF2 1`, IRIX boots to a root shell *on the graphics screen*.** The
SGI banner, the device probe lines, the RESTRICTED RIGHTS LEGEND and a `#` prompt with a cursor all
render. With the board absent the machine still comes up on the serial line exactly as before.

The remaining visible wrongness on that screen is the known `tset` bug - `setenv: Too few arguments`
and `wsiri: Command not found` - which is the one-character class of bug described below and is not
new.

Userland works. `/etc/rc` completes, init reaches its `initdefault` of single user and hands you a
`#` prompt; `ls`, `cat`, `echo`, `uname`, `env`, pipes and backquote substitution are all correct.

The path from VRAM to the window is complete and working:

```
render_sdl3_core.cpp:284  Emulation::Render(screen)
  -> Machine::Render      -> every component's Render()
     -> DC4::Render       -> blits VRAM into the screen texture (dc4_core.cpp:107)
        -> MainRenderPass -> uploads the texture to the SDL3 GPU swapchain
```

VRAM is drawn into by the GF2 now: `gf2_geometry.cpp` writes pixels through `BP3`, and `DC4::Render`
blits them out. VRAM row 0 is the *bottom* of the screen, which is GL's origin, and `DC4::Render`
does the flip.

### How IRIX chooses the graphics console

`con_init` (`0x20031ade`) forces the console to serial, then tries the graphics path inside a
`setjmp`/`nofault` guard:

```c
setConsole(CONSOLE_ON_SERIAL);          /* fallback first */
if (setjmp(jb) == 0) {
        nofault = jb;
        gr_init();                      /* touches the graphics hardware */
        havegrconsole = 1;
        if (consduart == 0)
                setConsole(CONSOLE_ON_WIN);
}
```

Nothing is patched to prefer serial - `_consduart` is 0 and every GL symbol is present
(`_gr_init` `0x20048912`, `_gefind` `0x2004862c`, `_havegrconsole`). Without the board it falls back
because `gr_init` bus errors and the longjmp fires; with `+set enableGF2 1` `gr_init` now returns and
the console moves to the window.

**A consequence worth remembering: once the console moves, kernel `printf` goes to the textport and
the serial log goes quiet.** A panic in `gr_init` therefore prints to a screen that cannot draw. That
is exactly how `panic: something wrong with intpixel32!!` hid for a whole session - it was in
`motion.log` all along, in the middle rather than at the end, because the boot carries on afterwards.
Grep the log; don't just tail it.

### What the graphics hardware is

Four boards, "Enhanced IRIS Graphics" / GL2: **GF2, UC4, DC4, BP3**. All four now exist in
`src/component/gpu/juniper/`. GL1 (GF1/UC3/DC3/BP2) is the previous generation and is *not* the same
- see the warning in `gf2.hpp`.

GF2 itself, per the owner:

* 14 custom geometry engine chips, each four 32-bit ALUs with a microcode store and a config
  register selecting its function. Pipelined: the first converts IEEE 754 to 20.8 fixed point, four
  make a 4x4 matrix multiplier, six do clipping and z-buffering, two scale two coordinates each, and
  the last converts back to IEEE 754. `gefind()` probes twelve of them.
* The pipeline output feeds **four AMD Am2903 bitslice processors** in parallel for a 16-bit
  datapath - that is the FBC, doing flat and Gouraud shading. Those four slices are exactly why the
  microcode is `unsigned short ucode[][4]` and why `Micro_Write` walks `wd = 0..3`.
* The FBC reaches VRAM through the **BPC** (bitplane controller), which has its own command set.
  UC3/DC3 went through the BPC as well; **UC4 and DC4 do not**, so their side of VRAM can be modelled
  as plain writes.
* On the IP2 the pipe is not on the backplane: a private bus at segment 6 plus parts of Multibus I/O,
  at a fixed address.

## What was fixed four sessions ago: the 68020 exception path

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

## What was fixed three sessions ago: userland

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

## What was fixed two sessions ago: GF2 bring-up

`src/component/gpu/juniper/gf2/` is new. It is the board the CPU actually talks to, and its absence
is why `gr_init` bus errored on its very first register access.

**Written from the IRIX sources, not from MAME.** MAME's `sgi_gl1_device` is explicitly interim
("TODO: everything"), emulates GL1 rather than GL2, and its `fbc_data_r` returns a constant `0x40`,
which would spin IRIX's `fbc_reset()` forever - that loop only exits on `0xfff` or `0x7ff`. What MAME
*was* good for is the address decode: its GF1 map matches `gl1/gfdev.h` register for register, which
is what identified GF2's block as the same map at board decode `2<<12`. `gl2/gl2/include/gf2.h` then
confirmed it outright.

Implemented:

* the four registers - FBCpixel `0x50002000`, FBCflags `0x50002400`, FBCdata `0x50002800`,
  GEflags `0x50002C00`.
* **the microcode store**, which is the part that matters. `Micro_Write` pushes 4096 states four
  slices deep through a *window* based at FBCdata, then reads every word back and compares, so the
  microcode has to be retained or the verify fails with `micro write error`. Slice 3 is eight bits
  wide and its readback is masked to match. The state address is split: bits 0-8 come from the window
  offset `(state & 0x1ff) << 1`, bits 9-11 and the slice come from GEflags. **State 0 of each
  512-state block lands on FBCdata itself**, so the window must be inclusive of it and the mode, not
  the address, decides which meaning applies. That off-by-one was the first bug.
* FBCdata as a command port: written under a debug mode it poses a question, read back under
  `READOUTRUN` it answers. `fbc_reset()` asks 8 (scratch RAM size, wants `0xfff`) and 7 (microcode
  version, wants top byte `0x02`).
* the FBC programmed interrupt, active low, raised when work is submitted and cleared by `FBCclrint`
  (`FBCpixel = 1`). `gl_getplaneinfo` pushes commands then spins on it at `0x2004cd40`, so a constant
  answer wedges the boot.
* segment 6 mapped as the geometry pipe, currently **recording** the command stream rather than
  executing it.

Watch out: GL2's constants are not GL1's. `RUNMODE` is `0x31` here and `1` there, and GEflags grew to
16 bits to carry the microcode addressing.

## What was fixed last session: the graphics console draws

`gf2_commands.cpp` and `gf2_geometry.cpp` are new. **This work is in the working tree, not a commit**
- the harness refuses `git add`. A prepared commit message is at `scratch/commitmsg.txt`:
`git add -A src/ && git commit -F scratch/commitmsg.txt`.

The board was one register short of working, and the failure was hidden: `gr_init()` reached
`gl_getplaneinfo()` and panicked with `something wrong with intpixel32!!` **to the textport**, which
could not draw.

* **The command stream.** Words at segment 6 are the GE's instruction set, except those wrapped in a
  `GEpassthru`, which go to the FBC. Framing rules, all easy to get wrong:
  * `im_passcmd(n, cmd)` is one long, `((GEpassthru | ((n-1) << 8)) << 16) | cmd`, and **n counts the
    FBC opcode itself**.
  * `n = 0` encodes as **`0xFF08`** because `(0-1) << 8` wraps. It means lock the pipe (at `GEPORT`)
    or free it (at `LASTGE`), *not* a 256 word body. Reading it as one swallows everything after it.
  * **One body can hold several FBC commands** - `gl_getplaneinfo` sends `pixelsetup`, `readpixels`
    and `pixelsetup` as a single group of eight - so it needs a parameter count per opcode.
    `FBCloadram` carries its own count; `FBCloadmasks` and `FBCdrawchars` take the rest of the body.
  * `LASTGE` is `GEPORT - 0x800` in `short*` units, i.e. **`0x60000000`**, and marks the last word of
    a command. `textport.c` `#undef`s it back to `GEPORT`, so it is not load bearing there.
  * A long write appears as two 16-bit writes, high half first.
  * **Check for outstanding operands before checking for a passthru header.** A word only means an
    opcode when nothing is waiting for it. The textport draws a rectangle at x = 8, and 8's low byte
    is `GEpassthru`, so testing the other way round eats the rest of that rectangle and resynchronises
    onto the middle of a command. That was worth 20 dropped fills a boot and a scatter of "unknown
    FBC opcode" lines that looked like a missing opcode and were not.
* **The readback FIFO.** A programmed interrupt leaves an answer: read the interrupt code by spying
  on the output register under `READOUTRUN`, then in `RUNMODE` `FBCdata` reads the head of the FIFO
  and `FBCclrint` pops it - or every read pops if `AUTOCLEAR` is set in `GEflags`. The interrupt
  drops when the FIFO drains. It is raised only when there is a result, not for any submitted work.
* **The plane mask comes from BP3**, so `+set numBitplanes` changes what IRIX believes. With 32
  fitted it settles on twelve usable and sends `FBCwrten 0x0fff` - a free end-to-end check.
* **The transform.** Matrix, perspective divide, viewport. The matrix arrives **transposed** (the
  macro is `im_do_loadmatrixtrans`, the array `orthomattrans`), so a vertex is a column vector. The
  viewport is 20.8 fixed point built as `((right + left) + 1) << 7` - 8 for the fraction less 1 for
  the halving - so a 1024 wide screen gives centre and half size both 512.0. Those cancel against the
  ortho matrix, so the screen clear arrives as `(0.5, 0.5)` to `(1023.5, 767.5)`. **If a change makes
  the screen clear stop landing on exactly those numbers, the transform is wrong.**
* **Characters.** Four words of `struct fontchar` each; the glyph is one word per row in the font
  RAM, leftmost pixel in the top bit, **bottom row first** - the font is stored the way GL addresses
  the screen. `shiftfontbase()` has already made `offset` an absolute font RAM address. The textport
  sends the position as a `GEpoint` *one command after* `FBCcharposnabs`, so it arrives transformed.

**The polarity fix that mattered as much as the rest:** `TOKEN_BIT_` has to read **clear**. The
trailing underscore says active low, but `gf2.h` defines `PIPEISBUSY` as `(FBCflags & TOKEN_BIT_)`
with no inversion, so a set bit means busy - and `tx_repaint()` gives up and reschedules whenever the
pipe is busy. Reporting it set meant the textport silently never drew at all.

Three pre-existing bugs in the neighbours only became reachable once the stack came up far enough to
use them:

* **BP3 had no bounds check on any VRAM access**, and UC4's fill and character draw walk x and y
  straight out of guest registers. An off-screen rectangle wrote into the host heap. Now guarded.
* `BP3::Write16` indexed the word array with `addr >> 2` instead of `addr >> 1`.
* `DC4::Render` started its row loop at `SIZE_Y` rather than `SIZE_Y - 1` and advanced the VRAM
  address before the read instead of after.
* VRAM was allocated unzeroed, and drawing keeps the planes the write mask leaves clear.

### A trap worth knowing about: a missing `return` is a wild pointer

`FBCcolor` and `FBCwrten` were written with `break` instead of `return`, so `ExecuteFBCCommand` fell
off the end without returning a value. At `-O2` GCC assumes that is unreachable, so the caller got
whatever was in the register as the command length, walked `offset` off into the 78KB GF2 object and
read garbage as commands. It surfaced as a `free()` on a garbage pointer inside a `std::string`
destructor that the build had optimised away entirely, on a line that could not execute. **ASan did
not catch it** - the reads stayed inside the object's own allocation.

What actually found it: compiling `gf2_commands.cpp` with `#pragma GCC optimize("O0")`, which turned
the SEGV into a clean SIGILL, and then `x/4i $pc` in gdb showing `ud2`. If a crash in this emulator
points at a line that cannot run, stop trusting the backtrace and go looking for UB.

## What was fixed last session, part two: the keyboard

The graphics console can be typed at now. Five separate things were wrong, and the IRIX side of each
is in `gl2/gl2/kgl/keyboard.c` and `gl2/gl2/include/kb.h`.

1. **ImGui was eating every keystroke.** The SDL pump gated guest input on `io->WantCaptureKeyboard`,
   which is true for as long as ImGui has keyboard *navigation* active - and
   `ImGuiConfigFlags_NavEnableKeyboard` is set, so that is essentially always. It now gates on
   `io->WantTextInput`, which is the narrower question: is the user typing into a debugger text
   field? This was the whole reason the keyboard looked dead; everything below it was invisible
   until this was fixed.
2. **No break codes.** `KB_SCANCTOUPDN(c)` is `(!((c) & 0x80))`, so **bit 7 clear is a key going
   down and bit 7 set is it coming up**, and both halves have to be sent. `kb_translate()` keeps
   shift, control and caps lock purely from them. Only key-down was ever sent, so a modifier latched
   on forever - and every key whose modifier combination `kb_translate()` does not recognise is
   dropped by its `default: kb_ringbell(); return;`. That is what "the keyboard stops working until
   you press shift again" actually is. Caps lock never worked at all, because it toggles on the
   *up* stroke.
3. **The modifier bit was inverted and latched.** The old code set bit 7 on the shift key itself,
   toggling on each press - so pressing shift told IRIX shift had been *released*, and alternate
   presses left it stuck down. Deleted: shift and control are ordinary keys and IRIX derives the
   state from their own make and break codes.
4. **Unmapped keys sent a serial BREAK.** The lookup was `sdlToSgi[key]` on a `std::unordered_map`,
   and `operator[]` inserts and returns 0 for a key that is not there. Button 0 is the BREAK KEY.
   It uses `find()` now.
5. **The first keystroke after boot was swallowed.** "Who are you" is a two byte request, `0x00`
   then `0x10`, answered with one byte, `0xAA`. It gets asked twice - the PROM at power on and
   IRIX's own `kb_init()` - and the old code answered only the first. IRIX then took the user's
   first keystroke as the reply, which is why the console said `Unknown Keyboard - assuming IRIS
   3000 Keyboard`. If that line ever comes back, the handshake has broken again.

Host auto-repeat is deliberately **not** forwarded: `KeyDownEvent::repeat` was already being set and
ignored, and passing it on is what turns one deliberate keypress into two or three when the emulator
stalls long enough for repeats to queue behind it. The cost is that holding a key no longer repeats
at all. Doing it properly means the emulated keyboard generating its own typematic repeat, which is
what the real 8048 does.

There is also a `FocusLost` event now, fired from the SDL pump and handled by the keyboard, which
releases everything still held. Without it, alt-tabbing away mid-keystroke leaves the guest believing
a modifier is down - the same latch as (2), arriving by a different route.

`+set logKeyboard 1` traces every make and break with its scancode. `scratch/kbtest.sh` boots the
machine, types two commands at the graphics console with `xdotool` and dumps VRAM, which is the
whole loop in one command.

## What was fixed this session, part two: the mouse, and a 68020 bug

**The mouse works.** Moving the host pointer over the window moves the guest's: `__mousex` and
`__mousey` track it tick for tick and `gl_cursorx`/`gl_cursory` follow, clamped to the screen.
Buttons are wired. `ip2_mouse.cpp` is new and is a quadrature encoder - host movement piles up as a
signed backlog and is handed over one transition at a time, a fresh interrupt only being offered once
the guest has read the register that acknowledges the last one.

**Underneath it was a real CPU bug: level 7 was level sensitive and has to be edge triggered.** A
68020 recognises level 7 on the *transition* to it, not for as long as the pins are held there;
Moira fired it on every poll. The mouse latch is only cleared by the handler reading the register, so
`_mouseintr` re-entered itself before it could execute its first instruction and buried the stack
under interrupt frames. **The symptom is a flood of illegal instructions**, and the fix is a
`nmiTaken` latch in `Moira::checkForIrq` cleared by `setIPL` when the level comes off 7. Nothing
below level 7 changed - level 7 could not work at all before this. The parity error is on level 7
too and would have hit the same wall.

The hardware cursor is implemented as well - `FBCselectcursor`, `FBCdrawcursor`, `FBCundrawcursor`,
with the screen saved underneath and put back, and **only the planes in the cursor's own write mask**
saved, because mex puts its pointer in the overlay and does not undraw before repainting under it.
Whether the pointer's bitmap comes out right is not settled; see `resume-prompt-mex.md`.

Two further bugs turned up while checking whether the pointer looked right, and both had been on the
screen the whole time:

* **`FBCloadmasks` ignored the font RAM base.** `gl_fontslot()` gives every GL process a 256 word
  aligned slot and `FBCbaseaddress` says where it starts; mex gets 2048. Loading its glyphs at face
  value dropped them 2048 words low, which picked the wrong cursor bitmap **and landed mex's font on
  top of the kernel's** - every digit and every piece of punctuation on the console was garbage
  (`ib%` for `ib0`, `nswap=%773` for `nswap=17731`) and had been since the textport first drew.
* **DC4 looked the colour map up with the whole twelve bit pixel.** The RAM is sixteen banks of 256,
  and the index into a bank is only ever the bottom eight bits; the bank comes from DCflags in
  multimap mode and from the pixel's top four bits in single map. Indexing with all twelve reaches
  entries nothing writes, so **every colour index of 256 or more came out black** - including the
  cursor at 1024, which is why the pointer was drawn correctly and invisible.

**`+set logGF2 1` costs mex about a minute of startup.** With it on, mex reaches `_inchan` at around
150 seconds rather than 80, so a `dumpAfterSeconds 115` catches it mid-startup and `procs.py` reports
it SRUN. That is not a hang - give it `dumpAfterSeconds 185` and it is idle at `0x20028302`. This
session mistook it for a blocker twice. `+set logMouse 1` is cheap and carries the cursor trace.

## What was fixed this session, part one: the window manager draws

**`mex`, the GL2 window manager, runs.** It starts from the graphics console, completes its GL2
startup, draws a bordered window with a red `console` title bar and the textport inside it, and goes
to sleep on `_inchan` waiting for input. `scratch/mexshot.png` is that screen.
**`resume-prompt-mex.md` is the detailed handoff** - what follows is the summary.

Seven things were in the way, every one of them hardware the emulator did not have, and each
unblocked exactly one step. Nothing about single user mode was ever involved.

1. **`FBCeof` was never executed** - `FBCParamCount` had no entry for opcode 0x26, so it read as an
   unknown opcode and the parser abandoned the body before the `case` for it could run.
2. **The board had no interrupt.** `fbc_progintr()`, the only thing that decrements `EOFpending`, has
   two callers and both are interrupt handlers. GF2 now drives **Multibus line 3** - `ivectors[3]`,
   patched with `_fbc_intr` by `con_init()` the moment `gr_init()` returns - carrying the **vertical
   retrace** at 60Hz and the **FBC programmed interrupt**, both gated in GEflags and both active low.
   `+set gf2RetraceHz 0` turns the retrace off, which separates a fault in that path from one behind
   it.
3. **`FBCfeedback`**, the pipe's reverse channel. `gr_switchstate()` hands the hardware from the
   textport to mex, and `saveeverything()` reads the matrix stack and graphics position back *out* of
   the pipe to do it. `FBCEOF1` is `0x0108`, which is also a well formed two word passthru header, so
   the terminator has to be recognised before the parser tests for a passthru.
4. **`GEreconfigure` had no length** - a configuration byte per chip, ending at the first word whose
   high byte is `0xff`. Getting that wrong is not a missed command but a lost parser.
5. **`UCR_VERTICAL` never read set**, so `gl_domapcolors()` could never drain the colour queue and
   `mapcolor()` spun waiting for room. UC4 reports it permanently now: DC4 does not scan out, so
   there is no window during which a colour map write would tear. The cost is that `gsync()` never
   blocks.
6. **Matrix concatenation did nothing.** The opcodes `0x20`-`0x2f` build a 4x4 matrix a row at a time
   and multiply it onto the stack; mex positions a window's textport with a single `GEcompletemm3`
   carrying the window origin. Without it every fill landed at the screen origin - which is what the
   owner saw as stray lines across the screen. A row of the GE's row vector matrices is a **column**
   of the transposed ones this emulator keeps, and the collected matrix goes on the **right**.
7. **The fill rule dropped a row.** Vertices arrive at pixel centres, not area corners, so a span
   covers the closed interval at both ends - which the x loop already did and the row loop did not.
   Textport rows abut fifteen pixels apart, so it read as a rule under every line of text, and the
   screen clear had been leaving its top row unpainted all along.

Two smaller things went in on the way: the DC4 colour map **reads back** now (`getmcolor()` and
`blink()` need it), and the cursor handshake no longer fires twice per retrace - `fbc_progintr` flips
FBCflags into READOUTRUN and straight back out again in the middle of servicing an interrupt, and
both of those writes carry a HOSTFLAG the host is not touching.

**A trap worth knowing about:** `FBC command 0x08` appears four thousand times in a `logGF2` boot and
is not a bug. The kernel's `gl_WaitForEOF` (`kgl/kgl.c:452`) has no EOF interrupt to wait on and waits
by filling the pipe instead - `passes = 0x80008`, pushed 76 times, "pipe only holds about 140
goobies". It is named in `ExecuteFBCCommand` now so it stops reading as an unimplemented command.

## The GUI: where it lives on the disk

**There is a dedicated handoff for this: `resume-prompt-mex.md`.** It has how to run mex, what each
blocker was, the two behaviours that look like bugs and are not, and what is still missing. Read that
rather than working from the summary below if the GUI is the job.

**`mex`, the GL2 window manager, is already on the shipped disk** - `/usr/bin/mex`, 100622 bytes, on
`md0c`, along with `/usr/people/{demos,mexdemos,gifts,guest,tutorial}` and `/usr/lib/gl2/{fonts,
mexrc}`. Nothing needs installing. It was unreachable only because `/usr` never got mounted.

`scratch/hd/3130-gui.img` fixes that: one file patched, `/etc/rc.s0` gained a
`/etc/mount /dev/md0c /usr`, and the whole 2754 file userland is up at boot. Run it from
`scratch/rungui`. **`/etc/rc` is the wrong hook** - it only mounts at init level 2 or 3 and this
machine's `initdefault` is `s`, so it never reaches those lines; `/etc/rc.s0` is the `sysinit` entry
and runs every boot. There is no `/etc/fstab` at all, which is why `mount -avt` has nothing to do.

**mex runs.** `main()` forks: the parent `pause()`s until the child signals it and then exits, so the
shell prompt comes straight back and it looks like nothing happened. The child is the window manager,
and once it is up it is **SSLEEP on `_inchan`** - a healthy idle window manager waiting for input,
which it will keep doing until the mouse exists. `3.7/mex/main.c` is the source; the sequence is
`grioctl(GR_PUTINCHAN)` per channel, `ginit()`, `loadfont`, `softqreset()`, `grioctl(GR_MEWMAN)`.

Worth knowing: `/etc/rc` already contains a mex autostart, `su iris -c '... mex'` gated on `/.mexrc`
existing - but only under `case \`/bin/uname -t\` in 2300|2300T|3010)`, which a 3130 does not match.

### Where the media is, and what not to use on it

`~/repos/sgiresearch/iris3000` has the **GL2-W3.6 distribution tapes** (`gl2-w3.6+options.tar.gz`):
"Bootstrap System 05-10-89" holding bootstrap, Standard System root and Standard System usr, and
"Options 08-22-89" holding NFS, XNS, FORTRAN, Pascal and laser. They are **big-endian binary cpio** -
`cpio -it -H bin < file` warns about the reverse byte order and then reads them fine. Listings are
kept in `scratch/hd/tape-*.list`. The tape `/usr` is the same release as the disk `/usr`, so the
tapes add nothing unless a disk has to be built from scratch. That directory also has the BP3, DC4,
GF2, UC4 and keyboard firmware dumps, the IP2 PALs, `sgidemos.tar.Z` and `gcc_2.1_SGI3k.tar`.

**`~/repos/rusty-backup` cannot touch this filesystem.** Its SGI support is EFS v1 as IRIX 5.3-6.5
writes it: it requires `fs_magic` (0x00072959 / 0x0007295A) at superblock offset 28 and rejects
anything else. IRIX 3.7 is EFS's *ancestor* and has **no magic at all** - `fs_ncg`, `fs_dirty`,
`fs_time`, then straight into `fs_fname` - so the header is 6 bytes shorter and everything from
`fs_time` on is displaced. `scratch/efswrite.py` is the tool that understands this one; it rewrites a
file in place when the new contents still fit the blocks already allocated, which is two edits (the
data blocks and `di_size`) and needs no allocator.

**The partition table is at image block 0**, not an SGI `dvh` - magic `0x00072959`, the drive name
("Priam V170") at 0x5c, and partition entries of `{first(4), size(4)}` starting at **offset 0x1a**:
md0a root at block **119** (17850), md0b swap at 17969 (17731), md0c `/usr` at **35700** (79730).

## Where the boot stops now

**Serial console (default, `enableGF2` off): nothing blocks it.** Init's `initdefault` is `s`, so it
reaches a single user root shell on its own in about 8 seconds. Root has no password.

**Graphics console (`+set enableGF2 1`): it works.** The board is found, the microcode loads and
verifies, `FBC_Reset` and the version check pass, `gefind()` runs its twelve-chip probe, the scratch
RAM tables and the font load, `gl_getplaneinfo` answers, the screen clears, and the textport draws
the whole boot to a `#` prompt.

It is still behind a cvar because the geometry side implements only what the textport uses.

### The next concrete step

**Find out what wakes mex.** It is up, it can be pointed at and clicked on, and it still sleeps on
`_inchan` - moving the mouse does not by itself queue an input event, because `DoQueueValuators` only
queues a valuator the process asked for with `GR_QDEVICE`. Whether mex is asking, and what it does
with the answer, is the thing that turns a window manager that draws into one that responds.

After that, in the order it will start to matter:

* **The screen mask.** `FBCmasklist` and `FBCloadviewport` do nothing, so drawing is not clipped to a
  window. With one window nothing lands outside it; with two overlapping ones it will.
* **Fill in the FBC command set.** Everything is framed and stepped over correctly, so each command
  is an isolated piece of work. `FBCdrawcursor`/`FBCundrawcursor`, `FBCbaseaddress` (the font RAM
  offset), `FBCconfig`, `FBClinewidth`, `FBClinestipple` and `FBCblockfill` are the ones the textport
  and the cursor actually use. The microcode source in `3.7/gl2/gl2/ucode/` says what each one does.
* **Real geometry.** Matrix concatenation and the viewport work now, but there is still no clipping
  and no z-buffer, which is what 3D needs.

### Reading the command stream

`+set logGF2 1` decodes the whole thing: every FBC command with its arguments, every GE command with
its operands in the format the opcode asked for, the viewport, and one line per polygon fill giving
its bounds, colour and write mask. That log is the fastest way to see what the guest is asking for.

`+set dumpAfterSeconds 30` writes VRAM out; a short Python script that reads
`dumps/dump_vrameditor_0000.bin` as host-order 32-bit pixels and prints `#` for colour 7 renders the
text as ASCII, which is a much better check than squinting at a screenshot. Colour 0 is the page,
7 is text, 2 is the cursor.

### The type-ahead bug (serial console)

**Still open, and still on the serial line only** - the graphics console keyboard is a different path
and does not show it. Worth re-testing now that the keyboard work is in, because "loses or duplicates
its first character" is the same shape as the swallowed-first-keystroke bug that turned out to be the
keyboard ID handshake, and nobody has checked whether the serial one has a similarly boring cause.

A line typed while a command is still running loses or duplicates its first character, so `cat` runs
as `at` and `echo` as `eecho`. Spacing input out (two `\p9` chunks, ~18s) avoids it entirely, which is
why every scripted test here does that. Known:

* The DUART is **not** dropping it. Zero FIFO overruns, and the tty *echo* of the mangled line is
  always byte-correct, so the kernel received every character and the loss is between the tty buffer
  and the shell's `read()`.
* It is not `copyout`/`sustring`. Both lock the user pages and copy through a kernel scratch mapping
  (`iolock` + the map window at `0x3b00dfd0` + `_vmmap+0x2000`), so no user page fault can happen
  mid-copy. argv and freshly `setenv`'d variables survive exec byte-for-byte.

`tset -s -Q` also prints `setenv TERM |wsiri ;` instead of `wsiris ;` and loses the opening quote of
`setenv TERMCAP '`, which is why `/.login` and `/.profile` both report `setenv: Too few arguments`.
It looks like the same one-character class of bug, but the disk is **GL2-W3.6** (see `/Versions`)
while the source tree is 3.7, so `bin/tset/tset.c` is one minor version ahead of the binary.

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
  so it changed nothing. Reverted. (It touches it constantly now that the board is fitted.)
* **The logger being thread unsafe.** It was the obvious suspect for a heap corruption and it is not
  the cause - serialising `Logger::Log` with a mutex changed nothing, and so did disabling the log
  window's post-log hook. Both were tried and backed out. See the missing-`return` note above for
  what it actually was.
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
* **Testing while the owner has an instance open**: both write `motion.log` into the working
  directory, so run from **`scratch/run`**, which already has `assets`, `roms`, `profile` and
  `motion` symlinked relatively out of the build directory, rather than killing theirs. Its
  `imgui.ini` also has the debugger and log windows collapsed, so the guest screen is visible.
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
| `+set enableGF2 1` | Fit the GF2 board. **Off by default** - with it on IRIX takes the graphics console, which now draws but cannot be typed at, so the serial line stops carrying the boot. On for graphics work, off when you want to drive the machine. |
| `+set logGF2 1` | GF2 tracing: every FBC command with its arguments, every GE command with its operands decoded per the opcode's format, the viewport, and one line per polygon fill with its bounds, colour and write mask. |
| `+set gf2RetraceHz 60` | The GF2 vertical retrace rate, on Multibus IRQ 3. **0 turns the interrupt off**, which is the quickest way to separate a fault in the retrace path from one behind it. |
| `+set logMouse 1` | Every host movement with the backlog it joined, every fiftieth tick the guest acknowledges, and every cursor draw and undraw with the glyph's sixteen words. Cheap - unlike `logGF2`, it does not slow the machine down. |
| `+set logKeyboard 1` | Every make and break code the emulated keyboard sends, with its scancode. |
| `+set numBitplanes 8` | How many bitplanes BP3 fits. The GF2 pixel readback answers `gl_getplaneinfo` from this, so IRIX believes it. Default 32, which IRIX caps to twelve usable. |
| `+set dumpAfterSeconds 35` | Same dump, on a stopwatch instead. Useful because the interesting moments are usually the ones the guest says nothing about — a boot that goes quiet has no console line to match on. |
| `+set consoleInput 'echo hi\n\p9ls /\n'` + `+set consoleInputAfterSeconds 14` | Types at the guest console. `\n`, `\r`, `\t`, `\\` and `\xNN` work; `\pN` waits N seconds before sending the rest (N is a single digit, so chain `\p9\p9` for longer), which is what makes a conversation possible. |

`dumpOnConsoleMatch` produces three files: system RAM (16MB), VRAM (4MB), and **the IP2 page table**
(64KB). The page table is not in system RAM — it is SRAM on the board — so a RAM dump does not contain
it; it has its own editor precisely so it can be dumped. Entries are host order in the
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

**These already exist in `scratch/`** — that directory is gitignored and kept in the tree precisely so
they survive between sessions. `scratch/README.md` lists them. The two that matter:

* `kd.py` — disassembles the kernel with **every operand annotated with the symbol it refers to**,
  which is what turns this from archaeology into reading code. `kd.py <symbol|hexva> [len]` (length
  defaults to the next symbol), `-w` for hex words, `-s <regex>` to grep symbols, `-a <va>` for every
  symbol at an address. `scratch/cs/` is the capstone it needs (m68k support); if it goes missing,
  `pip download capstone` and unzip the wheel.
* `syms.txt` — the parsed symbol table.

Not currently present, rebuild if wanted:

* `ksyms.py` — parse the symbol table to `syms.txt`
* `pdis.py` — disassemble the PROM (loads at `0x30000000`)

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

GF2 draws the textport, mex's window frame and the hardware cursor, and there is a pointer on the
screen that follows the mouse. The geometry side carries out load/push/pop matrix,
**matrix concatenation**, viewport, reconfigure, point and filled polygons; there is no clipping and
no z-buffer, so there is no 3D. The FBC command set is framed and stepped over correctly but only
`loadram`, `loadmasks`, `readpixels`, `color`, `wrten`, `charposnabs`, `drawchars`, `eof`, `feedback`,
`readcharposn`, `baseaddress`, `selectcursor`, `drawcursor` and `undrawcursor` do anything - in
particular **`masklist` does nothing, so drawing is not clipped to a window**. There is no BPC.

**`FBCbaseaddress` is applied to `FBCloadmasks` and to nothing else.** The FBC adds it to what it is
given; the cursor's address is the exception, and the microcode says so - "NOTE --- cursor font ram
adr is absolute!" - because the kernel has already added the base itself. Character descriptors are
absolute too, `shiftfontbase()` having adjusted them in software. If text or a glyph ever lands
somewhere unexpected, this is the first thing to re-check. The graphics console can be typed at now, but `consoleInput` still drives the
*serial* line - there is no scripted-input path to the keyboard, which is why `scratch/kbtest.sh`
drives it with `xdotool` instead. The mouse is wired up now; what is not is any way to drive it from a script, so every mouse test
here goes through `xdotool`.

No Ethernet, no Interphase SMD or Storager, no FPA, no DSD write path, no tape or floppy. **The
guest cannot write to the disk at all**, so anything that has to persist is patched into the image
from the host with `scratch/efswrite.py`. Installation media does exist after all - the GL2-W3.6
tapes in `~/repos/sgiresearch/iris3000` - but nothing needs installing, because `/usr` on `md0c` is
the same release and already complete. See the GUI section above.

`AddrSpace::GetMapping` linear-scans an `unordered_map` on every access. `Memory::Start` still writes
a fake reset vector into RAM at 0 that nothing needs any more. The RTC has 50 bytes of battery backed
RAM that are not persisted to the profile.

* **An unmapped access still does not bus error in `AddrSpace`** - it logs a warning and returns
  `0xFF`. The Multibus *is* now strict (`Multibus::DecodeSlave` returning `None` signals a fault),
  which is what makes device probes come out right, but the general case elsewhere is unchanged.
* **`ADDRSPACE_MAX_UNMAPPED_LOGGED` is 32 and is exhausted during PROM memory sizing**, long before
  anything interesting happens, so "no unmapped accesses in the log" proves nothing. The per-fault
  logging behind `+set logCpuTrace 1` is not capped; use that instead. `MULTIBUS_MAX_UNMAPPED_LOGGED`
  is 200 and is the one that catches graphics probes.
* **Getty will block on carrier if you ever reach multi-user.** `co_9600` in `/etc/gettydefs` is
  `B9600 SANE TAB3` with no `CLOCAL`, so `du_open` waits for carrier, and `du_act` decides it from
  DUART register `0x0D` where **a clear bit means carrier present** (`IPORT_DCDA 0x08` for channel A,
  `IPORT_DCDB 0x04` for B). `ip2_duart.cpp` returns `0xFF` there, so every line reads "no carrier".
  `init 2` will therefore give you no `login:` until that is fixed.
