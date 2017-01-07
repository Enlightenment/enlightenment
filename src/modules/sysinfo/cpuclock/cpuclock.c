#include "cpuclock.h"
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

typedef struct _Thread_Config Thread_Config;
struct _Thread_Config
{
   int interval;
   Instance *inst;
};

typedef struct _Pstate_Config Pstate_Config;
struct _Pstate_Config
{
   Instance *inst;
   int min;
   int max;
   int turbo;
};

static Cpu_Status *
_cpuclock_status_new(void)
{
   Cpu_Status *s;

   s = E_NEW(Cpu_Status, 1);
   if (!s) return NULL;
   s->active = -1;
   return s;
}

static void
_cpuclock_status_free(Cpu_Status *s)
{
   Eina_List *l;

   if (s->frequencies) eina_list_free(s->frequencies);
   if (s->governors)
     {
        for (l = s->governors; l; l = l->next)
          E_FREE_FUNC(l->data, free);
        eina_list_free(s->governors);
     }
   E_FREE_FUNC(s->cur_governor, free);
   if (s->orig_governor) eina_stringshare_del(s->orig_governor);
   E_FREE_FUNC(s, free);
}

static int
_cpuclock_cb_sort(const void *item1, const void *item2)
{
   int a, b;

   a = (long)item1;
   b = (long)item2;
   if (a < b) return -1;
   else if (a > b)
     return 1;
   return 0;
}

static void
_cpuclock_set_thread_governor(void *data, Ecore_Thread *th EINA_UNUSED)
{
   const char *governor = data;

   _cpuclock_sysfs_setall("scaling_governor", governor);
   if (!strcmp(governor, "ondemand"))
     _cpuclock_sysfs_set("ondemand/ignore_nice_load", "0");
   else if (!strcmp(governor, "conservative"))
     _cpuclock_sysfs_set("conservative/ignore_nice_load", "0");
}

static void
_cpuclock_set_thread_frequency(void *data, Ecore_Thread *th EINA_UNUSED)
{
   const char *freq = data;

#if defined __FreeBSD__ || defined __OpenBSD__
   _cpuclock_sysctl_frequency(freq);
   return;
#endif
   _cpuclock_sysfs_setall("scaling_setspeed", freq);
}

static void
_cpuclock_set_thread_pstate(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Pstate_Config *pc = data;

   _cpuclock_sysfs_pstate(pc->min, pc->max, pc->turbo);
}

static void
_cpuclock_set_thread_done(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED)
{
   return;
}

static void
_cpuclock_set_thread_pstate_done(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Pstate_Config *pc = data;

   E_FREE_FUNC(pc, free);

   return;
}

void
_cpuclock_set_governor(const char *governor)
{
#if defined __FreeBSD__ || defined __OpenBSD__
   return;
#endif

   ecore_thread_run(_cpuclock_set_thread_governor, _cpuclock_set_thread_done, NULL, governor);
}

static void
_cpuclock_set_frequency(int frequency)
{
   char buf[4096];
   const char *freq;

#ifdef __FreeBSD__
   frequency /= 1000;
#endif

   snprintf(buf, sizeof(buf), "%i", frequency);
   freq = eina_stringshare_add(buf);

   ecore_thread_run(_cpuclock_set_thread_frequency, _cpuclock_set_thread_done, NULL, freq);
}

void
_cpuclock_set_pstate(int min, int max, int turbo)
{
#if defined __FreeBSD__ || defined __OpenBSD__
   return;
#endif
   Pstate_Config *pc;

   pc = E_NEW(Pstate_Config, 1);
   if (!pc) return;

   pc->turbo = turbo;
   pc->min = min;
   pc->max = max;

   ecore_thread_run(_cpuclock_set_thread_pstate, _cpuclock_set_thread_pstate_done, NULL, pc);
}

static void
_cpuclock_face_cb_set_frequency(void *data, Evas_Object *obj EINA_UNUSED, const char *emission, const char *src EINA_UNUSED)
{
   Eina_List *l;
   int next_frequency = 0;
   Instance *inst = data;

   for (l = inst->cfg->cpuclock.status->frequencies; l; l = l->next)
     {
        if (inst->cfg->cpuclock.status->cur_frequency == (long)l->data)
          {
             if (!strcmp(emission, "e,action,frequency,increase"))
               {
                  if (l->next) next_frequency = (long)l->next->data;
                  break;
               }
             else if (!strcmp(emission, "e,action,frequency,decrease"))
               {
                  if (l->prev) next_frequency = (long)l->prev->data;
                  break;
               }
             else
               break;
          }
     }
   if (inst->cfg->cpuclock.status->can_set_frequency && next_frequency)
     _cpuclock_set_frequency(next_frequency);
}

