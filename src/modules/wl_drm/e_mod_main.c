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
   const Eina_List *l;
   E_Randr2_Screen *screen;
   Ecore_Drm_Event_Output *e;

   if (!(e = event)) goto end;

   DBG("WL_DRM OUTPUT CHANGE");

   EINA_LIST_FOREACH(e_randr2->screens, l, screen)
     {
        if ((!strcmp(screen->info.name, e->name)) && 
            (!strcmp(screen->info.screen, e->model)))
          {
             if (e->plug)
               {
                  if (!e_comp_wl_output_init(screen->id, e->make, e->model,
                                             e->x, e->y, e->w, e->h, 
                                             e->phys_width, e->phys_height,
                                             e->refresh, e->subpixel_order,
                                             e->transform))
                    {
                       ERR("Could not setup new output: %s", screen->id);
                    }
               }
             else
               e_comp_wl_output_remove(screen->id);

             break;
          }
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
          fabs((100 * s->config.mode.refresh) - (100 * mode->refresh));
        if (diff < distance)
          {
             m = mode;
             distance = diff;
          }
     }

   return m;
}

static Eina_Bool
_e_mod_drm_output_exists(Ecore_Drm_Output *output, unsigned int crtc)
{
   /* find out if this output can go into the 'possibles' */
   return ecore_drm_output_possible_crtc_get(output, crtc);
}

static char *
_e_mod_drm_output_screen_get(Ecore_Drm_Output *output)
{
   const char *model;

   model = ecore_drm_output_model_get(output);
   if (!model) return NULL;

   return strdup(model);
}

static E_Randr2_Screen *
_info_unconf_primary_find(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_primary = NULL;
   int priority = 0;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!((s->config.enabled) && 
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if (s->config.priority > priority)
          {
             s_primary = s;
             priority = s->config.priority;
          }
     }

   return s_primary;
}

static E_Randr2_Screen *
_info_unconf_left_find(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_left = NULL;
   int left_x = 0x7fffffff;
   int left_size = 0;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!((s->config.enabled) &&
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if ((s->config.geom.x <= left_x) &&
            ((s->config.geom.w * s->config.geom.h) > left_size))
          {
             left_size = s->config.geom.w * s->config.geom.h;
             left_x = s->config.geom.x;
             s_left = s;
          }
     }
   return s_left;
}

static E_Randr2_Screen *
_info_unconf_closest_find(E_Randr2 *r, E_Randr2_Screen *s2, Eina_Bool configured)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_sel = NULL;
   int dist = 0x7fffffff;
   int dx, dy;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (s == s2) continue;
        if (!((s->config.enabled) &&
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if ((!configured) &&
            (s->config.relative.mode != E_RANDR2_RELATIVE_UNKNOWN))
          continue;
        else if ((configured) &&
                 (s->config.relative.mode == E_RANDR2_RELATIVE_UNKNOWN))
          continue;
        dx = (s->config.geom.x + (s->config.geom.w / 2)) -
          (s2->config.geom.x + (s2->config.geom.w / 2));
        dy = (s->config.geom.y + (s->config.geom.h / 2)) -
          (s2->config.geom.y + (s2->config.geom.h / 2));
        dx = sqrt((dx * dx) + (dy * dy));
        if (dx < dist)
          {
             s_sel = s;
             dist = dx;
          }
     }
   return s_sel;
}

