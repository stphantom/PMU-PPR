
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>

#include "perf_event.h"
#include "matrix_multiply.h"



#define SLEEP_RUNS 3


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
                    pid_t pid, int cpu, int group_fd, unsigned long flags) {

   return syscall(__NR_perf_event_open,hw_event_uptr, pid, cpu,
                  group_fd, flags);
}



inline int get_pir(){
    unsigned long long  ret;
    __asm volatile (
        "mfspr 9, 896\n\t"
        "std 9, %0\n\t"
        :"=m" (ret)
        :
        :"9"
    );

    return (int)((ret>>50) & 0x7);
}
inline int get_ppr(){
    unsigned long long  ret;
    __asm volatile (
        "mfspr 9, 896\n\t"
        "std 9, %0\n\t"
        :"=m" (ret)
        :
        :"9"
    );

    return (int)((ret>>50) & 0x7);
}

inline void set_ppr(unsigned long long x) {

    __asm__ ( "li 0, 0x333\n\t"
              "ld 3, %0 \n\t"
              "sc \n\t"
              :
              :"m" (x)
              :"3"
            );

}



long long convert_to_ns(struct timespec *before,
                        struct timespec *after) {

  long long seconds;
  long long ns;

  seconds=after->tv_sec - before->tv_sec;
  ns = after->tv_nsec - before->tv_nsec;

  ns = (seconds*1000000000ULL)+ns;

  return ns;
}



int main(int argc, char **argv) {
   

   double error;

   int i,fd_insts,fd_cycles,read_result;
   long long cycles_count, insts_count;
   float IPC;

   struct timespec before,after;
   long long nsecs;
   struct perf_event_attr pe;

   int ppr;
   if (argc < 2) 
      ppr = 4;
   else 
      ppr = atoi(argv[1]); 


   memset(&pe,0,sizeof(struct perf_event_attr));
   pe.type=PERF_TYPE_HARDWARE;
   pe.size=sizeof(struct perf_event_attr);
   pe.config=PERF_COUNT_HW_CPU_CYCLES;
   pe.disabled=1;
   pe.exclude_kernel=1;
   pe.exclude_hv=1;


   fd_cycles=perf_event_open(&pe,0,-1,-1,0);
   if (fd_cycles<0) {
      fprintf(stderr,"Error opening leader %llx\n",pe.config);
   }


   pe.type=PERF_TYPE_HW_CACHE;
   pe.config=PERF_COUNT_HW_CACHE_L1D;
   //pe.config=PERF_COUNT_HW_INSTRUCTIONS;
   fd_insts=perf_event_open(&pe,0,-1,fd_cycles,0);
   if (fd_cycles<0) {
      fprintf(stderr,"Error opening event %llx\n",pe.config);
   }

      ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE,0);
      ioctl(fd_insts, PERF_EVENT_IOC_ENABLE,0);
   for(i=0;i<SLEEP_RUNS;i++) {

      ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
      ioctl(fd_insts, PERF_EVENT_IOC_RESET, 0);
     
      set_ppr(ppr);
      //sleep(1);
      clock_gettime(CLOCK_REALTIME,&before);
       naive_matrix_multiply(1);
      clock_gettime(CLOCK_REALTIME,&after);
     
      //ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE,0);
      //ioctl(fd_insts, PERF_EVENT_IOC_DISABLE,0);
      read_result=read(fd_cycles,&cycles_count,sizeof(long long));
      read_result=read(fd_insts,&insts_count,sizeof(long long));
      IPC = (float) insts_count / cycles_count;


      nsecs=convert_to_ns(&before,&after);
      
      printf("[Run %d] CYL: %lld  INS: %lld IPC: %f PPR: %d Time: %lld\n", i, cycles_count, insts_count, IPC, get_ppr(), nsecs);


   }


   return 0;
}