static void
_cpuclock_face_cb_set_governor(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *src EINA_UNUSED)
{
   Eina_List *l;
   char *next_governor = NULL;
   Instance *inst = data;

   for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
     {
        if (!strcmp(l->data, inst->cfg->cpuclock.status->cur_governor))
          {
             if (l->next)
               next_governor = l->next->data;
             else
               next_governor = inst->cfg->cpuclock.status->governors->data;
             break;
          }
     }
   if (next_governor) _cpuclock_set_governor(next_governor);
}

static Eina_Bool
_cpuclock_event_cb_powersave(void *data, int type, void *event)
{
   Instance *inst = data;
   E_Event_Powersave_Update *ev;
   Eina_List *l;
   Eina_Bool has_powersave = EINA_FALSE;
   Eina_Bool has_conservative = EINA_FALSE;

   if (type != E_EVENT_POWERSAVE_UPDATE) return ECORE_CALLBACK_PASS_ON;
   if (!inst->cfg->cpuclock.auto_powersave) return ECORE_CALLBACK_PASS_ON;

   ev = event;
   if (!inst->cfg->cpuclock.status->orig_governor)
     inst->cfg->cpuclock.status->orig_governor = eina_stringshare_add(inst->cfg->cpuclock.status->cur_governor);

   for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
     {
        if (!strcmp(l->data, "conservative"))
          has_conservative = EINA_TRUE;
        else if (!strcmp(l->data, "powersave"))
          has_powersave = EINA_TRUE;
        else if (!strcmp(l->data, "interactive"))
          has_powersave = EINA_TRUE;
     }

   switch (ev->mode)
     {
      case E_POWERSAVE_MODE_NONE:
      case E_POWERSAVE_MODE_LOW:
        _cpuclock_set_governor(inst->cfg->cpuclock.status->orig_governor);
        eina_stringshare_del(inst->cfg->cpuclock.status->orig_governor);
        inst->cfg->cpuclock.status->orig_governor = NULL;
        break;

      case E_POWERSAVE_MODE_MEDIUM:
      case E_POWERSAVE_MODE_HIGH:
        if ((inst->cfg->cpuclock.powersave_governor) || (has_conservative))
          {
             if (inst->cfg->cpuclock.powersave_governor)
               _cpuclock_set_governor(inst->cfg->cpuclock.powersave_governor);
             else
               _cpuclock_set_governor("conservative");
             break;
          }

      case E_POWERSAVE_MODE_EXTREME:
        if (has_powersave)
          _cpuclock_set_governor("powersave");
        break;
     }

   return ECORE_CALLBACK_PASS_ON;
}

void
_cpuclock_config_updated(Instance *inst)
{
   Edje_Message_Int_Set *frequency_msg;
   Edje_Message_String_Set *governor_msg;
   Eina_List *l;
   int i;
   unsigned int count;

   if (inst->cfg->cpuclock.status->frequencies)
     {
        count = eina_list_count(inst->cfg->cpuclock.status->frequencies);
        frequency_msg = malloc(sizeof(Edje_Message_Int_Set) + (count - 1) * sizeof(int));
        EINA_SAFETY_ON_NULL_RETURN(frequency_msg);
        frequency_msg->count = count;
        for (l = inst->cfg->cpuclock.status->frequencies, i = 0; l; l = l->next, i++)
          frequency_msg->val[i] = (long)l->data;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_INT_SET, 1, frequency_msg);
        free(frequency_msg);
     }

   if (inst->cfg->cpuclock.status->governors)
     {
        count = eina_list_count(inst->cfg->cpuclock.status->governors);
        governor_msg = malloc(sizeof(Edje_Message_String_Set) + (count - 1) * sizeof(char *));
        governor_msg->count = count;
        for (l = inst->cfg->cpuclock.status->governors, i = 0; l; l = l->next, i++)
          governor_msg->str[i] = (char *)l->data;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_STRING_SET, 2, governor_msg);
        free(governor_msg);
     }
}

