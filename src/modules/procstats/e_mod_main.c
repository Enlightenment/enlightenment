# include <e.h>
#include "process.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

#define POLL_INTERVAL 2.0

static int64_t         _mem_total;

typedef struct
{
   E_Powersave_Sleeper *sleeper;
   Ecore_Thread        *thread;
   Eina_List           *clients;
   int                  poll_interval;
   E_Module            *e_module;
} Proc_Stats_Module;

static Proc_Stats_Module *_this_module = NULL;

typedef struct
{
   E_Client       *ec;
   Evas_Object    *obj;
   Evas_Object    *frame_obj;
   Evas_Object    *popup;
   E_Object_Delfn *delfn;
   pid_t           pid;
   uint64_t        mem_size;
   uint64_t        cpu_time;
   uint64_t        cpu_time_prev;
} Proc_Stats_Client;

static void       _proc_stats_client_add(E_Client *ec);
static void       _proc_stats_client_display_update(Proc_Stats_Client *client);
static void       _proc_stats_client_remove(Proc_Stats_Client *client);
static Eina_Bool  _proc_stats_client_exists(E_Client *ec);
static void       _proc_stats_client_del(Proc_Stats_Client *client);
static Eina_Bool  _proc_stats_client_gone(Proc_Stats_Client *client);
static void       _proc_stats_client_children_update(Eina_List *children, Proc_Stats_Client *client);
static void       _proc_stats_client_update(Eina_List *procs, Proc_Stats_Client *client);

static Eina_Bool
_memory_total(void)
{
#if defined(__linux__)
   char line[256];
   FILE *f = fopen("/proc/meminfo", "r");
   if (!f) return 0;

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
   if(!_mem_total) return 0;
#else
   size_t len = sizeof(_mem_total);
   int mib[5] = { CTL_HW, HW_PHYSMEM, 0, 0, 0 };
   if (sysctl(mib, 2, &_mem_total, &len, NULL, 0) == -1)
     return 0;
#endif
   return 1;
}

static Eina_Bool
_proc_stats_client_exists(E_Client *ec)
{
   Proc_Stats_Client *client;
   Eina_List *l;
   Proc_Stats_Module *module = _this_module;

   EINA_LIST_FOREACH(module->clients, l, client)
     {
        if (client->pid == ec->netwm.pid) return 1;
     }
   return 0;
}

static void
_proc_stats_client_del(Proc_Stats_Client *client)
{
   if (client->popup) evas_object_del(client->popup);
   client->popup = NULL;
   edje_object_signal_emit(client->frame_obj, "e,state,procstats,off", "e");
   evas_object_del(client->obj);
   e_object_delfn_del(E_OBJECT(client->ec), client->delfn);

   free(client);
   client = NULL;
}

static void
_proc_stats_client_del_cb(void *data, void *obj EINA_UNUSED)
{
   Proc_Stats_Client *client = data;

   _proc_stats_client_remove(client);
}

static void
_proc_stats_client_move_cb(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Stats_Client *client;
   Evas_Coord ox, oy, ow, oh;

   client = data;

   if ((!client) || (!client->popup)) return;

   evas_object_geometry_get(client->obj, &ox, &oy, &ow, &oh);
   evas_object_move(client->popup, ox + (ow / 2), oy);

   if ((client->ec->hidden) || (client->ec->iconic))
     evas_object_hide(client->popup);
   else
     evas_object_show(client->popup);
}

static void
_proc_stats_icon_clicked_cb(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   Proc_Stats_Client *client;
   Evas_Object *o, *tb;
   Evas_Object *pb;
   Evas_Coord ox, oy, ow, oh;

   ev = event_info;
   client = data;

   if (ev->button != 1) return;

   if (!client) return;

   if (client->popup)
     {
        evas_object_del(client->popup);
        client->popup = NULL;
        return;
     }

   evas_object_geometry_get(client->obj, &ox, &oy, &ow, &oh);

   client->popup = o = elm_ctxpopup_add(e_comp->elm);
   E_FILL(o); E_EXPAND(o);
   elm_object_style_set(o, "noblock");
   evas_object_layer_set(o, E_LAYER_MENU);

   tb = elm_table_add(o);
   E_FILL(tb); E_EXPAND(tb);
   elm_object_content_set(o, tb);
   evas_object_show(tb);

   pb = elm_progressbar_add(o);
   elm_progressbar_span_size_set(pb, 140);
   E_FILL(pb); E_EXPAND(pb);
   elm_table_pack(tb, pb, 0, 0, 1, 1);
   evas_object_data_set(o, "pb_cpu", pb);
   evas_object_show(pb);

   pb = elm_progressbar_add(o);
   elm_progressbar_span_size_set(pb, 140);
   E_FILL(pb); E_EXPAND(pb);
   elm_table_pack(tb, pb, 0, 1, 1, 1);
   evas_object_data_set(o, "pb_mem", pb);
   evas_object_show(pb);

   _proc_stats_client_display_update(client);

   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_LEFT,
                                       ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy);
   evas_object_show(o);
}

