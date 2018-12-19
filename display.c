/********************************************************************
 * Part of Mike Field's emulate-risc-v project.
 *
 * (c) 2018 Mike Field <hamster@snap.net.nz>
 *
 * See https://github.com/hamsternz/emulate-risc-v for licensing
 * and additional info
 *
 ********************************************************************/
#include <malloc.h>
#include <string.h>
#include <ncurses.h>

#include "display.h"
#include "riscv.h"

#define BORDER_PAIR   1
#define INACTIVE_PAIR 2
#define ACTIVE_PAIR   3

#define N_LOG 6
#define LOG_SHOW 6

#define N_TRACE 17
#define TRACE_SHOW 17
#define TRACE_WIDTH 52

#define N_UART      6
#define UART_SHOW   6
#define UART_WIDTH 80
FILE *log_file;

static char *log_lines[N_LOG];
static int log_index = 0;
static int log_changed = 1;

static char *trace_lines[N_TRACE];
static int trace_index = 0;
static int trace_changed = 1;

static char *uart_lines[N_TRACE];
static int uart_index = 0;
static int uart_changed = 1;
static int uart_cursor_x = 0;
static int uart_cursor_y = 0;

/*****************************************************************/
static void update_reg(void) {
  int i;
  move(0, 0);
  attron(COLOR_PAIR(BORDER_PAIR));
  printw("Registers:");
  attron(COLOR_PAIR(ACTIVE_PAIR));
  for(i = 0; i < 16; i++) {
    move(1+i, 0);
    printw("r%02i %08X", i,    riscv_reg(i));
    printw(" r%02i %08X", i+16, riscv_reg(i+16));
  }
  move(1+i, 0);
  printw("       pc %08X       ",riscv_pc());
}

/*****************************************************************/
static void update_log(void) {
  int i, index;

  index = log_index-LOG_SHOW;
  if(index < 0) index = 0;

  move(18,0);
  attron(COLOR_PAIR(BORDER_PAIR));
  printw("Log:");
  attron(COLOR_PAIR(ACTIVE_PAIR));
  for(i = 0; i < LOG_SHOW; i++) {
     move(19+i,0);
     if(log_lines[index+i])
       printw("%-80s", log_lines[index+i]);
     else
       printw("%-80s", "");
  }
  log_changed = 0;
}
/*****************************************************************/
static void update_trace(void) {
  int i, index;

  move(0, 28);
  attron(COLOR_PAIR(BORDER_PAIR));
  printw("Trace:                     Cycle: %6i", riscv_cycle());
  clrtoeol();
  index = trace_index-TRACE_SHOW;
  if(index < 0) index = 0;

  attron(COLOR_PAIR(ACTIVE_PAIR));
  for(i = 0; i < TRACE_SHOW; i++) {
     move(1+i,28);
     if(trace_lines[index+i])
       printw("%s", trace_lines[index+i]);
  }
  trace_changed = 0;
}
/*****************************************************************/
static void update_uart(void) {
  int i, index;

  move(25, 0);
  attron(COLOR_PAIR(BORDER_PAIR));
  printw("UART data:");
  clrtoeol();
  index = uart_index-UART_SHOW;
  if(index < 0) index = 0;

  attron(COLOR_PAIR(ACTIVE_PAIR));
  for(i = 0; i < UART_SHOW; i++) {
     move(26+i,0);
     if(uart_lines[index+i])
       printw("%s", uart_lines[index+i]);
  }
  uart_changed = 0;
}

/*****************************************************************/
int display_start(void) {
  int i, maxx, maxy;
  if(log_file == NULL) {
    log_file = fopen("events.log","wb");
  }
  for(i = 0; i < N_TRACE; i++) {
    trace_lines[i] = malloc(TRACE_WIDTH+1);
    if(trace_lines[i] == NULL) {
      fprintf(stderr, "Unable to allocate trace lines\n");
      return 0;
    }
    memset(trace_lines[i], ' ', TRACE_WIDTH);
    trace_lines[i][TRACE_WIDTH] = 0;
  }

  for(i = 0; i < N_UART; i++) {
    uart_lines[i] = malloc(UART_WIDTH+1);
    if(uart_lines[i] == NULL) {
      fprintf(stderr, "Unable to allocate uart lines\n");
      return 0;
    }
    memset(uart_lines[i], ' ', UART_WIDTH);
    uart_lines[i][UART_WIDTH] = 0;
  }

  /* This sets up the screen */
  if(initscr()==NULL) {
    return 0;
  }

  if(!has_colors()) {
    endwin();
    fprintf(stderr,"Terminal must support colour\n");
    return 0;
  }

  getmaxyx(stdscr, maxy, maxx);
  if(maxx < 80 || maxy < 30)
  {
    endwin();
    fprintf(stderr,"Terminal must be at least 80x30 - currently %i x %i\n", maxx, maxy);
    return 0;
  }

  start_color();
  init_pair(BORDER_PAIR,   COLOR_WHITE, COLOR_BLACK);
  init_pair(INACTIVE_PAIR, COLOR_CYAN,  COLOR_BLUE);
  init_pair(ACTIVE_PAIR,   COLOR_WHITE, COLOR_BLUE);
  /* This sets up the keyboard to send character by
   *    *      character, without echoing to the screen */ 
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nonl();
  timeout(0);
  intrflush(stdscr, FALSE);
  return 1;
}

