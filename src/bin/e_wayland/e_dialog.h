#ifdef E_TYPEDEFS

typedef struct _E_Dialog E_Dialog;

#else
# ifndef E_DIALOG_H
#  define E_DIALOG_H

#  define E_DIALOG_TYPE 0xE0b01012

struct _E_Dialog
{
   E_Object e_obj_inherit;

   E_Win *win;
   Evas_Object *o_bg, *o_box;
   Evas_Object *o_text, *o_content;
   Evas_Object *o_icon, *o_event;
   Eina_List *buttons;
   void *data;
   int min_w, min_h;
   Eina_Bool resizable : 1;
};

EAPI E_Dialog *e_dialog_new(E_Container *con, const char *name, const char *class);
EAPI E_Dialog *e_dialog_normal_win_new(E_Container *con, const char *name, const char *class);
EAPI void e_dialog_button_add(E_Dialog *dia, const char *label, const char *icon, void (*func) (void *data, E_Dialog *dia), void *data);
EAPI int e_dialog_button_focus_num(E_Dialog *dia, int button);
EAPI int e_dialog_button_disable_num_set(E_Dialog *dia, int button, int disabled);
EAPI int e_dialog_button_disable_num_get(E_Dialog *dia, int button);
EAPI void e_dialog_title_set(E_Dialog *dia, const char *title);
EAPI void e_dialog_text_set(E_Dialog *dia, const char *text);
EAPI void e_dialog_icon_set(E_Dialog *dia, const char *icon, Evas_Coord size);
EAPI void e_dialog_border_icon_set(E_Dialog *dia, const char *icon);
EAPI void e_dialog_content_set(E_Dialog *dia, Evas_Object *obj, Evas_Coord minw, Evas_Coord minh);
EAPI void e_dialog_resizable_set(E_Dialog *dia, int resizable);
EAPI void e_dialog_show(E_Dialog *dia);

# endif
#endif
