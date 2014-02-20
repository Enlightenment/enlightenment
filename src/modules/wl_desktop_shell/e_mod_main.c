#include "e.h"
#include "e_comp_wl.h"
#include "e_surface.h"
#include "e_mod_main.h"
#include "e_desktop_shell_protocol.h"

/* shell function prototypes */
static void _e_wl_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_wl_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_wl_shell_cb_bind_desktop(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_wl_shell_cb_ping(E_Wayland_Surface *ews, unsigned int serial);
static void _e_wl_shell_cb_pointer_focus(struct wl_listener *listener EINA_UNUSED, void *data);

static void _e_wl_shell_grab_start(E_Wayland_Shell_Grab *grab, E_Wayland_Shell_Surface *ewss, struct wl_pointer *pointer, const struct wl_pointer_grab_interface *interface, enum e_desktop_shell_cursor cursor);
static void _e_wl_shell_grab_end(E_Wayland_Shell_Grab *ewsg);
static void _e_wl_shell_grab_cb_surface_destroy(struct wl_listener *listener, void *data EINA_UNUSED);

/* shell interface prototypes */
static void _e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource);

/* desktop shell function prototypes */
static void _e_wl_desktop_shell_cb_unbind(struct wl_resource *resource);

/* desktop shell interface prototypes */
static void _e_wl_desktop_shell_cb_background_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED);
static void _e_wl_desktop_shell_cb_panel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED);
static void _e_wl_desktop_shell_cb_lock_surface_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED);
static void _e_wl_desktop_shell_cb_unlock(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED);
static void _e_wl_desktop_shell_cb_shell_grab_surface_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface_resource);
static void _e_wl_desktop_shell_cb_ready(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED);

/* shell surface function prototypes */
static E_Wayland_Shell_Surface *_e_wl_shell_shell_surface_create(void *shell, E_Wayland_Surface *ews, const void *client EINA_UNUSED);
static void _e_wl_shell_shell_surface_create_toplevel(E_Wayland_Surface *ews);
static void _e_wl_shell_shell_surface_create_popup(E_Wayland_Surface *ews);
static void _e_wl_shell_shell_surface_destroy(struct wl_resource *resource);
static void _e_wl_shell_shell_surface_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_wl_shell_shell_surface_map(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_wl_shell_shell_surface_unmap(E_Wayland_Surface *ews);
static void _e_wl_shell_shell_surface_type_set(E_Wayland_Shell_Surface *ewss);
static void _e_wl_shell_shell_surface_type_reset(E_Wayland_Shell_Surface *ewss);
static void _e_wl_shell_shell_surface_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static int _e_wl_shell_shell_surface_cb_ping_timeout(void *data);

static void _e_wl_shell_render_post(void *data, Evas *e EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_key_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static Eina_Bool _e_wl_shell_shell_surface_cb_client_prop(void *data, int type, void *event);
static void _e_wl_shell_shell_surface_cb_ec_hook_focus_set(void *data, E_Client *ec);
static void _e_wl_shell_shell_surface_cb_ec_hook_focus_unset(void *data, E_Client *ec);
static void _e_wl_shell_shell_surface_cb_ec_hook_move_end(void *data, E_Client *ec);
static void _e_wl_shell_shell_surface_cb_ec_hook_resize_update(void *data, E_Client *ec);
static void _e_wl_shell_shell_surface_cb_ec_hook_resize_end(void *data, E_Client *ec);
static void _e_wl_shell_surface_cb_smart_client_resize(void *data, Evas_Object *obj, void *event_info);

static void _e_wl_shell_mouse_down_helper(E_Client *ec, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev, Eina_Bool move);

/* shell surface interface prototypes */
static void _e_wl_shell_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int serial);
static void _e_wl_shell_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial);
static void _e_wl_shell_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, unsigned int edges);
static void _e_wl_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_wl_shell_shell_surface_cb_transient_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource, int x, int y, unsigned int flags);
static void _e_wl_shell_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int method EINA_UNUSED, unsigned int framerate EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_popup_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, struct wl_resource *parent_resource, int x, int y, unsigned int flags EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title);
static void _e_wl_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas);

/* shell move_grab interface prototypes */
static void _e_wl_shell_move_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_move_grab_cb_motion(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, wl_fixed_t x, wl_fixed_t y);
static void _e_wl_shell_move_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button EINA_UNUSED, unsigned int state);

/* shell resize_grab interface prototypes */
static void _e_wl_shell_resize_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_resize_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_resize_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button EINA_UNUSED, unsigned int state);

/* shell popup_grab interface prototypes */
static void _e_wl_shell_popup_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y);
static void _e_wl_shell_popup_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_popup_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);

/* shell popup function prototypes */
static void _e_wl_shell_popup_grab_end(struct wl_pointer *pointer);

/* shell busy_grab interface prototypes */
static void _e_wl_shell_busy_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_busy_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
static void _e_wl_shell_busy_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button, unsigned int state);

/* local wayland interfaces */
static const struct wl_shell_interface _e_shell_interface = 
{
   _e_wl_shell_cb_shell_surface_get
};

static const struct e_desktop_shell_interface _e_desktop_shell_interface = 
{
   _e_wl_desktop_shell_cb_background_set,
   _e_wl_desktop_shell_cb_panel_set,
   _e_wl_desktop_shell_cb_lock_surface_set,
   _e_wl_desktop_shell_cb_unlock,
   _e_wl_desktop_shell_cb_shell_grab_surface_set,
   _e_wl_desktop_shell_cb_ready
};

static const struct wl_shell_surface_interface _e_shell_surface_interface = 
{
   _e_wl_shell_shell_surface_cb_pong,
   _e_wl_shell_shell_surface_cb_move,
   _e_wl_shell_shell_surface_cb_resize,
   _e_wl_shell_shell_surface_cb_toplevel_set,
   _e_wl_shell_shell_surface_cb_transient_set,
   _e_wl_shell_shell_surface_cb_fullscreen_set,
   _e_wl_shell_shell_surface_cb_popup_set,
   _e_wl_shell_shell_surface_cb_maximized_set,
   _e_wl_shell_shell_surface_cb_title_set,
   _e_wl_shell_shell_surface_cb_class_set
};

static const struct wl_pointer_grab_interface _e_move_grab_interface = 
{
   _e_wl_shell_move_grab_cb_focus,
   _e_wl_shell_move_grab_cb_motion,
   _e_wl_shell_move_grab_cb_button,
};

static const struct wl_pointer_grab_interface _e_resize_grab_interface = 
{
   _e_wl_shell_resize_grab_cb_focus,
   _e_wl_shell_resize_grab_cb_motion,
   _e_wl_shell_resize_grab_cb_button,
};

static const struct wl_pointer_grab_interface _e_popup_grab_interface = 
{
   _e_wl_shell_popup_grab_cb_focus,
   _e_wl_shell_popup_grab_cb_motion,
   _e_wl_shell_popup_grab_cb_button,
};

static const struct wl_pointer_grab_interface _e_busy_grab_interface = 
{
   _e_wl_shell_busy_grab_cb_focus,
   _e_wl_shell_busy_grab_cb_motion,
   _e_wl_shell_busy_grab_cb_button,
};

/* local variables */

static Eina_List *ec_hooks = NULL;
static Ecore_Event_Handler *prop_handler = NULL;

