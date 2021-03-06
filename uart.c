/********************************************************************
 * Part of Mike Field's emulate-risc-v project.
 *
 * (c) 2018 Mike Field <hamster@snap.net.nz>
 *
 * See https://github.com/hamsternz/emulate-risc-v for licensing
 * and additional info
 *
 ********************************************************************/
#define _GNU_SOURCE
#include <malloc.h>
#include <stdint.h>
#include <memory.h>
#include "region.h"
#include "ram.h"
#include "display.h"

#define UART_DEBUG 0
#define UART_FIFO_SIZE 8
#define UART_QUEUE_DEPTH 8
struct uart_data {
  uint16_t divisor; 

  uint8_t tx_fifo[UART_FIFO_SIZE];
  uint8_t tx_read_ptr;
  uint8_t tx_write_ptr;
  uint8_t tx_count;

  uint8_t rx_fifo[UART_FIFO_SIZE];
  uint8_t rx_read_ptr;
  uint8_t rx_write_ptr;
  uint8_t rx_count;

  uint8_t tx_watermark;
  uint8_t rx_watermark;
  uint8_t tx_enable;
  uint8_t rx_enable;
  uint8_t tx_irq_enable;
  uint8_t rx_irq_enable;
  uint8_t rx_irq_tx_pending;
  uint8_t rx_irq_rx_pending;
  uint8_t stop_bits;
  uint8_t debug;
};
/****************************************************************************/
int UART_init(struct region *r) {
  struct uart_data *data;

  if(r->data != NULL) {
    display_log("UART already initialized");
    return 0;
  }
 
  data = malloc(sizeof(struct uart_data));
  if(data == NULL){
    return 0;
  }

  memset(data, 0, sizeof(struct uart_data));
  data->divisor = 0xffff;
  data->debug   = UART_DEBUG;
  r->data = (void *)data;

  display_log("Set up UART region");
  return 1;
}

/****************************************************************************/
int UART_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value) {
   char buffer[100];
   struct uart_data *data = r->data;
   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory write 0x%08x\n", r->base+address);
   }


   switch(address) {
     case 0x00: // TRANSMIT DATA REGISTER
	 if(data->tx_count < UART_FIFO_SIZE) {
	   data->tx_fifo[data->tx_write_ptr] = value & 0xFF;
	   data->tx_count++;
           data->tx_write_ptr = (data->tx_write_ptr == UART_FIFO_SIZE-1) ? 0 : data->tx_write_ptr+1;

	   if(data->debug) {
             sprintf(buffer,"UART data added to tx queue 0x%02x", value & 0xff);
             display_log(buffer);
	   }
         } else {
           if(data->debug) {
             sprintf(buffer,"UART tx queue overflow adding 0x%02x", value & 0xff);
             display_log(buffer);
           }
	 }
	 break;    
     case 0x04:
	 break;
     case 0x08:
	 data->tx_enable    = (value  &  1) ? 1 : 0;
	 data->stop_bits    = (value  &  2) ? 2 : 1;
	 data->tx_watermark = (value >> 16) & 0x7;
         if(data->debug) {
           sprintf(buffer,"UART set tx_enable = %i, stop_bits = %i, tx_watermark = %i",
		data->tx_enable, data->stop_bits, data->tx_watermark);
           display_log(buffer);
         }
	 break;
     case 0x0C:
	 data->rx_enable    = (value  &  1) ? 1 : 0;
	 data->rx_watermark = (value >> 16) & 0x7;
         if(data->debug) {
           sprintf(buffer,"UART set rx_enable = %i, rx_watermark = %i",
		data->rx_enable, data->rx_watermark);
           display_log(buffer);
         }
	 break;
     case 0x10:
	 data->rx_irq_enable = (value  &  1) ? 1 : 0;
	 data->tx_irq_enable = (value  &  2) ? 1 : 0;
         if(data->debug) {
           sprintf(buffer,"UART set rx_irq_enable = %i, tx_irq_enable = %i",
                 data->rx_irq_enable, data->tx_irq_enable);
           display_log(buffer);
         }
	 break;
     case 0x14:
	 break;
     case 0x18:
	 data->divisor = value & 0xFFFF;
	 if(data->debug) {
           sprintf(buffer,"UART Divisor set to 0x%08x", value);
           display_log(buffer);
	 }
	 break;
     default:
         sprintf(buffer,"UART Wr unkown address 0x%08x: 0x%08x", address, value);
         display_log(buffer);
	 break;
   }

   // SHould I flush the queue? 
   if(data->tx_enable) {
     while(data->tx_count > 0) {
       display_uart_write(data->tx_fifo[data->tx_read_ptr] & 0xFF);
       data->tx_count--;
       data->tx_read_ptr = (data->tx_read_ptr == UART_FIFO_SIZE-1) ? 0 : data->tx_read_ptr+1;
     }
   }
   return 1;
}

