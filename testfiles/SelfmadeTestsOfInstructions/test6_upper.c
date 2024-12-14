void _start() {
    // Test LUI (load upper immediate)
    int large = 0x12345000;    // Will generate LUI

    // Test AUIPC
    asm volatile (
        "auipc t0, 0x1\n"      // Force AUIPC instruction
    );

    // Exit
    register int a7 asm("a7") = 3;
    asm volatile ("ecall");
}