/* external variables */

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop_Shell" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_global *gshell = NULL;
   E_Wayland_Input *input = NULL;
   const Eina_List *l = NULL;
   E_Comp *comp;

   /* test for valid compositor */
   if (!_e_wl_comp) return NULL;

   /* try to allocate space for the shell structure */
   if (!(shell = E_NEW(E_Wayland_Desktop_Shell, 1)))
     return NULL;

   /* tell the shell what compositor to use */
   shell->compositor = _e_wl_comp;

   /* setup shell destroy callback */
   shell->wl.destroy_listener.notify = _e_wl_shell_cb_destroy;
   wl_signal_add(&_e_wl_comp->signals.destroy, &shell->wl.destroy_listener);

   /* setup compositor ping callback */
   _e_wl_comp->ping_cb = _e_wl_shell_cb_ping;

   /* setup compositor shell interface functions */
   _e_wl_comp->shell_interface.shell = shell;
   _e_wl_comp->shell_interface.shell_surface_create = 
     _e_wl_shell_shell_surface_create;

   /* try to add this shell to the display's global list */
   if (!(gshell = 
         wl_global_create(_e_wl_comp->wl.display, &wl_shell_interface, 
                          1, shell, _e_wl_shell_cb_bind)))
     goto err;

   /* try to add the desktop shell interface to the display's global list */
   if (!wl_global_create(_e_wl_comp->wl.display, &e_desktop_shell_interface, 
                         2, shell, _e_wl_shell_cb_bind_desktop))
     goto err;

   /* for each input, we need to create a pointer focus listener */
   EINA_LIST_FOREACH(_e_wl_comp->seats, l, input)
     {
        struct wl_listener *lst;

        /* skip this input if it has no pointer */
        if (!input->has_pointer) continue;

        /* allocate space for the listener */
        lst = malloc(sizeof(*lst));

        /* set the listener callback function */
        lst->notify = _e_wl_shell_cb_pointer_focus;

        /* add this listener to the pointer focus signal */
        wl_signal_add(&input->wl.seat.pointer->focus_signal, lst);
     }

   ec_hooks = eina_list_append(ec_hooks, 
                      e_client_hook_add(E_CLIENT_HOOK_FOCUS_SET, 
                                        _e_wl_shell_shell_surface_cb_ec_hook_focus_set, NULL));
   ec_hooks = eina_list_append(ec_hooks, 
                      e_client_hook_add(E_CLIENT_HOOK_FOCUS_UNSET, 
                                        _e_wl_shell_shell_surface_cb_ec_hook_focus_unset, NULL));

   ec_hooks = eina_list_append(ec_hooks, 
                      e_client_hook_add(E_CLIENT_HOOK_MOVE_END, 
                                        _e_wl_shell_shell_surface_cb_ec_hook_move_end, NULL));
   ec_hooks = eina_list_append(ec_hooks, 
                      e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, 
                                        _e_wl_shell_shell_surface_cb_ec_hook_resize_end, NULL));
   ec_hooks = eina_list_append(ec_hooks, 
                      e_client_hook_add(E_CLIENT_HOOK_RESIZE_UPDATE, 
                                        _e_wl_shell_shell_surface_cb_ec_hook_resize_update, NULL));

   prop_handler = ecore_event_handler_add(E_EVENT_CLIENT_PROPERTY, _e_wl_shell_shell_surface_cb_client_prop, NULL);
   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     evas_event_callback_add(comp->evas, EVAS_CALLBACK_RENDER_POST,
                             _e_wl_shell_render_post, NULL);

   return m;

err:
   /* remove previously added shell global */
   if (gshell) wl_global_destroy(gshell);

   /* reset compositor shell interface */
   _e_wl_comp->shell_interface.shell = NULL;

   /* free the allocated shell structure */
   E_FREE(shell);

   return NULL;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   const Eina_List *l;
   E_Comp *comp;

   E_FREE_LIST(ec_hooks, e_client_hook_del);
   E_FREE_FUNC(prop_handler, ecore_event_handler_del);
   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     evas_event_callback_del_full(comp->evas, EVAS_CALLBACK_RENDER_POST,
                             _e_wl_shell_render_post, NULL);
   /* nothing to do here as shell will get the destroy callback from 
    * the compositor and we can cleanup there */
   return 1;
}

/* shell functions */
static void 
_e_wl_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Desktop_Shell *shell = NULL;

   /* try to get the shell from the listener */
   shell = 
     container_of(listener, E_Wayland_Desktop_Shell, wl.destroy_listener);

   /* free the allocated shell structure */
   E_FREE(shell);
}

static void 
_e_wl_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_resource *res = NULL;

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the shell to the client */
   res = wl_resource_create(client, &wl_shell_interface, 1, id);
   if (res)
     {
        struct wl_resource *dres = NULL;

        wl_resource_set_implementation(res, &_e_shell_interface, shell, NULL);

        dres = wl_resource_create(client, &e_desktop_shell_interface, -1, 0);
        if (dres)
          {
             wl_resource_set_implementation(dres, &_e_desktop_shell_interface, 
                                            shell, _e_wl_desktop_shell_cb_unbind);

             shell->wl.resource = dres;
          }
     }
}

static void 
_e_wl_shell_cb_bind_desktop(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_resource *res = NULL;

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the shell to the client */
   res = wl_resource_create(client, &e_desktop_shell_interface, 2, id);
   if (res)
     {
        wl_resource_set_implementation(res, &_e_desktop_shell_interface, 
                                       shell, _e_wl_desktop_shell_cb_unbind);
        shell->wl.resource = res;
     }
}

static void 
_e_wl_shell_cb_ping(E_Wayland_Surface *ews, unsigned int serial)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Desktop_Shell *shell = NULL;

   if (!ews) return;

   /* try to cast to our shell surface */
   if (!(ewss = ews->shell_surface)) return;

   shell = (E_Wayland_Desktop_Shell *)ewss->shell;

   if ((ewss->surface) && (ewss->surface == shell->grab_surface))
     return;

   /* if we do not have a ping timer yet, create one */
   if (!ewss->ping_timer)
     {
        E_Wayland_Ping_Timer *tmr = NULL;

        /* try to allocate space for a new ping timer */
        if (!(tmr = E_NEW(E_Wayland_Ping_Timer, 1)))
          return;

        tmr->serial = serial;

        /* add a timer to the event loop */
        tmr->source = 
          wl_event_loop_add_timer(_e_wl_comp->wl.loop, 
                                  _e_wl_shell_shell_surface_cb_ping_timeout,
                                  ewss);

        /* update timer source interval */
        wl_event_source_timer_update(tmr->source, 
                                     e_config->ping_clients_interval * (1000 / 8));

        ewss->ping_timer = tmr;

        /* send the ping to the shell surface */
        wl_shell_surface_send_ping(ewss->wl.resource, serial);
     }
}

static void 
_e_wl_shell_cb_pointer_focus(struct wl_listener *listener EINA_UNUSED, void *data)
{
   struct wl_pointer *ptr;
   E_Wayland_Surface *ews = NULL;

   if (!(ptr = data)) return;

   if (!ptr->focus) return;

   /* try to cast the current pointer focus to a wayland surface */
   if (!(ews = wl_resource_get_user_data(ptr->focus)))
     return;

   /* if this surface has a shell_surface and it is not active, then we 
    * should set the busy cursor on it */
   if ((ews->mapped) && (ews->shell_surface) && (!ews->shell_surface->active))
     {
        E_Wayland_Desktop_Shell *shell;

        /* set busy cursor */
        shell = ews->shell_surface->shell;
        e_desktop_shell_send_grab_cursor(shell->wl.resource, 
                                         E_DESKTOP_SHELL_CURSOR_BUSY);
     }
   else
     {
        unsigned int serial = 0;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        /* ping the surface */
        _e_wl_shell_cb_ping(ews, serial);
     }
}

static void 
_e_wl_shell_grab_start(E_Wayland_Shell_Grab *grab, E_Wayland_Shell_Surface *ewss, struct wl_pointer *pointer, const struct wl_pointer_grab_interface *interface, enum e_desktop_shell_cursor cursor)
{
   E_Wayland_Desktop_Shell *shell;

   /* safety check */
   if ((!grab) || (!ewss)) return;

   shell = (E_Wayland_Desktop_Shell *)ewss->shell;

   /* end any popup grabs */
   _e_wl_shell_popup_grab_end(pointer);

   /* setup grab properties */
   grab->grab.interface = interface;
   grab->shell_surface = ewss;
   grab->shell_surface_destroy.notify = _e_wl_shell_grab_cb_surface_destroy;

   /* add a listener in case this surface gets destroyed */
   wl_signal_add(&ewss->wl.destroy_signal, &grab->shell_surface_destroy);

   /* start the pointer grab */
   wl_pointer_start_grab(pointer, &grab->grab);

   /* set cursor */
   if (shell)
     e_desktop_shell_send_grab_cursor(shell->wl.resource, cursor);

   /* tell the pointer which surface is focused */
   wl_pointer_set_focus(pointer, ewss->surface->wl.surface, 0, 0);
}

static void 
_e_wl_shell_grab_end(E_Wayland_Shell_Grab *ewsg)
{
   /* safety */
   if (!ewsg) return;

   /* remove the destroy listener */
   if (ewsg->shell_surface)
     wl_list_remove(&ewsg->shell_surface_destroy.link);

   /* end the grab */
   wl_pointer_end_grab(ewsg->grab.pointer);
}

static void 
_e_wl_shell_grab_cb_surface_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Shell_Grab *ewsg = NULL;

   /* try to get the grab from the listener */
   ewsg = container_of(listener, E_Wayland_Shell_Grab, shell_surface_destroy);
   if (!ewsg) return;

   /* set the grab surface to null */
   ewsg->shell_surface = NULL;
}

/* shell interface functions */
static void 
_e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Desktop_Shell *shell = NULL;

   /* try to cast the surface resource to our structure */
   if (!(ews = wl_resource_get_user_data(surface_resource)))
     return;

   shell = wl_resource_get_user_data(resource);

   /* check if this surface already has a shell surface */
   if ((ews->configure) && 
       (ews->configure == _e_wl_shell_shell_surface_configure))
     {
        wl_resource_post_error(surface_resource, 
                               WL_DISPLAY_ERROR_INVALID_OBJECT, 
                               "Shell surface already requested for surface");
        return;
     }

   /* try to create a shell surface for this surface */
   if (!(ewss = _e_wl_shell_shell_surface_create(shell, ews, NULL)))
     {
        wl_resource_post_no_memory(surface_resource);
        return;
     }

   ewss->wl.resource = 
     wl_resource_create(client, &wl_shell_surface_interface, 1, id);
   wl_resource_set_implementation(ewss->wl.resource, 
                                  &_e_shell_surface_interface, ewss, 
                                  _e_wl_shell_shell_surface_destroy);
}

