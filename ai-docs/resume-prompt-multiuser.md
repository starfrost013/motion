# Resume prompt — the 3130 that boots to a login prompt (motion)

Paste the section below into a new session. Everything after the horizontal rule is the prompt.

This covers the work that turned a 3115 booting to a single-user serial shell into a 3130 booting
unattended to `IRIS login:`. `launch.md` alongside it is how to build and run.

---

We're working on `motion`, an SGI IRIS 3130 emulator at `/home/dani/repos/motion`. **This document
is what changed in the multi-user session and what is worth doing next.**

Every path in this document is relative to the repo root, not to `ai-docs/`.

`CLAUDE.md` bars AI-generated code from *upstream*, which means **never push**. Local commits on
`ai-main2` are fine and expected, with a `Co-Authored-By: Claude` trailer. Scratch files go in
`scratch/` inside the repo (gitignored), not `/tmp`.

**The IRIX 3.7 source tree is at `/home/dani/repos/SGi-IRIS-3.7/3.7`, and the PROM's own source is in
it** — `stand/lib/dev/` is where the standalone drivers live. Read them instead of guessing.

## Where it stands

```
Loading: sd.0:defaultboot
sii0 at mbio 0x7200 ipl 5
si0 (Priam V170 Name: Priam V170) slave 0
root on si0a
swap on si0b, swplo=0 nswap=17731
Tue Aug 25 09:23:54 PDT 2026
Hostname: IRIS
Standard daemons: syslogd lpsched.
Internet daemons: routed portmap inetd.
Mailer daemons: sendmail.
More standard daemons: cron.
IRIS login: root
# /bin/uname -t; who; tty
3130
root       console      Aug 25 09:24
/dev/console
```

Unattended, no prompts, zero filesystem corruption. Run it:

```bash
cd scratch/runsii
./motion +set skipLauncher 1 +set startPaused 0 +set enableGF2 1 \
         +set diskController storager +set profileDisk0Path 3130-si0-gui.img \
         +set diskWriteMode overlay
```

Drop `enableGF2` for the serial console. **`diskWriteMode overlay` should be your default** — see
below. The six commits are `124dd05..65d04f8`.

## The five things that changed, shortest first

**`2f78dac` — DUART carrier is active low.** Register 0x0D returned a flat `0xFF`, and two of those
bits are carrier detect. `sduart.c` says "the input is low-true" and `du_act()` clears `DP_DCD` when
the bit reads *set*; `IPORT_DCDA 0x08` / `IPORT_DCDB 0x04` for the IP2. So every line read "no
carrier", and since `co_9600` has no `CLOCAL`, `du_open()` waited forever and the console getty never
printed `login:`. **This is why multi-user looked like a hang.** Both channels report carrier now,
because in an emulator the terminal on the other end is always plugged in.

**`fce8873` — one disk controller, chosen by name.** `diskController` takes `dsd`, `storager` or
`none`; `profileDisk0Path` and `profileDisk1Path` are the machine's **two physical drives** on
whichever board that is (md0/md1 or si0/si1), not one drive each on two boards. Both boards are
combined disk/floppy/tape controllers, which is why it is one string rather than an enable flag per
board. `bootDevice` now defaults to the disk of whichever controller is fitted. This also fixed a
real bug: the DSD parsed `iopb.unit` and then ignored it, so `md1` attached as a second copy of `md0`.

**`893ad64` — copy-on-write disks.** `+set diskWriteMode overlay` opens the image read-only and keeps
written sectors in memory. `readonly` refuses writes; `direct` is the old behaviour and still the
default. `+set diskCommitOnExit 1` folds an overlay back into the image at shutdown. See
`src/base/filesystem/disk_image.hpp`.

**`3696aca` — a 68020 may make a misaligned access, and this one did.** The big one. `Memory`'s
16- and 32-bit accessors indexed the array with `addr >> 1` / `addr >> 2`, throwing the low address
bits away. A 68020 *permits* misaligned word and long operands, so IRIX's compiler emits
`move.l (a0)+,(a1)+` over unaligned buffers and every one of them read or wrote up to three bytes
early. That was the `wsiris` -> `|wsiri` corruption, `TZ` -> `TPST8PDT`, `telinit` -> `elinit`, and
the long-standing type-ahead bug. `PROM`, `PROM_SRAM` and `BP3` had the same indexing.

