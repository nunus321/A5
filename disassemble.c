#include "disassemble.h"
#include "read_elf.h" 
#include <stdio.h>

// Extract fields from RISC-V instruction formats
static inline uint32_t get_opcode(uint32_t inst) { return inst & 0x7F; }
static inline uint32_t get_rd(uint32_t inst) { return (inst >> 7) & 0x1F; }
static inline uint32_t get_rs1(uint32_t inst) { return (inst >> 15) & 0x1F; }
static inline uint32_t get_rs2(uint32_t inst) { return (inst >> 20) & 0x1F; }
static inline uint32_t get_funct3(uint32_t inst) { return (inst >> 12) & 0x7; }
static inline uint32_t get_funct7(uint32_t inst) { return (inst >> 25) & 0x7F; }

// Sign extend a value from a given bit width
static inline int32_t sign_extend(uint32_t x, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    return (x ^ sign_bit) - sign_bit;
}

// Get immediates for different instruction formats
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

// Register names
static const char* reg_names[] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    uint32_t opcode = get_opcode(instruction);
    uint32_t rd = get_rd(instruction);
    uint32_t rs1 = get_rs1(instruction);
    uint32_t rs2 = get_rs2(instruction);
    uint32_t funct3 = get_funct3(instruction);
    uint32_t funct7 = get_funct7(instruction);

    // Try to get symbol for this address
    const char* symbol = symbols ? symbols_value_to_sym(symbols, addr) : NULL;
    if (symbol) {
        snprintf(result, buf_size, "%s:", symbol);
        return;
    }

    switch (opcode) {
        case 0x37: // LUI
            snprintf(result, buf_size, "lui      %s,0x%x", reg_names[rd], get_u_imm(instruction) >> 12);
            break;
            
        case 0x17: // AUIPC
            snprintf(result, buf_size, "auipc    %s,0x%x", reg_names[rd], get_u_imm(instruction) >> 12);
            break;

        case 0x6F: // JAL
            snprintf(result, buf_size, "jal      %s,0x%x", reg_names[rd], addr + get_j_imm(instruction));
            break;

        case 0x67: // JALR
            snprintf(result, buf_size, "jalr     %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
            break;

        case 0x63: // Branch instructions
            switch (funct3) {
                case 0x0: // BEQ
                    snprintf(result, buf_size, "beq      %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                case 0x1: // BNE
                    snprintf(result, buf_size, "bne      %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                case 0x4: // BLT
                    snprintf(result, buf_size, "blt      %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                case 0x5: // BGE
                    snprintf(result, buf_size, "bge      %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                case 0x6: // BLTU
                    snprintf(result, buf_size, "bltu     %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                case 0x7: // BGEU
                    snprintf(result, buf_size, "bgeu     %s,%s,0x%x", reg_names[rs1], reg_names[rs2], addr + get_b_imm(instruction));
                    break;
                default:
                    snprintf(result, buf_size, "unknown branch");
                    break;
            }
            break;

        case 0x03: // Load instructions
            switch (funct3) {
                case 0x0: // LB
                    snprintf(result, buf_size, "lb       %s,%d(%s)", reg_names[rd], get_i_imm(instruction), reg_names[rs1]);
                    break;
                case 0x1: // LH
                    snprintf(result, buf_size, "lh       %s,%d(%s)", reg_names[rd], get_i_imm(instruction), reg_names[rs1]);
                    break;
                case 0x2: // LW
                    snprintf(result, buf_size, "lw       %s,%d(%s)", reg_names[rd], get_i_imm(instruction), reg_names[rs1]);
                    break;
                case 0x4: // LBU
                    snprintf(result, buf_size, "lbu      %s,%d(%s)", reg_names[rd], get_i_imm(instruction), reg_names[rs1]);
                    break;
                case 0x5: // LHU
                    snprintf(result, buf_size, "lhu      %s,%d(%s)", reg_names[rd], get_i_imm(instruction), reg_names[rs1]);
                    break;
                default:
                    snprintf(result, buf_size, "unknown load");
                    break;
            }
            break;

        case 0x23: // Store instructions
            switch (funct3) {
                case 0x0: // SB
                    snprintf(result, buf_size, "sb       %s,%d(%s)", reg_names[rs2], get_s_imm(instruction), reg_names[rs1]);
                    break;
                case 0x1: // SH
                    snprintf(result, buf_size, "sh       %s,%d(%s)", reg_names[rs2], get_s_imm(instruction), reg_names[rs1]);
                    break;
                case 0x2: // SW
                    snprintf(result, buf_size, "sw       %s,%d(%s)", reg_names[rs2], get_s_imm(instruction), reg_names[rs1]);
                    break;
                default:
                    snprintf(result, buf_size, "unknown store");
                    break;
            }
            break;

        case 0x13: // Immediate arithmetic
            switch (funct3) {
                case 0x0: // ADDI
                    snprintf(result, buf_size, "addi     %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                case 0x1: // SLLI
                    snprintf(result, buf_size, "slli     %s,%s,%d", reg_names[rd], reg_names[rs1], rs2);
                    break;
                case 0x2: // SLTI
                    snprintf(result, buf_size, "slti     %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                case 0x3: // SLTIU
                    snprintf(result, buf_size, "sltiu    %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                case 0x4: // XORI
                    snprintf(result, buf_size, "xori     %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                case 0x5: // SRLI/SRAI
                    if (funct7 == 0x20)
                        snprintf(result, buf_size, "srai     %s,%s,%d", reg_names[rd], reg_names[rs1], rs2);
                    else
                        snprintf(result, buf_size, "srli     %s,%s,%d", reg_names[rd], reg_names[rs1], rs2);
                    break;
                case 0x6: // ORI
                    snprintf(result, buf_size, "ori      %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                case 0x7: // ANDI
                    snprintf(result, buf_size, "andi     %s,%s,%d", reg_names[rd], reg_names[rs1], get_i_imm(instruction));
                    break;
                default:
                    snprintf(result, buf_size, "unknown immediate arithmetic");
                    break;
            }
            break;

        case 0x33: // Register arithmetic
            switch (funct3) {
                case 0x0: // ADD/SUB
                    if (funct7 == 0x20)
                        snprintf(result, buf_size, "sub      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else if (funct7 == 0x01) // MUL
                        snprintf(result, buf_size, "mul      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "add      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x1: // SLL/MULH
                    if (funct7 == 0x01)
                        snprintf(result, buf_size, "mulh     %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "sll      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x2: // SLT
                    snprintf(result, buf_size, "slt      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x3: // SLTU
                    snprintf(result, buf_size, "sltu     %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x4: // XOR/DIV
                    if (funct7 == 0x01)
                        snprintf(result, buf_size, "div      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "xor      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x5: // SRL/SRA/DIVU
                    if (funct7 == 0x20)
                        snprintf(result, buf_size, "sra      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else if (funct7 == 0x01)
                        snprintf(result, buf_size, "divu     %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "srl      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x6: // OR/REM
                    if (funct7 == 0x01)
                        snprintf(result, buf_size, "rem      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "or       %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                case 0x7: // AND/REMU
                    if (funct7 == 0x01)
                        snprintf(result, buf_size, "remu     %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    else
                        snprintf(result, buf_size, "and      %s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    break;
                default:
                    snprintf(result, buf_size, "unknown register arithmetic");
                    break;
            }
            break;

        case 0x73: // ECALL
            if (instruction == 0x73) {
                snprintf(result, buf_size, "ecall");
            } else {
                snprintf(result, buf_size, "unknown system");
            }
            break;

        default:
            snprintf(result, buf_size, "unknown instruction 0x%08x", instruction);
            break;
    }
}