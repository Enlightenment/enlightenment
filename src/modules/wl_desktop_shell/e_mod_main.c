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
static void _e_wl_shell_shell_surface_cb_ee_resize(Ecore_Evas *ee);
static void _e_wl_shell_shell_surface_cb_render_post(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_focus_in(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_focus_out(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_mouse_in(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_mouse_out(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_mouse_down(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_key_up(void *data, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_key_down(void *data, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_wl_shell_shell_surface_cb_bd_move_end(void *data, void *bd);
static void _e_wl_shell_shell_surface_cb_bd_resize_update(void *data, void *bd);
static void _e_wl_shell_shell_surface_cb_bd_resize_end(void *data, void *bd);

static void _e_wl_shell_mouse_down_helper(E_Border *bd, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev, Eina_Bool move);
static void _e_wl_shell_mouse_up_helper(E_Border *bd, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev EINA_UNUSED);

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
static void _e_wl_shell_move_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED);
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

/* external variables */

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop_Shell" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_global *gshell = NULL;
   E_Wayland_Input *input = NULL;
   Eina_List *l = NULL;

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

   grab->pointer = pointer;
//   grab->grab.focus = ewss->surface->wl.surface;

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
   wl_pointer_end_grab(ewsg->pointer);
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
   E_Container *con;

   /* get the current container */
   con = e_container_current_get(e_manager_current_get());

   /* create the ecore_evas */
   ews->ee = ecore_evas_new(NULL, ews->geometry.x, ews->geometry.y, 
                            ews->geometry.w, ews->geometry.h, NULL);
   ecore_evas_alpha_set(ews->ee, EINA_TRUE);
   ecore_evas_borderless_set(ews->ee, EINA_TRUE);
   ecore_evas_callback_resize_set(ews->ee, 
                                  _e_wl_shell_shell_surface_cb_ee_resize);
   ecore_evas_data_set(ews->ee, "surface", ews);

   /* ecore_evas_input_event_unregister(ews->ee); */

   ews->evas_win = ecore_evas_window_get(ews->ee);

   /* get a reference to the canvas */
   ews->evas = ecore_evas_get(ews->ee);

   /* setup a post_render callback */
   evas_event_callback_add(ews->evas, EVAS_CALLBACK_RENDER_POST, 
                           _e_wl_shell_shell_surface_cb_render_post, ews);

   /* create a surface smart object */
   ews->obj = e_surface_add(ews->evas);
   evas_object_move(ews->obj, 0, 0);
   evas_object_resize(ews->obj, ews->geometry.w, ews->geometry.h);
   evas_object_show(ews->obj);

   /* hook smart object callbacks */
   evas_object_smart_callback_add(ews->obj, "mouse_in", 
                                  _e_wl_shell_shell_surface_cb_mouse_in, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_out", 
                                  _e_wl_shell_shell_surface_cb_mouse_out, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_move", 
                                  _e_wl_shell_shell_surface_cb_mouse_move, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_up", 
                                  _e_wl_shell_shell_surface_cb_mouse_up, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_down", 
                                  _e_wl_shell_shell_surface_cb_mouse_down, ews);
   evas_object_smart_callback_add(ews->obj, "key_up", 
                                  _e_wl_shell_shell_surface_cb_key_up, ews);
   evas_object_smart_callback_add(ews->obj, "key_down", 
                                  _e_wl_shell_shell_surface_cb_key_down, ews);
   evas_object_smart_callback_add(ews->obj, "focus_in", 
                                  _e_wl_shell_shell_surface_cb_focus_in, ews);
   evas_object_smart_callback_add(ews->obj, "focus_out", 
                                  _e_wl_shell_shell_surface_cb_focus_out, ews);

   ecore_x_icccm_size_pos_hints_set(ews->evas_win, 0, ECORE_X_GRAVITY_NW, 
                                    0, 0, 9999, 9999, 0, 0, 1, 1, 0.0, 0.0);
   ecore_x_icccm_transient_for_unset(ews->evas_win);
   ecore_x_netwm_window_type_set(ews->evas_win, ECORE_X_WINDOW_TYPE_NORMAL);

   ecore_evas_lower(ews->ee);

   /* create the e border for this surface */
   ews->bd = e_border_new(con, ews->evas_win, 1, 1);
   e_surface_border_input_set(ews->obj, ews->bd);
   ews->bd->re_manage = 0;
   ews->bd->internal = 1;
   ews->bd->internal_ecore_evas = ews->ee;
   ews->bd->internal_no_remember = 1;
   ews->bd->internal_no_reopen = 1;

   e_border_move_resize(ews->bd, ews->geometry.x, ews->geometry.y, 
                        ews->geometry.w, ews->geometry.h);

   e_border_show(ews->bd);

   ews->bd_hooks = 
     eina_list_append(ews->bd_hooks, 
                      e_border_hook_add(E_BORDER_HOOK_MOVE_END, 
                                        _e_wl_shell_shell_surface_cb_bd_move_end, ews));
   ews->bd_hooks = 
     eina_list_append(ews->bd_hooks, 
                      e_border_hook_add(E_BORDER_HOOK_RESIZE_END, 
                                        _e_wl_shell_shell_surface_cb_bd_resize_end, ews));
   ews->bd_hooks = 
     eina_list_append(ews->bd_hooks, 
                      e_border_hook_add(E_BORDER_HOOK_RESIZE_UPDATE, 
                                        _e_wl_shell_shell_surface_cb_bd_resize_update, ews));

   ews->mapped = EINA_TRUE;
}

static void 
_e_wl_shell_shell_surface_create_popup(E_Wayland_Surface *ews)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   Ecore_X_Window parent = 0;
   struct wl_seat *seat;
   char opts[PATH_MAX];

   /* try to get the shell surface */
   if (!(ewss = ews->shell_surface)) return;

   if (ewss->parent)
     parent = ecore_evas_window_get(ewss->parent->ee);

   snprintf(opts, sizeof(opts), "parent=%d", parent);

   /* create an ecore evas to represent this 'popup' */
   ews->ee = ecore_evas_new(NULL, ewss->popup.x, ewss->popup.y, 
                            ews->geometry.w, ews->geometry.h, opts);

   ecore_evas_alpha_set(ews->ee, EINA_TRUE);
   ecore_evas_borderless_set(ews->ee, EINA_TRUE);
   ecore_evas_override_set(ews->ee, EINA_TRUE);
   ecore_evas_callback_resize_set(ews->ee, 
                                  _e_wl_shell_shell_surface_cb_ee_resize);
   ecore_evas_data_set(ews->ee, "surface", ews);

   ecore_evas_input_event_unregister(ews->ee);

   ews->evas_win = ecore_evas_window_get(ews->ee);

   /* get a reference to the canvas */
   ews->evas = ecore_evas_get(ews->ee);

   /* setup a post_render callback */
   evas_event_callback_add(ews->evas, EVAS_CALLBACK_RENDER_POST, 
                           _e_wl_shell_shell_surface_cb_render_post, ews);

   /* create a surface smart object */
   ews->obj = e_surface_add(ews->evas);
   evas_object_move(ews->obj, 0, 0);
   evas_object_resize(ews->obj, ews->geometry.w, ews->geometry.h);
   evas_object_show(ews->obj);

   /* hook smart object callbacks */
   evas_object_smart_callback_add(ews->obj, "mouse_in", 
                                  _e_wl_shell_shell_surface_cb_mouse_in, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_out", 
                                  _e_wl_shell_shell_surface_cb_mouse_out, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_move", 
                                  _e_wl_shell_shell_surface_cb_mouse_move, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_up", 
                                  _e_wl_shell_shell_surface_cb_mouse_up, ews);
   evas_object_smart_callback_add(ews->obj, "mouse_down", 
                                  _e_wl_shell_shell_surface_cb_mouse_down, ews);
   evas_object_smart_callback_add(ews->obj, "key_up", 
                                  _e_wl_shell_shell_surface_cb_key_up, ews);
   evas_object_smart_callback_add(ews->obj, "key_down", 
                                  _e_wl_shell_shell_surface_cb_key_down, ews);
   evas_object_smart_callback_add(ews->obj, "focus_in", 
                                  _e_wl_shell_shell_surface_cb_focus_in, ews);
   evas_object_smart_callback_add(ews->obj, "focus_out", 
                                  _e_wl_shell_shell_surface_cb_focus_out, ews);

   ecore_x_icccm_size_pos_hints_set(ews->evas_win, 0, ECORE_X_GRAVITY_NW, 
                                    0, 0, 9999, 9999, 0, 0, 1, 1, 0.0, 0.0);

   ecore_x_icccm_transient_for_set(ews->evas_win, parent);
   ecore_x_netwm_window_type_set(ews->evas_win, ECORE_X_WINDOW_TYPE_POPUP_MENU);

   /* ecore_evas_lower(ews->ee); */
   ecore_evas_show(ews->ee);
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
        Evas_Coord fx = 0, fy = 0;
        Evas_Coord tx = 0, ty = 0;

        fx += ews->geometry.x;
        fy += ews->geometry.y;

        tx = x + ews->geometry.x;
        ty = y + ews->geometry.y;

        ews->geometry.x = ews->geometry.x + tx - fx;
        ews->geometry.y = ews->geometry.y + ty - fy;
        ews->geometry.w = w;
        ews->geometry.h = h;
        ews->geometry.changed = EINA_TRUE;
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
      default:
        break;
     }
}

static void 
_e_wl_shell_shell_surface_unmap(E_Wayland_Surface *ews)
{
   Eina_List *l = NULL;
   E_Wayland_Input *input;
   E_Border_Hook *hook;

   if (!ews) return;

   EINA_LIST_FREE(ews->bd_hooks, hook)
     e_border_hook_del(hook);

   EINA_LIST_FOREACH(_e_wl_comp->seats, l, input)
     {
        if ((input->wl.seat.keyboard) && 
            (input->wl.seat.keyboard->focus == ews->wl.surface))
          wl_keyboard_set_focus(input->wl.seat.keyboard, NULL);
        if ((input->wl.seat.pointer) && 
            (input->wl.seat.pointer->focus == ews->wl.surface))
          wl_pointer_set_focus(input->wl.seat.pointer, NULL, 0, 0);
     }

   if (ews->obj)
     {
        /* delete smart callbacks */
        evas_object_smart_callback_del(ews->obj, "mouse_in", 
                                       _e_wl_shell_shell_surface_cb_mouse_in);
        evas_object_smart_callback_del(ews->obj, "mouse_out", 
                                       _e_wl_shell_shell_surface_cb_mouse_out);
        evas_object_smart_callback_del(ews->obj, "mouse_move", 
                                       _e_wl_shell_shell_surface_cb_mouse_move);
        evas_object_smart_callback_del(ews->obj, "mouse_up", 
                                       _e_wl_shell_shell_surface_cb_mouse_up);
        evas_object_smart_callback_del(ews->obj, "mouse_down", 
                                       _e_wl_shell_shell_surface_cb_mouse_down);
        evas_object_smart_callback_del(ews->obj, "key_up", 
                                       _e_wl_shell_shell_surface_cb_key_up);
        evas_object_smart_callback_del(ews->obj, "key_down", 
                                       _e_wl_shell_shell_surface_cb_key_down);
        evas_object_smart_callback_del(ews->obj, "focus_in", 
                                       _e_wl_shell_shell_surface_cb_focus_in);
        evas_object_smart_callback_del(ews->obj, "focus_out", 
                                       _e_wl_shell_shell_surface_cb_focus_out);

        /* delete the object */
        evas_object_del(ews->obj);
     }

   if (ews->bd) e_object_del(E_OBJECT(ews->bd));

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
        /* ecore_evas_fullscreen_set(ewss->surface->ee, EINA_FALSE); */
        /* unfullscreen the border */
        if (ewss->surface->bd)
          e_border_unfullscreen(ewss->surface->bd);

        /* restore the saved geometry */
        ewss->surface->geometry.x = ewss->saved.x;
        ewss->surface->geometry.y = ewss->saved.y;
        ewss->surface->geometry.w = ewss->saved.w;
        ewss->surface->geometry.h = ewss->saved.h;
        ewss->surface->geometry.changed = EINA_TRUE;
        break;
      case E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED:
        /* unmaximize the border */
        if (ewss->surface->bd)
          e_border_unmaximize(ewss->surface->bd, 
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

        free(ewss->title);
        wl_list_remove(&ewss->wl.link);
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
_e_wl_shell_shell_surface_cb_ee_resize(Ecore_Evas *ee)
{
   E_Wayland_Surface *ews = NULL;

   /* try to get the surface structure of this ecore_evas */
   if (!(ews = ecore_evas_data_get(ee, "surface")))
     return;

   /* if we have the surface smart object */
   if (ews->obj)
     {
        int w = 0, h = 0;

        /* grab the requested geometry */
        ecore_evas_request_geometry_get(ee, NULL, NULL, &w, &h);

        /* resize the surface smart object */
        evas_object_resize(ews->obj, w, h);
     }
}

static void 
_e_wl_shell_shell_surface_cb_render_post(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   unsigned int secs = 0;
   E_Wayland_Surface_Frame_Callback *cb = NULL, *ncb = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* grab the current server time */
   secs = e_comp_wl_time_get();

   /* for each frame callback in the surface, signal it is done */
   wl_list_for_each_safe(cb, ncb, &ews->wl.frames, wl.link)
     {
        wl_callback_send_done(cb->wl.resource, secs);
        wl_resource_destroy(cb->wl.resource);
     }
}

static void 
_e_wl_shell_shell_surface_cb_focus_in(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Input *input = NULL;
   Eina_List *l = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* if this surface is not visible, get out */
   if (!ews->mapped) return;

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
_e_wl_shell_shell_surface_cb_focus_out(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Input *input = NULL;
   Eina_List *l = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

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
_e_wl_shell_shell_surface_cb_mouse_in(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* if this surface is not visible, get out */
   if (!ews->mapped) return;

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
}

static void 
_e_wl_shell_shell_surface_cb_mouse_out(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* if (!ews->input) return; */

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        /* if we have a pointer grab and this is the currently focused surface */
        /* if ((ptr->grab) && (ptr->focus == ptr->current)) */
        /*   return; */

        /* unset the pointer image */
        ecore_x_window_cursor_set(ecore_evas_window_get(ews->ee), 0);

        /* set pointer focus */
        ptr->current = NULL;

        /* NB: Ideally, we should call this function to tell the 
         * pointer that nothing has focus, HOWEVER, when we do 
         * it breaks re-entrant focus of some wayland clients:
         * 
         * NB: I sent a patch for this already to the wayland devs */
        wl_pointer_set_focus(ptr, NULL, 0, 0);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Wayland_Surface *ews = NULL;
   Evas_Event_Mouse_Move *ev;
   struct wl_pointer *ptr = NULL;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        ptr->x = wl_fixed_from_int(ev->cur.output.x);
        ptr->y = wl_fixed_from_int(ev->cur.output.y);

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
_e_wl_shell_shell_surface_cb_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   Evas_Event_Mouse_Up *ev;
   int btn = 0;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        if (ev->button == 1)
          btn = ev->button + BTN_LEFT - 1; // BTN_LEFT
        else if (ev->button == 2)
          btn = BTN_MIDDLE;
        else if (ev->button == 3)
          btn = BTN_RIGHT;

        ptr->button_count--;

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, ev->timestamp, btn, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);

        if (ptr->button_count == 1)
          ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);
     }
}

static void 
_e_wl_shell_shell_surface_cb_mouse_down(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   Evas_Event_Mouse_Down *ev;
   int btn = 0;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

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
             ptr->x = wl_fixed_from_int(ev->output.x);
             ptr->y = wl_fixed_from_int(ev->output.y);

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
_e_wl_shell_shell_surface_cb_key_up(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Key_Up *ev;
   E_Wayland_Surface *ews = NULL;
   struct wl_keyboard *kbd;
   struct wl_keyboard_grab *grab;
   unsigned int key = 0, *end, *k;
   unsigned int serial = 0;
   xkb_keysym_t sym = XKB_KEY_NoSymbol;

   ev = event;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   /* try to get a reference to the input's keyboard */
   if (!(kbd = _e_wl_comp->input->wl.seat.keyboard)) return;

   /* does this keyboard have a focused surface ? */
   if (!kbd->focus) return;

   /* is the focused surface actually This surface ? */
   if (kbd->focus != ews->wl.surface) return;

   /* get the keycode for this key from X */
   sym = xkb_keysym_from_name(ev->keyname, 0);
   if (!sym) 
     sym = xkb_keysym_from_name(ev->keyname, XKB_KEYSYM_CASE_INSENSITIVE);

   key = sym - 8;

   end = (kbd->keys.data + kbd->keys.size);
   for (k = kbd->keys.data; k < end; k++)
     if ((*k == key)) *k = *--end;

   kbd->keys.size = (void *)end - kbd->keys.data;

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
_e_wl_shell_shell_surface_cb_key_down(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
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

   /* get the keycode for this key from X */
   key = ecore_x_keysym_keycode_get(ev->keyname) - 8;

   /* update the keyboards grab properties */
   kbd->grab_key = key;
   kbd->grab_time = ev->timestamp;

   end = (kbd->keys.data + kbd->keys.size);
   for (k = kbd->keys.data; k < end; k++)
     {
        /* ignore server generated key repeats */
        if ((*k == key)) return;
     }

   kbd->keys.size = (void *)end - kbd->keys.data;
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

static void 
_e_wl_shell_shell_surface_cb_bd_move_end(void *data, void *bd)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   E_Border *border = NULL;

   /* FIXME: wayland move/resize with keyboard ? */

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   border = bd;
   if (border != ews->bd) return;

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
_e_wl_shell_shell_surface_cb_bd_resize_update(void *data, void *bd)
{
   /* E_Wayland_Shell_Grab *grab = NULL; */
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   E_Border *border = NULL;

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   border = bd;
   if (border != ews->bd) return;

   /* try to get the pointer from this input */
   if ((ptr = _e_wl_comp->input->wl.seat.pointer))
     {
        Evas_Coord w = 0, h = 0;

        if (!ptr->grab) return;

        if (ptr->grab->interface != &_e_resize_grab_interface)
          return;

        if (!ews->shell_surface) return;

        if ((ptr->current) && (ptr->current != ews->wl.surface)) return;

        /* if (!(grab = (E_Wayland_Shell_Grab *)ptr->grab)) return; */

        w = border->w;
        h = border->h;

        wl_shell_surface_send_configure(ews->shell_surface->wl.resource, 
                                        ptr->grab->edges, w, h);
     }
}

static void 
_e_wl_shell_shell_surface_cb_bd_resize_end(void *data, void *bd)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_pointer *ptr = NULL;
   E_Border *border = NULL;

   /* FIXME: wayland move/resize with keyboard ? */

   /* try to cast data to our surface structure */
   if (!(ews = data)) return;

   border = bd;
   if (border != ews->bd) return;

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

        /* send this button press to the pointer */
        ptr->grab->interface->button(ptr->grab, 
                                     ptr->grab_time,
                                     ptr->grab_button, 
                                     WL_POINTER_BUTTON_STATE_RELEASED);
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
        E_Wayland_Surface *ews = NULL;
        E_Wayland_Shell_Grab *grab = NULL;

        ews = ewss->surface;

        grab = (E_Wayland_Shell_Grab *)_e_wl_comp->input->wl.seat.pointer->grab;
        if (grab->grab.interface == &_e_busy_grab_interface)
          {
             _e_wl_shell_grab_end(grab);
             free(ews->input->wl.seat.pointer->grab);
          }
     }

   if (tmr->source) wl_event_source_remove(tmr->source);

   /* free the allocated space for ping timer */
   E_FREE(tmr);
   ewss->ping_timer = NULL;
}

static void
_e_wl_shell_mouse_down_helper(E_Border *bd, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev, Eina_Bool move)
{
   if ((button >= 1) && (button <= 3))
     {
        bd->mouse.last_down[button - 1].mx = output->x;
        bd->mouse.last_down[button - 1].my = output->y;
        bd->mouse.last_down[button - 1].x = bd->x;
        bd->mouse.last_down[button - 1].y = bd->y;
        bd->mouse.last_down[button - 1].w = bd->w;
        bd->mouse.last_down[button - 1].h = bd->h;
     }
   else
     {
        bd->moveinfo.down.x = bd->x;
        bd->moveinfo.down.y = bd->y;
        bd->moveinfo.down.w = bd->w;
        bd->moveinfo.down.h = bd->h;
     }
   bd->mouse.current.mx = output->x;
   bd->mouse.current.my = output->y;

   if (move)
     {
        /* tell E to start moving the border */
        e_border_act_move_begin(bd, ev);

        /* we have to get a reference to the window_move action here, or else 
         * when e_border stops the move we will never get notified */
        bd->cur_mouse_action = e_action_find("window_move");
        if (bd->cur_mouse_action)
          e_object_ref(E_OBJECT(bd->cur_mouse_action));
     }
   else
     {
        /* tell E to start resizing the border */
        e_border_act_resize_begin(bd, ev);

        /* we have to get a reference to the window_resize action here, 
         * or else when e_border stops the resize we will never get notified */
        bd->cur_mouse_action = e_action_find("window_resize");
        if (bd->cur_mouse_action)
          e_object_ref(E_OBJECT(bd->cur_mouse_action));
     }

   e_focus_event_mouse_down(bd);

   if ((button >= 1) && (button <= 3))
     {
        bd->mouse.last_down[button - 1].mx = output->x;
        bd->mouse.last_down[button - 1].my = output->y;
        bd->mouse.last_down[button - 1].x = bd->x;
        bd->mouse.last_down[button - 1].y = bd->y;
        bd->mouse.last_down[button - 1].w = bd->w;
        bd->mouse.last_down[button - 1].h = bd->h;
     }
   else
     {
        bd->moveinfo.down.x = bd->x;
        bd->moveinfo.down.y = bd->y;
        bd->moveinfo.down.w = bd->w;
        bd->moveinfo.down.h = bd->h;
     }
   bd->mouse.current.mx = output->x;
   bd->mouse.current.my = output->y;
}

static void 
_e_wl_shell_mouse_up_helper(E_Border *bd, int button, Evas_Point *output, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   if ((button >= 1) && (button <= 3))
     {
        bd->mouse.last_up[button - 1].mx = output->x;
        bd->mouse.last_up[button - 1].my = output->y;
        bd->mouse.last_up[button - 1].x = bd->x;
        bd->mouse.last_up[button - 1].y = bd->y;
     }
   bd->mouse.current.mx = output->x;
   bd->mouse.current.my = output->y;
   if ((button >= 1) && (button <= 3))
     {
        bd->mouse.last_up[button - 1].mx = output->x;
        bd->mouse.last_up[button - 1].my = output->y;
        bd->mouse.last_up[button - 1].x = bd->x;
        bd->mouse.last_up[button - 1].y = bd->y;
     }

   bd->drag.start = 0;
}

static void 
_e_wl_shell_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Shell_Grab *grab = NULL;
   E_Binding_Event_Mouse_Button *ev;
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

   /* create a fake binding event for mouse button */
   ev = E_NEW(E_Binding_Event_Mouse_Button, 1);

   /* set button property of the binding event */
   if (grab->pointer->grab_button == BTN_LEFT)
     ev->button = 1;
   else if (grab->pointer->grab_button == BTN_MIDDLE)
     ev->button = 2;
   else if (grab->pointer->grab_button == BTN_RIGHT)
     ev->button = 3;

   /* set the clicked location in the binding event */
   ev->canvas.x = wl_fixed_to_int(ptr->x) + 
     (ewss->surface->bd->x + ewss->surface->bd->client_inset.l);
   ev->canvas.y = wl_fixed_to_int(ptr->y) + 
     (ewss->surface->bd->y + ewss->surface->bd->client_inset.t);

   /* call our helper function to initiate a move */
   _e_wl_shell_mouse_down_helper(ewss->surface->bd, ev->button, 
                                 &(Evas_Point){ev->canvas.x, ev->canvas.y}, 
                                 ev, EINA_TRUE);
}

static void 
_e_wl_shell_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, unsigned int edges)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Shell_Grab *grab = NULL;
   E_Binding_Event_Mouse_Button *ev;
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

   /* create a fake binding event for mouse button */
   ev = E_NEW(E_Binding_Event_Mouse_Button, 1);

   if (grab->pointer->grab_button == BTN_LEFT)
     ev->button = 1;
   else if (grab->pointer->grab_button == BTN_MIDDLE)
     ev->button = 2;
   else if (grab->pointer->grab_button == BTN_RIGHT)
     ev->button = 3;

   /* set the clicked location in the binding event */
   ev->canvas.x = wl_fixed_to_int(ptr->x) + 
     (ewss->surface->bd->x + ewss->surface->bd->client_inset.l);
   ev->canvas.y = wl_fixed_to_int(ptr->y) + 
     (ewss->surface->bd->y + ewss->surface->bd->client_inset.t);

   /* call our helper function to initiate a resize */
   _e_wl_shell_mouse_down_helper(ewss->surface->bd, ev->button, 
                                 &(Evas_Point){ev->canvas.x, ev->canvas.y}, 
                                 ev, EINA_FALSE);
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
    * Appears to be some recent issue with e_border...
    * 
    * Needs looking into */
   return;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL;

   /* check for valid border */
   if (ewss->surface->bd)
     {
        /* ecore_evas_fullscreen_set(ewss->surface->ee, EINA_TRUE); */

        /* tell E to fullscreen this window */
        e_border_fullscreen(ewss->surface->bd, E_FULLSCREEN_RESIZE);

        /* send configure message to the shell surface to inform of new size */
        wl_shell_surface_send_configure(ewss->wl.resource, 0, 
                                        ewss->surface->bd->w, 
                                        ewss->surface->bd->h);
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

   /* check for valid border */
   if (ewss->surface->bd)
     {
        unsigned int edges = 0;

        edges = (WL_SHELL_SURFACE_RESIZE_TOP | WL_SHELL_SURFACE_RESIZE_LEFT);

        /* tell E to maximize this window */
        e_border_maximize(ewss->surface->bd, 
                          (e_config->maximize_policy & E_MAXIMIZE_TYPE) | 
                          E_MAXIMIZE_BOTH);

        /* send configure message to the shell surface to inform of new size */
        wl_shell_surface_send_configure(ewss->wl.resource, edges, 
                                        ewss->surface->bd->w, 
                                        ewss->surface->bd->h);
     }
}

static void 
_e_wl_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* free any previous title */
   free(ewss->title);

   /* set new title */
   ewss->title = strdup(title);

   /* update ecore_evas with new title */
   if ((ews = ewss->surface))
     {
        if (ews->ee)
          ecore_evas_title_set(ews->ee, ewss->title);
     }
}

static void 
_e_wl_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = wl_resource_get_user_data(resource)))
     return;

   /* free any previous class */
   free(ewss->clas);

   /* set new class */
   ewss->clas = strdup(clas);

   /* update ecore_evas with new class */
   if ((ews = ewss->surface))
     {
        if (ews->ee)
          ecore_evas_name_class_set(ews->ee, ewss->title, ewss->clas);
     }
}

