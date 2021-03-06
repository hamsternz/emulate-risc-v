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

/****************************************************************************/
int SPI_init(struct region *r) {
  uint32_t *data;

  if(r->data != NULL) {
    display_log("SPI already initialized");
    return 0;
  }
 
  data = malloc(r->size);
  if(data == NULL){
    return 0;
  }
  r->data = (void *)data;
  memset(r->data, 0, r->size);
  display_log("Set up SPI region");
  return 1;
}

/****************************************************************************/
int SPI_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value) {
   char buffer[100];
   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory write 0x%08x\n", r->base+address);
   }
   sprintf(buffer,"SPI Wr address 0x%08x: 0x%08x", address, value);
   display_log(buffer);

   if(mask & 1) {
      ((unsigned char *)r->data)[address+0] = value; 
   }
   if(mask & 2) {
      ((unsigned char *)r->data)[address+1] = value>>8; 
   }
   if(mask & 4) {
      ((unsigned char *)r->data)[address+2] = value>>16; 
   }
   if(mask & 8) {
      ((unsigned char *)r->data)[address+3] = value>>24; 
   }
   return 1;
}

/****************************************************************************/
int SPI_get(struct region *r, uint32_t address, uint32_t *value) {
   uint32_t v = 0;
   char buffer[100];
   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory read 0x%08x\n", r->base+address);
     return 0;
   }

   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   v = ((unsigned char *)r->data)[address]; 
   v = v + (((unsigned char *)r->data)[address+1] << 8); 
   v = v + (((unsigned char *)r->data)[address+2] << 16); 
   v = v + (((unsigned char *)r->data)[address+3] << 24); 

   if(address == 0)
     v |= 1<<31;
   *value = v;
     
   sprintf(buffer,"SPI Rd address 0x%08x: 0x%08x", address, v);
   display_log(buffer);

   return 1;
}

/****************************************************************************/
void SPI_dump(struct region *r) {
   int i;

   printf("SPI 0x%08x length 0x%08x\n", r->base, r->size);
   for(i = 0; i < r->size && i < 4096; i++) {
      if(i%32 == 0) {
	 printf("%08x:", r->base+i);
      }
      printf(" %02x", ((unsigned char *)(r->data))[i]);
      if(i%32 == 31)
	 printf("\n");
   }
   if(i%32 != 0)
     printf("\n");
}
/****************************************************************************/
void SPI_free(struct region *r) {
   char buffer[100];
   sprintf(buffer, "Releasing SPI at 0x%08x", r->base);
   display_log(buffer);
   if(r->data != NULL) 
     free(r->data);
}
/****************************************************************************/
