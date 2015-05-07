#include "e.h"

/* local subsystem functions */
static void _e_obj_dialog_free(E_Obj_Dialog *od);
static void _e_obj_dialog_cb_delete(E_Win *win);
static void _e_obj_dialog_cb_close(void *data, Evas_Object *obj, const char *emission, const char *source);
static void _e_obj_dialog_cb_resize(E_Win *win);

/* local subsystem globals */

/* externally accessible functions */

static void
_key_down_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Key_Down *ev = event;

   if (!strcmp(ev->key, "Escape") && data)
     _e_obj_dialog_cb_delete(data);
}

E_API E_Obj_Dialog *
e_obj_dialog_new(E_Comp *c, char *title, char *class_name, char *class_class)
{
   E_Obj_Dialog *od;
   Evas_Object *o;
   Eina_Bool kg;
   Evas_Modifier_Mask mask;

   if (!c) c = e_util_comp_current_get();
   od = E_OBJECT_ALLOC(E_Obj_Dialog, E_OBJ_DIALOG_TYPE, _e_obj_dialog_free);
   if (!od) return NULL;
   od->win = e_win_new(c);
   if (!od->win)
     {
        free(od);
        return NULL;
     }
   e_win_delete_callback_set(od->win, _e_obj_dialog_cb_delete);
   e_win_resize_callback_set(od->win, _e_obj_dialog_cb_resize);
   od->win->data = od;
   e_win_dialog_set(od->win, 1);
   e_win_name_class_set(od->win, class_name, class_class);
   e_win_title_set(od->win, title);

   o = edje_object_add(e_win_evas_get(od->win));
   od->bg_object = o;

   e_win_centered_set(od->win, 1);
   od->cb_delete = NULL;

   mask = 0;
   kg = evas_object_key_grab(o, "Escape", mask, ~mask, 0);
   if (!kg)
     fprintf(stderr, "ERROR: unable to redirect \"Escape\" key events to object %p.\n", o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_KEY_DOWN,
                                  _key_down_cb, od->win);

   return od;
}

E_API void
e_obj_dialog_cb_delete_set(E_Obj_Dialog *od, void (*func)(E_Obj_Dialog *od))
{
   od->cb_delete = func;
}

E_API void
e_obj_dialog_icon_set(E_Obj_Dialog *od, char *icon)
{
   E_OBJECT_CHECK(od);
   E_OBJECT_TYPE_CHECK(od, E_OBJ_DIALOG_TYPE);
   eina_stringshare_replace(&od->win->client->internal_icon, icon);
 }

E_API void
e_obj_dialog_show(E_Obj_Dialog *od)
{
   Evas_Coord w, h, mw, mh;
   const char *s;

   E_OBJECT_CHECK(od);
   E_OBJECT_TYPE_CHECK(od, E_OBJ_DIALOG_TYPE);

   edje_object_size_min_get(od->bg_object, &mw, &mh);
   edje_object_size_min_restricted_calc(od->bg_object, &mw, &mh, mw, mh);
   evas_object_resize(od->bg_object, mw, mh);
   e_win_resize(od->win, mw, mh);
   e_win_size_min_set(od->win, mw, mh);
   edje_object_size_max_get(od->bg_object, &w, &h);
   if ((w > 0) && (h > 0))
     {
        if (w < mw) w = mw;
        if (h < mh) h = mh;
        e_win_size_max_set(od->win, w, h);
     }
   s = edje_object_data_get(od->bg_object, "borderless");
   if (s && (!strcmp(s, "1")))
     e_win_borderless_set(od->win, 1);
   s = edje_object_data_get(od->bg_object, "shaped");
   if (s && (!strcmp(s, "1")))
     e_win_shaped_set(od->win, 1);
   e_win_show(od->win);
}

E_API void
e_obj_dialog_obj_part_text_set(E_Obj_Dialog *od, const char *part, const char *text)
{
   E_OBJECT_CHECK(od);
   E_OBJECT_TYPE_CHECK(od, E_OBJ_DIALOG_TYPE);
   edje_object_part_text_set(od->bg_object, part, text);
}

E_API void
e_obj_dialog_obj_theme_set(E_Obj_Dialog *od, char *theme_cat, char *theme_obj)
{
   E_OBJECT_CHECK(od);
   E_OBJECT_TYPE_CHECK(od, E_OBJ_DIALOG_TYPE);

   e_theme_edje_object_set(od->bg_object, theme_cat, theme_obj);
   evas_object_move(od->bg_object, 0, 0);
   evas_object_show(od->bg_object);
   edje_object_signal_callback_add(od->bg_object, "e,action,close", "",
                                   _e_obj_dialog_cb_close, od);
}

/* local subsystem functions */
static void
_e_obj_dialog_free(E_Obj_Dialog *od)
{
   if (od->bg_object) evas_object_del(od->bg_object);
   e_object_del(E_OBJECT(od->win));
   free(od);
}

static void
_e_obj_dialog_cb_delete(E_Win *win)
{
   E_Obj_Dialog *od;

   od = win->data;
   if (od->cb_delete)
     od->cb_delete(od);
   e_object_del(E_OBJECT(od));
}

static void
_e_obj_dialog_cb_close(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Obj_Dialog *od;

   od = data;
   if (od->cb_delete)
     od->cb_delete(od);
   e_util_defer_object_del(E_OBJECT(od));
}

static void
_e_obj_dialog_cb_resize(E_Win *win)
{
   E_Obj_Dialog *od;

   od = win->data;
   evas_object_resize(od->bg_object, od->win->w, od->win->h);
}

