#ifndef MEMORYMAP_H
#define MEMORYMAP_H
int memorymap_initialise(char *image);
uint32_t memorymap_read(uint32_t address, uint8_t width, uint32_t *value);
int  memorymap_write(uint32_t address, uint8_t width, uint32_t value);
int  memorymap_aligned_read(uint32_t address, uint32_t *value);
int  memorymap_aligned_write(uint32_t address, uint8_t mask, uint32_t value);
void memorymap_dump(void);
void memorymap_dump(void);
void memorymap_finish(void);
#endif
