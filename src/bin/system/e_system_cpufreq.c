#include "e_system.h"

#if defined __OpenBSD__
// no local funcs
#elif defined __FreeBSD__
// no local funcs
#else
static int
sys_cpu_setall(const char *control, const char *value)
{
   int num = 0;
   char buf[4096];
   FILE *f;

   for (;;)
     {
        snprintf(buf, sizeof(buf),
                 "/sys/devices/system/cpu/cpu%i/cpufreq/%s", num, control);
        f = fopen(buf, "w");
        if (!f) return num;
        fprintf(f, "%s", value);
        fclose(f);
        num++;
     }
   return 1;
}

static int
sys_cpu_pstate(int min, int max, int turbo)
{
   FILE *f;

   f = fopen("/sys/devices/system/cpu/intel_pstate/min_perf_pct", "w");
   if (!f) return 0;
   fprintf(f, "%i", min);
   fclose(f);

   f = fopen("/sys/devices/system/cpu/intel_pstate/max_perf_pct", "w");
   if (!f) return 0;
   fprintf(f, "%i", max);
   fclose(f);

   f = fopen("/sys/devices/system/cpu/intel_pstate/no_turbo", "w");
   if (!f) return 0;
   fprintf(f, "%i", turbo ? 0 : 1);
   fclose(f);

   return 1;
}

static int
map_strings(const char *file, const char **strs, int *vals)
{
  char  buf[1024], *p;
  FILE *f;
  int   i;

  f = fopen(file, "r");
  if (!f) return -1;
  if (!fgets(buf, sizeof(buf), f)) goto err;
  p = strchr(buf, '\n');
  if (p) *p = '\0';
  for (i = 0; strs[i]; i++)
    {
      if (!strcmp(strs[i], buf))
        {
          fclose(f);
          return vals[i];
        }
    }
err:
  fclose(f);
  return -1;
}

static int
get_int(const char *file)
{
  char  buf[1024], *p;
  FILE *f;
  int   v = -1;

  f = fopen(file, "r");
  if (!f) return -1;
  if (!fgets(buf, sizeof(buf), f)) goto err;
  p = strchr(buf, '\n');
  if (p) *p = '\0';
  v = atoi(buf);
err:
  fclose(f);
  return v;
}

static char *
find_preferred(char **instr, const char **prefstr, const char *defstr)
{
  int i, j;

  for (j = 0; prefstr[j]; j++)
    { // check pref list in order
      for (i = 0; instr[i]; i++)
        { // if that is there - then return it
          if (!strcmp(instr[i], prefstr[j])) return (char *)prefstr[j];
        }
    }
  // not found - default string
  return (char *)defstr;
}

static int
sys_cpu_pwr_energy_set(int v)
{
  char buf[1024];
  FILE *f;
  char **strs, *p;
  char *wrstr[4] = { NULL };
  const char *lv0[]
    = { "power", "balance_power", "balance_performance", "performance", NULL };
  const char *lv1[]
    = { "balance_power", "power", "balance_performance", "performance", NULL };
  const char *lv2[]
    = { "balance_performance", "balance_power", "performance", "power", NULL };
  const char *lv3[]
    = { "performance", "balance_performance", "balance_power", "power", NULL };

  // get avail prefs
  f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/"
            "energy_performance_available_preferences",
            "r");
  if (!f) return 0;
  if (!fgets(buf, sizeof(buf), f))
    {
      fclose(f);
      return 0;
    }
  fclose(f);
  p = strchr(buf, '\n');
  if (p) *p = '\0';
  strs = eina_str_split(buf, " ", 0);
  if ((!strs) || (!strs[0]))
    {
      free(strs);
      return 0;
    }

  wrstr[0] = find_preferred(strs, lv0, "default");
  wrstr[1] = find_preferred(strs, lv1, "default");
  wrstr[2] = find_preferred(strs, lv2, "default");
  wrstr[3] = find_preferred(strs, lv3, "default");

  sys_cpu_setall("energy_performance_preference", wrstr[v]);
  // if we're at max - allow boost
  if (v == 3) sys_cpu_setall("boost", "1");
  else sys_cpu_setall("boost", "0");

  free(strs[0]);
  free(strs);
  return 1;
}

static int
sys_cpu_pwr_pstate_set(int v)
{
  FILE *f;

  f = fopen("/sys/devices/system/cpu/intel_pstate/max_perf_pct", "r");
  if (!f) return 0;
  fclose(f);
  if (v == 0) sys_cpu_pstate(0, 0, 0);
  else if (v == 1) sys_cpu_pstate(0, 50, 0);
  else if (v == 2) sys_cpu_pstate(0, 75, 0);
  else if (v == 3) sys_cpu_pstate(0, 100, 1);
  return 1;
}

