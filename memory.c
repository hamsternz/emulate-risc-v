/********************************************************************
 * Part of Mike Field's emulate-risc-v project.
 *
 * (c) 2018 Mike Field <hamster@snap.net.nz>
 *
 * See https://github.com/hamsternz/emulate-risc-v for licensing
 * and additional info
 *
 ********************************************************************/
#include <stdint.h> 
#include "memorymap.h"
#include "memory.h"
#include "display.h"

#define FIFO_SIZE (8)

/******************************/
struct fifo_read_data {
  uint32_t read_ptr;
  uint32_t write_ptr;
  uint32_t count;
  uint32_t data[FIFO_SIZE];
} read_data_fifo;

/******************************/
struct fifo_fetch_data {
  uint32_t read_ptr;
  uint32_t write_ptr;
  uint32_t count;
  uint32_t data[FIFO_SIZE];
} fetch_data_fifo;

/******************************/
struct fifo_write_request {
  uint32_t read_ptr;
  uint32_t write_ptr;
  uint32_t count;
  uint32_t address[FIFO_SIZE];
  uint32_t mask[FIFO_SIZE];
  uint32_t data[FIFO_SIZE];
} write_request_fifo;

/******************************/
struct fifo_read_request {
  uint32_t read_ptr;
  uint32_t write_ptr;
  uint32_t count;
  uint32_t address[FIFO_SIZE];
} read_request_fifo;

/******************************/
struct fifo_fetch_request {
  uint32_t read_ptr;
  uint32_t write_ptr;
  uint32_t count;
  uint32_t address[FIFO_SIZE];
} fetch_request_fifo;

/****************************************************************************/
void memory_reset(void) {

  read_data_fifo.count         = 0;
  read_data_fifo.read_ptr      = 0;
  read_data_fifo.write_ptr     = 0;

  fetch_data_fifo.count        = 0;
  fetch_data_fifo.read_ptr     = 0;
  fetch_data_fifo.write_ptr    = 0;

  write_request_fifo.count     = 0;
  write_request_fifo.read_ptr  = 0;
  write_request_fifo.write_ptr = 0;

  read_request_fifo.count      = 0;
  read_request_fifo.read_ptr   = 0;
  read_request_fifo.write_ptr  = 0;
  display_log("Memory reset");
}

/****************************************************************************/
int memory_initialise(void) {
  memory_reset();
  return memorymap_initialise();
}

/****************************************************************************/
uint32_t memory_read_request(uint32_t address) {
  if(read_request_fifo.count == FIFO_SIZE)
    return 0;
  read_request_fifo.address[read_request_fifo.write_ptr] = address;
  read_request_fifo.count++;
  read_request_fifo.write_ptr = (read_request_fifo.write_ptr == FIFO_SIZE-1) ? 0 : read_request_fifo.write_ptr+1;
  return 1;
}

/****************************************************************************/
uint32_t memory_fetch_request(uint32_t address) {
  if(fetch_request_fifo.count == FIFO_SIZE)
    return 0;

  fetch_request_fifo.address[fetch_request_fifo.write_ptr] = address;
  fetch_request_fifo.count++;
  fetch_request_fifo.write_ptr = (fetch_request_fifo.write_ptr == FIFO_SIZE-1) ? 0 : fetch_request_fifo.write_ptr+1;

  return 1;
}

/****************************************************************************/
uint32_t memory_read_data_empty(void) {
  return read_data_fifo.count == 0;
}
/****************************************************************************/
uint32_t memory_fetch_data_empty(void) {
  return fetch_data_fifo.count == 0;
}

/****************************************************************************/
uint32_t memory_read_data(void) {
  uint32_t rtn;
  if(read_data_fifo.count == 0) {
    display_log("Attempt to read empty FIFO read_data");
    return 0;
  }

  rtn = read_data_fifo.data[read_data_fifo.read_ptr];
  read_data_fifo.count--;
  read_data_fifo.read_ptr = (read_data_fifo.read_ptr == FIFO_SIZE-1) ? 0 : read_data_fifo.read_ptr+1; 
  return rtn;
}

