CC=g++
CFLAGS=-I.
DEPS = fastalloc.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

rw_sep: fastalloc.o rw_sep.o 
	$(CC) -o rw_sep rw_sep.o fastalloc.o

clean:
	rm *.o results.txt 
