#include "e.h"
#include "e_mod_main.h"

/* local function prototypes */
static void _e_desktop_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_desktop_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource);
static void _e_desktop_shell_shell_surface_cb_destroy_notify(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_desktop_shell_shell_surface_cb_destroy(struct wl_resource *resource);
static void _e_desktop_shell_shell_surface_configure(E_Surface *es, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

/* local wayland interfaces */
static const struct wl_shell_interface _e_desktop_shell_interface = 
{
   _e_desktop_shell_cb_shell_surface_get
};

static const struct wl_shell_surface_interface 
_e_desktop_shell_surface_interface = 
{
   NULL, // pong
   NULL, // move
   NULL, // resize
   NULL, // toplevel_set
   NULL, // transient
   NULL, // fullscreen
   NULL, // popup
   NULL, // maximized
   NULL, // title
   NULL // class
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
   /* struct wl_resource *res; */

   /* try to cast data to our shell */
   if (!(shell = data)) return;

   /* try to add the shell to the client */
   wl_client_add_object(client, &wl_shell_interface, 
                        &_e_desktop_shell_interface, id, shell);

   /* TODO: finish */
}

static void 
_e_desktop_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource)
{
   E_Surface *es;
   E_Shell_Surface *ess;

   printf("Desktop_Shell: Shell Surface Get\n");

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
   /* ess->map = ; */
   /* ess->unmap = ; */

   /* setup shell surface destroy callback */
   ess->wl.surface_destroy.notify = 
     _e_desktop_shell_shell_surface_cb_destroy_notify;
   wl_signal_add(&es->wl.surface.resource.destroy_signal, 
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

   /* if we have a popup grab, end it */
   if (ess->popup.grab.pointer) wl_pointer_end_grab(ess->popup.grab.pointer);

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

   if ((es->configure == _e_desktop_shell_shell_surface_configure))
     {
        if (!(ess = es->shell_surface)) return;
     }

   /* TODO: finish me */
}