/****************************************************************************/
uint32_t memory_fetch_data(void) {
  uint32_t rtn;
  if(fetch_data_fifo.count == 0) {
    display_log("Attempt to read empty FIFO fetch_data");
    return 0;
  }

  rtn = fetch_data_fifo.data[fetch_data_fifo.read_ptr];
  fetch_data_fifo.count--;
  fetch_data_fifo.read_ptr = (fetch_data_fifo.read_ptr == FIFO_SIZE-1) ? 0 : fetch_data_fifo.read_ptr+1;
  return rtn;
}

/****************************************************************************/
int      memory_write_full(void) {
  return write_request_fifo.count == FIFO_SIZE;
}

/****************************************************************************/
int      memory_write_request(uint32_t address, uint32_t mask, uint32_t value) {
  if(write_request_fifo.count == FIFO_SIZE) {
    return 0;
  }
  write_request_fifo.address[write_request_fifo.write_ptr] = address;
  write_request_fifo.mask[write_request_fifo.write_ptr]    = mask;
  write_request_fifo.data[write_request_fifo.write_ptr]    = value;
  write_request_fifo.count++;
  write_request_fifo.write_ptr = (write_request_fifo.write_ptr == FIFO_SIZE-1) ? 0 : write_request_fifo.write_ptr+1;
  return 1; 
}

/****************************************************************************/
int  memory_run(void) {
  if( write_request_fifo.count > 0) {
    uint32_t addr, data;
    uint8_t mask;

    addr = write_request_fifo.address[write_request_fifo.read_ptr];
    mask = write_request_fifo.mask[write_request_fifo.read_ptr];
    data = write_request_fifo.data[write_request_fifo.read_ptr];

    write_request_fifo.read_ptr = (write_request_fifo.read_ptr == FIFO_SIZE-1) ? 0 : write_request_fifo.read_ptr+1;
    write_request_fifo.count--;

    return memorymap_write(addr, mask, data);
  }



  /* Process the read request queue */
  if( read_request_fifo.count > 0 && read_data_fifo.count < FIFO_SIZE) {
    uint32_t data, addr;
    /* Pull the address */
    addr = read_request_fifo.address[read_request_fifo.read_ptr];
    read_request_fifo.count--;
    read_request_fifo.read_ptr = (read_request_fifo.read_ptr == FIFO_SIZE-1) ? 0 : read_request_fifo.read_ptr+1;

    if(!memorymap_read(addr, 4, &data)) {
      data = 0;
    }
    /*Push the data */
    read_data_fifo.data[read_data_fifo.write_ptr] = data;
    read_data_fifo.count++;
    read_data_fifo.write_ptr = (read_data_fifo.write_ptr == FIFO_SIZE-1) ? 0 : read_data_fifo.write_ptr+1;

    return 1;
  }

  /* Process the fetch request queue */
  if( fetch_request_fifo.count > 0 && fetch_data_fifo.count < FIFO_SIZE) {
    uint32_t addr, data;
    addr = fetch_request_fifo.address[fetch_request_fifo.read_ptr];
    fetch_request_fifo.count--;
    fetch_request_fifo.read_ptr = (fetch_request_fifo.read_ptr == FIFO_SIZE-1) ? 0 : fetch_request_fifo.read_ptr+1;


    if(!memorymap_read( addr, 4, &data)) {
      data = 0;
    }

    fetch_data_fifo.data[fetch_data_fifo.write_ptr] = data;
    fetch_data_fifo.count++;
    fetch_data_fifo.write_ptr = (fetch_data_fifo.write_ptr == FIFO_SIZE-1) ? 0 : fetch_data_fifo.write_ptr+1; 

    return 1;
  }
  return 1;
}

void     memory_finish(void) {
  memorymap_finish();
}
