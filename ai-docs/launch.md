# Building and running motion

Everything here was verified clean-room: a directory containing nothing but the built binary,
`roms/`, `assets/` and `profile/<disk>.img` boots an IRIS 3130 to a login prompt. Paths are relative
to the repo root.

## Build

```bash
git submodule update --init --recursive        # SDL3 and imgui; a fresh clone has neither
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

The binary lands in `build/output/RelWithDebInfo/motion`, and CMake copies `roms/` and `assets/`
next to it.

**Use `RelWithDebInfo`, not `Debug`.** `Debug` defines `DEBUG`, which enables `MOTION_ASSERT` — the
cylinder assert in the DSD's `CHSToLinear` fires on the stock disk images — and also turns on
AddressSanitizer and `-O0`.

## Run

Put a disk image in the `profile/` directory beside the binary, then:

```bash
cd build/output/RelWithDebInfo
./motion +set skipLauncher 1 +set startPaused 0 +set enableGF2 1 \
         +set diskController storager +set profileDisk0Path 3130-si0-gui.img \
         +set diskWriteMode overlay
```

That gives an IRIS 3130 booting unattended to `IRIS login:` on the graphics screen. Log in as `root`
— no password. `profile/` is created if it does not exist, and `ip2_sram.bin` (the PROM's battery
backed SRAM) is created inside it on first run.

Give it about 30 seconds to reach the login prompt, and another 30–80 for `mex`, the window manager,
if you start one.

## The settings that matter

| cvar | default | what it does |
| --- | --- | --- |
| `diskController` | `dsd` | Which disk controller is fitted: `dsd`, `storager` or `none`. **A 3130 image needs `storager`**; with the wrong one the machine looks for a drive that is not there. Both boards are combined disk/floppy/tape controllers, so this picks the whole personality. |
| `profileDisk0Path` | `3130.img` | The machine's first physical drive — `md0` on the DSD, `si0` on the Storager. Relative to `profile/`. |
| `profileDisk1Path` | `3130_2.img` | The second drive (`md1` / `si1`). Defaults to a name that does not exist, so that bay is empty. |
| `diskWriteMode` | `direct` | `overlay` is copy on write: the image is opened read-only and guest writes are held in memory, so it does not matter how you stop the emulator and two instances on one image stop being dangerous. `readonly` refuses writes. **Use `overlay` unless you mean to keep the changes.** |
| `diskCommitOnExit` | `0` | With `overlay`, fold what the guest wrote back into the image at shutdown. |
| `enableGF2` | `0` | Fit the GF2 graphics board and put the console on the screen. **With this off the window is black** — the console stays on the serial line, which is correct behaviour and surprises everybody once. |
| `bootDevice` | follows `diskController` | Which device the PROM loads the kernel from: `md`, `sd`, `mf`, `sf`, `mt`, `st`, `ip`, `xns`, `prom`, `eprom`. Rarely needs setting. |
| `skipLauncher` | `0` | `1` goes straight to the machine instead of the launcher window. |
| `startPaused` | `1` | `0` starts the machine running. |

Logging, all off by default and all cheap to turn on: `logStorager`, `logGF2`, `logMouse`,
`logKeyboard`, `logIP2MMU`, `logCpuTrace`. `logGF2` is the exception — it slows the machine enough to
change timing.

## Disk images

| image | controller | what it is |
| --- | --- | --- |
| `3130-si0-gui.img` | Storager | **The one to use.** Boots unattended to `IRIS login:`, `uname -t` says `3130`, `/usr` mounted from `si0f` with the full userland and the GL demos. |
| `~/3130.img` | DSD 5217 | The pristine dump. A 3115: single-user shell, `/usr` not mounted. Keep it as the reference and do not boot it. |
| `scratch/hd/3130-gui.img.bak` | DSD 5217 | The 3115 with `/usr` mounted off `md0c`. Clean. |

A DSD image needs `+set diskController dsd`, which is the default.

**Let the machine `sync` before you stop it, or run in `overlay` mode.** IRIX only flushes its free
bitmap from `efs_update`, so a machine killed after allocating a block leaves an inode citing a block
the on-disk bitmap still calls free, and the next boot prints `filesystem corruption on si0a`.
`overlay` makes the question go away.

## Scripting the console

```bash
./motion ... +set consoleInputAfterSeconds 50 +set consoleInput 'root\n\p9\p9who\n'
```

`\n`, `\r`, `\t`, `\\` and `\xNN` work; `\pN` waits N seconds (single digit, so chain `\p9\p9`).
Several commands on one line separated by `;` is cheaper than waiting between them.

**`consoleInput` drives the serial line.** With `+set enableGF2 1` the shell is on the graphics
screen and scripted input goes nowhere — drive that with `xdotool` instead. For the same reason the
serial log goes quiet after `Jumping to loaded program` when GF2 is fitted; that is the kernel moving
its console to the textport, not a hang. `SerialLine` also only logs on a newline, so a prompt with
no trailing newline (`# `, `login:`) never appears in `motion.log` at all — test by logging in, not
by grepping for the prompt.

## Reading and repairing the disk images from the host

`~/repos/rusty-backup` understands this filesystem — IRIX 3.7's EFS is the *ancestor* of the IRIX
5.3+ one and needs its `src/fs/efs_v1.rs`, not `src/fs/efs.rs`.

```bash
cd ~/repos/rusty-backup && cargo build --release --bin rb-cli
RB=~/repos/rusty-backup/target/release/rb-cli
$RB inspect <img>                      # partition table; it detects the 16-bit byte swap itself
$RB ls      <img>@1 /etc               # @1 is root, @3 is /usr
$RB get     <img>@1 /etc/inittab out   # absolute host paths; "-" for stdout is not supported
$RB put --force <img>@1 in /etc/inittab
$RB chmod   <img>@1 /etc/rc.getdate 755
$RB fsck [--repair] <img>@1
```

**Repair images with `rb-cli fsck --repair`, not with IRIX's `/etc/fsck`.** Running fsck against a
mounted root writes the corrected free list to the raw device while the kernel still holds the old
bitmap in core, and the next `sync` puts the stale copy straight back — the repair never sticks.
`rb-cli` works on the image with nothing running.

## Known rough edges

* **An intermittent kernel panic**, roughly 1 boot in 3: `kernel trap: type=2 pc=0` /
  `panic: trap(print)`. Suspected to be `AddrSpace` translating only the first address of a sized
  access, so a misaligned operand straddling a page boundary resolves entirely in the wrong page.
* **No network.** The kernel probes for an Excelan EXOS at `0x7ffc` and finds nothing; `ifconfig`
  fails at multi-user and `routed`/`inetd` start with nothing to do.
* **No tape or floppy**, on either controller.
* **The GF2 geometry pipe is partial** — no clipping and no z-buffer, and drawing is not clipped to a
  window. The GL demos in `/usr/people/demos` will draw something and be wrong in interesting ways.
* Shutdown segfaults on the way out. Pre-existing, harmless with `overlay`.

`resume-prompt.md` is the full state of the project; `resume-prompt-multiuser.md`,
`resume-prompt-disk.md` and `resume-prompt-mex.md` cover multi-user, storage and the window manager.
