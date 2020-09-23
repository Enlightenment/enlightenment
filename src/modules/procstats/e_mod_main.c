#include <e.h>
#include "process.h"

static Eina_List   *_clients = NULL;
static Ecore_Timer *_clients_timer = NULL;

#define _TIMER_FREQ 3.0

typedef struct _Proc_Stats Proc_Stats;
struct _Proc_Stats
{
   E_Client    *client;
   Evas_Object *obj;
   pid_t        pid;
   uint64_t     mem_size;
   uint64_t     cpu_time;
   uint64_t     cpu_time_prev;
};

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
_proc_stats_client_del_cb(void *data EINA_UNUSED, Evas *evas EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   edje_object_signal_emit(obj, "e,state,procstats,off", "e");
}

static void
_proc_stats_item_add(E_Client *ec)
{
   Evas_Object *o;
   Proc_Stats *item;

   if (ec->internal || ec->netwm.pid == -1) return;
   if (!ec->frame_object) return;
   if (_proc_stats_item_exists(ec)) return;

   o = edje_object_add(evas_object_evas_get(ec->frame));

   e_theme_edje_object_set(o, "base/theme/borders",
                    "e/widgets/border/default/border");

   if (!edje_object_part_exists(o, "e.procstats.text")) return;

   item = calloc(1, sizeof(Proc_Stats));
   EINA_SAFETY_ON_NULL_RETURN(item);
   item->pid = ec->netwm.pid;
   item->client = ec;
   item->obj = ec->frame_object;

   edje_object_signal_emit(item->obj, "e,state,procstats,on", "e");
   evas_object_event_callback_add(item->obj, EVAS_CALLBACK_DEL, _proc_stats_client_del_cb, NULL);

   _clients = eina_list_append(_clients, item);
}

static void
_proc_stats_item_del(Proc_Stats *item)
{
   edje_object_signal_emit(item->obj, "e,state,procstats,off", "e");
   free(item);
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

static void
_proc_stats_item_display(Proc_Stats *item)
{
   Edje_Message_Int_Set *msg;
   int mem_size;

   if (item->cpu_time_prev > item->cpu_time)
     return;

   if (!item->cpu_time_prev) item->cpu_time_prev = item->cpu_time;

   mem_size = item->mem_size >> 10;
   msg = malloc(sizeof(Edje_Message_Int_Set) + (sizeof(int) * 3));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 4;
   msg->val[0] = eina_cpu_count();
   msg->val[1] = (item->cpu_time - item->cpu_time_prev) / _TIMER_FREQ;
   msg->val[2] = mem_size;
   msg->val[3] = 0;
   edje_object_message_send(item->obj, EDJE_MESSAGE_INT_SET, 1, msg);
   free(msg);
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
