#include "e.h"
#include "evry_api.h"
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
// TODO - show error when input not parseable

typedef struct _Plugin Plugin;

struct _Plugin
{
   Evry_Plugin base;
};

static Eina_Bool _cb_data(void *data, int type, void *event);
static Eina_Bool _cb_error(void *data, int type, void *event);
static Eina_Bool _cb_del(void *data, int type, void *event);

static const Evry_API *evry = NULL;
static Evry_Module *evry_module = NULL;
static Evry_Plugin *_plug;
static Ecore_Event_Handler *action_handler = NULL;

static Ecore_Exe *exe = NULL;
static Eina_List *history = NULL;
static Eina_List *handlers = NULL;
static Eina_Bool active = EINA_FALSE;
static char _module_icon[] = "accessories-calculator";
static Evry_Item *cur_item = NULL;

/* Debounce: rather than killing and respawning a process on every
 * single keystroke, wait for a short pause in typing before actually
 * running the calculator. Cuts down on wasted process spawns during
 * fast typing (most of which would just get killed by the next
 * keystroke anyway). */
#define CALC_DEBOUNCE 0.15
static Ecore_Timer *fetch_timer = NULL;
static char pending_input[1024];

/* Detected once at module init: use qalc (more featureful - unit
 * conversions, etc.) when it's installed, otherwise fall back to bc,
 * which is assumed to always be available. */
static Eina_Bool _have_qalc = EINA_FALSE;

/* Look for `name` as an executable file in $PATH. */
static Eina_Bool
_binary_exists(const char *name)
{
   const char *path = getenv("PATH");
   char candidate[PATH_MAX];
   const char *p, *colon;

   if (!path) return EINA_FALSE;

   for (p = path; p && *p; p = colon ? colon + 1 : NULL)
     {
        size_t len;

        colon = strchr(p, ':');
        len = colon ? (size_t)(colon - p) : strlen(p);

        if (len && (len + strlen(name) + 2) < sizeof(candidate))
          {
             memcpy(candidate, p, len);
             candidate[len] = '/';
             strcpy(candidate + len + 1, name);

             if (access(candidate, X_OK) == 0)
               return EINA_TRUE;
          }
     }

   return EINA_FALSE;
}

static Evry_Plugin *
_begin(Evry_Plugin *plugin, const Evry_Item *item EINA_UNUSED)
{
   Plugin *p;

   if (active)
     return NULL;

   EVRY_PLUGIN_INSTANCE(p, plugin)

   /* Both qalc and bc are now run fresh (one-shot) for every query
    * rather than as a single long-lived pipe, so the event handlers
    * are registered once for the lifetime of this plugin instance
    * instead of once per spawned process. */
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EXE_EVENT_DATA, _cb_data, p));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EXE_EVENT_ERROR, _cb_error, p));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EXE_EVENT_DEL, _cb_del, p));

   active = EINA_TRUE;

   return EVRY_PLUGIN(p);
}

/* Quote `input` as a single POSIX shell word and write the result
 * (including surrounding quotes) into `out`. This is needed because
 * ecore_exe_pipe_run() executes its argument through a shell, and the
 * expression typed by the user must never be interpreted by it. */
static void
_shell_quote(const char *input, char *out, size_t outlen)
{
   size_t i, o = 0;

   if (outlen < 3)
     {
        if (outlen) out[0] = '\0';
        return;
     }

   out[o++] = '\'';

   for (i = 0; input[i] && (o + 5) < outlen; i++)
     {
        if (input[i] == '\'')
          {
             /* close quote, escaped literal quote, reopen quote */
             out[o++] = '\'';
             out[o++] = '\\';
             out[o++] = '\'';
             out[o++] = '\'';
          }
        else
          out[o++] = input[i];
     }

   out[o++] = '\'';
   out[o] = '\0';
}

static int
_run_calc(Plugin *p EINA_UNUSED, const char *cmd)
{
   exe = ecore_exe_pipe_run(cmd,
                            ECORE_EXE_PIPE_READ |
                            ECORE_EXE_PIPE_READ_LINE_BUFFERED |
                            ECORE_EXE_PIPE_ERROR |
                            ECORE_EXE_PIPE_ERROR_LINE_BUFFERED |
                            ECORE_EXE_USE_SH,
                            NULL);
   return !!exe;
}

