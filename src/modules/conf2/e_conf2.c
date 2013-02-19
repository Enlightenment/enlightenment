#include "e.h"

#include "e_mod_main.h"

static Elm_Genlist_Item_Class *itc_cats = NULL;
static Elm_Genlist_Item_Class *itc_tags = NULL;
static Elm_Genlist_Item_Class *itc_opts = NULL;
static Elm_Genlist_Item_Class *itc_thumb = NULL;
static E_Configure_Option_Ctx *ctx_entry = NULL;
static E_Configure_Option_Ctx *ctx_click = NULL;
static E_Configure_Option_Ctx *ctx_active = NULL;
static Evas_Object *list[2] = {NULL, NULL};
static Evas_Object *layout = NULL;
static Evas_Object *overlay = NULL;
static Evas_Object *back = NULL;
static Evas_Object *entry = NULL;
static Ecore_Timer *reset_timer = NULL;
static Eina_List *handlers = NULL;
static Eina_Bool overlay_locked = EINA_FALSE;

static Eina_Bool buttons_visible = EINA_TRUE;

static Eina_Bool _event_opt_changed(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Changed *ev);
static void _tag_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _opt_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info);

static void
_buttons_toggle(E_Configure_Option *co, Eina_Bool global)
{
   Eina_Bool show;

   if (global)
     show = !!e_configure_option_changed_list();
   else
     show = co->changed;
   if (show)
     {
        if (!buttons_visible)
          {
             elm_layout_signal_emit(layout, "e,action,apply_show", "e");
             elm_layout_signal_emit(layout, "e,action,discard_show", "e");
          }
        buttons_visible = EINA_TRUE;
     }
   else
     {
        if (buttons_visible)
          {
             elm_layout_signal_emit(layout, "e,action,apply_hide", "e");
             elm_layout_signal_emit(layout, "e,action,discard_hide", "e");
          }
        buttons_visible = EINA_FALSE;
     }
}

static double
_opt_overlay_value_update(E_Configure_Option *co, Evas_Object *obj)
{
   char buf[256];
   double d;
   Elm_Object_Item *it;
   Evas_Object *o;

   it = e_configure_option_data_get(co, "conf_item");
   if (overlay_locked)
     o = elm_object_part_content_get(overlay, "e.swallow.content");
   else
     o = elm_object_item_part_content_get(it, "elm.swallow.icon");
   d = elm_slider_value_get(obj);


   elm_slider_value_set(o, d);
   snprintf(buf, sizeof(buf), co->info, d);
   elm_object_part_text_set(overlay, "e.text.value", buf);
   return d;
}

static void
_opt_item_update(E_Configure_Option *co)
{
   Elm_Object_Item *it;
   Evas_Object *o, *o2 = NULL;
   void *val;

   it = e_configure_option_data_get(co, "conf_item");
   if (!it) return;
   o = elm_object_item_part_content_get(it, "elm.swallow.icon");
   if (overlay && (!overlay_locked) && (co == evas_object_data_get(overlay, "config_option")))
     o2 = elm_object_part_content_get(overlay, "e.swallow.content");
   val = (void*)e_configure_option_value_get(co);
   switch (co->type)
     {
      case E_CONFIGURE_OPTION_TYPE_BOOL:
        elm_check_state_pointer_set(o, val);
        elm_check_state_pointer_set(o, NULL);
        break;
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
        elm_slider_value_set(o, *(unsigned char*)val);
        if (o2)
          {
             elm_slider_value_set(o2, *(unsigned char*)val);
             _opt_overlay_value_update(co, o2);
          }
        break;
      case E_CONFIGURE_OPTION_TYPE_DOUBLE:
        elm_slider_value_set(o, *(double*)val);
        if (o2)
          {
             elm_slider_value_set(o2, *(double*)val);
             _opt_overlay_value_update(co, o2);
          }
        break;
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT:
        elm_slider_value_set(o, *(int*)val);
        if (o2)
          {
             elm_slider_value_set(o2, *(int*)val);
             _opt_overlay_value_update(co, o2);
          }
        break;
      case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT:
        elm_slider_value_set(o, *(unsigned int*)val);
        if (o2)
          {
             elm_slider_value_set(o2, *(unsigned int*)val);
             _opt_overlay_value_update(co, o2);
          }
        break;
      case E_CONFIGURE_OPTION_TYPE_ENUM:
        if (o2) evas_object_smart_callback_call(o2, "changed", NULL);
        break;
      case E_CONFIGURE_OPTION_TYPE_STR:
        if (o2) evas_object_smart_callback_call(o2, "activated", NULL);
        break;
      case E_CONFIGURE_OPTION_TYPE_INT:
      case E_CONFIGURE_OPTION_TYPE_UINT:
      case E_CONFIGURE_OPTION_TYPE_CUSTOM:
        break; //not applicable
     }

}

static Eina_Bool
_button_icon_set(Evas_Object *o, const char *name)
{
   Evas_Object *ob;

   ob = elm_icon_add(o);
   if (!elm_icon_standard_set(ob, name))
     {
        char buf[4096];
        const char *file;

        snprintf(buf, sizeof(buf), "e/icons/%s", name);
        file = e_theme_edje_icon_fallback_file_get(buf);
        if ((!file) || (!file[0])) return EINA_FALSE;
        if (!elm_image_file_set(ob, file, buf))
          {
             evas_object_del(ob);
             return EINA_FALSE;
          }
     }
   elm_object_content_set(o, ob);
   elm_object_style_set(o, "conf2");
   return EINA_TRUE;
}

