int  CLINT_init(struct region *r);
int  CLINT_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int  CLINT_get(struct region *r, uint32_t address, uint32_t *value);
void CLINT_dump(struct region *r);
void CLINT_free(struct region *r);
