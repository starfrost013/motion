# Resume prompt — the GL2 window manager (motion, SGI IRIS 3130)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

Read `resume-prompt.md` in the same directory first — it is the general handoff for this emulator and
this one assumes it. This one is only about the window manager.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. Read
`resume-prompt.md` for the general state of the project.

Every path in this document is relative to the repo root, not to `ai-docs/`.

**`mex`, the GL2 window manager, runs, the mouse works and there is a pointer on the screen.** mex starts from the graphics console,
completes its GL2 startup, draws a bordered window with a red `console` title bar and the textport
inside it, and goes to sleep on `_inchan` waiting for input. `scratch/mexshot.png` is what that looks
like. Moving the host pointer over the window moves the guest's: `__mousex` and `__mousey` track it
tick for tick and `gl_cursorx`/`gl_cursory` follow, clamped to the screen. Buttons are wired and
clicking does not upset it.

`CLAUDE.md` bars AI-generated code from the repository, and the harness refuses `git add` and
`git commit` anyway, so work stays in the working tree. Don't try to get around it; just say at the
end what is uncommitted. Scratch files go in `scratch/` inside the repo (gitignored), not `/tmp`.

**The IRIX 3.7 source tree is on this machine at `/home/dani/repos/SGi-IRIS-3.7/3.7`.** Read it
instead of guessing. For this task the directories that matter are `gl2/gl2/kgl/` (the kernel half of
the graphics driver), `gl2/gl2/common/` (the user-space GL library) and `mex/` (the window manager
itself, source included).

## Running it

`scratch/hd/3130-gui.img` is the stock disk with `/etc/rc.s0` patched to mount `/usr` at boot, so
`/usr/bin/mex` and `/usr/lib/gl2/{fonts,mexrc}` are all there without typing a mount. Run from
`scratch/rungui`.

```bash
cd scratch/rungui
./motion +set skipLauncher 1 +set startPaused 0 +set enableGF2 1
# wait ~40s for the # prompt, then type:  /usr/bin/mex
```

Three scripts do the whole loop in one command, all of them booting, typing `/usr/bin/mex` at the
graphics console with `xdotool`, and then differing only in what they collect:

| script | what it leaves you |
| --- | --- |
| `scratch/mexpc.sh` | a dump set and a log of CPU samples — the tool for a spin |
| `scratch/mexshot.sh` | `scratch/mexshot.xwd`, an X capture of the window |
| `scratch/mexlog.sh` | `motion.log` with `logGF2` on: the whole command stream |
| `scratch/mexmouse.sh` | mex plus a driven pointer and a click, with `logMouse` on |
| `scratch/mousequick.sh` | no mex, no graphics console needed: boot, wiggle, read the trace. 45 seconds, and the loop to use while working on the mouse itself |

`scratch/guestvars.py <dumpdir> <symbol>...` reads named kernel variables out of a dump set the same
way `procs.py` does, and prints `_havegrconsole` first as a check on the page table walk itself - if
that is not 0 or 1 every other number it gives you is noise.

**`logGF2` costs mex about a minute of startup, so give it a longer dump window.** It traces every
word in the pipe; with it on, mex reaches `_inchan` at around 150 seconds rather than 80, so a
`dumpAfterSeconds 115` catches it mid-startup and `procs.py` reports it SRUN. That is not a hang -
`dumpAfterSeconds 185` shows it idle at `0x20028302` having executed 920M instructions. Do not read
a spin in a `logGF2` run as a blocker without first giving it longer; this session called it one
twice. `logMouse` is cheap and carries the cursor trace, so prefer it when the pipe is not the
question.

**`xdotool search --name "motion 0.2.0" | tail -1` picks the *last* matching window**, so a stray
emulator left running from another session steals the typing and the run looks like mex never
started. Check `pgrep -af RelWithDebInfo/motion` before believing a null result.

**What mex does, so the behaviour is not misleading:** `main()` forks immediately. The parent
`pause()`s until the child signals it and then exits, so *the shell prompt comes straight back*. The
child is the window manager, and once it is up it is **SSLEEP on `_inchan`** — a healthy idle window
manager, not a hang. In `procs.py` it shows as a process whose parent is init, because the real
parent has exited.

## What was in the way, in the order it was hit