static Evas_Object *
_e_conf2_opt_tooltip_cb(void *data, Evas_Object *owner EINA_UNUSED, Evas_Object *tt, void *item EINA_UNUSED)
{
   Evas_Object *o;

   /* FIXME: tooltip theme seems to be broken for sizing */
   //e = elm_layout_add(tt);
   //elm_layout_theme_set(e, "tooltip", "base", "conf2");
   o = elm_label_add(tt);
   elm_object_style_set(o, "conf2");
   elm_object_text_set(o, data);
   evas_object_show(o);
   //elm_object_part_content_set(e, "elm.swallow.content", o);
   return o;
}

static void
_e_conf2_opt_realize(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *it)
{
   E_Configure_Option *co;

   co = elm_object_item_data_get(it);
   if (!co->help) return;
   elm_object_item_tooltip_content_cb_set(it, _e_conf2_opt_tooltip_cb, co->help, NULL);
   elm_object_item_tooltip_style_set(it, "conf2");
   elm_object_item_tooltip_window_mode_set(it, EINA_TRUE);
}

static void
_e_conf2_opts_list(E_Configure_Option_Ctx *ctx)
{
   Eina_List *l;
   E_Configure_Option *co;
   Elm_Object_Item *it;

   elm_genlist_clear(list[1]);
   EINA_LIST_FOREACH(ctx->opts, l, co)
     {
        it = elm_genlist_item_append(list[1], itc_opts, co, NULL, 0, _opt_sel, co);
        e_configure_option_data_set(co, "conf_item", it);
     }
}

static void
_ctx_active_update(void)
{
   Eina_List *l;
   Eina_Stringshare *tag;

   ctx_active->tags = eina_list_free(ctx_active->tags);
   ctx_active->changed = EINA_TRUE;
   if (ctx_entry && ctx_entry->tags)
     ctx_active->tags = eina_list_clone(ctx_entry->tags);
   else
     {
        if (ctx_click && ctx_click->tags)
          ctx_active->tags = eina_list_clone(ctx_click->tags);
        return;
     }
   if ((!ctx_click) || (!ctx_click->tags)) return;
   EINA_LIST_FOREACH(ctx_click->tags, l, tag)
     e_configure_option_ctx_tag_add(ctx_active, tag);
}

static void
_tag_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   const Eina_List *l;
   Eina_Stringshare *tag;

   E_FREE_FUNC(reset_timer, ecore_timer_del);
   if (!ctx_click) ctx_click = e_configure_option_ctx_new();
   if (!e_configure_option_ctx_tag_add(ctx_click, data)) return;
   _ctx_active_update();
   e_configure_option_ctx_option_list(ctx_active);
   elm_object_disabled_set(back, 0);
   if (!elm_genlist_items_count(list[1]))
     elm_layout_signal_emit(layout, "e,action,expand", "e");
   _e_conf2_opts_list(ctx_active);
   elm_genlist_clear(list[0]);
   e_configure_option_ctx_match_tag_list(ctx_active);
   EINA_LIST_FOREACH(ctx_active->match_tags, l, tag)
     elm_genlist_item_append(list[0], itc_tags, tag, NULL, 0, _tag_sel, tag);
}

static void
_cat_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_Stringshare *tag;
   const Eina_List *l;
   Eina_List *ll, *lll, *opts = NULL;
   E_Configure_Option *co;

   elm_object_disabled_set(back, 0);
   if (!ctx_click) ctx_click = e_configure_option_ctx_new();
   ctx_click->category = ctx_active->category = data;
   E_FREE_FUNC(reset_timer, ecore_timer_del);
   elm_genlist_clear(list[0]);
   EINA_LIST_FOREACH(e_configure_option_category_list_tags(data), l, tag)
     {
        elm_genlist_item_append(list[0], itc_tags, tag, NULL, 0, _tag_sel, tag);
        opts = eina_list_merge(opts, eina_list_clone(e_configure_option_tag_list_options(tag)));
     }
   EINA_LIST_FOREACH_SAFE(opts, ll, lll, co)
     {
        Eina_List *lc;

        while (1)
          {
             if (!lll) break;
             lc = eina_list_data_find_list(lll, co);
             if (!lc) break;
             if (lc == lll) lll = eina_list_next(lll);
             opts = eina_list_remove_list(opts, lc);
          }
     }
   ctx_active->opts = opts;
   if (!elm_genlist_items_count(list[1]))
     elm_layout_signal_emit(layout, "e,action,expand", "e");
   _e_conf2_opts_list(ctx_active);
}

static Eina_Bool
_reset_cb(void *d EINA_UNUSED)
{
   const Eina_List *l;
   Eina_Stringshare *cat;

   elm_layout_signal_emit(layout, "e,action,collapse", "e");
   elm_genlist_clear(list[0]);
   elm_genlist_clear(list[1]);
   ctx_active->match_tags = eina_list_free(ctx_active->match_tags);
   ctx_active->tags = eina_list_free(ctx_active->tags);
   ctx_active->opts = eina_list_free(ctx_active->opts);
   E_FREE_FUNC(ctx_entry, e_configure_option_ctx_free);
   E_FREE_FUNC(ctx_click, e_configure_option_ctx_free);
   EINA_LIST_FOREACH(e_configure_option_category_list(), l, cat)
     elm_genlist_item_append(list[0], itc_cats, cat, NULL, 0, _cat_sel, cat);
   elm_object_disabled_set(back, 1);
   _buttons_toggle(NULL, EINA_TRUE);
   ctx_active->category = NULL;
   reset_timer = NULL;
   return EINA_FALSE;
}

