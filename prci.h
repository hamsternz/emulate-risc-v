int  PRCI_init(struct region *r);
int  PRCI_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int  PRCI_get(struct region *r, uint32_t address, uint32_t *value);
void PRCI_dump(struct region *r);
void PRCI_free(struct region *r);