/*****************************************************************/
void display_update(void) {
  update_reg();

  if(log_changed) 
    update_log();

  if(trace_changed) 
    update_trace();
 
  if(uart_changed) 
    update_uart();
 
  refresh();
}

/*****************************************************************/
void display_log(char *str) {
  if(log_file != NULL) {
    fprintf(log_file,"%s\n",str);
  }
  if(log_index == N_LOG) {
    ////////////////////
    // Free up a line 
    ////////////////////
    int i;
    if(log_lines[0] != NULL)
      free(log_lines[0]);
    for(i = 1; i < N_LOG; i++) {
      log_lines[i-1] = log_lines[i];
    }	    
    log_lines[N_LOG-1] = NULL;
    log_index--;
    log_changed = 1;
  }  

  log_lines[log_index] = malloc(strlen(str)+1);

  if(log_lines[log_index] == NULL)
    return;

  log_changed = 1;
  strcpy(log_lines[log_index],str);
  log_index++;
}

/*****************************************************************/
void display_trace(char *str) {
  int i; 
  trace_changed = 1;

  if(trace_index == N_TRACE) {
    char *temp;
    ////////////////////
    // Move top line to the bottom (to be overwritten)
    ////////////////////
    temp = trace_lines[0];
    for(i = 1; i < N_TRACE; i++) {
      trace_lines[i-1] = trace_lines[i];
    }
    trace_lines[N_TRACE-1] = temp;
    trace_index--;
  } 

  if(trace_lines[trace_index] == NULL)
    return;

  for(i = 0; str[i] != '\0' && i < TRACE_WIDTH; i++){
    trace_lines[trace_index][i] = str[i];
  }

  for(; i < TRACE_WIDTH; i++) {
    trace_lines[trace_index][i] = ' ';
  }

  trace_index++;
}

/*****************************************************************/
void display_process_input(int *run, int *quit, int *trace, int *reset) {
  int key = getch();
  switch(key) {
    case 'R':
       *reset = 1;
       break;
    case 'r':
       *run = *run ? 0 : 2;
       break;
    case 't':
       *trace = !*trace;
       break;
    case 'q':
       *quit = !*quit;
       break;
    case ' ':
       *run = 1;
       break;
  }
}
/*****************************************************************/
void display_uart_write(char c) {

  /* Only display printable characters */
  if(c > 27 && c < 127) {
    uart_lines[uart_cursor_y][uart_cursor_x] = c;
    uart_cursor_x++;
  }

  /* Caridge return brings the cursor to the left */
  if(c == '\r') {
    uart_cursor_x = 0;
  }

  /* Do we need to move to the next line? */
  if(c == '\n' || uart_cursor_x == UART_WIDTH) {
    uart_cursor_y++;
    uart_cursor_x = 0;
    /* Have we run out of lines */
    if(uart_cursor_y == N_UART) {
      /* shuffle the lines up, moving the first line to the end */
      int i;
      char *t = uart_lines[0];
      for(i = 1; i < N_UART; i++) {
	uart_lines[i-1] = uart_lines[i];
      }
      uart_lines[N_UART-1] = t;

      /* Empty out the first line */
      memset(uart_lines[N_UART-1],' ', UART_WIDTH);
      uart_cursor_y--;
    }
  }
  uart_changed = 1;
}
/*****************************************************************/
void display_end(void) {
  int i;
  if(log_file != NULL) {
    fclose(log_file);
    log_file = NULL;
  }
  for(i = 0; i < N_TRACE; i++) {
    if(trace_lines[i] != NULL)
      free(trace_lines[i]);
  }
  endwin();
}
