#include "e.h"
#include "e_mod_main.h"

/* local function prototypes */
static void _e_desktop_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_desktop_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource);
static void _e_desktop_shell_shell_surface_cb_destroy_notify(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_desktop_shell_shell_surface_cb_destroy(struct wl_resource *resource);
static void _e_desktop_shell_shell_surface_configure(E_Surface *es, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_desktop_shell_shell_surface_map(E_Surface *es, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_desktop_shell_shell_surface_unmap(E_Surface *es);

/* shell surface interface prototypes */
static void _e_desktop_shell_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int serial);
static void _e_desktop_shell_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial);
static void _e_desktop_shell_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, unsigned int edges);
static void _e_desktop_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_desktop_shell_shell_surface_cb_transient_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource, int x, int y, unsigned int flags);
static void _e_desktop_shell_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int method EINA_UNUSED, unsigned int framerate EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED);
static void _e_desktop_shell_shell_surface_cb_popup_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, struct wl_resource *parent_resource, int x, int y, unsigned int flags EINA_UNUSED);
static void _e_desktop_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED);
static void _e_desktop_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title);
static void _e_desktop_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas);
static void _e_desktop_shell_shell_surface_type_set(E_Shell_Surface *ess);
static void _e_desktop_shell_shell_surface_type_reset(E_Shell_Surface *ess);

static void _e_desktop_shell_surface_map_toplevel(E_Shell_Surface *ess);
static void _e_desktop_shell_surface_map_popup(E_Shell_Surface *ess);
static void _e_desktop_shell_popup_grab_add(E_Shell_Surface *ess, E_Input *seat);

static void _e_desktop_shell_popup_grab_focus(E_Input_Pointer_Grab *grab);
static void _e_desktop_shell_popup_grab_motion(E_Input_Pointer_Grab *grab, unsigned int timestamp);
static void _e_desktop_shell_popup_grab_button(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);
static void _e_desktop_shell_popup_grab_end(E_Input_Pointer *pointer);

/* local wayland interfaces */
static const struct wl_shell_interface _e_desktop_shell_interface = 
{
   _e_desktop_shell_cb_shell_surface_get
};

static const struct wl_shell_surface_interface 
_e_desktop_shell_surface_interface = 
{
   _e_desktop_shell_shell_surface_cb_pong,
   _e_desktop_shell_shell_surface_cb_move,
   _e_desktop_shell_shell_surface_cb_resize,
   _e_desktop_shell_shell_surface_cb_toplevel_set,
   _e_desktop_shell_shell_surface_cb_transient_set,
   _e_desktop_shell_shell_surface_cb_fullscreen_set,
   _e_desktop_shell_shell_surface_cb_popup_set,
   _e_desktop_shell_shell_surface_cb_maximized_set,
   _e_desktop_shell_shell_surface_cb_title_set,
   _e_desktop_shell_shell_surface_cb_class_set
};

static E_Input_Pointer_Grab_Interface _popup_grab_interface = 
{
   _e_desktop_shell_popup_grab_focus,
   _e_desktop_shell_popup_grab_motion,
   _e_desktop_shell_popup_grab_button,
};

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Desktop_Shell *shell;
   struct wl_global *global;

   /* try to allocate space for our shell */
   if (!(shell = E_NEW(E_Desktop_Shell, 1)))
     return NULL;

   /* get a reference to the compositor */
   shell->compositor = _e_comp;

   /* setup shell destroy listener */
   shell->wl.destroy_listener.notify = _e_desktop_shell_cb_destroy;
   wl_signal_add(&_e_comp->signals.destroy, &shell->wl.destroy_listener);

   /* set a reference to this shell in the compositor */
   shell->compositor->shell_interface.shell = shell;

   /* try to add this shell to the globals */
   if (!(global = 
         wl_display_add_global(_e_comp->wl.display, &wl_shell_interface, 
                               shell, _e_desktop_shell_cb_bind)))
     {
        ERR("Could not add shell to globals: %m");
        goto err;
     }

   /* TODO: finish me */

   return m;

