int UART_init(struct region *r);
int UART_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
int UART_get(struct region *r, uint32_t address, uint32_t *value);
void UART_dump(struct region *r);
void UART_rx_enqueue(struct region *r, uint8_t c);
void UART_free(struct region *r);
