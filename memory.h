#ifndef _MEMORY_H
#define _MEMORY_H
int      memory_initialise(void);

void     memory_reset(void);
int      memory_run(void);

uint32_t memory_fetch_request(uint32_t address);
uint32_t memory_fetch_data_empty(void);
uint32_t memory_fetch_data(void);

uint32_t memory_read_request(uint32_t address);
uint32_t memory_read_data_empty(void);
uint32_t memory_read_data(void);

int      memory_write_full(void);
int      memory_write_request(uint32_t address, uint32_t mask, uint32_t value);

void     memory_finish(void);
#endif
