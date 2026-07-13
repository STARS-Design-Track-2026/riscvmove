// sample.c
// A sample C program for the RV32I core

int main() {
    // Pointer to memory address 0x4 (BRAM)
    volatile int *mem_ptr = (volatile int *)0x4;
    
    int a = 15;
    int b = 25;
    int sum = 0;
    
    // Simple loop to test branches/jumps and arithmetic
    for (int i = 0; i < 5; i++) {
        sum += (a + b);
    }
    
    // Store sum (which should be 200, or 0xC8) to address 4
    *mem_ptr = sum;
    
    return 0;
}
