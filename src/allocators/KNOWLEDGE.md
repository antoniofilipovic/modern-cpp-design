# What Every Programmer Should Know About Memory — Knowledge

## 1. Types of RAM

Modern computers use two fundamentally different types of RAM, each suited to a different role.
**DRAM** (Dynamic RAM) is used for main memory — the RAM sticks in your machine. It is cheap
and dense (1 transistor + 1 capacitor per bit), but slow and requires constant refreshing.
**SRAM** (Static RAM) is used for CPU caches (L1, L2, L3). It is fast and doesn't need
refreshing, but expensive and large (6 transistors per bit).

Both types are **volatile** — they lose data when power is removed. For persistent storage,
you need Flash/NAND (SSDs, phones), which traps charge in an insulated floating gate where
it can sit for years.

---

## 2. How a DRAM Cell Works

### 2.1 The Capacitor — Storing a Bit

A capacitor is two metal plates separated by an insulator. When voltage is applied across it,
electrons pile up on one plate and get pulled away from the other. Remove the voltage source
and the charge stays — the electrons have nowhere to go. This stored charge represents the bit:

- **Charged capacitor = 1**
- **Discharged capacitor = 0**

A DRAM cell is just one capacitor and one transistor (the access gate):

```
        Word Line (row select)
            │
            ├── Gate
            │
Bit Line ───┤ Transistor ├─── Capacitor ─── Ground
                                  │
                            (stores charge
                             = your bit)
```

### 2.2 Why Reads Are Destructive

To read the cell, the memory controller connects the capacitor to a sense wire (the bit line).
Here is what happens physically:

1. The bit line sits at a reference voltage (roughly halfway between "1" and "0").
2. The transistor gate opens, connecting the tiny capacitor to the comparatively huge bit line.
3. Charge flows from the capacitor onto the bit line (if charged) or from the bit line into the
   capacitor (if empty). This is basic charge sharing — two conductors at different voltages
   connected together will equalize.
4. The sense amplifier detects the tiny voltage bump (or dip) on the bit line and decides: 1 or 0.

In step 3, the capacitor's charge redistributes onto the much larger bit line. The capacitor is
tiny (femtofarads), the bit line is massive in comparison. After the read, the capacitor is
essentially drained to the bit line's voltage — **the original charge is gone**. The sense
amplifier got the answer, but the cell is destroyed.

That is why every read must be followed by a **write-back**: the sense amplifier drives the bit
line to full voltage (or ground) to recharge the capacitor to its original state.

### 2.3 Refresh — The "Dynamic" in DRAM

Even without reads, the capacitor leaks. The insulator between the plates isn't perfect — tiny
currents flow through it, and the transistor gate leaks too. A DRAM cell loses its charge in
milliseconds. So the memory controller periodically reads and writes back every row (typically
every 64ms) just to keep the data alive. This is called **refresh**.

Both SRAM and DRAM need continuous power — but for different reasons. SRAM needs power to keep
its transistor latches active. DRAM needs power to run the refresh circuitry, sense amplifiers,
and memory controller that perform those periodic refresh cycles. At the system level, neither
survives a power loss.

---

## 3. How an SRAM Cell Works

### 3.1 Cross-Coupled Inverters

Unlike DRAM's passive capacitor, SRAM stores data in an active circuit: **two cross-coupled
inverters** built from 4 transistors. The cross-coupling creates a stable feedback loop:

- Inverter A's output feeds inverter B's input
- Inverter B's output feeds inverter A's input

This creates exactly two stable states:

| State     | Node A (connects to BL) | Node B (connects to BL̄) |
|-----------|------------------------|--------------------------|
| Storing 1 | High                   | Low                      |
| Storing 0 | Low                    | High                     |

If node A is high, inverter B sees high input → drives node B low → inverter A sees low input
→ drives node A high. The loop reinforces itself. You would have to force it to flip — it will
not drift on its own. This is why SRAM doesn't need refreshing (as long as power is on).

### 3.2 BL and BL̄ — The Complementary Bit Lines

**BL** is the Bit Line and **BL̄** (pronounced "BL bar" or "negative BL") is the complementary
bit line. They are a pair of wires that carry the cell's value and its inverse:

- Cell stores 1 → BL goes high, BL̄ goes low
- Cell stores 0 → BL goes low, BL̄ goes high

Having both lines lets the sense amplifier compare them **differentially** — it doesn't need
to guess a threshold, it just checks which line is higher. This is faster and more
noise-resistant than a single-ended measurement.

### 3.3 The Access Transistors

The remaining 2 transistors (of the 6 total) are **access gates** — one connecting node A to
BL, one connecting node B to BL̄. The **Word Line (WL)** controls both gates:

```
                    Word Line (WL)
                    │           │
        BL ────── T5           T6 ────── BL̄
                    │           │
                  Node A ─── Node B
                    │    ╲╱    │
                    │    ╱╲    │
                  Inv A ─── Inv B
                  (2 transistors each)
```

- **Read**: WL goes high, gates open, the internal nodes drive BL and BL̄ to complementary
  voltages. The feedback loop is strong enough that this **does not disturb** the stored
  value — unlike DRAM's passive capacitor.
- **Write**: WL goes high, and the external circuitry drives BL and BL̄ hard enough to
  overpower the internal latches and force the cell into the new state.

### 3.4 The Tradeoff

SRAM uses 6 transistors per bit vs. DRAM's 1 transistor + 1 capacitor. This makes SRAM:
- Much larger per bit (less dense)
- More expensive
- But much faster and simpler (no refresh, non-destructive reads)

This is why SRAM is only used for small, fast caches (L1/L2/L3), while DRAM is used for the
large main memory.

---

## 4. DRAM Addressing — RAS and CAS

### 4.1 The 2D Grid

DRAM is organized as a 2D grid of rows and columns. To read a cell, you need both a row
address and a column address. Naively, you would need enough address pins for the full
address, but pins are expensive and take physical space.

The trick: **send the address in two halves over the same pins**.

### 4.2 Multiplexer and Demultiplexer

Two fundamental building blocks are used in DRAM addressing:

**Demultiplexer (DEMUX)**: One input, many outputs. A selection signal (binary number)
determines which output the input gets routed to. In DRAM, the row demultiplexer takes the
row address and activates exactly one of the many row word lines.

**Multiplexer (MUX)**: Many inputs, one output. A selection signal chooses which input to
connect to the output. In DRAM, the column multiplexer selects one specific column from the
row buffer and routes its data to the output pins.

### 4.3 The Two-Step Access

**Step 1 — Row Address Strobe (RAS)**:
1. The memory controller puts the **row address** on the address pins.
2. It pulls the RAS̄ signal low (active-low).
3. Inside the chip, the **row demultiplexer** decodes the address and activates that entire
   row — all capacitors in the row connect to their bit lines and get sensed.
4. The entire row is now sitting in the **row buffer** (the sense amplifiers).

