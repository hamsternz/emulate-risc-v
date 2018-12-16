#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "riscv.h"
#include "memorymap.h"
#include "display.h"

int main(int argc, char *argv[]) {
  int run = 1, quit = 0, trace = 1, reset = 0;

  if(!display_start()) {
    fprintf(stderr,"Unable to initialise display\n");
    return 0;
  }
  if(!memorymap_initialise(NULL)) {
    return 0;
  }
  display_log("Memory inisitalised");

  if(!riscv_initialise()) {
    return 0;
  }
  display_log("RISC-V initalised");
  riscv_reset();
  display_log("Press SPACE to run a sigle instruction, or 'r' to run. 'q' to quit");

  while(!quit) {
    if(run) {
       if(!riscv_run() ||run == 1)
         run = 0;
    }
    display_update();
    display_process_input(&run, &quit, &trace, &reset);
    if(reset) {
      riscv_reset();
      reset = 0;
    }
  }
  riscv_dump();
  riscv_finish();
  display_log("RISC-V shutdown");
//  memorymap_dump();
  memorymap_finish();
  display_log("Memory shutdown");
  display_update();
  display_end();
  return 0;
}
