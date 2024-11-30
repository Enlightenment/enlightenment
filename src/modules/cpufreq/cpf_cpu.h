#ifndef CPF_CPU_H
#define CPF_CPU_H 1

typedef enum
{
  CPU_USER,
  CPU_NICE,
  CPU_SYSTEM,
  CPU_IDLE,
  CPU_IOWAIT,
  CPU_IRQ,
  CPU_SOFTIRQ,
  CPU_STEAL,
  CPU_GUEST,
  CPU_GUEST_NICE
} Cpu_Field;

typedef struct {
  long long          cpu; // cpu == -1 vs cpu0 == 0, cpu1 == 1, cpu2 == 2 etc.
  unsigned long long total;
  unsigned long long fields[10]; // extend in future
} Cpu_Counters;

typedef struct
{
  int min, max, cur;
#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
  int fd_scaling_cur_freq;
#endif
} Cpu_Freqinfo;

typedef struct
{
  long          num;
  Cpu_Counters *counters;
  Cpu_Freqinfo *freqinfo;
} Cpu_Slice;

typedef struct
{
  Cpu_Slice prev, cur;
  int cores;
  int fd_proc_stat;
} Cpu_Perf;

Cpu_Perf *cpu_perf_add(void);
void      cpu_perf_del(Cpu_Perf *cp);
void      cpu_perf_update(Cpu_Perf *cp);

#endif