**Step 2 — Column Address Strobe (CAS)**:
1. The controller puts the **column address** on the **same** address pins.
2. It pulls the CAS̄ signal low.
3. The **column multiplexer** selects the specific column(s) from the row buffer and routes
   that data to the output pins.

### 4.4 Simple Example — 6-bit Address, 8×8 Grid

With 64 cells (8 rows × 8 columns), you need a 6-bit address. The upper 3 bits select the
row, the lower 3 bits select the column:

```
Address: 0b110101
         |||   ||
         |||  CAS (lower 3 bits) = 101 = column 5
         RAS (upper 3 bits) = 110 = row 6
```

The grid:

```
          Col 0   Col 1   Col 2   Col 3   Col 4   Col 5   Col 6   Col 7
        +-------+-------+-------+-------+-------+-------+-------+-------+
Row 0   | 0x1A  | 0x3F  | 0x00  | 0xBB  | 0x12  | 0x77  | 0xAA  | 0x01  |
Row 1   | 0x5C  | 0x42  | 0xFE  | 0x09  | 0x8D  | 0x63  | 0x2B  | 0xF0  |
  ...
Row 6   | 0x44  | 0xDE  | 0x91  | 0x07  | 0xBC  | 0xEF  | 0x33  | 0x58  |
Row 7   | ...   | ...   | ...   | ...   | ...   | ...   | ...   | ...   |
        +-------+-------+-------+-------+-------+-------+-------+-------+
```

Reading address `0b110101`:
1. RAS: DEMUX decodes `110` → activates row 6 → all 8 cells dump into the row buffer.
2. CAS: MUX selects column 5 from the row buffer → outputs **0xEF**.

### 4.5 Real-World Example — 1GB DRAM Chip

With 1GB = 2^30 bytes, you need a 30-bit physical address. It gets split into four fields:

```
 29    27 26                  13 12              3 2     0
┌────────┬──────────────────────┬─────────────────┬───────┐
│  BANK  │        ROW           │     COLUMN      │ BURST │
│ 3 bits │      14 bits         │    10 bits       │3 bits │
└────────┴──────────────────────┴─────────────────┴───────┘
 8 banks   16 384 rows/bank      1024 cols/row     8 bytes
                                                   per burst
```

Verification: 2^3 × 2^14 × 2^10 × 2^3 = 2^30 = 1GB.

For a concrete address like `0x0C841A05`:

```
  BANK  = 001              → Bank 1
  ROW   = 10010000100000   → Row 9280
  COLUMN= 0110100000       → Column 416
  BURST = 101              → Byte 5 within 8-byte burst
```

### 4.6 Pin Multiplexing

The chip has only **14 address pins** (enough for the row address, the widest field). The
10-bit column address reuses the same pins:

```
          14 physical address pins on the chip
     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
     │A13│A12│A11│A10│A9 │A8 │A7 │A6 │A5 │A4 │A3 │A2 │A1 │A0 │
     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

     During RAS:  all 14 pins carry row address
     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
     │R13│R12│R11│R10│R9 │R8 │R7 │R6 │R5 │R4 │R3 │R2 │R1 │R0 │
     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

     During CAS:  only 10 pins needed, 4 unused
     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
     │ - │ - │ - │ - │C9 │C8 │C7 │C6 │C5 │C4 │C3 │C2 │C1 │C0 │
     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

     Without multiplexing: 14 + 10 = 24 pins needed
     With multiplexing:    just 14 pins (saves 10 pins!)
```

---

## 5. DRAM Timing Parameters

When accessing DRAM, each step in the RAS/CAS process takes a certain number of **clock
cycles** (not milliseconds — at DDR4-3200, one cycle is ~0.625ns). These delays are reported
in the format **CL-tRCD-tRP-tRAS**, for example CL16-18-20-36.

### 5.1 The Four Main Timings

**tRP (Row Precharge Time) = 20 cycles** — The time to close the currently open row and reset
the bit lines to their reference voltage. You are "clearing the slate" before a new row can be
activated. The old row's write-back finishes, and the bit lines return to neutral.

**tRCD (RAS to CAS Delay) = 18 cycles** — The delay from activating a row (RAS) to when you
can send a column command (CAS). Physically, the row's word line goes high, all capacitors in
the row start sharing charge with their bit lines, and the sense amplifiers need time to detect
and amplify those tiny voltage differences. After tRCD cycles, the row buffer is stable.

**CL (CAS Latency) = 16 cycles** — The delay from sending the CAS command to data appearing
on the output pins. The column multiplexer selects the right sense amplifier, the data travels
through internal buffers to the I/O pins.

**tRAS (Row Active Time) = 36 cycles** — The **minimum time the row must remain open** from
activation. This is not a sequential step — it is a constraint. The sense amplifiers need enough
time to fully restore the charge back into the capacitors (remember, reads are destructive).
You cannot precharge the row until tRAS has elapsed since activation.

### 5.2 Timing Diagram — Single Read

```
Time ──────────────────────────────────────────────────────►

Addr pins:  [ row addr ]              [ col addr ]
RAS:     ────────\______________/──────────────────────
CAS:     ──────────────────────────────\__________/────
Data out:                                         [byte]

         ◄── tRCD ──►                 ◄── CL ──►
```

### 5.3 Page Hit vs. Row Miss

**Page hit** — the row you need is already open in the row buffer. You skip RAS and
precharge entirely, paying only CL:

```
Total = CL = 16 cycles
```

**Row miss** — you need a different row. You must precharge the old row, activate the
new one, then select the column:

```
Cycle   0         20              38              54
        │          │               │               │
        ├──tRP=20──┤───tRCD=18─────┤────CL=16──────┤
        │precharge │ row activating│ column select  │data out
```

```
Total = tRP + tRCD + CL = 20 + 18 + 16 = 54 cycles
```

That is a **3.4× difference** — which is why sequential memory access patterns are so much
faster than random access.

### 5.4 How tRAS Interacts with Other Timings

tRAS counts from row activation, and both tRCD and CL happen within that window:

```
Row activation (tRAS starts here)
  │
  │──tRCD=18──│──CL=16──│
  │                      │
  cycle 0              cycle 34 → data out
  │                              │
  │────────────tRAS=36───────────│
                                 cycle 36 → precharge allowed
```

- tRCD uses 18 cycles of the tRAS budget
- CL uses another 16 cycles
- 18 + 16 = 34, tRAS requires 36
- Only **2 cycles of idle waiting** before precharge can begin

In practice, tRAS is often close to tRCD + CL. The constraint only matters when you want to
precharge immediately after a read.

### 5.5 Page Mode — Multiple Columns from the Same Row

Once a row is open (after RAS), you can issue multiple CAS requests to different columns
cheaply — the row is already in the sense amplifiers:

