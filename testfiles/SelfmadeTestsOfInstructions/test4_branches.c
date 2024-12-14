void _start() {
    int count = 0;
    int a = 10;
    int b = 20;

    // Test BEQ
    if (a == a) {
        count++;
    }

    // Test BNE
    if (a != b) {
        count++;
    }

    // Test BLT
    if (a < b) {
        count++;
    }

    // Test BGE
    if (b >= a) {
        count++;
    }

    // Test BLTU/BGEU
    unsigned ua = -1;  // Maximum unsigned value
    unsigned ub = 1;
    if (ub < ua) {     // BLTU
        count++;
    }
    if (ua >= ub) {    // BGEU
        count++;
    }

    // Exit
    register int a7 asm("a7") = 3;
    asm volatile ("ecall");
}