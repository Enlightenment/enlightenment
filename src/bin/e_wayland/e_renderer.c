#include "e.h"

typedef struct _E_Renderer_Output_State E_Renderer_Output_State;
typedef struct _E_Renderer_Surface_State E_Renderer_Surface_State;

struct _E_Renderer_Output_State
{
#ifdef HAVE_WAYLAND_EGL
   EGLSurface surface;
   pixman_region32_t buffer[2];
#endif
   pixman_image_t *shadow;
   void *shadow_buffer;
   pixman_image_t *hw_buffer;
};

struct _E_Renderer_Surface_State
{
#ifdef HAVE_WAYLAND_EGL
   GLfloat color[4];
   GLuint textures[3];
   GLenum target;

   EGLImageKHR images[3];

   E_Shader *shader;

   pixman_region32_t texture_damage;

   int num_text;
   int num_images;
   int pitch, height;
#endif
   pixman_image_t *image;
   E_Buffer_Reference buffer_reference;
};

/* local function prototypes */
static void _e_renderer_region_repaint(E_Surface *surface, E_Output *output, pixman_region32_t *region, pixman_region32_t *surface_region, pixman_op_t pixman_op);
static void _e_renderer_surfaces_repaint(E_Output *output, pixman_region32_t *damage);
static void _e_renderer_surface_draw(E_Surface *surface, E_Output *output, pixman_region32_t *damage);
static Eina_Bool _e_renderer_cb_pixels_read(E_Output *output, int format, void *pixels, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_renderer_cb_output_buffer_set(E_Output *output, pixman_image_t *buffer);
static void _e_renderer_cb_output_repaint(E_Output *output, pixman_region32_t *damage);
static void _e_renderer_cb_damage_flush(E_Surface *surface);
static void _e_renderer_cb_attach(E_Surface *surface, struct wl_buffer *buffer);
static Eina_Bool _e_renderer_cb_output_create(E_Output *output, unsigned int window);
static void _e_renderer_cb_output_destroy(E_Output *output);
static Eina_Bool _e_renderer_cb_surface_create(E_Surface *surface);
static void _e_renderer_cb_surface_destroy(E_Surface *surface);
static void _e_renderer_cb_surface_color_set(E_Surface *surface, int r, int g, int b, int a);
static void _e_renderer_cb_destroy(E_Compositor *comp);

EAPI Eina_Bool 
e_renderer_create(E_Compositor *comp)
{
   E_Renderer *rend;

   /* try to allocate space for new renderer */
   if (!(rend = E_NEW(E_Renderer, 1))) return EINA_FALSE;

   /* FIXME: Vary these based on software or gl
    * NB: This code current ONLY does software */

   /* setup renderer functions */
   rend->pixels_read = _e_renderer_cb_pixels_read;
   rend->output_buffer_set = _e_renderer_cb_output_buffer_set;
   rend->output_repaint = _e_renderer_cb_output_repaint;
   rend->damage_flush = _e_renderer_cb_damage_flush;
   rend->attach = _e_renderer_cb_attach;
   rend->output_create = _e_renderer_cb_output_create;
   rend->output_destroy = _e_renderer_cb_output_destroy;
   rend->surface_create = _e_renderer_cb_surface_create;
   rend->surface_destroy = _e_renderer_cb_surface_destroy;
   rend->surface_color_set = _e_renderer_cb_surface_color_set;
   rend->destroy = _e_renderer_cb_destroy;

   comp->renderer = rend;

   return EINA_TRUE;
}

/* local functions */
static void 
_e_renderer_region_repaint(E_Surface *surface, E_Output *output, pixman_region32_t *region, pixman_region32_t *surf_region, pixman_op_t pixman_op)
{
//   E_Renderer *rend;
   E_Renderer_Output_State *out_state;
   E_Renderer_Surface_State *surf_state;
   pixman_region32_t fregion;
   pixman_box32_t *ext;
//   pixman_fixed_t fw = 0, fh = 0;

   printf("E_Renderer Region Repaint\n");

//   rend = output->compositor->renderer;
   out_state = output->state;
   surf_state = surface->state;

   pixman_region32_init(&fregion);
   if (surf_region)
     {
        pixman_region32_copy(&fregion, surf_region);
        pixman_region32_translate(&fregion, 
                                  surface->geometry.x, surface->geometry.y);
        pixman_region32_intersect(&fregion, &fregion, region);
     }
   else
     pixman_region32_copy(&fregion, region);

   ext = pixman_region32_extents(&fregion);
   printf("\tRepainting Region: %d %d %d %d\n", 
          ext->x1, ext->y1, (ext->x2 - ext->x1), 
          (ext->y2 - ext->y1));

   /* global to output ? */

   pixman_image_set_clip_region32(out_state->shadow, &fregion);
   pixman_image_set_filter(surf_state->image, PIXMAN_FILTER_NEAREST, NULL, 0);

   pixman_image_composite32(pixman_op, surf_state->image, NULL, 
                            out_state->shadow, 0, 0, 0, 0, 0, 0, 
                            pixman_image_get_width(out_state->shadow), 
                            pixman_image_get_height(out_state->shadow));
   pixman_image_set_clip_region32(out_state->shadow, NULL);
   pixman_region32_fini(&fregion);
}

static void 
_e_renderer_surfaces_repaint(E_Output *output, pixman_region32_t *damage)
{
   E_Compositor *comp;
   Eina_List *l;
   E_Surface *es;

   printf("E_Renderer Surfaces Repaint\n");

   comp = output->compositor;
   EINA_LIST_FOREACH(comp->surfaces, l, es)
     {
        if (es->plane == &comp->plane)
          {
             printf("\tDraw Surface: %p\n", es);
             _e_renderer_surface_draw(es, output, damage);
          }
     }
}

static void 
_e_renderer_surface_draw(E_Surface *surface, E_Output *output, pixman_region32_t *damage)
{
   E_Renderer_Surface_State *state;
   pixman_region32_t repaint, blend;

   if (!surface) return;
   if (!surface->output) surface->output = output;
   if (surface->output != output) return;

   if (!(state = surface->state)) return;
   if (!state->image) return;

   pixman_region32_init(&repaint);
   pixman_region32_intersect(&repaint, &surface->opaque, damage);
   pixman_region32_subtract(&repaint, &repaint, &surface->clip);

   if (!pixman_region32_not_empty(&repaint))
     {
        pixman_region32_fini(&repaint);
        return;
     }

   printf("E_Renderer Surface Draw: %p\n", surface);

   /* TODO: handle transforms ? */

   pixman_region32_init_rect(&blend, 0, 0, 
                             surface->geometry.w, surface->geometry.h);
   pixman_region32_subtract(&blend, &blend, &surface->opaque);

   if (pixman_region32_not_empty(&surface->opaque))
     _e_renderer_region_repaint(surface, output, &repaint, 
                                &surface->opaque, PIXMAN_OP_SRC);

   if (pixman_region32_not_empty(&blend))
     _e_renderer_region_repaint(surface, output, &repaint, 
                                &blend, PIXMAN_OP_OVER);

   pixman_region32_fini(&blend);
   pixman_region32_fini(&repaint);
}

static Eina_Bool 
_e_renderer_cb_pixels_read(E_Output *output, int format, void *pixels, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Renderer_Output_State *state;
   pixman_image_t *buffer;

   printf("E_Renderer Pixels Read\n");

   if (!(state = output->state)) return EINA_FALSE;
   if (!state->hw_buffer) return EINA_FALSE;

   buffer = 
     pixman_image_create_bits(format, w, h, pixels, 
                              (PIXMAN_FORMAT_BPP(format) / 8) * w);

   pixman_image_composite32(PIXMAN_OP_SRC, state->hw_buffer, NULL, 
                            buffer, 0, 0, 0, 0, 0, 0, 
                            pixman_image_get_width(state->hw_buffer), 
                            pixman_image_get_height(state->hw_buffer));
   pixman_image_set_transform(state->hw_buffer, NULL);
   pixman_image_unref(buffer);

   return EINA_TRUE;
}

static void 
_e_renderer_cb_output_buffer_set(E_Output *output, pixman_image_t *buffer)
{
   E_Renderer_Output_State *state;

   printf("E_Renderer Output Buffer Set\n");

   state = output->state;
   if (state->hw_buffer) pixman_image_unref(state->hw_buffer);
   state->hw_buffer = buffer;
   if (state->hw_buffer)
     {
        /* output->compositor->read_format; */
        pixman_image_ref(state->hw_buffer);
     }
}

static void 
_e_renderer_cb_output_repaint(E_Output *output, pixman_region32_t *damage)
{
   E_Renderer_Output_State *state;
   pixman_region32_t region;

   printf("E_Renderer Output Repaint\n");

   state = output->state;
   if (!state->hw_buffer) return;

   /* repaint surfaces */
   _e_renderer_surfaces_repaint(output, damage);

   /* copy to hw */
   pixman_region32_init(&region);
   pixman_region32_copy(&region, damage);

   /* global to output ? */

   pixman_image_set_clip_region32(state->hw_buffer, &region);
   pixman_image_composite32(PIXMAN_OP_SRC, state->shadow, NULL, 
                            state->hw_buffer, 0, 0, 0, 0, 0, 0, 
                            pixman_image_get_width(state->hw_buffer), 
                            pixman_image_get_height(state->hw_buffer));
   pixman_image_set_clip_region32(state->hw_buffer, NULL);

   pixman_region32_copy(&output->repaint.prev_damage, damage);
   wl_signal_emit(&output->signals.frame, output);
}

static void 
_e_renderer_cb_damage_flush(E_Surface *surface)
{
   printf("E_Renderer Damage Flush\n");
}

static void 
_e_renderer_cb_attach(E_Surface *surface, struct wl_buffer *buffer)
{
   E_Renderer_Surface_State *state;
   pixman_format_code_t format;
   Evas_Coord w = 0, h = 0;
   void *data;
   int stride = 0;

   state = surface->state;

   e_buffer_reference(&state->buffer_reference, buffer);

   if (state->image) pixman_image_unref(state->image);
   state->image = NULL;

   if (!buffer) return;

   printf("E_Renderer Attach\n");

   switch (wl_shm_buffer_get_format(buffer))
     {
      case WL_SHM_FORMAT_XRGB8888:
        format = PIXMAN_x8r8g8b8;
        break;
      case WL_SHM_FORMAT_ARGB8888:
        format = PIXMAN_a8r8g8b8;
        break;
     }

   w = wl_shm_buffer_get_width(buffer);
   h = wl_shm_buffer_get_height(buffer);
   data = wl_shm_buffer_get_data(buffer);
   stride = wl_shm_buffer_get_stride(buffer);

   state->image = pixman_image_create_bits(format, w, h, data, stride);
}

static Eina_Bool 
_e_renderer_cb_output_create(E_Output *output, unsigned int window)
{
   E_Renderer_Output_State *state;
   Evas_Coord w = 0, h = 0;

   /* try to allocate space for output state */
   if (!(state = E_NEW(E_Renderer_Output_State, 1)))
     return EINA_FALSE;

   w = output->current->w;
   h = output->current->h;

   state->shadow_buffer = malloc(w * h * sizeof(int));
   if (!state->shadow_buffer) return EINA_FALSE;

   state->shadow = 
     pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h, state->shadow_buffer, 
                              w * sizeof(int));
   if (!state->shadow)
     {
        free(state->shadow_buffer);
        free(state);
        return EINA_FALSE;
     }

   output->state = state;

   return EINA_TRUE;
}

static void 
_e_renderer_cb_output_destroy(E_Output *output)
{
   printf("E_Renderer Output Destroy\n");
}

static Eina_Bool 
_e_renderer_cb_surface_create(E_Surface *surface)
{
   E_Renderer_Surface_State *state;

   if (!(state = E_NEW(E_Renderer_Surface_State, 1)))
     return EINA_FALSE;

   surface->state = state;

   return EINA_TRUE;
}

static void 
_e_renderer_cb_surface_destroy(E_Surface *surface)
{
   E_Renderer_Surface_State *state;

   if ((state = surface->state))
     {
        if (state->image) pixman_image_unref(state->image);
        state->image = NULL;
     }

   e_buffer_reference(&state->buffer_reference, NULL);

   E_FREE(state);
}

static void 
_e_renderer_cb_surface_color_set(E_Surface *surface, int r, int g, int b, int a)
{

}

static void 
_e_renderer_cb_destroy(E_Compositor *comp)
{
   printf("E_Renderer Destroy\n");
}
