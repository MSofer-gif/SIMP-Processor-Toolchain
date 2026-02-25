/*
 * SIMP Assembler
 * Two-pass assembler that converts .asm files to memin.txt (20-bit machine code)
 * Usage: asm program.asm memin.txt
 */

#define _CRT_SECURE_NO_WARNINGS  /* lets us use standard C funcs in Visual Studio */

#include <stdio.h>   /* fopen, fclose, fgets, fprintf, feof */
#include <stdlib.h>  /* strtol, exit */
#include <string.h>  /* strcmp, strcpy, strncpy, strlen, strchr, strtok, memmove */
#include <ctype.h>   /* isspace, isdigit */

/* Section 1: Constants */

#define MAX_LINE_LENGTH 500
#define MAX_LABEL_LENGTH 50
#define MAX_LABELS 4096
#define MAX_MEMORY 4096

/* Section 2: Opcode Table */

/* index in array = opcode number, so opcodes[0] = "add" means add is opcode 0 */
const char* opcodes[] = {
    "add", "sub", "mul", "and", "or", "xor",
    "sll", "sra", "srl",
    "beq", "bne", "blt", "bgt", "ble", "bge",
    "jal", "lw", "sw", "reti", "in", "out", "halt"
};
#define NUM_OPCODES 22

/* Section 3: Register Table */

/* same idea - index = register number */
const char* registers[] = {
    "$zero", "$imm", "$v0",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2",
    "$s0", "$s1", "$s2",
    "$gp", "$sp", "$ra"
};
#define NUM_REGISTERS 16

/* Section 4: Symbol Table */

/* using parallel arrays here - label_names[i] goes with label_addresses[i] */
char label_names[MAX_LABELS][MAX_LABEL_LENGTH];
int label_addresses[MAX_LABELS];
int num_labels = 0;

/* Section 5: Lookup Functions */

/*
 * Function: get_opcode_number
 * Purpose: finds the opcode number for a given opcode string
 * Input: opcode - string like "add" or "beq"
 * Output: returns 0-21 if found, -1 if not found
 */
int get_opcode_number(const char* opcode) {
    int i;
    for (i = 0; i < NUM_OPCODES; i++) {
        if (strcmp(opcode, opcodes[i]) == 0) {
            return i;
        }
    }
    return -1;  /* not found */
}

/*
 * Function: get_register_number
 * Purpose: finds the register number for a given register string
 * Input: reg - string like "$zero" or "$t0"
 * Output: returns 0-15 if found, -1 if not found
 */
int get_register_number(const char* reg) {
    int i;
    for (i = 0; i < NUM_REGISTERS; i++) {
        if (strcmp(reg, registers[i]) == 0) {
            return i;
        }
    }
    return -1;  /* not found */
}

/*
 * Function: get_label_address
 * Purpose: looks up a label in the symbol table
 * Input: label - the label name without the colon
 * Output: returns the address if found, -1 if not found
 */
int get_label_address(const char* label) {
    int i;
    for (i = 0; i < num_labels; i++) {
        if (strcmp(label, label_names[i]) == 0) {
            return label_addresses[i];
        }
    }
    return -1;  /* not found */
}

/* Section 6: Symbol Table Functions */

/*
 * Function: add_label
 * Purpose: adds a new label to the symbol table
 * Input: name - label name, address - where it points to
 * Output: none (modifies global symbol table)
 */
void add_label(const char* name, int address) {
    if (num_labels >= MAX_LABELS) {
        fprintf(stderr, "Error: Too many labels (max %d)\n", MAX_LABELS);
        exit(1);
    }
    strncpy(label_names[num_labels], name, MAX_LABEL_LENGTH - 1);
    label_names[num_labels][MAX_LABEL_LENGTH - 1] = '\0';  /* make sure it's null terminated */
    label_addresses[num_labels] = address;
    num_labels++;
}

/* Section 7: String Helper Functions */

/*
 * Function: trim_whitespace
 * Purpose: removes spaces/tabs/newlines from start and end of string
 * Input: str - the string to trim (gets modified)
 * Output: returns pointer to trimmed string (might be different from input)
 */
