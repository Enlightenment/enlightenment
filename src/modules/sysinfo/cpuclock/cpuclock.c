#include "cpuclock.h"
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
 #include <sys/types.h>
 #include <sys/sysctl.h>
#endif

EINTERN void _cpuclock_poll_interval_update(Instance *inst);

typedef struct _Thread_Config Thread_Config;
struct _Thread_Config
{
   int                  interval;
   Instance            *inst;
   E_Powersave_Sleeper *sleeper;
};

typedef struct _Pstate_Config Pstate_Config;
struct _Pstate_Config
{
   Instance *inst;
   int       min;
   int       max;
   int       turbo;
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

   if (eina_list_count(s->frequencies)) eina_list_free(s->frequencies);
   if (s->governors)
     {
        for (l = s->governors; l; l = l->next)
          E_FREE(l->data);
        eina_list_free(s->governors);
     }
   E_FREE(s->cur_governor);
   if (s->orig_governor) eina_stringshare_del(s->orig_governor);
   E_FREE(s);
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

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
static void
_cpuclock_set_thread_frequency(void *data, Ecore_Thread *th EINA_UNUSED)
{
   const char *freq = data;
   int frequency = atoi(freq);
   _cpuclock_sysctl_frequency(frequency);
}

static void
_cpuclock_set_thread_done(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED)
{
   return;
}

#endif

void
_cpuclock_set_governor(const char *governor)
{
#if defined(__FreeBSD__) || defined(__OpenBSD__)
   return;
#endif
   char buf[4096], exe[4096];
   struct stat st;

   snprintf(exe, 4096, "%s/%s/cpuclock_sysfs",
            e_module_dir_get(sysinfo_config->module), MODULE_ARCH);
   if (stat(exe, &st) < 0) return;

   snprintf(buf, sizeof(buf),
            "%s %s %s", exe, "governor", governor);
   system(buf);
}

void
_cpuclock_set_frequency(int frequency)
{
   char buf[4096];

#if defined(__FreeBSD__)
   frequency /= 1000;
#endif

   snprintf(buf, sizeof(buf), "%i", frequency);
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
   const char *freq;
   freq = eina_stringshare_add(buf);
   ecore_thread_run(_cpuclock_set_thread_frequency, _cpuclock_set_thread_done, NULL, freq);
#else
   struct stat st;
   char exe[4096];
   snprintf(exe, 4096, "%s/%s/cpuclock_sysfs",
            e_module_dir_get(sysinfo_config->module), MODULE_ARCH);
   if (stat(exe, &st) < 0) return;
   snprintf(buf, sizeof(buf),
            "%s %s %i", exe, "frequency", frequency);
   system(buf);
#endif
}

void
_cpuclock_set_pstate(int min, int max, int turbo)
{
#if defined(__FreeBSD__) || defined(__OpenBSD__)
   return;
#endif
   char buf[4096], exe[4096];
   struct stat st;

   snprintf(exe, 4096, "%s/%s/cpuclock_sysfs",
            e_module_dir_get(sysinfo_config->module), MODULE_ARCH);
   if (stat(exe, &st) < 0) return;
   snprintf(buf, sizeof(buf),
            "%s %s %i %i %i", exe, "pstate", min, max, turbo);
   system(buf);
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
   if (inst->cfg->id == -1) return ECORE_CALLBACK_RENEW;

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
        EINA_FALLTHROUGH;
      /* no break */

      case E_POWERSAVE_MODE_EXTREME:
      default:
        if (has_powersave)
          _cpuclock_set_governor("powersave");
        break;
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Evas_Object *
_cpuclock_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->cpuclock.popup) return NULL;
   return cpuclock_configure(inst);
}

static void
_cpuclock_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);

   inst->cfg->cpuclock.popup = NULL;
   inst->cfg->cpuclock.popup_pbar = NULL;
}

static void
_cpuclock_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->cpuclock.popup = NULL;
}

