/*
 * SIMP Processor Simulator
 * Cycle-accurate simulator for 20-bit SIMP processor with interrupts, disk DMA, and monitor
 * Usage: sim memin.txt diskin.txt irq2in.txt memout.txt regout.txt trace.txt
 *        hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>   /* fopen, fclose, fgets, fprintf, fwrite, printf */
#include <stdlib.h>  /* strtol, atoi, malloc, realloc, free */
#include <string.h>  /* memset */
#include <stdint.h>  /* int32_t, uint32_t, uint8_t */

/* Section 1: Constants */

#define MEMORY_SIZE         4096
#define DISK_SECTORS        128
#define SECTOR_SIZE         128
#define DISK_SIZE           (DISK_SECTORS * SECTOR_SIZE)
#define MONITOR_SIZE        (256 * 256)
#define DISK_DELAY          1024  /* disk ops take 1024 cycles */
#define NUM_REGS            16
#define NUM_IO_REGS         23

/* Opcodes 0-21 */
#define OP_ADD      0
#define OP_SUB      1
#define OP_MUL      2
#define OP_AND      3
#define OP_OR       4
#define OP_XOR      5
#define OP_SLL      6
#define OP_SRA      7
#define OP_SRL      8
#define OP_BEQ      9
#define OP_BNE      10
#define OP_BLT      11
#define OP_BGT      12
#define OP_BLE      13
#define OP_BGE      14
#define OP_JAL      15
#define OP_LW       16
#define OP_SW       17
#define OP_RETI     18
#define OP_IN       19
#define OP_OUT      20
#define OP_HALT     21

/* Register indices - only need special ones */
#define REG_ZERO    0  /* always 0 */
#define REG_IMM     1  /* immediate value */

/* I/O register indices */
#define IO_IRQ0ENABLE       0
#define IO_IRQ1ENABLE       1
#define IO_IRQ2ENABLE       2
#define IO_IRQ0STATUS       3
#define IO_IRQ1STATUS       4
#define IO_IRQ2STATUS       5
#define IO_IRQHANDLER       6
#define IO_IRQRETURN        7
#define IO_CLKS             8
#define IO_LEDS             9
#define IO_DISPLAY7SEG      10
#define IO_TIMERENABLE      11
#define IO_TIMERCURRENT     12
#define IO_TIMERMAX         13
#define IO_DISKCMD          14
#define IO_DISKSECTOR       15
#define IO_DISKBUFFER       16
#define IO_DISKSTATUS       17
#define IO_RESERVED1        18
#define IO_RESERVED2        19
#define IO_MONITORADDR      20
#define IO_MONITORDATA      21
#define IO_MONITORCMD       22

/* for hwregtrace.txt output */
const char* io_reg_names[] = {
    "irq0enable", "irq1enable", "irq2enable",
    "irq0status", "irq1status", "irq2status",
    "irqhandler", "irqreturn",
    "clks", "leds", "display7seg",
    "timerenable", "timercurrent", "timermax",
    "diskcmd", "disksector", "diskbuffer", "diskstatus",
    "reserved", "reserved",
    "monitoraddr", "monitordata", "monitorcmd"
};

/* Section 2: Global State */

/* use 32-bit ints even though SIMP is 20-bit - easier for signed arithmetic */
int32_t regs[NUM_REGS];
int32_t io_regs[NUM_IO_REGS];
int32_t memory[MEMORY_SIZE];
int32_t disk[DISK_SIZE];
uint8_t monitor[MONITOR_SIZE];

uint32_t pc = 0;
uint32_t cycle_count = 0;
int halted = 0;
int in_interrupt = 0;  /* prevents nested interrupts */

/* disk DMA state - cpu starts op, disk handles transfer */
int disk_busy_until = -1;  /* -1 = idle */
int pending_disk_cmd = 0;
int pending_disk_sector = 0;
int pending_disk_buffer = 0;

/* irq2 timing from irq2in.txt */
int* irq2_times = NULL;
int irq2_count = 0;
int irq2_index = 0;

/* track prev values to only log changes */
int32_t prev_leds = 0;
int32_t prev_display7seg = 0;

FILE* trace_file = NULL;
FILE* hwregtrace_file = NULL;
FILE* leds_file = NULL;
FILE* display7seg_file = NULL;

/* Section 3: Helper Functions */

/*
 * Function: sign_extend
 * Purpose: extends N-bit value to 32-bit preserving sign
 * Input: value - the number, bits - original bit width
 * Output: sign-extended 32-bit value
 */