static void
_cpuclock_face_update_current(Instance *inst)
{
   Edje_Message_Int_Set *frequency_msg;
   Edje_Message_String governor_msg;

   frequency_msg = malloc(sizeof(Edje_Message_Int_Set) + (sizeof(int) * 4));
   EINA_SAFETY_ON_NULL_RETURN(frequency_msg);
   frequency_msg->count = 5;
   frequency_msg->val[0] = inst->cfg->cpuclock.status->cur_frequency;
   frequency_msg->val[1] = inst->cfg->cpuclock.status->can_set_frequency;
   frequency_msg->val[2] = inst->cfg->cpuclock.status->cur_min_frequency;
   frequency_msg->val[3] = inst->cfg->cpuclock.status->cur_max_frequency;
   frequency_msg->val[4] = 0; // pad
   edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_INT_SET, 3,
                            frequency_msg);
   free(frequency_msg);

   /* BSD crashes here without the if-condition
    * since it has no governors (yet) */
   if (inst->cfg->cpuclock.status->cur_governor)
     {
        governor_msg.str = inst->cfg->cpuclock.status->cur_governor;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_STRING, 4,
                                 &governor_msg);
     }
}

static void
_cpuclock_status_check_available(Cpu_Status *s)
{
   char buf[4096];
   Eina_List *l;
   // FIXME: this assumes all cores accept the same freqs/ might be wrong

#if defined (__OpenBSD__)
   int p;

   if (s->frequencies)
     {
        eina_list_free(s->frequencies);
        s->frequencies = NULL;
     }

   /* storing percents */
   p = 100;
   s->frequencies = eina_list_append(s->frequencies, (void *)p);
   p = 75;
   s->frequencies = eina_list_append(s->frequencies, (void *)p);
   p = 50;
   s->frequencies = eina_list_append(s->frequencies, (void *)p);
   p = 25;
   s->frequencies = eina_list_append(s->frequencies, (void *)p);
#elif defined (__FreeBSD__)
   int freq;
   size_t len = sizeof(buf);
   char *pos, *q;

   /* read freq_levels sysctl and store it in freq */
   if (sysctlbyname("dev.cpu.0.freq_levels", buf, &len, NULL, 0) == 0)
     {
        /* sysctl returns 0 on success */
        if (s->frequencies)
          {
             eina_list_free(s->frequencies);
             s->frequencies = NULL;
          }

        /* parse freqs and store the frequencies in s->frequencies */
        pos = buf;
        while (pos)
          {
             q = strchr(pos, '/');
             if (!q) break;

             *q = '\0';
             freq = atoi(pos);
             freq *= 1000;
             s->frequencies = eina_list_append(s->frequencies, (void *)freq);

             pos = q + 1;
             pos = strchr(pos, ' ');
          }
     }

   /* sort is not necessary because freq_levels is already sorted */
   /* freebsd doesn't have governors */
   if (s->governors)
     {
        for (l = s->governors; l; l = l->next)
          free(l->data);
        eina_list_free(s->governors);
        s->governors = NULL;
     }
#else
   FILE *f;

   f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies", "r");
   if (f)
     {
        char *freq;

        if (s->frequencies)
          {
             eina_list_free(s->frequencies);
             s->frequencies = NULL;
          }

        if (fgets(buf, sizeof(buf), f) == NULL)
          {
             fclose(f);
             return;
          }
        fclose(f);

        freq = strtok(buf, " ");
        do
          {
             if (atoi(freq) != 0)
               {
                  s->frequencies = eina_list_append(s->frequencies,
                                                    (void *)(long)atoi(freq));
               }
             freq = strtok(NULL, " ");
          }
        while (freq);

        s->frequencies = eina_list_sort(s->frequencies,
                                        eina_list_count(s->frequencies),
                                        _cpuclock_cb_sort);
     }
   else
     do
       {
#define CPUFREQ_SYSFSDIR "/sys/devices/system/cpu/cpu0/cpufreq"
          f = fopen(CPUFREQ_SYSFSDIR "/scaling_cur_freq", "r");
          if (!f) break;
          fclose(f);

          f = fopen(CPUFREQ_SYSFSDIR "/scaling_driver", "r");
          if (!f) break;
          if (fgets(buf, sizeof(buf), f) == NULL)
            {
               fclose(f);
               break;
            }
          fclose(f);
          if (strcmp(buf, "intel_pstate\n")) break;

          if (s->frequencies)
            {
               eina_list_free(s->frequencies);
               s->frequencies = NULL;
            }
#define CPUFREQ_ADDF(filename) \
          f = fopen(CPUFREQ_SYSFSDIR filename, "r"); \
          if (f) \
            { \
               if (fgets(buf, sizeof(buf), f) != NULL) \
                 s->frequencies = eina_list_append(s->frequencies, \
                                                   (void *)(long)(atoi(buf))); \
               fclose(f); \
            }
          CPUFREQ_ADDF("/cpuinfo_min_freq");
          CPUFREQ_ADDF("/cpuinfo_max_freq");
       }
     while (0);

   f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors", "r");
   if (f)
     {
        char *gov;
        int len;

        if (s->governors)
          {
             for (l = s->governors; l; l = l->next)
               free(l->data);
             eina_list_free(s->governors);
             s->governors = NULL;
          }

        if (fgets(buf, sizeof(buf), f) == NULL)
          {
             fclose(f);
             return;
          }
        fclose(f);
        len = strlen(buf);
        if (len > 0)
          {
             gov = buf + len - 1;
             while ((gov > buf) && (isspace(*gov)))
               {
                  *gov = 0;
                  gov--;
               }
          }
        gov = strtok(buf, " ");
        do
          {
             while ((*gov) && (isspace(*gov)))
               gov++;
             if (strlen(gov) != 0)
               s->governors = eina_list_append(s->governors, strdup(gov));
             gov = strtok(NULL, " ");
          }
        while (gov);

        s->governors =
          eina_list_sort(s->governors, eina_list_count(s->governors),
                         (int (*)(const void *, const void *))strcmp);
     }
#endif
}

