void _start() {
    int result = 0;
    int a = 123;
    int b = 456;

    // Test MUL
    result = a * b;        // MUL

    // Force MULH with large numbers
    int x = 0x7fffffff;
    int y = 0x7fffffff;
    result = (int)((long long)x * y >> 32);  // MULH

    // Test DIV
    result = a / b;        // DIV
    result = -a / b;       // DIV negative

    // Test DIVU
    unsigned ua = a;
    unsigned ub = b;
    result = ua / ub;      // DIVU

    // Test REM
    result = a % b;        // REM
    result = -a % b;       // REM negative

    // Test REMU
    result = ua % ub;      // REMU

    // Exit
    register int a7 asm("a7") = 3;
    asm volatile ("ecall");
}