char* trim_whitespace(char* str) {
    char* end;

    while (isspace((unsigned char)*str)) {  /* skip leading whitespace */
        str++;
    }
    if (*str == '\0') {
        return str;  /* string was all whitespace */
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {  /* find last non-whitespace */
        end--;
    }
    *(end + 1) = '\0';  /* chop off trailing whitespace */

    return str;
}

/*
 * Function: remove_comments
 * Purpose: cuts off everything after # in a line
 * Input: line - the line to process (gets modified)
 * Output: none
 */
void remove_comments(char* line) {
    char* comment = strchr(line, '#');  /* find the # */
    if (comment != NULL) {
        *comment = '\0';  /* replace # with end of string */
    }
}

/*
 * Function: is_empty_line
 * Purpose: checks if a line has only whitespace
 * Input: line - the line to check
 * Output: 1 if empty/whitespace only, 0 otherwise
 */
int is_empty_line(const char* line) {
    while (*line) {
        if (!isspace((unsigned char)*line)) {
            return 0;  /* found something thats not whitespace */
        }
        line++;
    }
    return 1;  /* got through whole string, it's empty */
}

/* Section 8: Instruction Helper Functions */

/*
 * Function: uses_immediate
 * Purpose: checks if instruction uses $imm register
 * Input: rd, rs, rt - the three register numbers
 * Output: 1 if any register is $imm (reg 1), 0 otherwise
 * Note: this determines if we use I-format (2 words) or R-format (1 word)
 */
int uses_immediate(int rd, int rs, int rt) {
    return (rd == 1 || rs == 1 || rt == 1);  /* $imm is register 1 */
}

/*
 * Function: parse_immediate
 * Purpose: converts immediate string to a number
 * Input: imm_str - can be decimal (5), hex (0x100), or label name
 * Output: the numeric value
 */
int parse_immediate(const char* imm_str) {
    int addr;

    while (isspace((unsigned char)*imm_str)) {  /* skip whitespace */
        imm_str++;
    }

    /* check for hex - starts with 0x */
    if (imm_str[0] == '0' && (imm_str[1] == 'x' || imm_str[1] == 'X')) {
        return (int)strtol(imm_str, NULL, 16);
    }

    /* check for decimal - starts with digit or +/- sign */
    if (isdigit((unsigned char)imm_str[0]) || imm_str[0] == '-' || imm_str[0] == '+') {
        return (int)strtol(imm_str, NULL, 10);
    }

    /* must be a label - look it up */
    addr = get_label_address(imm_str);
    if (addr == -1) {
        fprintf(stderr, "Error: Unknown label '%s'\n", imm_str);
        exit(1);
    }
    return addr;
}

/* Section 9: Main Parser */

/*
 * Function: parse_instruction_line
 * Purpose: parses one line of assembly and extracts all the parts
 * Input: line - the raw line from the file
 * Output: fills in label_out, opcode_out, rd_out, rs_out, rt_out, imm_out
 * Returns: 0 = empty/label only, 1 = got an instruction, -1 = error
 */
int parse_instruction_line(const char* line,
    char* label_out,
    char* opcode_out,
    char* rd_out,
    char* rs_out,
    char* rt_out,
    char* imm_out) {
    char line_copy[MAX_LINE_LENGTH];
    char* trimmed;
    char* colon;
    char* instruction_start;
    char* token;
    char* label_trimmed;
    char* rest;

    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);  /* copy because strtok destroys the string */
    line_copy[MAX_LINE_LENGTH - 1] = '\0';

    /* init outputs to empty */
    label_out[0] = '\0';
    opcode_out[0] = '\0';
    rd_out[0] = '\0';
    rs_out[0] = '\0';
    rt_out[0] = '\0';
    imm_out[0] = '\0';

    remove_comments(line_copy);
    trimmed = trim_whitespace(line_copy);
    if (is_empty_line(trimmed)) {
        return 0;
    }

    /* handle .word directive separately - format is ".word address data" */
    if (strncmp(trimmed, ".word", 5) == 0) {
        rest = trimmed + 5;
        strcpy(opcode_out, ".word");
        while (isspace((unsigned char)*rest)) {
            rest++;
        }
        sscanf(rest, "%s %s", rd_out, rs_out);  /* rd = address, rs = data */
        return 1;
    }

    /* check for label - look for colon */
    colon = strchr(trimmed, ':');
    instruction_start = trimmed;

    if (colon != NULL) {
        *colon = '\0';  /* split at colon */
        strncpy(label_out, trimmed, MAX_LABEL_LENGTH - 1);
        label_out[MAX_LABEL_LENGTH - 1] = '\0';

        label_trimmed = trim_whitespace(label_out);
        if (label_trimmed != label_out) {
            memmove(label_out, label_trimmed, strlen(label_trimmed) + 1);
        }

        instruction_start = colon + 1;  /* instruction is after the colon */
        while (isspace((unsigned char)*instruction_start)) {
            instruction_start++;
        }
    }

    if (is_empty_line(instruction_start)) {
        return 0;  /* just a label, no instruction */
    }

    /* parse the instruction: opcode $rd, $rs, $rt, imm */
    token = strtok(instruction_start, " \t,");
    if (token == NULL) return 0;
    strncpy(opcode_out, token, MAX_LABEL_LENGTH - 1);

    token = strtok(NULL, " \t,");
    if (token == NULL) return -1;
    strncpy(rd_out, token, MAX_LABEL_LENGTH - 1);

    token = strtok(NULL, " \t,");
    if (token == NULL) return -1;
    strncpy(rs_out, token, MAX_LABEL_LENGTH - 1);

    token = strtok(NULL, " \t,");
    if (token == NULL) return -1;
    strncpy(rt_out, token, MAX_LABEL_LENGTH - 1);

    token = strtok(NULL, " \t,");
    if (token == NULL) return -1;
    strncpy(imm_out, token, MAX_LABEL_LENGTH - 1);

    return 1;
}

