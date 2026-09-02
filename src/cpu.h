#ifndef def_CPU
#define def_CPU

#include <stdint.h>
#include <stdio.h>

#define Stack_Start                 0x0100U
#define InternalRAM_Size            0x0800U
#define PPU_Size                    0x0008U

#define PPU_Start                   0x2000U
#define APU_Start                   0x4000U
#define Unmapped_Start              0x4020U
#define ROM_Start                   0x8000U

// PPU registers in CPU addressing space

#define PPU_PPUCTRL                 0x2000U
#define PPU_PPUMASK                 0x2001U
#define PPU_PPUSTATUS               0x2002U
#define PPU_OAMADDR                 0x2003U
#define PPU_OAMDATA                 0x2004U
#define PPU_PPUSCROLL               0x2005U
#define PPU_PPUADDR                 0x2006U
#define PPU_PPUDATA                 0x2007U

// APU and I/O registers in CPU addressing space

#define SQ1_VOL                     0x4000U     // Duty cycle and volume/envelope
#define SQ1_SWEEP                   0x4001U     // Sweep control register
#define SQ1_LO                      0x4002U     // Low byte of period
#define SQ1_HI                      0x4003U     // High byte of period and length counter value
#define SQ2_VOL                     0x4004U     // Duty cycle and volume/envelope
#define SQ2_SWEEP                   0x4005U     // Sweep control register
#define SQ2_LO                      0x4006U     // Low byte of period
#define SQ2_HI                      0x4007U     // High byte of period and length counter value

#define TRI_LINEAR                  0x4008U     // Linear counter
#define TRI_UNUSED                  0x4009U     // Unused
#define TRI_LO                      0x400AU     // Low byte of period
#define TRI_HI                      0x400BU     // High byte of period and length counter value

#define NOISE_VOL                   0x400CU     // Volume
#define NOISE_UNUSED                0x400DU     // Unused
#define NOISE_LO                    0x400EU     // Period and waveform shape
#define NOISE_HI                    0x400FU     // Length counter value

#define DMC_FREQ                    0x4010U     // IRQ enable (I), Looping (L), and Frequency (R) (IL-- RRRR)
#define DMC_RAW                     0x4011U     // Load counter (D) (-DDD DDDD)
#define DMC_START                   0x4012U     // Sample address (A) (AAAA AAAA)
#define DMC_LEN                     0x4013U     // Sample length (L) (LLLL LLLL)

#define OAMDMA                      0x4014U
#define SND_CHN                     0x4015U     // Sound channels, but also IRQ status. Internal register, so does not update data bus
#define JOY0                        0x4016U     // First controller input
#define JOY1                        0x4017U     // Second controller input, doubles as APU Frame Counter


typedef struct CPU {
    uint16_t PC;
    uint8_t SP; // Low 8 bits of the next free location on the stack. Pushing bytes -> decrement, pulling bytes -> inccrement. Not bounds checked
    uint8_t Accumulator;
    uint8_t RegX;
    uint8_t RegY;
    uint8_t Status;
    uint8_t DataBus;
} CPU;

typedef enum AddrMode {
    AM_Implied,
    AM_Accumulator,
    AM_Relative,
    AM_Immediate,

    AM_ZeroPage,
    AM_ZeroPageX,
    AM_ZeroPageY,

    AM_Absolute,
    AM_AbsoluteX,
    AM_AbsoluteY,

    AM_Indirect,
    AM_IndirectX,
    AM_IndirectY
} AddrMode;

typedef enum BitwiseOp {
    BOP_AND,
    BOP_OR,
    BOP_XOR
} BitwiseOp;

extern CPU* CCPU;
extern uint8_t* CPUMemory;

extern uint8_t JOY0Latch;
extern uint8_t JOY1Latch;

extern uint8_t IsExecutingInstruction;

void CPUInit();

