#include <stdint.h> 
#include "memorymap.h"
#include "memory.h"
#include "display.h"

#define FIFO_SIZE (8)

/******************************/
uint32_t read_data_count;
struct fifo_read_data {
  uint32_t data;
} read_data_fifo[FIFO_SIZE];

/******************************/
uint32_t fetch_data_count;
struct fifo_fetch_data {
  uint32_t data;
} fetch_data_fifo[FIFO_SIZE];

/******************************/
uint32_t write_request_count;
struct fifo_write_request {
  uint32_t address;
  uint8_t  mask;
  uint32_t data;
} write_request_fifo[FIFO_SIZE];

/******************************/
uint32_t read_request_count;
struct fifo_read_request {
  uint32_t address;
} read_request_fifo[FIFO_SIZE];

/******************************/
uint32_t fetch_request_count;
struct fifo_fetch_request {
  uint32_t address;
} fetch_request_fifo[FIFO_SIZE];



/****************************************************************************/
int memory_initialise(void) {
  read_data_count = 0;
  fetch_data_count = 0;
  write_request_count = 0;
  read_request_count = 0;
  return memorymap_initialise();
}

/****************************************************************************/
uint32_t memory_read_request(uint32_t address) {
  if(read_request_count == FIFO_SIZE)
    return 0;
  read_request_fifo[read_request_count].address = address;
  read_request_count++;
  return 1;
}

/****************************************************************************/
uint32_t memory_fetch_request(uint32_t address) {
  if(fetch_request_count == FIFO_SIZE)
    return 0;
  fetch_request_fifo[fetch_request_count].address = address;
  fetch_request_count++;
  return 1;
}

/****************************************************************************/
uint32_t memory_read_data_empty(void) {
  return read_data_count == 0;
}
/****************************************************************************/
uint32_t memory_fetch_data_empty(void) {
  return fetch_data_count == 0;
}

/****************************************************************************/
uint32_t memory_read_data(void) {
  int i;
  uint32_t rtn;
  if(read_data_count == 0) {
    display_log("Attempt to read empty FIFO read_data");
    return 0;
  }

  rtn = read_data_fifo[0].data;
  for(i = 0; i < read_data_count-1; i++) {
    read_data_fifo[i].data = read_data_fifo[i+1].data;
  }
  read_data_count--;
  return rtn;
}

/****************************************************************************/
uint32_t memory_fetch_data(void) {
  int i;
  uint32_t rtn;
  if(fetch_data_count == 0) {
    display_log("Attempt to read empty FIFO fetch_data");
    return 0;
  }

  rtn = fetch_data_fifo[0].data;
  for(i = 0; i < fetch_data_count-1; i++) {
    fetch_data_fifo[i].data = fetch_data_fifo[i+1].data;
  }
  fetch_data_count--;
  return rtn;
}

/****************************************************************************/
int      memory_write_full(void) {
  return write_request_count == FIFO_SIZE;
}

/****************************************************************************/
int      memory_write_request(uint32_t address, uint8_t mask, uint32_t value) {
  if(write_request_count == FIFO_SIZE) {
    return 0;
  }
  write_request_fifo[write_request_count].address = address;
  write_request_fifo[write_request_count].mask    = mask;
  write_request_fifo[write_request_count].data    = value;
  write_request_count++;
  return 1; 
}

int  memory_run(void) {
  int rtn = 1;
  while( write_request_count > 0) {
    switch(write_request_fifo[0].mask) {
      case 0x1:
         rtn = memorymap_write(write_request_fifo[0].address, 1, write_request_fifo[0].data);
	 break;
      case 0x3:
         rtn = memorymap_write(write_request_fifo[0].address, 2, write_request_fifo[0].data);
	 break;
      case 0xF:
         rtn = memorymap_write(write_request_fifo[0].address, 4, write_request_fifo[0].data);
	 break;
      default:
	 display_log("Unexpected write width");
    }
    int i;
    /* Move the FIFO */
    for(i = 0; i < write_request_count-1; i++) {
      write_request_fifo[i].address = write_request_fifo[i+1].address;
      write_request_fifo[i].mask    = write_request_fifo[i+1].mask;
      write_request_fifo[i].data    = write_request_fifo[i+1].data;
    }
    write_request_count--;
  }


  /* Process the read request queue */
  while( read_request_count > 0 && read_data_count < FIFO_SIZE) {
    uint32_t data;
    int i;
    if(memorymap_read( read_request_fifo[0].address, 4, &data)) {
      read_data_fifo[read_data_count].data = data;
      read_data_count++;
    } else {
      read_data_fifo[read_data_count].data = 0;
      read_data_count++;
    }

    for(i = 0; i < read_request_count-1; i++) {
      read_request_fifo[i].address = read_request_fifo[i+1].address;
    }
    read_request_count--;
  }

  /* Process the fetch request queue */
  while( fetch_request_count > 0 && fetch_data_count < FIFO_SIZE) {
    uint32_t data;
    int i;
    if(memorymap_read( fetch_request_fifo[0].address, 4, &data)) {
      fetch_data_fifo[fetch_data_count].data = data;
      fetch_data_count++;
    } else {
      fetch_data_fifo[fetch_data_count].data = 0;
      fetch_data_count++;
    }

    for(i = 0; i < fetch_request_count-1; i++) {
      fetch_request_fifo[i].address = fetch_request_fifo[i+1].address;
    }
    fetch_request_count--;
  }
  

  return rtn;
}

void     memory_finish(void) {
  memorymap_finish();
}
