#ifdef E_TYPEDEFS

typedef struct _E_Win E_Win;

#else
# ifndef E_WIN_H
#  define E_WIN_H

#  define E_WIN_TYPE 0xE0b01011

struct _E_Win
{
   E_Object e_obj_inherit;

   int x, y, w, h;

   E_Container *con;
   Ecore_Evas *ee;
   Evas *evas;

   E_Border *border;

   Eina_Bool placed : 1;
   struct 
     {
        int w, h;
     } min, max, base;
   int step_x, step_y;
   struct 
     {
        double min, max;
     } aspect;

   void (*cb_move) (E_Win *win);
   void (*cb_resize) (E_Win *win);
   void (*cb_delete) (E_Win *win);
   void *data;

   struct 
     {
        Eina_Bool centered : 1;
        Eina_Bool dialog : 1;
        Eina_Bool no_remember : 1;
     } state;
};

EAPI E_Win *e_win_new(E_Container *con);
EAPI void e_win_show(E_Win *win);
EAPI void e_win_hide(E_Win *win);
EAPI void e_win_centered_set(E_Win *win, int centered);
EAPI void e_win_title_set(E_Win *win, const char *title);
EAPI void e_win_move(E_Win *win, int x, int y);
EAPI void e_win_resize(E_Win *win, int w, int h);
EAPI void e_win_name_class_set(E_Win *win, const char *name, const char *class);
EAPI void e_win_dialog_set(E_Win *win, int dialog);
EAPI void e_win_move_callback_set(E_Win *win, void (*func)(E_Win *win));
EAPI void e_win_resize_callback_set(E_Win *win, void (*func)(E_Win *win));
EAPI void e_win_delete_callback_set(E_Win *win, void (*func)(E_Win *win));
EAPI Evas *e_win_evas_get(E_Win *win);
EAPI void e_win_size_min_set(E_Win *win, int w, int h);
EAPI void e_win_size_max_set(E_Win *win, int w, int h);
EAPI void e_win_size_base_set(E_Win *win, int w, int h);
EAPI void e_win_step_set(E_Win *win, int x, int y);
EAPI void e_win_borderless_set(E_Win *win, int borderless);
EAPI void e_win_shaped_set(E_Win *win, int shaped);
EAPI void e_win_raise(E_Win *win);
EAPI void e_win_border_icon_set(E_Win *win, const char *icon);

EAPI E_Win *e_win_evas_object_win_get(Evas_Object *obj);

# endif
#endif
