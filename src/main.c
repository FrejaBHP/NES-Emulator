#include <emulator.h>

// Convert HEX to BIN:
// objcopy --input-target=ihex --output-target=binary testhex.hex testhex.bin


/* TO-DO
    CPU:
    Look at overflow flag
    Figure out how overflow and carry works for ADC + SBC -> in progress?
    Fully implement LDA, LDX and LDY - only have AM_Accumulator
*/ 

int main() {
    TestFnc();
}