/* desktop shell functions */
static void 
_e_wl_desktop_shell_cb_unbind(struct wl_resource *resource)
{
   E_Wayland_Desktop_Shell *shell = NULL;

   shell = wl_resource_get_user_data(resource);
   shell->wl.resource = NULL;
}

/* desktop shell interface functions */
static void 
_e_wl_desktop_shell_cb_background_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED)
{

}

static void 
_e_wl_desktop_shell_cb_panel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED)
{

}

static void 
_e_wl_desktop_shell_cb_lock_surface_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *surface_resource EINA_UNUSED)
{

}

static void 
_e_wl_desktop_shell_cb_unlock(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED)
{

}

static void 
_e_wl_desktop_shell_cb_shell_grab_surface_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface_resource)
{
   E_Wayland_Desktop_Shell *shell = NULL;

   /* try to get the shell */
   if (!(shell = wl_resource_get_user_data(resource)))
     return;

   shell->grab_surface = wl_resource_get_user_data(surface_resource);
}

static void 
_e_wl_desktop_shell_cb_ready(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED)
{

}

/* shell surface functions */
static E_Wayland_Shell_Surface *
_e_wl_shell_shell_surface_create(void *shell, E_Wayland_Surface *ews, const void *client EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   if (!ews) return NULL;

   /* try to allocate space for our shell surface structure */
   if (!(ewss = E_NEW(E_Wayland_Shell_Surface, 1)))
     return NULL;

   /* set some function pointers on the surface */
   ews->configure = _e_wl_shell_shell_surface_configure;
   ews->map = _e_wl_shell_shell_surface_map;
   ews->unmap = _e_wl_shell_shell_surface_unmap;

   /* set some properties on the shell surface */
   ewss->shell = (E_Wayland_Desktop_Shell *)shell;
   ewss->surface = ews;
   ews->shell_surface = ewss;

   /* setup shell surface destroy */
   wl_signal_init(&ewss->wl.destroy_signal);
   ewss->wl.surface_destroy.notify = _e_wl_shell_shell_surface_cb_destroy;
   wl_signal_add(&ews->wl.destroy_signal, &ewss->wl.surface_destroy);

   /* wl_list_init(&ewss->wl.surface_destroy.link); */

   wl_list_init(&ewss->wl.link);

   /* set some defaults on this shell surface */
   ewss->active = EINA_TRUE;
   ewss->type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;

   return ewss;
}

static void 
_e_wl_shell_shell_surface_create_toplevel(E_Wayland_Surface *ews)
{
   E_Comp *comp;

   /* get the current container */
   comp = e_util_comp_current_get();

   /* create the e client for this surface */
   ews->ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ews->pixmap));
   if (!ews->ec)
     ews->ec = e_client_new(comp, ews->pixmap, 1, 0);
   e_pixmap_ref(ews->pixmap);
   ews->ec->argb = 1;
   ews->ec->no_shape_cut = 1; // specify no input shape cutting for this client
   ews->ec->borderless = !ews->ec->internal;
   ews->ec->lock_border = 1;
   ews->ec->border.changed = ews->ec->changes.border = !ews->ec->borderless;
   ews->ec->comp_data = (E_Comp_Client_Data*)ews;
   ews->ec->icccm.title = eina_stringshare_ref(ews->shell_surface->title);
   ews->ec->icccm.class = eina_stringshare_ref(ews->shell_surface->clas);
   ews->ec->changes.icon = !!ews->ec->icccm.class;
   EC_CHANGED(ews->ec);

   /* hook object callbacks */
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_OUT,
                                  _e_wl_shell_shell_surface_cb_mouse_out, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_MOVE,
                                  _e_wl_shell_shell_surface_cb_mouse_move, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_UP,
                                  _e_wl_shell_shell_surface_cb_mouse_up, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_DOWN,
                                  _e_wl_shell_shell_surface_cb_mouse_down, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _e_wl_shell_shell_surface_cb_mouse_wheel, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_KEY_UP,
                                  _e_wl_shell_shell_surface_cb_key_up, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_KEY_DOWN,
                                  _e_wl_shell_shell_surface_cb_key_down, ews);
   evas_object_smart_callback_add(ews->ec->frame, "client_resize",
                                  _e_wl_shell_surface_cb_smart_client_resize, ews);

   ews->ec->client.w = ews->geometry.w;
   ews->ec->client.h = ews->geometry.h;

   ews->ec->visible = 1;
   evas_object_show(ews->ec->frame);

   evas_object_geometry_set(ews->ec->frame, ews->geometry.x, ews->geometry.y, 
                        ews->geometry.w, ews->geometry.h);

   ews->mapped = EINA_TRUE;
}

static void 
_e_wl_shell_shell_surface_create_popup(E_Wayland_Surface *ews)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   struct wl_seat *seat;
   //char opts[PATH_MAX];
   E_Comp *comp;

   /* try to get the shell surface */
   if (!(ewss = ews->shell_surface)) return;
   /* get the current comp */
   if (ewss->parent)
     comp = ewss->parent->ec->comp;
   else
     comp = e_util_comp_current_get();

   /* create the e client for this surface */
   ews->ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ews->pixmap));
   if (!ews->ec)
     ews->ec = e_client_new(comp, ews->pixmap, 1, 1);
   e_pixmap_ref(ews->pixmap);
   ews->ec->argb = 1;
   ews->ec->no_shape_cut = 1; // specify no input shape cutting for this client
   ews->ec->borderless = !ews->ec->internal;
   ews->ec->lock_border = 1;
   ews->ec->border.changed = ews->ec->changes.border = !ews->ec->borderless;
   ews->ec->comp_data = (E_Comp_Client_Data*)ews;
   ews->ec->icccm.title = eina_stringshare_ref(ewss->title);
   ews->ec->icccm.class = eina_stringshare_ref(ewss->clas);
   ews->ec->changes.icon = !!ews->ec->icccm.class;
   EC_CHANGED(ews->ec);

   /* hook object callbacks */
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_OUT,
                                  _e_wl_shell_shell_surface_cb_mouse_out, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_MOVE,
                                  _e_wl_shell_shell_surface_cb_mouse_move, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_UP,
                                  _e_wl_shell_shell_surface_cb_mouse_up, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_DOWN,
                                  _e_wl_shell_shell_surface_cb_mouse_down, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _e_wl_shell_shell_surface_cb_mouse_wheel, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_KEY_UP,
                                  _e_wl_shell_shell_surface_cb_key_up, ews);
   evas_object_event_callback_add(ews->ec->frame, EVAS_CALLBACK_KEY_DOWN,
                                  _e_wl_shell_shell_surface_cb_key_down, ews);
   evas_object_smart_callback_add(ews->ec->frame, "client_resize",
                                  _e_wl_shell_surface_cb_smart_client_resize, ews);

   ews->ec->client.w = ews->geometry.w;
   ews->ec->client.h = ews->geometry.h;

   ews->ec->visible = 1;
   evas_object_show(ews->ec->frame);

   evas_object_geometry_set(ews->ec->frame, ews->geometry.x, ews->geometry.y, 
                        ews->geometry.w, ews->geometry.h);


   ews->mapped = EINA_TRUE;

   /* set popup properties */
   ewss->popup.grab.interface = &_e_popup_grab_interface;
   ewss->popup.up = EINA_FALSE;

   if (ewss->parent)
     {
        /* TODO */
        /* ewss->popup.parent_destroy.notify = ; */

        /* add a signal callback to be raised when the parent gets destroyed */
        /* wl_signal_add(&ewss->parent->wl.surface.resource.destroy_signal,  */
        /*               &ewss->popup.parent_destroy); */
     }

   seat = ewss->popup.seat;
   if (seat->pointer->grab_serial == ewss->popup.serial)
     wl_pointer_start_grab(seat->pointer, &ewss->popup.grab);
   else
     wl_shell_surface_send_popup_done(ewss->wl.resource);
}

static void 
_e_wl_shell_shell_surface_destroy(struct wl_resource *resource)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Ping_Timer *tmr = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* if we have a popup grab, end it */
   if (ewss->popup.grab.pointer)
     wl_pointer_end_grab(ewss->popup.grab.pointer);

