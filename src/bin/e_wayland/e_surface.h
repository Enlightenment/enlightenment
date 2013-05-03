#ifdef E_TYPEDEFS

typedef struct _E_Surface E_Surface;

#else
# ifndef E_SURFACE_H
#  define E_SURFACE_H

struct _E_Surface
{
   struct 
     {
        struct wl_surface surface;
        struct wl_list link;
     } wl;

   E_Shell_Surface *shell_surface;

   Eina_Bool mapped : 1;

   void (*map)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*unmap)(E_Surface *surface);
   void (*configure)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
};

EAPI E_Surface *e_surface_new(unsigned int id);

# endif
#endif
