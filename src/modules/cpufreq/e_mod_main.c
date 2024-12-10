#include "e_mod_main.h"
#include "e.h"

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION, "cpufreq",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

/* actual module specifics */
typedef struct _Instance Instance;

struct _Instance
{
  E_Gadcon_Client *gcc;
  E_Gadcon_Popup  *popup;
  Evas_Object     *o_cpu;
  Evas_Object     *o_popup_lay;
  Evas_Object     *o_popup_disp;
  Evas_Object     *o_gadimg;
  Ecore_Timer     *update_timer;
  Eina_List       *strings;
  int              gadimg_w, gadimg_h;
  int              popup_w;
};

static void      _button_cb_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static Eina_Bool _cpufreq_event_cb_powersave(void *data EINA_UNUSED, int type, void *event);

static E_Config_DD *conf_edd = NULL;

Config *cpufreq_config = NULL;

static void
_cb_cpf_render(void *data)
{
  const Cpf_Stats *stats = cpf_perf_stats_get();
  Instance        *inst  = data;
  int              i, h, sc, v, min, avg, max;
  void            *pix;
  char             buf[128];
  Evas_Object     *o;

  if (!stats) return;
  for (i = 0; i < stats->rend_num; i++)
    {
      Cpf_Render *r = stats->rend[i];

      if (!r) continue;
      if ((r->w == inst->popup_w) && (r->h == 128)) // what we asked for...
        {
          o = inst->o_popup_disp;
          if (o)
            {
              sc = ELM_SCALE_SIZE(1);
              if (sc < 1) sc = 1;

              if      (stats->core_num <=  1) h = r->real_h * 64;
              else if (stats->core_num <=  2) h = r->real_h * 32;
              else if (stats->core_num <=  4) h = r->real_h * 16;
              else if (stats->core_num <=  8) h = r->real_h *  8;
              else if (stats->core_num <= 16) h = r->real_h *  4;
              else if (stats->core_num <= 32) h = r->real_h *  2;
              else h = r->real_h;
              h *= sc;
              evas_object_image_smooth_scale_set(o, EINA_FALSE);
              evas_object_image_size_set(o, r->real_w, r->real_h);
              pix = evas_object_image_data_get(o, EINA_TRUE);
              if (pix)
                {
                  memcpy(pix, r->pixels, r->real_w * r->real_h * sizeof(int));
                  evas_object_image_data_set(o, pix);
                }
              evas_object_image_data_update_add(o, 0, 0, r->real_w,
                                                r->real_h);
              evas_object_size_hint_min_set(o, r->real_w, h);
            }
        }
      else if ((r->w == inst->gadimg_w)
               && (r->h == inst->gadimg_h)) // what we asked for...
        {
          o = inst->o_gadimg;
          if (o)
            {
              evas_object_image_smooth_scale_set(o, EINA_TRUE);
              evas_object_image_size_set(o, r->real_w, r->real_h);
              pix = evas_object_image_data_get(o, EINA_TRUE);
              if (pix)
                {
                  memcpy(pix, r->pixels, r->real_w * r->real_h * sizeof(int));
                  evas_object_image_data_set(o, pix);
                }
              evas_object_image_data_update_add(o, 0, 0, r->real_w,
                                                r->real_h);
            }
        }
    }
  if (stats->core_num > 0)
    {
      v = 0;
      for (i = 0; i < stats->core_num; i++) v += stats->core_perf[i].usage;
      v = (v + (5 * stats->core_num)) / (stats->core_num * 10); // 0->100 avg
      snprintf(buf, sizeof(buf), "%i%%", v);
      edje_object_part_text_set(inst->o_cpu, "e.text.cpu.usage", buf);

      v = 0;
      for (i = 0; i < stats->core_num; i++) v += stats->core_perf[i].freq;
      v = (v + (stats->core_num / 2)) / stats->core_num; // mhz avg
      if (v < 1000) snprintf(buf, sizeof(buf), "%i", v); // mhz
      else snprintf(buf, sizeof(buf), "%1.1f", (double)v / 1000.0); // ghz
      edje_object_part_text_set(inst->o_cpu, "e.text.cpu.freq", buf);

      min = -1;
      max = 0;
      avg = 0;
      for (i = 0; i < stats->core_num; i++)
        {
          int u = stats->core_perf[i].usage;

          if (u > max) max = u;
          if (min == -1) min = u;
          else if (u < min) min = u;
          avg += u;
        }
      min = (min + 5) / 10;
      avg
        = (avg + (5 * stats->core_num)) / (stats->core_num * 10); // 0->100 avg
      max = (max + 5) / 10;
      if (inst->o_popup_lay)
        {
          o = elm_layout_edje_get(inst->o_popup_lay);
          snprintf(buf, sizeof(buf), "%i%%", max);
          edje_object_part_text_set(o, "e.text.cpu.usage.max", buf);
          snprintf(buf, sizeof(buf), "%i%%", avg);
          edje_object_part_text_set(o, "e.text.cpu.usage.avg", buf);
          snprintf(buf, sizeof(buf), "%i%%", min);
          edje_object_part_text_set(o, "e.text.cpu.usage.min", buf);
        }

      min = -1;
      max = 0;
      avg = 0;
      for (i = 0; i < stats->core_num; i++)
        {
          int u = stats->core_perf[i].freq;

          if (u > max) max = u;
          if (min == -1) min = u;
          else if (u < min) min = u;
          avg += u;
        }
      avg = avg / stats->core_num;
      if (inst->o_popup_lay)
        {
          o = elm_layout_edje_get(inst->o_popup_lay);
          snprintf(buf, sizeof(buf), "%i Mhz", max);
          edje_object_part_text_set(o, "e.text.cpu.freq.max", buf);
          snprintf(buf, sizeof(buf), "%i Mhz", avg);
          edje_object_part_text_set(o, "e.text.cpu.freq.avg", buf);
          snprintf(buf, sizeof(buf), "%i Mhz", min);
          edje_object_part_text_set(o, "e.text.cpu.freq.min", buf);
        }
    }
}