//   wl_list_remove(&ewss->wl.surface_destroy.link);
//   if (ewss->surface) ewss->surface->configure = NULL;

   /* try to cast the ping timer */
   if ((tmr = (E_Wayland_Ping_Timer *)ewss->ping_timer))
     {
        if (tmr->source) wl_event_source_remove(tmr->source);

        /* free the allocated space for ping timer */
        E_FREE(tmr);
        ewss->ping_timer = NULL;
     }

   wl_list_remove(&ewss->wl.link);

   /* try to free our allocated structure */
   /* E_FREE(ewss); */
}

static void 
_e_wl_shell_shell_surface_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   Eina_Bool changed_type = EINA_FALSE;

   ewss = ews->shell_surface;

   /* FIXME */
   /* if ((!ews->mapped) &&  */
   /*     (!wl_list_empty(&ewss->popup.grab_link)) */
   /*   { */
        
   /*   } */

   if (w == 0) return;

   /* if ((ews->configure == _e_wl_shell_shell_surface_configure)) */
   /*   { */
   /*      if (!(ewss = ews->shell_surface)) return; */
   /*   } */
   /* else  */
   /*   return; */

   /* handle shell_surface type change */
   if ((ewss->next_type != E_WAYLAND_SHELL_SURFACE_TYPE_NONE) && 
       (ewss->type != ewss->next_type))
     {
        /* set the shell surface type */
        _e_wl_shell_shell_surface_type_set(ewss);

        changed_type = EINA_TRUE;
     }

   /* if this surface is not mapped yet, then we need to map it */
   if (!ews->mapped)
     {
        /* if the surface has a map function, call that. Else we use a 
         * default one for this shell */
        if (ews->map) ews->map(ews, x, y, w, h);
        else _e_wl_shell_shell_surface_map(ews, x, y, w, h);
     }
   else if ((changed_type) || (x != 0) || (y != 0) || 
            (ews->geometry.w != w) || (ews->geometry.h != h))
     {
        if (ews->ec && e_client_util_resizing_get(ews->ec)) return;
        ews->geometry.w = w;
        ews->geometry.h = h;
        ews->geometry.changed = EINA_TRUE;
        if (ews->ec)
          {
             if ((ews->geometry.x != x) || (ews->geometry.y != y))
               {
                  ews->ec->client.x = ews->geometry.x = x;
                  ews->ec->client.y = ews->geometry.y = y;
                  e_comp_object_frame_xy_adjust(ews->ec->frame, x, y, &ews->ec->x, &ews->ec->y);
                  ews->ec->changes.pos = 1;
               }
             ews->ec->client.w = w;
             ews->ec->client.h = h;
             e_comp_object_frame_wh_adjust(ews->ec->frame, w, h, &ews->ec->w, &ews->ec->h);
             ews->ec->changes.size = 1;
             EC_CHANGED(ews->ec);
          }
     }
}

static void 
_e_wl_shell_shell_surface_map(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   /* safety check */
   if (!ews) return;

   /* update surface geometry */
   ews->geometry.w = w;
   ews->geometry.h = h;
   ews->geometry.changed = EINA_TRUE;

   /* starting position */
   switch (ews->shell_surface->type)
     {
      case E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL:
        ews->geometry.x = x;
        ews->geometry.y = y;
        ews->geometry.changed = EINA_TRUE;
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN:
        /* center */
        /* map fullscreen */
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED:
        /* maximize */
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_POPUP:
        ews->geometry.x = x;
        ews->geometry.y = y;
        ews->geometry.changed = EINA_TRUE;
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_NONE:
        ews->geometry.x += x;
        ews->geometry.y += y;
        ews->geometry.changed = EINA_TRUE;
        break;
      default:
        break;
     }

   /* stacking */

   /* activate */
   switch (ews->shell_surface->type)
     {
      case E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL:
        _e_wl_shell_shell_surface_create_toplevel(ews);
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_POPUP:
        _e_wl_shell_shell_surface_create_popup(ews);
        break;
      default: return;
     }
   if (ews->ec && ews->ec->icccm.title)
     e_comp_object_frame_title_set(ews->ec->frame, ews->ec->icccm.title);
}

static void 
_e_wl_shell_shell_surface_unmap(E_Wayland_Surface *ews)
{
   Eina_List *l = NULL;
   E_Wayland_Input *input;

   if (!ews) return;

   EINA_LIST_FOREACH(_e_wl_comp->seats, l, input)
     {
        if ((input->wl.seat.keyboard) && 
            (input->wl.seat.keyboard->focus == ews->wl.surface))
          wl_keyboard_set_focus(input->wl.seat.keyboard, NULL);
        if ((input->wl.seat.pointer) && 
            (input->wl.seat.pointer->focus == ews->wl.surface))
          wl_pointer_set_focus(input->wl.seat.pointer, NULL, 0, 0);
     }

   if (ews->ec)
     {
        //evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_MOUSE_OUT,
                                       //_e_wl_shell_shell_surface_cb_mouse_out, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_MOUSE_MOVE,
                                       _e_wl_shell_shell_surface_cb_mouse_move, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_MOUSE_UP,
                                       _e_wl_shell_shell_surface_cb_mouse_up, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_MOUSE_DOWN,
                                       _e_wl_shell_shell_surface_cb_mouse_down, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_MOUSE_WHEEL,
                                       _e_wl_shell_shell_surface_cb_mouse_wheel, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_KEY_UP,
                                       _e_wl_shell_shell_surface_cb_key_up, ews);
        evas_object_event_callback_del_full(ews->ec->frame, EVAS_CALLBACK_KEY_DOWN,
                                       _e_wl_shell_shell_surface_cb_key_down, ews);
        evas_object_smart_callback_del_full(ews->ec->frame, "client_resize",
                                       _e_wl_shell_surface_cb_smart_client_resize, ews);


        if (evas_object_visible_get(ews->ec->frame))
          {
             /* surface probably has render updates pending:
              * - check ourselves before we wreck ourselves
              * - copy image
              * - re-render
              */
             e_pixmap_image_clear(ews->pixmap, 0);
             e_pixmap_dirty(ews->pixmap);
             if (e_pixmap_refresh(ews->pixmap))
               {
                  e_comp_object_damage(ews->ec->frame, 0, 0, ews->ec->w, ews->ec->h);
                  e_comp_object_dirty(ews->ec->frame);
                  e_comp_object_render(ews->ec->frame);
               }
             e_comp_object_render_update_del(ews->ec->frame);
             evas_object_pass_events_set(ews->ec->frame, 1);
             evas_object_hide(ews->ec->frame);
          }
        e_object_del(E_OBJECT(ews->ec));
     }

   ews->mapped = EINA_FALSE;
}

static void 
_e_wl_shell_shell_surface_type_set(E_Wayland_Shell_Surface *ewss)
{
   /* safety check */
   if (!ewss) return;

   /* reset the shell surface type */
   _e_wl_shell_shell_surface_type_reset(ewss);

   ewss->type = ewss->next_type;
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;

   switch (ewss->type)
     {
      case E_WAYLAND_SHELL_SURFACE_TYPE_TRANSIENT:
        ewss->surface->geometry.x = 
          ewss->parent->geometry.x + ewss->transient.x;
        ewss->surface->geometry.y = 
          ewss->parent->geometry.y + ewss->transient.y;
        break;
        /* record the current geometry so we can restore it */
      case E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN:
      case E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED:
        ewss->saved.x = ewss->surface->geometry.x;
        ewss->saved.y = ewss->surface->geometry.y;
        ewss->saved.w = ewss->surface->geometry.w;
        ewss->saved.h = ewss->surface->geometry.h;
        ewss->saved.valid = EINA_TRUE;
        break;
      default:
        break;
     }
}

static void 
_e_wl_shell_shell_surface_type_reset(E_Wayland_Shell_Surface *ewss)
{
   /* safety check */
   if (!ewss) return;

   switch (ewss->type)
     {
      case E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN:
        /* unfullscreen the client */
        if (ewss->surface->ec)
          e_client_unfullscreen(ewss->surface->ec);

        /* restore the saved geometry */
        ewss->surface->geometry.x = ewss->saved.x;
        ewss->surface->geometry.y = ewss->saved.y;
        ewss->surface->geometry.w = ewss->saved.w;
        ewss->surface->geometry.h = ewss->saved.h;
        ewss->surface->geometry.changed = EINA_TRUE;
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED:
        /* unmaximize the client */
        if (ewss->surface->ec)
          e_client_unmaximize(ewss->surface->ec, 
                              (e_config->maximize_policy & E_MAXIMIZE_TYPE) | 
                              E_MAXIMIZE_BOTH);

        /* restore the saved geometry */
        ewss->surface->geometry.x = ewss->saved.x;
        ewss->surface->geometry.y = ewss->saved.y;
        ewss->surface->geometry.w = ewss->saved.w;
        ewss->surface->geometry.h = ewss->saved.h;
        ewss->surface->geometry.changed = EINA_TRUE;
        break;
      default:
        break;
     }

   /* reset the current type to none */
   ewss->type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;
}

