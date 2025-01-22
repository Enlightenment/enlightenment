#include "cpf_cpu.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#if defined (__FreeBSD__) || defined(__DragonFly__)
# include <sys/sysctl.h>
#endif


#define CPU_CORES_MAX 4096

static void
_mem_alloc_result(Cpu_Perf *cp)
{
  if (!cp->prev.counters || !cp->prev.freqinfo || !cp->cur.counters || !cp->cur.freqinfo)
    {
      fprintf(stderr, "PANIC! Out of memory to alloc small data for cpu usage/freq\n");
      abort();
    }
}

#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
static int
_file_fd_int_read(const char *path, int *retval)
{
  int fd = open(path, O_RDONLY);
  char buf[128];
  ssize_t sz;

  if (fd < 0) return -1;
  sz = pread(fd, buf, sizeof(buf) - 1, 0);
  if (sz > 0) *retval = atoi(buf);
  return fd;
}
#endif

static int
_init(Cpu_Perf *cp)
{
  char buf[4096];
  int i;
#if defined (__FreeBSD__) || defined(__DragonFly__)
  size_t len;
#else // Linux
  int fd;
#endif

  // open and keep open /proc/stat
#if defined (__FreeBSD__) || defined(__DragonFly__)
  if (sysctlbyname("kern.cp_times", buf, &len, NULL, 0) < 0) return -1;
#else // Linux
  cp->fd_proc_stat = open("/proc/stat", O_RDONLY);
  if (cp->fd_proc_stat < 0) return -1;
#endif

  cp->prev.num = -1;
  cp->cur.num = -1;

  cp->prev.counters = calloc(cp->cores, sizeof(*cp->prev.counters));
  cp->prev.freqinfo = calloc(cp->cores, sizeof(*cp->prev.freqinfo));
  cp->cur.counters = calloc(cp->cores, sizeof(*cp->cur.counters));
  cp->cur.freqinfo = calloc(cp->cores, sizeof(*cp->cur.freqinfo));
  _mem_alloc_result(cp);

  // init freq info before reading
#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
  for (i = 0; i < cp->cores; i++) cp->cur.freqinfo[i].fd_scaling_cur_freq = -1;
#endif

  // go over all possible cores and get frequency info
  for (i = 0; i < cp->cores; i++)
    {
#if defined (__FreeBSD__) || defined(__DragonFly__)
      char *p, *p2;
      int v;

      len = sizeof(buf) - 1;
      snprintf(buf, sizeof(buf), "dev.cpu.%i.freq_levels", i);
      if (sysctlbyname(buf, buf, &len, NULL, 0) < 0) break;
      buf[len] = 0;
      for (p = buf; *p; )
        { // "3500/84000 2800/50000 1500/20000 800/10000 350/4000"
          // "Mhz/mW Mhz/mW ..."
          p2 = strchr(p, '/');
          if (!p2) break;
          *p2 = 0; // nul terminate the buffer to be safe
          v = atoi(p) * 1000; // scale from Mhz to Khz
          if (cp->cur.freqinfo[i].min == 0) cp->cur.freqinfo[i].min = v;
          else if (v < cp->cur.freqinfo[i].min) cp->cur.freqinfo[i].min = v;
          if (cp->cur.freqinfo[i].max == 0) cp->cur.freqinfo[i].max = v;
          else if (v > cp->cur.freqinfo[i].max) cp->cur.freqinfo[i].max = v;
          p = p2 + 1;
          p = strchr(p, ' ');
          if (!p) break;
          p++;
        }
      snprintf(buf, sizeof(buf), "dev.cpu.%i.freq", i);
      if (sysctlbyname(buf, buf, &len, NULL, 0) < 0) break;
      cp->cur.freqinfo[i].cur = atoi(buf) * 1000;
#else // Linux
      snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/cpufreq/cpuinfo_min_freq", i);
      fd = _file_fd_int_read(buf, &(cp->cur.freqinfo[i].min));
      if (fd < 0) goto done;
      close(fd);
      snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/cpufreq/cpuinfo_max_freq", i);
      fd = _file_fd_int_read(buf, &(cp->cur.freqinfo[i].max));
      if (fd < 0) goto done;
      close(fd);
      snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_cur_freq", i);
      fd = open(buf, O_RDONLY);
done:
      cp->cur.freqinfo[i].fd_scaling_cur_freq = fd;
      if (fd < 0) break;
#endif
    }
#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
//  cp->cur.num = i;
#endif
  // dup into prev
  memcpy(cp->prev.freqinfo, cp->cur.freqinfo,
         cp->cores * sizeof(*cp->cur.freqinfo));
  return 0;
}

