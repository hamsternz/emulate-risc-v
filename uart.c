#include <malloc.h>
#include <stdint.h>
#include <memory.h>
#include "region.h"
#include "ram.h"
#include "display.h"

/****************************************************************************/
int UART_init(struct region *r) {
  uint32_t *data;

  if(r->data != NULL) {
    fprintf(stderr, "UART already initialized\n");
    return 0;
  }
 
  data = malloc(r->size);
  if(data == NULL){
    return 0;
  }
  r->data = (void *)data;
  memset(r->data, 0, r->size);
  fprintf(stderr,"Set up memory region at 0x%08x\n", r->base);
  return 1;
}

/****************************************************************************/
int UART_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value) {
  
   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory write 0x%08x\n", r->base+address);
   }

   if(mask & 1) {
      ((unsigned char *)r->data)[address+0] = value; 
      putchar(value);
   }
   return 1;
}

/****************************************************************************/
int UART_get(struct region *r, uint32_t address, uint32_t *value) {
   int width = 4;
   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory read 0x%08x size %i\n", r->base+address, width);
     return 0;
   }

   if(address+width > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x size %i\n", r->base+address, width);
     return 0;
   }

   *value = 0;
   return 1;
}

/****************************************************************************/
void UART_dump(struct region *r) {
#if 0
   printf("UART 0x%08x length 0x%08x\n", r->base, r->size);
   printf("\n");
#endif
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
