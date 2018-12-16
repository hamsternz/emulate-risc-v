#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include "memorymap.h"
#include "riscv.h"
#include "display.h"
#include "string.h"

#define ALLOW_RV32M 1

#define CSR_MCYCLE     (0xB00)
#define CSR_RDCYCLE    (0xC00)
#define CSR_RDTIME     (0xC01)
#define CSR_RDINSTRET  (0xC02) 
#define CSR_RDCYCLEH   (0xC00)
#define CSR_RDTIMEH    (0xC01)
#define CSR_RDINSTRETH (0xC02) 
#define CSR_MCPUID     (0xF00)
#define CSR_MIMPID     (0xF01)

uint32_t csr[0x1000];
uint32_t regs[32];
uint32_t pc;
int trace_active = 1;

static int op_auipc(void);
static int op_lui(void);
static int op_jal(void);
static int op_jalr(void);
static int op_fence(void);
static int op_fence_i(void);

static int op_add(void);
static int op_sub(void);
static int op_and(void);
static int op_xor(void);
static int op_or(void);
static int op_sll(void);
static int op_slt(void);
static int op_sltu(void);
static int op_srl(void);
static int op_sra(void);

static int op_addi(void);
static int op_andi(void);
static int op_xori(void);
static int op_ori(void);
static int op_slli(void);
static int op_slti(void);
static int op_sltiu(void);
static int op_srli(void);
static int op_srai(void);

static int op_beq(void);
static int op_bne(void);
static int op_blt(void);
static int op_bge(void);
static int op_bltu(void);
static int op_bgeu(void);

static int op_lb(void);
static int op_lh(void);
static int op_lw(void);
static int op_lbu(void);
static int op_lhu(void);

static int op_sb(void);
static int op_sh(void);
static int op_sw(void);

static int op_ecall(void);
static int op_ebreak(void);
//static int op_ereturn(uint32_t opcode);

static int op_csrrsi(void);
static int op_csrrci(void);
static int op_csrrwi(void);
static int op_csrrs(void);
static int op_csrrc(void);
static int op_csrrw(void);

#ifdef ALLOW_RV32M
static int op_mul(void);
static int op_mulh(void);
static int op_mulhsu(void);
static int op_mulhu(void);
static int op_div(void);
static int op_divu(void);
static int op_rem(void);
static int op_remu(void);
#endif 

static int op_unknown(void);

struct opcode_entry { 
  char *spec;
  int (*func)(void);
  uint32_t value;
  uint32_t mask;
} opcodes[] = {
   {"-------------------------0010111", op_auipc},
   {"-------------------------0110111", op_lui},
   {"-------------------------1101111", op_jal},
   {"-----------------000-----1100111", op_jalr}, 

   {"-----------------000-----1100011", op_beq},
   {"-----------------001-----1100011", op_bne},
   {"-----------------100-----1100011", op_blt},
   {"-----------------101-----1100011", op_bge},
   {"-----------------110-----1100011", op_bltu},
   {"-----------------111-----1100011", op_bgeu},

   {"-----------------000-----0000011", op_lb},
   {"-----------------001-----0000011", op_lh},
   {"-----------------010-----0000011", op_lw},
   {"-----------------100-----0000011", op_lbu},
   {"-----------------101-----0000011", op_lhu},

   {"-----------------000-----0100011", op_sb},
   {"-----------------001-----0100011", op_sh},
   {"-----------------010-----0100011", op_sw},


   {"-----------------000-----0010011", op_addi},
   {"-----------------010-----0010011", op_slti},
   {"-----------------011-----0010011", op_sltiu},
   {"-----------------100-----0010011", op_xori},
   {"-----------------110-----0010011", op_ori},
   {"-----------------111-----0010011", op_andi},
   {"0000000----------001-----0010011", op_slli},
   {"0000000----------101-----0010011", op_srli},
   {"0100000----------101-----0010011", op_srai},
             
   {"0000000----------000-----0110011", op_add},
   {"0100000----------000-----0110011", op_sub},
   {"0000000----------001-----0110011", op_sll},
   {"0000000----------010-----0110011", op_slt},
   {"0000000----------011-----0110011", op_sltu},
   {"0000000----------100-----0110011", op_xor},
   {"0000000----------101-----0110011", op_srl},
   {"0100000----------101-----0110011", op_sra},
   {"0000000----------110-----0110011", op_or},
   {"0000000----------111-----0110011", op_and},

   {"0000--------00000000000000001111", op_fence},
   {"00000000000000000001000000001111", op_fence_i},

   {"00000000000000000000000001110011", op_ecall},
   {"00000000000100000000000001110011", op_ebreak},

   {"-----------------001-----1110011", op_csrrw},
   {"-----------------010-----1110011", op_csrrs},
   {"-----------------011-----1110011", op_csrrc},
   {"-----------------101-----1110011", op_csrrwi},
   {"-----------------110-----1110011", op_csrrsi},
   {"-----------------111-----1110011", op_csrrci},
#ifdef ALLOW_RV32M
   // RV32M instructions 
   {"0000001----------000-----0110011", op_mul},     //TODO set pattern
   {"0000001----------001-----0110011", op_mulh},    //TODO set pattern
   {"0000001----------010-----0110011", op_mulhsu},  //TODO set pattern
   {"0000001----------011-----0110011", op_mulhu},   //TODO set pattern
   {"0000001----------100-----0110011", op_div},     //TODO set pattern
   {"0000001----------101-----0110011", op_divu},    //TODO set pattern
   {"0000001----------110-----0110011", op_rem},     //TODO set pattern
   {"0000001----------111-----0110011", op_remu},    //TODO set pattern
#endif
   {"--------------------------------", op_unknown}  // Catches all the others
};