Every one of these was hardware the emulator did not have, and each unblocked exactly one step.
Nothing about single user mode was ever involved: mex is started by hand from the root shell and
does not care what run level it is.

**1. `FBCeof` was never executed.** `FBCParamCount` had no entry for opcode 0x26, so it read as an
unknown opcode and the parser abandoned the body before the `case` for it could run. mex sat at 100%
CPU in `gl_syncgraphics` (`gl2/common/services/userserv.c`) spinning on `sh->EOFpending`.

**2. The board had no interrupt.** With EOF raised the spin moved into the kernel, because
`fbc_progintr()` — the only thing that decrements `EOFpending` — has two callers and both are
interrupt handlers. The GF2 now drives **Multibus line 3**, which is where `ivectors[3]` lives, and
`con_init()` patches `_fbc_intr` in there the moment `gr_init()` returns (`sys/ipII/console.c`, and
the shipped kernel agrees — `kd.py 200314c2`). Two sources share the line, both gated in GEflags and
both active low: the **vertical retrace** at 60Hz, and the **FBC programmed interrupt**.

The retrace matters far beyond drawing, because `fbc_intr` services the programmed interrupt at its
tail as well. A board that never retraces looks exactly like a board whose FBC never finishes
anything.

**3. `FBCfeedback` — the pipe's reverse channel.** `gr_switchstate()` takes the hardware away from
the textport and gives it to mex, and to do that `saveeverything()` reads the matrix stack and the
graphics position back *out* of the pipe: `FBCfeedback` switches the FBC from drawing to capturing,
`GEstoremm` appends 33 words each, and three magic words end it and raise `_INTFEEDBACK`. The host
gets a token, a word count, the buffer, and the three markers still on the end.

`FBCEOF1` is `0x0108`, which is also a well formed two word passthru header, so the parser has to
recognise the sequence **before** it tests for a passthru.

**4. `GEreconfigure` had no length.** It carries a configuration byte per geometry chip and ends at
the first word whose high byte is `0xff`. Stepping over it wrongly is not a missed command but a lost
parser: one of the words it carries is `0x0021`, which reads as `GEmidmm1` and eats the eight after
it as a matrix row.

**5. `UCR_VERTICAL` never read set.** `gl_domapcolors()` runs inside the retrace interrupt and drains
the shared memory colour queue only while that bit is set, so the queue never emptied and
`mapcolor()` spun in user space waiting for room in its sixty four entries. UC4 now reports the bit
permanently: DC4 does not scan out, it blits VRAM into a texture once a frame, so there is no moment
at which a colour map write would tear. **The cost is that `gsync()` never blocks.**

**6. Matrix concatenation did nothing.** This was the last one and it is the one the owner spotted
from the screen rather than from a log. mex positions a window's textport with a single
`GEcompletemm3` carrying `(152, 84, 0, 1)` — `im_do_translates`, the window origin. Without it every
fill landed at the screen origin, so the console rows painted colour 0 across the left of the screen
and left a white strip where the window stuck out past them, plus a one pixel rule per text row.

The sixteen opcodes `0x20`-`0x2f` build a 4x4 matrix a row at a time: low two bits are the row, the
two above them say mid / first / last / complete. A row never sent, and a component past the end of a
short operand form, keeps its identity value. The GE's matrices multiply a row vector on the left and
this emulator's are the transpose of that, so **a row of theirs is a column of ours**, and the
collected matrix goes on the **right** of the one on the stack.

**7. The fill rule dropped a row.** The pipe hands over vertices at pixel centres, not at the corners
of an area, which is why the screen clear arrives as 0.5 to 1023.5 and not 0 to 1024. The x loop
already treated the interval as closed at both ends; the row loop used a crossing parity that finds
nothing on a scanline lying exactly along a horizontal edge. Textport rows abut fifteen pixels apart,
so it showed up as a rule under every line of text — and the screen clear was leaving its top row
unpainted the whole time.

## Two things that look like bugs and are not

* **`FBC command 0x08` four thousand times a boot.** That is the kernel's `gl_WaitForEOF`
  (`kgl/kgl.c:452`), which has no EOF interrupt to wait on and waits by filling the pipe instead:
  `passes = 0x80008`, pushed 76 times, "pipe only holds about 140 goobies". Each long is a one word
  passthru whose body is `GEpassthru` itself. It is named in `ExecuteFBCCommand` now so it stops
  reading as an unimplemented command.
