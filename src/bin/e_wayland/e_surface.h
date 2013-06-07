#ifdef E_TYPEDEFS

typedef struct _E_Surface E_Surface;
typedef struct _E_Surface_Frame E_Surface_Frame;

#else
# ifndef E_SURFACE_H
#  define E_SURFACE_H

struct _E_Surface
{
   struct 
     {
        struct wl_resource resource;
        struct wl_list link;
     } wl;

   struct 
     {
        E_Buffer_Reference reference;
        Eina_Bool keep : 1;
     } buffer;

   struct 
     {
        struct wl_buffer *buffer;
        struct wl_listener buffer_destroy;
        Eina_List *frames;

        pixman_region32_t damage, opaque, input;

        Evas_Coord x, y;
        Eina_Bool new_attach : 1;
     } pending;

   pixman_region32_t damage;
   pixman_region32_t opaque;
   pixman_region32_t clip;
   pixman_region32_t input;

   Eina_List *frames;
   E_Plane *plane;

   struct 
     {
        Evas_Coord x, y;
        Evas_Coord w, h;
        Eina_Bool changed : 1;
     } geometry;

   E_Shell_Surface *shell_surface;

   Evas *evas;
   Ecore_Evas *ee;

   Eina_Bool mapped : 1;

   void (*map)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*unmap)(E_Surface *surface);
   void (*configure)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
};

struct _E_Surface_Frame
{
   E_Surface *surface;
   struct wl_resource resource;
};

EAPI E_Surface *e_surface_new(unsigned int id);
EAPI void e_surface_attach(E_Surface *es, struct wl_buffer *buffer);
EAPI void e_surface_unmap(E_Surface *es);
EAPI void e_surface_damage(E_Surface *es);
EAPI void e_surface_destroy(E_Surface *es);
EAPI void e_surface_damage_calculate(E_Surface *es, pixman_region32_t *opaque);

# endif
#endif
