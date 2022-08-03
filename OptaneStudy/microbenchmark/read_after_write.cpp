/* SPDX-License-Identifier: GPL-2.0 */
#include "../src/common.h"
#include "../src/fastalloc.h"
#include "../src/memaccess.h"
#include <stdint.h>
#include <sys/time.h>

#define TIMEDIFF(start, end)  ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_usec - start.tv_usec))

#define LATENCY_OPS_COUNT 1048576L
#define ALIGN_PERTHREAD 1
#define CACHELINE_SIZE 64
#define GLOBAL_BIT			30 /* 1GB/Global */
#define GLOBAL_WORKSET		(1L << GLOBAL_BIT)
/* Latency Test
 * 1 Thread
 * Run a series of microbenchmarks on combination of store, flush and fence operations.
 * Sequential -> Random
 */
char *buf_sep1;
char *buf_sep1_;
char *buf_sep2;

char *region_end_sep1;
char *region_end_sep1_;
char *region_end_sep2;

const int port[2] = {0, 1};
char file1[30], file2[30];

fastalloc * alloc1, * alloc2;

#define size (1 << 24) // 16MB * 4byte = 64MB
int buf_flush_cache[size] = {0};

void flush_cache()
{
	for(int i = 0; i < size; i ++)
	{
		buf_flush_cache[i] += 1;
	}
}

void init(long count, long stride_size)
{
	alloc1 = init_fast_allocator(port[0]);
	alloc2 = init_fast_allocator(port[1]);

	buf_sep1 = (char *)fast_alloc(alloc1, count * stride_size, true);
	buf_sep1_ = (char *)fast_alloc(alloc1, count * stride_size, true);
	buf_sep2 = (char *)fast_alloc(alloc2, count * stride_size, true);

	region_end_sep1 = buf_sep1 + GLOBAL_WORKSET;
	region_end_sep1_ = buf_sep1_ + GLOBAL_WORKSET;
	region_end_sep2 = buf_sep2 + GLOBAL_WORKSET;

	// printf("RW_SEP:\n	Buf_sep1 working set begin: %p end: %p\n	Buf_sep2 working set begin: %p end: %p\n",
	// 	buf_sep1, region_end_sep1, buf_sep2, region_end_sep2);
	// printf("RW_MIX:\n	Buf_mix working set begin: %p end: %p\n",
	// 	buf_mix, region_end_mix);
	// printf("out init\n");
}

