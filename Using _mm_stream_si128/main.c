// SYSTEM BUS RADIO
// https://github.com/fulldecent/system-bus-radio
// Copyright 2016 William Entriken

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>


uint64_t ticks_per_sec;

static uint64_t mach_absolute_time(void) {
#ifdef WIN32
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
#endif
#ifdef __GNUC__
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);                             \
  int64_t time_ns = (ticks_per_sec*(uint64_t)t.tv_sec) + ((uint64_t)t.tv_nsec);
  return time_ns;
#endif
}

#define MEM_SIZE (1<<23)
int mem[MEM_SIZE*3]; 
uint64_t offset = 128;
uint64_t mask1 = (1<<(23))-1;
int* ptr1 = mem;

// estimate memory accesses per second (very roughly)
static int mach_estimate_mem_access_per_sec(float tune_time) {
  printf("Estimating memory accesses per second ... \n");
  // mem_accesses should be large enough for average behavior
  // and small enough to finish some tries without interrupts
  int mem_accesses = 10000; 
  uint64_t min_ticks = UINT64_MAX;
  uint64_t tune_start = mach_absolute_time();
  uint64_t now = mach_absolute_time();

  while ((now - tune_start)*1.0/ticks_per_sec < tune_time) {
    uint64_t start = mach_absolute_time();
    int value = 0;
    for (int i = 0; i < mem_accesses; i++) {
      ptr1=(int*) (( ((uint64_t)ptr1) &~mask1 )|( (((uint64_t)ptr1) +offset) &mask1 ));
      *((int volatile*) &value) = *ptr1; //volatile prevents optimization removing it
    }
    now = mach_absolute_time();
    uint64_t ticks = mach_absolute_time() - start;
    if (ticks < min_ticks) {
      min_ticks = ticks;
    }
  }
  // (ops/sec) = ops*(ticks/sec)/ticks
  return (int) ((mem_accesses * ticks_per_sec) / min_ticks);
}


static void square_am_signal(float time, float frequency, int mem_access_per_sec) {
  printf("Playing / %0.3f seconds / %4.0f Hz\n", time, frequency);
  // ticks/cycle = (ticks/sec)/(cycles/sec)
  uint64_t ticks_per_cycle = (uint64_t) (ticks_per_sec / frequency);

  uint64_t start = mach_absolute_time();
  uint64_t end = (uint64_t) (start + time * ticks_per_sec);
  //printf("start = %"PRIu64"\nend   = %"PRIu64"\n", start, end);
  uint64_t now = mach_absolute_time();
  int mems_per_half_cycle = (int) (mem_access_per_sec/(2*frequency));
  //printf("mems_per_half_cycle = %d\n", mems_per_half_cycle);
  int cycle = 0;
  while (start < end) {
    // mems/(cycle/2) = (mems/sec)/(2*cycle/sec)
    int value;
    for (int i = 0; i < mems_per_half_cycle; i++) { 
		ptr1 = (int*)((((int)ptr1) &~mask1) | ((((int)ptr1) + offset) &mask1));
		*((int volatile*)&value) = *ptr1; // volatile prevents optimization removing it
    }    

    uint64_t reset = start + ticks_per_cycle;
    while (mach_absolute_time() < reset) {}
    start = reset;
    // if (((cycle++) % ((int) frequency)) == 0) printf("%d\n", cycle);
  }
}

int main(int argc, char* argv[]){
  ptr1 = (int*) 0;
  while(ptr1 < mem) { // align on a large boundary
    ptr1 = ptr1 + MEM_SIZE;
  }

  if (argc < 3 || argc > 4) {
    printf("usage: system-bus-radio <run_time (seconds)> <run_frequency (Hz)>\n");
    return 0;
  }
  
  float run_time    = (float) atof(argv[1]);
  float run_frequency = (float) atoi(argv[2]);
  int mem_access_per_sec = -1;
  if (argc == 4) {
    mem_access_per_sec = atoi(argv[3]);
  }

#ifdef WIN32
  LARGE_INTEGER pfreq;
  if(!QueryPerformanceFrequency(&pfreq) ){
    printf("ERROR: Could not read the performance frequency");
  }
    
  ticks_per_sec = pfreq.QuadPart;
#endif
#ifdef __GNUC__
  ticks_per_sec = 1000000000ULL;
#endif   
  printf("ticks_per_sec = %"PRIu64"\n", ticks_per_sec);
  if (mem_access_per_sec == -1) {
    mem_access_per_sec = mach_estimate_mem_access_per_sec(2.0); 
  }
  printf("mem_access_per_sec = %d\n", mem_access_per_sec);

  if (run_frequency < 0) {
    while (1) {
      square_am_signal(run_time/5, 30000, mem_access_per_sec);
      square_am_signal(run_time/5, 35000, mem_access_per_sec);
      square_am_signal(run_time/5, 40000, mem_access_per_sec);
      square_am_signal(run_time/5, 45000, mem_access_per_sec);
      square_am_signal(run_time/5, 50000, mem_access_per_sec);
    }
  } else {
    square_am_signal(run_time, run_frequency, mem_access_per_sec);
  }
  printf("DONE\n");
  return 0;
}
