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
        struct wl_list frames;

        pixman_region32_t damage, opaque, input;

        Evas_Coord x, y;
        Eina_Bool new_attach : 1;
     } pending;

   pixman_region32_t bounding;
   pixman_region32_t damage;
   pixman_region32_t opaque;
   pixman_region32_t clip;
   pixman_region32_t input;

   struct wl_list frames;

   E_Plane *plane;
   E_Output *output;
   unsigned int output_mask;

   struct 
     {
        Evas_Coord x, y;
        Evas_Coord w, h;
        Eina_Bool changed : 1;
     } geometry;

   E_Shell_Surface *shell_surface;

   Eina_Bool mapped : 1;

   void *state;

   void (*map)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*unmap)(E_Surface *surface);
   void (*configure)(E_Surface *surface, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
};

struct _E_Surface_Frame
{
   struct wl_resource resource;
   struct wl_list link;
};

EAPI E_Surface *e_surface_new(unsigned int id);
EAPI void e_surface_attach(E_Surface *es, struct wl_buffer *buffer);
EAPI void e_surface_unmap(E_Surface *es);
EAPI void e_surface_damage(E_Surface *es);
EAPI void e_surface_damage_below(E_Surface *es);
EAPI void e_surface_destroy(E_Surface *es);
EAPI void e_surface_damage_calculate(E_Surface *es, pixman_region32_t *opaque);
EAPI void e_surface_show(E_Surface *es);
EAPI void e_surface_repaint_schedule(E_Surface *es);
EAPI void e_surface_output_assign(E_Surface *es);

# endif
#endif
