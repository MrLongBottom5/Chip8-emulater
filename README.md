# CHIP-8 Emulator

A high-performance, spec-accurate CHIP-8 interpreter built from scratch in **C99** using the **Raylib** framework. This implementation passes modern strict hardware validation suites, accurately replicating the execution characteristics, logic flags, and historical memory indexing quirks of the original COSMAC VIP architecture.

## Features

* **Complete Instruction Set:** 100% core opcode coverage (all 35 standard instructions implemented, including `CXNN` random numbering and `BNNN` offset jumps).
* **Validation Suite Passed:** Successfully passes Timendus's rigorous hardware validation suite, aligning logic operations, bit-shifts, and subtraction flags to authentic specifications.
* **Procedural Sound Engine:** Features a dynamically synthesized 440Hz sine-wave audio generator utilizing Raylib's raw audio stream buffer—no external asset files required.
* **Robust Safety Layer:** Out-of-bounds memory guards, automatic screen-wrap bounds verification, and explicit hardware stack boundary protection against overflows/underflows.

---

## Hardware Configuration (Quirks Profile)

To match modern expectations while passing low-level test ROMs, the interpreter runs on the classic structural profile:

| Feature Flag | Mode | Behavioral Profile |
| :--- | :--- | :--- |
| **`VF RESET`** | `ON` | Logical operations (AND, OR, XOR) explicitly reset the V_F register to 0. |
| **`MEMORY`** | `ON` | `FX55` and `FX65` instructions increment the I register step-by-step (`I++`). |
| **`SHIFTING`** | `OFF` | `8XY6` and `8XYE` instructions shift the value stored inside V_Y into V_X. |
| **`CLIPPING`** | `OFF` | Render engine gracefully wraps sprite arrays mathematically across display boundaries. |

---

## Controls Mapping

The traditional 4x4 hex keypad layout is fully mapped to intuitive keyboard alignments:

```text
Original Hex Pad         Modern Keyboard Layout
+---+---+---+---+        +---+---+---+---+
| 1 | 2 | 3 | C |        | 1 | 2 | 3 | 4 |
+---+---+---+---+        +---+---+---+---+
| 4 | 5 | 6 | D |  --->  | Q | W | E | R |
+---+---+---+---+        +---+---+---+---+
| 7 | 8 | 9 | E |        | A | S | D | F |
+---+---+---+---+        +---+---+---+---+
| A | 0 | B | F |        | Z | X | C | V |
+---+---+---+---+        +---+---+---+---+