static void 
_e_wl_shell_shell_surface_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to get the shell surface from the listener */
   ewss = container_of(listener, E_Wayland_Shell_Surface, wl.surface_destroy);
   if (!ewss) return;

   if (ewss->wl.resource)
     wl_resource_destroy(ewss->wl.resource);
   else
     {
        E_Wayland_Ping_Timer *tmr = NULL;

        wl_signal_emit(&ewss->wl.destroy_signal, ewss);

        /* TODO: FIXME: Popup->grab_link */

        wl_list_remove(&ewss->wl.surface_destroy.link);
        ewss->surface->configure = NULL;

        /* destroy the ping timer */
        if ((tmr = (E_Wayland_Ping_Timer *)ewss->ping_timer))
          {
             if (tmr->source) wl_event_source_remove(tmr->source);
             E_FREE(tmr);
             ewss->ping_timer = NULL;
          }

        wl_list_remove(&ewss->wl.link);
        eina_stringshare_del(ewss->clas);
        eina_stringshare_del(ewss->title);
        free(ewss);
     }
}

static int 
_e_wl_shell_shell_surface_cb_ping_timeout(void *data)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Shell_Grab *grab = NULL;

   /* try to cast to our shell surface structure */
   if (!(ewss = data)) return 1;

   ewss->active = EINA_FALSE;

   /* this can be NULL if there's a long enough timeout */
   if (!ewss->surface->input) return 1;

   /* TODO: this should loop inputs */

   /* try to allocate space for our grab structure */
   if (!(grab = E_NEW(E_Wayland_Shell_Grab, 1))) return 1;

   /* set grab properties */
   grab->x = _e_wl_comp->input->wl.seat.pointer->grab_x;
   grab->y = _e_wl_comp->input->wl.seat.pointer->grab_y;

   /* set busy cursor */
   _e_wl_shell_grab_start(grab, ewss, _e_wl_comp->input->wl.seat.pointer,
                          &_e_busy_grab_interface, E_DESKTOP_SHELL_CURSOR_BUSY);

   return 1;
}

static void 
_e_wl_shell_render_post(void *data EINA_UNUSED, Evas *e EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews;
   unsigned int secs;
   E_Wayland_Surface_Frame_Callback *cb = NULL, *ncb = NULL;

   /* grab the current server time */
   secs = e_comp_wl_time_get();

   EINA_INLIST_FOREACH(_e_wl_comp->surfaces, ews)
     {
        if (!ews->updates) break;
        /* for each frame callback in the surface, signal it is done */
        wl_list_for_each_safe(cb, ncb, &ews->wl.frames, wl.link)
          {
             wl_callback_send_done(cb->wl.resource, secs);
             wl_resource_destroy(cb->wl.resource);
          }
        ews->updates = 0;
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   Evas_Event_Mouse_Out *ev = event;
   struct wl_pointer *ptr = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;
   if (ews->ec->cur_mouse_action || ews->ec->border_menu) return;
   if (e_object_is_del(E_OBJECT(ews->ec))) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        ptr->x = wl_fixed_from_int(ev->output.x - ews->ec->client.x);
        ptr->y = wl_fixed_from_int(ev->output.y - ews->ec->client.y);

        ptr->current_x = ptr->x;
        ptr->current_y = ptr->y;
        ptr->grab->x = ptr->x;
        ptr->grab->y = ptr->y;

        /* send this mouse movement to wayland */
        ptr->grab->interface->motion(ptr->grab, ev->timestamp, 
                                     ptr->grab->x, ptr->grab->y);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   Evas_Event_Mouse_Move *ev = event;
   struct wl_pointer *ptr = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;
   if (ews->ec->cur_mouse_action || ews->ec->border_menu) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        ptr->x = wl_fixed_from_int(ev->cur.output.x - ews->ec->client.x);
        ptr->y = wl_fixed_from_int(ev->cur.output.y - ews->ec->client.y);

        ptr->current_x = ptr->x;
        ptr->current_y = ptr->y;
        ptr->grab->x = ptr->x;
        ptr->grab->y = ptr->y;

        /* send this mouse movement to wayland */
        ptr->grab->interface->motion(ptr->grab, ev->timestamp, 
                                     ptr->grab->x, ptr->grab->y);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   Evas_Event_Mouse_Up *ev = event;
   int btn = 0;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;
   if (ews->ec->cur_mouse_action || ews->ec->border_menu) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        if (ev->button == 1)
          btn = ev->button + BTN_LEFT - 1; // BTN_LEFT
        else if (ev->button == 2)
          btn = BTN_MIDDLE;
        else if (ev->button == 3)
          btn = BTN_RIGHT;

        if (ptr->button_count > 0) ptr->button_count--;

        ptr->grab->interface->button(ptr->grab, ev->timestamp, btn, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);

        if (ptr->button_count == 1)
          ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   Evas_Event_Mouse_Down *ev;
   int btn = 0;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;
   if (ews->ec->cur_mouse_action || ews->ec->border_menu) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        unsigned int serial = 0;

        if (ev->button == 1)
          btn = ev->button + BTN_LEFT - 1; // BTN_LEFT
        else if (ev->button == 2)
          btn = BTN_MIDDLE;
        else if (ev->button == 3)
          btn = BTN_RIGHT;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        /* if the compositor has a ping callback, call it on this surface */
        if (_e_wl_comp->ping_cb) _e_wl_comp->ping_cb(ews, serial);

        /* update some pointer properties */
        if (ptr->button_count == 0)
          {
             ptr->x = wl_fixed_from_int(ev->output.x - ews->ec->client.x);
             ptr->y = wl_fixed_from_int(ev->output.y - ews->ec->client.y);
             ptr->grab_x = ptr->x;
             ptr->grab_y = ptr->y;
             ptr->grab_button = btn;
             ptr->grab_time = ev->timestamp;
          }

        ptr->button_count++;

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, ev->timestamp, btn, 
                                     WL_POINTER_BUTTON_STATE_PRESSED);

        if (ptr->button_count == 1)
          ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   Evas_Event_Mouse_Wheel *ev;
   int btn = 0;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;
   if (ews->ec->cur_mouse_action || ews->ec->border_menu) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        unsigned int serial = 0;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        /* if the compositor has a ping callback, call it on this surface */
        if (_e_wl_comp->ping_cb) _e_wl_comp->ping_cb(ews, serial);

        switch (ev->direction)
          {
           case 0:
             if (ev->z == -1) btn = 4;
             else if (ev->z == 1) btn = 5;
             break;
           case 1:
             if (ev->z == -1) btn = 6;
             else if (ev->z == 1) btn = 7;
             break;
          }

        /* send this wheel event to the pointer as a button press */
        ptr->grab->interface->button(ptr->grab, ev->timestamp, btn, 
                                     WL_POINTER_BUTTON_STATE_PRESSED);
     }
}

static void 
_e_wl_shell_shell_surface_cb_key_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Key_Up *ev;
   E_Wayland_Surface *ews = NULL;
   struct wl_keyboard *kbd;
   struct wl_keyboard_grab *grab;
   unsigned int key = 0, *end, *k;
   unsigned int serial = 0;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* try to get a reference to the input's keyboard */
   if (!(kbd = _e_wl_comp->input->wl.seat.keyboard)) return;

   /* does this keyboard have a focused surface ? */
   if (!kbd->focus) return;

   /* is the focused surface actually This surface ? */
   if (kbd->focus != ews->wl.surface) return;

#ifndef WAYLAND_ONLY
   if (_e_wl_comp->kbd_handler)
     /* get the keycode for this key from X, since we're definitely in X here */
     key = ecore_x_keysym_keycode_get(ev->keyname) - 8;
   else
#endif
     {
        xkb_keysym_t sym;

        sym = xkb_keysym_from_name(ev->key, 0);
        if (!sym) 
          sym = xkb_keysym_from_name(ev->key, XKB_KEYSYM_CASE_INSENSITIVE);
        key = sym - 8;
     }
   end = ((unsigned int *)kbd->keys.data + kbd->keys.size);
   for (k = kbd->keys.data; k < end; k++)
     if ((*k == key)) *k = *--end;

   kbd->keys.size = end - (unsigned int *)kbd->keys.data;

   /* try to get the current keyboard's grab interface. 
    * Fallback to the default grab */
   if (!(grab = kbd->grab)) grab = &kbd->default_grab;

   /* if we have a grab, send this key to it */
   if (grab)
     grab->interface->key(grab, ev->timestamp, key, 
                          WL_KEYBOARD_KEY_STATE_RELEASED);

   /* update xkb key state */
   xkb_state_update_key(_e_wl_comp->input->xkb.state, key + 8, XKB_KEY_UP);

   /* update keyboard modifiers */
   serial = wl_display_get_serial(_e_wl_comp->wl.display);
   e_comp_wl_input_modifiers_update(serial);
}