static int
sys_cpu_pwr_governor_set(int v)
{
  char buf[1024];
  FILE *f;
  char **strs, *p;
  char *wrstr[4] = { NULL };
  const char *lv0[]    = { "powersave",   "conservative", "ondemand",
                           "interactive", "performance",  NULL };
  const char *lv1[]    = { "conservative", "ondemand",    "interactive",
                           "powersave",    "performance", NULL };
  const char *lv2[]    = { "interactive",   "ondemand",  "conservative",
                           "performance", "powersave", NULL };
  const char *lv3[]    = { "performance",   "interactive",  "ondemand",
                           "conservative", "powersave", NULL };

  // get avail prefs
  f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors",
            "r");
  if (!f) return 0;
  if (!fgets(buf, sizeof(buf), f))
    {
      fclose(f);
      return 0;
    }
  fclose(f);
  p = strchr(buf, '\n');
  if (p) *p = '\0';
  strs = eina_str_split(buf, " ", 0);
  if ((!strs) || (!strs[0]))
    {
      free(strs);
      return 0;
    }
  wrstr[0] = find_preferred(strs, lv0, "performance");
  wrstr[1] = find_preferred(strs, lv1, "performance");
  wrstr[2] = find_preferred(strs, lv2, "performance");
  wrstr[3] = find_preferred(strs, lv3, "performance");

  sys_cpu_setall("scaling_governor", wrstr[v]);

  free(strs[0]);
  free(strs);
  return 1;
}
#endif

static void
_cb_cpufreq_pwr_set(void *data EINA_UNUSED, const char *params)
{
#if defined __OpenBSD__
  e_system_inout_command_send("cpufreq-pwr-set", "err");
#elif defined __FreeBSD__
  e_system_inout_command_send("cpufreq-pwr-set", "err");
#else
  int v = atoi(params); // 0->100

  if (v < 0) v = 0;
  else if (v > 100) v = 100;

  // translate 0 -> 3
  if (v < 33) v = 0;
  else if (v < 67) v = 1;
  else if (v < 100) v = 2;
  else v = 3;

  if (!sys_cpu_pwr_energy_set(v))
    {
      if (!sys_cpu_pwr_pstate_set(v))
        {
          if (!sys_cpu_pwr_governor_set(v)) goto err;
        }
    }
  e_system_inout_command_send("cpufreq-pwr-set", "ok");
  return;
err:
  e_system_inout_command_send("cpufreq-pwr-set", "err");
#endif
}

static void
_cb_cpufreq_pwr_get(void *data EINA_UNUSED, const char *params EINA_UNUSED)
{
#if defined __OpenBSD__
  e_system_inout_command_send("cpufreq-pwr-get", "err");
#elif defined __FreeBSD__
  e_system_inout_command_send("cpufreq-pwr-get", "err");
#else
  int v;
  const char *lv0[]
    = { "default",     "power", "balance_power", "balance_performance",
        "performance", NULL };
  int lv0vals[] = { 0, 0, 1, 2, 3 };
  const char *lv1[]
    = { "schedutil",     "userspace", "powersave", "conservative",
        "ondemand", "interactive", "performance", NULL };
  int lv1vals[] = { 0, 0, 0, 1, 1, 2, 3 };

  v = map_strings(
    "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", lv0,
    lv0vals);
  if (v < 0)
    {
      v = get_int("/sys/devices/system/cpu/intel_pstate/max_perf_pct");
      if (v < 0)
        {
          v = map_strings("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                          lv1, lv1vals);
          if (v < 0)
            {
              e_system_inout_command_send("cpufreq-pwr-get", "err");
              return;
            }
        }
    }

  // map 0->3 to 0->100
  if (v == 0) v = 0;
  else if (v == 1) v = 33;
  else if (v == 2) v = 67;
  else if (v == 3) v = 100;
  e_system_inout_command_send("cpufreq-pwr-get", "%i", v);
#endif
}

void
e_system_cpufreq_init(void)
{
  e_system_inout_command_register("cpufreq-pwr-set", _cb_cpufreq_pwr_set, NULL);
  e_system_inout_command_register("cpufreq-pwr-get", _cb_cpufreq_pwr_get, NULL);
}

void
e_system_cpufreq_shutdown(void)
{
   // only shutdown things we really have to - no need to free mem etc.
}
