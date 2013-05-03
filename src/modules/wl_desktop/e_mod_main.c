#include "e.h"
#include "e_mod_main.h"

/* local function prototypes */
static void _e_desktop_shell_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_desktop_shell_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);

/* local wayland interfaces */
static const struct wl_shell_interface _e_desktop_shell_interface = 
{
   NULL // shell_surface_get
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
   _e_comp->shell_interface.shell = shell;

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
   wl_list_remove(&_e_comp->signals.destroy->listener_list, 
                  &shell->wl.destroy_listener->link);

   /* free the allocated structure */
   E_FREE(shell);
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
   _e_comp->shell_interface.shell = NULL;

   /* remove the destroy signal */
   wl_list_remove(&_e_comp->signals.destroy->listener_list, 
                  &shell->wl.destroy_listener->link);

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
