#ifdef E_TYPEDEFS

typedef enum _E_Win_Layer
{
   E_WIN_LAYER_BELOW = 3,
   E_WIN_LAYER_NORMAL = 4,
   E_WIN_LAYER_ABOVE = 5
} E_Win_Layer;

typedef struct _E_Win E_Win;

typedef void (*E_Win_Cb)(E_Win*);

#else
#ifndef E_WIN_H
#define E_WIN_H

#define E_WIN_TYPE 0xE0b01011

struct _E_Win
{
   E_Object             e_obj_inherit;

   int                  x, y, w, h;
   E_Comp              *comp;
   E_Client            *client;
   Ecore_Evas          *ecore_evas;
   Evas                *evas;
   Ecore_Window         evas_win;
   unsigned char        placed : 1;
   int                  min_w, min_h, max_w, max_h, base_w, base_h;
   int                  step_x, step_y;
   double               min_aspect, max_aspect;
   E_Win_Cb             cb_move;
   E_Win_Cb             cb_resize;
   E_Win_Cb             cb_delete;
   void                *data;

   struct {
      unsigned char     centered : 1;
      unsigned char     dialog : 1;
      unsigned char     no_remember : 1;
      unsigned char     no_reopen : 1;
   } state;

   E_Pointer           *pointer;
};

EINTERN int    e_win_init               (void);
EINTERN int    e_win_shutdown           (void);
E_API Eina_Bool e_win_elm_available(void);
E_API E_Win *e_win_new                (E_Comp *c);
E_API void   e_win_show               (E_Win *win);
E_API void   e_win_hide               (E_Win *win);
E_API void   e_win_move               (E_Win *win, int x, int y);
E_API void   e_win_resize             (E_Win *win, int w, int h);
E_API void   e_win_move_resize        (E_Win *win, int x, int y, int w, int h);
E_API void   e_win_raise              (E_Win *win);
E_API void   e_win_lower              (E_Win *win);
E_API void   e_win_placed_set         (E_Win *win, int placed);
E_API Evas  *e_win_evas_get           (E_Win *win);
E_API void   e_win_shaped_set         (E_Win *win, int shaped);
E_API void   e_win_avoid_damage_set   (E_Win *win, int avoid);
E_API void   e_win_borderless_set     (E_Win *win, int borderless);
E_API void   e_win_layer_set          (E_Win *win, E_Win_Layer layer);
E_API void   e_win_sticky_set         (E_Win *win, int sticky);
E_API void   e_win_move_callback_set  (E_Win *win, void (*func) (E_Win *win));
E_API void   e_win_resize_callback_set(E_Win *win, void (*func) (E_Win *win));
E_API void   e_win_delete_callback_set(E_Win *win, void (*func) (E_Win *win));
E_API void   e_win_size_min_set       (E_Win *win, int w, int h);
E_API void   e_win_size_max_set       (E_Win *win, int w, int h);
E_API void   e_win_size_base_set      (E_Win *win, int w, int h);
E_API void   e_win_step_set           (E_Win *win, int x, int y);
E_API void   e_win_name_class_set     (E_Win *win, const char *name, const char *class);
E_API void   e_win_title_set          (E_Win *win, const char *title);
E_API void   e_win_client_icon_set    (E_Win *win, const char *icon);
E_API void   e_win_client_icon_key_set(E_Win *win, const char *key);
E_API void   e_win_centered_set       (E_Win *win, int centered);
E_API void   e_win_dialog_set         (E_Win *win, int dialog);
E_API void   e_win_no_remember_set    (E_Win *win, int no_remember);

E_API E_Win *e_win_evas_object_win_get(Evas_Object *obj);

#endif
#endif
