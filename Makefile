COPTS=-Wall -pedantic -O3 -g
LOPTS=-lncurses

main : main.o memorymap.o ram.o uart.o riscv.o display.o prci.o rom.o
	gcc -o main main.o riscv.o memorymap.o ram.o uart.o display.o prci.o rom.o $(LOPTS) 

main.o : main.c memorymap.h display.h riscv.h
	gcc -c main.c $(COPTS)

riscv.o : riscv.c riscv.h memorymap.h
	gcc -c riscv.c $(COPTS)

memorymap.o : memorymap.c memorymap.h region.h ram.h uart.h prci.h rom.h
	gcc -c memorymap.c $(COPTS)

display.o : display.c display.h riscv.h
	gcc -c display.c $(COPTS)

ram.o : ram.c ram.h region.h
	gcc -c ram.c $(COPTS)

rom.o : rom.c rom.h region.h
	gcc -c rom.c $(COPTS)

prci.o : prci.c prci.h region.h
	gcc -c prci.c $(COPTS)

uart.o : uart.c uart.h region.h
	gcc -c uart.c $(COPTS)