static void 
_e_wl_shell_shell_surface_cb_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Evas_Event_Key_Down *ev;
   E_Wayland_Surface *ews = NULL;
   struct wl_keyboard *kbd;
   struct wl_keyboard_grab *grab;
   unsigned int serial = 0, key = 0;
   unsigned int *end, *k;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* try to get a reference to the input's keyboard */
   if (!(kbd = _e_wl_comp->input->wl.seat.keyboard)) return;

   /* does this keyboard have a focused surface ? */
   if (!kbd->focus) return;

   /* is the focused surface actually This surface ? */
   if (kbd->focus != ews->wl.surface) return;

   serial = wl_display_next_serial(_e_wl_comp->wl.display);

   /* if the compositor has a ping callback, call it on this surface */
   if (_e_wl_comp->ping_cb) _e_wl_comp->ping_cb(ews, serial);

#ifndef WAYLAND_ONLY
   if (_e_wl_comp->kbd_handler)
     /* get the keycode for this key from X, since we're definitely in X here */
     key = ecore_x_keysym_keycode_get(ev->keyname) - 8;
   else
#endif
     {
        xkb_keysym_t sym;

        sym = xkb_keysym_from_name(ev->key, 0);
        if (!sym) 
          sym = xkb_keysym_from_name(ev->key, XKB_KEYSYM_CASE_INSENSITIVE);
        key = sym - 8;
     }
   /* update the keyboards grab properties */
   kbd->grab_key = key;
   kbd->grab_time = ev->timestamp;

   end = ((unsigned int *)kbd->keys.data + kbd->keys.size);
   for (k = kbd->keys.data; k < end; k++)
     {
        /* ignore server generated key repeats */
        if ((*k == key)) return;
     }

   kbd->keys.size = end - (unsigned int *)kbd->keys.data;
   k = wl_array_add(&kbd->keys, sizeof(*k));
   *k = key;

   /* try to get the current keyboard's grab interface. 
    * Fallback to the default grab */
   if (!(grab = kbd->grab)) grab = &kbd->default_grab;

   /* if we have a grab, send this key to it */
   if (grab)
     grab->interface->key(grab, ev->timestamp, key, 
                          WL_KEYBOARD_KEY_STATE_PRESSED);

   /* update xkb key state */
   xkb_state_update_key(_e_wl_comp->input->xkb.state, key + 8, XKB_KEY_DOWN);

   /* update keyboard modifiers */
   serial = wl_display_get_serial(_e_wl_comp->wl.display);
   e_comp_wl_input_modifiers_update(serial);
}

static Eina_Bool
_e_wl_shell_shell_surface_cb_client_prop(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev = event;

   if (e_pixmap_type_get(ev->ec->pixmap) != E_PIXMAP_TYPE_WL) return ECORE_CALLBACK_RENEW;
   if (!(ev->property & E_CLIENT_PROPERTY_ICON)) return ECORE_CALLBACK_RENEW;
   if (ev->ec->desktop)
     {
        if (!ev->ec->exe_inst)
          e_exec_phony(ev->ec);
     }
   return ECORE_CALLBACK_RENEW;
}

static void 
_e_wl_shell_shell_surface_cb_ec_hook_focus_unset(void *data EINA_UNUSED, E_Client *ec)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Input *input = NULL;
   Eina_List *l = NULL;

   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
   /* try to cast data to our surface structure */
   if (!(ews = (E_Wayland_Surface*)ec->comp_data)) return;
   
   /* if this surface is not visible, get out */
   if (!ews->mapped) return;

   /* loop the list of inputs */
   EINA_LIST_FOREACH(_e_wl_comp->seats, l, input)
     {
        /* set keyboard focus */
        wl_keyboard_set_focus(input->wl.seat.keyboard, NULL);

        /* end any keyboard grabs */
        wl_keyboard_end_grab(input->wl.seat.keyboard);
     }
}

static void 
_e_wl_shell_shell_surface_cb_ec_hook_focus_set(void *data EINA_UNUSED, E_Client *ec)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Input *input = NULL;
   struct wl_pointer *ptr = NULL;
   Eina_List *l = NULL;

   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
   /* try to cast data to our surface structure */
   if (!(ews = (E_Wayland_Surface*)ec->comp_data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        /* if the mouse entered this surface & it is not the current surface */
        if (ews->wl.surface != ptr->current)
          {
             const struct wl_pointer_grab_interface *grab;

             /* set this surface as the current */
             grab = ptr->grab->interface;
             ptr->current = ews->wl.surface;

             /* send a pointer focus event */
             grab->focus(ptr->grab, ews->wl.surface, 
                         ptr->current_x, ptr->current_y);
          }
     }
   /* loop the list of inputs */
   EINA_LIST_FOREACH(_e_wl_comp->seats, l, input)
     {
        /* set keyboard focus */
        wl_keyboard_set_focus(input->wl.seat.keyboard, ews->wl.surface);

        /* update the keyboard focus in the data device */
        wl_data_device_set_keyboard_focus(&input->wl.seat);
     }
}

static void 
_e_wl_shell_shell_surface_cb_ec_hook_move_end(void *data EINA_UNUSED, E_Client *ec)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;

return;
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
   /* try to cast data to our surface structure */
   if (!(ews = (E_Wayland_Surface*)ec->comp_data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        /* do we have a valid pointer grab ? */
        if (!ptr->grab) return;

        /* is this grab the 'move' interface ? */
        if (ptr->grab->interface != &_e_move_grab_interface)
          return;

        /* does this shell have a valid shell surface ? */
        if (!ews->shell_surface) return;

        /* is the shell surface the same one in the grab ? */
        if ((ptr->current) && (ptr->current != ews->wl.surface)) return;

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, 
                                     ptr->grab_time,
                                     ptr->grab_button, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);
     }
}

static void 
_e_wl_shell_shell_surface_cb_ec_hook_resize_end(void *data EINA_UNUSED, E_Client *ec)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
return;
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
   /* try to cast data to our surface structure */
   if (!(ews = (E_Wayland_Surface*)ec->comp_data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        /* do we have a valid pointer grab ? */
        if (!ptr->grab) return;

        /* is this grab the 'move' interface ? */
        if (ptr->grab->interface != &_e_resize_grab_interface)
          return;

        /* does this shell have a valid shell surface ? */
        if (!ews->shell_surface) return;

        /* is the shell surface the same one in the grab ? */
        if ((ptr->current) && (ptr->current != ews->wl.surface)) return;

        ptr->button_count--;

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, 
                                     ptr->grab_time,
                                     ptr->grab_button, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);

        if (ptr->button_count == 1)
          ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);
     }
}

static void 
_e_wl_shell_shell_surface_cb_ec_hook_resize_update(void *data EINA_UNUSED, E_Client *ec)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
return;
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
   /* try to cast data to our surface structure */
   if (!(ews = (E_Wayland_Surface*)ec->comp_data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        Evas_Coord w = 0, h = 0;

        if (!ptr->grab) return;

        if (ptr->grab->interface != &_e_resize_grab_interface)
          return;

        if (!ews->shell_surface) return;

        if ((ptr->current) && (ptr->current != ews->wl.surface)) return;

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, 
                                     ptr->grab_time,
                                     ptr->grab_button, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);
        w = ec->client.w;
        h = ec->client.h;

        wl_shell_surface_send_configure(ews->shell_surface->wl.resource, 
                                        ptr->grab->edges, w, h);
     }
}

static void
_e_wl_shell_surface_cb_smart_client_resize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Wayland_Shell_Grab *grab = NULL;
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;

   /* try to cast data to our surface structure */
   ews = data;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        Evas_Coord w = 0, h = 0;

        if (!(grab = (E_Wayland_Shell_Grab *)ptr->grab)) return;

        w = ews->ec->client.w;
        h = ews->ec->client.h;

        wl_shell_surface_send_configure(ews->shell_surface->wl.resource, 
                                        grab->edges, w, h);
     }
}

/* shell surface interface functions */
static void 
_e_wl_shell_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int serial)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Ping_Timer *tmr = NULL;
   Eina_Bool responsive = EINA_FALSE;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* try to cast the ping timer */
   if (!(tmr = (E_Wayland_Ping_Timer *)ewss->ping_timer))
     return;

   if (tmr->serial != serial) return;

   responsive = ewss->active;
   ewss->active = EINA_TRUE;

   if (!responsive)
     {
        E_Wayland_Shell_Grab *grab = NULL;

        grab = (E_Wayland_Shell_Grab *)_e_wl_comp->input->wl.seat.pointer->grab;
        if ((grab->grab.interface == &_e_busy_grab_interface) && 
            (grab->shell_surface == ewss))
          {
             _e_wl_shell_grab_end(grab);
             free(grab);
          }
     }

   if (tmr->source) wl_event_source_remove(tmr->source);

   /* free the allocated space for ping timer */
   E_FREE(tmr);
   ewss->ping_timer = NULL;
}

