/* SPDX-License-Identifier: GPL-2.0 */
#include "../src/common.h"
#include "../src/fastalloc.h"
// #include "../src/memaccess.h"
#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>

#define TIMEDIFF(start, end)  ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_usec - start.tv_usec))

#define LATENCY_OPS_COUNT 1048576L
#define ALIGN_PERTHREAD 1
#define CACHELINE_SIZE 64
#define GLOBAL_BIT			30 /* 1GB/Global */
#define GLOBAL_WORKSET		(1L << GLOBAL_BIT)
#define NUMTHREAD 8
/* Latency Test
 * 1 Thread
 * Run a series of microbenchmarks on combination of store, flush and fence operations.
 * Sequential -> Random
 */
char *buf_sep1[NUMTHREAD];
char *buf_sep1_[NUMTHREAD];
char *buf_sep2[NUMTHREAD];

char *region_end_sep1;
char *region_end_sep1_;
char *region_end_sep2;

int port[2] = {0, 1};
char file1[30], file2[30];

struct foo {
	char * start_addr;
	long access_size;
	long stride_skip;
	long delay;
	long count;
};
struct foo f1[NUMTHREAD];
struct foo f2[NUMTHREAD];

long access_size = 64;
long stride_size = 64;
long delay = 200;
long count = GLOBAL_WORKSET / access_size;
struct timeval tstart, tend;
long diff;
double throughput;
int ret;
pthread_t *tid_w = new pthread_t[NUMTHREAD];
pthread_t *tid_r = new pthread_t[NUMTHREAD];
fastalloc * alloc1, * alloc2;

#define cache_size (1 << 24) // 16MB * 4byte = 64MB
int buf_flush_cache[cache_size] = {0};

#define SIZEBTSTFLUSH_64_AVX512		\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clwb  0x0(%%r9, %%r10) \n" \
				"add $0x40, %%r10 \n"

#define SIZEBTNT_64_AVX512		\
				"vmovntdq  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"add $0x40, %%r10 \n"

#define SIZEBTST_FENCE	"mfence \n"
#define SIZEBTLD_FENCE  "mfence \n"