static int
_cpuclock_status_check_current(Cpu_Status *s)
{
   int ret = 0;
   int frequency = 0;

#if defined (__OpenBSD__)
   size_t len = sizeof(frequency);
   int percent, mib[] = {CTL_HW, HW_CPUSPEED};
   s->active = 0;

   _cpuclock_status_check_available(s);

   if (sysctl(mib, 2, &frequency, &len, NULL, 0) == 0)
     {
        frequency *= 1000;
        if (frequency != s->cur_frequency) ret = 1;
        s->cur_frequency = frequency;
        s->active = 1;
     }

   mib[1] = HW_SETPERF;

   if (sysctl(mib, 2, &percent, &len, NULL, 0) == 0)
     {
        s->cur_percent = percent;
     }

   s->can_set_frequency = 1;
   s->cur_governor = NULL;

#elif defined (__FreeBSD__)
   size_t len = sizeof(frequency);
   s->active = 0;

   /* frequency is stored in dev.cpu.0.freq */
   if (sysctlbyname("dev.cpu.0.freq", &frequency, &len, NULL, 0) == 0)
     {
        frequency *= 1000;
        if (frequency != s->cur_frequency) ret = 1;
        s->cur_frequency = frequency;
        s->active = 1;
     }

   /* hardcoded for testing */
   s->can_set_frequency = 1;
   s->cur_governor = NULL;

#else
   char buf[4096];
   FILE *f;
   int frequency_min = 0x7fffffff;
   int frequency_max = 0;
   int freqtot = 0;
   int i;

   s->active = 0;

   _cpuclock_status_check_available(s);
   // average out frequencies of all cores
   for (i = 0; i < 64; i++)
     {
        snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_cur_freq", i);
        f = fopen(buf, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL)
               {
                  fclose(f);
                  continue;
               }
             fclose(f);

             frequency = atoi(buf);
             if (frequency > frequency_max) frequency_max = frequency;
             if (frequency < frequency_min) frequency_min = frequency;
             freqtot += frequency;
             s->active = 1;
          }
        else
          break;
     }
   if (i < 1) i = 1;
   frequency = freqtot / i;
   if (frequency != s->cur_frequency) ret = 1;
   if (frequency_min != s->cur_min_frequency) ret = 1;
   if (frequency_max != s->cur_max_frequency) ret = 1;
   s->cur_frequency = frequency;
   s->cur_min_frequency = frequency_min;
   s->cur_max_frequency = frequency_max;

