# Resume prompt — the disk subsystem (motion, SGI IRIS 3130)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

Read `resume-prompt.md` in the same directory first — it is the general handoff for this emulator and
this one assumes it. This one is only about storage.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. Read
`resume-prompt.md` for the general state of the project; **this task is the disk subsystem, and what
is left of it is the Storager.**

`CLAUDE.md` bars AI-generated code from *upstream*, which means **never push**. Local commits on
`ai-main2` are fine and expected, with a `Co-Authored-By: Claude` trailer. Scratch files go in
`scratch/` inside the repo (gitignored), not `/tmp`.

**The IRIX 3.7 source tree is on this machine at `/home/dani/repos/SGi-IRIS-3.7/3.7`.** Read it
instead of guessing. For this task: `sys/multibus/` (the drivers — `dsd.c`, `sii.c`, `siireg.h`,
`siilist.h`, `iph.c`), `sys/h/dklabel.h` and `sys/h/dktype.h` (the on-disk label), `sys/efs/` (the
filesystem) and `sys/conf/proto_dv2000` (which says where every controller lives).

## Where it stands

**The DSD 5217 works both ways.** `md0` is the disk the machine actually boots from — `dsd0 at mbio
0x7f00 ipl 1` — and both `DSD5217_FUNC_READ_DATA` and `DSD5217_FUNC_WRITE` are implemented in
`src/component/storage/dsd5217.cpp`. `WriteSector()` mirrors `ReadSector()` and shares all of its
addressing.

That was verified by having IRIX repair its own disk: `/etc/fsck -y /dev/rmd0a` reports `FREE INODE
COUNT WRONG IN SUPERBLK`, `2 DUP BLKS IN FREE LIST`, `BAD FREE LIST`, `SUPERBLK MARKED DIRTY` and
`CHECKSUM WRONG IN SUPERBLK`, fixes them all and prints `FILE SYSTEM WAS MODIFIED`.

**Those faults were in the shipped image from the start**, not something the emulator did — the
bitmap has always had block 11626 marked free while `/etc/mtab`'s inode claimed it, and it was
invisible only because nothing ever rewrote that inode. The image is repaired now.

### The one thing that will bite you

**Let the machine `sync` before you stop it.** EFS flushes its free bitmap from `efs_update`, so a
machine killed after allocating a block leaves an inode pointing at a block the on-disk bitmap still
calls free, and the next boot prints `filesystem corruption on md0a`. Measured: `sync` before
quitting gives a clean boot, killing it without one does not. `/etc/fsck -y /dev/rmd0a` repairs it
if you forget. `scratch/hd/3130-gui.img.bak` is the pre-write backup.

This is not an emulator bug — it is what a real machine does when you pull the plug — but it is new,
because until the write path existed the disk could not be damaged at all.

**A second instance will now really corrupt the image.** `Profile::OpenDisk` opens it
`std::ios_base::in | std::ios_base::out` with no lock and no read-only mode, so two emulators on one
image was harmless only while nothing could write. Check `pgrep -af RelWithDebInfo/motion` before
starting one. It also breaks `xdotool`, which picks the *last* matching window.

## What is left: the Storager

`sii0` probes as **not installed** because nothing answers at its address. The kernel has the whole
driver compiled in — `_siiprobe`, `_siiattach`, `_siistrategy`, `_sifopen` are all in `syms.txt` — so
this is purely a matter of building the hardware.

It matters beyond storage: **model 3130 is the declaration that root lives on `si0a`.** `uname -t` is
nothing but `fopen("/etc/model")` (`bin/uname.c:127`), and `/etc/brc` maps `3030|3120B|3130` to
`root=si0a usr=si0f` while `3020|3115` maps to `md0a`/`md0c`. So a machine with a DSD is a 3115, and
the only way `3130` is a true statement is for the disk to be on a Storager. `/etc/model` currently
says `3115`, which is correct today.

### Where it lives

`sys/conf/proto_dv2000` is the definitive answer and nothing else in the tree states it:

