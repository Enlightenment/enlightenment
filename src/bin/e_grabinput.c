#include "e.h"

/* local subsystem functions */
static void      _e_grabinput_focus_do(Ecore_Window win, E_Focus_Method method);
static void      _e_grabinput_focus(Ecore_Window win, E_Focus_Method method);

/* local subsystem globals */
static Ecore_Window grab_mouse_win = 0;
static Ecore_Window grab_key_win = 0;
static Ecore_Window focus_win = 0;
static E_Focus_Method focus_method = E_FOCUS_METHOD_NO_INPUT;
static double last_focus_time = 0.0;

static Ecore_Window focus_fix_win = 0;
static E_Focus_Method focus_fix_method = E_FOCUS_METHOD_NO_INPUT;

/* externally accessible functions */
EINTERN int
e_grabinput_init(void)
{
   return 1;
}

EINTERN int
e_grabinput_shutdown(void)
{
   return 1;
}

E_API int
e_grabinput_get(Ecore_Window mouse_win, int confine_mouse, Ecore_Window key_win)
{
   if (grab_mouse_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_pointer_ungrab();
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          ecore_wl_input_ungrab(ecore_wl_input_get());
#endif
        grab_mouse_win = 0;
     }
   if (grab_key_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_keyboard_ungrab();
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          ecore_wl_input_ungrab(ecore_wl_input_get());
#endif

        grab_key_win = 0;
        focus_win = 0;
     }
   if (mouse_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          {
             int ret = 0;
             if (confine_mouse)
               ret = ecore_x_pointer_confine_grab(mouse_win);
             else
               ret = ecore_x_pointer_grab(mouse_win);
             if (!ret) return 0;
          }
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             Ecore_Wl_Window *wl_win;

             if ((wl_win = ecore_wl_window_find(mouse_win)))
               ecore_wl_input_grab(ecore_wl_input_get(), wl_win, 0);
          }
#endif
        grab_mouse_win = mouse_win;
     }
   if (key_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          {
             int ret = 0;

             ret = ecore_x_keyboard_grab(key_win);
             if (!ret)
               {
                  if (grab_mouse_win)
                    {
                       ecore_x_pointer_ungrab();
                       grab_mouse_win = 0;
                    }
                  return 0;
               }
          }
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             Ecore_Wl_Window *wl_win;

             if ((wl_win = ecore_wl_window_find(key_win)))
               ecore_wl_input_grab(ecore_wl_input_get(), wl_win, 0);
          }
#endif
        grab_key_win = key_win;
     }
#ifdef HAVE_WAYLAND_ONLY
   (void)confine_mouse;
#endif
   return 1;
}

E_API void
e_grabinput_release(Ecore_Window mouse_win, Ecore_Window key_win)
{
   if (mouse_win == grab_mouse_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_pointer_ungrab();
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          ecore_wl_input_ungrab(ecore_wl_input_get());
#endif

        grab_mouse_win = 0;
     }
   if (key_win == grab_key_win)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_keyboard_ungrab();
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          ecore_wl_input_ungrab(ecore_wl_input_get());
#endif

        grab_key_win = 0;
        if (focus_win != 0)
          {
             /* fprintf(stderr, "release focus to %x\n", focus_win); */
             _e_grabinput_focus(focus_win, focus_method);
             focus_win = 0;
             focus_method = E_FOCUS_METHOD_NO_INPUT;
          }
     }
}

E_API void
e_grabinput_focus(Ecore_Window win, E_Focus_Method method)
{
   if (grab_key_win != 0)
     {
        /* fprintf(stderr, "while grabbed focus changed to %x\n", win); */
        focus_win = win;
        focus_method = method;
     }
   else
     {
        /* fprintf(stderr, "focus to %x\n", win); */
        _e_grabinput_focus(win, method);
     }
}

E_API double
e_grabinput_last_focus_time_get(void)
{
   return last_focus_time;
}

E_API Ecore_Window
e_grabinput_last_focus_win_get(void)
{
   return focus_fix_win;
}

E_API Ecore_Window
e_grabinput_key_win_get(void)
{
   return grab_key_win;
}

E_API Ecore_Window
e_grabinput_mouse_win_get(void)
{
   return grab_mouse_win;
}

static void
_e_grabinput_focus_do(Ecore_Window win, E_Focus_Method method)
{
#ifdef HAVE_WAYLAND
   Ecore_Wl_Window *wl_win;
#endif

   /* fprintf(stderr, "focus to %x method %i\n", win, method); */
   switch (method)
     {
      case E_FOCUS_METHOD_NO_INPUT:
        break;

      case E_FOCUS_METHOD_LOCALLY_ACTIVE:
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          {
             ecore_x_window_focus_at_time(win, ecore_x_current_time_get());
             ecore_x_icccm_take_focus_send(win, ecore_x_current_time_get());
          }
#endif
#ifdef HAVE_WAYLAND
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             if ((wl_win = ecore_wl_window_find(win)))
               {
                  /* FIXME: Need to add an ecore_wl_window_focus function */
               }
          }
#endif
        break;

      case E_FOCUS_METHOD_GLOBALLY_ACTIVE:
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_icccm_take_focus_send(win, ecore_x_current_time_get());
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             if ((wl_win = ecore_wl_window_find(win)))
               {
                  /* FIXME: Need to add an ecore_wl_window_focus function */
               }
          }
#endif
        break;

      case E_FOCUS_METHOD_PASSIVE:
#ifndef HAVE_WAYLAND_ONLY
        if (e_comp_util_has_x())
          ecore_x_window_focus_at_time(win, ecore_x_current_time_get());
#else
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             if ((wl_win = ecore_wl_window_find(win)))
               {
                  /* FIXME: Need to add an ecore_wl_window_focus function */
               }
          }
#endif
        break;

      default:
        break;
     }
}

static void
_e_grabinput_focus(Ecore_Window win, E_Focus_Method method)
{
   focus_fix_win = win;
   focus_fix_method = method;
   /* fprintf(stderr, "foc do 1\n"); */
   _e_grabinput_focus_do(win, method);
   last_focus_time = ecore_loop_time_get();
}
