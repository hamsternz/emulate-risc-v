int  GPIO_init(struct region *r);
int  GPIO_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int  GPIO_get(struct region *r, uint32_t address, uint32_t *value);
void GPIO_dump(struct region *r);
void GPIO_free(struct region *r);