```
Time ──────────────────────────────────────────────────────────────────►

Addr pins:  [ row addr ]     [col 3] [col 4] [col 5] [col 6]

RAS:     ────────\________________________________________________/────
CAS:     ────────────────────────\__/─────\__/─────\__/─────\__/────────
Data out:                       [0x07]  [0xBC]  [0xEF]  [0x33]

         ◄──── tRCD ────►  ◄CL►  ◄CL►    ◄CL►    ◄CL►
```

Row opened once. Each additional column costs only CL, not the full tRP + tRCD + CL.

### 5.6 Presentation-Ready Diagrams

#### Page Hit

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║                    PAGE HIT — Same Row, Multiple Columns                       ║
║                         CL=16  tRCD=18  tRP=20  tRAS=36                        ║
╠══════════════════════════════════════════════════════════════════════════════════╣
║                                                                                ║
║  Cycle  0              18              34  36  38  40                           ║
║         │               │               │   │   │   │                           ║
║         ▼               ▼               ▼   ▼   ▼   ▼                           ║
║                                                                                ║
║  RAS ───┐_______________________________________________________________       ║
║         │               ▲                                                      ║
║         │          Row is ready                                                ║
║         │                                                                      ║
║  CAS ───────────────────┐__┐──────────────────────────                         ║
║                         │  │  │  │                                             ║
║                        C0  C1 C2 C3   ◄── just 1 cycle apart (burst)           ║
║                         │  │  │  │                                             ║
║  DATA ──────────────────────────────┤D0┤D1┤D2┤D3┤                             ║
║                                                                                ║
║         ├────tRCD=18────┤                                                      ║
║                         ├────CL=16──────┤                                      ║
║                                                                                ║
║  ┌─────────────────────────────────────────────────────────────────────┐        ║
║  │  Total: tRCD(18) + CL(16) = 34 cycles for first word              │        ║
║  │  Then:  1 cycle per additional word (burst mode)                   │        ║
║  │  Row stays open → next read from same row costs only CL            │        ║
║  └─────────────────────────────────────────────────────────────────────┘        ║
╚══════════════════════════════════════════════════════════════════════════════════╝
```

#### Row Miss

```
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                        ROW MISS — Different Row Needed                                 ║
║                           CL=16  tRCD=18  tRP=20  tRAS=36                              ║
╠══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                        ║
║                 OLD ROW ACTIVE                                                         ║
║                        │                                                               ║
║    Phase 1: PRECHARGE  │  Phase 2: ACTIVATE    Phase 3: READ                           ║
║    (close old row)     │  (open new row)       (select column)                         ║
║         tRP=20         │      tRCD=18              CL=16                               ║
║  ◄─────────────────────┼──────────────────►◄──────────────────►                        ║
║                        │                                                               ║
║  Cycle  0             20                  38                  54                        ║
║         │              │                   │                   │                        ║
║         ▼              ▼                   ▼                   ▼                        ║
║                                                                                        ║
║         ┌──────────────┐                                                               ║
║  PRE ───┘  precharging └───────────────────────────────────────────                     ║
║         resetting bit lines                                                            ║
║         to reference voltage                                                           ║
║                                                                                        ║
║                        ┌───────────────────────────────────────────                     ║
║  RAS ──────────────────┘  new row activating                                           ║
║                        │                   ▲                                            ║
║                        │              Row is ready                                     ║
║                                                                                        ║
║                                            ┌──┐                                        ║
║  CAS ─────────────────────────────────────-┘  └───────────────────                     ║
║                                            │                                           ║
║                                           Col                                          ║
║                                            │                                           ║
║  DATA ─────────────────────────────────────────────────────────┤D0┤                    ║
║                                                                                        ║
║         ├─────tRP=20───┤────tRCD=18────────┤──────CL=16───────┤                        ║
║                        ├──────────────tRAS=36─────────────────────┤                    ║
║                                                                   ▲                    ║
║                                                            can precharge               ║
║                                                            again here                  ║
║                                                                                        ║
║  ┌─────────────────────────────────────────────────────────────────────────────┐        ║
║  │  Total: tRP(20) + tRCD(18) + CL(16) = 54 cycles for data                  │        ║
║  │                                                                            │        ║
║  │  Compare:  Page Hit  = 16 cycles    (CL only)                              │        ║
║  │            Row Miss  = 54 cycles    (tRP + tRCD + CL)                      │        ║
║  │            Penalty   = 3.4× slower                                         │        ║
║  └─────────────────────────────────────────────────────────────────────────────┘        ║
╚══════════════════════════════════════════════════════════════════════════════════════════╝
```

#### Side-by-Side Summary

```
╔════════════════════════════╦════════════════════════════╗
║        PAGE HIT            ║        ROW MISS            ║
║     (same row open)        ║   (different row needed)   ║
╠════════════════════════════╬════════════════════════════╣
║                            ║                            ║
║                            ║  ┌─ tRP ──┐  close old    ║
║                            ║  │   20   │  row          ║
║                            ║  └────────┘               ║
║                            ║       │                    ║
║  ┌─ tRCD ─┐  (already     ║  ┌─ tRCD ─┐  activate     ║
║  │  skip  │   open)        ║  │   18   │  new row      ║
║  └────────┘               ║  └────────┘               ║
║       │                    ║       │                    ║
║  ┌─ CL ───┐  select       ║  ┌─ CL ───┐  select       ║
║  │   16   │  column        ║  │   16   │  column       ║
║  └────────┘               ║  └────────┘               ║
║       │                    ║       │                    ║
║      ▼                    ║      ▼                    ║
║  ┌────────┐               ║  ┌────────┐               ║
║  │  DATA  │               ║  │  DATA  │               ║
║  └────────┘               ║  └────────┘               ║
║                            ║                            ║
║  Total: 16 cycles          ║  Total: 54 cycles          ║
║                            ║  (3.4× slower)             ║
╚════════════════════════════╩════════════════════════════╝
```

---

## 6. Inside a DRAM Chip — Bit Arrays

### 6.1 A Cell Stores 1 Bit, Not 1 Byte

The earlier grid examples showed hex values (like `0xEF`) in each cell for simplicity. But the
actual DRAM array is a grid of **individual bits** — each cell is one capacitor storing one bit.

So how does a single chip output a byte? An **x8 chip** (the most common type) contains **8
independent bit-arrays** working in parallel. The same RAS and CAS go to all 8 arrays, each
array contributes 1 bit, and together they produce 1 byte:

```
╔══════════════════════════════════════════════════════════════════╗
║                     One x8 DRAM Chip (inside view)              ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║                  Same RAS + CAS sent to all 8 arrays             ║
║              ┌──────────┬──────────┬──────────┬──────────┐       ║
║              │          │          │          │          │       ║
║              ▼          ▼          ▼          ▼          ▼       ║
║                                                                  ║
║  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐            ║
║  │ Array 0  │ │ Array 1  │ │ Array 2  │ │ Array 3  │            ║
║  │          │ │          │ │          │ │          │            ║
║  │  Each    │ │  Each    │ │  Each    │ │  Each    │            ║
║  │  cell =  │ │  cell =  │ │  cell =  │ │  cell =  │            ║
║  │  1 cap   │ │  1 cap   │ │  1 cap   │ │  1 cap   │            ║
║  │  = 1 bit │ │  = 1 bit │ │  = 1 bit │ │  = 1 bit │            ║
║  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘            ║
║       │            │            │            │                   ║
║     bit 0        bit 1        bit 2        bit 3                 ║
║                                                                  ║
║  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐            ║
║  │ Array 4  │ │ Array 5  │ │ Array 6  │ │ Array 7  │            ║
║  │  1 bit   │ │  1 bit   │ │  1 bit   │ │  1 bit   │            ║
║  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘            ║
║       │            │            │            │                   ║
║     bit 4        bit 5        bit 6        bit 7                 ║
║       │            │            │            │                   ║
║       └────────────┴──────┬─────┴────────────┘                   ║
║                           │                                      ║
║                    ┌──────┴──────┐                                ║
║                    │  8 bits     │                                ║
║                    │  = 1 byte   │                                ║
║                    └──────┬──────┘                                ║
║                           │                                      ║
║                   DQ7 ... DQ0  (8 data pins)                     ║
╚═══════════════════════════╪══════════════════════════════════════╝
                            │
               One chip outputs 1 byte per beat