int32_t sign_extend(int32_t value, int bits) {
    int32_t mask = 1 << (bits - 1);
    return (value ^ mask) - mask;  /* xor trick - fills upper bits if negative */
}

/*
 * Function: read_memory
 * Purpose: reads from main memory with bounds check
 * Input: address (0-4095)
 * Output: value at address, or 0 if out of bounds
 */
int32_t read_memory(int address) {
    if (address < 0 || address >= MEMORY_SIZE) {
        return 0;
    }
    return memory[address];
}

/*
 * Function: write_memory
 * Purpose: writes to main memory with bounds check
 * Input: address, value
 * Output: none
 */
void write_memory(int address, int32_t value) {
    if (address >= 0 && address < MEMORY_SIZE) {
        memory[address] = value;
    }
}

/*
 * Function: get_reg
 * Purpose: reads register value, handling $zero and $imm special cases
 * Input: reg_num (0-15), imm_value for $imm
 * Output: register value
 */
int32_t get_reg(int reg_num, int32_t imm_value) {
    if (reg_num == REG_ZERO) return 0;           /* hardwired to 0 */
    if (reg_num == REG_IMM) return imm_value;    /* immediate from instruction */
    return regs[reg_num];
}

/*
 * Function: set_reg
 * Purpose: writes register value, ignoring writes to $zero and $imm
 * Input: reg_num, value
 * Output: none
 */
void set_reg(int reg_num, int32_t value) {
    if (reg_num != REG_ZERO && reg_num != REG_IMM) {
        regs[reg_num] = value;
    }
}

/* log I/O reads/writes to hwregtrace.txt */
void log_hw_read(int reg_num, int32_t value) {
    if (hwregtrace_file && reg_num >= 0 && reg_num < NUM_IO_REGS) {
        fprintf(hwregtrace_file, "%d READ %s %08X\n",
            cycle_count, io_reg_names[reg_num], (uint32_t)value);
    }
}

void log_hw_write(int reg_num, int32_t value) {
    if (hwregtrace_file && reg_num >= 0 && reg_num < NUM_IO_REGS) {
        fprintf(hwregtrace_file, "%d WRITE %s %08X\n",
            cycle_count, io_reg_names[reg_num], (uint32_t)value);
    }
}

/*
 * Function: read_io_reg
 * Purpose: reads I/O register and logs access
 * Input: reg_num (0-22)
 * Output: register value
 */
int32_t read_io_reg(int reg_num) {
    int32_t value = 0;
    if (reg_num >= 0 && reg_num < NUM_IO_REGS) {
        value = io_regs[reg_num];
        log_hw_read(reg_num, value);
    }
    return value;
}

/*
 * Function: write_io_reg
 * Purpose: writes I/O register with side effects (disk, leds, monitor)
 * Input: reg_num, value
 * Output: none
 */
void write_io_reg(int reg_num, int32_t value) {
    if (reg_num < 0 || reg_num >= NUM_IO_REGS) return;

    log_hw_write(reg_num, value);
    io_regs[reg_num] = value;

    switch (reg_num) {
    case IO_LEDS:
        if (leds_file && value != prev_leds) {
            fprintf(leds_file, "%d %08X\n", cycle_count, (uint32_t)value);
            prev_leds = value;
        }
        break;

    case IO_DISPLAY7SEG:
        if (display7seg_file && value != prev_display7seg) {
            fprintf(display7seg_file, "%d %08X\n", cycle_count, (uint32_t)value);
            prev_display7seg = value;
        }
        break;

    case IO_DISKCMD:
        /* start disk op if idle */
        if (value != 0 && io_regs[IO_DISKSTATUS] == 0) {
            io_regs[IO_DISKSTATUS] = 1;
            disk_busy_until = cycle_count + DISK_DELAY;
            pending_disk_cmd = value;
            pending_disk_sector = io_regs[IO_DISKSECTOR];
            pending_disk_buffer = io_regs[IO_DISKBUFFER];
        }
        break;

    case IO_MONITORCMD:
        if (value == 1) {
            int addr = io_regs[IO_MONITORADDR] & 0xFFFF;
            if (addr < MONITOR_SIZE) {
                monitor[addr] = (uint8_t)(io_regs[IO_MONITORDATA] & 0xFF);
            }
            io_regs[IO_MONITORCMD] = 0;
        }
        break;
    }
}

