#include "e.h"
#include "e_comp_wl.h"
#include "e_mod_main.h"
#include "e_desktop_shell_protocol.h"

/* shell function prototypes */
static void _e_wl_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_wl_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);

/* shell interface prototypes */
static void _e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource);

/* desktop shell function prototypes */
static void _e_wl_shell_cb_bind_desktop(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_wl_shell_cb_unbind_desktop(struct wl_resource *resource);

/* desktop shell interface prototypes */

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

/* local variables */

/* external variables */

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop_Shell" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Wayland_Desktop_Shell *shell = NULL;
   struct wl_global *gshell = NULL;

   /* try to allocate space for the shell structure */
   if (!(shell = E_NEW(E_Wayland_Desktop_Shell, 1)))
     return NULL;

   /* tell the shell what compositor to use */
   shell->compositor = _e_wl_comp;

   /* setup shell destroy callback */
   shell->wl.destroy_listener.notify = _e_wl_shell_cb_destroy;
   wl_signal_add(&_e_wl_comp->signals.destroy, &shell->wl.destroy_listener);

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
                              _e_wl_shell_cb_bind_desktop))
     {
        /* remove previously added shell global */
        wl_display_remove_global(_e_wl_comp->wl.display, gshell);

        goto err;
     }

   return m;

err:
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

/* shell interface functions */
static void 
_e_wl_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource, unsigned int id, struct wl_resource *surface_resource)
{

}

/* desktop shell functions */
static void 
_e_wl_shell_cb_bind_desktop(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
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
   res->destroy = _e_wl_shell_cb_unbind_desktop;
}

static void 
_e_wl_shell_cb_unbind_desktop(struct wl_resource *resource)
{
   free(resource);
}