static Evas_Object *
_cpuclock_popup_create(Instance *inst)
{
   Evas_Object *popup, *table, *label, *pbar;
   double f = inst->cfg->cpuclock.status->cur_frequency;
   char buf[4096], text[4096], *u;

   if (f < 1000000)
     {
        f += 500;
        f /= 1000;
        u = _("MHz");
     }
   else
     {
        f += 50000;
        f /= 1000000;
        u = _("GHz");
     }

   popup = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(popup, "noblock");
   evas_object_smart_callback_add(popup, "dismissed",
                                  _cpuclock_popup_dismissed, inst);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL,
                                  _cpuclock_popup_deleted, inst);

   table = elm_table_add(popup);
   E_EXPAND(table);
   E_FILL(table);
   elm_object_content_set(popup, table);
   evas_object_show(table);

   snprintf(text, sizeof(text), "<big><b>%s</b></big>", _("Frequency"));

   label = elm_label_add(table);
   E_EXPAND(label);
   E_ALIGN(label, 0.5, 0.5);
   elm_object_text_set(label, text);
   elm_table_pack(table, label, 0, 0, 2, 1);
   evas_object_show(label);    

   snprintf(buf, sizeof(buf), "%1.1f %s (%d %%)", f, u,
                 inst->cfg->cpuclock.percent);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_progressbar_value_set(pbar, (float)inst->cfg->cpuclock.percent / 100);
   elm_progressbar_unit_format_set(pbar, buf);
   elm_table_pack(table, pbar, 0, 1, 2, 1);
   evas_object_show(pbar);
   inst->cfg->cpuclock.popup_pbar = pbar;

   e_gadget_util_ctxpopup_place(inst->o_main, popup,
                                inst->cfg->cpuclock.o_gadget);
   evas_object_show(popup);

   return popup;
}

static void
_cpuclock_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->cpuclock.popup)
          elm_ctxpopup_dismiss(inst->cfg->cpuclock.popup);
        else
          inst->cfg->cpuclock.popup = _cpuclock_popup_create(inst);
     }
   else
     {
        if (inst->cfg->cpuclock.popup)
          elm_ctxpopup_dismiss(inst->cfg->cpuclock.popup);
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_CPUCLOCK)
          cpuclock_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

void
_cpuclock_config_updated(Instance *inst)
{
   Edje_Message_Int_Set *frequency_msg;
   Edje_Message_String_Set *governor_msg;
   Eina_List *l;
   int i;
   unsigned int count;

   if (inst->cfg->id == -1)
     {
        inst->cfg->cpuclock.status->cur_frequency = 3.0;
        inst->cfg->cpuclock.status->can_set_frequency = 1;
        inst->cfg->cpuclock.status->cur_min_frequency = 0.5;
        inst->cfg->cpuclock.status->cur_max_frequency = 3.5;
        return;
     }
   if (inst->cfg->cpuclock.status->frequencies)
     {
        count = eina_list_count(inst->cfg->cpuclock.status->frequencies);
        frequency_msg = malloc(sizeof(Edje_Message_Int_Set) + (count - 1) * sizeof(int));
        EINA_SAFETY_ON_NULL_RETURN(frequency_msg);
        frequency_msg->count = count;
        for (l = inst->cfg->cpuclock.status->frequencies, i = 0; l; l = l->next, i++)
          frequency_msg->val[i] = (long)l->data;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_INT_SET, 1, frequency_msg);
        E_FREE(frequency_msg);
     }

   if (inst->cfg->cpuclock.status->governors)
     {
        count = eina_list_count(inst->cfg->cpuclock.status->governors);
        governor_msg = malloc(sizeof(Edje_Message_String_Set) + (count - 1) * sizeof(char *));
        governor_msg->count = count;
        for (l = inst->cfg->cpuclock.status->governors, i = 0; l; l = l->next, i++)
          governor_msg->str[i] = (char *)l->data;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_STRING_SET, 2, governor_msg);
        E_FREE(governor_msg);
     }
   _cpuclock_poll_interval_update(inst);
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
   E_FREE(frequency_msg);
   if (inst->cfg->cpuclock.tot_min_frequency == 0)
     inst->cfg->cpuclock.tot_min_frequency = inst->cfg->cpuclock.status->cur_frequency;
   if (inst->cfg->cpuclock.status->cur_frequency >
       inst->cfg->cpuclock.tot_max_frequency)
     {
        inst->cfg->cpuclock.tot_max_frequency = inst->cfg->cpuclock.status->cur_frequency;
        inst->cfg->cpuclock.percent = 100;
     }
   if (inst->cfg->cpuclock.status->cur_frequency <
            inst->cfg->cpuclock.tot_min_frequency)
     {
        inst->cfg->cpuclock.tot_min_frequency = inst->cfg->cpuclock.status->cur_frequency;
        inst->cfg->cpuclock.percent = 0;
     }
   if ((inst->cfg->cpuclock.tot_min_frequency > 0) &&
       (inst->cfg->cpuclock.tot_max_frequency >=
        inst->cfg->cpuclock.tot_min_frequency))
     {
        inst->cfg->cpuclock.percent = ((double)(inst->cfg->cpuclock.status->cur_frequency -
                                                inst->cfg->cpuclock.tot_min_frequency) /
                                       (double)(inst->cfg->cpuclock.tot_max_frequency -
                                                inst->cfg->cpuclock.tot_min_frequency)) * 100;
     }
   else
     inst->cfg->cpuclock.percent = 0;
   /* BSD crashes here without the if-condition
    * since it has no governors (yet) */
   if (inst->cfg->cpuclock.status->cur_governor)
     {
        governor_msg.str = inst->cfg->cpuclock.status->cur_governor;
        edje_object_message_send(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), EDJE_MESSAGE_STRING, 4,
                                 &governor_msg);
     }

   if (inst->cfg->cpuclock.popup)
     {
        double f = inst->cfg->cpuclock.status->cur_frequency;
        char buf[4096], *u;

        if (f < 1000000)
          {
             f += 500;
             f /= 1000;
             u = _("MHz");
          }
        else
          {
             f += 50000;
             f /= 1000000;
             u = _("GHz");
          }
        snprintf(buf, sizeof(buf), "%1.1f %s (%d %%)", f, u,
                 inst->cfg->cpuclock.percent);
        elm_progressbar_unit_format_set(inst->cfg->cpuclock.popup_pbar, buf);
        elm_progressbar_value_set(inst->cfg->cpuclock.popup_pbar,
                                  (float)inst->cfg->cpuclock.percent / 100);
     }
}