static int rs1, rs2, rd;
int32_t jmpoffset;
int32_t broffset;
int32_t imm12wr;
int32_t imm12;
uint32_t upper20;
uint32_t csrid;
uint32_t uimm;
int shamt;
uint32_t current_instr;

/****************************************************************************/
uint32_t riscv_pc(void) {
  return pc;
}
/****************************************************************************/
uint32_t riscv_cycle(void) {
  return csr[CSR_RDCYCLE];
}
/****************************************************************************/
uint32_t riscv_reg(int i) {
  if(i > 31 || i < 0) 
    return 0;
  return regs[i];
}
/****************************************************************************/
static void decode(uint32_t instr) {
  int32_t broffset_12_12, broffset_11_11, broffset_10_05, broffset_04_01;
  int32_t jmpoffset_20_20, jmpoffset_19_12, jmpoffset_11_11, jmpoffset_10_01;
  rs1     = (instr >> 15) & 0x1f ;
  rs2     = (instr >> 20) & 0x1F;
  rd      = (instr >> 7)  & 0x1f;
  csrid   = (instr >> 20);
  uimm    = (instr >> 15) & 0x1f;
  shamt   = (instr >> 20) & 0x1f;
  upper20 = instr & 0xFFFFF000;
  imm12   = ((int32_t)instr) >> 20;

  jmpoffset_20_20 = (int32_t)(instr & 0x80000000)>>11;
  jmpoffset_19_12 = (instr & 0x000FF000);
  jmpoffset_11_11 = (instr & 0x00100000) >>  9;
  jmpoffset_10_01 = (instr & 0x7FE00000) >> 20;
  jmpoffset = jmpoffset_20_20 | jmpoffset_19_12 | jmpoffset_11_11 | jmpoffset_10_01;

  broffset_12_12 = (int)(instr & 0x80000000) >> 19;
  broffset_11_11 = (instr & 0x00000080) << 4;
  broffset_10_05 = (instr & 0x7E000000) >> 20;
  broffset_04_01 = (instr & 0x00000F00) >> 7;
  broffset = broffset_12_12 | broffset_11_11 | broffset_10_05 | broffset_04_01;

  imm12wr   =  instr; /* Note - becomes signed */
  imm12wr  >>= 20;
  imm12wr  &= 0xFFFFFFE0;
  imm12wr  |= (instr >> 7)  & 0x1f;
  current_instr = instr;
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
  sprintf(buffer,"%08X: ",pc);
  sprintf(buffer+10, fmt, a, b, c);
  display_trace(buffer);
}	

