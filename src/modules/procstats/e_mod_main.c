#include <e.h>
#include "process.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

static int64_t      _mem_total;
static Eina_List   *_clients = NULL;
static Ecore_Timer *_clients_timer = NULL;

#define _TIMER_FREQ 3.0

typedef struct _Proc_Stats Proc_Stats;
struct _Proc_Stats
{
   E_Client    *client;
   Evas_Object *obj;
   Evas_Object *obj_swallow;
   pid_t        pid;
   uint64_t     mem_size;
   uint64_t     cpu_time;
   uint64_t     cpu_time_prev;
};

static void
_memory_total(void)
{
#if defined(__linux__)
   char line[256];
   FILE *f = fopen("/proc/meminfo", "r");
   if (!f) return;

   while (fgets(line, sizeof(line), f) != NULL)
     {
        if (!strncmp("MemTotal:", line, 9))
          {
             char *p, *t;
             p = strchr(line, ':') + 1;
             while (isspace(*p)) p++;
             t = strtok(p, " ");
             _mem_total = atoll(t) * 1024;
             break;
          }
     }
   fclose(f);
#else
   size_t len = sizeof(_mem_total);
   int mib[5] = { CTL_HW, HW_PHYSMEM, 0, 0, 0 };
   sysctl(mib, 2, &_mem_total, &len, NULL, 0);
#endif
}

static Eina_Bool
_proc_stats_item_exists(E_Client *ec)
{
   Proc_Stats *item;
   Eina_List *l;

   EINA_LIST_FOREACH(_clients, l, item)
     {
        if (item->pid == ec->netwm.pid) return 1;
     }
   return 0;
}

static void
_proc_stats_item_del(Proc_Stats *item)
{
   edje_object_signal_emit(item->obj, "e,state,procstats,off", "e");
   evas_object_del(item->obj_swallow);
   free(item);
}

static void
_proc_stats_client_del_cb(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Stats *item = data;
   _proc_stats_item_del(item);
}

static void
_proc_stats_item_add(E_Client *ec)
{
   Evas_Object *tb, *pb;
   Evas_Object *o;
   Proc_Stats *item;

   if (ec->internal || ec->netwm.pid == -1) return;
   if (!ec->frame_object) return;
   if (_proc_stats_item_exists(ec)) return;

   o = edje_object_add(evas_object_evas_get(ec->frame));

   e_theme_edje_object_set(o, "base/theme/borders",
                    "e/widgets/border/default/border");

   if (!edje_object_part_exists(o, "e.procstats.swallow")) return;

   tb = elm_table_add(e_comp->elm);
   evas_object_show(tb);

   pb = elm_progressbar_add(e_comp->elm);
   elm_progressbar_span_size_set(pb, 60);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(pb);
   elm_table_pack(tb, pb, 0, 0, 1, 1);
   evas_object_data_set(tb,  "pb_cpu", pb);

   pb = elm_progressbar_add(e_comp->elm);
   elm_progressbar_span_size_set(pb, 100);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(pb);
   elm_table_pack(tb, pb, 1, 0, 1, 1);
   evas_object_data_set(tb,  "pb_mem", pb);

   item = calloc(1, sizeof(Proc_Stats));
   EINA_SAFETY_ON_NULL_RETURN(item);
   item->pid = ec->netwm.pid;
   item->client = ec;
   item->obj = ec->frame_object;
   item->obj_swallow = tb;

   edje_object_part_swallow(ec->frame_object, "e.procstats.swallow", tb);
   edje_object_signal_emit(ec->frame_object, "e,state,procstats,on", "e");

   evas_object_event_callback_add(item->obj, EVAS_CALLBACK_DEL, _proc_stats_client_del_cb, item);

   _clients = eina_list_append(_clients, item);
}

static void
_proc_stats_item_remove(Proc_Stats *item)
{
   Eina_List *l, *l_next;
   Proc_Stats *it;

   EINA_LIST_FOREACH_SAFE(_clients, l, l_next, it)
     {
        if (it == item)
          {
            _proc_stats_item_del(item);
            _clients = eina_list_remove_list(_clients, l);
            return;
          }
     }
}