static void
_cpuclock_status_check_available(Cpu_Status *s)
{
#if !defined(__OpenBSD__)
   char buf[4096];
   Eina_List *l;
#endif
   // FIXME: this assumes all cores accept the same freqs/ might be wrong

#if defined(__OpenBSD__)
   int p;

   if (s->frequencies)
     {
        eina_list_free(s->frequencies);
        s->frequencies = NULL;
     }

   /* storing percents */
   p = 100;
   s->frequencies = eina_list_append(s->frequencies, (void *)(long int)p);
   p = 75;
   s->frequencies = eina_list_append(s->frequencies, (void *)(long int)p);
   p = 50;
   s->frequencies = eina_list_append(s->frequencies, (void *)(long int)p);
   p = 25;
   s->frequencies = eina_list_append(s->frequencies, (void *)(long int)p);
#elif defined(__FreeBSD__)
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
             s->frequencies = eina_list_append(s->frequencies, (void *)(long)freq);

             pos = q + 1;
             pos = strchr(pos, ' ');
          }
     }

   /* sort is not necessary because freq_levels is already sorted */
   /* freebsd doesn't have governors */
   if (s->governors)
     {
        for (l = s->governors; l; l = l->next)
          E_FREE(l->data);
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
#define CPUFREQ_ADDF(filename)                                          \
  f = fopen(CPUFREQ_SYSFSDIR filename, "r");                            \
  if (f)                                                                \
    {                                                                   \
       if (fgets(buf, sizeof(buf), f) != NULL)                          \
         s->frequencies = eina_list_append(s->frequencies,              \
                                           (void *)(long)(atoi(buf)));  \
       fclose(f);                                                       \
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
               E_FREE(l->data);
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

#if defined(__OpenBSD__)
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

#elif defined(__FreeBSD__)
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

             E_FREE(s->cur_governor);
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
_cpuclock_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), 0, 0, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (inst->cfg->esm == E_SYSINFO_MODULE_CPUCLOCK)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->cpuclock.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_cpuclock_cb_frequency_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;; )
     {
        Cpu_Status *status;

        if (ecore_thread_check(th)) break;
        status = _cpuclock_status_new();
        if (_cpuclock_status_check_current(status))
          ecore_thread_feedback(th, status);
        else
          _cpuclock_status_free(status);
        if (ecore_thread_check(th)) break;
        e_powersave_sleeper_sleep(thc->sleeper, thc->interval);
        if (ecore_thread_check(th)) break;
     }
}

