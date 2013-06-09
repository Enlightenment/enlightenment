#include "e.h"

/* local function prototypes */
static void _e_output_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_output_cb_unbind(struct wl_resource *resource);

EAPI void 
e_output_init(E_Output *output, E_Compositor *comp, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, unsigned int transform)
{
   /* set some output properties */
   output->compositor = comp;
   output->x = x;
   output->y = y;
   output->w = w;
   output->h = h;
   output->mm_w = w;
   output->mm_h = h;
   output->dirty = EINA_TRUE;
   output->transform = transform;

   wl_list_init(&output->wl.resources);

   wl_signal_init(&output->signals.frame);
   wl_signal_init(&output->signals.destroy);

   output->id = ffs(~comp->output_pool) - 1;
   comp->output_pool |= (1 << output->id);

   /* add this output to the registry */
   output->wl.global = 
     wl_display_add_global(comp->wl.display, &wl_output_interface, 
                           output, _e_output_cb_bind);
}

EAPI void 
e_output_shutdown(E_Output *output)
{
   E_Compositor *comp;

   /* check for valid output */
   if (!output) return;

   comp = output->compositor;
   comp->output_pool &= ~(1 << output->id);

   wl_display_remove_global(comp->wl.display, output->wl.global);
}

EAPI void 
e_output_repaint(E_Output *output, unsigned int secs)
{
   E_Compositor *comp;

   comp = output->compositor;

   /* TODO: assign planes */

   e_compositor_damage_calculate(comp);
}

/* local functions */
static void 
_e_output_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Output *output;
   E_Output_Mode *mode;
   Eina_List *l;
   struct wl_resource *resource;

   /* check for valid output */
   if (!(output = data)) return;

   /* add this output to the client */
   resource = 
     wl_client_add_object(client, &wl_output_interface, NULL, id, output);
   wl_list_insert(&output->wl.resources, &resource->link);

   /* setup destroy callback */
   resource->destroy = _e_output_cb_unbind;

   /* send out this output's geometry */
   wl_output_send_geometry(resource, output->x, output->y, 
                           output->mm_w, output->mm_h, output->subpixel, 
                           output->make, output->model, output->transform);

   /* send output mode */
   EINA_LIST_FOREACH(output->modes, l, mode)
     wl_output_send_mode(resource, mode->flags, mode->w, mode->h, 
                         mode->refresh);
}

static void 
_e_output_cb_unbind(struct wl_resource *resource)
{
   wl_list_remove(&resource->link);
   free(resource);
}
