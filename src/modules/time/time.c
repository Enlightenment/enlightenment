#include "clock.h"

#include <sys/time.h>
#include <time.h>

static Eio_Monitor *clock_te_monitor = NULL;
static Eio_Monitor *clock_tz2_monitor = NULL;
static Eio_Monitor *clock_tzetc_monitor = NULL;
static Eina_List *clock_eio_handlers = NULL;

static Ecore_Timer *update_today = NULL;

#define ZONEINFO_DIR "/usr/share/zoneinfo/posix"
#define ZONEINFO_DIR_LEN sizeof(ZONEINFO_DIR) - 1

#define TE_DECL \
   const char *tzenv; \
   char prevtz[128] = {0}

#define TZSET(inst) \
   tzenv = getenv("TZ"); \
   if (tzenv) \
     strncpy(prevtz, tzenv, sizeof(prevtz) - 1); \
   if (inst->cfg->timezone) \
     setenv("TZ", inst->cfg->timezone, 1); \
   tzset()

#define TZUNSET() \
   if (prevtz[0]) \
     setenv("TZ", prevtz, 1); \
   else \
     unsetenv("TZ"); \
   tzset()

static void
_zoneinfo_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   eio_file_cancel(data);
}

static void
_zoneinfo_done(void *data, Eio_File *handler EINA_UNUSED)
{
   evas_object_event_callback_del(data, EVAS_CALLBACK_DEL, _zoneinfo_del);
}

static void
_zoneinfo_error(void *data, Eio_File *handler EINA_UNUSED, int error EINA_UNUSED)
{
   evas_object_event_callback_del(data, EVAS_CALLBACK_DEL, _zoneinfo_del);
}

static Eina_Bool
_zoneinfo_filter(void *data EINA_UNUSED, Eio_File *handler EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   return (info->type == EINA_FILE_REG) || (info->type == EINA_FILE_DIR);
}

static void
_zoneinfo_main(void *data, Eio_File *handler EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   if (info->type == EINA_FILE_REG)
     config_timezone_populate(data, &info->path[ZONEINFO_DIR_LEN + 1]);
   else
     {
        Eio_File *ls;

        ls = eio_file_direct_ls(info->path, _zoneinfo_filter, _zoneinfo_main, _zoneinfo_done, _zoneinfo_error, data);
        evas_object_event_callback_add(data, EVAS_CALLBACK_DEL, _zoneinfo_del, ls);
     }
}

EINTERN void
time_zoneinfo_scan(Evas_Object *obj)
{
   Eio_File *ls;

   ls = eio_file_direct_ls(ZONEINFO_DIR, _zoneinfo_filter, _zoneinfo_main, _zoneinfo_done, _zoneinfo_error, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _zoneinfo_del, ls);
}

EINTERN void
time_daynames_clear(Instance *inst)
{
   int x;

   for (x = 0; x < 7; x++)
     eina_stringshare_replace(&inst->daynames[x], NULL);
}

EINTERN void
time_datestring_format(Instance *inst, char *buf, int bufsz)
{
   struct timeval timev;
   struct tm *tm;
   time_t tt;
   const char *default_str = "%F";
   TE_DECL;

   buf[0] = 0;
   if (!inst->cfg->show_date) return;
   TZSET(inst);
   gettimeofday(&timev, NULL);
   tt = (time_t)(timev.tv_sec);
   tm = localtime(&tt);
   TZUNSET();
   switch (inst->cfg->show_date)
     {
      case CLOCK_DATE_DISPLAY_FULL:
        strftime(buf, bufsz, _("%a, %e %b, %Y"), (const struct tm *)tm);
        break;
      case CLOCK_DATE_DISPLAY_NUMERIC:
        strftime(buf, bufsz, _("%a, %x"), (const struct tm *)tm);
        break;
      case CLOCK_DATE_DISPLAY_DATE_ONLY:
        strftime(buf, bufsz, "%x", (const struct tm *)tm);
        break;
      case CLOCK_DATE_DISPLAY_ISO8601:
        strftime(buf, bufsz, "%F", (const struct tm *)tm);
        break;
      case CLOCK_DATE_DISPLAY_CUSTOM:
/* disable warning for known-safe code */
DISABLE_WARNING(format-nonliteral, format-nonliteral, format-nonliteral)
        if (!strftime(buf, bufsz, inst->cfg->time_str[1] ?: default_str, (const struct tm *)tm))
          strncpy(buf, "ERROR", bufsz - 1);
ENABLE_WARNING(format-nonliteral, format-nonliteral, format-nonliteral)
        break;
      default: break;
     }
}

