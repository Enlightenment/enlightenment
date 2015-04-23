#define E_COMP_WL
#include "e.h"
#include <Ecore_Drm.h>

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Drm" };

static Ecore_Event_Handler *activate_handler;
static Ecore_Event_Handler *output_handler;
static Eina_Bool session_state = EINA_FALSE;

static Eina_Bool
_e_mod_drm_cb_activate(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Drm_Event_Activate *e;

   if (!(e = event)) goto end;

   if (e->active)
     {
        E_Client *ec;

        if (session_state) goto end;
        session_state = EINA_TRUE;

        ecore_evas_show(e_comp->ee);
        E_CLIENT_FOREACH(ec)
          {
             if (ec->visible && (!ec->input_only))
               e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
          }
        e_comp_render_queue();
        e_comp_shape_queue_block(0);
        ecore_event_add(E_EVENT_COMPOSITOR_ENABLE, NULL, NULL, NULL);
     }
   else
     {
        session_state = EINA_FALSE;
        ecore_evas_hide(e_comp->ee);
        edje_file_cache_flush();
        edje_collection_cache_flush();
        evas_image_cache_flush(e_comp->evas);
        evas_font_cache_flush(e_comp->evas);
        evas_render_dump(e_comp->evas);

        e_comp_render_queue();
        e_comp_shape_queue_block(1);
        ecore_event_add(E_EVENT_COMPOSITOR_DISABLE, NULL, NULL, NULL);
     }

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_mod_drm_cb_output(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Drm_Event_Output *e;
   char buff[PATH_MAX];

   if (!(e = event)) goto end;

   if (!e->plug)
     {
        DBG("Caught Drm Output Unplug Event");
        /* FIXME: This needs to remove output from e_comp_wl */
        goto end;
     }

   snprintf(buff, sizeof(buff), "%d", e->id);

   if (!e_comp_wl_output_init(buff, e->make, e->model, e->x, e->y, e->w, e->h, 
                              e->phys_width, e->phys_height, e->refresh, 
                              e->subpixel_order, e->transform))
     {
        ERR("Could not setup new output: %s", buff);
     }

end:
   if (!e_randr2_cfg->ignore_hotplug_events)
     e_randr2_screen_refresh_queue(EINA_TRUE);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_mod_drm_cb_ee_resize(Ecore_Evas *ee EINA_UNUSED)
{
   e_comp_canvas_update();
}

static E_Randr2_Mode *
_mode_get(drmModeModeInfo *info)
{
   E_Randr2_Mode *mode;
   uint64_t refresh;

   mode = malloc(sizeof(E_Randr2_Mode));

   mode->w = info->hdisplay;
   mode->h = info->vdisplay;

   refresh = (info->clock * 1000000LL / info->htotal + info->vtotal / 2) / info->vtotal;
   if (info->flags & DRM_MODE_FLAG_INTERLACE)
     refresh *= 2;
   if (info->flags & DRM_MODE_FLAG_DBLSCAN)
     refresh /= 2;
   if (info->vscan > 1)
     refresh /= info->vscan;

   mode->refresh = refresh;
   mode->preferred = (info->type & DRM_MODE_TYPE_PREFERRED);

   return mode;
}

static char *
_get_edid(Ecore_Drm_Device *dev, drmModeConnector *conn)
{
   int i;
   drmModePropertyBlobPtr blob = NULL;
   drmModePropertyPtr prop;
   char *ret;

   for (i = 0; i < conn->count_props; i++)
     {
        if (!(prop = drmModeGetProperty(dev->drm.fd, conn->props[i])))
          continue;
        if ((prop->flags & DRM_MODE_PROP_BLOB) &&
            (!strcmp(prop->name, "EDID")))
          {
             blob = drmModeGetPropertyBlob(dev->drm.fd,
                                           conn->prop_values[i]);
             drmModeFreeProperty(prop);
             break;
          }
        drmModeFreeProperty(prop);
     }
   if (!blob) return NULL;
   ret = (char*)eina_memdup(blob->data, blob->length, 1);
   drmModeFreePropertyBlob(blob);
   return ret;
}

static Eina_Bool
_backlight_get(const char *devpath, uint32_t conn_type)
{
   Eina_List *devs, *l;
   const char *device;
   Eina_Bool found;

   if (!(devs = eeze_udev_find_by_filter("backlight", NULL, devpath)))
     devs = eeze_udev_find_by_filter("leds", NULL, devpath);

   if (!devs) return EINA_FALSE;

   EINA_LIST_FOREACH(devs, l, device)
     {
        if ((conn_type == DRM_MODE_CONNECTOR_LVDS) ||
            (conn_type == DRM_MODE_CONNECTOR_eDP) ||
            eeze_udev_syspath_check_sysattr(device, "type", "raw"))
          found = EINA_TRUE;
        if (found) break;
     }

   E_FREE_LIST(devs, eina_stringshare_del);

   return found;
}

static E_Randr2 *
_drm_randr_create(void)
{
   Ecore_Drm_Device *dev;
   const Eina_List *l;
   E_Randr2 *r;

   r = E_NEW(E_Randr2, 1);
   EINA_LIST_FOREACH(ecore_drm_devices_get(), l, dev)
     {
        drmModeRes *res;
        int o;

        res = drmModeGetResources(dev->drm.fd);
        if (!res) continue;
        for (o = 0; o < res->count_connectors; o++)
          {
             E_Randr2_Screen *s;
             size_t n, e = 0;
             int i;
             drmModeConnector *conn;
             const char *conn_types[] =
             {
                "None", "VGA", "DVI-I", "DVI-D", "DVI-A",
                "Composite", "S-Video", "LVDS", "Component", "DIN",
                "DisplayPort", "HDMI-A", "HDMI-B", "TV", "eDP", "Virtual",
                "DSI", "UNKNOWN"
             };
             E_Randr2_Connector rtype[] =
             {
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_DVI,
                E_RANDR2_CONNECTOR_DVI,
                E_RANDR2_CONNECTOR_DVI,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_DISPLAY_PORT,
                E_RANDR2_CONNECTOR_HDMI_A,
                E_RANDR2_CONNECTOR_HDMI_B,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_DISPLAY_PORT,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
                E_RANDR2_CONNECTOR_UNDEFINED,
             };
             char buf[4096];
             unsigned int type;

             /* FIXME: it's insane to have this code here instead of in ecore-drm. */
             conn = drmModeGetConnector(dev->drm.fd, res->connectors[o]);
             if (!conn) continue;
             s = E_NEW(E_Randr2_Screen, 1);
             type = MIN(conn->connector_type, EINA_C_ARRAY_LENGTH(conn_types) - 1);

             snprintf(buf, sizeof(buf), "%s-%d", conn_types[type], conn->connector_type_id);
             s->info.name = strdup(buf);
             s->info.connected = conn->connection == DRM_MODE_CONNECTED;
             if (s->info.connected)
               {
                  s->info.edid = _get_edid(dev, conn);
                  e = strlen(s->info.edid ?: "");
               }
             n = strlen(s->info.name);
             s->id = malloc(n + e + 2);
             eina_str_join_len(s->id, n + e + 2, '/', s->info.name, n, s->info.edid ?: "", e);
             s->id[n + e + 1] = 0;
             s->info.connector = rtype[type];
             s->info.is_lid = (type == DRM_MODE_CONNECTOR_LVDS) || (type == DRM_MODE_CONNECTOR_eDP);
             s->info.backlight = _backlight_get(dev->drm.path, conn->connector_type);
             for (i = 0; i < conn->count_modes; i++)
               {
                  E_Randr2_Mode *mode;

                  mode = _mode_get(conn->modes + i);
                  if (mode)
                    s->info.modes = eina_list_append(s->info.modes, mode);
               }
             s->info.size.w = conn->mmWidth;
             s->info.size.h = conn->mmHeight;

             r->screens = eina_list_append(r->screens, s);

             drmModeFreeConnector(conn);
          }
        drmModeFreeResources(res);
     }
   return r;
}

static Eina_Bool
_drm_randr_available(void)
{
   return EINA_TRUE;
}

static void
_drm_randr_stub(void)
{}

static void
_drm_randr_apply(void)
{
   /* TODO: what the actual fuck */
}

static E_Comp_Screen_Iface drmiface =
{
   .available = _drm_randr_available,
   .init = _drm_randr_stub,
   .shutdown = _drm_randr_stub,
   .create = _drm_randr_create,
   .apply = _drm_randr_apply
};

EAPI void *
e_modapi_init(E_Module *m)
{
   int w = 0, h = 0;

   printf("LOAD WL_DRM MODULE\n");

   /* try to init ecore_drm */
   /* if (!ecore_drm_init()) */
   /*   { */
   /*      fprintf(stderr, "Could not initialize ecore_drm"); */
   /*      return NULL; */
   /*   } */

   if (e_comp_config_get()->engine == E_COMP_ENGINE_GL)
     {
        e_comp->ee = ecore_evas_new("gl_drm", 0, 0, 1, 1, NULL);
        e_comp_gl_set(!!e_comp->ee);
     }

   /* fallback to framebuffer drm (non-accel) */
   if (!e_comp->ee)
     {
        if ((e_comp->ee = ecore_evas_new("drm", 0, 0, 1, 1, NULL)))
          {
             e_comp_gl_set(EINA_FALSE);
             elm_config_accel_preference_set("none");
             elm_config_accel_preference_override_set(EINA_TRUE);
             elm_config_all_flush();
             elm_config_save();
          }
        else
          {
             fprintf(stderr, "Could not create ecore_evas_drm canvas");
             return NULL;
          }
     }

   ecore_evas_data_set(e_comp->ee, "comp", e_comp);

   /* get the current screen geometry */
   ecore_evas_screen_geometry_get(e_comp->ee, NULL, NULL, &w, &h);

   ecore_evas_callback_resize_set(e_comp->ee, _e_mod_drm_cb_ee_resize);

   e_comp->screen = &drmiface;

   if (!e_comp_wl_init()) return NULL;
   if (!e_comp_canvas_init(w, h)) return NULL;

   ecore_evas_pointer_xy_get(e_comp->ee, &e_comp->wl_comp_data->ptr.x,
                             &e_comp->wl_comp_data->ptr.y);

   e_comp_wl_input_pointer_enabled_set(e_comp->wl_comp_data, EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(e_comp->wl_comp_data, EINA_TRUE);

   /* comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(comp->ee), 1); */
   e_comp->pointer = e_pointer_canvas_new(e_comp->ee, EINA_TRUE);
   e_comp->pointer->color = EINA_TRUE;

   /* FIXME: We need a way to trap for user changing the keymap inside of E
    *        without the event coming from X11 */

   /* FIXME: We should make a decision here ...
    *
    * Fetch the keymap from drm, OR set this to what the E config is....
    */

   /* FIXME: This is just for testing at the moment....
    * happens to jive with what drm does */
   e_comp_wl_input_keymap_set(e_comp->wl_comp_data, NULL, NULL, NULL);

   activate_handler =
      ecore_event_handler_add(ECORE_DRM_EVENT_ACTIVATE,
                              _e_mod_drm_cb_activate, NULL);

   output_handler =
      ecore_event_handler_add(ECORE_DRM_EVENT_OUTPUT,
                              _e_mod_drm_cb_output, NULL);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_drm */
   /* ecore_drm_shutdown(); */

   if (output_handler) ecore_event_handler_del(output_handler);
   output_handler = NULL;

   if (activate_handler) ecore_event_handler_del(activate_handler);
   activate_handler = NULL;

   return 1;
}