static void
_e_mod_drm_relative_fixup(E_Randr2 *r)
{
   E_Randr2_Screen *s, *s2;
   int d, dx, dy;

   s = _info_unconf_primary_find(r);
   if (s)
     s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
   else
     {
        s = _info_unconf_left_find(r);
        if (!s) return;
        s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
     }

   for (;;)
     {
        // find the next screen that is closest to the last one we configured
        /// that is still not configured yet
        s = _info_unconf_closest_find(r, s, EINA_FALSE);
        if (!s) break;
        s2 = _info_unconf_closest_find(r, s, EINA_TRUE);
        // fix up s->config.relative.mode, s->config.relative.to and
        // s->config.relative.align to match (as closely as possible)
        // the geometry given - config s relative to s2
        if (!s2) s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
        else
          {
             s->config.relative.to = strdup(s2->id);
             s->config.relative.align = 0.0;
             s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
             if ((s->config.geom.x + s->config.geom.w) <=
                 s2->config.geom.x)
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_LEFT;
                  d = s->config.geom.h - s2->config.geom.h;
                  dy = s2->config.geom.y - s->config.geom.y;
                  if (d != 0)
                    s->config.relative.align = ((double)dy) / ((double)d);
               }
             else if (s->config.geom.x >=
                      (s2->config.geom.x + s2->config.geom.w))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_RIGHT;
                  d = s->config.geom.h - s2->config.geom.h;
                  dy = s2->config.geom.y - s->config.geom.y;
                  if (d != 0)
                    s->config.relative.align = ((double)dy) / ((double)d);
               }
             else if ((s->config.geom.y + s->config.geom.h) <=
                      s2->config.geom.y)
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_ABOVE;
                  d = s->config.geom.w - s2->config.geom.w;
                  dx = s2->config.geom.x - s->config.geom.x;
                  if (d != 0)
                    s->config.relative.align = ((double)dx) / ((double)d);
               }
             else if (s->config.geom.y >=
                      (s2->config.geom.y + s2->config.geom.h))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_BELOW;
                  d = s->config.geom.w - s2->config.geom.w;
                  dx = s2->config.geom.x - s->config.geom.x;
                  if (d != 0)
                    s->config.relative.align = ((double)dx) / ((double)d);
               }
             else if ((s->config.geom.x == s2->config.geom.x) &&
                      (s->config.geom.y == s2->config.geom.y) &&
                      (s->config.geom.w == s2->config.geom.w) &&
                      (s->config.geom.h == s2->config.geom.h))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_CLONE;
               }
             if (s->config.relative.align < 0.0)
               s->config.relative.align = 0.0;
             else if (s->config.relative.align > 1.0)
               s->config.relative.align = 1.0;
          }
     }
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
             // size_t n, e = 0;
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

             s->info.screen = _e_mod_drm_output_screen_get(output);

             s->info.edid = ecore_drm_output_edid_get(output);
             if (s->info.edid)
               s->id = malloc(strlen(s->info.name) + 1 + strlen(s->info.edid) + 1);
             else
               s->id = malloc(strlen(s->info.name) + 1 + 1);
             if (!s->id)
               {
                  free(s->info.screen);
                  free(s->info.edid);
                  free(s);
                  continue;
               }
             strcpy(s->id, s->info.name);
             strcat(s->id, "/");
             if (s->info.edid) strcat(s->id, s->info.edid);

             printf("DRM RRR: Created Screen: %s\n", s->id);

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
                  for (j = 0; j < dev->crtc_count; j++)
                    {
                       if (_e_mod_drm_output_exists(output, dev->crtcs[j]))
                         {
                            ok = EINA_TRUE;
                            possible = EINA_TRUE;
                            break;
                         }
                    }
               }

             if (ok)
               {
                  unsigned int rotations;

                  if (!possible)
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

                  rotations =
                    ecore_drm_output_supported_rotations_get(output,
                                                             ECORE_DRM_PLANE_TYPE_PRIMARY);

                  if (rotations & ECORE_DRM_PLANE_ROTATION_NORMAL)
                    s->info.can_rot_0 = EINA_TRUE;
                  if (rotations & ECORE_DRM_PLANE_ROTATION_90)
                    s->info.can_rot_90 = EINA_TRUE;
                  if (rotations & ECORE_DRM_PLANE_ROTATION_180)
                    s->info.can_rot_180 = EINA_TRUE;
                  if (rotations & ECORE_DRM_PLANE_ROTATION_270)
                    s->info.can_rot_270 = EINA_TRUE;

                  if (cs)
                    {
                       if (cs->profile)
                         s->config.profile = strdup(cs->profile);
                       else
                         s->config.profile = NULL;
                       s->config.scale_multiplier = cs->scale_multiplier;
                    }
               }

             r->screens = eina_list_append(r->screens, s);
          }
     }

   _e_mod_drm_relative_fixup(r);

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
   Ecore_Drm_Output *out;
   E_Randr2_Screen *s;
   const Eina_List *l, *ll;
   int nw, nh, pw, ph, ww, hh;
   int minw, minh, maxw, maxh;
   int top_priority = 0;

   /* TODO: what the actual fuck */

   nw = e_randr2->w;
   nh = e_randr2->h;
   EINA_LIST_FOREACH(ecore_drm_devices_get(), l, dev)
     {
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

        EINA_LIST_FOREACH(e_randr2->screens, ll, s)
          {
             int orient;
             Ecore_Drm_Output_Mode *mode = NULL;

             printf("DRM RRR: find output for '%s'\n", s->info.name);

             out = ecore_drm_device_output_name_find(dev, s->info.name);
             if (!out) continue;

             if (s->config.configured)
               {
                  printf("\tDRM RRR: configured by E\n");

                  if (s->config.enabled)
                    {
                       printf("\tDRM RRR: Enabled\n");
                       mode = _e_mod_drm_mode_screen_find(s, out);
                    }
                  else
                    {
                       printf("\tDRM RRR: Disabled\n");
                    }

                  if (s->config.priority > top_priority)
                    top_priority = s->config.priority;

                  printf("\tDRM RRR: Priority: %d\n", s->config.priority);

                  printf("\tDRM RRR: Geom: %d %d %d %d\n", 
                         s->config.geom.x, s->config.geom.y,
                         s->config.geom.w, s->config.geom.h);

                  if (mode)
                    {
                       printf("\tDRM RRR: Found Valid Drm Mode\n");
                       printf("\t\tDRM RRR: %dx%d\n", mode->width, mode->height);
                    }
                  else
                    printf("\tDRM RRR: No Valid Drm Mode Found\n");

                  if (s->config.rotation == 0)
                    orient = (1 << 0);
                  else if (s->config.rotation == 90)
                    orient = (1 << 1);
                  else if (s->config.rotation == 180)
                    orient = (1 << 2);
                  else if (s->config.rotation == 270)
                    orient = (1 << 3);

                  ecore_drm_output_mode_set(out, mode,
                                            s->config.geom.x, s->config.geom.y);
                  ecore_drm_output_rotation_set(out,
                                                ECORE_DRM_PLANE_TYPE_PRIMARY,
                                                orient);

                  if (s->config.priority == top_priority)
                    ecore_drm_output_primary_set(out);

                  if (s->config.enabled)
                    ecore_drm_output_enable(out);
                  else
                    ecore_drm_output_disable(out);

                  printf("\tDRM RRR: Mode\n");
                  printf("\t\tDRM RRR: Geom: %d %d\n",
                         s->config.mode.w, s->config.mode.h);
                  printf("\t\tDRM RRR: Refresh: %f\n", s->config.mode.refresh);
                  printf("\t\tDRM RRR: Preferred: %d\n",
                         s->config.mode.preferred);

                  printf("\tDRM RRR: Rotation: %d\n", s->config.rotation);

                  printf("\tDRM RRR: Relative Mode: %d\n",
                         s->config.relative.mode);
                  printf("\tDRM RRR: Relative To: %s\n",
                         s->config.relative.to);
                  printf("\tDRM RRR: Align: %f\n", s->config.relative.align);
               }
          }
     }
}