```

---

## 7. Banks

### 7.1 What Banks Are

Banks are **independent sub-arrays** within a single DRAM chip. Each bank has its own row
decoder, sense amplifiers, and row buffer. A typical DDR4 chip has 8 banks (or 16 with bank
groups).

Critically, each bank contains its own set of 8 bit-arrays (for an x8 chip). So the hierarchy
inside one chip is:

```
One x8 DRAM Chip
├── Bank 0
│    ├── Array 0 (1-bit cells, own rows/cols)
│    ├── Array 1
│    ├── ...
│    └── Array 7
│    └── Own row buffer (sense amplifiers)
│    └── Own row decoder
├── Bank 1
│    ├── Array 0 through Array 7
│    └── Own row buffer, own decoder
├── ...
└── Bank 7
     ├── Array 0 through Array 7
     └── Own row buffer, own decoder

Shared: I/O data bus (8 data pins)
```

```
╔══════════════════════════════════════════════════════════════════╗
║                     One x8 DRAM Chip                            ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  ┌───────────────────────────────────────────────────────────┐   ║
║  │ Bank 0                                                    │   ║
║  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ║
║  │  │Arr 0│ │Arr 1│ │Arr 2│ │Arr 3│ │Arr 4│ │Arr 5│ │Arr 6│ │Arr 7│ ║
║  │  │1-bit│ │1-bit│ │1-bit│ │1-bit│ │1-bit│ │1-bit│ │1-bit│ │1-bit│ ║
║  │  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ ║
║  │     └───────┴───────┴───────┴───┬───┴───────┴───────┴───────┘ ║
║  │                  own row buffer │(sense amps)                  ║
║  │                  own row decoder│                              ║
║  └─────────────────────────────────┼─────────────────────────────┘ ║
║                                    │                                ║
║  ┌─────────────────────────────────┼─────────────────────────────┐ ║
║  │ Bank 1                          │                              │ ║
║  │  8 arrays × 1 bit each         │                              │ ║
║  │  own row buffer, own decoder    │                              │ ║
║  └─────────────────────────────────┼─────────────────────────────┘ ║
║                                   ...                               ║
║  ┌─────────────────────────────────┼─────────────────────────────┐ ║
║  │ Bank 7                          │                              │ ║
║  │  8 arrays × 1 bit each         │                              │ ║
║  │  own row buffer, own decoder    │                              │ ║
║  └─────────────────────────────────┼─────────────────────────────┘ ║
║                                    │                                ║
║                             Shared I/O                              ║
║                             (8 data pins)                           ║
╚════════════════════════════════════╪════════════════════════════════╝
```

### 7.2 Why Banks Exist

Without banks, every memory access would wait for the previous one to fully complete
(precharge, activate, read). With multiple banks you can **pipeline** — start activating a row
in one bank while still reading from another, and precharge a third simultaneously:

```
Time ────────────────────────────────────────────────►

Bank 0:  ┌─RAS──┬──CAS──┬─precharge─┐
Bank 1:  ────────┌─RAS──┬──CAS──┬─precharge─┐
Bank 2:  ────────────────┌─RAS──┬──CAS──┬─precharge─┐

         Operations overlap across banks → higher throughput
```

Each bank can have a different row open. When the memory controller interleaves accesses
across banks, it hides the latency of one bank's precharge/activation behind useful work in
another bank.

---

## 8. The DIMM — Multiple Chips Working Together

### 8.1 From One Chip to a RAM Stick

Everything discussed so far — bit arrays, banks, RAS, CAS, rows, columns — happens inside
**a single DRAM chip**. One x8 chip outputs 1 byte at a time (8 bit-arrays × 1 bit each).

One byte per transfer is too slow. So when you buy a "RAM stick" — a **DIMM** (Dual Inline
Memory Module) — it is a circuit board with **8 DRAM chips** soldered onto it.

### 8.2 How 8 Chips Work in Parallel

The critical idea: **all 8 chips receive the same RAS and CAS commands simultaneously, but
each chip provides a different 8-bit slice of the data.**

```
                    Shared command bus (RAS, CAS, address pins)
          ┌──────────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
          │          │      │      │      │      │      │      │      │
          ▼          ▼      ▼      ▼      ▼      ▼      ▼      ▼      ▼
       ┌──────┐ ┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐
       │Chip 0│ │Chip 1││Chip 2││Chip 3││Chip 4││Chip 5││Chip 6││Chip 7│
       └──┬───┘ └──┬───┘└──┬───┘└──┬───┘└──┬───┘└──┬───┘└──┬───┘└──┬───┘
          │        │       │       │       │       │       │       │
        8 bits   8 bits  8 bits  8 bits  8 bits  8 bits  8 bits  8 bits
          │        │       │       │       │       │       │       │
          └────────┴───────┴───────┴───────┴───────┴───────┴───────┘
                              Combined: 64-bit data bus
                              = 8 bytes per transfer