* **The parent mex process disappearing.** That is the fork in `main()`, not a crash.

## The mouse, and the CPU bug underneath it

**8. The quadrature encoder.** `ip2_mouse.cpp` answers both registers but used to do nothing else.
Host movement now piles up as a signed backlog and is handed over one transition at a time: bit 8 of
the word at `0x31000000` clear means x moved and bit 9 says which way, bit 10 and bit 11 the same for
y, all active low. Reading the word is the acknowledgement, so a new interrupt is only offered once
the guest has taken the last one, which paces the whole thing at exactly the rate the guest can
service. One tick is one guest pixel, and `AddMotion` inverts y because the host's grows downwards
and GL's grows up.

**9. Level 7 was level sensitive in Moira, and it has to be edge triggered.** This is the one that
mattered, and it was not a mouse bug at all. The mouse interrupt is vector 0x56, which the vector
PROM puts on level 7 - and a 68020 recognises level 7 on the *transition* to it, not for as long as
the pins are held there. Moira fired it on every poll instead. Since the mouse latch is only cleared
by the handler reading the register, `_mouseintr` re-entered itself before it could execute its first
instruction, buried the stack under interrupt frames and ran off into whatever that decoded as.

**If you see a flood of illegal instructions, this is the shape of thing to look for.** The fix is a
`nmiTaken` latch in `Moira::checkForIrq`, cleared by `setIPL` whenever the level comes off 7. Nothing
below level 7 changed; level 7 could not work at all before it.

The parity error is also on level 7 and would have hit exactly the same wall.

**10. The cursor.** `FBCselectcursor`, `FBCdrawcursor` and `FBCundrawcursor` are implemented, with
the screen saved underneath and put back. Only the planes in the cursor's own write mask are saved
and restored: mex puts its pointer in the overlay - glyph in font RAM, colour 1024, mask 0xc00, which
is planes ten and eleven - exactly so it can sit on top of the picture without disturbing it, and it
therefore does *not* undraw before repainting a window underneath. Saving whole pixels would make
putting the cursor back revert that repaint.

Its position arrives by two routes: `FBCdrawcursor` carries it outright, and the retrace handler
moves it without stopping the pipe by writing x to FBCdata, raising HOSTFLAG and handing over y when
the cursor interrupt comes back. So a plain write to FBCdata means "cursor y" purely because of what
is outstanding at the time.

**11. `FBCloadmasks` ignored the font RAM base, and it had been corrupting the screen all along.**
`gl_fontslot()` gives every GL process a 256 word aligned slot of the font RAM and `FBCbaseaddress`
tells the FBC where it starts, so two programs can each load a font at "0" without treading on each
other. mex gets slot 2048. It loads thirteen cursor glyphs at offsets 0, 16, 32 up to 192 and then
selects one by its *absolute* address, 2224 - the kernel adds the base itself for the cursor, because
the microcode takes that one address absolute and says so.

Putting those glyphs in at face value dropped every one of them 2048 words too low. Selecting 2224
then read font RAM nobody had written, and selecting 16 - where the kernel's arrow lives - got mex's
second cursor instead. **It also landed mex's font on top of the kernel's**, which is why every digit
and every piece of punctuation on the console screen was garbage: `ib%` for `ib0`, `nswap=%773` for
`nswap=17731`, `\c;X%%\ii;` for `(c)(1)(ii)`. That had been on screen since the textport first drew
and read as a font bug.

**12. DC4 looked the colour map up with the whole twelve bit pixel.** The RAM is sixteen banks of 256
entries, three components each, and the index into a bank is only ever the bottom eight bits - the
driver masks with `DCMULTIMASK` before working out a RAM address either way. What differs is where
the bank comes from: in multimap mode the current map in DCflags picks it, and in single map mode the
pixel's own top four bits do, which is how `gl_domapcolors` writes it. Indexing with all twelve bits
reaches entries nothing ever writes as soon as a colour goes past 255, so **every colour index of 256
or more came out black** - including mex's cursor at 1024, which is why the pointer was drawn
correctly and invisible.

## Also missing, in the order it will start to matter