/* shell move_grab interface functions */
static void 
_e_wl_shell_move_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* safety */
   if (!grab) return;

   /* remove focus */
   grab->focus = NULL;
}

static void 
_e_wl_shell_move_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* FIXME: This needs to become a no-op as the actual surface movement 
    * is handled by e_border now */
}

static void 
_e_wl_shell_move_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button EINA_UNUSED, unsigned int state)
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
     ptr->button_count--;
   else
     ptr->button_count++;

   if (ptr->button_count == 1)
     ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);

   /* test if we are done with the grab */
   if ((ptr->button_count == 0) && 
       (state == WL_POINTER_BUTTON_STATE_RELEASED))
     {
        E_Wayland_Surface *ews = NULL;
        E_Binding_Event_Mouse_Button *ev;

        if (!(ews = ewsg->shell_surface->surface)) return;

        /* create a fake binding event for mouse button */
        ev = E_NEW(E_Binding_Event_Mouse_Button, 1);

        /* set button property of the binding event */
        if (ptr->grab_button == BTN_LEFT)
          ev->button = 1;
        else if (ptr->grab_button == BTN_MIDDLE)
          ev->button = 2;
        else if (ptr->grab_button == BTN_RIGHT)
          ev->button = 3;

        /* set the clicked location in the binding event */
        ev->canvas.x = wl_fixed_to_int(ptr->x) + 
          (ews->bd->x + ews->bd->client_inset.l);
        ev->canvas.y = wl_fixed_to_int(ptr->y) + 
          (ews->bd->y + ews->bd->client_inset.t);

        /* call our helper function to end a move */
        _e_wl_shell_mouse_up_helper(ews->bd, ev->button, 
                                    &(Evas_Point){ev->canvas.x, ev->canvas.y}, 
                                    ev);

        /* end the grab */
        _e_wl_shell_grab_end(ewsg);
        free(grab);

        /* set surface geometry */
        _e_wl_shell_shell_surface_configure(ews, ews->bd->x, ews->bd->y, 
                                            ews->geometry.w, 
                                            ews->geometry.h);
     }
}