**`124dd05` — the Interphase Storager.** See the header comments in `storager2.hpp`.

## The disk image, and why it boots multi-user

`scratch/hd/3130-si0-gui.img`. Three files differ from `3130-si0.img`:

| file | change |
| --- | --- |
| `/etc/inittab` | `is:s:initdefault:` -> `is:2:` |
| `/etc/rc.getdate` | replaced with `date +%m%d%H%M.%S%y`, mode 755 |
| `/etc/bcheckrc` | `2300 \| 2300T \| 3010)` -> `... \| 3130)` |

**`s` is what SGI shipped** — verified against the Standard System root on the GL2-W3.6 distribution
tape, which is byte-identical to the disk. Nothing we did caused single-user; IRIX 3.7 genuinely
boots to `s` and waits.

The other two kill the boot's interactive prompts. `bcheckrc` asks "Is the date correct?" unless
`[ -x /etc/rc.getdate ]` *and* running it feeds `date` something it accepts — the shipped file is a
sample that fetches the date over XNS from a network date server, and is mode 644 so it never runs.
Handing back the RTC's own date satisfies the test. The second prompt, "Do you want to check
filesystem consistency?", is unconditional for a 3130; SGI only auto-checks for machines meant to
boot unattended.

**A caveat on that third change.** It makes every boot run `/etc/fsck -D -y` on the *mounted* root
with its output discarded. Fine while clean, but if damage ever appears the repair gets clobbered
(see below) and you will never see it, because the output goes to /dev/null. If that starts
happening, change the case to a plain `exit 0` for 3130 and repair offline instead.

## rusty-backup reads and writes this disk, and it is the tool to reach for

**An earlier note said it could not. That was wrong** and it cost real time.
`~/repos/rusty-backup` has two SGI implementations: `src/fs/efs.rs` is EFS as IRIX 5.3-6.5 writes it,
which does want `fs_magic` 0x00072959 at superblock offset 28 and rejects this volume — and
`src/fs/efs_v1.rs` is **the ancestor**, which is exactly what IRIX 3.7 has. The magic is not missing,
it is six bytes further on, because the 68020 packs the superblock 2-byte aligned and `fs_time` at
0x16 shifts everything after it:

```
fs_magic @0x26 = 0x00041755      <- EFS_V1_MAGIC
fs_size 17848  fs_firstcg 8  fs_cgfsize 3568  fs_ncg 5  fs_fname "root"  fs_fpack "sgi"
```

Its module header says it was verified against `<sys/efs_sb.h>` "as recovered from a real IRIS 3130
disk" — this disk — and `docs/SGI_EFS_v1.md` is the best description of this format anywhere.

```bash
cd ~/repos/rusty-backup && cargo build --release --bin rb-cli   # a few minutes, big dep tree
RB=~/repos/rusty-backup/target/release/rb-cli
$RB inspect  <img>                          # partition table; it spots the byte swap itself
$RB ls       <img>@1 /etc                   # @1 root, @3 /usr. Paths are volume-relative
$RB get      <img>@1 /etc/inittab out       # absolute host paths; it does not take "-" for stdout
$RB put --force <img>@1 in /etc/inittab     # mode and owner preserved from the replaced file
$RB chmod    <img>@1 /etc/rc.getdate 755
$RB fsck [--repair] <img>@1
```

### The repair trap, which wasted most of a session

**IRIX's `/etc/fsck` followed by `sync` undoes its own repair.** fsck writes the corrected free list
to the raw device while the kernel holds the same filesystem mounted with its own in-core bitmap, and
the next `sync` puts the stale copy straight back. Every fsck reported the identical faults and none
of them ever stuck.

Two ways out. Run fsck and stop the machine *without* syncing — that converges on the first try. Or,
much better, repair the image offline with `rb-cli fsck --repair <img>@1`, which has no kernel to
argue with. It found what IRIX had been claiming to fix for hours:

```
ERROR [BitmapMissingAllocation] block 11670 is used by inode 808 but the bitmap marks it free
ERROR [BitmapLeakedBlock] 11 block(s) marked in use but claimed by no inode
ERROR [TinodeMismatch] fs_tinode is 1290, but 1296 inodes are free
```

