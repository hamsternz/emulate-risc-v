int RAM_init(struct region *r);
int RAM_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int RAM_get(struct region *r, uint32_t address, uint32_t *value);
void RAM_dump(struct region *r);
void RAM_free(struct region *r);
