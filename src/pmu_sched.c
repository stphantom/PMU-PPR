#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>
#include <pthread.h>

#include "perf_event.h"
#include "pmu_sched.h"
#if defined __powerpc__
#include "ppr.h"
#endif


#define TARGET_LOW 2.0
#define TARGET_HIGH 2.0
#define SAMPLE_DURATION 200000	/* in microseconds */
#define SAMPLE_INTERVAL 500000	/* in microseconds */
#define PMU_PERF_TYPE PERF_TYPE_HARDWARE
#define NUM_PMU_COUNTERS 2
__u64 PMU_COUNETERS_LIST[NUM_PMU_COUNTERS] = {
    PERF_COUNT_HW_CPU_CYCLES,
    PERF_COUNT_HW_INSTRUCTIONS
};

#define MAX_PPR_V 6
#define MIN_PPR_V 1
#define DEFAULT_PPR_V 4


/* don't modify from here */


struct pmu_count {
    __u64 pmu_fd_count[NUM_PMU_COUNTERS];
    struct pmu_count *prev_pmu_count;
};

struct pmu_ctx {
    int enabled;
    int sample_interval;
    int pmu_fd[NUM_PMU_COUNTERS];
    struct pmu_count *curr_pmu_count;
};

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


void pmu_sched_exit()
{

#ifndef TLS
    pthread_key_delete(thread_pc);
#endif				/* ! TLS */

}



float pmu_sched_get_perf_value()
{
    struct pmu_ctx *pc = get_pmu_ctx();
    double curr_IPC;
    struct pmu_count *count = pc->curr_pmu_count;
    if (count != NULL)
        curr_IPC = (double) count->pmu_fd_count[1] / count->pmu_fd_count[0];
    else
	return 0.0;

    return curr_IPC;
}


void pmu_sched_migrate_thread()
{

    /* TODO: pthread_setaffinity_np */

}


void find_best_cpu()
{

    /* TODO: check cpu usage or just block the current processor */

}


void pmu_sched_sample_stop(int signum)
{

    struct pmu_ctx *pc = get_pmu_ctx();
    int read_result;
    int need_adjust = 0;
    int curr_ppr, target_ppr;
    float target;
    int i;

    struct pmu_count *curr_count;
    curr_count = (struct pmu_count *) malloc(sizeof(struct pmu_count));
    curr_count->prev_pmu_count = pc->curr_pmu_count;
    pc->curr_pmu_count = curr_count;

    for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	read_result =
	    read(pc->pmu_fd[i], &(curr_count->pmu_fd_count[i]),
		 sizeof(__u64));
	if (read_result < 0) {
	    printf("PMU read error\n");
	}
    }



    /* adjust: increase priority or switch to another cpu */
    target = pmu_sched_get_perf_value();

    if (target < TARGET_LOW)
	need_adjust = 1;
    else if (target > TARGET_HIGH)
	need_adjust = -1;

    if (need_adjust != 0) {
	curr_ppr = get_ppr();
	target_ppr = curr_ppr + need_adjust;
	if (target_ppr <= MAX_PPR_V && target_ppr >= MIN_PPR_V) {
	    pc->sample_interval = SAMPLE_INTERVAL;
	    set_ppr(target_ppr);
	} else {
	    pmu_sched_migrate_thread();
	}
    } else
	pc->sample_interval += SAMPLE_INTERVAL;

    /* Install timer_handler as the signal handler for SIGALRM.  */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &pmu_sched_sample_start;
    sigaction(SIGALRM, &sa, NULL);
    if (pc->sample_interval >= 1000000)
	alarm(pc->sample_interval / 1000000);
    else
	ualarm(pc->sample_interval, 0);
}

void pmu_sched_sample_start()
{

    struct pmu_ctx *pc = get_pmu_ctx();
    int i;
    if (pc->enabled != 1) {
	for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	    ioctl(pc->pmu_fd[i], PERF_EVENT_IOC_ENABLE, 0);
	}
	pc->enabled = 1;
    }
    for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	ioctl(pc->pmu_fd[i], PERF_EVENT_IOC_RESET, 0);
    }
    /* Install timer_handler as the signal handler for SIGALRM.  */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &pmu_sched_sample_stop;
    sigaction(SIGALRM, &sa, NULL);
    ualarm(SAMPLE_DURATION, 0);


}

void pmu_sched_init()
{
    /* Install timer_handler as the signal handler for SIGALRM.  */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &pmu_sched_sample_stop;
    sigaction(SIGALRM, &sa, NULL);

#ifndef TLS
    if (pthread_key_create(&thread_pc, NULL) != 0) {
	fprintf(stderr, "Error creating thread local\n");
	exit(1);
    }
#endif				/* ! TLS */
}

void pmu_sched_init_thread(int thread_type)
{
    if (thread_type == MAIN_THREAD) {
	struct pmu_ctx *pc =
	    (struct pmu_ctx *) malloc(sizeof(struct pmu_ctx));
	struct perf_event_attr pe;
	int i, group_fd = -1;

	pc->enabled = 0;
	pc->sample_interval = SAMPLE_INTERVAL;

	for (i = 0; i < NUM_PMU_COUNTERS; i++) {
	    memset(&pe, 0, sizeof(struct perf_event_attr));
	    pe.type = PMU_PERF_TYPE;
	    pe.size = sizeof(struct perf_event_attr);
	    pe.config = PMU_COUNETERS_LIST[i];
	    pe.disabled = 1;
	    pe.exclude_kernel = 1;
	    pe.exclude_hv = 1;


	    pc->pmu_fd[i] = perf_event_open(&pe, 0, -1, group_fd, 0);
	    if (pc->pmu_fd[i] < 0) {
		fprintf(stderr, "Error opening %llu\n",
			(long long unsigned) pe.config);
	    }

	    if (group_fd == -1)
		group_fd = pc->pmu_fd[i];

	}
	pc->curr_pmu_count = NULL;


#ifdef TLS
	thread_pc = pc;
#else				/* ! TLS */
	pthread_setspecific(thread_pc, pc);
#endif				/* ! TLS */
    } else {
	sigset_t timer_mask;
	sigemptyset(&timer_mask);
	sigaddset(&timer_mask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &timer_mask, NULL);
    }

}