int read_after_write_job()
{
	long access_size = 64;
	long stride_size = 64;
	long delay = 200;
	long count = GLOBAL_WORKSET / access_size;
	struct timeval tstart, tend;
	unsigned int c_store_start_hi, c_store_start_lo;
	unsigned int c_ntload_start_hi, c_ntload_start_lo;
	unsigned int c_ntload_end_hi, c_ntload_end_lo;
	unsigned long c_store_start;
	unsigned long c_ntload_start, c_ntload_end;
	long diff;
	double throughput;

	printf("size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		access_size, stride_size, delay, count);

#define RAW_BEFORE_WRITE 												\
	gettimeofday(&tstart, NULL); 										\
	asm volatile ( 														\
		"rdtscp \n\t" 													\
		"lfence \n\t" 													\
		"mov %%edx, %[hi]\n\t" 											\
		"mov %%eax, %[lo]\n\t" 											\
		: [hi] "=r" (c_store_start_hi), [lo] "=r" (c_store_start_lo) 	\
		: 																\
		: "rdx", "rax", "rcx" 											\
	);

#define RAW_BEFORE_READ													\
	asm volatile (														\
		"rdtscp \n\t"	/* read time-stamp counter init EDX:EAX regs */	\
		"lfence \n\t"													\
		"mov %%edx, %[hi]\n\t"											\
		"mov %%eax, %[lo]\n\t"											\
		: [hi] "=r" (c_ntload_start_hi), [lo] "=r" (c_ntload_start_lo)	\
		:																\
		: "rdx", "rax", "rcx"											\
	);

#define RAW_FINAL(job_name)												\
	asm volatile (														\
		"rdtscp \n\t"													\
		"lfence \n\t"													\
		"mov %%edx, %[hi]\n\t"											\
		"mov %%eax, %[lo]\n\t"											\
		: [hi] "=r" (c_ntload_end_hi), [lo] "=r" (c_ntload_end_lo)		\
		:																\
		: "rdx", "rax", "rcx"											\
	);																	\
	gettimeofday(&tend, NULL);												\
	diff = TIMEDIFF(tstart, tend);										\
	c_store_start = (((unsigned long)c_store_start_hi) << 32) | c_store_start_lo;			\
	c_ntload_start = (((unsigned long)c_ntload_start_hi) << 32) | c_ntload_start_lo;		\
	c_ntload_end = (((unsigned long)c_ntload_end_hi) << 32) | c_ntload_end_lo;				\
	printf("[%s] count %ld, total %ld ns, average %ld ns, ",		\
		#job_name, count, diff, diff / count);	\


	asm volatile ("mfence \n" :::);

	// Separate RaW job
	RAW_BEFORE_WRITE
	init(count, stride_size);
	stride_storeclwb(buf_sep1, access_size, stride_size, delay, count);
	asm volatile ("mfence \n" :::);
	RAW_BEFORE_READ
	stride_nt(buf_sep1, access_size, stride_size, delay, count);
	asm volatile ("mfence \n" :::);
	RAW_FINAL("raw-separate")
	fast_free(alloc1);
	fast_free(alloc2);

	// Naive RaW job
	init(count, stride_size);
	RAW_BEFORE_WRITE
	RAW_BEFORE_READ
	stride_read_after_write(buf_sep1, access_size, stride_size, delay, count);
	asm volatile ("mfence \n" :::);
	RAW_FINAL("raw-combined")
	fast_free(alloc1);
	fast_free(alloc2);

	flush_cache();
	asm volatile ("mfence \n" :::);
		
	printf("\n\n------------------------ port: {%d, %d} ------------------------------\n", port[0], port[1]);

	/*
	 * Task1: rw_sep on different channel job
	 */
	printf("\nTask1: sep rw on different channel, write all first then read all\n");

	init(count, stride_size);
	flush_cache();
	asm volatile ("mfence \n" :::);

	RAW_BEFORE_WRITE
	for(int i = 0; i < count; i++);
	stride_storeclwb(buf_sep1, access_size, stride_size, delay, count);
	RAW_BEFORE_READ
	stride_nt(buf_sep2, access_size, stride_size, delay, count);
	asm volatile ("mfence \n" :::);
	RAW_FINAL("rw_sep on diff channel")

	fast_free(alloc1);
	fast_free(alloc2);

	throughput = (double)count / (double)diff;
	printf("throughput= %.2f M ops/s \n", throughput * 1000.0);

	flush_cache();
	asm volatile ("mfence \n" :::);

	/*
	 * Task2: rw_sep on different channel job
	 */
	printf("\nTask2: sep rw on different channel, write and read mixed\n");
	long grn = 1;
	for(; grn <= 16; grn *=2)
	{
		init(count, stride_size);
		flush_cache();
		asm volatile ("mfence \n" :::);
		printf("grn = %ld: ", grn * access_size);
		RAW_BEFORE_WRITE
		RAW_BEFORE_READ
		for(int i = 0; i < count / grn; i++)
		{
			stride_storeclwb(buf_sep1 + i * grn * stride_size, grn * access_size, 0, delay, 1);
			stride_nt(buf_sep2 + i * grn * stride_size, grn * access_size, 0, delay, 1);
		}	
		asm volatile ("mfence \n" :::);
		RAW_FINAL("rw_sep on diff channel")
		
		throughput = (double)count / (double)diff;
		printf("throughput= %.2f M ops/s \n", throughput * 1000.0);

		fast_free(alloc1);
		fast_free(alloc2);
	}

	flush_cache();
	asm volatile ("mfence \n" :::);
	
	/*
	 * Task3: rw_sep on same channel job
	 */
	printf("\nTask3: rw on same channel, write all first then read all\n");

	init(count, stride_size);
	
	flush_cache();
	asm volatile ("mfence \n" :::);

	RAW_BEFORE_WRITE
	for(int i = 0; i < count; i++);
	stride_storeclwb(buf_sep1, access_size, stride_size, delay, count);
	RAW_BEFORE_READ
	stride_nt(buf_sep1_, access_size, stride_size, delay, count);
	asm volatile ("mfence \n" :::);
	RAW_FINAL("rw_sep on same channel")

	fast_free(alloc1);
	fast_free(alloc2);

	throughput = (double)count / (double)diff;
	printf("throughput= %.2f M ops/s \n", throughput * 1000.0);

	flush_cache();
	asm volatile ("mfence \n" :::);
	
	/*
	 * Task4: rw_sep on same channel job
	 */
	printf("\nTask4: rw on same channel, write and read mixed\n");
	grn = 1;
	for(; grn <= 16; grn *= 2)
	{
		init(count, stride_size);

		flush_cache();
		asm volatile ("mfence \n" :::);

		printf("grn = %ld: ", grn * access_size);
		RAW_BEFORE_WRITE
		RAW_BEFORE_READ
		for(int i = 0; i < count / grn; i++)
		{
			stride_storeclwb(buf_sep1 + i * grn * stride_size, grn * access_size, 0, delay, 1);
			stride_nt(buf_sep1_ + i * grn * stride_size, grn * access_size, 0, delay, 1);
		}
		asm volatile ("mfence \n" :::);
		RAW_FINAL("rw_sep on same channel")

		throughput = (double)count / (double)diff;
		printf("throughput= %.2f M ops/s \n", throughput * 1000.0);

		fast_free(alloc1);
		fast_free(alloc2);
	}

	asm volatile ("mfence \n" :::);
	return 0;
}

int main()
{
	read_after_write_job();
	return 0;
}
