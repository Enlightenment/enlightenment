#include "e.h"

/* local function prototypes */
static int _e_renderer_cb_pixels_read(E_Output *output, int format, void *pixels, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_renderer_cb_output_repaint(E_Output *output, pixman_region32_t *damage);
static void void _e_renderer_cb_damage_flush)(E_Surface *surface);
static void void _e_renderer_cb_attach(E_Surface *surface, struct wl_buffer *buffer);
static int _e_renderer_cb_surface_create(E_Surface *surface);
static void _e_renderer_cb_surface_destroy(E_Surface *surface);
static void _e_renderer_cb_surface_color_set(E_Surface *surface, int r, int g, int b, int a);
static void _e_renderer_cb_destroy)(E_Compositor *comp);

EAPI Eina_Bool 
e_renderer_create(E_Compositor *comp)
{
   E_Renderer *rend;

   /* try to allocate space for new renderer */
   if (!(rend = E_NEW(E_Renderer, 1))) return EINA_FALSE;

   /* setup renderer functions */
   rend->pixels_read = _e_renderer_cb_pixels_read;
   rend->output_repaint = _e_renderer_cb_output_repaint;
   rend->damage_flush = _e_renderer_cb_damage_flush;
   rend->attach = _e_renderer_cb_attach;
   rend->surface_create = _e_renderer_cb_surface_create;
   rend->surface_destroy = _e_renderer_cb_surface_destroy;
   rend->surface_color_set = _e_renderer_cb_surface_color_set;
   rend->destroy = _e_renderer_cb_destroy;

   comp->renderer = rend;

   return EINA_TRUE;
}

/* local functions */
static int 
_e_renderer_cb_pixels_read(E_Output *output, int format, void *pixels, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   return 1;
}

static void 
_e_renderer_cb_output_repaint(E_Output *output, pixman_region32_t *damage)
{

}

static void void 
_e_renderer_cb_damage_flush)(E_Surface *surface)
{

}

static void void 
_e_renderer_cb_attach(E_Surface *surface, struct wl_buffer *buffer)
{

}

static int 
_e_renderer_cb_surface_create(E_Surface *surface)
{
   return 1;
}

static void 
_e_renderer_cb_surface_destroy(E_Surface *surface)
{

}

static void 
_e_renderer_cb_surface_color_set(E_Surface *surface, int r, int g, int b, int a)
{

}

static void 
_e_renderer_cb_destroy)(E_Compositor *comp)
{

}

