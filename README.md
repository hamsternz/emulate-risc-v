emulate-risc-v : A very simple RISC-V ISA emulator
==================================================
(c) 2018 Mike Field

Currently this is a very simple emulator for the RV32I instruction set. 

It is not meant to be high perfromance or anything special, just something that 
I can use to get to know RISC-V 32-bt instructions, and can be used to run
programs binaries with GCC's RISC-V 

You can see how the memory map is layed out in memorymap.c. When a ROM or RAM 
memory region is added to the memory map it attempts to load the contents from
a file called "ram_[start_address_in_hex].img" (e.g. "ram_20400000.img"). These
can be created by running objcopy on RISC-V ELF executable.

The interface uses ncurses, and currently has the following commands:

    r    Toggle the CPU running flag
    
    R    Reset the CPU
    
  SPACE  Single step
  
    q    Quit
    
Your terminal has to have colour support, and if you use WIndows Subsystem for 
Linux make sure that your terminal type is set to "ansi.sys", as the WSL 
Terminfo database has a bug in it.

Building:
=========
Just run 'make'. To run, type "./main"
