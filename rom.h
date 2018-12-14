int ROM_init(struct region *r);
int ROM_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int ROM_get(struct region *r, uint32_t address, uint32_t *value);
void ROM_dump(struct region *r);
void ROM_free(struct region *r);
