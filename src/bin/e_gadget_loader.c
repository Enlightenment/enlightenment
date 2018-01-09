#include "config.h"
#define EFL_BETA_API_SUPPORT
#include <Ecore_Wl2.h>
#include <Elementary.h>
#include <dlfcn.h>
#include "e-gadget-client-protocol.h"
#include "action_route-client-protocol.h"
#include <uuid.h>

static Ecore_Event_Handler *handler;

static Eina_Hash *wins;
static Eina_Hash *gadget_globals;
static Eina_Hash *ar_globals;
static Eina_Hash *display_actions;
static Eina_List *tooltips;

typedef struct Gadget_Action
{
   Ecore_Wl2_Display *d;
   Eina_Stringshare *action;
   char handle[37];
   Eina_List *requestors;
   struct action_route_bind *ar_bind;
} Gadget_Action;

static inline Ecore_Wl2_Display *
win_display_get(Evas_Object *win)
{
   Ecore_Wl2_Window *ww;
   ww = elm_win_wl_window_get(win);
   return ecore_wl2_window_display_get(ww);
}

static void
win_emit(Ecore_Wl2_Display *d, const char *sig, uint32_t val)
{
   Evas_Object *win;

   win = eina_list_data_get(eina_hash_find(wins, &d));
   evas_object_smart_callback_call(win, sig, (uintptr_t*)(uintptr_t)val);
}

static void
_gadget_anchor(void *data, struct e_gadget *e_gadget EINA_UNUSED, uint32_t anchor)
{
   win_emit(data, "gadget_site_anchor", anchor);
}

static void
_gadget_orient(void *data, struct e_gadget *e_gadget EINA_UNUSED, uint32_t orient)
{
   win_emit(data, "gadget_site_orient", orient);
}

static void
_gadget_gravity(void *data, struct e_gadget *e_gadget EINA_UNUSED, uint32_t gravity)
{
   win_emit(data, "gadget_site_gravity", gravity);
}

static void
_gadget_configure(void *data, struct e_gadget *e_gadget EINA_UNUSED)
{
   Evas_Object *win;

   win = eina_list_data_get(eina_hash_find(wins, &data));
   evas_object_smart_callback_call(win, "gadget_configure", NULL);
}

static const struct e_gadget_listener _gadget_listener =
{
   _gadget_anchor,
   _gadget_orient,
   _gadget_gravity,
   _gadget_configure,
};

static void
_gadget_global_bind(Ecore_Wl2_Display *d, uint32_t id)
{
   Eina_List *l;
   Evas_Object *tt;

   struct e_gadget *gadget_global = wl_registry_bind(ecore_wl2_display_registry_get(d), id, &e_gadget_interface, 1);
   e_gadget_add_listener(gadget_global, &_gadget_listener, d);
   eina_hash_add(gadget_globals, &d, gadget_global);
   EINA_LIST_FOREACH(tooltips, l, tt)
     e_gadget_set_tooltip(gadget_global, ecore_wl2_window_surface_get(elm_win_wl_window_get(tt)));
}

static void
_ar_global_bind(Ecore_Wl2_Display *d, uint32_t id)
{
   struct action_route *ar_global = wl_registry_bind(ecore_wl2_display_registry_get(d), id, &action_route_interface, 1);
   eina_hash_add(ar_globals, &d, ar_global);
}

static Eina_Bool
_global_added(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Wl2_Event_Global *ev)
{
   if (eina_streq(ev->interface, "e_gadget"))
     _gadget_global_bind(ev->display, ev->id);
   else if (eina_streq(ev->interface, "action_route"))
     _ar_global_bind(ev->display, ev->id);
   return ECORE_CALLBACK_RENEW;
}

static void
_gadget_init(Ecore_Wl2_Display *d)
{
   Eina_Iterator *it;
   Ecore_Wl2_Global *g;

   if (wins)
     {
        if (eina_hash_find(gadget_globals, &d)) return;
     }
   else
     {
        gadget_globals = eina_hash_pointer_new(NULL);
        ar_globals = eina_hash_pointer_new(NULL);
     }
   it = ecore_wl2_display_globals_get(d);
   EINA_ITERATOR_FOREACH(it, g)
     {
        if (eina_streq(g->interface, "e_gadget"))
          _gadget_global_bind(d, g->id);
        else if (eina_streq(g->interface, "action_route"))
          _ar_global_bind(d, g->id);
     }
   eina_iterator_free(it);
   if (!handler)
     handler = ecore_event_handler_add(ECORE_WL2_EVENT_GLOBAL_ADDED, (Ecore_Event_Handler_Cb)_global_added, NULL);
}

static void
_ar_bind_activate(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   const char *params = event_info;
   Gadget_Action *ga = data;

   if (params && (!params[0])) params = NULL;
   action_route_bind_activate(ga->ar_bind, params);
}

static void
_ar_bind_del(Gadget_Action *ga)
{
   Evas_Object *r;
   eina_stringshare_del(ga->action);
   EINA_LIST_FREE(ga->requestors, r)
     evas_object_smart_callback_del_full(r, ga->handle, _ar_bind_activate, ga);
   free(ga);
}

static void
_ar_bind_end(void *data, struct action_route_bind *action_route_bind EINA_UNUSED)
{
   Gadget_Action *ga = data;
   Eina_List *l;
   Evas_Object *r;

   EINA_LIST_FOREACH(ga->requestors, l, r)
     evas_object_smart_callback_call(r, "gadget_action_end", ga->handle);
}