```
controller  iph0  at mb0 csr 0x7010  priority 5 vector ipintr     <- Interphase 2190, separate part
controller  sii0  at mb0 csr 0x7200  priority 5 vector siiintr    <- the Storager
disk        si0   at sii0 drive 0
disk        si1   at sii0 drive 1
disk        sf0   at sii0 drive 2 flags 0x01                      <- floppy on the same controller
controller  siq0  at mb0 csr 0x73fc  priority 5 vector siqintr    <- QIC tape, same register block
tape        sq0   at siq0 drive 0
```

The eight registers are byte-swapped in pairs at `csr + 0x1F8`, so they land at **0x73F8-0x73FF**:

```
R1 0x1F8   R0 0x1F9   R3 0x1FA   R2 0x1FB   R5 0x1FC   R4 0x1FD   R7 0x1FE   R6 0x1FF
```

Note `siq0`'s csr of `0x73fc` is `csr + 0x1FC`, i.e. R4/R5 — **the tape controller is a second
logical device sharing the disk controller's upper registers.** That is the same trick `qic0` and
`dsd0` play by both sitting at `0x7f00`, and it means over-decoding this block invents a tape drive.

### The probe, which is the first thing to satisfy

`siiprobe()` at `sii.c:108`, and it is short. Status is R0; the bits are `ST_BUSY 0x01`,
`ST_DONE 0x02`, `ST_ERROR 0x08`, `ST_QUEUEMODE 0x80`, `ST_IOPBMASK 0x78`. Commands go to R0:
`ST_START 0x01`, `ST_CLEAR 0x02`, `ST_RESET 0x80`, `ST_ABORT 0x40`.

1. write `ST_RESET` to R0, delay
2. write `0` to R0
3. wait for `ST_DONE` to go **away** — timeout is `CONF_DEAD`
4. wait for `ST_DONE` to come **true** — timeout is `CONF_DEAD`
5. write `ST_CLEAR` to R0, delay
6. wait for `ST_DONE` to go away again — timeout is `CONF_DEAD`
7. `CONF_ALIVE`

Getting this far turns `sii0 not installed` into `sii0 at mbio 0x7200 ipl 5` in the boot probe, which
is the first milestone and is worth doing on its own.

### Then the drive, then the transfers

`siiattach` walks `sii_uibs[]` — `sii_hesdi_uib`, `sii_sesdi_uib`, `sii_st506_uib` in `siilist.h` —
trying each until one initialises, which is how the controller supports several drive types. A `uib`
is in `siireg.h:179` and its fields are byte-swapped in pairs like everything else here; the comments
in that struct give the swapped offsets, so read them rather than counting.

Transfers go through an IOPB (`siireg.h:111`), which is a friendlier structure than the DSD's:

```c
struct iopb {
    u_short option_cmd;   /* 0-1 */    u_short err_stat;  /* 2-3  */
    u_short head_unit;    /* 4-5 */    u_short cyl;       /* 6-7  */
    u_short sec;          /* 8-9 */    u_short scc;       /* 10-11 */
    u_short bufh_dma;     /* 12-13 */  u_short bufl;      /* 14-15 */
    ...
};
```

Commands: `C_READ 0x81`, `C_WRITE 0x82`, `C_VERIFY 0x83`, `C_FORMAT 0x84`, `C_MAP 0x85`,
`C_REPORT 0x86`, `C_INIT 0x87`, `C_RESTORE 0x89`, `C_SEEK 0x8a`.

The DSD is the model to copy for the transfer itself. `ReadSector`/`WriteSector` in `dsd5217.cpp`
already solve the two things that are easy to get wrong and are the same here: the crossed Multibus
byte lanes (a 16-bit access at an even address puts the byte at that address in the low half) and
clamping a transfer against what is *left* of the count rather than the whole count.

## The disk image: a relabel, not a rebuild

This is the good news, and it was worth establishing before starting. `sys/h/dklabel.h` defines **one
`struct disk_label` at block 0 shared by every controller** — what differs is a field. So the
filesystems do not move a single block.