static void
_entry_change_cb(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Eina_Stringshare *txt, *tag;
   Eina_List *l;

   E_FREE_FUNC(reset_timer, ecore_timer_del);
   txt = elm_entry_entry_get(obj);
   E_FREE_FUNC(overlay, evas_object_del);
   if (!ctx_entry) ctx_entry = e_configure_option_ctx_new();
   if (!e_configure_option_ctx_update(ctx_entry, txt)) return;
   _ctx_active_update();
   if (!e_configure_option_ctx_option_list(ctx_active))
     {
        if (!ctx_active->tags) _reset_cb(NULL);
        else elm_genlist_clear(list[1]);
        return;
     }
   if ((ctx_click && ctx_click->tags) || ctx_active->category)
     elm_object_disabled_set(back, 0);
   if (!elm_genlist_items_count(list[1]))
     elm_layout_signal_emit(layout, "e,action,expand", "e");
   _e_conf2_opts_list(ctx_active);
   elm_genlist_clear(list[0]);
   e_configure_option_ctx_match_tag_list(ctx_active);
   EINA_LIST_FOREACH(ctx_active->match_tags, l, tag)
     elm_genlist_item_append(list[0], itc_tags, tag, NULL, 0, _tag_sel, tag);
}

static void
_apply_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_configure_option_apply_all();
   _buttons_toggle(NULL, EINA_TRUE);
}

static void
_discard_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   /* buttons hidden in event callback */
   if (overlay)
     {
        E_Configure_Option *co;

        co = evas_object_data_get(overlay, "config_option");
        if (co)
          {
             e_configure_option_reset(co);
             return;
          }
        
     }
   e_configure_option_reset_all();
}

static Eina_Bool
_cat_back_cb(void *data EINA_UNUSED)
{
   ctx_active->tags = eina_list_free(ctx_active->tags);
   ctx_active->opts = eina_list_free(ctx_active->opts);
   _cat_sel((void*)ctx_active->category, NULL, NULL);
   reset_timer = NULL;
   return EINA_FALSE;
}

static void
_back_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Elm_Object_Item *it;
   const Eina_List *l;
   Eina_Stringshare *tag;
   Eina_Bool cat = EINA_FALSE;

   E_FREE_FUNC(reset_timer, ecore_timer_del);
   if (overlay)
     {
        evas_object_del(overlay);
        return;
     }
   if (!ctx_click) return;
   E_FREE_FUNC(overlay, evas_object_del);
   if (ctx_click->tags)
     {
        cat = EINA_TRUE;
        if (!e_configure_option_ctx_tag_pop(ctx_click)) return;
        _ctx_active_update();
     }
   if ((!ctx_click->tags) && (!ctx_active->tags))
     {
        if (cat)
          reset_timer = ecore_timer_add(0.1, _cat_back_cb, NULL);
        else
          reset_timer = ecore_timer_add(0.01, _reset_cb, NULL);
        return;
     }
   e_configure_option_ctx_option_list(ctx_active);
   _e_conf2_opts_list(ctx_active);
   e_configure_option_ctx_match_tag_list(ctx_active);
   EINA_LIST_FOREACH(ctx_active->match_tags, l, tag)
     elm_genlist_item_append(list[0], itc_tags, tag, NULL, 0, _tag_sel, tag);
   it = elm_genlist_selected_item_get(list[0]);
   if (it) elm_genlist_item_selected_set(it, EINA_FALSE);
}

static void
_e_conf2_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_FREE_FUNC(itc_cats, elm_genlist_item_class_free);
   E_FREE_FUNC(itc_tags, elm_genlist_item_class_free);
   E_FREE_FUNC(itc_opts, elm_genlist_item_class_free);
   E_FREE_FUNC(itc_thumb, elm_genlist_item_class_free);
   E_FREE_FUNC(ctx_entry, e_configure_option_ctx_free);
   E_FREE_FUNC(ctx_click, e_configure_option_ctx_free);
   E_FREE_FUNC(ctx_active, e_configure_option_ctx_free);
   E_FREE_FUNC(reset_timer, ecore_timer_del);
   list[0] = list[1] = layout = overlay = back = entry = NULL;
   E_FREE_LIST(handlers, ecore_event_handler_del);
   e_configure_option_reset_all();
   buttons_visible = EINA_TRUE;
   overlay_locked = EINA_FALSE;
}

static char *
_e_conf2_text_get_opts(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   E_Configure_Option *co = data;
   char *txt = NULL;

   if (!strcmp(part, "elm.text"))
     txt = strdup(co->desc);
   else if (!strcmp(part, "elm.text.sub"))
     txt = strdup(co->name);

   return txt;
}

static void
_e_conf2_opt_change_double_int(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;
   int x;

   x = lround(elm_slider_value_get(obj));

   if (e_config->cfgdlg_auto_apply)
     *(int*)co->valptr = x;
   else
     eina_value_set(&co->val, x);
   e_configure_option_changed(co);
}

static void
_e_conf2_opt_change_double(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;

   if (e_config->cfgdlg_auto_apply)
     *(double*)co->valptr = elm_slider_value_get(obj);
   else
     eina_value_set(&co->val, elm_slider_value_get(obj));
   e_configure_option_changed(co);
}

static void
_e_conf2_opt_change_double_uchar(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;
   int x;
   unsigned char u;

   x = lround(elm_slider_value_get(obj));
   u = MAX(0, x);

   if (e_config->cfgdlg_auto_apply)
     *(unsigned char*)co->valptr = u;
   else
     eina_value_set(&co->val, u);
   e_configure_option_changed(co);
}

static void
_e_conf2_opt_change_bool(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;

   if (e_config->cfgdlg_auto_apply)
     *(Eina_Bool*)co->valptr = elm_check_state_get(obj);
   else
     eina_value_set(&co->val, elm_check_state_get(obj));
   e_configure_option_changed(co);
}

static void
_opt_overlay_slider_change(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;

   switch (co->type)
     {
        case E_CONFIGURE_OPTION_TYPE_DOUBLE:
          _opt_overlay_value_update(co, obj);
          _e_conf2_opt_change_double(co, obj, NULL);
          break;
        case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT:
          _opt_overlay_value_update(co, obj);
          _e_conf2_opt_change_double_int(co, obj, NULL);
          break;
        case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
          _opt_overlay_value_update(co, obj);
          _e_conf2_opt_change_double_uchar(co, obj, NULL);
        default:
          break;
     }
}