static void
_e_wl_shell_mouse_down_helper(E_Client *ec, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev, Eina_Bool move)
{
   if ((button >= 1) && (button <= 3))
     {
        ec->mouse.last_down[button - 1].mx = output->x;
        ec->mouse.last_down[button - 1].my = output->y;
        ec->mouse.last_down[button - 1].x = ec->x;
        ec->mouse.last_down[button - 1].y = ec->y;
        ec->mouse.last_down[button - 1].w = ec->w;
        ec->mouse.last_down[button - 1].h = ec->h;
     }
   else
     {
        ec->moveinfo.down.x = ec->x;
        ec->moveinfo.down.y = ec->y;
        ec->moveinfo.down.w = ec->w;
        ec->moveinfo.down.h = ec->h;
     }
   ec->mouse.current.mx = output->x;
   ec->mouse.current.my = output->y;

   if (move)
     {
        /* tell E to start moving the client */
        e_client_act_move_begin(ec, ev);

        /* we have to get a reference to the window_move action here, or else 
         * when e_client stops the move we will never get notified */
        ec->cur_mouse_action = e_action_find("window_move");
        if (ec->cur_mouse_action)
          e_object_ref(E_OBJECT(ec->cur_mouse_action));
     }
   else
     {
        /* tell E to start resizing the client */
        e_client_act_resize_begin(ec, ev);

        /* we have to get a reference to the window_resize action here, 
         * or else when e_client stops the resize we will never get notified */
        ec->cur_mouse_action = e_action_find("window_resize");
        if (ec->cur_mouse_action)
          e_object_ref(E_OBJECT(ec->cur_mouse_action));
     }

   e_focus_event_mouse_down(ec);

   if ((button >= 1) && (button <= 3))
     {
        ec->mouse.last_down[button - 1].mx = output->x;
        ec->mouse.last_down[button - 1].my = output->y;
        ec->mouse.last_down[button - 1].x = ec->x;
        ec->mouse.last_down[button - 1].y = ec->y;
        ec->mouse.last_down[button - 1].w = ec->w;
        ec->mouse.last_down[button - 1].h = ec->h;
     }
   else
     {
        ec->moveinfo.down.x = ec->x;
        ec->moveinfo.down.y = ec->y;
        ec->moveinfo.down.w = ec->w;
        ec->moveinfo.down.h = ec->h;
     }
   ec->mouse.current.mx = output->x;
   ec->mouse.current.my = output->y;
}

static void 
_e_wl_shell_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Shell_Grab *grab = NULL;
   E_Binding_Event_Mouse_Button ev;
   struct wl_pointer *ptr = NULL;

   /* try to cast the seat resource to our input structure */
   if (!(input = wl_resource_get_user_data(seat_resource)))
     return;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* if the shell surface is fullscreen, get out */
   if (ewss->type == E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN) return;

   /* check for valid move setup */
   if ((input->wl.seat.pointer->button_count == 0) || 
       (input->wl.seat.pointer->grab_serial != serial) || 
       (input->wl.seat.pointer->focus != ewss->surface->wl.surface))
     return;

   /* try to allocate space for our grab structure */
   if (!(grab = E_NEW(E_Wayland_Shell_Grab, 1))) return;

   /* try to get the pointer from this input */
   ptr = _e_wl_comp->input->wl.seat.pointer;

   /* set grab properties */
   grab->x = ptr->grab_x;
   grab->y = ptr->grab_y;

   /* start the movement grab */
   _e_wl_shell_grab_start(grab, ewss, input->wl.seat.pointer, 
                          &_e_move_grab_interface, 
                          E_DESKTOP_SHELL_CURSOR_MOVE);

   /* set button property of the binding event */
   if (grab->grab.pointer->grab_button == BTN_LEFT)
     ev.button = 1;
   else if (grab->grab.pointer->grab_button == BTN_MIDDLE)
     ev.button = 2;
   else if (grab->grab.pointer->grab_button == BTN_RIGHT)
     ev.button = 3;
   else
     ev.button = 0;

   /* set the clicked location in the binding event
    * the ptr coords are relative to the client, so adjust them to be canvas
    */
   e_comp_object_frame_xy_unadjust(ewss->surface->ec->frame,
     wl_fixed_to_int(ptr->x) + ewss->surface->ec->client.x,
     wl_fixed_to_int(ptr->y) + ewss->surface->ec->client.y, &ev.canvas.x, &ev.canvas.y);

   /* call our helper function to initiate a move */
   _e_wl_shell_mouse_down_helper(ewss->surface->ec, ev.button, 
                                 &(Evas_Point){ev.canvas.x, ev.canvas.y},
                                 &ev, EINA_TRUE);
}

static void 
_e_wl_shell_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, unsigned int edges)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Shell_Grab *grab = NULL;
   E_Binding_Event_Mouse_Button ev;
   struct wl_pointer *ptr = NULL;

   /* try to cast the seat resource to our input structure */
   if (!(input = wl_resource_get_user_data(seat_resource)))
     return;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* if the shell surface is fullscreen or maximized, get out */
   if ((ewss->type == E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN) || 
       (ewss->type == E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED))
     return;

   /* check for valid move setup */
   if ((input->wl.seat.pointer->button_count == 0) || 
       (input->wl.seat.pointer->grab_serial != serial) || 
       (input->wl.seat.pointer->focus != ewss->surface->wl.surface))
     return;

   if ((edges == 0) || (edges > 15) || 
       ((edges & 3) == 3) || ((edges & 12) == 12))
     return;

   /* try to allocate space for our grab structure */
   if (!(grab = E_NEW(E_Wayland_Shell_Grab, 1))) return;

   /* try to get the pointer from this input */
   ptr = _e_wl_comp->input->wl.seat.pointer;

   /* set grab properties */
   grab->edges = edges;
   grab->grab.edges = edges;
   grab->w = ewss->surface->geometry.w;
   grab->h = ewss->surface->geometry.h;

   /* start the resize grab */
   _e_wl_shell_grab_start(grab, ewss, input->wl.seat.pointer, 
                          &_e_resize_grab_interface, edges);

   if (grab->grab.pointer->grab_button == BTN_LEFT)
     ev.button = 1;
   else if (grab->grab.pointer->grab_button == BTN_MIDDLE)
     ev.button = 2;
   else if (grab->grab.pointer->grab_button == BTN_RIGHT)
     ev.button = 3;
   else
     ev.button = 0;

   /* set the clicked location in the binding event
    * the ptr coords are relative to the client, so adjust them to be canvas
    */
   e_comp_object_frame_xy_unadjust(ewss->surface->ec->frame,
     wl_fixed_to_int(ptr->x) + ewss->surface->ec->client.x,
     wl_fixed_to_int(ptr->y) + ewss->surface->ec->client.y, &ev.canvas.x, &ev.canvas.y);

   /* call our helper function to initiate a resize */
   _e_wl_shell_mouse_down_helper(ewss->surface->ec, ev.button, 
                                 &(Evas_Point){ev.canvas.x, ev.canvas.y},
                                 &ev, EINA_FALSE);
}

static void 
_e_wl_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL;
}

static void 
_e_wl_shell_shell_surface_cb_transient_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource, int x, int y, unsigned int flags)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   ews = wl_resource_get_user_data(parent_resource);

   ewss->parent = ews;
   ewss->transient.x = x;
   ewss->transient.y = y;
   ewss->transient.flags = flags;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_TRANSIENT;
}

static void 
_e_wl_shell_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int method EINA_UNUSED, unsigned int framerate EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* NB FIXME: Disable fullscreen support for right now.
    * 
    * Appears to be some recent issue with e_client...
    * 
    * Needs looking into */
   return;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL;

   /* check for valid client */
   if (ewss->surface->ec)
     {
        /* tell E to fullscreen this window */
        e_client_fullscreen(ewss->surface->ec, E_FULLSCREEN_RESIZE);

        /* send configure message to the shell surface to inform of new size */
        wl_shell_surface_send_configure(ewss->wl.resource, 0, 
                                        ewss->surface->ec->w, 
                                        ewss->surface->ec->h);
     }
}

static void 
_e_wl_shell_shell_surface_cb_popup_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, struct wl_resource *parent_resource, int x, int y, unsigned int flags EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Input *input = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* cast the seat resource to our input structure */
   input = wl_resource_get_user_data(seat_resource);

   /* set surface type */
   ewss->type = E_WAYLAND_SHELL_SURFACE_TYPE_POPUP;

   /* set surface popup properties */
   ewss->parent = wl_resource_get_user_data(parent_resource);
   ewss->popup.seat = &input->wl.seat;
   ewss->popup.serial = serial;
   ewss->popup.x = x;
   ewss->popup.y = y;
}

