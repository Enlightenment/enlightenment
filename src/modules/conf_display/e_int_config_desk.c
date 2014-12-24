#include "e.h"

static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static void         _cb_config(void *data, void *data2);
static Eina_Bool    _cb_bg_change(void *data, int type, void *event);

struct _E_Config_Dialog_Data
{
   int                  man_num;
   int                  zone_num;
   int                  desk_x;
   int                  desk_y;
   Eina_Stringshare  *bg;
   char                *name;
   char                *profile;
   Evas_Object         *preview;
   Ecore_Event_Handler *hdl;
};

E_Config_Dialog *
e_int_config_desk(Evas_Object *parent EINA_UNUSED, const char *params)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;
   E_Config_Dialog_Data *cfdata;
   int man_num, zone_num, dx, dy;

   if (!params) return NULL;
   man_num = zone_num = dx = dy = -1;
   if (sscanf(params, "%i %i %i %i", &man_num, &zone_num, &dx, &dy) != 4)
     return NULL;

   if (e_config_dialog_find("E", "internal/desk")) return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->man_num = man_num;
   cfdata->zone_num = zone_num;
   cfdata->desk_x = dx;
   cfdata->desk_y = dy;

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply;
   v->basic.create_widgets = _basic_create;
   v->override_auto_apply = 1;

   cfd = e_config_dialog_new(NULL, _("Desk Settings"), "E", "internal/desk",
                             "preferences-desktop", 0, v, cfdata);
   return cfd;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   Eina_List *l;
   char name[40];
   int ok = 0;
   E_Config_Desktop_Window_Profile *prof;
   cfdata->bg = e_bg_file_get(cfdata->man_num, cfdata->zone_num, cfdata->desk_x, cfdata->desk_y);

   for (l = e_config->desktop_names; l; l = l->next)
     {
        E_Config_Desktop_Name *dn;

        dn = l->data;
        if (!dn) continue;
        if (dn->manager != cfdata->man_num) continue;
        if (dn->zone != cfdata->zone_num) continue;
        if ((dn->desk_x != cfdata->desk_x) || (dn->desk_y != cfdata->desk_y))
          continue;

        if (dn->name)
          cfdata->name = strdup(dn->name);
        ok = 1;
        break;
     }
   if (!ok)
     {
        snprintf(name, sizeof(name), _(e_config->desktop_default_name), cfdata->desk_x, cfdata->desk_y);
        cfdata->name = strdup(name);
     }
   ok = 0;
   EINA_LIST_FOREACH(e_config->desktop_window_profiles, l, prof)
     {
        if (!((prof->manager == cfdata->man_num) &&
              (prof->zone == cfdata->zone_num) &&
              (prof->desk_x == cfdata->desk_x) &&
              (prof->desk_y == cfdata->desk_y)))
          continue;

        if (prof->profile)
          cfdata->profile = strdup(prof->profile);
        ok = 1;
        break;
     }

   if (!ok)
     cfdata->profile = strdup(e_config->desktop_default_window_profile);
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = cfd->data;
   if (!cfdata) return NULL;
   _fill_data(cfdata);
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   if (cfdata->hdl)
     ecore_event_handler_del(cfdata->hdl);
   eina_stringshare_del(cfdata->bg);
   E_FREE(cfdata->name);
   E_FREE(cfdata->profile);
   E_FREE(cfdata);
}

