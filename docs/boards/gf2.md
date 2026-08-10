# GF2

The GF2 board (**G**raphics and **F**ramebuffer 2) is the GL2/IP2 generation 3D graphics board.
There's a lot of logic on the GF2 but it really boils down to 3 parts.

* GE - Geometry Engine
* FBC - Frame Buffer Controller
* BPC - Bit Plane Controller

## Geometry Engine

The highest spec Juniper (IRIS 3000) series graphics option basically has 14 chips.

Two are GAs which FIFO the data coming in over the local bus and convert from IEEE 754 to a custom 20.8 FP format.
These are at the start and end of the chain.

A Geometry Engine is four 32-bit ALUs hooked up to a microcode store and a configuration register. The microcode store stores all possible operations and the configuration engine determines which part of the U/code runs

10MHz GEs use GE 2.5. Older versions are likely GE 2.0.

The rest of the GEs do this in order:

Matrix Multiplication. 1 chip does one row of the matrix. Chaining four together creates a 4x4 matrix multiplier. Cool eh?
In fact this can probably do one row per cycle. Later on, in Clover 1 (4D/60, 4D/70G etc) they doubled these up to run two matrix multiplications in parallel.

Clipping. There's six of these, which do the six clipping directions (left, right, top, bottom, near, far). Also, one of these
seems to be used for Z-buffering on some machines.

Scaling. One GE does two coordinates. 
    ONe does X/Y
    One does Z. Note there are two ALUs left. Jim Clark wrote about introducing stereoscopic 3D rendering using the other two.

Current best guess (based on a GF2 board that had all its heatsinks pulled & the 1982 paper)

```
GA In -> MatM X -> MatM Y -> MatM Z -> MatM W -> Z / ClipLeft?
                                                                          |
                                                                          |
Scale XY <- ClipFar <- ZOut /ClipNear <- ClipBottom <- ClipTop <- ClipRight 
|
|
--> ScaleZ -> Out
```

## Frame Buffer Controller

Four hopelessly bottlenecked AMD Am2903 bit-slice processors. These perform all non wireframe operations and run custom SGI microcode.

## Bit Plane Controller
The interface to the UC4 board (in GF1 boards this is the only way to write to the UC4, but in GF2, Multibus I/O at 50003000-50003fff is exposed as the UC4 board and used by e.g. IP2 PROM. Emulation of this has already occurred) and VRAM. 