static void
_cpuclock_cb_frequency_check_notify(void *data,
                                    Ecore_Thread *th EINA_UNUSED,
                                    void *msg)
{
   Cpu_Status *status = msg;
   Eina_Bool freq_changed = EINA_FALSE;
   Eina_Bool init_set = EINA_FALSE;
   Thread_Config *thc = data;

   if (!thc->inst) return;
   if (!thc->inst->cfg) return;
   if (thc->inst->cfg->esm != E_SYSINFO_MODULE_CPUCLOCK && thc->inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   if ((thc->inst->cfg->cpuclock.status) && (status) &&
       (
#if defined(__OpenBSD__)
         (status->cur_percent != thc->inst->cfg->cpuclock.status->cur_percent) ||
#endif
         (status->cur_frequency != thc->inst->cfg->cpuclock.status->cur_frequency) ||
         (status->cur_min_frequency != thc->inst->cfg->cpuclock.status->cur_min_frequency) ||
         (status->cur_max_frequency != thc->inst->cfg->cpuclock.status->cur_max_frequency) ||
         (status->can_set_frequency != thc->inst->cfg->cpuclock.status->can_set_frequency)))
     freq_changed = EINA_TRUE;
   E_FREE_FUNC(thc->inst->cfg->cpuclock.status, _cpuclock_status_free);
   thc->inst->cfg->cpuclock.status = status;
   if (freq_changed)
     {
        _cpuclock_face_update_current(thc->inst);
     }
   if (thc->inst->cfg->cpuclock.status->active == 0)
     elm_layout_signal_emit(thc->inst->cfg->cpuclock.o_gadget, "e,state,disabled", "e");
   else if (thc->inst->cfg->cpuclock.status->active == 1)
     elm_layout_signal_emit(thc->inst->cfg->cpuclock.o_gadget, "e,state,enabled", "e");

   if (!init_set)
     {
        _cpuclock_set_pstate(thc->inst->cfg->cpuclock.pstate_min - 1,
                             thc->inst->cfg->cpuclock.pstate_max - 1,
                             thc->inst->cfg->cpuclock.status->pstate_turbo);
        init_set = EINA_TRUE;
     }
}

static void
_cpuclock_cb_frequency_check_end(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Thread_Config *thc = data;

   e_powersave_sleeper_free(thc->sleeper);
   E_FREE(thc);
}

EINTERN void
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
        thc->sleeper = e_powersave_sleeper_new();
        thc->interval = inst->cfg->cpuclock.poll_interval;
        inst->cfg->cpuclock.frequency_check_thread =
          ecore_thread_feedback_run(_cpuclock_cb_frequency_check_main,
                                    _cpuclock_cb_frequency_check_notify,
                                    _cpuclock_cb_frequency_check_end,
                                    _cpuclock_cb_frequency_check_end, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static Eina_Bool
_screensaver_on(void *data)
{
   Instance *inst = data;

   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_screensaver_off(void *data)
{
   Instance *inst = data;

   _cpuclock_config_updated(inst);

   return ECORE_CALLBACK_RENEW;
}

static void
_cpuclock_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->o_main != event_data) return;

   if (inst->cfg->cpuclock.popup_pbar)
     E_FREE_FUNC(inst->cfg->cpuclock.popup_pbar, evas_object_del);
   if (inst->cfg->cpuclock.popup)
     E_FREE_FUNC(inst->cfg->cpuclock.popup, evas_object_del);
   if (inst->cfg->cpuclock.configure)
     E_FREE_FUNC(inst->cfg->cpuclock.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->cpuclock.handlers, handler)
     ecore_event_handler_del(handler);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_cpuclock_remove, data);
   evas_object_smart_callback_del_full(e_gadget_site_get(inst->o_main), "gadget_removed",
                                       _cpuclock_removed_cb, inst);
   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
        return;
     }
   if (inst->cfg->cpuclock.governor)
     eina_stringshare_del(inst->cfg->cpuclock.governor);
   E_FREE_FUNC(inst->cfg->cpuclock.status, _cpuclock_status_free);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   if (inst->cfg->id >= 0)
     sysinfo_instances = eina_list_remove(sysinfo_instances, inst);
   E_FREE(inst->cfg);
   E_FREE(inst);
}

void
sysinfo_cpuclock_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->cfg->cpuclock.popup_pbar)
     E_FREE_FUNC(inst->cfg->cpuclock.popup_pbar, evas_object_del);
   if (inst->cfg->cpuclock.popup)
     E_FREE_FUNC(inst->cfg->cpuclock.popup, evas_object_del);
   if (inst->cfg->cpuclock.configure)
     E_FREE_FUNC(inst->cfg->cpuclock.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->cpuclock.handlers, handler)
     ecore_event_handler_del(handler);
   if (inst->cfg->cpuclock.frequency_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpuclock.frequency_check_thread);
        inst->cfg->cpuclock.frequency_check_thread = NULL;
     }
   if (inst->cfg->cpuclock.governor)
     eina_stringshare_del(inst->cfg->cpuclock.governor);
   E_FREE_FUNC(inst->cfg->cpuclock.status, _cpuclock_status_free);
}

