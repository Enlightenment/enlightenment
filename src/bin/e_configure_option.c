#include "e.h"
#include "e_fm_device.h"

EAPI int E_EVENT_CONFIGURE_OPTION_CHANGED = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_ADD = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_DEL = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_CATEGORY_ADD = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_CATEGORY_DEL = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_TAG_ADD = -1;
EAPI int E_EVENT_CONFIGURE_OPTION_TAG_DEL = -1;

static Eio_File *theme_ls[2] = {NULL, NULL};
static Eio_Monitor *theme_mon[2] = {NULL, NULL};
static Eio_File *gtk_theme_ls = NULL;
static Eina_List *gtk_theme_dirs = NULL; //Eio_File
static Eio_Monitor *gtk_theme_mon = NULL;
static Eina_List *gtk_theme_mons = NULL; //Eio_Monitor
static Eio_File *bg_ls[2] = {NULL, NULL};
static Eio_Monitor *bg_mon[2] = {NULL, NULL};
static Eina_Hash *tags_name_hash = NULL; /* (const char*)tag:(Eina_Stringshare*)tag */
static Eina_Hash *tags_hash = NULL; /* tag:item */
static Eina_Hash *tags_alias_hash = NULL; /* alias:tag */
static Eina_Hash *tags_tag_alias_hash = NULL; /* tag:Eina_List(aliases) */
static Eina_Hash *tags_alias_name_hash = NULL; /* (const char*)alias:(Eina_Stringshare*)alias */
static Eina_List *tags_alias_list = NULL; /* alias:tag */
static Eina_List *tags_list = NULL; /* Eina_Stringshare */
static Eina_List *opts_changed_list = NULL; //co->changed
static Eina_List *handlers = NULL;
static Eina_List *bgs = NULL;
static Eina_List *sbgs = NULL;
static Eina_List *themes = NULL;
static Eina_List *sthemes = NULL;
static Eina_List *gtk_themes = NULL;
static Eina_Bool event_block = EINA_TRUE;
static Eina_List *categories = NULL;
static Eina_Hash *category_hash = NULL;
static Eina_Hash *category_icon_hash = NULL;
static Eina_Hash *domain_hash = NULL;
static const char *domain_current = NULL;

static void
_e_configure_option_event_str_end(void *d EINA_UNUSED, E_Event_Configure_Option_Category_Add *ev)
{
   eina_stringshare_del(ev->category);
   free(ev);
}

static void
_e_configure_option_event_category_add_del(Eina_Stringshare *cat, Eina_Bool del)
{
   E_Event_Configure_Option_Category_Add *ev;

   if (event_block)
     {
        if (del) eina_stringshare_del(cat);
        return;
     }
   ev = E_NEW(E_Event_Configure_Option_Category_Add, 1);
   ev->category = cat;
   if (del)
     ecore_event_add(E_EVENT_CONFIGURE_OPTION_CATEGORY_DEL, ev, (Ecore_End_Cb)_e_configure_option_event_str_end, NULL);
   else
     ecore_event_add(E_EVENT_CONFIGURE_OPTION_CATEGORY_ADD, ev, NULL, NULL);
}

static void
_e_configure_option_event_tag_add_del(Eina_Stringshare *tag, Eina_Bool del)
{
   E_Event_Configure_Option_Tag_Add *ev;

   if (event_block) return;
   ev = E_NEW(E_Event_Configure_Option_Tag_Add, 1);
   ev->tag = tag;
   if (del)
     ecore_event_add(E_EVENT_CONFIGURE_OPTION_TAG_DEL, ev, (Ecore_End_Cb)_e_configure_option_event_str_end, NULL);
   else
     ecore_event_add(E_EVENT_CONFIGURE_OPTION_TAG_ADD, ev, NULL, NULL);
}

static void
_e_configure_option_tag_alias_list_free(Eina_List *list)
{
   Eina_Stringshare *alias;

   EINA_LIST_FREE(list, alias)
     {
        eina_hash_del_by_key(tags_alias_name_hash, alias);
        tags_alias_list = eina_list_remove(tags_alias_list, alias);
        eina_hash_del_by_key(tags_alias_hash, alias);
     }
}

static void
_e_configure_option_tag_remove(E_Configure_Option *co, Eina_Stringshare *tag)
{
   Eina_List *items;

   items = eina_hash_find(tags_hash, tag);
   if (!items) return;
   items = eina_list_remove(items, co);
   co->tags = eina_list_remove(co->tags, tag);
   if (!items)
     {
        /* this seems dumb... */
        tags_list = eina_list_remove(tags_list, tag);
        eina_hash_del_by_key(tags_name_hash, tag);
        eina_hash_del_by_key(tags_tag_alias_hash, tag);
        _e_configure_option_event_tag_add_del(tag, EINA_TRUE);
     }
   eina_hash_set(tags_hash, tag, items);
}

static void
_e_configure_option_free(E_Configure_Option *co)
{
   if (co->changed)
     {
        eina_value_flush(&co->val);
        if (opts_changed_list)
          opts_changed_list = eina_list_remove(opts_changed_list, co);
     }
   if (co->data)
     eina_stringshare_del(eina_hash_find(co->data, "icon"));
   eina_hash_free(co->data);
   E_FREE_LIST(co->tags, eina_stringshare_del);
   eina_stringshare_del(co->name);
   eina_stringshare_del(co->desc);
   eina_stringshare_del(co->info);
   eina_stringshare_del(co->changed_action);
   free(co);
}

static void
_e_configure_option_list_free(Eina_Inlist *l)
{
   while (l)
     {
        E_Configure_Option *co = (E_Configure_Option*)l;

        l = eina_inlist_remove(l, l);
        e_configure_option_del(co);
     }
}

static void
_e_configure_option_event_del_end(void *d EINA_UNUSED, E_Event_Configure_Option_Del *ev)
{
   Eina_List *l, *ll;
   Eina_Stringshare *tag;

   EINA_LIST_FOREACH_SAFE(ev->co->tags, l, ll, tag)
     _e_configure_option_tag_remove(ev->co, tag);
   _e_configure_option_free(ev->co);
   free(ev);
}

static void
_e_configure_option_event_del(E_Configure_Option *co)
{
   E_Event_Configure_Option_Del *ev;

   if (event_block) return;
   ev = E_NEW(E_Event_Configure_Option_Del, 1);
   ev->co = co;
   ecore_event_add(E_EVENT_CONFIGURE_OPTION_CHANGED, ev, (Ecore_End_Cb)_e_configure_option_event_del_end, NULL);
}

static void
_e_configure_option_event_changed(E_Configure_Option *co)
{
   E_Event_Configure_Option_Changed *ev;

   if (event_block) return;
   ev = E_NEW(E_Event_Configure_Option_Changed, 1);
   ev->co = co;
   ecore_event_add(E_EVENT_CONFIGURE_OPTION_CHANGED, ev, NULL, NULL);
}

static void
_e_configure_option_tag_append(E_Configure_Option *co, const char *tag)
{
   Eina_List *items;

   items = eina_hash_find(tags_hash, tag);
   if (!items)
     {
        Eina_Stringshare *t;

        t = eina_stringshare_add(tag);
        eina_hash_add(tags_name_hash, tag, t);
        tags_list = eina_list_sorted_insert(tags_list, (Eina_Compare_Cb)strcmp, t);
        _e_configure_option_event_tag_add_del(t, EINA_FALSE);
     }
   eina_hash_set(tags_hash, tag, eina_list_append(items, co));
   co->tags = eina_list_append(co->tags, eina_stringshare_add(tag));
}

static Eina_List *
_e_configure_option_ctx_list_prune(Eina_Stringshare *tag, Eina_List *opts, Eina_List *tags)
{
   Eina_List *l, *ll, *lll, *topts;
   E_Configure_Option *co;
   Eina_Stringshare *t;

   EINA_LIST_FOREACH_SAFE(opts, l, ll, co)
     {
        Eina_Bool found = EINA_FALSE;

        EINA_LIST_FOREACH(tags, lll, t)
          {
             if (t == tag) continue;
             topts = eina_hash_find(tags_hash, t);
             if (!topts) continue;
             if (eina_list_data_find(topts, co))
               found = EINA_TRUE;
          }
        if (!found)
          opts = eina_list_remove_list(opts, l);
     }

   return opts;
}

static void
_e_configure_option_value_reset(E_Configure_Option *co)
{
   if (co->type == E_CONFIGURE_OPTION_TYPE_CUSTOM) return;
   if (co->changed) eina_value_flush(&co->val);
   switch (co->type)
     {
      case E_CONFIGURE_OPTION_TYPE_BOOL:
        eina_value_setup(&co->val, EINA_VALUE_TYPE_UCHAR);
        break;

      case E_CONFIGURE_OPTION_TYPE_INT:
      case E_CONFIGURE_OPTION_TYPE_ENUM:
        eina_value_setup(&co->val, EINA_VALUE_TYPE_INT);
        break;

      case E_CONFIGURE_OPTION_TYPE_UINT:
        eina_value_setup(&co->val, EINA_VALUE_TYPE_UINT);
        break;

      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT:
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT:
      case E_CONFIGURE_OPTION_TYPE_DOUBLE:
        eina_value_setup(&co->val, EINA_VALUE_TYPE_DOUBLE);
        break;

      case E_CONFIGURE_OPTION_TYPE_STR:
        eina_value_setup(&co->val, EINA_VALUE_TYPE_STRINGSHARE);
        break;

      case E_CONFIGURE_OPTION_TYPE_CUSTOM:
        break;
     }
}

