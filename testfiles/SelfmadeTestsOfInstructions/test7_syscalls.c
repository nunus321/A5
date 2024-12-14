void _start() {
    // Test getchar (syscall 1)
    register int a7 asm("a7") = 1;
    register int a0 asm("a0");
    asm volatile ("ecall");
    int c = a0;

    // Test putchar (syscall 2)
    a7 = 2;
    a0 = c;
    asm volatile ("ecall");

    // Exit (syscall 3)
    a7 = 3;
    asm volatile ("ecall");
}