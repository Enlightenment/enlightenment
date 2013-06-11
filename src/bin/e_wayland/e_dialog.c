#include "e.h"

/* local function prototypes */
static E_Dialog *_e_dialog_internal_new(E_Container *con, const char *name, const char *class, Eina_Bool dialog);
static void _e_dialog_cb_free(E_Dialog *dia);

EAPI E_Dialog *
e_dialog_new(E_Container *con, const char *name, const char *class)
{
   return _e_dialog_internal_new(con, name, class, EINA_TRUE);
}

EAPI E_Dialog *
e_dialog_normal_win_new(E_Container *con, const char *name, const char *class)
{
   return _e_dialog_internal_new(con, name, class, EINA_FALSE);
}

EAPI void 
e_dialog_button_add(E_Dialog *dia, const char *label, const char *icon, void (*func) (void *data, E_Dialog *dia), void *data)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   /* Evas_Object *o; */

   /* if (!func) func = _e_dialog_cb_delete; */
   /* TODO: e_widgets */
}

EAPI int 
e_dialog_button_focus_num(E_Dialog *dia, int button)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   return 0;
}

EAPI int 
e_dialog_button_disable_num_set(E_Dialog *dia, int button, int disabled)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   return 0;
}

EAPI int 
e_dialog_button_disable_num_get(E_Dialog *dia, int button)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   return 0;
}

EAPI void 
e_dialog_title_set(E_Dialog *dia, const char *title)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   e_win_title_set(dia->win, title);
}

EAPI void 
e_dialog_text_set(E_Dialog *dia, const char *text)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   if (!dia->o_text)
     {
        dia->o_text = edje_object_add(dia->win->evas);
        e_theme_edje_object_set(dia->o_text, "base/theme/dialog", 
                                "e/widgets/dialog/text");
        edje_object_part_swallow(dia->o_bg, "e.swallow.content", dia->o_text);
        evas_object_show(dia->o_text);
     }
   edje_object_part_text_set(dia->o_text, "e.textblock.message", text);
}

EAPI void 
e_dialog_icon_set(E_Dialog *dia, const char *icon, Evas_Coord size)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   /* TODO: e_icon */
}

EAPI void 
e_dialog_border_icon_set(E_Dialog *dia, const char *icon)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

}

EAPI void 
e_dialog_content_set(E_Dialog *dia, Evas_Object *obj, Evas_Coord minw, Evas_Coord minh)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   dia->o_content = obj;
   /* TODO: e_widget on focus_hook */
   edje_extern_object_min_size_set(obj, minw, minh);
   edje_object_part_swallow(dia->o_bg, "e.swallow.content", obj);
   evas_object_show(obj);
}

EAPI void 
e_dialog_resizable_set(E_Dialog *dia, int resizable)
{
   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   dia->resizable = resizable;
   if (dia->win)
     {
        /* TODO: Finish */
        /* if (resizable)  */
        /*   { */
        /*   } */
        /* else */
        /*   { */
        /*   } */
     }
}

EAPI void 
e_dialog_show(E_Dialog *dia)
{
   Evas_Coord mw, mh;

   E_OBJECT_CHECK(dia);
   E_OBJECT_TYPE_CHECK(dia, E_DIALOG_TYPE);

   if (dia->o_text)
     {
        edje_object_size_min_calc(dia->o_text, &mw, &mh);
        edje_extern_object_min_size_set(dia->o_text, mw, mh);
        edje_object_part_swallow(dia->o_bg, "e.swallow.content", dia->o_text);
     }

   /* TODO: e_widget_size_min_get */

   e_win_show(dia->win);
}

/* local functions */
static E_Dialog *
_e_dialog_internal_new(E_Container *con, const char *name, const char *class, Eina_Bool dialog)
{
   E_Dialog *dia;

   if (!con)
     {
        E_Manager *man;

        if (!(man = e_manager_current_get())) return NULL;
        if (!(con = e_container_current_get(man)))
          con = e_container_number_get(man, 0);
        if (!con) return NULL;
     }

   dia = E_OBJECT_ALLOC(E_Dialog, E_DIALOG_TYPE, _e_dialog_cb_free);
   if (!dia) return NULL;

   if (!(dia->win = e_win_new(con)))
     {
        free(dia);
        return NULL;
     }

   /* TODO: e_win callbacks */
   dia->win->data = dia;

   e_win_dialog_set(dia->win, dialog);
   e_win_name_class_set(dia->win, name, class);

   dia->o_bg = edje_object_add(dia->win->evas);
   e_theme_edje_object_set(dia->o_bg, "base/theme/dialog", 
                           "e/widgets/dialog/main");
   evas_object_move(dia->o_bg, 0, 0);
   evas_object_show(dia->o_bg);

   /* TODO: box object & event object */

   return dia;
}

static void 
_e_dialog_cb_free(E_Dialog *dia)
{
   if (dia->buttons)
     {
        E_FREE_LIST(dia->buttons, evas_object_del);
        /* Eina_List *l; */
        /* Evas_Object *o; */

        /* EINA_LIST_FOREACH(dia->buttons, l, o) */
        /*   evas_object_del(o); */

        /* eina_list_free(dia->buttons); */
     }

   if (dia->o_text) evas_object_del(dia->o_text);
   if (dia->o_icon) evas_object_del(dia->o_icon);
   if (dia->o_box) evas_object_del(dia->o_box);
   if (dia->o_bg) evas_object_del(dia->o_bg);
   if (dia->o_content) evas_object_del(dia->o_content);
   if (dia->o_event) evas_object_del(dia->o_event);

   e_object_del(E_OBJECT(dia->win));
   free(dia);
}