static Eina_Bool
_proc_stats_item_gone(Proc_Stats *item)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (item->client == ec) return 0;
     }

   return 1;
}

static void
_proc_stats_item_children_update(Eina_List *children, Proc_Stats *item)
{
   Eina_List *l;
   Proc_Info *child;

   EINA_LIST_FOREACH(children, l, child)
    {
       item->mem_size += child->mem_size;
       item->cpu_time += child->cpu_time;
       if (child->children)
         _proc_stats_item_children_update(child->children, item);
    }
}

static const char *
_size_format(unsigned long long bytes)
{
   const char *units = "BKMGTPEZY";
   unsigned long powi = 1;
   unsigned long long value;
   unsigned int precision = 2, powj = 1;

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       powi *= 1024;
       ++units;
       if (units[1] == '\0') break;
     }
   if (*units == 'B') precision = 0;
   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }
   return eina_slstr_printf("%1.*f%c", precision, (double) value / powi, *units);
}

static void
_proc_stats_item_display(Proc_Stats *item)
{
   Evas_Object *pb;
   double val = 0.0;

   if (item->cpu_time_prev > item->cpu_time)
     return;

   if (!item->cpu_time_prev) item->cpu_time_prev = item->cpu_time;

   pb = evas_object_data_get(item->obj_swallow, "pb_cpu");

   val = (item->cpu_time - item->cpu_time_prev) / _TIMER_FREQ;
   elm_progressbar_value_set(pb, val / 100.0);
   elm_object_part_text_set(pb, "elm.text.status", eina_slstr_printf("%1.0f %%", val));

   pb = evas_object_data_get(item->obj_swallow, "pb_mem");
   val = item->mem_size / (_mem_total / 100.0);
   elm_progressbar_value_set(pb, val / 100.0);
   elm_object_part_text_set(pb, "elm.text.status",
                            eina_slstr_printf("%s/%s", _size_format(item->mem_size),
                                              _size_format(_mem_total)));
}

static void
_proc_stats_item_update(Eina_List *procs, Proc_Stats *item)
{
   Proc_Info *proc;
   Eina_List *l;

   EINA_LIST_FOREACH(procs, l, proc)
     {
        if (proc->pid == item->pid)
          {
             item->cpu_time = item->mem_size = 0;
             item->mem_size += proc->mem_size;
             item->cpu_time +=  proc->cpu_time;
             _proc_stats_item_children_update(proc->children, item);
             break;
          }
     }
   _proc_stats_item_display(item);
   item->cpu_time_prev = item->cpu_time;
}

static Eina_Bool
_proc_stats_timer_cb(void *data EINA_UNUSED)
{
   Eina_List *procs, *l;
   E_Client *ec;
   Proc_Info *proc;
   Proc_Stats *item;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (!_proc_stats_item_exists(ec))
          _proc_stats_item_add(ec);
     }

   procs = proc_info_all_children_get();

   EINA_LIST_FOREACH(_clients, l, item)
     {
        if (_proc_stats_item_gone(item))
          _proc_stats_item_remove(item);
        else
         _proc_stats_item_update(procs, item);
     }

   EINA_LIST_FREE(procs, proc)
     proc_info_free(proc);

   return 1;
}

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
     "Procstats"
};

E_API int
e_modapi_init(E_Module *m EINA_UNUSED)
{
   _memory_total();
   _proc_stats_timer_cb(NULL);

   _clients_timer = ecore_timer_add(_TIMER_FREQ, _proc_stats_timer_cb, NULL);

   return 1;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Proc_Stats *item;

   if (_clients_timer)
     ecore_timer_del(_clients_timer);

   _clients_timer = NULL;

   EINA_LIST_FREE(_clients, item)
     _proc_stats_item_del(item);

   _clients = NULL;

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
