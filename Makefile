CC=gcc 
CFLAGS=-g -m32
LDFLAGS=-L$(libdir) -m32
LDLIBS=-lvm


libdir=/home/clausoa/lib

.PHONY: clean

mmu: mmu.o 

check: 
	./mmu

clean:
	@echo -n "Cleaning..."
	@rm *~ *.o mmu 2>/dev/null | true
	@echo "done"