static void 
_e_wl_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED;

   /* check for valid client */
   if (ewss->surface->ec)
     {
        unsigned int edges = 0;

        edges = (WL_SHELL_SURFACE_RESIZE_TOP | WL_SHELL_SURFACE_RESIZE_LEFT);

        /* tell E to maximize this window */
        e_client_maximize(ewss->surface->ec, 
                          (e_config->maximize_policy & E_MAXIMIZE_TYPE) | 
                          E_MAXIMIZE_BOTH);

        /* send configure message to the shell surface to inform of new size */
        wl_shell_surface_send_configure(ewss->wl.resource, edges, 
                                        ewss->surface->ec->w, 
                                        ewss->surface->ec->h);
     }
}

static void 
_e_wl_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   eina_stringshare_replace(&ewss->title, title);
   if (!ewss->surface->ec) return;
   eina_stringshare_refplace(&ewss->surface->ec->icccm.title, ewss->title);
   e_comp_object_frame_title_set(ewss->surface->ec->frame, title);
}

static void 
_e_wl_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   eina_stringshare_replace(&ewss->clas, clas);
   if (!ewss->surface->ec) return;
   eina_stringshare_refplace(&ewss->surface->ec->icccm.class, ewss->clas);
   ewss->surface->ec->changes.icon = 1;
   EC_CHANGED(ewss->surface->ec);
}

/* shell move_grab interface functions */
static void 
_e_wl_shell_move_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   struct wl_pointer *ptr;

   /* safety */
   if (!grab) return;

   ptr = grab->pointer;
   ptr->current = surface;
}

static void 
_e_wl_shell_move_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* FIXME: This needs to become a no-op as the actual surface movement 
    * is handled by e_client now */
}

static void 
_e_wl_shell_move_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   E_Wayland_Shell_Grab *ewsg = NULL;
   struct wl_pointer *ptr;

   /* safety */
   if (!grab) return;

   /* try to get the shell grab from the pointer grab */
   if (!(ewsg = container_of(grab, E_Wayland_Shell_Grab, grab)))
     return;

   /* try to get the pointer */
   if (!(ptr = grab->pointer)) return;

   if (state == WL_POINTER_BUTTON_STATE_RELEASED)
     {
        if (ptr->button_count > 0) ptr->button_count--;
     }
   else
     ptr->button_count++;

   if (ptr->button_count == 1)
     ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);

   /* test if we are done with the grab */
   if ((ptr->button_count == 0) && 
       (state == WL_POINTER_BUTTON_STATE_RELEASED))
     {
        struct wl_list *lst;
        struct wl_resource *res;

        /* end the grab */
        _e_wl_shell_grab_end(ewsg);
        free(grab);

        lst = &ptr->focus_resource_list;
        if (!wl_list_empty(lst))
          {
             wl_resource_for_each(res, lst)
               e_comp_wl_mouse_button(res, ptr->grab_serial, timestamp, button, state);
          }
     }
}

/* shell resize_grab interface functions */
static void 
_e_wl_shell_resize_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   struct wl_pointer *ptr;

   /* safety */
   if (!grab) return;

   ptr = grab->pointer;
   ptr->current = surface;
}

static void 
_e_wl_shell_resize_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* FIXME: This needs to become a no-op as the actual surface resize 
    * is handled by e_client now */
}

static void 
_e_wl_shell_resize_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   E_Wayland_Shell_Grab *ewsg = NULL;
   struct wl_pointer *ptr;

   /* safety */
   if (!grab) return;

   /* try to get the shell grab from the pointer grab */
   if (!(ewsg = container_of(grab, E_Wayland_Shell_Grab, grab)))
     return;

   /* try to get the pointer */
   if (!(ptr = grab->pointer)) return;

   if (state == WL_POINTER_BUTTON_STATE_RELEASED)
     {
        if (ptr->button_count > 0) ptr->button_count--;
     }
   else
     ptr->button_count++;

   if (ptr->button_count == 1)
     ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);

   /* test if we are done with the grab */
   if ((ptr->button_count == 0) && 
       (state == WL_POINTER_BUTTON_STATE_RELEASED))
     {
        struct wl_list *lst;
        struct wl_resource *res;

        /* end the grab */
        _e_wl_shell_grab_end(ewsg);
        free(grab);

        lst = &ptr->focus_resource_list;
        if (!wl_list_empty(lst))
          {
             wl_resource_for_each(res, lst)
               e_comp_wl_mouse_button(res, ptr->grab_serial, timestamp, button, state);
          }
     }
}

/* shell popup_grab interface prototypes */
static void 
_e_wl_shell_popup_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y)
{
   E_Wayland_Shell_Surface *ewss;
   struct wl_pointer *ptr;
   struct wl_client *client;

   /* try to get the pointer */
   if (!(ptr = grab->pointer)) return;

   /* try to get the surface from the pointer grab */
   ewss = container_of(grab, E_Wayland_Shell_Surface, popup.grab);
   if (!ewss) return;

   client = wl_resource_get_client(ewss->surface->wl.surface);

   if ((surface) && (wl_resource_get_client(surface) == client))
     {
        wl_pointer_set_focus(ptr, surface, x, y);
        grab->focus = surface;
     }
   else
     {
        wl_pointer_set_focus(ptr, NULL, 0, 0);
        grab->focus = NULL;
     }
}

static void 
_e_wl_shell_popup_grab_cb_motion(struct wl_pointer_grab *grab, unsigned int timestamp, wl_fixed_t x, wl_fixed_t y)
{
   struct wl_resource *res;

   wl_resource_for_each(res, &grab->pointer->focus_resource_list)
     wl_pointer_send_motion(res, timestamp, x, y);
}

static void 
_e_wl_shell_popup_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   struct wl_resource *res;
   struct wl_list *lst;

   /* try to get the shell surface */
   ewss = container_of(grab, E_Wayland_Shell_Surface, popup.grab);
   if (!ewss) return;

   lst = &grab->pointer->focus_resource_list;
   if (!wl_list_empty(lst))
     {
        uint32_t serial;

        serial = wl_display_get_serial(_e_wl_comp->wl.display);

        wl_resource_for_each(res, lst)
          wl_pointer_send_button(res, serial, timestamp, button, state);
     }
   else if ((state = WL_POINTER_BUTTON_STATE_RELEASED) && 
            ((ewss->popup.up) || 
                (timestamp - ewss->popup.seat->pointer->grab_time > 500)))
     {
        /* end the popup grab */
        _e_wl_shell_popup_grab_end(grab->pointer);
     }

   if (state == WL_POINTER_BUTTON_STATE_RELEASED)
     ewss->popup.up = EINA_TRUE;
}

/* shell popup functions */
static void 
_e_wl_shell_popup_grab_end(struct wl_pointer *pointer)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   struct wl_pointer_grab *grab;

   grab = pointer->grab;

   /* try to get the shell surface from this grab */
   ewss = container_of(grab, E_Wayland_Shell_Surface, popup.grab);
   if (!ewss) return;

   if (pointer->grab->interface == &_e_popup_grab_interface)
     {
        wl_shell_surface_send_popup_done(ewss->wl.resource);
        wl_pointer_end_grab(grab->pointer);
        ewss->popup.grab.pointer = NULL;
     }
}

/* shell busy_grab interface functions */
static void 
_e_wl_shell_busy_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   E_Wayland_Shell_Grab *ewsg = NULL;

   /* try to cast the pointer grab to our structure */
   if (!(ewsg = (E_Wayland_Shell_Grab *)grab)) return;

   /* if the grab's focus is not this surface, then end the grab */
   if ((!ewsg->shell_surface) || 
       (ewsg->shell_surface->surface->wl.surface != surface))
     {
        /* end the grab */
        _e_wl_shell_grab_end(ewsg);
        free(grab);
     }
}

static void 
_e_wl_shell_busy_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* no-op */
}

static void 
_e_wl_shell_busy_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button, unsigned int state)
{
   E_Wayland_Shell_Grab *ewsg = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Input *input = NULL;

   /* try to cast the pointer grab to our structure */
   if (!(ewsg = (E_Wayland_Shell_Grab *)grab)) return;

   /* try to get the current shell surface */
   if (!(ewss = ewsg->shell_surface)) return;
   /* if (!(ews = (E_Wayland_Surface *)ewsg->grab.pointer->current)) return; */

   /* try to cast the pointer seat to our input structure */
   if (!(input = (E_Wayland_Input *)ewsg->grab.pointer->seat))
     return;

   if ((ewss) && (button == 1) && (state))
     {
        E_Wayland_Shell_Grab *mgrab = NULL;

        /* try to allocate space for our grab structure */
        if (!(mgrab = E_NEW(E_Wayland_Shell_Grab, 1))) return;

        /* set grab properties */
        mgrab->x = input->wl.seat.pointer->grab_x;
        mgrab->y = input->wl.seat.pointer->grab_y;

        /* start the movement grab */
        _e_wl_shell_grab_start(mgrab, ewss, input->wl.seat.pointer, 
                               &_e_busy_grab_interface, 
                               E_DESKTOP_SHELL_CURSOR_MOVE);
     }
}