static int
_basic_apply(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   char name[40];

   if ((!cfdata->name) || (!cfdata->name[0]))
     {
        snprintf(name, sizeof(name), _(e_config->desktop_default_name),
                 cfdata->desk_x, cfdata->desk_y);
        free(cfdata->name);
        cfdata->name = strdup(name);
     }

   if (!cfdata->profile[0])
     cfdata->profile = strdup(e_config->desktop_default_window_profile);
   e_desk_name_del(cfdata->man_num, cfdata->zone_num,
                   cfdata->desk_x, cfdata->desk_y);
   e_desk_name_add(cfdata->man_num, cfdata->zone_num,
                   cfdata->desk_x, cfdata->desk_y, cfdata->name);
   e_desk_name_update();

   e_desk_window_profile_del(cfdata->man_num, cfdata->zone_num,
                             cfdata->desk_x, cfdata->desk_y);
   e_desk_window_profile_add(cfdata->man_num, cfdata->zone_num,
                             cfdata->desk_x, cfdata->desk_y, cfdata->profile);
   e_desk_window_profile_update();
   e_bg_del(cfdata->man_num, cfdata->zone_num, cfdata->desk_x, cfdata->desk_y);
   e_bg_add(cfdata->man_num, cfdata->zone_num,
            cfdata->desk_x, cfdata->desk_y, cfdata->bg);
   e_bg_update();

   e_config_save_queue();
   return 1;
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *of, *ol, *ob;
   E_Zone *zone;

   zone = e_zone_current_get(e_comp_get(NULL));

   o = e_widget_list_add(evas, 0, 0);

   ol = e_widget_list_add(evas, 0, 1);
   ob = e_widget_label_add(evas, _("Name"));
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);
   ob = e_widget_entry_add(evas, &(cfdata->name), NULL, NULL, NULL);
   e_widget_list_object_append(ol, ob, 1, 1, 0.5);
   e_widget_list_object_append(o, ol, 1, 1, 0.5);
   of = e_widget_frametable_add(evas, _("Desktop Window Profile"), 0);
   ob = e_widget_label_add(evas, _("Profile name"));
   e_widget_frametable_object_append(of, ob, 0, 0, 1, 1, 1, 1, 0, 0);
   ob = e_widget_entry_add(evas, &(cfdata->profile), NULL, NULL, NULL);
   e_widget_disabled_set(ob, !(e_config->use_desktop_window_profile));
   e_widget_frametable_object_append(of, ob, 1, 0, 2, 1, 1, 1, 1, 0);
   e_widget_list_object_append(o, of, 1, 1, 0.5);
   of = e_widget_frametable_add(evas, _("Wallpaper"), 0);
   ob = e_widget_preview_add(evas, 240, (240 * zone->h) / zone->w);
   cfdata->preview = ob;
   if (cfdata->bg)
     e_widget_preview_edje_set(ob, cfdata->bg, "e/desktop/background");
   e_widget_frametable_object_append(of, ob, 0, 0, 3, 1, 1, 1, 1, 0);
   ob = e_widget_button_add(evas, _("Set"), "configure",
                            _cb_config, cfdata, NULL);
   e_widget_frametable_object_append(of, ob, 1, 1, 1, 1, 1, 1, 1, 0);
   e_widget_list_object_append(o, of, 1, 1, 0.5);

   if (cfdata->hdl)
     ecore_event_handler_del(cfdata->hdl);
   cfdata->hdl = ecore_event_handler_add(E_EVENT_BG_UPDATE, _cb_bg_change, cfdata);

   return o;
}

static void
_cb_config(void *data, void *data2 __UNUSED__)
{
   E_Config_Dialog_Data *cfdata;
   char buf[256];

   cfdata = data;
   if (!cfdata) return;
   snprintf(buf, sizeof(buf), "%i %i %i %i",
            cfdata->man_num, cfdata->zone_num, cfdata->desk_x, cfdata->desk_y);
   e_configure_registry_call("internal/wallpaper_desk", NULL, buf);
}

static Eina_Bool
_cb_bg_change(void *data, int type, void *event)
{
   E_Event_Bg_Update *ev;
   E_Config_Dialog_Data *cfdata;
   const char *file;

   if (type != E_EVENT_BG_UPDATE) return ECORE_CALLBACK_PASS_ON;

   cfdata = data;
   ev = event;
   if (ev->manager != cfdata->man_num) return ECORE_CALLBACK_PASS_ON;
   if (ev->zone != cfdata->zone_num) return ECORE_CALLBACK_PASS_ON;
   if (ev->desk_x != cfdata->desk_x) return ECORE_CALLBACK_PASS_ON;
   if (ev->desk_y != cfdata->desk_y) return ECORE_CALLBACK_PASS_ON;

   file = e_bg_file_get(cfdata->man_num, cfdata->zone_num,
                        cfdata->desk_x, cfdata->desk_y);
   eina_stringshare_replace(&cfdata->bg, file);
   e_widget_preview_edje_set(cfdata->preview, cfdata->bg,
                             "e/desktop/background");

   return ECORE_CALLBACK_PASS_ON;
}