```

When the memory controller sends one RAS command, all 8 chips activate the same row number in
the same bank. When it sends one CAS command, all 8 chips select the same column number, and
each chip outputs its 1 byte. Together: **8 bytes appear on the bus simultaneously**.

### 8.3 Banks Across Chips — Always in Lockstep

When the controller selects a bank, **all 8 chips activate the same bank**. You never access
bank 3 on chip 0 and bank 5 on chip 2 simultaneously. The chips always work in lockstep:

```
╔═══════════════════════════════════════════════════════════════════════════╗
║  Command: "Bank 3, RAS row 9280, CAS col 416"                          ║
║           sent to ALL chips at once                                     ║
║                                                                         ║
║  ┌──────────┐ ┌──────────┐ ┌──────────┐         ┌──────────┐           ║
║  │  Chip 0  │ │  Chip 1  │ │  Chip 2  │   ...   │  Chip 7  │           ║
║  │          │ │          │ │          │         │          │           ║
║  │ Bank 0   │ │ Bank 0   │ │ Bank 0   │         │ Bank 0   │           ║
║  │ Bank 1   │ │ Bank 1   │ │ Bank 1   │         │ Bank 1   │           ║
║  │ Bank 2   │ │ Bank 2   │ │ Bank 2   │         │ Bank 2   │           ║
║  │►Bank 3 ◄ │ │►Bank 3 ◄ │ │►Bank 3 ◄ │         │►Bank 3 ◄ │           ║
║  │ Bank 4   │ │ Bank 4   │ │ Bank 4   │         │ Bank 4   │           ║
║  │ Bank 5   │ │ Bank 5   │ │ Bank 5   │         │ Bank 5   │           ║
║  │ Bank 6   │ │ Bank 6   │ │ Bank 6   │         │ Bank 6   │           ║
║  │ Bank 7   │ │ Bank 7   │ │ Bank 7   │         │ Bank 7   │           ║
║  └────┬─────┘ └────┬─────┘ └────┬─────┘         └────┬─────┘           ║
║       │            │            │                    │                  ║
║    1 byte       1 byte       1 byte              1 byte                ║
║    (8 bits      (8 bits      (8 bits             (8 bits               ║
║    from bank3   from bank3   from bank3          from bank3            ║
║    arrays)      arrays)      arrays)             arrays)               ║
║       │            │            │                    │                  ║
║       └────────────┴──────┬─────┴────────────────────┘                  ║
║                           │                                             ║
║                    8 bytes per beat                                     ║
║                    × 8 beats (burst)                                    ║
║                    = 64 bytes                                           ║
╚═══════════════════════════╪═════════════════════════════════════════════╝
```

### 8.4 The Complete Hierarchy

From a single bit-cell all the way to a cache line, there are three levels of parallelism:

```
DIMM (RAM stick)
 └── 8 chips (work in parallel, each gives 1 byte)
      └── each chip has 8 banks (can pipeline across banks)
           └── each bank has 8 bit-arrays (work in parallel, each gives 1 bit)
                └── each array is a 2D grid of 1-bit cells (capacitor + transistor)
```

```
╔═════════════════════════════════════════════════════════════════════╗
║  Three levels of parallelism: bit-cell → cache line               ║
╠═════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LEVEL 1 — inside each chip:    8 arrays × 1 bit   =  1 byte      ║
║                                                                    ║
║  LEVEL 2 — across the DIMM:     8 chips  × 1 byte  =  8 bytes     ║
║                                          (one "beat")              ║
║                                                                    ║
║  LEVEL 3 — burst mode:          8 beats  × 8 bytes  = 64 bytes    ║
║                                          = 1 cache line            ║
║                                                                    ║
║  Commands sent: 1 × RAS + 1 × CAS                                 ║
║  Data received: 64 bytes                                           ║
╚═════════════════════════════════════════════════════════════════════╝
```

---

## 9. Burst Mode and DDR

### 9.1 Auto-Incrementing Column Counter

When the memory controller issues **one CAS command**, it doesn't just read one column. The
chip has an internal column counter that **automatically increments** and reads the next
columns without any additional commands from the controller. In DDR4, the burst length is
fixed at 8 — meaning one CAS triggers 8 consecutive column reads.

### 9.2 DDR — Double Data Rate

DDR stands for **Double Data Rate**. Data is transferred on **both** the rising and falling
edges of the clock, not just one edge. So 8 transfers happen over 4 clock cycles:

```
One CAS command triggers 8 beats (auto-increment inside the chip):

Clock:    ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
            └──┘  └──┘  └──┘  └──┘
             ↑↓   ↑↓   ↑↓   ↑↓        ← transfers on BOTH edges
Beat:        0 1  2 3  4 5  6 7        ← 8 beats in 4 clock cycles

What ONE chip does internally:

  Row buffer: ... │C416│C417│C418│C419│C420│C421│C422│C423│ ...
                     │     │     │     │     │     │     │     │
  Beat:              0     1     2     3     4     5     6     7
  Auto-increment:  CAS  CAS+1 CAS+2 CAS+3 CAS+4 CAS+5 CAS+6 CAS+7
                    ▲
               only this one was
               sent by controller,
               rest are automatic
```

### 9.3 Complete Picture — One CAS → 64 Bytes

Combining 8 chips in parallel with 8 burst beats:

```
╔═══════════════════════════════════════════════════════════════════════════╗
║  1 CAS command, 8 chips, 8 beats (auto-increment), DDR                  ║
╠═══════════════════════════════════════════════════════════════════════════╣
║                                                                         ║
║  Beat 0 (clock rising edge):                                            ║
║  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐              ║
║  │Chip 0│Chip 1│Chip 2│Chip 3│Chip 4│Chip 5│Chip 6│Chip 7│  = 8 bytes  ║
║  │col416│col416│col416│col416│col416│col416│col416│col416│              ║
║  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘              ║
║                                                                         ║
║  Beat 1 (clock falling edge):                                           ║
║  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐              ║
║  │Chip 0│Chip 1│Chip 2│Chip 3│Chip 4│Chip 5│Chip 6│Chip 7│  = 8 bytes  ║
║  │col417│col417│col417│col417│col417│col417│col417│col417│              ║
║  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘              ║
║                                                                         ║
║  Beat 2: all chips col 418                                   = 8 bytes  ║
║  Beat 3: all chips col 419                                   = 8 bytes  ║
║  Beat 4: all chips col 420                                   = 8 bytes  ║
║  Beat 5: all chips col 421                                   = 8 bytes  ║
║  Beat 6: all chips col 422                                   = 8 bytes  ║
║  Beat 7: all chips col 423                                   = 8 bytes  ║
║                                                                         ║
║  Total: 8 beats × 8 bytes = 64 bytes = 1 cache line                    ║
║                                                                         ║
║  Commands sent by controller: 1 RAS + 1 CAS                            ║
║  Clock cycles used for data:  4 (DDR = 2 beats per cycle)              ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

---

## 10. Cache Lines

### 10.1 What a Cache Line Is

A cache line (64 bytes on modern x86) is the **smallest unit of data transfer between main
memory and the CPU cache**. It is not an arbitrary choice — it is the natural unit that falls
out of one DRAM burst:

```
1 burst = 8 beats × 8 chips × 1 byte = 64 bytes = 1 cache line
```

The CPU cache never fetches individual bytes from memory. It always fetches in 64-byte chunks
because that is what one burst delivers. Fetching less would waste the burst. Fetching more
would require additional CAS commands.

