#include "e.h"

E_API int E_EVENT_MANAGER_KEYS_GRAB = -1;

static Eina_List *managers = NULL;

static Ecore_Timer *timer_post_screensaver_lock = NULL;

static void
_e_manager_free(E_Manager *man)
{
   E_FREE_LIST(man->handlers, ecore_event_handler_del);
   managers = eina_list_remove(managers, man);
   free(man);
}

static Eina_Bool
_e_manager_cb_key_down(void *data, int ev_type EINA_UNUSED, Ecore_Event_Key *ev)
{
   E_Manager *man = data;

   if (ev->event_window != man->root)
     {
        E_Client *ec;

        ec = e_client_focused_get();
        /* *block actions when no client is focused (probably something else did a grab here so we'll play nice)
         * *block actions when client menu is up
         * *block actions when event (grab) window isn't comp window
         * *other cases?
         */
        if (!ec) return ECORE_CALLBACK_RENEW;
        if ((ec->border_menu) || (ev->event_window != man->comp->ee_win))
          return ECORE_CALLBACK_PASS_ON;
     }
   if (ev->root_window != man->root) man = e_manager_find_by_root(ev->root_window);
   if (!man) man = eina_list_data_get(managers);
   return !e_bindings_key_down_event_handle(E_BINDING_CONTEXT_MANAGER, E_OBJECT(man), ev);
}

static Eina_Bool
_e_manager_cb_key_up(void *data, int ev_type EINA_UNUSED, Ecore_Event_Key *ev)
{
   E_Manager *man = data;

   if (ev->event_window != man->root) return ECORE_CALLBACK_PASS_ON;
   if (ev->root_window != man->root) man = e_manager_find_by_root(ev->root_window);
   if (!man) man = eina_list_data_get(managers);
   return !e_bindings_key_up_event_handle(E_BINDING_CONTEXT_MANAGER, E_OBJECT(man), ev);
}

static Eina_Bool
_e_manager_cb_timer_post_screensaver_lock(void *data EINA_UNUSED)
{
   e_desklock_show_autolocked();
   timer_post_screensaver_lock = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_manager_cb_screensaver_on(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (e_config->desklock_autolock_screensaver)
     {
        E_FREE_FUNC(timer_post_screensaver_lock, ecore_timer_del);
        if (e_config->desklock_post_screensaver_time <= 1.0)
          e_desklock_show_autolocked();
        else
          timer_post_screensaver_lock = ecore_timer_add
              (e_config->desklock_post_screensaver_time,
              _e_manager_cb_timer_post_screensaver_lock, NULL);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_manager_cb_screensaver_off(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED)
{
   E_FREE_FUNC(timer_post_screensaver_lock, ecore_timer_del);
   return ECORE_CALLBACK_PASS_ON;
}

/* externally accessible functions */
EINTERN int
e_manager_init(void)
{
   E_EVENT_MANAGER_KEYS_GRAB = ecore_event_type_new();
   return 1;
}

EINTERN int
e_manager_shutdown(void)
{
   E_FREE_LIST(managers, e_object_del);
   E_FREE_FUNC(timer_post_screensaver_lock, ecore_timer_del);

   return 1;
}

E_API Eina_List *
e_manager_list(void)
{
   return managers;
}

E_API E_Manager *
e_manager_new(Ecore_Window root, E_Comp *c, int w, int h)
{
   E_Manager *man;

   man = E_OBJECT_ALLOC(E_Manager, E_MANAGER_TYPE, _e_manager_free);
   if (!man) return NULL;
   man->root = root;
   man->num = c->num;
   man->w = w;
   man->h = h;
   man->comp = c;
   
   managers = eina_list_append(managers, man);

   E_LIST_HANDLER_APPEND(man->handlers, ECORE_EVENT_KEY_DOWN,
                         _e_manager_cb_key_down, man);
   E_LIST_HANDLER_APPEND(man->handlers, ECORE_EVENT_KEY_UP,
                         _e_manager_cb_key_up, man);
   E_LIST_HANDLER_APPEND(man->handlers, E_EVENT_SCREENSAVER_ON,
                         _e_manager_cb_screensaver_on, man);
   E_LIST_HANDLER_APPEND(man->handlers, E_EVENT_SCREENSAVER_OFF,
                         _e_manager_cb_screensaver_off, man);

   return man;
}

E_API void
e_manager_resize(E_Manager *man, int w, int h)
{
   E_OBJECT_CHECK(man);
   E_OBJECT_TYPE_CHECK(man, E_MANAGER_TYPE);
   man->w = w;
   man->h = h;
   ecore_evas_resize(man->comp->ee, w, h);
}

E_API E_Manager *
e_manager_current_get(void)
{
   Eina_List *l;
   E_Manager *man;
   int x, y;

   if (!managers) return NULL;
   if (eina_list_count(managers) == 1)
     return eina_list_data_get(managers);
   EINA_LIST_FOREACH(managers, l, man)
     {
        ecore_evas_pointer_xy_get(man->comp->ee, &x, &y);
        if (x == -1 && y == -1)
          continue;
        if (E_INSIDE(x, y, man->x, man->y, man->w, man->h))
          return man;
     }
   return eina_list_data_get(managers);
}

E_API E_Manager *
e_manager_number_get(int num)
{
   Eina_List *l;
   E_Manager *man;

   if (!managers) return NULL;
   EINA_LIST_FOREACH(managers, l, man)
     {
        if (man->num == num)
          return man;
     }
   return NULL;
}

E_API void
e_managers_keys_grab(void)
{
   Eina_List *l;
   E_Manager *man;

   EINA_LIST_FOREACH(managers, l, man)
     {
        if (man->root)
          e_bindings_key_grab(E_BINDING_CONTEXT_ANY, man->root);
     }
   ecore_event_add(E_EVENT_MANAGER_KEYS_GRAB, NULL, NULL, NULL);
}

E_API void
e_managers_keys_ungrab(void)
{
   Eina_List *l;
   E_Manager *man;

   EINA_LIST_FOREACH(managers, l, man)
     {
        if (man->root)
          e_bindings_key_ungrab(E_BINDING_CONTEXT_ANY, man->root);
     }
}
