// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * simple_copy.c -- show how to use pmem_memcpy_persist()
 *
 * usage: simple_copy src-file dst-file
 *
 * Reads 4k from src-file and writes it to dst-file.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <x86intrin.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <io.h>
#endif
#include <string.h>
#include <libpmem.h>

/* just copying 4k to pmem for this example */
#define BUF_LEN 4096

int N_RW = 100000;
int test_loop = 300;

int
main(int argc, char *argv[])
{
	struct timeval start, start2;
    struct timeval end, end2;
    unsigned long diff1, diff2, sum_diff1 = 0, sum_diff2 = 0;
    int n = 0;

	int srcfd;
	char buf[BUF_LEN];
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem = 1;
	int cc;

	if (argc != 3) {
		fprintf(stderr, "usage: %s src-file dst-file\n", argv[0]);
		exit(1);
	}

	/* open src-file */
	if ((srcfd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	/* create a pmem file and memory map it */
	if ((pmemaddr = pmem_map_file(argv[2], BUF_LEN,
				PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}

	for(int i = 0; i < test_loop; i++)
	{
		/* read up to BUF_LEN from srcfd */
		gettimeofday(&start, NULL);
		for(int j = 0; j < N_RW/2; j++)
		{
			if ((cc = read(srcfd, buf, BUF_LEN)) < 0) {
				pmem_unmap(pmemaddr, mapped_len);
				perror("read");
				exit(1);
			}
		}
	
		/* write it to the pmem */
		for(int j = 0; j < N_RW/2; j++)
		{
			if (is_pmem) {
				pmem_memcpy_persist(pmemaddr, buf, cc);
			} else {
				memcpy(pmemaddr, buf, cc);
				pmem_msync(pmemaddr, cc);
			}	
		}
		gettimeofday(&end, NULL);

		_mm_mfence();

		gettimeofday(&start2, NULL);
		for(int j = 0; j < N_RW/2; j++)
		{
			/* read up to BUF_LEN from srcfd */
			if ((cc = read(srcfd, buf, BUF_LEN)) < 0) {
				pmem_unmap(pmemaddr, mapped_len);
				perror("read");
				exit(1);
			}
	
			/* write it to the pmem */
			if (is_pmem) {
				pmem_memcpy_persist(pmemaddr, buf, cc);
			} else {
				memcpy(pmemaddr, buf, cc);
				pmem_msync(pmemaddr, cc);
			}	
		}
		gettimeofday(&end2, NULL);

		diff1 = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
		// printf("Mixed total time is %ld us\n", diff1);
		diff2 = 1000000 * (end2.tv_sec - start2.tv_sec) + end2.tv_usec - start2.tv_usec;
		// printf("Sep total time is %ld us\n\n", diff2);

		if(diff1 < diff2) n++;

		sum_diff1 += diff1;
		sum_diff2 += diff2;
	}
	printf("Avg speedup = %f%%\n",\
            100.0 * ((float)sum_diff1 - (float)sum_diff2)/(float)sum_diff1);

	close(srcfd);
	pmem_unmap(pmemaddr, mapped_len);

	exit(0);
}