static void
_opt_overlay_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Elm_Object_Item *it;
   E_Event_Configure_Option_Changed ev;

   if (!e_configure_option_tags_list()) return;
   overlay = NULL;
   it = elm_genlist_selected_item_get(list[1]);
   if (it) elm_genlist_item_selected_set(it, EINA_FALSE);
   elm_layout_signal_emit(layout, "e,action,overlay_hide", "e");
   elm_object_disabled_set(list[0], EINA_FALSE);
   elm_object_disabled_set(list[1], EINA_FALSE);
   elm_object_focus_allow_set(entry, EINA_TRUE);
   elm_object_focus_allow_set(list[0], EINA_TRUE);
   elm_object_focus_allow_set(list[1], EINA_TRUE);
   elm_object_focus_set(list[1], EINA_TRUE);
   ev.co = evas_object_data_get(obj, "config_option");
   _event_opt_changed(NULL, E_EVENT_CONFIGURE_OPTION_CHANGED, &ev);
}

static void
_opt_overlay_list_change(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Configure_Option_Info *oi = data;

   elm_object_part_text_set(overlay, "e.text.value", oi->name);
   if (oi->co->type == E_CONFIGURE_OPTION_TYPE_STR)
     {
        if (e_config->cfgdlg_auto_apply)
          eina_stringshare_replace(oi->co->valptr, oi->value);
        else
          eina_value_set(&oi->co->val, oi->value);
     }
   else
     {
        if (e_config->cfgdlg_auto_apply)
          *(int*)oi->co->valptr = *(int*)oi->value;
        else
          eina_value_set(&oi->co->val, *(int*)oi->value);
     }

   e_configure_option_changed(oi->co);
}

static void
_opt_overlay_dismiss(void *d EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *source EINA_UNUSED)
{
   evas_object_del(overlay);
}

static void
_opt_overlay_create(void)
{
   Evas_Object *o;

   overlay = o = elm_layout_add(layout);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _opt_overlay_del, NULL);
   elm_layout_theme_set(o, "conf2", "option", "overlay");
}

static void
_e_conf2_item_del_thumb(void *data, Evas_Object *obj EINA_UNUSED)
{
   E_Configure_Option_Info *oi = data;

   eina_stringshare_del(oi->value);
   e_configure_option_info_free(oi);
}

static Evas_Object *
_e_conf2_content_get_thumb(void *data, Evas_Object *obj, const char *part EINA_UNUSED)
{
   E_Configure_Option_Info *oi = data;
   Evas_Object *o;
   Evas *e;
   E_Zone *zone;

   e = evas_object_evas_get(obj);
   o = e_configure_option_info_thumb_get(oi, e);
   if (o) return o;

   if (!oi->thumb_file) return NULL;
   zone = e_util_zone_current_get(e_manager_current_get());
   /* viva la revolucion de livethumb!!! */
   o = e_widget_preview_add(e, 170, (170 * zone->h) / zone->w);
   if (oi->thumb_key)
     e_widget_preview_edje_set(o, oi->thumb_file, oi->thumb_key);
   else
     e_widget_preview_file_set(o, oi->thumb_file, oi->thumb_key);

/* I guess elm_image sucks, this looks like shit :/
   o = elm_image_add(obj);
   elm_image_file_set(o, oi->thumb_file, oi->thumb_key);
   if (elm_image_animated_available_get(o))
     {
        elm_image_animated_set(o, EINA_TRUE);
        elm_image_animated_play_set(o, EINA_TRUE);
     }
   elm_image_aspect_fixed_set(o, EINA_TRUE);
   elm_image_prescale_set(o, 300);
*/
   return o;
}

static char *
_e_conf2_text_get_thumb(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   E_Configure_Option_Info *oi = data;

   return oi->name ? strdup(oi->name) : NULL;
}

static void
_opt_overlay_entry_change(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Configure_Option *co = data;
   Eina_Stringshare *str;

   str = elm_entry_entry_get(obj);
   if (str == *(Eina_Stringshare**)co->valptr) return;
   if (e_config->cfgdlg_auto_apply)
     eina_stringshare_replace(co->valptr, str);
   else
     eina_value_set(&co->val, str);
   e_configure_option_changed(co);
}

static void
_opt_overlay_radio_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_configure_option_info_free(data);
}

static void
_opt_overlay_radio_change(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Configure_Option_Info *oi = data;

   elm_object_part_text_set(overlay, "e.text.value", oi->name);
   if (e_config->cfgdlg_auto_apply)
     *(int*)oi->co->valptr = (long)(intptr_t)oi->value;
   else
     eina_value_set(&oi->co->val, (long)(intptr_t)oi->value);

   e_configure_option_changed(oi->co);
}

