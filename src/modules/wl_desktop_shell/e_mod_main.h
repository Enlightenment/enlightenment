#ifndef E_MOD_MAIN_H
# define E_MOD_MAIN_H

# define SLOGFNS 1

# ifdef SLOGFNS
#  include <stdio.h>
#  define SLOGFN(fl, ln, fn) printf("-E-SHELL: %25s: %5i - %s\n", fl, ln, fn);
# else
#  define SLOGFN(fl, ln, fn)
# endif

typedef struct _E_Wayland_Desktop_Shell E_Wayland_Desktop_Shell;
typedef struct _E_Wayland_Ping_Timer E_Wayland_Ping_Timer;

struct _E_Wayland_Desktop_Shell
{
   E_Wayland_Compositor *compositor;
   E_Wayland_Surface *grab_surface;

   struct 
     {
        struct wl_resource *resource;
        struct wl_listener destroy_listener;
     } wl;
};

struct _E_Wayland_Ping_Timer 
{
   struct wl_event_source *source;
   unsigned int serial;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);

#endif
