int display_start(void);
void display_log(char *str);
void display_update(void);
void display_trace(char *str);
void display_process_input(int *run,  int *quit, int *trace, int *reset);
void display_uart_write(char c);
int  display_uart_read(void);
void display_end(void);
