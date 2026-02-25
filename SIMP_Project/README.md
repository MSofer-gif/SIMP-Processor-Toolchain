# SIMP Processor Toolchain

## Overview
This project implements a complete custom toolchain for a hypothetical processor called "SIMP". The project was developed as part of the Computer Architecture course and consists of an Assembler, a fully functional Simulator, and several Assembly test programs that demonstrate the processor's capabilities.

## Architecture

The system operates as a pipeline:
1. **Assembler (`asm.c`)**: Translates SIMP assembly source code into machine code (`memin.txt`) using a classic two-pass algorithm. It handles label resolution, parses immediate values, and distinguishes between R-format and I-format instructions.
2. **Simulator (`sim.c`)**: A virtual machine that loads the generated machine code and executes it cycle-by-cycle using a Fetch-Decode-Execute loop. 

### Simulator Features:
* **Memory & Registers**: Simulates a 4096-word memory array, 16 general-purpose registers, and 23 I/O registers.
* **Interrupts**: Supports Timer (IRQ0), Disk DMA completion (IRQ1), and External (IRQ2) interrupts.
* **I/O Peripherals**: Simulates a Hard Disk (via DMA transfers), a 256x256 pixel Monitor (frame buffer), LEDs, and a 7-segment display.
* **Trace Generation**: Outputs detailed state files reflecting memory, registers, and hardware status after execution.

## Assembly Programs Included
To test the processor, we wrote the following programs in SIMP Assembly:
* `sort.asm`: A Bubble Sort implementation for an array of 16 integers.
* `binom.asm`: Calculates the Binomial Coefficient recursively, demonstrating function calls and stack management.
* `triangle.asm`: Draws a filled right triangle on the simulated monitor using scanline filling and division-by-subtraction.
* `disktest.asm`: Reads multiple sectors from the simulated disk using Direct Memory Access (DMA), processes the data, and writes the results back.

## Usage

### 1. Compile the Tools
Compile the assembler and simulator using GCC (or any standard C compiler):
```bash
gcc -o asm asm.c
gcc -o sim sim.c