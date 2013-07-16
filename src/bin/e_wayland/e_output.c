#include "e.h"

/* local function prototypes */
static void _e_output_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id);
static void _e_output_cb_unbind(struct wl_resource *resource);
static void _e_output_cb_idle_repaint(void *data);

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

   pixman_region32_init(&output->repaint.prev_damage);
   pixman_region32_init_rect(&output->repaint.region, x, y, w, h);

   pixman_region32_union(&comp->plane.damage, &comp->plane.damage, 
                         &output->repaint.region);
   e_output_repaint_schedule(output);

   wl_list_init(&output->wl.resources);
   wl_signal_init(&output->signals.frame);
   wl_signal_init(&output->signals.destroy);

   output->id = ffs(~comp->output_pool) - 1;
   comp->output_pool |= (1 << output->id);

   /* add this output to the registry */
   output->wl.global = 
     wl_global_create(comp->wl.display, &wl_output_interface, 2, output, 
                      _e_output_cb_bind);
}

EAPI void 
e_output_shutdown(E_Output *output)
{
   E_Compositor *comp;

   /* check for valid output */
   if (!output) return;

   wl_signal_emit(&output->signals.destroy, output);

   pixman_region32_fini(&output->repaint.region);
   pixman_region32_fini(&output->repaint.prev_damage);

   comp = output->compositor;
   comp->output_pool &= ~(1 << output->id);

   wl_global_destroy(output->wl.global);
}

EAPI void 
e_output_repaint(E_Output *output, unsigned int secs)
{
   E_Compositor *comp;
   E_Surface *es;
   E_Surface_Frame *cb, *cbnext;
   pixman_region32_t damage;
   Eina_List *l;
   struct wl_list frames;

   comp = output->compositor;

   EINA_LIST_FOREACH(comp->surfaces, l, es)
     {
        if (es->plane != &comp->plane) continue;
        e_surface_damage_below(es);
        es->plane = &comp->plane;
        e_surface_damage(es);
     }

   wl_list_init(&frames);
   EINA_LIST_FOREACH(comp->surfaces, l, es)
     {
        if (es->output != output) continue;
        wl_list_insert_list(&frames, &es->frames);
        wl_list_init(&es->frames);
     }

   e_compositor_damage_calculate(comp);

   pixman_region32_init(&damage);
   pixman_region32_intersect(&damage, &comp->plane.damage, 
                             &output->repaint.region);
   pixman_region32_subtract(&damage, &damage, &comp->plane.clip);

   if (output->cb_repaint) output->cb_repaint(output, &damage);

   pixman_region32_fini(&damage);
   output->repaint.needed = EINA_FALSE;

   e_compositor_repick(comp);
   wl_event_loop_dispatch(comp->wl.input_loop, 0);

   /* send surface frame callback done */
   wl_list_for_each_safe(cb, cbnext, &frames, link)
     {
        wl_callback_send_done(cb->resource, secs);
        wl_resource_destroy(cb->resource);
     }
}

EAPI void 
e_output_repaint_schedule(E_Output *output)
{
   E_Compositor *comp;
   struct wl_event_loop *loop;

   comp = output->compositor;

   loop = wl_display_get_event_loop(comp->wl.display);
   output->repaint.needed = EINA_TRUE;
   if (output->repaint.scheduled) return;

   wl_event_loop_add_idle(loop, _e_output_cb_idle_repaint, output);
   output->repaint.scheduled = EINA_TRUE;

   if (comp->wl.input_loop_source) 
     wl_event_source_remove(comp->wl.input_loop_source);
   comp->wl.input_loop_source = NULL;
}

EAPI void 
e_output_damage(E_Output *output)
{
   E_Compositor *comp;

   comp = output->compositor;
   pixman_region32_union(&comp->plane.damage, 
                         &comp->plane.damage, &output->repaint.region);
   e_output_repaint_schedule(output);
}

/* local functions */
static void 
_e_output_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id)
{
   E_Output *output;
   E_Output_Mode *mode;
   Eina_List *l;
   struct wl_resource *resource;

   /* check for valid output */
   if (!(output = data)) return;

   /* add this output to the client */
   resource = 
     wl_resource_create(client, &wl_output_interface, MIN(version, 2), id);
   wl_list_insert(&output->wl.resources, wl_resource_get_link(resource));
   wl_resource_set_implementation(resource, NULL, data, _e_output_cb_unbind);

   /* send out this output's geometry */
   wl_output_send_geometry(resource, output->x, output->y, 
                           output->mm_w, output->mm_h, output->subpixel, 
                           output->make, output->model, output->transform);

   /* send output mode */
   EINA_LIST_FOREACH(output->modes, l, mode)
     wl_output_send_mode(resource, mode->flags, mode->w, mode->h, 
                         mode->refresh);

   if (version >= 2) wl_output_send_done(resource);
}

static void 
_e_output_cb_unbind(struct wl_resource *resource)
{
   wl_list_remove(wl_resource_get_link(resource));
   free(resource);
}

static void 
_e_output_cb_idle_repaint(void *data)
{
   E_Output *output;

   if (!(output = data)) return;

   if (output->cb_repaint_start)
     output->cb_repaint_start(output);
}