/* shell resize_grab interface functions */
static void 
_e_wl_shell_resize_grab_cb_focus(struct wl_pointer_grab *grab, struct wl_resource *surface EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* safety */
   if (!grab) return;

   /* remove focus */
   grab->focus = NULL;
}

static void 
_e_wl_shell_resize_grab_cb_motion(struct wl_pointer_grab *grab EINA_UNUSED, unsigned int timestamp EINA_UNUSED, wl_fixed_t x EINA_UNUSED, wl_fixed_t y EINA_UNUSED)
{
   /* FIXME: This needs to become a no-op as the actual surface resize 
    * is handled by e_border now */
}

static void 
_e_wl_shell_resize_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp EINA_UNUSED, unsigned int button EINA_UNUSED, unsigned int state)
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
     ptr->button_count--;
   else
     ptr->button_count++;

   if (ptr->button_count == 1)
     ptr->grab_serial = wl_display_get_serial(_e_wl_comp->wl.display);

   /* test if we are done with the grab */
   if ((ptr->button_count == 0) && 
       (state == WL_POINTER_BUTTON_STATE_RELEASED))
     {
        E_Wayland_Surface *ews = NULL;
        E_Binding_Event_Mouse_Button *ev;

        if (!(ews = ewsg->shell_surface->surface)) return;

        /* create a fake binding event for mouse button */
        ev = E_NEW(E_Binding_Event_Mouse_Button, 1);

        /* set button property of the binding event */
        if (ptr->grab_button == BTN_LEFT)
          ev->button = 1;
        else if (ptr->grab_button == BTN_MIDDLE)
          ev->button = 2;
        else if (ptr->grab_button == BTN_RIGHT)
          ev->button = 3;

        /* set the clicked location in the binding event */
        ev->canvas.x = wl_fixed_to_int(ptr->x) + 
          (ews->bd->x + ews->bd->client_inset.l);
        ev->canvas.y = wl_fixed_to_int(ptr->y) + 
          (ews->bd->y + ews->bd->client_inset.t);

        /* call our helper function to end a move */
        _e_wl_shell_mouse_up_helper(ews->bd, ev->button, 
                                    &(Evas_Point){ev->canvas.x, ev->canvas.y}, 
                                    ev);

        /* end the grab */
        _e_wl_shell_grab_end(ewsg);
        free(grab);

        /* set surface geometry */
        _e_wl_shell_shell_surface_configure(ews, ews->bd->x, ews->bd->y, 
                                            ews->geometry.w, 
                                            ews->geometry.h);
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

   if ((res = grab->pointer->focus_resource))
     wl_pointer_send_motion(res, timestamp, x, y);
}

static void 
_e_wl_shell_popup_grab_cb_button(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   struct wl_resource *res;

   /* try to get the shell surface */
   ewss = container_of(grab, E_Wayland_Shell_Surface, popup.grab);
   if (!ewss) return;

   if ((res = grab->pointer->focus_resource))
     {
        struct wl_display *disp;
        unsigned int serial;

        disp = wl_client_get_display(wl_resource_get_client(res));
        serial = wl_display_get_serial(disp);

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
   if ((ewsg->grab.focus != surface))
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