static void
_finish(Evry_Plugin *plugin)
{
   GET_PLUGIN(p, plugin);
   Ecore_Event_Handler *h;
   Evry_Item *it;
   int items = 0;

   EINA_LIST_FREE (p->base.items, it)
     {
        if ((items++ > 1) && (items < 10))
          history = eina_list_prepend(history, eina_stringshare_add(it->label));

        if (it == cur_item)
          cur_item = NULL;

        EVRY_ITEM_FREE(it);
     }

   if (cur_item)
     {
        EVRY_ITEM_FREE(cur_item);
        cur_item = NULL;
     }

   EINA_LIST_FREE (handlers, h)
     ecore_event_handler_del(h);

   if (fetch_timer)
     {
        ecore_timer_del(fetch_timer);
        fetch_timer = NULL;
     }

   if (exe)
     {
        ecore_exe_terminate(exe);
        ecore_exe_free(exe);
        exe = NULL;
     }
   active = EINA_FALSE;

   E_FREE(p);
}

static Eina_Bool
_cb_action_performed(EINA_UNUSED void *data, EINA_UNUSED int type, void *event)
{
   Eina_List *l;
   Evry_Item *it, *it2, *it_old;
   Evry_Event_Action_Performed *ev = event;
   Evry_Plugin *p = _plug;

   if (!ev->it1 || !(ev->it1->plugin == p))
     return ECORE_CALLBACK_PASS_ON;

   if (!p->items)
     return ECORE_CALLBACK_PASS_ON;

   /* remove duplicates */
   if (p->items->next)
     {
        it = p->items->data;

        EINA_LIST_FOREACH (p->items->next, l, it2)
          {
             if (!strcmp(it->label, it2->label))
               {
                  p->items = eina_list_promote_list(p->items, l);
                  evry->item_changed(it, 0, 1);
                  EVRY_PLUGIN_UPDATE(p, EVRY_UPDATE_ADD);
                  return ECORE_CALLBACK_PASS_ON;
               }
          }
     }

   it_old = p->items->data;
   it_old->selected = EINA_FALSE;

   it2 = EVRY_ITEM_NEW(Evry_Item, p, it_old->label, NULL, NULL);
   it2->context = eina_stringshare_ref(p->name);
   p->items = eina_list_prepend(p->items, it2);
   evry->item_changed(it2, 0, 1);
   EVRY_PLUGIN_UPDATE(p, EVRY_UPDATE_ADD);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fetch_timer(void *data)
{
   Plugin *p = data;
   char expr[1024];
   char quoted[1200];
   char buf[1400];

   fetch_timer = NULL;

   /* This supersedes whatever the calculator was still computing for
    * an earlier, now-stale, partial expression. */
   if (exe)
     {
        ecore_exe_terminate(exe);
        ecore_exe_free(exe);
        exe = NULL;
     }

   /* qalc treats an empty expression as a request to start its
    * interactive mode and prints a "> " prompt, which would end up as
    * the result. Nothing to calculate yet, so don't spawn anything. */
   if (!*pending_input)
     return ECORE_CALLBACK_CANCEL;

   if (_have_qalc)
     {
        /* -t  : terse output - just the result, nothing else
         * +u8 : allow unicode in output (×, ², √, ...) */
        _shell_quote(pending_input, quoted, sizeof(quoted));
        snprintf(buf, sizeof(buf), "qalc -t +u8 %s", quoted);
     }
   else
     {
        /* bc has no notion of terse/default precision, so keep the
         * original scale=3 default unless the user set their own. */
        if (!strncmp(pending_input, "scale=", 6))
          snprintf(expr, sizeof(expr), "%s", pending_input);
        else
          snprintf(expr, sizeof(expr), "scale=3;%s", pending_input);

        _shell_quote(expr, quoted, sizeof(quoted));
        snprintf(buf, sizeof(buf), "printf '%%s\\n' %s | bc -l", quoted);
     }

   _run_calc(p, buf);

   return ECORE_CALLBACK_CANCEL;
}

static int
_fetch(Evry_Plugin *plugin, const char *input)
{
   GET_PLUGIN(p, plugin);

   Evry_Item *it;

   if (!input) return 0;

   if (!cur_item)
     {
        it = EVRY_ITEM_NEW(Evry_Item, p, "0", NULL, NULL);
        it->context = eina_stringshare_ref(p->base.name);
        cur_item = it;
     }

   if (history)
     {
        const char *result;

        EINA_LIST_FREE (history, result)
          {
             it = EVRY_ITEM_NEW(Evry_Item, p, result, NULL, NULL);
             it->context = eina_stringshare_ref(p->base.name);
             p->base.items = eina_list_prepend(p->base.items, it);
             eina_stringshare_del(result);
          }
     }

   snprintf(pending_input, sizeof(pending_input), "%s", input);

   if (fetch_timer) ecore_timer_del(fetch_timer);
   fetch_timer = ecore_timer_add(CALC_DEBOUNCE, _cb_fetch_timer, p);

   return EVRY_PLUGIN_HAS_ITEMS(p);
}

static Eina_Bool
_cb_data(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *ev = event;
   Evry_Plugin *p = data;
   Evry_Item *it;

   if (ev->exe != exe) return ECORE_CALLBACK_PASS_ON;

   if (ev->lines)
     {
        it = cur_item;
        eina_stringshare_del(it->label);
        it->label = eina_stringshare_add(ev->lines->line);

        if (!(it = eina_list_data_get(p->items)) || (it != cur_item))
          {
             p->items = eina_list_prepend(p->items, cur_item);
             EVRY_PLUGIN_UPDATE(p, EVRY_UPDATE_ADD);
          }
        else if (it)
          evry->item_changed(it, 0, 0);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_error(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *ev = event;
   Evry_Plugin *p = data;

   if (ev->exe != exe)
     return ECORE_CALLBACK_PASS_ON;

   p->items = eina_list_remove(p->items, cur_item);
   EVRY_PLUGIN_UPDATE(p, EVRY_UPDATE_ADD);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *e = event;

   if (e->exe != exe)
     return ECORE_CALLBACK_PASS_ON;

   exe = NULL;
   return ECORE_CALLBACK_PASS_ON;
}

static int
_plugins_init(const Evry_API *_api)
{
   evry = _api;

   if (!evry->api_version_check(EVRY_API_VERSION))
     return EINA_FALSE;

   action_handler = evry->event_handler_add(EVRY_EVENT_ACTION_PERFORMED,
                                            _cb_action_performed, NULL);

   _have_qalc = _binary_exists("qalc");

   _plug = EVRY_PLUGIN_BASE(N_("Calculator"), _module_icon, EVRY_TYPE_TEXT,
                            _begin, _finish, _fetch);

   _plug->history = EINA_FALSE;
   _plug->async_fetch = EINA_TRUE;

   if (evry->plugin_register(_plug, EVRY_PLUGIN_SUBJECT, 0))
     {
        Plugin_Config *pc = _plug->config;
        pc->view_mode = VIEW_MODE_LIST;
        pc->trigger = eina_stringshare_add("=");
        pc->trigger_only = EINA_TRUE;
        pc->aggregate = EINA_FALSE;
        /* pc->top_level = EINA_FALSE; */
        /* pc->min_query = 3; */
     }

   return EINA_TRUE;
}

static void
_plugins_shutdown(void)
{
   ecore_event_handler_del(action_handler);
   action_handler = NULL;

   EVRY_PLUGIN_FREE(_plug);
}

/***************************************************************************/

Eina_Bool
evry_plug_calc_init(E_Module *m EINA_UNUSED)
{
   EVRY_MODULE_NEW(evry_module, evry, _plugins_init, _plugins_shutdown);

   return EINA_TRUE;
}

void
evry_plug_calc_shutdown(void)
{
   EVRY_MODULE_FREE(evry_module);
}

void
evry_plug_calc_save(void){}

