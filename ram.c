/********************************************************************
 * Part of Mike Field's emulate-risc-v project.
 *
 * (c) 2018 Mike Field <hamster@snap.net.nz>
 *
** See https://github.com/hamsternz/emulate-risc-v for licensing
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
static void attempt_to_read(struct region *r) {
  uint32_t *data;
  char *fname;
  FILE *f;
  int a = 0;
  int i;

  data = (uint32_t *)(r->data);

  if(asprintf(&fname, "ram_%08x.img",r->base) < 1) {
    display_log("Unable to print file name to memory region");
    return;
  }

  if(fname == NULL) {
    display_log("Unable to allocate memory to read region");
    return;
  }

  f = fopen(fname,"rb");
  if( f == NULL) {
    fprintf(stderr, "File '%s' not present\n",fname);
    free(fname);
    return;
  }
  free(fname);

  while(1) {
    int c;
    uint32_t d = 0;
 
    c = fgetc(f);
    while(c == '\n' || c == '\r') {
      c = fgetc(f);
    }

    if(c == EOF) {
       printf("End of file at address %i\n", a);
       break;
    }

    if(c >= '0' && c <= '9') {
      d = c - '0';
    } else if(c >= 'A' && c <= 'F') {
      d = c - 'A' + 10;
    } else if(c >= 'a' && c <= 'f') {
      d = c - 'a' + 10;
    } else if(c != ' ') {
      fprintf(stderr, "unexpected characters in file\n");
      return;
    }

    for(i = 0; i < 7; i++) {
      c = fgetc(f);
      if(c >= '0' && c <= '9') {
	d *= 16;
        d += c - '0';
      } else if(c >= 'A' && c <= 'F') {
	d *= 16;
        d += c - 'A' + 10;
      } else if(c >= 'a' && c <= 'f') {
	d *= 16;
        d += c - 'a' + 10;
      } else if(c == ' ' || c == '\t' || c == '\n') {
	if(a*4+3 >= r->size) {
          display_log("Too much data for memory region");
          return;
	}
	break;
      } else {
        display_log("unexpected characters in file");
        return;
      }
    }

    data[a] = d;
    a++;

    while(c != '\n' && c != EOF) {
      c = fgetc(f);
    }
  } 

  fclose(f);
}
/****************************************************************************/
int RAM_init(struct region *r) {
  uint32_t *data;

  if(r->data != NULL) {
    display_log("RAM already initialized");
    return 0;
  }
 
  data = malloc(r->size);
  if(data == NULL){
    return 0;
  }
  r->data = (void *)data;
  memset(r->data, 0, r->size);
  attempt_to_read(r);
  display_log("Set up memory region");
  return 1;
}

/****************************************************************************/
int RAM_set(struct region *r, uint32_t address, uint8_t mask, uint32_t value) {

   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory write 0x%08x\n", r->base+address);
   }
#if 0
   fprintf(stderr, "Write %08x %x, 0x%08x\n", address, mask, value);
#endif
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
int RAM_get(struct region *r, uint32_t address, uint32_t *value) {
   uint32_t v = 0;
   if((address & 3) != 0) {
     fprintf(stderr,"Unaligned memory read 0x%08x\n", r->base+address);
     return 0;
   }

   if(address+4 > r->size) {
     fprintf(stderr,"Memory region boundary crossed at 0x%08x\n", r->base+address);
     return 0;
   }

   v = ((unsigned char *)r->data)[address++]; 
   v = v + (((unsigned char *)r->data)[address++] << 8); 
   v = v + (((unsigned char *)r->data)[address++] << 16); 
   v = v + (((unsigned char *)r->data)[address++] << 24); 
   *value = v;
   return 1;
}

/****************************************************************************/
void RAM_dump(struct region *r) {
   int i;

   printf("RAM 0x%08x length 0x%08x\n", r->base, r->size);
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
void RAM_free(struct region *r) {
   char buffer[100];
   sprintf(buffer, "Releasing RAM at 0x%08x", r->base);
   display_log(buffer);
   if(r->data != NULL) 
     free(r->data);
}
/****************************************************************************/