void *stride_nt(void * arg)
{
	struct foo *f = (struct foo*)arg;
	char * start_addr = f->start_addr;
	long size = f->access_size;
	long count = f->count;
	long skip = f->stride_skip;
	long delay = f->delay;
	asm volatile (
		"xor %%r8, %%r8 \n"						/* r8: access offset */
		"xor %%r11, %%r11 \n"					/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
// 1
"LOOP_STRIDENT_OUTER: \n"						/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"	/* r9: access loc */
		"xor %%r10, %%r10 \n"					/* r10: accessed size */
"LOOP_STRIDENT_INNER: \n"						/* inner (access) loop, unroll 8 times */
		SIZEBTNT_64_AVX512							/* Access: uses r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDENT_INNER \n"
		SIZEBTLD_FENCE

		"xor %%r10, %%r10 \n"
"LOOP_STRIDENT_DELAY: \n"						/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDENT_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDENT_OUTER \n"

		:: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay):
			"%r11", "%r10", "%r9", "%r8");
}

void *stride_storeclwb(void * arg)
{
	struct foo *f = (struct foo*)arg;
	char * start_addr = f->start_addr;
	long size = f->access_size;
	long count = f->count;
	long skip = f->stride_skip;
	long delay = f->delay;
	asm volatile (
		"xor %%r8, %%r8 \n"						/* r8: access offset */
		"xor %%r11, %%r11 \n"					/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
// 1
"LOOP_STRIDESTFLUSH_OUTER: \n"						/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"	/* r9: access loc */
		"xor %%r10, %%r10 \n"					/* r10: accessed size */
"LOOP_STRIDESTFLUSH_INNER: \n"						/* inner (access) loop, unroll 8 times */
		SIZEBTSTFLUSH_64_AVX512						/* Access: uses r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDESTFLUSH_INNER \n"
		SIZEBTST_FENCE

		"xor %%r10, %%r10 \n"
"LOOP_STRIDESTFLUSH_DELAY: \n"						/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDESTFLUSH_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDESTFLUSH_OUTER \n"

		:: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay):
			"%r11", "%r10", "%r9", "%r8");
}

void flush_cache()
{
	for(int i = 0; i < cache_size; i ++)
	{
		buf_flush_cache[i] += 1;
	}
}

void set_arg(long access_size, long stride_size, long delay, long count)
{
	for(int i = 0; i < NUMTHREAD; i++)
	{
		f1[i].start_addr = buf_sep1[i];
		f1[i].count = count;
		f1[i].delay = delay;
		f1[i].access_size = access_size;
		f1[i].stride_skip = stride_size;
	}
	for(int i = 0; i < NUMTHREAD; i++)
	{
		f2[i].start_addr = buf_sep2[i];
		f2[i].count = count;
		f2[i].delay = delay;
		f2[i].access_size = access_size;
		f2[i].stride_skip = stride_size;
	}
}

void init(long count, long stride_size)
{
	alloc1 = init_fast_allocator(port[0]);
	alloc2 = init_fast_allocator(port[1]);

	for(int i = 0; i < NUMTHREAD; i++)
	{
		buf_sep1[i] = (char *)fast_alloc(alloc1, count * stride_size, true);
		// buf_sep1_ = (char *)fast_alloc(alloc1, count * stride_size, true);
		buf_sep2[i] = (char *)fast_alloc(alloc2, count * stride_size, true);
	}
}

int read_after_write_job(int ch1, int ch2)
{
	port[0] = ch1;
	port[1] = ch2;

#define MESURE_BEGIN												\
	gettimeofday(&tstart, NULL); 									\

#define MESURE_END(job_name)										\
	gettimeofday(&tend, NULL);										\
	diff = TIMEDIFF(tstart, tend);									\
	printf("total %ld ns, average %ld ns, ",		\
		diff, diff / count);	\


	asm volatile ("mfence \n" :::);

	printf("\n\n------------------------ port: {%d, %d} ------------------------------\n", port[0], port[1]);

	/*
	 * Task1: rw on same channel
	 */
	printf("\nTask1: rw on same channel\n");

	init(count, stride_size);
	flush_cache();
	asm volatile ("mfence \n" :::);

	MESURE_BEGIN
	set_arg(access_size, stride_size, delay, count);
	for(int i = 0; i < NUMTHREAD; i++)
	{
		ret = pthread_create(&tid_w[i], NULL, &stride_storeclwb, &f1[i]);
		if(ret != 0)
		{
			cout << "pthread_create error: error_code=" << ret << endl;
		}
		ret = pthread_create(&tid_r[i], NULL, &stride_nt, &f1[i]);
		if(ret != 0)
		{
			cout << "pthread_create error: error_code=" << ret << endl;
		}
	}
	for(int i = 0; i < NUMTHREAD; i++)
	{
		pthread_join(tid_r[i], NULL);
		pthread_join(tid_w[i], NULL);
	}
	asm volatile ("mfence \n" :::);
	MESURE_END("rw_sep on diff channel")

	fast_free(alloc1);
	fast_free(alloc2);

	long avg_time1 = diff / count;
	double throughput1 = (double)count * NUMTHREAD / (double)diff;
	printf("throughput= %.2f M ops/s \n", throughput1 * 1000.0);

	flush_cache();
	asm volatile ("mfence \n" :::);

	/*
	 * Task2: rw on diff channel
	 */
	printf("\nTask2: rw on diff channel\n");

	init(count, stride_size);
	flush_cache();
	asm volatile ("mfence \n" :::);

	MESURE_BEGIN
	set_arg(access_size, stride_size, delay, count);
	for(int i = 0; i < NUMTHREAD; i++)
	{
		ret = pthread_create(&tid_w[i], NULL, &stride_storeclwb, &f1[i]);
		if(ret != 0)
		{
			cout << "pthread_create error: error_code=" << ret << endl;
		}
		ret = pthread_create(&tid_r[i], NULL, &stride_nt, &f2[i]);
		if(ret != 0)
		{
			cout << "pthread_create error: error_code=" << ret << endl;
		}
	}
	for(int i = 0; i < NUMTHREAD; i++)
	{
		pthread_join(tid_r[i], NULL);
		pthread_join(tid_w[i], NULL);
	}
	asm volatile ("mfence \n" :::);
	MESURE_END("rw_sep on diff channel")

	fast_free(alloc1);
	fast_free(alloc2);

	long avg_time2 = diff / count;
	double throughput2 = (double)count * NUMTHREAD / (double)diff;
	printf("throughput= %.2f M ops/s \n", throughput2 * 1000.0);

	flush_cache();
	asm volatile ("mfence \n" :::);
	
	/*
	 * Task3: rw on same channel, but seperate
	 */
	printf("\nTask3: rw on diff channel, but seperate\n");

	init(count, stride_size);
	flush_cache();
	asm volatile ("mfence \n" :::);

	MESURE_BEGIN
	set_arg(access_size, stride_size, delay, count);
	for(int i = 0; i < NUMTHREAD; i++)
	{
		ret = pthread_create(&tid_w[i], NULL, &stride_storeclwb, &f1[i]);
		if(ret != 0)
		{
			cout << "pthread_create error: error_code=" << ret << endl;
		}
	}
	for(int i = 0; i < NUMTHREAD; i++)
	{
		pthread_join(tid_w[i], NULL);
	}

	for(int i = 0; i < NUMTHREAD; i++)
	{
		ret = pthread_create(&tid_r[i], NULL, &stride_nt, &f1[i]);
		if(ret != 0)
		{
		    cout << "pthread_create error: error_code=" << ret << endl;
		}
	}
	for(int i = 0; i < NUMTHREAD; i++)
	{
		pthread_join(tid_r[i], NULL);
	}

	asm volatile ("mfence \n" :::);
	MESURE_END("rw_sep on diff channel")

	fast_free(alloc1);
	fast_free(alloc2);

	long avg_time3 = diff / count;
	double throughput3 = (double)count * NUMTHREAD / (double)diff;
	printf("throughput= %.2f M ops/s \n", throughput3 * 1000.0);

	flush_cache();
	asm volatile ("mfence \n" :::);

	printf("\n\tlat(ns)\tbw(M/s)\t\tspeedup\t\n");
	printf("task1\t%ld\t%f\t----\n", avg_time1, 1000.0 * throughput1);
	printf("task2\t%ld\t%f\t%.2f\n", avg_time2, 1000.0 * throughput2, throughput2 / throughput1);
	printf("task3\t%ld\t%f\t%.2f\n", avg_time3, 1000.0 * throughput3, throughput3 / throughput1);

	return 0;
}

int main()
{
	printf("size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		access_size, stride_size, delay, count);
	read_after_write_job(0, 1);
	read_after_write_job(1, 2);
	read_after_write_job(2, 3);
	read_after_write_job(3, 0);
	return 0;
}
