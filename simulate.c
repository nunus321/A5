#include "simulate.h"
#include "disassemble.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Register file
static int32_t registers[32];
#define zero registers[0]    // x0 is hardwired to 0
#define ra registers[1]      // Return address
#define sp registers[2]      // Stack pointer
#define a0 registers[10]     // Function argument/return value
#define a7 registers[17]     // System call number

// Helper function to log register changes
static void log_register_change(FILE *log_file, int reg_num, int32_t new_value) {
    if (log_file && reg_num != 0) { // Don't log changes to x0
        fprintf(log_file, "                R[%2d] <- %x", reg_num, new_value);
    }
}

// Helper function to log memory writes
static void log_memory_write(FILE *log_file, uint32_t addr, uint32_t value, int bytes) {
    if (log_file) {
        fprintf(log_file, "                M[%x] <- %x (%d bytes)", addr, value, bytes);
    }
}

// Helper function to indicate taken branches
static void log_branch_taken(FILE *log_file) {
    if (log_file) {
        fprintf(log_file, "            {T}");
    }
}

// Helper function to indicate instruction fetch from new address
static void log_jump_target(FILE *log_file) {
    if (log_file) {
        fprintf(log_file, "=>");
    }
}

// Helper functions for instruction decoding
static inline uint32_t extract_bits(uint32_t instruction, int start, int length) {
    return (instruction >> start) & ((1 << length) - 1);
}

static inline int32_t sign_extend(uint32_t x, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    return (x ^ sign_bit) - sign_bit;
}

// Immediate extraction functions
static int32_t get_i_imm(uint32_t inst) {
    return sign_extend(inst >> 20, 12);
}

static int32_t get_s_imm(uint32_t inst) {
    return sign_extend(((inst >> 25) << 5) | ((inst >> 7) & 0x1F), 12);
}

static int32_t get_b_imm(uint32_t inst) {
    return sign_extend(
        ((inst >> 31) << 12) | 
        ((inst >> 7) & 0x1) << 11 |
        ((inst >> 25) & 0x3F) << 5 |
        ((inst >> 8) & 0xF) << 1,
        13);
}

static int32_t get_u_imm(uint32_t inst) {
    return inst & 0xFFFFF000;
}