static void
_cb_gad_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
               void *info EINA_UNUSED)
{
  Instance *inst = data;
  Evas_Coord w, h;

  cpf_render_unreq(CPF_RENDER_COLORBAR_CPU_USAGE, inst->gadimg_w,
                   inst->gadimg_h);
  evas_object_geometry_get(obj, NULL, NULL, &w, &h);
  inst->gadimg_w = w;
  inst->gadimg_h = h;
  cpf_render_req(CPF_RENDER_COLORBAR_CPU_USAGE, inst->gadimg_w,
                 inst->gadimg_h);
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o, *o_base;
   E_Gadcon_Client *gcc;
   Instance *inst;

   inst = E_NEW(Instance, 1);

   o_base = o = edje_object_add(gc->evas);
   e_theme_edje_object_set(o, "base/theme/modules/cpufreq",
                           "e/modules/cpufreq/gadget");

   inst->o_gadimg = o = evas_object_image_filled_add(gc->evas);
   evas_object_image_alpha_set(o, EINA_TRUE);
   evas_object_image_size_set(o, 40, 40);
   inst->gadimg_w    = 40;

   inst->gadimg_h = 40;
   cpf_render_req(CPF_RENDER_COLORBAR_CPU_USAGE, 40, 40);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE, _cb_gad_resize, inst);
   edje_object_part_swallow(o_base, "e.swallow.content", o);
   evas_object_show(o);

   gcc = e_gadcon_client_new(gc, name, id, style, o_base);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->o_cpu = o_base;
   inst->popup_w = ELM_SCALE_SIZE(320);

   evas_object_event_callback_add(o_base, EVAS_CALLBACK_MOUSE_DOWN,
                                  _button_cb_mouse_down, inst);
   cpufreq_config->instances =
     eina_list_append(cpufreq_config->instances, inst);

   if (!cpufreq_config->handler)
     cpufreq_config->handler = ecore_event_handler_add(
       E_EVENT_POWERSAVE_UPDATE, _cpufreq_event_cb_powersave, NULL);

   cpf_render_req(CPF_RENDER_COLORBAR_ALL, inst->popup_w, 128);
   cpf_event_callback_add(CPF_EVENT_STATS, _cb_cpf_render, inst);
   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;
   const char *s;

   inst = gcc->data;
   cpf_event_callback_del(CPF_EVENT_STATS, _cb_cpf_render, inst);
   cpf_render_unreq(CPF_RENDER_COLORBAR_ALL, inst->popup_w, 128);
   E_FREE_FUNC(inst->popup, e_object_del);
   cpufreq_config->instances =
     eina_list_remove(cpufreq_config->instances, inst);
   evas_object_del(inst->o_gadimg);
   evas_object_del(inst->o_cpu);
   if (inst->update_timer) ecore_timer_del(inst->update_timer);
   EINA_LIST_FREE(inst->strings, s) eina_stringshare_del(s);
   free(inst);

   if (!cpufreq_config->instances)
     {
        if (cpufreq_config->handler)
          {
             ecore_event_handler_del(cpufreq_config->handler);
             cpufreq_config->handler = NULL;
          }
     }
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Cpufreq");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-cpufreq.edj",
            e_module_dir_get(cpufreq_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   static char idbuff[32];

   snprintf(idbuff, sizeof(idbuff), "%s.%d", _gadcon_class.name,
            eina_list_count(cpufreq_config->instances));
   return idbuff;
}

