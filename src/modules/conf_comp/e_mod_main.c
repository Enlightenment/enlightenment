#include "e.h"
#include "e_mod_main.h"

static E_Int_Menu_Augmentation *maug = NULL;

/* module private routines */
EINTERN Mod *_comp_mod = NULL;

/* public module routines. all modules must have these */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Composite Settings"
};

static void
_e_mod_comp_conf_cb(void *data __UNUSED__, E_Menu *m EINA_UNUSED, E_Menu_Item *mi __UNUSED__)
{
   e_int_config_comp_module(NULL, NULL);
}

static void
_e_mod_config_menu_create(void *d EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Composite"));
   e_util_menu_item_theme_icon_set(mi, "preferences-composite");
   e_menu_item_callback_set(mi, _e_mod_comp_conf_cb, NULL);
}

static Eina_Bool
_style_demo(void *data)
{
   Eina_List *style_shadows, *l;
   int demo_state;
   const E_Demo_Style_Item *it;

   demo_state = (long)evas_object_data_get(data, "style_demo_state");
   demo_state = (demo_state + 1) % 4;
   evas_object_data_set(data, "style_demo_state", (void *)(long)demo_state);

   style_shadows = evas_object_data_get(data, "style_shadows");
   EINA_LIST_FOREACH(style_shadows, l, it)
     {
        Evas_Object *ob = it->preview;
        Evas_Object *of = it->frame;

        switch (demo_state)
          {
           case 0:
             edje_object_signal_emit(ob, "e,state,visible,on", "e");
             edje_object_signal_emit(ob, "e,state,focus,on", "e");
             edje_object_part_text_set(of, "e.text.label", _("Visible"));
             break;

           case 1:
             edje_object_signal_emit(ob, "e,state,focus,off", "e");
             edje_object_part_text_set(of, "e.text.label", _("Focus-Out"));
             break;

           case 2:
             edje_object_signal_emit(ob, "e,state,focus,on", "e");
             edje_object_part_text_set(of, "e.text.label", _("Focus-In"));
             break;

           case 3:
             edje_object_signal_emit(ob, "e,state,visible,off", "e");
             edje_object_part_text_set(of, "e.text.label", _("Hidden"));
             break;

           default:
             break;
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_style_selector_del(void *data       __UNUSED__,
                    Evas *e,
                    Evas_Object *o,
                    void *event_info __UNUSED__)
{
   Eina_List *style_shadows, *style_list;
   Ecore_Timer *timer;
   Evas_Object *orec0;

   orec0 = evas_object_name_find(e, "style_shadows");
   style_list = evas_object_data_get(orec0, "list");

   style_shadows = evas_object_data_get(o, "style_shadows");
   if (style_shadows)
     {
        E_Demo_Style_Item *ds_it;

        EINA_LIST_FREE(style_shadows, ds_it)
          {
             style_list = eina_list_remove(style_list, ds_it);

             evas_object_del(ds_it->frame);
             evas_object_del(ds_it->livethumb);
             free(ds_it);
          }
        evas_object_data_set(o, "style_shadows", NULL);
     }

   timer = evas_object_data_get(o, "style_timer");
   if (timer)
     {
        ecore_timer_del(timer);
        evas_object_data_set(o, "style_timer", NULL);
     }

   evas_object_data_set(orec0, "list", style_list);
}

EINTERN Evas_Object *
_style_selector(Evas *evas, const char **source)
{
   Evas_Object *oi, *ob, *oo, *obd, *orec, *oly, *orec0;
   Eina_List *styles, *l, *style_shadows = NULL, *style_list;
   char *style;
   const char *str;
   int n, sel;
   Evas_Coord wmw, wmh;
   Ecore_Timer *timer;

   orec0 = evas_object_name_find(evas, "style_shadows");
   style_list = evas_object_data_get(orec0, "list");
   oi = e_widget_ilist_add(evas, 80, 80, source);
   evas_object_event_callback_add(oi, EVAS_CALLBACK_DEL,
                                  _style_selector_del, oi);
   sel = 0;
   styles = e_theme_comp_list();
   n = 0;
   EINA_LIST_FOREACH(styles, l, style)
     {
        E_Demo_Style_Item *ds_it;
        char buf[4096];

        ds_it = malloc(sizeof(E_Demo_Style_Item));

        ob = e_livethumb_add(evas);
        ds_it->livethumb = ob;
        e_livethumb_vsize_set(ob, 240, 240);

        oly = e_layout_add(e_livethumb_evas_get(ob));
        ds_it->layout = ob;
        e_layout_virtual_size_set(oly, 240, 240);
        e_livethumb_thumb_set(ob, oly);
        evas_object_show(oly);

        oo = edje_object_add(e_livethumb_evas_get(ob));
        ds_it->preview = oo;
        snprintf(buf, sizeof(buf), "e/comp/%s", style);
        e_theme_edje_object_set(oo, "base/theme/borders", buf);
        e_layout_pack(oly, oo);
        e_layout_child_move(oo, 39, 39);
        e_layout_child_resize(oo, 162, 162);
        edje_object_signal_emit(oo, "e,state,shadow,on", "e");
        edje_object_signal_emit(oo, "e,state,visible,on", "e");
        evas_object_show(oo);

        ds_it->frame = edje_object_add(evas);
        e_theme_edje_object_set
          (ds_it->frame, "base/theme/modules/comp", "e/modules/comp/preview");
        edje_object_part_swallow(ds_it->frame, "e.swallow.preview", ob);
        evas_object_show(ds_it->frame);
        style_shadows = eina_list_append(style_shadows, ds_it);

        obd = edje_object_add(e_livethumb_evas_get(ob));
        ds_it->border = obd;
        e_theme_edje_object_set(obd, "base/theme/borders",
                                "e/widgets/border/default/border");
        edje_object_part_text_set(obd, "e.text.title", _("Title"));
        edje_object_signal_emit(obd, "e,state,focused", "e");
        edje_object_part_swallow(oo, "e.swallow.content", obd);
        evas_object_show(obd);

        orec = evas_object_rectangle_add(e_livethumb_evas_get(ob));
        ds_it->client = orec;
        evas_object_color_set(orec, 0, 0, 0, 128);
        edje_object_part_swallow(obd, "e.swallow.client", orec);
        evas_object_show(orec);

        e_widget_ilist_append(oi, ds_it->frame, style, NULL, NULL, style);
        evas_object_show(ob);
        if (*source)
          {
             if (!strcmp(*source, style)) sel = n;
          }
        n++;

        style_list = eina_list_append(style_list, ds_it);
     }
   evas_object_data_set(orec0, "list", style_list);
   evas_object_data_set(oi, "style_shadows", style_shadows);
   timer = ecore_timer_add(3.0, _style_demo, oi);
   evas_object_data_set(oi, "style_timer", timer);
   evas_object_data_set(oi, "style_demo_state", (void *)1);
   e_widget_size_min_get(oi, &wmw, &wmh);
   e_widget_size_min_set(oi, 160, 100);
   e_widget_ilist_selected_set(oi, sel);
   e_widget_ilist_go(oi);

   EINA_LIST_FREE(styles, str)
     eina_stringshare_del(str);

   return oi;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   Mod *mod;
   E_Comp_Config *conf;

   conf = e_comp_config_get();
   if (!conf)
     {
        EINA_LOG_CRIT("COMP CONFIG MISSING!!!! ARGH!");
        return NULL;
     }

   mod = calloc(1, sizeof(Mod));
   m->data = mod;

   mod->module = m;
   e_configure_registry_category_add("appearance", 10, _("Look"), NULL,
                                     "preferences-look");
   e_configure_registry_item_add("appearance/comp", 120, _("Composite"), NULL,
                                 "preferences-composite", e_int_config_comp_module);

   e_configure_registry_category_add("internal", -1, _("Internal"),
                                     NULL, "enlightenment/internal");
   e_configure_registry_item_add("internal/comp_matches", -1, _("Composite Style Settings"),
                                 NULL, "preferences-composite", e_int_config_comp_match);

   mod->conf = conf;
   maug = e_int_menus_menu_augmentation_add_sorted("config/1", _("Composite"), _e_mod_config_menu_create, NULL, NULL, NULL);
   mod->conf->max_unmapped_pixels = 32 * 1024;
   mod->conf->keep_unmapped = 1;

   /* force some config vals off */
   mod->conf->lock_fps = 0;
   mod->conf->indirect = 0;

   /* XXX: update old configs. add config versioning */
   if (mod->conf->first_draw_delay == 0)
     mod->conf->first_draw_delay = 0.20;

   _comp_mod = mod;
   {
      E_Configure_Option *co;

      e_configure_option_domain_current_set("conf_comp");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("comp_settings"), _("Advanced composite settings panel"), _("composite"), _("border"));
      co->info = eina_stringshare_add("internal/comp_matches");
      E_CONFIGURE_OPTION_ICON(co, "preferences-composite");
   }

   e_module_delayed_set(m, 0);
   e_module_priority_set(m, -1000);

   return mod;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   Mod *mod = m->data;

   e_configure_registry_item_del("appearance/comp");
   e_configure_registry_category_del("appearance");

   e_configure_registry_item_del("internal/comp_matches");
   e_configure_registry_category_del("internal");

   if (mod->config_dialog)
     {
        e_object_del(E_OBJECT(mod->config_dialog));
        mod->config_dialog = NULL;
     }

   free(mod);
   e_configure_option_domain_clear("conf_comp");

   if (maug)
     {
        e_int_menus_menu_augmentation_del("config/1", maug);
        maug = NULL;
     }

   if (mod == _comp_mod) _comp_mod = NULL;

   return 1;
}
