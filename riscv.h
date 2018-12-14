#ifndef RISCV_H
#define RISCV_H
int riscv_initialise(void);
int riscv_run(void);
void riscv_reset(void);
void riscv_dump(void);
uint32_t riscv_cycle(void);
uint32_t riscv_reg(int i);
uint32_t riscv_pc(void);
void riscv_finish(void);
#endif