static void
_cpuclock_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));
   Eina_List *l = NULL;

   e_gadget_configure_cb_set(inst->o_main, _cpuclock_configure_cb);

   if (inst->cfg->cpuclock.pstate_min == 0) inst->cfg->cpuclock.pstate_min = 1;
   if (inst->cfg->cpuclock.pstate_max == 0) inst->cfg->cpuclock.pstate_max = 101;
   inst->cfg->cpuclock.percent = 0;
   inst->cfg->cpuclock.tot_min_frequency = 0;
   inst->cfg->cpuclock.tot_max_frequency = 0;

   inst->cfg->cpuclock.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->cpuclock.o_gadget,
                             "base/theme/gadget/cpuclock",
                             "e/gadget/cpuclock/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->cpuclock.o_gadget, "base/theme/gadget/cpuclock",
                             "e/gadget/cpuclock/main");
   E_EXPAND(inst->cfg->cpuclock.o_gadget);
   E_FILL(inst->cfg->cpuclock.o_gadget);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,governor,next", "*",
                                   _cpuclock_face_cb_set_governor, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,increase", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,decrease", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   evas_object_event_callback_add(inst->cfg->cpuclock.o_gadget, EVAS_CALLBACK_RESIZE, _cpuclock_resize_cb, inst);
   elm_box_pack_end(inst->o_main, inst->cfg->cpuclock.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpuclock.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _cpuclock_mouse_down_cb, inst);
   evas_object_show(inst->cfg->cpuclock.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _cpuclock_created_cb, data);

   inst->cfg->cpuclock.status = _cpuclock_status_new();
   _cpuclock_status_check_available(inst->cfg->cpuclock.status);

   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_POWERSAVE_UPDATE, _cpuclock_event_cb_powersave, inst);

   _cpuclock_config_updated(inst);
   if ((inst->cfg->cpuclock.restore_governor) && (inst->cfg->cpuclock.governor))
     {
        for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
          {
             if (!strcmp(l->data, inst->cfg->cpuclock.governor))
               {
                  _cpuclock_set_governor(inst->cfg->cpuclock.governor);
                  break;
               }
          }
     }
}

Evas_Object *
sysinfo_cpuclock_create(Evas_Object *parent, Instance *inst)
{
   Eina_List *l = NULL;

   if (inst->cfg->cpuclock.pstate_min == 0) inst->cfg->cpuclock.pstate_min = 1;
   if (inst->cfg->cpuclock.pstate_max == 0) inst->cfg->cpuclock.pstate_max = 101;
   inst->cfg->cpuclock.percent = 0;
   inst->cfg->cpuclock.tot_min_frequency = 0;
   inst->cfg->cpuclock.tot_max_frequency = 0;

   inst->cfg->cpuclock.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->cpuclock.o_gadget, "base/theme/gadget/cpuclock",
                           "e/gadget/cpuclock/main");
   E_EXPAND(inst->cfg->cpuclock.o_gadget);
   E_FILL(inst->cfg->cpuclock.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpuclock.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _cpuclock_mouse_down_cb, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,governor,next", "*",
                                   _cpuclock_face_cb_set_governor, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,increase", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   edje_object_signal_callback_add(elm_layout_edje_get(inst->cfg->cpuclock.o_gadget), "e,action,frequency,decrease", "*",
                                   _cpuclock_face_cb_set_frequency, inst);
   evas_object_event_callback_add(inst->cfg->cpuclock.o_gadget, EVAS_CALLBACK_RESIZE, _cpuclock_resize_cb, inst);
   evas_object_show(inst->cfg->cpuclock.o_gadget);

   inst->cfg->cpuclock.status = _cpuclock_status_new();
   _cpuclock_status_check_available(inst->cfg->cpuclock.status);

   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpuclock.handlers, E_EVENT_POWERSAVE_UPDATE, _cpuclock_event_cb_powersave, inst);

   _cpuclock_config_updated(inst);
   if ((inst->cfg->cpuclock.restore_governor) && (inst->cfg->cpuclock.governor))
     {
        for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
          {
             if (!strcmp(l->data, inst->cfg->cpuclock.governor))
               {
                  _cpuclock_set_governor(inst->cfg->cpuclock.governor);
                  break;
               }
          }
     }

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
     ci->id = eina_list_count(sysinfo_config->items) + 1;
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
   ci->cpuclock.configure = NULL;

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
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created", _cpuclock_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _cpuclock_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_cpuclock_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

