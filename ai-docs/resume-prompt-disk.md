# Resume prompt — the disk subsystem (motion, SGI IRIS 3130)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

Read `resume-prompt.md` in the same directory first — it is the general handoff for this emulator and
this one assumes it. This one is only about storage.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. Read
`resume-prompt.md` for the general state of the project; **this task is the disk subsystem.**

Every path in this document is relative to the repo root, not to `ai-docs/`.

`CLAUDE.md` bars AI-generated code from *upstream*, which means **never push**. Local commits on
`ai-main2` are fine and expected, with a `Co-Authored-By: Claude` trailer. Scratch files go in
`scratch/` inside the repo (gitignored), not `/tmp`.

**The IRIX 3.7 source tree is on this machine at `/home/dani/repos/SGi-IRIS-3.7/3.7`.** Read it
instead of guessing.

## Where it stands: the machine is a 3130

Both controllers work, both ways, and the machine now boots end to end off an **Interphase
Storager**, which is the thing that makes it a 3130 rather than a 3115:

```
Loading: sd.0:defaultboot            <- the PROM reads the kernel off the Storager
sii0 at mbio 0x7200 ipl 5
si0 (Priam V170 Name: Priam V170) slave 0
root on si0a
swap on si0b, swplo=0 nswap=17731
# /bin/uname -t
3130
# /etc/mount
/dev/si0a on / type rw,raw=/dev/rsi0a (0)
/dev/si0f on /usr type rw (1)
```

`/etc/fsck -y /dev/rsi0a` runs on it, finds faults, repairs them and prints
`***** FILE SYSTEM WAS MODIFIED *****`, which is the read and write paths both proved through the
queued-mode interrupt path in one command.

Run it:

```bash
cd scratch/runsii
./motion +set skipLauncher 1 +set startPaused 0 \
         +set diskController storager +set profileDisk0Path 3130-si0.img \
         +set diskWriteMode overlay
```

`bootDevice` works itself out - it defaults to the disk of whichever controller is fitted, so a
Storager machine loads its kernel from `sd.0` without being told to.

`diskWriteMode overlay` is copy on write: the image is opened read-only and guest writes are held in
memory, so it does not matter how you stop the emulator. Drop it when you want the writes kept, or
keep it and add `+set diskCommitOnExit 1` for the one boot where you do.

**The DSD 5217 is unchanged and still the default.** With no arguments the machine is the 3115 it
always was — `root on md0a`. These cvars change that:

| cvar | default | what it does |
| --- | --- | --- |
| `diskController` | `dsd` | **Which disk controller is fitted**: `dsd`, `storager` or `none`. A machine has one, and which one decides what the machine is. |
| `profileDisk0Path` | `3130.img` | The **first physical drive** - `md0` on the DSD, `si0` on the Storager. |
| `profileDisk1Path` | `3130_2.img` | The **second physical drive** - `md1` / `si1`. Does not exist by default, so that bay is empty. |
| `bootDevice` | follows `diskController` | Which device the **PROM** loads the kernel from. `md` for the DSD, `sd` for the Storager; setting it explicitly wins. |
| `logStorager` | 0 | One line per command the Storager executes. |
| `diskWriteMode` | `direct` | `overlay` is copy on write - see below. `readonly` refuses writes. |
| `diskCommitOnExit` | 0 | With `overlay`, write what the guest did back into the image on shutdown. |

**One controller at a time, and it brings its own floppy and tape.** Both of these boards are
combined disk/floppy/tape controllers - the DSD carries `mf0` and `qic0`, the Storager carries `sf0`
and `siq0` - which is why this is one string rather than an enable flag per board. The two drive
paths are the machine's two physical drives on whichever board that is.

## What the Storager is, and where the emulation lives

`src/component/storage/storager2.{hpp,cpp}`. `sii0` in the kernel, `sd`/`si` to the PROM,
"Interphase 3030" in `dktype.h`. `sys/conf/proto_dv2000` is the definitive statement of where it
lives:

```
controller  sii0  at mb0 csr 0x7200  priority 5 vector siiintr
disk        si0   at sii0 drive 0
disk        si1   at sii0 drive 1
disk        sf0   at sii0 drive 2 flags 0x01
controller  siq0  at mb0 csr 0x73fc  priority 5 vector siqintr
tape        sq0   at siq0 drive 0
```

The board decodes 512 bytes of Multibus I/O at 0x7200, holding three different things:

```
0x000-0x01F   fourteen control words at even offsets, then the IOPB count register at 0x1E
0x020-0x1DF   fourteen 32 byte IOPBs of on-board RAM, used in queued mode
0x1F8-0x1FF   R0-R7
```

All of that is in **Multibus** byte offsets. The IP2 crosses the byte lanes, so Multibus byte N is
the byte the 68020 wrote at N ^ 1 — which is why `siireg.h` defines `ST_R0` as 0x1F9 and `ST_R1` as
0x1F8 rather than the other way round, and why `dsd.c`'s probe says "remember that the port address
has to be byte swapped" and XORs it by hand. `Storager2::Read8`/`Write8` uncross it once so the rest
of the file reads like the controller's own firmware.