/* Section 10: Main Function */

int main(int argc, char* argv[]) {
    FILE* input_file;
    FILE* output_file;
    char line[MAX_LINE_LENGTH];
    int memory[MAX_MEMORY];
    int mem_index;
    int address;
    int i;
    int max_addr;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.asm> <output.txt>\n", argv[0]);
        return 1;
    }

    for (i = 0; i < MAX_MEMORY; i++) {
        memory[i] = 0;
    }

    /* Pass 1: Build symbol table
       need to count addresses right - I-format uses 2 words, R-format uses 1 */
    input_file = fopen(argv[1], "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", argv[1]);
        return 1;
    }

    address = 0;

    while (fgets(line, MAX_LINE_LENGTH, input_file) != NULL) {
        char label[MAX_LABEL_LENGTH];
        char opcode[MAX_LABEL_LENGTH];
        char rd[MAX_LABEL_LENGTH], rs[MAX_LABEL_LENGTH], rt[MAX_LABEL_LENGTH];
        char imm[MAX_LABEL_LENGTH];
        int result;
        int rd_num, rs_num, rt_num;

        result = parse_instruction_line(line, label, opcode, rd, rs, rt, imm);

        if (strlen(label) > 0) {
            add_label(label, address);  /* record where this label points */
        }

        if (strcmp(opcode, ".word") == 0) {
            continue;  /* .word doesn't affect instruction addresses */
        }

        if (result == 1 && strlen(opcode) > 0) {
            rd_num = get_register_number(rd);
            rs_num = get_register_number(rs);
            rt_num = get_register_number(rt);

            if (uses_immediate(rd_num, rs_num, rt_num)) {
                address += 2;  /* I-format: instruction + immediate */
            }
            else {
                address += 1;  /* R-format: just the instruction */
            }
        }
    }

    fclose(input_file);

    /* Pass 2: Generate machine code
       format: [opcode:8 bits][rd:4][rs:4][rt:4] = 20 bits total */
    input_file = fopen(argv[1], "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", argv[1]);
        return 1;
    }

    mem_index = 0;

    while (fgets(line, MAX_LINE_LENGTH, input_file) != NULL) {
        char label[MAX_LABEL_LENGTH];
        char opcode[MAX_LABEL_LENGTH];
        char rd[MAX_LABEL_LENGTH], rs[MAX_LABEL_LENGTH], rt[MAX_LABEL_LENGTH];
        char imm[MAX_LABEL_LENGTH];
        int result;
        int opcode_num, rd_num, rs_num, rt_num;
        int instruction;
        int imm_value;
        int word_addr, word_data;

        result = parse_instruction_line(line, label, opcode, rd, rs, rt, imm);

        if (result != 1 || strlen(opcode) == 0) {
            continue;  /* skip empty lines */
        }

        /* .word writes directly to a specific address */
        if (strcmp(opcode, ".word") == 0) {
            word_addr = parse_immediate(rd);
            word_data = parse_immediate(rs);
            if (word_addr >= 0 && word_addr < MAX_MEMORY) {
                memory[word_addr] = word_data;
            }
            continue;
        }

        opcode_num = get_opcode_number(opcode);
        if (opcode_num == -1) {
            fprintf(stderr, "Error: Unknown opcode '%s'\n", opcode);
            fclose(input_file);
            return 1;
        }

        rd_num = get_register_number(rd);
        rs_num = get_register_number(rs);
        rt_num = get_register_number(rt);

        if (rd_num == -1 || rs_num == -1 || rt_num == -1) {
            fprintf(stderr, "Error: Unknown register\n");
            fprintf(stderr, "  rd='%s' rs='%s' rt='%s'\n", rd, rs, rt);
            fclose(input_file);
            return 1;
        }

        /* pack everything into one 20-bit word */
        instruction = (opcode_num << 12) | (rd_num << 8) | (rs_num << 4) | rt_num;
        memory[mem_index] = instruction;
        mem_index++;

        /* if I-format, also store the immediate value in the next word */
        if (uses_immediate(rd_num, rs_num, rt_num)) {
            imm_value = parse_immediate(imm);
            memory[mem_index] = imm_value;
            mem_index++;
        }
    }

    fclose(input_file);

    /* Write output file - one 5-digit hex number per line */
    output_file = fopen(argv[2], "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", argv[2]);
        return 1;
    }

    /* find highest address that has data (could be from .word) */
    max_addr = mem_index;
    for (i = mem_index; i < MAX_MEMORY; i++) {
        if (memory[i] != 0) {
            max_addr = i + 1;
        }
    }

    /* write everything from 0 to max_addr */
    for (i = 0; i < max_addr; i++) {
        fprintf(output_file, "%05X\n", memory[i] & 0xFFFFF);  /* mask to 20 bits */
    }

    fclose(output_file);

    return 0;
}