static void
_opt_overlay_show(E_Configure_Option *co, Elm_Object_Item *it)
{
   Evas_Object *o, *sl;
   Eina_Bool global = EINA_FALSE, show = EINA_TRUE;
   Eina_List *l;
   double d;
   char buf[256];

   if (!it) it = e_configure_option_data_get(co, "conf_item");
   switch (co->type)
     {
        case E_CONFIGURE_OPTION_TYPE_CUSTOM:
          EINA_SAFETY_ON_NULL_RETURN(co->info);
          e_configure_registry_call(co->info, NULL, NULL);
          if (it) elm_genlist_item_selected_set(it, EINA_FALSE);
          break;
        case E_CONFIGURE_OPTION_TYPE_ENUM:
          if (!overlay) _opt_overlay_create();
          else show = EINA_FALSE;
          o = overlay;
          evas_object_data_set(overlay, "config_option", co);
          elm_object_part_text_set(o, "e.text.label", co->desc);
          l = e_configure_option_info_get(co);
          if (l)
            {
               E_Configure_Option_Info *oi;
               Evas_Object *bx, *g, *r;

               sl = elm_scroller_add(overlay);
               EXPAND(sl);
               FILL(sl);
               elm_scroller_bounce_set(sl, 0, 0);
               elm_scroller_policy_set(sl, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_AUTO);
               bx = elm_box_add(o);
               EXPAND(bx);
               FILL(bx);
               elm_object_content_set(sl, bx);
               elm_box_homogeneous_set(bx, 1);
               g = elm_radio_add(overlay);
               elm_object_focus_allow_set(list[0], EINA_FALSE);
               elm_object_focus_allow_set(list[1], EINA_FALSE);
               elm_object_focus_allow_set(entry, EINA_FALSE);
               EINA_LIST_FREE(l, oi)
                 {
                    if (!oi->name) continue; //FIXME sep
                    r = elm_radio_add(overlay);
                    elm_object_style_set(r, "conf2");
                    EXPAND(r);
                    FILL(r);
                    elm_radio_group_add(r, g);
                    elm_object_text_set(r, oi->name);
                    elm_radio_state_value_set(r, (long)(intptr_t)oi->value);
                    evas_object_event_callback_add(r, EVAS_CALLBACK_DEL, _opt_overlay_radio_del, oi);
                    evas_object_smart_callback_add(r, "changed", _opt_overlay_radio_change, oi);
                    if (oi->current)
                      {
                         elm_radio_value_set(g, (long)(intptr_t)oi->value);
                         elm_object_part_text_set(overlay, "e.text.value", oi->name);
                      }
                    evas_object_show(r);
                    elm_object_focus_set(r, EINA_TRUE);
                    elm_box_pack_end(bx, r);
                 }
               evas_object_show(bx);
            }
          else
            {
               /* FIXME? */
               evas_object_del(overlay);
               return;
            }
          elm_object_part_content_set(o, "e.swallow.content", sl);
          evas_object_show(sl);
          evas_object_show(o);
          elm_object_part_content_set(layout, "e.swallow.overlay", o);
          if (show)
            elm_layout_signal_emit(layout, "e,action,overlay_show", "e");
          elm_object_disabled_set(list[0], EINA_TRUE);
          elm_object_disabled_set(list[1], EINA_TRUE);
          break;
        case E_CONFIGURE_OPTION_TYPE_STR:
          if (!overlay) _opt_overlay_create();
          else show = EINA_FALSE;
          o = overlay;
          evas_object_data_set(overlay, "config_option", co);
          elm_object_part_text_set(o, "e.text.label", co->desc);
          l = e_configure_option_info_get(co);
          if (l)
            {
               E_Configure_Option_Info *oi;
               Elm_Object_Item *iit;

               sl = elm_genlist_add(overlay);
               elm_genlist_homogeneous_set(sl, EINA_TRUE);
               elm_scroller_bounce_set(sl, 0, 0);
               elm_scroller_policy_set(sl, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
               EINA_LIST_FREE(l, oi)
                 {
                    if (!oi->name) continue; //FIXME sep
                    iit = elm_genlist_item_append(sl, itc_thumb, oi, NULL, 0, _opt_overlay_list_change, oi);
                    elm_genlist_item_selected_set(iit, oi->current);
                    if (oi->current) elm_genlist_item_show(iit, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
                 }
            }
          else
            {
               Eina_Stringshare **s;

               sl = elm_entry_add(overlay);
               s = (Eina_Stringshare**)e_configure_option_value_get(co);
               if (s)
                 {
                    elm_entry_entry_set(sl, *s);
                    elm_entry_select_all(sl);
                 }
               elm_entry_single_line_set(sl, EINA_TRUE);
               elm_entry_scrollable_set(sl, EINA_TRUE);
               elm_entry_cursor_begin_set(sl);
               evas_object_smart_callback_add(sl, "activated", _opt_overlay_entry_change, co);
               edje_extern_object_min_size_set(sl, 170, 20);
               elm_object_part_text_set(overlay, "e.text.value", NULL);
            }
          elm_object_part_content_set(o, "e.swallow.content", sl);
          evas_object_show(sl);
          elm_object_focus_allow_set(list[0], EINA_FALSE);
          elm_object_focus_allow_set(list[1], EINA_FALSE);
          elm_object_focus_allow_set(entry, EINA_FALSE);
          elm_object_focus_set(sl, EINA_TRUE);
          evas_object_show(o);
          elm_object_part_content_set(layout, "e.swallow.overlay", o);
          if (show)
            elm_layout_signal_emit(layout, "e,action,overlay_show", "e");
          elm_object_disabled_set(list[0], EINA_TRUE);
          elm_object_disabled_set(list[1], EINA_TRUE);
          break;
        case E_CONFIGURE_OPTION_TYPE_DOUBLE:
        case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT:
        case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT:
        case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
          sl = elm_object_item_part_content_get(it, "elm.swallow.icon");
          if (!overlay) _opt_overlay_create();
          else show = EINA_FALSE;
          o = overlay;
          evas_object_data_set(overlay, "config_option", co);
          elm_layout_signal_emit(overlay, "e,state,bg_hide", "e");
          elm_object_part_text_set(o, "e.text.label", co->desc);
          d = elm_slider_value_get(sl);
          snprintf(buf, sizeof(buf), co->info, d);
          elm_object_part_text_set(overlay, "e.text.value", buf);
          sl = elm_slider_add(layout);
          elm_slider_min_max_set(sl, co->minmax[0], co->minmax[1]);
          /* raoulh! :) */
          elm_slider_value_set(sl, d);
          elm_object_style_set(sl, "knob_volume_big");
          evas_object_smart_callback_add(sl, "changed", _opt_overlay_slider_change, co);
          evas_object_show(sl);
          elm_object_part_content_set(o, "e.swallow.content", sl);
          evas_object_show(o);
          elm_object_part_content_set(layout, "e.swallow.overlay", o);
          if (show)
            elm_layout_signal_emit(layout, "e,action,overlay_show", "e");
          elm_object_disabled_set(list[0], EINA_TRUE);
          elm_object_disabled_set(list[1], EINA_TRUE);
          break;
        default:
          if (overlay) evas_object_del(overlay);
          global = EINA_TRUE;
     }
   _buttons_toggle(co, global);
}

static void
_opt_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   _opt_overlay_show(data, event_info);
}

static void
_e_conf2_slider_drag_start(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   overlay_locked = EINA_TRUE;
   _opt_overlay_show(data, NULL);
}

static void
_e_conf2_slider_drag_stop(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_del(overlay);
   overlay_locked = EINA_FALSE;
}

static Evas_Object *
_e_conf2_content_get_opts_slider_helper(E_Configure_Option *co)
{
   Evas_Object *o;

   o = elm_slider_add(layout);
   /* raoulh! :) */
   elm_object_style_set(o, "knob_volume");
   elm_slider_min_max_set(o, co->minmax[0], co->minmax[1]);
   evas_object_smart_callback_add(o, "changed", _opt_overlay_slider_change, co);
   evas_object_smart_callback_add(o, "slider,drag,start", _e_conf2_slider_drag_start, co);
   evas_object_smart_callback_add(o, "slider,drag,stop", _e_conf2_slider_drag_stop, co);
   return o;
}

static Evas_Object *
_e_conf2_content_get_opts(void *data, Evas_Object *obj, const char *part EINA_UNUSED)
{
   E_Configure_Option *co = data;
   Evas_Object *o = NULL;
   const void *val;
   //char buf[PATH_MAX];

   if (!strcmp(part, "elm.swallow.icon"))
     {
        switch (co->type)
          {
           case E_CONFIGURE_OPTION_TYPE_BOOL:
             o = elm_check_add(obj);
             elm_object_style_set(o, "conf2");
             val = e_configure_option_value_get(co);
             elm_check_state_set(o, *(Eina_Bool*)val);
             evas_object_smart_callback_add(o, "changed", _e_conf2_opt_change_bool, co);
             break;
           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR:
             o = _e_conf2_content_get_opts_slider_helper(co);
             val = e_configure_option_value_get(co);
             elm_slider_value_set(o, *(unsigned char*)val);
             break;
           case E_CONFIGURE_OPTION_TYPE_DOUBLE:
             o = _e_conf2_content_get_opts_slider_helper(co);
             val = e_configure_option_value_get(co);
             elm_slider_value_set(o, *(double*)val);
             break;
           case E_CONFIGURE_OPTION_TYPE_DOUBLE_INT:
             o = _e_conf2_content_get_opts_slider_helper(co);
             val = e_configure_option_value_get(co);
             elm_slider_value_set(o, *(int*)val);
             break;
           case E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT:
             o = _e_conf2_content_get_opts_slider_helper(co);
             val = e_configure_option_value_get(co);
             elm_slider_value_set(o, *(unsigned int*)val);
             break;
           case E_CONFIGURE_OPTION_TYPE_CUSTOM:
           case E_CONFIGURE_OPTION_TYPE_STR:
           case E_CONFIGURE_OPTION_TYPE_ENUM:
           case E_CONFIGURE_OPTION_TYPE_INT:
           case E_CONFIGURE_OPTION_TYPE_UINT:
             {
                Eina_Stringshare *icon;
                char buf[4096];

                icon = e_configure_option_data_get(co, "icon");
                if (!icon) break;
                if (eina_str_has_extension(icon, ".edj"))
                  o = e_util_icon_add(icon, evas_object_evas_get(obj));
                else
                  {
                     snprintf(buf, sizeof(buf), "e/icons/%s", icon);

                     o = e_icon_add(evas_object_evas_get(obj));
                     e_util_icon_theme_set(o, icon);
                  }
             }
             break;
          }
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        if (!co->changed) return NULL;
        o = edje_object_add(evas_object_evas_get(obj));
        e_theme_edje_object_set(o, "base/theme/widgets", "e/widgets/ilist/toggle_end");
     }
   return o;
}

static void
_e_conf2_item_del_opts(void *data, Evas_Object *obj EINA_UNUSED)
{
   if (e_configure_option_tags_list()) //if this is false, e is shutting down and the next call will crash
     e_configure_option_data_set(data, "conf_item", NULL);
}

static char *
_e_conf2_text_get_tags(void *data, Evas_Object *obj EINA_UNUSED, const char *part)
{
   char *txt;

   if (strcmp(part, "elm.text")) return NULL;
   txt = strdup(data);
   txt[0] = toupper(txt[0]);
   return txt;
}

static Evas_Object *
_e_conf2_content_get_cats(void *data, Evas_Object *obj, const char *part)
{
   char buf[4096];
   Eina_Stringshare *icon;
   Evas_Object *o;

   if (!strcmp(part, "elm.swallow.end")) return NULL;
   icon = e_configure_option_category_icon_get(data);
   if (!icon) return NULL;
   if (eina_str_has_extension(icon, ".edj"))
     o = e_util_icon_add(icon, evas_object_evas_get(obj));
   else
     {
        snprintf(buf, sizeof(buf), "e/icons/%s", icon);

        o = e_icon_add(evas_object_evas_get(obj));
        e_util_icon_theme_set(o, icon);
     }
   return o;
}

static Evas_Object *
_e_conf2_content_get_tags(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   /* FIXME */
   return NULL;
}

static void
_e_conf2_key(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   if (overlay) evas_object_del(overlay);
   else evas_object_del(obj);
}

static Eina_Bool
_event_tag_del(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Tag_Del *ev EINA_UNUSED)
{
   /* not even gonna fuck with this */
   _reset_cb(NULL);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_tag_add(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Tag_Add *ev)
{
   Eina_List *l;
   Eina_Stringshare *tag;

   e_configure_option_ctx_match_tag_list(ctx_active);
   if ((!ctx_active->match_tags) || (!eina_list_data_find(ctx_active->match_tags, ev->tag)))
     return ECORE_CALLBACK_RENEW;
   elm_genlist_clear(list[0]);
   EINA_LIST_FOREACH(ctx_active->match_tags, l, tag)
     elm_genlist_item_append(list[0], itc_tags, tag, NULL, 0, _tag_sel, tag);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_cat_del(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Category_Add *ev)
{
   Elm_Object_Item *it;
   Eina_Stringshare *cat;

   if (ctx_active->category == ev->category)
     {
        _reset_cb(NULL);
        return ECORE_CALLBACK_RENEW;
     }
   if (ctx_active->tags || ctx_active->opts) return ECORE_CALLBACK_RENEW; //not showing categories at present
   //new categories are added at end, so go backward
   for (it = elm_genlist_last_item_get(list[0]); it; it = elm_genlist_item_prev_get(it))
     {
        cat = elm_object_item_data_get(it);
        if (cat != ev->category) continue;
        elm_object_item_del(it);
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_cat_add(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Category_Add *ev)
{
   if (ctx_active->category || ctx_active->tags || ctx_active->opts) return ECORE_CALLBACK_RENEW; //not showing categories at present
   elm_genlist_item_append(list[0], itc_cats, ev->category, NULL, 0, _cat_sel, ev->category);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_opt_del(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Del *ev)
{
   Elm_Object_Item *it;
   Eina_List *l;

   if (!ctx_active->opts) return ECORE_CALLBACK_RENEW;
   l = eina_list_data_find(ctx_active->opts, ev->co);
   if (!l) return ECORE_CALLBACK_RENEW;
   ctx_active->opts = eina_list_remove_list(ctx_active->opts, l);
   
   if (ctx_click && ctx_click->opts)
     ctx_click->opts = eina_list_remove(ctx_click->opts, ev->co);
   if (ctx_entry && ctx_entry->opts)
     ctx_entry->opts = eina_list_remove(ctx_entry->opts, ev->co);

   for (it = elm_genlist_last_item_get(list[1]); it; it = elm_genlist_item_prev_get(it))
     {
        if (ev->co != elm_object_item_data_get(it)) continue;
        elm_object_item_del(it);
        break;
     }
   
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_opt_add(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Add *ev)
{
   Eina_List *l;
   Eina_Stringshare *tag;

   if (!ctx_active->opts) return ECORE_CALLBACK_RENEW;
   /* FIXME: this will only add the item if we are NOT directly viewing a category */
   if (!ctx_active->tags) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(ev->co->tags, l, tag)
     if (eina_list_data_find(ctx_active->tags, tag))
       {
          ctx_active->changed = EINA_TRUE;
          break;
       }
   if (!ctx_active->changed) return ECORE_CALLBACK_RENEW;
   e_configure_option_ctx_option_list(ctx_active);
   _e_conf2_opts_list(ctx_active);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_event_opt_changed(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Configure_Option_Changed *ev)
{
   E_Configure_Option *co;
   Elm_Object_Item *it;

   it = e_configure_option_data_get(ev->co, "conf_item");
   if (it)
     elm_genlist_item_fields_update(it, "elm.swallow.end", ELM_GENLIST_ITEM_FIELD_CONTENT);
   if (overlay)
     {
        co = evas_object_data_get(overlay, "config_option");
        if (ev->co != co) return ECORE_CALLBACK_RENEW;
        _buttons_toggle(co, EINA_FALSE);
        if (!co->changed) _opt_item_update(co);
        return ECORE_CALLBACK_RENEW;
     }
   _buttons_toggle(NULL, EINA_TRUE);
   if (buttons_visible) return ECORE_CALLBACK_RENEW;
   _opt_item_update(ev->co);
   return ECORE_CALLBACK_RENEW;
}

static void
_theme_change(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _reset_cb(NULL);
   _entry_change_cb(NULL, entry, NULL);
}

EINTERN void
e_conf2_show(E_Container *con EINA_UNUSED, const char *params)
{
   Evas_Object *box, *box2, *o, *win;

   if (list[0])
     {
        elm_win_activate(elm_object_top_widget_get(list[0]));
        return;
     }

   win = elm_win_add(NULL, "conf2", ELM_WIN_DIALOG_BASIC);
   elm_win_title_set(win, _("Control Panel"));
   o = elm_bg_add(win);
   elm_object_style_set(o, "conf2");
   EXPAND(o);
   elm_win_resize_object_add(win, o);
   evas_object_show(o);
   1 | evas_object_key_grab(win, "Escape", 0, 0, 1);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _e_conf2_key, NULL);
   /* FIXME this is insane */
   ecore_evas_name_class_set(ecore_evas_ecore_evas_get(evas_object_evas_get(win)), "conf2", "_configure");
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _e_conf2_del, NULL);
   box = elm_box_add(win);
   EXPAND(box);
   FILL(box);
   elm_win_resize_object_add(win, box);
   evas_object_show(box);

   box2 = elm_box_add(win);
   elm_box_horizontal_set(box2, EINA_TRUE);
   WEIGHT(box2, EVAS_HINT_EXPAND, 0);
   ALIGN(box2, EVAS_HINT_FILL, 0);
   elm_box_pack_end(box, box2);
   evas_object_show(box2);

   back = o = elm_button_add(win);
   elm_object_focus_allow_set(back, EINA_FALSE);
   evas_object_smart_callback_add(o, "clicked", _back_cb, NULL);
   //elm_object_text_set(o, _("Back"));
   if (!_button_icon_set(o, "back"))
     _button_icon_set(o, "go-previous");
   WEIGHT(o, 0, 0);
   FILL(o);
   elm_object_disabled_set(o, (params && params[0]));
   elm_box_pack_end(box2, o);
   evas_object_show(o);

   o = elm_label_add(win);
   elm_object_style_set(o, "conf2");
   elm_object_text_set(o, _("Search: "));
   WEIGHT(o, 0, EVAS_HINT_EXPAND);
   ALIGN(o, 0, EVAS_HINT_FILL);
   elm_box_pack_end(box2, o);
   evas_object_show(o);

   entry = o = elm_entry_add(win);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_object_style_set(o, "conf2");
   if (params && params[0])
     {
        elm_entry_entry_set(o, params);
        _entry_change_cb(NULL, o, NULL);
     }
   else
     reset_timer = ecore_timer_add(0.01, _reset_cb, NULL);
   EXPAND(o);
   FILL(o);
   evas_object_smart_callback_add(o, "changed", _entry_change_cb, NULL);
   evas_object_smart_callback_add(o, "activated", _entry_change_cb, NULL);
   elm_box_pack_end(box2, o);
   evas_object_show(o);
   elm_object_focus_set(o, EINA_TRUE);

   o = layout = elm_layout_add(win);
   EXPAND(o);
   FILL(o);
   elm_layout_freeze(o);
   elm_layout_theme_set(o, "conf2", "tag", "list");
   evas_object_smart_callback_add(o, "theme,changed", _theme_change, NULL);
   elm_layout_signal_callback_add(layout, "e,action,overlay_dismiss", "e", _opt_overlay_dismiss, NULL);

   o = list[0] = elm_genlist_add(win);
   elm_genlist_homogeneous_set(o, EINA_TRUE);
   elm_genlist_mode_set(o, ELM_LIST_COMPRESS);
   EXPAND(o);
   FILL(o);

   itc_cats = elm_genlist_item_class_new();
   itc_cats->item_style     = "conf2_double_label";
   itc_cats->func.text_get = _e_conf2_text_get_tags;
   itc_cats->func.content_get  = _e_conf2_content_get_cats;
   itc_cats->func.state_get = NULL;
   itc_cats->func.del       = NULL;

   itc_tags = elm_genlist_item_class_new();
   itc_tags->item_style     = "conf2_tag";
   itc_tags->func.text_get = _e_conf2_text_get_tags;
   itc_tags->func.content_get  = _e_conf2_content_get_tags;
   itc_tags->func.state_get = NULL;
   itc_tags->func.del       = NULL;
   elm_object_part_content_set(layout, "e.swallow.tags", o);
   evas_object_show(o);

   o = list[1] = elm_genlist_add(win);
   evas_object_smart_callback_add(o, "realized", _e_conf2_opt_realize, NULL);
   elm_genlist_homogeneous_set(o, EINA_TRUE);
   EXPAND(o);
   FILL(o);
   itc_opts = elm_genlist_item_class_new();
   itc_opts->item_style     = "conf2_double_label";
   itc_opts->func.text_get = _e_conf2_text_get_opts;
   itc_opts->func.content_get  = _e_conf2_content_get_opts;
   itc_opts->func.state_get = NULL;
   itc_opts->func.del  = _e_conf2_item_del_opts;
   elm_object_part_content_set(layout, "e.swallow.options", o);
   evas_object_show(o);

   itc_thumb = elm_genlist_item_class_new();
   itc_thumb->item_style     = "conf2_thumb";
   itc_thumb->func.text_get = _e_conf2_text_get_thumb;
   itc_thumb->func.content_get  = _e_conf2_content_get_thumb;
   itc_thumb->func.state_get = NULL;
   itc_thumb->func.del  = _e_conf2_item_del_thumb;

   o = elm_button_add(win);
   evas_object_smart_callback_add(o, "clicked", _discard_cb, NULL);
   elm_object_text_set(o, _("Discard"));
   if (!_button_icon_set(o, "dialog-cancel"))
     _button_icon_set(o, "view-refresh");
   WEIGHT(o, 0, 0);
   FILL(o);
   elm_object_part_content_set(layout, "e.swallow.discard", o);
   evas_object_show(o);

   o = elm_button_add(win);
   evas_object_smart_callback_add(o, "clicked", _apply_cb, NULL);
   elm_object_text_set(o, _("Apply"));
   _button_icon_set(o, "dialog-ok-apply");
   WEIGHT(o, 0, 0);
   FILL(o);
   elm_object_part_content_set(layout, "e.swallow.apply", o);
   evas_object_show(o);

   elm_layout_thaw(layout);
   evas_object_show(layout);
   elm_box_pack_end(box, layout);

   evas_object_show(win);
   {
      E_Border *bd;

      bd = evas_object_data_get(win, "E_Border");
      if (bd)
        {
           bd->client.icccm.min_w = 360;
           bd->client.icccm.min_h = 360;
        }
   }
   elm_win_size_base_set(win, 360, 360);
   evas_object_resize(win, 480, 480);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_CHANGED, _event_opt_changed, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_ADD, _event_opt_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_DEL, _event_opt_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_CATEGORY_ADD, _event_cat_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_CATEGORY_DEL, _event_cat_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_TAG_ADD, _event_tag_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIGURE_OPTION_TAG_DEL, _event_tag_del, NULL);
   ctx_active = e_configure_option_ctx_new();
}

EINTERN void
e_conf2_hide(void)
{
   if (!back) return;
   evas_object_del(elm_object_top_widget_get(back));
}
