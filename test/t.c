/*=============================================================================
#
# Author: Zhengyu He (zhengyu.he@gatech.edu)
# School of Electrical & Computer Engineering
# Georgia Institute of Technology
#
# Last modified: 2011-02-14 16:12
#
# Filename: t.c
#
# Description: 
#
=============================================================================*/
#define _GNU_SOURCE
#include <pthread.h>
#include <sys/time.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "pmu_sched.h"
#include "matrix_multiply.h"


#define NUM_THREADS 4



volatile int flag;

unsigned long long bk_pri;
unsigned long long pri;
long n;


double usec()
{
    struct timeval t0;
    gettimeofday(&t0, NULL);
    return ((double) (t0.tv_sec * 1e6 + t0.tv_usec));

}



#define WORK  naive_matrix_multiply(1)


void *bar(void *threadid)
{
    int tid;
    double t1, t2;

    pmu_sched_init_thread();
    tid = (long) threadid;
    int i;

    if (tid == 0) {

	for (pri = 0; pri <= 7; pri++) {

	    n = 0;
	    t1 = usec();
	    for (i = 0; i < 5; i++) {
		pmu_sched_sample();
		WORK;
	    }

	    t2 = usec();
	    printf("pri = %lld ", get_ppr(), pri);
	    printf("\t\t | bk_pri = %lld , n = %ld\n", bk_pri, n);
	    printf("time: \t\t%8.2f usec \n", t2 - t1);
	    printf("------------------------\n");
	}

	flag = 0;

    } else {

	set_ppr(bk_pri);
	//t1 = usec();
	while (flag) {
	    WORK;
	    sleep(tid);
	    n++;
	}
	//t2 = usec();
	//printf("%8.2f background. %d \n", t2-t1, a);

    }
}

int main(int argc, char **argv)
{
    long i;
    cpu_set_t cpuset;
    pthread_attr_t attr[NUM_THREADS];
    pthread_t threads[NUM_THREADS];

    if (argc != 2) {
	printf("usage: %s bk_priority\n", argv[0]);
	exit(1);
    }
    bk_pri = atoi(argv[1]);
    flag = 1;


    pmu_sched_init();
    for (i = 0; i < NUM_THREADS; i++) {
	CPU_ZERO(&cpuset);
	CPU_SET(i, &cpuset);
	pthread_attr_init(&(attr[i]));
	pthread_attr_setaffinity_np(&(attr[i]), sizeof(cpu_set_t),
				    &cpuset);
    }

    for (i = 0; i < NUM_THREADS; i++) {
	pthread_create(&(threads[i]), &(attr[i]), bar, (void *) i);
    }


    pmu_sched_exit();
    pthread_exit(0);
}