Inode 808 is `/etc/wtmp` and 815 is `/etc/mtab` — the two files that are only written when the
machine *does* something, which is why the damage was invisible at mount and only surfaced at
multi-user. Both volumes verify clean now (451 files on root, 2580 on /usr).

## There is no network, and that is the obvious next thing

Correct as of this session: **nothing network is emulated.** The kernel probes for it and finds
nothing —

```
ex0 not installed
ex1 not installed
hy0 not installed
```

— and at multi-user `/etc/rc` runs `ifconfig`, which fails with `ioctl (SIOCGIFFLAGS): no such
interface`, then starts `routed portmap inetd` anyway with nothing for them to do.

What the kernel is looking for, from `sys/conf/`:

```
proto_tcp / proto_nfs:
    device  ex0   at mb0 csr 0x7ffc   priority 2 vector exintr    <- Excelan EXOS
    device  ex1   at mb0 csr 0x7ffe   priority 2 vector exintr
    device  hy0   at mb0 csr 0xf000   priority 2 vector hyintr    <- Hyperchannel
proto_xns:
    device  nx0   at mb0 csr 0x7ffc   priority 2 vector nxintr
```

The driver is `sys/multibusif/if_ex.c` + `if_exreg.h` (and `if_nex.c` for the variant). Multibus
interrupt 2, so `ivectors[2] = level2`, and `level2()` in `sys/multibus/level.c` calls `exintr()`.
The board is an intelligent one — the host talks to on-board firmware through a request/reply ring in
Multibus memory rather than to registers — so it is closer in shape to the DSD 5217 than to the
Storager. Userland is all there: `ifconfig`, `routed`, `inetd`, `portmap`, `sendmail`, NFS and XNS on
the options tape.

**A warning that is already written down and worth repeating: do not over-decode near 0x7ffc.** The
DSD used to claim all of `0x7F00-0x7FFF` and answer `0xFF`, so the EXOS probe found a phantom board,
attached it, and hung forever in `_exconfig`. Anything fitted there has to be a real implementation
or nothing at all.

## Other things worth doing

* **The tape and floppy halves of the Storager** — `siq0` (QIC02 at 0x73FC) and `sf0`. Both are the
  same board and neither is fitted.
* **`si1`** works now (two drives on one controller), but there is no floppy image support.
* **GF2 geometry** — no clipping, no z-buffer, and `FBCmasklist` does nothing so drawing is not
  clipped to a window. The demos in `/usr/people/demos` (`airshow`, `jet`, `flight`, `cube`, `arch`)
  will draw *something* and be wrong in interesting ways. `+set logGF2 1` decodes the command stream.
* **An intermittent kernel panic, ~1 boot in 3.** Seen once at a multi-user login shell running
  `who`, and not reproduced by running the identical command twice more, so it is not that command:

  ```
  kernel trap: type=2 pc=0 ps=2000 aaddr=0
  panic: trap(print)
  ```

  `pc=0` in supervisor mode means the kernel branched to address zero and bus errored fetching there
  - a wild or null function pointer. The first suspect is the page-crossing gap immediately below,
  because that is the one known way a kernel structure can be read wrong without anything else
  noticing. `+set logCpuTrace 1` prints the last 48 retired PCs and the control-flow edge list, which
  is what identified the `jsr` restart bug and is the right tool here too.

* **`AddrSpace` splits nothing.** A misaligned word or long that straddles a page boundary is
  translated for its first page only and handed whole to one component; a real 68020 splits it into
  separate bus cycles and translates each. Rare, but wrong. Same for an access straddling two
  components.

## Traps, collected

* **`consoleInput` drives the serial line.** With `+set enableGF2 1` the shell is on the graphics
  screen and a scripted `sync` goes nowhere. Use `xdotool` for the graphics console.
* **The serial log goes quiet after `Jumping to loaded program` when GF2 is fitted**, because the
  kernel's console moves to the textport. That is not a hang.
* **`SerialLine` only logs on newline**, so a prompt with no trailing newline (`# `, `login:`) never
  appears in `motion.log` at all. Test by logging in, not by grepping for the prompt.
* **Spacing scripted commands with `\p9\p9` is no longer necessary** — that was the misaligned access
  bug. Chaining with `;` on one line is the cheapest way to drive a test.
* **Check `pgrep -af RelWithDebInfo/motion` before starting one**, not for the disk any more but
  because `xdotool` picks the *last* matching window.
