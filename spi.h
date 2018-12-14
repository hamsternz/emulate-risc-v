int  SPI_init(struct region *r);
int  SPI_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int  SPI_get(struct region *r, uint32_t address, uint32_t *value);
void SPI_dump(struct region *r);
void SPI_free(struct region *r);
