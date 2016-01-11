#include "e.h"
#include "e_mod_main.h"
#include "e_int_config_randr2.h"

/* local structures */
struct _E_Config_Dialog_Data
{
   char *params;
   E_Config_Dialog *cfd;
   Eina_List *screen_items;
   Eina_List *screen_items2;
   Eina_List *screens;
   Eina_List *freelist;
   Evas_Object *name_obj;
   Evas_Object *screen_obj;
   Evas_Object *lid_obj;
   Evas_Object *backlight_obj;
   Evas_Object *size_obj;
   Evas_Object *modes_obj;
   Evas_Object *rotations_obj;
   Evas_Object *enabled_obj;
   Evas_Object *priority_obj;
   Evas_Object *rel_mode_obj;
   Evas_Object *rel_to_obj;
   Evas_Object *rel_align_obj;
   int restore;
   int hotplug;
   int acpi;
   int screen;
};

typedef struct
{
   E_Config_Dialog_Data *cfdata;
   E_Randr2_Mode mode;
} Mode_CBData;

typedef struct
{
   E_Config_Dialog_Data *cfdata;
   int rot;
} Rot_CBData;

/* local function prototypes */
static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int          _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _basic_check(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

/* public functions */
E_Config_Dialog *
e_int_config_randr2(Evas_Object *parent EINA_UNUSED, const char *params)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "screen/screen_setup")) return NULL;
   if (!(v = E_NEW(E_Config_Dialog_View, 1))) return NULL;

   /* set dialog view functions & properties */
   v->create_cfdata        = _create_data;
   v->free_cfdata          = _free_data;
   v->basic.create_widgets = _basic_create;
   v->basic.apply_cfdata   = _basic_apply;
   v->basic.check_changed  = _basic_check;
   v->override_auto_apply  = EINA_TRUE;

   /* create new dialog */
   cfd = e_config_dialog_new(NULL, _("Screen Setup"),
                             "E", "screen/screen_setup",
                             "preferences-system-screen-resolution",
                             0, v, (void *)params);
   return cfd;
}

/* local functions */
static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;

   if (!(cfdata = E_NEW(E_Config_Dialog_Data, 1))) return NULL;
   if (cfd->data) cfdata->params = strdup(cfd->data);
   cfdata->restore = e_randr2_cfg->restore;
   cfdata->hotplug = !e_randr2_cfg->ignore_hotplug_events;
   cfdata->acpi = !e_randr2_cfg->ignore_acpi_events;
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   void *dt;
   E_Config_Randr2_Screen *cs;

   EINA_LIST_FREE(cfdata->screens, cs)
     {
        eina_stringshare_del(cs->id);
        eina_stringshare_del(cs->rel_to);
        free(cs);
     }
   free(cfdata->params);
   eina_list_free(cfdata->screen_items);
   eina_list_free(cfdata->screen_items2);
   EINA_LIST_FREE(cfdata->freelist, dt) free(dt);
   E_FREE(cfdata);
}

static void
_cb_restore_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   cfdata->restore = elm_check_state_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_hotplug_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   cfdata->hotplug = elm_check_state_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_acpi_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   cfdata->acpi = elm_check_state_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static E_Config_Randr2_Screen *
_config_screen_find(E_Config_Dialog_Data *cfdata)
{
   return eina_list_nth(cfdata->screens, cfdata->screen);
}

static E_Config_Randr2_Screen *
_config_screen_n_find(E_Config_Dialog_Data *cfdata, int n)
{
   return eina_list_nth(cfdata->screens, n);
}

static E_Randr2_Screen *
_screen_config_find(E_Config_Randr2_Screen *cs)
{
   Eina_List *l;
   E_Randr2_Screen *s;

   if (!cs->id) return NULL;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->id) continue;
        if (!strcmp(cs->id, s->id)) return s;
     }
   return NULL;
}

static E_Randr2_Screen *
_screen_config_id_find(const char *id)
{
   Eina_List *l;
   E_Randr2_Screen *s;

   if (!id) return NULL;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->id) continue;
        if (!strcmp(s->id, id)) return s;
     }
   return NULL;
}

static E_Config_Randr2_Screen *
_screen_config_randr_id_find(const char *id)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs;

   if (!id) return NULL;
   EINA_LIST_FOREACH(e_randr2_cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        if (!strcmp(cs->id, id)) return cs;
     }
   return NULL;
}

