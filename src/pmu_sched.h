#include "perf_event.h"

#define SAMPLE_DURATION 900000	/* in microseconds */
#define NUM_PMU_COUNTERS 2
__u64 PMU_COUNETERS_LIST[2] =
    { PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS };
#define MAX_PPR_V 6
#define MIN_PPR_V 2
#define DEFAULT_PPR_V 4



void pmu_sched_exit();

void pmu_sched_sample();

void pmu_sched_init();

void pmu_sched_init_thread();