//  printf("%i | %i %i\n", frequency, frequency_min, frequency_max);

   // FIXME: this assumes all cores are on the same governor
   f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", "r");
   if (f)
     {
        s->can_set_frequency = 1;
        fclose(f);
     }
   else
     {
        s->can_set_frequency = 0;
     }

   f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
   if (f)
     {
        char *p;

        if (fgets(buf, sizeof(buf), f) == NULL)
          {
             fclose(f);
             return ret;
          }
        fclose(f);

        for (p = buf; (*p != 0) && (isalnum(*p)); p++) ;
        *p = 0;

        if ((!s->cur_governor) || (strcmp(buf, s->cur_governor)))
          {
             ret = 1;

             free(s->cur_governor);
             s->cur_governor = strdup(buf);

             for (i = strlen(s->cur_governor) - 1; i >= 0; i--)
               {
                  if (isspace(s->cur_governor[i]))
                    s->cur_governor[i] = 0;
                  else
                    break;
               }
          }
     }
   f = fopen("/sys/devices/system/cpu/intel_pstate/min_perf_pct", "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f) != NULL)
          {
             s->pstate_min = atoi(buf);
             s->pstate = 1;
          }
        fclose(f);
     }
   f = fopen("/sys/devices/system/cpu/intel_pstate/max_perf_pct", "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f) != NULL)
          {
             s->pstate_max = atoi(buf);
             s->pstate = 1;
          }
        fclose(f);
     }
   f = fopen("/sys/devices/system/cpu/intel_pstate/no_turbo", "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f) != NULL)
          {
             s->pstate_turbo = atoi(buf);
             if (s->pstate_turbo) s->pstate_turbo = 0;
             else s->pstate_turbo = 1;
             s->pstate = 1;
          }
        fclose(f);
     }
#endif
   return ret;
}

static void
_cpuclock_cb_frequency_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        Cpu_Status *status;

        if (ecore_thread_check(th)) break;
        status = _cpuclock_status_new();
        if (_cpuclock_status_check_current(status))
          ecore_thread_feedback(th, status);
        else
          _cpuclock_status_free(status);
        if (ecore_thread_check(th)) break;
        usleep((1000000.0 / 8.0) * (double)thc->interval);
     }
   E_FREE_FUNC(thc, free);
}