### The single most useful thing found this session

**The PROM's own source is in the tree.** `stand/lib/dev/std.c` is the standalone Storager driver the
PROM binary was built from, and `stand/include/stdreg.h` declares the IOPB as **individual bytes with
the Multibus offset of each one in a comment**:

```c
struct iopb {
    u_char  i_option;   /* 1 */    u_char i_cmd;    /* 0 */
    u_char  i_error;    /* 3 */    u_char i_status; /* 2 */
    u_char  i_head;     /* 5 */    u_char i_unit;   /* 4 */
    u_char  i_cyll;     /* 7 */    u_char i_cylh;   /* 6 */
    ...
```

so the layout the board sees is not something to infer from `siireg.h`'s `u_short`s and `swapb()`
calls — it is written down:

```
0  command      1  options       8  sector hi      9  sector lo
2  status       3  error         a  count hi       b  count lo
4  unit         5  head          c  dma burst      d-f buffer address, big endian 24 bit
6  cylinder hi  7  cylinder lo   10-11 controller i/o address   15-17 linked iopb address
```

The UIB is documented the same way in both `siireg.h` and `stdreg.h`: heads at 0, sectors per track
at 1, bytes per sector little endian at 2-3, cylinders little endian at 0x1a-0x1b. Note the UIB is
little endian where the IOPB is big endian; that is the board, not a mistake.

`stand/cmd/sifex/` is the Storager formatter and exerciser and is the best description of the
commands this emulation does not implement yet.

### The two modes

**Ordinary mode.** R1/R2/R3 hold the 24-bit Multibus address of an IOPB in host memory; `ST_START`
in R0 executes it and the driver polls the IOPB's own status byte for `S_OK` (0x80) or `S_ERROR`
(0x82). Both `siicmd()` and `stdcmd()` use `STARTWO()`, which sets `ST_NOINTERRUPT`. Everything up to
and including reading the label happens this way.

**Queued mode.** `siistart()` writes 0xFF to all three address registers and then `ST_START`; from
then on the IOPBs are the board's own RAM and each is launched by writing `SC_ENABLE` (0x83) to its
control word at Multibus 0x7200 + 2n. This is the only path that interrupts.

**Completions have to queue.** `siistrategy()` runs `siistart()` under `spl6`, so up to fourteen
IOPBs can finish before the level 5 handler gets to run. R0's `ST_DONE` and R1's unit/IOPB byte
report the *head* of a queue and `ST_CLEAR` pops it, leaving the interrupt asserted while anything
remains. A single "which unit interrupted" register would drop all but the last completion, and a
dropped completion is a `struct buf` that is never `iodone()`d.

### Three things that will bite whoever touches this next

* **Do not decode 0x73FC-0x73FF.** Those are R4-R7 and R4-R7 *are* `siq0`: one controller answering
  as two logical devices, the same trick `qic0` and `dsd0` play at 0x7f00. `siqprobe()` does nothing
  but touch its R2 and `return CONF_ALIVE`, so anything answering there invents a tape controller and
  then hangs `siqattach` initialising a drive that is not fitted. They are left to bus error, and
  `siq0 not installed` is the truth.
* **The reset self test has to be visible.** `siiprobe()` writes `ST_RESET`, waits for `ST_DONE` to
  go *away*, then waits for it to come *true*, and calls a timeout on either one `CONF_DEAD`. A board
  that answers DONE the instant it is reset probes as dead just as surely as one that never answers.
  The emulation counts this in status reads rather than emulated microseconds, because the driver's
  timeouts are spin loops and tying it to the clock makes the answer depend on how fast the host is
  running the 68020 that day.
* **A non-queued command must not raise the interrupt.** `siiintr()` answers by taking an IOPB index
  out of R1 and dereferencing the `struct buf *` the driver stashed in that IOPB, and panics on a
  null one. An ordinary-mode command has neither. Every caller in `sii.c` uses `STARTWO()` so this
  never comes up, but the emulation logs and suppresses it rather than guessing.

## The disk image was a relabel, not a rebuild

`sys/h/dklabel.h` defines **one `struct disk_label` at block 0 shared by every controller**, so
moving a disk between controllers does not move a single filesystem block. `scratch/relabel.py` is
the tool; with no options it just prints the label.

```bash
cp scratch/hd/3130-gui.img scratch/hd/3130-si0.img
python3 scratch/relabel.py scratch/hd/3130-si0.img --controller 3 --copy-partition c:f
```

* `d_controller` at offset 0x06 goes from 0 (`DC_DSD5217`) to **3 (`DC_STORAGER`)**.
* partition slot **f** gets a copy of slot c, because `/etc/brc` maps `3030|3120B|3130` to
  `root=si0a usr=si0f` where `3020|3115` gets `md0a`/`md0c`.
* `/etc/rc.s0` was pointed at `/dev/si0f` with `scratch/efswrite.py`.
* `/etc/model` was written from inside the guest with `echo 3130 > /etc/model`.

