// test1_arithmetic.c
void _start() {    // Entry point required by simulator
    int result = 0;
    
    // Test ADD, ADDI
    result = 5 + 7;        // ADDI: immediate add
    int a = 10, b = 20;
    result = a + b;        // ADD: register add
    
    // Test SUB
    result = a - b;        // SUB: register subtract
    
    // Test shifts
    result = a << 2;       // SLLI: immediate left shift
    result = a >> 2;       // SRLI: immediate right shift
    result = -a >> 2;      // SRAI: immediate arithmetic right shift
    result = a << b;       // SLL: register left shift
    result = a >> b;       // SRL: register right shift
    result = -a >> b;      // SRA: register arithmetic right shift

    // Test logical operations
    result = a | 0xFF;     // ORI: immediate OR
    result = a & 0xFF;     // ANDI: immediate AND
    result = a ^ 0xFF;     // XORI: immediate XOR
    result = a | b;        // OR: register OR
    result = a & b;        // AND: register AND
    result = a ^ b;        // XOR: register XOR

    // Test comparisons
    result = (a < b);      // SLT: signed compare
    result = (a < 5);      // SLTI: immediate signed compare
    unsigned ua = a;
    unsigned ub = b;
    result = (ua < ub);    // SLTU: unsigned compare
    result = (ua < 5);     // SLTIU: immediate unsigned compare

    // Exit with result
    register int a7 asm("a7") = 3;  // exit syscall
    asm volatile ("ecall");
}