static void
_drm_dpms(int set)
{
   Ecore_Drm_Device *dev;
   Ecore_Drm_Output *out;
   E_Randr2_Screen *s;
   const Eina_List *l, *ll;

   EINA_LIST_FOREACH(ecore_drm_devices_get(), l, dev)
     {
        EINA_LIST_FOREACH(e_randr2->screens, ll, s)
          {
             out = ecore_drm_device_output_name_find(dev, s->info.name);
             if (!out) continue;

             if ((!s->config.configured) || s->config.enabled)
               ecore_drm_output_dpms_set(out, set);
          }
     }
}

static E_Comp_Screen_Iface drmiface =
{
   .available = _drm_randr_available,
   .init = _drm_randr_stub,
   .shutdown = _drm_randr_stub,
   .create = _drm_randr_create,
   .apply = _drm_randr_apply,
   .dpms = _drm_dpms,
};

static void
_drm_read_pixels(E_Comp_Wl_Output *output, void *pixels)
{
   Ecore_Drm_Device *dev;
   Ecore_Drm_Fb *fb;
   const Eina_List *drm_devs, *l;
   int i = 0, bstride;
   unsigned char *s, *d = pixels;

   drm_devs = ecore_drm_devices_get();
   EINA_LIST_FOREACH(drm_devs, l, dev)
     {
        fb = dev->next;
        if (!fb) fb = dev->current;
        if (fb) break;
     }

   if (!fb) return;

   bstride = output->w * sizeof(int);

   for (i = output->y; i < output->y + output->h; i++)
     {
        s = fb->mmap;
        s += (fb->stride * i) + (output->x * sizeof(int));
        memcpy(d, s, (output->w * sizeof(int)));
        d += bstride;
     }
}

static void
_e_mod_drm_keymap_set(struct xkb_context *ctx, struct xkb_keymap *map)
{
   struct xkb_rule_names names;

   ctx = xkb_context_new(0);
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   memset(&names, 0, sizeof(names));
   names.rules = strdup("evdev");
   names.model = strdup("pc105");
   names.layout = strdup("us");

   map = xkb_map_new_from_names(ctx, &names, 0);
   EINA_SAFETY_ON_NULL_RETURN(map);

   ecore_drm_device_keyboard_cached_context_set(ctx);
   ecore_drm_device_keyboard_cached_keymap_set(map);
}

E_API void *
e_modapi_init(E_Module *m)
{
   int w = 0, h = 0;
   struct xkb_context *ctx = NULL;
   struct xkb_keymap *map = NULL;

   printf("LOAD WL_DRM MODULE\n");

   /* try to init ecore_drm */
   /* if (!ecore_drm_init()) */
   /*   { */
   /*      fprintf(stderr, "Could not initialize ecore_drm"); */
   /*      return NULL; */
   /*   } */

   _e_mod_drm_keymap_set(ctx, map);

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

   e_comp_wl->screenshooter.read_pixels = _drm_read_pixels;

   ecore_evas_pointer_xy_get(e_comp->ee, &e_comp_wl->ptr.x,
                             &e_comp_wl->ptr.y);
   evas_event_feed_mouse_in(e_comp->evas, 0, NULL);

   e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
   e_comp_wl_input_touch_enabled_set(EINA_TRUE);

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
   e_comp_wl_input_keymap_set(NULL, NULL, NULL, ctx, map);

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
