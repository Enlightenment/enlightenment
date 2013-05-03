#include "e.h"

/* local function prototypes */
static void _e_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);

/* local wayland interfaces */
static const struct wl_surface_interface _e_surface_interface = 
{
   _e_surface_cb_destroy,
   NULL, // cb_attach
   NULL, // cb_damage
   NULL, // cb_frame
   NULL, // cb_opaque_set
   NULL, // cb_input_set
   NULL, // cb_commit
   NULL // cb_buffer_transform_set
};

EAPI E_Surface *
e_surface_new(unsigned int id)
{
   E_Surface *es;

   /* try to allocate space for a new surface */
   if (!(es = E_NEW(E_Surface, 1))) return NULL;

   /* initialize the destroy signal */
   wl_signal_init(&es->wl.surface.resource.destroy_signal);

   /* initialize the link */
   wl_list_init(&es->wl.link);

   /* TODO: finish me */

   /* setup the surface object */
   es->wl.surface.resource.object.id = id;
   es->wl.surface.resource.object.interface = &wl_surface_interface;
   es->wl.surface.resource.object.implementation = 
     (void (**)(void))&_e_surface_interface;
   es->wl.surface.resource.data = es;

   return es;
}

/* local functions */
static void 
_e_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}