Geometry, `d_type` and everything else stay exactly as they were: 987/7/17 = 117453 blocks = the
60135936 byte image, to the byte. **The map offset is 0x16**, and every offset is against the
unswapped image (`img[0::2], img[1::2] = raw[1::2], raw[0::2]`).

`setroot()` in `sys/ipII/autoconf.c` picks root from the label's `d_bootfs`/`d_rootfs` and never
looks at `d_controller`, so once the Storager attaches, root lands on `si0a` by itself.

**A wrinkle that used to bite and no longer can.** While both controllers could be fitted at once,
root moved to si0a — `sii` attaches after `dsd` and the last one to match wins — but **swap stayed on
md0b**, because `setroot()` only replaces the swap partition it has if the new one is strictly
*larger*, and the two were the same size. One controller at a time makes that unreachable.

## What is left

Nothing blocks a working disk. In rough order of how much each one buys:

* **The tape and floppy.** `siq0` (QIC02 cartridge tape at 0x73FC, `sq0`) and `sf0`, the floppy on
  unit 2 of the disk controller. Both are the same board and neither is fitted, so a real 3130's
  `siq0 at mbio 0x73fc ipl 5` line is missing. `sys/multibus/siq.c` and `siqreg.h` are the driver;
  the floppy is `sifattach()`/`sifstrategy()` in `sii.c` and `sii_mits_uib` in `siiuib.h`. Doing the
  tape means widening the decode to the whole 512 bytes and implementing R4-R7.
* **Formatting.** `C_FORMAT`, `C_MAP` and `C_REFORMAT` return "invalid command code", so a disk
  cannot be built from scratch on the emulated board — which is what `mkboot` and `stand/cmd/sifex/`
  do. Nothing needs it while a labelled image exists, but it is the difference between running a disk
  and making one.
* **Linked IOPBs.** `i_link{h,m,l}` at Multibus 0x15-0x17 chain one IOPB to the next. Both drivers
  zero them, so nothing exercises it.
* **Timing.** Commands complete inside the store that starts them, so `ST_BUSY` never reads set and
  there is no rotational latency or DMA duration. Nothing in either driver depends on it, and it
  removes a class of races that would otherwise need a scheduler — but it does mean disk I/O is free,
  which flatters anything that measures it.
* **`C_REPORT`'s answers are made up.** Firmware revision 2.1 and product code 2. `stdprobe()` only
  needs the command to succeed; `sifex` prints the rest. If a real Storager 2's report block ever
  turns up, put the true values in.

## Two things that are not bugs

* **Repairing this filesystem: `fsck` then `sync` undoes the repair.** IRIX's `/etc/fsck` writes the
  corrected free list to the raw device while the kernel has the same filesystem mounted with its own
  in-core bitmap, so the next `sync` writes the stale copy straight back over it. That is why every
  fsck kept reporting the identical faults and never converged. Two ways out: run fsck and stop the
  machine *without* syncing, or - much better - repair the image offline with
  `rb-cli fsck --repair <img>@1`, which has no kernel to argue with. See the rusty-backup note in
  `resume-prompt.md`; it found damage IRIX's own fsck had been claiming to fix for hours.

* **`filesystem corruption on si0a` after a boot you killed.** EFS flushes its free bitmap from
  `efs_update`, so a machine stopped after allocating a block leaves an inode pointing at a block the
  on-disk bitmap still calls free. It is what a real machine does when you pull the plug.
  **`+set diskWriteMode overlay` is the answer** - the image is opened read-only and guest writes are
  held in memory, so no amount of killing the emulator can damage it, and a boot purely to check
  something costs the disk nothing. Without it, `sync` first or repair with `/etc/fsck -y /dev/rsi0a`,
  and note that **`consoleInput` drives the serial line**, so with `+set enableGF2 1` the shell is on
  the graphics screen and a scripted `sync` goes nowhere.
* **`md1` in the probe list.** The DSD answers for unit 1 out of the same image, so md0 and md1 are
  the same disk twice. Pre-existing, unrelated to any of this, and harmless because nothing mounts
  md1 — but it is why the DSD prints two drives and the Storager prints one.

## Reading the image from the host

`scratch/efs.py` reads the pre-EFS filesystem and takes `MOTION_IMG` from the environment:

```bash
MOTION_IMG=scratch/hd/3130-si0.img python3 scratch/efs.py -l /etc 1
MOTION_IMG=scratch/hd/3130-si0.img python3 scratch/efs.py -c /etc/model
```

`scratch/efswrite.py <image> <part_blk> <path> <newfile>` rewrites a file in place when the new
contents still fit the blocks already allocated (`--show <path>` dumps one). Root is partition block
119, `/usr` is 35700. `scratch/relabel.py <image>` prints the label.

**Two emulators on one image will corrupt it** - in `direct` mode. `Profile::OpenDisk` opens it
`in | out` with no lock, so nothing stops two of them interleaving writes. In `overlay` or `readonly`
mode neither holds the file for writing and it stops mattering. Two instances still break `xdotool`,
which picks the *last* matching window.