err:
   /* reset compositor shell interface */
   _e_comp->shell_interface.shell = NULL;

   /* remove the destroy signal */
   wl_list_remove(&shell->wl.destroy_listener.link);

   /* free the allocated structure */
   E_FREE(shell);

   return NULL;
}

EAPI int 
e_modapi_shutdown(E_Module *m)
{
   return 1;
}

/* local functions */
static void 
_e_desktop_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Desktop_Shell *shell;

   /* try to get the shell from the listener */
   shell = container_of(listener, E_Desktop_Shell, wl.destroy_listener);

   /* reset compositor shell interface */
   shell->compositor->shell_interface.shell = NULL;

   /* remove the destroy signal */
   wl_list_remove(&shell->wl.destroy_listener.link);

   /* free the allocated structure */
   E_FREE(shell);
}

static void 
_e_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Desktop_Shell *shell;

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the shell to the client */
   wl_client_add_object(client, &wl_shell_interface, 
                        &_e_desktop_shell_interface, id, shell);
}

static void 
_e_desktop_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource)
{
   E_Surface *es;
   E_Shell_Surface *ess;

   /* try to cast the resource to our structure */
   if (!(es = surface_resource->data)) return;

   /* check if this surface has already been configured */
   if ((es->configure) && 
       (es->configure == _e_desktop_shell_shell_surface_configure))
     {
        wl_resource_post_error(surface_resource, 
                               WL_DISPLAY_ERROR_INVALID_OBJECT, 
                               "Shell surface already requested for surface");
        return;
     }

   /* try to create new shell surface */
   if (!(ess = e_shell_surface_new(es, id)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   es->configure = _e_desktop_shell_shell_surface_configure;
   es->map = _e_desktop_shell_shell_surface_map;
   es->unmap = _e_desktop_shell_shell_surface_unmap;

   /* setup shell surface destroy callback */
   ess->wl.surface_destroy.notify = 
     _e_desktop_shell_shell_surface_cb_destroy_notify;
   wl_signal_add(&es->wl.resource.destroy_signal, 
                 &ess->wl.surface_destroy);

   /* setup shell surface interface */
   ess->wl.resource.destroy = _e_desktop_shell_shell_surface_cb_destroy;
   ess->wl.resource.object.implementation = 
     (void (**)(void))&_e_desktop_shell_surface_interface;

   /* add this shell surface to the client */
   wl_client_add_resource(client, &ess->wl.resource);
}

static void 
_e_desktop_shell_shell_surface_cb_destroy_notify(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Shell_Surface *ess;

   /* try to get the shell surface from the listener */
   if (!(ess = container_of(listener, E_Shell_Surface, wl.surface_destroy)))
     return;

   if (ess->wl.resource.client)
     wl_resource_destroy(&ess->wl.resource);
   else
     wl_signal_emit(&ess->wl.resource.destroy_signal, &ess->wl.resource);
}

static void 
_e_desktop_shell_shell_surface_cb_destroy(struct wl_resource *resource)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* TODO: finish me */
   /* if we have a popup grab, end it */
   /* if (!wl_list_empty(&ess->popup.grabs)) */
   /*   { */
   /*      wl_list_remove(&ess->popup.grabs); */
   /*      wl_list_init(&ess->popup.grabs); */
   /* } */

   wl_list_remove(&ess->wl.surface_destroy.link);
   ess->surface->configure = NULL;

   /* TODO: handle ping timer */

   wl_list_remove(&ess->wl.link);

   /* free the allocated structure */
   E_FREE(ess);
}

static void 
_e_desktop_shell_shell_surface_configure(E_Surface *es, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Shell_Surface *ess;
   Eina_Bool changed = EINA_FALSE;

   if ((es->configure == _e_desktop_shell_shell_surface_configure))
     {
        if (!(ess = es->shell_surface)) return;
     }
   else
     return;

   /* handle surface type change */
   if ((ess->ntype != E_SHELL_SURFACE_TYPE_NONE) && 
       (ess->type != ess->ntype))
     {
        /* set the new surface type */
        _e_desktop_shell_shell_surface_type_set(ess);

        changed = EINA_TRUE;
     }

   /* check if surface has been mapped */
   if (!es->mapped)
     {
        /* if we have a map function, then call it */
        if (es->map) es->map(es, x, y, w, h);
     }
   else if ((changed) || (x != 0) || (y != 0) || 
            (es->geometry.w != w) || (es->geometry.h != h))
     {
        Evas_Coord fx = 0, fy = 0, tx = 0, ty = 0;

        fx += es->geometry.x;
        fy += es->geometry.y;
        tx = (x + es->geometry.x);
        ty = (y + es->geometry.y);

        es->geometry.x = (es->geometry.x + tx - fx);
        es->geometry.y = (es->geometry.y + ty - fy);
        es->geometry.w = w;
        es->geometry.h = h;
        es->geometry.changed = EINA_TRUE;
     }
}

static void 
_e_desktop_shell_shell_surface_map(E_Surface *es, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   /* safety check */
   if (!es) return;

   /* update surface geometry */
   es->geometry.w = w;
   es->geometry.h = h;
   es->geometry.changed = EINA_TRUE;

   /* starting position */
   switch (es->shell_surface->type)
     {
      case E_SHELL_SURFACE_TYPE_TOPLEVEL:
      case E_SHELL_SURFACE_TYPE_POPUP:
        es->geometry.x = x;
        es->geometry.y = y;
        es->geometry.changed = EINA_TRUE;
        break;
      case E_SHELL_SURFACE_TYPE_FULLSCREEN:
        /* center */
        /* map fullscreen */
        break;
      case E_SHELL_SURFACE_TYPE_MAXIMIZED:
        /* maximize */
        break;
      case E_SHELL_SURFACE_TYPE_NONE:
        es->geometry.x += x;
        es->geometry.y += y;
        es->geometry.changed = EINA_TRUE;
        break;
      default:
        break;
     }

   /* stacking */

   if (es->shell_surface->type != E_SHELL_SURFACE_TYPE_NONE)
     e_surface_output_assign(es);

   /* activate */
   switch (es->shell_surface->type)
     {
      case E_SHELL_SURFACE_TYPE_TOPLEVEL:
        _e_desktop_shell_surface_map_toplevel(es->shell_surface);
        break;
      case E_SHELL_SURFACE_TYPE_POPUP:
        _e_desktop_shell_surface_map_popup(es->shell_surface);
        break;
      default:
        break;
     }
}

static void 
_e_desktop_shell_shell_surface_unmap(E_Surface *es)
{
   /* Eina_List *l = NULL; */
   /* E_Wayland_Input *input; */

   if (!es) return;

   /* EINA_LIST_FOREACH(_e_wl_comp->seats, l, input) */
   /*   { */
   /*      if ((input->wl.seat.keyboard) &&  */
   /*          (input->wl.seat.keyboard->focus == &es->wl.surface)) */
   /*        wl_keyboard_set_focus(input->wl.seat.keyboard, NULL); */
   /*      if ((input->wl.seat.pointer) &&  */
   /*          (input->wl.seat.pointer->focus == &es->wl.surface)) */
   /*        wl_pointer_set_focus(input->wl.seat.pointer, NULL, 0, 0); */
   /*   } */

   /* if (es->ee) ecore_evas_free(es->ee); */

   es->mapped = EINA_FALSE;
}

/* shell surface interface functions */
static void 
_e_desktop_shell_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int serial)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;
   printf("Shell Surface Pong\n");
}

