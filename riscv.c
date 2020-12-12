/********************************************************************
 * Part of Mike Field's emulate-risc-v project.
 *
 * (c) 2018 Mike Field <hamster@snap.net.nz>
 *
 * See https://github.com/hamsternz/emulate-risc-v for licensing
 * and additional info
 *
 ********************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "riscv.h"
#include "display.h"
#include "string.h"
#include "memory.h"

#define ALLOW_RV32M 1

#define CSR_MCYCLE     (0xB00)
#define CSR_RDCYCLE    (0xC00)
#define CSR_RDTIME     (0xC01)
#define CSR_RDINSTRET  (0xC02) 
#define CSR_RDCYCLEH   (0xC83)
#define CSR_RDTIMEH    (0xC81)
#define CSR_RDINSTRETH (0xC82) 
#define CSR_MCPUID     (0xF00)
#define CSR_MIMPID     (0xF01)

/* CPU State info */
uint32_t csr[0x1000];
uint32_t regs[32];
uint32_t pc;
/* Processor state outside of registers */
static uint8_t  stalled;
static uint8_t  read_dispatched;
static uint8_t  fetch_in_progress;

/* Misc info */
uint32_t stalled_count;
int trace_active = 1;


#define ALU_ADD            ( 0)
#define ALU_SUB            ( 1)
#define ALU_SLL            ( 2)
#define ALU_SEQ            ( 3)
#define ALU_SLT            ( 4)
#define ALU_SLTU           ( 5)
#define ALU_XOR            ( 6)
#define ALU_SRL            ( 7)
#define ALU_SRA            ( 8)
#define ALU_OR             ( 9)
#define ALU_AND            (10)
#define ALU_NEXT_I         (11)
#define ALU_PC_U20         (12)
#define ALU_U20            (13)
#define ALU_MUL            (14)
#define ALU_MULH           (15)
#define ALU_MULHSU         (16)
#define ALU_MULHU          (17)
#define ALU_DIV            (18)
#define ALU_DIVU           (19)
#define ALU_REM            (20)
#define ALU_REMU           (21)
#define ALU_CSR            (22)
#define ALU_NUL            (99) // Just so people don't get confused

#define MEM_NONE  (0)
#define MEM_LOAD  (1)
#define MEM_STORE (2)

#define CSR_NOP  (0)
#define CSR_RW   (1)
#define CSR_RS   (2)
#define CSR_RC   (3)
#define CSR_RWI  (4)
#define CSR_RSI  (5)
#define CSR_RCI  (6)

#define PC_NEXT_I         (0)
#define PC_COND_JUMP      (1)
#define PC_COND_JUMP_INV  (2)
#define PC_REL_JUMP       (3)
#define PC_INDIRECT       (4)
#define PC_STALLED        (6)

/* Breaking the instruction into fields */
static uint32_t current_instr;
static uint8_t rs1, rs2, rd, func3;
static int32_t jmpoffset, broffset, imm12wr, imm12;
static uint32_t upper20, upper7, csrid, uimm, shamt;

/* Function to store the trace in the trace buffer */
static void trace(char *fmt, uint32_t a, uint32_t b, uint32_t c);
static void exception( char *reason);

