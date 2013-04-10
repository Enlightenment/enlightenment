#include "e.h"
#include "e_comp_wl.h"
#include "e_surface.h"
#include "e_mod_main.h"
#include "e_desktop_shell_protocol.h"

/* shell function prototypes */
static void _e_wl_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_wl_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_wl_shell_cb_ping(E_Wayland_Surface *ews, unsigned int serial);
static void _e_wl_shell_cb_pointer_focus(struct wl_listener *listener EINA_UNUSED, void *data);

/* shell interface prototypes */
static void _e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource);

/* desktop shell function prototypes */
static void _e_wl_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_wl_desktop_shell_cb_unbind(struct wl_resource *resource);

/* desktop shell interface prototypes */

/* shell surface function prototypes */
static E_Wayland_Shell_Surface *_e_wl_shell_shell_surface_create(void *shell EINA_UNUSED, E_Wayland_Surface *ews, const void *client EINA_UNUSED);
static void _e_wl_shell_shell_surface_destroy(struct wl_resource *resource);
static void _e_wl_shell_shell_surface_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_wl_shell_shell_surface_map(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_wl_shell_shell_surface_unmap(E_Wayland_Surface *ews);
static void _e_wl_shell_shell_surface_type_set(E_Wayland_Shell_Surface *ewss);
static void _e_wl_shell_shell_surface_type_reset(E_Wayland_Shell_Surface *ewss);
static void _e_wl_shell_shell_surface_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static int _e_wl_shell_shell_surface_cb_ping_timeout(void *data);
static void _e_wl_shell_shell_surface_cb_render_post(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED);

/* shell surface interface prototypes */
static void _e_wl_shell_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int serial);
static void _e_wl_shell_shell_surface_cb_move(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial);
static void _e_wl_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_wl_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED);
static void _e_wl_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title);
static void _e_wl_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas);

/* local wayland interfaces */
static const struct wl_shell_interface _e_shell_interface = 
{
   _e_wl_shell_cb_shell_surface_get
};

static const struct e_desktop_shell_interface _e_desktop_shell_interface = 
{
   NULL, // desktop_background_set
   NULL, // desktop_panel_set
   NULL, // desktop_lock_surface_set
   NULL, // desktop_unlock
   NULL // _e_wl_shell_cb_shell_grab_surface_set
};

static const struct wl_shell_surface_interface _e_shell_surface_interface = 
{
   _e_wl_shell_shell_surface_cb_pong,
   _e_wl_shell_shell_surface_cb_move,
   NULL, // resize
   _e_wl_shell_shell_surface_cb_toplevel_set,
   NULL, // transient_set
   NULL, // fullscreen_set
   NULL, // popup_set
   _e_wl_shell_shell_surface_cb_maximized_set,
   _e_wl_shell_shell_surface_cb_title_set,
   _e_wl_shell_shell_surface_cb_class_set
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

   /* try to add this shell to the display's global list */
   if (!(gshell = 
         wl_display_add_global(_e_wl_comp->wl.display, &wl_shell_interface, 
                               shell, _e_wl_shell_cb_bind)))
     goto err;

   /* try to add the desktop shell interface to the display's global list */
   if (!wl_display_add_global(_e_wl_comp->wl.display, 
                              &e_desktop_shell_interface, shell, 
                              _e_wl_desktop_shell_cb_bind))
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
   if (gshell) wl_display_remove_global(_e_wl_comp->wl.display, gshell);

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

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the shell to the client */
   wl_client_add_object(client, &wl_shell_interface, 
                        &_e_shell_interface, id, shell);
}

static void 
_e_wl_shell_cb_ping(E_Wayland_Surface *ews, unsigned int serial)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   if (!ews) return;

   /* try to cast to our shell surface */
   if (!(ewss = ews->shell_surface)) return;

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
                                     e_config->ping_clients_interval);

        ewss->ping_timer = tmr;

        /* send the ping to the shell surface */
        wl_shell_surface_send_ping(&ewss->wl.resource, serial);
     }
}