static void
_shutdown(Cpu_Perf *cp)
{
#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
  long i;

  // close /proc/styat fe
  if (cp->fd_proc_stat >= 0) close(cp->fd_proc_stat);
  cp->fd_proc_stat = -1;
  // close fds for cpufreq info
  for (i = 0; i < cp->prev.num; i++)
    {
      if (cp->prev.freqinfo[i].fd_scaling_cur_freq >= 0)
        close(cp->prev.freqinfo[i].fd_scaling_cur_freq);
    }
#endif
  free(cp->prev.counters);
  free(cp->prev.freqinfo);
  free(cp->cur.counters);
  free(cp->cur.freqinfo);
}

Cpu_Perf *
cpu_perf_add(void)
{
  Cpu_Perf *cp = calloc(1, sizeof(*cp));

  if (!cp) return NULL;
  // max cores here and 1 extra for global cpu %
  cp->cores = CPU_CORES_MAX + 1;
  if (_init(cp) == 0) return cp;
  free(cp);
  return NULL;
}

void
cpu_perf_del(Cpu_Perf *cp)
{
  _shutdown(cp);
  free(cp);
}

#if defined (__FreeBSD__) || defined(__DragonFly__)
#else // Linux
static int
_line_read(char **buf, char *line, ssize_t line_max)
{ // read a line until \n ansd nul terminate it i nt linux up to line_max
  char *next, *l;
  int ret = 0;

  l = line;
  for (next = *buf; *next; next++)
    {
      if (*next == '\n')
        {
          next++;
          goto done;
        }
      l[0] = *next;
      l[1] = 0; // null terminate
      l++;
      ret++;
      if (ret >= line_max) goto done;
    }
  if (!*next) ret = -1;
done:
  *buf = next;
  return ret;
}

static int
_word_read(char **buf,  char *word, ssize_t word_max)
{ // read a word from bnuf, put in word up to word_max and advance buf
  char *next, *w;
  int ret = 0;

  w = word;
  for (next = *buf; *next; next++)
    {
      if (!*next) goto done;
      if ((*next) && (*next == ' '))
        {
          do
            {
              next++;
            }
          while ((*next) && (*next == ' '));
          goto done;
        }
      w[0] = *next;
      w[1] = 0; // null terminate
      w++;
      ret++;
      if (ret >= word_max) goto done;
    }
  if (!*next) ret = -1;
done:
  *buf = next;
  return ret;
}

static void
_cpu_fields_read(char *buf, Cpu_Counters *c)
{ // read all cpu fields with spaces between until no more slots from buf
  char word[128];
  unsigned int field = 0;

  c->total = 0; // also sum up total so start at 0
  while (_word_read(&buf, word, sizeof(word) - 1) >= 0)
    {
      c->fields[field] = atoll(word);
      c->total += c->fields[field];
      field++;
      if (field >= (sizeof(c->fields) / sizeof(c->fields[0]))) break;
    }
}
#endif

static void
_alloc_info_again(Cpu_Perf *cp)
{ // reset everything internally as cpu code went up
  _shutdown(cp);
  cp->cores = CPU_CORES_MAX + 1;
  _init(cp);
}

