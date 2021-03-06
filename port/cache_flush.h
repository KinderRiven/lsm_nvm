#ifndef CACHE_FLUSH_H
#define CACHE_FLUSH_H

#include <stdio.h>
#include <stdlib.h>

#ifdef _ENABLE_PMEMIO
#include "pmdk/src/include/libpmem.h"
#endif

//Cacheline size
//TODO: Make it configurable
#define CACHE_LINE_SIZE 64
#define ASMFLUSH(dest) __asm__ __volatile__("clflush %0" \
                                            :            \
                                            : "m"(*(volatile char *)dest))

static int global_cpu_speed_mhz = 2200;
static int global_write_latency_ns = 500;

static unsigned long long asm_rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc"
                       : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static unsigned long long asm_rdtscp(void)
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtscp"
                       : "=a"(lo), "=d"(hi)::"rcx");
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static void init_pflush(int cpu_speed_mhz, int write_latency_ns)
{
  global_cpu_speed_mhz = cpu_speed_mhz;
  global_write_latency_ns = write_latency_ns;
}

static uint64_t cycles_to_ns(int cpu_speed_mhz, uint64_t cycles)
{
  return (cycles * 1000 / cpu_speed_mhz);
}

static uint64_t ns_to_cycles(int cpu_speed_mhz, uint64_t ns)
{
  return (ns * cpu_speed_mhz / 1000);
}

static inline int emulate_latency_ns(int ns)
{
  if (ns < 0)
  {
    return 0;
  }

  uint64_t cycles;
  uint64_t start;
  uint64_t stop;

  start = asm_rdtsc();
  cycles = ns_to_cycles(global_cpu_speed_mhz, ns);
  do
  {
    stop = asm_rdtsc();
  } while (stop - start < cycles);

  return 0;
}

static inline void clflush(volatile char *__p)
{
  asm volatile("clflush %0"
               : "+m"(*__p));
  emulate_latency_ns(global_write_latency_ns);
}

static inline void mfence()
{
  asm volatile("mfence" ::
                   : "memory");
  return;
}

static inline void flush_cache(void *ptr, size_t size)
{

#ifdef _ENABLE_PMEMIO
  pmem_persist((const void *)ptr, size);
#else
  unsigned int i = 0;
  uint64_t addr = (uint64_t)ptr;

  mfence();
  for (i = 0; i < size; i = i + CACHE_LINE_SIZE)
  {
    clflush((volatile char *)addr);
    addr += CACHE_LINE_SIZE;
  }
  mfence();
#endif
}

static inline void memcpy_persist(void *dest, void *src, size_t size)
{

#ifdef _ENABLE_PMEMIO
  pmem_memcpy_persist(dest, (const void *)src, size);
#else
  unsigned int i = 0;
  uint64_t addr = (uint64_t)dest;
  memcpy(dest, src, size);

  mfence();
  for (i = 0; i < size; i = i + CACHE_LINE_SIZE)
  {
    clflush((volatile char *)addr);
    addr += CACHE_LINE_SIZE;
  }
  mfence();
#endif
}
#endif
