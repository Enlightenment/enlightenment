#ifdef E_TYPEDEFS

typedef enum _E_Shell_Surface_Type E_Shell_Surface_Type;
typedef struct _E_Shell_Surface E_Shell_Surface;

#else
# ifndef E_SHELL_SURFACE_H
#  define E_SHELL_SURFACE_H

enum _E_Shell_Surface_Type
{
   E_SHELL_SURFACE_TYPE_NONE,
   E_SHELL_SURFACE_TYPE_TOPLEVEL,
   E_SHELL_SURFACE_TYPE_TRANSIENT,
   E_SHELL_SURFACE_TYPE_FULLSCREEN,
   E_SHELL_SURFACE_TYPE_MAXIMIZED,
   E_SHELL_SURFACE_TYPE_POPUP
};

struct _E_Shell_Surface
{
   struct 
     {
        struct wl_resource resource;
        struct wl_listener surface_destroy;
        struct wl_list link;
     } wl;

   struct 
     {
        Evas_Coord x, y, w, h;
        Eina_Bool valid : 1;
     } saved;

   struct 
     {
        struct wl_pointer_grab grab;
        struct wl_seat *seat;
        struct wl_listener parent_destroy;
        Evas_Coord x, y;
        Eina_Bool up : 1;
        unsigned int serial;
     } popup;

   struct 
     {
        Evas_Coord x, y;
        unsigned int flags;
     } transient;

   E_Surface *surface, *parent;
   E_Shell_Surface_Type type, ntype;

   char *title, *clas;

   Eina_Bool active : 1;

   void *ping_timer;
};

# endif
#endif
