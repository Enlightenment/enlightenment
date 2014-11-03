#include "e_mod_main.h"

static void         _pol_conf_desk_add(Config *conf, E_Desk *desk);
static void         _pol_conf_desk_del(Config *conf, Config_Desk *d);
static Config_Desk *_pol_conf_desk_get(Config *conf, Config_Desk *d);

static void        *_pol_cfd_data_create(E_Config_Dialog *cfd);
static void         _pol_cfd_data_free(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static int          _pol_cfd_data_basic_apply(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static void         _pol_cfd_desk_list_update(E_Config_Dialog_Data *cfdata, E_Zone *zone);
static void         _pol_cfd_hook_zone_change(void *data, Evas_Object *obj);
static Evas_Object *_pol_cfd_data_basic_widgets_create(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas, E_Config_Dialog_Data *cfdata);

static void
_pol_conf_desk_add(Config *conf, E_Desk *desk)
{
   Config_Desk *d;

   d = E_NEW(Config_Desk, 1);
   d->comp_num = desk->zone->comp->num;
   d->zone_num = desk->zone->num;
   d->x = desk->x;
   d->y = desk->y;
   d->enable = 1;

   conf->desks = eina_list_append(conf->desks, d);
}

static void
_pol_conf_desk_del(Config *conf, Config_Desk *d)
{
   conf->desks = eina_list_remove(conf->desks, d);
   free(d);
}

static Config_Desk *
_pol_conf_desk_get(Config *conf, Config_Desk *d)
{
   Eina_List *l;
   Config_Desk *d2;

   EINA_LIST_FOREACH(conf->desks, l, d2)
     {
        if ((d2->comp_num == d->comp_num) &&
            (d2->zone_num == d->zone_num) &&
            (d2->x == d->x) && (d2->y == d->y))
          {
             return d2;
          }
     }

   return NULL;
}

static void *
_pol_cfd_data_create(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata = NULL;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->conf = E_NEW(Config, 1);
   cfdata->conf->launcher.title = eina_stringshare_ref(_pol_mod->conf->launcher.title);
   cfdata->conf->launcher.clas = eina_stringshare_ref(_pol_mod->conf->launcher.clas);
   cfdata->conf->launcher.type = _pol_mod->conf->launcher.type;
   cfdata->conf->use_softkey = _pol_mod->conf->use_softkey;
   cfdata->conf->softkey_size = _pol_mod->conf->softkey_size;

   _pol_mod->conf_dialog = cfd;

   return cfdata;
}

static void
_pol_cfd_data_free(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   _pol_mod->conf_dialog = NULL;
   E_FREE_LIST(cfdata->conf->desks, free);
   eina_stringshare_del(cfdata->conf->launcher.title);
   eina_stringshare_del(cfdata->conf->launcher.clas);
   free(cfdata->conf);
   free(cfdata);
}

static int
_pol_cfd_data_basic_apply(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   E_Comp *comp;
   E_Zone *zone;
   E_Desk *desk;
   Pol_Softkey *softkey;
   Pol_Desk *pd;
   Config_Desk *d, *d2;
   Eina_Bool changed = EINA_FALSE;
   const Eina_List *l;
   Eina_Inlist *il;
   Eina_Iterator *it;

   if (_pol_mod->conf->use_softkey != cfdata->conf->use_softkey)
     {
        _pol_mod->conf->use_softkey = cfdata->conf->use_softkey;
        if (_pol_mod->conf->use_softkey)
          {
             it = eina_hash_iterator_data_new(hash_pol_desks);
             while (eina_iterator_next(it, (void **)&pd))
               {
                  softkey = e_mod_pol_softkey_get(pd->zone);
                  if (!softkey)
                    softkey = e_mod_pol_softkey_add(pd->zone);
                  if (e_desk_current_get(pd->zone) == pd->desk)
                    e_mod_pol_softkey_show(softkey);
               }
             eina_iterator_free(it);
          }
        else
          {
             EINA_INLIST_FOREACH_SAFE(_pol_mod->softkeys, il, softkey)
               e_mod_pol_softkey_del(softkey);
          }

        changed = EINA_TRUE;
     }

   if (_pol_mod->conf->softkey_size != cfdata->conf->softkey_size)
     {
        _pol_mod->conf->softkey_size = cfdata->conf->softkey_size;
        if (_pol_mod->conf->use_softkey)
          {
             EINA_INLIST_FOREACH(_pol_mod->softkeys, softkey)
               e_mod_pol_softkey_update(softkey);
          }
        changed = EINA_TRUE;
     }

   EINA_LIST_FOREACH(cfdata->conf->desks, l, d)
     {
        comp = e_comp_number_get(d->comp_num);
        zone = e_comp_zone_number_get(comp, d->zone_num);
        desk = e_desk_at_xy_get(zone, d->x, d->y);
        if (!desk) continue;

        d2 = _pol_conf_desk_get(_pol_mod->conf, d);
        if (d2)
          {
             /* disable policy for this desktop */
             if (d2->enable != d->enable)
               {
                  pd = eina_hash_find(hash_pol_desks, &desk);
                  if (pd) e_mod_pol_desk_del(pd);
                  _pol_conf_desk_del(_pol_mod->conf, d2);
                  changed = EINA_TRUE;
               }
          }
        else
          {
             /* apply policy for all clients in this desk,
              * and add desk to configuration list and desk hash */
             if (d->enable)
               {
                  e_mod_pol_desk_add(desk);
                  _pol_conf_desk_add(_pol_mod->conf, desk);
                  changed = EINA_TRUE;
               }
          }
     }

   if (changed)
     e_config_save_queue();

   return 1;
}

static void
_pol_cfd_desk_list_update(E_Config_Dialog_Data *cfdata, E_Zone *zone)
{
   Evas_Object *o, *ch;
   Evas *evas;
   E_Desk *desk;
   Config_Desk *d, *d2;
   int i, n;

   evas = evas_object_evas_get(cfdata->o_list);
   evas_object_del(cfdata->o_desks);
   E_FREE_LIST(cfdata->conf->desks, free);

   o = e_widget_list_add(evas, 1, 0);
   cfdata->o_desks = o;

   n = zone->desk_y_count * zone->desk_x_count;
   for (i = 0; i < n; i++)
     {
        desk = zone->desks[i];

        d = E_NEW(Config_Desk, 1);
        d->comp_num = zone->comp->num;
        d->zone_num = zone->num;
        d->x = desk->x;
        d->y = desk->y;
        d->enable = 0;

        d2 = _pol_conf_desk_get(_pol_mod->conf, d);
        if (d2)
          d->enable = d2->enable;

        ch = e_widget_check_add(evas, desk->name, &d->enable);
        e_widget_list_object_append(o, ch, 1, 1, 0.5);

        cfdata->conf->desks = eina_list_append(cfdata->conf->desks, d);
     }

   e_widget_list_object_append(cfdata->o_list, o, 1, 1, 0.5);
}

static void
_pol_cfd_hook_zone_change(void *data, Evas_Object *obj)
{
   E_Config_Dialog_Data *cfdata;
   E_Zone *zone;
   int n;

   cfdata = data;
   n = e_widget_ilist_selected_get(obj);
   zone = e_widget_ilist_nth_data_get(obj, n);
   if (zone)
     _pol_cfd_desk_list_update(cfdata, zone);
}

static Evas_Object *
_pol_cfd_data_basic_widgets_create(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *base, *fl, *lb, *lo, *o;
   E_Comp *comp;
   E_Zone *zone;
   Eina_List *l;

   comp = e_comp_get(NULL);

   base = e_widget_list_add(evas, 0, 0);

   fl = e_widget_framelist_add(evas, _("Softkey"), 0);
   o = e_widget_check_add(evas, _("Use softkey for navigation among the windows"),
                          &cfdata->conf->use_softkey);
   e_widget_framelist_object_append(fl, o);
   o = e_widget_label_add(evas, _("Icon Size"));
   e_widget_framelist_object_append(fl, o);
   o = e_widget_slider_add(evas, 1, 0, "%1.0f", 32, 256, 1, 0, NULL,
                           &(cfdata->conf->softkey_size), 150);
   e_widget_framelist_object_append(fl, o);
   e_widget_list_object_append(base, fl, 1, 1, 0.5);

   fl = e_widget_framelist_add(evas, _("Virtual Desktops"), 0);
   lb = e_widget_label_add(evas, _("Enable mobile policy per desktop"));
   e_widget_framelist_object_append(fl, lb);

   lo = e_widget_list_add(evas, 0, 1);
   cfdata->o_list = lo;
   o = e_widget_ilist_add(evas, 0, 0, NULL);
   e_widget_ilist_multi_select_set(o, EINA_FALSE);
   e_widget_size_min_set(o, 100, 100);
   e_widget_on_change_hook_set(o, _pol_cfd_hook_zone_change, cfdata);
   EINA_LIST_REVERSE_FOREACH(comp->zones, l, zone)
     e_widget_ilist_append(o, NULL, zone->name, NULL, zone, NULL);
   e_widget_ilist_go(o);
   e_widget_ilist_selected_set(o, 0);
   e_widget_list_object_append(lo, o, 1, 1, 0.5);

   /* update virtual desktops of first zone */
   zone = eina_list_data_get(comp->zones);
   _pol_cfd_desk_list_update(cfdata, zone);

   e_widget_framelist_object_append(fl, lo);
   e_widget_list_object_append(base, fl, 1, 1, 0.5);

   return base;
}

void
e_mod_pol_conf_init(Mod *mod)
{
   E_Comp *comp;
   E_Zone *zone;
   E_Desk *desk;
   Config *conf;

   mod->conf_desk_edd = E_CONFIG_DD_NEW("Policy_Mobile_Config_Desk", Config_Desk);
#undef T
#undef D
#define T Config_Desk
#define D mod->conf_desk_edd
   E_CONFIG_VAL(D, T, comp_num, INT);
   E_CONFIG_VAL(D, T, zone_num, UINT);
   E_CONFIG_VAL(D, T, x, INT);
   E_CONFIG_VAL(D, T, y, INT);
   E_CONFIG_VAL(D, T, enable, INT);

   mod->conf_edd = E_CONFIG_DD_NEW("Policy_Mobile_Config", Config);
#undef T
#undef D
#define T Config
#define D mod->conf_edd
   E_CONFIG_VAL(D, T, launcher.title, STR);
   E_CONFIG_VAL(D, T, launcher.clas, STR);
   E_CONFIG_VAL(D, T, launcher.type, UINT);
   E_CONFIG_LIST(D, T, desks, mod->conf_desk_edd);
   E_CONFIG_VAL(D, T, use_softkey, INT);
   E_CONFIG_VAL(D, T, softkey_size, INT);

#undef T
#undef D

   mod->conf = e_config_domain_load("module.policy-mobile", mod->conf_edd);
   if (!mod->conf)
     {
        conf = E_NEW(Config, 1);
        mod->conf = conf;
        conf->launcher.title = eina_stringshare_add("Illume Home");
        conf->launcher.clas = eina_stringshare_add("Illume-Home");
        conf->launcher.type = E_WINDOW_TYPE_NORMAL;
        conf->use_softkey = 1;
        conf->softkey_size = 42;

        comp = e_comp_get(NULL);
        zone = e_zone_current_get(comp);
        desk = e_desk_current_get(zone);
        _pol_conf_desk_add(conf, desk);
     }
}

void
e_mod_pol_conf_shutdown(Mod *mod)
{
   Config *conf;

   if (mod->conf)
     {
        conf = mod->conf;
        E_FREE_LIST(conf->desks, free);
        eina_stringshare_del(conf->launcher.title);
        eina_stringshare_del(conf->launcher.clas);
        free(mod->conf);
     }

   E_CONFIG_DD_FREE(mod->conf_desk_edd);
   E_CONFIG_DD_FREE(mod->conf_edd);
}

Config_Desk *
e_mod_pol_conf_desk_get_by_nums(Config *conf, unsigned int comp_num, unsigned int zone_num, int x, int y)
{
   Eina_List *l;
   Config_Desk *d2;

   EINA_LIST_FOREACH(conf->desks, l, d2)
     {
        if ((d2->comp_num == comp_num) &&
            (d2->zone_num == zone_num) &&
            (d2->x == x) && (d2->y == y))
          {
             return d2;
          }
     }

   return NULL;
}

E_Config_Dialog *
e_int_config_pol_mobile(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;
   char buf[PATH_MAX];

   if (e_config_dialog_find("E", "windows/policy-mobile"))
     return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _pol_cfd_data_create;
   v->free_cfdata = _pol_cfd_data_free;
   v->basic.apply_cfdata = _pol_cfd_data_basic_apply;
   v->basic.create_widgets = _pol_cfd_data_basic_widgets_create;

   snprintf(buf, sizeof(buf), "%s/e-module-policy-mobile.edj",
            e_module_dir_get(_pol_mod->module));

   cfd = e_config_dialog_new(NULL, _("Mobile Policy Configuration"), "E",
                             "windows/policy-mobile", buf, 0, v, NULL);
   return cfd;
}