* **Nothing wakes mex.** It sleeps on `_inchan` and moving the mouse does not by itself queue an
  input event - `DoQueueValuators` only queues a valuator the process asked for with `GR_QDEVICE`.
  Whether mex is asking, and what it does when it gets one, is the next thing to find out now that
  events can be generated at all.

* **`FBCmasklist` and `FBCloadviewport` (the screen mask) do nothing**, so drawing is not clipped to
  a window. With one window nothing lands outside it; with two overlapping ones it will.
* **`FBCbaseaddress`** — `ws->fontrambase`, the offset the FBC adds to every font RAM address. Not
  implemented, so a client with its own font would draw from the wrong place.
* `FBCconfig`, `FBCdrawcursor`/`FBCundrawcursor`, `FBClinewidth`, `FBClinestipple`, `FBCpolystipple`,
  `FBCblockfill` — framed and stepped over, no behaviour.
* One `Unknown FBC opcode 0x10` per run (`FBCmove`). IRIX never sends it, so a length was not
  invented for it; the parser abandons that body rather than guessing and desynchronising.

## The method that found all of this

It is worth following exactly, because guessing does not work here.

1. **Is it blocked or spinning?** `+set dumpAfterSeconds N` for a dump set, then
   `python3 scratch/procs.py scratch/rungui/dumps`. It walks the guest's process table and resolves
   each `p_wchan` against the kernel symbols. `SSLEEP` with a wchan tells you what it is waiting for;
   **`SRUN` with no wchan means it is spinning**, and you go to step 2.
2. **Where is it spinning?** `+set logCpuTrace 1` prints a PC sample every 5M instructions. A user PC
   is below `0x20000000`; a kernel PC is above it. The samples repeat over the loop body, so three or
   four consecutive samples give you the whole loop.
3. **What is that code?** For the kernel, `python3 scratch/kd.py <hexva> [len]` disassembles with
   every operand annotated with the symbol it refers to, and `kd.py -a <va>` names an address. For a
   user binary, extract it with `scratch/efs.py` and disassemble with capstone at text base `0x1000`
   — `scratch/hd/mex.bin` is already extracted.
4. **What is it waiting for?** Take the constants out of the disassembly and grep the IRIX sources
   for them. `EOFPENDINGBITS 0x3fff` in `gl2/include/shmem.h` is what turned an anonymous spin into a
   named handshake in about a minute.
5. **When the screen is wrong rather than stuck, read the command stream.** `scratch/mexlog.sh`
   leaves `motion.log` full of it. One line per polygon fill with its bounds is what identified the
   missing translate: the fills were landing at `(0.5, 0.5)-(719.5, 14.5)` when the window was at
   150, 82, and `GEcompletemm3 (152, 84, 0, 1)` was sitting in the log four lines above them.

## Useful switches

| cvar | what it does |
| --- | --- |
| `+set enableGF2 1` | Fit the GF2. Required — without it the console is the serial line. |
| `+set logGF2 1` | Every FBC and GE command with decoded operands, the viewport, and a line per polygon fill. |
| `+set gf2RetraceHz 60` | The vertical retrace rate. **Set it to 0 to turn the interrupt off**, which is the quickest way to tell a fault in the retrace path from one behind it. |
| `+set logCpuTrace 1` | Periodic PC + register samples. The tool for a spin. |
| `+set dumpAfterSeconds N` | RAM, VRAM and the IP2 page table to `dumps/`. `procs.py` needs the first and third. |

## Reading the screen out of a dump

VRAM held three colours before mex — 0 (page), 7 (text) and 2 (the cursor). With mex up there are
**four**: colour 1 is the title text. Colour 7 goes from about 20k pixels to about 40k, because the
window border and title bar are drawn in it.

```bash
python3 -c "
import struct,collections
d=open('scratch/rungui/dumps/dump_vrameditor_0000.bin','rb').read()
px=struct.unpack('<%dI'%(len(d)//4),d)
print(collections.Counter(v&0xFFF for v in px).most_common(8))"
```

Screen row s counting from the top is VRAM row `767 - s`; VRAM is 1024 rows and only the bottom 768
are displayed. Printing one character per pixel for colour 7 renders the text legibly enough to read
titles out of a dump, which is faster than a screenshot when you only want to know whether something
drew.
