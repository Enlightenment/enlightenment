#define E_COMP_WL
#include "e.h"

#define COMPOSITOR_VERSION 3

#define E_COMP_WL_PIXMAP_CHECK \
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return

/* local functions */
static void 
_e_comp_wl_log_cb_print(const char *format, va_list args)
{
   EINA_LOG_DOM_INFO(e_log_dom, format, args);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* dispatch pending wayland events */
   wl_event_loop_dispatch(cdata->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_cb_prepare(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return;

   /* flush pending client events */
   wl_display_flush_clients(cdata->wl.disp);
}

static Eina_Bool 
_e_comp_wl_cb_module_idle(void *data)
{
   E_Comp_Data *cdata;
   E_Module  *mod = NULL;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* check if we are still loading modules */
   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   if (!(mod = e_module_find("wl_desktop_shell")))
     mod = e_module_new("wl_desktop_shell");

   if (mod)
     {
        e_module_enable(mod);

        /* FIXME: NB: 
         * Do we need to dispatch pending wl events here ?? */

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_surface_cb_attach(struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{

}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{

}

static void 
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{

}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{

}

static void 
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{

}

static void 
_e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{

}

static void 
_e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t transform EINA_UNUSED)
{
   DBG("Surface Buffer Transform");
}

static void 
_e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t scale EINA_UNUSED)
{
   DBG("Surface Buffer Scale");
}

static const struct wl_surface_interface _e_surface_interface = 
{
   _e_comp_wl_surface_cb_destroy,
   _e_comp_wl_surface_cb_attach,
   _e_comp_wl_surface_cb_damage,
   _e_comp_wl_surface_cb_frame,
   _e_comp_wl_surface_cb_opaque_region_set,
   _e_comp_wl_surface_cb_input_region_set,
   _e_comp_wl_surface_cb_commit,
   _e_comp_wl_surface_cb_buffer_transform_set,
   _e_comp_wl_surface_cb_buffer_scale_set
};

static void 
_e_comp_wl_compositor_cb_surface_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;
   uint64_t wid;
   pid_t pid;
   E_Pixmap *ep;

   if (!(comp = wl_resource_get_user_data(resource))) return;

   DBG("Compositor Cb Surface Create: %d", id);

   /* try to create an internal surface */
   if (!(res = wl_resource_create(client, &wl_surface_interface, 
                                  wl_resource_get_version(resource), id)))
     {
        ERR("Could not create compositor surface");
        wl_resource_post_no_memory(resource);
        return;
     }

   /* FIXME: set callback ? */
   /* set implementation on resource */
   wl_resource_set_implementation(res, &_e_surface_interface, NULL, NULL);
//                                  _callback);

   DBG("\tCreated Resource: %d", wl_resource_get_id(res));

   /* set implementation */

   /* get the client pid and generate a pixmap id */
   wl_client_get_credentials(client, &pid, NULL, NULL);
   wid = e_comp_wl_id_get(pid, id);

   DBG("\tClient Pid: %d", pid);

   /* check for existing pixmap */
   if (!(ep = e_pixmap_find(E_PIXMAP_TYPE_WL, wid)))
     {
        /* try to create new pixmap */
        if (!(ep = e_pixmap_new(E_PIXMAP_TYPE_WL, wid)))
          {
             ERR("Could not create new pixmap");
             wl_resource_destroy(res);
             wl_resource_post_no_memory(resource);
             return;
          }
     }

   DBG("\tUsing Pixmap: %llu", wid);

   /* set reference to pixmap so we can fetch it later */
   wl_resource_set_user_data(res, ep);

   /* emit surface create signal */
   wl_signal_emit(&comp->wl_comp_data->signals.surface.create, res);
}

static void 
_e_comp_wl_compositor_cb_region_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;

   if (!(comp = wl_resource_get_user_data(resource))) return;
}

static const struct wl_compositor_interface _e_comp_interface = 
{
   _e_comp_wl_compositor_cb_surface_create,
   _e_comp_wl_compositor_cb_region_create
};

static void 
_e_comp_wl_compositor_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   if (!(res = 
         wl_resource_create(client, &wl_compositor_interface, 
                            MIN(version, COMPOSITOR_VERSION), id)))
     {
        ERR("Could not create compositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_comp_interface, comp, NULL);
}

static void 
_e_comp_wl_compositor_cb_del(E_Comp *comp)
{
   E_Comp_Data *cdata;

   /* get existing compositor data */
   if (!(cdata = comp->wl_comp_data)) return;

   /* delete fd handler */
   if (cdata->fd_hdlr) ecore_main_fd_handler_del(cdata->fd_hdlr);

   /* free allocated data structure */
   free(cdata);
}

static Eina_Bool 
_e_comp_wl_compositor_create(void)
{
   E_Comp *comp;
   E_Comp_Data *cdata;
   const char *name;
   int fd = 0;

   /* check for existing compositor. create if needed */
   if (!(comp = e_comp_get(NULL)))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
        E_OBJECT_DEL_SET(comp, _e_comp_wl_compositor_cb_del);
     }

   /* create new compositor data */
   cdata = E_NEW(E_Comp_Data, 1);

   /* set wayland log handler */
   wl_log_set_handler_server(_e_comp_wl_log_cb_print);

   /* try to create a wayland display */
   if (!(cdata->wl.disp = wl_display_create()))
     {
        ERR("Could not create a Wayland display: %m");
        goto disp_err;
     }

   /* try to setup wayland socket */
   if (!(name = wl_display_add_socket_auto(cdata->wl.disp)))
     {
        ERR("Could not create Wayland display socket: %m");
        goto sock_err;
     }

   /* set wayland display environment variable */
   e_env_set("WAYLAND_DISPLAY", name);

   /* initialize compositor signals */
   wl_signal_init(&cdata->signals.surface.create);
   wl_signal_init(&cdata->signals.surface.activate);
   wl_signal_init(&cdata->signals.surface.kill);

   /* try to add compositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_compositor_interface, 
                         COMPOSITOR_VERSION, comp, 
                         _e_comp_wl_compositor_cb_bind))
     {
        ERR("Could not add compositor to wayland globals: %m");
        goto comp_global_err;
     }

   /* try to init data manager */
   if (!e_comp_wl_data_manager_init(cdata))
     {
        ERR("Could not initialize data manager");
        goto data_err;
     }

   /* try to init input */
   if (!e_comp_wl_input_init(cdata))
     {
        ERR("Could not initialize input");
        goto input_err;
     }

   /* initialize shm mechanism */
   wl_display_init_shm(cdata->wl.disp);

   /* get the wayland display loop */
   cdata->wl.loop = wl_display_get_event_loop(cdata->wl.disp);

   /* get the file descriptor of the wayland event loop */
   fd = wl_event_loop_get_fd(cdata->wl.loop);

   /* create a listener for wayland main loop events */
   cdata->fd_hdlr = 
     ecore_main_fd_handler_add(fd, (ECORE_FD_READ | ECORE_FD_ERROR), 
                               _e_comp_wl_cb_read, cdata, NULL, NULL);
   ecore_main_fd_handler_prepare_callback_set(cdata->fd_hdlr, 
                                              _e_comp_wl_cb_prepare, cdata);

   /* setup module idler to load shell mmodule */
   ecore_idler_add(_e_comp_wl_cb_module_idle, cdata);

   if (comp->comp_type == E_PIXMAP_TYPE_X)
     {
        e_comp_wl_input_pointer_enabled_set(cdata, EINA_TRUE);
        e_comp_wl_input_keyboard_enabled_set(cdata, EINA_TRUE);
     }

   /* set compositor wayland data */
   comp->wl_comp_data = cdata;

   return EINA_TRUE;

input_err:
   e_comp_wl_data_manager_shutdown(cdata);
data_err:
comp_global_err:
   e_env_unset("WAYLAND_DISPLAY");
sock_err:
   wl_display_destroy(cdata->wl.disp);
disp_err:
   free(cdata);
   return EINA_FALSE;
}

/* public functions */
EAPI Eina_Bool 
e_comp_wl_init(void)
{
   /* set gl available if we have ecore_evas support */
   if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL) || 
       ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_DRM))
     e_comp_gl_set(EINA_TRUE);

   /* try to create a wayland compositor */
   if (!_e_comp_wl_compositor_create())
     {
        e_error_message_show(_("Enlightenment cannot create a Wayland Compositor!\n"));
        return EINA_FALSE;
     }

   /* try to init ecore_wayland */
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland!\n"));
        return EINA_FALSE;
     }

   return EINA_TRUE;
}

EAPI struct wl_signal 
e_comp_wl_surface_create_signal_get(E_Comp *comp)
{
   return comp->wl_comp_data->signals.surface.create;
}

/* internal functions */
EINTERN void 
e_comp_wl_shutdown(void)
{
   /* shutdown ecore_wayland */
   ecore_wl_shutdown();
}

EINTERN struct wl_resource *
e_comp_wl_surface_create(struct wl_client *client, int version, uint32_t id)
{
   struct wl_resource *ret = NULL;

   if ((ret = wl_resource_create(client, &wl_surface_interface, version, id)))
     {
        DBG("Created Surface: %d", wl_resource_get_id(ret));
     }

   return ret;
}