static void 
_e_desktop_shell_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;
   printf("Shell Surface Move\n");
}

static void 
_e_desktop_shell_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, unsigned int edges)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;
   printf("Shell Surface Resize\n");
}

static void 
_e_desktop_shell_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* set next surface type */
   ess->ntype = E_SHELL_SURFACE_TYPE_TOPLEVEL;
}

static void 
_e_desktop_shell_shell_surface_cb_transient_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource, int x, int y, unsigned int flags)
{
   E_Shell_Surface *ess;
   E_Surface *es;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   es = parent_resource->data;

   ess->parent = es;
   ess->transient.x = x;
   ess->transient.y = y;
   ess->transient.flags = flags;

   /* set next surface type */
   ess->ntype = E_SHELL_SURFACE_TYPE_TRANSIENT;
}

static void 
_e_desktop_shell_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, unsigned int method EINA_UNUSED, unsigned int framerate EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* set next surface type */
   ess->ntype = E_SHELL_SURFACE_TYPE_FULLSCREEN;
}

static void 
_e_desktop_shell_shell_surface_cb_popup_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource, unsigned int serial, struct wl_resource *parent_resource, int x, int y, unsigned int flags EINA_UNUSED)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* set next surface type */
   ess->ntype = E_SHELL_SURFACE_TYPE_POPUP;
   ess->parent = parent_resource->data;
   ess->popup.seat = seat_resource->data;
   ess->popup.serial = serial;
   ess->popup.x = x;
   ess->popup.y = y;
}

