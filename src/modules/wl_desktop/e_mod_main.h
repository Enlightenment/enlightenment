#ifndef E_MOD_MAIN_H
# define E_MOD_MAIN_H

typedef struct _E_Desktop_Shell E_Desktop_Shell;

struct _E_Desktop_Shell
{
   E_Compositor *compositor;

   struct 
     {
        struct wl_resource *resource;
        struct wl_listener destroy_listener;
     } wl;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);

#endif