static void 
_e_wl_shell_cb_pointer_focus(struct wl_listener *listener EINA_UNUSED, void *data)
{
   struct wl_pointer *ptr;
   E_Wayland_Surface *ews = NULL;

   if (!(ptr = data)) return;

   /* try to cast the current pointer focus to a wayland surface */
   if (!(ews = (E_Wayland_Surface *)ptr->focus)) return;

   /* if this surface has a shell_surface and it is not active, then we 
    * should set the busy cursor on it */
   if ((ews->shell_surface) && (!ews->shell_surface->active))
     {
        /* TODO: handle busy cursor */
     }
   else
     {
        unsigned int serial = 0;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        /* ping the surface */
        _e_wl_shell_cb_ping(ews, serial);
     }
}

/* shell interface functions */
static void 
_e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the surface resource to our structure */
   if (!(ews = surface_resource->data)) return;

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
   if (!(ewss = _e_wl_shell_shell_surface_create(NULL, ews, NULL)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   ewss->wl.resource.destroy = _e_wl_shell_shell_surface_destroy;
   ewss->wl.resource.object.id = id;
   ewss->wl.resource.object.interface = &wl_shell_surface_interface;
   ewss->wl.resource.object.implementation = 
     (void (**)(void))&_e_shell_surface_interface;
   ewss->wl.resource.data = ewss;

   /* add this shell surface to the client */
   wl_client_add_resource(client, &ewss->wl.resource);
}

/* desktop shell functions */
static void 
_e_wl_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_resource *res = NULL;

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the desktop shell to the client */
   if (!(res = wl_client_add_object(client, &e_desktop_shell_interface, 
                                    &_e_desktop_shell_interface, id, shell)))
     {
        wl_resource_post_error(res, WL_DISPLAY_ERROR_INVALID_OBJECT, 
                               "Permission Denied");
        wl_resource_destroy(res);
        return;
     }

   shell->wl.resource = res;

   /* set desktop shell destroy callback */
   res->destroy = _e_wl_desktop_shell_cb_unbind;
}

static void 
_e_wl_desktop_shell_cb_unbind(struct wl_resource *resource)
{
   free(resource);
}

/* desktop shell interface functions */

/* shell surface functions */
static E_Wayland_Shell_Surface *
_e_wl_shell_shell_surface_create(void *shell EINA_UNUSED, E_Wayland_Surface *ews, const void *client EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to allocate space for our shell surface structure */
   if (!(ewss = E_NEW(E_Wayland_Shell_Surface, 1)))
     return NULL;

   /* set some function pointers on the surface */
   ews->configure = _e_wl_shell_shell_surface_configure;
   ews->map = _e_wl_shell_shell_surface_map;
   ews->unmap = _e_wl_shell_shell_surface_unmap;

   /* set some properties on the shell surface */
   ewss->surface = ews;
   ews->shell_surface = ewss;

   /* setup shell surface destroy */
   wl_signal_init(&ewss->wl.resource.destroy_signal);
   ewss->wl.surface_destroy.notify = _e_wl_shell_shell_surface_cb_destroy;
   wl_signal_add(&ews->wl.surface.resource.destroy_signal, 
                 &ewss->wl.surface_destroy);

   wl_list_init(&ewss->wl.link);

   /* set some defaults on this shell surface */
   ewss->active = EINA_TRUE;
   ewss->type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_NONE;

   return ewss;
}

static void 
_e_wl_shell_shell_surface_destroy(struct wl_resource *resource)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = resource->data)) return;

   wl_list_remove(&ewss->wl.surface_destroy.link);
   ewss->surface->configure = NULL;

   wl_list_remove(&ewss->wl.link);

   /* try to free our allocated structure */
   E_FREE(ewss);
}

static void 
_e_wl_shell_shell_surface_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   Eina_Bool changed_type = EINA_FALSE;

   if ((ews->configure == _e_wl_shell_shell_surface_configure))
     {
        if (!(ewss = ews->shell_surface)) return;
     }
   else 
     return;

   printf("Configure Surface: %d %d %d %d\n", x, y, w, h);

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
   else if ((changed_type) || 
            (ews->geometry.x != x) || (ews->geometry.y != y) || 
            (ews->geometry.w != w) || (ews->geometry.h != h))
     {
        /* TODO */
        if ((ews->geometry.x != x) || (ews->geometry.y != y))
          {
             ews->geometry.x = x;
             ews->geometry.y = y;
             ews->geometry.changed = EINA_FALSE;

             if ((ews->bd) && 
                 (ewss->type != E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED))
               e_border_move(ews->bd, x, y);
          }

        if ((ews->geometry.w != w) || (ews->geometry.h != h))
          {
             ews->geometry.w = w;
             ews->geometry.h = h;
             ews->geometry.changed = EINA_FALSE;

             if ((ews->bd) && 
                 (ewss->type != E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED))
               e_border_resize(ews->bd, w, h);
          }
     }
}

