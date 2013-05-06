#ifndef E_MOD_MAIN_H
# define E_MOD_MAIN_H

# include <Ecore_X.h>

typedef struct _E_Compositor_X11 E_Compositor_X11;
typedef struct _E_Output_X11 E_Output_X11;

struct _E_Compositor_X11
{
   E_Compositor base;

   Ecore_X_Display *display;
};

struct _E_Output_X11
{
   E_Output base;
   E_Output_Mode mode;

   Ecore_X_Window win;
   Ecore_X_Pixmap pmap;
   Ecore_X_GC gc;

   struct wl_event_source *frame_timer;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);

#endif