EINTERN int
time_string_format(Instance *inst, char *buf, int bufsz)
{
   struct timeval timev;
   struct tm *tm;
   time_t tt;
   const char *default_fmt = "%R";
   TE_DECL;

   buf[0] = 0;
   TZSET(inst);
   gettimeofday(&timev, NULL);
   tt = (time_t)(timev.tv_sec);
   tm = localtime(&tt);
   TZUNSET();
/* disable warning for known-safe code */
DISABLE_WARNING(format-nonliteral, format-nonliteral, format-nonliteral)
   if (!strftime(buf, bufsz, inst->cfg->time_str[0] ?: default_fmt, (const struct tm *)tm))
     strncpy(buf, "ERROR", bufsz - 1);
ENABLE_WARNING(format-nonliteral, format-nonliteral, format-nonliteral)
   return tm->tm_sec;
}

EINTERN void
time_instance_update(Instance *inst)
{
   struct timeval timev;
   struct tm *tm, tms, tmm, tm2;
   time_t tt;
   int started = 0, num, i;
   int day;

   tzset();
   gettimeofday(&timev, NULL);
   tt = (time_t)(timev.tv_sec);
   tm = localtime(&tt);

   time_daynames_clear(inst);
   if (!tm) return;

   // tms == current date time "saved"
   // tm2 == date to look at adjusting for madj
   // tm2 == month baseline @ 1st
   memcpy(&tms, tm, sizeof(struct tm));
   num = 0;
   for (day = (0 - 6); day < (31 + 16); day++)
     {
        memcpy(&tmm, &tms, sizeof(struct tm));
        tmm.tm_sec = 0;
        tmm.tm_min = 0;
        tmm.tm_hour = 10;
        tmm.tm_mon += inst->madj;
        tmm.tm_mday = 1; // start at the 1st of the month
        tmm.tm_wday = 0; // ignored by mktime
        tmm.tm_yday = 0; // ignored by mktime
        tmm.tm_isdst = 0; // ignored by mktime
        tt = mktime(&tmm);
        tm = localtime(&tt);
        memcpy(&tm2, tm, sizeof(struct tm));

        tt = mktime(&tmm);
        tt += (day * 60 * 60 * 24);
        tm = localtime(&tt);
        memcpy(&tmm, tm, sizeof(struct tm));
        if (!started)
          {
             if (tm->tm_wday == inst->cfg->week.start)
               {
                  char buf[32];

                  for (i = 0; i < 7; i++, tm->tm_wday = (tm->tm_wday + 1) % 7)
                    {
                       strftime(buf, sizeof(buf), "%a", tm);
                       inst->daynames[i] = eina_stringshare_add(buf);
                    }
                  started = 1;
               }
          }
        if (started)
          {
             int y = num / 7;
             int x = num % 7;

             if (y < 6)
               {
                  inst->daynums[x][y] = tmm.tm_mday;

                  inst->dayvalids[x][y] = 0;
                  if (tmm.tm_mon == tm2.tm_mon) inst->dayvalids[x][y] = 1;

                  inst->daytoday[x][y] = 0;
                  if ((tmm.tm_mon == tms.tm_mon) &&
                      (tmm.tm_year == tms.tm_year) &&
                      (tmm.tm_mday == tms.tm_mday))
                    inst->daytoday[x][y] = 1;

                  inst->dayweekends[x][y] = 0;
                  for (i = inst->cfg->weekend.start;
                       i < (inst->cfg->weekend.start + inst->cfg->weekend.len);
                       i++)
                    {
                       if (tmm.tm_wday == (i % 7))
                         {
                            inst->dayweekends[x][y] = 1;
                            break;
                         }
                    }
               }
             num++;
          }
     }

   memcpy(&tmm, &tms, sizeof(struct tm));
   tmm.tm_sec = 0;
   tmm.tm_min = 0;
   tmm.tm_hour = 10;
   tmm.tm_mon += inst->madj;
   tmm.tm_mday = 1; // start at the 1st of the month
   tmm.tm_wday = 0; // ignored by mktime
   tmm.tm_yday = 0; // ignored by mktime
   tmm.tm_isdst = 0; // ignored by mktime
   tt = mktime(&tmm);
   tm = localtime(&tt);
   memcpy(&tm2, tm, sizeof(struct tm));
   inst->year[sizeof(inst->year) - 1] = 0;
   strftime(inst->year, sizeof(inst->year) - 1, "%Y", (const struct tm *)&tm2);
   inst->month[sizeof(inst->month) - 1] = 0;
   strftime(inst->month, sizeof(inst->month) - 1, "%B", (const struct tm *)&tm2); // %b for short month
}


