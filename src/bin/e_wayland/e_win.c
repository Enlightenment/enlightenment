#include "e.h"

/* local function prototypes */
static void _e_win_cb_free(E_Win *win);
static void _e_win_cb_move(Ecore_Evas *ee);
static void _e_win_cb_resize(Ecore_Evas *ee);
static void _e_win_cb_delete(Ecore_Evas *ee);

static void _e_win_prop_update(E_Win *win);

EAPI E_Win *
e_win_new(E_Container *con)
{
   E_Win *win;
   unsigned int parent = 0;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);

   win = E_OBJECT_ALLOC(E_Win, E_WIN_TYPE, _e_win_cb_free);
   if (!win) return NULL;

   win->x = 0;
   win->y = 0;
   win->w = 1;
   win->h = 1;
   win->placed = EINA_FALSE;
   win->min.w = 0;
   win->min.h = 0;
   win->max.w = 9999;
   win->max.h = 9999;
   win->base.w = 0;
   win->base.h = 0;
   win->step_x = 1;
   win->step_y = 1;
   win->aspect.min = 0.0;
   win->aspect.max = 0.0;
   win->con = con;
   win->state.centered = EINA_FALSE;

   /* FIXME: Ideally, frame should be false and e_border should handle 
    * adding the frame */

   parent = ecore_evas_wayland_window_get(con->bg_ee)->id;

   win->ee = e_canvas_new(parent, win->x, win->y, win->w, win->h, 
                          EINA_FALSE, EINA_TRUE, NULL);
   e_canvas_add(win->ee);

   ecore_evas_data_set(win->ee, "E_Win", win);
   ecore_evas_callback_move_set(win->ee, _e_win_cb_move);
   ecore_evas_callback_resize_set(win->ee, _e_win_cb_resize);
   ecore_evas_callback_delete_request_set(win->ee, _e_win_cb_delete);

   win->evas = ecore_evas_get(win->ee);
   ecore_evas_name_class_set(win->ee, "E", "_e_internal_window");
   ecore_evas_title_set(win->ee, "E");

   /* TODO: create pointer */

   return win;
}

EAPI void 
e_win_show(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   /* TODO: create e_border ? */

   _e_win_prop_update(win);
   ecore_evas_show(win->ee);
}

EAPI void 
e_win_hide(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   /* TODO: hide border ? */
   ecore_evas_hide(win->ee);
}

EAPI void 
e_win_centered_set(E_Win *win, int centered)
{
   E_Zone *zone;
   int x, y, w, h;

   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   if ((win->state.centered) && (!centered))
     {
        win->state.centered = EINA_FALSE;
     }
   else if ((!win->state.centered) && (centered))
     {
        win->state.centered = EINA_TRUE;
     }

   if (!win->state.centered) return;

   zone = e_zone_current_get(win->con);
   e_zone_useful_geometry_get(zone, &x, &y, &w, &h);
   printf("Center Window: %d %d\n", 
          x + (w - win->w) / 2, y + (h - win->h) / 2);
   ecore_evas_move(win->ee, x + (w - win->w) / 2, y + (h - win->h) / 2);
}

EAPI void 
e_win_title_set(E_Win *win, const char *title)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   ecore_evas_title_set(win->ee, title);
}

EAPI void 
e_win_move(E_Win *win, int x, int y)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   /* FIXME: e_border ? */
   win->x = x;
   win->y = y;
   ecore_evas_move(win->ee, x, y);
}

EAPI void 
e_win_resize(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   /* FIXME: e_border */
   win->w = w;
   win->h = h;
   ecore_evas_resize(win->ee, w, h);
}

EAPI void 
e_win_name_class_set(E_Win *win, const char *name, const char *class)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   ecore_evas_name_class_set(win->ee, name, class);
}

EAPI void 
e_win_dialog_set(E_Win *win, int dialog)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   if (win->state.dialog == dialog) return;
   win->state.dialog = dialog;
   _e_win_prop_update(win);
}

EAPI void 
e_win_move_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->cb_move = func;
}

EAPI void 
e_win_resize_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->cb_resize = func;
}

EAPI void 
e_win_delete_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->cb_delete = func;
}

EAPI Evas *
e_win_evas_get(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   return win->evas;
}

EAPI void 
e_win_size_min_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->min.w = w;
   win->min.h = h;
}

EAPI void 
e_win_size_max_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->max.w = w;
   win->max.h = h;
}

EAPI void 
e_win_size_base_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->base.w = w;
   win->base.h = h;
}

EAPI void 
e_win_step_set(E_Win *win, int x, int y)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   win->step_x = x;
   win->step_y = y;
}

EAPI void 
e_win_borderless_set(E_Win *win, int borderless)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   ecore_evas_borderless_set(win->ee, borderless);
}

EAPI void 
e_win_shaped_set(E_Win *win, int shaped)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   ecore_evas_shaped_set(win->ee, shaped);
}

EAPI void 
e_win_raise(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);

   ecore_evas_raise(win->ee);
}

EAPI void 
e_win_border_icon_set(E_Win *win, const char *icon)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
}

EAPI E_Win *
e_win_evas_object_win_get(Evas_Object *obj)
{
   Evas *evas;
   Evas_Object *wobj;
   E_Win *win;
   
   if (!obj) return NULL;
   evas = evas_object_evas_get(obj);
   wobj = evas_object_name_find(evas, "E_Win");
   if (!wobj) return NULL;
   win = evas_object_data_get(wobj, "E_Win");
   return win;
}

/* local functions */
static void 
_e_win_cb_free(E_Win *win)
{
   if (win->ee)
     {
        e_canvas_del(win->ee);
        ecore_evas_free(win->ee);
     }
}

static void 
_e_win_cb_move(Ecore_Evas *ee)
{
   E_Win *win;

   if (!(win = ecore_evas_data_get(ee, "E_Win"))) return;
   ecore_evas_geometry_get(win->ee, &win->x, &win->y, &win->w, &win->h);
   if (win->cb_move) win->cb_move(win);
}

static void 
_e_win_cb_resize(Ecore_Evas *ee)
{
   E_Win *win;

   if (!(win = ecore_evas_data_get(ee, "E_Win"))) return;
   ecore_evas_geometry_get(win->ee, &win->x, &win->y, &win->w, &win->h);
   if (win->cb_resize) win->cb_resize(win);
}

static void 
_e_win_cb_delete(Ecore_Evas *ee)
{
   E_Win *win;

   if (!(win = ecore_evas_data_get(ee, "E_Win"))) return;
   if (win->cb_delete) win->cb_delete(win);
}

static void 
_e_win_prop_update(E_Win *win)
{
   if (win->state.dialog)
     ecore_evas_wayland_type_set(win->ee, ECORE_WL_WINDOW_TYPE_TRANSIENT);
   else
     ecore_evas_wayland_type_set(win->ee, ECORE_WL_WINDOW_TYPE_TOPLEVEL);
}