/* Section 4: Disk Simulation */

/*
 * Function: complete_disk_operation
 * Purpose: finishes DMA transfer and triggers IRQ1
 * Input: uses global pending_disk_* state
 * Output: copies data, sets irq1status=1
 */
void complete_disk_operation(void) {
    int sector_start = pending_disk_sector * SECTOR_SIZE;
    int mem_addr = pending_disk_buffer;
    int i;

    if (pending_disk_cmd == 1) {  /* read: disk -> memory */
        for (i = 0; i < SECTOR_SIZE; i++) {
            if (mem_addr + i < MEMORY_SIZE && sector_start + i < DISK_SIZE) {
                memory[mem_addr + i] = disk[sector_start + i];
            }
        }
    }
    else if (pending_disk_cmd == 2) {  /* write: memory -> disk */
        for (i = 0; i < SECTOR_SIZE; i++) {
            if (mem_addr + i < MEMORY_SIZE && sector_start + i < DISK_SIZE) {
                disk[sector_start + i] = memory[mem_addr + i];
            }
        }
    }

    io_regs[IO_DISKSTATUS] = 0;
    io_regs[IO_DISKCMD] = 0;
    io_regs[IO_IRQ1STATUS] = 1;  /* fire disk interrupt */
    disk_busy_until = -1;
    pending_disk_cmd = 0;
}

/* Section 5: Timer and Interrupt Handling */

/*
 * Function: update_timer
 * Purpose: increments timer, fires IRQ0 when current==max
 * Input: none (uses io_regs)
 * Output: may set irq0status=1
 */
void update_timer(void) {
    if (io_regs[IO_TIMERENABLE]) {
        if (io_regs[IO_TIMERCURRENT] == io_regs[IO_TIMERMAX]) {
            io_regs[IO_IRQ0STATUS] = 1;
            io_regs[IO_TIMERCURRENT] = 0;  /* reset on match, don't increment */
        } else {
            io_regs[IO_TIMERCURRENT]++;
        }
    }
}

/*
 * Function: check_irq2
 * Purpose: fires IRQ2 at cycles specified in irq2in.txt
 * Input: none (uses irq2_times array)
 * Output: may set irq2status=1
 */
void check_irq2(void) {
    while (irq2_index < irq2_count && irq2_times[irq2_index] <= (int)cycle_count) {
        if (irq2_times[irq2_index] == (int)cycle_count) {
            io_regs[IO_IRQ2STATUS] = 1;
        }
        irq2_index++;
    }
}

void check_disk(void) {
    if (disk_busy_until >= 0 && (int)cycle_count >= disk_busy_until) {
        complete_disk_operation();
    }
}

/*
 * Function: check_interrupts
 * Purpose: jumps to handler if interrupt pending and not already in one
 * Input: none
 * Output: returns 1 if interrupt taken, 0 otherwise
 */
int check_interrupts(void) {
    int irq = (io_regs[IO_IRQ0ENABLE] && io_regs[IO_IRQ0STATUS]) ||
              (io_regs[IO_IRQ1ENABLE] && io_regs[IO_IRQ1STATUS]) ||
              (io_regs[IO_IRQ2ENABLE] && io_regs[IO_IRQ2STATUS]);

    if (irq && !in_interrupt) {
        io_regs[IO_IRQRETURN] = pc;
        pc = io_regs[IO_IRQHANDLER];
        in_interrupt = 1;
        return 1;
    }
    return 0;
}

/* Section 6: Instruction Execution */

/* writes trace line: PC INST R0-R15 (before execution) */
void write_trace(uint32_t inst_pc, int32_t instruction) {
    int i;
    if (!trace_file) return;
    fprintf(trace_file, "%03X %05X", inst_pc & 0xFFF, instruction & 0xFFFFF);
    for (i = 0; i < NUM_REGS; i++) {
        fprintf(trace_file, " %08X", (uint32_t)regs[i]);
    }
    fprintf(trace_file, "\n");
}

/*
 * Function: execute_instruction
 * Purpose: fetch-decode-execute one instruction
 * Input: none (uses pc, memory)
 * Output: updates regs, memory, pc
 *
 * Instruction format (20 bits): opcode[8] | rd[4] | rs[4] | rt[4]
 * If any reg is $imm, next word has the immediate value
 */