/****************************************************************************/
void UART_rx_enqueue(struct region *r, uint8_t c) {
  char buffer[100];
  struct uart_data *data = r->data;
  if(data->rx_enable) {
    if(data->rx_count < UART_QUEUE_DEPTH) {
      data->rx_fifo[data->tx_write_ptr] = c;
      data->rx_count++;
      data->tx_write_ptr = (data->tx_write_ptr == UART_FIFO_SIZE-1) ? 0 : data->tx_write_ptr+1;
      if(data->debug) {
        sprintf(buffer,"UART data added to rx queue 0x%02x", c);
        display_log(buffer);
      }
    } else {
      if(data->debug) {
        sprintf(buffer,"UART rx queue overflow adding 0x%02x", c);
        display_log(buffer);
      }
    }
  } else {
    if(data->debug) {
      sprintf(buffer,"UART rx disabled while adding 0x%02x", c);
      display_log(buffer);
    }
  }
}
/****************************************************************************/
int UART_get(struct region *r, uint32_t address, uint32_t *value) {
   uint32_t v = 0;
   char buffer[100];
   struct uart_data *data = r->data;

   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory read 0x%08x\n", r->base+address);
     return 0;
   }

   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   v = 0;

   switch(address) {
     case 0x00:
       v = (data->tx_count == UART_QUEUE_DEPTH) ? (1<<31) : 0;
       if(data->debug) {
         sprintf(buffer,"UART is %s to accept tx data",
           data->tx_count == UART_QUEUE_DEPTH ? "not ready" : "ready");
         display_log(buffer);
       }
       break;
     case 0x04:
       if(data->rx_count > 0) {
	 v = data->rx_fifo[data->rx_read_ptr];
	 data->rx_count--;
	 data->rx_read_ptr = (data->rx_read_ptr == UART_FIFO_SIZE-1) ? 0 : data->rx_read_ptr+1;

         if(data->debug) {
           sprintf(buffer,"UART rx queue read - 0x%03x", v);
	   display_log(buffer);
         }
       } else {
         v = (1<<31);
         if(data->debug) {
           display_log("UART rx queue is empty");
         }
       }
       break;
     case 0x08:
       v = 0;
       v |= data->tx_enable      ? 1 : 0;
       v |= data->stop_bits == 2 ? 2 : 0;
       v |= (data->tx_watermark << 16);
       if(data->debug) {
         sprintf(buffer,"UART get tx_enable = %i, stop_bits = %i, tx_watermark = %i",
           data->tx_enable, data->stop_bits, data->tx_watermark);
         display_log(buffer);
       }
       break;
     case 0x0C:
       v = 0;
       v |= data->rx_enable      ? 1 : 0;
       v |= (data->tx_watermark << 16);
       if(data->debug) {
         sprintf(buffer,"UART get rx_enable = %i, rx_watermark = %i",
           data->rx_enable, data->rx_watermark);
         display_log(buffer);
       }
       break;
     case 0x10:
       v = 0;
       v |= data->rx_irq_enable ? 1 : 0;
       v |= data->tx_irq_enable ? 2 : 0;
       if(data->debug) {
         sprintf(buffer,"UART get rx_irq_enable = %i, tx_irq_enable = %i",
                 data->rx_irq_enable, data->tx_irq_enable);
         display_log(buffer);
       }
       break;
     case 0x14:
       v = 0;
       v |= data->tx_count > data->tx_watermark ? 1 : 0;
       v |= data->rx_count > data->rx_watermark ? 2 : 0;
       if(data->debug) {
         sprintf(buffer,"UART get rx_irq_pending = %i, tx_irq_pending = %i",
                 v&1, v>>1);
         display_log(buffer);
       }
       break;
     case 0x18:
       v = data->divisor;
       if(data->debug) {
         sprintf(buffer,"UART get divisor = 0x%08x", v);
         display_log(buffer);
       }
       break;
     default:
       sprintf(buffer,"UART Wr unkown address 0x%08x: 0x%08x", address, v);
       display_log(buffer);
       break;
   }
   *value = v;

   return 1;
}

/****************************************************************************/
void UART_dump(struct region *r) {
   printf("UART 0x%08x length 0x%08x\n", r->base, r->size);
}
/****************************************************************************/
void UART_free(struct region *r) {
   char buffer[100];
   sprintf(buffer, "Releasing UART at 0x%08x", r->base);
   display_log(buffer);
   if(r->data != NULL) 
     free(r->data);
}
/****************************************************************************/