static void
_cpufreq_cb_menu_configure(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   if (!cpufreq_config) return;
   if (cpufreq_config->config_dialog) return;
   e_int_config_cpufreq_module(NULL, NULL);
}

static void
_popup_del(Instance *inst)
{
  E_FREE_FUNC(inst->popup, e_object_del);
  inst->o_popup_disp = NULL;
  inst->o_popup_lay = NULL;
}

static void _popup_del_cb(void *obj)
{
  _popup_del(e_object_data_get(obj));
}

static void
_cb_popup_image_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                    void *info EINA_UNUSED)
{
  Instance *inst = data;
  inst->o_popup_disp = NULL;
  inst->o_popup_lay  = NULL;
}

static char *
_cb_unit_func(double v)
{
  if      (v < (1.0/4.0)) return strdup(_("Powersave"));
  else if (v < (2.0/4.0)) return strdup(_("Balanced Low"));
  else if (v < (3.0/4.0)) return strdup(_("Balanced Hi"));
  else                    return strdup(_("Performance"));
}

static void
_cb_ac_power_slider(void *data EINA_UNUSED, Evas_Object *o,
                    void *info EINA_UNUSED)
{
  double v = elm_slider_value_get(o);

  if (v < (1.0 / 4.0))
    {
      elm_slider_value_set(o, (0.0 / 3.0));
      cpf_perf_level_set(0);
    }
  else if (v < (2.0 / 4.0))
    {
      elm_slider_value_set(o, (1.0 / 3.0));
      cpf_perf_level_set(33);
    }
  else if (v < (3.0 / 4.0))
    {
      elm_slider_value_set(o, (2.0 / 3.0));
      cpf_perf_level_set(67);
    }
  else
    {
      elm_slider_value_set(o, (3.0 / 3.0));
      cpf_perf_level_set(100);
    }
}

static void
_cb_settings(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
  if (!cpufreq_config) return;
  if (cpufreq_config->config_dialog) return;
  e_int_config_cpufreq_module(NULL, NULL);
}

static void
_button_cb_mouse_down(void *data, Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Evas_Object           *o, *o_table, *o_lay;
   int                    lv;
   const char            *f;

   if ((ev->button == 1) && (!inst->popup))
     {
       inst->popup = e_gadcon_popup_new(inst->gcc, 0);

       o_table = o = elm_table_add(e_comp->elm);

       o_lay = o = elm_layout_add(e_comp->elm);
       f = e_theme_edje_file_get("base/theme/modules/cpufreq",
                                 "e/modules/cpufreq/popup/detail");
       elm_layout_file_set(o, f, "e/modules/cpufreq/popup/detail");
       elm_table_pack(o_table, o, 0, 0, 10, 1);
       evas_object_show(o);
       inst->o_popup_lay = o;

       o = evas_object_image_filled_add(e_comp->evas);
       evas_object_image_alpha_set(o, EINA_TRUE);
       evas_object_image_size_set(o, inst->popup_w, 128);
       evas_object_size_hint_min_set(o, inst->popup_w, 128);
       elm_object_part_content_set(o_lay, "e.swallow.content", o);
       evas_object_show(o);
       inst->o_popup_disp = o;
       evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_popup_image_del,
                                      inst);

       o = elm_slider_add(e_comp->elm);
       elm_slider_horizontal_set(o, EINA_TRUE);
       elm_slider_min_max_set(o, 0, 1);
       elm_slider_step_set(o, (1.0 / 4.0));
       elm_slider_indicator_show_set(o, EINA_FALSE);
       elm_slider_units_format_function_set(o, _cb_unit_func, NULL);
       evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
       evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
       elm_table_pack(o_table, o, 0, 1, 10, 1);
       evas_object_show(o);

       lv = cpf_perf_level_get(); // get from config
       if (lv < 33) elm_slider_value_set(o, (0.0 / 3.0));
       else if (lv < 67) elm_slider_value_set(o, (1.0 / 3.0));
       else if (lv < 100) elm_slider_value_set(o, (2.0 / 3.0));
       else elm_slider_value_set(o, (3.0 / 3.0));

       evas_object_smart_callback_add(o, "changed", _cb_ac_power_slider, inst);

       o = elm_button_add(e_comp->elm);
       elm_object_text_set(o, _("Settings"));
       evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
       evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
       elm_table_pack(o_table, o, 0, 2, 10, 1);
       evas_object_show(o);

       evas_object_smart_callback_add(o, "clicked", _cb_settings, inst);

       e_gadcon_popup_content_set(inst->popup, o_table);
       e_gadcon_popup_show(inst->popup);
       e_object_data_set(E_OBJECT(inst->popup), inst);
       E_OBJECT_DEL_SET(inst->popup, _popup_del_cb);

       _cb_cpf_render(inst);
     }
   else if ((ev->button == 1) && (inst->popup))
     {
       E_FREE_FUNC(inst->popup, e_object_del);
     }
   else if (ev->button == 3)
     {
        E_Menu *m;
        E_Menu_Item *mi;
        int cx, cy;

        m = e_menu_new();

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _cpufreq_cb_menu_configure, NULL);

        m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon,
                                          &cx, &cy, NULL, NULL);

        e_menu_activate_mouse(m,
                              e_zone_current_get(),
                              cx + ev->output.x, cy + ev->output.y, 1, 1,
                              E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
        evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                                 EVAS_BUTTON_NONE, ev->timestamp, NULL);
     }
}