static Eina_Bool
_e_configure_option_apply(E_Configure_Option *co, Eina_List **events, Eina_List **funcs, Eina_List **acts)
{
   E_Action *act;
   void (*none)(void) = NULL;

   if (!co->changed) return EINA_FALSE;
   if ((co->type == E_CONFIGURE_OPTION_TYPE_DOUBLE_INT) || (co->type == E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT))
     {
        double dbl;
        long x;

        eina_value_get(&co->val, &dbl);
        x = lround(dbl);
        if (co->type == E_CONFIGURE_OPTION_TYPE_DOUBLE_INT)
          *(int *)co->valptr = x;
        else
          *(unsigned int *)co->valptr = MAX(x, 0);
     }
   else if (co->type == E_CONFIGURE_OPTION_TYPE_STR)
     {
        eina_stringshare_replace(co->valptr, NULL);
        eina_value_get(&co->val, co->valptr);
        eina_stringshare_ref(*(Eina_Stringshare**)co->valptr);
     }
   else if (co->type != E_CONFIGURE_OPTION_TYPE_CUSTOM)
     eina_value_get(&co->val, co->valptr);
   _e_configure_option_value_reset(co);

   if (co->changed_action)
     {
        act = e_action_find(co->changed_action);
        while (act && act->func.go)
          {
             if (!acts)
               {
                  act->func.go((E_Object *)e_util_container_current_get(), NULL);
                  break;
               }
             if (*acts && eina_list_data_find(*acts, act)) break;
             *acts = eina_list_append(*acts, act);
             break;
          }
     }
   while (co->event_type)
     {
        if (!events)
          {
             ecore_event_add(co->event_type, NULL, NULL, NULL);
             break;
          }
        if (*events && eina_list_data_find(*events, (intptr_t *)(long)co->event_type)) break;
        *events = eina_list_append(*events, (intptr_t *)(long)co->event_type);
        break;
     }
   if (co->type == E_CONFIGURE_OPTION_TYPE_BOOL)
     none = co->funcs[*(Eina_Bool *)co->valptr].none;
   else if (co->funcs[0].none)
     none = co->funcs[0].none;
   while (none)
     {
        if (!funcs)
          {
             none();
             break;
          }
        if (*funcs && eina_list_data_find(*funcs, none)) break;
        *funcs = eina_list_append(*funcs, none);
        break;
     }
   if (co->funcs[0].one)
     {
        switch (co->type)
          {
           case E_CONFIGURE_OPTION_TYPE_BOOL:
             co->funcs[0].one(*(Eina_Bool *)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
             co->funcs[0].one(*(unsigned char *)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT: //lround(double)
           case E_CONFIGURE_OPTION_TYPE_INT:
           case E_CONFIGURE_OPTION_TYPE_ENUM:
             co->funcs[0].one(*(int *)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT: //lround(double)
           case E_CONFIGURE_OPTION_TYPE_UINT:
             co->funcs[0].one(*(unsigned int *)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE:
             co->funcs[0].one(*(double *)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_STR:
             co->funcs[0].one(*(Eina_Stringshare **)co->valptr);
             break;

           case E_CONFIGURE_OPTION_TYPE_CUSTOM:
             break;
          }
     }
   co->changed = EINA_FALSE;
   _e_configure_option_event_changed(co);

   return co->requires_restart;
}

/********************************************************
 * CHANGE CALLBACKS
 */

static void
_e_configure_screensaver_changed(void)
{
   if ((e_config->backlight.idle_dim) &&
       (e_config->backlight.timer > (e_config->screensaver_timeout)))
     {
        e_config->screensaver_timeout = e_config->backlight.timer;
        e_config->dpms_standby_timeout = e_config->screensaver_timeout;
        e_config->dpms_suspend_timeout = e_config->screensaver_timeout;
        e_config->dpms_off_timeout = e_config->screensaver_timeout;
     }
   e_screensaver_update();
   e_dpms_update();
}

static void
_e_configure_dpms_changed(void)
{
   e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_NORMAL);
   e_backlight_level_set(NULL, e_config->backlight.normal, -1.0);

   _e_configure_screensaver_changed();
   e_backlight_update();
}

static void
_e_configure_pointer_changed(void)
{
   const Eina_List *l;
   E_Manager *man;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        if (man->pointer && !e_config->show_cursor)
          {
             e_pointer_hide(man->pointer);
             continue;
          }
        if (man->pointer) e_object_del(E_OBJECT(man->pointer));
        man->pointer = e_pointer_window_new(man->root, 1);
     }
}

static void
_e_configure_framerate_changed(void)
{
   edje_frametime_set(1.0 / e_config->framerate);
   e_canvas_recache();
}

static void
_e_configure_zone_desks_count_changed(void)
{
   const Eina_List *l, *ll, *lll;
   E_Manager *man;
   E_Container *con;
   E_Zone *zone;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     EINA_LIST_FOREACH(man->containers, ll, con)
       EINA_LIST_FOREACH(con->zones, lll, zone)
         e_zone_desk_count_set(zone, e_config->zone_desks_x_count, e_config->zone_desks_x_count);
}

/********************************************************
 * THUMB CALLBACKS
 */

static Eina_Bool
_e_configure_transition_timer_cb(void *obj)
{
   Eina_Stringshare *file, *key;
   Evas_Object *o, *oo;

   evas_object_data_del(obj, "conf_timer");
   o = edje_object_part_swallow_get(obj, "e.swallow.bg.new");
   edje_object_part_unswallow(obj, o);
   oo = edje_object_part_swallow_get(obj, "e.swallow.bg.old");
   edje_object_part_unswallow(obj, oo);
   edje_object_file_get(obj, &file, &key);
   eina_stringshare_ref(file);
   eina_stringshare_ref(key);
   edje_object_file_set(obj, NULL, NULL);
   edje_object_file_set(obj, file, key);
   eina_stringshare_del(file);
   eina_stringshare_del(key);
   edje_object_part_swallow(obj, "e.swallow.bg.new", o);
   edje_object_part_swallow(obj, "e.swallow.bg.old", oo);
   edje_object_signal_emit(obj, "e,action,start", "e");
   return EINA_FALSE;
}

static void
_e_configure_transition_done_cb(void *d EINA_UNUSED, Evas_Object *obj, const char *s EINA_UNUSED, const char *so EINA_UNUSED)
{
   Ecore_Timer *timer;

   timer = evas_object_data_get(obj, "conf_timer");
   if (timer) ecore_timer_reset(timer);
   else evas_object_data_set(obj, "conf_timer", ecore_timer_add(1.0, _e_configure_transition_timer_cb, obj));
}

static void
_e_configure_transition_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ecore_Timer *timer;

   timer = evas_object_data_del(obj, "conf_timer");
   if (timer) ecore_timer_del(timer);
}

static Evas_Object *
_e_configure_transition_thumb_cb(E_Configure_Option_Info *oi, Evas *evas)
{
   Evas_Object *oj = NULL, *obj, *o, *oa;
   Evas *e;
   E_Zone *zone;

   zone = e_util_zone_current_get(e_manager_current_get());

   obj = e_widget_preview_add(evas, 150, (200 * zone->h) / zone->w);
   oa = e_widget_aspect_add(evas, 150, (200 * zone->h) / zone->w);
   e_widget_aspect_child_set(oa, obj);

   e = e_widget_preview_evas_get(obj);

   if (oi->thumb_key)
     {
        oj = edje_object_add(e);
        edje_object_file_set(oj, oi->thumb_file, oi->thumb_key);
        edje_object_signal_callback_add(oj, "e,state,done", "*", _e_configure_transition_done_cb, NULL);
        evas_object_event_callback_add(oj, EVAS_CALLBACK_DEL, _e_configure_transition_del_cb, NULL);
        e_widget_preview_extern_object_set(obj, oj);

        o = edje_object_add(e);
        edje_object_file_set(o, oi->thumb_file, "e/transpreview/0");
        edje_object_part_swallow(oj, "e.swallow.bg.new", o);
     }

   o = edje_object_add(e);
   edje_object_file_set(o, oi->thumb_file, "e/transpreview/1");

   if (oi->thumb_key)
     {
        edje_object_part_swallow(oj, "e.swallow.bg.old", o);
        edje_object_signal_emit(oj, "e,action,start", "e");
     }
   else
     e_widget_preview_extern_object_set(obj, o);

   return obj;
}

static Evas_Object *
_e_configure_border_style_thumb_cb(E_Configure_Option_Info *oi, Evas *evas)
{
   Evas_Object *ob, *orect, *oj = NULL;

   ob = e_livethumb_add(evas);
   e_livethumb_vsize_set(ob, 150, 150);
   if (oi->thumb_key)
     {
        oj = edje_object_add(e_livethumb_evas_get(ob));
        edje_object_file_set(oj, oi->thumb_file, oi->thumb_key);
        e_livethumb_thumb_set(ob, oj);
     }
   orect = evas_object_rectangle_add(e_livethumb_evas_get(ob));
   evas_object_color_set(orect, 0, 0, 0, 128);
   evas_object_show(orect);
   if (oi->thumb_key)
     edje_object_part_swallow(oj, "e.swallow.client", orect);
   else
     e_livethumb_thumb_set(ob, orect);
   return ob;
}

static Evas_Object *
_e_configure_icon_theme_thumb_cb(E_Configure_Option_Info *oi, Evas *evas)
{
   Evas_Object *of, *o;
   unsigned int x, y;
   const char **example_icon, *example_icons[] =
   {
      NULL,
      "folder",
      "user-home",
      "text-x-generic",
      "system-run",
      "preferences-system",
      NULL,
   };
   if (oi->thumb_file)
     {
        example_icons[0] = oi->thumb_file;
        example_icon = example_icons;
     }
   else
     example_icon = example_icons + 1;

   of = e_widget_frametable_add(evas, _("Preview"), 1);
   for (x = y = 0; (*example_icon); example_icon++)
     {
        const char *path;

        path = efreet_icon_path_find(oi->value, *example_icon, 48);
        if (!path) continue;
        o = e_icon_add(evas);
        if (e_icon_file_set(o, path))
          e_icon_fill_inside_set(o, 1);
        else
          {
             evas_object_del(o);
             continue;
          }
        e_widget_frametable_object_append_full(of, o, x++, y, 1, 1, 1, 1, 0, 0,
                                               0.5, 0.5, 48, 48, 64, 64);
        x %= 3;
        if (!x) y++;
     }
   return of;
}

static Evas_Object *
_e_configure_netwm_theme_thumb_cb(E_Configure_Option_Info *oi EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;

/* FIXME: uhhhhhhhhhhhhhhhhhhhhhh */
//o = e_widget_label_add(evas, oi->name);
   o = e_box_add(evas);
   return o;
}

/********************************************************
 * INFO CALLBACKS
 */

/* FIXME
   static Eina_List *
   _e_configure_language_info_cb(E_Configure_Option *co)
   {
   Eina_List *l, *ret = NULL;
   E_Configure_Option_Info *oi;
   char *lang;

   l = e_intl_language_list();
   EINA_LIST_FREE(l, lang)
     {

        //oi = e_configure_option_info_new(co,
        free(lang);
     }

   return ret;
   }
 */

static Eina_List *
_e_configure_border_shade_transition_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Linear",
      "Accelerate, then decelerate",
      "Accelerate",
      "Decelerate",
      "Pronounced accelerate",
      "Pronounced decelerate",
      "Pronounced accelerate, then decelerate",
      "Bounce",
      "Bounce more"
   };

   for (x = 0; x <= E_TRANSITION_BOUNCE_LOTS; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_window_placement_policy_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Try not to cover other windows",
      "Try not to cover gadgets",
      "Place at mouse pointer (automatic)",
      "Place at mouse pointer (interactive)"
   };

   for (x = 0; x <= E_WINDOW_PLACEMENT_MANUAL; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_focus_policy_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Click",
      "Pointer",
      "Sloppy"
   };

   for (x = 0; x <= E_FOCUS_SLOPPY; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_focus_setting_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Don't set focus on new windows",
      "Set focus on all new windows",
      "Set focus only on all new dialog windows",
      "Set focus only on new dialog windows if dialog's parent window has focus"
   };

   for (x = 0; x <= E_FOCUS_NEW_DIALOG_IF_OWNER_FOCUSED; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_window_activehint_policy_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Ignore application",
      "Animate application window",
      "Raise and set focus to application window"
   };

   for (x = 0; x <= E_ACTIVEHINT_POLICY_ACTIVATE; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_fullscreen_policy_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Resize window, do not resize screen",
      "Resize window and screen"
   };

   for (x = 0; x <= E_FULLSCREEN_ZOOM; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_maximize_policy_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Fullscreen", /* 1 */
      "Expand to maximum size without covering shelves", /* 2 */
      NULL,
      "Expand to maximum size ignoring shelves" /* 4 */
   };

   for (x = E_MAXIMIZE_FULLSCREEN; x <= E_MAXIMIZE_FILL; x++)
     {
        if (!name[x - 1]) continue;
        oi = e_configure_option_info_new(co, _(name[x - 1]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_font_hinting_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "No hinting",
      "Automatic hinting",
      "Bytecode hinting"
   };

   for (x = 0; x <= EVAS_FONT_HINTING_BYTECODE; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_desklock_login_box_zone_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   int num = 0;
   E_Manager *m;
   Eina_List *ml;
   const char *name[] =
   {
      "Show on all screens",
      "Show on screen of pointer",
      "Show on screen %d"
   };

   EINA_LIST_FOREACH(e_manager_list(), ml, m)
     {
        Eina_List *cl;
        E_Container *con;

        EINA_LIST_FOREACH(m->containers, cl, con)
          num += eina_list_count(con->zones);
     }

   for (x = -2; x < num; x++)
     {
        char buf[128];

        if (x < 0)
          oi = e_configure_option_info_new(co, _(name[x + 2]), (intptr_t *)(long)x);
        else
          {
             snprintf(buf, sizeof(buf), _(name[2]), x);
             oi = e_configure_option_info_new(co, buf, (intptr_t *)(long)x);
          }
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_clientlist_group_by_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "No grouping",
      "Virtual desktop",
      "Window class"
   };

   for (x = 0; x <= E_CLIENTLIST_GROUP_CLASS; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_clientlist_separate_with_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "No separation",
      "Separator bars",
      "Separate menus"
   };

   for (x = 0; x <= E_CLIENTLIST_GROUP_SEP_MENU; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_clientlist_sort_by_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "No sorting",
      "Alphabetical order",
      "Window stacking layer",
      "Recently used windows first"
   };

   for (x = 0; x <= E_CLIENTLIST_SORT_MOST_RECENT; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_clientlist_separate_iconified_apps_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Group by owner virtual desktop",
      "Group by current virtual desktop",
      "Separate group"
   };

   for (x = 0; x <= E_CLIENTLIST_GROUPICONS_SEP; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_desk_flip_animate_mode_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "No animation",
      "Pane",
      "Zoom"
   };

   for (x = 0; x <= E_DESKFLIP_ANIMATION_MODE_ZOOM; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_screen_limits_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Allow windows partly out of the screen limits",
      "Allow windows completely out of the screen limits",
      "Keep windows completely within the screen limits"
   };

   for (x = 0; x <= E_SCREEN_LIMITS_WITHIN; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static int
_e_configure_icon_theme_info_sort_cb(const void *data1, const void *data2)
{
   const Efreet_Icon_Theme *m1 = data1, *m2 = data2;

   if (!data2) return -1;
   if (!m1->name.name) return 1;
   if (!m2->name.name) return -1;

   return strcmp(m1->name.name, m2->name.name);
}

static Eina_List *
_e_configure_icon_theme_info_cb(E_Configure_Option *co)
{
   Eina_List *l, *ret = NULL;
   Efreet_Icon_Theme *theme;
   E_Configure_Option_Info *oi;

   l = efreet_icon_theme_list_get();
   if (!l) return NULL;
   l = eina_list_sort(l, 0, _e_configure_icon_theme_info_sort_cb);
   EINA_LIST_FREE(l, theme)
     {
        if (!theme->directories) continue;  //invalid
        if (!e_util_strcasecmp(theme->name.internal, "hicolor")) continue;  //default
        oi = e_configure_option_info_new(co, theme->name.name, eina_stringshare_add(theme->name.internal));
        oi->current = !e_util_strcmp(e_config->icon_theme, theme->name.internal);
        oi->thumb_file = eina_stringshare_add(theme->example_icon);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_netwm_theme_info_cb(E_Configure_Option *co)
{
   Eina_List *l, *ret = NULL;
   E_Configure_Option_Info *oi;
   Eina_Stringshare *dir;
   Eina_Bool gtk2, gtk3;

   EINA_LIST_FOREACH(gtk_themes, l, dir)
     {
        char buf[PATH_MAX];
        const char *name;

        snprintf(buf, sizeof(buf), "%s/gtk-2.0", dir);
        gtk2 = ecore_file_is_dir(buf);
        snprintf(buf, sizeof(buf), "%s/gtk-3.0", dir);
        gtk3 = ecore_file_is_dir(buf);
        if ((!gtk2) && (!gtk3)) continue;
        name = ecore_file_file_get(dir);
        snprintf(buf, sizeof(buf), "%s (%s)%s%s%s", name, gtk2 ? "gtk2" : "gtk3",
                 (gtk2 && gtk3) ? " (" : "", (gtk2 && gtk3) ? "gtk3" : "", (gtk2 && gtk3) ? ")" : "");
        oi = e_configure_option_info_new(co, buf, eina_stringshare_add(name));
        oi->current = (e_config->xsettings.net_theme_name == oi->value);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_configure_transition_info_cb(E_Configure_Option *co)
{
   Eina_Stringshare *style;
   Eina_List *l, *ret = NULL;
   Eina_Stringshare *cur, *file;
   char buf[4096];
   const char *key;
   E_Configure_Option_Info *oi;

   file = e_theme_edje_file_get("base/theme/widgets", "e/transpreview/0");
   key = "e/transitions/%s";
   cur = *(Eina_Stringshare **)co->valptr;

   oi = e_configure_option_info_new(co, _("None"), NULL);
   oi->thumb_file = eina_stringshare_ref(file);
   oi->current = (!cur);
   ret = eina_list_append(ret, oi);

   l = e_theme_transition_list();
   EINA_LIST_FREE(l, style)
     {
        oi = e_configure_option_info_new(co, style, style);
        ret = eina_list_append(ret, oi);
        oi->thumb_file = eina_stringshare_ref(file);
        snprintf(buf, sizeof(buf), key, style);
        oi->thumb_key = eina_stringshare_add(buf);
        oi->current = (style == cur);
     }
   return ret;
}

static Eina_List *
_e_configure_border_style_info_cb(E_Configure_Option *co)
{
   Eina_Stringshare *style;
   Eina_List *l, *ret = NULL;
   Eina_Stringshare *file;
   char buf[4096];
   const char *key;
   E_Configure_Option_Info *oi;

   file = e_theme_edje_file_get("base/theme/borders", "e/widgets/border/default/border");
   oi = e_configure_option_info_new(co, "borderless", eina_stringshare_add("borderless"));
   ret = eina_list_append(ret, oi);
   oi->thumb_file = eina_stringshare_ref(file);
   oi->thumb_key = eina_stringshare_add("e/widgets/border/borderless/border");
   oi->current = !e_util_strcmp(e_config->theme_default_border_style, "borderless");
   key = "e/widgets/border/%s/border";

   l = e_theme_border_list();
   EINA_LIST_FREE(l, style)
     {
        oi = e_configure_option_info_new(co, style, style);
        ret = eina_list_append(ret, oi);
        oi->thumb_file = eina_stringshare_ref(file);
        snprintf(buf, sizeof(buf), key, style);
        oi->thumb_key = eina_stringshare_add(buf);
        if (e_config->theme_default_border_style)
          oi->current = (style == e_config->theme_default_border_style);
        else if (!strcmp(style, "default"))
          oi->current = EINA_TRUE;
     }
   return ret;
}

static Eina_List *
_e_configure_init_default_theme_info_cb(E_Configure_Option *co)
{
   Eina_List *l, *ret = NULL;
   Eina_Stringshare *file;
   char buf[PATH_MAX], *p;
   E_Configure_Option_Info *oi;
   Eina_Stringshare *key;

   key = eina_stringshare_add("e/init/splash");
   EINA_LIST_FOREACH(themes, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        strncpy(buf, ecore_file_file_get(file), p - file);
        oi = e_configure_option_info_new(co, buf, eina_stringshare_add(buf));
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        ret = eina_list_append(ret, oi);
        oi->current = (oi->name == *(Eina_Stringshare **)co->valptr);
     }
   if (ret && sthemes)
     ret = eina_list_append(ret, e_configure_option_info_new(co, NULL, NULL));
   EINA_LIST_FOREACH(sthemes, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        strncpy(buf, ecore_file_file_get(file), p - file);
        oi = e_configure_option_info_new(co, buf, eina_stringshare_add(buf));
        ret = eina_list_append(ret, oi);
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        oi->current = (oi->name == *(Eina_Stringshare **)co->valptr);
     }
   eina_stringshare_del(key);
   return ret;
}

static Eina_List *
_e_configure_background_info_cb(E_Configure_Option *co)
{
   Eina_List *l, *ret = NULL;
   Eina_Stringshare *file;
   const char *f;
   Eina_Stringshare *cur;
   char buf[PATH_MAX], *p;
   E_Configure_Option_Info *oi;
   Eina_Stringshare *key;

   key = eina_stringshare_add("e/desktop/background");
   cur = *(Eina_Stringshare **)co->valptr;
   if (!cur)
     {
        E_Config_Theme *ct;
        ct = e_theme_config_get("theme");
        cur = ct->file;
     }
   EINA_LIST_FOREACH(themes, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        f = ecore_file_file_get(file);
        memcpy(buf, f, p - f);
        buf[p - f] = 0;
        oi = e_configure_option_info_new(co, buf, eina_stringshare_ref(file));
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        ret = eina_list_append(ret, oi);
        oi->current = ((file == cur) || (!e_util_strcmp(f, cur)));
     }
   EINA_LIST_FOREACH(sthemes, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        f = ecore_file_file_get(file);
        memcpy(buf, f, p - f);
        buf[p - f] = 0;
        oi = e_configure_option_info_new(co, buf, eina_stringshare_ref(file));
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        ret = eina_list_append(ret, oi);
        oi->current = ((file == cur) || (!e_util_strcmp(f, cur)));
     }
   EINA_LIST_FOREACH(bgs, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        f = ecore_file_file_get(file);
        memcpy(buf, f, p - f);
        buf[p - f] = 0;
        oi = e_configure_option_info_new(co, buf, eina_stringshare_ref(file));
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        ret = eina_list_append(ret, oi);
        oi->current = ((file == cur) || (!e_util_strcmp(f, cur)));
     }
   if (ret && sthemes)
     ret = eina_list_append(ret, e_configure_option_info_new(co, NULL, NULL));
   EINA_LIST_FOREACH(sbgs, l, file)
     {
        if (!edje_file_group_exists(file, key)) continue;
        p = strrchr(file, '.');
        f = ecore_file_file_get(file);
        memcpy(buf, f, p - f);
        buf[p - f] = 0;
        oi = e_configure_option_info_new(co, buf, eina_stringshare_ref(file));
        ret = eina_list_append(ret, oi);
        oi->thumb_file = eina_stringshare_ref(file);
        oi->thumb_key = eina_stringshare_ref(key);
        oi->current = ((file == cur) || (!e_util_strcmp(f, cur)));
     }
   eina_stringshare_del(key);
   return ret;
}

static int
_sort_cb(const char *a, const char *b)
{
   const char *f1, *f2;

   f1 = ecore_file_file_get(a);
   f2 = ecore_file_file_get(b);
   return e_util_strcasecmp(f1, f2);
}

static Eina_Bool
_gtk_theme_check(const char *file)
{
   Eina_Stringshare *dir;
   Eina_List *l;

   EINA_LIST_FOREACH(gtk_themes, l, dir)
     if (!strcasecmp(ecore_file_file_get(dir), ecore_file_file_get(file))) return EINA_FALSE;
   return EINA_TRUE;
}

static void
_init_main_cb(void *data __UNUSED__, Eio_File *handler, const char *file)
{
   if (handler == theme_ls[0])
     themes = eina_list_sorted_insert(themes, (Eina_Compare_Cb)_sort_cb, eina_stringshare_add(file));
   else if (handler == theme_ls[1])
     sthemes = eina_list_sorted_insert(sthemes, (Eina_Compare_Cb)_sort_cb, eina_stringshare_add(file));
   else if (handler == bg_ls[0])
     bgs = eina_list_sorted_insert(bgs, (Eina_Compare_Cb)_sort_cb, eina_stringshare_add(file));
   else if (handler == bg_ls[1])
     sbgs = eina_list_sorted_insert(sbgs, (Eina_Compare_Cb)_sort_cb, eina_stringshare_add(file));
   else if (_gtk_theme_check(file))
     gtk_themes = eina_list_sorted_insert(gtk_themes, (Eina_Compare_Cb)_sort_cb, eina_stringshare_add(file));
}

static void
_init_error_cb(void *data __UNUSED__, Eio_File *handler, int error __UNUSED__)
{
   if ((!theme_ls[0]) && (!theme_ls[1]) && (!bg_ls[0]) && (!bg_ls[1]) &&
       (!gtk_theme_ls) && (!gtk_theme_dirs)) goto out;
   if (theme_ls[0] == handler)
     {
        theme_ls[0] = NULL;
        E_FREE_LIST(themes, eina_stringshare_del);
     }
   else if (theme_ls[1] == handler)
     {
        theme_ls[1] = NULL;
        E_FREE_LIST(sthemes, eina_stringshare_del);
     }
   else if (bg_ls[0] == handler)
     {
        bg_ls[0] = NULL;
        E_FREE_LIST(bgs, eina_stringshare_del);
     }
   else if (bg_ls[1] == handler)
     {
        bg_ls[1] = NULL;
        E_FREE_LIST(sbgs, eina_stringshare_del);
     }
   else if (gtk_theme_ls == handler)
     gtk_theme_ls = NULL;
   else
     gtk_theme_dirs = eina_list_remove(gtk_theme_dirs, handler);
   return;
out:
   E_FREE_LIST(themes, eina_stringshare_del);
   E_FREE_LIST(sthemes, eina_stringshare_del);
   E_FREE_LIST(bgs, eina_stringshare_del);
   E_FREE_LIST(sbgs, eina_stringshare_del);
   E_FREE_LIST(gtk_themes, eina_stringshare_del);
}

static void
_init_done_cb(void *data __UNUSED__, Eio_File *handler)
{
   if ((!theme_ls[0]) && (!theme_ls[1]) && (!bg_ls[0]) && (!bg_ls[1]) &&
       (!gtk_theme_ls) && (!gtk_theme_dirs)) goto out;
   if (theme_ls[0] == handler)
     theme_ls[0] = NULL;
   else if (theme_ls[1] == handler)
     theme_ls[1] = NULL;
   else if (bg_ls[0] == handler)
     bg_ls[0] = NULL;
   else if (bg_ls[1] == handler)
     bg_ls[1] = NULL;
   else if (gtk_theme_ls == handler)
     gtk_theme_ls = NULL;
   else
     gtk_theme_dirs = eina_list_remove(gtk_theme_dirs, handler);

   return;
out:
   E_FREE_LIST(themes, eina_stringshare_del);
   E_FREE_LIST(sthemes, eina_stringshare_del);
   E_FREE_LIST(bgs, eina_stringshare_del);
   E_FREE_LIST(sbgs, eina_stringshare_del);
   E_FREE_LIST(gtk_themes, eina_stringshare_del);
}

static Eina_Bool
_gtk_filter_cb(void *data __UNUSED__, Eio_File *handler __UNUSED__, const char *file)
{
   return ecore_file_is_dir(file);
}

static Eina_Bool
_edj_filter_cb(void *data __UNUSED__, Eio_File *handler __UNUSED__, const char *file)
{
   return eina_str_has_extension(file, ".edj");
}

static Eina_Bool
_monitor_theme_rescan(void *d __UNUSED__, int type __UNUSED__, Eio_Monitor_Event *ev)
{
   char buf[PATH_MAX];

   if (theme_mon[0] == ev->monitor)
     {
        if (theme_ls[0]) return ECORE_CALLBACK_RENEW;
        E_FREE_LIST(themes, eina_stringshare_del);
        e_user_dir_concat_static(buf, "themes");
        theme_ls[0] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
     }
   else if (theme_mon[1] == ev->monitor)
     {
        if (theme_ls[1]) return ECORE_CALLBACK_RENEW;
        E_FREE_LIST(sthemes, eina_stringshare_del);
        e_prefix_data_concat_static(buf, "data/themes");
        theme_ls[1] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
     }
   else if (bg_mon[0] == ev->monitor)
     {
        if (bg_ls[0]) return ECORE_CALLBACK_RENEW;
        E_FREE_LIST(bgs, eina_stringshare_del);
        e_user_dir_concat_static(buf, "backgrounds");
        bg_ls[0] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
     }
   else if (bg_mon[1] == ev->monitor)
     {
        if (bg_ls[1]) return ECORE_CALLBACK_RENEW;
        E_FREE_LIST(sbgs, eina_stringshare_del);
        e_prefix_data_concat_static(buf, "data/backgrounds");
        bg_ls[1] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
     }
   else
     {
        const char *dir;
        Eina_List *l;

        if (gtk_theme_ls && (gtk_theme_mon == ev->monitor)) return ECORE_CALLBACK_RENEW;
        if (gtk_theme_ls) eio_file_cancel(gtk_theme_ls);
        E_FREE_LIST(gtk_themes, eina_stringshare_del);
        E_FREE_LIST(gtk_theme_dirs, eio_file_cancel);
        e_user_homedir_concat_static(buf, ".themes");
        gtk_theme_ls = eio_file_ls(buf, _gtk_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
        EINA_LIST_FOREACH(efreet_data_dirs_get(), l, dir)
          {
             Eio_File *ls;

             snprintf(buf, sizeof(buf), "%s/themes", dir);
             ls = eio_file_ls(buf, _gtk_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
             if (!ls) continue;
             gtk_theme_dirs = eina_list_append(gtk_theme_dirs, ls);
          }
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_monitor_error(void *d __UNUSED__, int type __UNUSED__, Eio_Monitor_Event *ev)
{
   if (theme_mon[0] == ev->monitor)
     theme_mon[0] = NULL;
   else if (theme_mon[1] == ev->monitor)
     theme_mon[1] = NULL;
   else if (bg_mon[0] == ev->monitor)
     bg_mon[0] = NULL;
   else if (bg_mon[1] == ev->monitor)
     bg_mon[1] = NULL;
   else if (gtk_theme_mon == ev->monitor)
     gtk_theme_mon = NULL;
   else
     gtk_theme_mons = eina_list_remove(gtk_theme_mons, ev->monitor);
   return ECORE_CALLBACK_RENEW;
}

EINTERN int
e_configure_option_init(void)
{
   E_Configure_Option *co;
   char buf[PATH_MAX];

   E_EVENT_CONFIGURE_OPTION_CHANGED = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_ADD = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_DEL = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_CATEGORY_ADD = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_CATEGORY_DEL = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_TAG_ADD = ecore_event_type_new();
   E_EVENT_CONFIGURE_OPTION_TAG_DEL = ecore_event_type_new();

   domain_hash = eina_hash_string_superfast_new((Eina_Free_Cb)_e_configure_option_list_free);
   tags_name_hash = eina_hash_string_superfast_new(NULL);
   tags_hash = eina_hash_string_superfast_new(NULL);
   tags_tag_alias_hash = eina_hash_stringshared_new((Eina_Free_Cb)_e_configure_option_tag_alias_list_free);
   tags_alias_hash = eina_hash_string_superfast_new(NULL);
   tags_alias_name_hash = eina_hash_string_superfast_new(NULL);
#define OPT_ADD(TYPE, NAME, DESC, ...)                                                                         \
  co = e_configure_option_add(E_CONFIGURE_OPTION_TYPE_##TYPE, DESC, #NAME, &e_config->NAME, NULL); \
  e_configure_option_tags_set(co, (const char *[]){__VA_ARGS__, NULL}, 0)
#define OPT_HELP(STR) \
  co->help = eina_stringshare_add(STR)
#define OPT_MINMAX_STEP_FMT(MIN, MAX, STEP, FMT)                   \
  co->minmax[0] = (MIN), co->minmax[1] = (MAX), co->step = (STEP), \
  co->info = eina_stringshare_add(_(FMT))
#define OPT_ICON(ICON) \
  e_configure_option_data_set(co, "icon", eina_stringshare_add(ICON))

  e_configure_option_domain_current_set("internal");

   OPT_ADD(BOOL, show_splash, _("Show splash screen on startup"), _("splash"), _("startup"));
   OPT_ADD(STR, init_default_theme, _("Startup splash theme"), _("splash"), _("startup"), _("theme"), _("animate"));
   co->info_cb = _e_configure_init_default_theme_info_cb;
   OPT_ICON("preferences-startup");

   OPT_ADD(STR, transition_start, _("Startup transition effect"), _("transition"), _("startup"), _("theme"), _("animate"));
   co->info_cb = _e_configure_transition_info_cb;
   co->thumb_cb = _e_configure_transition_thumb_cb;
   OPT_ICON("preferences-transitions");
   OPT_ADD(STR, transition_desk, _("Desk change transition effect"), _("transition"), _("vdesk"), _("theme"), _("animate"));
   co->info_cb = _e_configure_transition_info_cb;
   co->thumb_cb = _e_configure_transition_thumb_cb;
   OPT_ICON("preferences-transitions");
   OPT_ADD(STR, transition_change, _("Wallpaper change transition effect"), _("transition"), _("wallpaper"), _("theme"), _("animate"));
   co->info_cb = _e_configure_transition_info_cb;
   co->thumb_cb = _e_configure_transition_thumb_cb;
   OPT_ICON("preferences-transitions");

   OPT_ADD(STR, desktop_default_background, _("Default desktop wallpaper"), _("wallpaper"));
   co->info_cb = _e_configure_background_info_cb;
   co->funcs[0].none = co->funcs[1].none = e_bg_update;
   OPT_ICON("preferences-desktop-wallpaper");
   OPT_ADD(STR, desktop_default_name, _("Default desktop name"), _("desktop"), _("name"));
   OPT_HELP(_("Used in Pager displays"));
   OPT_ICON("preferences-desktop");
   //OPT_ADD(STR, desktop_default_window_profile;

   OPT_ADD(DOUBLE, menus_scroll_speed, _("Menu scroll speed"), _("menu"), _("scroll"), _("speed"));
   OPT_MINMAX_STEP_FMT(1.0, 20000.0, 100, "%5.0f pixels/s");
   OPT_HELP(_("Speed at which the menus move onto screen if offscreen"));
   OPT_ADD(DOUBLE, menus_fast_mouse_move_threshhold, _("Menu fast move threshold"), _("menu"), _("move"), _("speed"));
   OPT_MINMAX_STEP_FMT(1.0, 2000.0, 10, "%4.0f pixels/s");
   OPT_HELP(_("Moving the mouse faster than this speed over a menu causes menu items to not be selected"));
   OPT_ADD(DOUBLE, menus_click_drag_timeout, _("Menu mouse deactivate delay"), _("menu"), _("drag"), _("delay"));
   OPT_MINMAX_STEP_FMT(0.0, 10.0, 0.25, "%2.2f seconds");
   OPT_HELP(_("The minimum time before a menu can be closed by clicking the mouse outside the menu"));
   OPT_ADD(DOUBLE_INT, menu_autoscroll_margin, _("Menu autoscroll margin"), _("menu"), _("scroll"));
   OPT_MINMAX_STEP_FMT(0, 50, 1, "%2.0f pixels");
   OPT_HELP(_("The distance from the edge of the screen before menus begin to move away from the edge"));
   OPT_ADD(DOUBLE_INT, menu_autoscroll_cursor_margin, _("Menu autoscroll cursor margin"), _("menu"), _("scroll"), _("mouse"), _("pointer"));
   OPT_MINMAX_STEP_FMT(0, 50, 1, "%2.0f pixels");
   OPT_HELP(_("The distance of the mouse pointer from the edge of the screen before menus begin to move away from the edge"));

   OPT_ADD(BOOL, border_shade_animate, _("Enable window shading animation"), _("border"), _("shade"), _("animate"));
   OPT_ADD(ENUM, border_shade_transition, _("Window shade animation type"), _("border"), _("shade"), _("animate")); //enum
   co->info_cb = _e_configure_border_shade_transition_info_cb;
   OPT_ICON("preferences-system-windows");
   OPT_ADD(DOUBLE, border_shade_speed, _("Window shade animation speed"), _("border"), _("shade"), _("animate"), _("speed"));
   OPT_MINMAX_STEP_FMT(100, 9900, 100, "%4.0f pixels/s");

   /* advanced */
   OPT_ADD(DOUBLE, framerate, _("Framerate"), _("speed"), _("animate"));
   OPT_MINMAX_STEP_FMT(5.0, 200.0, 1, "%1.0f frames/second");
   OPT_HELP(_("The framerate at which animations in Enlightenment occur"));
   co->funcs[0].none = _e_configure_framerate_changed;
   OPT_ADD(DOUBLE_INT, priority, _("Application exec priority"), _("application"), _("exec"), _("priority"));
   OPT_MINMAX_STEP_FMT(0, 19, 1, "%1.0f");
   co->funcs[0].one = ecore_exe_run_priority_set;
   OPT_ADD(DOUBLE_INT, image_cache, _("Image cache size"), _("cache"), _("image"), _("size"));
   OPT_MINMAX_STEP_FMT(0, 32 * 1024, 1024, "%4.0f KiB");
   co->funcs[0].none = _e_configure_framerate_changed;
   OPT_ADD(DOUBLE_INT, font_cache, _("Font cache size"), _("cache"), _("font"), _("size"));
   OPT_MINMAX_STEP_FMT(0, 4 * 1024, 128, "%3.0f KiB");
   co->funcs[0].none = _e_configure_framerate_changed;
   OPT_ADD(DOUBLE_INT, edje_cache, _("Edje cache size"), _("cache"), _("edje"), _("size"));
   OPT_MINMAX_STEP_FMT(0, 256, 1, "%1.0f files");
   co->funcs[0].none = _e_configure_framerate_changed;
   OPT_ADD(DOUBLE_INT, edje_collection_cache, _("Edje collection cache size"), _("cache"), _("edje"), _("size"));
   OPT_MINMAX_STEP_FMT(0, 512, 2, "%1.0f collections");
   co->funcs[0].none = _e_configure_framerate_changed;
   OPT_ADD(DOUBLE_INT, cache_flush_poll_interval, _("Cache flushing interval"), _("cache"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(8, 4096, 8, "%1.0f ticks");
   co->funcs[0].none = _e_configure_framerate_changed;

   OPT_ADD(DOUBLE_INT, zone_desks_x_count, _("Horizontal virtual desktop count"), _("vdesk"), _("desktop"), _("screen"));
   co->funcs[0].none = _e_configure_zone_desks_count_changed;
   OPT_ADD(DOUBLE_INT, zone_desks_y_count, _("Vertical virtual desktop count"), _("vdesk"), _("desktop"), _("screen"));
   co->funcs[0].none = _e_configure_zone_desks_count_changed;
   OPT_ADD(BOOL, edge_flip_dragging, _("Edge flip while dragging"), _("edge"), _("flip"), _("drag"), _("binding"));
   OPT_HELP(_("Enable edge binding functionality while dragging objects to screen edge"));
   OPT_ADD(BOOL, use_shaped_win, _("Use shaped windows instead of ARGB"), _("border"));

   OPT_ADD(CUSTOM, modules, _("Module settings"), _("module"));
   co->info = eina_stringshare_add("extensions/modules");
   OPT_ICON("preferences-plugin");
   OPT_ADD(BOOL, no_module_delay, _("Disable module delay"), _("module"), _("delay"));
   OPT_HELP(_("If enabled, this causes E to load all modules at once during startup "
              "instead of loading them incrementally"));

   /* FIXME */
   //OPT_ADD(CUSTOM, language, _("Language"), _("language"));
   //co->info = eina_stringshare_add("language/language_settings");
   //OPT_ICON("preferences-desktop-locale");
   //co->requires_restart = 1;
   //co->funcs[0].one = e_intl_language_set;
   //OPT_ADD(CUSTOM, desklock_language, _("Desklock language"), _("desklock"), _("language"));
   //co->info = eina_stringshare_add("language/desklock_language_settings");
   //OPT_ICON("preferences-desklock-locale");

   OPT_ADD(ENUM, window_placement_policy, _("Window placement policy"), _("border"), _("placement")); //enum
   co->info_cb = _e_configure_window_placement_policy_info_cb;
   OPT_ICON("preferences-system-windows");
   OPT_HELP(_("Determines where and how new windows are placed when created"));
   OPT_ADD(BOOL, window_grouping, _("Group new windows from same application"), _("border"), _("placement"));
   OPT_ADD(BOOL, desk_auto_switch, _("Switch to desk of new window"), _("border"), _("placement"), _("vdesk"));

   OPT_ADD(ENUM, focus_policy, _("Window focus policy"), _("focus"), _("border")); //enum
   co->info_cb = _e_configure_focus_policy_info_cb;
   OPT_ICON("preferences-focus");
   OPT_ADD(ENUM, focus_setting, _("New window focus policy"), _("focus"), _("border")); //enum
   co->info_cb = _e_configure_focus_setting_info_cb;
   OPT_ICON("preferences-focus");
   OPT_ADD(BOOL, pass_click_on, _("Pass click to unfocused windows"), _("focus"), _("border"), _("click"));
   OPT_HELP(_("When clicking an unfocused window, pass this click through to the application instead of only using it to focus the window"));
   OPT_ADD(ENUM, window_activehint_policy, _("Policy when applications request focus"), _("focus"), _("border")); //enum
   co->info_cb = _e_configure_window_activehint_policy_info_cb;
   OPT_ICON("preferences-focus");
   OPT_ADD(BOOL, always_click_to_raise, _("Always raise window when clicked"), _("focus"), _("border"), _("click"), _("raise"));
   OPT_ADD(BOOL, always_click_to_focus, _("Always focus window when clicked"), _("focus"), _("border"), _("click"));
   OPT_ADD(BOOL, use_auto_raise, _("Enable window autoraise"), _("focus"), _("border"), _("raise"));
   OPT_ADD(DOUBLE, auto_raise_delay, _("Window autoraise delay"), _("focus"), _("border"), _("raise"), _("delay"));
   OPT_MINMAX_STEP_FMT(0.0, 9.9, 0.1, "%1.1f seconds");
   OPT_ADD(BOOL, focus_last_focused_per_desktop, _("Revert window focus on desk switch"), _("border"), _("focus"), _("vdesk"));
   OPT_ADD(BOOL, focus_revert_on_hide_or_close, _("Revert window focus on window hide or close"), _("border"), _("focus"));
   OPT_ADD(BOOL, pointer_slide, _("Warp pointer to new windows and away from closed windows"), _("border"), _("focus"), _("warp"), _("pointer"));
   OPT_ADD(BOOL, border_raise_on_mouse_action, _("Windows raise on mouse move/resize"), _("border"), _("raise"), _("focus"), _("mouse"), _("pointer"), _("move"), _("resize"));
   OPT_ADD(BOOL, border_raise_on_focus, _("Windows raise when focused"), _("border"), _("raise"), _("focus"), _("mouse"));

   OPT_ADD(DOUBLE_INT, drag_resist, _("Shelf gadget resistance"), _("gadget"), _("resist"), _("drag"), _("shelf"));
   OPT_MINMAX_STEP_FMT(0, 100, 2, "%2.0f pixels");

   OPT_ADD(BOOL, use_resist, _("Enable resistance when dragging windows"), _("border"), _("resist"), _("drag"));
   OPT_ADD(DOUBLE_INT, desk_resist, _("Window resistance against screen edges"), _("border"), _("resist"), _("vdesk"), _("screen"), _("drag"));
   OPT_MINMAX_STEP_FMT(0, 100, 2, "%2.0f pixels");
   OPT_ADD(DOUBLE_INT, window_resist, _("Window resistance against other windows"), _("border"), _("resist"), _("drag"));
   OPT_MINMAX_STEP_FMT(0, 100, 2, "%2.0f pixels");
   OPT_ADD(DOUBLE_INT, gadget_resist, _("Window resistance against desktop gadgets"), _("gadget"), _("resist"), _("desktop"), _("drag"));
   OPT_MINMAX_STEP_FMT(0, 100, 2, "%2.0f pixels");

   OPT_ADD(BOOL, geometry_auto_move, _("Ensure initial placement of windows inside useful geometry"), _("border"), _("placement"));
   OPT_HELP(_("Useful geometry is calculated as the screen size minus the geometry of any shelves which do not allow windows to overlap them."
              "This option ensures that non-user placement of windows will be inside the useful geometry of the current screen"));
   OPT_ADD(BOOL, geometry_auto_resize_limit, _("Limit window autoresizing to useful geometry"), _("border"), _("placement"), _("resize"));
   OPT_HELP(_("Useful geometry is calculated as the screen size minus the geometry of any shelves which do not allow windows to overlap them"));

   OPT_ADD(BOOL, winlist_warp_while_selecting, _("Winlist moves pointer to currently selected window while selecting"), _("border"), _("winlist"), _("focus"), _("warp"), _("pointer"));
   OPT_ADD(BOOL, winlist_warp_at_end, _("Winlist moves pointer to currently selected window after winlist closes"), _("border"), _("winlist"), _("focus"), _("warp"), _("pointer"));
   OPT_ADD(BOOL, winlist_no_warp_on_direction, _("Disable pointer warping on winlist directional focus change"), _("border"), _("winlist"), _("focus"), _("warp"), _("pointer"));
   OPT_HELP(_("This option, when enabled, disables pointer warping only when switching windows using a directional winlist action (up/down/left/right)"));
   OPT_ADD(DOUBLE, winlist_warp_speed, _("Winlist pointer warp speed while selecting"), _("border"), _("winlist"), _("focus"), _("warp"), _("pointer"), _("speed"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(BOOL, winlist_scroll_animate, _("Enable winlist scroll animation"), _("border"), _("winlist"), _("animate"));
   OPT_ADD(DOUBLE, winlist_scroll_speed, _("Winlist scroll speed"), _("border"), _("winlist"), _("animate"), _("speed"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(BOOL, winlist_list_show_iconified, _("Winlist shows iconified windows"), _("border"), _("winlist"), _("minimize"));
   OPT_ADD(BOOL, winlist_list_show_other_desk_iconified, _("Winlist shows iconified windows from other desks"), _("border"), _("winlist"), _("minimize"), _("vdesk"));
   OPT_ADD(BOOL, winlist_list_show_other_screen_iconified, _("Winlist shows iconified windows from other screens"), _("border"), _("winlist"), _("minimize"), _("screen"));
   OPT_ADD(BOOL, winlist_list_show_other_desk_windows, _("Winlist shows windows from other desks"), _("border"), _("winlist"), _("vdesk"));
   OPT_ADD(BOOL, winlist_list_show_other_screen_windows, _("Winlist shows windows from other screens"), _("border"), _("winlist"), _("screen"));
   OPT_ADD(BOOL, winlist_list_uncover_while_selecting, _("Winlist uniconifies and unshades windows while selecting"), _("border"), _("winlist"), _("raise"));
   OPT_ADD(BOOL, winlist_list_jump_desk_while_selecting, _("Winlist switches desks while selecting"), _("border"), _("winlist"), _("vdesk"));
   OPT_ADD(BOOL, winlist_list_focus_while_selecting, _("Winlist focuses windows while selecting"), _("border"), _("winlist"), _("focus"));
   OPT_ADD(BOOL, winlist_list_raise_while_selecting, _("Winlist raises windows while selecting"), _("border"), _("winlist"), _("raise"), _("focus"));
   OPT_ADD(DOUBLE, winlist_pos_align_x, _("Winlist horizontal alignment"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(DOUBLE, winlist_pos_align_y, _("Winlist vertical alignment"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(DOUBLE, winlist_pos_size_w, _("Winlist width"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(DOUBLE, winlist_pos_size_h, _("Winlist height"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0.0, 1.0, 0.01, "%1.2f");
   OPT_ADD(DOUBLE_INT, winlist_pos_min_w, _("Winlist minimum width"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0, 4000, 10, "%4.0f pixels");
   OPT_ADD(DOUBLE_INT, winlist_pos_min_h, _("Winlist minimum height"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0, 4000, 10, "%4.0f pixels");
   OPT_ADD(DOUBLE_INT, winlist_pos_max_w, _("Winlist maximum width"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0, 4000, 10, "%4.0f pixels");
   OPT_ADD(DOUBLE_INT, winlist_pos_max_h, _("Winlist maximum height"), _("border"), _("winlist"), _("placement"));
   OPT_MINMAX_STEP_FMT(0, 4000, 10, "%4.0f pixels");

   OPT_ADD(ENUM, fullscreen_policy, _("Fullscreen window policy"), _("fullscreen")); //enum
   co->info_cb = _e_configure_fullscreen_policy_info_cb;
   OPT_ICON("preferences-window-geometry");
   OPT_ADD(ENUM, maximize_policy, _("Window maximize policy"), _("border"), _("maximize")); //enum
   co->info_cb = _e_configure_maximize_policy_info_cb;
   OPT_ICON("preferences-window-geometry");
   OPT_ADD(BOOL, allow_manip, _("Allow moving of maximized windows"), _("border"), _("maximize"));
   OPT_ADD(BOOL, border_fix_on_shelf_toggle, _("Adjust windows on shelf toggle"), _("border"), _("shelf"), _("placement"));
   OPT_HELP(_("When using an autohiding shelf, this option causes maximized windows to expand and contract to fill the space that the shelf occupies when it hides"));
   OPT_ADD(BOOL, allow_above_fullscreen, _("Allow windows above fullscreen windows"), _("border"), _("focus"), _("placement"), _("fullscreen"));

   OPT_ADD(BOOL, kill_if_close_not_possible, _("Kill window if process not responding to close"), _("border"), _("kill"));
   OPT_ADD(BOOL, kill_process, _("Kill process instead of client"), _("border"), _("kill"));
   OPT_ADD(DOUBLE, kill_timer_wait, _("Window kill delay"), _("border"), _("kill"), _("delay"));
   OPT_MINMAX_STEP_FMT(1, 30, 1, "%1.0f seconds");
   OPT_ADD(BOOL, ping_clients, _("Enable window client pinging"), _("border"), _("delay"));
   OPT_ADD(DOUBLE_INT, ping_clients_interval, _("Window client ping interval (CPU ticks)"), _("border"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(1, 256, 1, "%1.0f seconds");

   /* FIXME */
   //OPT_ADD(CUSTOM, remembers, _("Window remember settings"), _("border"), _("remember"));
   //co->info = eina_stringshare_add("windows/window_remembers");
   //OPT_ICON("preferences-desktop-window-remember");
   OPT_ADD(BOOL, remember_internal_windows, _("Remember internal window geometry"), _("border"), _("remember"));
   OPT_HELP(_("This option causes E to remember the geometry of its internal dialogs and windows, NOT including filemanager windows"));
   OPT_ADD(BOOL, remember_internal_fm_windows, _("Remember internal filemanager window geometry"), _("border"), _("remember"), _("files"));
   OPT_HELP(_("This option causes E to remember the geometry of its internal filemanager windows, NOT including dialog windows"));

   OPT_ADD(BOOL, move_info_follows, _("Window position info follows window when moving"), _("border"), _("placement"), _("move"));
   OPT_ADD(BOOL, resize_info_follows, _("Window geometry info follows window when resizing"), _("border"), _("placement"), _("resize"));
   OPT_ADD(BOOL, move_info_visible, _("Window position info visible when moving"), _("border"), _("placement"), _("move"));
   OPT_ADD(BOOL, resize_info_visible, _("Window geometry info visible when moving"), _("border"), _("placement"), _("resize"));

   /* FIXME */
   //OPT_ADD(CUSTOM, input_method, _("Input method"), _("input"), _("language"));
   //co->info = eina_stringshare_add("language/input_method_settings");
   //OPT_ICON("preferences-imc");
   //co->funcs[0].one = e_intl_input_method_set;

   OPT_ADD(BOOL, transient.move, _("Transient windows follow movement of their child"), _("border"), _("transient"), _("move"), _("placement"));
   OPT_ADD(BOOL, transient.resize, _("Transient windows follow resize of their child"), _("border"), _("transient"), _("placement"), _("resize"));
   OPT_ADD(BOOL, transient.raise, _("Transient windows follow raise of their child"), _("border"), _("transient"), _("raise"), _("placement"));
   OPT_ADD(BOOL, transient.lower, _("Transient windows follow lower of their child"), _("border"), _("transient"), _("placement"), _("raise"));
   OPT_ADD(BOOL, transient.layer, _("Transient windows follow layer change of their child"), _("border"), _("transient"), _("placement"));
   OPT_ADD(BOOL, transient.desktop, _("Transient windows follow desk change of their child"), _("border"), _("transient"), _("placement"), _("vdesk"));
   OPT_ADD(BOOL, transient.iconify, _("Transient windows follow iconification of their child"), _("border"), _("transient"), _("minimize"));

   OPT_ADD(BOOL, menu_icons_hide, _("Disable icons in menus"), _("menu"), _("image"));
   OPT_ADD(BOOL, menu_eap_name_show, _("Application menus shows Name field"), _("menu"));
   OPT_HELP(_("This information is taken from the related .desktop file"));
   OPT_ADD(BOOL, menu_eap_generic_show, _("Application menus shows Generic field"), _("menu"));
   OPT_HELP(_("This information is taken from the related .desktop file"));
   OPT_ADD(BOOL, menu_eap_comment_show, _("Application menus shows Comment field"), _("menu"));
   OPT_HELP(_("This information is taken from the related .desktop file"));

   OPT_ADD(BOOL, menu_favorites_show, _("Show Favorite Applications in the main menu"), _("menu"), _("application"));
   OPT_ADD(BOOL, menu_apps_show, _("Show Applications in the main menu"), _("menu"), _("application"));

   OPT_ADD(BOOL, menu_gadcon_client_toplevel, _("Show gadget settings in top-level gadget menu"), _("menu"), _("gadget"));

   OPT_ADD(STR, exebuf_term_cmd, _("Launch commands with this command"), _("exec"));
   OPT_ICON("modules-launcher");
   OPT_HELP(_("Command used to launch files and applications"));

   OPT_ADD(BOOL, use_app_icon, _("Window borders use application icon"), _("border"), _("image"));
   OPT_HELP(_("Applications provide their own icons. If this option is not set, E will use internal theme icons instead of the application-provided icon"));

   /* FIXME */
   //OPT_ADD(CUSTOM, path_append_data, _("Enlightenment profile settings"), _("profile"));
   //co->info = eina_stringshare_add("settings/profiles");
   //OPT_ICON("preferences-profiles");

   OPT_ADD(BOOL, cnfmdlg_disabled, _("Disable confirmation dialogs"), _("confirm"), _("dialog"));
   OPT_HELP(_("This option suppresses all confirmation dialogs and assumes that the user has clicked the confirm option"));
   OPT_ADD(BOOL, cfgdlg_auto_apply, _("Configuration dialogs automatically apply their changes"), _("dialog"), _("settings"));
   OPT_HELP(_("This option causes any configuration options to be applied immediately when changed instead of requiring the 'Apply' button to be clicked"));
   OPT_ADD(BOOL, cfgdlg_default_mode, _("Configuration dialogs show advanced view by default"), _("dialog"), _("border"), _("settings"));
   OPT_HELP(_("Configurations dialogs can have basic and advanced views; this option causes all configuration dialogs to show the advanced view by default"));
   OPT_ADD(BOOL, cfgdlg_normal_wins, _("Configuration dialog windows are normal windows"), _("dialog"), _("border"));
   OPT_HELP(_("This option causes configuration dialogs to be normal windows instead of dialog windows"));

   OPT_ADD(ENUM, font_hinting, _("Set font hinting mode"), _("font"), _("hinting")); //enum
   co->info_cb = _e_configure_font_hinting_info_cb;
   OPT_ICON("preferences-desktop-font");

/* seems to be disabled ?
   OPT_ADD(STR, desklock_personal_passwd, _("Desklock custom password"), _("desklock")); //passwd
   OPT_HELP(_("This option causes desklock to use a custom-provided password instead of the user's password"));
 */
   OPT_ADD(BOOL, desklock_auth_method, _("Use custom command for desklock"), _("desklock"), _("exec"));
   OPT_HELP(_("This option allows an external application to manage desklock"));
   OPT_ADD(STR, desklock_custom_desklock_cmd, _("Custom desklock command"), _("desklock"), _("exec"));
   OPT_ICON("preferences-system-lock-screen");
   OPT_ADD(ENUM, desklock_login_box_zone, _("Desklock login box shows on which screen?"), _("desklock"), _("screen")); //enum+slider
   co->info_cb = _e_configure_desklock_login_box_zone_info_cb;
   OPT_ICON("preferences-system-lock-screen");
   OPT_ADD(BOOL, desklock_start_locked, _("Desklock activates on login"), _("desklock"));
   OPT_ADD(BOOL, desklock_on_suspend, _("Desklock activates on resume from suspend"), _("desklock"));
   OPT_ADD(BOOL, desklock_autolock_screensaver, _("Desklock activates during screensaver"), _("desklock"), _("delay"), _("screensaver"));
   OPT_ADD(DOUBLE, desklock_post_screensaver_time, _("Desklock activates X seconds after screensaver activates"), _("desklock"), _("delay"), _("screensaver"));
   OPT_MINMAX_STEP_FMT(0.0, 300.0, 10, "%2.0f seconds");
   OPT_ADD(BOOL, desklock_autolock_idle, _("Desklock activates when idle"), _("desklock"), _("delay"));
   OPT_ADD(DOUBLE, desklock_autolock_idle_timeout, _("Desklock activates when idle for X seconds"), _("desklock"), _("delay"));
   OPT_MINMAX_STEP_FMT(30, 5400.0, 60, "%2.0f seconds");
   OPT_ADD(BOOL, desklock_use_custom_desklock, _("Use custom desklock wallpaper"), _("desklock"), _("wallpaper"));
   OPT_ADD(BOOL, desklock_ask_presentation, _("Prompt for desklock timer delay if deactivated quickly"), _("desklock"), _("delay"));
   OPT_ADD(DOUBLE, desklock_ask_presentation_timeout, _("Desklock quick deactivation timer delay"), _("desklock"), _("delay"));
   OPT_MINMAX_STEP_FMT(1.0, 300.0, 10, "%2.0f seconds");

   OPT_ADD(BOOL, screensaver_enable, _("Enable screensaver"), _("screensaver"));
   OPT_ADD(DOUBLE_INT, screensaver_timeout, _("Screensaver activates when idle for X seconds"), _("screensaver"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(30, 5400, 60, "%2.0f seconds");

   //OPT_ADD(INT, screensaver_interval, _("screensaver"), _("delay")); NOT USED?
   //OPT_ADD(INT, screensaver_blanking, _("screensaver"), _("delay")); NOT USED?
   //OPT_ADD(INT, screensaver_expose, _("screensaver"), _("delay")); NOT USED?
   OPT_ADD(BOOL, screensaver_ask_presentation, _("Prompt for screensaver timer delay if deactivated quickly"), _("screensaver"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_screensaver_changed;
   OPT_ADD(DOUBLE, screensaver_ask_presentation_timeout, _("Screensaver quick deactivation timer delay"), _("screensaver"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(1.0, 300.0, 10, "%2.0f seconds");
   co->funcs[0].none = co->funcs[1].none = _e_configure_screensaver_changed;
   OPT_ADD(BOOL, screensaver_suspend, _("Suspend when screensaver activates"), _("screensaver"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_screensaver_changed;
   OPT_ADD(BOOL, screensaver_suspend_on_ac, _("Suspend when screensaver activates even if on AC"), _("screensaver"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_screensaver_changed;
   OPT_ADD(DOUBLE, screensaver_suspend_delay, _("Screensaver suspend delay"), _("screensaver"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(1.0, 20.0, 1, "%1.0f seconds");
   co->funcs[0].none = co->funcs[1].none = _e_configure_screensaver_changed;

/* appears to use screensaver values
   OPT_ADD(BOOL, dpms_enable, _("Enable DPMS"), _("dpms"));
   OPT_ADD(BOOL, dpms_standby_enable, _("dpms"));
   OPT_ADD(DOUBLE_INT, dpms_standby_timeout, _("dpms"), _("delay"));
   OPT_MINMAX_STEP_FMT(30, 5400);
   OPT_ADD(BOOL, dpms_suspend_enable, _("dpms"));
   OPT_ADD(DOUBLE_INT, dpms_suspend_timeout, _("dpms"), _("delay"));
   OPT_MINMAX_STEP_FMT(30, 5400);
   OPT_ADD(BOOL, dpms_off_enable, _("dpms"));
   OPT_ADD(DOUBLE_INT, dpms_off_timeout, _("dpms"), _("delay"));
   OPT_MINMAX_STEP_FMT(30, 5400);
 */
   OPT_ADD(ENUM, clientlist_group_by, _("Window list menu grouping policy"), _("menu"), _("border")); //enum
   co->info_cb = _e_configure_clientlist_group_by_info_cb;
   OPT_ICON("preferences-winlist");
   OPT_ADD(BOOL, clientlist_include_all_zones, _("Window list menu includes windows from all screens"), _("menu"), _("border"), _("screen"));
   OPT_ADD(ENUM, clientlist_separate_with, _("Window list menu separator policy"), _("menu"), _("border")); //enum
   co->info_cb = _e_configure_clientlist_separate_with_info_cb;
   OPT_ICON("preferences-winlist");
   OPT_ADD(ENUM, clientlist_sort_by, _("Window list menu sort policy"), _("menu"), _("border")); //enum
   co->info_cb = _e_configure_clientlist_sort_by_info_cb;
   OPT_ICON("preferences-winlist");
   OPT_ADD(ENUM, clientlist_separate_iconified_apps, _("Window list menu iconified window grouping policy"), _("menu"), _("border"), _("minimize")); //enum
   co->info_cb = _e_configure_clientlist_separate_iconified_apps_info_cb;
   OPT_ICON("preferences-winlist");
   OPT_ADD(BOOL, clientlist_warp_to_iconified_desktop, _("Window list menu warps to desktop of selected iconified window"), _("menu"), _("border"), _("warp"), _("vdesk"));
   OPT_ADD(BOOL, clientlist_limit_caption_len, _("Enable window list menu length limit"), _("menu"), _("border"), _("size"));
   OPT_ADD(DOUBLE_INT, clientlist_max_caption_len, _("Window list menu length limit (characters)"), _("menu"), _("border"), _("size")); //slider
   OPT_MINMAX_STEP_FMT(2, E_CLIENTLIST_MAX_CAPTION_LEN, 2, "%1.0f characters");

   OPT_ADD(BOOL, use_e_cursor, _("Use Enlightenment theme cursor"), _("pointer"), _("theme"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;
   OPT_ADD(DOUBLE_INT, cursor_size, _("Mouse cursor size"), _("pointer"), _("size"), _("theme"));
   OPT_MINMAX_STEP_FMT(8, 128, 4, "%1.0f pixels");
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;
   OPT_ADD(BOOL, show_cursor, _("Show mouse cursor"), _("pointer"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;
   OPT_ADD(BOOL, idle_cursor, _("Enable idle effects for mouse cursor"), _("pointer"), _("animate"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;
   OPT_ADD(BOOL, mouse_hand, _("Enable left-handed mouse"), _("mouse"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;
   /* FIXME: need to group these two
      OPT_ADD(DOUBLE_INT, mouse_accel_numerator, _("mouse"), _("speed")); //slider
      OPT_MINMAX_STEP_FMT(1, 10);
      OPT_ADD(DOUBLE_INT, mouse_accel_denominator, _("mouse"), _("speed")); //also slider
      OPT_MINMAX_STEP_FMT(1, 10);
    */
   OPT_ADD(DOUBLE_INT, mouse_accel_threshold, _("Mouse acceleration threshold"), _("mouse"), _("speed"));
   OPT_MINMAX_STEP_FMT(1, 10, 1, "%1.0f");
   co->funcs[0].none = co->funcs[1].none = _e_configure_pointer_changed;

   OPT_ADD(BOOL, desk_flip_wrap, _("Enable desk flip between last and first desks"), _("vdesk"), _("flip"), _("edge"));
   OPT_ADD(BOOL, fullscreen_flip, _("Enable desk flipping with fullscreen windows"), _("vdesk"), _("flip"), _("edge"), _("fullscreen"));
   OPT_ADD(BOOL, multiscreen_flip, _("Enable desk flipping with multiple monitors (DANGEROUS)"), _("vdesk"), _("flip"), _("edge"), _("screen"));

   OPT_ADD(ENUM, desk_flip_animate_mode, _("Desk flip animation type"), _("vdesk"), _("animate"), _("flip")); //enum
   co->info_cb = _e_configure_desk_flip_animate_mode_info_cb;
   OPT_ICON("preferences-desktop");
   //OPT_ADD(INT, desk_flip_animate_interpolation, _("vdesk"), _("animate"), _("flip")); //NOT USED?
   OPT_ADD(DOUBLE, desk_flip_animate_time, _("Desk flip animation length"), _("vdesk"), _("animate"), _("flip"), _("speed"));
   OPT_MINMAX_STEP_FMT(0, 5, 0.05, "%1.2f seconds");

   OPT_ADD(STR, theme_default_border_style, _("Default window border style"), _("border"), _("theme"));
   co->info_cb = _e_configure_border_style_info_cb;
   co->thumb_cb = _e_configure_border_style_thumb_cb;
   OPT_ICON("preferences-system-windows");

   OPT_ADD(ENUM, screen_limits, _("Window screen limit policy"), _("screen"), _("size"), _("border"), _("placement")); //enum
   co->info_cb = _e_configure_screen_limits_info_cb;
   OPT_ICON("preferences-system-windows");

   OPT_ADD(DOUBLE_INT, thumb_nice, _("Thumbnailing process priority"), _("priority"), _("image")); //enum
   OPT_MINMAX_STEP_FMT(0, 19, 1, "%1.0f");
   OPT_HELP(_("Enlightenment runs its own thumbnailing daemon in the background. This option configures the priority of that process"));

   OPT_ADD(BOOL, thumbscroll_enable, _("Enable click-to-drag scrolling (thumbscrolling)"), _("scroll"));
   OPT_ADD(DOUBLE_INT, thumbscroll_threshhold, _("Thumbscroll threshold"), _("scroll"), _("speed")); //slider
   OPT_MINMAX_STEP_FMT(0, 64, 4, "%1.0f pixels");
   OPT_ADD(DOUBLE, thumbscroll_momentum_threshhold, _("Thumbscroll momentum threshold"), _("scroll"), _("speed")); //slider
   OPT_MINMAX_STEP_FMT(0, 2000, 20, "%1.0f pixels/second");
   OPT_ADD(DOUBLE, thumbscroll_friction, _("Thumbscroll resistance"), _("scroll"), _("resist")); //slider
   OPT_MINMAX_STEP_FMT(0, 5.0, 0.1, "%1.0f");

   OPT_ADD(BOOL, show_desktop_icons, _("Show files on desktop"), _("desktop"), _("files"));
   co->changed_action = eina_stringshare_add("fileman_reset");
   OPT_ADD(BOOL, filemanager_single_click, _("Filemanager uses single click to activate"), _("click"), _("files"));
   co->changed_action = eina_stringshare_add("fileman_reset");
   OPT_ADD(BOOL, device_desktop, _("Filemanager shows removable devices on desktop"), _("desktop"), _("files"));
   co->funcs[0].none = e_fm2_device_hide_desktop_icons;
   co->funcs[1].none = e_fm2_device_show_desktop_icons;
   OPT_ADD(BOOL, device_auto_mount, _("Filemanager automatically mounts removable devices when attached"), _("files"));
   OPT_ADD(BOOL, device_auto_open, _("Filemanager automatically opens removable devices when attached"), _("files"));
   OPT_ADD(BOOL, filemanager_copy, _("Filemanager always performs `cp+rm` instead of `mv`"), _("files"));
   OPT_ADD(BOOL, filemanager_secure_rm, _("Filemanager deletes files securely"), _("files"));

   OPT_ADD(DOUBLE, border_keyboard.timeout, _("Window change timeout when moving or resizing using keyboard"), _("border"), _("placement"), _("delay"), _("key")); //slider
   OPT_MINMAX_STEP_FMT(1, 10, 1, "%1.0f seconds");
   OPT_ADD(DOUBLE_UCHAR, border_keyboard.move.dx, _("Window horizontal movement speed when using keyboard"), _("border"), _("placement"), _("move"), _("key")); //slider
   OPT_MINMAX_STEP_FMT(1, 255, 1, "%1.0f pixels");
   OPT_ADD(DOUBLE_UCHAR, border_keyboard.move.dy, _("Window vertical movement speed when using keyboard"), _("border"), _("placement"), _("move"), _("key")); //slider
   OPT_MINMAX_STEP_FMT(1, 255, 1, "%1.0f pixels");
   OPT_ADD(DOUBLE_UCHAR, border_keyboard.resize.dx, _("Window horizontal resize speed when using keyboard"), _("border"), _("placement"), _("resize"), _("key")); //slider
   OPT_MINMAX_STEP_FMT(1, 255, 1, "%1.0f pixels");
   OPT_ADD(DOUBLE_UCHAR, border_keyboard.resize.dy, _("Window vertical movement speed when using keyboard"), _("border"), _("placement"), _("resize"), _("key")); //slider
   OPT_MINMAX_STEP_FMT(1, 255, 1, "%1.0f pixels");

   //OPT_ADD(DOUBLE, scale.min, _("Minimum scaling"), _("scale"), _("size"));
   //co->requires_restart = 1;
   //OPT_ADD(DOUBLE, scale.max, _("Maximum scaling"), _("scale"), _("size"));
   //co->requires_restart = 1;
   OPT_ADD(DOUBLE, scale.factor, _("Overall scaling factor"), _("scale"), _("size"));
   OPT_MINMAX_STEP_FMT(0.25, 8.0, 0.25, "%1.2f pixels");
   co->requires_restart = 1;
   OPT_ADD(BOOL, scale.use_dpi, _("Use screen DPI for scaling"), _("scale"), _("size"));
   co->requires_restart = 1;
   OPT_ADD(BOOL, scale.use_custom, _("Use custom DPI for scaling"), _("scale"), _("size"));
   co->requires_restart = 1;
   OPT_ADD(DOUBLE_INT, scale.base_dpi, _("Custom DPI to use when scaling"), _("scale"), _("size"));
   OPT_MINMAX_STEP_FMT(30, 800, 1, "%1.0f DPI");
   co->requires_restart = 1;

   OPT_ADD(DOUBLE_INT, syscon.main.icon_size, _("System Console primary action icon size"), _("syscon"), _("size"), _("image"));
   OPT_MINMAX_STEP_FMT(16, 256, 4, "%1.0f pixels");
   OPT_ADD(DOUBLE_INT, syscon.secondary.icon_size, _("System Console secondary action icon size"), _("syscon"), _("size"), _("image"));
   OPT_MINMAX_STEP_FMT(16, 256, 4, "%1.0f pixels");
   OPT_ADD(DOUBLE_INT, syscon.extra.icon_size, _("System Console extra action icon size"), _("syscon"), _("size"), _("image"));
   OPT_MINMAX_STEP_FMT(16, 256, 4, "%1.0f pixels");
   OPT_ADD(DOUBLE, syscon.timeout, _("System Console idle timeout"), _("syscon"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(0, 60, 1, "%1.0f seconds");
   OPT_ADD(BOOL, syscon.do_input, _("System Console performs default action after idle timeout"), _("syscon"), _("input"));
   //Eina_List    *actions

   OPT_ADD(DOUBLE, backlight.normal, _("Backlight \"normal\" brightness"), _("backlight")); //slider
   OPT_MINMAX_STEP_FMT(0, 1.0, 0.01, "%1.2f");
   co->funcs[0].none = _e_configure_dpms_changed;
   OPT_ADD(DOUBLE, backlight.dim, _("Backlight \"dim\" brightness"), _("backlight")); //slider
   OPT_MINMAX_STEP_FMT(0, 1.0, 0.01, "%1.2f");
   co->funcs[0].none = _e_configure_dpms_changed;
   OPT_ADD(DOUBLE, backlight.transition, _("Backlight transition length"), _("backlight"), _("animate"), _("speed")); //slider
   OPT_MINMAX_STEP_FMT(0, 5.0, 0.1, "%1.1f seconds");
   co->funcs[0].none = _e_configure_dpms_changed;
   OPT_ADD(BOOL, backlight.idle_dim, _("Backlight dims after idle"), _("backlight"));
   co->funcs[0].none = co->funcs[1].none = _e_configure_dpms_changed;
   OPT_ADD(DOUBLE, backlight.timer, _("Backlight idle delay"), _("backlight"), _("delay")); //slider
   OPT_MINMAX_STEP_FMT(5, 300, 1, "%1.0f seconds");
   co->funcs[0].none = _e_configure_dpms_changed;

   /* FIXME
      OPT_ADD(DOUBLE, powersave.none, _("powersave"));
      OPT_MINMAX_STEP_FMT(0.01, 5400.00);
      OPT_ADD(DOUBLE, powersave.low, _("powersave"));
      OPT_MINMAX_STEP_FMT(0.01, 5400.00);
      OPT_ADD(DOUBLE, powersave.medium, _("powersave"));
      OPT_MINMAX_STEP_FMT(0.01, 5400.00);
      OPT_ADD(DOUBLE, powersave.high, _("powersave"));
      OPT_MINMAX_STEP_FMT(0.01, 5400.00);
      OPT_ADD(DOUBLE, powersave.extreme, _("powersave"));
      OPT_MINMAX_STEP_FMT(0.01, 5400.00);
      OPT_ADD(DOUBLE_UINT, powersave.min, _("powersave"));
      OPT_MINMAX_STEP_FMT(E_POWERSAVE_MODE_NONE, E_POWERSAVE_MODE_EXTREME);
      OPT_ADD(DOUBLE_UINT, powersave.max, _("powersave"));
      OPT_MINMAX_STEP_FMT(E_POWERSAVE_MODE_NONE, E_POWERSAVE_MODE_EXTREME);
    */
   OPT_ADD(BOOL, deskenv.load_xrdb, _("Load ~/.xrdb on startup"), _("environment"));
   OPT_ADD(BOOL, deskenv.load_xmodmap, _("Load ~/.Xmodmap"), _("environment"));
   OPT_ADD(BOOL, deskenv.load_gnome, _("Run gnome-settings-daemon"), _("environment"));
   OPT_ADD(BOOL, deskenv.load_kde, _("Run kdeinit"), _("environment"));

   OPT_ADD(BOOL, xsettings.enabled, _("Enable GTK application settings"), _("environment"), _("theme"), _("xsettings"));
   co->funcs[1].none = co->funcs[0].none = e_xsettings_config_update;
   OPT_ADD(BOOL, xsettings.match_e17_theme, _("Try setting GTK theme to match E17 theme"), _("environment"), _("theme"), _("xsettings"));
   co->funcs[1].none = co->funcs[0].none = e_xsettings_config_update;
   OPT_ADD(STR, xsettings.net_theme_name, _("GTK theme name"), _("environment"), _("theme"), _("name"), _("xsettings"));
   co->funcs[0].none = e_xsettings_config_update;
   co->info_cb = _e_configure_netwm_theme_info_cb;
   co->thumb_cb = _e_configure_netwm_theme_thumb_cb;
   OPT_ICON("preferences-desktop-theme");

   OPT_ADD(BOOL, xsettings.match_e17_icon_theme, _("Enable use of icon theme for applications"), _("environment"), _("theme"), _("image"), _("xsettings"));
   co->funcs[0].none = e_xsettings_config_update;
   OPT_ADD(STR, icon_theme, _("Icon theme"), _("environment"), _("image"), _("theme"), _("xsettings")); //enum
   co->funcs[0].none = e_xsettings_config_update;
   co->info_cb = _e_configure_icon_theme_info_cb;
   co->thumb_cb = _e_configure_icon_theme_thumb_cb;
   co->event_type = E_EVENT_CONFIG_ICON_THEME;
   OPT_ICON("preferences-desktop-theme");
   OPT_ADD(BOOL, icon_theme_overrides, _("Icon theme overrides E17 internal theme icons"), _("environment"), _("image"), _("theme"), _("xsettings")); //
   co->funcs[1].none = co->funcs[0].none = e_xsettings_config_update;

   OPT_ADD(BOOL, exe_always_single_instance, _("Always launch applications as single-instance"), _("exec"));
   //OPT_ADD(INT, use_desktop_window_profile,

   e_user_dir_concat_static(buf, "themes");
   theme_ls[0] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
   theme_mon[0] = eio_monitor_add(buf);
   e_prefix_data_concat_static(buf, "data/themes");
   theme_ls[1] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
   theme_mon[1] = eio_monitor_add(buf);

   e_user_homedir_concat_static(buf, ".themes");
   gtk_theme_ls = eio_file_ls(buf, _gtk_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
   gtk_theme_mon = eio_monitor_add(buf);
   {
      const char *dir;
      const Eina_List *l;

      EINA_LIST_FOREACH(efreet_data_dirs_get(), l, dir)
        {
           Eio_File *ls;

           snprintf(buf, sizeof(buf), "%s/themes", dir);
           ls = eio_file_ls(buf, _gtk_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
           if (!ls) continue;
           gtk_theme_dirs = eina_list_append(gtk_theme_dirs, ls);
           gtk_theme_mons = eina_list_append(gtk_theme_mons, eio_monitor_add(buf));
        }
   }

   e_user_dir_concat_static(buf, "backgrounds");
   bg_ls[0] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
   bg_mon[0] = eio_monitor_add(buf);
   e_prefix_data_concat_static(buf, "data/backgrounds");
   bg_ls[1] = eio_file_ls(buf, _edj_filter_cb, _init_main_cb, _init_done_cb, _init_error_cb, NULL);
   bg_mon[1] = eio_monitor_add(buf);

   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_SELF_DELETED, _monitor_error, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_FILE_CREATED, _monitor_theme_rescan, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_FILE_DELETED, _monitor_theme_rescan, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_ERROR, _monitor_error, NULL);

   category_hash = eina_hash_string_superfast_new((Eina_Free_Cb)eina_list_free);
   category_icon_hash = eina_hash_string_superfast_new((Eina_Free_Cb)eina_stringshare_del);

   e_configure_option_category_tag_add(_("appearance"), _("wallpaper"));
   e_configure_option_category_tag_add(_("appearance"), _("splash"));
   e_configure_option_category_tag_add(_("appearance"), _("font"));
   e_configure_option_category_tag_add(_("appearance"), _("border"));
   e_configure_option_category_tag_add(_("appearance"), _("transition"));
   e_configure_option_category_tag_add(_("appearance"), _("scale"));
   e_configure_option_category_tag_add(_("appearance"), _("xsettings"));
   e_configure_option_category_icon_set(_("appearance"), "preferences-look");

   e_configure_option_category_tag_add(_("applications"), _("application"));
   e_configure_option_category_tag_add(_("applications"), _("files"));
   e_configure_option_category_tag_add(_("applications"), _("xsettings"));
   e_configure_option_category_icon_set(_("applications"), "preferences-applications");

   e_configure_option_category_tag_add(_("screen"), _("desklock"));
   e_configure_option_category_tag_add(_("screen"), _("screen"));
   e_configure_option_category_tag_add(_("screen"), _("vdesk"));
   e_configure_option_category_tag_add(_("screen"), _("desktop"));
   e_configure_option_category_tag_add(_("screen"), _("shelf"));
   e_configure_option_category_icon_set(_("screen"), "preferences-desktop-display");

   e_configure_option_category_tag_add(_("windows"), _("border"));
   e_configure_option_category_tag_add(_("windows"), _("focus"));
   e_configure_option_category_tag_add(_("windows"), _("winlist"));
   e_configure_option_category_tag_add(_("windows"), _("placement"));
   e_configure_option_category_tag_add(_("windows"), _("resist"));
   e_configure_option_category_tag_add(_("windows"), _("remember"));
   e_configure_option_category_tag_add(_("windows"), _("minimize"));
   e_configure_option_category_tag_add(_("windows"), _("kill"));
   e_configure_option_category_tag_add(_("windows"), _("transient"));
   e_configure_option_category_tag_add(_("windows"), _("move"));
   e_configure_option_category_tag_add(_("windows"), _("resize"));
   e_configure_option_category_icon_set(_("windows"), "preferences-system-windows");

   e_configure_option_category_tag_add(_("menus"), _("menu"));
   e_configure_option_category_icon_set(_("menus"), "preferences-menus");

   e_configure_option_category_tag_add(_("input"), _("mouse"));
   e_configure_option_category_tag_add(_("input"), _("pointer"));
   e_configure_option_category_tag_add(_("input"), _("key"));
   e_configure_option_category_tag_add(_("input"), _("binding"));
   e_configure_option_category_icon_set(_("input"), "preferences-behavior");

   e_configure_option_category_tag_add(_("advanced"), _("framerate"));
   e_configure_option_category_tag_add(_("advanced"), _("cache"));
   e_configure_option_category_tag_add(_("advanced"), _("priority"));
   e_configure_option_category_icon_set(_("advanced"), "preferences-advanced");

   e_configure_option_category_tag_add(_("settings"), _("dialog"));
   e_configure_option_category_tag_add(_("settings"), _("profile"));
   e_configure_option_category_tag_add(_("settings"), _("module"));
   e_configure_option_category_icon_set(_("settings"), "preferences-preferences");

   e_configure_option_tag_alias_add(_("border"), _("window"));
   e_configure_option_tag_alias_add(_("exec"), _("launch"));
   e_configure_option_tag_alias_add(_("image"), _("icon"));
   e_configure_option_tag_alias_add(_("theme"), _("style"));
   e_configure_option_tag_alias_add(_("pointer"), _("cursor"));
   e_configure_option_tag_alias_add(_("minimize"), _("iconif"));

   event_block = EINA_FALSE;

   return 1;
}

EINTERN int
e_configure_option_shutdown(void)
{
   opts_changed_list = eina_list_free(opts_changed_list);
   event_block = EINA_TRUE;
   E_FREE_LIST(tags_list, eina_stringshare_del);
   domain_current = NULL;
   E_FN_DEL(eina_hash_free, domain_hash);
   E_FN_DEL(eina_hash_free, tags_hash);
   E_FN_DEL(eina_hash_free, tags_tag_alias_hash);
   E_FN_DEL(eina_hash_free, tags_name_hash);
   E_FN_DEL(eina_hash_free, tags_alias_name_hash);
   E_FN_DEL(eina_hash_free, tags_alias_hash);
      E_FREE_LIST(tags_alias_list, eina_stringshare_del);
   E_FN_DEL(eio_monitor_del, theme_mon[0]);
   E_FN_DEL(eio_monitor_del, theme_mon[1]);
   E_FN_DEL(eio_file_cancel, theme_ls[0]);
   E_FN_DEL(eio_file_cancel, theme_ls[1]);
   E_FN_DEL(eio_file_cancel, gtk_theme_ls);
   E_FREE_LIST(gtk_theme_dirs, eio_file_cancel);
   E_FN_DEL(eio_monitor_del, gtk_theme_mon);
   E_FREE_LIST(gtk_theme_mons, eio_monitor_del);
   E_FREE_LIST(gtk_themes, eina_stringshare_del);
   E_FREE_LIST(themes, eina_stringshare_del);
   E_FREE_LIST(sthemes, eina_stringshare_del);
   E_FN_DEL(eio_monitor_del, bg_mon[0]);
   E_FN_DEL(eio_monitor_del, bg_mon[1]);
   E_FN_DEL(eio_file_cancel, bg_ls[0]);
   E_FN_DEL(eio_file_cancel, bg_ls[1]);
   E_FREE_LIST(bgs, eina_stringshare_del);
   E_FREE_LIST(sbgs, eina_stringshare_del);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FN_DEL(eina_hash_free, category_hash);
   E_FN_DEL(eina_hash_free, category_icon_hash);
   categories = eina_list_free(categories);
   return 1;
}

EAPI E_Configure_Option *
e_configure_option_add(E_Configure_Option_Type type, const char *desc, const char *name, void *valptr, const void *data)
{
   E_Configure_Option *co;
   Eina_Inlist *l;

   co = E_NEW(E_Configure_Option, 1);
   l = eina_hash_find(domain_hash, domain_current);
   eina_hash_set(domain_hash, domain_current, eina_inlist_append(l, EINA_INLIST_GET(co)));
   co->type = type;
   _e_configure_option_value_reset(co);
   co->name = eina_stringshare_add(name);
   co->desc = eina_stringshare_add(desc);
   co->valptr = valptr;
   co->data = (void *)data;
   _e_configure_option_event_changed(co);
   return co;
}

EAPI void
e_configure_option_tags_set(E_Configure_Option *co, const char **const tags, unsigned int num_tags)
{
   Eina_List *l, *ll;
   Eina_Stringshare *tag;
   unsigned int x;
   const char **t;

   EINA_SAFETY_ON_NULL_RETURN(co);
   EINA_LIST_FOREACH_SAFE(co->tags, l, ll, tag)
     _e_configure_option_tag_remove(co, tag);
   if (!tags) return;
   if (num_tags)
     {
        for (x = 0; x < num_tags; x++)
          _e_configure_option_tag_append(co, tags[x]);
     }
   else
     {
        for (t = tags; t && *t; t++)
          _e_configure_option_tag_append(co, *t);
     }
}

EAPI E_Configure_Option_Info *
e_configure_option_info_new(E_Configure_Option *co, const char *name, const void *value)
{
   E_Configure_Option_Info *oi;

   oi = E_NEW(E_Configure_Option_Info, 1);
   oi->co = co;
   oi->name = eina_stringshare_add(name);
   oi->value = (void *)value;
   return oi;
}

EAPI void
e_configure_option_info_free(E_Configure_Option_Info *oi)
{
   if (!oi) return;
   eina_stringshare_del(oi->name);
   eina_stringshare_del(oi->thumb_file);
   eina_stringshare_del(oi->thumb_key);
   free(oi);
}

EAPI Eina_List *
e_configure_option_info_get(E_Configure_Option *co)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(co, NULL);
   if (!co->info_cb) return NULL;
   return co->info_cb(co);
}

EAPI Evas_Object *
e_configure_option_info_thumb_get(E_Configure_Option_Info *oi, Evas *evas)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(oi, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(evas, NULL);
   if (!oi->co->thumb_cb) return NULL;
   return oi->co->thumb_cb(oi, evas);
}

EAPI void
e_configure_option_del(E_Configure_Option *co)
{
   Eina_List *l, *ll;
   Eina_Stringshare *tag;

   if (!co) return;
   if (!event_block)
     {
        _e_configure_option_event_del(co);
        return;
     }
   EINA_LIST_FOREACH_SAFE(co->tags, l, ll, tag)
     _e_configure_option_tag_remove(co, tag);
   _e_configure_option_free(co);
}

EAPI void *
e_configure_option_data_set(E_Configure_Option *co, const char *key, const void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(co, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(key, NULL);
   if ((!co->data) && (!data)) return NULL;
   if (!co->data) co->data = eina_hash_string_small_new(NULL);
   return eina_hash_set(co->data, key, data);
}

EAPI void *
e_configure_option_data_get(E_Configure_Option *co, const char *key)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(co, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(key, NULL);
   if (!co->data) return NULL;
   return eina_hash_find(co->data, key);
}

EAPI void
e_configure_option_changed(E_Configure_Option *co)
{
   Eina_Bool e_restart;
   unsigned char u;
   int i;
   unsigned int ui;
   double d;
   long long int l;
   Eina_Stringshare *s;
   Eina_Bool changed;

   EINA_SAFETY_ON_NULL_RETURN(co);
   changed = co->changed;
   co->changed = EINA_TRUE;
   if (!e_config->cfgdlg_auto_apply)
     {
        switch (co->type)
          {
           case E_CONFIGURE_OPTION_TYPE_BOOL:
             eina_value_get(&co->val, &u);
             if (*(unsigned char *)co->valptr != u) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
             eina_value_get(&co->val, &d);
             l = lround(d);
             u = MAX(0, l);
             if (*(unsigned char *)co->valptr != u) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT: //lround(double)
             eina_value_get(&co->val, &d);
             if (*(int *)co->valptr != lround(d)) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_INT:
           case E_CONFIGURE_OPTION_TYPE_ENUM:
             eina_value_get(&co->val, &i);
             if (*(int *)co->valptr != i) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT: //lround(double)
             eina_value_get(&co->val, &d);
             l = lround(d);
             ui = MAX(0, l);
             if (*(unsigned int *)co->valptr != ui) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_UINT:
             eina_value_get(&co->val, &ui);
             if (*(unsigned int *)co->valptr != ui) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_DOUBLE:
             eina_value_get(&co->val, &d);
             if (fabs(*(double *)co->valptr - d) > DBL_EPSILON) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_STR:
             eina_value_get(&co->val, &s);
             if (*(Eina_Stringshare **)co->valptr != s) break;
             co->changed = EINA_FALSE;
             break;

           case E_CONFIGURE_OPTION_TYPE_CUSTOM:
             break;
          }
        if (changed && (!co->changed))
          {
             _e_configure_option_value_reset(co);
             opts_changed_list = eina_list_remove(opts_changed_list, co);
          }
        else if ((!changed) && (co->changed))
          opts_changed_list = eina_list_append(opts_changed_list, co);
     }
   else
     {
        e_restart = _e_configure_option_apply(co, NULL, NULL, NULL);
        e_config_save_queue();
        if (e_restart) e_sys_action_do(E_SYS_RESTART, NULL);
     }
   _e_configure_option_event_changed(co);
}

EAPI void
e_configure_option_reset(E_Configure_Option *co)
{
   EINA_SAFETY_ON_NULL_RETURN(co);

   if (!co->changed) return;
   _e_configure_option_value_reset(co);
   opts_changed_list = eina_list_remove(opts_changed_list, co);
   co->changed = EINA_FALSE;
   _e_configure_option_event_changed(co);
}

EAPI void
e_configure_option_reset_all(void)
{
   E_Configure_Option *co;

   EINA_LIST_FREE(opts_changed_list, co)
     {
        _e_configure_option_value_reset(co);
        co->changed = EINA_FALSE;
        _e_configure_option_event_changed(co);
     }
}

EAPI void
e_configure_option_apply(E_Configure_Option *co)
{
   Eina_Bool e_restart;

   EINA_SAFETY_ON_NULL_RETURN(co);

   if (!co->changed) return;

   if (opts_changed_list)
     opts_changed_list = eina_list_remove(opts_changed_list, co);
   e_restart = _e_configure_option_apply(co, NULL, NULL, NULL);
   e_config_save_queue();
   if (e_restart) e_sys_action_do(E_SYS_RESTART, NULL);
}

EAPI void
e_configure_option_apply_all(void)
{
   E_Configure_Option *co;
   Eina_Bool e_restart = EINA_FALSE;
   Eina_List *events = NULL, *funcs = NULL, *acts = NULL;
   E_Action *act;
   void (*none)(void);
   intptr_t *event;

   EINA_LIST_FREE(opts_changed_list, co)
     e_restart |= _e_configure_option_apply(co, &events, &funcs, &acts);
   EINA_LIST_FREE(events, event)
     ecore_event_add((long)event, NULL, NULL, NULL);
   EINA_LIST_FREE(funcs, none)
     none();
   EINA_LIST_FREE(acts, act)
     act->func.go((E_Object *)e_util_container_current_get(), NULL);
   e_config_save_queue();
   if (e_restart) e_sys_action_do(E_SYS_RESTART, NULL);
}

EAPI const void *
e_configure_option_value_get(E_Configure_Option *co)
{
   static unsigned char u;
   static double d;
   static long l;
   static unsigned int i;
   static Eina_Stringshare *s;

   EINA_SAFETY_ON_NULL_RETURN_VAL(co, NULL);
   if (e_config->cfgdlg_auto_apply || (!co->changed)) return co->valptr;
   switch (co->type)
     {
      case E_CONFIGURE_OPTION_TYPE_BOOL:
        eina_value_get(&co->val, &u);
        return &u;

      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT: //lround(double)
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT: //lround(double)
      case E_CONFIGURE_OPTION_TYPE_ENUM: //lround(double)
      case E_CONFIGURE_OPTION_TYPE_DOUBLE:
        eina_value_get(&co->val, &d);
        return &d;

      case E_CONFIGURE_OPTION_TYPE_INT:
        eina_value_get(&co->val, &l);
        return &l;

      case E_CONFIGURE_OPTION_TYPE_UINT:
        eina_value_get(&co->val, &i);
        return &i;

      case E_CONFIGURE_OPTION_TYPE_STR:
        eina_value_get(&co->val, &s);
        return &s;

      case E_CONFIGURE_OPTION_TYPE_CUSTOM:
        break;
     }
   return NULL;
}

EAPI const Eina_List *
e_configure_option_tags_list(void)
{
   return tags_list;
}

EAPI const Eina_List *
e_configure_option_changed_list(void)
{
   return opts_changed_list;
}

EAPI const Eina_List *
e_configure_option_tag_list_options(const char *tag)
{
   if (!tags_hash) return NULL;
   return eina_hash_find(tags_hash, tag);
}

EAPI const Eina_List *
e_configure_option_category_list_tags(const char *cat)
{
   if (!categories) return NULL;
   return eina_hash_find(category_hash, cat);
}

EAPI const Eina_List *
e_configure_option_category_list(void)
{
   return categories;
}

EAPI void
e_configure_option_category_tag_add(const char *cat, const char *tag)
{
   Eina_Stringshare *t, *c;
   Eina_List *l;

   if (!tags_name_hash) return;
   EINA_SAFETY_ON_NULL_RETURN(cat);
   EINA_SAFETY_ON_NULL_RETURN(tag);
   t = eina_hash_find(tags_name_hash, tag);
   if (!t) return;
   if (!categories)
     {
        c = eina_stringshare_add(cat);
        eina_hash_add(category_hash, cat, eina_list_append(NULL, t));
        categories = eina_list_append(categories, c);
        _e_configure_option_event_category_add_del(c, EINA_FALSE);
        return;
     }
   l = eina_hash_find(category_hash, cat);
   if (l)
     {
        if (!eina_list_data_find(l, t))
          eina_hash_set(category_hash, cat, eina_list_append(l, t));
     }
   else
     {
        c = eina_stringshare_add(cat);
        categories = eina_list_append(categories, c);
        eina_hash_add(category_hash, cat, eina_list_append(NULL, t));
        _e_configure_option_event_category_add_del(c, EINA_FALSE);
     }
}

EAPI void
e_configure_option_category_tag_del(const char *cat, const char *tag)
{
   Eina_List *l;
   Eina_Stringshare *t, *c;

   if (!cat) return;
   if (!tag) return;
   if (!categories) return;
   if (!tags_list) return;

   l = eina_hash_find(category_hash, cat);
   if (!l) return;
   t = eina_hash_find(tags_name_hash, tag);
   l = eina_list_remove(l, t);
   eina_hash_set(category_hash, cat, l);
   if (l) return;
   c = eina_stringshare_add(cat);
   categories = eina_list_remove(categories, c);
   _e_configure_option_event_category_add_del(c, EINA_TRUE);
   eina_stringshare_del(c);
}

EAPI Eina_Stringshare *
e_configure_option_category_icon_get(const char *cat)
{
   if (!category_icon_hash) return NULL;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cat, NULL);
   return eina_hash_find(category_icon_hash, cat);
}

EAPI void
e_configure_option_category_icon_set(const char *cat, const char *icon)
{
   if (!category_icon_hash) return;
   EINA_SAFETY_ON_NULL_RETURN(cat);
   eina_stringshare_del(eina_hash_set(category_icon_hash, cat, eina_stringshare_add(icon)));
}

EAPI void
e_configure_option_tag_alias_add(const char *tag, const char *alias)
{
   Eina_Stringshare *t, *o;
   Eina_List *l;

   if (!tags_name_hash) return;
   EINA_SAFETY_ON_NULL_RETURN(tag);
   EINA_SAFETY_ON_NULL_RETURN(alias);

   t = eina_hash_find(tags_name_hash, tag);
   if (!t) return;
   o = eina_hash_set(tags_alias_hash, alias, t);
   if (o) return;  //alias already in list
   o = eina_stringshare_add(alias);
   tags_alias_list = eina_list_append(tags_alias_list, o);
   l = eina_hash_find(tags_tag_alias_hash, t);
   eina_hash_set(tags_tag_alias_hash, t, eina_list_append(l, o));
}

EAPI void
e_configure_option_tag_alias_del(const char *tag, const char *alias)
{
   Eina_Stringshare *t, *a;
   Eina_List *l;

   if (!tags_alias_hash) return;
   EINA_SAFETY_ON_NULL_RETURN(tag);
   EINA_SAFETY_ON_NULL_RETURN(alias);

   t = eina_hash_set(tags_alias_hash, alias, NULL);
   if (!t) return;  //alias doesn't exist
   a = eina_hash_find(tags_alias_name_hash, alias);
   tags_alias_list = eina_list_remove(tags_alias_list, a);
   l = eina_hash_find(tags_tag_alias_hash, t);
   eina_hash_set(tags_tag_alias_hash, t, eina_list_remove(l, a));
   eina_stringshare_del(a);
}

EAPI E_Configure_Option_Ctx *
e_configure_option_ctx_new(void)
{
   return E_NEW(E_Configure_Option_Ctx, 1);
}

EAPI Eina_Bool
e_configure_option_ctx_update(E_Configure_Option_Ctx *ctx, const char *str)
{
   Eina_List *l, *ll, *tlist, *tmp, *clist = NULL;
   Eina_Stringshare *tag, *alias;
   char *s, *e;

   if ((!str) || (!str[0]))
     {
        if ((!ctx->tags) && (ctx->opts)) return EINA_FALSE;
        ctx->tags = eina_list_free(ctx->tags);
        ctx->opts = eina_list_free(ctx->opts);
        return ctx->changed = EINA_TRUE;
     }
   tlist = eina_list_clone(tags_alias_list);
   for (s = e = strdupa(str); e[0]; e++)
     {
        if (isalnum(e[0])) continue;
        e[0] = 0;
        if (e - s > 1)
          {
             tmp = NULL;
             EINA_LIST_FOREACH_SAFE(tlist, l, ll, alias)
               {
                  if ((!strcasestr(s, alias)) && (!strcasestr(alias, s))) continue;
                  tag = eina_hash_find(tags_alias_hash, alias);
                  if (eina_list_data_find(clist, tag))
                    {
                       if (strncasecmp(s, alias, e - s)) continue;
                       tmp = eina_list_free(tmp);
                       break;
                    }
                  tlist = eina_list_remove(tlist, l);
                  if (strncasecmp(s, alias, e - s))
                    {
                       tmp = eina_list_append(tmp, tag);
                       continue;
                    }
                  tmp = eina_list_free(tmp);
                  tmp = eina_list_append(tmp, tag);
                  break;
               }
             if (tmp) clist = eina_list_merge(clist, tmp);
          }
        s = e + 1;
     }
   if (e - s > 1)
     {
        tmp = NULL;
        EINA_LIST_FOREACH_SAFE(tlist, l, ll, alias)
          {
             if ((!strcasestr(s, alias)) && (!strcasestr(alias, s))) continue;
             tag = eina_hash_find(tags_alias_hash, alias);
             if (eina_list_data_find(clist, tag))
               {
                  if (strncasecmp(s, alias, e - s)) continue;
                  tmp = eina_list_free(tmp);
                  break;
               }
             tlist = eina_list_remove(tlist, l);
             if (strncasecmp(s, alias, e - s))
               {
                  tmp = eina_list_append(tmp, tag);
                  continue;
               }
             tmp = eina_list_free(tmp);
             tmp = eina_list_append(tmp, tag);
             break;
          }
        if (tmp) clist = eina_list_merge(clist, tmp);
     }
   eina_list_free(tlist);
   tlist = eina_list_clone(tags_list);
   for (s = e = strdupa(str); e[0]; e++)
     {
        if (isalnum(e[0])) continue;
        e[0] = 0;
        if (e - s > 1)
          {
             tmp = NULL;
             EINA_LIST_FOREACH_SAFE(tlist, l, ll, tag)
               {
                  if ((!strcasestr(s, tag)) && (!strcasestr(tag, s))) continue;
                  if (eina_list_data_find(clist, tag))
                    {
                       if (strncasecmp(s, tag, e - s)) continue;
                       tmp = eina_list_free(tmp);
                       break;
                    }
                  tlist = eina_list_remove(tlist, l);
                  if (strncasecmp(s, tag, e - s))
                    {
                       tmp = eina_list_append(tmp, tag);
                       continue;
                    }
                  tmp = eina_list_free(tmp);
                  tmp = eina_list_append(tmp, tag);
                  break;
               }
             if (tmp) clist = eina_list_merge(clist, tmp);
          }
        s = e + 1;
     }
   if (e - s > 1)
     {
        tmp = NULL;
        EINA_LIST_FOREACH_SAFE(tlist, l, ll, tag)
          {
             if ((!strcasestr(s, tag)) && (!strcasestr(tag, s))) continue;
             if (eina_list_data_find(clist, tag))
               {
                  if (strncasecmp(s, tag, e - s)) continue;
                  tmp = eina_list_free(tmp);
                  break;
               }
             tlist = eina_list_remove(tlist, l);
             if (strncasecmp(s, tag, e - s))
               {
                  tmp = eina_list_append(tmp, tag);
                  continue;
               }
             tmp = eina_list_free(tmp);
             tmp = eina_list_append(tmp, tag);
             break;
          }
        if (tmp) clist = eina_list_merge(clist, tmp);
     }
   eina_list_free(tlist);
   if (eina_list_count(clist) != eina_list_count(ctx->tags))
     goto update;
   for (l = ctx->tags, ll = clist; l && ll; l = eina_list_next(l), ll = eina_list_next(ll))
     if (l->data != ll->data) goto update;

   eina_list_free(clist);
   return EINA_FALSE;
update:
   eina_list_free(ctx->tags);
   ctx->tags = clist;
   return ctx->changed = EINA_TRUE;
}

EAPI Eina_Bool
e_configure_option_ctx_tag_add(E_Configure_Option_Ctx *ctx, Eina_Stringshare *tag)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, EINA_FALSE);

   if (ctx->tags && eina_list_data_find(ctx->tags, tag)) return EINA_FALSE;
   ctx->tags = eina_list_append(ctx->tags, tag);
   return ctx->changed = EINA_TRUE;
}

EAPI Eina_Bool
e_configure_option_ctx_tag_pop(E_Configure_Option_Ctx *ctx)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);
   if (!ctx->tags) return EINA_FALSE;
   ctx->tags = eina_list_remove_list(ctx->tags, eina_list_last(ctx->tags));
   return ctx->changed = EINA_TRUE;
}

EAPI const Eina_List *
e_configure_option_ctx_match_tag_list(E_Configure_Option_Ctx *ctx)
{
   const Eina_List *l;
   E_Configure_Option *co;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   ctx->match_tags = eina_list_free(ctx->match_tags);
   e_configure_option_ctx_option_list(ctx);
   if (eina_list_count(ctx->opts) < 2) return NULL;
   EINA_LIST_FOREACH(e_configure_option_ctx_option_list(ctx), l, co)
     {
        Eina_List *ll;
        Eina_Stringshare *tag;

        EINA_LIST_FOREACH(co->tags, ll, tag)
          {
             if ((!ctx->match_tags) || (!eina_list_data_find(ctx->match_tags, tag)))
               ctx->match_tags = eina_list_append(ctx->match_tags, tag);
          }
     }
   return ctx->match_tags;
}

EAPI const Eina_List *
e_configure_option_ctx_option_list(E_Configure_Option_Ctx *ctx)
{
   Eina_List *l;
   E_Configure_Option *co;
   Eina_Stringshare *tag;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   if ((!ctx->changed) && (ctx->opts)) return ctx->opts;
   ctx->opts = eina_list_free(ctx->opts);
   EINA_LIST_FOREACH(ctx->tags, l, tag)
     {
        Eina_List *ll, *lll, *opts = NULL;

        ll = eina_hash_find(tags_hash, tag);
        if (!ll) continue;
        opts = eina_list_clone(ll);
        if (ctx->opts)
          {
             /* prune duplicates */
             EINA_LIST_FOREACH_SAFE(opts, ll, lll, co)
               {
                  if (eina_list_data_find_list(ctx->opts, co))
                    opts = eina_list_remove_list(opts, ll);
               }
          }
        if (eina_list_count(ctx->tags) > 1)
          /* reduce to matches */
          opts = _e_configure_option_ctx_list_prune(tag, opts, ctx->tags);
        if (opts)
          ctx->opts = eina_list_merge(ctx->opts, opts);
     }
   ctx->changed = EINA_FALSE;
   return ctx->opts;
}

EAPI void
e_configure_option_ctx_free(E_Configure_Option_Ctx *ctx)
{
   if (!ctx) return;
   eina_list_free(ctx->tags);
   eina_list_free(ctx->opts);
   eina_list_free(ctx->match_tags);
   free(ctx);
}

EAPI void
e_configure_option_domain_current_set(const char *domain)
{
   domain_current = domain;
}

EAPI Eina_Inlist *
e_configure_option_domain_list(const char *domain)
{
   if (!domain_hash) return NULL;
   EINA_SAFETY_ON_NULL_RETURN_VAL(domain, NULL);
   return eina_hash_find(domain_hash, domain);
}

EAPI void
e_configure_option_domain_clear(const char *domain)
{
   if (!domain_hash) return;
   EINA_SAFETY_ON_NULL_RETURN(domain);
   eina_hash_del_by_key(domain_hash, domain);
}

EAPI const Eina_List *
e_configure_option_util_themes_get(void)
{
   return themes;
}

EAPI const Eina_List *
e_configure_option_util_themes_system_get(void)
{
   return sthemes;
}

EAPI const Eina_List *
e_configure_option_util_themes_gtk_get(void)
{
   return gtk_themes;
}
