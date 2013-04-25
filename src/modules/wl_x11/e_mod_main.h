#ifndef E_MOD_MAIN_H
# define E_MOD_MAIN_H

# include <Ecore_X.h>

typedef struct _E_Compositor_X11 E_Compositor_X11;

struct _E_Compositor_X11
{
   E_Compositor base;

   Ecore_X_Display *display;
   Ecore_X_Window win;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);

#endif