static Eina_Bool
_update_today_timer(void *data EINA_UNUSED)
{
   time_t t, t_tomorrow;
   const struct tm *now;
   struct tm today;

   t = time(NULL);
   now = localtime(&t);
   memcpy(&today, now, sizeof(today));
   today.tm_sec = 1;
   today.tm_min = 0;
   today.tm_hour = 0;

   t_tomorrow = mktime(&today) + 24 * 60 * 60;
   if (update_today) ecore_timer_interval_set(update_today, t_tomorrow - t);
   else update_today = ecore_timer_loop_add(t_tomorrow - t, _update_today_timer, NULL);
   return EINA_TRUE;
}


static Eina_Bool
_clock_eio_update(void *d EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Eio_Monitor_Event *ev = event;

   if ((ev->monitor == clock_te_monitor) ||
       (ev->monitor == clock_tz2_monitor) ||
       (ev->monitor == clock_tzetc_monitor))
     {
        if (eina_streq(ev->filename, "/etc/localtime") ||
            eina_streq(ev->filename, "/etc/timezone"))
          clock_instances_redo();
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_clock_time_update(void *d EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   clock_instances_redo();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_clock_eio_error(void *d EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Eio_Monitor_Event *ev = event;

   if ((ev->monitor == clock_te_monitor) ||
       (ev->monitor == clock_tz2_monitor) ||
       (ev->monitor == clock_tzetc_monitor))
     {
        E_FREE_FUNC(clock_te_monitor, eio_monitor_del);
        if (ecore_file_exists("/etc/localtime"))
          clock_te_monitor = eio_monitor_add("/etc/localtime");

        E_FREE_FUNC(clock_tz2_monitor, eio_monitor_del);
        if (ecore_file_exists("/etc/timezone"))
          clock_tz2_monitor = eio_monitor_add("/etc/timezone");

        E_FREE_FUNC(clock_tzetc_monitor, eio_monitor_del);
        if (ecore_file_is_dir("/etc"))
          clock_tzetc_monitor = eio_monitor_add("/etc");
     }

   return ECORE_CALLBACK_PASS_ON;
}

EINTERN void
time_init(void)
{
   if (ecore_file_exists("/etc/localtime"))
     clock_te_monitor = eio_monitor_add("/etc/localtime");
   if (ecore_file_exists("/etc/timezone"))
     clock_tz2_monitor = eio_monitor_add("/etc/timezone");
   if (ecore_file_is_dir("/etc"))
     clock_tzetc_monitor = eio_monitor_add("/etc");

    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_ERROR, _clock_eio_error, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_FILE_CREATED, _clock_eio_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_FILE_MODIFIED, _clock_eio_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_FILE_DELETED, _clock_eio_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_SELF_DELETED, _clock_eio_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, EIO_MONITOR_SELF_RENAME, _clock_eio_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, E_EVENT_SYS_RESUME, _clock_time_update, NULL);
    E_LIST_HANDLER_APPEND(clock_eio_handlers, ECORE_EVENT_SYSTEM_TIMEDATE_CHANGED, _clock_time_update, NULL);
   _update_today_timer(NULL);
}

EINTERN void
time_shutdown(void)
{
   E_FREE_FUNC(update_today, ecore_timer_del);
   E_FREE_FUNC(clock_te_monitor, eio_monitor_del);
   E_FREE_FUNC(clock_tz2_monitor, eio_monitor_del);
   E_FREE_FUNC(clock_tzetc_monitor, eio_monitor_del);
}