static void 
_e_wl_shell_shell_surface_map(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Container *con = NULL;
   Evas *evas;

   /* safety check */
   if (!ews) return;

   /* update surface geometry */
   ews->geometry.x = x;
   ews->geometry.y = y;
   ews->geometry.w = w;
   ews->geometry.h = h;

   /* get the current container */
   con = e_container_current_get(e_manager_current_get());

   /* create an ecore evas to represent this 'window' */
   ews->ee = ecore_evas_new(NULL, x, y, w, h, NULL);
   ecore_evas_alpha_set(ews->ee, EINA_TRUE);
   ecore_evas_borderless_set(ews->ee, EINA_FALSE);

   /* get a reference to the canvas */
   evas = ecore_evas_get(ews->ee);

   /* setup a post_render callback */
   evas_event_callback_add(evas, EVAS_CALLBACK_RENDER_POST, 
                           _e_wl_shell_shell_surface_cb_render_post, ews);

   /* create a surface smart object */
   ews->obj = e_surface_add(evas);
   evas_object_move(ews->obj, 0, 0);
   evas_object_resize(ews->obj, w, h);
   evas_object_show(ews->obj);

   /* create the e border for this surface */
   ews->bd = e_border_new(con, ecore_evas_window_get(ews->ee), 1, 1);
   e_border_move_resize(ews->bd, x, y, w, h);
   e_border_show(ews->bd);

   ews->mapped = EINA_TRUE;
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
            (input->wl.seat.keyboard->focus == &ews->wl.surface))
          wl_keyboard_set_focus(input->wl.seat.keyboard, NULL);
        if ((input->wl.seat.pointer) && 
            (input->wl.seat.pointer->focus == &ews->wl.surface))
          wl_pointer_set_focus(input->wl.seat.pointer, NULL, 0, 0);
     }

   if (ews->obj) evas_object_del(ews->obj);
   if (ews->ee) ecore_evas_free(ews->ee);
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
        /* record the current geometry so we can restore it on unmaximize */
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

   if (ewss->wl.resource.client)
     wl_resource_destroy(&ewss->wl.resource);
   else
     wl_signal_emit(&ewss->wl.resource.destroy_signal, &ewss->wl.resource);
}

static int 
_e_wl_shell_shell_surface_cb_ping_timeout(void *data)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast to our shell surface structure */
   if (!(ewss = data)) return 1;

   ewss->active = EINA_FALSE;

   /* TODO: handle busy grab */

   return 1;
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
        wl_callback_send_done(&cb->wl.resource, secs);
        wl_resource_destroy(&cb->wl.resource);
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
   if (!(ewss = resource->data)) return;

   /* try to cast the ping timer */
   if (!(tmr = (E_Wayland_Ping_Timer *)ewss->ping_timer))
     return;

   if (tmr->serial != serial) return;

   responsive = ewss->active;
   ewss->active = EINA_TRUE;

   if (!responsive)
     {
        /* TODO: Handle busy grab */
     }

   if (tmr->source) wl_event_source_remove(tmr->source);

   /* free the allocated space for ping timer */
   E_FREE(tmr);
   ewss->ping_timer = NULL;
}

static void 
_e_wl_shell_shell_surface_cb_move(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial)
{
   /* TODO */
}

static void 
_e_wl_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = resource->data)) return;

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL;
}

static void 
_e_wl_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Wayland_Shell_Surface *ewss = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = resource->data)) return;

   /* TODO */

   /* set next surface type */
   ewss->next_type = E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED;
}

static void 
_e_wl_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Wayland_Shell_Surface *ewss = NULL;
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource to our shell surface */
   if (!(ewss = resource->data)) return;

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
   if (!(ewss = resource->data)) return;

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

