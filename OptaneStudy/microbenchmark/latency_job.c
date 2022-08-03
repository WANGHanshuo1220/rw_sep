#include "../src/common.h"
#include "../src/fastalloc.h"
#include <stdint.h>

#define TIMEDIFF(start, end)  ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec))

#define BASIC_OPS_TASK_COUNT 25
#define LATENCY_OPS_COUNT	1048576L // 1 MB
  
/* Latency Test
 * 1 Thread
 * Run a series of microbenchmarks on combination of store, flush and fence operations.
 * Sequential -> Random
 */
int latency_job()
{
	fastalloc * alloc;
	int i, j;
	long total;
	long average, stddev;
	long min, max;
	int skip;
	long result;

	long *rbuf = (long *)malloc(LATENCY_OPS_COUNT * sizeof(long));

	init_fast_allocator(false);

	// char *buf;
	printf("fastalloc begin...\n");
	char * buf = (char *)fast_alloc( LATENCY_OPS_COUNT * 256, true);
	printf("fastalloc end...\n");
	char *m = buf;

	/* Sequential */
	for (i = 0; i < BASIC_OPS_TASK_COUNT; i++)
	{
		skip = latency_tasks_skip[i];
		buf = m;
		// BENCHMARK_BEGIN(flags);
		for (j = 0; j < LATENCY_OPS_COUNT; j++)
		{
			result = bench_func[i](buf);

			buf += skip;
	
			rbuf[j] = result;

			if (result < min)
				min = result;
			if (result > max)
				max = result;
			total += result;
		}
		// BENCHMARK_END(flags);

		average = total / LATENCY_OPS_COUNT;
		stddev = 0;
		for (j = 0; j < LATENCY_OPS_COUNT; j++)
		{
			stddev += (rbuf[j] - average) * (rbuf[j] - average);
		}
		printf("[%d]%s avg %lu, stddev^2 %lu, max %lu, min %lu\n",\
			i, latency_tasks_str[i], average, stddev / LATENCY_OPS_COUNT, max, min);
		printf("[%d]%s Done\n", i, latency_tasks_str[i]);
	}

	/* Random */
	// for (i = 0; i < BASIC_OPS_TASK_COUNT; i++)
	// {
	// 	printf("Generating random bytes at %p, size %lx", loc, LATENCY_OPS_COUNT * 8);

	// 	printf("Running %s\n", latency_tasks_str[i]);
	// 	buf = ctx->addr;
	// 	total = 0;
	// 	min = 0xffffffff;
	// 	max = 0;
	// 	// rbuf = (long *)(sbi->rep->virt_addr + (i + BASIC_OPS_TASK_COUNT + 1) * LATENCY_OPS_COUNT * sizeof(long));

	// 	// BENCHMARK_BEGIN(flags);
	// 	for (j = 0; j < LATENCY_OPS_COUNT; j++)
	// 	{
	// 		result = bench_func[i](&buf[loc[j] & BASIC_OP_MASK]);
	// 		rbuf[j] = result;

	// 		if (result < min)
	// 			min = result;
	// 		if (result > max)
	// 			max = result;
	// 		total += result;
	// 	}
	// 	// BENCHMARK_END(flags);

	// 	average = total / LATENCY_OPS_COUNT;
	// 	stddev = 0;
	// 	for (j = 0; j < LATENCY_OPS_COUNT; j++)
	// 	{
	// 		stddev += (rbuf[j] - average) * (rbuf[j] - average);
	// 	}
	// 	printf("[%d]%s avg %lu, stddev^2 %lu, max %lu, min %lu\n", i, latency_tasks_str[i], average, stddev / LATENCY_OPS_COUNT, max, min);
	// 	printf("[%d]%s done\n", i, latency_tasks_str[i]);
	// }
	printf("LATTester_LAT_END:\n");
	// wait_for_stop();
	return 0;
}

int main()
{
	latency_job();
	return 0;
}
