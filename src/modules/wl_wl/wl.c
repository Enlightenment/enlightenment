#include "e.h"

static struct wl_display *disp;
static Ecore_Fd_Handler *wl_fdh;

static void
_wl_handle_global(void *data EINA_UNUSED, struct wl_registry *registry EINA_UNUSED, unsigned int id, const char *interface, unsigned int version)
{
   Ecore_Wl_Global *global;

   if (!(global = calloc(1, sizeof(Ecore_Wl_Global)))) return;

   global->id = id;
   global->interface = strdup(interface);
   global->version = version;
   e_comp->wl_comp_data->wl.globals = eina_inlist_append(e_comp->wl_comp_data->wl.globals, EINA_INLIST_GET(global));

   if (!strcmp(interface, "wl_shm"))
     e_comp->wl_comp_data->wl.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
   ecore_event_add(E_EVENT_WAYLAND_GLOBAL_ADD, NULL, NULL, NULL);
}

static void
_wl_handle_global_remove(void *data EINA_UNUSED, struct wl_registry *registry EINA_UNUSED, unsigned int id)
{
   Ecore_Wl_Global *global;
   Eina_Inlist *tmp;

   EINA_INLIST_FOREACH_SAFE(e_comp->wl_comp_data->wl.globals, tmp, global)
     {
        if (global->id != id) continue;
        e_comp->wl_comp_data->wl.globals =
          eina_inlist_remove(e_comp->wl_comp_data->wl.globals, EINA_INLIST_GET(global));
        free(global->interface);
        free(global);
     }
}

static const struct wl_registry_listener _global_registry_listener =
{
   _wl_handle_global,
   _wl_handle_global_remove
};


static Eina_Bool
_ecore_wl_cb_idle_enterer(void *data EINA_UNUSED)
{
   int ret = 0;

   ret = wl_display_get_error(disp);
   if (ret < 0) goto err;

   ret = wl_display_dispatch_pending(disp);
   if (ret < 0) goto err;

   ret = wl_display_flush(disp);
   if ((ret < 0) && (errno == EAGAIN))
     ecore_main_fd_handler_active_set(wl_fdh, ECORE_FD_READ | ECORE_FD_WRITE);

   return ECORE_CALLBACK_RENEW;

err:
   if ((ret < 0) && ((errno != EAGAIN) && (errno != EINVAL)))
     {
        /* raise exit signal */
        fprintf(stderr, "Wayland socket error: %s\n", strerror(errno));
        abort();

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}


static Eina_Bool
_ecore_wl_cb_handle_data(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl)
{
   int ret = 0;

   if (ecore_main_fd_handler_active_get(hdl, ECORE_FD_ERROR))
     {
        fprintf(stderr, "Received error on wayland display fd\n");
        abort();

        return ECORE_CALLBACK_CANCEL;
     }

   if (ecore_main_fd_handler_active_get(hdl, ECORE_FD_READ))
     ret = wl_display_dispatch(disp);
   else if (ecore_main_fd_handler_active_get(hdl, ECORE_FD_WRITE))
     {
        ret = wl_display_flush(disp);
        if (ret == 0)
          ecore_main_fd_handler_active_set(hdl, ECORE_FD_READ);
     }

   if ((ret < 0) && ((errno != EAGAIN) && (errno != EINVAL)))
     {
        /* raise exit signal */
        abort();

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

EINTERN void
wl_wl_init(void)
{
   disp = wl_display_connect(getenv("WAYLAND_DISPLAY"));
   ecore_main_fd_handler_add(wl_display_get_fd(disp), ECORE_FD_READ | ECORE_FD_WRITE | ECORE_FD_ERROR,
                               _ecore_wl_cb_handle_data, NULL, NULL, NULL);
   e_comp->wl_comp_data->wl.registry = wl_display_get_registry(disp);
   wl_registry_add_listener(e_comp->wl_comp_data->wl.registry, &_global_registry_listener, NULL);
   ecore_idle_enterer_add(_ecore_wl_cb_idle_enterer, NULL);
}