static void
_ar_bind_status(void *data, struct action_route_bind *action_route_bind, uint32_t state)
{
   uuid_t u;
   Gadget_Action *ga = data;
   Evas_Object *r;

   if (state == ACTION_ROUTE_BIND_STATE_REJECTED)
     {
        Eina_Hash *h;
        Eina_List *l;
        h = eina_hash_find(display_actions, &ga->d);
        EINA_LIST_FOREACH(ga->requestors, l, r)
          {
             if (ga->handle[0])
               evas_object_smart_callback_call(r, "gadget_action_deleted", ga->handle);
             else
               evas_object_smart_callback_call(r, "gadget_action", NULL);
          }
        eina_hash_del_by_key(h, ga->action);
        return;
     }
   uuid_generate(u);
   uuid_unparse_lower(u, ga->handle);
   ga->ar_bind = action_route_bind;
   r = eina_list_data_get(ga->requestors);
   evas_object_smart_callback_add(r, ga->handle, _ar_bind_activate, ga);
   evas_object_smart_callback_call(r, "gadget_action", ga->handle);
}

static const struct action_route_bind_listener _ar_bind_interface =
{
   _ar_bind_status,
   _ar_bind_end
};

static void
uriopen_request(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ecore_Wl2_Display *d = data;
   const char *uri = event_info;
   struct e_gadget *gadget_global = eina_hash_find(gadget_globals, &d);

   e_gadget_open_uri(gadget_global, uri);
}

static void
action_request(void *data, Evas_Object *obj, void *event_info)
{
   Gadget_Action *ga;
   const char *action = event_info;
   Ecore_Wl2_Display *d = data;
   void *ar_global;
   struct action_route_bind *ar_bind;
   Eina_Hash *h;

   if ((!action) || (!action[0]))
     {
        evas_object_smart_callback_call(obj, "gadget_action", NULL);
        return;
     }
   if (display_actions)
     {
        h = eina_hash_find(display_actions, &d);
        if (h)
          {
             ga = eina_hash_find(h, action);
             if (ga && (!eina_list_data_find(ga->requestors, obj)))
               {
                  ga->requestors = eina_list_append(ga->requestors, obj);
                  evas_object_smart_callback_add(obj, ga->handle, _ar_bind_activate, ga);
               }
             evas_object_smart_callback_call(obj, "gadget_action", ga ? ga->handle : NULL);
             return;
          }
     }
   ar_global = eina_hash_find(ar_globals, &d);
   if (!ar_global)
     {
        evas_object_smart_callback_call(obj, "gadget_action", NULL);
        return;
     }
   ga = calloc(1, sizeof(Gadget_Action));
   ga->d = d;
   ga->requestors = eina_list_append(ga->requestors, obj);
   ga->action = eina_stringshare_add(action);
   if (!display_actions)
     display_actions = eina_hash_string_superfast_new(NULL);
   h = eina_hash_find(display_actions, &d);
   if (!h)
     {
        h = eina_hash_pointer_new((Eina_Free_Cb)_ar_bind_del);
        eina_hash_add(display_actions, &d, h);
     }
   
   ar_bind = action_route_bind_action(ar_global, action);
   action_route_bind_add_listener(ar_bind, &_ar_bind_interface, ga);
   wl_display_roundtrip(ecore_wl2_display_get(d));
}

static void
win_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ecore_Wl2_Display *d = win_display_get(obj);
   eina_hash_list_remove(wins, &d, obj);
   tooltips = eina_list_remove(tooltips, obj);
}

static Evas_Object *
win_add(Evas_Object *win)
{
   Ecore_Wl2_Display *d;
   Eina_Bool first;
   if (!win) return NULL;
   d = win_display_get(win);
   _gadget_init(d);
   if (elm_win_type_get(win) == ELM_WIN_TOOLTIP)
     {
        tooltips = eina_list_append(tooltips, win);
        if (eina_hash_population(gadget_globals))
          {
             struct e_gadget *gadget_global = eina_hash_find(gadget_globals, &d);
             if (gadget_global)
               e_gadget_set_tooltip(gadget_global, ecore_wl2_window_surface_get(elm_win_wl_window_get(win)));
          }
     }
   if (!wins)
     wins = eina_hash_pointer_new(NULL);
   first = !eina_hash_find(wins, &d);
   eina_hash_list_append(wins, &d, win);
   if (first)
     {
        evas_object_smart_callback_add(win, "gadget_action_request", action_request, d);
        evas_object_smart_callback_add(win, "gadget_open_uri", uriopen_request, d);
     }
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, win_del, NULL);
   elm_win_borderless_set(win, 1);
   return win;
}

int
eina_init(void)
{
   int (*_eina_init)(void) = dlsym(RTLD_NEXT, __func__);

   if (wins) return _eina_init();
   if (getenv("RUNNER_DEBUG")) raise(SIGSTOP);
   return _eina_init();
}

Evas_Object *
elm_win_util_dialog_add(Evas_Object *parent, const char *name, const char *title)
{
   Evas_Object *(*_elm_win_util_dialog_add)(Evas_Object *, const char *, const char *) = dlsym(RTLD_NEXT, __func__);

   return win_add(_elm_win_util_dialog_add(parent, name, title));
}

Evas_Object *
elm_win_util_standard_add(const char *name, const char *title)
{
   Evas_Object *(*_elm_win_util_standard_add)(const char *, const char *) = dlsym(RTLD_NEXT, __func__);

   return win_add(_elm_win_util_standard_add(name, title));
}

Evas_Object *
elm_win_add(Evas_Object *parent, const char *name, Elm_Win_Type type)
{
   Evas_Object *(*_elm_win_add)(Evas_Object *,const char*, Elm_Win_Type) = dlsym(RTLD_NEXT, __func__);

   return win_add(_elm_win_add(parent, name, type));
}