static void 
_e_desktop_shell_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Shell_Surface *ess;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* set next surface type */
   ess->ntype = E_SHELL_SURFACE_TYPE_MAXIMIZED;
}

static void 
_e_desktop_shell_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Shell_Surface *ess;
   E_Surface *es;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* free any previous title */
   free(ess->title);

   ess->title = strdup(title);

   if ((es = ess->surface))
     {
        /* TODO: Finish me */
     }
}

static void 
_e_desktop_shell_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas)
{
   E_Shell_Surface *ess;
   E_Surface *es;

   /* try to cast the resource to our shell surface */
   if (!(ess = resource->data)) return;

   /* free any previous title */
   free(ess->clas);

   ess->clas = strdup(clas);

   if ((es = ess->surface))
     {
        /* TODO: Finish me */
     }
}

static void 
_e_desktop_shell_shell_surface_type_set(E_Shell_Surface *ess)
{
   /* safety check */
   if (!ess) return;

   /* reset the shell surface type */
   _e_desktop_shell_shell_surface_type_reset(ess);

   ess->type = ess->ntype;
   ess->ntype = E_SHELL_SURFACE_TYPE_NONE;

   switch (ess->type)
     {
      case E_SHELL_SURFACE_TYPE_TRANSIENT:
        ess->surface->geometry.x = 
          ess->parent->geometry.x + ess->transient.x;
        ess->surface->geometry.y = 
          ess->parent->geometry.y + ess->transient.y;
        break;
        /* record the current geometry so we can restore it */
      case E_SHELL_SURFACE_TYPE_FULLSCREEN:
      case E_SHELL_SURFACE_TYPE_MAXIMIZED:
        ess->saved.x = ess->surface->geometry.x;
        ess->saved.y = ess->surface->geometry.y;
        ess->saved.w = ess->surface->geometry.w;
        ess->saved.h = ess->surface->geometry.h;
        ess->saved.valid = EINA_TRUE;
        break;
      default:
        break;
     }
}

static void 
_e_desktop_shell_shell_surface_type_reset(E_Shell_Surface *ess)
{
   /* safety check */
   if (!ess) return;

   switch (ess->type)
     {
      case E_SHELL_SURFACE_TYPE_FULLSCREEN:
        /* ecore_evas_fullscreen_set(ess->surface->ee, EINA_FALSE); */
        /* unfullscreen the border */
        /* if (ess->surface->bd) */
        /*   e_border_unfullscreen(ess->surface->bd); */

        /* restore the saved geometry */
        ess->surface->geometry.x = ess->saved.x;
        ess->surface->geometry.y = ess->saved.y;
        ess->surface->geometry.w = ess->saved.w;
        ess->surface->geometry.h = ess->saved.h;
        ess->surface->geometry.changed = EINA_TRUE;
        break;
      case E_SHELL_SURFACE_TYPE_MAXIMIZED:
        /* unmaximize the border */
        /* if (ess->surface->bd) */
        /*   e_border_unmaximize(ess->surface->bd,  */
        /*                       (e_config->maximize_policy & E_MAXIMIZE_TYPE) |  */
        /*                       E_MAXIMIZE_BOTH); */

        /* restore the saved geometry */
        ess->surface->geometry.x = ess->saved.x;
        ess->surface->geometry.y = ess->saved.y;
        ess->surface->geometry.w = ess->saved.w;
        ess->surface->geometry.h = ess->saved.h;
        ess->surface->geometry.changed = EINA_TRUE;
        break;
      default:
        break;
     }

   /* reset the current type to none */
   ess->type = E_SHELL_SURFACE_TYPE_NONE;
}