void
cpu_perf_update(Cpu_Perf *cp)
{ // poll cpu usage + frequency info core by core
  char buf[16384];
#if defined (__FreeBSD__) || defined(__DragonFly__)
  long long buf2[8192];
#endif
  Cpu_Slice slice_tmp;
  long num = 0, i;
  long cpu = 0;

  // swap prev and cur so we can compare
  slice_tmp = cp->cur;
  cp->cur = cp->prev;
  cp->prev = slice_tmp;

#if defined (__FreeBSD__) || defined(__DragonFly__)
  Cpu_Counters *c;
  size_t len;

  // get all cpu counbters for all cores
  len = sizeof(buf2) - 1;
  if (sysctlbyname("kern.cp_times", buf2, &len, NULL, 0) >= 0)
    { // "1020 8910 8993 2147 9137 ..."
      // "cpu0user cpu0nice cpu0system cpu0intr cpu0idle cpu1user cpu1nice ..."
      int sys_cores = len / (5 * sizeof(long long));

      if (sys_cores > (cp->cores - 1))
        { // ran out of slots - realloc and try again! core count changed
          _alloc_info_again(cp);
          cpu_perf_update(cp);
          return;
        }
      while (cpu < (cp->cores - 1))
        {
          long long *times = (long long *)(buf2 + (cpu * 5));

          if (((cpu + 1) * 5 * sizeof(long long)) > len) break;
          c = &(cp->cur.counters[cpu + 1]);
          c->cpu = cpu;
          c->fields[CPU_USER] = times[0];
          c->fields[CPU_NICE] = times[1];
          c->fields[CPU_SYSTEM] = times[2];
          c->fields[CPU_IRQ] = times[3];
          c->fields[CPU_IDLE] = times[4];
          c->total = times[0] + times[1] + times[2] + times[3] + times[4];
          num = cpu + 2;
          cpu++;
        }
    }
#else // Linux
  char line[1024];
  char word[127];
  char *p, *l;
  ssize_t sz;

  // get all cpu counbters for all cores and more from /proc/stat
  sz = pread(cp->fd_proc_stat, buf, sizeof(buf) - 1, 0);
  if (sz <= 0) return;
  buf[sz] = 0; // nul terminate the buffer to be safe
  p = buf;
  // read cpu usage counters
  while (_line_read(&p, line, sizeof(line) - 1) >= 0)
    {
      l = line;
      if (_word_read(&l, word, sizeof(word) - 1) >= 0)
        {
          if (!strncmp(word, "cpu", 3))
            { // this line is a cpu counter line
              if (word[3] == 0)
                { // basic cpu counter for all
                  cp->cur.counters[0].cpu = -1;
                  _cpu_fields_read(l, &(cp->cur.counters[0]));
                  num = 1;
                }
              else
                { // core specific counter
                  cpu = atoi(word + 3); // numbers are cpu is core num

                  if ((cpu >= 0) && (cpu < (cp->cores - 1)))
                    {
                      cp->cur.counters[cpu + 1].cpu = cpu;
                      _cpu_fields_read(l, &(cp->cur.counters[cpu + 1]));
                      num = cpu + 2;
                    }
                  else if (cpu >= (cp->cores - 1))
                    { // ran out of slots - realloc and try again! core count changed
                      _alloc_info_again(cp);
                      cpu_perf_update(cp);
                      return;
                    }
                }
            }
        }
    }
#endif
  cp->cur.num = num;

  // read cpufreq info
  for (i = 0; i < num; i++)
    {
      cpu = cp->cur.counters[i].cpu;

      if (cpu >= 0)
        {
#if defined (__FreeBSD__) || defined(__DragonFly__)
          snprintf(buf, sizeof(buf), "dev.cpu.%li.freq", i);
          if (sysctlbyname(buf, buf, &len, NULL, 0) == 0)
            { // get freq of that core
              buf[len] =  0; // nul terminate the buffer to be safe
              cp->cur.freqinfo[cp->cur.counters[i].cpu].cur =
                atoi(buf) * 1000; // scale Mhz to Khz
            }
#else // Linux
          if (cp->cur.freqinfo[cpu].fd_scaling_cur_freq >= 0)
            { // get freq of that core
              sz = pread(cp->cur.freqinfo[cpu].fd_scaling_cur_freq,
                         buf, sizeof(buf) - 1, 0);
              if (sz > 0)
                {
                  buf[sz + 1] = 0; // nul terminate the buffer to be safe
                  cp->cur.freqinfo[cp->cur.counters[i].cpu].cur = atoi(buf);
                }
            }
#endif
        }
    }

  if (cp->cur.num != cp->prev.num)
    { // we dropped core num, so realloc down
      _shutdown(cp);
      cp->cores = cp->cur.num + 1;
      _init(cp);
      cp->cur.num = num;
    }
}