static int32_t get_j_imm(uint32_t inst) {
    return sign_extend(
        ((inst >> 31) << 20) |
        ((inst >> 12) & 0xFF) << 12 |
        ((inst >> 20) & 0x1) << 11 |
        ((inst >> 21) & 0x3FF) << 1,
        21);
}

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    struct Stat stats = {0};  // Initialize statistics
    uint32_t pc = start_addr;  // Program counter
    uint32_t prev_pc = pc;     // Previous PC for jump detection
    char disasm_buf[100];      // Buffer for disassembly

    // Initialize registers
    for (int i = 0; i < 32; i++) {
        registers[i] = 0;
    }

    // Main simulation loop
    while (1) {
        // Check if we jumped to this instruction
        if (pc != prev_pc + 4 && log_file) {
            log_jump_target(log_file);
        }

        // Fetch instruction
        uint32_t instruction = memory_rd_w(mem, pc);
        
        // Log the instruction if logging is enabled
        if (log_file) {
            disassemble(pc, instruction, disasm_buf, sizeof(disasm_buf), symbols);
            fprintf(log_file, "%8ld %8x : %08X     %-30s", 
                    stats.insns, pc, instruction, disasm_buf);
        }

        // Increment instruction count
        stats.insns++;

        // Keep x0 as zero
        zero = 0;

        // Extract common fields
        uint32_t opcode = instruction & 0x7F;
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t funct7 = (instruction >> 25) & 0x7F;

        // Default next PC is next instruction
        uint32_t next_pc = pc + 4;
        prev_pc = pc;  // Save current PC

        // Execute instruction
        switch (opcode) {
            case 0x37: // LUI
                registers[rd] = get_u_imm(instruction);
                log_register_change(log_file, rd, registers[rd]);
                break;

            case 0x17: // AUIPC
                registers[rd] = pc + get_u_imm(instruction);
                log_register_change(log_file, rd, registers[rd]);
                break;

            case 0x6F: // JAL
                registers[rd] = pc + 4;
                next_pc = pc + get_j_imm(instruction);
                log_register_change(log_file, rd, registers[rd]);
                break;

            case 0x67: // JALR
                {
                    uint32_t temp = pc + 4;
                    next_pc = (registers[rs1] + get_i_imm(instruction)) & ~1;
                    registers[rd] = temp;
                    log_register_change(log_file, rd, registers[rd]);
                }
                break;

            case 0x63: // Branch instructions
                {
                    bool take_branch = false;
                    switch (funct3) {
                        case 0x0: // BEQ
                            take_branch = (registers[rs1] == registers[rs2]);
                            break;
                        case 0x1: // BNE
                            take_branch = (registers[rs1] != registers[rs2]);
                            break;
                        case 0x4: // BLT
                            take_branch = (registers[rs1] < registers[rs2]);
                            break;
                        case 0x5: // BGE
                            take_branch = (registers[rs1] >= registers[rs2]);
                            break;
                        case 0x6: // BLTU
                            take_branch = ((uint32_t)registers[rs1] < (uint32_t)registers[rs2]);
                            break;
                        case 0x7: // BGEU
                            take_branch = ((uint32_t)registers[rs1] >= (uint32_t)registers[rs2]);
                            break;
                    }
                    if (take_branch) {
                        next_pc = pc + get_b_imm(instruction);
                        log_branch_taken(log_file);
                    }
                }
                break;

            case 0x03: // Load instructions
                {
                    uint32_t addr = registers[rs1] + get_i_imm(instruction);
                    switch (funct3) {
                        case 0x0: // LB
                            registers[rd] = sign_extend(memory_rd_b(mem, addr), 8);
                            log_register_change(log_file, rd, registers[rd]);
                            break;
                        case 0x1: // LH
                            registers[rd] = sign_extend(memory_rd_h(mem, addr), 16);
                            log_register_change(log_file, rd, registers[rd]);
                            break;
                        case 0x2: // LW
                            registers[rd] = memory_rd_w(mem, addr);
                            log_register_change(log_file, rd, registers[rd]);
                            break;
                        case 0x4: // LBU
                            registers[rd] = memory_rd_b(mem, addr) & 0xFF;
                            log_register_change(log_file, rd, registers[rd]);
                            break;
                        case 0x5: // LHU
                            registers[rd] = memory_rd_h(mem, addr) & 0xFFFF;
                            log_register_change(log_file, rd, registers[rd]);
                            break;
                    }
                }
                break;

            case 0x23: // Store instructions
                {
                    uint32_t addr = registers[rs1] + get_s_imm(instruction);
                    switch (funct3) {
                        case 0x0: // SB
                            memory_wr_b(mem, addr, registers[rs2]);
                            log_memory_write(log_file, addr, registers[rs2] & 0xFF, 1);
                            break;
                        case 0x1: // SH
                            memory_wr_h(mem, addr, registers[rs2]);
                            log_memory_write(log_file, addr, registers[rs2] & 0xFFFF, 2);
                            break;
                        case 0x2: // SW
                            memory_wr_w(mem, addr, registers[rs2]);
                            log_memory_write(log_file, addr, registers[rs2], 4);
                            break;
                    }
                }
                break;

            case 0x13: // Immediate arithmetic
                switch (funct3) {
                    case 0x0: // ADDI
                        registers[rd] = registers[rs1] + get_i_imm(instruction);
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x1: // SLLI
                        registers[rd] = registers[rs1] << (instruction >> 20) & 0x1F;
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x2: // SLTI
                        registers[rd] = (registers[rs1] < get_i_imm(instruction)) ? 1 : 0;
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x3: // SLTIU
                        registers[rd] = ((uint32_t)registers[rs1] < (uint32_t)get_i_imm(instruction)) ? 1 : 0;
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x4: // XORI
                        registers[rd] = registers[rs1] ^ get_i_imm(instruction);
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x5: // SRLI/SRAI
                        if (funct7 == 0x20) { // SRAI
                            registers[rd] = registers[rs1] >> (instruction >> 20) & 0x1F;
                        } else { // SRLI
                            registers[rd] = (uint32_t)registers[rs1] >> (instruction >> 20) & 0x1F;
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x6: // ORI
                        registers[rd] = registers[rs1] | get_i_imm(instruction);
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x7: // ANDI
                        registers[rd] = registers[rs1] & get_i_imm(instruction);
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                }
                break;

            case 0x33: // Register arithmetic
                switch (funct3) {
                    case 0x0: // ADD/SUB/MUL
                        if (funct7 == 0x20) {
                            registers[rd] = registers[rs1] - registers[rs2];
                        } else if (funct7 == 0x01) {
                            registers[rd] = registers[rs1] * registers[rs2];
                        } else {
                            registers[rd] = registers[rs1] + registers[rs2];
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x1: // SLL/MULH
                        if (funct7 == 0x01) {
                            registers[rd] = ((int64_t)registers[rs1] * (int64_t)registers[rs2]) >> 32;
                        } else {
                            registers[rd] = registers[rs1] << (registers[rs2] & 0x1F);
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x2: // SLT
                        registers[rd] = (registers[rs1] < registers[rs2]) ? 1 : 0;
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x3: // SLTU
                        registers[rd] = ((uint32_t)registers[rs1] < (uint32_t)registers[rs2]) ? 1 : 0;
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x4: // XOR/DIV
                        if (funct7 == 0x01) {
                            if (registers[rs2] != 0) {
                                registers[rd] = (int32_t)((int32_t)registers[rs1] / (int32_t)registers[rs2]);
                            } else {
                                registers[rd] = -1;
                            }
                        } else {
                            registers[rd] = registers[rs1] ^ registers[rs2];
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x5: // SRL/SRA/DIVU
                        if (funct7 == 0x20) {
                            registers[rd] = registers[rs1] >> (registers[rs2] & 0x1F);
                        } else if (funct7 == 0x01) {
                            if (registers[rs2] != 0) {
                                registers[rd] = (int32_t)((uint32_t)registers[rs1] / (uint32_t)registers[rs2]);
                            } else {
                                registers[rd] = -1;
                            }
                        } else {
                            registers[rd] = (uint32_t)registers[rs1] >> (registers[rs2] & 0x1F);
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x6: // OR/REM
                        if (funct7 == 0x01) {
                            if (registers[rs2] != 0) {
                                registers[rd] = (int32_t)((int32_t)registers[rs1] % (int32_t)registers[rs2]);
                            } else {
                                registers[rd] = registers[rs1];
                            }
                        } else {
                            registers[rd] = registers[rs1] | registers[rs2];
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                    case 0x7: // AND/REMU
                        if (funct7 == 0x01) {
                            if (registers[rs2] != 0) {
                                registers[rd] = (int32_t)((uint32_t)registers[rs1] % (uint32_t)registers[rs2]);
                            } else {
                                registers[rd] = registers[rs1];
                            }
                        } else {
                            registers[rd] = registers[rs1] & registers[rs2];
                        }
                        log_register_change(log_file, rd, registers[rd]);
                        break;
                }
                break;

                    case 0x73: // ECALL
                        if (instruction == 0x73) {
                            switch (a7) {
                                case 1: // getchar
                                    a0 = getchar();
                                    if (log_file) fprintf(log_file, "getchar() -> %c\n", a0);
                                    log_register_change(log_file, 10, a0);
                                    break;
                                case 2: // putchar
                                    putchar(a0);
                                    if (log_file) fprintf(log_file, "putchar(%c)\n", a0);
                                    break;
                                case 3: case 93: // exit
                                    if (log_file) fprintf(log_file, "exit()\n");
                                    return stats;
                                default:
                                    fprintf(stderr, "Unknown syscall: %d\n", a7);
                                    exit(1);
                            }
                        }
                        break;

            default:
                fprintf(stderr, "Unknown instruction at PC=%x: %x\n", pc, instruction);
                exit(1);
        }

        // Add newline to log if needed
        if (log_file) {
            fprintf(log_file, "\n");
        }

        // Update PC
        pc = next_pc;
    }

    return stats;
}