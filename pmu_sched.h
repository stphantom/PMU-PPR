#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>

#include "perf_event.h"
#if defined __powerpc__
#include "ppr.h"
#endif



#define SAMPLE_DURATION 500000	/* in microseconds */
#define NUM_PMU_COUNTERS 2
__u64 PMU_COUNETERS_LIST[2] =
    { PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS }
#define MAX_PPR_V 6
#define MIN_PPR_V 2
#define DEFAULT_PPR_V 4

int pmu_sched_evaluate(){
    struct pmu_ctx *pc = get_pmu_ctx();
    double curr_IPC, prev_IPC;
    struct pmu_count *count;
    count = pc->curr_pmu_count;
    curr_IPC = (double) count->pmu_fd_count[2] / count->pmu_fd_count[1];
    count = count->prev_pmu_count;
    prev_IPC = (double) count->pmu_fd_count[2] / count->pmu_fd_count[1];

    if (curr_IPC < prev_IPC * 0.85)
	return 1;
    else
	return 0;

}



/* don't modify from here */

struct pmu_count {
    long long pmu_fd_count[NUM_PMU_COUNTERS];
    struct pmu_count *prev_pmu_count;
} struct pmu_ctx {
    int pmu_fd[NUM_PMU_COUNTERS];
    struct pmu_count *curr_pmu_count;
}
#ifndef __NR_perf_event_open
#if defined(__i386__)
#define __NR_perf_event_open    336
#elif defined(__x86_64__)
#define __NR_perf_event_open    298
#elif defined __powerpc__
#define __NR_perf_event_open    319
#elif defined __arm__
#define __NR_perf_event_open    364
#endif
#endif
int perf_event_open(struct perf_event_attr *hw_event_uptr,
		    pid_t pid, int cpu, int group_fd, unsigned long flags)
{

    return syscall(__NR_perf_event_open, hw_event_uptr, pid, cpu,
		   group_fd, flags);
}

#ifdef TLS
static __thread struct pmu_ctx *thread_pc = NULL;
#else				/* ! TLS */
static pthread_key_t thread_pc;
#endif				/* ! TLS */


static inline struct pmu_ctx *get_pmu_ctx()
{
#ifdef TLS
    return thread_pc;
#else				/* ! TLS */
    return (struct pmu_ctx *) pthread_getspecific(thread_pc);
#endif				/* ! TLS */
}


void pmu_init()
{
    /* Install timer_handler as the signal handler for SIGALRM.  */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &pmu_sched_sample_stop;
    sigaction(SIGALRM, &sa, NULL);

#ifndef TLS
    if (pthread_key_create(&pc, NULL) != 0) {
	fprintf(stderr, "Error creating thread local\n");
	exit(1);
    }
#endif				/* ! TLS */
}

struct pmu_ctx *pmu_sched_init_thread()
{

    pc = (struct pmu_ctx *) malloc(sizeof(struct pmu_ctx));
    struct perf_event_attr pe;
    int i, last_fd = -1;

    for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	memset(&pe, 0, sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_HARDWARE;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = PMU_COUNETERS_LIST[i];
	pe.disabled = 1;
	pe.exclude_kernel = 1;
	pe.exclude_hv = 1;


	pc->pmu_fd[i] = perf_event_open(&pe, 0, -1, last_fd, 0);
	if (pc->pmu_fd[i] < 0) {
	    fprintf(stderr, "Error opening %llx\n", pe.config);
	}
	last_fd = pc->pmu_fd[i];

    }
    pc->curr_pmu_count = NULL;


#ifdef TLS
    thread_pc = pc;
#else				/* ! TLS */
    pthread_setspecific(thread_pc, pc);
#endif				/* ! TLS */


    return pc;

}

void pmu_sched_exit()
{

#ifndef TLS
    pthread_key_delete(pc);
#endif				/* ! TLS */

}


int pmu_sched_sample()
{

    struct pmu_ctx *pc = get_pmu_ctx();
    int i;

    for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	ioctl(pc->pmu_fd[i], PERF_EVENT_IOC_RESET, 0);
	ioctl(pc->pmu_fd[i], PERF_EVENT_IOC_ENABLE, 0);
    }
    ualarm(SAMPLE_DURATION, 0);
    return 0;
}


void pmu_sched_sample_stop(int signum)
{

    struct pmu_ctx *pc = get_pmu_ctx();
    int read_result;
    int need_adjust;
    int curr_ppr;
    int i;

    struct pmu_count *curr_count;
    curr_count = (struct pmu_count *) malloc(sizeof(struct pmu_count));
    curr_count->prev_pmu_count = pc->curr_pmu_count;
    pc->curr_pmu_count = curr_count;

    for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	ioctl(pc->pmu_fd[i], PERF_EVENT_IOC_DISABLE, 0);
	read_result =
	    read(pc->pmu_fd[i], &(curr_count->pmu_fd_count[i]),
		 sizeof(long long));
	if (read_result < 0) {
	    printf("PMU read error\n");
	}
    }

    /* adjust: increase priority or switch to another cpu */
    need_adjust = pmu_sched_evaluate();
    if (need_adjust) {
	curr_ppr = get_ppr();
	if (curr_ppr < MAX_PPR_V)
	    set_ppr(curr_ppr + 1);
	else
	    pmu_sched_migrate_thread();
    }

}



void pmu_sched_migrate_thread()
{

    /* TODO: pthread_setaffinity_np */

}


void find_best_cpu()
{

    /* TODO: check cpu usage or just block the current processor */

}