static void
_cpuclock_cb_frequency_check_notify(void *data,
                                   Ecore_Thread *th EINA_UNUSED,
                                   void *msg)
{
   Cpu_Status *status = msg;
   Eina_Bool freq_changed = EINA_FALSE;
   Thread_Config *thc = data;
   Instance *inst = thc->inst;

   if (inst->cfg->esm != E_SYSINFO_MODULE_CPUCLOCK && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;
   if (!inst->cfg) return;
   if ((inst->cfg->cpuclock.status) && (status) &&
       (
#ifdef __OpenBSD__
   (status->cur_percent       != inst->cfg->cpuclock.status->cur_percent      ) ||
#endif
   (status->cur_frequency     != inst->cfg->cpuclock.status->cur_frequency    ) ||
   (status->cur_min_frequency != inst->cfg->cpuclock.status->cur_min_frequency) ||
   (status->cur_max_frequency != inst->cfg->cpuclock.status->cur_max_frequency) ||
   (status->can_set_frequency != inst->cfg->cpuclock.status->can_set_frequency)))
     freq_changed = EINA_TRUE;
   _cpuclock_status_free(inst->cfg->cpuclock.status);
   inst->cfg->cpuclock.status = status;
   if (freq_changed)
     {
        _cpuclock_face_update_current(inst);
     }
   if (inst->cfg->cpuclock.status->active == 0)
     elm_layout_signal_emit(inst->cfg->cpuclock.o_gadget, "e,state,disabled", "e");
   else if (inst->cfg->cpuclock.status->active == 1)
     elm_layout_signal_emit(inst->cfg->cpuclock.o_gadget, "e,state,enabled", "e");

   _cpuclock_set_pstate(inst->cfg->cpuclock.pstate_min - 1,
                  inst->cfg->cpuclock.pstate_max - 1, inst->cfg->cpuclock.status->pstate_turbo);
}

void
_cpuclock_poll_interval_update(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->interval = inst->cfg->cpuclock.poll_interval;
        inst->cfg->cpuclock.frequency_check_thread =
          ecore_thread_feedback_run(_cpuclock_cb_frequency_check_main,
                                    _cpuclock_cb_frequency_check_notify,
                                    NULL, NULL, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_cpuclock_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   if (inst->cfg->cpuclock.handler)
     ecore_event_handler_del(inst->cfg->cpuclock.handler);

   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
     }

    if (inst->cfg->cpuclock.governor)
     eina_stringshare_del(inst->cfg->cpuclock.governor);
   if (inst->cfg->cpuclock.status) _cpuclock_status_free(inst->cfg->cpuclock.status);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_cpuclock_remove(Instance *inst)
{
   if (inst->cfg->cpuclock.handler)
     ecore_event_handler_del(inst->cfg->cpuclock.handler);

   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
     }

    if (inst->cfg->cpuclock.governor)
     eina_stringshare_del(inst->cfg->cpuclock.governor);
   if (inst->cfg->cpuclock.status) _cpuclock_status_free(inst->cfg->cpuclock.status);
}

static void
_cpuclock_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->cfg->cpuclock.pstate_min == 0) inst->cfg->cpuclock.pstate_min = 1;
   if (inst->cfg->cpuclock.pstate_max == 0) inst->cfg->cpuclock.pstate_max = 101;

   inst->cfg->cpuclock.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->cpuclock.o_gadget, "base/theme/modules/cpufreq",
                           "e/modules/cpufreq/main");
   E_EXPAND(inst->cfg->cpuclock.o_gadget);
   E_FILL(inst->cfg->cpuclock.o_gadget);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,governor,next", "*",
                                   _cpuclock_face_cb_set_governor, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,increase", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,decrease", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   elm_box_pack_end(inst->o_main, inst->cfg->cpuclock.o_gadget);
   evas_object_show(inst->cfg->cpuclock.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _cpuclock_created_cb, data);
   inst->cfg->cpuclock.status = _cpuclock_status_new();
   _cpuclock_status_check_available(inst->cfg->cpuclock.status);
   _cpuclock_poll_interval_update(inst);
   inst->cfg->cpuclock.handler = ecore_event_handler_add(E_EVENT_POWERSAVE_UPDATE, 
                                               _cpuclock_event_cb_powersave, inst);
   _cpuclock_config_updated(inst);
}

Evas_Object *
sysinfo_cpuclock_create(Evas_Object *parent, Instance *inst)
{
   if (inst->cfg->cpuclock.pstate_min == 0) inst->cfg->cpuclock.pstate_min = 1;
   if (inst->cfg->cpuclock.pstate_max == 0) inst->cfg->cpuclock.pstate_max = 101;

   inst->cfg->cpuclock.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->cpuclock.o_gadget, "base/theme/modules/cpufreq",
                           "e/modules/cpufreq/main");
   E_EXPAND(inst->cfg->cpuclock.o_gadget);
   E_FILL(inst->cfg->cpuclock.o_gadget);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,governor,next", "*",
                                   _cpuclock_face_cb_set_governor, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,increase", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,decrease", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   evas_object_show(inst->cfg->cpuclock.o_gadget);
   inst->cfg->cpuclock.status = _cpuclock_status_new();
   _cpuclock_status_check_available(inst->cfg->cpuclock.status);
   _cpuclock_poll_interval_update(inst);
   inst->cfg->cpuclock.handler = ecore_event_handler_add(E_EVENT_POWERSAVE_UPDATE,
                                               _cpuclock_event_cb_powersave, inst);
   _cpuclock_config_updated(inst);

   return inst->cfg->cpuclock.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_CPUCLOCK) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_CPUCLOCK;
   ci->cpuclock.poll_interval = 32;
   ci->cpuclock.restore_governor = 0;
   ci->cpuclock.auto_powersave = 1;
   ci->cpuclock.powersave_governor = NULL;
   ci->cpuclock.governor = NULL;
   ci->cpuclock.pstate_min = 1;
   ci->cpuclock.pstate_max = 101;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
cpuclock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_smart_callback_add(parent, "gadget_created", _cpuclock_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _cpuclock_removed_cb, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}