### 10.2 Why This Matters for Your Code

When your program reads a single `int` at some address, the memory controller fetches the
**entire 64-byte cache line** containing that int:

```
You read address 0x1000 (a 4-byte int):

Cache line fetched: addresses 0x0FC0 - 0x0FFF  (64 bytes, aligned)

┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬─────┬─────┐
│0FC0│0FC4│0FC8│0FCC│0FD0│ ...│0FEC│0FF0│0FF4│0FF8│0FFC │     │
└────┴────┴────┴────┴────┴────┴────┴────┴──▲─┴────┴─────┴─────┘
                                           │
                                     your int here
                                     but ALL 64 bytes
                                     are now in L1 cache
```

Accessing nearby memory right after is effectively free — it is already in the cache from the
same burst. This is the hardware foundation behind why **sequential access patterns are fast
and random access patterns are slow**.

### 10.3 Reading a 4KB Page

A 4KB OS page is **not** read in a single DRAM operation. It is 64 cache lines, each fetched
on demand:

```
4KB page = 4096 bytes = 64 × 64-byte cache lines
```

However, a DRAM row buffer is typically 8KB (1024 columns × 8 chips × 1 byte), which is
larger than a 4KB OS page. So if a 4KB page maps to a single DRAM row, all 64 cache lines
are **page hits** — the row is opened once and the controller issues 64 CAS commands:

```
          ┌─RAS─────────────────────────────────────────────┐
          │    tRCD                                         │
Row open: ─────┤                                             │
               │                                             │
CAS:           ├CAS─┤CAS─┤CAS─┤CAS─┤ ... ├CAS─┤           │
               │ L0 │ L1 │ L2 │ L3 │     │L63 │           │
               │                                             │
          1 × tRCD + 64 × CL                                │
          vs                                                 │
          64 × (tRP + tRCD + CL)  ← if each were a row miss │
```

This is why the OS and hardware work together to keep frequently accessed data within the same
DRAM row whenever possible.

---

## 11. DDR Generations

The fundamental DRAM principles — RAS, CAS, rows, columns, sense amplifiers, destructive
reads, refresh — have not changed across generations. Those are physics-level properties of
DRAM cells. What changes is how the chip is organized and how fast data moves.

### 11.1 Generation Comparison

| | DDR2 | DDR3 | DDR4 | DDR5 |
|---|---|---|---|---|
| **Prefetch** | 4n | 8n | 8n | 16n |
| **Burst length** | 4 | 8 | 8 | 16 (or 2×8) |
| **Voltage** | 1.8V | 1.5V | 1.2V | 1.1V |
| **Banks** | 4-8 | 8 | 16 | 32 |
| **Bank groups** | no | no | yes (4) | yes (8) |
| **Channels/DIMM** | 1 | 1 | 1 | 2 |
| **Typical speed** | 800 MT/s | 1600 MT/s | 3200 MT/s | 4800-6400 MT/s |

### 11.2 Key Changes Across Generations

**Prefetch width keeps growing** — In DDR2, one CAS prefetched 4 columns internally (4n
prefetch). DDR4 does 8n. DDR5 does 16n. The chip reads more columns at once from the row
buffer into an internal I/O buffer, then streams them out over the data pins. More prefetch =
higher burst throughput without making the DRAM array itself faster. The array speed has barely
changed across generations — the speedup is in the I/O path.

**DDR5 splits the DIMM into two independent channels** — DDR4 has one 64-bit channel across
all 8 chips. DDR5 splits that into two 32-bit channels, each with its own command/address bus:

```
DDR4 DIMM:
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│Chip 0│Chip 1│Chip 2│Chip 3│Chip 4│Chip 5│Chip 6│Chip 7│
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
└──────────────── one 64-bit channel ───────────────────┘
  One command at a time for all 8 chips


DDR5 DIMM:
┌──────┬──────┬──────┬──────┐  ┌──────┬──────┬──────┬──────┐
│Chip 0│Chip 1│Chip 2│Chip 3│  │Chip 4│Chip 5│Chip 6│Chip 7│
└──────┴──────┴──────┴──────┘  └──────┴──────┴──────┴──────┘
└──── Channel A (32-bit) ────┘  └──── Channel B (32-bit) ────┘
  Independent commands!           Independent commands!
```

Each channel can issue its own RAS and CAS independently. Two different memory requests can be
served simultaneously — better for multi-core CPUs where different cores want different data.
The burst math still works: 4 chips × 1 byte × 16 beats = 64 bytes = 1 cache line per channel.

