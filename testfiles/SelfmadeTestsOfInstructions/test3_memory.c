void _start() {
    // Setup a memory area
    int memory[16];
    char *bytes = (char*)memory;
    
    // Test word operations
    memory[0] = 0x12345678;    // SW
    int w = memory[0];         // LW

    // Test halfword operations
    short *halfs = (short*)memory;
    halfs[0] = 0x1234;         // SH
    short h = halfs[0];        // LH
    unsigned short uh = halfs[0];  // LHU

    // Test byte operations
    bytes[0] = 0x12;           // SB
    char b = bytes[0];         // LB
    unsigned char ub = bytes[0];   // LBU

    // Exit
    register int a7 asm("a7") = 3;
    asm volatile ("ecall");
}