void DumpMemory();
void DumpWriteLine(FILE* file, uint16_t startAddr);
void DumpWriteLineHalf(FILE* file, uint16_t startAddr);

void PushByte(uint8_t value);
uint8_t PopByte();

uint16_t AssembleAbsoluteAddress(uint8_t first, uint8_t second);
uint8_t GetHighByte(uint16_t value);
uint8_t GetLowByte(uint16_t value);

uint8_t FlipByteSign(uint8_t value);

uint8_t SetBit(uint8_t value, uint8_t bit);
uint8_t ClearBit(uint8_t value, uint8_t bit);
uint8_t CheckBit(uint8_t value, uint8_t bit);
void OverrideBit8(uint8_t* addr, uint8_t bit, uint8_t value);
void OverrideBit16(uint16_t* addr, uint8_t bit, uint8_t value);

uint8_t GetZeroPage(uint8_t index);
uint8_t GetZeroPageX(uint8_t index);
uint8_t GetZeroPageY(uint8_t index);

void StoreZeroPage(uint8_t index, const uint8_t value);
void StoreZeroPageX(uint8_t index, const uint8_t value);
void StoreZeroPageY(uint8_t index, const uint8_t value);

uint8_t GetAbsolute(uint16_t index);
uint8_t GetAbsoluteX(uint16_t index);
uint8_t GetAbsoluteY(uint16_t index);

void StoreAbsolute(uint16_t index, const uint8_t value);
void StoreAbsoluteX(uint16_t index, const uint8_t value);
void StoreAbsoluteY(uint16_t index, const uint8_t value);

uint16_t GetIndirectAddr(uint16_t lookupAddr);
uint16_t GetIndirectZPAddrX(uint8_t zpAddr);
uint16_t GetIndirectZPAddrY(uint8_t zpAddr);

uint8_t IsPageCrossed8(uint8_t prev, uint8_t next);
uint8_t IsPageCrossed16(uint16_t prev, uint16_t next);

void GoToIRQ();
void TriggerNMI();

void CheckForInterrupts();

void ReadPPUSTATUS();
void ReadPPUDATA();

void WriteToPPUCTRL();
void WriteToPPUSCROLL();
void WriteToOAMDATA();
void WriteToPPUADDR();
void WriteToPPUDATA();
void WriteToOAMDMA();

void CheckSetSFZero(uint8_t value);
void CheckSetSFNegative(uint8_t value);

uint8_t ReadProgramByte();
void ExecuteInstruction(uint8_t opcode);

void BitwiseAccInstruction(AddrMode am, BitwiseOp op);
void BranchInstruction(uint8_t regpos, uint8_t sign); // sign = 1 (set)/0 (clear)

void ADC(AddrMode am);

void AND(AddrMode am);

void ASL(AddrMode am);

void BCC();
void BCS();
void BEQ();
void BMI();
void BNE();
void BPL();
void BVC();
void BVS();

void BIT(AddrMode am);

void BRK();

void CMP(AddrMode am);
void CPX(AddrMode am);
void CPY(AddrMode am);

void DEC(AddrMode am);
void DEX();
void DEY();

void EOR(AddrMode am);

void INC(AddrMode am);
void INX();
void INY();

void JMP(AddrMode am);
void JSR();

void LDA(AddrMode am);
void LDX(AddrMode am);
void LDY(AddrMode am);

void LSR(AddrMode am);

void ORA(AddrMode am);

void PHA();
void PHP();

void PLA();
void PLP();

void ROL(AddrMode am);
void ROR(AddrMode am);

void RTI();
void RTS();

void SBC(AddrMode am);

void TAX();
void TAY();
void TXA();
void TYA();

void TSX();
void TXS();

void STA(AddrMode am);
void STX(AddrMode am);
void STY(AddrMode am);

void SEC();
void CLC();

void SEI();
void CLI();

void SED();
void CLD();

void CLV();

void NOP(AddrMode am);


void SLO(AddrMode am);

#endif