void execute_instruction(void) {
    int32_t instruction;
    int opcode, rd, rs, rt;
    int32_t imm = 0;
    int32_t rs_val, rt_val, rd_val;
    int32_t result;
    int uses_imm;
    uint32_t inst_pc;
    int branch_taken = 0;

    /* FETCH */
    inst_pc = pc;
    instruction = read_memory(pc);

    /* DECODE */
    opcode = (instruction >> 12) & 0xFF;
    rd = (instruction >> 8) & 0xF;
    rs = (instruction >> 4) & 0xF;
    rt = instruction & 0xF;

    uses_imm = (rd == REG_IMM || rs == REG_IMM || rt == REG_IMM);
    if (uses_imm) {
        imm = read_memory(pc + 1);
        imm = sign_extend(imm, 20);
    }

    regs[REG_IMM] = imm;  /* for trace output */
    write_trace(inst_pc, instruction);

    rs_val = get_reg(rs, imm);
    rt_val = get_reg(rt, imm);
    rd_val = get_reg(rd, imm);  /* for branches */

    /* EXECUTE */
    switch (opcode) {
    case OP_ADD:
        set_reg(rd, rs_val + rt_val);
        break;
    case OP_SUB:
        set_reg(rd, rs_val - rt_val);
        break;
    case OP_MUL:
        set_reg(rd, rs_val * rt_val);
        break;
    case OP_AND:
        set_reg(rd, rs_val & rt_val);
        break;
    case OP_OR:
        set_reg(rd, rs_val | rt_val);
        break;
    case OP_XOR:
        set_reg(rd, rs_val ^ rt_val);
        break;
    case OP_SLL:
        set_reg(rd, rs_val << rt_val);
        break;
    case OP_SRA:
        set_reg(rd, rs_val >> rt_val);  /* arithmetic - preserves sign */
        break;
    case OP_SRL:
        set_reg(rd, (int32_t)((uint32_t)rs_val >> rt_val));  /* logical - fills with 0s */
        break;

    /* branches: pc = rd_val if condition true */
    case OP_BEQ:
        if (rs_val == rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;
    case OP_BNE:
        if (rs_val != rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;
    case OP_BLT:
        if (rs_val < rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;
    case OP_BGT:
        if (rs_val > rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;
    case OP_BLE:
        if (rs_val <= rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;
    case OP_BGE:
        if (rs_val >= rt_val) { pc = rd_val & 0xFFF; branch_taken = 1; }
        break;

    case OP_JAL:
        result = pc + (uses_imm ? 2 : 1);  /* return addr = next instruction */
        set_reg(rd, result);
        pc = rs_val & 0xFFF;
        branch_taken = 1;
        break;

    case OP_LW:
        result = read_memory((rs_val + rt_val) & 0xFFF);
        set_reg(rd, sign_extend(result, 20));
        break;
    case OP_SW:
        write_memory((rs_val + rt_val) & 0xFFF, rd_val & 0xFFFFF);
        break;

    case OP_RETI:
        pc = io_regs[IO_IRQRETURN];
        in_interrupt = 0;
        branch_taken = 1;
        break;

    case OP_IN:
        set_reg(rd, read_io_reg((rs_val + rt_val) & 0x1F));
        break;
    case OP_OUT:
        write_io_reg((rs_val + rt_val) & 0x1F, rd_val);
        break;

    case OP_HALT:
        halted = 1;
        break;

    default:
        break;  /* unknown opcode = nop */
    }

    /* advance pc if no branch/jump/halt - I-format uses 2 words, R-format uses 1 */
    if (!branch_taken && !halted) {
        pc += uses_imm ? 2 : 1;
    }
}

/* Section 7: File I/O */

int load_memin(const char* filename) {
    FILE* f = fopen(filename, "r");
    char line[16];
    int addr = 0;

    if (!f) {
        printf("Error: Cannot open %s\n", filename);
        return -1;
    }
    memset(memory, 0, sizeof(memory));
    while (fgets(line, sizeof(line), f) && addr < MEMORY_SIZE) {
        memory[addr++] = (int32_t)strtol(line, NULL, 16);
    }
    fclose(f);
    return 0;
}

int load_diskin(const char* filename) {
    FILE* f = fopen(filename, "r");
    char line[16];
    int addr = 0;

    memset(disk, 0, sizeof(disk));
    if (!f) return 0;  /* ok if missing - disk just empty */

    while (fgets(line, sizeof(line), f) && addr < DISK_SIZE) {
        disk[addr++] = (int32_t)strtol(line, NULL, 16);
    }
    fclose(f);
    return 0;
}

int load_irq2in(const char* filename) {
    FILE* f = fopen(filename, "r");
    char line[32];
    int count = 0;
    int capacity = 100;

    irq2_times = (int*)malloc(capacity * sizeof(int));
    irq2_count = 0;
    irq2_index = 0;

    if (!f || !irq2_times) {
        if (irq2_times == NULL) irq2_times = (int*)malloc(sizeof(int));
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        if (count >= capacity) {
            capacity *= 2;
            irq2_times = (int*)realloc(irq2_times, capacity * sizeof(int));
        }
        irq2_times[count++] = atoi(line);
    }
    irq2_count = count;
    fclose(f);
    return 0;
}

int save_memout(const char* filename) {
    FILE* f = fopen(filename, "w");
    int i;
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    for (i = 0; i < MEMORY_SIZE; i++) {
        fprintf(f, "%05X\n", (uint32_t)memory[i] & 0xFFFFF);
    }
    fclose(f);
    return 0;
}

int save_regout(const char* filename) {
    FILE* f = fopen(filename, "w");
    int i;
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    for (i = 2; i < NUM_REGS; i++) {  /* skip $zero and $imm */
        fprintf(f, "%08X\n", (uint32_t)regs[i]);
    }
    fclose(f);
    return 0;
}

int save_cycles(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    fprintf(f, "%u\n", cycle_count);
    fclose(f);
    return 0;
}

int save_diskout(const char* filename) {
    FILE* f = fopen(filename, "w");
    int i;
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    for (i = 0; i < DISK_SIZE; i++) {
        fprintf(f, "%05X\n", (uint32_t)disk[i] & 0xFFFFF);
    }
    fclose(f);
    return 0;
}

int save_monitor_txt(const char* filename) {
    FILE* f = fopen(filename, "w");
    int i;
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    for (i = 0; i < MONITOR_SIZE; i++) {
        fprintf(f, "%02X\n", monitor[i]);
    }
    fclose(f);
    return 0;
}

int save_monitor_yuv(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) { printf("Error: Cannot create %s\n", filename); return -1; }
    fwrite(monitor, 1, MONITOR_SIZE, f);
    fclose(f);
    return 0;
}

/* Section 8: Main Simulation Loop */

void run_cycle(void) {
    io_regs[IO_CLKS] = cycle_count;  /* update at start so reads see current cycle */
    check_irq2();
    check_disk();
    if (!check_interrupts()) {
        execute_instruction();
    }
    update_timer();  /* timer updates at end of cycle */
    cycle_count++;
}

void init_simulator(void) {
    memset(regs, 0, sizeof(regs));
    memset(io_regs, 0, sizeof(io_regs));
    memset(monitor, 0, sizeof(monitor));
    pc = 0;
    cycle_count = 0;
    halted = 0;
    in_interrupt = 0;
    disk_busy_until = -1;
    pending_disk_cmd = 0;
    prev_leds = 0;
    prev_display7seg = 0;
}

/* Section 9: Main Function */

int main(int argc, char* argv[]) {
    if (argc != 14) {
        printf("Usage: sim memin.txt diskin.txt irq2in.txt memout.txt regout.txt\n");
        printf("       trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt\n");
        printf("       diskout.txt monitor.txt monitor.yuv\n");
        return 1;
    }

    init_simulator();

    if (load_memin(argv[1]) != 0) return 1;
    load_diskin(argv[2]);
    load_irq2in(argv[3]);

    trace_file = fopen(argv[6], "w");
    hwregtrace_file = fopen(argv[7], "w");
    leds_file = fopen(argv[9], "w");
    display7seg_file = fopen(argv[10], "w");

    if (!trace_file || !hwregtrace_file || !leds_file || !display7seg_file) {
        printf("Error: Cannot create output files\n");
        return 1;
    }

    while (!halted) {
        run_cycle();
        if (cycle_count > 100000000) {
            printf("Warning: Exceeded 100M cycles, stopping\n");
            break;
        }
    }

    fclose(trace_file);
    fclose(hwregtrace_file);
    fclose(leds_file);
    fclose(display7seg_file);

    save_memout(argv[4]);
    save_regout(argv[5]);
    save_cycles(argv[8]);
    save_diskout(argv[11]);
    save_monitor_txt(argv[12]);
    save_monitor_yuv(argv[13]);

    if (irq2_times) free(irq2_times);

    printf("Simulation completed: %u cycles\n", cycle_count);
    return 0;
}