static void
_handle_powersave_mode(E_Powersave_Mode mode)
{
   switch (mode)
     {
      case E_POWERSAVE_MODE_NONE:
        printf("PWSV: none\n");
        EINA_FALLTHROUGH;
      case E_POWERSAVE_MODE_LOW:
        printf("PWSV: low=%i\n", cpufreq_config->power_hi);
        cpf_perf_level_set(cpufreq_config->power_hi);
        break;

      case E_POWERSAVE_MODE_MEDIUM:
        printf("PWSV: med\n");
        EINA_FALLTHROUGH;
      case E_POWERSAVE_MODE_HIGH:
        printf("PWSV: hi\n");
        EINA_FALLTHROUGH;
      case E_POWERSAVE_MODE_EXTREME:
        printf("PWSV: extreme=%i\n", cpufreq_config->power_lo);
        cpf_perf_level_set(cpufreq_config->power_lo);
        break;

      case E_POWERSAVE_MODE_FREEZE:
        printf("PWSV: freeze\n");
        cpf_perf_level_set(0);
        break;

      default:
        break;
     }
}

static Eina_Bool
_cpufreq_event_cb_powersave(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Powersave_Update *ev = event;

   _handle_powersave_mode(ev->mode);
   return ECORE_CALLBACK_PASS_ON;
}

/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Cpufreq"
};

E_API void *
e_modapi_init(E_Module *m)
{
   conf_edd = E_CONFIG_DD_NEW("Cpufreq_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, config_version, INT);
   E_CONFIG_VAL(D, T, check_interval, DOUBLE);
   E_CONFIG_VAL(D, T, power_lo, INT);
   E_CONFIG_VAL(D, T, power_hi, INT);

   cpufreq_config = e_config_domain_load("module.cpufreq", conf_edd);
   if ((cpufreq_config) &&
       (cpufreq_config->config_version != CPUFREQ_CONFIG_VERSION))
     E_FREE(cpufreq_config);

   if (!cpufreq_config)
     {
        cpufreq_config = E_NEW(Config, 1);
        cpufreq_config->config_version = CPUFREQ_CONFIG_VERSION;
        cpufreq_config->check_interval = 0.2;
        cpufreq_config->power_lo       = 0;
        cpufreq_config->power_hi       = 100;
     }
   E_CONFIG_LIMIT(cpufreq_config->check_interval, 0.1, 1.0);
   E_CONFIG_LIMIT(cpufreq_config->power_lo, 0, 100);
   E_CONFIG_LIMIT(cpufreq_config->power_hi, 0, 100);

   cpf_init();

   cpufreq_config->module = m;

   e_gadcon_provider_register(&_gadcon_class);
   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL,
                                     "preferences-advanced");
   e_configure_registry_item_add("advanced/cpufreq", 120, _("CPU Frequency"),
                                 NULL, "preferences-cpu-speed",
                                 e_int_config_cpufreq_module);
   cpf_poll_time_set(cpufreq_config->check_interval);

   _handle_powersave_mode(e_powersave_mode_get());
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   e_configure_registry_item_del("advanced/cpufreq");
   e_configure_registry_category_del("advanced");

   e_gadcon_provider_unregister(&_gadcon_class);

   cpf_shutdown();

   if (cpufreq_config->config_dialog)
     e_object_del(E_OBJECT(cpufreq_config->config_dialog));

   free(cpufreq_config);
   cpufreq_config = NULL;
   E_CONFIG_DD_FREE(conf_edd);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.cpufreq", conf_edd, cpufreq_config);
   return 1;
}