/****************************************************************************/
void riscv_reset(void) {
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
static int op_unknown(void) {           trace("???? (%08x)", current_instr,0,0);
  exception("Unknown Opcode exception");
  return 0;
}

/****************************************************************************/
static int op_ecall(void) {                            trace("ECALL",  0,0,0);
  exception("Unknown Opcode exception");
  return 0;
}

/****************************************************************************/
static int op_ebreak(void) {                          trace("EBREAK",  0,0,0);
  exception("Unknown Opcode exception");
  return 0;
}

/****************************************************************************/
static int op_beq(void) {     trace("BEQ   r%i, r%i, %i", rs1, rs2, broffset);
  if(regs[rs1] == regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}
/****************************************************************************/
static int op_bne(void) {    trace("BNE   r%i, r%i, %i", rs1, rs2, broffset);
  if(regs[rs1] != regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}
/****************************************************************************/
static int op_blt(void) {     trace("BLT   r%i, r%i, %i", rs1, rs2, broffset);
  if((int32_t)regs[rs1] < (int32_t)regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}

/****************************************************************************/
static int op_bltu(void) {    trace("BLT   r%i, r%i, %i", rs1, rs2, broffset);
  if((uint32_t)regs[rs1] < (uint32_t)regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}

/****************************************************************************/
static int op_bge(void) {     trace("BLT   r%i, r%i, %i", rs1, rs2, broffset);
  if((int32_t)regs[rs1] >= (int32_t)regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}

/****************************************************************************/
static int op_bgeu(void) {    trace("BLT   r%i, r%i, %i", rs1, rs2, broffset);
  if((uint32_t)regs[rs1] >= (uint32_t)regs[rs2]) {
    pc += broffset;
  } else {
    pc += 4;
  }
  return 1;
}

/****************************************************************************/
static int op_auipc(void) {          trace("AUIPC r%u, x%08x",  rd, upper20, 0);
  if(rd!=0)
    regs[rd] = pc + upper20;
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_lui(void) {              trace("LUI   r%u, x%08x",  rd, upper20, 0);

  if(rd!=0) 
    regs[rd] = upper20;
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_jal(void) {            trace("JAL   r%u, %i",  rd, jmpoffset, 0);

  if(rd!=0) 
    regs[rd] = pc+4;
  pc = (pc + jmpoffset) & (~1);
  return 1;
}

/****************************************************************************/
static int op_jalr(void) {              trace("JALR  r%u, %i",  rd, imm12, 0);

   if(rd != 0)
      regs[rd] = pc+4;
   pc = (regs[rs1] + imm12) & (~1);
 
   return 1;
}

/****************************************************************************/
static int op_fence(void) {                              trace("FENCE",0,0,0);
   pc += 4;
   return 1;
}

/****************************************************************************/
static int op_fence_i(void) {                           trace("FENCEI",0,0,0);
   pc += 4;
   return 1;
}

/****************************************************************************/
static int op_add(void) {           trace("ADD   r%u, r%u, %i",  rd, rs1, rs2);

  if(rd != 0)
    regs[rd] = regs[rs1] + regs[rs2];    

  pc += 4;
  return 1;
}
/****************************************************************************/
static int op_addi(void) {       trace("ADDI  r%u, r%u, %i",  rd, rs1, imm12);

  if(rd != 0) regs[rd] = regs[rs1] + imm12;    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_andi(void) {       trace("ADDI  r%u, r%u, %i",  rd, rs1, imm12);

  if(rd != 0) regs[rd] = regs[rs1] + imm12;    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_or(void) {            trace("OR    r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0) regs[rd] = regs[rs1] | regs[rs2];    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_ori(void) {         trace("ORI   r%u, r%u, %i",  rd, rs1, imm12);

  if(rd != 0) regs[rd] = regs[rs1] | imm12;    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_xor(void) {          trace("XOR   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0) regs[rd] = regs[rs1] ^ regs[rs2];    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_xori(void) {       trace("XORI  r%u, r%u, %i",  rd, rs1, imm12);
  trace("XORI  r%u, r%u, %i",  rd, rs1, imm12);

  if(rd != 0) regs[rd] = regs[rs1] ^ imm12;    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_and(void) {          trace("AND   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0) regs[rd] = regs[rs1] & regs[rs2];    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_sub(void) {          trace("SUB   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0) regs[rd] = regs[rs1] - regs[rs2];    

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_slli(void) {       trace("SLLI  r%u, r%u, %i",  rd, rs1, shamt);

  if(rd != 0) regs[rd] = regs[rs1] << shamt;   
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_slt(void) {          trace("SLT   r%u, r%u, r%u",  rd, rs1, rs2);
  if(rd != 0)  {
    if((int32_t) regs[rs1] < (int32_t)regs[rs2])
      regs[rd] = 1;
    else
      regs[rd] = 0;
  }
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_slti(void) {       trace("SLTI  r%u, r%u, %i",  rd, rs1, imm12);
  
  if(rd != 0)  {
    if((int32_t) regs[rs1] < (int32_t)imm12)
      regs[rd] = 1;
    else
      regs[rd] = 0;
  }
  pc += 4;
  return 1;
}

/****************************************************************************/
int op_sltiu(void) {             trace("SLUI  r%u, r%u, %i",  rd, rs1, imm12);
 
  if(rd != 0)  {
     if((uint32_t) regs[rs1] < (uint32_t)imm12)
      regs[rd] = 1;
    else
      regs[rd] = 0;
  }
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_srl(void) {          trace("SRL   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0) regs[rd] = (uint32_t)regs[rs1] >> (regs[rs2] & 0x1f);
    pc += 4;
  return 1;
}

/****************************************************************************/
static int op_srli(void) {       trace("SRLI  r%u, r%u, %i",  rd, rs1, shamt);
  if(rd != 0) 
    regs[rd] = (uint32_t)regs[rs1] >> shamt;
  pc += 4;
  return 1;
}

/****************************************************************************/
int op_sltu(void) {                trace("SLU   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0)  {
    if((uint32_t) regs[rs1] < (uint32_t)(regs[rs2] & 0x1f))
      regs[rd] = 1;
    else
      regs[rd] = 0;
  }
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_sra(void) {          trace("SRA   r%u, r%u, r%u",  rd, rs1, rs2);

  if(rd != 0)
     regs[rd] = (int32_t)regs[rs1] >> (regs[rs2] & 0x1f);
  pc += 4;
  return 1;
}

/****************************************************************************/
int op_srai(void) {              trace("SRAI  r%u, r%u, %i",  rd, rs1, shamt);

  if(rd != 0)
    regs[rd] = (int32_t)regs[rs1] >> shamt;
  pc += 4;
  return 1;
}

/****************************************************************************/
int op_sll(void) {                 trace("SLL   r%u, r%u, r%u",  rd, rs1, rs2);
  if(rd != 0) 
    regs[rd] = regs[rs1] << shamt;   
  pc += 4;
  return 1;
}
/****************************************************************************/
static int op_lb(void) {          trace("LB    r%u, r%u + %i",  rd, rs1, imm12);
  
  if(rd != 0) {
    if(!memorymap_read(regs[rs1]+imm12,1, &regs[rd])) 
      return 0;
    regs[rd] &= 0x000000FF;
    /* Sign extend */
    if(regs[rd] & 0x80) 
      regs[rd] |= 0xFFFFFF00;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_lh(void) {          trace("LH    r%u, r%u + %i",  rd, rs1, imm12);
  
  if(rd != 0) {
    if(!memorymap_read(regs[rs1]+imm12,2, &regs[rd])) 
      return 0;
    regs[rd] &= 0x0000FFFF;
    /* Sign extend */
    if(regs[rd] & 0x8000) 
      regs[rd] |= 0xFFFF0000;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_lw(void) {          trace("LW    r%u, r%u + %i",  rd, rs1, imm12);
   
  if(rd != 0) {
    if(!memorymap_read(regs[rs1]+imm12,4, &regs[rd])) 
      return 0;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_lbu(void) {        trace("LBU   r%u, r%u + %i",  rd, rs1, imm12);

  if(rd != 0) {
    if(!memorymap_read(regs[rs1]+imm12,1, &regs[rd])) 
      return 0;
    regs[rd] &= 0x000000FF;
  }
  pc = pc + 4;
  return 1;
}
/****************************************************************************/
static int op_lhu(void) {        trace("LHU   r%u, r%u + %i",  rd, rs1, imm12);

  if(rd != 0) {
    if(!memorymap_read(regs[rs1]+imm12,1, &regs[rd])) 
      return 0;
    regs[rd] &= 0x0000FFFF;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_csrrw(void) {     trace("CSRRW r%u, r%u, %i",  rd, rs1, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rs1 != 0) csr[csrid] = regs[rs1];
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_csrrs(void) { trace("CSRRS r%u, r%u, %i",  rd, rs1, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rd != 0) regs[rd] = csr[csrid];
  if(rs1 != 0) csr[csrid] |= regs[rs1];
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_csrrc(void) { trace("CSRRC r%u, r%u, %i",  rd, rs1, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rd != 0) regs[rd] = csr[csrid];
  if(rs1 != 0) csr[csrid] &= ~regs[rs1];
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_csrrwi(void) { trace("CSRRWI r%u, r%u, %i",  rd, uimm, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rd != 0) regs[rd] = csr[csrid];
  csr[csrid] = uimm;
  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_csrrsi(void) { trace("CSRRSI r%u, r%u, %i",  rd, uimm, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rd != 0) regs[rd] = csr[csrid];
  csr[csrid] |= uimm;

  pc += 4;
  return 1;
}

/****************************************************************************/
static int op_csrrci(void) { trace("CSRRCI r%u, r%u, %i",  rd, uimm, csrid);
  char buffer[100];
  sprintf(buffer,"CSR 0x%03x accessed",csrid);
  display_log(buffer);

  if(rd != 0) regs[rd] = csr[csrid];
  csr[csrid] &= ~uimm;
  return 1;
}

/****************************************************************************/
static int op_sb(void) { trace("SB    r%u+%i, r%u", rs1 , imm12wr, rs2);
  if(!memorymap_write(regs[rs1]+imm12wr, 1, regs[rs2])) {
    display_log("SB failed");
    return 0;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_sh(void) { trace("SH    r%u+%i, r%u", rs1 , imm12wr, rs2);
  if(!memorymap_write(regs[rs1]+imm12wr, 2, regs[rs2])) {
    display_log("SH failed");
    return 0;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
static int op_sw(void) { trace("SW    r%u+%i, r%u", rs1 , imm12wr, rs2);

  if(!memorymap_write(regs[rs1]+imm12wr, 4, regs[rs2])) {
    display_log("SW failed");
    return 0;
  }
  pc = pc + 4;
  return 1;
}

/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_mul(void) {          trace("MUL   r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked MUL");
  if(rd != 0) regs[rd] = regs[rs1] * regs[rs2]; //TODO CHeck/fix
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_mulh(void) {         trace("MULH  r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked MULH");
  if(rd != 0) regs[rd] = regs[rs1] * regs[rs2]; //TODO Check/fix
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_mulhsu(void) {      trace("MULHUS r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked MULHSU");
  if(rd != 0) regs[rd] = regs[rs1] * regs[rs2]; //TODO Check/fix
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_mulhu(void) {        trace("MULHU r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked MULHU");
  if(rd != 0) regs[rd] = regs[rs1] * regs[rs2]; //TODO Check/fix
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_div(void) {          trace("DIV   r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked DIV");
  if(rd != 0) regs[rd] = (int32_t)(regs[rs1]) / (int32_t)(regs[rs2]); //TODO Check/fix
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_divu(void) {         trace("DIVU  r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked DIVU");
  if(rd != 0) regs[rd] = regs[rs1] / regs[rs2]; // TODO Check ordering
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_rem(void) {          trace("REM   r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked REM");
  if(rd != 0) regs[rd] = regs[rs1] % regs[rs2];  // TODO Check ordering / sign
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
#ifdef ALLOW_RV32M
static int op_remu(void) {         trace("REMU  r%u, r%u, %i",  rd, rs1, rs2);
  display_log("Unchecked REMU");
  if(rd != 0) regs[rd] = regs[rs1] % regs[rs2];  // TODO Check ordering
  pc += 4;
  return 1;
}
#endif
/****************************************************************************/
static int do_op(void) {
  uint32_t instr;
  int i;
  if((pc & 3) != 0) {
    display_log("Attempt to execute unaligned code");
    return 0;
  }

  /* Fetch */
  if(!memorymap_read(pc,4, &instr)) {
    display_log("Unable to fetch instruction");
    return 0;
  }
  /* Decode */
  decode(instr);
 
  /* Execute */
  for(i = 0; i < sizeof(opcodes)/sizeof(struct opcode_entry); i++) {
     if((instr & opcodes[i].mask) == opcodes[i].value) {
       return opcodes[i].func();
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

  csr[CSR_RDTIME]++;
  if(csr[CSR_RDTIME] == 0)
    csr[CSR_RDTIMEH]++;

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