static void
_proc_stats_client_add(E_Client *ec)
{
   Evas_Object *o;
   Proc_Stats_Client *client;
   char buf[PATH_MAX];
   Proc_Stats_Module *module = _this_module;

   if (ec->internal || ec->netwm.pid == -1) return;
   if (!ec->frame_object) return;
   if (_proc_stats_client_exists(ec)) return;

   if (!edje_object_part_exists(ec->frame_object, "e.procstats.swallow")) return;

   client = calloc(1, sizeof(Proc_Stats_Client));
   EINA_SAFETY_ON_NULL_RETURN(client);

   o = edje_object_add(e_comp->evas);
   if (!e_theme_edje_object_set(o, "base/theme/modules/procstats",
                               "e/modules/procstats/border"))
     {
        evas_object_del(o);
        snprintf(buf, sizeof(buf), "%s/e-module-procstats.edj", e_module_dir_get(module->e_module));
        o = elm_icon_add(e_comp->elm);
        elm_image_file_set(o, buf, "icon");
     }
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_max_set(o, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   E_FILL(o); E_EXPAND(o);

   client->obj = o;
   client->frame_obj = ec->frame_object;
   client->pid = ec->netwm.pid;
   client->ec = ec;

   edje_object_part_swallow(ec->frame_object, "e.procstats.swallow", client->obj);
   evas_object_show(client->obj);
   edje_object_signal_emit(ec->frame_object, "e,state,procstats,on", "e");

   client->delfn = e_object_delfn_add(E_OBJECT(ec), _proc_stats_client_del_cb, client);
   evas_object_event_callback_add(client->obj, EVAS_CALLBACK_MOVE, _proc_stats_client_move_cb, client);
   evas_object_event_callback_add(client->obj, EVAS_CALLBACK_MOUSE_UP, _proc_stats_icon_clicked_cb, client);

   module->clients = eina_list_append(module->clients, client);
}

static void
_proc_stats_client_remove(Proc_Stats_Client *client)
{
   Eina_List *l, *l_next;
   Proc_Stats_Client *it;
   Proc_Stats_Module *module = _this_module;

   EINA_LIST_FOREACH_SAFE(module->clients, l, l_next, it)
     {
        if (it == client)
          {
             _proc_stats_client_del(client);
             module->clients = eina_list_remove_list(module->clients, l);
             return;
          }
     }
}

static Eina_Bool
_proc_stats_client_gone(Proc_Stats_Client *client)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (client->ec == ec) return 0;
     }

   return 1;
}

static void
_proc_stats_client_children_update(Eina_List *children, Proc_Stats_Client *client)
{
   Eina_List *l;
   Proc_Info *child;

   EINA_LIST_FOREACH(children, l, child)
    {
       client->mem_size += child->mem_size;
       client->cpu_time += child->cpu_time;
       if (child->children)
         _proc_stats_client_children_update(child->children, client);
    }
}

static char *
_size_format(unsigned long long bytes)
{
   char buf[1024];
   unsigned long powi = 1;
   unsigned long long value;
   unsigned int precision = 2, powj = 1;
   int i = 0;
   const char *units[8] = {
      _("B"), _("KiB"), _("MiB"), _("GiB"),
      _("TiB"), _("PiB"), _("EiB"), _("ZiB"),
   };

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       powi *= 1024;
       ++i;
       if (i == 7) break;
     }
   if (!i) precision = 0;
   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }
   snprintf(buf, sizeof(buf), "%1.*f%s", precision, (double) value / powi, units[i]);

   return strdup(buf);
}

