# CC=g++
# CFLAGS=-I.
# DEPS = src/common.h 
# SDIR = ./microbenchmark/
# BDIR = ./bin/
# 
# $(BDIR)/%.o: $(SDIR)/%.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)
# 
# $(BDIR)/rw_sep: 
# 	$(CC) -o latency_job $(BDIR)latency_job.o
# 	rm $(BDIR)/*.o

IDIR = ./src
CC = g++
CFLAGS = -I$(IDIR) -lpthread

ODIR = ./bin

SDIR = ./microbenchmark

# ASM_DIR = /usr/include/asm-generic/ 

# $(ODIR)/latency_job: $(ODIR)/latency_job.o $(ODIR)/fastalloc.o $(ODIR)/memaccess.o
# 	$(CC) -o $@ $^ $(CFLAGS) # -s $(ASM_DIR)
# 
# $(ODIR)/latency_job.o: $(SDIR)/latency_job.c $(IDIR) 
# 	$(CC) -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)
# 
# $(ODIR)/fastalloc.o: $(IDIR)/fastalloc.cpp $(IDIR) 
# 	$(CC) -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)
# 
# $(ODIR)/memaccess.o: $(IDIR)/memaccess.c $(IDIR) 
# 	$(CC) -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)
# 
# clean:
# 	rm ./bin/*

$(ODIR)/raw: $(ODIR)/raw.o $(ODIR)/fastalloc.o $(ODIR)/memaccess.o
	$(CC) -g -o $@ $^ $(CFLAGS) # -s $(ASM_DIR)

$(ODIR)/raw.o: $(SDIR)/rw_sep_multithread.cpp $(IDIR) 
	$(CC) -g -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)

$(ODIR)/fastalloc.o: $(IDIR)/fastalloc.cpp $(IDIR) 
	$(CC) -g -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)

$(ODIR)/memaccess.o: $(IDIR)/memaccess.c $(IDIR) 
	$(CC) -g -c -o $@ $< $(CFLAGS) # -s $(ASM_DIR)

clean:
	rm ./bin/*
