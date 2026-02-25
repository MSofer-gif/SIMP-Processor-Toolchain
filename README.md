# SIMP Processor Toolchain

## Overview / The Task
This project involves building a complete software toolchain—an Assembler and a Simulator—for a custom processor architecture named "SIMP". Alongside the toolchain, we were tasked with developing four distinct assembly programs to test and demonstrate the processor's full capabilities, including its I/O peripherals, memory management, and interrupt handling. 

## How We Implemented It

### 1. The Assembler (`asm.c`)
The assembler translates SIMP assembly source code into machine code (`memin.txt`). 
* We implemented a classic two-pass algorithm. 
* **Pass 1:** Scans the code to collect labels and builds the symbol table.
* **Pass 2:** Generates the final machine code by resolving addresses.
* The parsing logic carefully distinguishes between R-format instructions and I-format instructions (which utilize the `$imm` register for immediate values) to accurately advance the program counter.

### 2. The Simulator (`sim.c`)
The simulator acts as the virtual machine, running a continuous Fetch-Decode-Execute loop. 
* **Hardware Simulation:** It simulates a 4096-word memory array, 16 general-purpose registers, and 23 specific I/O registers.
* **Interrupt Controller:** We implemented a robust interrupt system evaluating IRQ signals at the start of each cycle. It handles Timer events (IRQ0), Disk DMA completion (IRQ1), and External timing events (IRQ2) without allowing nested interrupts.
* **Peripherals:** The system supports Direct Memory Access (DMA) for the simulated hard disk (requiring a 1024-cycle delay) and controls a 256x256 pixel monitor through memory-mapped I/O.

### 3. Assembly Programs
To prove the architecture's correctness, we wrote the following programs from scratch:
* **`sort.asm`:** A Bubble Sort algorithm sorting an array of 16 integers.
* **`binom.asm`:** Computes the Binomial Coefficient recursively, demonstrating manual stack frame management and function calls.
* **`triangle.asm`:** Draws a filled right triangle on the monitor using a scanline filling algorithm and division-by-subtraction.
* **`disktest.asm`:** Reads sectors from the disk using DMA polling, computes element-wise sums, and writes the result back to a new sector.

## Full Documentation
For an in-depth look at the architecture, memory layout, and implementation challenges (such as handling DMA timing and two-pass label resolution), please refer to the full project documentation:
📄 **[SIMP Project Documentation PDF](link_to_pdf_here)**
