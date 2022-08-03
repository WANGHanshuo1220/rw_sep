#include <atomic>
#include <iostream>
#include <unistd.h>
#include <map>
#include <sys/time.h>
#include <x86intrin.h>
#include "fastalloc.h"

// concurrency_fastalloc* concurrency_myallocator;

int sep_grn = 513;
int N_RW = 100000;
int test_loop = 10000;
int times = (sep_grn -1) * 4 + 1;

typedef struct foo_s
{
    int64_t data[8];
} foo_t;

int main(int argc, char *argv[])
{
    struct timeval start, start2;
    struct timeval end, end2;
    unsigned long diff1, diff2, sum_diff1 = 0, sum_diff2 = 0;
    int n = 0;
    char str[100];
    FILE *fp = NULL;

    printf("opening file...\n");

    fp = fopen("results.txt", "w");
    
    if(!fp){
        printf("open file error\n");
    }
    else{
        printf("file open succes\n");
    }

    // init fast_allocator
    init_fast_allocator(true);

    // reinialize parameter
    if (argc >= 2) {
        printf("sep_grn (argv[1]): %d\n", atoi(argv[1]));
        sep_grn = atoi(argv[1]);
        times = (atoi(argv[1]) - 1) * 4 + 1;
    }
    else
    {
        printf("sep_grn (default): %d\n", sep_grn);
    }

    if (argc >= 3) {
        printf("test_loop (argv[2]): %d\n", atoi(argv[2]));
        test_loop = atoi(argv[2]);
    }
    else
    {
        printf("test_loop (default): %d\n", test_loop);
    }

    // allocate objs
    foo_t ** a = (foo_t **)malloc(8 * times);
    
    for(int i = 0; i < times; i++)
    {
        a[i] = (foo_t *)concurrency_fast_alloc(sizeof(foo_t), true);
    }

    printf("alloc done\n");

    for (int i = 0; i < times; i++)
    {
	printf("a[%d] addr = %p\n", i, a[i]);
        for (int j = 0; j < 8; j++)
        {
            a[i]->data[j] = j;
        }    
    }

    printf("init done\n");

    for(sep_grn = 16; sep_grn < 1024; sep_grn *= 2)
    {
        sum_diff1 = 0;
        sum_diff2 = 0;
	n = 0;
        for(int t = 0; t < test_loop; t++)
        {
            // -------------- wr_mix start ----------------
            gettimeofday(&start, NULL);
            for (int i = 0; i < N_RW / sep_grn; i++)
            {
                for(int j = 0; j < sep_grn; j++)
                {
                    if (a[4*(j+0)]->data[0]);
                    _mm_stream_si32((int *)&a[4*j]->data[0], a[4*j]->data[0] + 1);
                }
                // _mm_mfence();
            }
            gettimeofday(&end, NULL);  
            // -------------- wr_mix end -----------------

            _mm_mfence();

            // -------------- wr_sep start -----------------
            gettimeofday(&start2, NULL);
            for (int i = 0; i < N_RW / sep_grn; i++)
            {
                for (int j = 0; j < sep_grn; j+=2)
                {
                    if (a[4*j]->data[0]);
                    if (a[4*(j+1)]->data[0]);
                }

                for (int j = 0; j < sep_grn; j+=2)
                {
                    _mm_stream_si32((int *)&a[4*j]->data[0], a[4*j]->data[0] + 1);
                    _mm_stream_si32((int *)&a[4*(j+1)]->data[0], a[4*(j+1)]->data[0] + 1);
                }
                // _mm_mfence();
            }
            gettimeofday(&end2, NULL);  
            // -------------- wr_sep end -----------------

            // mesurement
            // printf("For %dth loop:\n", t);
            diff1 = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
            // printf("Mixed total time is %ld us\n", diff1);
            diff2 = 1000000 * (end2.tv_sec - start2.tv_sec) + end2.tv_usec - start2.tv_usec;
            // printf("Sep total time is %ld us\n\n", diff2);

            if(diff1 < diff2) n++;

            sum_diff1 += diff1;
            sum_diff2 += diff2;
        }
        printf("writing file\n");
        sprintf(str, "sep_grn = %d\t, speedup = %f%%\t, with %f%% positive(n = %d)\n", sep_grn, \
            100.0 * ((float)sum_diff1 - (float)sum_diff2)/(float)sum_diff1, 100.0 * (float)n/(float)test_loop,		  n);
        fputs(str, fp);
    }
    fast_free();
    _mm_mfence();

//    float r = (float)n / (float)test_loop;
//    printf("%.2f%% speedup posotive\n", r * 100.0);
//    printf("Avg Time speedup is %.2f%\n", \
//        100 * ((float)sum_diff1 - (float)sum_diff2) / (float)sum_diff1);

    return 0;
}
