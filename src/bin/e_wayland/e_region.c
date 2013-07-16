#include "e.h"

/* local function prototypes */
static void _e_region_destroy(struct wl_resource *resource);
static void _e_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);

/* local wayland interfaces */
static const struct wl_region_interface _e_region_interface = 
{
   _e_region_cb_destroy,
   _e_region_cb_add,
   _e_region_cb_subtract
};

EAPI E_Region *
e_region_new(struct wl_client *client, unsigned int id)
{
   E_Region *reg;

   /* try to allocation space for a new region */
   if (!(reg = E_NEW_RAW(E_Region, 1))) return NULL;

   pixman_region32_init(&reg->region);

   reg->resource = 
     wl_resource_create(client, &wl_region_interface, 1, id);
   wl_resource_set_implementation(reg->resource, &_e_region_interface, 
                                  reg, _e_region_destroy);

   return reg;
}

/* local functions */
static void 
_e_region_destroy(struct wl_resource *resource)
{
   E_Region *reg;

   if (!(reg = wl_resource_get_user_data(resource))) return;
   pixman_region32_fini(&reg->region);
   E_FREE(reg);
}

static void 
_e_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Region *reg;

   if (!(reg = wl_resource_get_user_data(resource))) return;

   pixman_region32_union_rect(&reg->region, &reg->region, x, y, w, h);
}

static void 
_e_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Region *reg;
   pixman_region32_t rect;

   /* try to cast resource to our region */
   if (!(reg = wl_resource_get_user_data(resource))) return;

   pixman_region32_init_rect(&rect, x, y, w, h);
   pixman_region32_subtract(&reg->region, &reg->region, &rect);
   pixman_region32_init(&rect);
}
