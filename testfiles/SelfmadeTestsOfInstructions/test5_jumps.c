void function_target() {
    // Target for JAL instruction
}

int indirect_target(int x) {
    return x + 1;  // Target for JALR instruction
}

void _start() {
    // Test JAL with direct call
    function_target();

    // Test JALR with indirect call
    int (*fptr)(int) = indirect_target;
    int result = fptr(42);

    // Exit
    register int a7 asm("a7") = 3;
    asm volatile ("ecall");
}