**More banks and bank groups** — DDR5 has 32 banks (vs DDR4's 16). More banks = more rows can
be open simultaneously = more opportunities to pipeline and hide latency. Bank groups add
another layer — banks within different groups can be accessed with tighter timing than banks in
the same group.

### 11.3 The Core Lesson

The DRAM array itself hasn't gotten much faster across generations. Each generation mostly
improves the I/O path (wider prefetch, more parallelism, lower voltage for higher clocks). The
latency of opening a row and sensing the capacitors is still roughly the same in nanoseconds.
That is why caches and access patterns matter as much today as they did when the paper was
written.

---

## 12. CPU Cache Hierarchy

### 12.1 Multi-Processor vs. Multi-Core

The paper (written 2007) discusses multi-processor systems — multiple physical CPU packages
(sockets) on the motherboard. Most consumer machines today are **single-socket, multi-core**:
one physical chip containing many cores.

### 12.2 Cache Levels

Modern CPUs have a hierarchy of SRAM caches between the cores and main memory (DRAM). The
general pattern is:

- **L1** — private per core, split into L1d (data) and L1i (instructions). Fastest, smallest.
- **L2** — private per core (or per small cluster). Larger, slightly slower.
- **L3** — shared across all cores. Largest cache, slowest of the three.

The exact layout depends on the specific CPU. Different architectures make different choices
about what is private vs. shared.

### 12.3 Example — Intel Core i7-14700K

This is a modern hybrid architecture with two types of cores:

- **8 Performance cores (P-cores)**: larger, faster, hyperthreaded (2 threads each)
- **12 Efficiency cores (E-cores)**: smaller, power-efficient, no hyperthreading, grouped in
  clusters of 4

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║                    Intel Core i7-14700K — Cache Hierarchy                      ║
║                    1 Socket, 20 Cores, 28 Threads                              ║
╠══════════════════════════════════════════════════════════════════════════════════╣
║                                                                                ║
║  8 Performance Cores (P-cores, hyperthreaded)                                  ║
║  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐                              ║
║  │ P-Core 0│ │ P-Core 1│ │ P-Core 2│ │ P-Core 3│  ... (×8 total)              ║
║  │ 2 thrds │ │ 2 thrds │ │ 2 thrds │ │ 2 thrds │                              ║
║  ├─────────┤ ├─────────┤ ├─────────┤ ├─────────┤                              ║
║  │L1d: 48K │ │L1d: 48K │ │L1d: 48K │ │L1d: 48K │  ← private per core         ║
║  │L1i: 64K │ │L1i: 64K │ │L1i: 64K │ │L1i: 64K │                              ║
║  ├─────────┤ ├─────────┤ ├─────────┤ ├─────────┤                              ║
║  │ L2: 2MB │ │ L2: 2MB │ │ L2: 2MB │ │ L2: 2MB │  ← private per core         ║
║  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘                              ║
║       │           │           │           │                                    ║
║  ─────┴───────────┴───────────┴───────────┴────────────────────────────────    ║
║                                                                                ║
║  12 Efficiency Cores (E-cores, 3 clusters of 4)                                ║
║  ┌──────────────────────────┐ ┌──────────────────────────┐                     ║
║  │  E-core Cluster 1        │ │  E-core Cluster 2        │  Cluster 3 ...      ║
║  │  ┌────┐┌────┐┌────┐┌────┐│ │  ┌────┐┌────┐┌────┐┌────┐│                     ║
║  │  │ E0 ││ E1 ││ E2 ││ E3 ││ │  │ E4 ││ E5 ││ E6 ││ E7 ││                     ║
║  │  │ 32K││ 32K││ 32K││ 32K││ │  │ 32K││ 32K││ 32K││ 32K││  ← L1 private      ║
║  │  └──┬─┘└──┬─┘└──┬─┘└──┬─┘│ │  └──┬─┘└──┬─┘└──┬─┘└──┬─┘│                     ║
║  │     └─────┴──┬──┴─────┘  │ │     └─────┴──┬──┴─────┘  │                     ║
║  │     L2: 4MB (shared by 4)│ │     L2: 4MB (shared by 4)│  ← L2 per cluster  ║
║  └────────────┬─────────────┘ └────────────┬─────────────┘                     ║
║               │                            │                                   ║
║  ─────────────┴────────────────────────────┴───────────────────────────────    ║
║                                                                                ║
║  ┌──────────────────────────────────────────────────────────────────────────┐   ║
║  │                   L3 Cache: 33 MB (shared by ALL 20 cores)              │   ║
║  └──────────────────────────────────────────────────────────────────────────┘   ║
║                                      │                                         ║
║  ┌──────────────────────────────────────────────────────────────────────────┐   ║
║  │                   Memory Controller → DRAM                              │   ║
║  └──────────────────────────────────────────────────────────────────────────┘   ║
║                                                                                ║
║  ┌──────────────────────────────────────────────────────────────────────┐       ║
║  │  Summary:                                                            │       ║
║  │  P-core: L1d=48KB L1i=64KB  L2=2MB    ← all private per core        │       ║
║  │  E-core: L1d=32KB L1i=32KB  L2=4MB    ← L1 private, L2 per cluster  │       ║
║  │  All:    L3=33MB                       ← shared across all 20 cores  │       ║
║  │                                                                      │       ║
║  │  8 P-cores × 2 threads = 16 threads                                 │       ║
║  │  12 E-cores × 1 thread = 12 threads                                 │       ║
║  │  Total: 20 cores, 28 threads                                        │       ║
║  └──────────────────────────────────────────────────────────────────────┘       ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

The paper's description of "L1 private, everything else shared" was a simplification for 2007
hardware. Modern CPUs have L1 **and** L2 private (per core or per small cluster), with only
L3 truly shared. L3 is where data goes when cores need to communicate through the cache
hierarchy.

---

## 13. Cache Tagging — How the Cache Finds Your Data

### 13.1 The Problem

The CPU cache holds copies of some cache lines from main memory. It is small (e.g., 48KB for
L1d) compared to the gigabytes of main memory. When the CPU accesses an address, the cache
must answer two questions:

1. **Is this address's data in the cache?**
2. **If yes, where exactly in the cache?**

Searching the entire cache for every access would be far too slow. Instead, the cache uses a
system of **tagging** that allows it to look in exactly the right place immediately.

### 13.2 Address Sizes — Why Not 64 Bits?

CPUs today are called "64-bit" but that refers to the **register width** — how wide the
integers and pointers are. The actual address bus is narrower because nobody needs 2^64 bytes
of addressable memory (that would be 16 exabytes).

There are two address sizes that matter:

- **Virtual address** — the address your program sees. On x86-64, this is 48 bits (256 TB).
- **Physical address** — the actual wires going to memory. On the i7-14700K, this is 46 bits
  (64 TB max physical RAM).

The upper bits of a 64-bit pointer are unused (they must be sign-extended copies of bit 47,
or the CPU faults). This is why you see addresses like `0x00007FFF_12345678` — the top 16
bits are always `0x0000` or `0xFFFF`.

The cache tag uses the **physical address** (after virtual-to-physical translation by the TLB,
described in section 13.8).

### 13.3 Splitting the Address Into Three Fields

Every physical address is split into three fields that the cache uses for lookup:

```
Physical address (46 bits on i7-14700K):

 45                    12 11        6 5       0
┌────────────────────────┬──────────┬─────────┐
│         TAG            │   SET    │  OFFSET  │
│       34 bits          │  6 bits  │  6 bits  │
└────────────────────────┴──────────┴─────────┘
```

(This example is for the L1d: 48KB, 64-byte lines, 12-way, giving 64 sets.)

**Offset (6 bits)** — Which byte within the 64-byte cache line. 2^6 = 64, so 6 bits can
address any byte inside a line. The cache always fetches the whole 64-byte line; the offset
just selects which byte you want from it.

**Set (6 bits)** — Which "row" or "slot group" in the cache this address maps to. This is
computed directly from the address bits — no searching needed. It tells the cache exactly
**where to look**. The number of set bits depends on how many sets the cache has.

**Tag (remaining bits)** — The identity proof. Many different addresses map to the same set
(because there are billions of addresses but only 64 sets). The tag is the part that
distinguishes which actual address is stored there. It answers: "is this really *my* data, or
just some other address that happened to land in the same set?"

### 13.4 How Many Sets?

The number of sets is determined by the cache geometry:

```
Sets = Cache Size / (Line Size × Ways)
```

For the i7-14700K:

```
L1d:  48 KB / (64 B × 12 ways) =   64 sets →  6 set bits
L2:    2 MB / (64 B × 16 ways) = 2048 sets → 11 set bits
```

More sets = fewer addresses competing for the same set = less chance of eviction conflicts.

### 13.5 Direct-Mapped Cache (Simplest Case)

To understand tagging, start with the simplest cache: **direct-mapped** (1 way per set).
Each address maps to exactly one place in the cache — there is no choice.

Imagine a tiny cache with 4 sets. Multiple addresses map to the same set, distinguished only
by their tags:

```
Address 0x0000 ──┐
Address 0x0100 ──┼──► Set 0  (same set! different tags)
Address 0x0200 ──┘

Address 0x0040 ──┐
Address 0x0140 ──┼──► Set 1
Address 0x0240 ──┘
```

The cache stores one tag and one data line per set:

```
┌─────┬───────────┬────────────────────────────────────────┐
│ Set │    Tag     │              Data (64 bytes)           │
├─────┼───────────┼────────────────────────────────────────┤
│  0  │ 0x00002   │ [cache line contents...]               │
│  1  │ 0x00005   │ [cache line contents...]               │
│  2  │ 0x00001   │ [cache line contents...]               │
│  3  │ 0x00003   │ [cache line contents...]               │
└─────┴───────────┴────────────────────────────────────────┘
```

When the CPU reads address `0x01A3`:

```
Address:  0x01A3

Split (4 sets = 2 set bits, 64-byte lines = 6 offset bits):

  TAG          SET    OFFSET
  0000000001   10     100011
  = 0x01       = 2    = byte 35 within the line

Step 1: Go to set 2
Step 2: Compare stored tag (0x00001) with our tag (0x00001)
Step 3: Match! → cache HIT → return byte 35 from that line
```

If the tags don't match → **cache miss** → fetch the line from memory, store it in set 2
(evicting whatever was there), and write the new tag.

The problem with direct-mapped caches: if two frequently-used addresses happen to map to the
same set, they keep evicting each other endlessly. This is called **thrashing**.

### 13.6 Set-Associative Cache (What Real CPUs Use)

To reduce thrashing, each set holds **multiple lines** called **ways**. A 12-way
set-associative cache (like the i7-14700K's L1d) stores up to 12 different cache lines in
each set:

```
Set 0:
┌──────┬──────────┬──────────────────────┐
│ Way  │   Tag    │    Data (64 bytes)   │
├──────┼──────────┼──────────────────────┤
│  0   │ 0xA003F  │ [line contents...]   │
│  1   │ 0xB107C  │ [line contents...]   │
│  2   │ 0x00002  │ [line contents...]   │
│  3   │ 0xFFF01  │ [line contents...]   │
│ ...  │   ...    │        ...           │
│  11  │ 0x88812  │ [line contents...]   │
└──────┴──────────┴──────────────────────┘
  12 ways = 12 lines can live in the same set
```

The full lookup process:

```
╔═══════════════════════════════════════════════════════════════════════╗
║                    Cache Lookup — Set-Associative                    ║
╠═══════════════════════════════════════════════════════════════════════╣
║                                                                     ║
║  Address: 0x.......                                                 ║
║           ┌──────────────┬───────┬────────┐                         ║
║           │     TAG      │  SET  │ OFFSET │                         ║
║           └──────┬───────┴───┬───┴────┬───┘                         ║
║                  │           │        │                              ║
║                  │     ┌─────┘        └──── selects byte within     ║
║                  │     │                    the 64-byte line        ║
║                  │     ▼                                            ║
║                  │   Set N:                                         ║
║                  │   ┌──────────┬──────┐                            ║
║                  ├──►│ Tag == ? │ Way 0│──┐                         ║
║                  ├──►│ Tag == ? │ Way 1│──┤                         ║
║                  ├──►│ Tag == ? │ Way 2│──┤                         ║
║                  ├──►│ Tag == ? │ Way 3│──┤  all compared           ║
║                  │   │  ...     │ ...  │  │  IN PARALLEL            ║
║                  ├──►│ Tag == ? │Way 11│──┤                         ║
║                  │   └──────────┴──────┘  │                         ║
║                  │                        ▼                         ║
║                  │              ┌──────────────────┐                ║
║                  │              │ Any tag matched?  │                ║
║                  │              └────┬─────────┬───┘                ║
║                  │                   │         │                    ║
║                  │                  YES        NO                   ║
║                  │                   │         │                    ║
║                  │              Cache HIT   Cache MISS              ║
║                  │              return data  fetch from             ║
║                  │              at offset    memory, pick           ║
║                  │                           a way to evict         ║
╚══════════════════╪══════════════════════════════════════════════════╝
```

On a miss, the cache must **evict** one of the 12 ways to make room. It typically chooses the
**least recently used (LRU)** way — the one that hasn't been accessed for the longest time.

### 13.7 The Associativity Tradeoff

```
Direct-mapped (1-way):   fast lookup, lots of thrashing
Fully associative:        no thrashing, slow lookup (must compare ALL tags)
N-way set-associative:    middle ground — check N tags per lookup
```

A fully associative cache has just 1 set — any line can go anywhere. No thrashing, but every
lookup must compare the tag against every single line in the cache. This is too slow and
power-hungry for large caches.

N-way set-associative is the practical compromise. The L1d being 12-way means: for any memory
address, there are exactly 12 possible places it can live in the cache. Enough to avoid most
thrashing, few enough that parallel tag comparison is still fast in hardware.

### 13.8 Virtual vs. Physical Addresses and the TLB

Programs use **virtual addresses**, but the cache tags need **physical addresses** (because
two programs might use the same virtual address for different physical memory). Something must
translate between them. This is the job of the **TLB** (Translation Lookaside Buffer).

**Virtual memory** is a system where each program thinks it has its own private address space.
The OS divides memory into **pages** (typically 4KB). A page table maps virtual page numbers
to physical page numbers. The TLB is a small, fast cache of recent page table translations.

For any address, the bottom 12 bits (the page offset for 4KB pages) are the same in both the
virtual and physical address — only the page number part gets translated:

```
Virtual address (48 bits):
┌──────────────────────────────────┬────────────┐
│     Virtual page number          │ Page offset │
│         36 bits                  │   12 bits   │
└──────────────────────────────────┴────────────┘
                 │                        │
            TLB translates               same
                 │                        │
                 ▼                        ▼
┌──────────────────────────────────┬────────────┐
│     Physical page number         │ Page offset │
│         34 bits                  │   12 bits   │
└──────────────────────────────────┴────────────┘
Physical address (46 bits)
```

Modern L1 caches exploit this with a trick called **VIPT** (Virtually Indexed, Physically
Tagged): the set index bits (bits 6-11 for a 64-set cache) fall entirely within the 12-bit
page offset, which is **the same in both virtual and physical addresses**. This means the
cache can start the set lookup using the virtual address (available immediately) while the
TLB translates the upper bits for the tag comparison in parallel:

```
  45                       12 11      6 5      0
  ┌─────────────────────────┬─────────┬────────┐
  │          TAG             │   SET   │ OFFSET │
  │   needs TLB translation  │ same in │ same   │
  │   (physical page number) │ virtual │        │
  │                          │ and     │        │
  │                          │physical │        │
  └─────────────────────────┴─────────┴────────┘
                                 │
                          ◄─────┘
                    Can start set lookup
                    BEFORE TLB finishes!
```

This is why L1 caches are carefully sized so that set + offset bits don't exceed the page
offset size (12 bits for 4KB pages). If they did, the set lookup would need the physical
address and would have to wait for the TLB — making every L1 access slower.
