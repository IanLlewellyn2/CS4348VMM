PROG = vmm
OBJS = vmm.o
CC = g++
CPPFLAGS = -c  #compile flags
#LDFLAGS =

vmm: vmm.o
	$(CC) -o vmm vmm.o
vmm.o: vmm.cpp 
	$(CC) $(CPPFLAGS) vmm.cpp
clean:
	rm -f core $(PROG) $(OBJS)