The label on `scratch/hd/3130-gui.img` today, read with the image unswapped:

| field | offset | value | wanted |
| --- | --- | --- | --- |
| `d_magic` | 0x00 | `0x072959` | unchanged |
| `d_type` | 0x04 | 1 = `DT_V170` | a drive the Storager's uib list knows |
| `d_controller` | 0x06 | 0 = `DC_DSD5217` | **3 = `DC_STORAGER`** |
| `d_cylinders` | 0x08 | 987 | to suit |
| `d_heads` | 0x0a | 7 | to suit |
| `d_sectors` | 0x0c | 17 | to suit |
| `d_bootfs` | 0x14 | 0 | unchanged — partition a |
| `d_swapfs` | 0x15 | 1 | unchanged — partition b |
| `d_map[8]` | 0x16 | 8 x `{base(4), size(4)}` | copy slot c into slot **f** |

```
a: base=119     size=17850     root
b: base=17969   size=17731     swap
c: base=35700   size=79730     /usr   -> also write this into slot f (index 5)
d: base=119     size=115311
g: base=119     size=115311
h: base=0       size=115430    whole disk
```

**The map offset is 0x16, not the 0x1a quoted in `resume-prompt.md`** — that one is off by four.
Verify it the way this was verified: search the unswapped image for the pair `{119, 17850}`.

`setroot()` in `sys/ipII/autoconf.c` picks root from the label's `d_bootfs`/`d_rootfs` and matches
the drive against `rootdev`; **it does not look at `d_controller`**. So once the Storager attaches and
reads the label, root lands on `si0<d_bootfs>` without any further persuasion.

Work on a **copy**. `scratch/hd/3130-gui.img` is the working md0 disk and `~/3130.img` is pristine;
build the si0 one alongside them so nothing that currently boots is put at risk.

## Reading the image from the host

`scratch/efs.py` reads the pre-EFS filesystem. **It takes `MOTION_IMG` from the environment** and
otherwise defaults to the stock `~/3130.img`, which is how you read the patched disk under `hd/` and
not the pristine one:

```bash
MOTION_IMG=scratch/hd/3130-gui.img python3 scratch/efs.py -l /etc 1
MOTION_IMG=scratch/hd/3130-gui.img python3 scratch/efs.py -c /etc/model
```

Remember the image is 16-bit byte swapped — unswap once
(`img[0::2], img[1::2] = raw[1::2], raw[0::2]`) and work on that copy, exactly as `efs.py` and
`kd.py` do. Every offset in this document is against the *unswapped* copy.

`scratch/guestvars.py <dumpdir> <symbol>...` reads kernel variables out of a dump set and checks its
own page-table walk by printing `_havegrconsole` first; if that is not 0 or 1 the rest is noise.

## What "working" looks like, in order

1. `sii0 at mbio 0x7200 ipl 5` in the boot probe instead of `sii0 not installed`. Get this from a
   serial boot — `+set enableGF2 0` puts the console on the serial line so the probe list lands in
   `motion.log`, which is far quicker than reading it off the graphics screen.
2. `siiattach` finding a drive and printing its geometry.
3. The relabelled image mounting: `root on si0a`.
4. `/usr` on `si0f`, and then `echo 3130 > /etc/model` is finally a true statement.

## Two useful negatives

* **`/etc/fstab` does not exist on this disk and never did**, which is why `/etc/rc`'s
  `mount -avt efs` has nothing to do and why `/usr` is mounted by a patched line in `/etc/rc.s0`
  instead. `/etc/brc` would build one, but only on the boot where it also creates `/etc/model`.
* **The device nodes are all present already** — `/dev/si0a`, `/dev/si0f`, `/dev/sq0`, `/dev/sf0a`.
  IRIX ships a full `/dev` whether or not the hardware is fitted, so a missing node is never the
  reason something does not work here; `/dev/si0a: Invalid argument` is the driver refusing because
  the probe found nothing.
