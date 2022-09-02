CC=gcc
CFLAGS=-g -pedantic -l crypto -std=gnu11 -Wall -Werror -Wextra 

.PHONY: all 
all: nyufile 

nyufile: nyufile.o sysinfo.o rootinfo.o recover.o	
	$(CC) -o $@ $^ $(CFLAGS)

nyufile.o: nyufile.c nyufile.h

sysinfo.o: sysinfo.c nyufile.h

rootinfo.o: rootinfo.c nyufile.h

recover.o: recover.c nyufile.h

.PHONY: clean
clean:
	rm -f *.o nyuenc
