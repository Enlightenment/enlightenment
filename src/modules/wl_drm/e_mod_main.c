#define E_COMP_WL
#include "e.h"
#include <Ecore_Drm.h>

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Drm" };

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

static Ecore_Drm_Output_Mode *
_e_mod_drm_mode_screen_find(E_Randr2_Screen *s, Ecore_Drm_Output *output)
{
   Ecore_Drm_Output_Mode *mode, *m = NULL;
   const Eina_List *l;
   int diff, distance = 0x7fffffff;

   EINA_LIST_FOREACH(ecore_drm_output_modes_get(output), l, mode)
     {
        diff = (100 * abs(s->config.mode.w - mode->width)) + 
          (100 * abs(s->config.mode.h - mode->height)) + 
          abs((100 * s->config.mode.refresh) - (100 * mode->refresh));
        if (diff < distance)
          {
             m = mode;
             distance = diff;
          }
     }

   return m;
}

static E_Randr2 *
_drm_randr_create(void)
{
   Ecore_Drm_Device *dev;
   Ecore_Drm_Output *output;
   const Eina_List *l, *ll;
   E_Randr2 *r = NULL;
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
   unsigned int type;

   printf("DRM RRR: ................. info get!\n");

   r = E_NEW(E_Randr2, 1);
   if (!r) return NULL;

   EINA_LIST_FOREACH(ecore_drm_devices_get(), l, dev)
     {
        EINA_LIST_FOREACH(dev->outputs, ll, output)
          {
             E_Randr2_Screen *s;
             E_Config_Randr2_Screen *cs;
             const Eina_List *m;
             Ecore_Drm_Output_Mode *omode;
             size_t n, e = 0;
             unsigned int j;
             int priority;
             Eina_Bool ok = EINA_FALSE;
             Eina_Bool possible = EINA_FALSE;

             s = E_NEW(E_Randr2_Screen, 1);
             if (!s) continue;

             s->info.name = ecore_drm_output_name_get(output);
             printf("DRM RRR: .... out %s\n", s->info.name);

             s->info.connected = ecore_drm_output_connected_get(output);
             printf("DRM RRR: ...... connected %i\n", s->info.connected);

             if (s->info.connected)
               {
                  s->info.edid = ecore_drm_output_edid_get(output);
                  e = strlen(s->info.edid ?: "");
               }
             n = strlen(s->info.name);
             s->id = malloc(n + e + 2);
             eina_str_join_len(s->id, n + e + 2, '/', s->info.name, n, s->info.edid ?: "", e);
             s->id[n + e + 1] = 0;

             type = MIN(ecore_drm_output_connector_type_get(output),
                        EINA_C_ARRAY_LENGTH(conn_types) - 1);
             s->info.connector = rtype[type];
             s->info.is_lid = ((type == DRM_MODE_CONNECTOR_LVDS) || 
                               (type == DRM_MODE_CONNECTOR_eDP));
             s->info.lid_closed = (s->info.is_lid && e_acpi_lid_is_closed());
             printf("DRM RRR: ...... lid_closed = %i (%i && %i)\n",
                    s->info.lid_closed, s->info.is_lid, e_acpi_lid_is_closed());

             s->info.backlight = ecore_drm_output_backlight_get(output);

             ecore_drm_output_physical_size_get(output, &s->info.size.w,
                                                &s->info.size.h);

             EINA_LIST_FOREACH(ecore_drm_output_modes_get(output), m, omode)
               {
                  E_Randr2_Mode *rmode;

                  rmode = malloc(sizeof(E_Randr2_Mode));
                  if (!rmode) continue;

                  rmode->w = omode->width;
                  rmode->h = omode->height;
                  rmode->refresh = omode->refresh;
                  rmode->preferred = (omode->flags & DRM_MODE_TYPE_PREFERRED);

                  s->info.modes = eina_list_append(s->info.modes, rmode);
               }

             /* TODO: this does NOT handle possibles yet */

             cs = NULL;
             priority = 0;
             if (e_randr2_cfg)
               cs = e_randr2_config_screen_find(s, e_randr2_cfg);
             if (cs)
               priority = cs->priority;
             else if (ecore_drm_output_primary_get(dev) == output)
               priority = 100;
             s->config.priority = priority;

             for (j = 0; j < dev->crtc_count; j++)
               {
                  if (dev->crtcs[j] == ecore_drm_output_crtc_id_get(output))
                    {
                       ok = EINA_TRUE;
                       break;
                    }
               }

             if (!ok)
               {
                  /* get possible crtcs, compare to output_crtc_id_get */
                  WRN("GET POSSIBLE CRTCS");
               }

             if ((ok) && (!possible))
               {
                  unsigned int refresh;

                  ecore_drm_output_position_get(output, &s->config.geom.x,
                                                &s->config.geom.y);
                  ecore_drm_output_crtc_size_get(output, &s->config.geom.w,
                                                 &s->config.geom.h);

                  ecore_drm_output_current_resolution_get(output,
                                                          &s->config.mode.w,
                                                          &s->config.mode.h,
                                                          &refresh);
                  s->config.mode.refresh = refresh;
                  s->config.enabled = 
                    ((s->config.mode.w != 0) && (s->config.mode.h != 0));

                  printf("DRM RRR: '%s' %i %i %ix%i\n", s->info.name,
                         s->config.geom.x, s->config.geom.y,
                         s->config.geom.w, s->config.geom.h);
               }

             r->screens = eina_list_append(r->screens, s);
          }
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
   Ecore_Drm_Device *dev;
   Ecore_Drm_Output *out, **outconf;
   E_Randr2_Screen *s, **screenconf;
   const Eina_List *l, *ll;
   int nw, nh, pw, ph, ww, hh;
   int minw, minh, maxw, maxh;
   int top_priority = 0, i, numout;

   /* TODO: what the actual fuck */

   nw = e_randr2->w;
   nh = e_randr2->h;
   EINA_LIST_FOREACH(ecore_drm_devices_get(), l, dev)
     {
        int outputs_num, crtcs_num;

        ecore_drm_screen_size_range_get(dev, &minw, &minh, &maxw, &maxh);
        printf("DRM RRR: size range: %ix%i -> %ix%i\n", minw, minh, maxw, maxh);

        ecore_drm_outputs_geometry_get(dev, NULL, NULL, &pw, &ph);
        if (nw > maxw) nw = maxw;
        if (nh > maxh) nh = maxh;
        if (nw < minw) nw = minw;
        if (nh < minh) nh = minh;
        ww = nw;
        hh = nh;
        if (nw < pw) ww = pw;
        if (nh < ph) hh = ph;

        printf("DRM RRR: set vsize: %ix%i\n", ww, hh);

        outputs_num = eina_list_count(dev->outputs);
        crtcs_num = dev->crtc_count;

        if ((crtcs_num > 0) && (outputs_num > 0))
          {
             outconf = calloc(crtcs_num, sizeof(Ecore_Drm_Output *));
             screenconf = alloca(crtcs_num * sizeof(E_Randr2_Screen *));
             memset(screenconf, 0, crtcs_num * sizeof(E_Randr2_Screen *));

             EINA_LIST_FOREACH(e_randr2->screens, ll, s)
               {
                  printf("DRM RRR: find output for '%s'\n", s->info.name);

                  if (s->config.configured)
                    {
                       out = ecore_drm_device_output_name_find(dev, s->info.name);
                       if (out)
                         {
                            printf("DRM RRR:   enabled: %i\n", s->config.enabled);
                            if (s->config.enabled)
                              {
                                 if (s->config.priority > top_priority)
                                   top_priority = s->config.priority;

                                 for (i = 0; i < crtcs_num; i++)
                                   {
                                      if (!outconf[i])
                                        {
                                           printf("DRM RRR:     crtc slot empty: %i\n", i);

                                           /* TODO: get crtc info for dev->crtcs[i] */
                                           /* check if this output can go on this crtc */

                                           outconf[i] = out;
                                           screenconf[i] = s;

                                           break;
                                        }
                                   }
                              }
                         }
                    }
               }

             numout = 0;
             for (i = 0; i < crtcs_num; i++)
               if (outconf[i]) numout++;

             if (numout)
               {
                  for (i = 0; i < crtcs_num; i++)
                    {
                       /* TODO: find clones */
                       if (outconf[i])
                         {
                            int orient = 0;
                            Ecore_Drm_Output_Mode *mode;

                            mode = _e_mod_drm_mode_screen_find(screenconf[i],
                                                               outconf[i]);
                            printf("DRM RRR: crtc on: %i = '%s'     @ %i %i    - %ix%i orient %i mode %s out %s\n",
                                   i, screenconf[i]->info.name,
                                   screenconf[i]->config.geom.x,
                                   screenconf[i]->config.geom.y,
                                   screenconf[i]->config.geom.w,
                                   screenconf[i]->config.geom.h,
                                   orient, mode->info.name,
                                   ecore_drm_output_name_get(outconf[i]));
                         }
                    }
               }
             else
               printf("DRM RRR: EERRRRRROOOORRRRRRR no outputs to configure!\n");

             free(outconf);
          }
     }

   printf("DRM RRR: set vsize: %ix%i\n", nw, nh);
}

static E_Comp_Screen_Iface drmiface =
{
   .available = _drm_randr_available,
   .init = _drm_randr_stub,
   .shutdown = _drm_randr_stub,
   .create = _drm_randr_create,
   .apply = _drm_randr_apply
};

E_API void *
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

E_API int
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