static void 
_e_desktop_shell_surface_map_toplevel(E_Shell_Surface *ess)
{
   /* if (!(es->ee =  */
   /*       e_canvas_new(0, es->geometry.x, es->geometry.y,  */
   /*                    es->geometry.w, es->geometry.h, EINA_FALSE,  */
   /*                    EINA_FALSE, NULL))) */
   /*   return; */
   /* e_canvas_add(es->ee); */
   /* ecore_evas_data_set(es->ee, "surface", es); */
   /* es->evas = ecore_evas_get(es->ee); */
}

static void 
_e_desktop_shell_surface_map_popup(E_Shell_Surface *ess)
{
   E_Surface *es, *parent;
   E_Input *seat;

   es = ess->surface;
   parent = ess->parent;

   seat = ess->popup.seat;

   /* set popup position */
   es->geometry.x = ess->popup.x;
   es->geometry.y = ess->popup.y;
   es->geometry.changed = EINA_TRUE;

   if ((seat) && (seat->pointer->grab->serial == ess->popup.serial))
     _e_desktop_shell_popup_grab_add(ess, seat);
   else
     {
        wl_shell_surface_send_popup_done(&ess->wl.resource);
        seat->pointer->grab->client = NULL;
     }
}

static void 
_e_desktop_shell_popup_grab_add(E_Shell_Surface *ess, E_Input *seat)
{
   if (wl_list_empty(&seat->pointer->grab->surfaces))
     {
        seat->pointer->grab->client = ess->wl.resource.client;
        seat->pointer->grab->interface = &_popup_grab_interface;

        if (seat->pointer->grab->button_count > 0)
          seat->pointer->grab->up = EINA_FALSE;

        e_input_pointer_grab_start(seat->pointer);
     }

   wl_list_insert(&seat->pointer->grab->surfaces, &ess->wl.link);
}

static void 
_e_desktop_shell_popup_grab_focus(E_Input_Pointer_Grab *grab)
{
   E_Input_Pointer *ptr;
   E_Surface *es;
   struct wl_client *client;

   if (!(ptr = grab->pointer)) return;

   es = e_compositor_surface_find(ptr->seat->compositor, ptr->x, ptr->y);

   if ((es) && (es->wl.resource.client == grab->client))
     e_input_pointer_focus_set(ptr, es, ptr->x, ptr->y);
   else
     e_input_pointer_focus_set(ptr, NULL, 0, 0);
}

static void 
_e_desktop_shell_popup_grab_motion(E_Input_Pointer_Grab *grab, unsigned int timestamp)
{
   E_Input_Pointer *ptr;

   ptr = grab->pointer;
   if ((ptr) && (ptr->focus_resource))
     wl_pointer_send_motion(ptr->focus_resource, timestamp, ptr->x, ptr->y);
}

static void 
_e_desktop_shell_popup_grab_button(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   struct wl_resource *res;

   if ((res = grab->pointer->focus_resource))
     {
        struct wl_display *disp;
        unsigned int serial = 0;

        disp = wl_client_get_display(res->client);
        serial = wl_display_get_serial(disp);
        wl_pointer_send_button(res, serial, timestamp, button, state);
     }
   else if ((state == WL_POINTER_BUTTON_STATE_RELEASED) && 
            ((grab->up) || ((timestamp - grab->timestamp) > 500)))
     _e_desktop_shell_popup_grab_end(grab->pointer);

   if (state == WL_POINTER_BUTTON_STATE_RELEASED)
     grab->up = EINA_TRUE;
}

static void 
_e_desktop_shell_popup_grab_end(E_Input_Pointer *pointer)
{
   E_Input_Pointer_Grab *grab;

   if (!(grab = pointer->grab)) return;

   if (grab->interface == &_popup_grab_interface)
     {
        E_Shell_Surface *ess;

        e_input_pointer_grab_end(grab->pointer);
        grab->client = NULL;
        grab->interface = NULL;

        wl_list_for_each(ess, &grab->surfaces, wl.link)
          wl_shell_surface_send_popup_done(&ess->wl.resource);

        wl_list_init(&grab->surfaces);
     }
}