static void
_proc_stats_client_display_update(Proc_Stats_Client *client)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *pb;
   Eina_Strbuf *buf;
   char *s;
   double val = 0.0;

   if (client->cpu_time_prev > client->cpu_time)
     return;

   if (!client->cpu_time_prev) client->cpu_time_prev = client->cpu_time;

   msg = malloc(sizeof(Edje_Message_Int_Set) + (sizeof(int) * 4));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 5;
   msg->val[0] = eina_cpu_count();
   msg->val[1] = (client->cpu_time - client->cpu_time_prev) / POLL_INTERVAL;
   msg->val[2] = (int) (_mem_total / 4096);
   msg->val[3] = (int) (client->mem_size / 4096);
   msg->val[4] = 0;
   edje_object_message_send(client->frame_obj, EDJE_MESSAGE_INT_SET, 1973, msg);
   edje_object_message_send(client->obj, EDJE_MESSAGE_INT_SET, 1973, msg);
   free(msg);

   if (!client->popup) return;

   pb = evas_object_data_get(client->popup, "pb_cpu");

   val = ((client->cpu_time - client->cpu_time_prev) / POLL_INTERVAL);
   elm_progressbar_value_set(pb, val / 100.0);

   buf = eina_strbuf_new();

   eina_strbuf_append_printf(buf, "%1.0f%%", val);
   elm_object_part_text_set(pb, "elm.text.status", eina_strbuf_string_get(buf));
   eina_strbuf_reset(buf);

   pb = evas_object_data_get(client->popup, "pb_mem");
   val = client->mem_size / (_mem_total / 100.0);
   elm_progressbar_value_set(pb, val / 100.0);

   s = _size_format(client->mem_size);
   eina_strbuf_append_printf(buf, "%s / ", s);
   free(s);
   s = _size_format(_mem_total);
   eina_strbuf_append(buf, s);
   free(s);

   elm_object_part_text_set(pb, "elm.text.status", eina_strbuf_string_get(buf));
   eina_strbuf_free(buf);
}

static void
_proc_stats_client_update(Eina_List *procs, Proc_Stats_Client *client)
{
   Proc_Info *proc;
   Eina_List *l;

   EINA_LIST_FOREACH(procs, l, proc)
     {
        if (proc->pid == client->pid)
          {
             client->cpu_time = client->mem_size = 0;
             client->mem_size += proc->mem_size;
             client->cpu_time +=  proc->cpu_time;
             _proc_stats_client_children_update(proc->children, client);
             break;
          }
     }
   _proc_stats_client_display_update(client);
   client->cpu_time_prev = client->cpu_time;
}

static void
_proc_stats_thread_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Proc_Stats_Module *module;
   Eina_List *procs, *l;
   E_Client *ec;
   Proc_Info *proc;
   Proc_Stats_Client *client;

   module = data;
   procs = msgdata;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (!_proc_stats_client_exists(ec))
          _proc_stats_client_add(ec);
     }

   EINA_LIST_FOREACH(module->clients, l, client)
     {
        if (_proc_stats_client_gone(client))
          _proc_stats_client_remove(client);
        else
          _proc_stats_client_update(procs, client);
     }

   EINA_LIST_FREE(procs, proc)
     proc_info_free(proc);
}

static void
_proc_stats_thread(void *data, Ecore_Thread *thread)
{
   Proc_Stats_Module *module = data;

   while (!ecore_thread_check(thread))
     {
        Eina_List *procs = proc_info_all_children_get();
        ecore_thread_feedback(thread, procs);
        for (int i = 0; i < 8 * module->poll_interval; i++)
          {
             if (ecore_thread_check(thread))
               return;
             usleep(125000);
             // e_powersave_sleeper_sleep(module->sleeper, module->poll_interval, EINA_TRUE);
          }
     }
}

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Procstats"
};

E_API int
e_modapi_init(E_Module *m)
{
   Proc_Stats_Module *module;

   if (!_memory_total())
     return 0;

   _this_module = module = calloc(1, sizeof(Proc_Stats_Module));
   EINA_SAFETY_ON_NULL_RETURN_VAL(module, 0);

   module->e_module = m;
   //module->sleeper = e_powersave_sleeper_new();
   module->poll_interval = POLL_INTERVAL;

   _proc_stats_thread_feedback_cb(module, NULL, proc_info_all_children_get());

   module->thread = ecore_thread_feedback_run(_proc_stats_thread,
                                              _proc_stats_thread_feedback_cb,
                                              NULL, NULL, module, 1);
   return 1;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Proc_Stats_Client *client;
   Proc_Stats_Module *module = _this_module;

   ecore_thread_cancel(module->thread);

   //e_powersave_sleeper_free(module->sleeper);

   EINA_LIST_FREE(module->clients, client)
     _proc_stats_client_del(client);

   free(module);
   _this_module = NULL;

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