static void
_cb_mode_set(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Mode_CBData *dat = data;
   E_Config_Dialog_Data *cfdata = dat->cfdata;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->mode_w = dat->mode.w;
   cs->mode_h = dat->mode.h;
   cs->mode_refresh = dat->mode.refresh;
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rot_set(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Rot_CBData *dat = data;
   E_Config_Dialog_Data *cfdata = dat->cfdata;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rotation = dat->rot;
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_basic_screen_info_fill(E_Config_Dialog_Data *cfdata, E_Config_Randr2_Screen *cs, E_Randr2_Screen *s)
{
   char buf[100];
   Eina_List *l;
   E_Randr2_Mode *m;
   Elm_Object_Item *it, *it_sel;
   void *dt;

   if (!s) return;
   // fill all the screen status info
   elm_object_text_set(cfdata->name_obj, s->info.name);
   elm_object_text_set(cfdata->screen_obj, s->info.screen);
   elm_check_state_set(cfdata->lid_obj, s->info.is_lid);
   elm_check_state_set(cfdata->backlight_obj, s->info.backlight);
   snprintf(buf, sizeof(buf), "%imm x %imm", s->info.size.w, s->info.size.h);
   elm_object_text_set(cfdata->size_obj, buf);
   // XXX: connector

   EINA_LIST_FREE(cfdata->freelist, dt) free(dt);

   elm_list_clear(cfdata->modes_obj);
   it_sel = NULL;
   EINA_LIST_FOREACH(s->info.modes, l, m)
     {
        Mode_CBData *mode_cbdata = calloc(1, sizeof(Mode_CBData));

        if (mode_cbdata)
          {
             mode_cbdata->cfdata = cfdata;
             mode_cbdata->mode = *m;
             snprintf(buf, sizeof(buf), "%ix%i @ %1.2fHz", m->w, m->h, m->refresh);
             it = elm_list_item_append(cfdata->modes_obj, buf, NULL, NULL, _cb_mode_set, mode_cbdata);
             cfdata->freelist = eina_list_append(cfdata->freelist, mode_cbdata);
             /* printf("mode add %p %p %p\n", mode_cbdata, cfdata->modes_obj, it); */
             if ((cs->mode_w == m->w) && (cs->mode_h == m->h) &&
                 (cs->mode_refresh == m->refresh))
               it_sel = it;
          }
     }
   if (it_sel) elm_list_item_selected_set(it_sel, EINA_TRUE);
   elm_list_go(cfdata->modes_obj);

   elm_list_clear(cfdata->rotations_obj);
   it_sel = NULL;
   if (s->info.can_rot_0)
     {
        Rot_CBData *rot_cbdata = calloc(1, sizeof(Rot_CBData));
        if (rot_cbdata)
          {
             rot_cbdata->cfdata = cfdata;
             rot_cbdata->rot = 0;
             it = elm_list_item_append(cfdata->rotations_obj, "0", NULL, NULL, _cb_rot_set, rot_cbdata);
             cfdata->freelist = eina_list_append(cfdata->freelist, rot_cbdata);
             if (cs->rotation == 0) it_sel = it;
          }
     }
   if (s->info.can_rot_90)
     {
        Rot_CBData *rot_cbdata = calloc(1, sizeof(Rot_CBData));
        if (rot_cbdata)
          {
             rot_cbdata->cfdata = cfdata;
             rot_cbdata->rot = 90;
             it = elm_list_item_append(cfdata->rotations_obj, "90", NULL, NULL, _cb_rot_set, rot_cbdata);
             cfdata->freelist = eina_list_append(cfdata->freelist, rot_cbdata);
             if (cs->rotation == 90) it_sel = it;
          }
     }
   if (s->info.can_rot_180)
     {
        Rot_CBData *rot_cbdata = calloc(1, sizeof(Rot_CBData));
        if (rot_cbdata)
          {
             rot_cbdata->cfdata = cfdata;
             rot_cbdata->rot = 180;
             it = elm_list_item_append(cfdata->rotations_obj, "180", NULL, NULL, _cb_rot_set, rot_cbdata);
             cfdata->freelist = eina_list_append(cfdata->freelist, rot_cbdata);
             if (cs->rotation == 180) it_sel = it;
          }
     }
   if (s->info.can_rot_270)
     {
        Rot_CBData *rot_cbdata = calloc(1, sizeof(Rot_CBData));
        if (rot_cbdata)
          {
             rot_cbdata->cfdata = cfdata;
             rot_cbdata->rot = 270;
             it = elm_list_item_append(cfdata->rotations_obj, "270", NULL, NULL, _cb_rot_set, rot_cbdata);
             cfdata->freelist = eina_list_append(cfdata->freelist, rot_cbdata);
             if (cs->rotation == 270) it_sel = it;
          }
     }
   if (it_sel) elm_list_item_selected_set(it_sel, EINA_TRUE);
   elm_list_go(cfdata->rotations_obj);

   elm_check_state_set(cfdata->enabled_obj, cs->enabled);

   elm_slider_value_set(cfdata->priority_obj, cs->priority);

   if (cs->rel_mode == E_RANDR2_RELATIVE_NONE)
     elm_object_text_set(cfdata->rel_mode_obj, _("None"));
   else if (cs->rel_mode == E_RANDR2_RELATIVE_CLONE)
     elm_object_text_set(cfdata->rel_mode_obj, _("Clone"));
   else if (cs->rel_mode == E_RANDR2_RELATIVE_TO_LEFT)
     elm_object_text_set(cfdata->rel_mode_obj, _("Left of"));
   else if (cs->rel_mode == E_RANDR2_RELATIVE_TO_RIGHT)
     elm_object_text_set(cfdata->rel_mode_obj, _("Right of"));
   else if (cs->rel_mode == E_RANDR2_RELATIVE_TO_ABOVE)
     elm_object_text_set(cfdata->rel_mode_obj, _("Above"));
   else if (cs->rel_mode == E_RANDR2_RELATIVE_TO_BELOW)
     elm_object_text_set(cfdata->rel_mode_obj, _("Below"));
   else
     elm_object_text_set(cfdata->rel_mode_obj, "???");

   elm_slider_value_set(cfdata->rel_align_obj, cs->rel_align);

   if (!cs->rel_to)
     elm_object_text_set(cfdata->rel_to_obj, "");
   else
     {
        char *str = strdup(cs->rel_to);
        if (str)
          {
             char *p = strchr(str, '/');
             if (p)
               {
                  *p = 0;
                  elm_object_text_set(cfdata->rel_to_obj, str);
               }
             free(str);
          }
     }
}

static void
_cb_screen_select(void *data, Evas_Object *obj, void *event)
{
   E_Config_Dialog_Data *cfdata = data;
   Elm_Object_Item *it;
   Eina_List *l;
   int i = 0;

   EINA_LIST_FOREACH(cfdata->screen_items, l, it)
     {
        if (it == event)
          {
             E_Config_Randr2_Screen *cs;
             cfdata->screen = i;
             cs = _config_screen_find(cfdata);
             if (cs)
               {
                  E_Randr2_Screen *s = _screen_config_find(cs);
                  if (s)
                    {
                       elm_object_text_set(obj, s->info.name);
                       _basic_screen_info_fill(cfdata, cs, s);
                    }
               }
             e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
             return;
          }
        i++;
     }
}

static void
_cb_rel_to_set(void *data, Evas_Object *obj, void *event)
{
   E_Config_Dialog_Data *cfdata = data;
   Elm_Object_Item *it;
   Eina_List *l;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   int i = 0;

   EINA_LIST_FOREACH(cfdata->screen_items2, l, it)
     {
        if (it == event)
          {
             E_Config_Randr2_Screen *cs2 = _config_screen_n_find(cfdata, i);
             if (cs2)
               {
                  printf("find cs = %p\n", cs2);
                  printf("cs id = %s\n", cs2->id);
               }
             if (cs2 == cs) return;
             if (cs2)
               {
                  E_Randr2_Screen *s = _screen_config_id_find(cs2->id);
                  if (s)
                    {
                       printf("SEt to %p [%s]\n", cs, cs->id);
                       printf("find s = %p\n", s);
                       printf("s id = %s\n", s->id);
                       elm_object_text_set(obj, s->info.name);
                       eina_stringshare_del(cs->rel_to);
                       cs->rel_to = eina_stringshare_add(s->id);
                    }
               }
             e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
             return;
          }
        i++;
     }
}

static void
_cb_rel_align_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_align = elm_slider_value_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_none(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_NONE;
   elm_object_text_set(obj, _("None"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_clone(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_CLONE;
   elm_object_text_set(obj, _("Clone"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_left_of(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_TO_LEFT;
   elm_object_text_set(obj, _("Left of"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_right_of(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_TO_RIGHT;
   elm_object_text_set(obj, _("Right of"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_above(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_TO_ABOVE;
   elm_object_text_set(obj, _("Above"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_rel_mode_below(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->rel_mode = E_RANDR2_RELATIVE_TO_BELOW;
   elm_object_text_set(obj, _("Below"));
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_priority_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->priority = elm_slider_value_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_enabled_changed(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   E_Config_Randr2_Screen *cs = _config_screen_find(cfdata);
   if (!cs) return;
   cs->enabled = elm_check_state_get(obj);
   printf("RR: enabled = %i\n", cs->enabled);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *win = cfd->dia->win;
   Evas_Object *o, *bx, *tb, *bx2;
   Eina_List *l;
   E_Randr2_Screen *s, *first = NULL;
   E_Config_Randr2_Screen *first_cfg = NULL;
   int i;

   e_dialog_resizable_set(cfd->dia, 1);

   o = elm_box_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
   bx = o;

   o = elm_table_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   tb = o;

   o = elm_hoversel_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Outputs"));
   cfdata->screens = NULL;
   cfdata->screen_items = NULL;
   i = 0;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        Elm_Object_Item *it = NULL;

        if (s->info.connected)
          {
             E_Config_Randr2_Screen *cs;

             cs = calloc(1, sizeof(E_Config_Randr2_Screen));
             if (cs)
               {
                  if (s->id)
                    cs->id = eina_stringshare_add(s->id);
                  if (s->config.relative.to)
                    cs->rel_to = eina_stringshare_add(s->config.relative.to);
                  cs->rel_align = s->config.relative.align;
                  cs->mode_refresh = s->config.mode.refresh;
                  cs->mode_w = s->config.mode.w;
                  cs->mode_h = s->config.mode.h;
                  cs->rotation = s->config.rotation;
                  cs->priority = s->config.priority;
                  cs->rel_mode = s->config.relative.mode;
                  cs->enabled = s->config.enabled;
                  cfdata->screens = eina_list_append(cfdata->screens, cs);
                  it = elm_hoversel_item_add(o, s->info.name,
                                             NULL, ELM_ICON_NONE,
                                             _cb_screen_select, cfdata);
                  if (cfdata->params)
                    {
                       if ((s->info.name) &&
                           (!strcmp(s->info.name, cfdata->params)) &&
                           (!first))
                         {
                            first = s;
                            first_cfg = cs;
                            cfdata->screen = i;
                            elm_object_text_set(o, s->info.name);
                          }
                    }
                  else
                    {
                       if (!first)
                         {
                            first = s;
                            first_cfg = cs;
                            cfdata->screen = i;
                            elm_object_text_set(o, s->info.name);
                         }
                    }
                  cfdata->screen_items = eina_list_append(cfdata->screen_items, it);
                  i++;
               }
          }
     }
   elm_table_pack(tb, o, 0, 0, 1, 1);
   evas_object_show(o);

   o = elm_entry_add(win);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_single_line_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_table_pack(tb, o, 0, 1, 1, 1);
   evas_object_show(o);
   cfdata->name_obj = o;

   o = elm_entry_add(win);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_single_line_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_table_pack(tb, o, 0, 2, 1, 1);
   evas_object_show(o);
   cfdata->screen_obj = o;

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Laptop lid"));
   elm_table_pack(tb, o, 0, 3, 1, 1);
   evas_object_show(o);
   cfdata->lid_obj = o;

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Backlight"));
   elm_table_pack(tb, o, 0, 4, 1, 1);
   evas_object_show(o);
   cfdata->backlight_obj = o;

   o = elm_label_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_table_pack(tb, o, 0, 5, 1, 1);
   evas_object_show(o);
   cfdata->size_obj = o;

   o = elm_list_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 1, 0, 1, 10);
   evas_object_show(o);
   cfdata->modes_obj = o;

   o = elm_list_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 2, 0, 1, 4);
   evas_object_show(o);
   cfdata->rotations_obj = o;

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("On"));
   elm_table_pack(tb, o, 2, 4, 1, 1);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_enabled_changed, cfdata);
   cfdata->enabled_obj = o;

   o = elm_slider_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Priority"));
   elm_slider_unit_format_set(o, "%3.0f");
   elm_slider_span_size_set(o, 100);
   elm_slider_min_max_set(o, 0, 100);
   elm_table_pack(tb, o, 2, 5, 1, 1);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_priority_changed, cfdata);
   cfdata->priority_obj = o;

   o = elm_hoversel_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Relative"));
   elm_hoversel_item_add(o, _("None"), NULL, ELM_ICON_NONE, _cb_rel_mode_none, cfdata);
   elm_hoversel_item_add(o, _("Clone"), NULL, ELM_ICON_NONE, _cb_rel_mode_clone, cfdata);
   elm_hoversel_item_add(o, _("Left of"), NULL, ELM_ICON_NONE, _cb_rel_mode_left_of, cfdata);
   elm_hoversel_item_add(o, _("Right of"), NULL, ELM_ICON_NONE, _cb_rel_mode_right_of, cfdata);
   elm_hoversel_item_add(o, _("Above"), NULL, ELM_ICON_NONE, _cb_rel_mode_above, cfdata);
   elm_hoversel_item_add(o, _("Below"), NULL, ELM_ICON_NONE, _cb_rel_mode_below, cfdata);
   elm_table_pack(tb, o, 2, 6, 1, 1);
   evas_object_show(o);
   cfdata->rel_mode_obj = o;

   o = elm_hoversel_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("To"));
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        Elm_Object_Item *it = NULL;

        if (s->info.connected)
          {
             it = elm_hoversel_item_add(o, s->info.name,
                                        NULL, ELM_ICON_NONE,
                                        _cb_rel_to_set, cfdata);
             cfdata->screen_items2 = eina_list_append(cfdata->screen_items2, it);
          }
     }
   elm_table_pack(tb, o, 2, 7, 1, 1);
   evas_object_show(o);
   cfdata->rel_to_obj = o;

   o = elm_slider_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Align"));
   elm_slider_unit_format_set(o, "%1.1f");
   elm_slider_span_size_set(o, 100);
   elm_slider_min_max_set(o, 0.0, 1.0);
   elm_table_pack(tb, o, 2, 8, 1, 1);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_rel_align_changed, cfdata);
   cfdata->rel_align_obj = o;

   _basic_screen_info_fill(cfdata, first_cfg, first);

   o = elm_box_add(win);
   elm_box_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, EVAS_HINT_FILL);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   bx2 = o;

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Restore setup on start"));
   elm_check_state_set(o, cfdata->restore);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_restore_changed, cfdata);

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Monitor hotplug"));
   elm_check_state_set(o, cfdata->hotplug);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_hotplug_changed, cfdata);

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Lid Events"));
   elm_check_state_set(o, cfdata->acpi);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_acpi_changed, cfdata);

   evas_smart_objects_calculate(evas_object_evas_get(win));

   e_util_win_auto_resize_fill(win);
   elm_win_center(win, 1, 1);
   cfdata->cfd = cfd;
   return bx;
}

static int
_basic_apply(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs, *cs2;

   e_randr2_cfg->restore = cfdata->restore;
   e_randr2_cfg->ignore_hotplug_events = !cfdata->hotplug;
   e_randr2_cfg->ignore_acpi_events = !cfdata->acpi;

   printf("APPLY....................\n");
   EINA_LIST_FOREACH(cfdata->screens, l, cs2)
     {
        if (!cs2->id) continue;
        printf("APPLY .... %p\n", cs2);
        cs = _screen_config_randr_id_find(cs2->id);
        if (!cs)
          {
             cs = calloc(1, sizeof(E_Config_Randr2_Screen));
             cs->id = eina_stringshare_add(cs2->id);
             e_randr2_cfg->screens = eina_list_append(e_randr2_cfg->screens, cs);
          }
        if (cs->rel_to) eina_stringshare_del(cs->rel_to);
        cs->rel_to = NULL;
        printf("APPLY %s .... rel to %s\n", cs->id, cs2->rel_to);
        if (cs2->rel_to) cs->rel_to = eina_stringshare_add(cs2->rel_to);
        cs->rel_align = cs2->rel_align;
        cs->mode_refresh = cs2->mode_refresh;
        cs->mode_w = cs2->mode_w;
        cs->mode_h = cs2->mode_h;
        cs->mode_refresh = cs2->mode_refresh;
        cs->rotation = cs2->rotation;
        cs->priority = cs2->priority;
        cs->rel_mode = cs2->rel_mode;
        printf("APPLY %s .... rel mode %i\n", cs->id, cs->rel_mode);
        cs->enabled = cs2->enabled;
     }
   e_randr2_config_save();
   e_randr2_config_apply();
   return 1;
}

static int
_basic_check(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata EINA_UNUSED)
{
   return 1;
}