/* Functions for running opcodes */
static int op_unified(void);
static int op_auipc(void)   { trace("AUIPC  r%u, x%08x",    rd,  upper20,   0);        return op_unified(); }
static int op_lui(void)     { trace("LUI    r%u, x%08x",    rd,  upper20,   0);        return op_unified(); }
static int op_jal(void)     { trace("JAL    r%u, %i",       rd,  jmpoffset, 0);        return op_unified(); }
static int op_jalr(void)    { trace("JALR   r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); }
static int op_fence(void)   { trace("FENCE",                0,   0,         0);        return op_unified(); }
static int op_fence_i(void) { trace("FENCEI",               0,   0,         0);        return op_unified(); }
static int op_beq(void)     { trace("BEQ    r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); }
static int op_bne(void)     { trace("BNE    r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); }
static int op_blt(void)     { trace("BLT    r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); }
static int op_bltu(void)    { trace("BLTU   r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); } 
static int op_bge(void)     { trace("BGE    r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); }
static int op_bgeu(void)    { trace("BGEU   r%i, r%i, %i",  rs1, rs2,       broffset); return op_unified(); } 
static int op_add(void)     { trace("ADD    r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_addi(void)    { trace("ADDI   r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_andi(void)    { trace("ADDI   r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_or(void)      { trace("OR     r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_ori(void)     { trace("ORI    r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_xor(void)     { trace("XOR    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_xori(void)    { trace("XORI   r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_and(void)     { trace("AND    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_sub(void)     { trace("SUB    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_slli(void)    { trace("SLLI   r%u, r%u, %i",  rd,  rs1,       shamt);    return op_unified(); }
static int op_slt(void)     { trace("SLT    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_slti(void)    { trace("SLTI   r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_sltiu(void)   { trace("SLUI   r%u, r%u, %i",  rd,  rs1,       imm12);    return op_unified(); }
static int op_srl(void)     { trace("SRL    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_srli(void)    { trace("SRLI   r%u, r%u, %i",  rd,  rs1,       shamt);    return op_unified(); }
static int op_sltu(void)    { trace("SLU    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_sra(void)     { trace("SRA    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }
static int op_srai(void)    { trace("SRAI   r%u, r%u, %i",  rd,  rs1,       shamt);    return op_unified(); }
static int op_sll(void)     { trace("SLL    r%u, r%u, r%u", rd,  rs1,       rs2);      return op_unified(); }

static int op_csrrw(void)   { trace("CSRRW  r%u, r%u, %i",  rd,  rs1,       csrid);    return op_unified(); }
static int op_csrrs(void)   { trace("CSRRS  r%u, r%u, %i",  rd,  rs1,       csrid);    return op_unified(); }
static int op_csrrc(void)   { trace("CSRRS  r%u, r%u, %i",  rd,  rs1,       csrid);    return op_unified(); }
static int op_csrrwi(void)  { trace("CSRRWI r%u, r%u, %i",  rd,  uimm,      csrid);    return op_unified(); }
static int op_csrrsi(void)  { trace("CSRRSI r%u, r%u, %i",  rd,  uimm,      csrid);    return op_unified(); }
static int op_csrrci(void)  { trace("CSRRCI r%u, r%u, %i",  rd,  uimm,      csrid);    return op_unified(); }
#ifdef ALLOW_RV32M
static int op_mul(void)     { trace("MUL    r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_mulh(void)    { trace("MULH   r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_mulhsu(void)  { trace("MULHUS r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_mulhu(void)   { trace("MULHUS r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_div(void)     { trace("DIV    r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_divu(void)    { trace("DIVU   r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_rem(void)     { trace("REM    r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
static int op_remu(void)    { trace("REMU   r%u, r%u, %i",  rd,  rs1,       rs2);      return op_unified(); }
#endif
static int op_sb(void)      { trace("SB     r%u+%i, r%u",   rs1, imm12wr,   rs2);      return op_unified(); }
static int op_sh(void)      { trace("SH     r%u+%i, r%u",   rs1, imm12wr,   rs2);      return op_unified(); }
static int op_sw(void)      { trace("SW     r%u+%i, r%u",   rs1, imm12wr,   rs2);      return op_unified(); }
static int op_lb(void)      { trace("LB     r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); } 
static int op_lh(void)      { trace("LH     r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); }
static int op_lw(void)      { trace("LW     r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); }
static int op_lbu(void)     { trace("LBU    r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); }
static int op_lhu(void)     { trace("LHU    r%u, r%u + %i", rd,  rs1,       imm12);    return op_unified(); }

static int op_ecall(void)   { trace("ECALL",       0,            0,0); exception("Unknown Opcode exception"); return 0; }
static int op_ebreak(void)  { trace("EBREAK",      0,            0,0); exception("Unknown Opcode exception"); return 0; }
static int op_unknown(void) { trace("???? (%08x)", current_instr,0,0); exception("Unknown Opcode exception"); return 0; }

struct opcode_entry { 
  char *spec;
  int (*func)(void);
  uint8_t  op2_immediate;
  uint8_t  alu_mode;
  uint8_t  store_result;
  uint8_t  pc_mode;
  uint8_t  csr_mode;
  uint8_t  memory_mode;
  uint32_t memory_mask;
  uint32_t load_sign_check;
  uint32_t value;
  uint32_t mask;
} opcodes[] = {                             // immed op2 , ALU, store, pc_mode,     CSR Update
   {"-------------------------0010111", op_auipc,    0, ALU_PC_U20,  1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-------------------------0110111", op_lui,      0, ALU_U20,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-------------------------1101111", op_jal,      0, ALU_NEXT_I,  1, PC_REL_JUMP,      CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------000-----1100111", op_jalr,     0, ALU_NEXT_I,  1, PC_INDIRECT,      CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},

   {"-----------------000-----1100011", op_beq,      0, ALU_SEQ,     0, PC_COND_JUMP,     CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------001-----1100011", op_bne,      0, ALU_SEQ,     0, PC_COND_JUMP_INV, CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------100-----1100011", op_blt,      0, ALU_SLT,     0, PC_COND_JUMP,     CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------101-----1100011", op_bge,      0, ALU_SLT,     0, PC_COND_JUMP_INV, CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------110-----1100011", op_bltu,     0, ALU_SLTU,    0, PC_COND_JUMP,     CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------111-----1100011", op_bgeu,     0, ALU_SLTU,    0, PC_COND_JUMP_INV, CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},

   {"-----------------000-----0000011", op_lb,       0, ALU_NUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_LOAD, 0x000000FF, 0x00000080},
   {"-----------------001-----0000011", op_lh,       0, ALU_NUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_LOAD, 0x0000FFFF, 0x00008000},
   {"-----------------010-----0000011", op_lw,       0, ALU_NUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_LOAD, 0xFFFFFFFF, 0x00000000},
   {"-----------------100-----0000011", op_lbu,      0, ALU_NUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_LOAD, 0x000000FF, 0x00000000},
   {"-----------------101-----0000011", op_lhu,      0, ALU_NUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_LOAD, 0x0000FFFF, 0x00000000},

   {"-----------------000-----0100011", op_sb,       0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_STORE, 0x000000FF, 0x00000000},
   {"-----------------001-----0100011", op_sh,       0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_STORE, 0x0000FFFF, 0x00000000},
   {"-----------------010-----0100011", op_sw,       0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_STORE, 0xFFFFFFFF, 0x00000000},

   {"-----------------000-----0010011", op_addi,     1, ALU_ADD,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------010-----0010011", op_slti,     1, ALU_SLT,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------011-----0010011", op_sltiu,    1, ALU_SLTU,    1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------100-----0010011", op_xori,     1, ALU_XOR,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------110-----0010011", op_ori,      1, ALU_OR,      1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------111-----0010011", op_andi,     1, ALU_AND,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------001-----0010011", op_slli,     1, ALU_SLL,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------101-----0010011", op_srli,     1, ALU_SRL,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0100000----------101-----0010011", op_srai,     1, ALU_SRA,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
             
   {"0000000----------000-----0110011", op_add,      0, ALU_ADD,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0100000----------000-----0110011", op_sub,      0, ALU_SUB,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------001-----0110011", op_sll,      0, ALU_SLL,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------010-----0110011", op_slt,      0, ALU_SLT,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------011-----0110011", op_sltu,     0, ALU_SLTU,    1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------100-----0110011", op_xor,      0, ALU_XOR,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------101-----0110011", op_srl,      0, ALU_SRL,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0100000----------101-----0110011", op_sra,      0, ALU_SRA,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------110-----0110011", op_or,       0, ALU_OR,      1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000000----------111-----0110011", op_and,      0, ALU_AND,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},

   {"0000--------00000000000000001111", op_fence,    0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"00000000000000000001000000001111", op_fence_i,  0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},

   {"00000000000000000000000001110011", op_ecall,    0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"00000000000100000000000001110011", op_ebreak,   0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},

   {"-----------------001-----1110011", op_csrrw,    0, ALU_CSR,     1, PC_NEXT_I,        CSR_RW,   MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------010-----1110011", op_csrrs,    0, ALU_CSR,     1, PC_NEXT_I,        CSR_RS,   MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------011-----1110011", op_csrrc,    0, ALU_CSR,     1, PC_NEXT_I,        CSR_RC,   MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------101-----1110011", op_csrrwi,   0, ALU_CSR,     1, PC_NEXT_I,        CSR_RWI,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------110-----1110011", op_csrrsi,   0, ALU_CSR,     1, PC_NEXT_I,        CSR_RSI,  MEM_NONE, 0x00000000, 0x00000000},
   {"-----------------111-----1110011", op_csrrci,   0, ALU_CSR,     1, PC_NEXT_I,        CSR_RCI,  MEM_NONE, 0x00000000, 0x00000000},
#ifdef ALLOW_RV32M
   {"0000001----------000-----0110011", op_mul,      0, ALU_MUL,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   // RV32M instructions  
   {"0000001----------001-----0110011", op_mulh,     0, ALU_MULH,    1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------010-----0110011", op_mulhsu,   0, ALU_MULHSU,  1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------011-----0110011", op_mulhu,    0, ALU_MULHU,   1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------100-----0110011", op_div,      0, ALU_DIV,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------101-----0110011", op_divu,     0, ALU_DIVU,    1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------110-----0110011", op_rem,      0, ALU_REM,     1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
   {"0000001----------111-----0110011", op_remu,     0, ALU_REMU,    1, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000},
#endif
   {"--------------------------------", op_unknown,  0, ALU_NUL,     0, PC_NEXT_I,        CSR_NOP,  MEM_NONE, 0x00000000, 0x00000000}
};
struct opcode_entry *op;


/****************************************************************************/
uint32_t riscv_pc(void) {
  return pc;
}
/****************************************************************************/
uint32_t riscv_cycle_count(void) {
  return csr[CSR_RDCYCLE];
}
/****************************************************************************/
uint32_t riscv_stalled_count(void) {
  return stalled_count;
}
/****************************************************************************/
uint32_t riscv_reg(int i) {
  if(i > 31 || i < 0) 
    return 0;
  return regs[i];
}
/****************************************************************************/
static int decode(void) {
  int32_t broffset_12_12, broffset_11_11, broffset_10_05, broffset_04_01;
  int32_t jmpoffset_20_20, jmpoffset_19_12, jmpoffset_11_11, jmpoffset_10_01;
  int valid = 1;
  if((current_instr & 0x3) != 3) {
    valid = 0;
  }
  csrid   = (current_instr >> 20);
  rs1     = (current_instr >> 15) & 0x1f ;
  rs2     = (current_instr >> 20) & 0x1F;
  rd      = (current_instr >>  7) & 0x1f;
  uimm    = (current_instr >> 15) & 0x1f;
  shamt   = (current_instr >> 20) & 0x1f;
  upper20 = current_instr & 0xFFFFF000;
  imm12   = ((int32_t)current_instr) >> 20;
  upper7  = (current_instr >> 25) & 0x7F;
  func3   = (current_instr >> 12) & 0x7;

  jmpoffset_20_20 = (int32_t)(current_instr & 0x80000000)>>11;
  jmpoffset_19_12 = (current_instr & 0x000FF000);
  jmpoffset_11_11 = (current_instr & 0x00100000) >>  9;
  jmpoffset_10_01 = (current_instr & 0x7FE00000) >> 20;
  jmpoffset       = jmpoffset_20_20 | jmpoffset_19_12 | jmpoffset_11_11 | jmpoffset_10_01;

  broffset_12_12 = (int)(current_instr & 0x80000000) >> 19;
  broffset_11_11 = (current_instr & 0x00000080) << 4;
  broffset_10_05 = (current_instr & 0x7E000000) >> 20;
  broffset_04_01 = (current_instr & 0x00000F00) >> 7;
  broffset       = broffset_12_12 | broffset_11_11 | broffset_10_05 | broffset_04_01;

  imm12wr   =  current_instr; /* Note - becomes signed */
  imm12wr >>= 20;
  imm12wr  &= 0xFFFFFFE0;
  imm12wr  |= (current_instr >> 7)  & 0x1f;
  read_dispatched = 0;

  return valid;
}

/****************************************************************************/
static void exception( char *reason) {
  char buffer[200];
  if(strlen(reason) < 100)
    sprintf(buffer, "EXCEPTION: %s : instruction 0x%08x", reason, current_instr);
  else
    sprintf(buffer, "EXCEPTION: [reason too long] : instruction 0x%08x", current_instr);
  display_log(reason);
}	

/****************************************************************************/
static void trace(char *fmt, uint32_t a, uint32_t b, uint32_t c) {
  char buffer[128];
  if(!trace_active)
    return;
  sprintf(buffer,"%08X:%c",pc, stalled ? '*' : ' ');
  sprintf(buffer+10, fmt, a, b, c);
  display_trace(buffer);
}	

/****************************************************************************/
void riscv_reset(void) {
  memory_reset();
  memset(regs,0xFF,sizeof(regs));
  regs[0] = 0;
  pc = 0x20400000;
  display_log("RISC-V reset");
}

/****************************************************************************/
int riscv_initialise(void) {
  int i;
  for(i = 0; i < sizeof(opcodes)/sizeof(struct opcode_entry); i++) {
     int j;
     if(strlen(opcodes[i].spec) != 32) {
       return 0;
     }
     opcodes[i].mask = 0;
     opcodes[i].value = 0;
     for(j = 0; j < 32; j++) {
       opcodes[i].value <<= 1;
       opcodes[i].mask  <<= 1;

       /* Change the string specifiers into masks */
       switch(opcodes[i].spec[j]) {
          case '0':
             opcodes[i].mask  |= 1;
             break;
          case '1':
             opcodes[i].mask  |= 1;
             opcodes[i].value |= 1;
             break;
          case '-':
             break;
          default:
	     display_log("Unknown character in opcode");
	     return 0;
        }
     }
  }
  return 1;
}
/****************************************************************************/
static int op_unified(void) {
  uint32_t op1, op2, res, csr_res; 
  uint32_t pc_next_i, pc_cond_jump, pc_rel_jump, pc_indirect; 

  /*******************************************************
   * Build local variables based on global state 
   ******************************************************/
  /* Options for next PC value */
  pc_next_i    = pc + 4;
  pc_cond_jump = pc + broffset;
  pc_rel_jump  = pc + jmpoffset;
  pc_indirect  = (regs[rs1] + imm12) & (~1);

  /* Operands */
  op1 = regs[rs1];
  op2 = op->op2_immediate ? imm12 : regs[rs2];

  /* Find the results */
  switch(op->alu_mode) {
    case ALU_ADD:    res = op1 + op2;                                               break;
    case ALU_SUB:    res = op1 - op2;                                               break;
    case ALU_SLL:    res = op1 << (op2 & 0x1f);                                     break;
    case ALU_XOR:    res = op1 ^ op2;                                               break;
    case ALU_SRL:    res = (uint32_t)op1 >> (op2 & 0x1f);                           break;
    case ALU_SRA:    res =  (int32_t)op1 >> (op2 & 0x1f);                           break;
    case ALU_OR:     res = op1 | op2;                                               break;
    case ALU_AND:    res = op1 & op2;                                               break;
  
    case ALU_SEQ:    res = (op1 == op2)                     ? 1 : 0;                break;
    case ALU_SLT:    res = ((int32_t)  op1 <  (int32_t)op2) ? 1 : 0;                break;
    case ALU_SLTU:   res = ((uint32_t) op1 < (uint32_t)op2) ? 1 : 0;                break;

    case ALU_MUL:    res = ((uint64_t)op1 * (uint64_t)op2);                         break;
    case ALU_MULH:   res = ( (int64_t)op1 *  (int64_t)op2) >> 32;                   break;
    case ALU_MULHSU: res = ( (int64_t)op1 * (uint64_t)op2) >> 32;                   break;
    case ALU_MULHU:  res = ((uint64_t)op1 * (uint64_t)op2) >> 32;                   break;
    case ALU_DIV:    res = (op2 == 0) ? 0xFFFFFFFF : (int32_t)op1 / (int32_t)op2;   break;
    case ALU_DIVU:   res = (op2 == 0) ? 0xFFFFFFFF : (uint32_t)op1 / (uint32_t)op2; break;
    case ALU_REM:    res = (op2 == 0) ? 0xFFFFFFFF : (int32_t)op1 % (int32_t)op2;   break;
    case ALU_REMU:   res = (op2 == 0) ? 0xFFFFFFFF : (uint32_t)op1 % (uint32_t)op2; break;

    // Maybe seperate
    case ALU_NEXT_I: res = pc_next_i;                                             break;
    case ALU_PC_U20: res = pc + upper20;                                          break;
    case ALU_U20:    res = upper20;                                               break;
    case ALU_CSR:    res = csr[csrid];                                            break;
    default:         res = 0;                                                     break; 
  }

  switch(op->csr_mode) {
    case CSR_RW:  csr_res = regs[rs1];               break;
    case CSR_RS:  csr_res = csr[csrid] | regs[rs1];  break;
    case CSR_RC:  csr_res = csr[csrid] & ~regs[rs1]; break;
    case CSR_RWI: csr_res = uimm;                    break;
    case CSR_RSI: csr_res = csr[csrid] | uimm;       break;
    case CSR_RCI: csr_res = csr[csrid] & ~uimm;      break;
    default:      csr_res = 0;                       break;
  }

  if(op->csr_mode != CSR_NOP) { 
    char buffer[100];
    sprintf(buffer,"CSR 0x%03x accessed",csrid);
    display_log(buffer);
  }

  /* And now do the write */
  if(op->memory_mode == MEM_STORE) {
    if(memory_write_full()) {
      stalled = 1;
    } else {
      uint32_t addr;
      int unaligned = 0;
      addr = regs[rs1]+imm12wr;
      stalled = 0;

      switch(addr & 3) {
        case 1:
          if(op->memory_mask  == 0xFFFFFFFF) {
            unaligned = 1;
          }
          break;
        case 2:
          if(op->memory_mask  == 0xFFFFFFFF) {
            unaligned = 1;
          }
          break;
        case 3:
          if(op->memory_mask  != 0xFF) {
            unaligned = 1;
          }
          break;
      }
      if(unaligned) {
        char buffer[100];
        sprintf(buffer,"Unaligned write at %08x %08x",addr, op->memory_mask);
        display_log(buffer);
      }

      if(!memory_write_request(addr, op->memory_mask, regs[rs2])) {
        return 0;
      }
    }
  }
  
  /* do we need to do a load? */
  if(op->memory_mode == MEM_LOAD) {
    if(rd != 0) {
      if(!read_dispatched) {
        uint32_t addr;
        int unaligned = 0;
        addr = regs[rs1]+imm12;
        stalled = 1;

        switch(addr & 3) {
          case 1:
            if(op->memory_mask  == 0xFFFFFFFF) {
              unaligned = 1;
            }
            break;
          case 2:
            if(op->memory_mask  == 0xFFFFFFFF) {
              unaligned = 1;
            }
            break;
          case 3:
            if(op->memory_mask  != 0xFF) {
              unaligned = 1;
            }
            break;
        }
        if(unaligned) {
          char buffer[100];
          sprintf(buffer,"Unaligned read at %08x %08x",addr, op->memory_mask);
          display_log(buffer);
        }

        if(memory_read_request(regs[rs1]+imm12)) {
          read_dispatched = 1;
        } 
        /* Unable to queue request -  will retry */
      } else {
        /* To get here we are stalled waiting for data */
        if(!memory_read_data_empty())  {
          stalled = 0;
          res = memory_read_data() & op->memory_mask;
          /* Sign extend */
          res |= (res & op->load_sign_check) ? ~op->memory_mask : 0;
        }
      }
    }
  }

  /******************************************************************
   * All assignments from local to global state variables below here
   *****************************************************************/

  /* Store the results? */
  if(!stalled) {
    if(op->store_result && rd != 0)
      regs[rd] = res;

    /* Any CSR updates? */
    switch(op->csr_mode) {
      case CSR_RW:  if(rs1 != 0) csr[csrid] = csr_res;  break;
      case CSR_RS:  if(rs1 != 0) csr[csrid] = csr_res;  break;
      case CSR_RC:  if(rs1 != 0) csr[csrid] = csr_res;  break;
      case CSR_RWI: csr[csrid] = csr_res;               break;
      case CSR_RSI: csr[csrid] = csr_res;               break;
      case CSR_RCI: csr[csrid] = csr_res;               break;
      default:                                          break;
    }

    /* Which instruction next? */
    switch(op->pc_mode) {
      case PC_NEXT_I:        pc = pc_next_i;                      break;
      case PC_COND_JUMP:     pc = res ? pc_cond_jump : pc_next_i; break;
      case PC_COND_JUMP_INV: pc = res ? pc_next_i : pc_cond_jump; break;
      case PC_REL_JUMP:      pc = pc_rel_jump;                    break;
      case PC_INDIRECT:      pc = pc_indirect;                    break;
      default:                                                    break;
    }
  }
  return 1;
}

/****************************************************************************/
static int do_op(void) {
  int i;
  if((pc & 3) != 0) {
    display_log("Attempt to execute unaligned code");
    return 0;
  }

  if(!stalled) {
    /* Fetch */
    if(!fetch_in_progress) {
      if(!memory_fetch_request(pc)) {
        display_log("Unable to fetch instruction");
        return 0;
      }
      fetch_in_progress = 1;
    } else if(!memory_fetch_data_empty()) {
      fetch_in_progress = 0;

      current_instr = memory_fetch_data();
      /* Decode */
      if(!decode()) 
        return 0;
    }
  } 

  if(stalled || fetch_in_progress) {
    stalled_count++;
  }

  if(fetch_in_progress) {
    //display_trace("Fetch in progress");
    return 1;
  }

  /* Execute */
  for(i = 0; i < sizeof(opcodes)/sizeof(struct opcode_entry); i++) {
     if((current_instr & opcodes[i].mask) == opcodes[i].value) {
       op = opcodes+i;
       return op->func();
    }
  }
  return 0;
}

/****************************************************************************/
uint32_t riscv_cycle_count_l(void) {
  return csr[CSR_RDCYCLE];
}

/****************************************************************************/
uint32_t riscv_cycle_count_h(void) {
  return csr[CSR_RDCYCLEH];
}

/****************************************************************************/
int riscv_run(void) {
  ///////////////////////////////////////
  // Update counters 
  ////////////////////////////////////
  csr[CSR_RDCYCLE]++;
  if(csr[CSR_RDCYCLE] == 0)
    csr[CSR_RDCYCLEH]++;
  csr[CSR_MCYCLE] = csr[CSR_RDCYCLE];

  if(do_op()) {
    csr[CSR_RDTIME]++;
    if(csr[CSR_RDTIME] == 0)
      csr[CSR_RDTIMEH]++;
    return 1;
  } else {
    char buffer[100];
    sprintf(buffer,"Instruction : %08x",current_instr);
    display_log(buffer);
  }
  return 0;
}
/****************************************************************************/
void riscv_dump(void) {
#if 0
  int i;
  printf("=========================================================================================================\n");
  printf("Dumping RISC-V registers\n");
  for(i = 0; i < 8; i++) {
    printf("r%02i: %08x    r%02i: %08x    r%02i: %08x    r%02i: %08x\n",
            i, regs[i], i+8, regs[i+8], i+16, regs[i+16], i+24, regs[i+24]);
  }
  printf("\n");
  printf("pc:  %08x\n", pc);
#endif
}
/****************************************************************************/
void riscv_finish(void) {
}
/****************************************************************************/
