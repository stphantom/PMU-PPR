/* header file */

#define MAIN_THREAD 1
#define SECONDARY_THREAD 0
void pmu_sched_exit();

void pmu_sched_sample_start();

void pmu_sched_init();

void pmu_sched_init_thread(int);

float pmu_sched_get_perf_value();
